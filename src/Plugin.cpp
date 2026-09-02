#include "API.h"
#include "Events.h"
#include "SettingsIni.hpp"
#include "Main.hpp"
#include "DataHandler.hpp"
#include "Initialization.hpp"
#include "Serialization.hpp"
#include "Papyrus.hpp"

#include "API/DDO-API.h"
#include "API/DF-API.h"

#include "Utils/ModUtils.hpp"
#include "Utils/MiscUtils.hpp"

static inline bool postLoadEventsLoaded = false;

static void PostLoadEvents() {
	if (postLoadEventsLoaded) return;
	postLoadEventsLoaded = true;

	if (DismemberingFrameworkAPI::LoadAPI()) {
		ModData::dismemberingFrameworkEnabled = true;
		logger::info("Successfully registered Dismembering Framework API.");
	}
	if (DeathDropOverhaulAPI::LoadAPI()) {
		ModData::deathDropOverhaulEnabled = true;
		logger::info("Successfully registered Death Drop Overhaul API.");
	}

	Events::ModEventSink::LoadEventsPostLoad();
};

static void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	auto postLoadEventsAlternate = []() {
		std::jthread([]() {
			while (!postLoadEventsLoaded) {
				static std::atomic_bool taskRunning = false;
				if (!taskRunning.exchange(true)) {
					SKSE::GetTaskInterface()->AddTask([]() {
						auto player = RE::PlayerCharacter::GetSingleton();
						if (player && player->Is3DLoaded() && player->GetParentCell() && player->GetParentCell()->IsAttached()) {
							PostLoadEvents();
						}
						taskRunning = false;
					});
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}).detach();
	};

	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostLoad:

		if (!SKSE::GetMessagingInterface()->RegisterListener(NULL, [](SKSE::MessagingInterface::Message* message) {
			switch (message->type) {
			case NGD_API_TYPE_KEY:
				message->dataLen = sizeof(NGDecapitationsAPI::NGDecapitationsAPI*);
				*(NGDecapitationsAPI::NGDecapitationsAPI**)message->data = NGDecapitationsAPI::g_API;
				break;
			}
		})) REPORT_AND_FAIL("Unable to register API message listener.");
		else logger::info("Successfully registered API message listener.");
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		ModData::DataHandler::GetSingleton()->LoadData();
		ModData::Serialization::RegisterSerializationCallbacks();
		Events::ModEventSink::LoadEvents();
		Events::MainEvent::InstallHooks();
		if (!NGDecapitationsAPI::g_API) NGDecapitationsAPI::g_API = new NGDecapitationsAPI::NGDecapitationsAPI;
		postLoadEventsAlternate();
		break;

	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		PostLoadEvents();
		break;
	}
}

static void InitializeLog(std::string_view pluginName, spdlog::level::level_enum a_level = spdlog::level::info)
{
	auto path = logger::log_directory();
	if (!path) REPORT_AND_FAIL("Failed to find standard logging directory.");

	*path /= std::format("{}.log", pluginName);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	const auto level = a_level;

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	if (level == spdlog::level::trace) spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
	else spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	const auto plugin{ SKSE::PluginDeclaration::GetSingleton() };
	const auto name{ plugin->GetName() };
	const auto version{ plugin->GetVersion() };

	SKSE::Init(a_skse);

	if (!SettingsIni::ReadSettings()) {
		InitializeLog(name, spdlog::level::info);
		logger::warn("Failed to load settings file.");
	} else {
		if (SettingsIni::iVerboseMode <= 0) {
			InitializeLog(name, spdlog::level::err);
		} else if (SettingsIni::iVerboseMode >= 2) {
			InitializeLog(name, spdlog::level::trace);
		} else {
			InitializeLog(name, spdlog::level::info);
		}
	}

	logger::info("{} v{} by Seb263 : Loaded - Game version : {}", name, version.string("."), REL::Module::get().version().string("."));

	auto g_message = SKSE::GetMessagingInterface();
	if (!g_message) REPORT_AND_FAIL("Messaging Interface not found.");
	else if (!g_message->RegisterListener(MessageHandler)) REPORT_AND_FAIL("Failed to register MessageHandler listener.");
	else logger::info("Successfully registered MessageHandler listener.");

	auto g_papyrus = SKSE::GetPapyrusInterface();
	if (!g_papyrus) REPORT_AND_FAIL("Papyrus Interface not found.");
	else if (!g_papyrus->Register(Papyrus::BindPapyrusFunctions)) REPORT_AND_FAIL("Failed to register Papyrus functions.");
	else logger::info("Successfully registered Papyrus functions.");

	return true;
}
