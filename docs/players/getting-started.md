# Getting Started

## Requirements

- **SKSE64** (or SKSE VR - one DLL covers SE, AE and VR)
- **SkyUI** (the MCM)
- **RaceMenu** - its skee plugin performs the actual body changes. Both generations work: the AE build (0.4.19+) and pre-AE RaceMenu 0.4.16 on Skyrim SE 1.5.97, which exposes an older bone-scaling interface that SLIF NG binds separately. Morphs behave identically on both
- **XPMSSE** - the standard skeleton nodes (`NPC Belly` and friends)
- **PapyrusUtil** - present in every SexLab load order; used once, to read an old SLIF save during migration

## Installing

1. **Uninstall or disable SexLab Inflation Framework** and its patches. SLIF NG replaces it file for file - the plugin is literally named `SexLab Inflation Framework.esp`, which is how existing mods find it without needing patches.
2. Install SLIF NG with the FOMOD and answer its one question: **which body did you build in BodySlide?**
    - **CBBE 3BA (3BBB)** - belly and breasts become real BodySlide morphs, slider names verified against the 3BA reference set
    - **CBBE (generic)** - plain CBBE naming
    - **BHUNP** - UUNP-derived naming (slider names unverified; see the page footer note in the installer)
    - **None - node scaling** - no morphs, pure skeleton-bone scaling: exactly what old SLIF does out of the box, works with any body
    - UBE 2.0 support always installs and matches by race, so UBE characters get their own sliders even alongside a 3BA game
3. Load your game and play. There is no step 3.

If the save previously ran old SLIF, open the MCM and press **Import from old SLIF** on the Settings page once. Every mod's per-actor values are copied out of the old framework's storage into SLIF NG, and a notification reports how many were carried over; the button then greys itself out. On a save that never ran SLIF it reads `nothing found` and is disabled.

    It is a button rather than an automatic step on purpose. Doing the walk automatically meant doing it inside SkyUI's menu-registration pass, and that could stop SkyUI registering the mods after SLIF NG - several users lost most of their MCM list to it in 0.4.8.

!!! tip "Wrong body picked?"
    You do not need to reinstall. Every profile ships in `Data/SLIFNG/Bodies/`; copy the one you want over `default.ini`, then reload a save. To *tune* a profile rather than swap it, put your changes in an [overlay](bodies.md#changing-a-profile-without-losing-it-on-update) so an update cannot overwrite them. [Body Profile Format](../authors/body-profiles.md) documents both.

## What you should see

Inflation-driving mods (Beeing Female NG pregnancy, Fill Her Up, Sexlab Survival gluttony, Estrus, ...) simply keep working - they cannot tell SLIF NG from the original. Differences you may notice:

- Bodies swell toward new sizes in steps instead of snapping (**incremental inflation**, on by default, MCM toggle).
- No nine-page configuration MCM. One Settings page, one per-actor diagnostics page - see the [MCM Reference](mcm.md).
- When several mods inflate the same body part at once, the result follows the selected [calculation type](mcm.md#calculation-type) instead of quietly stacking.

## Uninstalling / going back

Going back to old SLIF is a plain swap in your mod manager: disable SLIF NG, re-enable SLIF. Values SLIF NG applied live under old SLIF's own RaceMenu key, so the original framework finds and overwrites them. SLIF NG's own ledger lives in the SKSE co-save and is ignored by everything else.
