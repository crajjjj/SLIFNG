#include "Papyrus.h"

#include "Ledger.h"
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
		constexpr std::int32_t kApiVersion = 1;

		// CONTRACT sec.4.1: exactly -1.0 means "not specified, keep the default".
		// Tested for equality, not `< 0` / `<= 0`: a deliberate multiplier of 0
		// (suppress entirely) and a negative minimum (morphs may go below zero)
		// are legitimate values a consumer can send.
		constexpr float kUnset = -1.0f;

		void ResolveDefaults(float& a_min, float& a_max, float& a_mult)
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
		}

		std::int32_t GetVersion(RE::StaticFunctionTag*)
		{
			return kApiVersion;
		}

		bool Inflate(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_key, float a_value, float a_min, float a_max, float a_mult,
			RE::BSFixedString a_oldMod)
		{
			logger::info("[API] Inflate({:08X} '{}', mod='{}', key='{}', value={}, min={}, max={}, mult={}, old='{}')",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_mod.c_str(), a_key.c_str(), a_value, a_min, a_max, a_mult, a_oldMod.c_str());
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
			// Accepts a slif_* key OR a raw skeleton node name (FHU sends
			// "NPC Belly" verbatim; the reference tolerates it because
			// ConvertToNode passes unknown strings through). Both spellings land
			// on the SAME canonical target, so two consumers driving one physical
			// node aggregate instead of clobbering.
			const auto* resolved = Vocabulary::Resolve(raw);
			if (!resolved) {
				logger::warn("[API]   -> unknown node key '{}' from '{}' — ignored", raw, a_mod.c_str());
				return false;
			}
			const std::string key = resolved->key;
			if (key != raw) {
				logger::info("[API]   -> raw node '{}' routed to canonical target '{}'", raw, key);
			}

			ResolveDefaults(a_min, a_max, a_mult);
			const bool changed = Ledger::GetSingleton().Set(
				a_actor->GetFormID(), a_mod.c_str(), key, a_value, a_min, a_max, a_mult);
			if (!changed) {
				logger::info("[API]   -> unchanged (early-out)");
				return false;  // unchanged-value early-out (the reference lacked one)
			}
			Skee::ApplyDeferred(a_actor, key);
			return true;
		}

		bool Morph(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::BSFixedString a_mod,
			RE::BSFixedString a_morph, float a_value, float a_min, float a_max, float a_mult,
			RE::BSFixedString a_oldMod)
		{
			logger::info("[API] Morph({:08X} '{}', mod='{}', morph='{}', value={}, min={}, max={}, mult={}, old='{}')",
				a_actor ? a_actor->GetFormID() : 0, a_actor ? a_actor->GetName() : "<none>",
				a_mod.c_str(), a_morph.c_str(), a_value, a_min, a_max, a_mult, a_oldMod.c_str());
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
			ResolveDefaults(a_min, a_max, a_mult);
			const bool changed = Ledger::GetSingleton().Set(
				a_actor->GetFormID(), a_mod.c_str(), target, a_value, a_min, a_max, a_mult, slider);
			if (!changed) {
				logger::info("[API]   -> unchanged (early-out)");
				return false;
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
			Skee::UnregisterDeferred(a_actor, std::move(affected));
		}

		void SetAggregationMode(RE::StaticFunctionTag*, std::int32_t a_mode)
		{
			logger::info("[API] SetAggregationMode({})", a_mode);
			auto& ledger = Ledger::GetSingleton();
			const auto mode = a_mode == 1 ? AggregationMode::kAdditive : AggregationMode::kHighestWins;
			if (mode != ledger.GetMode()) {
				ledger.SetMode(mode);
				// Mode switch = recompute + one re-apply pass (CONTRACT sec.4.3).
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
		std::vector<RE::BSFixedString> GetActorReport(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Report::ForActor(a_actor);
		}

		void LogActorReport(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			Report::LogForActor(a_actor);
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
		a_vm->RegisterFunction("SetMasterScale", script, SetMasterScale);
		a_vm->RegisterFunction("GetMasterScale", script, GetMasterScale);
		a_vm->RegisterFunction("SetTargetScale", script, SetTargetScale);
		a_vm->RegisterFunction("GetTargetScale", script, GetTargetScale);
		a_vm->RegisterFunction("IsMorphEngineReady", script, IsMorphEngineReady);
		a_vm->RegisterFunction("IsNodeEngineReady", script, IsNodeEngineReady);
		a_vm->RegisterFunction("TrackedActorCount", script, TrackedActorCount);
		a_vm->RegisterFunction("SetVerboseLogging", script, SetVerboseLogging);
		a_vm->RegisterFunction("GetActorReport", script, GetActorReport);
		a_vm->RegisterFunction("LogActorReport", script, LogActorReport);
		a_vm->RegisterFunction("DumpLedger", script, DumpLedger);
		a_vm->RegisterFunction("DumpActor", script, DumpActor);
		return true;
	}
}
