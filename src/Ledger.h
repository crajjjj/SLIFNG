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

#include "Calc.h"

namespace SLIFNG
{
	struct Contribution
	{
		float value{ 0.0f };
		float min{ 0.0f };
		float max{ 100.0f };
		float mult{ 1.0f };
		// The reference's per-row step size for incremental inflation (default
		// 0.1). Stored per contribution like everything else; consumed by the
		// native ramp when incremental mode is on.
		float increment{ 0.1f };

		[[nodiscard]] float Effective() const
		{
			// std::clamp is UB when lo > hi, and a consumer can send an inverted
			// pair; order them rather than trusting the caller.
			const float lo = (std::min)(min, max);
			const float hi = (std::max)(min, max);
			return std::clamp(value, lo, hi) * mult;
		}

	};

	inline constexpr std::string_view kMorphPrefix = "morph:";

	// SLIF NG extension for consumers like SGO4: a SEMANTIC region the actor's
	// body profile maps onto sliders ("region:weight" -> the profile's
	// [Weight] section). Node-scale semantics (1.0 = neutral, folds across
	// mods), but there is no skeleton bone behind it - profile sliders only.
	inline constexpr std::string_view kRegionPrefix = "region:";

	// The sentinel the reference API uses for "every mod" (the pinned default
	// of SLIF_Main.unregisterActor / unregisterNode) - never a real mod key.
	inline constexpr std::string_view kAllMods = "all mods";

	inline bool IsMorphTarget(const std::string& a_target)
	{
		return a_target.starts_with(kMorphPrefix);
	}

	inline bool IsRegionTarget(const std::string& a_target)
	{
		return a_target.starts_with(kRegionPrefix);
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
			float a_value, float a_min, float a_max, float a_mult, float a_increment,
			const std::string& a_sliderName = {});

		// Remove one mod's contribution to one target. True if something was removed.
		// a_mod == kAllMods removes every mod's contribution to that target.
		bool RemoveTarget(RE::FormID a_actor, const std::string& a_mod, const std::string& a_target);

		// Remove one mod entirely for an actor (kAllMods = all of them); returns
		// the targets that lost a contribution (each needs a re-apply).
		std::vector<std::string> RemoveMod(RE::FormID a_actor, const std::string& a_mod);

		// What a NODE-SCALE target currently SHOWS: the hidden pin if hidden,
		// else the ramp's in-flight display value if one is mid-ramp, else the
		// fold. This is what the apply path writes and what GetValue("All Mods")
		// reports - during the reference's incremental queue, its stored
		// "All Mods<node>" was likewise the current stepped value.
		[[nodiscard]] float Aggregate(RE::FormID a_actor, const std::string& a_target) const;

		// The fold alone: SLIF's own addCalculationType, reproduced exactly
		// (six types, Top X by default - CONTRACT sec.4.3). Neutral (1.0) when
		// no positive contributions. This is the ramp's GOAL value.
		[[nodiscard]] float FoldNode(RE::FormID a_actor, const std::string& a_target) const;

		// ---- ramp display overrides (incremental inflation, PLAN P8) --------
		// While a ramp is in flight, the target's SHOWN value is this override
		// rather than the fold; the ramp advances it each tick and clears it on
		// arrival. Transient by design: never serialized, so a load snaps every
		// actor to the fold (which is also what the reference's ReapplyAll-on-
		// load effectively did to its queue).
		void SetDisplay(RE::FormID a_actor, const std::string& a_target, float a_value);
		void ClearDisplay(RE::FormID a_actor, const std::string& a_target);
		[[nodiscard]] std::optional<float> DisplayOf(RE::FormID a_actor,
			const std::string& a_target) const;

		// Incremental inflation on/off (the reference's per-preset
		// inflation_type, as one global switch; its shipped default is instant).
		[[nodiscard]] bool GetGradual() const;
		void SetGradual(bool a_on);

		// The value for ONE skee slider. Per MOD: its direct "morph:<slider>"
		// contribution plus what its node values transform into through the
		// actor's body profile - WITHIN a mod the two layers add (they are one
		// intent, e.g. BF NG sends slif_belly AND PregnancyBelly). ACROSS mods
		// the calculation type folds, exactly as it does for nodes.
		// Deliberate deviation from the reference, which sums morphs
		// unconditionally: that corner shipped disabled (stock morph percents
		// are all 0) and was never field-tested, and once a profile TRANSFORMS
		// a node into a slider, "several mods, one physical target" applies to
		// the slider - composition must not depend on which API spelling a mod
		// used. Neutral is 0.0.
		[[nodiscard]] float AggregateSlider(RE::FormID a_actor, const std::string& a_sliderLower) const;

		// The slider fold WITHOUT the ramp's display override - the goal a
		// direct-morph ramp steps toward.
		[[nodiscard]] float FoldSlider(RE::FormID a_actor, const std::string& a_sliderLower) const;

		// The consumer's original spelling for a lowercase slider (what skee is
		// given); falls back to the lowercase form if never seen.
		[[nodiscard]] std::string SliderName(const std::string& a_sliderLower) const;
		void RememberSlider(const std::string& a_sliderName);

		// The reference's "slif_<morphName>": the raw sum of every mod's direct
		// contribution to one slider. Raw, not bounded - SLIF stores morph
		// min/max/mult but never applies them (SLIF_Morph_Util.CalculateMorphValue
		// sums the stored values verbatim), and consumers like Sexlab Survival
		// read this exact number back out of StorageUtil.
		[[nodiscard]] float DirectMorph(RE::FormID a_actor, const std::string& a_sliderLower) const;

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

		// Per-ACTOR magnitude on top of the global pair (SGO4's BellyScaleMult
		// as a framework feature): what apply multiplies by is
		// master * target * actor. Persisted (cosave v8); 1.0 rows are dropped.
		void SetActorTargetScale(RE::FormID a_actor, const std::string& a_scaleId, float a_scale);
		[[nodiscard]] float GetActorTargetScale(RE::FormID a_actor, const std::string& a_scaleId) const;
		[[nodiscard]] float EffectiveScaleFor(RE::FormID a_actor, const std::string& a_scaleId) const;
		[[nodiscard]] std::vector<std::pair<std::string, float>> ActorScales(RE::FormID a_actor) const;

		[[nodiscard]] float GetContribution(RE::FormID a_actor, const std::string& a_mod,
			const std::string& a_target) const;

		// The stored bounds behind SLIF_Main/SLIF_Morph.GetMinValue/GetMaxValue.
		// a_mod == kAllMods folds them the way the aggregate does: the LOOSEST
		// bound any contributor set, since that is what the fold may reach.
		[[nodiscard]] float GetBoundMin(RE::FormID a_actor, const std::string& a_mod,
			const std::string& a_target) const;
		[[nodiscard]] float GetBoundMax(RE::FormID a_actor, const std::string& a_mod,
			const std::string& a_target) const;

		// Whether anything is stored at all, so a getter can honour the caller's
		// `default` instead of inventing a neutral (reference semantics: an
		// absent StorageUtil key returns the caller's default, NOT 0 or 1).
		[[nodiscard]] bool HasTarget(RE::FormID a_actor, const std::string& a_mod,
			const std::string& a_target) const;

		// Update ONE mod's stored bounds on one target without touching its
		// value (SLIF_Main.updateActorList: Estrus Chaurus pushes new MCM max
		// scales this way). -1.0 keeps the field as stored, mirroring the
		// reference's SetFloatValueConditional semantics. True if changed.
		bool UpdateBounds(RE::FormID a_actor, const std::string& a_mod, const std::string& a_target,
			float a_min, float a_max, float a_mult, float a_increment);

		// ---- hidden nodes (SLIF_Main.hideNode / showNode) -------------------
		// Devious Devices pins the belly flat under a chastity belt. A hidden
		// target OVERRIDES the fold rather than joining it, and it is keyed by
		// ACTOR+TARGET, not by mod - reference SLIF stores one `<node>_hidden`
		// flag per actor and any mod may lift it.
		//
		// The node path takes the pin value verbatim (reference behaviour: a
		// scale of ~0.01 collapses the node). The MORPH path forces every slider
		// the hidden target drives to NEUTRAL instead, because a morph body has
		// no way to express "collapsed": running the pin through the blend gives
		// PregnancyBelly = (0.01 - 1.0) * 0.1333 = -0.13, which is a concave
		// belly, not a flat one.
		bool Hide(RE::FormID a_actor, const std::string& a_target, float a_value);
		bool Show(RE::FormID a_actor, const std::string& a_target);
		[[nodiscard]] bool IsHidden(RE::FormID a_actor, const std::string& a_target) const;

		// Which mods hold a contribution to one target on one actor (for the
		// actor diagnostics page).
		[[nodiscard]] std::vector<std::string> ModsDriving(RE::FormID a_actor,
			const std::string& a_target) const;

		[[nodiscard]] std::vector<RE::FormID> TrackedActors() const;
		[[nodiscard]] std::vector<std::string> TargetsOf(RE::FormID a_actor) const;
		// TargetsOf split for the query API: canonical node keys, and morph
		// sliders (lowercase; map through SliderName for the display spelling).
		[[nodiscard]] std::vector<std::string> NodeTargetsOf(RE::FormID a_actor) const;
		[[nodiscard]] std::vector<std::string> MorphSlidersOf(RE::FormID a_actor) const;
		[[nodiscard]] bool HasEntries(RE::FormID a_actor) const;

		// One-shot legacy-import marker. Persisted with the save, because the
		// question "has THIS save been imported" is save state, not settings.
		[[nodiscard]] bool Migrated() const;
		void SetMigrated(bool a_done);

		Calc::Type GetMode() const { return _mode; }
		void SetMode(Calc::Type a_mode) { _mode = a_mode; }
		std::uint32_t GetTopX() const { return _topX; }
		void SetTopX(std::uint32_t a_topX) { _topX = a_topX > 0 ? a_topX : 1; }

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

		[[nodiscard]] bool IsHiddenLocked(RE::FormID a_actor, const std::string& a_target) const;

		// Unlocked internals, for callers already holding _lock.
		void RememberSliderLocked(const std::string& a_sliderName);
		[[nodiscard]] float AggregateSliderLocked(RE::FormID a_actor, const std::string& a_sliderLower) const;
		[[nodiscard]] float FoldSliderLocked(RE::FormID a_actor, const std::string& a_sliderLower) const;
		[[nodiscard]] float FoldNodeLocked(RE::FormID a_actor, const std::string& a_target) const;
		[[nodiscard]] float DirectMorphLocked(RE::FormID a_actor, const std::string& a_sliderLower) const;
		[[nodiscard]] std::vector<std::string> TargetsOfLocked(RE::FormID a_actor) const;

		[[nodiscard]] float DisplayedNodeLocked(RE::FormID a_actor, const std::string& a_target) const;
		[[nodiscard]] float RampFactorLocked(RE::FormID a_actor, const std::string& a_target) const;

		mutable std::recursive_mutex _lock;
		std::unordered_map<RE::FormID, ModMap> _actors;
		// actor -> target -> the ramp's current in-flight value (transient).
		std::unordered_map<RE::FormID, std::unordered_map<std::string, float>> _display;
		// actor -> target -> pin value. Deliberately NOT part of _actors: a hide
		// is not a contribution and must not fold with one.
		std::unordered_map<RE::FormID, std::unordered_map<std::string, float>> _hidden;
		// lowercase slider -> original spelling handed to skee
		std::unordered_map<std::string, std::string> _sliderNames;
		// scale id -> user multiplier (absent = 1.0, never stored when == 1.0)
		std::unordered_map<std::string, float> _targetScales;
		// actor -> scale id -> per-actor multiplier (same drop-at-1.0 rule)
		std::unordered_map<RE::FormID, std::unordered_map<std::string, float>> _actorScales;
		float _masterScale{ 1.0f };
		bool _migrated{ false };
		// ON by default - a deliberate SLIF NG choice (the reference shipped
		// instant): bodies swelling in steps reads better than snapping, and
		// the toggle is one MCM click away. A v7+ cosave keeps its saved value.
		bool _gradual{ true };
		// SLIF's Config.json calculation_type numbering; its default is Top X.
		Calc::Type _mode{ Calc::Type::kTopX };
		std::uint32_t _topX{ Calc::kDefaultTopX };
	};
}
