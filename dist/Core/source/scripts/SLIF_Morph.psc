Scriptname SLIF_Morph Hidden
{SLIF NG - body-morph surface of the SLIF compatibility contract
(CONTRACT.md sec.3/4). Signature FROZEN - pinned from 1.2.2 bytecode.}

; Pinned caller: Fill Her Up Baka (9-arg call baked; sends value / 10 and
; oldModName = "sr_FillHerUp.esp").
; morphName is a PASS-THROUGH BodySlide slider name - no vocabulary.
; value 0.0 = neutral and MUST clear the contribution (CONTRACT sec.4.1).
Function morph(Actor kActor, string modName, string morphName, float value, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
	; increment: reference gradual-queue param, unused until PLAN P8.
	SLIFNG.Morph(kActor, modName, morphName, value, minimum, maximum, multiplier, oldModName)
EndFunction

; Pinned caller: Devious Interests. We already had the native; only the shim
; was missing, so the call was aborting in the VM.
Function unregisterMorph(Actor kActor, string morphName, string modName = "All Mods") Global
	SLIFNG.UnregisterMorph(kActor, morphName, modName)
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
