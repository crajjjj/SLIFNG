# SLIF NG — Compatibility Contract

**Status: PINNED** — extracted from shipped bytecode, not from sources.

## Provenance

| Fact | Value |
|---|---|
| Reference implementation | SexLab Inflation Framework SE **1.2.2 beta** (`.pex` bytecode; the bundled `.psc` sources are 1.2.1a and were NOT used) |
| Also active in survey load order | SLIF-SE-1.2.2-r2 community patch (replaces 4 scripts; does not change this surface) |
| Method | Decompiled `SLIF_Main.pex`, `SLIF_Morph.pex`, `SLIF_ScannerAlias.pex`, `SLIF_Scale.pex`, `SLIF_Calc.pex`, `SLIF_Util.pex` + static scan of every loose consumer `.pex` in the NEFARAM load order + MME BSA extraction + SGO4IF sources + SLIF's runtime `Modlist.json` |
| Surveyed consumers | Beeing Female NG, Fill Her Up Baka (+ its 2.0.3.12 patch), Sexlab Survival, Estrus Chaurus **core and** Spider Addon, Devious Devices NG, Devious Interests, Trap Needs to be Real Trap, Milk Mod Economy (BSA), SGO4IF (uses no SLIF at all) |
| Date pinned | 2026-09-15; surface widened 2026-09-16 (see correction below) |

**Scope rule: implement the observed surface below, nothing more.**

> **Survey correction (2026-09-16) — the surface had NOT converged.** This
> paragraph used to claim it had, on the strength of six mods scanned. A full
> grep of every consumer `.psc` in the load order then found **ten more entry
> points** in four mods that the first pass never looked at: Devious Devices NG
> (`hideNode`/`showNode`), Devious Interests (`unregisterMorph`), Estrus Chaurus
> core — distinct from the Spider Addon that WAS surveyed —
> (`GetValue`/`GetMinValue`/`GetMaxValue`/`inflateBoth`/`resetActor`/
> `updateActorList`) and Sexlab Survival's read path. Two lessons, both cheap:
> a mod's *addon* is not the mod, and "converged" needs a stopping rule that is
> not "we stopped finding things".

> **Survey correction (2026-09-15):** the first scan matched `slif_*` literals
> and so missed that FHU passes a raw skeleton node name instead — see sec.5.1.
> The function/event surface was unaffected, but the *vocabulary* was not
> closed. Any future consumer survey must scan for raw `"NPC ..."` node strings
> reaching the SLIF entry points, not just `slif_*` keys.

## 1. Detection surface

- Plugin file MUST be named **`SexLab Inflation Framework.esp`** — consumers detect
  SLIF via `Game.IsPluginInstalled("SexLab Inflation Framework.esp")`
  (e.g. BF NG `FWSystemConfig.HasSLIF()`) or `Game.GetModByName`.
- FormIDs `0x800` (menu quest) and `0x801` (scanner quest) are resolved only by
  SLIF-internal scripts in the reference implementation; no surveyed consumer
  resolves them. Keep the quest at `0x800` with the `SLIF_ScannerAlias` player
  alias (it hosts the mod-event registrations and `OnPlayerLoadGame`).
  Tier-2: keep `0x801` too.
- The plugin is **ESL-flagged** (light) as of 0.4.2, so it costs no load-order
  slot. This is contract-safe: the LOCAL FormIDs are still `0x800`/`0x801`, and
  every surveyed consumer detects SLIF by plugin NAME
  (`IsPluginInstalled` / `GetModByName`), never by resolving a form from it.
  What it does change is the global FormID prefix (`FE:xxx:800`), so a save
  made before 0.4.2 drops its reference to the old quest and starts the new
  one fresh: the MCM re-registers, and `OnConfigInit` runs instead of
  `OnVersionUpdate`. Harmless here because the ledger lives in the DLL co-save
  keyed by ACTOR FormIDs, and `OnConfigInit` already runs the legacy import.

## 2. Mod events (dynamic dispatch — arity-flexible)

Handlers live on the player-filled `ReferenceAlias` (script `SLIF_ScannerAlias`).
Registered in `OnInit`, re-registered in `OnPlayerLoadGame`.

| Event | Handler signature (pinned from 1.2.2 bytecode) | Observed senders |
|---|---|---|
| `SLIF_inflate` | `(Form Sender, String modName, String node, float value, String oldModName = "")` | Sexlab Survival, FHU, Estrus Spider |
| `SLIF_unregisterActor` | `(Form Sender, String modName)` | Sexlab Survival, **Milk Mod Economy** (its ONLY SLIF call) |
| `SLIF_unregisterNode` | `(Form Sender, String modName, String node)` | FHU |

Reference routing (preserve semantically):

```papyrus
OnSLIF_inflate         -> SLIF_Main.inflate(Sender as Actor, modName, node, value, -1, -1, oldModName, -1.0, -1.0, -1.0, -1.0)
OnSLIF_unregisterActor -> SLIF_Main.unregisterActor(Sender as Actor, modName)
OnSLIF_unregisterNode  -> SLIF_Main.unregisterNode(Sender as Actor, node, modName)
```

> **WARNING — argument-order swap:** the event carries `(modName, node)`; the
> global `unregisterNode` takes `(node, modName)`. Do not "fix" this.

### All 21 events are registered anyway (2026-09-19)

The other events still have **zero observed senders**: a scan of 14,005 compiled
scripts across the survey load order finds only three `SLIF_*` event-name
literals, `SLIF_inflate` (8 files), `SLIF_unregisterNode` (4) and
`SLIF_unregisterActor` (3). Every other `SLIF_*` string in a consumer `.pex` is a
local variable or that mod's own wrapper function, not an event name (Fill Her
Up's `SLIF_event` local, its `SLIF_morph` wrapper that calls the direct global,
its empty `SLIF_unregisterMorph` stub).

They are registered regardless, because the scope rule does not transfer from
direct calls to events. **A missing direct call announces itself; a missing event
does not.** An unimplemented global logs
`Static function X not found on object slif_main` on every attempt, which is how
the section 3 gap was found. A mod event is dispatched by name at runtime: an
unregistered one produces no error anywhere, it simply never arrives, and the
sending mod looks broken for no discoverable reason.

So events are not a place to be economical. The whole surface is one
registration plus one forwarding line each, and it removes a class of silent
incompatibility that no log line would reveal.

Routing for the rest, where SLIF NG differs from the reference:

| Event | Routed to | Note |
|---|---|---|
| `SLIF_morph`, `SLIF_unregisterMorph`, `SLIF_hideNode`, `SLIF_showNode`, `SLIF_resetActor` | the matching global | Identical behaviour |
| `SLIF_registerActor`, `SLIF_updateActor`, the `set*` family | `SLIFNG.UpdateActorBounds` | The reference used these to create a row and seed bounds before any value arrived; SLIF NG creates the row on first write, so only the bounds half has work to do. Per-ACTOR, since every one of these events carries a `Sender` - `UpdateModBounds` is the load-order-wide form behind `updateActorList` and would move everyone's bounds |
| `SLIF_unregisterMorphActor` | `unregisterActor` | **Wider than the reference**, which cleared only the morph side. One row per mod holds both, and leaving half the inflation stuck is the worse reading of "stop inflating this actor" |
| `SLIF_registerMorphActor`, `SLIF_updateMorphActor`, `SLIF_setMorphDefaultValues` | no-op | Morph bounds arrive with the value on `SLIF_Morph.morph`, so pre-registration has nothing left to do. Registered so the sender is not left guessing |

## 3. Direct global calls (arity baked into caller `.pex` — must match EXACTLY)

Papyrus parameter defaults do not exist in compiled `.pex`; callers bake the
literal defaults at each call site (verified in decompiled consumer bytecode:
FHU emits all four `-1.0` trailing defaults explicitly). These declarations are
therefore frozen — same names, order, types, count. Adding, removing, or
reordering ANY parameter breaks every pre-compiled caller.

```papyrus
; SLIF_Main.psc (Hidden) ------------------------------------------------------

Function inflate(Actor kActor, string modName, string node, float value, int gender = -1, int perspective = -1, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
; caller: Beeing Female NG (11-arg call baked)

Function unregisterNode(Actor kActor, string node, string modName = "All Mods") Global
; caller: Beeing Female NG (3-arg call baked)

; SLIF_Morph.psc (Hidden) -----------------------------------------------------

Function morph(Actor kActor, string modName, string morphName, float value, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
; callers: Fill Her Up Baka (9-arg call baked) AND Beeing Female NG
;          (FWAbilityBeeingFemale.psc:1427-1434, its FillHerUpUpdateNotes path)

; --- promoted from Tier-2 on 2026-09-16, after a full load-order survey ------
; grep of every consumer .psc for SLIF_Main./SLIF_Morph. calls. All of these
; were being called by installed mods and aborting in the VM.

Float Function GetValue(Actor kActor, string modName, string node, float default = 0.0) Global
Float Function GetMinValue(Actor kActor, string modName, string node, float default = 0.0) Global
Float Function GetMaxValue(Actor kActor, string modName, string node, float default = 100.0) Global
; callers: Sexlab Survival (GetValue, on a 1-game-hour timer), Estrus Chaurus (all three)
;
; TWO SEMANTICS THE GETTERS MUST HONOUR (verified against EC 4.390 bytecode
; and sources, 2026-09-18):
;  * EC reads bounds through SINGLE-SIDE keys - GetMinValue/GetMaxValue with
;    "slif_left_breast" / "slif_left_butt" (reference convert_keys naming one
;    side of a sync pair). These resolve to the pair target (side aliases in
;    Vocabulary); the sync fidelity gap of sec.5.1 applies.
;  * An UNRESOLVED key returns the CALLER'S DEFAULT, never 0.0 (the reference
;    ConvertToNode passes unknown strings through to an absent StorageUtil
;    read). EC clamps its pregnancy growth with
;    GetMaxValue(..., "slif_left_breast", MaxBreastScale) - a 0.0 here crushes
;    the actor flat. 0.0 is returned only for genuinely invalid parameters
;    (null actor / empty strings), as validParameters does.

Function hideNode(Actor kActor, String modName, String node, float value = 0.0000001, string oldModName = "") Global
Function showNode(Actor kActor, String modName, String node) Global
; caller: Devious Devices NG (zadLibs.SetNodeHidden - belly flat under a belt)

Function inflateBoth(Actor kActor, string modName, string syncKey, float value, int gender = -1, int perspective = -1, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
Function resetActor(Actor kActor, string modName = "All Mods", string node = "", float value = 1.0, int gender = -1, int newGender = -1, int perspective = -1, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
Function updateActorList(String modName = "All Mods", string node = "", int gender = -1, int newGender = -1, int perspective = -1, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
; caller: Estrus Chaurus - its MCM pushes NEW min/max bounds through this when
; the user moves a max-scale slider. Implemented as a bounds-only pass over
; the ledger (values never move, -1.0 keeps a field as stored) plus a
; re-apply of whoever changed; the reference's other job (re-pushing drifted
; applied values) has no equivalent here because nothing can drift.

; SLIF_Morph.psc, same promotion ---------------------------------------------

Function unregisterMorph(Actor kActor, string morphName, string modName = "All Mods") Global
; caller: Devious Interests

Float Function GetValue(Actor kActor, string modName, string morphName, float default = 0.0) Global
Float Function GetMinValue(Actor kActor, string modName, string morphName, float default = 0.0) Global
Float Function GetMaxValue(Actor kActor, string modName, string morphName, float default = 100.0) Global
; caller: Sexlab Survival
```

`gender` and `perspective` are accepted and ignored (reference behavior for the
observed calls: gender is re-derived internally, perspective is unused).

### The READ surface is not optional

Six of the entry points above are getters, and treating them as decoration was
a mistake this project actually made. Sexlab Survival's
`_SLS_BodyInflationTracking` recomputes `_SLS_BodyInflationScale` from three
`SLIF_Main.GetValue` calls every game hour and gates a whole scene branch on
`>= 0.3`. With `GetValue` absent the VM logged
`Static function GetValue not found on object slif_main` in a loop and SLS read
every actor as flat - no error a player would ever connect to SLIF NG.

A framework whose consumers ASK it questions is not a write-only sink.

## 4. Semantics

### 4.1 Values
- **Node scale (`inflate`)**: `1.0` = neutral. BF NG sends `scale + 1`
  (so `1.0 .. N`). Values act as a multiplicative scale on the target node(s).
- **Morph (`morph`)**: `0.0` = neutral and MUST **clear** the morph contribution
  (reference: `SLIF_Scale.SetBodyMorph` clears at exactly 0.0). FHU sends
  `value / 10` (morph weight range ~0..1+).
- `minimum/maximum/multiplier/increment = -1.0` means "not specified, keep
  defaults" (defaults: 0 / 100 / 1.0 / 0.1). Clamp value into [min, max], then
  multiply by multiplier.

### 4.2 `oldModName` — legacy-key takeover
`oldModName` names the **NiOverride key the consumer used before handing control
to SLIF** (its no-SLIF fallback path). On first contact, remove all NiOverride
node-transform AND body-morph contributions under that key for the actor:

| Consumer | oldModName value |
|---|---|
| Beeing Female NG | `"BeeingFemale"` |
| FHU (event path) | `"FHU_MODKEY"` |
| FHU (morph path) | `"sr_FillHerUp.esp"` |
| Sexlab Survival | `""` (none) |

### 4.3 Multi-mod aggregation — RE-DECIDED (2026-09-17): SLIF's own six
### calculation types, SLIF's numbering, SLIF's default (Top X)

> This section previously read "configurable, default highest-wins" with a
> two-mode fold of our own design. Overridden by a direct decision: **keep
> SLIF's formulas — they are heavily field-tested** — and because the default
> a migrating save has been LOOKING at is Top X, not highest-wins (SLIF reads
> `calculation_type` from Config.json with default 0, and the surveyed
> install has no such key).

Per (actor, target): each mod's value is stored separately in the ledger
(bounded per contribution: `SetBounds(value, min, max) * mult`, inverted
bounds tolerated by ordering them); the applied result is
`SLIF_Calc.addCalculationType` reproduced verbatim over those contributions
(engine: `Calc::Fold`). Contributions `<= 0` are skipped, exactly as the
reference skips them. Types, by SLIF's Config.json numbering:

| # | Type | Fold over the sorted (desc) positive contributions |
|---|------|---|
| 0 | **Top X (DEFAULT)** | `v0 + v1/3 + v2/6` (top_x = 3, each further place `/ (3*x)`) |
| 1 | Highest wins | `v0` |
| 2 | Subtract and add one | `1 + SUM(v_i - 1)`, floored at 0 |
| 3 | Square root | `sqrt(SUM(v_i^2))` |
| 4 | Average | plain average |
| 5 | Additive | plain sum (1.5 + 1.4 -> 2.9 — the reference's own additive) |

Every type except subtract-one falls back to the neutral 1.0 when the result
is `<= 0`. There is NO post-fold clamp (the per-contribution bounds are the
only clamp) and a below-neutral contribution CAN show — e.g. a lone 0.5 under
highest-wins applies as 0.5. Both are reference behaviour, kept.

**Sliders fold across mods too — a deliberate deviation (2026-09-18).** The
reference sums morph contributions unconditionally
(`SLIF_Morph_Util.CalculateMorphValue`), but that corner shipped disabled
(stock morph percents are all 0) and was never field-tested — and once a body
profile TRANSFORMS a node value into a slider, "several mods, one physical
target" applies to the slider, so composition must not depend on which API
spelling a mod used (FHU sends the same belly as `"NPC Belly"` or as a morph
depending on its own MCM). SLIF NG therefore builds ONE value per mod — its
direct contribution plus its transformed node share, summed WITHIN the mod
because they are one intent (BF NG sends `slif_belly` AND `PregnancyBelly`) —
and folds ACROSS mods with the calculation type, exactly as for nodes.
Worked example (real save): BF NG 0.495 direct + FHU 0.090 direct + SLS
`slif_belly` 2.2 -> 0.160 through the 3BA profile: highest-wins applies
0.495 (BF NG alone), Top X 0.568, additive 0.745. The StorageUtil mirror
`slif_<morphName>` stays the RAW DIRECT SUM (reference bookkeeping — that is
what Sexlab Survival reads back).

Because aggregation is a pure fold over stored inputs, switching types
recomputes and re-applies in one pass with no data migration (unlike the
reference, where a calc-type change left stale applied values). Overlap is
real: FHU's default `InflateMorph` and BF NG's belly morph are both
`PregnancyBelly` — under Top X the largest leads and the second adds a third
of itself; diagnostics (P4) shows the active type per target.

Implementation consequence: morphs are NOT applied under per-mod skee keys
(skee would compose those additively). The engine aggregates and writes ONE
value per target under the single `"SexLab Inflation Framework.esp"` skee key
— same key and single-key model as the reference (our own engine, our choice
of key). Unregister/cleanup stays a one-key removal, and legacy applied
values from a real-SLIF save are simply recomputed and overwritten in place
at migration.

### 4.4 `unregisterActor(actor, modName)`
Remove that mod's contributions for the actor (stored values + applied output),
then re-apply the remainder. MME calls this WITHOUT ever having registered —
must be a safe no-op that still clears any stale
`"SexLab Inflation Framework.esp"` output on the actor (that cleanup is exactly
why MME calls it).

## 5. Node vocabulary (complete observed set)

Resolutions pinned from the live `lists/000_Default_Lists.json` of 1.2.2:

| Key sent by consumers | Reference resolution | SLIF NG obligation |
|---|---|---|
| `slif_belly` | node `NPC Belly` | scale belly (BF NG, SLS) |
| `slif_breast` | sync -> `NPC L Breast` + `NPC R Breast` | scale both breasts (BF NG, Estrus) |
| `slif_butt` | sync -> `NPC L Butt` + `NPC R Butt` | scale butt (Estrus) |
| `slif_scrotum` | node `NPC GenitalsScrotum [GenScrot]` | scale scrotum (Estrus) |
| `slif_breast01` | **on ignore_keys list -> silent no-op** | keep as no-op (bug-compatible) |
| `slif_breast_p` | **in no list -> silent no-op** | keep as no-op (bug-compatible) |

(Estrus Spider sends the last two; real SLIF drops them silently. Preserve the
no-op — but report it in the Phase-4 diagnostics instead of staying silent.)

### 5.1 Raw skeleton-node spellings (NOT optional)

**`inflate` / `unregisterNode` must also accept a raw node name.** The reference
tolerates it by accident: `SLIF_Main.ConvertToNode` returns its argument
unchanged when it is not a known convert key, so an unrecognised string is used
directly as a node name.

| Consumer | sends | evidence |
|---|---|---|
| **Fill Her Up Baka** | **`"NPC Belly"`** (both inflate AND deflate) | `sr_inflateQuest.psc:144` `BELLY_NODE`, used at `:2496` / `:3173` (SetNodeScale) and `:2035` (RemoveNodeScale), both routed to the SLIF events at `:3005-3008` |
| Estrus Spider | maps node -> `slif_*` itself before sending (`zzEstrusSpider_BodyMod.psc:9-20`) | safe either way |
| SLS / BF NG | `slif_*` keys only | safe |

Both spellings MUST resolve to the **same canonical target**. Giving
`"NPC Belly"` its own target would let FHU and BF NG drive one physical node
through two ledger entries and clobber each other at the skee write.

**Fidelity gap (accepted):** a sync-pair node maps to the paired target, so a
consumer sending only `"NPC L Breast"` gets BOTH breasts scaled where the
reference scaled one. No observed consumer does this (FHU only ever sends the
unpaired `"NPC Belly"`), but it is a real difference.

Morph names (`SLIF_Morph.morph`) are **pass-through strings** — no vocabulary.
Consumers supply BodySlide slider names from their own config, and their
ORIGINAL case must reach skee (the ledger key is lowercased, the skee call is
not).

## 6. Persistent-state compatibility (free save migration)

SLIF NG reads/keeps these existing StorageUtil names so a save that ran real
SLIF migrates with zero user action:

- global: `slif_actor_list` (FormList), `slif_actor_name_list`,
  `slif_morph_actor_list`
- per actor: `slif_mod_list` (StringList), `slif_gender` (int),
  float `"<modName><node>"` and
  `"<modName><node>_min|_max|_mult|_increment"`,
  morphs: `"slif_<modName>_<morphName>"`, `"slif_<morphName>"`
- **`"slif_<morphName>"` is not just a migration read — it is kept WRITTEN.**
  Sexlab Survival reads it straight out of StorageUtil every game hour
  (`_SLS_BodyInflationTracking.psc:27` — no API involved), so the SLIF_Morph
  shim mirrors the engine's combined direct-morph total under that exact name
  on every morph()/unregisterMorph(). Best-effort: a whole-mod
  unregisterActor leaves a stale mirror, but SLS re-derives right after
  unregistering, and nothing else reads it.
- NiOverride/skee key namespace used for applied output:
  **`"SexLab Inflation Framework.esp"`** — keep it, so stale transforms written
  by real SLIF are found and owned (or cleanly removed) by SLIF NG.

### 6.1 `slif_scale_<morph>` is NOT a contract obligation

Worth recording, because it looks like one. Reference `SLIF_Morph_Util`
(`:348`) reads `StorageUtil.GetFloatValue(kActor, "slif_scale_" + morphName)`
and adds it to the direct morph value, which reads like a user offset. It is
not: `SLIF_Scale.SetBodyMorphs` (`:251`) WRITES that key as a cache of the
morph value derived from the actor's NODE values — the reference's node->morph
bridge, the same job SLIF NG's `AggregateSlider` does live (the reference sums
the two sources; SLIF NG applies the configured fold). Its producer also sits
inside the commented-out block in `SetAndUpdateMorphs`, reachable only via the
genderless path, so the key is vestigial in 1.2.2. **No consumer reads or
writes it.** SLIF NG does not implement it.

(SLIF NG's user magnitude scaling — PLAN P5 — is a separate, additive feature,
not an emulation of this key.)

## 7. Tier-2 (not observed, recommended for ecosystem safety)

Required by no surveyed consumer; cheap insurance against unsurveyed mods
(Devourment, Hentai Pregnancy, SL Parasites, Being a Cow were NOT surveyed):

`SLIF_Main.IsInstalled()` (implemented), `SLIF_Main.IsRegistered(actor, mod)`,
`SLIF_Main.registerActor(...)`, `SLIF_Main.GetGender(actor, gender)`,
mod events `SLIF_registerActor`, `SLIF_updateActor`, `SLIF_morph`,
`SLIF_registerMorphActor`, `SLIF_unregisterMorph`.

**Correction (2026-09-16).** This section previously filed the `Get*Value`
getters here and called SLS's references to them "dead code today". That was
wrong: `_SLS_BodyInflationTracking` calls them on a timer. They are Tier-1 and
are now in section 3. The error came from grepping SLS's *interface* script and
stopping there instead of following through to its callers - the survey method,
not the survey's scope, was at fault.

**Survey method that found the rest** (repeat it before release):

```sh
grep -rhoiE "SLIF_(Main|Morph|Config)\.[A-Za-z0-9_]+" --include=*.psc <mods dir>
```

run per consumer folder, EXCLUDING reference SLIF's own sources (they dominate
the counts with internal calls). That one command turned up ten missing entry
points across four installed mods.

Any unimplemented call surfaces in the Papyrus log as
`"<fn> is not a function or does not exist"` — that log line IS the detection
mechanism for missed consumers. Document it for users and watch for reports.

## 8. Explicit non-goals (dropped surface)

No SLIF-compatible presets API (user presets are a SLIF NG feature, PLAN P5), no queue MCM, no six calculation types, no list-category
editor, no per-actor MCM value editing, no `SLIF_Config` JSON path API, no
gradual-inflation drain loop (PLAN P8 replaces it with a native ramp), no
`SLIF_Scale` / `SLIF_Util` / `SLIF_Calc` public helpers. Roughly 84 of the
reference's ~90 public functions have no observed callers and are intentionally
absent.
