#pragma once

// SLIF NG inter-plugin query API - the C++ mirror of the Papyrus read surface.
//
// This header is the PUBLIC contract for other SKSE plugins: copy it into your
// project as-is (it has no dependency on SLIF NG's sources, only a forward
// declaration of RE::Actor). Everything here is READ-ONLY; to write values,
// use the Papyrus surface (SLIF_Main.inflate / SLIF_Morph.morph), which is the
// pinned SLIF-compatible entry point for all mutations.
//
// USAGE (from your plugin, at SKSE::MessagingInterface::kPostPostLoad or
// later, so SLIF NG has registered its listener):
//
//   #include "SLIFNG_API.h"
//
//   SLIFNG_API::InterfaceExchangeMessage msg;
//   SKSE::GetMessagingInterface()->Dispatch(
//       SLIFNG_API::InterfaceExchangeMessage::kMessageType,
//       &msg, sizeof(msg), "SLIFNG");
//   if (msg.query) {
//       const float belly = msg.query->GetValue(actor, "All Mods", "slif_belly", 1.0f);
//   }
//   // a newer interface: ask, then cast (see InterfaceExchangeMessage below)
//   if (msg.query && msg.query->Version() >= 2) {
//       auto* q2 = static_cast<SLIFNG_API::IQueryInterface2*>(msg.query);
//       const bool hasWeight = q2->HasTarget(actor, "region:weight");
//   }
//
// msg.query stays null when SLIF NG is not installed - no link-time coupling.
// The pointer is valid for the lifetime of the game process; every call is
// thread-safe (the store is mutex-guarded), though RE::Actor* arguments must
// of course be valid when you pass them.
//
// Target spellings accepted everywhere a key is taken, exactly as in Papyrus:
// a slif_* key ("slif_belly"), a raw skeleton node ("NPC Belly"), or
// "morph:<slider>" ("morph:PregnancyBelly"). Mod names compare
// case-insensitively; "All Mods" reads the aggregate. An absent row returns
// YOUR default - never an invented neutral.

#include <cstdint>

namespace RE
{
	class Actor;
}

namespace SLIFNG_API
{
	// The NEWEST IQueryInterfaceN this build serves, and what Version() returns.
	// Monotonic: bump when an interface is added, never edit an existing one's
	// layout. Gate a newer call on `Version() >= N`, never on equality - 0.3.0
	// shipped a header calling this a breaking-change counter, and a consumer
	// that wrote `Version() == 1` will lock itself out of every later build.
	inline constexpr std::uint32_t kQueryVersion = 2;

	class IQueryInterface1
	{
	public:
		virtual std::uint32_t Version() const = 0;

		// Whether SLIF NG holds anything at all for this actor.
		virtual bool IsTracked(RE::Actor* a_actor) const = 0;
		virtual std::uint32_t TrackedActorCount() const = 0;

		// The stored value: a mod's own row, or the aggregate for "All Mods"
		// (aggregate comes with the user's magnitude scaling applied - it
		// answers "how inflated does this actor look"). Absent -> a_default.
		virtual float GetValue(RE::Actor* a_actor, const char* a_modName, const char* a_target,
			float a_default) const = 0;
		virtual float GetMinValue(RE::Actor* a_actor, const char* a_modName, const char* a_target,
			float a_default) const = 0;
		virtual float GetMaxValue(RE::Actor* a_actor, const char* a_modName, const char* a_target,
			float a_default) const = 0;

		// The value currently shown for a target, WITHOUT the user magnitude:
		// 1.0-neutral for node targets, 0.0-neutral for "morph:<slider>".
		// Mid-ramp this is the in-flight value, as the reference's was.
		virtual float GetApplied(RE::Actor* a_actor, const char* a_target) const = 0;

		// The cross-mod direct-morph total for one BodySlide slider - the
		// reference's "slif_<morphName>" StorageUtil value.
		virtual float GetCombinedMorph(RE::Actor* a_actor, const char* a_slider) const = 0;

		// SLIF's Config.json calculation_type numbering:
		// 0 Top X (default), 1 Highest wins, 2 Subtract and add one,
		// 3 Square root, 4 Average, 5 Additive.
		virtual std::uint32_t GetCalculationType() const = 0;

		// Whether incremental (ramped) inflation is switched on.
		virtual bool IsIncrementalInflation() const = 0;
	};

	// Added in query version 2. Null on an older SLIF NG - check before use.
	class IQueryInterface2 : public IQueryInterface1
	{
	public:
		// Would a write to this target do anything on this actor?
		//
		// A canonical key ("slif_belly") or a morph ("morph:X") is always true:
		// worst case the skeleton node drives it. A semantic region
		// ("region:weight") is true only when this actor's body profile defines
		// that section, because a region is sliders or nothing - so this is the
		// branch to take before sending one, with a canonical key as fallback.
		// Dead keys and unknown spellings are false.
		virtual bool HasTarget(RE::Actor* a_actor, const char* a_target) const = 0;
	};

	// The exchange struct NEVER grows: SLIF NG 0.3.0 shipped a server that
	// size-checks the dispatch exactly, so adding a field here would make that
	// build reject the message and hand a consumer nothing at all. Reach a newer
	// interface by asking the one you were given for its version, then casting -
	// the implementation derives the whole chain by single inheritance, so the
	// pointer value is identical and Version() is what makes the cast sound.
	//
	//   if (msg.query && msg.query->Version() >= 2) {
	//       auto* q2 = static_cast<SLIFNG_API::IQueryInterface2*>(msg.query);
	//       const bool hasWeight = q2->HasTarget(actor, "region:weight");
	//   }
	struct InterfaceExchangeMessage
	{
		enum : std::uint32_t
		{
			kMessageType = 0x534C4946  // 'SLIF'
		};

		IQueryInterface1* query{ nullptr };
	};
}
