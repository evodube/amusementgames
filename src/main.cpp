#include "Forms.h"

namespace
{
	constexpr auto kName = "AmusementGames";

	void OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			ag::log::info("game data ready");
			ag::Forms::AddWorkshopMenu();
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

	ag::log::info("loaded; waiting for game data");
	return true;
}
