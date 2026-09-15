#include <REL/Module.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include "Ledger.h"
#include "Papyrus.h"
#include "Skee.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace
{
	void InitializeLogging()
	{
		auto path = log_directory();
		if (!path) {
			report_and_fail("Unable to lookup SKSE logs directory.");
		}
		*path /= PluginDeclaration::GetSingleton()->GetName();
		*path += L".log";

		std::shared_ptr<spdlog::logger> logger;
		if (IsDebuggerPresent()) {
			logger = std::make_shared<spdlog::logger>(
				"Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
		} else {
			logger = std::make_shared<spdlog::logger>(
				"Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		}
		logger->set_level(spdlog::level::info);
		// Flush on warn, not info: the per-call diagnostics are info level and a
		// synchronous flush each one would land on the Papyrus caller's thread.
		logger->flush_on(spdlog::level::warn);

		spdlog::set_default_logger(std::move(logger));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
	}

	// One revert callback for the whole plugin: the ledger AND every
	// session-scoped cache must die with the outgoing save.
	void OnRevert(SKSE::SerializationInterface* a_intfc)
	{
		SLIFNG::Ledger::OnRevert(a_intfc);
		SLIFNG::Skee::OnRevert();
	}

	void InitializeSerialization()
	{
		auto* serde = GetSerializationInterface();
		if (!serde) {
			report_and_fail("Failed to obtain serialization interface.");
		}
		serde->SetUniqueID(_byteswap_ulong('SLIF'));
		serde->SetSaveCallback(SLIFNG::Ledger::OnGameSaved);
		serde->SetLoadCallback(SLIFNG::Ledger::OnGameLoaded);
		serde->SetRevertCallback(OnRevert);
		log::info("Cosave serialization initialized.");
	}

	void InitializePapyrus()
	{
		auto* papyrus = GetPapyrusInterface();
		if (!papyrus || !papyrus->Register(SLIFNG::Papyrus::RegisterFuncs)) {
			report_and_fail("Failed to register Papyrus bindings.");
		}
		log::info("Papyrus functions bound.");
	}

	void OnMessage(MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case MessagingInterface::kPostPostLoad:
			// skee publishes its interface map by now.
			SLIFNG::Skee::Initialize();
			break;
		case MessagingInterface::kDataLoaded:
			// Retry actors that had no 3D when their values were restored.
			SLIFNG::Skee::RegisterLoadHook();
			break;
		case MessagingInterface::kPostLoadGame:
			// Ledger is source of truth: recompute + re-apply heals any desync.
			SLIFNG::Skee::ReapplyAll();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const LoadInterface* skse)
{
	InitializeLogging();

	const auto* plugin = PluginDeclaration::GetSingleton();
	log::info("{} v{} is loading...", plugin->GetName(), plugin->GetVersion());
	log::info("Runtime version: {}", REL::Module::get().version().string());

	Init(skse);
	InitializeSerialization();
	InitializePapyrus();
	if (!GetMessagingInterface()->RegisterListener(OnMessage)) {
		report_and_fail("Failed to register messaging listener.");
	}

	log::info("{} loaded.", plugin->GetName());
	return true;
}
