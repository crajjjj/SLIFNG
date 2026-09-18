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
```

`IQueryInterface1` carries: `Version`, `IsTracked`, `TrackedActorCount`, `GetValue`, `GetMinValue`, `GetMaxValue`, `GetApplied`, `GetCombinedMorph`, `GetCalculationType`, `IsIncrementalInflation`. It is **read-only by design** - mutations go through the pinned Papyrus surface, which is the compatibility contract. The pointer stays valid for the process lifetime and every call is thread-safe (the store is mutex-guarded); breaking changes would ship as an `IQueryInterface2` beside it, never by editing v1.

## Legacy StorageUtil mirror

One value is still written to StorageUtil for legacy readers: `slif_<morphName>` - the cross-mod direct-morph total, which Sexlab Survival reads without any API. Everything else the old framework kept in StorageUtil exists only as a one-time migration *source*; do not read or write those keys against SLIF NG.
