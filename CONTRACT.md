# SLIF NG — Compatibility Contract

**Status: PINNED** — extracted from shipped bytecode, not from sources.

## Provenance

| Fact | Value |
|---|---|
| Reference implementation | SexLab Inflation Framework SE **1.2.2 beta** (`.pex` bytecode; the bundled `.psc` sources are 1.2.1a and were NOT used) |
| Also active in survey load order | SLIF-SE-1.2.2-r2 community patch (replaces 4 scripts; does not change this surface) |
| Method | Decompiled `SLIF_Main.pex`, `SLIF_Morph.pex`, `SLIF_ScannerAlias.pex`, `SLIF_Scale.pex`, `SLIF_Calc.pex`, `SLIF_Util.pex` + static scan of every loose consumer `.pex` in the NEFARAM load order + MME BSA extraction + SGO4IF sources + SLIF's runtime `Modlist.json` |
| Surveyed consumers | Beeing Female NG, Fill Her Up Baka, Sexlab Survival, Estrus Chaurus Spider Addon, Milk Mod Economy (BSA), SGO4IF (uses no SLIF at all) |
| Date pinned | 2026-09-15 |

Six mods surveyed across two independent sources (static call-site scan + SLIF's
runtime registration list); the FUNCTION surface converged and did not grow with
the last two mods added. **Scope rule: implement the observed surface below,
nothing more.**

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

The other 23 `SLIF_*` mod events of the reference implementation have **zero
observed senders** and are NOT part of this contract.

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
```

`gender` and `perspective` are accepted and ignored (reference behavior for the
observed calls: gender is re-derived internally, perspective is unused).

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

### 4.3 Multi-mod aggregation — DECIDED: configurable, default highest-wins
Per (actor, target): each mod's clamped value is stored separately in the
ledger; the applied result is a pure fold over those contributions, computed
at apply time. Two modes, selectable globally (MCM) and overridable per
target (P5 presets):

- **Highest wins** (DEFAULT): maximum contribution shows, others are masked.
- **Additive**: morphs = plain sum (0.0-neutral); node scales = sum of
  deviations from neutral, `1.0 + SUM(v_i - 1.0)` (1.5 + 1.4 -> 1.9, never a
  naive multiplier sum). The per-target `maximum` clamps AFTER aggregation —
  the safety valve against compounding.

Because aggregation is a fold over stored inputs, switching modes recomputes
and re-applies in one pass with no data migration (unlike the reference, where
a calc-type change left stale applied values). Overlap is real: FHU's default
`InflateMorph` and BF NG's belly morph are both `PregnancyBelly` — under
highest-wins the larger one shows and the smaller is masked; diagnostics (P4)
shows the active mode per target and masked/summed status. The reference's
other calc types (Top X, subtract-one, square-root, average) are NOT
implemented — no consumer distinguishes them; the fold is one function if one
is ever genuinely missed.

Floor: highest-wins never returns below neutral (1.0 node / 0.0 morph), so a
contribution *below* neutral can never win - it can only be masked. Additive
does honour below-neutral contributions, clamped at both ends by the
per-contribution bounds. (Negative morph values are legitimate input - see
sec.4.1 - they simply cannot win a highest-wins fold.)

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

`SLIF_Main.IsInstalled()`, `SLIF_Main.IsRegistered(actor, mod)`,
`SLIF_Main.GetValue/GetMinValue/GetMaxValue(actor, mod, node, default)`,
`SLIF_Morph.GetValue(...)` — SLS ships `_sls_intslif.pex` referencing both
`SLIF_Main.GetValue` and `SLIF_Morph.GetValue`; no live caller in its own
sources, so dead code today, but it is compiled and could be revived —
`SLIF_Main.registerActor(...)`, `SLIF_Main.GetGender(actor, gender)`,
mod events `SLIF_registerActor`, `SLIF_updateActor`, `SLIF_morph`,
`SLIF_registerMorphActor`, `SLIF_unregisterMorph`.

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
