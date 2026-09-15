Scriptname SLIFNG_Migrate Hidden
{One-shot import of a save that previously ran reference SLIF (CONTRACT sec.6).

WHY THIS IS PAPYRUS AND NOT C++: the legacy ledger lives in StorageUtil, which
is PapyrusUtil's Papyrus API. PapyrusUtil publishes no C++ interface for other
SKSE plugins, so SLIFNG.dll cannot read those keys at all - the walk has to
happen up here and push each row in through the natives.

Key construction below is taken from reference SLIF's own sources, and was
checked against the raw StorageUtil block of a real pre-SLIF-NG co-save:

  per actor
    slif_mod_list                     StringList  mods driving NODES
    <mod>slif_node_list               StringList  that mod's nodes
    <mod><node>                       Float       + _min/_max/_mult/_increment
    slif_morph_mod_list               StringList  mods driving MORPHS
    slif_morph_list_<mod>             StringList  that mod's morphs
    slif_<mod>_<morph>                Float       + _min/_max/_mult

"All Mods" is SLIF's aggregate pseudo-mod, not a contributor - importing it
would double every value, so it is skipped on both sides.}

String Function AllMods() Global
	return "All Mods"
EndFunction

; Imports every actor reference SLIF tracked. Returns the number of
; contributions moved across. Safe to run twice (values overwrite, they do not
; accumulate), but the MCM disables the button once it has run.
Int Function Run() Global
	Int moved = 0
	moved += ImportNodeSide()
	moved += ImportMorphSide()
	SLIFNG.SetMigrated(true)
	SLIFNG.DumpLedger()   ; leaves the imported state in the log for inspection
	return moved
EndFunction

Int Function ImportNodeSide() Global
	Int moved = 0
	Int count = StorageUtil.FormListCount(none, "slif_actor_list")
	Int i = 0
	While i < count
		Actor kActor = StorageUtil.FormListGet(none, "slif_actor_list", i) as Actor
		If kActor
			Int modCount = StorageUtil.StringListCount(kActor, "slif_mod_list")
			Int m = 0
			While m < modCount
				String modName = StorageUtil.StringListGet(kActor, "slif_mod_list", m)
				If modName != "" && modName != AllMods()
					moved += ImportNodesFor(kActor, modName)
				EndIf
				m += 1
			EndWhile
		EndIf
		i += 1
	EndWhile
	return moved
EndFunction

Int Function ImportNodesFor(Actor kActor, String modName) Global
	Int moved = 0
	Int nodeCount = StorageUtil.StringListCount(kActor, modName + "slif_node_list")
	Int n = 0
	While n < nodeCount
		String node = StorageUtil.StringListGet(kActor, modName + "slif_node_list", n)
		If node != "" && StorageUtil.HasFloatValue(kActor, modName + node)
			Float value = StorageUtil.GetFloatValue(kActor, modName + node, 1.0)
			; -1.0 as the default means "SLIF never stored a bound here", which is
			; exactly the sentinel our API reads as "keep the defaults".
			Float mn = StorageUtil.GetFloatValue(kActor, modName + node + "_min", -1.0)
			Float mx = StorageUtil.GetFloatValue(kActor, modName + node + "_max", -1.0)
			Float mu = StorageUtil.GetFloatValue(kActor, modName + node + "_mult", -1.0)
			; node is a RAW skeleton name here ("NPC Belly"); Inflate resolves
			; that to the canonical target exactly as Fill Her Up's calls do.
			If SLIFNG.Inflate(kActor, modName, node, value, mn, mx, mu, "")
				moved += 1
			EndIf
		EndIf
		n += 1
	EndWhile
	return moved
EndFunction

Int Function ImportMorphSide() Global
	Int moved = 0
	Int count = StorageUtil.FormListCount(none, "slif_morph_actor_list")
	Int i = 0
	While i < count
		Actor kActor = StorageUtil.FormListGet(none, "slif_morph_actor_list", i) as Actor
		If kActor
			Int modCount = StorageUtil.StringListCount(kActor, "slif_morph_mod_list")
			Int m = 0
			While m < modCount
				String modName = StorageUtil.StringListGet(kActor, "slif_morph_mod_list", m)
				If modName != "" && modName != AllMods()
					moved += ImportMorphsFor(kActor, modName)
				EndIf
				m += 1
			EndWhile
		EndIf
		i += 1
	EndWhile
	return moved
EndFunction

Int Function ImportMorphsFor(Actor kActor, String modName) Global
	Int moved = 0
	String prefix = "slif_" + modName + "_"
	Int morphCount = StorageUtil.StringListCount(kActor, "slif_morph_list_" + modName)
	Int n = 0
	While n < morphCount
		String morphName = StorageUtil.StringListGet(kActor, "slif_morph_list_" + modName, n)
		If morphName != "" && StorageUtil.HasFloatValue(kActor, prefix + morphName)
			Float value = StorageUtil.GetFloatValue(kActor, prefix + morphName, 0.0)
			Float mn = StorageUtil.GetFloatValue(kActor, prefix + morphName + "_min", -1.0)
			Float mx = StorageUtil.GetFloatValue(kActor, prefix + morphName + "_max", -1.0)
			Float mu = StorageUtil.GetFloatValue(kActor, prefix + morphName + "_mult", -1.0)
			If SLIFNG.Morph(kActor, modName, morphName, value, mn, mx, mu, "")
				moved += 1
			EndIf
		EndIf
		n += 1
	EndWhile
	return moved
EndFunction

; How much there is to import, without importing it - lets the MCM say whether
; the button would do anything before you press it.
Int Function CountLegacyActors() Global
	Int a = StorageUtil.FormListCount(none, "slif_actor_list")
	Int b = StorageUtil.FormListCount(none, "slif_morph_actor_list")
	If b > a
		return b
	EndIf
	return a
EndFunction
