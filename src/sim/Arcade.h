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
		double      aimX{ 0.0 };      // -1..1: where your throw goes across the lane (rollers' guide), 0 centre
		bool        showAim{ false };
		std::vector<std::pair<std::string, double>> race;  // the zapper race: each lane's name and how far up (0..1)
		bool        timerOnly{ false };  // only the clock while playing (and the result at the end)
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

	// rows of a rotation of a_angle radians about z (a piece turned to face along y)
	void RotZ(double a_angle, double a_out[3][3]);
	// ...about x (a target flipping back, a head tipping)
	void RotX(double a_angle, double a_out[3][3]);
}
