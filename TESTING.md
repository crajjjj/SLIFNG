# SLIF NG — Smoke Testing (log-driven, minimal play time)

Everything below is verifiable from `Documents\My Games\Skyrim Special Edition\SKSE\SLIFNG.log`
(`Skyrim VR` on VR). The whole pipeline logs at info level: every API call with
its arguments and outcome, every apply with the fold mode, path taken
(morph vs NiTransform fallback), the value skee reports BACK after the write
(`readback`), and whether the actor's 3D was loaded.

## Setup (once)

In MO2: enable **`SLIF NG (dev)`**, disable
`[NoDelete][1300] SexLab Inflation Framework` and
`[NoDelete][1301] SLIF-SE-1.2.2-r2-Modern-PPlus-Overwrite`. Sort, launch, load
any save with a female player.

## T0 — Load sanity (zero interaction)

Just load the save and quit. Expect in the log, in order:

```
SLIFNG v0.1.0 is loading...
Cosave serialization initialized.
Papyrus functions bound.
[Skee] BodyMorph interface vN
[Skee] NiTransform interface vN
[Ledger] loaded 0 actor(s)          <- first run; the count on later runs
[ReapplyAll] 0 target(s) re-applied ...
```

Red flags: `interface missing` (RaceMenu problem), any `corrupt cosave`,
`is not a function or does not exist` in Papyrus.0.log (script mismatch).

## T1 — Full pipeline from the console (no gameplay)

Open the console and run:

```
cgf "SLIFNG_Debug.Ping"          <- notification proves dll+pex+registration
cgf "SLIFNG_Debug.SmokeTest"     <- scripted end-to-end run
```

SmokeTest exercises, in order: belly inflate x2 (SmokeA); an overlapping x1.5
(SmokeB, masked under highest-wins); a repeat send (must log
`-> unchanged (early-out)`); **a direct `PregnancyBelly` morph from SmokeC —
the same slider `slif_belly` drives, so the two must FOLD, not clobber**;
the dead key `slif_breast01` (must log `dead key ... bug-compatible no-op`);
an unknown key `slif_bogus` (must WARN `unknown node key`); an unrelated
morph; a switch to additive and back (watch the belly grow then shrink);
then teardown — unregistering SmokeA must leave SmokeB/SmokeC's inflation
**intact**, and only the final unregister empties the ledger and clears
everything.

The highest-value assertions, all regression checks for bugs found in review:

0. **`Inflate(SmokeD, "NPC Belly", 1.8)` must be ACCEPTED**, logging
   `raw node 'npc belly' routed to canonical target 'slif_belly'` — never
   `unknown node key`. This is exactly what Fill Her Up sends; before the fix
   FHU's inflation *and* deflation were both silently dead.

1. After `Morph(SmokeC, "PregnancyBelly", 0.4)`, the `PregnancyBelly` readback
   is still `1` (SmokeA's node-key deviation wins) — **not** `0.4`. Under
   additive it becomes `1.9`, not `0.4` or `1.5`.
2. `UnregisterMod(SmokeA)` must NOT flatten the body — SmokeB/SmokeC still
   drive it. Only the last unregister logs `cleared all owned output`.
3. The slider name in every `[Apply]` line is `PregnancyBelly`, original case
   — never `pregnancybelly`.

Expected key lines:

```
[API] Inflate(00000014 'Prisoner', mod='SmokeA', key='slif_belly', value=2 ...)
[Apply] ... node-key 'slif_belly' agg 2 (highest) -> morph path: 'PregnancyBelly'=1 (readback 1) ... 3D loaded
[API] Inflate(... mod='SmokeB' ... value=1.5 ...)
[Apply] ... agg 2 (highest) ...                      <- overlap masked, still 2
[API]   -> unchanged (early-out)
[API]   -> dead key 'slif_breast01' (bug-compatible no-op ...)
[API]   -> unknown node key 'slif_bogus' ...
[API] SetAggregationMode(1)
[Apply] ... agg 2.5 (additive) ...
[Dump] ===== ledger: 1 actor(s), mode=... =====
[Apply] ... agg 1 ... 'PregnancyBelly'=0 ...          <- after unregister: neutral
[Dump] ... (no ledger entries after the second dump)
```

`readback` must equal the value set — that is skee confirming the write took.

## T2 — Cosave round-trip

```
cgf "SLIFNG_Debug.IPlayer" "SaveTest" "slif_belly" 1.8
```
Save, quit to desktop, relaunch, load that save. Expect:

```
[Ledger] loaded 1 actor(s)
[ReapplyAll] 1 target(s) re-applied ...
[Apply] ... 'slif_belly' agg 1.8 ...
```

and the belly is big immediately on load. Clean up:
`cgf "SLIFNG_Debug.UPlayer" "SaveTest"`.

## T3 — Real consumers (the P7 matrix, needs play)

With BF NG pregnancy progressing / SLS gluttony / FHU inflation active, grep
the log for `[API] Inflate` and `[API] Morph` lines carrying
`mod='Beeing Female'`, `'Sexlab Survival'`, `'Fill Her Up'` — every consumer
call surfaces there with its outcome; `oldModName` first-contacts log
`[Skee] cleaned legacy key ...` once per actor per session. MME: a
`[API] UnregisterMod(... mod='Milk Mod Economy')` on maid reset.

## MCM pages not updating after a build?

`Pages` is a script PROPERTY, so SkyUI stores it in the save the first time it
registers the menu and `OnConfigInit` never fires again. A page added in a later
build therefore stays invisible on an existing save. The fix is already in
`SLIF_Menu.psc` - bump `GetVersion()` whenever `Pages` or `ModName` changes, and
`OnVersionUpdate` re-runs `BuildPages()`.

If a page is still missing after loading a save made with the older script,
force SkyUI to re-read the menu from the console (a third guard also exists:
`OnConfigOpen` rebuilds if `Pages.length` disagrees with the expected count,
borrowed from SLO Aroused NG):

```
setstage SKI_ConfigManagerInstance 1
```

then wait for the "Registered new menus" notification and reopen the MCM.

## Which fill mode to use

`TOP_TO_BOTTOM` only spills into the RIGHT column once the LEFT one is full, so
a short page leaves half the menu blank. The two pages want different modes:

* **Settings** - `LEFT_TO_RIGHT`: fills alternately, so options are written in
  PAIRS (left, right, left, ...) and `AddEmptyOption()` pads whichever column
  runs out. Headers pair up too, which is what gives the page two titled
  columns.
* **Actor** - `TOP_TO_BOTTOM`: the report is long and variable, so filling one
  column and flowing into the next keeps related rows adjacent. Pairing it
  would interleave unrelated sections.

## Actor page layout rules

It renders in a TWO-COLUMN SkyUI MCM, which is unforgiving:

* a label past ~30 chars collides with its own value column;
* a value past ~20 chars runs LEFT across the label;
* an **empty value means "section header"**, so a placeholder line must still
  carry a value (`"Registered" / "nothing"`) or it draws as a header with a
  divider through it.

`Report::ForActor` obeys these: long lists become one row per item (missing
skeleton nodes), contributions use a sub-header per target with short indented
rows per mod, and the skee readback only gets a row when it DISAGREES with what
we wrote.

## Other console tools

```
cgf "SLIFNG_Debug.Dump"                                  full ledger to log
cgf "SLIFNG_Debug.DumpP"                                 player's entries only
cgf "SLIFNG_Debug.MPlayer" "TestMod" "BreastsNewSH" 0.7  any morph by name
cgf "SLIFNG_Debug.Mode" 1                                additive; 0 = highest
cgf "SLIFNG_Debug.Verbose" false                         quieten per-call logs
cgf "SLIFNG_Debug.IPlayer" "TestMod" "NPC Belly" 1.8     raw node name (FHU form)
```

## Note on logging cost

Verbose per-call logging defaults ON in dev builds and costs a log line plus one
extra skee readback per slider written. Release should ship it OFF
(`SLIFNG.SetVerboseLogging(false)`). The log flushes on `warn`, not `info`, so
routine lines do not force a synchronous disk write on the caller's thread.
