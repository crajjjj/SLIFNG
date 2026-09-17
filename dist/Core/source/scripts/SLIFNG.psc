Scriptname SLIFNG Hidden
{SLIF NG native engine surface (SLIFNG.dll). The pinned SLIF_* shims route
here; the whole pipeline (legacy-key cleanup, vocabulary incl. dead-key
no-ops, clamp, keyed per-mod ledger, configurable fold, ONE coalesced skee
apply) runs natively. Values: node targets 1.0 = neutral, morphs 0.0 = neutral.
min/max/mult accept -1.0 = "keep defaults" (0 / 100 / 1.0).}

; API version of the native surface.
; 2: increment parameter on Inflate/Morph, incremental inflation, and the
;    enumeration surface for mod authors (IsTracked/GetTrackedActors/...).
Int Function GetVersion() Global Native

; Returns true when the value changed and was applied (false = early-out,
; dead key, unknown key, or missing engine).
; slifKey accepts EITHER a slif_* key OR a raw skeleton node name ("NPC Belly"
; - what FHU sends); both resolve to the same canonical target.
; increment: the per-row step for incremental inflation (SLIF's own knob;
; -1.0 = keep the default 0.1). Only consumed while incremental mode is on.
Bool Function Inflate(Actor kActor, String modName, String slifKey, Float value, Float minimum, Float maximum, Float multiplier, Float increment, String oldModName) Global Native
Bool Function Morph(Actor kActor, String modName, String morphName, Float value, Float minimum, Float maximum, Float multiplier, Float increment, String oldModName) Global Native

Function UnregisterNode(Actor kActor, String slifKey, String modName) Global Native
Function UnregisterMorph(Actor kActor, String morphName, String modName) Global Native
Function UnregisterMod(Actor kActor, String modName) Global Native

; Calculation type - SLIF's own six, SLIF's own Config.json numbering, SLIF's
; own default (CONTRACT sec.4.3):
;   0 = Top X (DEFAULT: largest + second/3 + third/6)   1 = Highest wins
;   2 = Subtract and add one (1 + sum of deviations)    3 = Square root
;   4 = Average                                         5 = Additive (plain sum)
; Applies to NODE scales only; direct morph contributions always sum, as in
; the reference. Switching recomputes and re-applies every tracked actor in
; one pass - with no stale leftovers, unlike the reference.
Function SetAggregationMode(Int mode) Global Native
Int Function GetAggregationMode() Global Native

; ---- the reference's READ surface (SLIF_Main/SLIF_Morph.Get*Value) ----
; target: a slif_* key, a raw skeleton node, or "morph:<slider>".
; modName "All Mods" reads the fold; any other name reads that mod's own row.
; An ABSENT key returns `default` - the reference reads StorageUtil with the
; caller's default, so "nothing registered" must not read as a neutral value.
Float Function GetValue(Actor kActor, String modName, String target, Float default) Global Native
Float Function GetMinValue(Actor kActor, String modName, String target, Float default) Global Native
Float Function GetMaxValue(Actor kActor, String modName, String target, Float default) Global Native

; ---- hidden nodes (Devious Devices pins the belly under a chastity belt) ----
; A hide OVERRIDES the fold instead of competing with it, and is keyed by
; actor+target rather than by mod, matching the reference's per-actor
; `<node>_hidden` flag. See Ledger.h for what "hidden" means on a morph body.
Function HideNode(Actor kActor, String modName, String slifKey, Float value, String oldModName) Global Native
Function ShowNode(Actor kActor, String modName, String slifKey) Global Native

; Diagnostics: a mod's stored raw value / the folded applied value for a
; target ("slif_belly" or "morph:<slider>").
Float Function GetContribution(Actor kActor, String modName, String target) Global Native
Float Function GetApplied(Actor kActor, String target) Global Native

; The reference's "slif_<morphName>": the cross-mod sum of direct morph
; contributions for one slider. SLIF_Morph mirrors it into StorageUtil under
; that exact name - Sexlab Survival reads it from StorageUtil directly.
Float Function GetCombinedMorph(Actor kActor, String morphName) Global Native

; ---- incremental inflation (the reference's "Inflation Type") ---------------
; Off (instant) by default - the reference's shipped default too. When on,
; node-value CHANGES step toward their new fold by each row's increment every
; quarter second, entirely off the Papyrus VM; hide/unregister stay instant.
; Turning it off snaps every in-flight ramp to its final value.
Function SetIncrementalInflation(Bool enabled) Global Native
Bool Function IsIncrementalInflation() Global Native

; ---- query surface for mod authors ------------------------------------------
; The values come through GetValue / GetApplied / GetContribution /
; GetCombinedMorph above; these answer "what is there to ask about".
; C++ plugins get the same surface via messaging - see src/API/SLIFNG_API.h.
Bool Function IsTracked(Actor kActor) Global Native
Actor[] Function GetTrackedActors() Global Native
; Canonical node keys with a stored contribution ("slif_belly", ...).
String[] Function GetNodeTargets(Actor kActor) Global Native
; BodySlide sliders with a stored direct contribution, original case.
String[] Function GetMorphTargets(Actor kActor) Global Native
; Which mods hold a contribution to one target (key, raw node, or
; "morph:<slider>").
String[] Function GetModsDriving(Actor kActor, String target) Global Native

; Diagnostics: write the full ledger (or one actor's entries) to SLIFNG.log -
; contributions, fold results, active mode. See SLIFNG_Debug.psc for
; console-callable wrappers (cgf).
; Per-call diagnostics (every API entry + apply + a skee readback per slider).
; Dev builds default ON; turn it off for normal play.
Function SetVerboseLogging(Bool enabled) Global Native

; ---- user magnitude scaling ----
; Rides on top of the fold at apply time; never changes what consumers stored,
; so it is safe to move at any point and applies retroactively to everyone.
; A scaleId is the lowercase SLIDER name for a morph target, or the canonical
; key for a node target: "pregnancybelly", "breasts", "slif_butt".
; 1.0 = unchanged, 0.0 = that output suppressed entirely.
Function SetMasterScale(Float scale) Global Native
Float Function GetMasterScale() Global Native
Function SetTargetScale(String scaleId, Float scale) Global Native
Float Function GetTargetScale(String scaleId) Global Native

; One-shot legacy-import marker, persisted with the save (cosave v4). The
; import walk itself lives in SLIFNG_Migrate.psc - StorageUtil is a Papyrus API
; with no C++ interface, so the DLL cannot read the legacy keys.
Bool Function HasMigrated() Global Native
Function SetMigrated(Bool done) Global Native

; Engine availability + a count, for the MCM status lines.
Bool Function IsMorphEngineReady() Global Native
Bool Function IsNodeEngineReady() Global Native
Int Function TrackedActorCount() Global Native

; Actor diagnostics, formatted by the engine: interleaved {label, value, ...}
; where an empty value marks a section header. Same text either way.
String[] Function GetActorReport(Actor kActor) Global Native
; The same report split in two, so the MCM can pair the halves into real
; columns instead of leaving the right half blank.
String[] Function GetActorReportLeft(Actor kActor) Global Native
String[] Function GetActorReportRight(Actor kActor) Global Native
Function LogActorReport(Actor kActor) Global Native

; Probe: dump skee's known morph-name table (and one actor's morphs) to the
; log. Decides whether body detection is possible - see PLAN P2.
Function LogKnownMorphs(Actor kActor) Global Native

Function DumpLedger() Global Native
Function DumpActor(Actor kActor) Global Native
