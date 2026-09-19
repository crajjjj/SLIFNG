# Old SLIF vs. SLIF NG

Everything below is pinned against decompiled SLIF SE 1.2.2 bytecode and its live runtime configuration, not against its (older) bundled sources.

## What consumers see: identical

| Surface | Old SLIF | SLIF NG |
|---|---|---|
| Plugin name / detection | `SexLab Inflation Framework.esp`, quest `0x800` `SLIF_Menu`, `0x801` scanner | Same names, same FormIDs, same EditorIDs (clean-room ESP); `Game.IsPluginInstalled` and `Quest.GetQuest("SLIF_Menu")` both work |
| Frozen signatures | `inflate` (11 args), `unregisterNode`, `unregisterActor`, `morph` (9 args), `hideNode` / `showNode`, `inflateBoth`, `inflateMultiple`, `morphMultiple`, `resetActor`, `updateActorList`, `unregisterMorph`, `GetValue` / `GetMinValue` / `GetMaxValue` (both scripts), `IsInstalled` | Byte-identical declarations; arities match what consumer `.pex` files baked at compile time |
| Mod events | `SLIF_inflate`, `SLIF_unregisterActor`, `SLIF_unregisterNode` - including the `(modName, node)` vs `(node, modName)` argument-order swap | Same handlers, same swap, kept verbatim |
| Node keys | `slif_belly`, the `slif_breast` / `slif_butt` sync pairs, `slif_scrotum`, the `slif_left_breast`-style side aliases; raw node names (`"NPC Belly"`) pass through as targets | Same [four targets](players/bodies.md#the-node-targets), same resolution, same sync expansion; the raw names of those four pass through, unrecognised ones do not |
| Bug compatibility | `slif_breast01`, `slif_breast_p` silently dropped | Same no-op, but reported in diagnostics instead of vanishing |
| `oldModName` cleanup | Removes the consumer's pre-SLIF NiOverride key on first contact | Same, once per actor per session |
| Applied skee key | `"SexLab Inflation Framework.esp"` for all output | Same key, so a real-SLIF save's applied values are owned and overwritten in place |
| Getter semantics | `"All Mods"` = the folded total, sync keys average left/right, an absent key returns the caller's default, `GetMaxValue` defaults to 100 | Same, including the asymmetric default |
| `slif_<morphName>` in StorageUtil | Written by the engine, read directly by Sexlab Survival | Still written (mirrored by the shim), so that direct read keeps working |

## The math: identical where it was tested

| Formula | Both |
|---|---|
| Per-contribution clamp | `SetBounds(value, min, max) * mult`, inverted bounds tolerated by ordering them |
| Defaults | min 0, max 100, mult 1.0, increment 0.1; `-1.0` means "keep the default" |
| Calculation types | All six, SLIF's numbering, **default 0 = Top X** (`largest + second/3 + third/6`); non-positive contributions skipped; neutral floor on an empty result; below-neutral values can show |
| Region morph curve | The node value drives sliders through the profile's scale factor, exactly the reference's percent/steps arithmetic reparameterised |
| Morph clear | Exactly `0.0` clears the contribution |
| `hideNode` | Pin floored at `0.0000001`, per actor and node, any mod may lift it |

## Deliberate differences

| Case | Old SLIF | SLIF NG | Why |
|---|---|---|---|
| Several mods, one **slider** | Morph contributions always stack, even under "highest wins" | One value per mod (its direct morph + its transformed node share), then the calculation type folds across mods - same as nodes | The always-sum corner shipped disabled (every stock morph percent is 0) and was never field-tested; composition must not depend on which API spelling a mod used |
| Incremental inflation | A Papyrus drain loop, one full body rebuild per step (default increment 0.1); **off** by default | A native ramp off the script engine, one coalesced apply per actor per tick; **on** by default | Stepped swelling reads better and no longer costs anything; instant is one MCM click away |
| Hidden node on a morph body | Morph side left stale while hidden | Sliders forced neutral | A pin of 0.01 through a blend would produce a concave belly, not a flat one |
| A single sync node (`"NPC L Breast"` alone) | Scales just that node | Scales the pair | One target per sync pair; no observed consumer sends one side |
| A bone outside the vocabulary (`"NPC Spine2"`) | `ConvertToNode` returned the string unchanged, so it scaled whatever you named | Rejected with a log line; the write returns `false` and `HasTarget` says so up front | An open vocabulary cannot be routed onto shared targets, and a typo became an invisible bone scale; growth goes to `morph:` / `region:` instead |
| Non-unique NPCs | Transient NetImmerse scaling by default | Always NiOverride/skee, for everyone | One code path, persistent and self-healing |
| Calculation-type switch | Left stale applied values behind | Recompute + one re-apply pass | The fold is pure over stored inputs |
| `updateActorList` | A full re-push sweep (its store could drift) | A genuine no-op, plus a bounds push (`UpdateModBounds`) for the callers that used it to widen limits | Nothing can drift: every change applies immediately and a load re-derives |

## Engine and architecture

| Aspect | Old SLIF | SLIF NG |
|---|---|---|
| Engine | ~11,700 lines of Papyrus across 27 scripts | A native SKSE DLL (CommonLibSSE-NG, SE/AE/VR) plus 10 thin scripts |
| State | Hundreds of StorageUtil keys in the Papyrus save | One compact co-save record; recomputed and re-applied on every load |
| Apply cost | Rebuilds per step, JSON reads per apply, no unchanged-value skip | Early-out on unchanged values, one body rebuild per batch, all geometry work on the main thread |
| Body configuration | Hand-edited JSON through nine MCM pages; morphs ship disabled | Per-actor INI profiles picked in the installer; UBE matched by race; verified slider names |
| Diagnostics | Silent | An MCM actor page: who inflates what, what it becomes on this body, what RaceMenu actually shows, which foreign mods stack on top |

## Performance

SLIF NG is not faster in frames per second, and a user who says old SLIF ran fine
is very likely right about their own setup. Three reasons it could be:

* **The expensive path was off by default.** `SLIF_Util.inflateNode` tests
  `InflateInstant` first, and the reference's own default inflation type is
  instant (`SLIF_Util.GetDefaultInflationType` returns 1). Unless a mod was
  switched to Incremental, the drain loop below never ran at all.
* **Papyrus cost is latency, not frame rate.** The script engine has its own
  budget; overrunning it delays scripts. The symptom is a belly that updates a
  few seconds late, or a stack dump in the log, not a stutter.
* **It scales with actors x consumers x churn.** One player and one inflation mod
  is nothing at all. The complaints came from many-NPC load orders with several
  mods writing at once.

### Where the cost actually was

Not in the sheer number of small reads. In one structure: `SLIF_Calc.DoSingleQueue`
is a `while` loop with no `Utility.Wait`, running on the **calling mod's** stack
until the queue drains.

```papyrus
while(StorageUtil.StringListCount(kActor, node + "slif_queue_mods") > 0)
    SetNodeValueIncrement(kActor, aToString, node)
endWhile
```

Every iteration reaches `SLIF_Scale.SetNodeScale` and then `SetMorphValue`, which
per step performs two `JsonUtil` path reads, a node-transform write plus an
`UpdateNodeTransforms` for each node on the path, a `SetBodyMorph`, and a
`Debug.Trace` whose message string is concatenated whether or not tracing is on.

At the default increment of 0.1, a belly travelling from 1.0 to 2.2 is twelve of
those back to back, and whatever asked for it (a Beeing Female cycle tick, a
SexLab orgasm hook) blocks for all twelve. The sync-pair variant, `DoMultiQueue`
for breasts and butt, re-checks three queue counts per iteration and applies two
nodes per step.

There was no defined cadence: the VM's scheduling was the cadence, which is also
why the same inflation ramped at different speeds on different machines.

### What replaces it

| Per `inflate` call | Old SLIF | SLIF NG |
|---|---|---|
| Papyrus operations | dozens, growing with the distance travelled | one native call |
| `StorageUtil` reads and writes | several per step | none on the node path; one mirror write on the morph path, for [Sexlab Survival's direct read](#what-consumers-see-identical) |
| `JsonUtil` reads | two per step | none |
| Body rebuilds | one per step | one per batch, coalesced |
| A re-sent unchanged value | full pass anyway | early-out before anything is touched |
| Ramp cadence | whatever the VM scheduled | a 100 ms native ticker, one coalesced apply per actor per tick |
| Blocks the caller | yes, for the whole ramp | no; geometry work is handed to the main thread |

Whole-mod figures are in [Engine and architecture](#engine-and-architecture)
above: 27 Papyrus scripts become 10 thin ones, 1,012 `StorageUtil` call sites
become 29 (all but one inside the one-shot legacy importer), and 187 `JsonUtil`
call sites become none.

### What is not claimed

**None of this has been benchmarked.** The figures above are static analysis of
both codebases: operation counts per call path, not a profiler run. No frame
rate, script-lag or save-size measurement has been taken, and the reference
numbers come from its bundled 1.2.1a sources while the shipped bytecode is 1.2.2
(the queue structure is very unlikely to differ, but the line counts are
approximate).

So the fair summary is narrow: **bounded work per call instead of work
proportional to how far a body is travelling, and no script loop on the
consumer's thread.** If old SLIF performed acceptably for you, SLIF NG's real
arguments are elsewhere: the setup surface (nine MCM pages and hand-edited JSON
become a profile chosen in the installer plus one magnitude knob), the much
smaller persisted state, and diagnostics that tell you what is actually
happening.

## Dropped (nothing calls them) and new (old SLIF had nothing)

**Dropped:** roughly 70 of ~90 public functions and 21 of 24 mod events, the grow/shrink/absorb spells, the actor scanner, the scrotum timer, the presets JSON API, 17 translations. An unknown call surfaces as one loud, searchable log line - that is the detection channel for unsurveyed consumers.

**New:** automatic old-save migration, the user magnitude knobs (master, per-target, per-actor), the actor diagnostics page, semantic profile regions (`region:weight`), batched writes, the read API for mod authors (Papyrus and C++), foreign-key detection, verbose logging.
