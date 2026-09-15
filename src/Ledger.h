#pragma once

// The keyed per-mod ledger: the single source of truth (PLAN.md "Storage
// model"). actor -> mod -> target -> clamped contribution. Aggregation is a
// pure fold over the stored contributions, computed at apply time, so the
// mode is switchable at runtime with no data migration (CONTRACT.md sec.4.3).
//
// Target ids (always lowercase):
//   "slif_belly" etc.        node-scale targets, value 1.0 = neutral
//   "morph:<slider name>"    morph targets, value 0.0 = neutral
//
// Lowercasing matches Papyrus string semantics (a consumer may spell the same
// slider two ways), but skee is fed the ORIGINAL spelling - see _sliderNames.

namespace SLIFNG
{
	enum class AggregationMode : std::uint32_t
	{
		kHighestWins = 0,
		kAdditive = 1,
	};

	struct Contribution
	{
		float value{ 0.0f };
		float min{ 0.0f };
		float max{ 100.0f };
		float mult{ 1.0f };

		[[nodiscard]] float Effective() const
		{
			// std::clamp is UB when lo > hi, and a consumer can send an inverted
			// pair; order them rather than trusting the caller.
			const float lo = (std::min)(min, max);
			const float hi = (std::max)(min, max);
			return std::clamp(value, lo, hi) * mult;
		}

		// The same projection applied to the bound, so a fold can clamp its
		// aggregate with the per-contribution ceiling/floor (CONTRACT 4.3).
		[[nodiscard]] float EffectiveMax() const { return (std::max)(min, max) * mult; }
		[[nodiscard]] float EffectiveMin() const { return (std::min)(min, max) * mult; }
	};

	inline constexpr std::string_view kMorphPrefix = "morph:";

	// The sentinel the reference API uses for "every mod" (the pinned default
	// of SLIF_Main.unregisterActor / unregisterNode) - never a real mod key.
	inline constexpr std::string_view kAllMods = "all mods";

	inline bool IsMorphTarget(const std::string& a_target)
	{
		return a_target.starts_with(kMorphPrefix);
	}

	inline std::string SliderOf(const std::string& a_morphTarget)
	{
		return a_morphTarget.substr(kMorphPrefix.size());
	}

	class Ledger
	{
	public:
		static Ledger& GetSingleton()
		{
			static Ledger singleton;
			return singleton;
		}

		// Returns true when the stored value actually changed (early-out support).
		// a_sliderName: the consumer's ORIGINAL spelling for a morph target
		// (remembered for the skee call); ignored for node targets.
		bool Set(RE::FormID a_actor, const std::string& a_mod, const std::string& a_target,
			float a_value, float a_min, float a_max, float a_mult,
			const std::string& a_sliderName = {});

		// Remove one mod's contribution to one target. True if something was removed.
		// a_mod == kAllMods removes every mod's contribution to that target.
		bool RemoveTarget(RE::FormID a_actor, const std::string& a_mod, const std::string& a_target);

		// Remove one mod entirely for an actor (kAllMods = all of them); returns
		// the targets that lost a contribution (each needs a re-apply).
		std::vector<std::string> RemoveMod(RE::FormID a_actor, const std::string& a_mod);

		// The configurable fold for a NODE-SCALE target (CONTRACT sec.4.3).
		// Neutral (1.0) when no contributions.
		[[nodiscard]] float Aggregate(RE::FormID a_actor, const std::string& a_target) const;

		// The fold for ONE skee slider, across every source that drives it:
		// direct "morph:<slider>" contributions AND node-key contributions whose
		// vocabulary blend maps onto this slider. This is what keeps BF NG's
		// slif_belly and FHU's morph:pregnancybelly aggregating instead of
		// clobbering each other. Neutral is 0.0.
		[[nodiscard]] float AggregateSlider(RE::FormID a_actor, const std::string& a_sliderLower) const;

		// The consumer's original spelling for a lowercase slider (what skee is
		// given); falls back to the lowercase form if never seen.
		[[nodiscard]] std::string SliderName(const std::string& a_sliderLower) const;
		void RememberSlider(const std::string& a_sliderName);

		// ---- user magnitude scaling (PLAN P5) -------------------------------
		// Applied at APPLY time, on top of the fold, so it never touches stored
		// contributions: consumers keep sending what they mean and the user
		// decides how big that looks. A scale id is the lowercase SLIDER name
		// for a morph target, or the canonical key for a node target
		// ("pregnancybelly", "slif_butt"), so both paths tune the same way.
		[[nodiscard]] float MasterScale() const;
		void SetMasterScale(float a_scale);
		[[nodiscard]] float TargetScale(const std::string& a_scaleId) const;
		void SetTargetScale(const std::string& a_scaleId, float a_scale);
		// master * per-target, i.e. what apply actually multiplies by.
		[[nodiscard]] float EffectiveScale(const std::string& a_scaleId) const;
		[[nodiscard]] std::vector<std::string> ScaledTargets() const;

		[[nodiscard]] float GetContribution(RE::FormID a_actor, const std::string& a_mod,
			const std::string& a_target) const;

		// Which mods hold a contribution to one target on one actor (for the
		// actor diagnostics page).
		[[nodiscard]] std::vector<std::string> ModsDriving(RE::FormID a_actor,
			const std::string& a_target) const;

		[[nodiscard]] std::vector<RE::FormID> TrackedActors() const;
		[[nodiscard]] std::vector<std::string> TargetsOf(RE::FormID a_actor) const;
		[[nodiscard]] bool HasEntries(RE::FormID a_actor) const;

		// One-shot legacy-import marker. Persisted with the save, because the
		// question "has THIS save been imported" is save state, not settings.
		[[nodiscard]] bool Migrated() const;
		void SetMigrated(bool a_done);

		AggregationMode GetMode() const { return _mode; }
		void SetMode(AggregationMode a_mode) { _mode = a_mode; }

		// Diagnostics: write the full ledger state (or one actor's) to the log -
		// every contribution, every fold result, the active mode.
		void DumpToLog() const;
		void DumpActorToLog(RE::FormID a_actor) const;

		// SKSE co-save callbacks (registered in main.cpp).
		static void OnGameSaved(SKSE::SerializationInterface* a_intfc);
		static void OnGameLoaded(SKSE::SerializationInterface* a_intfc);
		static void OnRevert(SKSE::SerializationInterface* a_intfc);

	private:
		Ledger() = default;

		// mod key (lowercase) -> target (lowercase) -> contribution
		using ModMap = std::unordered_map<std::string, std::unordered_map<std::string, Contribution>>;

		// Unlocked internals, for callers already holding _lock.
		void RememberSliderLocked(const std::string& a_sliderName);
		[[nodiscard]] float AggregateSliderLocked(RE::FormID a_actor, const std::string& a_sliderLower) const;
		[[nodiscard]] std::vector<std::string> TargetsOfLocked(RE::FormID a_actor) const;

		mutable std::recursive_mutex _lock;
		std::unordered_map<RE::FormID, ModMap> _actors;
		// lowercase slider -> original spelling handed to skee
		std::unordered_map<std::string, std::string> _sliderNames;
		// scale id -> user multiplier (absent = 1.0, never stored when == 1.0)
		std::unordered_map<std::string, float> _targetScales;
		float _masterScale{ 1.0f };
		bool _migrated{ false };
		AggregationMode _mode{ AggregationMode::kHighestWins };
	};
}
