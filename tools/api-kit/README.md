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

**Ask before you write** when the target might not exist on this body:

```papyrus
if SLIFNG.HasTarget(akActor, "region:weight")
	SLIF_Main.inflate(akActor, "My Mod", "region:weight", 1.4)
else
	SLIF_Main.inflate(akActor, "My Mod", "slif_belly", 1.15)   ; bone fallback
endif
```

A write returns `false` for several unrelated reasons, so it can't tell you *why* nothing
happened; `HasTarget` can. Canonical keys and morphs are always true (worst case the skeleton
node drives them); a `region:` key is true only when the actor's body profile defines it.
Needs `SLIFNG.GetVersion() >= 3`, and `msg.query2->HasTarget(...)` is the C++ equivalent.

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
if (msg.query2) {                      // null on a SLIF NG older than this header
    const bool hasWeight = msg.query2->HasTarget(actor, "region:weight");
}
```

The interface is **read-only by design** - mutations go through the Papyrus surface above,
which is the one pinned, SLIF-compatible entry point. The returned pointer is valid for the
process lifetime and every call is thread-safe. An absent row returns **your** default, never an
invented neutral, so you can always tell "nothing tracked" from "tracked at neutral".

Interfaces are versioned by **addition** - an existing `IQueryInterfaceN` is never edited - so a
plugin built against an older header keeps working untouched. The exchange struct grows with
each one, and SLIF NG treats its size as a lower bound: it fills `query` for every consumer and
writes `query2` only when your dispatch was large enough to hold it. Null-check the pointer you
are about to use; `Version()` reports the newest interface this build serves.

## Target spellings

Everywhere a target key is taken, all three of these are accepted and resolve to the same thing:

| Spelling | Example | Notes |
|---|---|---|
| `slif_*` key | `slif_belly` | the original SLIF vocabulary |
| raw skeleton node | `NPC Belly` | what Fill Her Up sends |
| `morph:<slider>` | `morph:PregnancyBelly` | a BodySlide slider, read side |
| `region:<name>` | `region:weight` | SLIF NG only - resolved through the actor's body profile |

A **region** is a semantic key: your mod says *how much*, and the actor's body profile decides
which sliders that means - so one call works on 3BA, UBE and BHUNP without your mod knowing a
single slider name. If the region you need isn't defined yet, ship a small overlay file
(`Data/SLIFNG/Bodies/Regions/YourMod-<body>.ini`) rather than a whole profile, which would
override the user's body choice. Format and rules: the `README.txt` in that folder, and
<https://crajjjj.github.io/SLIFNG/authors/body-profiles/#region-overlays>.

Mod names compare case-insensitively, and `"All Mods"` reads the aggregate.

## Where the documentation is

- Author guide: <https://crajjjj.github.io/SLIFNG/authors/>
- Player guide and MCM reference: <https://crajjjj.github.io/SLIFNG/>
- **`CONTRACT.md`** in the repo is the authoritative description of the frozen SLIF surface -
  what was pinned from the reference implementation's bytecode, and why.
- Source and issues: <https://github.com/crajjjj/SLIFNG>
