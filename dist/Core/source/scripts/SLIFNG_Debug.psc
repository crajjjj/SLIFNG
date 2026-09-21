Scriptname SLIFNG_Debug Hidden
{Console test drivers, reachable as real console commands through
ConsoleUtil Extended - see dist\Core\skse\CustomConsole\SLIFNG_Debug.yaml,
which maps each subcommand onto one of the globals below.

  slifng ping
  slifng inflate slif_belly 2.0
  slifng morph PregnancyBelly 0.6
  slifng clear
  slifng calc 5                 ; 0 Top X (default) .. 5 Additive
  slifng gradual true
  slifng smoke

("slifng" is the alias; the full name SLIFNG_Debug works too. Args have
defaults, so `slifng inflate` alone inflates the belly to 2.0.)

These exist so the whole pipeline can be exercised without a consumer mod, and
verified from SLIFNG.log. NOTE what the log can and cannot tell you: a matching
skee readback proves the WRITE landed, not that the mesh changed. Anything about
whether a body actually moved has to be seen on screen.}

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

; Per-actor magnitude, applied to the player here.
Function ScaleA(String scaleId, Float value) Global
	SLIFNG.SetActorTargetScale(Game.GetPlayer(), scaleId, value)
EndFunction

; Semantic region on the player: drives whatever sliders the body profile's
; [<region>] section lists (nothing, logged, if the profile lacks it).
Function RPlayer(String modName, String region, Float value) Global
	SLIFNG.Inflate(Game.GetPlayer(), modName, "region:" + region, value, -1.0, -1.0, -1.0, -1.0, "")
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
	;    One value per mod (direct + transformed node share), then the calc
	;    type folds across mods - Top X here: 0.4 + 0.133/3 + 0.107/6 = 0.462;
	;    highest wins shows 0.4 (SmokeC alone). See CONTRACT 4.3.
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
