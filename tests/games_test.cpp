// The arcade games' physics and rules on their own (no game): g++ -std=c++20 -Isrc tests/games_test.cpp src/sim/*.cpp
#include "sim/Arcade.h"
#include "sim/Layouts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

using namespace sim;

namespace
{
	int g_failed = 0;

	void Check(bool a_ok, const char* a_what)
	{
		std::printf("%s %s\n", a_ok ? "ok  " : "FAIL", a_what);
		if (!a_ok) {
			++g_failed;
		}
	}

	Input LookAt(const Vec3& a_eye, const Vec3& a_at)
	{
		Input in;
		in.eye = a_eye;
		in.dir = (a_at - a_eye).Normalized();
		return in;
	}

	void BallComesToRest()
	{
		World w;
		w.spec.ballRadius = 2.0;
		Rect floor;
		floor.centre = { 0, 0, 0 };
		floor.halfU = floor.halfV = 100;
		w.rects.push_back(floor);
		w.balls.assign(1, Ball{});
		w.Launch(0, { 0, 0, 30 }, { 40, 0, 0 });
		std::vector<Event> ev;
		for (int i = 0; i < 600; ++i) {
			w.Step(1.0 / 60.0, ev);
		}
		const auto& b = w.balls[0];
		Check(std::abs(b.pos.z - 2.0) < 0.2, "a dropped ball ends up resting on the floor");
		Check(b.vel.Length() < 2.0, "...and stops rolling");
		Check(!ev.empty() && ev[0].kind == Event::Kind::kHit, "...with a hit when it lands");
	}

	void BallThroughHoop()
	{
		World w;
		w.spec.ballRadius = layout::Hoop().ballRadius;
		layout::BuildHoop(w);
		w.balls.assign(1, Ball{});
		const auto& L = layout::Hoop();
		w.Launch(0, L.rim + Vec3{ 0, 0, 40 }, { 0, 0, 0 });
		std::vector<Event> ev;
		bool basket = false;
		for (int i = 0; i < 120 && !basket; ++i) {
			w.Step(1.0 / 60.0, ev);
			for (const auto& e : ev) {
				basket |= e.kind == Event::Kind::kCaught && e.other == layout::kHoopBasket;
			}
		}
		Check(basket, "a ball dropped through the hoop's middle is a basket");

		// one dropped on the rim's front edge bounces off, no basket
		World w2;
		w2.spec.ballRadius = L.ballRadius;
		layout::BuildHoop(w2);
		w2.balls.assign(1, Ball{});
		w2.Launch(0, L.rim + Vec3{ 0, -L.rimRadius - 1.0, 40 }, { 0, 0, 0 });
		std::vector<Event> ev2;
		bool basket2 = false, rim = false;
		for (int i = 0; i < 120; ++i) {
			w2.Step(1.0 / 60.0, ev2);
		}
		for (const auto& e : ev2) {
			basket2 |= e.kind == Event::Kind::kCaught && e.other == layout::kHoopBasket;
			rim |= e.kind == Event::Kind::kHit && e.where.z > L.rim.z - 15.0;
		}
		Check(rim, "a ball dropped on the rim hits it");
		Check(!basket2, "...and isn't a basket");
	}

	// plays a whole game with a_policy deciding the input each frame; returns the score
	template <class F>
	int Play(Game& a_g, F a_policy, double a_maxSeconds, double* a_took = nullptr)
	{
		a_g.Start(7);
		double t = 0.0;
		const double dt = 1.0 / 60.0;
		while (!a_g.Over() && t < a_maxSeconds) {
			Input in = a_policy(a_g, t);
			a_g.Update(dt, in);
			t += dt;
		}
		if (a_took) {
			*a_took = t;
		}
		return a_g.Score();
	}

	void Rollers()
	{
		const auto& L = layout::Rollers();
		// each power, a ball straight up the middle: where does it go?
		std::map<int, int> where;
		for (int p = 0; p <= 10; ++p) {
			auto   g = MakeGame(GameId::kAtomicRollers);
			double charge = 0.05 + L.chargeSeconds * p / 10.0;
			int    before = 0;
			g->Start(1);
			double t = 0.0, held = 0.0;
			bool   down = false, rolled = false;
			int    got = -1;
			while (t < 15.0 && got < 0) {
				Input in = LookAt({ 0, L.release.y - 40, L.release.z + 60 }, { 0, L.release.y + 40, L.release.z });
				if (!rolled) {
					if (!down) {
						in.pressed = in.held = true;
						down = true;
					} else if (held < charge) {
						in.held = true;
					} else {
						in.released = true;
						rolled = true;
					}
					held += 1.0 / 60.0;
				}
				g->Update(1.0 / 60.0, in);
				t += 1.0 / 60.0;
				if (rolled && g->sinceLastPoints == 0.0) {
					got = g->Score() - before;
				}
			}
			where[p] = got;
			std::printf("     rollers: power %3d%% -> %d\n", p * 10, got);
		}
		bool any = false;
		for (auto& [p, s] : where) {
			any |= s > 0;
		}
		Check(any, "atomic rollers: some power gets a straight ball into a ring");

		// the guide line follows where you look on the lane
		{
			auto g2 = MakeGame(GameId::kAtomicRollers);
			g2->Start(1);
			const Vec3 eye{ 0, L.release.y - 40, L.release.z + 60 };
			g2->Update(1.0 / 60.0, LookAt(eye, { 20, -40, 38.4 }));
			HudInfo h;
			g2->Hud(h);
			Check(h.guide.size() > 5 && h.guide.back().x > 5.0, "atomic rollers: looking right of the middle, the guide line goes right");
			g2->Update(1.0 / 60.0, LookAt(eye, { 0, -40, 38.4 }));
			HudInfo h2;
			g2->Hud(h2);
			Check(!h2.guide.empty() && std::abs(h2.guide.back().x) < 1.0, "atomic rollers: looking down the middle, it's straight");
		}

		// a whole game, full power every time
		auto   g = MakeGame(GameId::kAtomicRollers);
		double took = 0.0;
		bool   held = false;
		double heldFor = 0.0;
		Play(*g, [&](Game&, double) {
			Input in = LookAt({ 0, L.release.y - 40, L.release.z + 60 }, { 0, L.release.y + 40, L.release.z });
			if (!held) {
				in.pressed = in.held = true;
				held = true;
				heldFor = 0.0;
			} else if (heldFor < 0.7) {
				in.held = true;
				heldFor += 1.0 / 60.0;
			} else {
				in.released = true;
				held = false;
			}
			return in;
		}, 200.0, &took);
		Check(g->Over(), "atomic rollers: nine balls and it's over");
		std::printf("     rollers: a game took %.1f s, scored %d, %d tickets\n", took, g->Score(), g->TicketsFor(g->Score()));
	}

	void HoopShot()
	{
		const auto& L = layout::Hoop();
		auto        g = MakeGame(GameId::kHoopShot);
		// stand at the front, aim a little over the rim, try each power
		const Vec3 eye{ 0, -175, 120 };
		int        best = 0;
		double     bestCharge = 0.0;
		for (int p = 0; p <= 20; ++p) {
			const double charge = L.chargeSeconds * p / 20.0;
			double       h = 0.0;
			bool         down = false;
			const int    s = Play(*g, [&](Game&, double) {
                Input in = LookAt(eye, L.rim + Vec3{ 0, 0, 0 });
                if (!down) {
                    in.pressed = in.held = true;
                    down = true;
                    h = 0.0;
                } else if (h < charge) {
                    in.held = true;
                    h += 1.0 / 60.0;
                } else {
                    in.released = true;
                    down = false;
                }
                return in;
            }, 60.0);
			if (s > best) {
				best = s;
				bestCharge = charge;
			}
		}
		std::printf("     hoop shot: best %d points at %.2f s of charge\n", best, bestCharge);
		Check(best > 0, "hoop shot: from the front, some power scores");
	}

	void Whac()
	{
		const auto& L = layout::Whac();
		auto        g = MakeGame(GameId::kWhacACommie);
		const Vec3  eye{ 0, -70, 120 };
		double      took = 0.0;
		// a quick player: whacks every head as soon as it's well up (the weapon's hit event)
		const int s = Play(*g, [&](Game& a_g, double) {
            std::vector<Piece> pieces;
            a_g.Pieces(pieces);
            for (int i = 0; i < static_cast<int>(pieces.size()); ++i) {
                if (!pieces[i].hidden && pieces[i].pos.z > L.holes[i].z + L.headDown + (L.headUp - L.headDown) * 0.6) {
                    a_g.OnPieceHit(i);
                }
            }
            return LookAt(eye, { 0, 0, 0 });
        }, 60.0, &took);
		std::printf("     whac-a-commie: %d points in %.1f s, %d tickets\n", s, took, g->TicketsFor(s));
		Check(s > 20, "whac-a-commie: a quick player whacks plenty");
		Check(g->Over() && took < 33.0, "whac-a-commie: over after its 30 seconds");
		auto      idle = MakeGame(GameId::kWhacACommie);
		const int none = Play(*idle, [&](Game& a_g, double) {
            a_g.OnPieceHit(3);  // a hole whacked while its head's down: nothing
            Input in = LookAt(eye, L.holes[3]);
            in.pressed = true;  // (clicking scores nothing: only the weapon's hits)
            return in;
        }, 60.0);
		Check(none < 20, "whac-a-commie: clicks score nothing; whacking one hole only gets its own heads");
	}

	void Zapper()
	{
		const auto& L = layout::Zapper();
		const auto& lane = L.lanes[L.playerLane];
		auto        g = MakeGame(GameId::kNukaZapperRace);
		const Vec3  eye = L.gunRest + Vec3{ 0, -40, 50 };
		bool        was = false;
		const int   s = Play(*g, [&](Game&, double) {
            Input in = LookAt(eye, lane.target);
            in.pressed = !was;
            was = !was;
            return in;
        }, 60.0);
		Check(s == L.hitsToWin * L.pointsPerHit + L.winPoints, "nuka-zapper race: squirting dead on target wins");
		auto      lazy = MakeGame(GameId::kNukaZapperRace);
		const int s2 = Play(*lazy, [&](Game&, double) { return LookAt(eye, lane.target + Vec3{ 0, 0, 50 }); }, 60.0);
		Check(s2 == 0 && lazy->Over(), "nuka-zapper race: not squirting, the others win and you're placed");

		// settlers racing each other (you not in it): lanes 0, 2 and 5 raced, the rest empty; one wins, the shots come
		// from raced lanes only, the empty lanes' rockets stay down
		{
			auto race = MakeGame(GameId::kNukaZapperRace);
			race->Start(11);
			SetZapperRacers(*race, -1, { 0.9, -1, 0.2, -1, -1, 0.5 }, { "A", "", "B", "", "", "C" });
			bool onlyRaced = true;
			double t = 0.0;
			while (!race->Over() && t < 120.0) {
				race->Update(1.0 / 60.0, Input{});
				for (int lane : TakeZapperShots(*race)) {
					onlyRaced &= lane == 0 || lane == 2 || lane == 5;
				}
				t += 1.0 / 60.0;
			}
			std::vector<Piece> pieces;
			race->Pieces(pieces);
			const int w = ZapperWinner(*race);
			std::printf("     nuka-zapper race, settlers: lane %d won in %.0fs, score %d\n", w, t, race->Score());
			Check(race->Over() && (w == 0 || w == 2 || w == 5) && onlyRaced && pieces[1].pos.z == L.lanes[1].rocketBottom.z,
				"nuka-zapper race: settlers race each other, empty lanes stay still");
		}
	}

	void ThrowSettlers()
	{
		// settlers at Atomic Rollers and Hoop Shot: a whole game each, some points
		for (GameId id : { GameId::kAtomicRollers, GameId::kHoopShot }) {
			int got[2]{};
			for (int k = 0; k < 2; ++k) {
				auto    g = MakeGame(id);
				Thrower t(3, k == 0 ? 0.9 : 0.2, id);
				got[k] = Play(*g, [&](Game& a_g, double) { return t.Next(1.0 / 60.0, a_g.Ready()); }, 200.0);
			}
			std::printf("     %s settlers: a good one %d, a poor one %d\n", GameName(id), got[0], got[1]);
			Check(got[0] > 0, id == GameId::kHoopShot ? "hoop shot: a settler plays it" : "atomic rollers: a settler plays it");
		}
	}

	void ThrowWaits()
	{
		// a settler at Atomic Rollers throws only with the last ball counted and the next in hand, and not after the last
		auto    g = MakeGame(GameId::kAtomicRollers);
		Thrower t(11, 0.6, GameId::kAtomicRollers);
		g->Start(3);
		int    early = 0, presses = 0;
		double tm = 0.0;
		while (tm < 120.0) {
			const bool  ready = g->Ready();
			const Input in = t.Next(1.0 / 60.0, ready);
			if (in.pressed) {
				++presses;
				early += ready ? 0 : 1;
			}
			g->Update(1.0 / 60.0, in);
			tm += 1.0 / 60.0;
		}
		std::printf("     atomic rollers settler: %d throws, %d while the game was busy or over\n", presses, early);
		Check(g->Over() && presses == layout::Rollers().balls && early == 0, "atomic rollers: a settler waits for each ball to count, and stops after the last");
	}

	void ThrowCue()
	{
		// the throw's animation cue comes its lead before each release, once a throw
		for (GameId id : { GameId::kAtomicRollers, GameId::kHoopShot }) {
			Thrower t(7, 0.5, id);
			const double lead = id == GameId::kHoopShot ? 0.8 : 22.0 / 30.0;
			t.SetLead(lead);
			const double dt = 1.0 / 60.0;
			double       cueAt = -1.0, worst = 0.0;
			int          cues = 0, throws = 0;
			bool         twice = false;
			for (int i = 0; i < 60 * 30; ++i) {
				const double now = i * dt;
				const Input  in = t.Next(dt);
				if (t.TakeCue()) {
					twice = twice || cueAt >= 0.0;
					cueAt = now;
					++cues;
				}
				if (in.released && cueAt >= 0.0) {
					worst = std::max(worst, std::abs((now - cueAt) - lead));
					cueAt = -1.0;
					++throws;
				}
			}
			std::printf("     %s: %d throws, %d cues, worst %.3f s off\n", GameName(id), throws, cues, worst);
			Check(throws > 3 && !twice && worst < 0.05, id == GameId::kHoopShot ? "hoop shot: the shot's animation cued on time" : "atomic rollers: the roll's animation cued on time");
		}
	}

	void WhacSettlers()
	{
		// a settler at Whac-a-Commie: a good one whacks more than a poor one
		int got[2]{};
		for (int k = 0; k < 2; ++k) {
			auto       g = MakeGame(GameId::kWhacACommie);
			WhacPlayer p(5, k == 0 ? 0.9 : 0.1);
			got[k] = Play(*g, [&](Game& a_g, double) {
				p.Update(a_g, 1.0 / 60.0);
				return Input{};
			}, 40.0);
		}
		std::printf("     whac-a-commie settlers: a good one %d, a poor one %d\n", got[0], got[1]);
		Check(got[0] > 5 && got[0] > got[1] && got[1] > 0, "whac-a-commie: a settler plays it (a good one beats a poor one)");
	}

	void Gallery()
	{
		auto       g = MakeGame(GameId::kBanditRoundup);
		const Vec3 eye{ 0, -60, 120 };
		bool       was = false;
		const int  s = Play(*g, [&](Game& a_g, double) {
            std::vector<Piece> pieces;
            a_g.Pieces(pieces);
            const auto& L = layout::Gallery();
            Vec3        at{ 0, 300, 140 };
            for (const auto& p : pieces) {
                // an upright one near the middle
                if (std::abs(p.rot[1][1]) > 0.99 && std::abs(p.pos.x) < 120) {
                    at = p.pos + Vec3{ 0, 0, L.kinds[p.model].height };
                    break;
                }
            }
            Input in = LookAt(eye, at);
            in.pressed = !was;
            was = !was;
            return in;
        }, 60.0);
		std::printf("     bandit roundup: %d points, %d tickets\n", s, g->TicketsFor(s));
		Check(s > 20, "bandit roundup: aiming at the targets hits them");
		// its tickets at Nuka-World's rate: what its gallery scores each target (100 to 300), a ticket every 200, so
		// between half a ticket and one and a half a hit (a bullseye's 3 points no more)
		Check(g->TicketsFor(s) * 2 >= s / 3 - 2 && g->TicketsFor(s) * 2 <= s * 3 + 2,
			"bandit roundup: tickets at Nuka-World's rate (half a ticket to one and a half a hit)");

		// a settler at it: a good shot scores, a poor one less
		{
			auto good = MakeGame(GameId::kBanditRoundup);
			auto poor = MakeGame(GameId::kBanditRoundup);
			GalleryShooter sg(7, 0.006), sp(7, 0.03);
			const int a = Play(*good, [&](Game& a_g, double) { return sg.Next(a_g, eye, 1.0 / 60.0); }, 70.0);
			const int b = Play(*poor, [&](Game& a_g, double) { return sp.Next(a_g, eye, 1.0 / 60.0); }, 70.0);
			std::printf("     bandit roundup settlers: a good shot %d points, a poor one %d\n", a, b);
			Check(a > 30 && a > b && b > 0, "bandit roundup: a settler plays it (a good shot beats a poor one)");
		}

		// 1v1: two settlers side by side at the same targets, each scoring their own hits; the winner 10 more
		{
			auto duel = MakeGame(GameId::kBanditRoundup);
			duel->Start(11);
			SetGalleryVersus(*duel, "ANN", "BOB");
			const Vec3     left = layout::SettlerStand1v1(0) + Vec3{ 0, 0, 115 }, right = layout::SettlerStand1v1(1) + Vec3{ 0, 0, 115 };
			GalleryShooter a(3, 0.006), b(4, 0.02);
			for (int i = 0; i < 70 * 60 && !duel->Over(); ++i) {
				Input ia = a.Next(*duel, left, 1.0 / 60.0);
				duel->Update(1.0 / 60.0, ia);
				Input ib = b.Next(*duel, right, 1.0 / 60.0);
				ib.who = 1;
				duel->Update(0.0, ib);
			}
			const int sa = GalleryScore(*duel, 0), sb = GalleryScore(*duel, 1);
			std::printf("     bandit roundup 1v1: %d to %d\n", sa, sb);
			Check(duel->Over() && sa > 10 && sb > 10 && sa != sb && duel->Score() == sa, "bandit roundup: a 1v1, each scoring their own hits");
			HudInfo h;
			duel->Hud(h);
			Check(h.verdict == (sa > sb ? "ANN WON" : "BOB WON"), "bandit roundup: a 1v1's winner");
		}

		// one shot dead centre at a bullseye, either row: 3
		for (int row = 0; row < 2; ++row) {
			auto shot = MakeGame(GameId::kBanditRoundup);
			shot->Start(5);
			std::vector<Piece> pieces;
			shot->Pieces(pieces);
			const auto& L = layout::Gallery();
			int         first = 0;
			for (int r = 0; r < row; ++r) {
				first += L.rows[r].count;
			}
			const auto& p = pieces[first];
			const auto& k = L.kinds[p.model];
			Input in = LookAt(eye, p.pos + Vec3{ k.bullX, 0, k.bullZ });
			in.pressed = true;
			shot->Update(0.0, in);
			Check(shot->Score() == 3, row == 0 ? "bandit roundup: dead centre of a top-row bullseye is 3" :
			                                     "bandit roundup: dead centre of a bottom-row bullseye is 3");
		}
	}

	void Tickets()
	{
		TicketTable t{ 10, 1, 50, 400, 10 };
		Check(t.For(0) == 1, "tickets: at least one for playing");
		Check(t.For(95) == 9, "tickets: a ticket a ten points");
		Check(t.For(400) == 50, "tickets: the jackpot bonus, up to the most");
		Check(t.For(10000) == 50, "tickets: never more than the most");
	}
}

int main()
{
	BallComesToRest();
	BallThroughHoop();
	Rollers();
	HoopShot();
	Whac();
	Zapper();
	Gallery();
	WhacSettlers();
	ThrowSettlers();
	ThrowCue();
	ThrowWaits();
	Tickets();
	std::printf(g_failed ? "%d FAILED\n" : "all passed\n", g_failed);
	return g_failed ? 1 : 0;
}
