#pragma once

// Actor diagnostics: everything SLIF NG knows about one actor, flattened into
// label/value pairs the MCM renders as rows. The ENGINE formats, so the
// Papyrus side stays a dumb printer and the same text can go to the log.
//
// Returned INTERLEAVED: { label0, value0, label1, value1, ... }. A pair with an
// empty value is a section header.

namespace SLIFNG::Report
{
	// Split so the MCM can pair the halves into real columns (SkyUI's
	// TOP_TO_BOTTOM only flows right once the LEFT column is FULL, and this page
	// is never that long). ForActor concatenates both, for the log.
	std::vector<RE::BSFixedString> IdentityRows(RE::Actor* a_actor);
	std::vector<RE::BSFixedString> StateRows(RE::Actor* a_actor);
	std::vector<RE::BSFixedString> ForActor(RE::Actor* a_actor);
	void LogForActor(RE::Actor* a_actor);
}
