# SLIF NG integration kit

Everything another mod needs to talk to SLIF NG, and nothing else. Shipped as
`SLIFNG-API-<version>+.zip` (built with `python tools/pack-api-kit.py`); the same files live in
the SLIF NG repo, so you can also copy them from there.

The **`+`** means what it looks like: the kit describes that SLIF NG version *and later*. The
Papyrus write surface is **frozen** by the compatibility contract and the query interface only
grows, so a kit stays valid for every newer release. `VERSIONS.txt` carries the numbers to gate
an optional integration on.

```
cpp/SLIFNG_API.h        C++ inter-plugin query API (SKSE plugins)
papyrus/SLIF_Main.psc   node inflation - the pinned SLIF surface
papyrus/SLIF_Morph.psc  body-slider morphs - the pinned SLIF surface
papyrus/SLIFNG.psc      SLIF NG's own native surface (writes + enumeration)
VERSIONS.txt            the versions to gate on
```

**You do not need SLIF NG installed to build against these**, and nothing here creates a hard
dependency - that is the point of shipping them separately from the mod archive.

## Which surface do I use?

**Write through `SLIF_Main` / `SLIF_Morph`.** These are the original SLIF signatures, frozen:
same names, order, types and count. Code written against them runs on plain SLIF SE **and** on
SLIF NG, so an integration costs you nothing in compatibility.

```papyrus
; node inflation: value is a multiplicative scale, 1.0 = neutral
SLIF_Main.inflate(akActor, "MyMod", "slif_belly", 1.35)

; body sliders: value is a BodySlide slider value, 0.0 = neutral
SLIF_Morph.morph(akActor, "MyMod", "PregnancyBelly", 0.8)

; clean up when your effect ends
SLIF_Main.unregisterNode(akActor, "slif_belly", "MyMod")
SLIF_Main.unregisterActor(akActor, "MyMod")
```

Pass **your own mod name** as `modName` - that is the ledger key SLIF folds per-mod
contributions under, and it is what lets several mods inflate the same actor without fighting.

**Read through `SLIFNG`** (Papyrus) or **`SLIFNG_API.h`** (C++). Reading back what is applied,
enumerating tracked actors, and querying another mod's row are SLIF NG additions - plain SLIF
has no equivalent, so gate these on the version.

## Detection and version gating

```papyrus
; is any SLIF present?
bool hasSlif = Game.IsPluginInstalled("SexLab Inflation Framework.esp")

; is it SLIF NG, and new enough for the call you want?
; returns 0 when SLIF NG's DLL is absent (plain SLIF, or nothing installed)
if SLIFNG.GetVersion() >= 2
	; the enumeration / read surface is available
endif
```

SLIF NG ships the plugin under the **same name** as SLIF (`SexLab Inflation Framework.esp`) on
purpose, so existing consumers keep working unmodified. That also means the plugin name alone
cannot tell the two apart - `SLIFNG.GetVersion()` is what distinguishes them.

## C++ (SKSE plugins)

`cpp/SLIFNG_API.h` is a **reference, not a library**: no `.lib`, no import library, and it
depends on nothing but a forward declaration of `RE::Actor`, so copying it into your project
adds zero build-time dependency. Fetch the interface over SKSE messaging at
`kPostPostLoad` or later:

```cpp
SLIFNG_API::InterfaceExchangeMessage msg;
SKSE::GetMessagingInterface()->Dispatch(
    SLIFNG_API::InterfaceExchangeMessage::kMessageType, &msg, sizeof(msg), "SLIFNG");
if (msg.query) {                       // null = SLIF NG not installed
    const float belly = msg.query->GetValue(actor, "All Mods", "slif_belly", 1.0f);
}
```

The interface is **read-only by design** - mutations go through the Papyrus surface above,
which is the one pinned, SLIF-compatible entry point. The returned pointer is valid for the
process lifetime and every call is thread-safe. An absent row returns **your** default, never an
invented neutral, so you can always tell "nothing tracked" from "tracked at neutral".

Feature-detect with `IQueryInterface1::Version()`. The interface is versioned by addition:
breaking layout changes would arrive as `IQueryInterface2` alongside this one, never by editing
it.

## Target spellings

Everywhere a target key is taken, all three of these are accepted and resolve to the same thing:

| Spelling | Example | Notes |
|---|---|---|
| `slif_*` key | `slif_belly` | the original SLIF vocabulary |
| raw skeleton node | `NPC Belly` | what Fill Her Up sends |
| `morph:<slider>` | `morph:PregnancyBelly` | a BodySlide slider, read side |
| `region:<name>` | `region:weight` | SLIF NG only - resolved through the actor's body profile |

Mod names compare case-insensitively, and `"All Mods"` reads the aggregate.

## Where the documentation is

- Author guide: <https://crajjjj.github.io/SLIFNG/authors/>
- Player guide and MCM reference: <https://crajjjj.github.io/SLIFNG/>
- **`CONTRACT.md`** in the repo is the authoritative description of the frozen SLIF surface -
  what was pinned from the reference implementation's bytecode, and why.
- Source and issues: <https://github.com/crajjjj/SLIFNG>
