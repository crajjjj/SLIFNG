# SLIF NG — Smoke Testing

Most of what follows is verifiable from `Documents\My Games\Skyrim Special Edition\SKSE\SLIFNG.log`
(`Skyrim VR` on VR). The whole pipeline logs at info level: every API call with
its arguments and outcome, every apply with the fold mode, path taken
(morph vs NiTransform fallback), the value skee reports BACK after the write
(`readback`), and whether the actor's 3D was loaded.

## What the log CANNOT tell you

**A matching `readback` proves the write landed. It does not prove the body
moved.** `GetMorph` reads skee's morph dictionary - a map lookup that returns
whatever `SetMorph` put there, whether or not a vertex changed. Every apply can
log perfectly while nothing happens on screen.

Nor is the gap theoretical. One user reported a belly change only becoming
visible after unequipping and re-equipping armour, with nothing in the log to
show for it - and the most likely cause was never in our code at all: skee
applies morphs per MESH, only where that mesh carries `BODYTRI` extra data, and
BodySlide writes that per OUTFIT when "Build Morphs" is checked. An armour built
without morphs cannot move, however correct the value we wrote. (That report is
unconfirmed - it may equally have been their BodySlide build - which is itself
the point: the log could not settle it either way.)

So any step below that claims something *visibly* happens is a step you have to
**watch**, on a character who is **dressed as well as nude** - body and armour
are separate meshes with separate morph data. A test run that only greps the log
has not tested the mod.

## Setup (once)

In MO2: enable **`SLIF NG (dev)`**, disable
`[NoDelete][1300] SexLab Inflation Framework` and
`[NoDelete][1301] SLIF-SE-1.2.2-r2-Modern-PPlus-Overwrite`. Sort, launch, load
any save with a female player.

The console drivers below need **[ConsoleUtil Extended](https://www.nexusmods.com/skyrimspecialedition/mods/133569)**;
without it none of the `slifng` commands exist. They are declared in
`dist/Core/skse/CustomConsole/SLIFNG_Debug.yaml`, and every argument has a
default, so `slifng inflate` alone inflates the belly to 2.0.

## T0 — Load sanity (zero interaction)

Just load the save and quit. Expect in the log, in order:

```
SLIFNG v0.3.0 is loading...
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
slifng ping          <- notification proves dll+pex+registration
slifng smoke     <- scripted end-to-end run
```

SmokeTest exercises, in order: belly inflate x2 (SmokeA); an overlapping
x1.5 (SmokeB); a repeat send (must log `-> unchanged (early-out)`); **a
direct `PregnancyBelly` morph from SmokeC - per MOD, direct morph and
transformed node share add; ACROSS mods the calculation type folds, sliders
and nodes alike**; a raw `"NPC Belly"` send (SmokeD - FHU's spelling, must
route to the same slif_belly target); the dead key `slif_breast01` (must log
`dead key ... bug-compatible no-op`); an unknown key `slif_bogus` (must WARN
`unknown node key`); an unrelated morph; a walk through the calculation
types (Top X -> highest wins -> additive -> Top X - watch the belly step
down, then jump, then settle); then teardown - unregistering SmokeA must
leave the other mods' inflation **intact**, and only the final unregister
empties the ledger and clears everything.

The calculation types are SLIF's own six, SLIF's numbering, SLIF's default
(**0 = Top X**: largest + second/3 + third/6). Node folds skip non-positive
contributions and fall back to neutral 1.0; direct morph contributions are a
plain raw sum, always. `slifng calc N` switches at runtime.

The highest-value assertions:

1. With SmokeA 2.0 + SmokeB 1.5 + SmokeD 1.8 on slif_belly, the DEFAULT node
   fold is `2.0 + 1.8/3 + 1.5/6 = 2.85` - Top X, not 2.0 (highest) and not
   5.3 (additive). Mode 1 shows 2.0; mode 5 shows 5.3.
2. `PregnancyBelly` folds PER MOD (3BA profile, belly share = (v-1)/7.5):
   contributions A 0.133, B 0.067, D 0.107, C 0.4 direct. Top X shows
   `0.4 + 0.133/3 + 0.107/6 = 0.462`; highest wins shows `0.4` (SmokeC
   alone - NOT a 0.585-style sum); additive shows `0.707`.
3. `UnregisterMod(SmokeA)` must NOT flatten the body - SmokeB/C/D still
   drive it. Only the last unregister logs `cleared all owned output`.
4. The slider name in every `[Apply]` line keeps its original case
   (`PregnancyBelly`, never `pregnancybelly`).

`readback` must equal the value set - that is skee confirming the write took.

## T2 — Cosave round-trip

```
slifng inflate slif_belly 1.8 SaveTest
```
Save, quit to desktop, relaunch, load that save. Expect:

```
[Ledger] loaded 1 actor(s)
[ReapplyAll] 1 actor(s) re-applied ...
[Apply] ... 'slif_belly' agg 1.8 ...
```

and the belly is big immediately on load. Clean up:
`slifng clear SaveTest`.

## T3 — Real consumers (the P7 matrix, needs play)

With BF NG pregnancy progressing / SLS gluttony / FHU inflation active, grep
the log for `[API] Inflate` and `[API] Morph` lines carrying
`mod='Beeing Female'`, `'Sexlab Survival'`, `'Fill Her Up'` — every consumer
call surfaces there with its outcome; `oldModName` first-contacts log
`[Skee] cleaned legacy key ...` once per actor per session. MME: a
`[API] UnregisterMod(... mod='Milk Mod Economy')` on maid reset.

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

## T4 - Legacy import button (legacy save)

Load a save that ran reference SLIF (with SLIF NG replacing it). Nothing must
happen on its own: the import is a button since 0.4.12 (it ran from SkyUI's
registration callbacks in 0.4.8 and cost users their MCM list). MCM > Settings
> Old-SLIF import reads "pending: N actor(s)". Click it, and expect:

```
[API] Inflate(... mod='Beeing Female', key='npc belly' ...)   <- the walk
[API] SetMigrated(true)
```

plus the notification "SLIF NG: imported N contribution(s)". The row then
reads "done" and greys out, and stays that way after a save and load (the
cosave flag). On a save that never ran SLIF it reads "nothing to import".

Since 0.4.16 `OnVersionUpdate` really fires on a load where the version rose
(before, nothing called `CheckVersion` after the first session), so an upgrade
step added there runs on the next load of every existing save. It holds one,
at 416: see T8.

## T5 — Incremental inflation (ON by default)

```
slifng inflate slif_belly 3.0 RampTest
```

Incremental is the shipped default (`slifng gradual false` for
instant). The belly must swell in visible steps (0.1 per quarter second by default -
about 5 seconds to reach 3.0), not snap; `[Apply]` lines tick in the log with
the display value climbing. `GetValue(player, "All Mods", "slif_belly")`
mid-ramp returns the in-flight value, as the reference's queue did. Opening a
menu pauses the swell. `slifng gradual false` mid-ramp must snap
straight to the fold. Hide/unregister during a ramp stays instant.

Rebuild budget (0.4.19): the frame rate must not visibly drop while the
belly swells, on any body. Each ramp ends with one line
`[Ramp] <id> settled after N s: K step(s), rebuild avg X ms, worst Y ms`;
with verbose logging every step also logs `[Ramp] <id> step over N ms: ...,
rebuild X ms, budget Y ms`. A light body (3BA, BHUNP) shows steps about
100 ms apart; a dense one (UBE with a body preset) shows fewer, bigger steps
whose rebuild times add up to about a tenth of the ramp. The settle time stays
close to the unthrottled one (here about 5 s) either way.

## T6 — SGO4 adoption surface (0.3.0)

```
slifng region weight 1.5
```
With the [Weight] template commented IN in the body profile: the region's
sliders move ((1.5 - 1) / FullScale x Max each) and the actor page shows a
"Region weight" section. With it left commented out: one log line
`region 'region:weight': profile ... has no such section` and nothing else.

```
slifng morph PregnancyBelly 0.8   ; incremental ON
```
The DIRECT morph now swells in steps too (0.1 per quarter second), not only
node targets; `[Apply]` lines tick with the climbing value.

```
slifng scalea pregnancybelly 0.5
```
Halves the player's PregnancyBelly output without touching anyone else; the
actor page gains "  this actor x pregnancybelly  0.5x". Survives save/load
(cosave v8).

Foreign keys: with any other mod (SGO4, FHU fallback) holding a morph on a
slider SLIF NG drives, the actor page's Applied section lists
"  also <key>  <value>" rows. skee SUMS keys - if the body looks bigger than
our numbers explain, that row is why.

## T7 - A key that moves from bones to sliders (0.4.15)

0.4.15 maps `slif_butt` to a slider on CBBE 3BA, BHUNP and UBE, where 0.4.14
scaled bones. The engine must not leave the old bone scale under the new morph.

On a 0.4.14 install (or with `[slif_butt]` deleted from the profile):

```
slifng inflate slif_butt 1.6
```
Save, quit, update (or restore the section), load. Expect ONE log line

`[Apply] <actor> key 'slif_butt': dropped a leftover bone scale, profile '...' drives it with sliders`

and a butt that is morph-sized, not bone-sized with a morph on top. The actor
page reads `> drives BigButt`. Then `slifng inflate slif_butt 1.0` must return
the body to neutral: a bone scale that survived would stay behind here, since
nothing on the slider path ever writes a bone.

The opposite direction is NOT handled: delete a section mid-save while its
target is inflated and the slider value stays until the actor is reset.

## T8 - The MCM comes back on its own (0.4.16)

Before 0.4.16 nothing re-armed SkyUI's registration events on a load (they do
not survive a save), so the menu registered only in the session its quest
started. `SLIF_ScannerAlias.OnPlayerLoadGame` now calls the menu's
`OnGameReload` on every load, like SkyUI's own `SKI_PlayerLoadGameAlias`: it
re-arms the listeners and registers only if the menu is not registered. The
416 step in `OnVersionUpdate` clears the saved config manager ONCE, so a menu
SkyUI already dropped registers again. Use a throwaway save: the reset below
permanently removes the menu of any OTHER mod with this same bug.

The upgrade path, which is the case the 0.4.16 reports are about. On a 0.4.15
install, load a save that has run SLIF NG before (not the session it was
installed in):

```
setstage SKI_ConfigManagerInstance 1
```

Wait for "MCM: Registered N new menu(s)": SLIF NG is gone from the list. Save,
quit, update to 0.4.16, load, and wait about a minute WITHOUT any console
command: SLIF NG must be back, listed as "SLIF NG". The Papyrus log shows
`SLIF_Menu ... Registered SLIF NG at MCM.`

Then on 0.4.16, run the same `setstage` once more: SLIF NG must come back
within a minute of the reset.

## T9 - The C++ handshake (0.4.18)

0.4.18 adds the `SLIFNG_GetQueryInterface` export, moves the messaging
listener to `kPostLoad`, and retries the skee request as a broadcast. Without a
consumer plugin, check the log after a plain launch:

- `[API] inter-plugin query listener registered` appears exactly once.
- `[Skee] BodyMorph interface v...` appears as in T0: the skee request still
  lands.
- The DLL lists the export: `llvm-objdump -p BodyInflationNG.dll` shows
  `SLIFNG_GetQueryInterface` next to the three `SKSEPlugin_*` exports.

With a consumer plugin installed (any SKSE DLL that reads SLIF NG from C++),
`SLIFNG.log` shows `[API] query interface v2 handed out through
SLIFNG_GetQueryInterface` when it uses the export, or `handed to '<name>'` when
it uses messaging, and the consumer reads non-default values on an inflated
actor.

## T10 - The legacy morph reset (0.4.19)

`SLIF_Morph.resetActor` is what Bimbos of Skyrim clears its morphs with before
re-adding them. From the console, on a nude player:

```
slifng morph PregnancyBelly 0.6
slifng morph BreastsFantasy 0.4
slifng morphreset PregnancyBelly   belly flat, breasts stay
slifng morphreset                  breasts flat too (whole row)
slifng dumpp                       no ConsoleTest rows left
```

No `resetActor not found on object slif_morph` line in the Papyrus log. With
Bimbos installed and its "Use SLIF for morphs" on, press Reapply Morphs to
Actors in its MCM several times: the body must stay the same size.

## Other console tools

```
slifng dump                                  full ledger to log
slifng dumpp                                 player's entries only
slifng morph BreastsNewSH 0.7  any morph by name
slifng calc 1                                additive; 0 = highest
slifng verbose false                         quieten per-call logs
slifng inflate "NPC Belly" 1.8     raw node name (FHU form)
```

## Note on logging cost

Verbose per-call logging defaults ON in dev builds and costs a log line plus one
extra skee readback per slider written. Release should ship it OFF
(`SLIFNG.SetVerboseLogging(false)`). The log flushes on `warn`, not `info`, so
routine lines do not force a synchronous disk write on the caller's thread.
