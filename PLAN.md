# SLIF NG — Implementation Plan

## Status — 2026-09-16

**P1-P5 substantially done and running in a live load order.** SLIFNG.dll,
the ESP/SEQ, the pinned shims, per-actor body profiles, the FOMOD and a two-page
MCM all ship; BF NG, Fill Her Up and Sexlab Survival have been observed folding
correctly together on one actor (`SLIFNG.log`, 2026-09-15).

P6 migration is **built but untested** (the import button has never met a real
old-SLIF save). Not started: **P8 ramp**, and P6's uninstall path. Not run:
**P7**, three of its six rows.

Release gates, in order:
1. **qotsafan's permission** for the `SexLab Inflation Framework.esp` name (P0)
   — the repo is already public, so this is now the pacing item.
2. ~~P6 legacy-save migration~~ **built** (MCM import button); still needs one
   real migrating-save test.
3. **P7 rows for Estrus, MME and an old-SLIF save** — never exercised (the
   import button now makes the old-save row testable).

## Goal

A drop-in replacement for SexLab Inflation Framework SE that existing consumers
cannot tell apart (per [CONTRACT.md](CONTRACT.md)), with **zero required
configuration** and a fraction of the script load. Users install it *instead of*
SLIF; saves that ran SLIF migrate automatically.

## Principles

1. **Contract first.** Only the pinned surface. An unknown consumer calling a
   missing function produces a loud, searchable Papyrus log line — that is the
   feature-request channel, not a reason to pre-build 84 unused functions.
2. **Detect, don't ask.** Body/profile detection replaces the reference's nine
   MCM pages. The user-facing MCM is one page (+ diagnostics).
3. **Bug-compatible where consumers depend on behavior** (dead Estrus keys stay
   no-ops; the arg-order swap stays).
4. **Own native engine — DECIDED: reimplement.** SLIF NG ships its own SKSE
   DLL (CommonLibSSE-NG, same toolchain as BF NG/SLANG) instead of depending
   on IF-NG. Rationale: IF-NG proved to be a thin morphs-only skee wrapper
   (~400 lines) with no node backend, no ramp, and a serialization bug — less
   work to reimplement correctly than to fork, fix, credit, and track.
   IF-NG's fork stays around as a pattern reference only (skee
   InterfaceExchange handshake); SKEE.h comes from RaceMenu's public SDK.
5. **State is native from day one**: one co-save store holding keyed per-mod
   contributions (SLANG serialization patterns), aggregation in C++.
   StorageUtil is demoted to a ONE-TIME migration importer for real-SLIF saves
   (CONTRACT §6). This dissolves the two-store reconciliation problem.

## Architecture

```
consumer .pex (BF NG / FHU / SLS / Estrus / MME)
   │  mod events                 │  direct globals
   ▼                             ▼
SLIF_ScannerAlias.psc  ──►  SLIF_Main.psc / SLIF_Morph.psc     (pinned shims)
                                 │  thin: unchanged-value early-out, then one
                                 ▼  native call
                        SLIFNG.dll (own engine, CommonLibSSE-NG)
                        - vocabulary map (6 keys, CONTRACT §5) + body profiles
                        - clamp (min/max/mult); keyed per-mod ledger
                          (actor → mod → target → value), co-save serialized
                          with SLANG patterns (ResolveFormID, bounds checks)
                        - aggregation: highest-wins per (actor, target),
                          ONE applied value under the single SLIF skee key
                          (CONTRACT §4.3)
                        - oldModName legacy-key cleanup
                        - apply: skee IBodyMorphInterface (morph-first) /
                          INiTransform node fallback, coalesced per event
                        - one-time StorageUtil import of real-SLIF saves (P6)
                        - (P8) task-based smooth ramp
```

## Storage model

**One native store, SKSE co-save.** SLIFNG.dll owns the keyed per-mod ledger:
actor FormID → (mod name → target → clamped value, + per-target min/max/mult).
Serialized via `SKSE::GetSerializationInterface`, cosave unique ID `'SLIF'` (uint32 = max 4 chars; free to claim - reference SLIF never had a DLL).
Nothing lives in the Papyrus save section; StorageUtil is used ONLY as a
one-time import source for saves that ran real SLIF (P6); disk JSON/INI is
config only (body profiles, user presets — P5).

**Source of truth is the ledger.** Applied skee values are derived output: on
load, recompute aggregates from the ledger and re-apply — self-healing by
design (same philosophy as BF NG's 3.5.14/15 state healing).

**Serialization template: SexlabArousedNG** (own project,
`C:\Playground\Skyrim\mods\SKSE\SexlabArousedNG`) — lift these patterns:
- `src/ArousalManager.cpp OnGameLoaded`: read the entry's payload FIRST, then
  `if (!serde->ResolveFormID(formId, newFormId)) continue;` — skipping an
  unresolvable actor without desyncing the stream; store under the RESOLVED id.
- `include/SerializationHelper.h`: bounds-checked reads — `ReadDataHelper<T>`
  decrements the remaining record `length` and throws on truncation; plus
  length-prefixed `WriteString`/`ReadString` (needed the moment keyed per-mod
  contributions — mod names are strings — go native, open question 5).
- Every index read from the co-save is range-checked (`std::out_of_range` on
  corrupt data), the whole load wrapped in try/catch that flags an error
  instead of crashing the load.
- `OpenRecord('DATA', kSerializationDataVersion)` + version gate on load;
  `SetRevertCallback` clears ALL state and invalidates lookup caches;
  `std::scoped_lock` on a member mutex in save AND load callbacks.
- Load-time self-heal precedent: recompute the derived value
  (`RecalculateArousal()`), warn on mismatch, keep the recomputed one — the
  exact "ledger is source of truth, recompute on load" rule above.


## Phases

### P0 — Spikes (decide before building)
- [ ] **Arity experiment:** compile a test caller against a shortened `inflate`
      signature, run against the full 11-param implementation; confirm the VM
      rejects it (expected). Documents the "frozen signature" rule with proof.
- [x] **Node→morph mapping — DONE: morph-first, node fallback.**
      Map the four live node keys onto morph blends through the body profiles
      (`slif_belly` → PregnancyBelly blend); keep the NiOverride node transform
      as fallback where the actor's body has no matching morph. The P0
      side-by-side screenshots on 3BA + BHUNP would VALIDATE this direction.
      Shipped and running in-game (SLS/FHU/BF NG all fold correctly on 3BA);
      no formal screenshot comparison was ever made.
- [x] **IF-NG evaluation — CLOSED: reimplement instead of depend.**
      Verified (fork == upstream, v1.1.0): thin morphs-only skee wrapper, no
      node backend, no ramp (the Nexus "Smooth Transitions" claim means "call
      repeatedly yourself"), raw-FormID serialization bug. Decision: SLIF NG
      ships its OWN DLL; IF-NG kept only as a pattern reference for the skee
      InterfaceExchange handshake. No dependency, no credit obligation, no
      upstream coordination.
- [x] **DLL skeleton bootstrap: DONE (builds clean).** xmake + shared local
      CommonLibSSE-NG checkout (BF NG's submodule — replace with own submodule
      at git init); SKEE.h (full 452-line community header via SexLab P+'s
      copy, incl. INiTransformInterface); SLANG-pattern serialization
      (ResolveFormID payload-first skip, bounds-checked reads, versioned
      record, revert, mutex); Papyrus natives registered; skee handshake at
      kPostPostLoad; ReapplyAll self-heal at kPostLoadGame.
      -> dist/Core/skse/plugins/SLIFNG.dll. Smoke (in-game): empty ledger
      round-trips a save — pending first launch.
- [ ] **Perf spike:** `ApplyBodyMorphs` cost per actor — confirm one call per
      actor per event is fine for 10+ actors during load-game re-apply.
- [ ] **Permissions:** shipping an ESP named `SexLab Inflation Framework.esp`
      replaces qotsafan's mod file-for-file — check SLIF's LoversLab
      permissions / contact the author before ANY public release. (The former
      IF-NG permission question is moot: no dependency after the reimplement
      decision. If any IF-NG-derived pattern ends up recognizable in code,
      credit Acook1e anyway as courtesy.)

### P1 — MVP (native core + pinned shims, instant apply) — DONE, RUNNING
- [x] `SLIFNG.dll` core: keyed per-mod ledger + serialization (from the P0
      skeleton), clamp, highest-wins aggregate, vocabulary map (6 keys),
      `oldModName` legacy-key cleanup — aggregation implemented as a
      parameterized fold (mode enum: highest | additive) even though P1 ships
      highest-only wiring, so the P4 toggle and P5 per-target overrides bolt
      on without touching the ledger — apply via skee morph-first with
      NiTransform node fallback — **one** apply/model-update per call, never
      per step (the reference's worst perf bug was per-step rebuilds).
- [x] Native Papyrus surface `SLIFNG.psc` (native functions the shims call:
      `Inflate`, `Morph`, `UnregisterMod`, `UnregisterTarget`, diagnostics
      getters).
- [x] Pinned shims wired: `SLIF_ScannerAlias.psc` (3 events),
      `SLIF_Main.psc` (`inflate`, `unregisterNode`, `unregisterActor`,
      `IsInstalled`), `SLIF_Morph.psc` (`morph`) — each shim = unchanged-value
      early-out (the reference lacks one; BF NG re-sends unchanged values every
      cycle tick) + one native call.

### P2 — Vocabulary + body profiles (per-ACTOR, not global) — CORE DONE
- [x] **Node fallback is now REACHABLE.** A key the actor's profile does not
      list has no usable slider on that body, so the apply path drives it with a
      skeleton node scale instead. Declaring absence in the profile is the only
      honest mechanism - see the note below on why detection is impossible.
- [x] **INI profiles** at `Data/SLIFNG/Bodies/*.ini`, loaded at kDataLoaded
      (race + plugin matchers need the data handler). Sections per vocabulary
      key with `FullScale` + `MorphN`/`MorphNMax`, i.e. BF NG's proven format:
      Max is the slider value at full node deviation, converted internally to
      weight-per-deviation. Ships default / CBBE 3BA / BHUNP / UBE. A compiled
      built-in default keeps a bare install working with no files at all.
- [x] **Per-actor resolution + cache**: race EditorID substring first (decisive
      for UBE), then plugin presence, else default.ini. Cached per FormID and
      cleared on revert. The ledger fold resolves blends per actor too, so a
      node key drives whatever THAT actor's body uses.
- [ ] MCM body override (pick a different installed profile without swapping
      files). The installer + race matching cover the common cases, so this is
      convenience, not correctness.
- [ ] **Auto-pick accuracy — the open question.** UBE resolves RELIABLY (race
      EditorID is per-actor hard evidence). 3BA / BHUNP / plain CBBE do NOT:
      plugin presence says a body is INSTALLED, not that BodySlide BUILT it,
      and several can be installed at once. A probe now ships
      (`cgf "SLIFNG_Debug.Probe"`) to settle whether skee's
      `VisitStrings`/`VisitMorphs` enumerate the slider names learned from the
      loaded morphs.tri:
        * if YES -> real detection is possible: pick the profile whose sliders
          are actually present, and warn when a profile names one that is not.
        * if it only returns names something already SET -> useless for
          detection, the declarative profile stands, and the remaining option is
          parsing the body's .tri ourselves.
      One launch decides it; do not build auto-detection before reading it.
- [x] **Slider names verified against real slider sets** (the `<Slider name>`
      values in a body's `.osp` are what BodySlide writes into morphs.tri, which
      is what skee matches):
      * CBBE 3BA - all five verified present.
      * UBE 2.0 - `PregnancyBelly` verified present (it shares CBBE's name);
        `BreastsSH`/`BreastsNewSH` are **absent**, so the placeholder profile
        was silently dead. Replaced with UBE's own `BreastsBigger`. Race matcher
        confirmed: EditorIDs are `00UBE_BretonRace` etc, so `UBE_` matches.
      * default.ini keeps BreastsSH AND BreastsNewSH as candidates (a missing
        slider contributes nothing) - noted in-file, same trade BF NG makes.
- [x] **BHUNP sourced from reference SLIF's own UUNP table.** SLIF generates
      three body variants of its mappings
      (`StorageUtilData/.../bodymorphs/{000_Default,001_UUNP,002_CBBE_SE}`), and
      that UUNP table is what it drove UUNP-family bodies with for years -
      the best source short of a BHUNP install. Corrected the guess: `Breasts`
      is not a UUNP slider; `BreastsSH` is.
- [x] **The two candidates are MUTUALLY EXCLUSIVE - no double-up.** Verified
      against CBBE 3BA Reference.osp and SLIF's UUNP table: `BreastsSH` is
      UUNP-only, `BreastsNewSH` is CBBE-only, and NO body carries both. So the
      generic profile's pair always fires exactly once. An earlier note warning
      of "roughly double the effect" was wrong and has been corrected in-file.
- [x] **BHUNP aligned to BF NG's own tuned BHUNP profile** (BreastsSSH 0.4 /
      DoubleMelon 0.25 / BreastsSmall 0.1), so a character looks the same
      whichever backend drives her.
- [x] **The CBBE/UUNP slider split is now documented, with evidence.** SLIF's
      CBBE SE table has `BreastsNewSH` and NO `BreastsSH`; its UUNP table has
      `BreastsSH` and NO `BreastsNewSH`. That split is precisely why one
      hardcoded mapping cannot serve both bodies. `PregnancyBelly` is in all
      three tables - the one name safe everywhere.
- [x] **Node vocabulary confirmed body-INDEPENDENT.** All three of SLIF's
      `lists/*.json` resolve our four keys identically (`NPC Belly`,
      `NPC L/R Breast`, `NPC L/R Butt`, `NPC GenitalsScrotum [GenScrot]`), so
      only the MORPH side needs per-body profiles. One hardcoded node table is
      correct.
- [x] ~~Known gap: the node fallback is unreachable for a morph-mapped key.~~
      `slif_belly`/`slif_breast` always take the morph path, so on a body whose
      BodySlide set lacks `PregnancyBelly`/`BreastsSH` nothing happens instead
      of falling back to a NiTransform node scale (PLAN P0 promised a fallback
      "where the actor's body has no matching morph"). skee exposes no cheap
      "does this body support slider X" probe, so the per-actor body PROFILE is
      the right place to answer it: a profile knows its own slider set, and a
      target with no usable slider routes to nodes. Blocked on this phase.

### P3 — Packaging
- [x] **FOMOD asks which body you built** - the best available answer, since no
      runtime probe beats being told. The chosen profile installs AS
      `default.ini` ("the body this game uses"), which retires the unreliable
      plugin matchers entirely. Options: CBBE 3BA (auto-Recommended when
      3BBB.esp/CBBE 3BA.esp is active), CBBE generic, BHUNP (Recommended on
      BHUNP.esp, and its description says the names are unverified).
      **UBE ships in Core regardless** - it is race-matched per ACTOR, so a UBE
      character and a 3BA character coexist in one save, each with the right
      sliders. The installer answer is only the fallback for everyone else.
      Changing body later needs no reinstall: replace that one file.
- [x] ESP `SexLab Inflation Framework.esp`: DONE (authored clean-room via
      houseCARL, mirroring the reference record structure read from the real
      1.2.2 ESP): quest `0x800` EditorID SLIF_Menu, Flags 273
      (start-game-enabled), PlayerAlias id 0 forced to PlayerRef, VMAD v5/of2
      fragment alias binding `SLIF_ScannerAlias` (Local); `0x801` stub quest
      SLIF_Scanner. SEQ generated. Masters: Skyrim.esm + Update.esm.
      ESL-flag decision deferred.
- [x] FOMOD ships `SLIFNG.dll` + ESP + SEQ + scripts together in Core;
      requirements stay minimal: SKSE, RaceMenu (skee). info.xml states
      "install INSTEAD of SLIF".
- [x] Mod VERSION: packed `(M)MmmPP` in `SLIF_Menu.GetVersion()` (100 =
      0.01.00), shown in the MCM header and used by SkyUI to fire
      OnVersionUpdate. Same scheme as ArousedBodyMorphs. **Bump it whenever
      Pages, ModName or the option layout changes** - it is what makes an added
      page appear on an existing save.
- [ ] Move the version to a data file the way BF NG does
      (`FWVersion.GetMCMVersion()` reads `BF_VersionMCM` from an INI), so a
      release bump needs no script recompile. Not urgent at one MCM script.
- [ ] ESL-flag decision for the ESP.

### P4 — Diagnostics ("Check my setup") — ACTOR PAGE DONE
- [x] **MCM "Actor" page**: subject toggle (player / crosshair target),
      identity (name, FormID, race, sex, 3D-loaded), body heuristic + REAL
      skeleton-node probe via `Get3D()->GetObjectByName`, per-mod contributions
      grouped by what they drive, and applied-vs-default per slider with the
      skee readback beside it (which proves our write landed - it is NOT an
      availability test, see the note in P2). One button writes the identical
      text to SLIFNG.log for bug reports. The ENGINE formats the report (`Report::ForActor` returns
      interleaved label/value pairs) so the MCM is a dumb printer and the log
      and page can never drift.
- [x] Per-slider magnitude knobs deliberately NOT in the MCM — a load order can
      drive dozens of sliders, and a page of per-slider sliders is the exact
      complexity this framework exists to remove. `SetTargetScale` stays in the
      engine for presets (P5); the MCM shows one overall magnitude.
- [x] Body identification: the resolved PROFILE is now shown alongside the old
      heuristic, so the two can be compared at a glance.
- [x] **Settings + diagnostics both landed in the SkyUI MCM**, not the
      SKSEMenuFramework window the earlier note planned: two MCM pages turned
      out to be enough, and it drops a dependency. Revisit only if a table
      outgrows MCM widgets.
- [x] Dead Estrus keys and unknown vocabulary are logged with their outcome
      (`dead key ... bug-compatible no-op`, `unknown node key ... ignored`)
      instead of vanishing.
- [ ] "Last N API calls" ring buffer in the Actor page (today the log carries
      the call history; the page shows current state only).

### P5 — Presets (user-facing)
- [x] **Magnitude scaling DONE** (cosave v3): a master multiplier plus
      per-target multipliers, applied ON TOP of the fold at apply time so no
      stored contribution is touched — consumers keep sending what they mean,
      the user decides how big it reads. Node targets scale the DEVIATION from
      neutral, not the raw value, so an un-inflated actor is never resized.
      A change re-applies every tracked actor (it moves no contribution, so it
      cannot ride the value early-out). MCM exposes overall/belly/breasts;
      `SLIFNG.SetTargetScale(id, v)` reaches any slider by name.
- [ ] Named presets: save/load the user's whole tuning as one file — per-region
      magnitudes, per-consumer-mod overrides (mute a mod, scale a mod's effect),
      body-profile override, instant/gradual choice. Simple INI (BF NG
      `Profile/` conventions), shareable.
- [ ] Ship 2–3 curated presets (subtle / default / exaggerated).
- [ ] Optional one-time IMPORT of a legacy SLIF `Presets.json` (read-only,
      best-effort mapping); the `SLIF_Config` presets API itself stays
      unimplemented — no consumer calls it (CONTRACT §8).

### P6 — Migration & cleanup  ← import BUILT, untested; uninstall path unbuilt
CONTRACT sec.6 promises a save that ran real SLIF migrates. The NiOverride side
is automatic (legacy-key cleanup on `oldModName`, and the SLIF_Menu/Scanner/Timer
stubs that stop SkyUI's config manager aborting); the StorageUtil ledger is now
imported too, but by an MCM BUTTON rather than "no user action" - a deliberate
departure from the contract's wording, so the import cannot fire on a save that
never ran SLIF. It has never been run against a real migrating save.

**The legacy layout, read from a real co-save** (NEFARAM Save33, 2026-09-14,
before SLIF NG was installed) - this is what an importer must walk:

```
global (none-keyed)
  slif_actor_list          FormList  [0x14]              tracked actors
  slif_complete_actor_list FormList  [0x14]
  slif_morph_actor_list    FormList  [0x14]
  slif_actor_name_list     StrList   ["Anna"]
  slif_installed / slif_valid_nioverride / slif_working   int flags

per actor
  slif_gender              int
  slif_mod_list            StrList   ["All Mods","Sexlab Survival"]   node-driving mods
  slif_morph_mod_list      StrList   ["All Mods","Fill her up"]       morph-driving mods
  <mod>slif_node_list      StrList   ["NPC Belly"]      which nodes THAT mod drives
  slif_morph_list_<mod>    StrList   ["PregnancyBelly"] which morphs THAT mod drives
  slif_morph_mod_list_<morph> StrList ["Fill her up"]   reverse index
  <mod><node>              float     + _min/_max/_mult/_increment   node contribution
  slif_<mod>_<morph>       float     e.g. slif_fillherup_pregnancybelly = 0.09
  slif_<morph>             float     the aggregate, e.g. slif_pregnancybelly = 0.09
```

Shape-for-shape the same model as our ledger (per mod, per target, plus bounds),
so the import is a translation rather than a reconstruction.

**Design consequence: the importer must be PAPYRUS-side.** StorageUtil is
PapyrusUtil's API and publishes no C++ interface for other SKSE plugins, so the
DLL cannot read these keys. A one-shot script has to walk them on first
`OnPlayerLoadGame` and push each row into the native ledger through the existing
natives, then mark the actor migrated. That also means PapyrusUtil becomes a
soft dependency for migration only - noted in P3's requirements.
- [x] **Legacy import DONE — an MCM button, not an automatic pass.**
      `SLIFNG_Migrate.psc` walks the legacy StorageUtil ledger (node side via
      `slif_mod_list` -> `<mod>slif_node_list` -> `<mod><node>` + bounds; morph
      side via `slif_morph_mod_list` -> `slif_morph_list_<mod>` ->
      `slif_<mod>_<morph>`) and pushes each row through the ordinary natives, so
      imported data goes through exactly the same clamp/fold/apply path as a
      live call. `-1.0` is used as the read default, which is already our
      "unspecified, keep defaults" sentinel. "All Mods" is skipped on both sides
      - it is SLIF's aggregate pseudo-mod and importing it would double
      everything. Node names arrive RAW ("NPC Belly") and resolve through the
      same path Fill Her Up's calls take.
      The button greys out once run (flag persisted in cosave v4, so it stays
      disabled for THAT save) and also when there is nothing to find; the label
      shows the actor count up front. Chosen over an automatic pass so a user
      who does not want old values simply never presses it - and so it cannot
      fire on a save that never ran SLIF.
- [ ] Uninstall path: clear our applied output for all tracked actors.

### P7 — Compatibility matrix (gate for any release)
Status from the live session logged 2026-09-15 (`SLIFNG.log`), CBBE 3BA body:

| Consumer | Path | Pass criteria | State |
|---|---|---|---|
| Beeing Female NG | `SLIF_Morph.morph` (belly + breasts) | growth + reset at birth | **partial** — growth observed (PregnancyBelly 0.247 -> 0.495, breasts 0.330 -> 0.660) and legacy key `BeeingFemale` cleaned once; **birth reset not yet seen** |
| FHU Baka | event inflate + `SLIF_Morph.morph` + `unregisterNode` | inflation + deflation, legacy keys cleared | **pass** — deflate sequence 0.09 -> 0.065 -> 0.025 -> 0 observed, `sr_FillHerUp.esp` cleaned once |
| Sexlab Survival | event inflate + `unregisterActor` | gluttony belly on player | **pass** — `slif_belly` 2.2 -> PregnancyBelly 0.16, early-out on the repeat |
| Estrus Spider | event inflate incl. dead keys | live keys scale; dead keys no-op + diagnostic | **not run** |
| Milk Mod Economy | `unregisterActor` only | stale SLIF output cleared; MME's own scaling untouched | **not run** |
| Old-SLIF save | migration | values survive, no double-apply, no orphan transforms | **not run** — blocked on P6, which is unbuilt |

Also proved incidentally: the cross-source fold (SLS `slif_belly` + BF NG
`morph:pregnancybelly` correctly resolved to max, 0.247 not 0.16), deferred
threading (API and Apply on different threads), and the cosave round-trip.

### P8 — Gradual growth (native ramp in SLIFNG.dll)
Task/timer-based interpolation toward target values, entirely off the Papyrus
VM, exposed as a per-consumer-mod toggle. Pure engine work in our own DLL.
- [ ] Interpolation task in the engine's update path; per-target rate;
      completion coalesces to one ApplyBodyMorphs. Never a Papyrus drain loop.

## Risks

| Risk | Mitigation |
|---|---|
| Unsurveyed consumers call absent functions | Tier-2 list (CONTRACT §7); loud log line; publish the contract and invite reports before wide release |
| Permissions on the ESP name / replacing SLIF | P0 spike; worst case rename ESP + ship a tiny SLIF-named forwarder, decide then |
| Own DLL = own maintenance (game updates) | CommonLibSSE-NG + Address Library keep it version-independent; same burden you already carry for BF NG and SLANG |
| skee interface changes | SKEE.h interface is versioned; pin + check `GetVersion` at handshake |
| Node-vs-morph look differs from reference | P0 screenshot comparison decides; keep NiOverride fallback path |
| Same-name scripts colliding with a half-removed real SLIF | installer instructions + P4 diagnostics detect leftover `SLIF_Calc.pex` etc. |

## Out of scope (permanently, unless a real consumer appears)

The SLIF `Presets.json` / `SLIF_Config` presets API (user presets are a SLIF NG
feature — P5 — not an API compatibility item), queue UI, six calculation types,
per-actor value editor, list-category system, `SLIF_Config` JSON path API,
translations beyond English at MVP.

## Open questions

1. ~~Node-vs-morph~~ **DECIDED: morph-first with node fallback** (P0
   screenshots validate, not decide).
2. ~~Aggregation default~~ **DECIDED: configurable fold, default
   highest-wins** (CONTRACT §4.3): global MCM toggle (P4) + per-target
   override (P5); additive = deviation-sum for node scales, plain sum for
   morphs, clamped after aggregation; mode switch = recompute + one re-apply.
3. Does SLIF NG absorb BF NG's own BodyMorph backend later (one scaling path
   instead of five `VisualScaling` modes)?
4. ~~Release identity~~ **DECIDED: standalone repo** (this folder); after the
   reimplement decision it is fully self-contained (own DLL, no IF-NG).
5. ~~Native ledger timing~~ **DECIDED: native from day one** in SLIFNG.dll
   (reimplement decision); StorageUtil is a one-time import source only.
   Residual: do we keep mirroring the two global StorageUtil lists
   (`slif_actor_list`, `slif_actor_name_list`) for unsurveyed mods that might
   READ them? Cheap to do; decide in P1.
