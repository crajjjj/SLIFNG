# C++ API (SKSE plugins)

The C++ query interface is the native mirror of the [Query API](query-api.md): the same read surface, served from the same core inside `BodyInflationNG.dll`, so a C++ caller and a Papyrus caller can never get different answers. It is **read-only by design**. Writes go through the pinned Papyrus surface (`SLIF_Main.inflate` / `SLIF_Morph.morph`, see the [Write API](write-api.md)), because that is the one entry point every SLIF consumer shares.

There is no library to link, no import `.lib`, and nothing to include from SLIF NG's sources. The whole integration is: copy one header, send one SKSE message once, keep the pointer it hands back.

## What you need

`SLIFNG_API.h`, from either place:

- `cpp/SLIFNG_API.h` inside the integration kit, `SLIFNG-API-<version>+.zip` (the `+` means that version and later)
- [`src/API/SLIFNG_API.h`](https://github.com/crajjjj/SLIFNG/blob/main/src/API/SLIFNG_API.h) in the repository

It depends on nothing but `<cstdint>` and a forward declaration of `RE::Actor`, so it drops into any CommonLibSSE or CommonLibSSE-NG project without touching your build. Nothing in it creates a dependency on SLIF NG being installed: when it is absent you get a null pointer and nothing else happens.

## The handshake

SLIF NG hands out its interface over SKSE plugin messaging, the same exchange pattern RaceMenu uses for skee. You dispatch an `InterfaceExchangeMessage` addressed to `"SLIFNG"`, and SLIF NG fills in the `query` pointer before the call returns.

```cpp
#include "SLIFNG_API.h"

namespace
{
    SLIFNG_API::IQueryInterface1* g_slif = nullptr;

    void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
    {
        if (a_msg->type != SKSE::MessagingInterface::kPostPostLoad) {
            return;
        }
        SLIFNG_API::InterfaceExchangeMessage msg;
        SKSE::GetMessagingInterface()->Dispatch(
            SLIFNG_API::InterfaceExchangeMessage::kMessageType,
            &msg, sizeof(msg), "SLIFNG");
        g_slif = msg.query;  // stays nullptr when SLIF NG is not installed
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
    return true;
}
```

Three details carry the whole thing:

- **When.** Dispatch from your SKSE message handler at `kPostPostLoad` or `kDataLoaded`, not from your own `SKSEPlugin_Load`. SKSE loads plugin DLLs one after another in file-name order and SLIF NG registers its listener while it loads, so inside your own load function it may simply not be there yet. From `kPostLoad` onward every plugin has loaded.
- **Who.** `"SLIFNG"` is the plugin's declared SKSE name, the one in its version data. It is not the DLL file name (`BodyInflationNG.dll`) and not the ESP.
- **Keep it.** Do the exchange once and store the pointer. It points at a static object inside the DLL and is valid for the rest of the process. There is nothing to release.

!!! tip "Did it work?"
    SLIF NG logs every exchange it answers. With the handshake in place, `SLIFNG.log` (in the SKSE log folder, next to your own plugin's log) contains:

    ```
    [API] query interface v2 handed to 'YourPluginName'
    ```

    where the name is your plugin's declared name. If `msg.query` is null with SLIF NG installed **and** that line is missing, the message never arrived: almost always a dispatch from too early, or a receiver string other than `"SLIFNG"`.

## Reading values

Everything on `IQueryInterface1` (query interface 1, SLIF NG 0.3.0 and later):

| Call | Returns |
|---|---|
| `Version()` | The newest `IQueryInterface<N>` this build serves (see [Versions](#versions-and-newer-calls)) |
| `IsTracked(actor)` | Whether SLIF NG holds anything at all for this actor |
| `TrackedActorCount()` | How many actors it holds |
| `GetValue(actor, mod, target, default)` | That mod's stored row for the target, or the aggregate for `"All Mods"`. Absent: `default` |
| `GetMinValue` / `GetMaxValue(actor, mod, target, default)` | The stored bounds behind that row. Absent: `default` |
| `GetApplied(actor, target)` | The value currently shown for the target, without the user's magnitude |
| `GetCombinedMorph(actor, slider)` | Cross-mod total of direct morph writes to one slider (the legacy `slif_<slider>` number) |
| `GetCalculationType()` | SLIF's `calculation_type` numbering: 0 Top X, 1 Highest wins, 2 Subtract and add one, 3 Square root, 4 Average, 5 Additive |
| `IsIncrementalInflation()` | Whether ramped inflation is switched on |

The semantics are exactly the Papyrus ones, and the [Query API page](query-api.md#shared-semantics) spells them out. The two that matter most from C++:

- **An absent row returns *your* default**, never an invented neutral. Pick the neutral your maths wants (`1.0f` for node targets, `0.0f` for morphs), or a sentinel such as `-1.0f` when you need to tell "nothing stored" from "stored at neutral".
- **A `target` is spelled the way the writer spelled it, or canonically.** `"slif_belly"`, the raw node `"NPC Belly"`, `"morph:PregnancyBelly"` and `"region:weight"` are all accepted, and the first two resolve to the same ledger target. Spelling is case-insensitive.

### A specific mod's row

Every mod that drives SLIF passes its own name as `modName` when it writes, and that string is the key you read with. `GetValue(actor, modName, target, default)` returns what that mod last stored for that target, raw: no clamping, no user magnitude. Mod names compare case-insensitively.

| Mod | `modName` it writes under | Targets it writes |
|---|---|---|
| Fill Her Up (Baka edition) | `"Fill Her Up"` | `"NPC Belly"` (node scaling) **or** `"morph:PregnancyBelly"` (its default `InflateMorph`, with up to three more sliders from its MCM). Which one depends on its own MCM |
| Beeing Female NG | `"Beeing Female"` | `slif_*` node keys and `morph:<slider>` |
| Sexlab Survival | `"Sexlab Survival"` | `"slif_belly"` |

Any other mod's string is one grep away: SLIF NG logs every write it receives as

```
[API] Inflate(00000014 'Anna', mod='Sexlab Survival', key='slif_belly', value=1.2, ...)
```

so `SLIFNG.log` from a session where the mod was active tells you the exact `mod=` and `key=` to read.

Fill Her Up is the worked example. It drives one of two targets depending on its MCM, so read both, and use a sentinel default so an absent row is distinguishable:

```cpp
// node path: a scale multiplier, 1.0 = neutral
// morph path: a BodySlide slider value, 0.0 = neutral (FHU sends its internal inflation / 10)
if (g_slif) {
    const float node  = g_slif->GetValue(actor, "Fill Her Up", "NPC Belly", -1.0f);
    const float morph = g_slif->GetValue(actor, "Fill Her Up", "morph:PregnancyBelly", -1.0f);

    const bool fhuDrivesBone   = node >= 0.0f;   // FHU is scaling the belly bone by `node`
    const bool fhuDrivesSlider = morph >= 0.0f;  // FHU is pushing PregnancyBelly by `morph`
}
```

### The aggregate

A mod's row answers "what did that mod ask for". If the question is "how big does this actor look, counting every mod", read the aggregate instead:

```cpp
const float belly = g_slif->GetValue(actor, "All Mods", "NPC Belly", 1.0f);  // with the user's magnitude
const float raw   = g_slif->GetApplied(actor, "NPC Belly");                 // without it
```

`"All Mods"` is a pseudo-mod meaning "the folded total", not a mod literally called that. `GetValue` on it includes the user's MCM magnitude, which is the number that agrees with what the player sees; `GetApplied` is the same fold without it. Mid-ramp both are the in-flight value. The difference, and when each is right, is on the [Query API page](query-api.md#which-getter-do-i-want).

## Versions and newer calls

Interfaces are versioned **by addition**. `IQueryInterface1` is never edited; a new call lands on `IQueryInterface2`, which derives from it, and so on. `Version()` returns the newest interface the running build serves, and `kQueryVersion` in your copy of the header is the newest one that copy knows about.

| `Version()` | Since | Adds |
|---|---|---|
| 1 | SLIF NG 0.3.0 | Everything in the table above |
| 2 | SLIF NG 0.4.0 | `HasTarget(actor, target)`: would a write to this target do anything on this actor? (the [ask-before-you-write](query-api.md#hastarget-ask-before-you-write) check) |

To reach a newer call, ask the pointer you were handed for its version, then cast:

```cpp
if (g_slif && g_slif->Version() >= 2) {
    auto* q2 = static_cast<SLIFNG_API::IQueryInterface2*>(g_slif);
    const bool hasWeight = q2->HasTarget(actor, "region:weight");
}
```

The cast is sound because the implementation derives the whole chain by single inheritance, so the pointer value is identical and `Version()` is the proof. Two rules:

- **Gate on `>=`, never on `==`.** The number is a monotonic "newest served", not a breaking-change counter. A consumer that wrote `Version() == 1` locks itself out of every later build.
- **Never add a field to `InterfaceExchangeMessage`.** The exchange struct is frozen at one pointer and the handshake is an exact size match (`sizeof(msg)`), so a dispatch from any header version is accepted by any SLIF NG. A bigger struct would be rejected, and you would read SLIF NG as absent.

## What is not in C++

- **Enumeration.** "Which mods drive this target", "which targets does this actor have" and "which actors are tracked" exist only in Papyrus (`SLIFNG.GetModsDriving`, `GetNodeTargets`, `GetMorphTargets`, `GetTrackedActors`). From C++ you need to know the mod name and target up front, as above. (`GetContribution` is not missing: for a named mod, `GetValue` returns the same raw row.)
- **`DrivenBy`.** Whether a target moves a bone or sliders on this actor is Papyrus-only for now.
- **Writes.** By design. Send them through the Papyrus surface from your own script side, so the frozen SLIF contract stays the single entry point for every consumer.

If you need one of these natively, open an issue describing the shape you want. The interface grows by adding a version, so it costs nothing in compatibility.

### The `SLIFNG_Settled` event from C++

The [`SLIFNG_Settled`](query-api.md#slifng_settled-mod-event) mod event is an ordinary SKSE `ModCallbackEvent`, so a C++ plugin can sink it directly instead of relaying through a script:

```cpp
class SettledSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
{
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
        RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
    {
        if (a_event && a_event->eventName == "SLIFNG_Settled") {
            auto* actor = a_event->sender ? a_event->sender->As<RE::Actor>() : nullptr;
            // a_event->strArg = the target ("slif_breast", "morph:PregnancyBelly", ...)
            // a_event->numArg = the settled value, identical to GetApplied(actor, target)
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

// at kDataLoaded:
static SettledSink g_settledSink;
SKSE::GetModCallbackEventSource()->AddEventSink(&g_settledSink);
```

It fires once per target when that target stops changing on an actor, so filter on `strArg`. It exists since SLIF NG 0.4.1 (Papyrus API version 5); an older build simply never sends it.

## Threading and lifetime

- Every call is thread-safe. The store behind it is mutex-guarded and the Papyrus natives share it, so you can call from any thread.
- The `RE::Actor*` you pass must be valid at the moment of the call. Resolve it from a handle each time rather than caching a raw pointer across frames.
- Calls are cheap lookups, not geometry work. Nothing is posted to the main thread and nothing touches skee.

## Troubleshooting

| Symptom | Cause |
|---|---|
| `msg.query` is null, SLIF NG installed, no `handed to` line in `SLIFNG.log` | Dispatched before SLIF NG loaded (from your own `SKSEPlugin_Load`), or the receiver is not `"SLIFNG"`, or `sizeof(msg)` is not one pointer because the struct was extended |
| `msg.query` is null and `SLIFNG.log` does not exist | SLIF NG's DLL is not running (plain SLIF SE, or the plugin failed to load) |
| `GetValue` always returns your default | The mod name or target is spelled differently from what the writer sent (check the `[API] Inflate(... mod=..., key=...)` lines), or the actor is not tracked, or the key is dead (`slif_breast01` style) |
| Crash calling `HasTarget` | Cast to `IQueryInterface2` without checking `Version() >= 2` on an older SLIF NG |
| Value differs from the Papyrus `GetValue` | It cannot, both run the same core. Compare like with like: a mod's own row and `"All Mods"` differ by design |
