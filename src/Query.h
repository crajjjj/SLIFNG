#pragma once

#include "Ledger.h"
#include "Vocabulary.h"

// The read-only query core, shared verbatim by the Papyrus natives
// (SLIFNG.psc) and the inter-plugin SKSE interface (API/SLIFNG_API.h), so a
// C++ consumer and a Papyrus consumer can never disagree about an answer.
//
// Reference semantics throughout: modName "All Mods" reads the aggregate, any
// other name reads that mod's own row, an ABSENT key returns the caller's
// default, and a target may be spelled as a slif_* key, a raw skeleton node,
// or "morph:<slider>".
//
// One deliberate departure, inherited from the Papyrus getters: the "All Mods"
// aggregate is returned WITH the user's magnitude scaling applied, because the
// question consumers ask is "how inflated does this actor look" and the scale
// genuinely changes the answer.

namespace SLIFNG::Query
{
	// "slif_belly" / "NPC Belly" / "morph:pregnancybelly" -> ledger target.
	// Empty when the key is not vocabulary and not a morph target.
	inline std::string ResolveTarget(const char* a_raw)
	{
		if (!a_raw || !*a_raw) {
			return {};
		}
		const std::string lower = Lower(a_raw);
		if (IsMorphTarget(lower) || IsRegionTarget(lower)) {
			return lower;
		}
		const auto* resolved = Vocabulary::Resolve(lower);
		return resolved ? resolved->key : std::string{};
	}

	inline float Value(RE::Actor* a_actor, const char* a_mod, const char* a_target, float a_default)
	{
		if (!a_actor || !a_target || !*a_target) {
			return 0.0f;  // reference returns 0.0 for invalid parameters
		}
		const std::string target = ResolveTarget(a_target);
		if (target.empty()) {
			// UNRESOLVED is not INVALID: the reference's ConvertToNode passes
			// unknown strings through and the StorageUtil read then hands back
			// the caller's default. Estrus Chaurus leans on exactly this -
			// GetMaxValue(..., "slif_left_breast", MaxScale) CLAMPS its breast
			// growth, and a 0.0 here would crush the actor flat.
			return a_default;
		}
		auto& ledger = Ledger::GetSingleton();
		const auto formID = a_actor->GetFormID();
		const std::string mod = Lower(a_mod ? a_mod : "");
		if (!ledger.HasTarget(formID, mod, target)) {
			return a_default;
		}
		if (mod != kAllMods) {
			return ledger.GetContribution(formID, mod, target);
		}
		if (IsMorphTarget(target)) {
			const std::string slider = SliderOf(target);
			return ledger.AggregateSlider(formID, slider) * ledger.EffectiveScale(slider);
		}
		// Node scales are multipliers: the user magnitude scales the DEVIATION
		// from neutral, exactly as the apply path does.
		const float shown = ledger.Aggregate(formID, target);
		return 1.0f + (shown - 1.0f) * ledger.EffectiveScale(target);
	}

	inline float MinValue(RE::Actor* a_actor, const char* a_mod, const char* a_target, float a_default)
	{
		if (!a_actor || !a_target || !*a_target) {
			return 0.0f;
		}
		const std::string target = ResolveTarget(a_target);
		if (target.empty()) {
			return a_default;  // see Value(): unresolved is not invalid
		}
		auto& ledger = Ledger::GetSingleton();
		const auto formID = a_actor->GetFormID();
		const std::string mod = Lower(a_mod ? a_mod : "");
		return ledger.HasTarget(formID, mod, target) ? ledger.GetBoundMin(formID, mod, target)
		                                             : a_default;
	}

	inline float MaxValue(RE::Actor* a_actor, const char* a_mod, const char* a_target, float a_default)
	{
		if (!a_actor || !a_target || !*a_target) {
			return 0.0f;
		}
		const std::string target = ResolveTarget(a_target);
		if (target.empty()) {
			return a_default;  // see Value(): unresolved is not invalid
		}
		auto& ledger = Ledger::GetSingleton();
		const auto formID = a_actor->GetFormID();
		const std::string mod = Lower(a_mod ? a_mod : "");
		return ledger.HasTarget(formID, mod, target) ? ledger.GetBoundMax(formID, mod, target)
		                                             : a_default;
	}

	// The shown value for any target spelling (1.0-neutral for nodes,
	// 0.0-neutral for morphs), without the user magnitude.
	inline float Applied(RE::Actor* a_actor, const char* a_target)
	{
		if (!a_actor || !a_target || !*a_target) {
			return 0.0f;
		}
		return Ledger::GetSingleton().Aggregate(a_actor->GetFormID(), Lower(a_target));
	}

	// The reference's "slif_<morphName>": cross-mod direct-morph total.
	inline float CombinedMorph(RE::Actor* a_actor, const char* a_slider)
	{
		if (!a_actor || !a_slider || !*a_slider) {
			return 0.0f;
		}
		return Ledger::GetSingleton().DirectMorph(a_actor->GetFormID(), Lower(a_slider));
	}

	inline bool IsTracked(RE::Actor* a_actor)
	{
		return a_actor && Ledger::GetSingleton().HasEntries(a_actor->GetFormID());
	}
}
