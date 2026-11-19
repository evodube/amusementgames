#pragma once

namespace ag::Hud
{
	// Registers the arcade HUD with F4SE Menu Framework, if it's installed. Safe to call more than once.
	void Install();

	// The terminal HUD is up (Menu Framework there, its fonts loaded), so results don't need the game's notifications.
	bool Showing();
}
