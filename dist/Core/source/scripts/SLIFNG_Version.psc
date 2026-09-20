Scriptname SLIFNG_Version Hidden
{The ONE place the mod version lives, SL Widgets convention (slw_util.psc):
GetVersion() is the mod version packed as major*10000 + minor*100 + patch,
GetVersionString() is the display form, and SLIF_Menu.GetVersion() delegates
here so the MCM config revision IS the packed mod version.

FLOOR WARNING: the packed value must stay ABOVE 122 forever. A migrating save
carries the reference SLIF_Menu's stored config version 122, and SkyUI only
fires OnVersionUpdate on an increase - so a 0.1.x version (packed 1xx or
lower) would silently disable every MCM version update on exactly the saves
that need migrating. 0.2.0 (200) is the first legal version; there was never
a released 0.1.x for this reason.}

Int Function GetVersion() Global
	Return 412
	; 0.2.0  ->    200
	; 0.3.0  ->    300
	; 1.0.0  ->  10000
	; 1.2.3  ->  10203
EndFunction

String Function GetVersionString() Global
	Return "0.4.12"
EndFunction
