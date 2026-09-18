#include "APIServer.h"

#include "API/SLIFNG_API.h"
#include "Calc.h"
#include "Ledger.h"
#include "Query.h"

namespace SLIFNG::APIServer
{
	namespace
	{
		// One static implementation behind the public pure-virtual contract.
		// Everything routes through Query so a C++ caller and a Papyrus caller
		// can never get different answers.
		class QueryImpl final : public SLIFNG_API::IQueryInterface2
		{
		public:
			static QueryImpl* GetSingleton()
			{
				static QueryImpl singleton;
				return &singleton;
			}

			std::uint32_t Version() const override
			{
				return SLIFNG_API::kQueryVersion;
			}

			bool HasTarget(RE::Actor* a_actor, const char* a_target) const override
			{
				return Query::HasTarget(a_actor, a_target);
			}

			bool IsTracked(RE::Actor* a_actor) const override
			{
				return Query::IsTracked(a_actor);
			}

			std::uint32_t TrackedActorCount() const override
			{
				return static_cast<std::uint32_t>(Ledger::GetSingleton().TrackedActors().size());
			}

			float GetValue(RE::Actor* a_actor, const char* a_modName, const char* a_target,
				float a_default) const override
			{
				return Query::Value(a_actor, a_modName, a_target, a_default);
			}

			float GetMinValue(RE::Actor* a_actor, const char* a_modName, const char* a_target,
				float a_default) const override
			{
				return Query::MinValue(a_actor, a_modName, a_target, a_default);
			}

			float GetMaxValue(RE::Actor* a_actor, const char* a_modName, const char* a_target,
				float a_default) const override
			{
				return Query::MaxValue(a_actor, a_modName, a_target, a_default);
			}

			float GetApplied(RE::Actor* a_actor, const char* a_target) const override
			{
				return Query::Applied(a_actor, a_target);
			}

			float GetCombinedMorph(RE::Actor* a_actor, const char* a_slider) const override
			{
				return Query::CombinedMorph(a_actor, a_slider);
			}

			std::uint32_t GetCalculationType() const override
			{
				return static_cast<std::uint32_t>(Ledger::GetSingleton().GetMode());
			}

			bool IsIncrementalInflation() const override
			{
				return Ledger::GetSingleton().GetGradual();
			}
		};

		// The exchange struct GROWS as interfaces are added, so its size is a
		// lower bound, never an equality test: a consumer built against the v1
		// header dispatches a smaller struct and must still be served, and we
		// must never write a field past what it allocated.
		constexpr std::size_t kExchangeSizeV1 = sizeof(SLIFNG_API::IQueryInterface1*);

		void OnPluginMessage(SKSE::MessagingInterface::Message* a_msg)
		{
			if (!a_msg || a_msg->type != SLIFNG_API::InterfaceExchangeMessage::kMessageType ||
				!a_msg->data || a_msg->dataLen < kExchangeSizeV1) {
				return;
			}
			auto* exchange = static_cast<SLIFNG_API::InterfaceExchangeMessage*>(a_msg->data);
			exchange->query = QueryImpl::GetSingleton();
			const bool wantsV2 = a_msg->dataLen >= sizeof(SLIFNG_API::InterfaceExchangeMessage);
			if (wantsV2) {
				exchange->query2 = QueryImpl::GetSingleton();
			}
			logger::info("[API] query interface v{} handed to '{}' (consumer header: v{})",
				SLIFNG_API::kQueryVersion, a_msg->sender ? a_msg->sender : "<unnamed plugin>",
				wantsV2 ? 2 : 1);
		}
	}

	void Install()
	{
		// Sender nullptr = listen to dispatches from EVERY plugin; the message
		// type filters ours. SKSE's own lifecycle messages come from "SKSE"
		// with small integer types, so they fall through harmlessly.
		if (SKSE::GetMessagingInterface()->RegisterListener(nullptr, OnPluginMessage)) {
			logger::info("[API] inter-plugin query listener registered (dispatch "
						 "InterfaceExchangeMessage to \"SLIFNG\")");
		} else {
			logger::error("[API] could not register the inter-plugin listener");
		}
	}
}
