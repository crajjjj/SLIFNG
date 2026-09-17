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
	Debug.Notification("SLIF NG: imported " + moved + " value(s) from the old SLIF save")
EndFunction

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

	AddHeaderOption("SLIF NG " + ModVersion())
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
	_oMode    = AddTextOption("Two mods, one target", ModeName())
	_oImport  = AddTextOption("Old-SLIF import", ImportLabel(), ImportFlags())

	_oGradual = AddToggleOption("Incremental inflation", SLIFNG.IsIncrementalInflation())
	AddEmptyOption()

	_oMaster  = AddSliderOption("Overall magnitude", SLIFNG.GetMasterScale(), "{2}x")
	AddEmptyOption()
EndFunction

; A STATUS row, not a button: the import runs automatically on the first load
; of a legacy save (SLIF_ScannerAlias.AutoMigrate). It stays clickable only in
; the "pending" state, as a manual fallback if the automatic pass never got to
; run (e.g. the alias failed to fill).
String Function ImportLabel()
	if SLIFNG.HasMigrated()
		return "done (automatic)"
	elseIf SLIFNG_Migrate.CountLegacyActors() == 0
		return "nothing to import"
	endIf
	return "pending: " + SLIFNG_Migrate.CountLegacyActors() + " actor(s)"
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
String Function ModeName()
	int mode = SLIFNG.GetAggregationMode()
	if mode == 1
		return "Highest wins"
	elseIf mode == 2
		return "Subtract and add one"
	elseIf mode == 3
		return "Square root"
	elseIf mode == 4
		return "Average"
	elseIf mode == 5
		return "Additive"
	endIf
	return "Top X"
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
		; Click cycles through the six types; wraps after Additive.
		int nextMode = SLIFNG.GetAggregationMode() + 1
		if nextMode > 5
			nextMode = 0
		endIf
		SLIFNG.SetAggregationMode(nextMode)
		SetTextOptionValue(_oMode, ModeName())
	elseIf a_option == _oGradual
		SLIFNG.SetIncrementalInflation(!SLIFNG.IsIncrementalInflation())
		SetToggleOptionValue(_oGradual, SLIFNG.IsIncrementalInflation())
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
		SetInfoText("How several mods driving the same target combine - SLIF's own six types, applied across mods to nodes and sliders alike (one mod's own node+morph layers still add).\nTop X (SLIF's default): largest + second/3 + third/6.  Highest wins: only the largest shows.\nSubtract and add one: 1 + summed deviations.  Square root: sqrt of summed squares.  Average.  Additive: plain sum.\nClick to cycle.")
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
		SetInfoText("Runs by itself through the MCM version update on the first load of a save that ran the old SLIF - every mod's per-actor values are copied across so a migrating character keeps her shape. This row only reports the outcome; click it only if it somehow still says pending.")
	elseIf a_option == _oGradual
		SetInfoText("Bodies swell toward a new value in steps (each mod's own increment, default 0.1 per quarter second) instead of snapping - the old SLIF's Incremental inflation type, run natively. Hiding a node and unregistering stay instant. Off = instant, the old SLIF's default.")
	elseIf a_option == _oRefresh
		SetInfoText("Re-read this actor's state. The page is a snapshot, not live.")
	elseIf a_option == _oLog
		SetInfoText("Writes exactly what this page shows to SKSE\\SLIFNG.log, so it can be pasted into a bug report.")
	endIf
EndEvent
