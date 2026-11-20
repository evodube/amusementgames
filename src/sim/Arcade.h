#pragma once

// The five games' rules, plain C++ with no game dependencies (tests/games_test.cpp). Each game runs in its machine's
// space (sim/Ball3D.h: x right, y into the machine, z up, game units) and is driven by the plugin each frame with
// where you're looking and the fire button. It says back what's to be seen (its pieces: balls, heads, rockets,
// targets, each one of the machine's vanilla models), what's to be heard (sound cues) and what the HUD shows.

#include "sim/Ball3D.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace sim
{
	enum class GameId : int
	{
		kAtomicRollers,
		kHoopShot,
		kWhacACommie,
		kNukaZapperRace,
		kBanditRoundup,
		kCount
	};

	const char* GameName(GameId a_id);

	// what the player's doing this frame, in machine space
	struct Input
	{
		Vec3 eye;              // the camera
		Vec3 dir{ 0, 1, 0 };   // where it looks (unit)
		bool pressed{ false }; // the fire button went down this frame
		bool held{ false };    // ...is down
		bool released{ false };// ...came up this frame
	};

	// a sound the plugin plays (the game's own Nuka-Cade sound descriptors: Cue picks which)
	enum class Cue : int
	{
		kStart,       // the machine starting up
		kEnd,         // the finish bell
		kWin,         // a win (the race, a new best)
		kBallHitHard, // a ball against the machine, hard
		kBallHitSoft, // ...softly
		kPoints10,    // Atomic Rollers' points calls
		kPoints20,
		kPoints30,
		kPoints40,
		kPoints50,
		kPoints100,
		kBasket,      // a basket
		kBounce,      // the basketball on something
		kWhack,       // a commie whacked
		kPopUp,       // ...popping up
		kSquirt,      // a Nuka-Zapper squirt
		kTargetHit,   // a gallery target or the zapper target hit
		kShot,        // the gallery gun
		kTicket,      // tickets coming out
		kHurry,       // the last seconds (the fast music)
		kCount
	};

	struct SoundCue
	{
		Cue    cue{ Cue::kStart };
		Vec3   where;
		double gain{ 1.0 };
	};

	// one of the vanilla models a game shows (the plugin's list of forms for that game, by index)
	struct Piece
	{
		int    model{ 0 };
		Vec3   pos;
		double rot[3][3]{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };  // rows: the piece's axes in machine space
		double scale{ 1.0 };
		bool   hidden{ false };
	};

	struct HudInfo
	{
		std::string line1;            // e.g. "BALL 4 OF 9"
		std::string line2;            // e.g. "LAST: 40"
		double      timeLeft{ -1.0 }; // seconds (-1: not a timed game)
		double      power{ -1.0 };    // 0..1 while charging a throw (-1: no meter)
		std::vector<std::pair<std::string, double>> race;  // the zapper race: each lane's name and how far up (0..1)
		std::string verdict;             // the zapper race over: "YOU WON!" or "YOU LOST · <winner> WON"
		std::string clock;               // an untimed game's stand-in for the clock (Atomic Rollers' ball, the race's countdown)
		bool        showPoints{ false }; // the points as they come, on screen (Atomic Rollers)
		int         waiting{ -1 };       // the machine's things on show while you play (Atomic Rollers: the balls left in its
		                                 // pocket); -1: none (they're in your hands)
		std::vector<Vec3> guide;         // the aim line, machine space, from the ball out (Atomic Rollers, as fo4bowling's)
	};

	class Game
	{
	public:
		virtual ~Game() = default;

		virtual GameId Id() const = 0;
		virtual void   Start(std::uint32_t a_seed) = 0;
		virtual void   Update(double a_dt, const Input& a_in) = 0;
		virtual bool   Over() const = 0;
		virtual void   Pieces(std::vector<Piece>& a_out) const = 0;
		virtual void   Hud(HudInfo& a_out) const = 0;
		// one of the game's pieces hit by your weapon (Whac-a-Commie: a head whacked)
		virtual void   OnPieceHit(int) {}

		int Score() const { return _score; }
		// the tickets a score is worth at this game
		virtual int TicketsFor(int a_score) const = 0;

		// the sounds since the last call
		std::vector<SoundCue> TakeSounds()
		{
			std::vector<SoundCue> out;
			out.swap(_sounds);
			return out;
		}
		// the score's last change, for the HUD's call-out ("+40"), and how long ago
		int    lastPoints{ 0 };
		double sinceLastPoints{ 99.0 };

	protected:
		void Say(Cue a_cue, const Vec3& a_where, double a_gain = 1.0) { _sounds.push_back({ a_cue, a_where, a_gain }); }
		void Add(int a_points)
		{
			_score += a_points;
			lastPoints = a_points;
			sinceLastPoints = 0.0;
		}

		int                   _score{ 0 };
		std::vector<SoundCue> _sounds;
		std::mt19937          _rng{ 1 };
	};

	// tickets: points / perTicket, at least min (for having played), at most max, and a bonus at or over jackpot
	struct TicketTable
	{
		int perTicket{ 10 };
		int min{ 1 };
		int max{ 100 };
		int jackpot{ 0 };     // 0: none
		int jackpotBonus{ 0 };

		int For(int a_score) const;
	};

	std::unique_ptr<Game> MakeGame(GameId a_id);

	// Nuka-Zapper Race with settlers at it (after Start): a_skill, one a lane: a settler of that skill (0..1) racing
	// that lane, named a_names[lane], or negative: nobody there (its rocket stays down); a_you: your lane, or -1 (the
	// settlers race each other: the game's score is then the winner's)
	void SetZapperRacers(Game& a_game, int a_you, const std::vector<double>& a_skill, const std::vector<std::string>& a_names);
	// ...the lanes that squirted since the last call (their settlers' shots to show)
	std::vector<int> TakeZapperShots(Game& a_game);
	// ...the lane that won (-1: not yet)
	int ZapperWinner(const Game& a_game);

	// a settler playing Whac-a-Commie: picks a head that's up, reacts (the better, the quicker), swings, and lands it
	// (or not: the better, the likelier) once the swing comes down. Update: true when a swing starts (its animation)
	class WhacPlayer
	{
	public:
		WhacPlayer(std::uint32_t a_seed, double a_skill);
		bool Update(Game& a_game, double a_dt);

	private:
		std::mt19937 _rng;
		double       _skill;
		double       _cool{ 1.5 };   // (first: the mallet up)
		int          _target{ -1 };
		double       _react{ 0.0 };
		double       _swing{ -1.0 };
	};

	// a settler playing Atomic Rollers or Hoop Shot: stands where you would, looks up the lane (a little off, the
	// worse, the more) or at the rim, holds the throw a while (Hoop Shot: about the best charge, off by a bit) and lets
	// go, a few seconds between throws. Next: its input this frame (released: the throw, its animation); Eye: where
	// it stands to throw from
	class Thrower
	{
	public:
		Thrower(std::uint32_t a_seed, double a_skill, GameId a_game);
		Input Next(double a_dt);
		Vec3  Eye() const;

	private:
		std::mt19937 _rng;
		double       _skill;
		GameId       _game;
		double       _wait{ 2.0 };   // till the next throw's charge starts
		double       _charge{ 0.0 }; // ...how long it's held
		double       _held{ -1.0 };  // ...held so far (-1: not holding)
		Vec3         _at;            // ...where it's aimed
	};

	// a settler playing Bandit Roundup: picks an upright target near the middle, aims at its bullseye (off by about
	// a_spread radians), fires every so often. Next() gives its input for this frame.
	class GalleryShooter
	{
	public:
		GalleryShooter(std::uint32_t a_seed, double a_spread);
		Input Next(const Game& a_game, const Vec3& a_eye, double a_dt);
		// the shot just given wasn't taken (not on target yet): another next frame
		void Again() { _wait = 0.0; }

	private:
		std::mt19937 _rng;
		double       _spread;
		double       _wait{ 1.5 };  // till the next shot (the first: once they've the rifle up)
	};

	// rows of a rotation of a_angle radians about z (a piece turned to face along y)
	void RotZ(double a_angle, double a_out[3][3]);
	// ...about x (a target flipping back, a head tipping)
	void RotX(double a_angle, double a_out[3][3]);
}
