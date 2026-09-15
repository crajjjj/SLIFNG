#pragma once

// Per-ACTOR body profiles (PLAN P2).
//
// Why a declared profile rather than runtime detection: skee exposes no way to
// ask whether a body supports a given BodySlide slider. SetMorph/GetMorph are a
// dictionary keyed by (actor, slider, key) that never touches the mesh, so a
// slider missing from the body's morphs.tri stores and reads back exactly like
// a real one and is silently dropped later inside ApplyBodyMorphs. The only
// honest source of "what sliders does this body have" is a file that says so.
//
// Why per ACTOR and not per game: UBE is race-based and coexists with 3BA/BHUNP
// in one load order, so a single global body is wrong by construction.
//
// A profile maps each vocabulary key to a morph blend. A key the profile does
// NOT list has no usable slider on that body and falls through to the node
// path - which is what makes the fallback reachable at all.

namespace SLIFNG::BodyProfile
{
	struct Blend
	{
		std::string slider;  // BodySlide slider name, original case (skee gets this)
		float weight;        // slider value per 1.0 of node-scale deviation
	};

	struct Target
	{
		std::vector<Blend> morphs;  // empty => this body drives the key via nodes
	};

	struct Profile
	{
		std::string name;      // display name
		std::string file;      // source filename, for diagnostics
		std::string race;      // substring matched against the race EditorID
		std::vector<std::string> plugins;  // any one present => match
		bool isDefault{ false };
		std::unordered_map<std::string, Target> targets;  // canonical key -> blend
	};

	// Load Data/SLIFNG/Bodies/*.ini (alphabetical; default.ini is the fallback).
	// Safe to call again - a reload replaces the set and clears the actor cache.
	void Load();
	[[nodiscard]] std::size_t Count();

	// Resolve (and cache) the profile for one actor. Never null once Load() has
	// run: falls back to the built-in default when no file matches.
	[[nodiscard]] const Profile* ForActor(RE::Actor* a_actor);
	[[nodiscard]] std::string ResolvedName(RE::Actor* a_actor);

	// The blend this actor's body uses for a canonical key; nullptr/empty => node
	// path. The FormID overload exists for the ledger fold, which only holds ids.
	[[nodiscard]] const std::vector<Blend>* BlendFor(RE::Actor* a_actor, const std::string& a_key);
	[[nodiscard]] const std::vector<Blend>* BlendForID(RE::FormID a_actor, const std::string& a_key);

	// Actor -> profile assignments are session state; drop them with the save.
	void ClearCache();
}
