# Write API

Everything that changes a body. If your mod already integrates with old SLIF, it works unchanged and you can stop reading.

## Start here

Inflating a belly is one call. There is no registration step, no init, nothing to check first:

```papyrus
SLIF_Main.inflate(akActor, "My Mod", "slif_belly", 1.5)
```

`1.0` is neutral, so that is "half again". To stop:

```papyrus
SLIF_Main.unregisterNode(akActor, "slif_belly", "My Mod")
```

Your mod name is how SLIF NG keeps your contribution apart from everyone else's, so use the same string everywhere and make it recognisable — it appears in the user's MCM and in bug reports.

!!! tip "Integration kit"
    Every release attaches `SLIFNG-API-<version>+.zip` ([releases](https://github.com/crajjjj/SLIFNG/releases)): the C++ query header, the three `.psc` you compile against (`SLIF_Main`, `SLIF_Morph`, `SLIFNG`), and a `VERSIONS.txt` of the numbers to gate on. You do **not** need SLIF NG installed to build against it, and it adds no hard dependency. Build it yourself with `python tools/pack-api-kit.py`.

## Node, morph, or region?

The one decision worth making up front. All three fold together across mods, so the choice is about what you are describing, not about power:

| | Use when | Call | Neutral |
|---|---|---|---|
| **Node key** | You mean a body part SLIF has a name for: belly, breasts, butt, scrotum | `SLIF_Main.inflate(a, mod, "slif_belly", 1.5)` | `1.0` |
| **Morph** | You know the exact BodySlide slider you want | `SLIF_Morph.morph(a, mod, "PregnancyBelly", 0.4)` | `0.0` |
| **Region** | You mean a concept with no bone: weight, muscle | `SLIF_Main.inflate(a, mod, "region:weight", 1.4)` | `1.0` |

**Prefer a node key or a region.** Both are body-independent: the actor's [body profile](body-profiles.md) decides what they mean on *their* body, so one call is right on 3BA, UBE and BHUNP alike. A raw morph name is only correct on bodies that happen to ship that slider, and silently does nothing elsewhere.

## Values

| Argument | Meaning |
|---|---|
| `value` | Node and region targets: a multiplicative scale, `1.0` = neutral (Beeing Female sends `scale + 1`). Morphs: an additive weight, `0.0` = neutral, and **exactly `0.0` clears your contribution** |
| `minimum` / `maximum` | Your value is clamped into this range before aggregation. `-1.0` = keep the defaults, `0` and `100` |
| `multiplier` | Applied after the clamp. `-1.0` = keep `1.0` |
| `increment` | Step per quarter second while the user has incremental inflation on. `-1.0` = keep `0.1` |
| `oldModName` | The NiOverride key your mod wrote under *before* handing control to SLIF. Cleaned once per actor per session, which is what stops a mid-save handover double-inflating. Pass `""` if you never had one |
| `gender`, `perspective` | Accepted and ignored, as in the reference |

## Node keys

A `node` argument accepts any spelling of one physical thing, and they all land on the same ledger target:

- a `slif_*` key — `slif_belly`; `slif_breast` and `slif_butt` are L+R pairs
- a raw skeleton node name — `"NPC Belly"`
- a side alias — `slif_left_breast` resolves to the pair
- a region — `region:weight`

`slif_breast01` and `slif_breast_p` are silent no-ops, bug-compatible with the reference.

The node vocabulary is **closed**: four targets over six XPMSSE bones, listed in [The node targets](../players/bodies.md#the-node-targets). A spelling outside that table and its aliases is rejected with a log line and returns `false`. Old SLIF instead used any unrecognised string as a bone name, so a typo became an invisible bone scale. New targets are added on the morph side — `morph:<slider>` or a profile region — never as a new bone.

## `SLIF_Main`

What you will actually call:

```papyrus
Function inflate(Actor kActor, string modName, string node, float value, \
    int gender = -1, int perspective = -1, string oldModName = "", \
    float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global

Function unregisterNode(Actor kActor, string node, string modName = "All Mods") Global
Function unregisterActor(Actor kActor, string modName = "All Mods") Global   ; everything you drive on them
```

Occasionally useful:

```papyrus
; pin a node flat and WIN over every contribution until showNode.
; Keyed per actor+node, not per mod, so any mod may lift it. Devious Devices
; uses this for a chastity belt. On a morph-driven body the hidden target's
; sliders are forced neutral.
Function hideNode(Actor kActor, String modName, String node, float value = 0.0000001, string oldModName = "") Global
Function showNode(Actor kActor, String modName, String node) Global

; one native call and ONE coalesced apply for the whole set - cheaper than a
; loop of inflate. Bounds arrays shorter than `nodes` mean defaults for the tail.
Function inflateMultiple(Actor kActor, string modName, string[] nodes, float[] values, \
    int gender, int perspective, string oldModName, \
    float[] minimum, float[] maximum, float[] multiplier, float[] increment) Global

Bool Function IsInstalled() Global
```

Present for compatibility; you are unlikely to need them:

| Function | Behaviour here |
|---|---|
| `inflateBoth(kActor, modName, syncKey, value, …)` | Identical to `inflate`. A sync pair is already one target, so both bones always carry the same value |
| `resetActor(kActor, modName, node, …)` | Drops one node, or everything for that mod when `node == ""`. The reference's `value` argument is ignored: the remaining contributions are recomputed instead, which cannot drift from what other mods still want |
| `updateActorList(modName, node, …)` | Pushes new bounds across every tracked actor. The reference's re-apply sweep is unnecessary here — nothing can drift |

Trailing argument lists match the reference exactly; see [CONTRACT.md](https://github.com/crajjjj/SLIFNG/blob/main/CONTRACT.md) if you need them verbatim.

!!! note "`unregisterActor` is safe to call blind"
    You may call it for an actor you never registered. It still clears stale applied output, which is Milk Mod Economy's entire integration.

## `SLIF_Morph`

```papyrus
Function morph(Actor kActor, string modName, string morphName, float value, \
    string oldModName = "", float minimum = -1.0, float maximum = -1.0, \
    float multiplier = -1.0, float increment = -1.0) Global

Function morphMultiple(Actor kActor, string modName, string[] morphNames, float[] values, \
    string oldModName, float[] minimum, float[] maximum, float[] multiplier, float[] increment) Global

Function unregisterMorph(Actor kActor, string morphName, string modName = "All Mods") Global
Function unregisterActor(Actor kActor, string modName = "All Mods") Global
```

`morphName` is a pass-through BodySlide slider name — no vocabulary, and its original case reaches skee. The cross-mod direct total is mirrored into StorageUtil as `slif_<morphName>` for legacy readers.

!!! warning "`SLIF_Morph.unregisterActor` clears your node inflation too"
    The reference cleared only the morph side. One ledger row per mod holds both here, so this drops everything you drive on that actor. Use `unregisterMorph` if you want one slider gone and the rest kept.

## Semantic regions

A region inflates something with no skeleton bone — weight gain, muscle mass — without your knowing a single slider name. You send intent; the actor's [body profile](body-profiles.md#custom-regions) decides which sliders that means on *their* body. It is an ordinary `SLIF_Main.inflate` with `region:<name>` as the node key:

```papyrus
SLIF_Main.inflate(akActor, "My Mod", "region:weight", 1.4)   ; 1.0 neutral, so 40% up
SLIF_Main.unregisterNode(akActor, "region:weight", "My Mod")
Float now = SLIFNG.GetApplied(akActor, "region:weight")
```

Backed by a profile section named after the region (matching is case-insensitive):

```ini
[Weight]
FullScale=2.0           ; the node deviation at which the sliders reach Max
Morph1=ChubbyWaist
Morph1Max=0.6
Morph2=ChubbyButt
Morph2Max=0.4
```

At `1.4` the deviation is `0.4`, so `ChubbyWaist` lands on `0.4 x 0.6 / 2.0 = 0.12` and `ChubbyButt` on `0.08`.

!!! warning "A region is sliders or nothing"
    Unlike a canonical key, a region has **no bone to fall back to** — on a profile without that section the call is a logged no-op. Ask first and keep a fallback:

    ```papyrus
    if SLIFNG.HasTarget(akActor, "region:weight")
        SLIF_Main.inflate(akActor, "My Mod", "region:weight", 1.4)
    else
        SLIF_Main.inflate(akActor, "My Mod", "slif_belly", 1.15)
    endif
    ```

    [`HasTarget`](query-api.md#hastarget-ask-before-you-write) needs `SLIFNG.GetVersion() >= 3`.

Prefer an existing region name (`weight` is the conventional one) so several mods drive the same concept instead of fragmenting it. If the region you need is not defined on the bodies you support, ship a [region overlay](body-profiles.md#region-overlays) — a small INI per body, named after your mod — rather than a whole profile, which would override the user's body choice.

## Mod events

An alternative to the direct calls, useful if you would rather not compile against `SLIF_Main` at all. Handlers live on the player alias; register nothing yourself, just send:

```papyrus
int e = ModEvent.Create("SLIF_inflate")
ModEvent.PushForm(e, target)          ; the affected actor
ModEvent.PushString(e, "My Mod")      ; your mod name
ModEvent.PushString(e, "slif_belly")  ; node key / raw node / region
ModEvent.PushFloat(e, 2.2)
ModEvent.PushString(e, "")            ; oldModName
ModEvent.Send(e)
```

All 21 of the reference's `SLIF_*` events are handled, including `SLIF_morph`, `SLIF_unregisterActor`, `SLIF_unregisterNode`, `SLIF_hideNode` / `SLIF_showNode` and the `set*` bounds family. A mod event that nobody registered fails silently rather than logging, which is why the whole surface is registered even though only three have a known sender.

!!! warning "The argument-order swap is part of the contract"
    The `SLIF_unregisterNode` **event** carries `(modName, node)`, while the `unregisterNode` **function** takes `(node, modName)`. This mismatch is the reference's and is kept on purpose — do not "fix" your sender.

## Incremental inflation

When the user has it on, value *changes* step toward their new fold at your row's `increment` per quarter second, natively, off the script engine. **Send your final intended value once.** Do not build your own drain loop: retargeting mid-ramp just works, and `hideNode` and unregistration stay instant. To act on a finished shape rather than a moving one, listen for [`SLIFNG_Settled`](query-api.md#slifng_settled-mod-event).

## Why the signatures never change

Papyrus bakes argument lists into compiled callers, so a signature change breaks every `.pex` already built against it — including ones whose authors have moved on. Names, order, types and count are therefore frozen, pinned from SLIF 1.2.2 bytecode. New capability arrives as new functions, never as a new parameter.
