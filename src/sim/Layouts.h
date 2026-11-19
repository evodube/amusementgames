#pragma once

// Where things are on each machine, in its model's space (x right, y into the machine, z up from the floor; game
// units). Measured off Nuka-World's models (docs/HANDOFF.md says which and how); the shapes are the ones the balls
// run on and the spots the heads, rockets and targets go.

#include "sim/Ball3D.h"

#include <vector>

namespace sim::layout
{
	// ---------------------------------------------------------------- Atomic Rollers
	// DLC04_Nukacade_SkeeBall01.nif, measured off its Havok collision: a lane flat then rising, a hump, a gutter
	// across the machine (BallHole00, no points), then the board climbing at 1 in 2 with its holes up the middle
	// (BallHole01-05: 100-500) and in the top corners (BallHole06/07: 700), and a trough over the top (no points)
	struct RollersHole
	{
		double x{ 0.0 };
		double y{ 0.0 };      // where it's let into the board
		double radius{ 7.5 };
		int    points{ 0 };
	};

	struct RollersLayout
	{
		int    balls{ 9 };
		double ballRadius{ 4.0 };              // DLC04_Nukacade_Ball01: 8 across
		Vec3   release{ 0.0, -150.0, 42.5 };   // the ball in your hand: the lane's near end
		double aimHalfWidth{ 30.0 };           // how far either side of the lane's middle you can aim
		double laneHalfWidth{ 38.4 };
		double boardHalfWidth{ 60.2 };
		double laneFrontY{ -160.0 }, laneFrontZ{ 38.4 };
		double laneBendY{ -80.0 }, laneBendZ{ 37.5 };   // flat to here, then rising
		double rampStartY{ 8.6 }, rampStartZ{ 46.0 };   // the hump's ramp...
		double rampEndY{ 32.2 }, rampEndZ{ 57.8 };
		double humpEndY{ 48.0 };                        // ...its flat top
		double gutterZ{ 34.0 };                         // the gutter's floor
		double boardStartY{ 69.0 }, boardStartZ{ 58.3 };
		double boardEndY{ 162.0 }, boardEndZ{ 104.8 };
		double laneSlope{ 0.0 };
		std::vector<RollersHole> holes{ { 0.0, 79.5, 7.5, 100 }, { 0.0, 97.5, 7.5, 200 }, { 0.0, 114.0, 7.5, 300 }, { 0.0, 132.0, 7.5, 400 },
			{ 0.0, 153.0, 7.5, 500 }, { -38.5, 153.0, 7.5, 700 }, { 38.5, 153.0, 7.5, 700 } };
		double minSpeed{ 150.0 };
		double maxSpeed{ 480.0 };
		double chargeSeconds{ 1.2 };
	};

	// ---------------------------------------------------------------- Hoop Shot
	// DLC04_Nukacade_BasketBall01.nif: the hoop (BallHole01 under it), the backboard, the cage's sides and the return
	// ramp sloping back down under the front shelf
	inline constexpr int kHoopBasket = 1;
	inline constexpr int kHoopReturn = 2;

	struct HoopLayout
	{
		double seconds{ 45.0 };
		double hurrySeconds{ 10.0 };
		int    ballsInPlay{ 5 };
		double ballRadius{ 12.0 };          // DLC04_Basketball01: 24 across
		Vec3   rim{ 0.0, 125.5, 154.6 };    // the hoop's centre
		double rimRadius{ 19.5 };
		double rimTube{ 0.7 };
		double boardY{ 157.7 };             // the backboard's face
		double boardZ{ 184.0 };
		double boardHalfWidth{ 40.0 };
		double boardHalfHeight{ 38.0 };
		double rampFrontY{ -90.0 }, rampFrontZ{ 50.2 };   // the return ramp: up from under the shelf...
		double rampBackY{ 157.0 }, rampBackZ{ 92.9 };     // ...to the back
		double shelfFrontY{ -128.0 }, shelfBackY{ -90.0 }, shelfZ{ 57.9 };
		double sideX{ 56.0 };               // the cage's sides...
		double sideTopZ{ 142.0 };           // ...up to here (open above)
		double handForward{ 30.0 };         // the ball in your hand, in front of your eyes...
		double handDrop{ 16.0 };            // ...and down a bit
		double arcLift{ 0.32 };             // the throw goes up this much more than you look
		double minSpeed{ 260.0 };
		double maxSpeed{ 720.0 };
		double chargeSeconds{ 0.7 };
		double reloadSeconds{ 0.35 };
		double ballLife{ 6.0 };
		int    points{ 100 };               // a basket (Nuka-World's)
	};

	// ---------------------------------------------------------------- Whac-a-Commie
	// DLC04_Nukacade_WhackaCommie01.nif: the seven holes (mole01-07). The head (DLC04_Nukacade_Commie01.nif) has its
	// origin at its top: at the hole's node it's out of sight, raised headUp it's all the way out
	struct WhacLayout
	{
		double            seconds{ 30.0 };
		std::vector<Vec3> holes{ { -12.07, -20.51, 63.19 }, { 12.02, -20.51, 63.19 }, { -24.15, -0.14, 67.4 }, { -0.03, -0.14, 67.46 },
			{ 23.92, -0.14, 67.4 }, { -12.07, 19.95, 71.47 }, { 12.02, 19.95, 71.47 } };
		double            headDown{ -1.0 };    // a head's origin below its hole's node, down...
		double            headUp{ 17.0 };      // ...and above it, up
		double            headYaw{ 0.0 };      // turned to face you
		double            hitRadius{ 8.0 };
		double            riseSeconds{ 0.18 };
		double            hitSeconds{ 0.25 };
		double            upLongest{ 1.3 };    // how long a head stays up at the start...
		double            upShortest{ 0.6 };   // ...and at the end
		double            gapLongest{ 0.9 };   // between pop-ups at the start...
		double            gapShortest{ 0.35 }; // ...and at the end
		int               mostUp{ 3 };         // most at once by the end
		double            swingSeconds{ 0.18 };
		int               points{ 100 };       // a whack (Nuka-World's)
	};

	// ---------------------------------------------------------------- Nuka-Zapper Race
	// DLC04_Nukacade_NukaZapperRace.nif: six lanes, each a target on the back wall (target01-06) and a rocket
	// (rocket01-06) going up the board above it; the counter's at 64
	struct ZapperLane
	{
		Vec3 gun;          // where the stream comes from
		Vec3 target;       // the target's centre (facing you)
		Vec3 rocketBottom; // the rocket at the start...
		Vec3 rocketTop;    // ...and at the finish
	};

	inline ZapperLane ZapLane(double a_x)
	{
		return { { a_x, 20.0, 76.0 }, { a_x, 160.0, 79.0 }, { a_x, 161.1, 119.9 }, { a_x, 161.1, 175.0 } };  // (topping out under the scoreboard)
	}

	struct ZapperLayout
	{
		std::vector<ZapperLane> lanes{ ZapLane(-182.2), ZapLane(-109.5), ZapLane(-37.5), ZapLane(34.1), ZapLane(106.4), ZapLane(178.2) };
		double targetRadius{ 9.0 };
		double secondsToWin{ 10.0 };  // on target all the way
		double dropEvery{ 0.04 };
		double dropSeconds{ 0.25 };
		int    maxDrops{ 24 };
		std::vector<int> placePoints{ 1000, 600, 400, 200, 100, 100 };
	};

	// ---------------------------------------------------------------- Bandit Roundup
	// DLC04_Nukacade_ShootingGallery01.nif: two tracks, the upper running left to right (SpawnLeftUp01), the lower right
	// to left (SpawnRightDown01)
	struct GalleryRow
	{
		double y{ 300.0 };    // how far back
		double z{ 150.0 };    // the targets' feet
		double speed{ 60.0 }; // across: + to the right, - to the left
		int    count{ 6 };
	};

	struct GalleryKind  // the five cut-outs, in the plugin's model order: duck, bunny, pig, bottle, star
	{
		int    points{ 100 };
		double radius{ 10.0 };  // how big a target it is
		double height{ 10.0 };  // its middle above its feet
		double weight{ 1.0 };   // how often it comes up
	};

	struct GalleryLayout
	{
		double                   seconds{ 60.0 };
		double                   left{ -224.0 };
		double                   right{ 232.0 };
		std::vector<GalleryRow>  rows{ { 418.3, 180.2, 75.0, 6 }, { 375.0, 111.2, -95.0, 6 } };
		std::vector<GalleryKind> kinds{ { 100, 14.0, 14.0, 4.0 }, { 150, 15.0, 16.0, 3.0 }, { 200, 13.0, 11.0, 2.0 }, { 250, 8.0, 22.0, 1.2 },
			{ 300, 12.0, 11.0, 0.6 } };
		double                   flipSeconds{ 0.15 };
		double                   shotSeconds{ 0.3 };
	};

	const RollersLayout& Rollers();
	const HoopLayout&    Hoop();
	const WhacLayout&    Whac();
	const ZapperLayout&  Zapper();
	const GalleryLayout& Gallery();

	// the machines' shapes for the ball games
	void BuildRollers(World& a_world);
	void BuildHoop(World& a_world);
}
