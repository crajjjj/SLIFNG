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
// Where the answer comes from, in order of reliability:
//   1. RACE  - hard per-actor evidence. UBE ships its own races, so this is
//              real detection, not a guess.
//   2. The INSTALLER - the user knows which body they built in BodySlide, and
//      no runtime probe can beat being told. The FOMOD writes the chosen
//      profile as default.ini, i.e. "the body this game uses".
//   3. PLUGIN presence - still supported for hand-written profiles, but NOT
//      used by the shipped ones: an installed plugin says a body exists, not
//      that BodySlide built it, and several can be installed at once.
//
// A profile maps each vocabulary key to a morph blend. A key the profile does
// NOT list has no usable slider on that body and falls through to the node
// path - which is what makes the fallback reachable at all.
//
// OVERLAYS (Bodies/Regions/*.ini) add or replace sections in a profile without
// editing it. A profile is a whole file and only one wins per actor, so without
// this a consumer mod could not contribute a custom region at all: it would
// have to overwrite the user's body choice to add one section. An overlay names
// the profile it applies to by Name= (the body is what decides slider names,
// not the actor), so a mod ships one small file per body it knows, each under
// its own filename - nothing ever file-conflicts in a mod manager. Merging
// happens once at load, so nothing downstream of ForActor changes.

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
		// Overlays only: the profile Name=s this applies to, lowercased ("*" = all).
		// Empty on a profile; non-empty is what marks a file as an overlay.
		std::vector<std::string> appliesTo;
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

	// Whether this actor's profile actually drives a_key with sliders. Lets a
	// consumer branch BEFORE writing, instead of inferring from a write that
	// returns false for several unrelated reasons: for a region (profile-only,
	// no bone behind it) false means the call would be a logged no-op, so the
	// consumer can fall back to a canonical key.
	[[nodiscard]] bool HasTarget(RE::Actor* a_actor, const std::string& a_key);

	// Actor -> profile assignments are session state; drop them with the save.
	void ClearCache();
}
