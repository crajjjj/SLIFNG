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

; ------------------------------------------------------------- translation
; Every string this menu shows is a $key, resolved from
; Data/Interface/Translations/<data file>_<LANGUAGE>.txt - for us
; "SexLab Inflation Framework_ENGLISH.txt", because the game looks that file up
; by the DATA FILE name, not by ModName (SkyUI wiki, Localization: "modname is
; replaced by the name of the mod data file"). SL Widgets is the house
; reference for the rest of the idiom: one $PREFIX_ per mod, keys in the psc,
; prose only in the txt.
;
; The lookup is WHOLE-STRING - "$KEY" is replaced, "$KEY " + count is NOT - so
; a runtime value goes in through SkyUI's argument form instead: pass
; "$KEY{" + value + "}" and the txt holds the key as "$KEY{}" with a "{}"
; where the value lands (ImportLabel; Apropos2 and Mini Needs do the same).
; That form is SkyUI's, not the game's: it works on menu strings, not on
; Debug.Notification, which is why the two counted notifications stay English.
;
; A language with no file of its own shows raw $keys, not English - which is
; why the English file ships copied under every language name.
String Property PAGE_SETTINGS = "$SLIFNG_Page_Settings" AutoReadOnly Hidden
String Property PAGE_ACTOR = "$SLIFNG_Page_Actor" AutoReadOnly Hidden

; ---------------------------------------------------------------- versioning
; The SkyUI MCM versioning feature, in the SL Widgets idiom - see
; github.com/schlangster/skyui/wiki/MCM-Advanced-Features#Versioning.
;
; SkyUI fires OnConfigInit ONCE and `Pages` is a script PROPERTY, so it lives
; in the save from then on: a page added in a later build stays invisible to
; anyone already running the mod. OnVersionUpdate is the official channel for
; fixing that AND for one-shot save upgrades. The contract:
;   * The version number lives in ONE place, SLIFNG_Version.psc, packed from
;     the mod version as major*10000 + minor*100 + patch (SL Widgets'
;     slw_util convention: 0.2.0 -> 200, 1.2.3 -> 10203); this GetVersion()
;     only delegates. Bump the mod version whenever Pages, ModName, the
;     option layout, or the save-side data needs an upgrade step.
;   * OnConfigInit puts a FRESH install directly into the final state.
;   * OnVersionUpdate is a LADDER of cumulative blocks,
;         If (a_version >= N && CurrentVersion < N)
;     one per packed revision, each with a Debug.Trace - APPEND a new block
;     for a new revision, never edit an old one, so a save can climb any
;     number of versions in one load.
;
; THE FLOOR IS 122, NOT 1: a migrating save stores the reference SLIF_Menu's
; config version 122 and SkyUI only fires updates on an increase, so the
; packed version must stay above 122 forever - 0.2.0 (200) is the first
; legal mod version. Details in SLIFNG_Version.psc.
Int Function GetVersion()
	Return SLIFNG_Version.GetVersion()
EndFunction

String Function ModVersion()
	return SLIFNG_Version.GetVersionString()
EndFunction

Int Function ExpectedPageCount()
	return 2
EndFunction

Event OnConfigInit()
	{Fires ONCE, when the quest starts fresh: a new game, or SLIF NG added to a
	save that never had quest 0x800 running (e.g. the old SLIF was uninstalled
	earlier - its StorageUtil ghost may still be there, so the import runs
	here too).}
	BuildPages()
	TryLegacyImport()
EndEvent

Event OnVersionUpdate(int a_version)
	{Fires on game reload when GetVersion() outgrew the version stored in the
	save - for a migrating save that stored value is the REFERENCE SLIF_Menu's
	122, which is exactly the save CONTRACT sec.6 is about.

	Lock-aware: this runs while SkyUI's config manager holds its registration
	lock, so no calls into OTHER script INSTANCES here (that pattern froze
	ArousedBodyMorphs' predecessor). Our own properties, Global functions and
	natives only - none of those can contend the lock.}

	; a_version is the new version, CurrentVersion is the old version
	If (a_version >= 200 && CurrentVersion < 200)
		Debug.Trace(self + ": Updating script to version 200")
		; Arriving from reference SLIF (122) or a pre-200 SLIF NG dev build:
		; the save's Pages property still holds the OLD menu layout, and the
		; StorageUtil ledger may still be the reference's.
		BuildPages()
		TryLegacyImport()
	EndIf

	If (a_version >= 403 && CurrentVersion < 403)
		Debug.Trace(self + ": Updating script to version 403")
		; 0.4.3 moved every menu string to translation keys, the PAGE NAMES
		; included - a save from before it still holds the literal "Settings"
		; and "Actor", which would leave the tabs showing untranslated text and
		; OnPageReset falling through to the Settings branch for both.
		BuildPages()
	EndIf
EndEvent

; P6, automatic, via MCM versioning: walk the reference's StorageUtil state
; into the ledger (SLIFNG_Migrate) with zero user action. The cosave flag makes
; it one-shot per save whichever event lands first; a save with nothing to
; import is flagged too, so it is never re-scanned. All Global + native calls:
; safe under SkyUI's registration lock.
Function TryLegacyImport()
	if SLIFNG.HasMigrated()
		return
	endif
	if SLIFNG_Migrate.CountLegacyActors() == 0
		SLIFNG.SetMigrated(true)
		return
	endif
	Int moved = SLIFNG_Migrate.Run()
	; English on purpose: SkyUI's "$KEY{arg}" substitution is a MENU feature, and
	; the HUD notification path has nothing like it - a composed string there
	; would only ever print verbatim.
	Debug.Notification("SLIF NG: imported " + moved + " value(s) from the old SLIF save")
EndFunction

Event OnConfigOpen()
	; Self-heal, borrowed from SLO Aroused NG's `Pages.length < 4` guard: if a
	; save somehow carries a stale page array while reporting a current version,
	; rebuild anyway rather than showing a menu with pages missing.
	;
	; The name check is what carries the translation switch: a save written
	; before it stores the old hard-coded "Settings"/"Actor" and needs the
	; array rebuilt even though the page COUNT never changed - and its stored
	; config version can already be the current one, so no version-ladder block
	; would fire for it.
	if Pages.length != ExpectedPageCount() || Pages[0] != PAGE_SETTINGS
		BuildPages()
	endIf
EndEvent

Function BuildPages()
	; ModName is the mod's own name, not prose: it stays untranslated, the way
	; SL Widgets keeps "SLWidgets".
	ModName = "SLIF NG"
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
		; composed with a count, English - see TryLegacyImport
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
