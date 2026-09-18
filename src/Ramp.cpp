#include "Ramp.h"

#include "Ledger.h"
#include "Skee.h"

#include <thread>

namespace SLIFNG::Ramp
{
	namespace
	{
		constexpr float kArrived = 0.0001f;

		std::mutex g_lock;
		// actor -> target -> per-tick step
		std::unordered_map<RE::FormID, std::unordered_map<std::string, float>> g_active;
		std::atomic_bool g_threadStarted{ false };
		std::atomic_bool g_tickQueued{ false };

		// One step of every active ramp. MAIN THREAD (posted via the task
		// interface): it ends in skee geometry work.
		void Tick()
		{
			g_tickQueued.store(false, std::memory_order_release);

			// Menus/console freeze game time; a body visibly swelling behind the
			// journal would look wrong, so ramps hold their breath too.
			if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				return;
			}

			// Snapshot under the lock, apply outside it.
			std::vector<std::pair<RE::FormID, std::vector<std::pair<std::string, float>>>> work;
			{
				std::scoped_lock lock(g_lock);
				work.reserve(g_active.size());
				for (const auto& [formID, targets] : g_active) {
					std::vector<std::pair<std::string, float>> rows(targets.begin(), targets.end());
					work.emplace_back(formID, std::move(rows));
				}
			}

			auto& ledger = Ledger::GetSingleton();
			for (const auto& [formID, targets] : work) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
				if (!actor || !actor->Is3DLoaded()) {
					// Nobody is watching: snap to the fold and let the 3D-load
					// hook apply the final value whenever the actor streams in.
					std::scoped_lock lock(g_lock);
					for (const auto& [target, step] : targets) {
						ledger.ClearDisplay(formID, target);
					}
					g_active.erase(formID);
					continue;
				}

				// The user's speed preference scales the consumer's own increment.
				// Read per tick rather than baked in at Begin, so dragging the
				// MCM slider retimes ramps that are already travelling.
				// kStepFraction converts "per quarter second" (the increment's
				// contractual unit) into "per tick", so the cadence sets
				// smoothness and only the speed multiplier sets speed.
				const float speed = ledger.GetRampSpeed() * kStepFraction;
				std::vector<std::string> touched;
				for (const auto& [target, rawStep] : targets) {
					const float step = rawStep * speed;
					// Re-read the goal every tick: a consumer changing its mind
					// mid-ramp retargets the ramp instead of fighting it.
					float goal;
					if (IsMorphTarget(target)) {
						// Direct-morph ramps (SGO4's smooth scaling, natively):
						// the goal is the slider fold; hides only pin nodes.
						goal = ledger.FoldSlider(formID, SliderOf(target));
					} else {
						// A hide wins instantly, so its ramp just ends.
						if (ledger.IsHidden(formID, target)) {
							ledger.ClearDisplay(formID, target);
							std::scoped_lock lock(g_lock);
							g_active[formID].erase(target);
							continue;
						}
						goal = ledger.FoldNode(formID, target);
					}
					const float current = ledger.DisplayOf(formID, target).value_or(goal);
					const float remaining = goal - current;
					if (std::abs(remaining) <= (std::max)(step, kArrived)) {
						ledger.ClearDisplay(formID, target);  // shows the fold
						std::scoped_lock lock(g_lock);
						g_active[formID].erase(target);
					} else {
						const float next = current + (remaining > 0.0f ? step : -step);
						ledger.SetDisplay(formID, target, next);
					}
					touched.push_back(target);
				}
				if (!touched.empty()) {
					// One coalesced apply per actor per tick - the exact thing
					// the reference's per-step queue could not do.
					Skee::ApplyTargets(actor, touched);
				}
			}

			std::scoped_lock lock(g_lock);
			std::erase_if(g_active, [](const auto& entry) { return entry.second.empty(); });
		}

		// The ticker never touches the game: it only posts Tick() onto the main
		// thread while ramps exist, and idles otherwise.
		void TickerLoop()
		{
			for (;;) {
				std::this_thread::sleep_for(kStepInterval);
				{
					std::scoped_lock lock(g_lock);
					if (g_active.empty()) {
						continue;
					}
				}
				if (g_tickQueued.exchange(true, std::memory_order_acq_rel)) {
					continue;  // a tick is already in flight; do not pile up
				}
				if (auto* task = SKSE::GetTaskInterface()) {
					task->AddTask([]() { Tick(); });
				} else {
					g_tickQueued.store(false, std::memory_order_release);
				}
			}
		}

		void EnsureTicker()
		{
			if (!g_threadStarted.exchange(true)) {
				std::thread(TickerLoop).detach();
				logger::info("[Ramp] ticker started ({} ms/step)",
					std::chrono::duration_cast<std::chrono::milliseconds>(kStepInterval).count());
			}
		}
	}

	void Begin(RE::Actor* a_actor, const std::string& a_lowerTarget, float a_from, float a_step)
	{
		if (!a_actor || a_step <= 0.0f) {
			return;
		}
		auto& ledger = Ledger::GetSingleton();
		// Seed the display with what the target shows NOW, so nothing jumps at
		// ramp start; the first tick begins moving it. If a ramp is already in
		// flight for this target, the display is mid-way - keep it, retarget.
		if (!ledger.DisplayOf(a_actor->GetFormID(), a_lowerTarget)) {
			ledger.SetDisplay(a_actor->GetFormID(), a_lowerTarget, a_from);
		}
		{
			std::scoped_lock lock(g_lock);
			g_active[a_actor->GetFormID()][a_lowerTarget] = a_step;
		}
		EnsureTicker();
	}

	std::size_t ActiveCount()
	{
		std::scoped_lock lock(g_lock);
		std::size_t count = 0;
		for (const auto& [formID, targets] : g_active) {
			count += targets.size();
		}
		return count;
	}

	void Cancel(RE::FormID a_actor, const std::string& a_lowerTarget)
	{
		Ledger::GetSingleton().ClearDisplay(a_actor, a_lowerTarget);
		std::scoped_lock lock(g_lock);
		const auto it = g_active.find(a_actor);
		if (it != g_active.end()) {
			it->second.erase(a_lowerTarget);
			if (it->second.empty()) {
				g_active.erase(it);
			}
		}
	}

	void CancelAll()
	{
		auto& ledger = Ledger::GetSingleton();
		std::scoped_lock lock(g_lock);
		for (const auto& [formID, targets] : g_active) {
			for (const auto& [target, step] : targets) {
				ledger.ClearDisplay(formID, target);
			}
		}
		g_active.clear();
	}

	void OnRevert()
	{
		// The ledger's revert clears the display overrides themselves, but a
		// revert can race a tick, so clear both sides.
		CancelAll();
	}
}
