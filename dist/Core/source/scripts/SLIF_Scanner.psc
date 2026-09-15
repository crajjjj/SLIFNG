Scriptname SLIF_Scanner extends Quest
{Compatibility stub for quest 0x801 of "SexLab Inflation Framework.esp".

SLIF NG has no scanner: the native ledger is the source of truth and a
TESObjectLoadedEvent hook re-applies actors as they stream in, so there is
nothing to poll. The CLASS must still exist, because a save that ran reference
SLIF holds SLIF_Scanner instances bound to this very FormID - see the note in
SLIF_Menu.psc for what a resolvable form with a missing script class does to
the VM. Keeping the form scripted lets those instances load and be discarded
cleanly.

initializeScanner() is kept because the reference's alias script called it; an
unsurveyed consumer may too. It is deliberately a no-op.}

Function initializeScanner()
	; no-op: SLIF NG does not poll. See the script docstring.
EndFunction
