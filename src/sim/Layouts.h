#pragma once

#include "sim/Arcade.h"

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
	struct RollersCup
	{
		Vec3 at;        // its BallHole node
		int  points{ 0 };
	};

	struct RollersLayout
	{
		int    balls{ 9 };
		double ballRadius{ 4.0 };              // DLC04_Nukacade_Ball01: 8 across
		Vec3   release{ 0.0, -148.0, 42.6 };   // the ball in your hand: on the lane's near end
		double aimHalfWidth{ 32.0 };           // how far either side of the lane's middle you can aim
		double laneHalfWidth{ 38.4 };
		double laneFrontY{ -160.0 }, laneFrontZ{ 38.4 };  // the lane's surface: flat...
		double laneBendY{ -80.0 }, laneBendZ{ 37.5 };     // ...to here, then rising...
		double laneTopZ{ 46.0 };                          // ...to this at the hump (rampStartY)
		double rampStartY{ 8.6 };              // where the hump starts (what you aim at across the lane)
		Vec3   pocket{ 48.5, -126.0, 36.4 };   // the right-hand pocket where the balls come back (BallReturn01's channel)...
		double pocketStep{ 8.4 };              // ...each ball behind the one before
		std::vector<RollersCup> cups{ { { 0.0, 81.7, 38.0 }, 100 }, { { 0.0, 98.7, 46.5 }, 200 }, { { 0.0, 115.7, 55.0 }, 300 },
			{ { 0.0, 134.1, 64.2 }, 400 }, { { 0.0, 153.8, 74.0 }, 500 }, { { -38.5, 153.8, 74.0 }, 700 }, { { 38.5, 153.8, 74.0 }, 700 } };
		Vec3   gutter{ 0.0, 57.6, 38.0 };      // BallHole00: the gutter before the board
		Vec3   trough{ 0.0, 173.0, 30.0 };     // over the board's top
		double minSpeed{ 280.0 };
		double maxSpeed{ 680.0 };
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
		// the balls waiting just behind the front shelf, at the foot of the ramp the balls come back down: resting on the
		// floor you see there (the model's, flat at 47.9; its collision runs a few units above it), the base game's
		// basketball 12.5 round
		double restBallY{ -76.0 }, restBallZ{ 60.4 };
		double restBallSpacing{ 26.0 };     // across, side by side
		double sideX{ 56.0 };               // the cage's sides...
		double sideTopZ{ 142.0 };           // ...up to here (open above)
		double handForward{ 30.0 };         // the ball in your hand, in front of your eyes...
		double handDrop{ 20.0 };            // ...down below your sights (its top just in view at the bottom)...
		double handSide{ 14.0 };            // ...and to the right, out of your way
		double handWobble{ 3.0 };           // each throw: the ball's place in your hand off by about this...
		double aimWobble{ 0.02 };           // ...its line by about this (radians)...
		double paceWobble{ 0.025 };         // ...and its pace by about this much of it
		double arcLift{ 0.6 };              // the throw goes up this much more than you look (a lob, down into the hoop)
		double minSpeed{ 400.0 };
		double maxSpeed{ 640.0 };
		double chargeSeconds{ 0.7 };
		double reloadSeconds{ 0.35 };
		double ballLife{ 6.0 };
		int    points{ 100 };               // a basket (Nuka-World's)
		double bounce{ 0.385 };             // how much of its speed into a surface the ball keeps (the cage, board, rim)
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
		Vec3              malletRest{ -38.74, -36.02, 70.72 };  // the mallet in its holder (the whacker01 node)
		                                                    // (its handle's end here, its head toward the front-left corner)
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
		return { { a_x, 20.0, 76.0 }, { a_x, 160.0, 79.0 }, { a_x, 161.1, 119.9 }, { a_x, 161.1, 228.0 } };  // (topping out just under the cabinet's ceiling, at 254: the bottle's 46 tall)
	}

	struct ZapperLayout
	{
		std::vector<ZapperLane> lanes{ ZapLane(-182.2), ZapLane(-109.5), ZapLane(-37.5), ZapLane(34.1), ZapLane(106.4), ZapLane(178.2) };
		int    playerLane{ 3 };             // yours: the one nearest the machine's middle, a Thirst Zapper on the counter by it
		Vec3   gunRest{ 34.1, 0.0, 68.0 };  // where that Thirst Zapper lies (the counter's at 64): stand behind it to play
		double targetRadius{ 11.0 };        // DLC04_Nukacade_NukaZapperTarget01: 30 across
		int    hitsToWin{ 30 };             // hits up to the top, then the win (a race of 15 seconds or so)
		double shotSeconds{ 0.4 };          // your squirts, counted at most this often
		double aiShotShortest{ 0.3 };       // the other lanes' squirts: between these apart...
		double aiShotLongest{ 0.6 };
		double aiHitWorst{ 0.4 };           // ...each hitting with this chance (the worst player)...
		double aiHitBest{ 0.75 };           // ...to this (the best)
		double rocketRise{ 3.0 };           // how fast a rocket climbs to its height (a fraction a second)
		int    pointsPerHit{ 100 };         // (as Nuka-World's steps up)
		int    winPoints{ 1000 };           // a win's bonus (as Nuka-World's)
		double standY{ -80.0 };             // settlers racing stand this far out, behind their lane's gun
		std::vector<int> settlerLanes{ 2, 4, 1, 5, 0 };  // the lanes settlers take, in turn (all but yours, out from it)
		double gatherSeconds{ 10.0 };       // the race starts this long after the second settler's ready...
		double gatherLongest{ 180.0 };      // ...and is given up if two aren't ready in this long
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
		bool   bullseyes{ false };  // a bullseye dead centre is worth more here
	};

	struct GalleryKind  // the five cut-outs, in the plugin's model order: duck, bunny, pig, bottle, star
	{
		int    points{ 100 };
		double radius{ 10.0 };  // how big a target it is
		double height{ 10.0 };  // its middle above its feet
		double weight{ 1.0 };   // how often it comes up
		double bullX{ 0.0 };    // its bullseye's centre (the round plate on it, from its model)...
		double bullZ{ 10.0 };   // ...above its feet
		int    vanilla{ 100 };  // what Nuka-World's own gallery scores it (its tickets: a ticket every 200 of these)
	};

	struct GalleryLayout
	{
		double                   seconds{ 60.0 };
		double                   left{ -224.0 };
		double                   right{ 232.0 };
		std::vector<GalleryRow>  rows{ { 418.3, 180.2, 75.0, 6, true }, { 375.0, 111.2, -95.0, 6, true } };
		// 100 a hit, whatever it is; the bullseyes' centres from the models' bullseye plates (their :173 shapes). The
		// tickets go by Nuka-World's own values (DLC04_Nukacade_BanditRoundup's ScoreEvents, its targets told apart by
		// their bounds: the bunny 100, the pig 150, the duck 200, the star 250, the bottle 300)
		std::vector<GalleryKind> kinds{ { 100, 14.0, 14.0, 4.0, -0.7, 11.1, 200 }, { 100, 15.0, 16.0, 3.0, -0.4, 11.0, 100 },
			{ 100, 13.0, 11.0, 2.0, -1.1, 15.0, 150 }, { 100, 8.0, 22.0, 1.2, 0.0, 20.0, 300 }, { 100, 12.0, 11.0, 0.6, -0.15, 14.15, 250 } };
		Vec3                     stand{ -15.0, -135.0, 0.0 };   // where a settler playing it stands: well back from the
		                                                        // counter (its front at -47; near it, the game reads the
		                                                        // counter as low cover and they crouch to aim over it;
		                                                        // from -105 the rifle's muzzle came up to the counter's
		                                                        // front), a little left of the middle
		Vec3                     aimAt{ 0.0, 397.0, 155.0 };  // a settler playing it aims here: between the two tracks
		Vec3                     rifleRest{ 12.0, 3.0, 67.0 };  // the rifle lying along the counter (its top at 64, from y -32 to
		                                                        // 38: too shallow for it to point at the targets), its grip here
		double                   deadCentre{ 2.0 };   // within this of a bullseye's centre (either row's)...
		int                      bullseyePoints{ 300 }; // ...is worth this
		double                   flipSeconds{ 0.15 };
		double                   shotSeconds{ 0.6 };  // the lever between shots
	};

	// Bandit Roundup 1v1: the left (0) and right (1) shooters' spots, as far back as one alone stands
	inline Vec3 SettlerStand1v1(int a_side) { return { a_side == 0 ? -70.0 : 60.0, -135.0, 0.0 }; }

	// where a settler stands to play each game (machine space, on the floor)
	inline Vec3 SettlerStand(GameId a_game)
	{
		switch (a_game) {
		case GameId::kAtomicRollers: return { -21.4, -193.5, 0.0 };  // (its front at -162, the ball's release at -148: our roll
		                                                             // steps the left foot in and lets go 21 right, 46 ahead, 45 up
		                                                             // of her spot; her toes 4 clear of the front)
		case GameId::kHoopShot: return { 0.0, -160.0, 0.0 };       // (its front at -127)
		case GameId::kWhacACommie: return { 0.0, -105.0, 0.0 };    // (its front at -40, the holes -20 to 20: a standing swing's
		                                                           // mallet comes down about a metre ahead; from -68 it hit the back)
		default: return { -15.0, -135.0, 0.0 };                    // Bandit Roundup (GalleryLayout::stand)
		}
	}

	// where a settler's ball leaves her hands (machine space): the release frame of our animation, from her spot.
	// Hoop Shot's set shot: the ball on her fingers, in front of the right palm, on the ball's back (2 right, 22 ahead, 145 up)
	inline Vec3 SettlerRelease(GameId a_game)
	{
		const Vec3 at = SettlerStand(a_game);
		return a_game == GameId::kHoopShot ? Vec3{ at.x + 1.7, at.y + 21.6, at.z + 144.5 } : Vec3{ at.x + 21.4, at.y + 45.5, at.z + 42.6 };
	}

	const RollersLayout& Rollers();
	const HoopLayout&    Hoop();
	const WhacLayout&    Whac();
	const ZapperLayout&  Zapper();
	const GalleryLayout& Gallery();

	// the machines' shapes for the ball games
	void BuildRollers(World& a_world);
	void BuildHoop(World& a_world);
}
