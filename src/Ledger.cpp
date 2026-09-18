#include "Ledger.h"

#include "BodyProfile.h"
#include "Serialization.h"
#include "Vocabulary.h"

namespace SLIFNG
{
	namespace
	{
		constexpr std::uint32_t kLedgerRecord = 'LEDG';
		// v2: aggregation mode + slider display-name table.
		// v3: user magnitude scaling (master + per-target).
		// v4: one-shot legacy-import marker.
		// v5: the hidden-node map.
		// v6: the mode field switched to SLIF's calculation_type numbering
		//     (0 Top X .. 5 Additive) and gained the top_x count beside it.
		// v7: per-contribution increment; the incremental-inflation flag.
		constexpr std::uint32_t kLedgerVersion = 7;
		constexpr float kScaleEpsilon = 0.0001f;
	}

	bool Ledger::Set(RE::FormID a_actor, const std::string& a_mod, const std::string& a_target,
		float a_value, float a_min, float a_max, float a_mult, float a_increment,
		const std::string& a_sliderName)
	{
		std::scoped_lock lock(_lock);
		RememberSliderLocked(a_sliderName);
		auto& entry = _actors[a_actor][Lower(a_mod)][Lower(a_target)];
		const Contribution next{ a_value, a_min, a_max, a_mult, a_increment };
		const bool changed = entry.value != next.value || entry.min != next.min ||
		                     entry.max != next.max || entry.mult != next.mult ||
		                     entry.increment != next.increment;
		entry = next;
		return changed;
	}

	void Ledger::RememberSlider(const std::string& a_sliderName)
	{
		std::scoped_lock lock(_lock);
		RememberSliderLocked(a_sliderName);
	}

	void Ledger::RememberSliderLocked(const std::string& a_sliderName)
	{
		if (a_sliderName.empty()) {
			return;
		}
		const std::string lower = Lower(a_sliderName);
		const auto it = _sliderNames.find(lower);
		if (it == _sliderNames.end()) {
			_sliderNames.emplace(lower, a_sliderName);
			return;
		}
		// BodySlide slider names are mixed case; if we first saw an all-lowercase
		// spelling, upgrade to a mixed-case one the moment a consumer sends it.
		if (it->second == lower && a_sliderName != lower) {
			it->second = a_sliderName;
		}
	}

	std::string Ledger::SliderName(const std::string& a_sliderLower) const
	{
		std::scoped_lock lock(_lock);
		const auto it = _sliderNames.find(a_sliderLower);
		return it != _sliderNames.end() ? it->second : a_sliderLower;
	}

	bool Ledger::UpdateBounds(RE::FormID a_actor, const std::string& a_mod,
		const std::string& a_target, float a_min, float a_max, float a_mult, float a_increment)
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return false;
		}
		const auto modIt = actorIt->second.find(Lower(a_mod));
		if (modIt == actorIt->second.end()) {
			return false;
		}
		const auto it = modIt->second.find(Lower(a_target));
		if (it == modIt->second.end()) {
			return false;
		}
		auto& entry = it->second;
		const Contribution before = entry;
		if (a_min != -1.0f) {
			entry.min = a_min;
		}
		if (a_max != -1.0f) {
			entry.max = a_max;
		}
		if (a_mult != -1.0f) {
			entry.mult = a_mult;
		}
		if (a_increment != -1.0f) {
			entry.increment = a_increment;
		}
		return entry.min != before.min || entry.max != before.max ||
		       entry.mult != before.mult || entry.increment != before.increment;
	}

	bool Ledger::RemoveTarget(RE::FormID a_actor, const std::string& a_mod, const std::string& a_target)
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return false;
		}
		const std::string mod = Lower(a_mod);
		const std::string target = Lower(a_target);
		bool removed = false;

		for (auto modIt = actorIt->second.begin(); modIt != actorIt->second.end();) {
			if (mod == kAllMods || modIt->first == mod) {
				removed |= modIt->second.erase(target) > 0;
			}
			modIt = modIt->second.empty() ? actorIt->second.erase(modIt) : std::next(modIt);
		}
		if (actorIt->second.empty()) {
			_actors.erase(actorIt);
		}
		return removed;
	}

	std::vector<std::string> Ledger::RemoveMod(RE::FormID a_actor, const std::string& a_mod)
	{
		std::scoped_lock lock(_lock);
		std::vector<std::string> affected;
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return affected;
		}
		const std::string mod = Lower(a_mod);

		for (auto modIt = actorIt->second.begin(); modIt != actorIt->second.end();) {
			if (mod == kAllMods || modIt->first == mod) {
				for (const auto& [target, contribution] : modIt->second) {
					if (std::find(affected.begin(), affected.end(), target) == affected.end()) {
						affected.push_back(target);
					}
				}
				modIt = actorIt->second.erase(modIt);
			} else {
				++modIt;
			}
		}
		if (actorIt->second.empty()) {
			_actors.erase(actorIt);
		}
		return affected;
	}

	float Ledger::Aggregate(RE::FormID a_actor, const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		const std::string target = Lower(a_target);
		if (IsMorphTarget(target)) {
			return AggregateSliderLocked(a_actor, SliderOf(target));
		}

		// A hidden target OVERRIDES the fold: DD's chastity belt must win over
		// whatever Beeing Female or FHU is asking for, not merely compete.
		if (const auto hiddenIt = _hidden.find(a_actor); hiddenIt != _hidden.end()) {
			const auto pin = hiddenIt->second.find(target);
			if (pin != hiddenIt->second.end()) {
				return pin->second;
			}
		}
		return DisplayedNodeLocked(a_actor, target);
	}

	float Ledger::FoldNode(RE::FormID a_actor, const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		return FoldNodeLocked(a_actor, Lower(a_target));
	}

	// The ramp's in-flight value when one is active, else the fold.
	float Ledger::DisplayedNodeLocked(RE::FormID a_actor, const std::string& a_target) const
	{
		if (const auto actorIt = _display.find(a_actor); actorIt != _display.end()) {
			const auto it = actorIt->second.find(a_target);
			if (it != actorIt->second.end()) {
				return it->second;
			}
		}
		return FoldNodeLocked(a_actor, a_target);
	}

	void Ledger::SetDisplay(RE::FormID a_actor, const std::string& a_target, float a_value)
	{
		std::scoped_lock lock(_lock);
		_display[a_actor][Lower(a_target)] = a_value;
	}

	void Ledger::ClearDisplay(RE::FormID a_actor, const std::string& a_target)
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _display.find(a_actor);
		if (actorIt == _display.end()) {
			return;
		}
		actorIt->second.erase(Lower(a_target));
		if (actorIt->second.empty()) {
			_display.erase(actorIt);
		}
	}

	std::optional<float> Ledger::DisplayOf(RE::FormID a_actor, const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _display.find(a_actor);
		if (actorIt == _display.end()) {
			return std::nullopt;
		}
		const auto it = actorIt->second.find(Lower(a_target));
		return it != actorIt->second.end() ? std::optional<float>{ it->second } : std::nullopt;
	}

	bool Ledger::GetGradual() const
	{
		std::scoped_lock lock(_lock);
		return _gradual;
	}

	void Ledger::SetGradual(bool a_on)
	{
		std::scoped_lock lock(_lock);
		_gradual = a_on;
	}

	// SLIF_Calc.addCalculationType over the per-mod effective contributions -
	// the reference's own math, verbatim (the arithmetic lives in Calc::Fold).
	// The former highest/additive pair with its post-fold bounds clamp is gone:
	// "keep SLIF formulas" means the per-contribution clamp is the only clamp,
	// and a below-neutral contribution CAN show (SLIF only floors a fold
	// result that lands at or below zero).
	float Ledger::FoldNodeLocked(RE::FormID a_actor, const std::string& a_target) const
	{
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return 1.0f;
		}
		std::vector<float> values;
		for (const auto& [mod, targets] : actorIt->second) {
			const auto it = targets.find(a_target);
			if (it != targets.end()) {
				values.push_back(it->second.Effective());
			}
		}
		if (values.empty()) {
			return 1.0f;
		}
		return Calc::Fold(_mode, std::move(values), _topX);
	}

	float Ledger::AggregateSlider(RE::FormID a_actor, const std::string& a_sliderLower) const
	{
		std::scoped_lock lock(_lock);
		return AggregateSliderLocked(a_actor, a_sliderLower);
	}

	float Ledger::AggregateSliderLocked(RE::FormID a_actor, const std::string& a_sliderLower) const
	{
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return 0.0f;  // morph neutral
		}
		const std::string morphTarget = std::string{ kMorphPrefix } + a_sliderLower;

		// One value per MOD: its direct contribution (raw, the reference's
		// morph bookkeeping) plus its node values transformed through the
		// actor's body profile. Within a mod the layers ADD; across mods the
		// calculation type folds - see the Ledger.h comment for why this
		// deliberately deviates from the reference's unconditional sum.
		std::vector<float> perMod;
		for (const auto& [mod, targets] : actorIt->second) {
			float combined = 0.0f;
			bool any = false;
			if (const auto it = targets.find(morphTarget); it != targets.end()) {
				combined += it->second.value;
				any = true;
			}
			for (const auto& [target, contribution] : targets) {
				// A hidden node target drives NO slider: neutral is the only
				// honest morph-side reading of "collapsed" (see Ledger.h).
				// Which sliders a node key drives is a property of THIS
				// ACTOR's body, so the blend comes from its profile.
				if (IsMorphTarget(target) || IsHiddenLocked(a_actor, target)) {
					continue;
				}
				const auto* blends = BodyProfile::BlendForID(a_actor, target);
				if (!blends) {
					continue;
				}
				for (const auto& blend : *blends) {
					if (Lower(blend.slider) == a_sliderLower) {
						// The ramp factor keeps a mid-ramp node dragging its
						// derived slider shares along, so bone and slider
						// swell in step during incremental inflation.
						combined += (contribution.Effective() - 1.0f) * blend.weight *
						            RampFactorLocked(a_actor, target);
						any = true;
					}
				}
			}
			if (any) {
				perMod.push_back(combined);
			}
		}
		if (perMod.empty()) {
			return 0.0f;
		}
		return Calc::FoldSlider(_mode, std::move(perMod), _topX);
	}

	// Mid-ramp, a node target SHOWS DisplayedNode instead of its fold; its
	// transformed slider shares scale by the same progress ratio.
	float Ledger::RampFactorLocked(RE::FormID a_actor, const std::string& a_target) const
	{
		const auto actorIt = _display.find(a_actor);
		if (actorIt == _display.end()) {
			return 1.0f;
		}
		const auto it = actorIt->second.find(a_target);
		if (it == actorIt->second.end()) {
			return 1.0f;
		}
		const float fold = FoldNodeLocked(a_actor, a_target);
		if (std::abs(fold - 1.0f) < 0.0001f) {
			return 1.0f;
		}
		return (it->second - 1.0f) / (fold - 1.0f);
	}

	float Ledger::DirectMorph(RE::FormID a_actor, const std::string& a_sliderLower) const
	{
		std::scoped_lock lock(_lock);
		return DirectMorphLocked(a_actor, a_sliderLower);
	}

	// SLIF_Morph_Util.CalculateMorphValue: the raw stored values, summed. No
	// bounds, no multiplier, no calculation type - the reference stores morph
	// min/max/mult but never applies them, and Sexlab Survival reads this exact
	// number back out of StorageUtil as "slif_<morphName>".
	float Ledger::DirectMorphLocked(RE::FormID a_actor, const std::string& a_sliderLower) const
	{
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return 0.0f;
		}
		const std::string morphTarget = std::string{ kMorphPrefix } + a_sliderLower;
		float total = 0.0f;
		for (const auto& [mod, targets] : actorIt->second) {
			const auto it = targets.find(morphTarget);
			if (it != targets.end()) {
				total += it->second.value;
			}
		}
		return total;
	}

	bool Ledger::Migrated() const
	{
		std::scoped_lock lock(_lock);
		return _migrated;
	}

	void Ledger::SetMigrated(bool a_done)
	{
		std::scoped_lock lock(_lock);
		_migrated = a_done;
	}

	float Ledger::MasterScale() const
	{
		std::scoped_lock lock(_lock);
		return _masterScale;
	}

	void Ledger::SetMasterScale(float a_scale)
	{
		std::scoped_lock lock(_lock);
		_masterScale = (std::max)(0.0f, a_scale);
	}

	float Ledger::TargetScale(const std::string& a_scaleId) const
	{
		std::scoped_lock lock(_lock);
		const auto it = _targetScales.find(Lower(a_scaleId));
		return it != _targetScales.end() ? it->second : 1.0f;
	}

	void Ledger::SetTargetScale(const std::string& a_scaleId, float a_scale)
	{
		std::scoped_lock lock(_lock);
		const std::string id = Lower(a_scaleId);
		const float clamped = (std::max)(0.0f, a_scale);
		// 1.0 is the default: drop the entry rather than persist a no-op, so the
		// map (and the cosave) only ever carries deliberate user choices.
		if (std::abs(clamped - 1.0f) < kScaleEpsilon) {
			_targetScales.erase(id);
		} else {
			_targetScales[id] = clamped;
		}
	}

	float Ledger::EffectiveScale(const std::string& a_scaleId) const
	{
		std::scoped_lock lock(_lock);
		const auto it = _targetScales.find(Lower(a_scaleId));
		return _masterScale * (it != _targetScales.end() ? it->second : 1.0f);
	}

	std::vector<std::string> Ledger::ScaledTargets() const
	{
		std::scoped_lock lock(_lock);
		std::vector<std::string> out;
		out.reserve(_targetScales.size());
		for (const auto& [id, scale] : _targetScales) {
			out.push_back(id);
		}
		return out;
	}

	float Ledger::GetContribution(RE::FormID a_actor, const std::string& a_mod,
		const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return 0.0f;
		}
		const auto modIt = actorIt->second.find(Lower(a_mod));
		if (modIt == actorIt->second.end()) {
			return 0.0f;
		}
		const auto it = modIt->second.find(Lower(a_target));
		return it != modIt->second.end() ? it->second.value : 0.0f;
	}

	// Bounds and existence, for the reference's GetMinValue / GetMaxValue /
	// GetValue getters. kAllMods folds across contributors the way the value
	// aggregate does; a single mod reads its own row.
	float Ledger::GetBoundMin(RE::FormID a_actor, const std::string& a_mod,
		const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return 0.0f;
		}
		const std::string target = Lower(a_target);
		const std::string mod = Lower(a_mod);
		float lowest = std::numeric_limits<float>::infinity();
		for (const auto& [modKey, targets] : actorIt->second) {
			if (mod != kAllMods && modKey != mod) {
				continue;
			}
			const auto it = targets.find(target);
			if (it != targets.end()) {
				lowest = (std::min)(lowest, it->second.min);
			}
		}
		return std::isinf(lowest) ? 0.0f : lowest;
	}

	float Ledger::GetBoundMax(RE::FormID a_actor, const std::string& a_mod,
		const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return 0.0f;
		}
		const std::string target = Lower(a_target);
		const std::string mod = Lower(a_mod);
		float highest = -std::numeric_limits<float>::infinity();
		for (const auto& [modKey, targets] : actorIt->second) {
			if (mod != kAllMods && modKey != mod) {
				continue;
			}
			const auto it = targets.find(target);
			if (it != targets.end()) {
				highest = (std::max)(highest, it->second.max);
			}
		}
		return std::isinf(highest) ? 0.0f : highest;
	}

	bool Ledger::HasTarget(RE::FormID a_actor, const std::string& a_mod,
		const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return false;
		}
		const std::string target = Lower(a_target);
		const std::string mod = Lower(a_mod);
		for (const auto& [modKey, targets] : actorIt->second) {
			if (mod != kAllMods && modKey != mod) {
				continue;
			}
			if (targets.contains(target)) {
				return true;
			}
		}
		return false;
	}

	// ---- hidden nodes ------------------------------------------------------

	bool Ledger::Hide(RE::FormID a_actor, const std::string& a_target, float a_value)
	{
		std::scoped_lock lock(_lock);
		const std::string target = Lower(a_target);
		// The reference floors the pin at 0.0000001: a node scale of exactly 0
		// is a degenerate transform.
		const float pinned = (std::max)(a_value, 0.0000001f);
		auto& targets = _hidden[a_actor];
		const auto it = targets.find(target);
		if (it != targets.end() && std::abs(it->second - pinned) < 0.0000001f) {
			return false;  // already hidden at this value
		}
		targets[target] = pinned;
		return true;
	}

	bool Ledger::Show(RE::FormID a_actor, const std::string& a_target)
	{
		std::scoped_lock lock(_lock);
		const auto actorIt = _hidden.find(a_actor);
		if (actorIt == _hidden.end()) {
			return false;
		}
		if (actorIt->second.erase(Lower(a_target)) == 0) {
			return false;
		}
		if (actorIt->second.empty()) {
			_hidden.erase(actorIt);
		}
		return true;
	}

	bool Ledger::IsHidden(RE::FormID a_actor, const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		return IsHiddenLocked(a_actor, Lower(a_target));
	}

	bool Ledger::IsHiddenLocked(RE::FormID a_actor, const std::string& a_target) const
	{
		const auto actorIt = _hidden.find(a_actor);
		return actorIt != _hidden.end() && actorIt->second.contains(a_target);
	}

	std::vector<std::string> Ledger::ModsDriving(RE::FormID a_actor, const std::string& a_target) const
	{
		std::scoped_lock lock(_lock);
		std::vector<std::string> out;
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return out;
		}
		const std::string target = Lower(a_target);
		for (const auto& [mod, targets] : actorIt->second) {
			if (targets.find(target) != targets.end()) {
				out.push_back(mod);
			}
		}
		std::sort(out.begin(), out.end());
		return out;
	}

	std::vector<RE::FormID> Ledger::TrackedActors() const
	{
		std::scoped_lock lock(_lock);
		std::vector<RE::FormID> out;
		out.reserve(_actors.size());
		for (const auto& [formID, mods] : _actors) {
			out.push_back(formID);
		}
		return out;
	}

	std::vector<std::string> Ledger::TargetsOf(RE::FormID a_actor) const
	{
		std::scoped_lock lock(_lock);
		return TargetsOfLocked(a_actor);
	}

	std::vector<std::string> Ledger::TargetsOfLocked(RE::FormID a_actor) const
	{
		std::vector<std::string> out;
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return out;
		}
		for (const auto& [mod, targets] : actorIt->second) {
			for (const auto& [target, contribution] : targets) {
				if (std::find(out.begin(), out.end(), target) == out.end()) {
					out.push_back(target);
				}
			}
		}
		return out;
	}

	std::vector<std::string> Ledger::NodeTargetsOf(RE::FormID a_actor) const
	{
		std::scoped_lock lock(_lock);
		std::vector<std::string> out;
		for (auto& target : TargetsOfLocked(a_actor)) {
			if (!IsMorphTarget(target)) {
				out.push_back(std::move(target));
			}
		}
		return out;
	}

	std::vector<std::string> Ledger::MorphSlidersOf(RE::FormID a_actor) const
	{
		std::scoped_lock lock(_lock);
		std::vector<std::string> out;
		for (const auto& target : TargetsOfLocked(a_actor)) {
			if (IsMorphTarget(target)) {
				out.push_back(SliderOf(target));
			}
		}
		return out;
	}

	bool Ledger::HasEntries(RE::FormID a_actor) const
	{
		std::scoped_lock lock(_lock);
		return _actors.find(a_actor) != _actors.end();
	}

	namespace
	{
		const char* ActorLabel(RE::FormID a_id, std::string& a_buf)
		{
			const auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_id);
			a_buf = actor && actor->GetName() ? actor->GetName() : "<unresolved>";
			return a_buf.c_str();
		}
	}

	void Ledger::DumpActorToLog(RE::FormID a_actor) const
	{
		std::string nameBuf;
		logger::info("[Dump] actor {:08X} '{}'", a_actor, ActorLabel(a_actor, nameBuf));
		std::scoped_lock lock(_lock);
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			logger::info("[Dump]   (no ledger entries)");
			return;
		}
		for (const auto& [mod, targets] : actorIt->second) {
			for (const auto& [target, c] : targets) {
				logger::info("[Dump]   {} / {} = {} (min {} max {} mult {} -> effective {})",
					mod, target, c.value, c.min, c.max, c.mult, c.Effective());
			}
		}
	}

	void Ledger::DumpToLog() const
	{
		std::scoped_lock lock(_lock);
		logger::info("[Dump] ===== ledger: {} actor(s), calc={} (top_x={}), master scale {} =====",
			_actors.size(), Calc::TypeName(_mode), _topX, _masterScale);
		for (const auto& [id, scale] : _targetScales) {
			logger::info("[Dump]   user scale '{}' = {}", id, scale);
		}
		for (const auto& [formID, mods] : _actors) {
			DumpActorToLog(formID);
			for (const auto& target : TargetsOfLocked(formID)) {
				if (IsMorphTarget(target)) {
					const std::string slider = SliderOf(target);
					logger::info("[Dump]   fold slider '{}' -> {}", SliderName(slider),
						AggregateSliderLocked(formID, slider));
				} else {
					logger::info("[Dump]   fold node '{}' -> {}", target, Aggregate(formID, target));
				}
			}
		}
		logger::info("[Dump] ===== end =====");
	}

	void Ledger::OnGameSaved(SKSE::SerializationInterface* a_intfc)
	{
		using namespace Serialization;
		auto& inst = GetSingleton();
		std::scoped_lock lock(inst._lock);

		if (!a_intfc->OpenRecord(kLedgerRecord, kLedgerVersion)) {
			logger::error("[Ledger] failed to open cosave record");
			return;
		}

		Write(a_intfc, static_cast<std::uint32_t>(inst._mode));
		Write(a_intfc, inst._topX);
		Write(a_intfc, static_cast<std::uint32_t>(inst._gradual ? 1 : 0));
		Write(a_intfc, static_cast<std::uint32_t>(inst._migrated ? 1 : 0));

		Write(a_intfc, inst._masterScale);
		Write(a_intfc, static_cast<std::uint32_t>(inst._targetScales.size()));
		for (const auto& [id, scale] : inst._targetScales) {
			WriteString(a_intfc, id);
			Write(a_intfc, scale);
		}

		Write(a_intfc, static_cast<std::uint32_t>(inst._sliderNames.size()));
		for (const auto& [lower, original] : inst._sliderNames) {
			WriteString(a_intfc, lower);
			WriteString(a_intfc, original);
		}

		Write(a_intfc, static_cast<std::uint32_t>(inst._actors.size()));
		for (const auto& [formID, mods] : inst._actors) {
			Write(a_intfc, formID);
			Write(a_intfc, static_cast<std::uint32_t>(mods.size()));
			for (const auto& [mod, targets] : mods) {
				WriteString(a_intfc, mod);
				Write(a_intfc, static_cast<std::uint32_t>(targets.size()));
				for (const auto& [target, c] : targets) {
					WriteString(a_intfc, target);
					Write(a_intfc, c.value);
					Write(a_intfc, c.min);
					Write(a_intfc, c.max);
					Write(a_intfc, c.mult);
					Write(a_intfc, c.increment);
				}
			}
		}
		// v5: hidden nodes last, so the field is purely additive on the wire.
		Write(a_intfc, static_cast<std::uint32_t>(inst._hidden.size()));
		for (const auto& [formID, targets] : inst._hidden) {
			Write(a_intfc, formID);
			Write(a_intfc, static_cast<std::uint32_t>(targets.size()));
			for (const auto& [target, pin] : targets) {
				WriteString(a_intfc, target);
				Write(a_intfc, pin);
			}
		}

		logger::info("[Ledger] saved {} actor(s), calc={}", inst._actors.size(),
			Calc::TypeName(inst._mode));
	}

	void Ledger::OnGameLoaded(SKSE::SerializationInterface* a_intfc)
	{
		using namespace Serialization;
		auto& inst = GetSingleton();
		std::scoped_lock lock(inst._lock);
		inst._actors.clear();
		inst._hidden.clear();
		inst._display.clear();
		inst._sliderNames.clear();
		inst._targetScales.clear();
		inst._masterScale = 1.0f;
		inst._migrated = false;
		inst._gradual = true;  // the default for saves that predate the flag
		inst._mode = Calc::Type::kTopX;
		inst._topX = Calc::kDefaultTopX;

		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t length = 0;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type != kLedgerRecord) {
				logger::warn("[Ledger] unknown cosave record {:08X}", type);
				continue;
			}
			if (version > kLedgerVersion) {
				logger::warn("[Ledger] cosave written by a NEWER build (v{} > v{}) — entries dropped",
					version, kLedgerVersion);
				continue;
			}
			try {
				// v1 had neither the mode nor the slider table; every later field
				// is read only when the record is new enough to carry it, so old
				// saves upgrade in place instead of losing their inflation.
				if (version >= 2) {
					const auto mode = Read<std::uint32_t>(a_intfc, length);
					if (version >= 6) {
						inst._mode = Calc::IsValidType(mode) ? static_cast<Calc::Type>(mode)
						                                     : Calc::Type::kTopX;
						inst._topX = (std::max)(1u, Read<std::uint32_t>(a_intfc, length));
					} else {
						// v2-v5 stored the old two-value enum: 0 highest, 1 additive.
						inst._mode = mode == 1 ? Calc::Type::kAdditive : Calc::Type::kHighestWins;
					}
					if (version >= 7) {
						inst._gradual = Read<std::uint32_t>(a_intfc, length) != 0;
					}

					if (version >= 4) {
						inst._migrated = Read<std::uint32_t>(a_intfc, length) != 0;
					}
					if (version >= 3) {
						inst._masterScale = Read<float>(a_intfc, length);
						const auto scaleCount = Read<std::uint32_t>(a_intfc, length);
						for (std::uint32_t i = 0; i < scaleCount; ++i) {
							std::string id = ReadString(a_intfc, length);
							const float scale = Read<float>(a_intfc, length);
							inst._targetScales[std::move(id)] = scale;
						}
					}

					const auto sliderCount = Read<std::uint32_t>(a_intfc, length);
					for (std::uint32_t i = 0; i < sliderCount; ++i) {
						std::string lower = ReadString(a_intfc, length);
						std::string original = ReadString(a_intfc, length);
						inst._sliderNames[std::move(lower)] = std::move(original);
					}
				} else {
					logger::info("[Ledger] upgrading a v{} cosave record to v{}", version, kLedgerVersion);
				}

				const auto actorCount = Read<std::uint32_t>(a_intfc, length);
				for (std::uint32_t i = 0; i < actorCount; ++i) {
					const auto rawFormID = Read<RE::FormID>(a_intfc, length);

					// Read the payload FIRST so an unresolvable actor can be skipped
					// without desyncing the stream, then resolve (SLANG pattern).
					ModMap mods;
					const auto modCount = Read<std::uint32_t>(a_intfc, length);
					for (std::uint32_t m = 0; m < modCount; ++m) {
						std::string mod = ReadString(a_intfc, length);
						auto& targets = mods[mod];
						const auto targetCount = Read<std::uint32_t>(a_intfc, length);
						for (std::uint32_t t = 0; t < targetCount; ++t) {
							std::string target = ReadString(a_intfc, length);
							Contribution c;
							c.value = Read<float>(a_intfc, length);
							c.min = Read<float>(a_intfc, length);
							c.max = Read<float>(a_intfc, length);
							c.mult = Read<float>(a_intfc, length);
							if (version >= 7) {
								c.increment = Read<float>(a_intfc, length);
							}
							targets[target] = c;
						}
					}

					RE::FormID resolved = 0;
					if (!a_intfc->ResolveFormID(rawFormID, resolved)) {
						logger::warn("[Ledger] dropped unresolvable actor {:08X} (load order changed)", rawFormID);
						continue;
					}
					inst._actors[resolved] = std::move(mods);
				}

				if (version >= 5) {
					const auto hiddenActors = Read<std::uint32_t>(a_intfc, length);
					for (std::uint32_t i = 0; i < hiddenActors; ++i) {
						const auto rawFormID = Read<RE::FormID>(a_intfc, length);
						std::unordered_map<std::string, float> targets;
						const auto targetCount = Read<std::uint32_t>(a_intfc, length);
						for (std::uint32_t t = 0; t < targetCount; ++t) {
							std::string target = ReadString(a_intfc, length);
							targets[std::move(target)] = Read<float>(a_intfc, length);
						}
						RE::FormID resolved = 0;
						if (!a_intfc->ResolveFormID(rawFormID, resolved)) {
							continue;  // payload already consumed; stream stays in sync
						}
						inst._hidden[resolved] = std::move(targets);
					}
				}
			} catch (const std::exception& e) {
				logger::error("[Ledger] corrupt cosave record: {}", e.what());
				inst._actors.clear();
				inst._hidden.clear();
				inst._sliderNames.clear();
				inst._targetScales.clear();
				inst._masterScale = 1.0f;
				inst._migrated = false;
				inst._gradual = true;
				inst._display.clear();
				inst._mode = Calc::Type::kTopX;
				inst._topX = Calc::kDefaultTopX;
			}
		}
		logger::info("[Ledger] loaded {} actor(s), calc={} (top_x={})", inst._actors.size(),
			Calc::TypeName(inst._mode), inst._topX);
	}

	void Ledger::OnRevert(SKSE::SerializationInterface*)
	{
		auto& inst = GetSingleton();
		std::scoped_lock lock(inst._lock);
		inst._actors.clear();
		inst._hidden.clear();
		inst._display.clear();
		inst._sliderNames.clear();
		inst._targetScales.clear();
		inst._masterScale = 1.0f;
		inst._migrated = false;
		inst._gradual = true;
		inst._mode = Calc::Type::kTopX;
		inst._topX = Calc::kDefaultTopX;
		logger::info("[Ledger] reverted");
	}
}
