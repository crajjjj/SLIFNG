# SLIF NG

A lightweight, zero-configuration drop-in replacement for
**SexLab Inflation Framework SE**: pinned Papyrus compatibility shims over an
own native engine (`SLIFNG.dll`, CommonLibSSE-NG — one DLL for SE/AE/VR) that
keeps a keyed per-mod ledger in the SKSE co-save and applies through
RaceMenu/skee. No framework dependencies; serialization follows the
SexlabArousedNG patterns. (Acook1e's Morph Inflation Framework NG was
evaluated as an engine and consciously reimplemented instead — see PLAN P0.)

Users install it *instead of* SLIF; existing consumer mods
(Beeing Female NG, Fill Her Up, Sexlab Survival, Estrus, MME) keep working
unmodified, and saves that ran real SLIF migrate automatically.

Why: SLIF's real consumer surface is **6 entry points out of ~90 public
functions** (measured, not guessed — see provenance in CONTRACT.md), while its
setup complexity (nine MCM pages of body-specific JSON editing, silent failure
when misconfigured) is the top user complaint. SLIF NG implements exactly the
used surface, detects instead of asking, and diagnoses instead of failing
silently.

## Documents

- **[CONTRACT.md](CONTRACT.md)** — the pinned compatibility contract: exact
  signatures from 1.2.2 bytecode, event routing, vocabulary, semantics,
  StorageUtil/NiOverride namespaces, tier-2 list, non-goals. Read first;
  everything in `dist/` must conform to it.
- **[PLAN.md](PLAN.md)** — architecture and phased plan (P0 spikes → P8),
  compatibility test matrix, risks.
- **[TESTING.md](TESTING.md)** — log-driven smoke procedure: `cgf` console
  drivers exercise the whole pipeline, outcomes verified in `SLIFNG.log`.

## Building

```sh
git clone --recurse-submodules https://github.com/crajjjj/SLIFNG
cd SLIFNG
xmake f -m release && xmake        # -> dist/Core/skse/plugins/SLIFNG.dll
```

Already cloned without submodules? `git submodule update --init --recursive`
— the recursion is required, CommonLibSSE-NG pulls openvr itself.

- **C++**: xmake 3.x, MSVC v143, CommonLibSSE-NG v8.0.1 (alandtse fork,
  submodule at `lib/commonlibsse-ng`). One DLL covers SE / AE / VR.
- **Papyrus**: `skyrimse.ppj` (sources in `dist/Core/source/scripts`, output to
  `dist/Core/Scripts`). Needs the SkyUI 5.1 SDK on the import path for
  `SLIF_Menu` (`SKI_ConfigBase`); adjust `ModsFolder` in the .ppj.

## Layout

```
CONTRACT.md                       frozen API contract (the spec)
PLAN.md                           implementation plan
dist/Core/source/scripts/         Papyrus sources (contract stubs so far)
  SLIF_ScannerAlias.psc           mod-event surface (3 events)
  SLIF_Main.psc                   inflate / unregisterNode / unregisterActor
  SLIF_Morph.psc                  morph
```

## Status

P1 code-complete (2026-09-15): `SLIFNG.dll` builds (ledger + fold +
serialization + skee apply + Papyrus natives), all four .psc compile, shims
wired, clean-room ESP (quest 0x800 + scripted player alias + 0x801 stub) and
SEQ authored. A ready-to-test mod folder exists at
`E:/nefaram/mods/SLIF NG (dev)` — enable it and disable the two real-SLIF
mods to smoke-test. Remaining: in-game smokes (P7 matrix), arity experiment,
qotsafan permission (release gate only).
