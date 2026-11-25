#pragma once

// Our machines in the world as their models load and unload (the game's object-loaded event), so finding the ones
// near you is a look through a handful, not every object in the loaded cells: the scoreboards' look for machines ran
// every second through every object of the 25 or so loaded cells, machines built or not (lag reports). Reseeded with
// one full look after a save loads (anything the events missed). As Wasteland Wagers' Pieces.
namespace ag::machines
{
	void Listen();  // (at game data ready)
	void Reseed();  // (after a save loads, or a new game)
	bool Empty();   // none of ours loaded
	// ours loaded within a_range of a_pos (gone ones dropped)
	std::vector<RE::TESObjectREFR*> Near(const RE::NiPoint3& a_pos, float a_range);
}
