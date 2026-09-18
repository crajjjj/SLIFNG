# Building from Source

## Clone

```sh
git clone --recurse-submodules https://github.com/crajjjj/SLIFNG
cd SLIFNG
```

Already cloned without submodules? `git submodule update --init --recursive` - the recursion matters, CommonLibSSE-NG pulls openvr itself.

## The DLL

Requirements: **xmake 3.x**, **MSVC v143** (Visual Studio 2022 build tools). CommonLibSSE-NG (alandtse fork) is the submodule at `lib/commonlibsse-ng`; one build covers SE, AE and VR via Address Library.

```sh
xmake f -m release && xmake        # -> dist/Core/skse/plugins/SLIFNG.dll
```

## The Papyrus scripts

Sources live in `dist/Core/source/scripts/`, compiled output in `dist/Core/Scripts/`. A `skyrimse.ppj` ships for pyro-style builds; either way the import path needs:

- the vanilla + SKSE sources
- the **SkyUI 5.1 SDK** (for `SKI_ConfigBase` - `SLIF_Menu`)
- **PapyrusUtil**'s `StorageUtil.psc` (for `SLIFNG_Migrate` and the `SLIF_Morph` mirror)

!!! warning "Always rebuild the whole script set"
    Papyrus bakes call arities into compiled callers. If a native signature in `SLIFNG.psc` changes, every shim that calls it must be recompiled in the same pass - a partial rebuild produces scripts that abort at run time with no compile error.

## Repository layout

```
src/                      the SKSE plugin
  Ledger.{h,cpp}          contributions, folds, hidden pins, magnitudes, co-save
  Calc.h                  SLIF's six calculation types, verbatim
  BodyProfile.{h,cpp}     per-actor INI profiles, custom regions
  Skee.{h,cpp}            NiTransform/BodyMorph application, coalescing, threading
  Ramp.{h,cpp}            incremental inflation (the native drain-loop replacement)
  Papyrus.cpp             the native surface SLIFNG.psc binds to
  Query.h                 the shared read core (Papyrus + C++ answers)
  APIServer.cpp           serves API/SLIFNG_API.h over SKSE messaging
  API/SLIFNG_API.h        the public header other plugins copy
dist/Core/                the shippable mod (ESP, SEQ, scripts, DLL, profiles)
dist/Bodies/              the FOMOD's per-body default.ini variants
dist/fomod/               installer metadata
CONTRACT.md               the pinned compatibility contract - read this first
PLAN.md                   architecture and phase history
TESTING.md                the log-driven smoke procedure (cgf drivers)
```

## Packaging a release

```sh
python tools/pack-api-kit.py    # -> Release/SLIFNG-API-<version>+.zip
```

Packages the integration kit: `src/API/SLIFNG_API.h`, the three consumer-facing scripts, the
README from `tools/api-kit/`, and a `VERSIONS.txt` whose numbers are read out of the sources
that define them (`kApiVersion` from `Papyrus.cpp`, `kQueryVersion` from `SLIFNG_API.h`, the mod
version from `SLIFNG_Version.psc`) so they cannot drift from what the mod reports at runtime.

The mod archive itself is `dist/` zipped with `Core`, `Bodies` and `fomod` at the root — that is
the FOMOD the installer reads. Both zips go on the GitHub release; `Release/` is build output and
is not tracked.

## Testing

There is no test suite in the usual sense; there is a **log-driven smoke procedure**. `TESTING.md` walks it: `cgf "SLIFNG_Debug.SmokeTest"` exercises the whole pipeline from the console with no consumer mod installed, and every assertion is a line you can grep in `SLIFNG.log`. The compatibility matrix in `PLAN.md` (P7) is the release gate.

## Versioning

The mod version lives in exactly one script, `SLIFNG_Version.psc`, packed as `major*10000 + minor*100 + patch` (SL Widgets convention). The MCM's `GetVersion()` delegates to it, and SkyUI's `OnVersionUpdate` ladder keys on the packed number. **The packed value must stay above 122 forever**: a migrating save carries the replaced SLIF MCM's stored version 122 and SkyUI only fires version updates on an increase - which is why 0.2.0 was the first version that could exist.
