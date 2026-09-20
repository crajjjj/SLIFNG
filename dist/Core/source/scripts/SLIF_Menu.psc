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
int _oGradual
int _oSpeed
int _oMaster
int _oVerbose
int _oDump
int _oImport

; --- Actor page ---
int _oTarget
int _oRefresh
int _oLog
int _oReset

bool _verbose = true          ; mirrors the engine's dev default
bool _useCrosshair = false    ; false = player, true = whatever you are looking at

String Property PAGE_SETTINGS = "$SLIFNG_Page_Settings" AutoReadOnly Hidden
String Property PAGE_ACTOR = "$SLIFNG_Page_Actor" AutoReadOnly Hidden

Int Function GetVersion()
	Return SLIFNG_Version.GetVersion()
EndFunction

String Function ModVersion()
	return SLIFNG_Version.GetVersionString()
EndFunction

Event OnConfigInit()
	ModName = "SLIF NG"
	Debug.Notification("[SLIF NG] MCM menu initialized")
EndEvent

Event OnVersionUpdate(int a_version)
EndEvent

Event OnConfigOpen()
	{The ONE place Pages is built, following slw_menu.psc. SkyUI reads Pages when
	a menu is OPENED, not when it is registered, so here is early enough - and no
	save can then carry a stale page array, which is what the version-ladder
	rebuilds used to be for.

	Rebuilt UNCONDITIONALLY, with no length/name guard. A guard would have to
	read Pages before it has ever been assigned, and reading .length off an
	unassigned array is a Papyrus error that would abort this very callback -
	leaving Pages unbuilt and the menu blank. Two property writes per open cost
	nothing and cannot fail.}
	BuildPages()
EndEvent

Function BuildPages()
	Pages = new String[2]
	Pages[0] = PAGE_SETTINGS
	Pages[1] = PAGE_ACTOR
EndFunction

Event OnPageReset(String a_page)
	if a_page == PAGE_ACTOR
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

	; The mod's own name and its version number are not prose - no key.
	AddHeaderOption("SLIF NG " + ModVersion())
	AddHeaderOption("$SLIFNG_Hdr_Diagnostics")

	_oVersion = AddTextOption("$SLIFNG_Opt_EngineApi", SLIFNG.GetVersion())
	_oVerbose = AddToggleOption("$SLIFNG_Opt_Verbose", _verbose)

	_oEngine  = AddTextOption("$SLIFNG_Opt_Engine", EngineStatus())
	_oDump    = AddTextOption("$SLIFNG_Opt_Dump", "")

	_oActors  = AddTextOption("$SLIFNG_Opt_Tracked", SLIFNG.TrackedActorCount())
	AddEmptyOption()

	AddHeaderOption("$SLIFNG_Hdr_Behaviour")
	AddHeaderOption("$SLIFNG_Hdr_Migration")

	; ONE overall magnitude only. Per-slider multipliers exist in the engine
	; (SLIFNG.SetTargetScale) but are deliberately not surfaced here - a load
	; order can drive dozens of sliders and a page of per-slider knobs is the
	; exact complexity this framework exists to avoid. Presets (P5) set them.
	_oMode    = AddMenuOption("$SLIFNG_Opt_Mode", ModeName())
	_oImport  = AddTextOption("$SLIFNG_Opt_Import", ImportLabel(), ImportFlags())

	_oGradual = AddToggleOption("$SLIFNG_Opt_Gradual", SLIFNG.IsIncrementalInflation())
	_oSpeed   = AddSliderOption("$SLIFNG_Opt_Speed", SLIFNG.GetRampSpeed(), "{2}x", SpeedFlags())
	AddEmptyOption()

	_oMaster  = AddSliderOption("$SLIFNG_Opt_Master", SLIFNG.GetMasterScale(), "{2}x")
	AddEmptyOption()
EndFunction

; A STATUS row, not a button: the import runs automatically on the first load
; of a legacy save (SLIF_ScannerAlias.AutoMigrate). It stays clickable only in
; the "pending" state, as a manual fallback if the automatic pass never got to
; run (e.g. the alias failed to fill).
String Function ImportLabel()
	if SLIFNG.HasMigrated()
		return "$SLIFNG_Import_Done"
	elseIf SLIFNG_Migrate.CountLegacyActors() == 0
		return "$SLIFNG_Import_None"
	endIf
	; SkyUI's argument form: the txt keys this as "$SLIFNG_Import_Pending{}" and
	; drops the count into the "{}" in its value.
	return "$SLIFNG_Import_Pending{" + SLIFNG_Migrate.CountLegacyActors() + "}"
EndFunction

Int Function ImportFlags()
	if SLIFNG.HasMigrated() || SLIFNG_Migrate.CountLegacyActors() == 0
		return OPTION_FLAG_DISABLED
	endIf
	return OPTION_FLAG_NONE
EndFunction

; SLIF's own six calculation types, SLIF's numbering (0 = Top X is the
; reference's default). Folds across mods on one target - nodes and sliders
; alike; one mod's own node+morph layers still add.
; Array index == SLIF's calculation_type number, so the menu dialog's
; SetMenuDialogStartIndex/Accept index maps 1:1 onto the native value.
String[] Function ModeNames()
	String[] names = new String[6]
	names[0] = "$SLIFNG_Mode_TopX"
	names[1] = "$SLIFNG_Mode_Highest"
	names[2] = "$SLIFNG_Mode_SubAddOne"
	names[3] = "$SLIFNG_Mode_Sqrt"
	names[4] = "$SLIFNG_Mode_Average"
	names[5] = "$SLIFNG_Mode_Additive"
	return names
EndFunction

String Function ModeName()
	int mode = SLIFNG.GetAggregationMode()
	String[] names = ModeNames()
	if mode < 0 || mode >= names.length
		return names[0]
	endIf
	return names[mode]
EndFunction

String Function EngineStatus()
	if !SLIFNG.IsMorphEngineReady()
		return "$SLIFNG_Engine_NotFound"
	elseIf !SLIFNG.IsNodeEngineReady()
		return "$SLIFNG_Engine_MorphsOnly"
	endIf
	return "$SLIFNG_Engine_OK"
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
		return "$SLIFNG_Target_Crosshair"
	endIf
	return "$SLIFNG_Target_Player"
EndFunction

Function RenderActorPage()
	; REAL two columns. TOP_TO_BOTTOM only flows right once the LEFT column is
	; FULL, and this page is never that long - it sat one-sided. So the engine
	; hands back the report in two halves and we interleave them here, padding
	; whichever runs out first.
	SetCursorFillMode(LEFT_TO_RIGHT)

	Actor subject = SelectedActor()

	_oTarget  = AddTextOption("$SLIFNG_Opt_Showing", TargetName())
	_oRefresh = AddTextOption("$SLIFNG_Opt_Refresh", "")
	_oLog     = AddTextOption("$SLIFNG_Opt_LogActor", "")
	_oReset   = AddTextOption("$SLIFNG_Opt_Reset", "")

	if !subject
		AddHeaderOption("$SLIFNG_Hdr_NoTarget")
		AddEmptyOption()
		return
	endIf

	; Interleaved {label, value, ...}; an empty value means the pair is a header.
	; NOT translated: these come from the engine and are diagnostics - actor and
	; slider names, mod keys, numbers - the same text that goes to SLIFNG.log
	; and into bug reports.
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

Event OnOptionMenuOpen(int a_option)
	if a_option != _oMode
		return
	endIf
	SetMenuDialogOptions(ModeNames())
	SetMenuDialogStartIndex(SLIFNG.GetAggregationMode())
	SetMenuDialogDefaultIndex(0)
EndEvent

Event OnOptionMenuAccept(int a_option, int a_index)
	if a_option != _oMode
		return
	endIf
	; The dialog index IS SLIF's calculation_type number (see ModeNames).
	SLIFNG.SetAggregationMode(a_index)
	SetMenuOptionValue(_oMode, ModeName())
EndEvent

; Greyed out while inflation is instant: with no ramp there is nothing to pace.
int Function SpeedFlags()
	if SLIFNG.IsIncrementalInflation()
		return OPTION_FLAG_NONE
	endIf
	return OPTION_FLAG_DISABLED
EndFunction

Event OnOptionSliderOpen(int a_option)
	if a_option == _oMaster
		SetSliderDialogStartValue(SLIFNG.GetMasterScale())
		SetSliderDialogDefaultValue(1.0)
		SetSliderDialogRange(0.0, 3.0)
		SetSliderDialogInterval(0.05)
	elseIf a_option == _oSpeed
		SetSliderDialogStartValue(SLIFNG.GetRampSpeed())
		SetSliderDialogDefaultValue(1.0)
		SetSliderDialogRange(0.1, 5.0)
		SetSliderDialogInterval(0.1)
	endIf
EndEvent

Event OnOptionSliderAccept(int a_option, float a_value)
	if a_option == _oMaster
		; Re-applies every tracked actor: a magnitude change moves no stored
		; contribution, so it cannot ride the normal value early-out.
		SLIFNG.SetMasterScale(a_value)
		SetSliderOptionValue(a_option, a_value, "{2}x")
	elseIf a_option == _oSpeed
		; No re-apply: the ramp ticker reads this every step, so anything
		; in flight retimes itself and anything finished is already there.
		SLIFNG.SetRampSpeed(a_value)
		SetSliderOptionValue(a_option, a_value, "{2}x")
	endIf
EndEvent

Event OnOptionSelect(int a_option)
	if a_option == _oReset
		; The wipe itself is instant; whether the shape comes back is up to
		; the mods - some re-send every tick, some only on events.
		if ShowMessage("$SLIFNG_Msg_ResetActor", true, "$Yes", "$No")
			SLIFNG.UnregisterMod(SelectedActor(), "All Mods")
			Debug.Notification("$SLIFNG_Notif_ActorCleared")
			ForcePageReset()
		endIf
	elseIf a_option == _oGradual
		SLIFNG.SetIncrementalInflation(!SLIFNG.IsIncrementalInflation())
		SetToggleOptionValue(_oGradual, SLIFNG.IsIncrementalInflation())
		; the speed slider is meaningless while inflation is instant
		SetOptionFlags(_oSpeed, SpeedFlags())
	elseIf a_option == _oVerbose
		_verbose = !_verbose
		SLIFNG.SetVerboseLogging(_verbose)
		SetToggleOptionValue(_oVerbose, _verbose)
	elseIf a_option == _oDump
		SLIFNG.DumpLedger()
		Debug.Notification("$SLIFNG_Notif_Dumped")
	elseIf a_option == _oImport
		Int moved = SLIFNG_Migrate.Run()
		; English on purpose: SkyUI's "$KEY{arg}" substitution is a MENU feature,
		; and the HUD notification path has nothing like it - a composed string
		; there would only ever print verbatim.
		Debug.Notification("SLIF NG: imported " + moved + " contribution(s)")
		ForcePageReset()   ; redraw so the option greys out
	elseIf a_option == _oTarget
		_useCrosshair = !_useCrosshair
		ForcePageReset()
	elseIf a_option == _oRefresh
		ForcePageReset()
	elseIf a_option == _oLog
		SLIFNG.LogActorReport(SelectedActor())
		Debug.Notification("$SLIFNG_Notif_ReportWritten")
	endIf
EndEvent

Event OnOptionHighlight(int a_option)
	if a_option == _oMode
		SetInfoText("$SLIFNG_Info_Mode")
	elseIf a_option == _oMaster
		SetInfoText("$SLIFNG_Info_Master")
	elseIf a_option == _oVerbose
		SetInfoText("$SLIFNG_Info_Verbose")
	elseIf a_option == _oDump
		SetInfoText("$SLIFNG_Info_Dump")
	elseIf a_option == _oEngine
		SetInfoText("$SLIFNG_Info_Engine")
	elseIf a_option == _oTarget
		SetInfoText("$SLIFNG_Info_Target")
	elseIf a_option == _oImport
		SetInfoText("$SLIFNG_Info_Import")
	elseIf a_option == _oSpeed
		SetInfoText("$SLIFNG_Info_Speed")
	elseIf a_option == _oGradual
		SetInfoText("$SLIFNG_Info_Gradual")
	elseIf a_option == _oRefresh
		SetInfoText("$SLIFNG_Info_Refresh")
	elseIf a_option == _oReset
		SetInfoText("$SLIFNG_Info_Reset")
	elseIf a_option == _oLog
		SetInfoText("$SLIFNG_Info_Log")
	endIf
EndEvent
