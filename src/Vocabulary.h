#pragma once

// The observed SLIF node vocabulary (CONTRACT.md sec.5). Two spellings reach
// SLIF_Main.inflate and BOTH must resolve to the SAME canonical target:
//
//   * a slif_* key        - SLS, Estrus Spider, BF NG
//   * a raw skeleton node - FHU sends "NPC Belly" verbatim
//
// The reference tolerates the raw form by accident: ConvertToNode returns its
// argument unchanged when it isn't a known key, so an unrecognised string is
// simply used as a node name. Routing both spellings onto one ledger target is
// what stops FHU and BF NG driving the same physical node through two targets
// and clobbering each other at the skee write.
//
// Unknown or dead keys are bug-compatible silent no-ops (logged, never applied).
//
// MVP: the morph-first mapping ships hardcoded, mirroring BF NG's proven
// BodyMorph/default.ini blends. P2 moves these tables into per-body INI
// profiles with per-actor resolution.

namespace SLIFNG::Vocabulary
{
	struct MorphBlend
	{
		const char* slider;  // BodySlide slider name (skee morph)
		float weight;        // slider value per 1.0 of node-scale deviation
	};

	// UNIT CONVERSION - node scale and morph weight are NOT the same scale.
	// A consumer's node value is a multiplier (1.0 = neutral, and BF NG's SLIF
	// magnitudes run to 8.5); a BodySlide slider is a 0..1 blend where 1.0 is
	// already the full shape. Mapping deviation 1:1 onto the slider is what made
	// a Sexlab Survival belly of 2.2 write PregnancyBelly=1.2 - past full-term
	// from a meal.
	//
	// The calibration comes from BF NG, which drives BOTH backends and states
	// the exchange rate itself (FWSystemConfig.OnConfigInit): with SLIF it uses
	// BellyMaxScale 7.5 / BreastsMaxScale 10.0, and with its own BodyMorph
	// backend it uses 1.0 for the same visual, mapped through a profile whose
	// sliders top out at 1.0. So full deviation 7.5 == belly slider 1.0, and
	// full deviation 10.0 == breast slider 1.0.
	inline constexpr float kBellyPerDeviation = 1.0f / 7.5f;   // 0.1333
	inline constexpr float kBreastPerDeviation = 1.0f / 10.0f;  // 0.1

	struct NodeTarget
	{
		const char* key;              // canonical slif_* key (lowercase)
		const char* nodes[2];         // skeleton node(s) for the NiTransform fallback (nullptr = unused)
		MorphBlend morphs[2];         // morph-first mapping ({nullptr,0} = none -> node fallback)
	};

	// Live keys. slif_breast / slif_butt are "sync" pairs in the reference:
	// both L/R nodes always receive the same value.
	inline constexpr NodeTarget kTargets[] = {
		{ "slif_belly", { "NPC Belly", nullptr },
			{ { "PregnancyBelly", kBellyPerDeviation }, { nullptr, 0.0f } } },
		{ "slif_breast", { "NPC L Breast", "NPC R Breast" },
			{ { "BreastsSH", kBreastPerDeviation }, { "BreastsNewSH", kBreastPerDeviation } } },
		{ "slif_butt", { "NPC L Butt", "NPC R Butt" }, { { nullptr, 0.0f }, { nullptr, 0.0f } } },
		{ "slif_scrotum", { "NPC GenitalsScrotum [GenScrot]", nullptr }, { { nullptr, 0.0f }, { nullptr, 0.0f } } },
	};

	// Bug-compatible dead keys: real SLIF 1.2.2 drops these silently
	// (ignore_keys list / absent from every list). Estrus Spider sends them.
	inline constexpr const char* kDeadKeys[] = {
		"slif_breast01",
		"slif_breast_p",
	};

	inline const NodeTarget* Find(const std::string& a_lowerKey)
	{
		for (const auto& target : kTargets) {
			if (a_lowerKey == target.key) {
				return &target;
			}
		}
		return nullptr;
	}

	// Raw skeleton-node spelling -> the target that owns that node.
	// NOTE (fidelity gap): a sync pair's nodes both map to the one paired
	// target, so a consumer sending ONLY "NPC L Breast" gets BOTH breasts
	// scaled, where the reference would have scaled just the left one. No
	// observed consumer does this - FHU only ever sends the unpaired
	// "NPC Belly" - but it is a real difference, recorded in CONTRACT sec.5.
	inline const NodeTarget* FindByNode(const std::string& a_lowerNode)
	{
		for (const auto& target : kTargets) {
			for (const auto* node : target.nodes) {
				if (node && a_lowerNode == Lower(node)) {
					return &target;
				}
			}
		}
		return nullptr;
	}

	// Accepts either spelling and yields the canonical target.
	inline const NodeTarget* Resolve(const std::string& a_lowerKey)
	{
		if (const auto* byKey = Find(a_lowerKey)) {
			return byKey;
		}
		return FindByNode(a_lowerKey);
	}

	inline bool IsDeadKey(const std::string& a_lowerKey)
	{
		for (const auto* dead : kDeadKeys) {
			if (a_lowerKey == dead) {
				return true;
			}
		}
		return false;
	}
}
