Scriptname SLIFNG_Debug Hidden
{Console test drivers - every function is callable via SKSE's cgf console
command with plain string/number args, so the whole pipeline can be exercised
and verified from SLIFNG.log without any consumer mod or gameplay.

  cgf "SLIFNG_Debug.Ping"
  cgf "SLIFNG_Debug.IPlayer" "TestMod" "slif_belly" 2.0
  cgf "SLIFNG_Debug.MPlayer" "TestMod" "PregnancyBelly" 0.6
  cgf "SLIFNG_Debug.UPlayer" "TestMod"
  cgf "SLIFNG_Debug.Mode" 1        ; 0 Top X (default) .. 5 Additive
  cgf "SLIFNG_Debug.Verbose" false
  cgf "SLIFNG_Debug.Gradual" true  ; incremental inflation on/off
  cgf "SLIFNG_Debug.Scale" 0.5
  cgf "SLIFNG_Debug.ScaleT" "pregnancybelly" 1.5
  cgf "SLIFNG_Debug.Dump"
  cgf "SLIFNG_Debug.Probe"       ; can skee tell us the body's sliders?
  cgf "SLIFNG_Debug.Report"      ; player diagnostics
  cgf "SLIFNG_Debug.ReportX"     ; crosshair target
  cgf "SLIFNG_Debug.SmokeTest"
}

; Liveness check: proves DLL + pex + registration all work.
Function Ping() Global
	Debug.Notification("SLIF NG v" + SLIFNG.GetVersion() + " mode " + SLIFNG.GetAggregationMode())
EndFunction

; Node-key inflate on the player (value 1.0 = neutral, 2.0 = double).
Function IPlayer(String modName, String slifKey, Float value) Global
	SLIFNG.Inflate(Game.GetPlayer(), modName, slifKey, value, -1.0, -1.0, -1.0, -1.0, "")
EndFunction

; Direct morph on the player (value 0.0 = neutral).
Function MPlayer(String modName, String morphName, Float value) Global
	SLIFNG.Morph(Game.GetPlayer(), modName, morphName, value, -1.0, -1.0, -1.0, -1.0, "")
EndFunction

Function UPlayer(String modName) Global
	SLIFNG.UnregisterMod(Game.GetPlayer(), modName)
EndFunction

; SLIF's calculation types, SLIF's numbering: 0 Top X (the default),
; 1 Highest wins, 2 Subtract and add one, 3 Square root, 4 Average, 5 Additive.
Function Mode(Int mode) Global
	SLIFNG.SetAggregationMode(mode)
EndFunction

Function Verbose(Bool enabled) Global
	SLIFNG.SetVerboseLogging(enabled)
EndFunction

; Incremental inflation: node changes step by their increment (default 0.1)
; every quarter second instead of snapping.
Function Gradual(Bool enabled) Global
	SLIFNG.SetIncrementalInflation(enabled)
	Debug.Notification("SLIF NG incremental inflation: " + enabled)
EndFunction

; User magnitude. 1.0 = as mods intended, 0.0 = suppressed.
Function Scale(Float value) Global
	SLIFNG.SetMasterScale(value)
EndFunction

Function ScaleT(String scaleId, Float value) Global
	SLIFNG.SetTargetScale(scaleId, value)
EndFunction

Function Dump() Global
	SLIFNG.DumpLedger()
EndFunction

Function DumpP() Global
	SLIFNG.DumpActor(Game.GetPlayer())
EndFunction

; Full actor diagnostics (body, contributions, applied-vs-default) to the log.
Function Report() Global
	SLIFNG.LogActorReport(Game.GetPlayer())
EndFunction

; Does skee know which sliders this body HAS? Run once and read SLIFNG.log.
Function Probe() Global
	SLIFNG.LogKnownMorphs(Game.GetPlayer())
EndFunction

Function ReportX() Global
	SLIFNG.LogActorReport(Game.GetCurrentCrosshairRef() as Actor)
EndFunction

; One-shot scripted smoke run: exercises inflate, overlap aggregation,
; dead keys, early-out, mode switch, morph path, and cleanup - then dumps.
; Read SLIFNG.log afterwards; the belly should visibly grow then reset.
Function SmokeTest() Global
	Actor player = Game.GetPlayer()
	Debug.Notification("SLIF NG smoke test - watch the log")

	; -- node-key aggregation (default calc = Top X, as in SLIF) --------------
	SLIFNG.Inflate(player, "SmokeA", "slif_belly", 2.0, -1.0, -1.0, -1.0, -1.0, "")   ; alone: fold 2.0
	SLIFNG.Inflate(player, "SmokeB", "slif_belly", 1.5, -1.0, -1.0, -1.0, -1.0, "")   ; Top X: 2.0 + 1.5/3 = 2.5
	SLIFNG.Inflate(player, "SmokeA", "slif_belly", 2.0, -1.0, -1.0, -1.0, -1.0, "")   ; must early-out

	; -- CROSS-SOURCE: a direct morph on the SAME slider slif_belly drives.
	;    Slider value = direct sum + blend(fold): morphs never fold, they add
	;    (the reference's slif_<morph> + slif_scale_<morph> composition).
	SLIFNG.Morph(player, "SmokeC", "PregnancyBelly", 0.4, -1.0, -1.0, -1.0, -1.0, "")

	; -- RAW NODE NAME: exactly what FHU sends ("NPC Belly", not slif_belly).
	;    Must route to the slif_belly target - same ledger entry family, so it
	;    aggregates with SmokeA/B instead of being rejected or clobbering.
	SLIFNG.Inflate(player, "SmokeD", "NPC Belly", 1.8, -1.0, -1.0, -1.0, -1.0, "")    ; Top X: 2 + 1.8/3 + 1.5/6 = 2.85

	; -- contract edge cases --------------------------------------------------
	SLIFNG.Inflate(player, "SmokeA", "slif_breast01", 3.0, -1.0, -1.0, -1.0, -1.0, "") ; dead key no-op
	SLIFNG.Inflate(player, "SmokeA", "slif_bogus", 3.0, -1.0, -1.0, -1.0, -1.0, "")    ; unknown key
	SLIFNG.Morph(player, "SmokeA", "BreastsNewSH", 0.5, -1.0, -1.0, -1.0, -1.0, "")    ; unrelated slider

	; -- calc-type switch recomputes everything -------------------------------
	SLIFNG.SetAggregationMode(1)                                                 ; highest wins: 2.0
	SLIFNG.DumpLedger()
	Utility.Wait(3.0)
	SLIFNG.SetAggregationMode(5)                                                 ; additive (plain sum): 5.3
	Utility.Wait(3.0)
	SLIFNG.SetAggregationMode(0)                                                 ; back to Top X: 2.85

	; -- teardown: SmokeA leaves B and C behind (must NOT wipe them), the
	;    last unregister empties the ledger and clears all owned output.
	SLIFNG.UnregisterMod(player, "SmokeA")
	SLIFNG.DumpLedger()
	Utility.Wait(2.0)                                                            ; belly still inflated by B/C
	SLIFNG.UnregisterMod(player, "SmokeB")
	SLIFNG.UnregisterMod(player, "SmokeC")
	SLIFNG.UnregisterMod(player, "SmokeD")
	SLIFNG.DumpLedger()
	Debug.Notification("SLIF NG smoke test done - body should be back to normal")
EndFunction
