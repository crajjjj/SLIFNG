Scriptname SLIFNG Hidden
{SLIF NG native engine surface (SLIFNG.dll). The pinned SLIF_* shims route
here; the whole pipeline (legacy-key cleanup, vocabulary incl. dead-key
no-ops, clamp, keyed per-mod ledger, configurable fold, ONE coalesced skee
apply) runs natively. Values: node targets 1.0 = neutral, morphs 0.0 = neutral.
min/max/mult accept -1.0 = "keep defaults" (0 / 100 / 1.0).}

; API version of the native surface (1 = P1 MVP).
Int Function GetVersion() Global Native

; Returns true when the value changed and was applied (false = early-out,
; dead key, unknown key, or missing engine).
; slifKey accepts EITHER a slif_* key OR a raw skeleton node name ("NPC Belly"
; - what FHU sends); both resolve to the same canonical target.
Bool Function Inflate(Actor kActor, String modName, String slifKey, Float value, Float minimum, Float maximum, Float multiplier, String oldModName) Global Native
Bool Function Morph(Actor kActor, String modName, String morphName, Float value, Float minimum, Float maximum, Float multiplier, String oldModName) Global Native

Function UnregisterNode(Actor kActor, String slifKey, String modName) Global Native
Function UnregisterMorph(Actor kActor, String morphName, String modName) Global Native
Function UnregisterMod(Actor kActor, String modName) Global Native

; 0 = highest wins (default), 1 = additive. Switching recomputes and
; re-applies every tracked actor in one pass (CONTRACT sec.4.3).
Function SetAggregationMode(Int mode) Global Native
Int Function GetAggregationMode() Global Native

; Diagnostics: a mod's stored raw value / the folded applied value for a
; target ("slif_belly" or "morph:<slider>").
Float Function GetContribution(Actor kActor, String modName, String target) Global Native
Float Function GetApplied(Actor kActor, String target) Global Native

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

; Engine availability + a count, for the MCM status lines.
Bool Function IsMorphEngineReady() Global Native
Bool Function IsNodeEngineReady() Global Native
Int Function TrackedActorCount() Global Native

; Actor diagnostics, formatted by the engine: interleaved {label, value, ...}
; where an empty value marks a section header. Same text either way.
String[] Function GetActorReport(Actor kActor) Global Native
Function LogActorReport(Actor kActor) Global Native

; Probe: dump skee's known morph-name table (and one actor's morphs) to the
; log. Decides whether body detection is possible - see PLAN P2.
Function LogKnownMorphs(Actor kActor) Global Native

Function DumpLedger() Global Native
Function DumpActor(Actor kActor) Global Native
