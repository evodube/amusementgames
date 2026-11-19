#include "Arcade.h"
#include "Forms.h"
#include "Hook.h"
#include "Hud.h"
#include "MenuWatch.h"
#include "Scoreboard.h"

namespace
{
	constexpr auto kName = "AmusementGames";

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kPostPostLoad:
			ag::menus::Watch();
			ag::Hud::Install();  // F4SE Menu Framework is loaded by now, if it's installed
			break;
		case F4SE::MessagingInterface::kGameDataReady:
			ag::log::info("game data ready");
			ag::Forms::AddWorkshopMenu();
			ag::menus::Watch();
			ag::Hook::Install();
			ag::Arcade::Get().Listen();
			break;
		case F4SE::MessagingInterface::kPreLoadGame:
			ag::Arcade::Get().Abort("a save loading");
			ag::Scoreboards::Clear();
			break;
		case F4SE::MessagingInterface::kPreSaveGame:
			ag::Scoreboards::Clear();  // (no digits saved into the cells: they go back up after)
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			ag::Hook::Install();  // no-op if already hooked
			break;
		default:
			break;
		}
	}
}

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = kName;
	a_info->version = 1;

	if (a_f4se->IsEditor()) {
		return false;
	}
	return a_f4se->RuntimeVersion() >= F4SE::RUNTIME_1_10_163;
}

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se, { .log = true, .logName = kName, .logRotate = 3, .trampoline = false });

	ag::log::info("AmusementGames loading on runtime {} ({})", a_f4se->RuntimeVersion().string(),
		REX::FModule::IsRuntimeOG() ? "OG" : REX::FModule::IsRuntimeNG() ? "NG" : "AE");

	auto* messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		ag::log::error("could not register F4SE message listener");
		return false;
	}

	if (auto* serialization = F4SE::GetSerializationInterface()) {
		serialization->SetUniqueID(0x4147414D);  // 'AGAM'
		serialization->SetSaveCallback(ag::Arcade::Save);
		serialization->SetLoadCallback(ag::Arcade::Load);
		serialization->SetRevertCallback(ag::Arcade::Revert);
	} else {
		ag::log::warn("no F4SE serialization: best scores won't be saved");
	}

	ag::log::info("loaded; waiting for game data");
	return true;
}
