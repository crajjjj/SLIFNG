# Write API (SLIF-compatible)

Everything here is old SLIF's own surface, pinned from 1.2.2 bytecode. Signatures are **frozen**: Papyrus bakes argument lists into compiled callers, so names, order, types and count never change. If your mod already integrates with SLIF, you have nothing to do.

!!! tip "Integration kit"
    Every release attaches `SLIFNG-API-<version>+.zip` ([releases](https://github.com/crajjjj/SLIFNG/releases)): the C++ query header, the three `.psc` a consumer compiles against (`SLIF_Main`, `SLIF_Morph`, `SLIFNG`), and a `VERSIONS.txt` of the numbers to gate on. You do **not** need SLIF NG installed to build against it, and it adds no hard dependency. Build it yourself with `python tools/pack-api-kit.py`.

## Values in one paragraph

Node targets are multiplicative scales, `1.0` = neutral (Beeing Female sends `scale + 1`). Morphs are additive weights, `0.0` = neutral, and **exactly `0.0` clears your contribution**. `minimum/maximum/multiplier/increment` accept `-1.0` for "keep the defaults" (0 / 100 / 1.0 / 0.1); your value is clamped into `[min, max]` and multiplied by `multiplier` before aggregation. `increment` is the per-quarter-second step when incremental inflation is on. `oldModName` names the NiOverride key your mod used *before* handing control over - it is cleaned once per actor per session, which is how a mid-save handover avoids double inflation.

## Node keys

A `node` argument accepts any of: a `slif_*` key (`slif_belly`; `slif_breast` / `slif_butt` are L+R pairs), a **raw skeleton node name** (`"NPC Belly"`), a side alias (`slif_left_breast` resolves to the pair), or a SLIF NG **semantic region** (`region:weight`, see [Body Profile Format](body-profiles.md#custom-regions)). Every spelling of one physical thing lands on the same ledger target. `slif_breast01` and `slif_breast_p` are silent no-ops, bug-compatible with the reference.

## SLIF_Main

```papyrus
Function inflate(Actor kActor, string modName, string node, float value, \
    int gender = -1, int perspective = -1, string oldModName = "", \
    float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global

Function unregisterNode(Actor kActor, string node, string modName = "All Mods") Global
Function unregisterActor(Actor kActor, string modName = "All Mods") Global

Function hideNode(Actor kActor, String modName, String node, float value = 0.0000001, string oldModName = "") Global
Function showNode(Actor kActor, String modName, String node) Global

Function inflateBoth(Actor kActor, string modName, string syncKey, float value, ...) Global
Function inflateMultiple(Actor kActor, string modName, string[] nodes, float[] values, \
    int gender, int perspective, string oldModName, \
    float[] minimum, float[] maximum, float[] multiplier, float[] increment) Global

Function resetActor(...) Global          ; drop one node, or everything when node == ""
Function updateActorList(...) Global     ; bounds push; the reference's re-apply sweep is unnecessary here
Bool Function IsInstalled() Global
```

Notes:

- `gender` / `perspective` are accepted and ignored (reference behaviour).
- `hideNode` pins a node flat (floored at `0.0000001`) and **wins over every contribution** until `showNode`; it is keyed per actor+node, not per mod - any mod may lift it. On a morph-driven body, hidden targets force their sliders neutral.
- `unregisterActor` is safe to call without ever having registered; it still clears stale applied output (this is Milk Mod Economy's whole integration).
- `inflateMultiple` (and `morphMultiple`) are batched: one native call, one coalesced apply for the whole set. Bounds arrays shorter than `nodes` mean "defaults" for the tail.

## SLIF_Morph

```papyrus
Function morph(Actor kActor, string modName, string morphName, float value, \
    string oldModName = "", float minimum = -1.0, float maximum = -1.0, \
    float multiplier = -1.0, float increment = -1.0) Global

Function morphMultiple(Actor kActor, string modName, string[] morphNames, float[] values, \
    string oldModName, float[] minimum, float[] maximum, float[] multiplier, float[] increment) Global

Function unregisterMorph(Actor kActor, string morphName, string modName = "All Mods") Global
Function unregisterActor(Actor kActor, string modName = "All Mods") Global
```

`morphName` is a pass-through BodySlide slider name - no vocabulary, and its original case reaches skee. The cross-mod direct total is mirrored into StorageUtil as `slif_<morphName>` for legacy readers.

## Semantic regions

A **region** lets you inflate something that has no skeleton bone - weight gain, muscle mass - without knowing a single slider name. You send intent; the actor's [body profile](body-profiles.md#custom-regions) decides which sliders that means on *their* body, so one call is correct on 3BA, UBE and BHUNP alike. It is an ordinary write through `SLIF_Main.inflate`, with `region:<name>` as the node key:

```papyrus
; node-scale semantics: 1.0 = neutral, so 1.4 is "40% of the way up"
SLIF_Main.inflate(akActor, "My Mod", "region:weight", 1.4)

; folds across mods, unregisters and reads back like any other target
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
    Unlike a canonical key, a region has **no bone to fall back to** - on a profile without that section the call is a logged no-op. Ask first and keep a fallback:

    ```papyrus
    if SLIFNG.HasTarget(akActor, "region:weight")
        SLIF_Main.inflate(akActor, "My Mod", "region:weight", 1.4)
    else
        SLIF_Main.inflate(akActor, "My Mod", "slif_belly", 1.15)
    endif
    ```

    [`HasTarget`](query-api.md#hastarget-ask-before-you-write) needs `SLIFNG.GetVersion() >= 3`.

Prefer an existing region name (`weight` is the conventional one) so several mods drive the same concept instead of fragmenting it. If the region you need is not defined on the bodies you support, ship a [region overlay](body-profiles.md#region-overlays) - a small INI per body, named after your mod - rather than a whole profile, which would override the user's body choice.

## Mod events

Handlers live on the player alias; register nothing yourself, just send:

```papyrus
int e = ModEvent.Create("SLIF_inflate")
ModEvent.PushForm(e, target)          ; the affected actor
ModEvent.PushString(e, "My Mod")      ; your mod name
ModEvent.PushString(e, "slif_belly")  ; node key / raw node
ModEvent.PushFloat(e, 2.2)
ModEvent.PushString(e, "")            ; oldModName
ModEvent.Send(e)
```

Also handled: `SLIF_unregisterActor(Form, modName)` and `SLIF_unregisterNode(Form, modName, node)`.

!!! warning "The argument-order swap is part of the contract"
    The `SLIF_unregisterNode` **event** carries `(modName, node)`, while the `unregisterNode` **function** takes `(node, modName)`. This mismatch is the reference's and is kept on purpose - do not "fix" your sender.

## Incremental inflation and your mod

When the user has incremental inflation on, node and morph value *changes* step toward their new fold at your row's `increment` per quarter second, natively. Send your final intended value once; do not build your own drain loop - retargeting mid-ramp just works, and hide/unregister stay instant.
