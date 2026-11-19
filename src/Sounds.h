#pragma once

#include "sim/Arcade.h"

// The games' sounds: Nuka-World's own Nuka-Cade sound descriptors, played at the machine (Papyrus Sound.Play), and
// each game's music loop (stopped again with Sound.StopInstance).
namespace ag::Sounds
{
	// a game's cue, at the machine
	void Play(sim::GameId a_game, sim::Cue a_cue, RE::TESObjectREFR* a_at, double a_gain);

	// the game's music: its normal loop, or (a_fast) the last seconds' (with its fast start); a_on false stops it
	void Music(sim::GameId a_game, RE::TESObjectREFR* a_at, bool a_on, bool a_fast = false);

	// the token stand's / ticket sounds
	void Tickets(RE::TESObjectREFR* a_at);
}
