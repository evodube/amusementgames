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
	}

	void Settings::Save() const
	{
		WriteFloat(L"Settlers", L"bSettlersPlay", settlersOn ? 1.0f : 0.0f);
		WriteFloat(L"Settlers", L"fGapShortest", settlersGapShortest);
		WriteFloat(L"Settlers", L"fGapLongest", settlersGapLongest);
		WriteFloat(L"Settlers", L"fDuelPercent", settlersDuel);
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
			log::info("settings pages registered in F4SE Menu Framework");
		}
	}
}
