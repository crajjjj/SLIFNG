#pragma once

// Incremental inflation (PLAN P8): the native replacement for the reference's
// Papyrus drain queue.
//
// The reference's "Incremental" inflation type stepped a node toward its new
// value by `increment` per queue pass, with no cadence at all - the loop in
// SLIF_Calc.DoSingleQueue spins as fast as the VM schedules it, and every step
// paid a full set of StorageUtil reads and NiOverride writes. The visible
// effect (a body swelling in steps instead of snapping) is kept; the engine is
// not: one native ticker advances every active ramp on the main thread and
// each actor gets at most ONE coalesced apply per tick.
//
// Semantics:
//  * A ramp moves a node target's SHOWN value (the ledger's display override)
//    toward its FOLD by the triggering contribution's `increment` per quarter
//    second of game time, however many body rebuilds that time is spread over.
//  * The goal is re-read every tick, so a consumer that changes its mind
//    mid-ramp simply retargets the ramp - no queue to flush.
//  * hideNode stays instant (a chastity belt snaps shut), unregister stays
//    instant (the reference's RemoveNodeScale was instant too), and direct
//    morphs stay instant; sliders DERIVED from a ramping node follow the ramp
//    through the display override.
//  * Ramps are session state: a save mid-ramp reloads at the fold, which is
//    where the ramp was headed anyway.
//
// The on/off switch (Ledger::SetGradual) mirrors the reference's per-preset
// inflation_type as one global toggle. SLIF NG ships it ON - a deliberate
// departure from the reference's instant default
// (SLIF_Util.GetDefaultInflationType returns 1 = instant): stepped swelling
// reads better, and instant is one MCM click away.

#include <chrono>

namespace SLIFNG::Ramp
{
	// Milliseconds between ticks: the FASTEST a ramp can step. SGO4's Papyrus
	// smooth-scaling loop uses 100 ms, and at that cadence swelling reads as
	// motion rather than as four visible jumps a second.
	inline constexpr std::chrono::milliseconds kStepInterval{ 100 };

	// A consumer's `increment` means distance per QUARTER SECOND - that is what
	// the contract documents and what every existing caller was tuned against.
	// A step moves the increment scaled by the game time since that actor's
	// last step, so neither the cadence nor the rebuild budget below changes
	// how long a ramp takes - only how many steps it is drawn in.
	inline constexpr std::chrono::milliseconds kIncrementPeriod{ 250 };

	// Every step is a full skee body rebuild, and skee runs it synchronously
	// on the main thread: each morphable shape the actor wears (body, hands,
	// feet, armor, the player's first-person meshes too) gets its skin
	// partition deep-copied, every slider on the actor (an OBody preset sets
	// dozens) summed in again, and its vertices uploaded. That cost is the
	// body's, not ours - cheap on a light mesh, tens of milliseconds on a dense
	// one - and at a fixed ten steps a second players on a dense body with a
	// body preset reported the game falling to about 15 fps for as long as the
	// ramp ran, and only then. So steps draw on a budget of
	// measured rebuild time: rebuilds may take at most kRebuildShare of the
	// main thread across ALL ramping actors. A cheap body keeps the full
	// 100 ms cadence; an expensive one steps less often, in bigger steps.
	inline constexpr float kRebuildShare = 0.1f;

	// The budget's burst allowance: two ticks' worth of refill, so a few cheap
	// actors ramping together still all step in the same tick.
	inline constexpr float kBudgetCapMs =
		2.0f * kRebuildShare * static_cast<float>(kStepInterval.count());

	// The longest one expensive rebuild may hold the next step back. Past this
	// the budget stops counting the debt, so a lone ramp always steps at least
	// once a second - the cadence FHU's own loop stepped at by default.
	inline constexpr std::chrono::milliseconds kMaxStepWait{ 1000 };

	// Begin (or retarget) a ramp toward the target's current fold.
	// a_from: the value the target shows right now (seeds the display override).
	// a_step: the triggering contribution's increment (per quarter second).
	void Begin(RE::Actor* a_actor, const std::string& a_lowerTarget, float a_from, float a_step);

	// True while any ramp is in flight (diagnostics).
	[[nodiscard]] std::size_t ActiveCount();

	// Is THIS target still travelling on this actor? The settle notification
	// keys on it: an apply for a target with no ramp in flight is a final
	// value, and Tick drops an arrived target BEFORE it applies, so the last
	// step of a ramp reads as settled exactly once.
	[[nodiscard]] bool IsActive(RE::FormID a_actor, const std::string& a_lowerTarget);

	// Cancel one target's ramp (its display override included) - used by the
	// instant paths (unregister, hide/show) so a stale mid-ramp value can never
	// outlive the thing that was ramping.
	void Cancel(RE::FormID a_actor, const std::string& a_lowerTarget);

	// Cancel everything - the incremental toggle switching off, or a revert.
	void CancelAll();

	// Drop every ramp and its display overrides (revert / new save loaded).
	void OnRevert();
}
