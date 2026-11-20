#pragma once

// Settlers reacting at the games, with the game's own idles (AmusementGamesNative.NpcReact plays them a moment after
// their game, once the gun's away): a cheer (the base game's crowd cheers, Nuka-World's wild ones), a clap, a nod, a
// head shake.
namespace ag::Reactions
{
	enum class Kind
	{
		kCheer,
		kClap,
		kNod,
		kShake
	};

	// a settler's own game over: how it went against their best there before (0: none yet) and their place on its high
	// scores (-1: not on them)
	Kind Judge(int a_score, int a_bestBefore, int a_place);

	// someone reacting, a_delay seconds from now
	void Play(RE::TESFormID a_actor, Kind a_kind, double a_delay, std::mt19937& a_rng);
}
