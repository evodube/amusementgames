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
		swingOn = ReadFloat(L"WhacACommie", L"bAgilitySwing", 1.0f) != 0.0f;
		swingPerAgility = std::clamp(ReadFloat(L"WhacACommie", L"fSwingPerAgility", 3.0f), 0.0f, 50.0f);
		swingMost = std::clamp(ReadFloat(L"WhacACommie", L"fSwingMost", 2.0f), 1.0f, 5.0f);
		settlersOn = ReadFloat(L"Settlers", L"bSettlersPlay", 1.0f) != 0.0f;
		settlersGapShortest = std::clamp(ReadFloat(L"Settlers", L"fGapShortest", 45.0f), 5.0f, 1800.0f);
		settlersGapLongest = std::clamp(ReadFloat(L"Settlers", L"fGapLongest", 120.0f), settlersGapShortest, 3600.0f);
	}

	void Settings::Save() const
	{
		WriteFloat(L"WhacACommie", L"bAgilitySwing", swingOn ? 1.0f : 0.0f);
		WriteFloat(L"WhacACommie", L"fSwingPerAgility", swingPerAgility);
		WriteFloat(L"WhacACommie", L"fSwingMost", swingMost);
		WriteFloat(L"Settlers", L"bSettlersPlay", settlersOn ? 1.0f : 0.0f);
		WriteFloat(L"Settlers", L"fGapShortest", settlersGapShortest);
		WriteFloat(L"Settlers", L"fGapLongest", settlersGapLongest);
	}

	namespace SettingsMenu
	{
		namespace
		{
			using namespace ImGuiMCP;

			const RE::ActorValueInfo* AV(const char* a_editorID)
			{
				return RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>(a_editorID);
			}

			void Saved(bool a_changed)
			{
				if (a_changed) {
					Settings::Get().Save();
				}
			}

			void __stdcall DrawWhac()
			{
				auto& s = Settings::Get();
				TextWrapped("Your swing with the Commie Whacker gets quicker the more Agility you have (the game's "
				            "WeaponSpeedMult, raised while you play and put back after).");
				Spacing();
				Saved(Checkbox("Agility quickens your swing", &s.swingOn));
				SetNextItemWidth(260.0f);
				SliderFloat("Quicker per point of Agility", &s.swingPerAgility, 0.0f, 50.0f, "%.1f %%");
				Saved(IsItemDeactivatedAfterEdit());
				SetNextItemWidth(260.0f);
				SliderFloat("At most", &s.swingMost, 1.0f, 5.0f, "%.2fx");
				Saved(IsItemDeactivatedAfterEdit());
				Spacing();
				Separator();
				// what it comes to for you, right now
				auto*       player = RE::PlayerCharacter::GetSingleton();
				const auto* agility = AV("Agility");
				const auto* speed = AV("WeaponSpeedMult");
				if (!player) {
					TextDisabled("(load a game to see yours)");
					return;
				}
				if (!agility || !speed) {
					TextColored(ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "Couldn't find the game's %s actor value: the swing can't be quickened.",
						!agility ? "Agility" : "WeaponSpeedMult");
					return;
				}
				const float agi = player->GetActorValue(*agility);
				const float mult = s.swingOn ? std::clamp(1.0f + s.swingPerAgility / 100.0f * (agi - 1.0f), 1.0f, s.swingMost) : 1.0f;
				const float now = player->GetActorValue(*speed);
				Text("Your Agility: %.0f", agi);
				Text("Your swing in Whac-a-Commie: %.2fx as quick", mult);
				Text("WeaponSpeedMult right now: %.2f%s", now, now <= 0.0f ? " (0: the game's normal, as quick as made)" : "");
				TextDisabled("While you play, WeaponSpeedMult shows the quickening (e.g. 1.27 at Agility 10, 3%% a point). "
				             "Watch it here with the game paused mid-swing to see it's on; set 'Quicker per point' high (20%%+) "
				             "to see the difference clearly.");
			}

			void __stdcall DrawSettlers()
			{
				auto& s = Settings::Get();
				TextWrapped("Now and then, while you're about, a settler of the settlement walks over to a free Bandit "
				            "Roundup and plays a round.");
				Spacing();
				Saved(Checkbox("Settlers play the games", &s.settlersOn));
				SetNextItemWidth(260.0f);
				SliderFloat("Between their games, at least", &s.settlersGapShortest, 5.0f, 600.0f, "%.0f s");
				Saved(IsItemDeactivatedAfterEdit());
				SetNextItemWidth(260.0f);
				SliderFloat("...and at most", &s.settlersGapLongest, 5.0f, 1200.0f, "%.0f s");
				if (s.settlersGapLongest < s.settlersGapShortest) {
					s.settlersGapLongest = s.settlersGapShortest;
				}
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
			F4SEMenuFramework::AddSectionItem("Whac-a-Commie", &DrawWhac);
			F4SEMenuFramework::AddSectionItem("Settlers", &DrawSettlers);
			log::info("settings pages registered in F4SE Menu Framework");
		}
	}
}
