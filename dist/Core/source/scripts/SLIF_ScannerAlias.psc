Scriptname SLIF_ScannerAlias extends ReferenceAlias
{SLIF NG - event surface of the SLIF compatibility contract (CONTRACT.md sec.2).
Lives on the player alias of quest 0x800 in "SexLab Inflation Framework.esp".
Signatures are PINNED from SLIF SE 1.2.2 bytecode - do not alter.

ALL 21 of the reference's mod events are registered here, not only the three
with observed senders. A mod event is dispatched by NAME at runtime, so an
unregistered one does not fail loudly the way a missing direct call does - it
simply never arrives, and the sending mod looks broken for no visible reason.
That asymmetry is why the contract's "implement only what is observed" rule is
relaxed for events: the whole surface costs one registration and one forwarding
line each, and cannot be discovered from a log line the way a direct call can.}

Function RegisterForModEvents()
	; inflate side
	RegisterForModEvent("SLIF_inflate",               "OnSLIF_inflate")
	RegisterForModEvent("SLIF_registerActor",         "OnSLIF_registerActor")
	RegisterForModEvent("SLIF_unregisterActor",       "OnSLIF_unregisterActor")
	RegisterForModEvent("SLIF_updateActor",           "OnSLIF_updateActor")
	RegisterForModEvent("SLIF_resetActor",            "OnSLIF_resetActor")
	RegisterForModEvent("SLIF_unregisterNode",        "OnSLIF_unregisterNode")
	RegisterForModEvent("SLIF_setDefaultValues",      "OnSLIF_setDefaultValues")
	RegisterForModEvent("SLIF_setMinMax",             "OnSLIF_setMinMax")
	RegisterForModEvent("SLIF_setMin",                "OnSLIF_setMinimum")
	RegisterForModEvent("SLIF_setMinimum",            "OnSLIF_setMinimum")
	RegisterForModEvent("SLIF_setMax",                "OnSLIF_setMaximum")
	RegisterForModEvent("SLIF_setMaximum",            "OnSLIF_setMaximum")
	RegisterForModEvent("SLIF_setMult",               "OnSLIF_setMultiplier")
	RegisterForModEvent("SLIF_setMultiplier",         "OnSLIF_setMultiplier")
	RegisterForModEvent("SLIF_setIncr",               "OnSLIF_setIncrement")
	RegisterForModEvent("SLIF_setIncrement",          "OnSLIF_setIncrement")
	RegisterForModEvent("SLIF_hideNode",              "OnSLIF_hideNode")
	RegisterForModEvent("SLIF_showNode",              "OnSLIF_showNode")
	; morph side
	RegisterForModEvent("SLIF_morph",                 "OnSLIF_morph")
	RegisterForModEvent("SLIF_registerMorphActor",    "OnSLIF_registerMorphActor")
	RegisterForModEvent("SLIF_unregisterMorphActor",  "OnSLIF_unregisterMorphActor")
	RegisterForModEvent("SLIF_updateMorphActor",      "OnSLIF_updateMorphActor")
	RegisterForModEvent("SLIF_resetMorphActor",       "OnSLIF_resetMorphActor")
	RegisterForModEvent("SLIF_unregisterMorph",       "OnSLIF_unregisterMorph")
	RegisterForModEvent("SLIF_setMorphDefaultValues", "OnSLIF_setMorphDefaultValues")
EndFunction

Event OnInit()
	RegisterForModEvents()
EndEvent

Event OnPlayerLoadGame()
	RegisterForModEvents()
	; P6 legacy migration is NOT here, and is no longer automatic anywhere: it
	; is a button on the MCM's Settings page. Doing it from a load hook or from
	; SkyUI's registration callbacks is what broke other mods' menus in 0.4.8 -
	; see the note above ImportLabel in SLIF_Menu.psc.
EndEvent

; -- inflate side -------------------------------------------------------------
; WARNING: the event carries (modName, node); SLIF_Main.unregisterNode takes
; (node, modName). The swap is part of the contract - do not "fix" it.

Event OnSLIF_inflate(Form Sender, String modName, String node, float value, String oldModName = "")
	SLIF_Main.inflate(Sender as Actor, modName, node, value, -1, -1, oldModName)
EndEvent

Event OnSLIF_unregisterActor(Form Sender, String modName)
	SLIF_Main.unregisterActor(Sender as Actor, modName)
EndEvent

Event OnSLIF_unregisterNode(Form Sender, String modName, String node)
	SLIF_Main.unregisterNode(Sender as Actor, node, modName)
EndEvent

Event OnSLIF_resetActor(Form Sender, String modName, String node = "", float value = 1.0, String oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0)
	SLIF_Main.resetActor(Sender as Actor, modName, node, value, -1, -1, -1, oldModName, minimum, maximum, multiplier, increment)
EndEvent

Event OnSLIF_hideNode(Form Sender, String modName, String node, float value = 0.0000001, String oldModName = "")
	SLIF_Main.hideNode(Sender as Actor, modName, node, value, oldModName)
EndEvent

Event OnSLIF_showNode(Form Sender, String modName, String node)
	SLIF_Main.showNode(Sender as Actor, modName, node)
EndEvent

; registerActor / updateActor: the reference used these to create an actor row
; and seed its bounds BEFORE any value arrived. SLIF NG creates the row on first
; write, so only the bounds half carries over. Note UpdateActorBounds, not
; UpdateModBounds: these events carry a Sender, so they must move THAT actor's
; bounds. UpdateModBounds is the load-order-wide form behind updateActorList.
Event OnSLIF_registerActor(Form Sender, String modName, String node = "", String oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0)
	SLIFNG.UpdateActorBounds(Sender as Actor, modName, node, minimum, maximum, multiplier, increment)
EndEvent

Event OnSLIF_updateActor(Form Sender, String modName, String node = "", String oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0)
	SLIFNG.UpdateActorBounds(Sender as Actor, modName, node, minimum, maximum, multiplier, increment)
EndEvent

; The set* family, all bounds-only. -1.0 is our "leave as stored" sentinel, so
; each event moves exactly the field it names and nothing else.
Event OnSLIF_setDefaultValues(Form Sender, String modName, String node, float minimum = 0.0, float maximum = 100.0, float multiplier = 1.0, float increment = 0.1)
	SLIFNG.UpdateActorBounds(Sender as Actor, modName, node, minimum, maximum, multiplier, increment)
EndEvent

Event OnSLIF_setMinMax(Form Sender, String modName, String node, float minimum = 0.0, float maximum = 100.0)
	SLIFNG.UpdateActorBounds(Sender as Actor, modName, node, minimum, maximum, -1.0, -1.0)
EndEvent

Event OnSLIF_setMinimum(Form Sender, String modName, String node, float minimum = 0.0)
	SLIFNG.UpdateActorBounds(Sender as Actor, modName, node, minimum, -1.0, -1.0, -1.0)
EndEvent

Event OnSLIF_setMaximum(Form Sender, String modName, String node, float maximum = 100.0)
	SLIFNG.UpdateActorBounds(Sender as Actor, modName, node, -1.0, maximum, -1.0, -1.0)
EndEvent

Event OnSLIF_setMultiplier(Form Sender, String modName, String node, float multiplier = 1.0)
	SLIFNG.UpdateActorBounds(Sender as Actor, modName, node, -1.0, -1.0, multiplier, -1.0)
EndEvent

Event OnSLIF_setIncrement(Form Sender, String modName, String node, float increment = 0.1)
	SLIFNG.UpdateActorBounds(Sender as Actor, modName, node, -1.0, -1.0, -1.0, increment)
EndEvent

; -- morph side ---------------------------------------------------------------

Event OnSLIF_morph(Form Sender, String modName, String morphName, float value, string oldModName = "")
	SLIF_Morph.morph(Sender as Actor, modName, morphName, value, oldModName)
EndEvent

Event OnSLIF_unregisterMorph(Form Sender, String modName, String morphName)
	SLIF_Morph.unregisterMorph(Sender as Actor, morphName, modName)
EndEvent

; resetMorphActor: a named morph resets that morph; an empty name means "all of
; this mod's morphs", which for us is that mod's whole row on the actor.
Event OnSLIF_resetMorphActor(Form Sender, String modName, String morphName = "", float value = 1.0, String oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0)
	if morphName == ""
		SLIF_Main.unregisterActor(Sender as Actor, modName)
	else
		SLIF_Morph.unregisterMorph(Sender as Actor, morphName, modName)
	endIf
EndEvent

; WIDER than the reference: it cleared only the morph side, leaving the same
; mod's node inflation in place. SLIF NG holds both in one row per mod, so this
; clears both. Nothing observed sends it, and "stop inflating this actor" is the
; closer reading of the sender's intent than leaving half of it stuck.
Event OnSLIF_unregisterMorphActor(Form Sender, String modName)
	SLIF_Main.unregisterActor(Sender as Actor, modName)
EndEvent

; Morph bounds arrive WITH the value on SLIF_Morph.morph (min/max/mult are
; parameters there), so pre-registration has nothing left to do. Registered
; anyway: an unregistered event is invisible to whoever sent it.
Event OnSLIF_registerMorphActor(Form Sender, String modName, String morphName = "", String oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0)
EndEvent

Event OnSLIF_updateMorphActor(Form Sender, String modName, String morphName = "", String oldModName = "", float minimum = -1.0, float maximum = -1.0, float multiplier = -1.0, float increment = -1.0)
EndEvent

Event OnSLIF_setMorphDefaultValues(Form Sender, String modName, String morphName, float minimum = 0.0, float maximum = 100.0, float multiplier = 1.0, float increment = 0.1)
EndEvent
