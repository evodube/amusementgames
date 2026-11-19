#pragma once

#include "sim/Arcade.h"

// The machines' own number displays: Nuka-World's digits (DLC04_Nukacade_Numbers, the large ones on the zapper and
// the gallery as Nuka-World has them) put at each machine's Score01-08 and HighScore01-08 nodes, showing the game's
// score (the last one played on that machine, or the one being played) and your best at that game. Each digit is
// set as Nuka-World's own script sets it: PlayAnimation("Num0" + digit) on its behaviour graph.
//
// Boards go up for the machines near you and come down when you're away, and before every save (so no digits are
// saved into the cell; any left over from before are cleared off a machine when its board goes up).
namespace ag::Scoreboards
{
	// every frame (the game's thread)
	void Tick(double a_dt);

	// all boards down (a save, a load)
	void Clear();

	// our machines of a game near you (their boards up)
	std::vector<RE::ObjectRefHandle> Machines(sim::GameId a_game);
}
