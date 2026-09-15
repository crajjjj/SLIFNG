#pragma once

// Application layer: pushes aggregated ledger values onto actors through
// RaceMenu/skee — morph-first, NiTransform node fallback (PLAN.md P1).
//
// THREADING: ApplyBodyMorphs / UpdateNodeTransforms do direct geometry work and
// are MAIN THREAD ONLY. Papyrus natives run on the VM's worker threads, so
// every path reachable from a native goes through a *Deferred entry point,
// which captures an ActorHandle and hands the skee work to
// SKSE::GetTaskInterface(). The non-deferred forms are main-thread-only and
// exist for the task bodies and the 3D-load hook.
//
// COALESCING: all of an actor's sliders are written first and the body is
// rebuilt ONCE (PLAN P1: "one apply/model-update per call, never per step").

namespace SLIFNG::Skee
{
	// The single skee key all applied output lives under — the reference's own
	// key, so legacy applied values are recomputed and overwritten in place at
	// migration (CONTRACT.md sec.4.3).
	inline constexpr const char* kAppliedKey = "SexLab Inflation Framework.esp";

	// Query skee's interfaces; call at SKSE kPostPostLoad.
	void Initialize();
	bool IsReady();      // morph application available (morph-first path)
	bool IsNodeReady();  // node-transform application available (fallback path)

	// Verbose per-call logging: every API entry, every apply, plus a skee
	// readback per slider. Costs a synchronous log line and an extra native
	// call per write, so it is a switch rather than a constant.
	void SetVerbose(bool a_on);
	bool Verbose();

	// What skee currently holds for one slider under OUR key - the readback the
	// diagnostics page compares against what we meant to write.
	float ReadMorph(RE::Actor* a_actor, const std::string& a_sliderName);

	// PROBE (PLAN P2): dump skee's own string table and one actor's morph list
	// to the log. If VisitStrings enumerates the slider names skee learned from
	// the loaded morphs.tri, body detection is genuinely possible and profiles
	// could be auto-picked / validated instead of merely declared. If it only
	// returns names something already SET, it is useless for detection and the
	// declarative profile stands. One launch settles it.
	void LogKnownMorphs(RE::Actor* a_actor);

	// ---- main-thread only (task bodies, 3D-load hook) ----
	void Apply(RE::Actor* a_actor, const std::string& a_lowerTarget);
	void ApplyTargets(RE::Actor* a_actor, const std::vector<std::string>& a_lowerTargets);
	void CleanLegacyKey(RE::Actor* a_actor, const std::string& a_key);
	void ClearOwned(RE::Actor* a_actor);
	void ReapplyActor(RE::Actor* a_actor);
	void ReapplyAll();

	// ---- safe from any thread (what the Papyrus natives call) ----
	void ApplyDeferred(RE::Actor* a_actor, const std::string& a_lowerTarget);
	void ApplyTargetsDeferred(RE::Actor* a_actor, std::vector<std::string> a_lowerTargets);
	void CleanLegacyKeyDeferred(RE::Actor* a_actor, const std::string& a_key);
	// Applies the listed targets, then clears anything still owned when the
	// ledger holds nothing more for the actor — one task, so the two halves
	// cannot race (CONTRACT sec.4.4, MME's blind unregister).
	void UnregisterDeferred(RE::Actor* a_actor, std::vector<std::string> a_lowerTargets);
	void ReapplyAllDeferred();

	// Start listening for actor 3D loads (call at kDataLoaded).
	void RegisterLoadHook();

	// Session-scoped caches die with the save (call from the revert callback).
	void OnRevert();
}
