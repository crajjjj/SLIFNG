Scriptname SLIF_Main Hidden
{SLIF NG - direct-call surface of the SLIF compatibility contract
(CONTRACT.md sec.3/4). Consumer .pex files bake these exact arities at their
call sites (Papyrus defaults do not exist in compiled code), so the three
public signatures below are FROZEN: same names, order, types, count.}

; Pinned caller: Beeing Female NG (11-arg call baked).
; gender/perspective: accepted and ignored (reference behavior).
; value: multiplicative node scale, 1.0 = neutral (BF NG sends scale + 1).
; oldModName: consumer's pre-SLIF NiOverride key to clean up (CONTRACT sec.4.2).
; min/max/mult/incr = -1.0 -> keep defaults 0 / 100 / 1.0 / 0.1.
Function inflate(Actor kActor, string modName, string node, float value, int gender = -1, int perspective = -1, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
	; increment feeds the native ramp when incremental inflation is on
	; (PLAN P8); with instant inflation it is stored and idle, as before.
	SLIFNG.Inflate(kActor, modName, node, value, minimum, maximum, multiplier, increment, oldModName)
EndFunction

; Pinned caller: Beeing Female NG (3-arg call baked).
; NOTE the (node, modName) order - opposite of the SLIF_unregisterNode event.
Function unregisterNode(Actor kActor, string node, string modName = "All Mods") Global
	SLIFNG.UnregisterNode(kActor, node, modName)
EndFunction

; Pinned caller: Milk Mod Economy via the SLIF_unregisterActor event (its only
; SLIF interaction). MME never registers first: MUST be a safe no-op that still
; clears stale "SexLab Inflation Framework.esp" output on the actor
; (CONTRACT sec.4.4 - that cleanup is why MME calls it).
Function unregisterActor(Actor kActor, string modName = "All Mods") Global
	SLIFNG.UnregisterMod(kActor, modName)
EndFunction

; Tier-2 (CONTRACT sec.7): no observed caller, kept as cheap ecosystem insurance.
Bool Function IsInstalled() Global
	return true
EndFunction

; =========================================================== read surface ====
; SURVEYED, not speculative: Sexlab Survival and Estrus Chaurus both READ back
; what SLIF holds. SLS computes _SLS_BodyInflationScale from three GetValue
; calls and gates a whole scene branch on the result, so a missing GetValue is
; not a quiet degradation - the VM logs "Static function GetValue not found on
; object slif_main" on every game-hour tick and SLS reads every actor as flat.
;
; "All Mods" is the reference's aggregate pseudo-mod: it means "the folded
; total", not a mod literally called that.

Float Function GetValue(Actor kActor, string modName, string node, float default = 0.0) Global
	return SLIFNG.GetValue(kActor, modName, node, default)
EndFunction

Float Function GetMinValue(Actor kActor, string modName, string node, float default = 0.0) Global
	return SLIFNG.GetMinValue(kActor, modName, node, default)
EndFunction

; NOTE the default of 100.0, not 0.0 - the reference's own asymmetry, kept.
Float Function GetMaxValue(Actor kActor, string modName, string node, float default = 100.0) Global
	return SLIFNG.GetMaxValue(kActor, modName, node, default)
EndFunction

; ====================================================== node hide / show =====
; Pinned caller: Devious Devices NG (zadLibs.SetNodeHidden), which pins
; "NPC Belly" flat while a chastity belt is worn and releases it on unequip.
; A hide must WIN over whatever Beeing Female or FHU is asking for, so it
; overrides the fold rather than joining it.

Function hideNode(Actor kActor, String modName, String node, float value = 0.0000001, string oldModName = "") Global
	SLIFNG.HideNode(kActor, modName, node, value, oldModName)
EndFunction

Function showNode(Actor kActor, String modName, String node) Global
	SLIFNG.ShowNode(kActor, modName, node)
EndFunction

; ===================================================== bulk / sync forms =====
; Pinned caller: Estrus Chaurus.
; inflateBoth takes a SYNC KEY ("slif_breast") and scales both its L/R nodes.
; Our vocabulary already models a sync pair as ONE target - both nodes always
; carry the same value - so this is plain inflate, not a second code path.
Function inflateBoth(Actor kActor, string modName, string syncKey, float value, int gender = -1, int perspective = -1, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
	SLIFNG.Inflate(kActor, modName, syncKey, value, minimum, maximum, multiplier, increment, oldModName)
EndFunction

; resetActor: drop a mod's hold on ONE node, or on everything when node is "".
; The reference's `value` is what it writes the node back to; we recompute from
; the remaining contributions instead, which is the same answer and cannot
; drift from what the other mods still want.
Function resetActor(Actor kActor, string modName = "All Mods", string node = "", float value = 1.0, int gender = -1, int newGender = -1, int perspective = -1, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
	if node == ""
		SLIFNG.UnregisterMod(kActor, modName)
	else
		SLIFNG.UnregisterNode(kActor, node, modName)
	endIf
EndFunction

; updateActorList: what consumers actually use it FOR is pushing NEW BOUNDS
; onto values they already registered - Estrus Chaurus's MCM calls it with
; fresh min/max whenever its max-scale sliders move. The reference's other
; job (re-pushing applied values that drifted from its store) has no
; equivalent here: nothing can drift. -1.0 leaves a bound as stored.
; Pinned from 1.2.2 bytecode (no observed caller yet; SGO4 is the intended
; adopter). One native call, one coalesced apply for the whole set - the
; reference looped inflate() per node instead.
Function inflateMultiple(Actor kActor, string modName, string[] nodes, float[] values, int gender, int perspective, string oldModName, float[] minimum, float[] maximum, float[] multiplier, float[] increment) Global
	SLIFNG.InflateMany(kActor, modName, nodes, values, minimum, maximum, multiplier, increment, oldModName)
EndFunction

Function updateActorList(String modName = "All Mods", string node = "", int gender = -1, int newGender = -1, int perspective = -1, string oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0) Global
	if node != ""
		SLIFNG.UpdateModBounds(modName, node, minimum, maximum, multiplier, increment)
	endif
EndFunction
