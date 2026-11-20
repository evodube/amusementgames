#pragma once

#include "sim/Arcade.h"

// How good a settler is at a game (0 worst, 1 best), each time they step up to one: their own talent (rolled the first
// time they play anything, then theirs for good), their practice at that machine (the games they've played there),
// their form on the day, and the Settlers page's skill setting. Kept in the co-save.
namespace ag::SettlerSkill
{
	// their skill this game (counts the game as played: practice); a_rng for their talent and form
	double Roll(RE::TESFormID a_id, sim::GameId a_game, std::mt19937& a_rng);
	// a game of theirs over, this score: their best there before it (0: none yet), the best now kept
	int Scored(RE::TESFormID a_id, sim::GameId a_game, int a_score);

	void Save(const F4SE::SerializationInterface* a_intfc);
	void Load(const F4SE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);
	void Revert();

	inline constexpr std::uint32_t kSaveType = 0x4147534B;  // 'AGSK'
	inline constexpr std::uint32_t kSaveVersion = 2;  // (2: each one's best at each game too)
}
