# C++ API (SKSE plugins)

The C++ query interface is the native mirror of the [Query API](query-api.md): the same reads, answered by the same core inside `BodyInflationNG.dll`, so C++ and Papyrus can never disagree. It is **read-only**. Writes go through the Papyrus surface (`SLIF_Main.inflate` / `SLIF_Morph.morph`, see the [Write API](write-api.md)).

## Quick start

1. Copy **`SLIFNG_API.h`** into your project, from the integration kit (`cpp/` in `SLIFNG-API-<version>+.zip`) or from [`src/API/`](https://github.com/crajjjj/SLIFNG/blob/main/src/API/SLIFNG_API.h) in the repository. It needs nothing but `<cstdint>`, and there is no library to link.
2. Once every SKSE DLL has loaded, look up the `SLIFNG_GetQueryInterface` export and keep the pointer it returns:

```cpp
#include "SLIFNG_API.h"

SLIFNG_API::IQueryInterface1* g_slif = nullptr;

void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
{
    if (a_msg->type != SKSE::MessagingInterface::kPostLoad) {
        return;
    }
    if (auto* dll = GetModuleHandleA(SLIFNG_API::kModuleName)) {
        auto get = reinterpret_cast<SLIFNG_API::GetQueryInterfaceFn>(
            GetProcAddress(dll, SLIFNG_API::kGetQueryInterfaceExport));
        if (get) {
            g_slif = get();
        }
    }
    // g_slif stays nullptr when SLIF NG is not installed
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
    return true;
}
```

- **When:** `kPostLoad` or any later message (`kPostPostLoad` and `kDataLoaded` work too). Not from your own `SKSEPlugin_Load`: SKSE loads DLLs in file-name order, so if yours sorts first, SLIF NG is not loaded yet.
- **Absent:** `GetModuleHandleA` returns null when SLIF NG is not installed or failed to load. Nothing else happens.
- **Lifetime:** call once and keep the pointer. It is valid for the whole process, and there is nothing to release.

To check it worked, look for this line in `SLIFNG.log`, in the SKSE log folder next to your own plugin's log:

```
[API] query interface v2 handed out through SLIFNG_GetQueryInterface
```

The export exists since SLIF NG 0.4.18. For older builds, see [SLIF NG 0.4.17 and older](#slif-ng-0417-and-older).

## Reading values

| Call | Returns |
|---|---|
| `GetValue(actor, mod, target, default)` | That mod's stored row for the target, or the aggregate for `"All Mods"` |
| `GetMinValue` / `GetMaxValue(actor, mod, target, default)` | The stored bounds behind that row |
| `GetApplied(actor, target)` | The value currently shown for the target, without the user's magnitude |
| `GetCombinedMorph(actor, slider)` | Cross-mod total of direct morph writes to one slider (the legacy `slif_<slider>` number) |
| `IsTracked(actor)` / `TrackedActorCount()` | Whether SLIF NG holds anything for this actor / how many actors it holds |
| `GetCalculationType()` | SLIF's `calculation_type`: 0 Top X, 1 Highest wins, 2 Subtract and add one, 3 Square root, 4 Average, 5 Additive |
| `IsIncrementalInflation()` | Whether ramped inflation is switched on |
| `Version()` | The newest interface this build serves, see [Newer calls](#newer-calls) |

The semantics are the Papyrus ones ([Query API](query-api.md#shared-semantics)). Two matter most:

- **An absent row returns *your* default**, never an invented neutral. Use `1.0f` for node targets and `0.0f` for morphs, or a sentinel such as `-1.0f` to tell "nothing stored" from "stored at neutral".
- **Targets** can be spelled `"slif_belly"`, `"NPC Belly"`, `"morph:PregnancyBelly"` or `"region:weight"`. The first two are the same target. Spelling and mod names are case-insensitive.

### A specific mod's row

Read with the name the mod writes under. The row is raw: no clamping, no user magnitude.

| Mod | `modName` | Targets it writes |
|---|---|---|
| Fill Her Up (Baka edition) | `"Fill Her Up"` | `"NPC Belly"` (node scaling) **or** `"morph:PregnancyBelly"` (its default `InflateMorph`, plus up to three more sliders), depending on its MCM |
| Beeing Female NG | `"Beeing Female"` | `slif_*` node keys and `morph:<slider>` |
| Sexlab Survival | `"Sexlab Survival"` | `"slif_belly"` |

For any other mod, `SLIFNG.log` names both strings on every write it receives:

```
[API] Inflate(00000014 'Anna', mod='Sexlab Survival', key='slif_belly', value=1.2, ...)
```

Fill Her Up drives one of two targets, so read both, with a sentinel default:

```cpp
const float node  = g_slif->GetValue(actor, "Fill Her Up", "NPC Belly", -1.0f);             // scale, 1.0 = neutral
const float morph = g_slif->GetValue(actor, "Fill Her Up", "morph:PregnancyBelly", -1.0f);  // slider, 0.0 = neutral
const bool fhuDrivesBone   = node >= 0.0f;
const bool fhuDrivesSlider = morph >= 0.0f;
```

### The aggregate

To ask "how big does this actor look, counting every mod", read the pseudo-mod `"All Mods"`:

```cpp
const float belly = g_slif->GetValue(actor, "All Mods", "NPC Belly", 1.0f);  // with the user's magnitude
const float raw   = g_slif->GetApplied(actor, "NPC Belly");                 // without it
```

`GetValue` on `"All Mods"` includes the user's MCM magnitude, so it agrees with what the player sees. `GetApplied` is the same total without it. Mid-ramp both are the in-flight value ([which getter do I want](query-api.md#which-getter-do-i-want)).

## Newer calls

Interfaces grow by addition: `IQueryInterface2` derives from `IQueryInterface1` and adds calls, and so on. Ask the pointer for its version, then cast:

```cpp
if (g_slif && g_slif->Version() >= 2) {
    auto* q2 = static_cast<SLIFNG_API::IQueryInterface2*>(g_slif);
    const bool hasWeight = q2->HasTarget(actor, "region:weight");
}
```

| `Version()` | Since | Adds |
|---|---|---|
| 1 | SLIF NG 0.3.0 | Everything under [Reading values](#reading-values) |
| 2 | SLIF NG 0.4.0 | `HasTarget(actor, target)`: would a write to this target do anything on this actor? ([ask before you write](query-api.md#hastarget-ask-before-you-write)) |

Gate on `>=`, never on `==`: the number only grows, and `Version() == 1` would lock you out of every later build.

## The `SLIFNG_Settled` event

[`SLIFNG_Settled`](query-api.md#slifng_settled-mod-event) fires once per target when that target stops changing on an actor. It is an ordinary `ModCallbackEvent`, so a C++ plugin can sink it directly:

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

Filter on `strArg`. Sent since SLIF NG 0.4.1.

## Threading

- Every call is thread-safe and cheap: a locked lookup, no geometry work, nothing posted to the main thread.
- The `RE::Actor*` you pass must be valid at that moment. Resolve it from a handle each time instead of caching a raw pointer.

## Not in C++

- **Enumeration** ("which mods drive this target", "which targets does this actor have", "which actors are tracked") and **`DrivenBy`** are Papyrus only (`SLIFNG.GetModsDriving`, `GetNodeTargets`, `GetMorphTargets`, `GetTrackedActors`, `DrivenBy`). From C++ you need the mod name and target up front.
- **Writes**, by design: send them through the Papyrus surface, so every consumer shares one entry point.

Need one of these natively? Open an issue with the shape you want. A new call costs nothing in compatibility.

## Troubleshooting

| Symptom | Cause |
|---|---|
| `GetModuleHandleA` returns null, `SLIFNG.log` exists | Called too early: from your own `SKSEPlugin_Load`, with your DLL sorting before `BodyInflationNG.dll`. Move it to `kPostLoad` or later |
| `GetModuleHandleA` returns null, no `SLIFNG.log` | SLIF NG's DLL is not running (plain SLIF SE, or it failed to load) |
| `GetProcAddress` returns null | SLIF NG 0.4.17 or older, see below |
| `GetValue` always returns your default | The mod name or target is spelled differently from what the writer sent (check the `[API] Inflate(...)` lines), the actor is not tracked, or the key is dead (`slif_breast01` style) |
| Crash calling `HasTarget` | Cast to `IQueryInterface2` without checking `Version() >= 2` |
| Value differs from Papyrus `GetValue` | It cannot, both run the same core. A mod's own row and `"All Mods"` differ by design |

## SLIF NG 0.4.17 and older

These builds have no export. They hand out the same interface over SKSE messaging instead: dispatch an `InterfaceExchangeMessage` to `"SLIFNG"` at `kPostPostLoad` or later, and if `msg.query` stays null, dispatch it again with no receiver (the header shows the code). SKSE does not always deliver that message, which is why the export replaced it, so on 0.4.18 and later use the export.
