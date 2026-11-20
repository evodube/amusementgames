#include "sim/Arcade.h"

#include "sim/Layouts.h"

#include <algorithm>
#include <utility>
#include <array>
#include <cmath>
#include <format>

namespace sim
{
	namespace
	{
		constexpr double kPi = 3.14159265358979323846;

		double Clamp01(double a_v) { return std::clamp(a_v, 0.0, 1.0); }

		// where the ray (eye, dir) meets the plane through a_point with normal a_n; false if it doesn't (parallel, behind)
		bool RayPlane(const Input& a_in, const Vec3& a_point, const Vec3& a_n, Vec3& a_out)
		{
			const double den = a_in.dir.Dot(a_n);
			if (std::abs(den) < 1e-9) {
				return false;
			}
			const double t = (a_point - a_in.eye).Dot(a_n) / den;
			if (t <= 0.0) {
				return false;
			}
			a_out = a_in.eye + a_in.dir * t;
			return true;
		}

		// a throw's power: up while the button's held, a full charge in a_full seconds
		struct Charge
		{
			bool   on{ false };
			double t{ 0.0 };

			double Power(double a_full) const { return Clamp01(t / a_full); }
		};

		// ======================================================================== Atomic Rollers (skee-ball)

		class Rollers final : public Game
		{
		public:
			GameId Id() const override { return GameId::kAtomicRollers; }

			void Start(std::uint32_t a_seed) override
			{
				_rng.seed(a_seed);
				const auto& L = layout::Rollers();
				_world = {};
				_world.spec.ballRadius = L.ballRadius;
				layout::BuildRollers(_world);
				_world.balls.assign(1, Ball{});
				_ball = 0;
				_left = L.balls;
				_phase = Phase::kAim;
				_wait = 0.0;
				_last = -1;
				_reballs = 0;
				_reball = false;
				_score = 0;
				Say(Cue::kStart, L.release);
				HoldBall();
			}

			void Update(double a_dt, const Input& a_in) override
			{
				const auto& L = layout::Rollers();
				sinceLastPoints += a_dt;
				switch (_phase) {
				case Phase::kAim:
					// where you look on the lane is where it goes (the guide line shows it), charged while held, rolled on
					// release
					{
						Vec3 at;
						if (RayPlane(a_in, Vec3{ 0, 0, L.laneFrontZ }, { 0, 0, 1 }, at) && at.y > L.release.y + 20.0) {
							Vec3 d{ at.x - L.release.x, at.y - L.release.y, 0.0 };
							d = d.Normalized();
							// no further across than the lane's edge by the hump
							const double most = L.aimHalfWidth / (L.rampStartY - L.release.y);
							if (std::abs(d.x / d.y) > most) {
								d = Vec3{ std::copysign(most, d.x), 1.0, 0.0 }.Normalized();
							}
							_aimDir = d;
						}
						HoldBall();
						if (a_in.pressed) {
							_charge = { true, 0.0 };
						}
						if (_charge.on) {
							_charge.t += a_dt;
						}
						if (_charge.on && a_in.released) {
							Roll(_charge.Power(L.chargeSeconds));
							_charge = {};
						}
					}
					break;
				case Phase::kRolling:
					{
						std::vector<Event> events;
						_world.Step(a_dt, events);
						for (const auto& e : events) {
							OnEvent(e);
						}
						auto& b = _world.balls[_ball];
						if (b.state == BallState::kFlying) {
							// stopped (no movement for 2 s), or still rattling about after 9 s, past the hump: caught on
							// the board's sides or in a corner, where a ball can't come to rest in the real thing; or
							// out of the machine. No points were counted: gone, and the same ball again (a few times
							// a game at most)
							const bool onBoard = b.pos.y > L.rampStartY;
							const bool stuck = (onBoard && (b.still > 2.0 || b.age > 9.0)) || b.pos.z < -50.0;
							if (stuck && _reballs < kMostReballs) {
								b.state = BallState::kOff;
								++_reballs;
								_reball = true;
								_phase = Phase::kBetween;
								_wait = 0.6;
								break;
							}
							if (stuck || b.still > 0.8 || b.age > 9.0) {
								b.state = BallState::kCaught;  // rolled back down the lane short of the hump: no points
								_last = 0;
								Add(0);
							}
						}
						if (b.state == BallState::kCaught) {
							// no points (the gutter, or back down the lane short of the hump): the ball back, not one
							// of the nine used (up to kMostReballs a game, with the stuck ones)
							if (_last == 0 && _reballs < kMostReballs) {
								++_reballs;
								_reball = true;
							}
							_phase = Phase::kBetween;
							_wait = 0.6;
						}
					}
					break;
				case Phase::kBetween:
					_wait -= a_dt;
					if (_wait <= 0.0 && _reball) {
						_reball = false;  // (a ball that scored nothing, again: not one of the nine used)
						_phase = Phase::kAim;
						HoldBall();
					} else if (_wait <= 0.0) {
						if (--_left <= 0) {
							_phase = Phase::kDone;
							_wait = 1.5;
							Say(Cue::kEnd, L.release);
						} else {
							_phase = Phase::kAim;
							HoldBall();
						}
					}
					break;
				case Phase::kDone:
					_wait -= a_dt;
					break;
				}
			}

			bool Over() const override { return _phase == Phase::kDone && _wait <= 0.0; }
			bool Ready() const override { return _phase == Phase::kAim; }

			void Pieces(std::vector<Piece>& a_out) const override
			{
				const auto& b = _world.balls[_ball];
				Piece       p;
				p.model = 0;
				p.pos = b.pos;
				b.rot.ToMatrix(p.rot);
				p.hidden = b.state == BallState::kCaught || b.state == BallState::kOff || _phase == Phase::kDone;
				a_out.push_back(p);
			}

			void Hud(HudInfo& a_out) const override
			{
				const auto& L = layout::Rollers();
				const int   n = std::min(L.balls, L.balls - _left + 1);
				a_out.line1 = _phase == Phase::kDone ? "GAME OVER" : std::format("BALL {} OF {}", n, L.balls);
				a_out.line2 = _last >= 0 ? std::format("LAST BALL: {}", _last) : std::string("HOLD TO CHARGE, LET GO TO ROLL");
				a_out.power = _charge.on ? _charge.Power(L.chargeSeconds) : (_phase == Phase::kAim ? 0.0 : -1.0);
				a_out.clock = _phase == Phase::kDone ? std::string() : std::format("BALL {}/{}", n, L.balls);
				a_out.showPoints = true;
				// the balls still to come, waiting in the pocket (the one on the lane or rolling not among them)
				a_out.waiting = _phase == Phase::kDone ? 0 : std::max(0, _left - 1);
				if (_phase == Phase::kAim) {
					// along the aim from the ball, on the lane's surface, to the hump (or the lane's edge)
					const Vec3& from = L.release;
					for (double t = L.ballRadius + 2.0;; t += 8.0) {
						const Vec3 p = from + _aimDir * t;
						if (p.y > L.rampStartY || std::abs(p.x) > L.laneHalfWidth - L.ballRadius) {
							break;
						}
						a_out.guide.push_back({ p.x, p.y, LaneZ(p.y) + 0.6 });
					}
				}
			}

			int TicketsFor(int a_score) const override { return TicketTable{ 100, 1, 70, 0, 0 }.For(a_score); }  // Nuka-World's: 100 a ticket

		private:
			enum class Phase
			{
				kAim,
				kRolling,
				kBetween,
				kDone
			};

			void HoldBall()
			{
				const auto& L = layout::Rollers();
				auto&       b = _world.balls[_ball];
				b.state = BallState::kHeld;
				b.pos = L.release;
				b.vel = {};
			}

			// the lane's surface height a distance along it
			static double LaneZ(double a_y)
			{
				const auto& L = layout::Rollers();
				if (a_y <= L.laneBendY) {
					return L.laneFrontZ + (L.laneBendZ - L.laneFrontZ) * (a_y - L.laneFrontY) / (L.laneBendY - L.laneFrontY);
				}
				return L.laneBendZ + (L.laneTopZ - L.laneBendZ) * (a_y - L.laneBendY) / (L.rampStartY - L.laneBendY);
			}

			void Roll(double a_power)
			{
				const auto&  L = layout::Rollers();
				const double speed = L.minSpeed + (L.maxSpeed - L.minSpeed) * a_power;
				// along the guide line
				const Vec3 from = _world.balls[_ball].pos;
				_world.Launch(_ball, from, _aimDir * speed);
				_phase = Phase::kRolling;
			}

			void OnEvent(const Event& a_e)
			{
				switch (a_e.kind) {
				case Event::Kind::kHit:
					Say(a_e.speed > 120.0 ? Cue::kBallHitHard : Cue::kBallHitSoft, a_e.where, Clamp01(a_e.speed / 300.0));
					break;
				case Event::Kind::kCaught:
					{
						const int points = std::max(0, a_e.other);
						_last = points;
						Add(points);
						// Nuka-World's calls: 100 .. 500, and 700 the corners
						const Cue cue = points >= 700 ? Cue::kPoints100 :
						                points >= 500 ? Cue::kPoints50 :
						                points >= 400 ? Cue::kPoints40 :
						                points >= 300 ? Cue::kPoints30 :
						                points >= 200 ? Cue::kPoints20 :
						                points >= 100 ? Cue::kPoints10 :
						                                Cue::kBallHitSoft;
						Say(cue, a_e.where);
					}
					break;
				default:
					break;
				}
			}

			World  _world;
			int    _ball{ 0 };
			int    _left{ 9 };
			Phase  _phase{ Phase::kAim };
			double _wait{ 0.0 };
			Vec3   _aimDir{ 0, 1, 0 };
			int    _last{ -1 };
			Charge _charge;
			int    _reballs{ 0 };      // stuck balls given back this game
			bool   _reball{ false };   // ...one now
			static constexpr int kMostReballs = 9;  // balls that scored nothing given back, a game at most
		};

		// ======================================================================== Hoop Shot

		class Hoop final : public Game
		{
		public:
			GameId Id() const override { return GameId::kHoopShot; }

			void Start(std::uint32_t a_seed) override
			{
				_rng.seed(a_seed);
				const auto& L = layout::Hoop();
				_world = {};
				_world.spec.ballRadius = L.ballRadius;
				_world.spec.ballRestitution = 0.55;
				layout::BuildHoop(_world);
				_world.balls.assign(L.ballsInPlay, Ball{});
				_time = L.seconds;
				_score = 0;
				_baskets = 0;
				_cool = 0.0;
				_hurried = false;
				_ending = 1.5;
				Say(Cue::kStart, L.rim);
			}

			void Update(double a_dt, const Input& a_in) override
			{
				const auto& L = layout::Hoop();
				sinceLastPoints += a_dt;
				if (_time > 0.0) {
					_time = std::max(0.0, _time - a_dt);
					if (!_hurried && _time <= L.hurrySeconds) {
						_hurried = true;
						Say(Cue::kHurry, L.rim);
					}
					if (_time <= 0.0) {
						Say(Cue::kEnd, L.rim);
					}
				} else {
					_ending -= a_dt;
				}
				_cool -= a_dt;
				_eye = a_in.eye;
				_dir = a_in.dir;
				if (_time > 0.0) {
					if (a_in.pressed) {
						_charge = { true, 0.0 };
					}
					if (_charge.on) {
						_charge.t += a_dt;
					}
					if (_charge.on && a_in.released) {
						Throw(a_in, _charge.Power(L.chargeSeconds));
						_charge = {};
					}
				} else {
					_charge = {};
				}
				std::vector<Event> events;
				_world.Step(a_dt, events);
				for (const auto& e : events) {
					OnEvent(e);
				}
				for (auto& b : _world.balls) {
					if (b.state == BallState::kFlying && (b.age > L.ballLife || b.pos.z < -50.0 || b.still > 1.0)) {
						b.state = BallState::kOff;
					}
					if (b.state == BallState::kCaught) {
						b.state = BallState::kOff;  // back down the return: free for the next throw
					}
				}
			}

			bool Over() const override { return _time <= 0.0 && _ending <= 0.0; }
			bool Ready() const override
			{
				return _time > 0.0 && _cool <= 0.0 &&
				       std::ranges::any_of(_world.balls, [](const Ball& b) { return b.state != BallState::kFlying; });
			}

			void Pieces(std::vector<Piece>& a_out) const override
			{
				for (const auto& b : _world.balls) {
					Piece p;
					p.model = 0;
					p.pos = b.pos;
					b.rot.ToMatrix(p.rot);
					p.hidden = b.state != BallState::kFlying;
					a_out.push_back(p);
				}
				// the ball in your hand while charging: held up in front of you
				Piece hand;
				hand.model = 0;
				hand.pos = Hand(_eye, _dir);
				hand.hidden = _time <= 0.0 || _cool > 0.0;
				a_out.push_back(hand);
			}

			void Hud(HudInfo& a_out) const override
			{
				a_out.timeLeft = _time;
				a_out.line1 = std::format("BASKETS {}", _baskets);
				a_out.line2 = _time > 0.0 && _time <= layout::Hoop().hurrySeconds ? std::string("LAST SECONDS!") :
				                                                                    std::string("HOLD TO CHARGE, LET GO TO SHOOT");
				a_out.power = _charge.on ? _charge.Power(layout::Hoop().chargeSeconds) : (_time > 0.0 ? 0.0 : -1.0);
			}

			int TicketsFor(int a_score) const override { return TicketTable{ 200, 1, 40, 0, 0 }.For(a_score); }  // a ticket every 2 baskets (Nuka-World's rate: 200)

		private:
			void Throw(const Input& a_in, double a_power)
			{
				const auto& L = layout::Hoop();
				if (_cool > 0.0) {
					return;
				}
				int free = -1;
				for (int i = 0; i < static_cast<int>(_world.balls.size()); ++i) {
					if (_world.balls[i].state != BallState::kFlying) {
						free = i;
						break;
					}
				}
				if (free < 0) {
					return;
				}
				const double speed = L.minSpeed + (L.maxSpeed - L.minSpeed) * a_power;
				// from your hand toward where you look (past the hand, at the distance of the hoop). No two throws alike: the
				// ball's a little elsewhere in your hand each time, and leaves it a little off line and off pace
				std::normal_distribution<double> n(0.0, 1.0);
				const Vec3 right = a_in.dir.Cross({ 0, 0, 1 }).Normalized();
				const Vec3 from = (a_in.hasFrom ? a_in.from : Hand(a_in.eye, a_in.dir)) + right * (n(_rng) * L.handWobble) + Vec3{ 0, 0, n(_rng) * L.handWobble * 0.6 };
				const Vec3 aimAt = a_in.eye + a_in.dir * std::max(60.0, (L.rim - a_in.eye).Length());
				Vec3       toward = (aimAt - from).Normalized();
				toward.z += L.arcLift;
				toward = toward.Normalized();
				toward = (toward + right * (n(_rng) * L.aimWobble) + Vec3{ 0, 0, n(_rng) * L.aimWobble }).Normalized();
				_world.Launch(free, from, toward * (speed * (1.0 + n(_rng) * L.paceWobble)));
				_world.balls[free].spin = Vec3{ 1, 0, 0 } * -8.0;  // backspin
				_cool = L.reloadSeconds;
			}

			// the ball in your hand: below your sights and to the right
			static Vec3 Hand(const Vec3& a_eye, const Vec3& a_dir)
			{
				const auto& L = layout::Hoop();
				const Vec3  right = a_dir.Cross({ 0, 0, 1 }).Normalized();
				return a_eye + a_dir * L.handForward + right * L.handSide + Vec3{ 0, 0, -L.handDrop };
			}

			void OnEvent(const Event& a_e)
			{
				const auto& L = layout::Hoop();
				if (a_e.kind == Event::Kind::kCaught && a_e.other == layout::kHoopBasket) {
					++_baskets;
					Add(L.points);
					Say(Cue::kBasket, a_e.where);
				} else if (a_e.kind == Event::Kind::kHit || a_e.kind == Event::Kind::kBalls) {
					Say(Cue::kBounce, a_e.where, Clamp01(a_e.speed / 400.0));
				}
			}

			World  _world;
			double _time{ 0.0 };
			double _cool{ 0.0 };
			double _ending{ 1.5 };
			int    _baskets{ 0 };
			bool   _hurried{ false };
			Charge _charge;
			Vec3   _eye;
			Vec3   _dir{ 0, 1, 0 };
		};

		// ======================================================================== Whac-a-Commie

		class Whac final : public Game
		{
		public:
			GameId Id() const override { return GameId::kWhacACommie; }

			void Start(std::uint32_t a_seed) override
			{
				_rng.seed(a_seed);
				const auto& L = layout::Whac();
				_heads.assign(L.holes.size(), Head{});
				_time = L.seconds;
				_next = 1.0;
				_score = 0;
				_hits = 0;
				_misses = 0;
				_ending = 1.5;
				_hurried = false;
				Say(Cue::kStart, L.holes.empty() ? Vec3{} : L.holes[0]);
			}

			void Update(double a_dt, const Input& a_in) override
			{
				const auto& L = layout::Whac();
				sinceLastPoints += a_dt;
				(void)a_in;  // (a whack is your weapon hitting a head: OnPieceHit)
				if (_time > 0.0) {
					_time = std::max(0.0, _time - a_dt);
					if (!_hurried && _time <= 10.0) {
						_hurried = true;
						Say(Cue::kHurry, L.holes[0]);
					}
					if (_time <= 0.0) {
						Say(Cue::kEnd, L.holes[0]);
					}
				} else {
					_ending -= a_dt;
				}
				// how far into the round: faster and more at once as it goes
				const double k = 1.0 - _time / L.seconds;
				const double upFor = (L.upLongest + (L.upShortest - L.upLongest) * k) * _upScale;
				const int    most = 1 + static_cast<int>(k * (L.mostUp - 1) + 0.5);
				if (_time > 0.0) {
					_next -= a_dt;
					if (_next <= 0.0) {
						_next = (L.gapLongest + (L.gapShortest - L.gapLongest) * k) * std::uniform_real_distribution<double>(0.7, 1.3)(_rng);
						int up = 0;
						std::vector<int> down;
						for (int i = 0; i < static_cast<int>(_heads.size()); ++i) {
							if (_heads[i].phase != Head::kDown) {
								++up;
							} else {
								down.push_back(i);
							}
						}
						if (up < most && !down.empty()) {
							auto& h = _heads[down[std::uniform_int_distribution<std::size_t>(0, down.size() - 1)(_rng)]];
							h = { Head::kRising, 0.0, upFor };
							Say(Cue::kPopUp, L.holes[&h - _heads.data()], 0.6);
						}
					}
				}
				for (int i = 0; i < static_cast<int>(_heads.size()); ++i) {
					auto& h = _heads[i];
					h.t += a_dt;
					switch (h.phase) {
					case Head::kRising:
						if (h.t >= L.riseSeconds) {
							h = { Head::kUp, 0.0, h.upFor };
						}
						break;
					case Head::kUp:
						if (h.t >= h.upFor || _time <= 0.0) {
							h = { Head::kSinking, 0.0, h.upFor };
							++_misses;
						}
						break;
					case Head::kSinking:
					case Head::kHit:
						if (h.t >= (h.phase == Head::kHit ? L.hitSeconds : L.riseSeconds)) {
							h = {};
						}
						break;
					default:
						break;
					}
				}
			}

			// a whack: your Commie Whacker hit a head (its hit event); it counts if the head's up far enough (a quarter up:
			// out of its hole, the mallet seen landing on it)
			void OnPieceHit(int a_piece) override
			{
				const auto& L = layout::Whac();
				if (_time <= 0.0 || a_piece < 0 || a_piece >= static_cast<int>(_heads.size()) || Height(a_piece) <= 0.25 ||
					_heads[a_piece].phase == Head::kHit) {
					return;
				}
				Add(L.points);
				++_hits;
				_heads[a_piece] = { Head::kHit, 0.0, _heads[a_piece].upFor };
				Say(Cue::kWhack, L.holes[a_piece]);
			}

			bool Over() const override { return _time <= 0.0 && _ending <= 0.0; }

			void Pieces(std::vector<Piece>& a_out) const override
			{
				const auto& L = layout::Whac();
				for (int i = 0; i < static_cast<int>(_heads.size()); ++i) {
					Piece p;
					p.model = 0;
					const double up = Height(i);
					p.pos = HeadAt(i);
					RotZ(L.headYaw, p.rot);
					if (_heads[i].phase == Head::kHit) {
						// knocked: a tip back as it drops
						double tip[3][3];
						RotX(-0.5 * (1.0 - up), tip);
						double m[3][3];
						for (int r = 0; r < 3; ++r) {
							for (int c = 0; c < 3; ++c) {
								m[r][c] = tip[r][0] * p.rot[0][c] + tip[r][1] * p.rot[1][c] + tip[r][2] * p.rot[2][c];
							}
						}
						std::copy(&m[0][0], &m[0][0] + 9, &p.rot[0][0]);
					}
					p.hidden = up <= 0.0;
					a_out.push_back(p);
				}
			}

			void Hud(HudInfo& a_out) const override
			{
				a_out.timeLeft = _time;
				a_out.showPoints = true;  // (+100 each whack)
				a_out.line1 = std::format("COMMIES WHACKED {}", _hits);
				a_out.line2 = std::format("GOT AWAY {}", _misses);
			}

			int TicketsFor(int a_score) const override { return TicketTable{ 200, 1, 40, 0, 0 }.For(a_score); }  // a ticket every 2 whacks (Nuka-World's rate: 200)

		private:
			struct Head
			{
				enum Phase
				{
					kDown,
					kRising,
					kUp,
					kSinking,
					kHit
				};
				Phase  phase{ kDown };
				double t{ 0.0 };
				double upFor{ 1.0 };
			};

		public:
			// a settler's game: heads up this much longer (their swing comes down about a second after they choose it)
			void SetUpScale(double a_scale) { _upScale = a_scale; }
			double HeightOf(int a_i) const { return a_i >= 0 && a_i < static_cast<int>(_heads.size()) ? Height(a_i) : 0.0; }

		private:
			double _upScale{ 1.0 };

			// where a head's origin is now
			Vec3 HeadAt(int a_i) const
			{
				const auto& L = layout::Whac();
				return L.holes[a_i] + Vec3{ 0, 0, L.headDown + (L.headUp - L.headDown) * Height(a_i) };
			}

			// 0 (down in its hole) .. 1 (all the way up)
			double Height(int a_i) const
			{
				const auto& L = layout::Whac();
				const auto& h = _heads[a_i];
				switch (h.phase) {
				case Head::kRising:
					{
						// a springy pop: past the top and back
						const double t = Clamp01(h.t / L.riseSeconds);
						return Clamp01(1.0 - std::cos(t * kPi * 0.5) + 0.12 * std::sin(t * kPi));
					}
				case Head::kUp: return 1.0;
				case Head::kSinking: return 1.0 - Clamp01(h.t / L.riseSeconds);
				case Head::kHit: return 1.0 - Clamp01(h.t / L.hitSeconds);
				default: return 0.0;
				}
			}

			std::vector<Head> _heads;
			double            _time{ 0.0 };
			double            _next{ 0.0 };
			double            _ending{ 1.5 };
			int               _hits{ 0 };
			int               _misses{ 0 };
			bool              _hurried{ false };
		};

		// ======================================================================== Nuka-Zapper Race
		// Nuka-World's rules: each squirt of your Thirst Zapper that hits your target sends your rocket a step up; ten
		// and you've won. The other lanes race you, squirting and missing as players do. Your lane is the one with the
		// Thirst Zapper lying on the counter (the plugin puts it there)

		class Zapper final : public Game
		{
		public:
			GameId Id() const override { return GameId::kNukaZapperRace; }

			void Start(std::uint32_t a_seed) override
			{
				_rng.seed(a_seed);
				const auto& L = layout::Zapper();
				_lanes.assign(L.lanes.size(), Lane{});
				static constexpr std::array<const char*, 8> names{ "SPARKY", "THE KID", "LUCKY LOU", "BUTTONS", "DIZZY", "ACE", "SLICK", "PEANUT" };
				std::vector<int> order{ 0, 1, 2, 3, 4, 5, 6, 7 };
				std::shuffle(order.begin(), order.end(), _rng);
				for (std::size_t i = 0; i < _lanes.size(); ++i) {
					_lanes[i].name = names[order[i % order.size()]];
					_lanes[i].skill = std::uniform_real_distribution<double>(0.0, 1.0)(_rng);
					_lanes[i].next = std::uniform_real_distribution<double>(0.2, 0.8)(_rng);
				}
				_you = std::clamp(L.playerLane, 0, static_cast<int>(_lanes.size()) - 1);
				_lanes[_you].name = "YOU";
				_shots.clear();
				_shotHits.clear();
				_winner = -1;
				_place = 0;
				_score = 0;
				_ending = 2.0;
				_countdown = 3.0;
				_cool = 0.0;
				Say(Cue::kStart, L.lanes.empty() ? Vec3{} : L.lanes[0].target);
			}

			void Update(double a_dt, const Input& a_in) override
			{
				const auto& L = layout::Zapper();
				sinceLastPoints += a_dt;
				for (auto& l : _lanes) {
					// the rockets climb to their heights
					const double want = static_cast<double>(l.hits) / L.hitsToWin;
					l.shown += (want - l.shown) * std::min(1.0, L.rocketRise * a_dt);
				}
				if (_countdown > 0.0) {
					_countdown -= a_dt;
					return;
				}
				if (_place > 0) {
					_ending -= a_dt;
					return;
				}
				_cool -= a_dt;
				// you: a squirt (a click), a hit if your sights are on your target
				if (_you >= 0 && a_in.pressed && _cool <= 0.0) {
					_cool = L.shotSeconds;
					const auto& g = L.lanes[_you];
					Vec3        at;
					if (RayPlane(a_in, g.target, { 0, -1, 0 }, at) && std::hypot(at.x - g.target.x, at.z - g.target.z) <= L.targetRadius) {
						Hit(_you);
					}
				}
				// the others
				for (int i = 0; i < static_cast<int>(_lanes.size()) && _place == 0; ++i) {
					if (i == _you || _lanes[i].empty) {
						continue;
					}
					auto& l = _lanes[i];
					l.next -= a_dt;
					if (l.next <= 0.0) {
						l.next = std::uniform_real_distribution<double>(L.aiShotShortest, L.aiShotLongest)(_rng);
						_shots.push_back(i);
						const double chance = L.aiHitWorst + (L.aiHitBest - L.aiHitWorst) * l.skill;
						const bool   hit = std::uniform_real_distribution<double>(0.0, 1.0)(_rng) < chance;
						_shotHits.push_back(hit);
						if (hit) {
							Hit(i);
						}
					}
				}
			}

			bool Over() const override { return _place > 0 && _ending <= 0.0; }

			void Pieces(std::vector<Piece>& a_out) const override
			{
				const auto& L = layout::Zapper();
				for (int i = 0; i < static_cast<int>(_lanes.size()); ++i) {
					Piece p;
					p.model = 0;
					p.pos = L.lanes[i].rocketBottom + (L.lanes[i].rocketTop - L.lanes[i].rocketBottom) * _lanes[i].shown;
					a_out.push_back(p);
				}
			}

			void Hud(HudInfo& a_out) const override
			{
				if (_countdown > 0.0) {
					a_out.line1 = std::format("READY... {}", static_cast<int>(std::ceil(_countdown)));
					a_out.clock = std::to_string(static_cast<int>(std::ceil(_countdown)));
				} else if (_place > 0 && _you >= 0) {
					static constexpr std::array<const char*, 7> ord{ "", "1ST", "2ND", "3RD", "4TH", "5TH", "6TH" };
					a_out.line1 = _place == 1 ? std::string("YOU WIN!") : std::format("YOU CAME {}", ord[std::min<std::size_t>(_place, 6)]);
				} else if (_you >= 0) {
					a_out.line1 = std::format("HITS {} OF {}", _lanes[_you].hits, layout::Zapper().hitsToWin);
				}
				a_out.line2 = "SQUIRT YOUR TARGET: FIRST ROCKET UP WINS";
				if (_winner >= 0 && _you >= 0) {
					a_out.verdict = _winner == _you ? std::string("YOU WON!") : std::format("YOU LOST · {} WON", _lanes[_winner].name);
				}
				for (const auto& l : _lanes) {
					if (!l.empty) {
						a_out.race.emplace_back(l.name, l.shown);
					}
				}
			}

			int TicketsFor(int a_score) const override { return TicketTable{ 400, 1, 10, 0, 0 }.For(a_score); }  // a ticket every 4 hits: a win (4000) still 10

			void SetRacers(int a_you, const std::vector<double>& a_skill, const std::vector<std::string>& a_names)
			{
				_you = a_you >= 0 && a_you < static_cast<int>(_lanes.size()) ? a_you : -1;
				for (int i = 0; i < static_cast<int>(_lanes.size()); ++i) {
					auto& l = _lanes[i];
					if (i == _you) {
						l.name = "YOU";
						l.empty = false;
						continue;
					}
					const double sk = i < static_cast<int>(a_skill.size()) ? a_skill[i] : -1.0;
					l.empty = sk < 0.0;
					l.skill = std::clamp(sk, 0.0, 1.0);
					if (i < static_cast<int>(a_names.size()) && !a_names[i].empty()) {
						l.name = a_names[i];
					}
				}
			}

			std::vector<int> TakeShots()
			{
				_shotHits.clear();
				return std::exchange(_shots, {});
			}
			std::vector<std::pair<int, bool>> TakeShotsHit()
			{
				std::vector<std::pair<int, bool>> out;
				for (std::size_t i = 0; i < _shots.size(); ++i) {
					out.emplace_back(_shots[i], i < _shotHits.size() && _shotHits[i]);
				}
				_shots.clear();
				_shotHits.clear();
				return out;
			}
			int              Winner() const { return _winner; }

		private:
			void Hit(int a_lane)
			{
				const auto& L = layout::Zapper();
				auto&       l = _lanes[a_lane];
				++l.hits;
				if (a_lane == _you) {
					Add(L.pointsPerHit);
					Say(Cue::kTargetHit, L.lanes[a_lane].target, 0.8);
				} else if (_you < 0) {
					// (settlers only: the score's the leader's)
					_score = std::max(_score, l.hits * L.pointsPerHit);
					Say(Cue::kTargetHit, L.lanes[a_lane].target, 0.5);
				}
				if (l.hits >= L.hitsToWin) {
					_winner = a_lane;
					if (_you < 0) {
						_place = 1;
						Add(L.winPoints);
						Say(Cue::kWin, L.lanes[a_lane].rocketTop);
						return;
					}
					// the race is over: you're placed by how many hits you had
					int ahead = 0;
					for (int i = 0; i < static_cast<int>(_lanes.size()); ++i) {
						if (i != _you && _lanes[i].hits > _lanes[_you].hits) {
							++ahead;
						}
					}
					_place = a_lane == _you ? 1 : std::max(2, ahead + 1);
					if (_place == 1) {
						Add(L.winPoints);
						Say(Cue::kWin, L.lanes[a_lane].rocketTop);
					} else {
						Say(Cue::kEnd, L.lanes[a_lane].rocketTop);
					}
				}
			}

			struct Lane
			{
				std::string name;
				double      skill{ 0.5 };
				int         hits{ 0 };
				double      shown{ 0.0 };  // the rocket's height, catching up with the hits
				double      next{ 0.5 };
				bool        empty{ false };  // nobody racing this lane
			};

			std::vector<Lane> _lanes;
			int               _you{ 0 };
			int               _place{ 0 };
			double            _ending{ 2.0 };
			double            _countdown{ 3.0 };
			double            _cool{ 0.0 };
			std::vector<int>  _shots;
			std::vector<bool> _shotHits;  // ...each a hit or not
			int               _winner{ -1 };
		};

		// ======================================================================== Bandit Roundup (the shooting gallery)

		class Gallery final : public Game
		{
		public:
			GameId Id() const override { return GameId::kBanditRoundup; }

			void Start(std::uint32_t a_seed) override
			{
				_rng.seed(a_seed);
				const auto& L = layout::Gallery();
				_targets.clear();
				for (int r = 0; r < static_cast<int>(L.rows.size()); ++r) {
					const auto& row = L.rows[r];
					const double span = L.right - L.left;
					for (int k = 0; k < row.count; ++k) {
						Target t;
						t.row = r;
						t.x = L.left + span * (k + 0.5) / row.count;
						t.kind = PickKind();
						_targets.push_back(t);
					}
				}
				_time = L.seconds;
				_cool = { 0.0, 0.0 };
				_score = 0;
				_rival = 0;
				_shots = { 0, 0 };
				_hits = { 0, 0 };
				_vanilla = { 0, 0 };
				_bullseyes = 0;
				_settled = false;
				_ending = 1.5;
				_hurried = false;
				Say(Cue::kStart, { 0, L.rows.empty() ? 0.0 : L.rows[0].y, L.rows.empty() ? 0.0 : L.rows[0].z });
			}

			void Update(double a_dt, const Input& a_in) override
			{
				const auto& L = layout::Gallery();
				sinceLastPoints += a_dt;
				if (_time > 0.0) {
					_time = std::max(0.0, _time - a_dt);
					if (!_hurried && _time <= 10.0) {
						_hurried = true;
						Say(Cue::kHurry, {});
					}
					if (_time <= 0.0) {
						Say(Cue::kEnd, {});
						// a 1v1 won: 1000 more (as a zapper race won)
						if (_versus && !_settled) {
							_settled = true;
							if (_score > _rival) {
								Add(1000);
							} else if (_rival > _score) {
								_rival += 1000;
							}
						}
					}
				} else {
					_ending -= a_dt;
				}
				const double span = L.right - L.left;
				// faster in the last ten seconds
				const double rush = _time <= 10.0 ? 1.35 : 1.0;
				for (auto& t : _targets) {
					const auto& row = L.rows[t.row];
					t.x += row.speed * rush * a_dt;
					// round the end of the track: back up, a new face
					if (t.x > L.right) {
						t.x -= span;
						Reset(t);
					} else if (t.x < L.left) {
						t.x += span;
						Reset(t);
					}
					if (t.down) {
						t.flip = std::min(1.0, t.flip + a_dt / L.flipSeconds);
					}
				}
				const int who = _versus && a_in.who == 1 ? 1 : 0;
				_cool[0] -= a_dt;
				_cool[1] -= a_dt;
				if (a_in.pressed && _time > 0.0 && _cool[who] <= 0.0) {
					_cool[who] = L.shotSeconds;
					++_shots[who];
					// the nearest upright target the shot passes through
					int    best = -1;
					double bestT = 1e30;
					Vec3   bestAt;
					for (int i = 0; i < static_cast<int>(_targets.size()); ++i) {
						const auto& t = _targets[i];
						if (t.down) {
							continue;
						}
						const auto& row = L.rows[t.row];
						const Vec3  c{ t.x, row.y, row.z + L.kinds[t.kind].height };
						Vec3        at;
						if (!RayPlane(a_in, c, { 0, -1, 0 }, at)) {
							continue;
						}
						if (std::hypot(at.x - c.x, at.z - c.z) <= L.kinds[t.kind].radius) {
							const double d = (at - a_in.eye).Length();
							if (d < bestT) {
								bestT = d;
								best = i;
								bestAt = at;
							}
						}
					}
					if (best >= 0) {
						auto& t = _targets[best];
						t.down = true;
						t.flip = 0.0;
						++_hits[who];
						// dead centre of a target's bullseye: worth more
						const auto& k = L.kinds[t.kind];
						const auto& r = L.rows[t.row];
						const bool  bull = r.bullseyes && std::hypot(bestAt.x - (t.x + k.bullX), bestAt.z - (r.z + k.bullZ)) <= L.deadCentre;
						if (bull) {
							++_bullseyes;
						}
						const int pts = bull ? L.bullseyePoints : k.points;
						_vanilla[who] += k.vanilla;  // (the tickets: what Nuka-World's gallery scores it)
						if (who == 0) {
							Add(pts);
						} else {
							_rival += pts;
						}
						const auto& row = L.rows[t.row];
						Say(Cue::kTargetHit, { t.x, row.y, row.z });
					}
				}
			}

			bool Over() const override { return _time <= 0.0 && _ending <= 0.0; }

			void Pieces(std::vector<Piece>& a_out) const override
			{
				const auto& L = layout::Gallery();
				for (const auto& t : _targets) {
					const auto& row = L.rows[t.row];
					Piece       p;
					p.model = t.kind;
					p.pos = { t.x, row.y, row.z };
					// facing you (both rows: turned round, they'd show you their backs), flipped back on its hinge when hit
					RotX(-t.flip * kPi * 0.5, p.rot);
					a_out.push_back(p);
				}
			}

			void Hud(HudInfo& a_out) const override
			{
				a_out.timeLeft = _time;
				if (_versus) {
					a_out.line1 = std::format("{} {}   {} {}", _names[0], _score, _names[1], _rival);
					a_out.scores = { { _names[0], _score }, { _names[1], _rival } };
					a_out.line2 = _shots[0] > 0 ? std::format("{} SHOTS, {}% HIT", _shots[0], _hits[0] * 100 / _shots[0]) : std::string("FIRE AT THE TARGETS");
					if (_time <= 0.0) {
						a_out.verdict = _score == _rival ? std::string("A TIE") :
						                _score > _rival  ? (_names[0] == "YOU" ? std::string("YOU WON!") : std::format("{} WON", _names[0])) :
						                                   (_names[0] == "YOU" ? std::format("YOU LOST · {} WON", _names[1]) : std::format("{} WON", _names[1]));
					}
					return;
				}
				a_out.line1 = std::format("BANDITS {}", _hits[0]);
				a_out.line2 = _shots[0] > 0 ? std::format("{} SHOTS, {}% HIT", _shots[0], _hits[0] * 100 / _shots[0]) : std::string("FIRE AT THE TARGETS");
			}

			void SetVersus(const std::string& a_name0, const std::string& a_name1)
			{
				_versus = true;
				_names = { a_name0, a_name1 };
			}
			int  ScoreOf(int a_who) const { return a_who == 1 ? _rival : _score; }
			bool Versus() const { return _versus; }

			// Nuka-World's rate: a ticket every 200 of its points, what it scores each target hit (the bunny 100 to the
			// bottle 300); not from our score (100 a hit), so a bullseye's more points, not more tickets. Yours (side 0)
			int TicketsFor(int) const override { return TicketTable{ 200, 1, 100, 0, 0 }.For(_vanilla[0]); }

		private:
			struct Target
			{
				int    row{ 0 };
				int    kind{ 0 };
				double x{ 0.0 };
				double flip{ 0.0 };
				bool   down{ false };
			};

			int PickKind()
			{
				const auto&                        L = layout::Gallery();
				std::vector<double>                w;
				for (const auto& k : L.kinds) {
					w.push_back(k.weight);
				}
				return std::discrete_distribution<int>(w.begin(), w.end())(_rng);
			}

			void Reset(Target& a_t)
			{
				a_t.down = false;
				a_t.flip = 0.0;
				a_t.kind = PickKind();
			}

			std::vector<Target>        _targets;
			double                     _time{ 0.0 };
			std::array<double, 2>      _cool{};
			double                     _ending{ 1.5 };
			std::array<int, 2>         _shots{};
			std::array<int, 2>         _hits{};
			std::array<int, 2>         _vanilla{};  // each one's hits at Nuka-World's values (the tickets)
			int                        _bullseyes{ 0 };
			bool                       _hurried{ false };
			bool                       _versus{ false };
			bool                       _settled{ false };
			int                        _rival{ 0 };  // the other's score (1v1)
			std::array<std::string, 2> _names;
		};
	}

	const char* GameName(GameId a_id)
	{
		switch (a_id) {
		case GameId::kAtomicRollers: return "ATOMIC ROLLERS";
		case GameId::kHoopShot: return "HOOP SHOT";
		case GameId::kWhacACommie: return "WHAC-A-COMMIE";
		case GameId::kNukaZapperRace: return "NUKA-ZAPPER RACE";
		case GameId::kBanditRoundup: return "BANDIT ROUNDUP";
		default: return "?";
		}
	}

	int TicketTable::For(int a_score) const
	{
		int n = perTicket > 0 ? a_score / perTicket : 0;
		if (jackpot > 0 && a_score >= jackpot) {
			n += jackpotBonus;
		}
		return std::clamp(n, min, std::max(min, max));
	}

	void SetWhacUpScale(Game& a_game, double a_scale)
	{
		if (auto* w = dynamic_cast<Whac*>(&a_game)) {
			w->SetUpScale(a_scale);
		}
	}

	double WhacHeight(const Game& a_game, int a_head)
	{
		const auto* w = dynamic_cast<const Whac*>(&a_game);
		return w ? w->HeightOf(a_head) : 0.0;
	}

	void SetZapperRacers(Game& a_game, int a_you, const std::vector<double>& a_skill, const std::vector<std::string>& a_names)
	{
		if (auto* z = dynamic_cast<Zapper*>(&a_game)) {
			z->SetRacers(a_you, a_skill, a_names);
		}
	}

	void SetGalleryVersus(Game& a_game, const std::string& a_name0, const std::string& a_name1)
	{
		if (auto* g = dynamic_cast<Gallery*>(&a_game)) {
			g->SetVersus(a_name0, a_name1);
		}
	}

	int GalleryScore(const Game& a_game, int a_who)
	{
		const auto* g = dynamic_cast<const Gallery*>(&a_game);
		return g ? g->ScoreOf(a_who) : (a_who == 0 ? a_game.Score() : 0);
	}

	bool GalleryVersus(const Game& a_game)
	{
		const auto* g = dynamic_cast<const Gallery*>(&a_game);
		return g && g->Versus();
	}

	std::vector<int> TakeZapperShots(Game& a_game)
	{
		auto* z = dynamic_cast<Zapper*>(&a_game);
		return z ? z->TakeShots() : std::vector<int>{};
	}

	std::vector<std::pair<int, bool>> TakeZapperShotsHit(Game& a_game)
	{
		auto* z = dynamic_cast<Zapper*>(&a_game);
		return z ? z->TakeShotsHit() : std::vector<std::pair<int, bool>>{};
	}

	int ZapperWinner(const Game& a_game)
	{
		const auto* z = dynamic_cast<const Zapper*>(&a_game);
		return z ? z->Winner() : -1;
	}

	Thrower::Thrower(std::uint32_t a_seed, double a_skill, GameId a_game) :
		_rng(a_seed), _skill(std::clamp(a_skill, 0.0, 1.0)), _game(a_game)
	{
		Plan();
	}

	void Thrower::Plan()
	{
		std::normal_distribution<double> n(0.0, 1.0);
		const double                     off = 1.0 - _skill;
		if (_game == GameId::kHoopShot) {
			const auto& L = layout::Hoop();
			_at = L.rim + Vec3{ n(_rng) * 4.0 * off, 0.0, n(_rng) * 3.0 * off };
			_charge = std::max(0.05, 0.34 * (1.0 + n(_rng) * (0.04 + 0.12 * off)));
		} else {
			const auto& L = layout::Rollers();
			_at = Vec3{ n(_rng) * (2.0 + 10.0 * off), L.release.y + 40.0, L.release.z };
			_charge = L.chargeSeconds * std::uniform_real_distribution<double>(0.3, 0.9)(_rng);
		}
	}

	Vec3 Thrower::Eye() const
	{
		if (_game == GameId::kHoopShot) {
			return { 0.0, -175.0, 120.0 };
		}
		const auto& L = layout::Rollers();
		return { 0.0, L.release.y - 40.0, L.release.z + 60.0 };
	}

	Input Thrower::Next(double a_dt, bool a_ready)
	{
		Input in;
		in.eye = Eye();
		if (_held < 0.0) {
			if (a_ready) {
				_wait -= a_dt;
			} else {
				// (the game's busy: the last ball not counted yet, or over. Atomic Rollers: the next roll straight
				// after it's counted)
				_wait = std::max(_wait, _game == GameId::kHoopShot ? 1.0 : std::max(0.0, _lead - _charge));
			}
			if (_wait > 0.0 || !a_ready) {
				in.dir = (_game == GameId::kHoopShot ? layout::Hoop().rim : Vec3{ 0.0, layout::Rollers().release.y + 40.0, layout::Rollers().release.z }) - in.eye;
				in.dir = in.dir.Normalized();
			} else {
				// the throw planned: pressed, held _charge, let go
				_held = 0.0;
				in.pressed = in.held = true;
				in.dir = (_at - in.eye).Normalized();
			}
		} else if (_held < _charge) {
			_held += a_dt;
			in.held = true;
			in.dir = (_at - in.eye).Normalized();
		} else {
			in.released = true;
			in.dir = (_at - in.eye).Normalized();
			if (_game == GameId::kHoopShot) {
				in.hasFrom = true;  // from her hands, where the shot lets go
				in.from = layout::SettlerRelease(_game);
			}
			_held = -1.0;
			Plan();
			// (Atomic Rollers: the next once the last ball's counted, as soon as its animation can be under way)
			_wait = _game == GameId::kHoopShot ? 2.2 : std::max(0.0, _lead - _charge);
			_cued = false;
		}
		// the animation's cue: _lead before the release
		const double toRelease = _held < 0.0 ? std::max(0.0, _wait) + _charge : _charge - _held;
		if (!_cued && !in.released && (a_ready || _held >= 0.0) && toRelease <= _lead) {  // (none while the game's busy)
			_cued = _cue = true;
		}
		return in;
	}

	WhacPlayer::WhacPlayer(std::uint32_t a_seed, double a_skill, bool a_scores) :
		_rng(a_seed), _skill(std::clamp(a_skill, 0.0, 1.0)), _scores(a_scores)
	{}

	bool WhacPlayer::Update(Game& a_game, double a_dt)
	{
		const auto& L = layout::Whac();
		std::vector<Piece> pieces;
		a_game.Pieces(pieces);
		const auto height = [&](int a_i) {
			if (a_i < 0 || a_i >= static_cast<int>(pieces.size()) || a_i >= static_cast<int>(L.holes.size()) || pieces[a_i].hidden) {
				return 0.0;
			}
			return (pieces[a_i].pos.z - L.holes[a_i].z - L.headDown) / (L.headUp - L.headDown);
		};
		std::uniform_real_distribution<double> u(0.0, 1.0);
		if (_swing >= 0.0) {
			_swing -= a_dt;
			if (_swing < 0.0) {
				// the mallet comes down: on the head if they're good enough and it's still up
				if (u(_rng) < 0.6 + 0.35 * _skill && _scores) {
					a_game.OnPieceHit(_target);
				}
				_target = -1;
				_cool = 0.2;
			}
			return false;
		}
		if (_cool > 0.0) {
			_cool -= a_dt;
			return false;
		}
		if (_target >= 0 && height(_target) < 0.4) {
			_target = -1;  // (gone back down before they got to it)
		}
		if (_target < 0) {
			std::vector<int> up;
			for (int i = 0; i < static_cast<int>(pieces.size()); ++i) {
				if (height(i) > 0.6) {
					up.push_back(i);
				}
			}
			if (up.empty()) {
				return false;
			}
			_target = up[std::uniform_int_distribution<std::size_t>(0, up.size() - 1)(_rng)];
			_react = 0.4 - 0.25 * _skill + 0.1 * u(_rng);
			return false;
		}
		_react -= a_dt;
		if (_react > 0.0) {
			return false;
		}
		_swing = L.swingSeconds + 0.12;  // (the swing's down this long after it starts)
		return true;
	}

	GalleryShooter::GalleryShooter(std::uint32_t a_seed, double a_spread) :
		_rng(a_seed), _spread(a_spread)
	{}

	Input GalleryShooter::Next(const Game& a_game, const Vec3& a_eye, double a_dt)
	{
		Input in;
		in.eye = a_eye;
		in.dir = Vec3{ 0, 1, 0 };
		_wait -= a_dt;
		std::vector<Piece> pieces;
		a_game.Pieces(pieces);
		const auto& L = layout::Gallery();
		// the upright target nearest the middle, in front of you
		const Piece* pick = nullptr;
		for (const auto& p : pieces) {
			if (p.hidden || p.rot[2][2] < 0.999 || std::abs(p.pos.x - a_eye.x) > 150.0) {
				continue;
			}
			if (!pick || std::abs(p.pos.x - a_eye.x) < std::abs(pick->pos.x - a_eye.x)) {
				pick = &p;
			}
		}
		if (!pick) {
			return in;
		}
		const auto& k = L.kinds[pick->model];
		const Vec3  at = pick->pos + Vec3{ k.bullX, 0, k.bullZ };
		in.dir = (at - a_eye).Normalized();
		if (_wait <= 0.0) {
			std::normal_distribution<double> n(0.0, _spread);
			const Vec3 right = in.dir.Cross({ 0, 0, 1 }).Normalized();
			const Vec3 up = right.Cross(in.dir).Normalized();
			in.dir = (in.dir + right * n(_rng) + up * n(_rng)).Normalized();
			in.pressed = true;
			_wait = std::uniform_real_distribution<double>(0.9, 1.8)(_rng);
		}
		return in;
	}

	std::unique_ptr<Game> MakeGame(GameId a_id)
	{
		switch (a_id) {
		case GameId::kAtomicRollers: return std::make_unique<Rollers>();
		case GameId::kHoopShot: return std::make_unique<Hoop>();
		case GameId::kWhacACommie: return std::make_unique<Whac>();
		case GameId::kNukaZapperRace: return std::make_unique<Zapper>();
		case GameId::kBanditRoundup: return std::make_unique<Gallery>();
		default: return nullptr;
		}
	}

	void RotZ(double a_angle, double a_out[3][3])
	{
		const double c = std::cos(a_angle), s = std::sin(a_angle);
		a_out[0][0] = c;
		a_out[0][1] = s;
		a_out[0][2] = 0;
		a_out[1][0] = -s;
		a_out[1][1] = c;
		a_out[1][2] = 0;
		a_out[2][0] = 0;
		a_out[2][1] = 0;
		a_out[2][2] = 1;
	}

	void RotX(double a_angle, double a_out[3][3])
	{
		const double c = std::cos(a_angle), s = std::sin(a_angle);
		a_out[0][0] = 1;
		a_out[0][1] = 0;
		a_out[0][2] = 0;
		a_out[1][0] = 0;
		a_out[1][1] = c;
		a_out[1][2] = s;
		a_out[2][0] = 0;
		a_out[2][1] = -s;
		a_out[2][2] = c;
	}
}
