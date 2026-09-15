Scriptname SLIF_ScannerAlias extends ReferenceAlias
{SLIF NG - event surface of the SLIF compatibility contract (CONTRACT.md sec.2).
Lives on the player alias of quest 0x800 in "SexLab Inflation Framework.esp".
Signatures are PINNED from SLIF SE 1.2.2 bytecode - do not alter.}

; Only the three events with observed senders are registered (contract scope
; rule). An unknown SLIF_* event sent by an unsurveyed mod simply never fires
; here; unknown DIRECT calls log loudly instead - see CONTRACT.md sec.7.
Function RegisterForModEvents()
	RegisterForModEvent("SLIF_inflate",          "OnSLIF_inflate")
	RegisterForModEvent("SLIF_unregisterActor",  "OnSLIF_unregisterActor")
	RegisterForModEvent("SLIF_unregisterNode",   "OnSLIF_unregisterNode")
EndFunction

Event OnInit()
	RegisterForModEvents()
EndEvent

Event OnPlayerLoadGame()
	RegisterForModEvents()
	; TODO(P6): legacy real-SLIF save migration + one coalesced re-apply per actor
EndEvent

; -- pinned handlers ----------------------------------------------------------
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
