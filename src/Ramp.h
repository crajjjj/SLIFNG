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
// each actor gets ONE coalesced apply per tick.
//
// Semantics:
//  * A ramp moves a node target's SHOWN value (the ledger's display override)
//    toward its FOLD by the triggering contribution's `increment` per tick.
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
	// Milliseconds between steps. The reference had no defined cadence at all
	// (VM latency was the cadence). SGO4's Papyrus smooth-scaling loop uses
	// 100 ms, and it pays a full UpdateModelWeight per step on the VM to do it;
	// natively, with one coalesced apply per actor per tick, that cadence is
	// cheap - so this matches it and the swelling reads as motion rather than
	// as four visible jumps a second.
	inline constexpr std::chrono::milliseconds kStepInterval{ 100 };

	// A consumer's `increment` means distance per QUARTER SECOND - that is what
	// the contract documents and what every existing caller was tuned against.
	// Ticking faster must therefore NOT inflate faster: each tick moves its own
	// fraction of the increment, so shortening the interval buys smoothness at
	// exactly the same speed. Changing kStepInterval alone would silently
	// retime every consumer's ramp.
	inline constexpr std::chrono::milliseconds kIncrementPeriod{ 250 };
	inline constexpr float kStepFraction =
		static_cast<float>(kStepInterval.count()) / static_cast<float>(kIncrementPeriod.count());

	// Begin (or retarget) a ramp toward the target's current fold.
	// a_from: the value the target shows right now (seeds the display override).
	// a_step: per-tick step, from the triggering contribution's increment.
	void Begin(RE::Actor* a_actor, const std::string& a_lowerTarget, float a_from, float a_step);

	// True while any ramp is in flight (diagnostics).
	[[nodiscard]] std::size_t ActiveCount();

	// Cancel one target's ramp (its display override included) - used by the
	// instant paths (unregister, hide/show) so a stale mid-ramp value can never
	// outlive the thing that was ramping.
	void Cancel(RE::FormID a_actor, const std::string& a_lowerTarget);

	// Cancel everything - the incremental toggle switching off, or a revert.
	void CancelAll();

	// Drop every ramp and its display overrides (revert / new save loaded).
	void OnRevert();
}
