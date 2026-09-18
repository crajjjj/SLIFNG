# Troubleshooting & Console Tools

## The log

Everything SLIF NG does is verifiable in one file:

```
Documents\My Games\Skyrim Special Edition\SKSE\SLIFNG.log   (Skyrim VR on VR)
```

A healthy load starts like this:

```
SLIFNG v0.3.0 is loading...
Cosave serialization initialized.
Papyrus functions bound.
[Skee] BodyMorph interface v4
[Skee] NiTransform interface v3
[BodyProfile] 2 profile(s) available; fallback 'CBBE 3BA'
[Ledger] loaded N actor(s), calc=top_x (top_x=3)
[ReapplyAll] N actor(s) re-applied, M deferred to 3D load
```

Red flags and what they mean:

| Line | Meaning |
|---|---|
| `[Skee] ... interface missing` | RaceMenu missing or too old - nothing can be applied |
| `[Ledger] cosave version X != Y` | A save from an older dev build; state drops and rebuilds as mods re-send |
| `corrupt cosave record` | The record failed integrity checks and was discarded rather than crashing the load |
| `is not a function or does not exist` (Papyrus.0.log) | Some mod calls an API entry SLIF NG does not implement - report it, this line is the detection channel |

**Verbose logging** (MCM, or `cgf "SLIFNG_Debug.Verbose" true`) additionally logs every API call and every apply with a RaceMenu readback - the readback equalling the written value is skee confirming the write landed.

## First stop: the Actor page

Nine problems out of ten are answered by MCM > SLIF NG > Actor for the affected character:

- **Body bigger than expected?** Look for `also <key> ... (external)` rows under Applied - another mod is writing the same slider under its own RaceMenu key, and RaceMenu *sums* keys, so the visible body is your numbers **plus** that one. `(external)` means exactly that: the mod writes straight to skee, so it is in no ledger and no calculation type arbitrates it. OBody is the common one, and legitimately so - a per-NPC baseline shape is not an inflation. Nothing to fix unless the total is wrong for you; the stacking ends only if that mod routes through SLIF.
- **Nothing visible at all?** Check `Body / Skeleton nodes` (a missing `NPC Belly` means no XPMSSE or a broken skeleton) and `RaceMenu / skee` on the Settings page.
- **Morphs not moving?** The profile's sliders must exist in your body's `morphs.tri` - the body must be built in BodySlide with *Build Morphs* checked. `> drives bone scale` rows mean the profile maps that target to the bone, not to sliders. If the sliders are simply named differently on your body (the BHUNP profile's names are unverified), correct them in an [overlay](bodies.md#changing-a-profile-without-losing-it-on-update) rather than in the profile itself, which the next update overwrites.
- **Stuck shape?** *Reset this actor* wipes SLIF NG's state for that character (mods may or may not re-send - some only push on events).

## Console test drivers

Every part of the pipeline can be exercised without any consumer mod, via SKSE's `cgf`:

```
cgf "SLIFNG_Debug.Ping"                            liveness: DLL + scripts + registration
cgf "SLIFNG_Debug.SmokeTest"                       scripted end-to-end run, results in the log
cgf "SLIFNG_Debug.IPlayer" "TestMod" "slif_belly" 2.0
cgf "SLIFNG_Debug.MPlayer" "TestMod" "PregnancyBelly" 0.6
cgf "SLIFNG_Debug.RPlayer" "TestMod" "weight" 1.4   custom region (if the profile defines it)
cgf "SLIFNG_Debug.UPlayer" "TestMod"                unregister the test mod
cgf "SLIFNG_Debug.Mode" 1                           calculation type 0-5
cgf "SLIFNG_Debug.Gradual" false                    incremental inflation on/off
cgf "SLIFNG_Debug.Scale" 0.5                        overall magnitude
cgf "SLIFNG_Debug.ScaleT" "pregnancybelly" 1.5      per-target magnitude
cgf "SLIFNG_Debug.ScaleA" "pregnancybelly" 0.5      per-actor (player) magnitude
cgf "SLIFNG_Debug.Dump"                             whole ledger to the log
cgf "SLIFNG_Debug.Report"                           the Actor page, as log text
cgf "SLIFNG_Debug.Probe"                            what sliders skee knows about
```

## Known constraints

- The plugin **must** stay named `SexLab Inflation Framework.esp` - that name is how every consumer detects the framework.
- The plugin is **ESL-flagged** (light), so it takes no load-order slot. Updating from before 0.4.2 re-creates its quest under a new FormID: your inflation values, calculation type and magnitudes all survive (they live in the SKSE co-save, keyed by actor), but **the MCM re-registers** - open it once after updating.
- Leftover loose scripts from a half-removed old SLIF (`SLIF_Calc.pex`, `SLIF_Util.pex`, ...) can shadow nothing here - SLIF NG replaces the whole script set - but a mod manager showing file conflicts between SLIF and SLIF NG means both are enabled; disable old SLIF.
- An uninstall path (clearing all applied output before removing the mod) is not built yet; going back to old SLIF is safe (same RaceMenu key), removing inflation frameworks entirely mid-save is not.
