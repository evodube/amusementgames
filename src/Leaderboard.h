#pragma once

#include "sim/Arcade.h"

// The Prize Counter's high scores: each game's top 10 across every settlement, each player's best only, by name;
// the game's generic names (Settler, Trader, Provisioner, Scavenger) left off. Shown on the counter through
// AmusementGames.esl's globals (a score each, 0x848 on: game * 10 + place) and AmusementGamesQuest's ref aliases (the
// player at each, from 6 on, the same order), which AmusementGamesNative.TopAlias fills.
namespace ag::Leaderboard
{
	inline constexpr int           kPlaces = 10;
	inline constexpr std::uint32_t kFirstScoreID = 0x848;  // GLOB AmusementGamesTop<Game><NN>
	inline constexpr int           kFirstAlias = 6;        // AmusementGamesQuest's alias Top<Game><NN>

	// a game played to its end by someone (an actor's ref id), scoring this (on the game's thread)
	void Record(sim::GameId a_game, RE::TESFormID a_actor, int a_score);

	void Save(const F4SE::SerializationInterface* a_intfc);
	void Load(const F4SE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);
	void Revert();

	inline constexpr std::uint32_t kSaveType = 0x41475450;  // 'AGTP': the boards
	inline constexpr std::uint32_t kSaveVersion = 1;
}
