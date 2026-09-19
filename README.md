# SLIF NG

A modern, zero-configuration replacement for **SexLab Inflation Framework SE**.

You install it *instead of* SLIF. Every mod that talks to SLIF today - Beeing
Female NG, Fill Her Up, Sexlab Survival, Estrus Chaurus and its Spider addon,
Devious Devices, Devious Interests, Milk Mod Economy - keeps working without
any patch, and a save that ran the old SLIF migrates by itself on first load.
No JSON to edit, no nine pages of sliders to understand: pick your body in the
installer and play.

Under the hood the Papyrus framework is gone. A native SKSE plugin
(`SLIFNG.dll`, one DLL for SE / AE / VR) keeps every mod's inflation values in
the co-save and applies them through RaceMenu, while thin script shims keep
the exact API old SLIF exposed - same function names, same arguments, same
math.

## Why replace SLIF at all

The old framework is ~90 public functions and ~11,700 lines of Papyrus, but a
survey of its actual consumers (decompiled bytecode, not guesswork - see
[CONTRACT.md](CONTRACT.md)) found that installed mods use only a small
fraction of it. What players actually meet is the other side of it: a framework
that ships inert (all 270 sliders in its three body tables sit at 0%, so a stock
install scales bones and drives no BodySlide morph at all), a nine-page MCM of
body-specific JSON editing, and silent failure
when any of it is misconfigured. SLIF NG implements exactly the surface real
mods call, natively, and diagnoses instead of failing silently.

## What changed from old SLIF

| | Old SLIF SE 1.2.2 | SLIF NG |
|---|---|---|
| **Engine** | ~11,700 lines of Papyrus, per-step body rebuilds, no unchanged-value skip | Native C++ (SKSE), one coalesced apply per actor, unchanged values skipped |
| **Setup** | Nine MCM pages; body morphs need hand-edited JSON and ship disabled | Zero configuration: pick your body once in the installer |
| **The math** | Six calculation types, Top X default; clamp + multiplier per mod | **The same formulas, kept on purpose** - they were the field-tested part |
| **Body support** | One global body config for the whole game | Per-actor profiles: UBE characters auto-detected by race, everyone else uses your installer choice; verified slider names |
| **Morphs vs nodes** | Node scaling only, unless you configure morphs yourself | Same default (node scaling), but the installer can switch belly/breasts to real BodySlide morphs |
| **Several mods, one slider** | Morph contributions always stack, even under "highest wins" | The calculation type applies to sliders the same way it applies to nodes |
| **Incremental inflation** | A Papyrus drain loop, per-step rebuilds; off by default | Native ramp, off the script engine entirely; on by default, with an MCM speed multiplier that retimes growth already in progress |
| **Old-save migration** | n/a | Automatic on first load; your characters keep their shape |
| **Diagnostics** | Silent when something is wrong | Actor page in the MCM: who inflates what, what it becomes on your body, what RaceMenu actually shows; everything logged |
| **State in your save** | Hundreds of StorageUtil keys in the Papyrus save | One compact native co-save record; self-heals on every load |
| **For mod authors** | Write-only unless you dig | Query API in Papyrus and C++, plus an event when a body finishes changing (see below) |
| **Semantic regions** | n/a | A mod can inflate "weight" or "muscle" without naming a single slider; the body profile decides what that means per body |
| **Load order** | A full plugin slot | ESL-flagged: no slot at all |
| **Gone** | Grow/shrink/absorb spells, actor scanner, scrotum timer, presets JSON API, 17 translations, ~70 functions nothing calls | Deliberately dropped; an unknown call logs one loud, searchable line instead of failing quietly |

The compatibility rules - including the handful of deliberate behaviour
differences and the bug-for-bug quirks that were kept (dead Estrus keys, the
event argument-order swap) - are all written down in
[CONTRACT.md](CONTRACT.md).

## Requirements

- SKSE64, SkyUI, RaceMenu (its skee plugin does the actual body changes)
  - **RaceMenu 0.4.19+ (AE build): everything works.**
  - **RaceMenu 0.4.16 (Skyrim SE 1.5.97): morphs work, bone scaling does not.** Its skee reports
    NiTransform v2, whose vtable differs from v3, so SLIF NG refuses that one interface rather
    than call the wrong slots. Belly and breasts are morph-driven and behave normally; targets
    that fall back to bones (butt, scrotum, and any key your body profile does not map) will not
    move. The log says so explicitly.
- XPMSSE (the standard skeleton nodes)
- PapyrusUtil (present in every SexLab load order; used to read an old SLIF
  save during migration)

## Installing

1. **Uninstall / disable SexLab Inflation Framework** (and its patches).
   SLIF NG replaces it file for file.
2. Install SLIF NG with the FOMOD and answer one question: **which body did
   you build in BodySlide** (CBBE 3BA, generic CBBE, BHUNP, or "none - node
   scaling", which behaves exactly like old SLIF and works with any body).
   UBE support installs automatically and matches by race.
3. Load your game. If the save ran old SLIF, the import runs by itself and
   reports how many values it carried over. Done.

The plugin is ESL-flagged, so it costs no load-order slot.

Settings live in one MCM page (calculation type, incremental inflation and
its speed, overall magnitude); the second page is per-actor diagnostics.

## For mod authors

The write API is old SLIF's, unchanged - `SLIF_Main.inflate`,
`SLIF_Morph.morph`, the mod events, all pinned from 1.2.2 bytecode. If your
mod worked against SLIF, it works here.

New on top of it:

- **A read API** for asking what the framework holds. *Papyrus*: `SLIFNG.psc` -
  `IsTracked`, `GetTrackedActors`, `GetNodeTargets`, `GetMorphTargets`,
  `GetModsDriving`, plus the value getters (`GetValue`, `GetApplied`,
  `GetContribution`, `GetCombinedMorph`). *C++ (SKSE plugins)*: copy
  [src/API/SLIFNG_API.h](src/API/SLIFNG_API.h) into your project and exchange
  the interface over SKSE messaging (same handshake pattern as skee).
  Read-only by design; both mirrors answer from the same core, so they can
  never disagree.
- **`SLIFNG_Settled`**, a mod event fired when a target *stops* changing -
  deliberately not per step. The signal you need if you place something on a
  body and must wait for it to finish resizing first.
- **`HasTarget` / `DrivenBy`** - would this write do anything on this actor,
  and does it move the skeleton bone or only vertices? The second matters if
  you rig anything to a bone, because morphs move no bones at all.
- **Semantic regions** - send `region:weight` and let the actor's body profile
  decide which sliders that means, so one call is correct on 3BA, UBE and
  BHUNP alike. Ship a small overlay INI if a body does not define the region
  you need; it adds to the user's profile instead of replacing it.

Every release attaches **`SLIFNG-API-<version>+.zip`**: the C++ header, the
three `.psc` a consumer compiles against, sample region overlays, and a
`VERSIONS.txt` of the numbers to gate on. You do not need SLIF NG installed to
build against it, and it creates no hard dependency.

## Documentation site

Full documentation - player guide, MCM reference, the developer API, the
aggregation math - lives at **https://crajjjj.github.io/SLIFNG/** (built from
[docs/](docs/)).

## Documents

- **[CONTRACT.md](CONTRACT.md)** - the pinned compatibility contract: exact
  signatures, event routing, vocabulary, semantics, the deliberate deviations.
  The spec everything in `dist/` must conform to.
- **[PLAN.md](PLAN.md)** - architecture, phased plan, compatibility test
  matrix, risks.
- **[TESTING.md](TESTING.md)** - log-driven smoke procedure (`cgf` console
  drivers, outcomes verified in `SLIFNG.log`).

## Building from source

```sh
git clone --recurse-submodules https://github.com/crajjjj/SLIFNG
cd SLIFNG
xmake f -m release && xmake        # -> dist/Core/skse/plugins/SLIFNG.dll
```

Already cloned without submodules? `git submodule update --init --recursive`
(the recursion is required, CommonLibSSE-NG pulls openvr itself).

- **C++**: xmake 3.x, MSVC v143, CommonLibSSE-NG (alandtse fork, submodule at
  `lib/commonlibsse-ng`). One DLL covers SE / AE / VR.
- **Papyrus**: `skyrimse.ppj` (sources in `dist/Core/source/scripts`, output
  to `dist/Core/Scripts`). Needs the SkyUI 5.1 SDK on the import path for
  `SLIF_Menu` (`SKI_ConfigBase`); adjust `ModsFolder` in the .ppj.

## Status

**Released, 0.4.4.** The engine, shims, ESP, FOMOD, MCM (translatable),
automatic migration, incremental inflation and the author API all ship and run
in a live load order. Beeing Female NG, Fill Her Up and Sexlab Survival have been observed
working together on one actor; a real old-SLIF save has been migrated; and
co-save persistence, the native ramp, the settle event and the ESL flag are
each verified in game against `SLIFNG.log`.

Still open: the remaining compatibility-matrix rows (Estrus, Devious Devices,
Milk Mod Economy, a Beeing Female birth reset), and qotsafan's permission
before any Nexus release, since the mod ships the
`SexLab Inflation Framework.esp` plugin name.
