#include "Skee.h"

#include "API/SKEE.h"
#include "BodyProfile.h"
#include "Calc.h"
#include "Ledger.h"
#include "Ramp.h"
#include "Vocabulary.h"

#include <atomic>
#include <set>

namespace SLIFNG::Skee
{
	namespace
	{
		SKEE::IBodyMorphInterface* g_bodyMorph = nullptr;
		SKEE::INiTransformInterface* g_niTransform = nullptr;
		// Pre-AE RaceMenu (skee NiTransform v2). SKEE.h ships a purpose-built
		// shim for it - see SKEE::Legacy - whose *Reserved padding lands
		// AddNodeTransform, RemoveNodeTransformComponent and UpdateNodeTransforms
		// on the same vtable slots (5, 6, 16) as skee 0.4.16's own class. Only
		// one of these two is ever non-null.
		SKEE::Legacy::INiTransformInterface* g_niTransformLegacy = nullptr;
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

		// ---- the two node ABIs, isolated -----------------------------------
		// Every node write in this file goes through these three. v2 speaks
		// OverrideVariant and FixedString-by-value; v3 has typed setters. Nothing
		// outside here needs to know which RaceMenu is installed.

		void DropNodeScale(RE::Actor* a_actor, bool a_firstPerson, bool a_female,
			const char* a_node, const char* a_key)
		{
			if (g_niTransform) {
				g_niTransform->RemoveNodeTransformScale(a_actor, a_firstPerson, a_female, a_node, a_key);
			} else if (g_niTransformLegacy) {
				// index 0 is what OverrideVariant::SetFloat writes for a scale.
				g_niTransformLegacy->RemoveNodeTransformComponent(a_actor, a_firstPerson, a_female,
					SKEE::Legacy::FixedString(a_node), SKEE::Legacy::FixedString(a_key),
					SKEE::Legacy::OverrideVariant::Scale, 0);
			}
		}

		void PushNodeScale(RE::Actor* a_actor, bool a_firstPerson, bool a_female,
			const char* a_node, const char* a_key, float a_scale)
		{
			if (g_niTransform) {
				g_niTransform->AddNodeTransformScale(a_actor, a_firstPerson, a_female, a_node, a_key, a_scale);
			} else if (g_niTransformLegacy) {
				SKEE::Legacy::OverrideVariant value;
				value.SetFloat(SKEE::Legacy::OverrideVariant::Scale, a_scale);
				g_niTransformLegacy->AddNodeTransform(a_actor, a_firstPerson, a_female,
					SKEE::Legacy::FixedString(a_node), SKEE::Legacy::FixedString(a_key), value);
			}
		}

		void RefreshNode(RE::Actor* a_actor, bool a_firstPerson, bool a_female, const char* a_node)
		{
			if (g_niTransform) {
				g_niTransform->UpdateNodeTransforms(a_actor, a_firstPerson, a_female, a_node);
			} else if (g_niTransformLegacy) {
				g_niTransformLegacy->UpdateNodeTransforms(a_actor, a_firstPerson, a_female,
					SKEE::Legacy::FixedString(a_node));
			}
		}

		void SetNodeScale(RE::Actor* a_actor, const char* a_node, float a_scale)
		{
			const bool female = IsFemale(a_actor);
			const bool player = IsPlayer(a_actor);
			if (std::abs(a_scale - 1.0f) < kEpsilon) {
				DropNodeScale(a_actor, false, female, a_node, kAppliedKey);
				if (player) {
					DropNodeScale(a_actor, true, female, a_node, kAppliedKey);
				}
			} else {
				PushNodeScale(a_actor, false, female, a_node, kAppliedKey, a_scale);
				if (player) {
					PushNodeScale(a_actor, true, female, a_node, kAppliedKey, a_scale);
				}
			}
			if (a_actor->Is3DLoaded()) {
				RefreshNode(a_actor, false, female, a_node);
				if (player) {
					RefreshNode(a_actor, true, female, a_node);
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
			const float scaled = folded * ledger.EffectiveScaleFor(a_actor->GetFormID(), a_sliderLower);
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
			const char* mode = Calc::TypeName(ledger.GetMode());

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

			// --- profile-driven sliders first. Which sliders (if any) a target
			// drives is the actor's body profile's call, not a global table: a
			// UBE actor and a 3BA actor in one save write different sliders for
			// the same slif_breast. A custom "region:" target is profile-ONLY -
			// there is no bone behind it.
			const auto* blends = BodyProfile::BlendFor(a_actor, a_lowerTarget);
			if (IsRegionTarget(a_lowerTarget) && (!blends || blends->empty())) {
				logger::info("[Apply] {:08X} region '{}': profile '{}' has no such section — "
							 "nothing to drive",
					a_actor->GetFormID(), a_lowerTarget, BodyProfile::ResolvedName(a_actor));
				return false;
			}
			if (blends && !blends->empty()) {
				if (!IsReady()) {
					logger::warn("[Apply] {:08X} key '{}': BodyMorph unavailable — NOT applied",
						a_actor->GetFormID(), a_lowerTarget);
					return false;
				}
				std::string detail;
				for (const auto& blend : *blends) {
					const float folded = WriteSlider(a_actor, Lower(blend.slider), blend.slider);
					if (verbose) {
						detail += std::format(" '{}'={} (readback {})", blend.slider, folded,
							g_bodyMorph->GetMorph(a_actor, blend.slider.c_str(), kAppliedKey));
					}
				}
				if (verbose) {
					logger::info("[Apply] {:08X} '{}' node-key '{}' agg {} ({}, profile '{}') -> morph path:{} — 3D {}",
						a_actor->GetFormID(), a_actor->GetName(), a_lowerTarget,
						ledger.Aggregate(a_actor->GetFormID(), a_lowerTarget), mode,
						BodyProfile::ResolvedName(a_actor), detail,
						a_actor->Is3DLoaded() ? "loaded" : "UNLOADED");
				}
				return true;
			}

			const auto* target = Vocabulary::Find(a_lowerTarget);
			if (!target) {
				logger::warn("[Apply] '{}': not in vocabulary — skipped", a_lowerTarget);
				return false;
			}

			// --- node path (the profile lists no sliders for this key) ---
			if (!IsNodeReady()) {
				// ONCE per session, not per apply. The cause is global (skee gave us
				// no NiTransform at all, neither ABI), and a consumer re-sending a
				// butt value every cycle tick would otherwise flush the log
				// synchronously each time - warn flushes, info does not.
				static std::once_flag once;
				std::call_once(once, [&] {
					logger::warn("[Apply] key '{}' needs NiTransform, which is unavailable — node",
						a_lowerTarget);
					logger::warn("[Apply]   targets are NOT applied this session (see [Skee] above).");
				});
				return false;
			}
			// Node scales are multipliers around 1.0, so the user magnitude scales
			// the DEVIATION from neutral - not the raw value, which would move the
			// neutral point and resize an un-inflated actor.
			const float folded = ledger.Aggregate(a_actor->GetFormID(), a_lowerTarget);
			const float aggregated =
				1.0f + (folded - 1.0f) * ledger.EffectiveScaleFor(a_actor->GetFormID(), a_lowerTarget);
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

	// WHY THE VERSION SPLIT, and why the numbers are exact.
	//
	// skee's interfaces are versioned and GetVersion() sits at vtable slot 1 in
	// every published header, so it is the one method always safe to call before
	// trusting anything else. Verified against skee's own sources
	// (expired6978/SKSE64Plugins) as well as the two shipped skee64.dll builds:
	//
	//   IBodyMorphInterface - 22 methods, SAME ORDER in the 0.4.16-era header
	//     (skee/IPluginInterface.h @ 87a5cadd5, Oct 2020) and the current one,
	//     and SKEE.h matches both exactly. Morphs therefore need no shim at all.
	//
	//   INiTransformInterface - 22 methods matching SKEE.h exactly in the current
	//     header, and ABSENT from the 0.4.16 header: it was not public API then.
	//
	// So v2's "NiTransform" is not an older subset of v3, it is a different
	// interface. 0.4.16's concrete class reads
	//   GetVersion, Save, Load, Revert, AddNodeTransform,
	//   RemoveNodeTransformComponent, RemoveNodeTransform, ...
	// - 17 virtuals plus the destructor, exactly the 18 slots measured in the DLL.
	// It diverges at SLOT 2 (Save where v3 has Revert), has no typed per-component
	// setters (one AddNodeTransform taking an OverrideVariant), and passes strings
	// as SKEEFixedString BY VALUE rather than const char*.
	//
	// None of which means it is unreachable: SKEE.h ships SKEE::Legacy, a shim
	// built for exactly this vtable, whose *Reserved padding puts AddNodeTransform,
	// RemoveNodeTransformComponent and UpdateNodeTransforms on slots 5, 6 and 16 -
	// the same slots 0.4.16's own class uses, checked name by name against that
	// source. The Revert/Save/Load permutation at slots 2-4 is harmless because
	// none of those three is ever called through this pointer.
	//
	// So v2 is REBOUND through the legacy shim rather than refused, and both
	// RaceMenu generations get morphs AND bone scaling.
	constexpr std::uint32_t kMinBodyMorphVersion = 4;
	constexpr std::uint32_t kMinNiTransformVersion = 3;

	void Initialize()
	{
		auto* map = SKEE::GetInterfaceMap();
		if (!map) {
			// The handshake is an SKSE message to the plugin registered as "skee",
			// which answers only if RaceMenu's skee64.dll actually loaded for THIS
			// runtime. A RaceMenu built for the other runtime does not load at all,
			// and then nothing is there to answer - by far the most common cause.
			logger::error("[Skee] RaceMenu's skee did not answer the interface exchange.");
			logger::error("[Skee]   Runtime: {}", REL::Module::get().version().string());
			logger::error("[Skee]   Check that RaceMenu is installed AND built for this runtime:");
			logger::error("[Skee]   the Anniversary Edition build on 1.5.97 (or the reverse) does");
			logger::error("[Skee]   not load, so its skee64.dll never registers.");
			return;
		}

		g_bodyMorph = SKEE::GetBodyMorphInterface(map);
		g_niTransform = SKEE::GetNiTransformInterface(map);

		if (g_bodyMorph) {
			const auto version = g_bodyMorph->GetVersion();
			if (version < kMinBodyMorphVersion) {
				logger::error("[Skee] BodyMorph interface v{} is older than v{} — refusing it,",
					version, kMinBodyMorphVersion);
				logger::error("[Skee]   because its layout predates what this build calls. Update RaceMenu.");
				g_bodyMorph = nullptr;
			} else {
				logger::info("[Skee] BodyMorph interface v{}", version);
			}
		} else {
			logger::error("[Skee] BodyMorph interface missing — morph application disabled");
		}

		if (g_niTransform) {
			const auto version = g_niTransform->GetVersion();
			if (version < kMinNiTransformVersion) {
				// Pre-AE RaceMenu (0.4.16 on Skyrim SE 1.5.97). The modern typed
				// setters are unreachable here, but SKEE.h's Legacy shim IS the
				// right shape for this vtable, so re-bind through that and keep
				// node scaling working instead of going dark.
				g_niTransformLegacy = reinterpret_cast<SKEE::Legacy::INiTransformInterface*>(g_niTransform);
				g_niTransform = nullptr;
				logger::info("[Skee] NiTransform interface v{} — using the legacy (pre-AE) ABI",
					version);
			} else {
				logger::info("[Skee] NiTransform interface v{}", version);
			}
		} else {
			logger::error("[Skee] NiTransform interface missing — node fallback disabled");
		}
	}

	bool IsReady() { return g_bodyMorph != nullptr; }
	bool IsNodeReady() { return g_niTransform != nullptr || g_niTransformLegacy != nullptr; }

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

	std::vector<std::pair<std::string, float>> ForeignMorphKeys(RE::Actor* a_actor,
		const std::string& a_sliderName)
	{
		std::vector<std::pair<std::string, float>> out;
		if (!a_actor || !g_bodyMorph) {
			return out;
		}
		struct Collector : SKEE::IBodyMorphInterface::MorphKeyVisitor
		{
			std::vector<std::pair<std::string, float>>* out{ nullptr };
			void Visit(const char* a_key, float a_value) override
			{
				if (a_key && *a_key && _stricmp(a_key, kAppliedKey) != 0) {
					out->emplace_back(a_key, a_value);
				}
			}
		} visitor;
		visitor.out = &out;
		g_bodyMorph->VisitKeys(a_actor, a_sliderName.c_str(), visitor);
		return out;
	}

	void LogKnownMorphs(RE::Actor* a_actor)
	{
		if (!g_bodyMorph) {
			logger::warn("[Probe] BodyMorph interface unavailable");
			return;
		}

		struct Collector : SKEE::IBodyMorphInterface::StringVisitor
		{
			std::vector<std::string> names;
			void Visit(const char* a_name) override
			{
				if (a_name) {
					names.emplace_back(a_name);
				}
			}
		} all;
		g_bodyMorph->VisitStrings(all);
		std::sort(all.names.begin(), all.names.end());
		logger::info("[Probe] VisitStrings returned {} name(s)", all.names.size());

		// Are the sliders our profiles name actually among them? That is the
		// question that decides detect-vs-declare.
		for (const char* probe : { "PregnancyBelly", "BreastsSH", "BreastsNewSH",
				 "DoubleMelon", "BreastsBigger", "Juicy_breasts", "BellyFatty" }) {
			const bool known = std::find_if(all.names.begin(), all.names.end(),
									[probe](const std::string& n) { return Lower(n) == Lower(probe); }) !=
			                   all.names.end();
			logger::info("[Probe]   '{}' known to skee: {}", probe, known ? "YES" : "no");
		}
		std::string sample;
		for (std::size_t i = 0; i < all.names.size() && i < 60; ++i) {
			sample += (i ? ", " : "") + all.names[i];
		}
		logger::info("[Probe] first names: {}", sample.empty() ? "(none)" : sample);

		if (a_actor) {
			struct ActorMorphs : SKEE::IBodyMorphInterface::MorphVisitor
			{
				std::vector<std::string> names;
				void Visit(RE::TESObjectREFR*, const char* a_name) override
				{
					if (a_name) {
						names.emplace_back(a_name);
					}
				}
			} mine;
			g_bodyMorph->VisitMorphs(a_actor, mine);
			std::sort(mine.names.begin(), mine.names.end());
			std::string list;
			for (const auto& n : mine.names) {
				list += (list.empty() ? "" : ", ") + n;
			}
			logger::info("[Probe] actor {:08X} HasMorphs={} VisitMorphs -> {} name(s): {}",
				a_actor->GetFormID(), g_bodyMorph->HasMorphs(a_actor) ? "yes" : "no",
				mine.names.size(), list.empty() ? "(none)" : list);
		}
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

	// "This target has stopped moving." The signal a consumer needs to act ON a
	// finished shape (OLactis repositions particle emitters once breasts settle),
	// which polling cannot answer honestly - mid-ramp and final look identical
	// from outside. Deliberately NOT per step: at 10 ticks a second that would
	// be a flood, and every listener would just debounce it back into this.
	//
	// Every settle path funnels through ApplyTargets - an instant write, a
	// ramp's last step, an unregister, a magnitude change - so the one honest
	// test is "is a ramp still in flight for this target": Tick drops an arrived
	// target BEFORE applying, so its final step reads as settled exactly once,
	// and its earlier steps are suppressed.
	void NotifySettled(RE::Actor* a_actor, const std::string& a_lowerTarget)
	{
		auto* source = SKSE::GetModCallbackEventSource();
		if (!source) {
			return;
		}
		const float settled = Ledger::GetSingleton().Aggregate(a_actor->GetFormID(), a_lowerTarget);
		if (g_verbose.load(std::memory_order_relaxed)) {
			logger::info("[Settled] {:08X} '{}' -> {}", a_actor->GetFormID(), a_lowerTarget, settled);
		}
		SKSE::ModCallbackEvent event{
			RE::BSFixedString{ "SLIFNG_Settled" },
			RE::BSFixedString{ a_lowerTarget.c_str() },
			settled,
			a_actor
		};
		source->SendEvent(&event);
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
		// After the geometry is in place, never before: a listener that reads
		// the body back must see the settled shape, not the one being replaced.
		for (const auto& target : a_lowerTargets) {
			if (!Ramp::IsActive(a_actor->GetFormID(), target)) {
				NotifySettled(a_actor, target);
			}
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
						DropNodeScale(a_actor, false, female, node, a_key.c_str());
						DropNodeScale(a_actor, true, female, node, a_key.c_str());
						if (a_actor->Is3DLoaded()) {
							RefreshNode(a_actor, false, female, node);
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
						DropNodeScale(a_actor, false, female, node, kAppliedKey);
						DropNodeScale(a_actor, true, female, node, kAppliedKey);
						if (a_actor->Is3DLoaded()) {
							RefreshNode(a_actor, false, female, node);
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
