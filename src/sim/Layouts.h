#pragma once

// Where things are on each machine, in its model's space (x right, y into the machine, z up from the floor; game
// units). Measured off Nuka-World's models (docs/HANDOFF.md says which and how); the shapes are the ones the balls
// run on and the spots the heads, rockets and targets go.

#include "sim/Ball3D.h"

#include <vector>

namespace sim::layout
{
	// ---------------------------------------------------------------- Atomic Rollers
	struct RollersLayout
	{
		int    balls{ 9 };
		double ballRadius{ 4.0 };            // DLC04_Nukacade_Ball01: 8 across
		Vec3   release{ 0.0, -140.0, 82.0 };  // the ball in your hand: the lane's near end
		double aimHalfWidth{ 16.0 };          // how far either side of the lane's middle you can aim
		double laneHalfWidth{ 22.0 };
		double boardHalfWidth{ 50.0 };        // the board is wider than the lane
		double laneFrontZ{ 79.0 };            // the lane's surface at its near end...
		double laneBackZ{ 92.0 };             // ...and where it meets the ramp
		double laneFrontY{ -150.0 };
		double rampStartY{ 55.0 };
		double rampEndY{ 72.0 };
		double rampTopZ{ 101.0 };             // the hump's lip: the ball leaves it into the air
		double laneSlope{ 0.065 };            // the lane's rise per unit along it
		double boardBottomY{ 95.0 };          // the scoring board: from here...
		double boardBottomZ{ 96.0 };
		double boardTopY{ 168.0 };            // ...up and back to here
		double boardTopZ{ 168.0 };
		double ringCentreU{ 0.45 };           // the rings' centre, 0..1 up the board
		std::vector<double> ringRadii{ 44.5, 35.0, 25.5, 16.0, 6.0 };  // a ball (8 across) fits between any two  // the 10, 20, 30, 40 and 50 rings' rims
		std::vector<int>    ringPoints{ 10, 20, 30, 40, 50 };
		double cornerHoleU{ 0.92 };            // the 100 holes, high in the corners
		double cornerHoleX{ 40.0 };
		double cornerHoleRadius{ 6.0 };
		double minSpeed{ 180.0 };
		double maxSpeed{ 560.0 };
		double chargeSeconds{ 1.2 };
	};

	// ---------------------------------------------------------------- Hoop Shot
	inline constexpr int kHoopBasket = 1;
	inline constexpr int kHoopReturn = 2;

	struct HoopLayout
	{
		double seconds{ 45.0 };
		double hurrySeconds{ 10.0 };
		int    ballsInPlay{ 5 };
		double ballRadius{ 12.0 };           // DLC04_Basketball01: 24 across
		Vec3   rim{ 0.0, 128.0, 178.0 };  // the hoop's centre
		double rimRadius{ 20.0 };
		double rimTube{ 0.8 };
		double boardY{ 142.0 };           // the backboard's face
		double boardZ{ 192.0 };
		double boardHalfWidth{ 38.0 };
		double boardHalfHeight{ 26.0 };
		double floorFrontY{ -110.0 };     // the return ramp: from under the hoop down to the front
		double floorFrontZ{ 70.0 };
		double floorBackZ{ 100.0 };
		double sideX{ 62.0 };             // the cage's sides
		double cageTopZ{ 230.0 };
		double handForward{ 30.0 };       // the ball in your hand, in front of your eyes...
		double handDrop{ 14.0 };          // ...and down a bit
		double arcLift{ 0.32 };           // the throw goes up this much more than you look
		double minSpeed{ 260.0 };
		double maxSpeed{ 720.0 };
		double chargeSeconds{ 0.7 };
		double reloadSeconds{ 0.35 };
		double ballLife{ 5.0 };
	};

	// ---------------------------------------------------------------- Whac-a-Commie
	struct WhacLayout
	{
		double            seconds{ 30.0 };
		std::vector<Vec3> holes{ { -30.0, 8.0, 0.0 }, { -15.0, -4.0, 0.0 }, { 0.0, 8.0, 0.0 }, { 15.0, -4.0, 0.0 }, { 30.0, 8.0, 0.0 } };
		double            headDownZ{ 80.0 };   // a head's origin hidden in its hole...
		double            headUpZ{ 98.0 };     // ...and popped up
		double            headYaw{ 0.0 };      // turned to face you
		double            hitRadius{ 9.0 };
		double            riseSeconds{ 0.18 };
		double            hitSeconds{ 0.25 };
		double            upLongest{ 1.3 };    // how long a head stays up at the start...
		double            upShortest{ 0.6 };   // ...and at the end
		double            gapLongest{ 0.9 };   // between pop-ups at the start...
		double            gapShortest{ 0.35 }; // ...and at the end
		int               mostUp{ 3 };         // most at once by the end
		double            swingSeconds{ 0.18 };
	};

	// ---------------------------------------------------------------- Nuka-Zapper Race
	struct ZapperLane
	{
		Vec3 gun;          // where the stream comes from
		Vec3 target;       // the target's centre (facing you)
		Vec3 rocketBottom; // the rocket at the start...
		Vec3 rocketTop;    // ...and at the finish
	};

	struct ZapperLayout
	{
		std::vector<ZapperLane> lanes{
			{ { -195.0, -30.0, 110.0 }, { -195.0, 120.0, 150.0 }, { -195.0, 150.0, 170.0 }, { -195.0, 150.0, 275.0 } },
			{ { -65.0, -30.0, 110.0 }, { -65.0, 120.0, 150.0 }, { -65.0, 150.0, 170.0 }, { -65.0, 150.0, 275.0 } },
			{ { 65.0, -30.0, 110.0 }, { 65.0, 120.0, 150.0 }, { 65.0, 150.0, 170.0 }, { 65.0, 150.0, 275.0 } },
			{ { 195.0, -30.0, 110.0 }, { 195.0, 120.0, 150.0 }, { 195.0, 150.0, 170.0 }, { 195.0, 150.0, 275.0 } },
		};
		double targetRadius{ 6.0 };
		double secondsToWin{ 10.0 };  // on target all the way
		double dropEvery{ 0.04 };
		double dropSeconds{ 0.25 };
		int    maxDrops{ 24 };
	};

	// ---------------------------------------------------------------- Bandit Roundup
	struct GalleryRow
	{
		double y{ 300.0 };    // how far back
		double z{ 150.0 };    // the targets' hinge height
		double speed{ 60.0 }; // across: + to the right, - to the left
		int    count{ 6 };
	};

	struct GalleryKind  // the five cut-outs, in the plugin's model order: duck, bunny, pig, bottle, star
	{
		int    points{ 10 };
		double radius{ 10.0 };  // how big a target it is
		double height{ 10.0 };  // its middle above the hinge
		double weight{ 1.0 };   // how often it comes up
	};

	struct GalleryLayout
	{
		double                   seconds{ 40.0 };
		double                   left{ -230.0 };
		double                   right{ 230.0 };
		std::vector<GalleryRow>  rows{ { 330.0, 175.0, 70.0, 6 }, { 300.0, 135.0, -95.0, 5 }, { 270.0, 95.0, 55.0, 7 } };
		std::vector<GalleryKind> kinds{ { 10, 11.0, 12.0, 4.0 }, { 20, 10.0, 12.0, 3.0 }, { 30, 10.0, 11.0, 2.0 }, { 40, 6.0, 9.0, 1.2 },
			{ 50, 7.0, 10.0, 0.6 } };
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
