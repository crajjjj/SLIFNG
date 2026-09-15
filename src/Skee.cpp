#include "Skee.h"

#include "API/SKEE.h"
#include "Ledger.h"
#include "Vocabulary.h"

#include <atomic>
#include <set>

namespace SLIFNG::Skee
{
	namespace
	{
		SKEE::IBodyMorphInterface* g_bodyMorph = nullptr;
		SKEE::INiTransformInterface* g_niTransform = nullptr;
		std::atomic_bool g_verbose{ true };  // dev default; release ships false

		// (actor formID, lowercase legacy key) pairs already cleaned this session.
		// Session-scoped, so it MUST be dropped on revert: loading a different
		// save in the same session needs its own cleanup pass.
		std::set<std::pair<RE::FormID, std::string>> g_cleanedLegacy;
		std::mutex g_cleanedLock;

		constexpr float kEpsilon = 0.0001f;

		// Consumers derive sex from Papyrus' GetLeveledActorBase() when they
		// write their own legacy-key transforms (FHU sr_inflateQuest:3010, BF NG),
		// and CleanLegacyKey must target the same isFemale bucket or it silently
		// misses. This IS that call: CommonLib's Actor::GetActorBase() resolves
		// GetBaseObject()->As<TESNPC>(), i.e. the reference's actual base object
		// — Papyrus' *Leveled* base. (Papyrus' own GetActorBase() is the one that
		// walks UP the template chain; CommonLib has no method by that meaning.)
		bool IsFemale(RE::Actor* a_actor)
		{
			const auto* base = a_actor->GetActorBase();
			return base && base->GetSex() == RE::SEX::kFemale;
		}

		bool IsPlayer(RE::Actor* a_actor)
		{
			return a_actor->IsPlayerRef();
		}

		void SetNodeScale(RE::Actor* a_actor, const char* a_node, float a_scale)
		{
			const bool female = IsFemale(a_actor);
			const bool player = IsPlayer(a_actor);
			if (std::abs(a_scale - 1.0f) < kEpsilon) {
				g_niTransform->RemoveNodeTransformScale(a_actor, false, female, a_node, kAppliedKey);
				if (player) {
					g_niTransform->RemoveNodeTransformScale(a_actor, true, female, a_node, kAppliedKey);
				}
			} else {
				g_niTransform->AddNodeTransformScale(a_actor, false, female, a_node, kAppliedKey, a_scale);
				if (player) {
					g_niTransform->AddNodeTransformScale(a_actor, true, female, a_node, kAppliedKey, a_scale);
				}
			}
			if (a_actor->Is3DLoaded()) {
				g_niTransform->UpdateNodeTransforms(a_actor, false, female, a_node);
				if (player) {
					g_niTransform->UpdateNodeTransforms(a_actor, true, female, a_node);
				}
			}
		}

		void SetMorphValue(RE::Actor* a_actor, const char* a_slider, float a_value)
		{
			if (std::abs(a_value) < kEpsilon) {
				g_bodyMorph->ClearMorph(a_actor, a_slider, kAppliedKey);
			} else {
				g_bodyMorph->SetMorph(a_actor, a_slider, kAppliedKey, a_value);
			}
		}

		// Writes one skee slider from the cross-source fold, so a node key and a
		// direct morph naming the same slider aggregate instead of clobbering.
		// a_sliderLower identifies the fold; a_sliderName is the spelling skee
		// gets (BodySlide slider names must keep the consumer's original case).
		float WriteSlider(RE::Actor* a_actor, const std::string& a_sliderLower,
			const std::string& a_sliderName)
		{
			auto& ledger = Ledger::GetSingleton();
			// The user magnitude rides on TOP of the fold and is never stored in a
			// contribution: consumers keep sending what they mean, the user only
			// decides how big that reads (PLAN P5).
			const float folded = ledger.AggregateSlider(a_actor->GetFormID(), a_sliderLower);
			const float scaled = folded * ledger.EffectiveScale(a_sliderLower);
			SetMorphValue(a_actor, a_sliderName.c_str(), scaled);
			return scaled;
		}

		// Stage one target's skee writes WITHOUT rebuilding the body; the caller
		// rebuilds once for the whole batch. Returns true if a morph was touched
		// (i.e. a body rebuild is owed).
		bool StageTarget(RE::Actor* a_actor, const std::string& a_lowerTarget)
		{
			auto& ledger = Ledger::GetSingleton();
			const bool verbose = g_verbose.load(std::memory_order_relaxed);
			const char* mode = ledger.GetMode() == AggregationMode::kAdditive ? "additive" : "highest";

			// --- morph target: fold every source driving this one slider ---
			if (IsMorphTarget(a_lowerTarget)) {
				if (!IsReady()) {
					logger::warn("[Apply] {:08X} target '{}': BodyMorph unavailable — NOT applied",
						a_actor->GetFormID(), a_lowerTarget);
					return false;
				}
				const std::string sliderLower = SliderOf(a_lowerTarget);
				const std::string sliderName = ledger.SliderName(sliderLower);
				const float folded = WriteSlider(a_actor, sliderLower, sliderName);
				if (verbose) {
					logger::info("[Apply] {:08X} '{}' morph '{}' = {} ({}) — readback {} — 3D {}",
						a_actor->GetFormID(), a_actor->GetName(), sliderName, folded, mode,
						g_bodyMorph->GetMorph(a_actor, sliderName.c_str(), kAppliedKey),
						a_actor->Is3DLoaded() ? "loaded" : "UNLOADED");
				}
				return true;
			}

			const auto* target = Vocabulary::Find(a_lowerTarget);
			if (!target) {
				logger::warn("[Apply] '{}': not in vocabulary — skipped", a_lowerTarget);
				return false;
			}

			// --- node key with a morph mapping: drive each mapped slider ---
			if (target->morphs[0].slider) {
				if (!IsReady()) {
					logger::warn("[Apply] {:08X} key '{}': BodyMorph unavailable — NOT applied",
						a_actor->GetFormID(), a_lowerTarget);
					return false;
				}
				std::string detail;
				for (const auto& blend : target->morphs) {
					if (blend.slider) {
						const float folded = WriteSlider(a_actor, Lower(blend.slider), blend.slider);
						if (verbose) {
							detail += std::format(" '{}'={} (readback {})", blend.slider, folded,
								g_bodyMorph->GetMorph(a_actor, blend.slider, kAppliedKey));
						}
					}
				}
				if (verbose) {
					logger::info("[Apply] {:08X} '{}' node-key '{}' agg {} ({}) -> morph path:{} — 3D {}",
						a_actor->GetFormID(), a_actor->GetName(), a_lowerTarget,
						ledger.Aggregate(a_actor->GetFormID(), a_lowerTarget), mode, detail,
						a_actor->Is3DLoaded() ? "loaded" : "UNLOADED");
				}
				return true;
			}

			// --- node fallback (no morph mapping: slif_butt, slif_scrotum) ---
			if (!IsNodeReady()) {
				logger::warn("[Apply] {:08X} key '{}': NiTransform unavailable — NOT applied",
					a_actor->GetFormID(), a_lowerTarget);
				return false;
			}
			// Node scales are multipliers around 1.0, so the user magnitude scales
			// the DEVIATION from neutral - not the raw value, which would move the
			// neutral point and resize an un-inflated actor.
			const float folded = ledger.Aggregate(a_actor->GetFormID(), a_lowerTarget);
			const float aggregated = 1.0f + (folded - 1.0f) * ledger.EffectiveScale(a_lowerTarget);
			for (const auto* node : target->nodes) {
				if (node) {
					SetNodeScale(a_actor, node, aggregated);
				}
			}
			if (verbose) {
				logger::info("[Apply] {:08X} '{}' node-key '{}' agg {} ({}) -> NiTransform fallback — 3D {}",
					a_actor->GetFormID(), a_actor->GetName(), a_lowerTarget, aggregated, mode,
					a_actor->Is3DLoaded() ? "loaded" : "UNLOADED");
			}
			return false;  // no morph touched: nothing to rebuild
		}

		// Run a skee job on the main thread with a handle that survives the hop.
		template <typename Fn>
		void OnMainThread(RE::Actor* a_actor, Fn&& a_job)
		{
			if (!a_actor) {
				return;
			}
			auto* task = SKSE::GetTaskInterface();
			if (!task) {
				logger::error("[Skee] task interface unavailable — skee work dropped");
				return;
			}
			const RE::ActorHandle handle = a_actor->GetHandle();
			task->AddTask([handle, job = std::forward<Fn>(a_job)]() {
				if (auto actorPtr = handle.get()) {
					job(actorPtr.get());
				}
			});
		}

		class ActorLoadSink : public RE::BSTEventSink<RE::TESObjectLoadedEvent>
		{
		public:
			static ActorLoadSink* GetSingleton()
			{
				static ActorLoadSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event,
				RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
			{
				if (!a_event || !a_event->loaded) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto formID = a_event->formID;
				if (!Ledger::GetSingleton().HasEntries(formID)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				// Defer off the load path: morph application rebuilds the body.
				if (auto* task = SKSE::GetTaskInterface()) {
					task->AddTask([formID]() {
						if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
							ReapplyActor(actor);
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Initialize()
	{
		auto* map = SKEE::GetInterfaceMap();
		if (!map) {
			logger::error("[Skee] interface map unavailable — is RaceMenu installed?");
			return;
		}
		g_bodyMorph = SKEE::GetBodyMorphInterface(map);
		g_niTransform = SKEE::GetNiTransformInterface(map);
		if (g_bodyMorph) {
			logger::info("[Skee] BodyMorph interface v{}", g_bodyMorph->GetVersion());
		} else {
			logger::error("[Skee] BodyMorph interface missing — morph application disabled");
		}
		if (g_niTransform) {
			logger::info("[Skee] NiTransform interface v{}", g_niTransform->GetVersion());
		} else {
			logger::error("[Skee] NiTransform interface missing — node fallback disabled");
		}
	}

	bool IsReady() { return g_bodyMorph != nullptr; }
	bool IsNodeReady() { return g_niTransform != nullptr; }

	void SetVerbose(bool a_on)
	{
		g_verbose.store(a_on, std::memory_order_relaxed);
		logger::info("[Skee] verbose logging {}", a_on ? "ON" : "OFF");
	}

	bool Verbose() { return g_verbose.load(std::memory_order_relaxed); }

	float ReadMorph(RE::Actor* a_actor, const std::string& a_sliderName)
	{
		if (!a_actor || !g_bodyMorph) {
			return 0.0f;
		}
		return g_bodyMorph->GetMorph(a_actor, a_sliderName.c_str(), kAppliedKey);
	}

	void RegisterLoadHook()
	{
		if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESObjectLoadedEvent>(ActorLoadSink::GetSingleton());
			logger::info("[Skee] 3D-load retry hook registered");
		} else {
			logger::error("[Skee] could not register the 3D-load retry hook");
		}
	}

	void ApplyTargets(RE::Actor* a_actor, const std::vector<std::string>& a_lowerTargets)
	{
		if (!a_actor) {
			return;
		}
		bool rebuild = false;
		for (const auto& target : a_lowerTargets) {
			rebuild |= StageTarget(a_actor, target);
		}
		// ONE rebuild for the whole batch, however many sliders it touched.
		if (rebuild && IsReady()) {
			g_bodyMorph->ApplyBodyMorphs(a_actor);
		}
	}

	void Apply(RE::Actor* a_actor, const std::string& a_lowerTarget)
	{
		ApplyTargets(a_actor, { a_lowerTarget });
	}

	void CleanLegacyKey(RE::Actor* a_actor, const std::string& a_key)
	{
		if (!a_actor || a_key.empty() || !IsReady()) {
			return;
		}
		const auto pair = std::make_pair(a_actor->GetFormID(), Lower(a_key));
		{
			std::scoped_lock lock(g_cleanedLock);
			if (!g_cleanedLegacy.insert(pair).second) {
				return;  // already cleaned this session
			}
		}

		g_bodyMorph->ClearBodyMorphKeys(a_actor, a_key.c_str());

		// The known consumers only ever scaled vocabulary nodes under their
		// legacy keys, so targeted removal covers migration; a full VisitNodes
		// sweep can replace this if an unknown legacy node ever surfaces.
		if (IsNodeReady()) {
			const bool female = IsFemale(a_actor);
			for (const auto& target : Vocabulary::kTargets) {
				for (const auto* node : target.nodes) {
					if (node) {
						g_niTransform->RemoveNodeTransformScale(a_actor, false, female, node, a_key.c_str());
						g_niTransform->RemoveNodeTransformScale(a_actor, true, female, node, a_key.c_str());
						if (a_actor->Is3DLoaded()) {
							g_niTransform->UpdateNodeTransforms(a_actor, false, female, node);
						}
					}
				}
			}
		}
		g_bodyMorph->ApplyBodyMorphs(a_actor);
		logger::info("[Skee] cleaned legacy key '{}' on {:08X}", a_key, a_actor->GetFormID());
	}

	void ClearOwned(RE::Actor* a_actor)
	{
		if (!a_actor || !IsReady()) {
			return;
		}
		g_bodyMorph->ClearBodyMorphKeys(a_actor, kAppliedKey);
		if (IsNodeReady()) {
			const bool female = IsFemale(a_actor);
			for (const auto& target : Vocabulary::kTargets) {
				for (const auto* node : target.nodes) {
					if (node) {
						g_niTransform->RemoveNodeTransformScale(a_actor, false, female, node, kAppliedKey);
						g_niTransform->RemoveNodeTransformScale(a_actor, true, female, node, kAppliedKey);
						if (a_actor->Is3DLoaded()) {
							g_niTransform->UpdateNodeTransforms(a_actor, false, female, node);
						}
					}
				}
			}
		}
		g_bodyMorph->ApplyBodyMorphs(a_actor);
		logger::info("[Skee] cleared all owned output on {:08X} (ledger empty)", a_actor->GetFormID());
	}

	void ReapplyActor(RE::Actor* a_actor)
	{
		if (!a_actor || !IsReady()) {
			return;
		}
		ApplyTargets(a_actor, Ledger::GetSingleton().TargetsOf(a_actor->GetFormID()));
	}

	void ReapplyAll()
	{
		if (!IsReady()) {
			logger::warn("[ReapplyAll] BodyMorph unavailable — skipped");
			return;
		}
		auto& ledger = Ledger::GetSingleton();
		int applied = 0;
		int deferred = 0;
		for (const auto formID : ledger.TrackedActors()) {
			auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
			if (!actor || !actor->Is3DLoaded()) {
				// Retried by the 3D-load hook (RegisterLoadHook) when they stream in.
				++deferred;
				continue;
			}
			ApplyTargets(actor, ledger.TargetsOf(formID));  // one rebuild per actor
			++applied;
		}
		logger::info("[ReapplyAll] {} actor(s) re-applied, {} deferred to 3D load", applied, deferred);
	}

	// ---- deferred entry points (safe from the Papyrus VM threads) ----

	void ApplyDeferred(RE::Actor* a_actor, const std::string& a_lowerTarget)
	{
		OnMainThread(a_actor, [target = a_lowerTarget](RE::Actor* actor) {
			ApplyTargets(actor, { target });
		});
	}

	void ApplyTargetsDeferred(RE::Actor* a_actor, std::vector<std::string> a_lowerTargets)
	{
		OnMainThread(a_actor, [targets = std::move(a_lowerTargets)](RE::Actor* actor) {
			ApplyTargets(actor, targets);
		});
	}

	void CleanLegacyKeyDeferred(RE::Actor* a_actor, const std::string& a_key)
	{
		OnMainThread(a_actor, [key = a_key](RE::Actor* actor) { CleanLegacyKey(actor, key); });
	}

	void UnregisterDeferred(RE::Actor* a_actor, std::vector<std::string> a_lowerTargets)
	{
		OnMainThread(a_actor, [targets = std::move(a_lowerTargets)](RE::Actor* actor) {
			ApplyTargets(actor, targets);
			// The ledger is authoritative. Only once it holds NOTHING for this
			// actor is any remaining output under our key unowned — left by
			// reference SLIF in a migrated save. That is what MME asks for when
			// it calls unregister blind (CONTRACT sec.4.4); doing it any earlier
			// would wipe other mods' live output.
			if (!Ledger::GetSingleton().HasEntries(actor->GetFormID())) {
				ClearOwned(actor);
			}
		});
	}

	void ReapplyAllDeferred()
	{
		if (auto* task = SKSE::GetTaskInterface()) {
			task->AddTask([]() { ReapplyAll(); });
		}
	}

	void OnRevert()
	{
		std::scoped_lock lock(g_cleanedLock);
		g_cleanedLegacy.clear();
		logger::info("[Skee] legacy-cleanup cache reverted");
	}
}
