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

### `HasTarget` — ask before you write

A write returns `false` for several unrelated reasons (unchanged value, dead key, unknown key, no engine), so it cannot tell you *why* nothing happened. `HasTarget` answers the one question you need first: **would writing to this target do anything on this actor?**

```papyrus
if SLIFNG.HasTarget(kActor, "region:weight")
    SLIF_Main.inflate(kActor, "My Mod", "region:weight", 1.4)
else
    SLIF_Main.inflate(kActor, "My Mod", "slif_belly", 1.15)   ; bone fallback
endif
```

- A **canonical key** (`slif_belly`) or a **morph** (`morph:X`) is always `true` — worst case the skeleton node drives it.
- A **region** (`region:weight`) is `true` only when this actor's profile (or an [overlay](body-profiles.md#region-overlays)) defines that section, because a region is sliders or nothing.
- Dead keys (`slif_breast01`) and unknown spellings are `false`.

Requires `SLIFNG.GetVersion() >= 3`; on an older build the function does not exist, so guard the call site if you support both.

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
if (msg.query2) {  // query version 2+; null on an older SLIF NG
    const bool hasWeight = msg.query2->HasTarget(actor, "region:weight");
}
```

`IQueryInterface1` carries: `Version`, `IsTracked`, `TrackedActorCount`, `GetValue`, `GetMinValue`, `GetMaxValue`, `GetApplied`, `GetCombinedMorph`, `GetCalculationType`, `IsIncrementalInflation`. `IQueryInterface2` adds `HasTarget`. Both are **read-only by design** - mutations go through the pinned Papyrus surface, which is the compatibility contract. The pointer stays valid for the process lifetime and every call is thread-safe (the store is mutex-guarded).

Interfaces are versioned by **addition**: an existing `IQueryInterfaceN` is never edited, so a plugin built against an older header keeps working untouched. The exchange struct grows with each one, and SLIF NG treats its size as a lower bound - it fills `query` for everyone and only writes `query2` when your dispatch was large enough to hold it. Always null-check the pointer you are about to use.

## Legacy StorageUtil mirror

One value is still written to StorageUtil for legacy readers: `slif_<morphName>` - the cross-mod direct-morph total, which Sexlab Survival reads without any API. Everything else the old framework kept in StorageUtil exists only as a one-time migration *source*; do not read or write those keys against SLIF NG.
