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
	; increment belongs to the reference's gradual queue - accepted and unused
	; until the native ramp (PLAN P8). Everything else runs in SLIFNG.dll.
	SLIFNG.Inflate(kActor, modName, node, value, minimum, maximum, multiplier, oldModName)
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
