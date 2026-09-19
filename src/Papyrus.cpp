#include "Papyrus.h"

#include "BodyProfile.h"
#include "Calc.h"
#include "Ledger.h"
#include "Query.h"
#include "Ramp.h"
#include "Report.h"
#include "Skee.h"
#include "Vocabulary.h"

// Native surface for the pinned SLIF shims (dist/Core/source/scripts/SLIFNG.psc).
// The shims stay dumb: one call in, the whole pipeline (legacy cleanup,
// vocabulary, clamp, ledger, fold, single coalesced apply) happens here.

namespace SLIFNG::Papyrus
{
	namespace
	{
		// 2: increment parameter on Inflate/Morph, incremental inflation,
		//    the enumeration surface for mod authors.
		// 3: HasTarget, and region overlays (Bodies/Regions/*.ini).
		// 4: SetRampSpeed/GetRampSpeed.
		// 5: DrivenBy, and the SLIFNG_Settled mod event.
		constexpr std::int32_t kApiVersion = 5;

		// CONTRACT sec.4.1: exactly -1.0 means "not specified, keep the default".
		// Tested for equality, not `< 0` / `<= 0`: a deliberate multiplier of 0
		// (suppress entirely) and a negative minimum (morphs may go below zero)
		// are legitimate values a consumer can send.
		constexpr float kUnset = -1.0f;

		void ResolveDefaults(float& a_min, float& a_max, float& a_mult, float& a_increment)
		{
			if (a_min == kUnset) {
				a_min = 0.0f;
			}
			if (a_max == kUnset) {
				a_max = 100.0f;
			}
			if (a_mult == kUnset) {
				a_mult = 1.0f;
			}
			if (a_increment == kUnset) {
				a_increment = 0.1f;
			}
		}

		std::int32_t GetVersion(RE::StaticFunctionTag*)
		{
			return kApiVersion;
		}

		bool Inflate(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_key, float a_value, float a_min, float a_max, float a_mult,
			float a_increment, RE::BSFixedString a_oldMod)
		{
			logger::info("[API] Inflate({:08X} '{}', mod='{}', key='{}', value={}, min={}, max={}, mult={}, incr={}, old='{}')",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_mod.c_str(), a_key.c_str(), a_value, a_min, a_max, a_mult, a_increment,
				a_oldMod.c_str());
			if (!a_actor || a_mod.empty() || a_key.empty()) {
				logger::warn("[API]   -> rejected (null actor or empty mod/key)");
				return false;
			}
			if (!a_oldMod.empty()) {
				Skee::CleanLegacyKeyDeferred(a_actor, a_oldMod.c_str());
			}

			const std::string raw = Lower(a_key.c_str());
			if (Vocabulary::IsDeadKey(raw)) {
				logger::info("[API]   -> dead key '{}' (bug-compatible no-op, sender '{}')", raw, a_mod.c_str());
				return false;
			}
			// Accepts a slif_* key, a raw skeleton node name (FHU sends
			// "NPC Belly" verbatim; the reference tolerates it because
			// ConvertToNode passes unknown strings through), or a SLIF NG
			// "region:<name>" semantic key that the actor's body profile maps to
			// sliders. Every spelling of one physical thing lands on the SAME
			// ledger target, so consumers aggregate instead of clobbering.
			std::string key;
			if (IsRegionTarget(raw)) {
				key = raw;
			} else {
				const auto* resolved = Vocabulary::Resolve(raw);
				if (!resolved) {
					logger::warn("[API]   -> unknown node key '{}' from '{}' — ignored", raw, a_mod.c_str());
					return false;
				}
				key = resolved->key;
				if (key != raw) {
					logger::info("[API]   -> raw node '{}' routed to canonical target '{}'", raw, key);
				}
			}

			ResolveDefaults(a_min, a_max, a_mult, a_increment);
			auto& ledger = Ledger::GetSingleton();
			// What the target shows right now, captured BEFORE the write: the
			// seed a ramp starts stepping from.
			const float before = ledger.Aggregate(a_actor->GetFormID(), key);
			const bool changed = ledger.Set(
				a_actor->GetFormID(), a_mod.c_str(), key, a_value, a_min, a_max, a_mult, a_increment);
			if (!changed) {
				logger::info("[API]   -> unchanged (early-out)");
				return false;  // unchanged-value early-out (the reference lacked one)
			}
			// Incremental inflation: value CHANGES ramp; everything else stays
			// instant (hide wins immediately, unregister removes immediately -
			// the reference's RemoveNodeScale was instant too). Unloaded actors
			// skip the ramp: nobody is watching, and the 3D-load hook applies
			// the final value when they stream in.
			if (ledger.GetGradual() && a_actor->Is3DLoaded() &&
				!ledger.IsHidden(a_actor->GetFormID(), key)) {
				Ramp::Begin(a_actor, key, before, a_increment);
				return true;
			}
			Skee::ApplyDeferred(a_actor, key);
			return true;
		}

		bool Morph(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_morph, float a_value, float a_min, float a_max, float a_mult,
			float a_increment, RE::BSFixedString a_oldMod)
		{
			logger::info("[API] Morph({:08X} '{}', mod='{}', morph='{}', value={}, min={}, max={}, mult={}, incr={}, old='{}')",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_mod.c_str(), a_morph.c_str(), a_value, a_min, a_max, a_mult, a_increment,
				a_oldMod.c_str());
			if (!a_actor || a_mod.empty() || a_morph.empty()) {
				logger::warn("[API]   -> rejected (null actor or empty mod/morph)");
				return false;
			}
			if (!a_oldMod.empty()) {
				Skee::CleanLegacyKeyDeferred(a_actor, a_oldMod.c_str());
			}

			// The ledger key is lowercased (Papyrus string semantics); skee is
			// given the consumer's ORIGINAL spelling, which Set() remembers.
			const std::string slider = a_morph.c_str();
			const std::string target = std::string{ kMorphPrefix } + Lower(slider);
			ResolveDefaults(a_min, a_max, a_mult, a_increment);
			auto& ledger = Ledger::GetSingleton();
			// The slider's shown value BEFORE the write - the ramp's seed.
			const float before = ledger.Aggregate(a_actor->GetFormID(), target);
			const bool changed = ledger.Set(a_actor->GetFormID(), a_mod.c_str(),
				target, a_value, a_min, a_max, a_mult, a_increment, slider);
			if (!changed) {
				logger::info("[API]   -> unchanged (early-out)");
				return false;
			}
			// Direct morphs ramp too - SGO4's Papyrus smooth-scaling loop (one
			// full UpdateModelWeight per 0.01 step, on the VM) done natively:
			// the display override lives in slider space and Tick steps it
			// toward the slider fold.
			if (ledger.GetGradual() && a_actor->Is3DLoaded()) {
				Ramp::Begin(a_actor, target, before, a_increment);
				return true;
			}
			Skee::ApplyDeferred(a_actor, target);
			return true;
		}

		void UnregisterNode(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_key,
			RE::BSFixedString a_mod)
		{
			logger::info("[API] UnregisterNode({:08X} '{}', key='{}', mod='{}')",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_key.c_str(), a_mod.c_str());
			if (!a_actor || a_key.empty()) {
				return;
			}
			auto& ledger = Ledger::GetSingleton();
			const std::string raw = Lower(a_key.c_str());
			// Same dual spelling as Inflate (FHU deflates with "NPC Belly").
			const auto* resolved = Vocabulary::Resolve(raw);
			const std::string key = resolved ? resolved->key : raw;

			std::vector<std::string> affected;
			if (ledger.RemoveTarget(a_actor->GetFormID(), a_mod.c_str(), key)) {
				Ramp::Cancel(a_actor->GetFormID(), key);
				affected.push_back(key);
			}
			// A consumer may also name a slider here rather than a node key.
			const std::string morphTarget = std::string{ kMorphPrefix } + raw;
			if (ledger.RemoveTarget(a_actor->GetFormID(), a_mod.c_str(), morphTarget)) {
				affected.push_back(morphTarget);
			}
			if (!affected.empty()) {
				Skee::ApplyTargetsDeferred(a_actor, std::move(affected));
			}
		}

		void UnregisterMorph(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_morph,
			RE::BSFixedString a_mod)
		{
			logger::info("[API] UnregisterMorph({:08X} '{}', morph='{}', mod='{}')",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_morph.c_str(), a_mod.c_str());
			if (!a_actor || a_morph.empty()) {
				return;
			}
			const std::string target = std::string{ kMorphPrefix } + Lower(a_morph.c_str());
			if (Ledger::GetSingleton().RemoveTarget(a_actor->GetFormID(), a_mod.c_str(), target)) {
				Ramp::Cancel(a_actor->GetFormID(), target);
				Skee::ApplyDeferred(a_actor, target);
			}
		}

		void UnregisterMod(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod)
		{
			logger::info("[API] UnregisterMod({:08X} '{}', mod='{}')",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>", a_mod.c_str());
			if (!a_actor || a_mod.empty()) {
				return;
			}
			// Drop the mod's contributions, then recompute each affected target:
			// a target nobody drives any more folds to neutral, which clears it.
			// Apply + the ledger-empty clear ride ONE task so they cannot race.
			auto affected = Ledger::GetSingleton().RemoveMod(a_actor->GetFormID(), a_mod.c_str());
			for (const auto& target : affected) {
				Ramp::Cancel(a_actor->GetFormID(), target);
			}
			Skee::UnregisterDeferred(a_actor, std::move(affected));
		}

		// SLIF's Config.json calculation_type numbering: 0 Top X (the reference
		// default), 1 Highest wins, 2 Subtract and add one, 3 Square root,
		// 4 Average, 5 Additive.
		void SetAggregationMode(RE::StaticFunctionTag*, std::int32_t a_mode)
		{
			logger::info("[API] SetAggregationMode({})", a_mode);
			if (a_mode < 0 || !Calc::IsValidType(static_cast<std::uint32_t>(a_mode))) {
				logger::warn("[API]   -> calculation type {} out of range 0-5 — ignored", a_mode);
				return;
			}
			auto& ledger = Ledger::GetSingleton();
			const auto mode = static_cast<Calc::Type>(a_mode);
			if (mode != ledger.GetMode()) {
				ledger.SetMode(mode);
				// Type switch = recompute + one re-apply pass (CONTRACT sec.4.3) -
				// unlike the reference, where changing it left stale applied values.
				Skee::ReapplyAllDeferred();
			}
		}

		std::int32_t GetAggregationMode(RE::StaticFunctionTag*)
		{
			return static_cast<std::int32_t>(Ledger::GetSingleton().GetMode());
		}

		float GetContribution(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_target)
		{
			if (!a_actor) {
				return 0.0f;
			}
			return Ledger::GetSingleton().GetContribution(
				a_actor->GetFormID(), a_mod.c_str(), Lower(a_target.c_str()));
		}

		// ---- the reference's READ surface -----------------------------------
		// Sexlab Survival and Estrus Chaurus both ASK what an actor's inflation
		// is, not only set it: SLS gates a whole scene branch on
		// _SLS_BodyInflationScale, computed from three GetValue calls. Without
		// these the VM logged "Static function GetValue not found on object
		// slif_main" on a loop and SLS read every actor as flat.
		//
		// Reference semantics, preserved: modName "All Mods" reads the AGGREGATE,
		// any other name reads that mod's own row, and an ABSENT key returns the
		// caller's `default` rather than a neutral we invented.
		//
		// Departure, deliberate: the aggregate is returned with the user's
		// magnitude scaling applied, because the question is "how inflated does
		// this actor look" and the scale genuinely changes the answer. A consumer
		// that gates content on size should agree with the body it can see.

		// The bodies live in Query.h, shared with the inter-plugin SKSE API so
		// both callers always get the same answer.
		std::string ResolveTarget(const std::string& a_raw)
		{
			return Query::ResolveTarget(a_raw.c_str());
		}

		float GetValue(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_target, float a_default)
		{
			return Query::Value(a_actor, a_mod.c_str(), a_target.c_str(), a_default);
		}

		float GetMinValue(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_target, float a_default)
		{
			return Query::MinValue(a_actor, a_mod.c_str(), a_target.c_str(), a_default);
		}

		float GetMaxValue(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_target, float a_default)
		{
			return Query::MaxValue(a_actor, a_mod.c_str(), a_target.c_str(), a_default);
		}

		// ---- hidden nodes (Devious Devices) ---------------------------------

		void HideNode(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_key, float a_value, RE::BSFixedString a_oldMod)
		{
			logger::info("[API] HideNode({:08X} '{}', mod='{}', key='{}', value={})",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_mod.c_str(), a_key.c_str(), a_value);
			if (!a_actor || a_key.empty()) {
				logger::warn("[API]   -> rejected (null actor or empty key)");
				return;
			}
			if (!a_oldMod.empty()) {
				Skee::CleanLegacyKeyDeferred(a_actor, a_oldMod.c_str());
			}
			const std::string target = ResolveTarget(a_key.c_str());
			if (target.empty()) {
				logger::warn("[API]   -> unknown node key '{}' from '{}' — ignored",
					a_key.c_str(), a_mod.c_str());
				return;
			}
			if (!Ledger::GetSingleton().Hide(a_actor->GetFormID(), target, a_value)) {
				logger::info("[API]   -> already hidden (early-out)");
				return;
			}
			Ramp::Cancel(a_actor->GetFormID(), target);  // a belt snaps shut, never ramps
			Skee::ApplyDeferred(a_actor, target);
		}

		void ShowNode(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_key)
		{
			logger::info("[API] ShowNode({:08X} '{}', mod='{}', key='{}')",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_mod.c_str(), a_key.c_str());
			if (!a_actor || a_key.empty()) {
				return;
			}
			const std::string target = ResolveTarget(a_key.c_str());
			if (target.empty() || !Ledger::GetSingleton().Show(a_actor->GetFormID(), target)) {
				logger::info("[API]   -> was not hidden (no-op)");
				return;
			}
			Ramp::Cancel(a_actor->GetFormID(), target);
			// Restores whatever the fold says it should be now, which is the
			// reference's behaviour too (it re-reads the calculated value).
			Skee::ApplyDeferred(a_actor, target);
		}

		float GetApplied(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_target)
		{
			if (!a_actor) {
				return 0.0f;
			}
			return Ledger::GetSingleton().Aggregate(a_actor->GetFormID(), Lower(a_target.c_str()));
		}

		// --- user magnitude scaling (PLAN P5) ---
		// A change cannot go through the value early-out (no contribution moved),
		// so every setter re-applies every tracked actor explicitly.
		void SetMasterScale(RE::StaticFunctionTag*, float a_scale)
		{
			logger::info("[API] SetMasterScale({})", a_scale);
			auto& ledger = Ledger::GetSingleton();
			if (std::abs(ledger.MasterScale() - a_scale) > 0.0001f) {
				ledger.SetMasterScale(a_scale);
				Skee::ReapplyAllDeferred();
			}
		}

		float GetMasterScale(RE::StaticFunctionTag*) { return Ledger::GetSingleton().MasterScale(); }

		void SetTargetScale(RE::StaticFunctionTag*, RE::BSFixedString a_scaleId, float a_scale)
		{
			if (a_scaleId.empty()) {
				return;
			}
			logger::info("[API] SetTargetScale('{}', {})", a_scaleId.c_str(), a_scale);
			auto& ledger = Ledger::GetSingleton();
			if (std::abs(ledger.TargetScale(a_scaleId.c_str()) - a_scale) > 0.0001f) {
				ledger.SetTargetScale(a_scaleId.c_str(), a_scale);
				Skee::ReapplyAllDeferred();
			}
		}

		float GetTargetScale(RE::StaticFunctionTag*, RE::BSFixedString a_scaleId)
		{
			return Ledger::GetSingleton().TargetScale(a_scaleId.c_str());
		}

		// ---- batch writes (one coalesced apply; SGO4 sets many sliders per
		// update). Ramping entries ramp individually; everything else lands in
		// ONE deferred apply for the actor.
		std::int32_t InflateMany(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			std::vector<RE::BSFixedString> a_keys, std::vector<float> a_values,
			std::vector<float> a_mins, std::vector<float> a_maxs, std::vector<float> a_mults,
			std::vector<float> a_increments, RE::BSFixedString a_oldMod)
		{
			logger::info("[API] InflateMany({:08X} '{}', mod='{}', {} key(s))",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_mod.c_str(), a_keys.size());
			if (!a_actor || a_mod.empty()) {
				return 0;
			}
			if (!a_oldMod.empty()) {
				Skee::CleanLegacyKeyDeferred(a_actor, a_oldMod.c_str());
			}
			const auto at = [](const std::vector<float>& a_arr, std::size_t a_index) {
				return a_index < a_arr.size() ? a_arr[a_index] : -1.0f;
			};
			auto& ledger = Ledger::GetSingleton();
			const auto formID = a_actor->GetFormID();
			const bool gradual = ledger.GetGradual() && a_actor->Is3DLoaded();
			std::vector<std::string> batch;
			std::int32_t changed = 0;
			for (std::size_t i = 0; i < a_keys.size() && i < a_values.size(); ++i) {
				const std::string raw = Lower(a_keys[i].c_str());
				if (raw.empty() || Vocabulary::IsDeadKey(raw)) {
					continue;
				}
				std::string key;
				if (IsRegionTarget(raw)) {
					key = raw;
				} else if (const auto* resolved = Vocabulary::Resolve(raw)) {
					key = resolved->key;
				} else {
					logger::warn("[API]   -> unknown key '{}' in batch from '{}' — skipped", raw,
						a_mod.c_str());
					continue;
				}
				float min = at(a_mins, i);
				float max = at(a_maxs, i);
				float mult = at(a_mults, i);
				float increment = at(a_increments, i);
				ResolveDefaults(min, max, mult, increment);
				const float before = ledger.Aggregate(formID, key);
				if (!ledger.Set(formID, a_mod.c_str(), key, a_values[i], min, max, mult, increment)) {
					continue;
				}
				++changed;
				if (gradual && !ledger.IsHidden(formID, key)) {
					Ramp::Begin(a_actor, key, before, increment);
				} else if (std::find(batch.begin(), batch.end(), key) == batch.end()) {
					batch.push_back(key);
				}
			}
			if (!batch.empty()) {
				Skee::ApplyTargetsDeferred(a_actor, std::move(batch));
			}
			return changed;
		}

		std::int32_t MorphMany(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			std::vector<RE::BSFixedString> a_morphs, std::vector<float> a_values,
			std::vector<float> a_mins, std::vector<float> a_maxs, std::vector<float> a_mults,
			std::vector<float> a_increments, RE::BSFixedString a_oldMod)
		{
			logger::info("[API] MorphMany({:08X} '{}', mod='{}', {} slider(s))",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_mod.c_str(), a_morphs.size());
			if (!a_actor || a_mod.empty()) {
				return 0;
			}
			if (!a_oldMod.empty()) {
				Skee::CleanLegacyKeyDeferred(a_actor, a_oldMod.c_str());
			}
			const auto at = [](const std::vector<float>& a_arr, std::size_t a_index) {
				return a_index < a_arr.size() ? a_arr[a_index] : -1.0f;
			};
			auto& ledger = Ledger::GetSingleton();
			const auto formID = a_actor->GetFormID();
			const bool gradual = ledger.GetGradual() && a_actor->Is3DLoaded();
			std::vector<std::string> batch;
			std::int32_t changed = 0;
			for (std::size_t i = 0; i < a_morphs.size() && i < a_values.size(); ++i) {
				if (a_morphs[i].empty()) {
					continue;
				}
				const std::string slider = a_morphs[i].c_str();
				const std::string target = std::string{ kMorphPrefix } + Lower(slider);
				float min = at(a_mins, i);
				float max = at(a_maxs, i);
				float mult = at(a_mults, i);
				float increment = at(a_increments, i);
				ResolveDefaults(min, max, mult, increment);
				const float before = ledger.Aggregate(formID, target);
				if (!ledger.Set(formID, a_mod.c_str(), target, a_values[i], min, max, mult,
						increment, slider)) {
					continue;
				}
				++changed;
				if (gradual) {
					Ramp::Begin(a_actor, target, before, increment);
				} else if (std::find(batch.begin(), batch.end(), target) == batch.end()) {
					batch.push_back(target);
				}
			}
			if (!batch.empty()) {
				Skee::ApplyTargetsDeferred(a_actor, std::move(batch));
			}
			return changed;
		}

		// ---- per-actor magnitude (SGO4's BellyScaleMult, as a framework knob) --
		void SetActorTargetScale(RE::StaticFunctionTag*, RE::Actor* a_actor,
			RE::BSFixedString a_scaleId, float a_scale)
		{
			if (!a_actor || a_scaleId.empty()) {
				return;
			}
			logger::info("[API] SetActorTargetScale({:08X}, '{}', {})", a_actor->GetFormID(),
				a_scaleId.c_str(), a_scale);
			auto& ledger = Ledger::GetSingleton();
			if (std::abs(ledger.GetActorTargetScale(a_actor->GetFormID(), a_scaleId.c_str()) -
						 a_scale) < 0.0001f) {
				return;
			}
			ledger.SetActorTargetScale(a_actor->GetFormID(), a_scaleId.c_str(), a_scale);
			Skee::ApplyTargetsDeferred(a_actor, ledger.TargetsOf(a_actor->GetFormID()));
		}

		float GetActorTargetScale(RE::StaticFunctionTag*, RE::Actor* a_actor,
			RE::BSFixedString a_scaleId)
		{
			if (!a_actor || a_scaleId.empty()) {
				return 1.0f;
			}
			return Ledger::GetSingleton().GetActorTargetScale(a_actor->GetFormID(), a_scaleId.c_str());
		}

		// The reference's "slif_<morphName>": the cross-mod direct-morph total.
		// The SLIF_Morph shim mirrors it into StorageUtil under that exact name,
		// because Sexlab Survival reads it straight out of StorageUtil
		// (_SLS_BodyInflationTracking) rather than through any API.
		float GetCombinedMorph(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_morph)
		{
			return Query::CombinedMorph(a_actor, a_morph.c_str());
		}

		// ---- incremental inflation (PLAN P8) --------------------------------
		void SetIncrementalInflation(RE::StaticFunctionTag*, bool a_on)
		{
			auto& ledger = Ledger::GetSingleton();
			if (a_on == ledger.GetGradual()) {
				return;
			}
			logger::info("[API] SetIncrementalInflation({})", a_on);
			ledger.SetGradual(a_on);
			if (!a_on) {
				// Snap every in-flight ramp to its fold.
				Ramp::CancelAll();
				Skee::ReapplyAllDeferred();
			}
		}

		bool IsIncrementalInflation(RE::StaticFunctionTag*)
		{
			return Ledger::GetSingleton().GetGradual();
		}

		// Multiplier on the consumer's own increment. No re-apply: the tick
		// reads it every step, so in-flight ramps retime themselves and a
		// finished one is already at its fold.
		void SetRampSpeed(RE::StaticFunctionTag*, float a_speed)
		{
			logger::info("[API] SetRampSpeed({})", a_speed);
			Ledger::GetSingleton().SetRampSpeed(a_speed);
		}

		float GetRampSpeed(RE::StaticFunctionTag*)
		{
			return Ledger::GetSingleton().GetRampSpeed();
		}

		// ---- enumeration surface for mod authors ----------------------------
		// The values themselves come through GetValue/GetApplied/GetContribution;
		// these answer "what is there to ask about". The same surface is served
		// to C++ plugins via API/SLIFNG_API.h.
		bool HasTarget(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_target)
		{
			return Query::HasTarget(a_actor, a_target.c_str());
		}

		// HOW this actor's body realises a target, which HasTarget deliberately
		// does not answer (it reports "would a write do anything", and a
		// canonical key is always yes). The difference is visible: "sliders"
		// moves vertices and no bone, so anything rigged to that bone - a
		// particle emitter, an attached object - does NOT follow and needs its
		// own compensation; "node" scales the bone, so children come along.
		RE::BSFixedString DrivenBy(RE::StaticFunctionTag*, RE::Actor* a_actor,
			RE::BSFixedString a_target)
		{
			if (!a_actor || a_target.empty()) {
				return RE::BSFixedString{ "none" };
			}
			const std::string target = Query::ResolveTarget(a_target.c_str());
			if (target.empty()) {
				return RE::BSFixedString{ "none" };  // dead or unknown key
			}
			if (IsMorphTarget(target)) {
				return RE::BSFixedString{ "sliders" };  // named outright by the caller
			}
			if (BodyProfile::HasTarget(a_actor, target)) {
				return RE::BSFixedString{ "sliders" };
			}
			// A region has no bone behind it, so "not in the profile" is nothing
			// at all; a canonical key falls back to scaling its skeleton node.
			return RE::BSFixedString{ IsRegionTarget(target) ? "none" : "node" };
		}

		bool IsTracked(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Query::IsTracked(a_actor);
		}

		std::vector<RE::Actor*> GetTrackedActors(RE::StaticFunctionTag*)
		{
			std::vector<RE::Actor*> out;
			for (const auto formID : Ledger::GetSingleton().TrackedActors()) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
					out.push_back(actor);
				}
			}
			return out;
		}

		// Canonical node keys with a stored contribution ("slif_belly", ...).
		std::vector<RE::BSFixedString> GetNodeTargets(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			std::vector<RE::BSFixedString> out;
			if (!a_actor) {
				return out;
			}
			for (const auto& target : Ledger::GetSingleton().NodeTargetsOf(a_actor->GetFormID())) {
				out.emplace_back(target);
			}
			return out;
		}

		// BodySlide sliders with a stored DIRECT contribution, original case.
		std::vector<RE::BSFixedString> GetMorphTargets(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			std::vector<RE::BSFixedString> out;
			if (!a_actor) {
				return out;
			}
			auto& ledger = Ledger::GetSingleton();
			for (const auto& slider : ledger.MorphSlidersOf(a_actor->GetFormID())) {
				out.emplace_back(ledger.SliderName(slider));
			}
			return out;
		}

		// Which mods hold a contribution to one target (any spelling).
		std::vector<RE::BSFixedString> GetModsDriving(RE::StaticFunctionTag*, RE::Actor* a_actor,
			RE::BSFixedString a_target)
		{
			std::vector<RE::BSFixedString> out;
			if (!a_actor || a_target.empty()) {
				return out;
			}
			const std::string target = ResolveTarget(a_target.c_str());
			if (target.empty()) {
				return out;
			}
			for (const auto& mod : Ledger::GetSingleton().ModsDriving(a_actor->GetFormID(), target)) {
				out.emplace_back(mod);
			}
			return out;
		}

		// SLIF_Main.updateActorList's real job: a consumer pushes NEW BOUNDS
		// for values it already registered - Estrus Chaurus's MCM does this
		// when its max-scale sliders move. The reference re-registered every
		// actor; here it is one bounds pass over the ledger plus a re-apply of
		// whoever actually changed. The value itself never moves.
		// Per-ACTOR bounds, for the reference's per-actor bounds events
		// (SLIF_setMinimum and friends carry a Sender). UpdateModBounds below is
		// the load-order-wide form and must NOT be used for these: it walks every
		// tracked actor, so a mod tightening one actor's ceiling would move
		// everyone's.
		void UpdateActorBounds(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_key, float a_min, float a_max, float a_mult, float a_increment)
		{
			logger::info("[API] UpdateActorBounds({:08X}, mod='{}', key='{}', min={}, max={}, mult={}, incr={})",
				a_actor ? a_actor->GetFormID() : 0, a_mod.c_str(), a_key.c_str(), a_min, a_max,
				a_mult, a_increment);
			if (!a_actor || a_mod.empty() || a_key.empty()) {
				return;
			}
			const std::string target = Query::ResolveTarget(a_key.c_str());
			if (target.empty()) {
				logger::info("[API]   -> unknown key, ignored");
				return;
			}
			if (Ledger::GetSingleton().UpdateBounds(a_actor->GetFormID(), a_mod.c_str(), target,
					a_min, a_max, a_mult, a_increment)) {
				Skee::ApplyDeferred(a_actor, target);
			}
		}

		void UpdateModBounds(RE::StaticFunctionTag*, RE::BSFixedString a_mod,
			RE::BSFixedString a_key, float a_min, float a_max, float a_mult, float a_increment)
		{
			logger::info("[API] UpdateModBounds(mod='{}', key='{}', min={}, max={}, mult={}, incr={})",
				a_mod.c_str(), a_key.c_str(), a_min, a_max, a_mult, a_increment);
			if (a_mod.empty() || a_key.empty()) {
				return;
			}
			const std::string target = Query::ResolveTarget(a_key.c_str());
			if (target.empty() || IsMorphTarget(target)) {
				return;
			}
			auto& ledger = Ledger::GetSingleton();
			for (const auto formID : ledger.TrackedActors()) {
				if (!ledger.UpdateBounds(formID, a_mod.c_str(), target, a_min, a_max, a_mult,
						a_increment)) {
					continue;
				}
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
					Skee::ApplyDeferred(actor, target);
				}
			}
		}

		bool HasMigrated(RE::StaticFunctionTag*) { return Ledger::GetSingleton().Migrated(); }

		void SetMigrated(RE::StaticFunctionTag*, bool a_done)
		{
			logger::info("[API] SetMigrated({})", a_done);
			Ledger::GetSingleton().SetMigrated(a_done);
		}

		bool IsMorphEngineReady(RE::StaticFunctionTag*) { return Skee::IsReady(); }
		bool IsNodeEngineReady(RE::StaticFunctionTag*) { return Skee::IsNodeReady(); }

		std::int32_t TrackedActorCount(RE::StaticFunctionTag*)
		{
			return static_cast<std::int32_t>(Ledger::GetSingleton().TrackedActors().size());
		}

		void SetVerboseLogging(RE::StaticFunctionTag*, bool a_on)
		{
			Skee::SetVerbose(a_on);
		}

		// Interleaved {label, value, ...}; an empty value marks a section header.
		// Two halves so the MCM can lay them out as real columns.
		std::vector<RE::BSFixedString> GetActorReport(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Report::ForActor(a_actor);
		}

		std::vector<RE::BSFixedString> GetActorReportLeft(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Report::IdentityRows(a_actor);
		}

		std::vector<RE::BSFixedString> GetActorReportRight(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Report::StateRows(a_actor);
		}

		void LogActorReport(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			Report::LogForActor(a_actor);
		}

		void LogKnownMorphs(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			Skee::LogKnownMorphs(a_actor);
		}

		void DumpLedger(RE::StaticFunctionTag*)
		{
			Ledger::GetSingleton().DumpToLog();
		}

		void DumpActor(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			if (a_actor) {
				Ledger::GetSingleton().DumpActorToLog(a_actor->GetFormID());
			}
		}
	}

	bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm)
	{
		constexpr auto script = "SLIFNG";
		a_vm->RegisterFunction("GetVersion", script, GetVersion);
		a_vm->RegisterFunction("Inflate", script, Inflate);
		a_vm->RegisterFunction("Morph", script, Morph);
		a_vm->RegisterFunction("UnregisterNode", script, UnregisterNode);
		a_vm->RegisterFunction("UnregisterMorph", script, UnregisterMorph);
		a_vm->RegisterFunction("UnregisterMod", script, UnregisterMod);
		a_vm->RegisterFunction("SetAggregationMode", script, SetAggregationMode);
		a_vm->RegisterFunction("GetAggregationMode", script, GetAggregationMode);
		a_vm->RegisterFunction("GetContribution", script, GetContribution);
		a_vm->RegisterFunction("GetApplied", script, GetApplied);
		a_vm->RegisterFunction("GetValue", script, GetValue);
		a_vm->RegisterFunction("GetMinValue", script, GetMinValue);
		a_vm->RegisterFunction("GetMaxValue", script, GetMaxValue);
		a_vm->RegisterFunction("HideNode", script, HideNode);
		a_vm->RegisterFunction("ShowNode", script, ShowNode);
		a_vm->RegisterFunction("SetMasterScale", script, SetMasterScale);
		a_vm->RegisterFunction("GetMasterScale", script, GetMasterScale);
		a_vm->RegisterFunction("SetTargetScale", script, SetTargetScale);
		a_vm->RegisterFunction("GetTargetScale", script, GetTargetScale);
		a_vm->RegisterFunction("SetActorTargetScale", script, SetActorTargetScale);
		a_vm->RegisterFunction("GetActorTargetScale", script, GetActorTargetScale);
		a_vm->RegisterFunction("InflateMany", script, InflateMany);
		a_vm->RegisterFunction("MorphMany", script, MorphMany);
		a_vm->RegisterFunction("GetCombinedMorph", script, GetCombinedMorph);
		a_vm->RegisterFunction("SetIncrementalInflation", script, SetIncrementalInflation);
		a_vm->RegisterFunction("IsIncrementalInflation", script, IsIncrementalInflation);
		a_vm->RegisterFunction("SetRampSpeed", script, SetRampSpeed);
		a_vm->RegisterFunction("GetRampSpeed", script, GetRampSpeed);
		a_vm->RegisterFunction("HasTarget", script, HasTarget);
		a_vm->RegisterFunction("DrivenBy", script, DrivenBy);
		a_vm->RegisterFunction("IsTracked", script, IsTracked);
		a_vm->RegisterFunction("GetTrackedActors", script, GetTrackedActors);
		a_vm->RegisterFunction("GetNodeTargets", script, GetNodeTargets);
		a_vm->RegisterFunction("GetMorphTargets", script, GetMorphTargets);
		a_vm->RegisterFunction("GetModsDriving", script, GetModsDriving);
		a_vm->RegisterFunction("UpdateModBounds", script, UpdateModBounds);
		a_vm->RegisterFunction("UpdateActorBounds", script, UpdateActorBounds);
		a_vm->RegisterFunction("HasMigrated", script, HasMigrated);
		a_vm->RegisterFunction("SetMigrated", script, SetMigrated);
		a_vm->RegisterFunction("IsMorphEngineReady", script, IsMorphEngineReady);
		a_vm->RegisterFunction("IsNodeEngineReady", script, IsNodeEngineReady);
		a_vm->RegisterFunction("TrackedActorCount", script, TrackedActorCount);
		a_vm->RegisterFunction("SetVerboseLogging", script, SetVerboseLogging);
		a_vm->RegisterFunction("GetActorReport", script, GetActorReport);
		a_vm->RegisterFunction("GetActorReportLeft", script, GetActorReportLeft);
		a_vm->RegisterFunction("GetActorReportRight", script, GetActorReportRight);
		a_vm->RegisterFunction("LogActorReport", script, LogActorReport);
		a_vm->RegisterFunction("LogKnownMorphs", script, LogKnownMorphs);
		a_vm->RegisterFunction("DumpLedger", script, DumpLedger);
		a_vm->RegisterFunction("DumpActor", script, DumpActor);
		return true;
	}
}
