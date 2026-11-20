#include "Settings.h"

#include "F4SEMenuFramework.h"

namespace ag
{
	namespace
	{
		constexpr auto kIni = L"Data\\F4SE\\Plugins\\AmusementGames_user.ini";

		float ReadFloat(const wchar_t* a_section, const wchar_t* a_key, float a_default)
		{
			wchar_t buf[64]{};
			::GetPrivateProfileStringW(a_section, a_key, L"", buf, 64, kIni);
			if (!buf[0]) {
				return a_default;
			}
			try {
				return std::stof(buf);
			} catch (...) {
				return a_default;
			}
		}

		void WriteFloat(const wchar_t* a_section, const wchar_t* a_key, float a_value)
		{
			::WritePrivateProfileStringW(a_section, a_key, std::format(L"{}", a_value).c_str(), kIni);
		}
	}

	void Settings::Load()
	{
		settlersOn = ReadFloat(L"Settlers", L"bSettlersPlay", 1.0f) != 0.0f;
		settlersGapShortest = std::clamp(ReadFloat(L"Settlers", L"fGapShortest", 45.0f), 5.0f, 1800.0f);
		settlersGapLongest = std::clamp(ReadFloat(L"Settlers", L"fGapLongest", 120.0f), settlersGapShortest, 3600.0f);
		settlersDuel = std::clamp(ReadFloat(L"Settlers", L"fDuelPercent", 35.0f), 0.0f, 100.0f);
		settlersSkill = std::clamp(ReadFloat(L"Settlers", L"fSkill", 0.0f), -50.0f, 50.0f);
		incomeCaps = std::clamp(ReadFloat(L"Income", L"fCapsPerSettler", 1.0f), 0.0f, 100.0f);
		incomeTickets = std::clamp(ReadFloat(L"Income", L"fTicketsPerSettler", 2.0f), 0.0f, 100.0f);
		playTickets = std::clamp(ReadFloat(L"Income", L"fPlayTicketsMult", 2.0f), 0.5f, 5.0f);
	}

	void Settings::Save() const
	{
		WriteFloat(L"Settlers", L"bSettlersPlay", settlersOn ? 1.0f : 0.0f);
		WriteFloat(L"Settlers", L"fGapShortest", settlersGapShortest);
		WriteFloat(L"Settlers", L"fGapLongest", settlersGapLongest);
		WriteFloat(L"Settlers", L"fDuelPercent", settlersDuel);
		WriteFloat(L"Settlers", L"fSkill", settlersSkill);
		WriteFloat(L"Income", L"fCapsPerSettler", incomeCaps);
		WriteFloat(L"Income", L"fTicketsPerSettler", incomeTickets);
		WriteFloat(L"Income", L"fPlayTicketsMult", playTickets);
	}

	namespace SettingsMenu
	{
		namespace
		{
			using namespace ImGuiMCP;

			void Saved(bool a_changed)
			{
				if (a_changed) {
					Settings::Get().Save();
				}
			}

			void __stdcall DrawSettlers()
			{
				auto& s = Settings::Get();
				TextWrapped("While you're about, settlers of the settlement walk over to the Bandit Roundups and play: one "
				            "settler to each machine, as many at once as there are machines (the one you're at left to you). "
				            "After a game a machine rests a while before the next settler comes to it.");
				Spacing();
				Saved(Checkbox("Settlers play the games", &s.settlersOn));
				SetNextItemWidth(260.0f);
				SliderFloat("A machine's rest between games, at least", &s.settlersGapShortest, 5.0f, 600.0f, "%.0f s");
				Saved(IsItemDeactivatedAfterEdit());
				SetNextItemWidth(260.0f);
				SliderFloat("...and at most", &s.settlersGapLongest, 5.0f, 1200.0f, "%.0f s");
				if (s.settlersGapLongest < s.settlersGapShortest) {
					s.settlersGapLongest = s.settlersGapShortest;
				}
				Saved(IsItemDeactivatedAfterEdit());
				SetNextItemWidth(260.0f);
				SliderFloat("Bandit Roundup: two settlers playing each other", &s.settlersDuel, 0.0f, 100.0f, "%.0f %% of games");
				Saved(IsItemDeactivatedAfterEdit());
				Spacing();
				TextWrapped("Each settler has a talent of their own, gets better the more they play a machine, and plays to "
				            "their stats (Perception for aiming, Agility for quick hands). This shifts everyone's skill.");
				SetNextItemWidth(260.0f);
				SliderFloat("Settlers' skill", &s.settlersSkill, -50.0f, 50.0f, "%+.0f");
				Saved(IsItemDeactivatedAfterEdit());
			}

			void __stdcall DrawIncome()
			{
				auto& s = Settings::Get();
				TextWrapped("Each game machine earns its settlement caps and Nuka-Cade tickets every day, for each settler "
				            "living there. They go into the settlement's workbench when you're next nearby (up to 30 days' "
				            "worth at once).");
				Spacing();
				SetNextItemWidth(260.0f);
				SliderFloat("Caps a day, per settler, per machine", &s.incomeCaps, 0.0f, 20.0f, "%.1f");
				Saved(IsItemDeactivatedAfterEdit());
				SetNextItemWidth(260.0f);
				SliderFloat("Tickets a day, per settler, per machine", &s.incomeTickets, 0.0f, 20.0f, "%.1f");
				Saved(IsItemDeactivatedAfterEdit());
				Spacing();
				TextWrapped("The tickets you win playing: each game's own rate (Nuka-World's) times this.");
				SetNextItemWidth(260.0f);
				SliderFloat("Tickets from playing", &s.playTickets, 0.5f, 5.0f, "%.1fx");
				Saved(IsItemDeactivatedAfterEdit());
			}
		}

		void Install()
		{
			static bool installed = false;
			Settings::Get().Load();
			if (installed || !F4SEMenuFramework::IsInstalled()) {
				return;
			}
			installed = true;
			F4SEMenuFramework::SetSection("Amusement Games");
			F4SEMenuFramework::AddSectionItem("Settlers", &DrawSettlers);
			F4SEMenuFramework::AddSectionItem("Income", &DrawIncome);
			log::info("settings pages registered in F4SE Menu Framework");
		}
	}
}
