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
// WHICH sliders (if any) a target drives is NOT vocabulary any more: that is a
// property of the actor's BODY, so it lives in the per-body profiles
// (BodyProfile). A target with no blend in the actor's profile is driven by a
// skeleton NODE scale - which is also the reference's own out-of-the-box
// behaviour (its shipped bodymorphs config has every percent at 0).

namespace SLIFNG::Vocabulary
{
	struct NodeTarget
	{
		const char* key;       // canonical slif_* key (lowercase)
		const char* label;     // human name for the MCM / log ("Belly")
		const char* nodes[2];  // skeleton node(s) for the node path (nullptr = unused)
	};

	// Live keys. slif_breast / slif_butt are "sync" pairs in the reference:
	// both L/R nodes always receive the same value.
	inline constexpr NodeTarget kTargets[] = {
		{ "slif_belly", "Belly", { "NPC Belly", nullptr } },
		{ "slif_breast", "Breasts", { "NPC L Breast", "NPC R Breast" } },
		{ "slif_butt", "Butt", { "NPC L Butt", "NPC R Butt" } },
		{ "slif_scrotum", "Scrotum", { "NPC GenitalsScrotum [GenScrot]", nullptr } },
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

	// Readable form of a canonical key for UI/log rows: "Belly (NPC Belly)",
	// "Breasts (L+R)". Unknown keys come back unchanged.
	inline std::string Describe(const std::string& a_lowerKey)
	{
		const auto* target = Find(a_lowerKey);
		if (!target) {
			return a_lowerKey;
		}
		std::string out = target->label;
		if (target->nodes[1]) {
			out += " (L+R)";
		} else if (target->nodes[0] && std::string_view{ target->nodes[0] }.size() <= 12) {
			out += std::string{ " (" } + target->nodes[0] + ")";
		}
		return out;
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

	// Reference convert_keys that name ONE SIDE of a sync pair. OBSERVED:
	// Estrus Chaurus reads its bounds through them
	// (SLIF_Main.GetMinValue/GetMaxValue with "slif_left_breast" /
	// "slif_left_butt"). They resolve to the PAIR target, so the sync
	// fidelity gap documented at FindByNode applies to them too.
	inline constexpr const char* kSideAliases[][2] = {
		{ "slif_left_breast", "slif_breast" },
		{ "slif_right_breast", "slif_breast" },
		{ "slif_left_butt", "slif_butt" },
		{ "slif_right_butt", "slif_butt" },
	};

	// Accepts any observed spelling and yields the canonical target.
	inline const NodeTarget* Resolve(const std::string& a_lowerKey)
	{
		if (const auto* byKey = Find(a_lowerKey)) {
			return byKey;
		}
		for (const auto& alias : kSideAliases) {
			if (a_lowerKey == alias[0]) {
				return Find(alias[1]);
			}
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
