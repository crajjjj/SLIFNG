#include "Ramp.h"

#include "Ledger.h"
#include "Skee.h"

#include <functional>
#include <thread>

namespace SLIFNG::Ramp
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr float kArrived = 0.0001f;

		// A tick normally lands kStepInterval plus at most a frame after the
		// last one. A longer gap is time the ramp was not running for (a
		// loading screen, a stall), so it is banked only up to this.
		constexpr std::chrono::milliseconds kMaxTickGap{ 500 };

		// A step needs at least half a tick of banked time. The first tick after
		// a Begin can land a few milliseconds after it, and a full rebuild for a
		// near-zero move would only spend the budget the real first step needs.
		constexpr float kMinStepMs = 0.5f * static_cast<float>(kStepInterval.count());

		struct ActorRamp
		{
			// target -> the triggering contribution's increment
			std::unordered_map<std::string, float> targets;
			// Game time banked since this actor last stepped. A step spends all
			// of it, so a step the budget held back lands further, not later.
			float owedMs = 0.0f;
			Clock::time_point lastBanked = Clock::now();
			// What its rebuilds cost, for the one line logged when it settles.
			std::uint32_t steps = 0;
			float costMs = 0.0f;
			float worstMs = 0.0f;
			Clock::time_point startedAt = Clock::now();
		};

		std::mutex g_lock;
		std::unordered_map<RE::FormID, ActorRamp> g_active;
		std::atomic_bool g_threadStarted{ false };
		std::atomic_bool g_tickQueued{ false };

		// Rebuild budget in milliseconds of main-thread time. MAIN THREAD only
		// (Tick is its sole reader and writer), so it needs no lock.
		float g_budgetMs = kBudgetCapMs;
		Clock::time_point g_budgetAt = Clock::now();

		float Ms(Clock::duration a_duration)
		{
			return std::chrono::duration<float, std::milli>(a_duration).count();
		}

		void RefillBudget(Clock::time_point a_now)
		{
			const float elapsed = (std::min)(Ms(a_now - g_budgetAt), Ms(kMaxTickGap));
			g_budgetAt = a_now;
			g_budgetMs = (std::min)(g_budgetMs + elapsed * kRebuildShare, kBudgetCapMs);
		}

		void SpendBudget(float a_costMs)
		{
			// The floor caps the debt one rebuild can leave, which caps how long
			// it holds the next step back (kMaxStepWait).
			constexpr float kFloor = -kRebuildShare * static_cast<float>(kMaxStepWait.count());
			g_budgetMs = (std::max)(g_budgetMs - a_costMs, kFloor);
		}

		// Erase actors whose last target arrived, logging what their ramp cost.
		// Caller holds g_lock.
		void SweepSettled(Clock::time_point a_now)
		{
			for (auto it = g_active.begin(); it != g_active.end();) {
				const auto& ramp = it->second;
				if (!ramp.targets.empty()) {
					++it;
					continue;
				}
				if (ramp.steps > 0) {
					logger::info("[Ramp] {:08X} settled after {:.1f} s: {} step(s), rebuild avg {:.1f} ms, worst {:.1f} ms",
						it->first, Ms(a_now - ramp.startedAt) / 1000.0f, ramp.steps,
						ramp.costMs / static_cast<float>(ramp.steps), ramp.worstMs);
				}
				it = g_active.erase(it);
			}
		}

		struct Work
		{
			RE::FormID formID;
			float owedMs;
			std::vector<std::pair<std::string, float>> targets;
		};

		// One step of every ramp the budget allows. MAIN THREAD (posted via the
		// task interface): it ends in skee geometry work.
		void Tick()
		{
			g_tickQueued.store(false, std::memory_order_release);
			const auto now = Clock::now();

			// Menus/console freeze game time; a body visibly swelling behind the
			// journal would look wrong, so ramps hold their breath too. The
			// paused time is not ramp time: move every clock past it, or the
			// first tick after the menu would spend it in one jump.
			if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				g_budgetAt = now;
				std::scoped_lock lock(g_lock);
				for (auto& [formID, ramp] : g_active) {
					ramp.lastBanked = now;
				}
				return;
			}
			RefillBudget(now);

			// Bank the elapsed time and snapshot under the lock, apply outside it.
			std::vector<Work> work;
			{
				std::scoped_lock lock(g_lock);
				work.reserve(g_active.size());
				for (auto& [formID, ramp] : g_active) {
					ramp.owedMs += (std::min)(Ms(now - ramp.lastBanked), Ms(kMaxTickGap));
					ramp.lastBanked = now;
					work.push_back({ formID, ramp.owedMs, { ramp.targets.begin(), ramp.targets.end() } });
				}
			}
			// Longest-waiting first, so a tight budget is shared round-robin
			// rather than spent on whichever actor the map happens to list first.
			std::ranges::sort(work, std::greater{}, &Work::owedMs);

			auto& ledger = Ledger::GetSingleton();
			for (const auto& [formID, owedMs, targets] : work) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
				if (!actor || !actor->Is3DLoaded()) {
					// Nobody is watching: snap to the fold and let the 3D-load
					// hook apply the final value whenever the actor streams in.
					std::scoped_lock lock(g_lock);
					for (const auto& [target, increment] : targets) {
						ledger.ClearDisplay(formID, target);
					}
					g_active.erase(formID);
					continue;
				}
				if (owedMs < kMinStepMs || g_budgetMs <= 0.0f) {
					continue;  // keeps banking; its next step covers the wait
				}

				// The user's speed preference scales the consumer's own increment.
				// Read per tick rather than baked in at Begin, so dragging the
				// MCM slider retimes ramps that are already travelling. The owed
				// time converts "per quarter second" (the increment's contractual
				// unit) into this step's distance, so the cadence and the budget
				// set smoothness and only the speed multiplier sets speed.
				const float speed = ledger.GetRampSpeed() * owedMs /
				                    static_cast<float>(kIncrementPeriod.count());
				std::vector<std::string> touched;
				for (const auto& [target, increment] : targets) {
					const float step = increment * speed;
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
							if (const auto it = g_active.find(formID); it != g_active.end()) {
								it->second.targets.erase(target);
							}
							continue;
						}
						goal = ledger.FoldNode(formID, target);
					}
					const float current = ledger.DisplayOf(formID, target).value_or(goal);
					const float remaining = goal - current;
					if (std::abs(remaining) <= (std::max)(step, kArrived)) {
						ledger.ClearDisplay(formID, target);  // shows the fold
						std::scoped_lock lock(g_lock);
						if (const auto it = g_active.find(formID); it != g_active.end()) {
							it->second.targets.erase(target);
						}
					} else {
						const float next = current + (remaining > 0.0f ? step : -step);
						ledger.SetDisplay(formID, target, next);
					}
					touched.push_back(target);
				}
				if (touched.empty()) {
					continue;
				}

				// One coalesced apply per actor per step - the exact thing the
				// reference's per-step queue could not do. skee rebuilds inline
				// on this thread, so the wall time here IS the rebuild's cost.
				const auto applyStart = Clock::now();
				Skee::ApplyTargets(actor, touched);
				const float costMs = Ms(Clock::now() - applyStart);
				SpendBudget(costMs);
				if (Skee::Verbose()) {
					logger::info("[Ramp] {:08X} step over {:.0f} ms: {} target(s), rebuild {:.1f} ms, budget {:.1f} ms",
						formID, owedMs, touched.size(), costMs, g_budgetMs);
				}

				std::scoped_lock lock(g_lock);
				if (const auto it = g_active.find(formID); it != g_active.end()) {
					auto& ramp = it->second;
					// Only what this step spent: a Begin that raced the apply
					// starts a fresh bank, it does not inherit a zeroed one.
					ramp.owedMs = (std::max)(ramp.owedMs - owedMs, 0.0f);
					++ramp.steps;
					ramp.costMs += costMs;
					ramp.worstMs = (std::max)(ramp.worstMs, costMs);
				}
			}

			std::scoped_lock lock(g_lock);
			SweepSettled(Clock::now());
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
				logger::info("[Ramp] ticker started ({} ms/step at most, rebuilds capped at {:.0f}% of the main thread)",
					std::chrono::duration_cast<std::chrono::milliseconds>(kStepInterval).count(),
					kRebuildShare * 100.0f);
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
			// A new actor entry starts its bank and its clock now.
			g_active[a_actor->GetFormID()].targets[a_lowerTarget] = a_step;
		}
		EnsureTicker();
	}

	bool IsActive(RE::FormID a_actor, const std::string& a_lowerTarget)
	{
		std::scoped_lock lock(g_lock);
		const auto it = g_active.find(a_actor);
		return it != g_active.end() && it->second.targets.contains(a_lowerTarget);
	}

	std::size_t ActiveCount()
	{
		std::scoped_lock lock(g_lock);
		std::size_t count = 0;
		for (const auto& [formID, ramp] : g_active) {
			count += ramp.targets.size();
		}
		return count;
	}

	void Cancel(RE::FormID a_actor, const std::string& a_lowerTarget)
	{
		Ledger::GetSingleton().ClearDisplay(a_actor, a_lowerTarget);
		std::scoped_lock lock(g_lock);
		const auto it = g_active.find(a_actor);
		if (it != g_active.end()) {
			it->second.targets.erase(a_lowerTarget);
			if (it->second.targets.empty()) {
				g_active.erase(it);
			}
		}
	}

	void CancelAll()
	{
		auto& ledger = Ledger::GetSingleton();
		std::scoped_lock lock(g_lock);
		for (const auto& [formID, ramp] : g_active) {
			for (const auto& [target, increment] : ramp.targets) {
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
