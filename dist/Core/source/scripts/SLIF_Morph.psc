Scriptname SLIF_Morph Hidden
{SLIF NG - body-morph surface of the SLIF compatibility contract
(CONTRACT.md sec.3/4). Signature FROZEN - pinned from 1.2.2 bytecode.

Composition: within one mod, its direct morph and its transformed node share
add (one intent); ACROSS mods the calculation type folds, for sliders exactly
as for nodes - see CONTRACT sec.4.3 for why this deviates from the
reference's untested always-sum corner. The StorageUtil mirror below stays
the raw direct sum, which is the reference bookkeeping consumers read.}

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
	; With incremental inflation on, a direct morph ramps in slider space by
	; `increment` per quarter second, like a node does.
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

; Pinned from 1.2.2 bytecode; documented in the write API but previously absent
; here, so a caller would have hit "Static function unregisterActor not found on
; object slif_morph". No surveyed consumer sends it - Milk Mod Economy uses
; SLIF_Main's - but a documented function has to exist.
;
; WIDER than the reference, which cleared only the morph side and left the same
; mod's node inflation in place: one ledger row per mod holds both, and leaving
; half the inflation stuck is the worse reading of "stop inflating this actor".
; The SLIF_unregisterMorphActor event resolves the same way.
Function unregisterActor(Actor kActor, string modName = "All Mods") Global
	SLIFNG.UnregisterMod(kActor, modName)
EndFunction

; Pinned caller: Bimbos of Skyrim, whose clearBimboMorphs bakes the 9-arg call
; (actor, "CCBimbo", "", 0.0, "", -1.0, -1.0, -1.0, -1.0) and then re-adds every
; slider on top of GetValue. Without this function that clear aborted in the VM
; and each "Reapply Morphs" stacked the morphs instead of rebuilding them.
;
; The reference's semantics, kept: every morph the mod holds on the actor (or
; the named one) is WRITTEN to `value` through morph() above, bounds and
; oldModName included. Not unregistered: an unregister cancels a ramp and snaps
; the body flat, so with incremental inflation on, Bimbos' clear-then-re-add
; would collapse and regrow the body on every re-apply. A write ramps from what
; the body shows, so the re-add a moment later only retargets that ramp. The
; mod's node inflation is untouched, as in the reference; "All Mods" writes
; every mod's morphs.
Function resetActor(Actor kActor, string modName = "All Mods", string morphName = "", float value = 0.0, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
	String[] morphs
	if morphName != ""
		morphs = new String[1]
		morphs[0] = morphName
	else
		morphs = SLIFNG.GetMorphTargets(kActor)
	endif
	Int i = 0
	While morphs && i < morphs.length
		String[] mods = SLIFNG.GetModsDriving(kActor, "morph:" + morphs[i])
		Int j = 0
		While mods && j < mods.length
			if modName == "All Mods"
				morph(kActor, mods[j], morphs[i], value, oldModName, minimum, maximum, multiplier, increment)
			elseif mods[j] == modName
				morph(kActor, modName, morphs[i], value, oldModName, minimum, maximum, multiplier, increment)
			endif
			j += 1
		EndWhile
		i += 1
	EndWhile
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

; Pinned from 1.2.2 bytecode (no observed caller yet; SGO4 sets several
; sliders per update and is the intended adopter). One native call, one
; coalesced apply; the StorageUtil mirrors are refreshed per name.
Function morphMultiple(Actor kActor, string modName, string[] morphNames, float[] values, string oldModName, float[] minimum, float[] maximum, float[] multiplier, float[] increment) Global
	if SLIFNG.MorphMany(kActor, modName, morphNames, values, minimum, maximum, multiplier, increment, oldModName) > 0
		Int i = 0
		While i < morphNames.length
			MirrorCombined(kActor, morphNames[i])
			i += 1
		EndWhile
	endif
EndFunction
