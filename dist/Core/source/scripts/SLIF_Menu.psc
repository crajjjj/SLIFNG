Scriptname SLIF_Menu extends SKI_ConfigBase
{SLIF NG settings menu, attached to quest 0x800 of
"SexLab Inflation Framework.esp".

WHY THIS MUST EXIST, even before the real MCM is designed: a save that ran
reference SLIF has a SLIF_Menu instance registered with SkyUI. Because SLIF NG
ships the same plugin name and the same FormID, that form still RESOLVES on
load - so SkyUI's entry is a live object whose script class is missing, and
SKI_ConfigManager.Cleanup() dies on it with

    error: Method GetFormID not found on SLIF_Menu. Aborting call

which aborts the cleanup loop and leaves the ENTIRE MCM list empty. (A mod that
is simply uninstalled does not do this: its form is gone, so SkyUI skips a plain
None.) Shipping a real SKI_ConfigBase here is what keeps migrating users' menus
alive.}

; --- Settings page ---
int _oVersion
int _oEngine
int _oActors
int _oMode
int _oMaster
int _oVerbose
int _oDump
int _oImport

; --- Actor page ---
int _oTarget
int _oRefresh
int _oLog

bool _verbose = true          ; mirrors the engine's dev default
bool _useCrosshair = false    ; false = player, true = whatever you are looking at

; ---------------------------------------------------------------- versioning
; SkyUI fires OnConfigInit ONCE and `Pages` is a script PROPERTY, so it lives in
; the save from then on: a page added in a later build stays invisible to anyone
; already running the mod. Three sibling projects solve this three ways; this
; takes the useful half of each.
;
; Packed (M)MmmPP -- 100 => 0.01.00, 10203 => 1.02.03. Same scheme as
; ArousedBodyMorphs. Bump alongside the mod version whenever Pages, ModName, or
; the option layout changes.
Int Function GetVersion()
	return 100
EndFunction

String Function VersionString()
	Int v = GetVersion()
	return (v / 10000) + "." + ((v / 100) % 100) + "." + (v % 100)
EndFunction

Int Function ExpectedPageCount()
	return 2
EndFunction

Event OnConfigInit()
	BuildPages()
EndEvent

Event OnVersionUpdate(int a_version)
	{Deliberately LIGHT. This runs during SkyUI's registration with the script
	lock contended - reaching across to another quest or alias here froze the
	game in ArousedBodyMorphs' predecessor. Setting our own properties is safe;
	anything else belongs in OnGameReload.}
	BuildPages()
EndEvent

Event OnConfigOpen()
	; Self-heal, borrowed from SLO Aroused NG's `Pages.length < 4` guard: if a
	; save somehow carries a stale page array while reporting a current version,
	; rebuild anyway rather than showing a menu with pages missing.
	if Pages.length != ExpectedPageCount()
		BuildPages()
	endIf
EndEvent

Function BuildPages()
	ModName = "SLIF NG"
	Pages = new String[2]
	Pages[0] = "Settings"
	Pages[1] = "Actor"
EndFunction

Event OnPageReset(String a_page)
	if a_page == "Actor"
		RenderActorPage()
	else
		RenderSettingsPage()
	endIf
EndEvent

; =============================================================== Settings ====

Function RenderSettingsPage()
	; TWO COLUMNS. TOP_TO_BOTTOM only spills into the right column once the LEFT
	; one is full, so a short page like this leaves half the menu blank.
	; LEFT_TO_RIGHT instead fills alternately - every Add call takes the next
	; slot, left then right - so options are written in PAIRS and AddEmptyOption
	; pads whichever column runs out first.
	SetCursorFillMode(LEFT_TO_RIGHT)

	AddHeaderOption("SLIF NG " + VersionString())
	AddHeaderOption("Diagnostics")

	_oVersion = AddTextOption("Engine API version", SLIFNG.GetVersion())
	_oVerbose = AddToggleOption("Verbose logging", _verbose)

	_oEngine  = AddTextOption("RaceMenu / skee", EngineStatus())
	_oDump    = AddTextOption("Dump to SLIFNG.log", "")

	_oActors  = AddTextOption("Tracked actors", SLIFNG.TrackedActorCount())
	AddEmptyOption()

	AddHeaderOption("Behaviour")
	AddHeaderOption("Migration")

	; ONE overall magnitude only. Per-slider multipliers exist in the engine
	; (SLIFNG.SetTargetScale) but are deliberately not surfaced here - a load
	; order can drive dozens of sliders and a page of per-slider knobs is the
	; exact complexity this framework exists to avoid. Presets (P5) set them.
	_oMode   = AddTextOption("Two mods, one target", ModeName())
	_oImport = AddTextOption("Import from old SLIF", ImportLabel(), ImportFlags())

	_oMaster = AddSliderOption("Overall magnitude", SLIFNG.GetMasterScale(), "{2}x")
	AddEmptyOption()
EndFunction

; Disabled once it has run (the flag rides in the co-save, so it stays disabled
; across reloads of THIS save) and also when there is simply nothing to import.
String Function ImportLabel()
	if SLIFNG.HasMigrated()
		return "done"
	elseIf SLIFNG_Migrate.CountLegacyActors() == 0
		return "nothing found"
	endIf
	return SLIFNG_Migrate.CountLegacyActors() + " actor(s)"
EndFunction

Int Function ImportFlags()
	if SLIFNG.HasMigrated() || SLIFNG_Migrate.CountLegacyActors() == 0
		return OPTION_FLAG_DISABLED
	endIf
	return OPTION_FLAG_NONE
EndFunction

String Function ModeName()
	if SLIFNG.GetAggregationMode() == 1
		return "Additive"
	endIf
	return "Highest wins"
EndFunction

String Function EngineStatus()
	if !SLIFNG.IsMorphEngineReady()
		return "NOT FOUND"
	elseIf !SLIFNG.IsNodeEngineReady()
		return "morphs only"
	endIf
	return "OK"
EndFunction

; ================================================================== Actor ====

Actor Function SelectedActor()
	if _useCrosshair
		return Game.GetCurrentCrosshairRef() as Actor
	endIf
	return Game.GetPlayer()
EndFunction

String Function TargetName()
	if _useCrosshair
		return "Crosshair target"
	endIf
	return "Player"
EndFunction

Function RenderActorPage()
	; REAL two columns. TOP_TO_BOTTOM only flows right once the LEFT column is
	; FULL, and this page is never that long - it sat one-sided. So the engine
	; hands back the report in two halves and we interleave them here, padding
	; whichever runs out first.
	SetCursorFillMode(LEFT_TO_RIGHT)

	Actor subject = SelectedActor()

	_oTarget  = AddTextOption("Showing", TargetName())
	_oRefresh = AddTextOption("Refresh", "")
	_oLog     = AddTextOption("Write to SLIFNG.log", "")
	AddEmptyOption()

	if !subject
		AddHeaderOption("Nothing under the crosshair")
		AddEmptyOption()
		return
	endIf

	; Interleaved {label, value, ...}; an empty value means the pair is a header.
	String[] left = SLIFNG.GetActorReportLeft(subject)
	String[] right = SLIFNG.GetActorReportRight(subject)

	Int rows = left.length
	if right.length > rows
		rows = right.length
	endIf

	Int i = 0
	While i < rows - 1
		EmitRow(left, i)
		EmitRow(right, i)
		i += 2
	EndWhile
EndFunction

; One report row into the next slot, or a blank when that half has run out.
Function EmitRow(String[] rowsArr, Int i)
	if i + 1 >= rowsArr.length
		AddEmptyOption()
	elseIf rowsArr[i + 1] == ""
		AddHeaderOption(rowsArr[i])
	else
		AddTextOption(rowsArr[i], rowsArr[i + 1])
	endIf
EndFunction

; =================================================================== input ===

Event OnOptionSliderOpen(int a_option)
	if a_option != _oMaster
		return
	endIf
	SetSliderDialogStartValue(SLIFNG.GetMasterScale())
	SetSliderDialogDefaultValue(1.0)
	SetSliderDialogRange(0.0, 3.0)
	SetSliderDialogInterval(0.05)
EndEvent

Event OnOptionSliderAccept(int a_option, float a_value)
	if a_option != _oMaster
		return
	endIf
	; Re-applies every tracked actor: a magnitude change moves no stored
	; contribution, so it cannot ride the normal value early-out.
	SLIFNG.SetMasterScale(a_value)
	SetSliderOptionValue(a_option, a_value, "{2}x")
EndEvent

Event OnOptionSelect(int a_option)
	if a_option == _oMode
		if SLIFNG.GetAggregationMode() == 1
			SLIFNG.SetAggregationMode(0)
		else
			SLIFNG.SetAggregationMode(1)
		endIf
		SetTextOptionValue(_oMode, ModeName())
	elseIf a_option == _oVerbose
		_verbose = !_verbose
		SLIFNG.SetVerboseLogging(_verbose)
		SetToggleOptionValue(_oVerbose, _verbose)
	elseIf a_option == _oDump
		SLIFNG.DumpLedger()
		Debug.Notification("SLIF NG: state written to SLIFNG.log")
	elseIf a_option == _oImport
		Int moved = SLIFNG_Migrate.Run()
		Debug.Notification("SLIF NG: imported " + moved + " contribution(s)")
		ForcePageReset()   ; redraw so the option greys out
	elseIf a_option == _oTarget
		_useCrosshair = !_useCrosshair
		ForcePageReset()
	elseIf a_option == _oRefresh
		ForcePageReset()
	elseIf a_option == _oLog
		SLIFNG.LogActorReport(SelectedActor())
		Debug.Notification("SLIF NG: actor report written to SLIFNG.log")
	endIf
EndEvent

Event OnOptionHighlight(int a_option)
	if a_option == _oMode
		SetInfoText("Highest wins: the largest contribution shows, the others are masked.\nAdditive: contributions stack, clamped by their bounds.")
	elseIf a_option == _oMaster
		SetInfoText("Scales EVERYTHING this framework applies. 1.00x leaves mods exactly as they intended; 0.00x suppresses all inflation. Applies instantly to every tracked actor.")
	elseIf a_option == _oVerbose
		SetInfoText("Logs every API call and every apply, with a skee readback per slider. Useful for diagnosis; turn it off for normal play.")
	elseIf a_option == _oDump
		SetInfoText("Writes every tracked actor's contributions and fold results to SKSE\\SLIFNG.log.")
	elseIf a_option == _oEngine
		SetInfoText("Whether SLIF NG found RaceMenu's skee interfaces. 'morphs only' means node scaling is unavailable.")
	elseIf a_option == _oTarget
		SetInfoText("Switch between the player and whatever is under your crosshair. Close the menu, look at an NPC, reopen.")
	elseIf a_option == _oImport
		SetInfoText("Copies what reference SLIF stored in THIS save - every mod's per-actor belly/breast values - into SLIF NG, so a migrating character keeps her shape. Runs once, then greys out. Harmless to skip: mods re-send their values as play continues.")
	elseIf a_option == _oRefresh
		SetInfoText("Re-read this actor's state. The page is a snapshot, not live.")
	elseIf a_option == _oLog
		SetInfoText("Writes exactly what this page shows to SKSE\\SLIFNG.log, so it can be pasted into a bug report.")
	endIf
EndEvent
