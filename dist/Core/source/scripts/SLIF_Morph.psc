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
