#pragma once

// Amusement Games' settings: in F4SE Menu Framework's menu (Amusement Games > Whac-a-Commie, Settlers), kept in
// Data\F4SE\Plugins\AmusementGames_user.ini.
namespace ag
{
	struct Settings
	{
		static Settings& Get()
		{
			static Settings s;
			return s;
		}

		// settlers playing (Bandit Roundup)
		bool  settlersOn{ true };
		float settlersGapShortest{ 45.0f };  // seconds between their games...
		float settlersGapLongest{ 120.0f };  // ...to this
		float settlersDuel{ 35.0f };         // % of Bandit Roundup games two settlers play each other

		void Load();
		void Save() const;
	};

	namespace SettingsMenu
	{
		// the pages in F4SE Menu Framework, if it's installed (kPostPostLoad)
		void Install();
	}
}
