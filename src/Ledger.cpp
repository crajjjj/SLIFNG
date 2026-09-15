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
		constexpr std::uint32_t kLedgerVersion = 4;
		constexpr float kScaleEpsilon = 0.0001f;
	}

	bool Ledger::Set(RE::FormID a_actor, const std::string& a_mod, const std::string& a_target,
		float a_value, float a_min, float a_max, float a_mult, const std::string& a_sliderName)
	{
		std::scoped_lock lock(_lock);
		RememberSliderLocked(a_sliderName);
		auto& entry = _actors[a_actor][Lower(a_mod)][Lower(a_target)];
		const Contribution next{ a_value, a_min, a_max, a_mult };
		const bool changed = entry.value != next.value || entry.min != next.min ||
		                     entry.max != next.max || entry.mult != next.mult;
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

		constexpr float neutral = 1.0f;  // node scales
		const auto actorIt = _actors.find(a_actor);
		if (actorIt == _actors.end()) {
			return neutral;
		}

		float highest = neutral;
		float deviationSum = 0.0f;
		float clampMax = -std::numeric_limits<float>::infinity();
		float clampMin = std::numeric_limits<float>::infinity();
		bool any = false;
		for (const auto& [mod, targets] : actorIt->second) {
			const auto it = targets.find(target);
			if (it == targets.end()) {
				continue;
			}
			const float effective = it->second.Effective();
			any = true;
			highest = (std::max)(highest, effective);
			deviationSum += effective - neutral;
			clampMax = (std::max)(clampMax, it->second.EffectiveMax());
			clampMin = (std::min)(clampMin, it->second.EffectiveMin());
		}
		if (!any) {
			return neutral;
		}

		switch (_mode) {
		case AggregationMode::kAdditive:
			// Sum of deviations from neutral (1.5 + 1.4 -> 1.9), clamped AFTER
			// aggregation by the per-contribution bounds - the safety valve
			// against compounding (CONTRACT 4.3), at BOTH ends so a stack of
			// below-neutral contributions cannot drive a node scale to <= 0.
			// `any` guarantees both bounds were set, so a legitimate bound of 0
			// clamps to 0 rather than reading as "unbounded".
			return std::clamp(neutral + deviationSum, clampMin, clampMax);
		case AggregationMode::kHighestWins:
		default:
			return highest;
		}
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

		float highest = 0.0f;
		float sum = 0.0f;
		float clampMax = -std::numeric_limits<float>::infinity();
		float clampMin = std::numeric_limits<float>::infinity();
		bool any = false;

		for (const auto& [mod, targets] : actorIt->second) {
			for (const auto& [target, contribution] : targets) {
				// Every source that drives this one skee slider folds together here:
				// a direct morph target, or a node key whose blend maps onto it.
				// Bounds are projected through the same transform as the value, so
				// additive keeps its safety valve in slider space too.
				std::optional<float> driven;
				float boundHi = 0.0f;
				float boundLo = 0.0f;
				if (target == morphTarget) {
					driven = contribution.Effective();
					boundHi = contribution.EffectiveMax();
					boundLo = contribution.EffectiveMin();
				} else if (!IsMorphTarget(target)) {
					// Which sliders a node key drives is a property of THIS ACTOR's
					// body, so the blend comes from its profile, not a global table.
					if (const auto* blends = BodyProfile::BlendForID(a_actor, target)) {
						for (const auto& blend : *blends) {
							if (Lower(blend.slider) == a_sliderLower) {
								driven = (contribution.Effective() - 1.0f) * blend.weight;
								boundHi = (contribution.EffectiveMax() - 1.0f) * blend.weight;
								boundLo = (contribution.EffectiveMin() - 1.0f) * blend.weight;
								break;
							}
						}
					}
				}
				if (driven) {
					any = true;
					highest = (std::max)(highest, *driven);
					sum += *driven;
					clampMax = (std::max)(clampMax, (std::max)(boundHi, boundLo));
					clampMin = (std::min)(clampMin, (std::min)(boundHi, boundLo));
				}
			}
		}
		if (!any) {
			return 0.0f;
		}
		return _mode == AggregationMode::kAdditive ? std::clamp(sum, clampMin, clampMax) : highest;
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
		logger::info("[Dump] ===== ledger: {} actor(s), mode={}, master scale {} =====",
			_actors.size(), _mode == AggregationMode::kAdditive ? "additive" : "highest-wins",
			_masterScale);
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
				}
			}
		}
		logger::info("[Ledger] saved {} actor(s), mode={}", inst._actors.size(),
			inst._mode == AggregationMode::kAdditive ? "additive" : "highest-wins");
	}

	void Ledger::OnGameLoaded(SKSE::SerializationInterface* a_intfc)
	{
		using namespace Serialization;
		auto& inst = GetSingleton();
		std::scoped_lock lock(inst._lock);
		inst._actors.clear();
		inst._sliderNames.clear();
		inst._targetScales.clear();
		inst._masterScale = 1.0f;
		inst._migrated = false;
		inst._mode = AggregationMode::kHighestWins;

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
					inst._mode = mode == static_cast<std::uint32_t>(AggregationMode::kAdditive)
					                 ? AggregationMode::kAdditive
					                 : AggregationMode::kHighestWins;

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
			} catch (const std::exception& e) {
				logger::error("[Ledger] corrupt cosave record: {}", e.what());
				inst._actors.clear();
				inst._sliderNames.clear();
				inst._targetScales.clear();
				inst._masterScale = 1.0f;
				inst._migrated = false;
				inst._mode = AggregationMode::kHighestWins;
			}
		}
		logger::info("[Ledger] loaded {} actor(s), mode={}", inst._actors.size(),
			inst._mode == AggregationMode::kAdditive ? "additive" : "highest-wins");
	}

	void Ledger::OnRevert(SKSE::SerializationInterface*)
	{
		auto& inst = GetSingleton();
		std::scoped_lock lock(inst._lock);
		inst._actors.clear();
		inst._sliderNames.clear();
		inst._targetScales.clear();
		inst._masterScale = 1.0f;
		inst._migrated = false;
		inst._mode = AggregationMode::kHighestWins;
		logger::info("[Ledger] reverted");
	}
}
