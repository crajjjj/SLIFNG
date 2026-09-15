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

int _oVersion
int _oEngine
int _oActors
int _oMode
int _oMaster
int _oBelly
int _oBreast
int _oVerbose
int _oDump

bool _verbose = true   ; mirrors the engine's dev default

Event OnConfigInit()
	ModName = "SLIF NG"
	Pages = new String[1]
	Pages[0] = "$SLIFNG_Settings"
EndEvent

Event OnPageReset(String a_page)
	SetCursorFillMode(TOP_TO_BOTTOM)

	AddHeaderOption("SLIF NG")
	_oVersion = AddTextOption("Engine API version", SLIFNG.GetVersion())
	_oEngine  = AddTextOption("RaceMenu / skee", EngineStatus())
	_oActors  = AddTextOption("Tracked actors", SLIFNG.TrackedActorCount())

	AddHeaderOption("Aggregation")
	_oMode    = AddTextOption("When two mods drive one target", ModeName())

	AddHeaderOption("Size")
	_oMaster  = AddSliderOption("Overall magnitude", SLIFNG.GetMasterScale(), "{2}x")
	_oBelly   = AddSliderOption("Belly", SLIFNG.GetTargetScale("pregnancybelly"), "{2}x")
	_oBreast  = AddSliderOption("Breasts", SLIFNG.GetTargetScale("breasts"), "{2}x")

	AddHeaderOption("Diagnostics")
	_oVerbose = AddToggleOption("Verbose logging", _verbose)
	_oDump    = AddTextOption("Dump state to SLIFNG.log", "")
EndEvent

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

Event OnOptionSliderOpen(int a_option)
	if a_option == _oMaster
		SetSliderDialogStartValue(SLIFNG.GetMasterScale())
	elseIf a_option == _oBelly
		SetSliderDialogStartValue(SLIFNG.GetTargetScale("pregnancybelly"))
	elseIf a_option == _oBreast
		SetSliderDialogStartValue(SLIFNG.GetTargetScale("breasts"))
	else
		return
	endIf
	SetSliderDialogDefaultValue(1.0)
	SetSliderDialogRange(0.0, 3.0)
	SetSliderDialogInterval(0.05)
EndEvent

Event OnOptionSliderAccept(int a_option, float a_value)
	; Every setter re-applies every tracked actor: a magnitude change moves no
	; stored contribution, so it cannot ride the normal value early-out.
	if a_option == _oMaster
		SLIFNG.SetMasterScale(a_value)
	elseIf a_option == _oBelly
		SLIFNG.SetTargetScale("pregnancybelly", a_value)
	elseIf a_option == _oBreast
		SLIFNG.SetTargetScale("breasts", a_value)
	else
		return
	endIf
	SetSliderOptionValue(a_option, a_value, "{2}x")
EndEvent

Event OnOptionSelect(int a_option)
	if a_option == _oMode
		; Switching recomputes every tracked actor in one pass - no data change.
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
	endIf
EndEvent

Event OnOptionHighlight(int a_option)
	if a_option == _oMode
		SetInfoText("Highest wins: the largest contribution shows, others are masked.\nAdditive: contributions stack (clamped by their bounds).")
	elseIf a_option == _oVerbose
		SetInfoText("Logs every API call and every apply, with a skee readback per slider. Useful for diagnosis; turn off for normal play.")
	elseIf a_option == _oDump
		SetInfoText("Writes every tracked actor's per-mod contributions and fold results to SKSE\\SLIFNG.log.")
	elseIf a_option == _oEngine
		SetInfoText("Whether SLIF NG found RaceMenu's skee interfaces. 'morphs only' means node scaling is unavailable.")
	elseIf a_option == _oMaster
		SetInfoText("Scales EVERYTHING this framework applies. 1.00x leaves mods exactly as they intended; 0.00x suppresses all inflation. Applies instantly to everyone.")
	elseIf a_option == _oBelly
		SetInfoText("Extra scaling for belly output only, on top of the overall magnitude. Use this if bellies specifically are too large or too small.")
	elseIf a_option == _oBreast
		SetInfoText("Extra scaling for breast output only, on top of the overall magnitude.")
	endIf
EndEvent
