# SLIF NG

A modern, zero-configuration replacement for **SexLab Inflation Framework SE** (SLIF). You install it *instead of* SLIF; every mod that talks to SLIF keeps working without any patch, and a save that ran the old framework is imported with one button press.

!!! warning "Adult modding ecosystem - reference documentation"
    SLIF NG is a body-inflation framework for the adult (18+) side of Skyrim modding: its consumers are mods like Beeing Female NG, Fill Her Up and Sexlab Survival. **These pages are reference documentation** - they describe installation, configuration and the developer API, and contain no adult media. Install and use the mod only where appropriate for your age and local laws.

Under the hood the old Papyrus framework is gone. A native SKSE plugin (`BodyInflationNG.dll`, one DLL for SE / AE / VR) keeps every mod's inflation values in the co-save and applies them through RaceMenu, while thin script shims keep the exact API old SLIF exposed - same function names, same arguments, **same math**: the six calculation types were the field-tested part of the reference and were kept formula for formula.

## For Players

- [Getting Started](players/getting-started.md) - requirements, the one installer question, and importing an old-SLIF save
- [Bodies, Nodes & Morphs](players/bodies.md) - what a "node" and a "morph" actually are, and how the body choice changes what you see
- [MCM Reference](players/mcm.md) - both pages, every option, and what the calculation types mean
- [Supported Mods](players/compatibility.md) - which mods drive SLIF NG and through which door
- [Troubleshooting & Console Tools](players/troubleshooting.md) - the log, the actor page, and the `slifng` console drivers

## For Mod Authors

- [Overview & Architecture](authors/overview.md) - the ledger, the shims, and how an inflate becomes a body change
- [Write API](authors/write-api.md) - old SLIF's surface, pinned from bytecode, plus the SLIF NG extensions (batch calls, semantic regions)
- [Query API](authors/query-api.md) - reading the store from Papyrus or from another SKSE plugin
- [Aggregation Math](authors/math.md) - the six calculation types, the per-mod slider fold, clamps and defaults
- [Body Profile Format](authors/body-profiles.md) - the INI schema, per-actor resolution, custom regions
- [Building from Source](authors/building.md) - xmake, the Papyrus compiler, repo layout

---

## Status

**0.3.0, feature-complete and in testing - not yet publicly released.** The engine, shims, ESP, FOMOD, MCM, automatic migration, incremental inflation and the author API all ship and run in a live load order. Before a public release: the remaining compatibility-matrix rows (Estrus, Devious Devices, Milk Mod Economy, a Beeing Female birth reset) and qotsafan's permission for shipping the `SexLab Inflation Framework.esp` plugin name.

The full compatibility contract - exact signatures, event routing, deliberate deviations, bug-for-bug quirks kept on purpose - lives in [CONTRACT.md](https://github.com/crajjjj/SLIFNG/blob/main/CONTRACT.md) in the repository.
