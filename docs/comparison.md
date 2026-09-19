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

SLIF NG is faster, and not because C++ is faster than Papyrus. The work itself is
gone, not recompiled. Every row below is a thing old SLIF did on each apply that
SLIF NG does not do at all.

| Per apply | Old SLIF | SLIF NG | Why it is cheaper |
|---|---|---|---|
| Body rebuild | One per increment step | One per actor per tick, whatever it touched | A twelve-step ramp is twelve rebuilds against one |
| Config reads | Two `JsonUtil` path lookups, every apply | None | Body tables are parsed once at load, not per call |
| Stored state | StorageUtil round-trips for value, bounds, queue and aggregate | An in-memory map, flushed to the co-save on save | No Papyrus-to-plugin call per field |
| Vocabulary lookup | `StringListFind` / `GetListEntry` walks over JSON-backed lists | Hash lookup | Linear scan against constant time |
| Re-sent unchanged value | Full pass anyway | Returns before touching anything | Beeing Female re-sends its values every cycle tick |
| Ramp | A `while` loop with no `Utility.Wait`, on the **calling mod's stack** | A native ticker, caller returns immediately | Off the shared script budget, and nobody blocks |
| Trace strings | Concatenated whether or not tracing is on | Built only when verbose logging is on | ~58 trace sites, several on the hot path |

The ramp is the big one. This is the original, and it runs to completion inside
whatever called it:

```papyrus
while(StorageUtil.StringListCount(kActor, node + "slif_queue_mods") > 0)
    SetNodeValueIncrement(kActor, aToString, node)
endWhile
```

Twelve passes for a belly going 1.0 to 2.2 at the default increment of 0.1, each
paying the full cost above, with a Beeing Female cycle tick or a SexLab orgasm
hook blocked for all twelve, at whatever cadence the VM happened to schedule.

### Why a stock comparison shows nothing

If you benchmark old SLIF against a rewrite and measure no difference, this is
why: **old SLIF ships inert.** Every slider in all three of its body-morph tables
has a percentage of 0.

| Shipped table | Sliders | Non-zero % |
|---|---|---|
| `000_Default_Bodymorphs.json` | 101 | **0** |
| `001_UUNP_Bodymorphs.json` | 68 | **0** |
| `002_CBBE_SE_Bodymorphs.json` | 101 | **0** |

`CalculateBodyMorphValue` multiplies by `percent / 100.0`, so every `SetBodyMorph`
receives `0.0` and clears itself, and Incremental inflation is off by default too.
A stock install is one bone write per call on a reactive path: it reaches neither
expensive path above, so the comparison measures two implementations of "write one
bone scale" and correctly finds them equal. Configure it to do what you installed
it for and the table applies.

### Beyond speed

The rest of the case does not depend on the timings at all:

| | Old SLIF | SLIF NG |
|---|---|---|
| Morphs out of the box | Bones only until you edit JSON through nine MCM pages | Verified slider sets per body, chosen in the installer, working on first launch |
| Two mods, one slider | Always stack, even under "highest wins" | One value per mod, then your calculation type folds, as nodes already did |
| State | Hundreds of StorageUtil keys in the Papyrus save | One co-save record, recomputed and re-applied every load |
| When it breaks | Silent | An actor page naming every contributor and what RaceMenu actually holds |

The counts above are operation counts along each call path rather than a profiler
trace, and the reference figures come from its 1.2.1a sources while the shipped
bytecode is 1.2.2.

## Dropped (nothing calls them) and new (old SLIF had nothing)

**Dropped:** roughly 70 of ~90 public functions and 21 of 24 mod events, the grow/shrink/absorb spells, the actor scanner, the scrotum timer, the presets JSON API, 17 translations. An unknown call surfaces as one loud, searchable log line - that is the detection channel for unsurveyed consumers.

**New:** automatic old-save migration, the user magnitude knobs (master, per-target, per-actor), the actor diagnostics page, semantic profile regions (`region:weight`), batched writes, the read API for mod authors (Papyrus and C++), foreign-key detection, verbose logging.
