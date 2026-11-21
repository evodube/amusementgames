#pragma once

// Amusement Games' settings: in F4SE Menu Framework's menu (Amusement Games > Settlers, Income), kept in
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
		float settlersSkill{ 0.0f };         // added to every settler's skill at the games: -50 to +50 (hundredths)
		float settlersFrom{ 8.0f };          // the hours (game time) settlers start games of their own: from...
		float settlersTo{ 22.0f };           // ...until (past midnight if earlier than from; the same: all day)

		// a machine's takings, paid into its settlement's workshop: a day, for each settler there
		float incomeCaps{ 1.0f };
		float incomeTickets{ 2.0f };

		// the tickets you win playing: each game's own rate times this
		float playTickets{ 2.0f };

		void Load();
		void Save() const;
	};

	namespace SettingsMenu
	{
		// the pages in F4SE Menu Framework, if it's installed (kPostPostLoad)
		void Install();
	}
}
