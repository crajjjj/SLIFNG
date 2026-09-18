# Query API (Papyrus & C++)

Old SLIF was effectively write-only. SLIF NG adds a read surface, served from one shared core in the DLL, so the Papyrus answer and the C++ answer can never disagree.

## Shared semantics

- A `target` may be spelled as a `slif_*` key, a raw skeleton node (`"NPC Belly"`), `morph:<slider>`, or `region:<name>`.
- `modName` compares case-insensitively. `"All Mods"` is the aggregate pseudo-mod: it reads the folded total, not a mod literally named that.
- **An absent row returns YOUR default**, never an invented neutral - reference semantics, and load-bearing: Estrus Chaurus clamps its own growth with `GetMaxValue(..., maxScale)` and a `0.0` there would crush the actor flat.
- The `"All Mods"` aggregate is returned **with the user's magnitude applied** (a deliberate departure): the question consumers ask is "how inflated does this actor look", and the answer should match the visible body. Mid-ramp, it is the in-flight value, as the reference's was.

## Papyrus (`SLIFNG.psc`)

```papyrus
; the classic getters (also exposed as SLIF_Main.GetValue etc.)
Float Function GetValue(Actor kActor, String modName, String target, Float default) Global Native
Float Function GetMinValue(...) / GetMaxValue(...) Global Native
Float Function GetApplied(Actor kActor, String target) Global Native        ; shown value, no magnitude
Float Function GetContribution(Actor kActor, String modName, String target) Global Native
Float Function GetCombinedMorph(Actor kActor, String morphName) Global Native  ; the slif_<morph> total

; enumeration - "what is there to ask about"
Bool     Function HasTarget(Actor kActor, String target) Global Native  ; would a write do anything?
Bool     Function IsTracked(Actor kActor) Global Native
Actor[]  Function GetTrackedActors() Global Native
String[] Function GetNodeTargets(Actor kActor) Global Native    ; canonical keys with stored state
String[] Function GetMorphTargets(Actor kActor) Global Native   ; sliders with direct contributions
String[] Function GetModsDriving(Actor kActor, String target) Global Native

; user magnitude (write, but author-relevant)
Function SetTargetScale(String scaleId, Float scale) Global Native            ; global per target
Function SetActorTargetScale(Actor kActor, String scaleId, Float scale) Global Native
Float Function GetTargetScale(...) / GetActorTargetScale(...) Global Native
```

A `scaleId` is the lowercase slider name for a morph target or the canonical key for a node target (`"pregnancybelly"`, `"slif_butt"`). Apply multiplies `master * target * actor`.

### `HasTarget`: ask before you write

A write returns `false` for several unrelated reasons (unchanged value, dead key, unknown key, no engine), so it cannot tell you *why* nothing happened. `HasTarget` answers the one question you need first: **would writing to this target do anything on this actor?**

```papyrus
if SLIFNG.HasTarget(kActor, "region:weight")
    SLIF_Main.inflate(kActor, "My Mod", "region:weight", 1.4)
else
    SLIF_Main.inflate(kActor, "My Mod", "slif_belly", 1.15)   ; bone fallback
endif
```

- A **canonical key** (`slif_belly`) or a **morph** (`morph:X`) is always `true`, since worst case the skeleton node drives it.
- A **region** (`region:weight`) is `true` only when this actor's profile (or an [overlay](body-profiles.md#region-overlays)) defines that section, because a region is sliders or nothing.
- Dead keys (`slif_breast01`) and unknown spellings are `false`.

Requires `SLIFNG.GetVersion() >= 3`; on an older build the function does not exist, so guard the call site if you support both.

### `DrivenBy`: does the bone actually move?

`HasTarget` answers "would a write do anything"; this answers **how**, which is a different question and the one that matters if you have something rigged to a skeleton bone.

```papyrus
String how = SLIFNG.DrivenBy(kActor, "slif_breast")
```

| Result | Meaning |
|---|---|
| `sliders` | BodySlide morphs move vertices and **no bone moves** - anything rigged to that bone (a particle emitter, an attached object) does *not* follow and needs its own compensation |
| `node` | The skeleton bone is scaled, so its children come along automatically |
| `none` | A region this profile does not define, or an unknown/dead key |

Which one you get depends on the actor's [body profile](body-profiles.md), so ask per actor, not once per game. Requires `SLIFNG.GetVersion() >= 5`.

## `SLIFNG_Settled` (mod event)

Sent when a target **stops changing** on an actor. That covers a plain write, the last step of a ramp, an unregister, and a magnitude change - every path that ends with a final value on the body.

```papyrus
RegisterForModEvent("SLIFNG_Settled", "OnSlifSettled")   ; re-register in OnPlayerLoadGame

Event OnSlifSettled(String eventName, String target, Float value, Form sender)
    if target == "slif_breast"
        ; sender has finished growing - safe to measure and act on the shape
    endIf
EndEvent
```

- `target` - `slif_breast`, `morph:PregnancyBelly`, `region:weight`
- `value` - the settled value, identical to `GetApplied(sender, target)`
- `sender` - the Actor

**It is deliberately not per step.** The ramp ticks ten times a second; an event per step would be a flood, and every listener would immediately debounce it back into exactly this. This is the moment consumers actually want: *she has finished changing, act now*.

It fires **once per target**, so an actor whose belly and breasts both settle sends two events. Filter on `target` - there is no way to ask for a subset.

Requires `SLIFNG.GetVersion() >= 5`.

## Recipe: acting on a finished shape

The case this surface exists for: a mod that attaches something to a body and must place it correctly *after* SLIF NG has finished resizing her. OLactis is the worked example - it equips an invisible armor addon carrying two particle emitters rigged to the breast bones, and the emitters have to sit on the nipple.

Three questions have to be answered from outside SLIF NG, and each maps to one call:

| Question | Answer |
|---|---|
| When is it safe to measure? | the `SLIFNG_Settled` event - never mid-ramp |
| How much has she grown? | the event's `value` (or `GetApplied`) - node-space, identical on every body |
| Did the **bone** move? | `DrivenBy` - decides whether compensation is needed at all |

```papyrus
Scriptname MyEmitters extends Quest

Armor Property EmitterArmor Auto

Event OnInit()
    Hook()
EndEvent

Event OnPlayerLoadGame()
    Hook()                       ; registrations do not survive a save
EndEvent

Function Hook()
    if Game.IsPluginInstalled("SexLab Inflation Framework.esp") && SLIFNG.GetVersion() >= 5
        RegisterForModEvent("SLIFNG_Settled", "OnSlifSettled")
    endIf
EndFunction

Event OnSlifSettled(String eventName, String target, Float value, Form sender)
    if target != "slif_breast"
        return                   ; belly, butt, sliders... not ours
    endIf
    Actor who = (sender as Actor)
    if who
        Refit(who, value)
    endIf
EndEvent

Function Refit(Actor who, Float scale)
    Float offset = BaseOffset(who)          ; your own 0-weight / 100-weight lerp

    ; Emitters hang off NPC L/R Breast03. Under NODE scaling the parent bone
    ; carries them and they are already in place; under SLIDERS only vertices
    ; move, so they would be left behind.
    if SLIFNG.DrivenBy(who, "slif_breast") == "sliders"
        offset = offset + GrowthOffset(scale)
    endIf

    ApplyEmitters(who, offset)              ; set offsets, THEN equip
EndFunction
```

Four things that example is doing deliberately:

- **Gate on `GetVersion()`, not the mod version.** It returns `0` when the DLL is absent, so one check covers "no SLIF", "old SLIF" and "SLIF NG too old".
- **Re-register in `OnPlayerLoadGame`.** Mod event registrations do not survive a save. This is the most common integration bug there is.
- **No SLIF script is a property.** Property types resolve at script *load*, so `SLIFNG Property ...` would make the whole script fail to load with SLIF absent. Global calls resolve lazily at call time - which is what makes an optional dependency possible.
- **The `DrivenBy` branch is not optional.** On a bone-driven body, adding a growth offset would double-count, because the bone already carried the attachment outward.

### Where the boundary is

**SLIF NG owns "how much", your mod owns "where".** We know slider values and node scales; we have no idea where a nipple is. That is mesh geometry - it would mean walking the deformed `NiAVObject` tree and knowing which vertices count as a nipple on each body, which is both a different domain and body-specific in a way the profile format deliberately is not.

So offset math stays in the consumer. What SLIF NG guarantees is that the three inputs above are correct, body-agnostic and correctly timed; given those, the offset is a pure function.

!!! note "Why morphs cannot just move the bone"
    It would be tempting for SLIF NG to apply a small node scale alongside morphs so attachments follow automatically. It does not, and should not: that would visibly double-scale the body to fix one consumer's attachment problem. Morphs moving vertices and not bones is inherent to how BodySlide works, not a gap to paper over.

## C++ (other SKSE plugins)

Copy [`src/API/SLIFNG_API.h`](https://github.com/crajjjj/SLIFNG/blob/main/src/API/SLIFNG_API.h) into your project - it is self-contained (only a forward declaration of `RE::Actor`) - and exchange the interface over SKSE messaging, the same handshake pattern skee itself uses:

```cpp
#include "SLIFNG_API.h"

// at kPostPostLoad or later
SLIFNG_API::InterfaceExchangeMessage msg;
SKSE::GetMessagingInterface()->Dispatch(
    SLIFNG_API::InterfaceExchangeMessage::kMessageType,
    &msg, sizeof(msg), "SLIFNG");

if (msg.query) {  // null when SLIF NG is not installed - no link-time coupling
    const float belly = msg.query->GetValue(actor, "All Mods", "slif_belly", 1.0f);
    const bool  busy  = msg.query->IsTracked(actor);
}
if (msg.query && msg.query->Version() >= 2) {   // a newer interface: ask, then cast
    auto* q2 = static_cast<SLIFNG_API::IQueryInterface2*>(msg.query);
    const bool hasWeight = q2->HasTarget(actor, "region:weight");
}
```

`IQueryInterface1` carries: `Version`, `IsTracked`, `TrackedActorCount`, `GetValue`, `GetMinValue`, `GetMaxValue`, `GetApplied`, `GetCombinedMorph`, `GetCalculationType`, `IsIncrementalInflation`. `IQueryInterface2` adds `HasTarget`. Both are **read-only by design** - mutations go through the pinned Papyrus surface, which is the compatibility contract. The pointer stays valid for the process lifetime and every call is thread-safe (the store is mutex-guarded).

Interfaces are versioned by **addition**: an existing `IQueryInterfaceN` is never edited, so a plugin built against an older header keeps working untouched. The **exchange struct never grows** - it stays one pointer, and the handshake stays an exact size match, so a dispatch from any header version is accepted by any SLIF NG. You reach a newer interface by asking the one you were handed for its version and casting: the implementation derives the whole chain by single inheritance, so the pointer is identical and `Version()` is what makes the cast sound.

Gate on `Version() >= N`, never on equality. (SLIF NG 0.3.0's header described this number as a breaking-change counter; it is a monotonic "newest interface served", so `== 1` would lock a consumer out of every later build.)

## Legacy StorageUtil mirror

One value is still written to StorageUtil for legacy readers: `slif_<morphName>` - the cross-mod direct-morph total, which Sexlab Survival reads without any API. Everything else the old framework kept in StorageUtil exists only as a one-time migration *source*; do not read or write those keys against SLIF NG.
