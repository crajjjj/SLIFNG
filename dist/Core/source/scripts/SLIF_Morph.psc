Scriptname SLIF_Morph Hidden
{SLIF NG - body-morph surface of the SLIF compatibility contract
(CONTRACT.md sec.3/4). Signature FROZEN - pinned from 1.2.2 bytecode.

Direct morph contributions aggregate ADDITIVELY across mods, always - the
calculation type applies to node scales only. That asymmetry is the
reference's (SLIF_Morph_Util.CalculateMorphValue) and is kept.}

; The one StorageUtil name a consumer reads out of SLIF directly rather than
; through any API: Sexlab Survival's _SLS_BodyInflationTracking does
;   StorageUtil.GetFloatValue(PlayerRef, "slif_" + "BreastsFantasy")
; to turn a morph back into an inflation percentage. The engine owns the value;
; this mirror keeps the reference's name populated so that read keeps working.
; (Best-effort: a whole-mod unregisterActor leaves a stale mirror behind, but
; SLS re-derives its scale right after unregistering, so nothing consumes the
; stale value. CONTRACT sec.6.)
Function MirrorCombined(Actor kActor, String morphName) Global
	float combined = SLIFNG.GetCombinedMorph(kActor, morphName)
	if combined == 0.0
		StorageUtil.UnsetFloatValue(kActor, "slif_" + morphName)
	else
		StorageUtil.SetFloatValue(kActor, "slif_" + morphName, combined)
	endif
EndFunction

; Pinned caller: Fill Her Up Baka (9-arg call baked; sends value / 10 and
; oldModName = "sr_FillHerUp.esp").
; morphName is a PASS-THROUGH BodySlide slider name - no vocabulary.
; value 0.0 = neutral and MUST clear the contribution (CONTRACT sec.4.1).
Function morph(Actor kActor, string modName, string morphName, float value, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
	; increment is stored for parity; direct morphs apply instantly even in
	; incremental mode (sliders DERIVED from a ramping node follow the ramp).
	if SLIFNG.Morph(kActor, modName, morphName, value, minimum, maximum, multiplier, increment, oldModName)
		MirrorCombined(kActor, morphName)
	endif
EndFunction

; Pinned caller: Devious Interests. We already had the native; only the shim
; was missing, so the call was aborting in the VM.
Function unregisterMorph(Actor kActor, string morphName, string modName = "All Mods") Global
	SLIFNG.UnregisterMorph(kActor, morphName, modName)
	MirrorCombined(kActor, morphName)
EndFunction

; ---- read surface (pinned caller: Sexlab Survival) ----
; "All Mods" is the aggregate pseudo-mod. An absent key returns `default`.
Float Function GetValue(Actor kActor, string modName, string morphName, float default = 0.0) Global
	return SLIFNG.GetValue(kActor, modName, "morph:" + morphName, default)
EndFunction

Float Function GetMinValue(Actor kActor, string modName, string morphName, float default = 0.0) Global
	return SLIFNG.GetMinValue(kActor, modName, "morph:" + morphName, default)
EndFunction

Float Function GetMaxValue(Actor kActor, string modName, string morphName, float default = 100.0) Global
	return SLIFNG.GetMaxValue(kActor, modName, "morph:" + morphName, default)
EndFunction
