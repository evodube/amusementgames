#include "sim/Arcade.h"

#include "sim/Layouts.h"

#include <algorithm>
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
					// across the lane where you look (the guide), charged while held, rolled on release
					{
						Vec3 at;
						if (RayPlane(a_in, L.release + Vec3{ 0, 40, 0 }, { 0, -1, 0 }, at)) {
							_aim = std::clamp(at.x / L.aimHalfWidth, -1.0, 1.0);
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
						if (b.state == BallState::kFlying && (b.still > 0.8 || b.age > 9.0 || b.pos.z < -50.0)) {
							b.state = BallState::kCaught;  // stuck or lost: no points
							_last = 0;
							Add(0);
						}
						if (b.state == BallState::kCaught) {
							_phase = Phase::kBetween;
							_wait = 0.6;
						}
					}
					break;
				case Phase::kBetween:
					_wait -= a_dt;
					if (_wait <= 0.0) {
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
				a_out.aimX = _aim;
				a_out.showAim = _phase == Phase::kAim;
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
				b.pos = L.release + Vec3{ _aim * L.aimHalfWidth * 0.5, 0, 0 };
				b.vel = {};
			}

			void Roll(double a_power)
			{
				const auto&  L = layout::Rollers();
				const double speed = L.minSpeed + (L.maxSpeed - L.minSpeed) * a_power;
				// up the lane, turned toward the aim point (across the lane at the ramp)
				const Vec3 from = _world.balls[_ball].pos;
				const Vec3 to{ _aim * L.aimHalfWidth, L.rampStartY, from.z };
				Vec3       d = (to - from).Normalized();
				d.z = L.laneSlope;  // along the lane's rise
				d = d.Normalized();
				_world.Launch(_ball, from, d * speed);
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
			double _aim{ 0.0 };
			int    _last{ -1 };
			Charge _charge;
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
				_world.spec.ballRestitution = 0.5;
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

			int TicketsFor(int a_score) const override { return TicketTable{ 2, 1, 40, 0, 0 }.For(a_score); }  // a ticket every 2 hits (Nuka-World's rate)

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
				// from your hand toward where you look (past the hand, at the distance of the hoop)
				const Vec3 from = Hand(a_in.eye, a_in.dir);
				const Vec3 aimAt = a_in.eye + a_in.dir * std::max(60.0, (L.rim - a_in.eye).Length());
				Vec3       toward = (aimAt - from).Normalized();
				toward.z += L.arcLift;
				_world.Launch(free, from, toward.Normalized() * speed);
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
				_swing = std::max(0.0, _swing - a_dt);
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
				const double upFor = L.upLongest + (L.upShortest - L.upLongest) * k;
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
				// a whack: at the head nearest where you look on the top, if it's up far enough
				if (a_in.pressed && _time > 0.0 && _swing <= 0.0) {
					_swing = L.swingSeconds;
					{
						// the head the look passes nearest (within its size), if it's up far enough
						int    best = -1;
						double bestD = L.hitRadius;
						for (int i = 0; i < static_cast<int>(_heads.size()); ++i) {
							if (Height(i) <= 0.35) {
								continue;
							}
							const Vec3   c = HeadAt(i) + Vec3{ 0, 0, -9.0 };  // the face (the model hangs below its origin)
							const double t = std::max(0.0, (c - a_in.eye).Dot(a_in.dir));
							const double d = (a_in.eye + a_in.dir * t - c).Length();
							if (d < bestD) {
								best = i;
								bestD = d;
							}
						}
						if (best >= 0) {
							auto& h = _heads[best];
							Add(L.points);
							++_hits;
							h = { Head::kHit, 0.0, h.upFor };
							Say(Cue::kWhack, L.holes[best]);
						}
					}
				}
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
				a_out.line1 = std::format("COMMIES WHACKED {}", _hits);
				a_out.line2 = std::format("GOT AWAY {}", _misses);
			}

			int TicketsFor(int a_score) const override { return TicketTable{ 2, 1, 40, 0, 0 }.For(a_score); }  // a ticket every 2 hits (Nuka-World's rate)

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
			double            _swing{ 0.0 };
			double            _ending{ 1.5 };
			int               _hits{ 0 };
			int               _misses{ 0 };
			bool              _hurried{ false };
		};

		// ======================================================================== Nuka-Zapper Race
		// Hold the button with your stream on your target and your rocket climbs; ten steps up (Nuka-World's ten hits)
		// and you've won. The other lanes race you, their aim wandering about their targets. Every lane's stream is a
		// solid one: the game's own drops, close together, each flying from the gun to where it's aimed

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
					_lanes[i].skill = std::uniform_real_distribution<double>(0.45, 1.0)(_rng);
				}
				_you = -1;
				_place = 0;
				_score = 0;
				_ending = 2.0;
				_countdown = 3.0;
				_drops.clear();
				Say(Cue::kStart, L.lanes.empty() ? Vec3{} : L.lanes[0].target);
			}

			void Update(double a_dt, const Input& a_in) override
			{
				const auto& L = layout::Zapper();
				sinceLastPoints += a_dt;
				if (_you < 0) {
					// your lane: the gun nearest you
					double best = 1e30;
					for (int i = 0; i < static_cast<int>(L.lanes.size()); ++i) {
						const double d = std::abs(a_in.eye.x - L.lanes[i].target.x);
						if (d < best) {
							best = d;
							_you = i;
						}
					}
					if (_you >= 0) {
						_lanes[_you].name = "YOU";
					}
				}
				for (auto& d : _drops) {
					d.t += a_dt;
				}
				std::erase_if(_drops, [](const Drop& a_d) { return a_d.t >= a_d.flight; });
				if (_countdown > 0.0) {
					_countdown -= a_dt;
					return;
				}
				const bool racing = _place == 0;
				if (!racing) {
					_ending -= a_dt;
				}
				for (int i = 0; i < static_cast<int>(_lanes.size()); ++i) {
					auto&       lane = _lanes[i];
					const auto& g = L.lanes[i];
					bool        firing = false;
					bool        on = false;
					Vec3        from = g.gun;
					Vec3        aim;
					if (i == _you) {
						firing = a_in.held && racing;
						// out of your Thirst Zapper, toward where you look (on the target's wall)
						const Vec3 right = a_in.dir.Cross({ 0, 0, 1 }).Normalized();
						from = a_in.eye + a_in.dir * L.muzzleForward + right * L.muzzleSide + Vec3{ 0, 0, -L.muzzleDrop };
						if (!RayPlane(a_in, g.target, { 0, -1, 0 }, aim)) {
							aim = a_in.eye + a_in.dir * 200.0;
						}
						on = firing && std::hypot(aim.x - g.target.x, aim.z - g.target.z) <= L.targetRadius;
					} else {
						// the others: an aim that wanders about the target, steadier the better they are
						firing = racing;
						const double spread = L.targetRadius * (2.0 - 1.4 * lane.skill);
						std::normal_distribution<double> n(0.0, 1.0);
						const double pull = 2.5;
						lane.wx += (-lane.wx * pull) * a_dt + n(_rng) * spread * std::sqrt(a_dt) * 2.0;
						lane.wz += (-lane.wz * pull) * a_dt + n(_rng) * spread * std::sqrt(a_dt) * 2.0;
						aim = g.target + Vec3{ lane.wx, 0, lane.wz };
						on = std::hypot(lane.wx, lane.wz) <= L.targetRadius;
					}
					if (on && racing) {
						lane.progress = std::min(1.0, lane.progress + a_dt / L.secondsToWin);
						const int steps = static_cast<int>(lane.progress * L.steps + 1e-9);
						if (steps > lane.steps) {
							const int up = steps - lane.steps;
							lane.steps = steps;
							if (i == _you) {
								Add(up * L.pointsPerStep);
								Say(Cue::kTargetHit, g.target, 0.6);
							}
						}
						if (lane.progress >= 1.0) {
							Finish(i);
						}
					}
					// the stream
					if (firing) {
						if (i == _you) {
							Say(Cue::kSquirt, from, 0.7);  // (the plugin spaces these out)
						}
						lane.dropIn -= a_dt;
						while (lane.dropIn <= 0.0) {
							lane.dropIn += L.dropEvery;
							const double flight = std::max(0.05, (aim - from).Length() / L.streamSpeed);
							if (static_cast<int>(_drops.size()) < L.maxDrops) {
								_drops.push_back({ from, aim, 0.0, flight });
							}
						}
					} else {
						lane.dropIn = 0.0;
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
					p.pos = L.lanes[i].rocketBottom + (L.lanes[i].rocketTop - L.lanes[i].rocketBottom) * _lanes[i].progress;
					a_out.push_back(p);
				}
				// the drops: the fixed number the plugin keeps (unused ones hidden)
				for (int k = 0; k < L.maxDrops; ++k) {
					Piece p;
					p.model = 1;
					p.scale = L.dropScale;
					if (k < static_cast<int>(_drops.size())) {
						const auto&  d = _drops[k];
						const double u = d.t / d.flight;
						// a little droop on the way
						p.pos = d.from + (d.to - d.from) * u + Vec3{ 0, 0, -0.5 * 686.0 * d.t * d.t * 0.15 };
					} else {
						p.hidden = true;
					}
					a_out.push_back(p);
				}
			}

			void Hud(HudInfo& a_out) const override
			{
				if (_countdown > 0.0) {
					a_out.line1 = std::format("READY... {}", static_cast<int>(std::ceil(_countdown)));
				} else if (_place > 0) {
					static constexpr std::array<const char*, 7> ord{ "", "1ST", "2ND", "3RD", "4TH", "5TH", "6TH" };
					a_out.line1 = _place == 1 ? std::string("YOU WIN!") : std::format("YOU CAME {}", ord[std::min<std::size_t>(_place, 6)]);
				} else {
					a_out.line1 = "HOLD FIRE ON YOUR TARGET";
				}
				a_out.line2 = "FIRST ROCKET TO THE TOP WINS";
				for (const auto& l : _lanes) {
					a_out.race.emplace_back(l.name, l.progress);
				}
			}

			int TicketsFor(int a_score) const override { return TicketTable{ 2, 1, 10, 0, 0 }.For(a_score); }  // a ticket every 2 points (Nuka-World's rate)

		private:
			void Finish(int a_lane)
			{
				const auto& L = layout::Zapper();
				if (_place > 0) {
					return;
				}
				int ahead = 0;
				for (int i = 0; i < static_cast<int>(_lanes.size()); ++i) {
					if (i != _you && _you >= 0 && _lanes[i].progress > _lanes[_you].progress) {
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

			struct Lane
			{
				std::string name;
				double      skill{ 0.8 };
				double      progress{ 0.0 };
				int         steps{ 0 };
				double      wx{ 0.0 };
				double      wz{ 0.0 };
				double      dropIn{ 0.0 };
			};
			struct Drop
			{
				Vec3   from;
				Vec3   to;
				double t{ 0.0 };
				double flight{ 0.25 };
			};

			std::vector<Lane> _lanes;
			std::vector<Drop> _drops;
			int               _you{ -1 };
			int               _place{ 0 };
			double            _ending{ 2.0 };
			double            _countdown{ 3.0 };
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
				_cool = 0.0;
				_score = 0;
				_shots = 0;
				_hits = 0;
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
				_cool -= a_dt;
				if (a_in.pressed && _time > 0.0 && _cool <= 0.0) {
					_cool = L.shotSeconds;
					++_shots;
					// the nearest upright target the shot passes through
					int    best = -1;
					double bestT = 1e30;
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
							}
						}
					}
					if (best >= 0) {
						auto& t = _targets[best];
						t.down = true;
						t.flip = 0.0;
						++_hits;
						Add(L.kinds[t.kind].points);
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
					// facing you, flipped back on its hinge when hit; turned to face the way it goes
					RotX(-t.flip * kPi * 0.5, p.rot);
					if (row.speed < 0.0) {
						double flip[3][3];
						RotZ(kPi, flip);
						double m[3][3];
						for (int r = 0; r < 3; ++r) {
							for (int c = 0; c < 3; ++c) {
								m[r][c] = p.rot[r][0] * flip[0][c] + p.rot[r][1] * flip[1][c] + p.rot[r][2] * flip[2][c];
							}
						}
						std::copy(&m[0][0], &m[0][0] + 9, &p.rot[0][0]);
					}
					a_out.push_back(p);
				}
			}

			void Hud(HudInfo& a_out) const override
			{
				a_out.timeLeft = _time;
				a_out.line1 = std::format("BANDITS {}", _hits);
				a_out.line2 = _shots > 0 ? std::format("{} SHOTS, {}% HIT", _shots, _hits * 100 / _shots) : std::string("FIRE AT THE TARGETS");
			}

			int TicketsFor(int a_score) const override { return TicketTable{ 2, 1, 60, 0, 0 }.For(a_score); }  // a ticket every 2 hits (Nuka-World's rate)

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

			std::vector<Target> _targets;
			double              _time{ 0.0 };
			double              _cool{ 0.0 };
			double              _ending{ 1.5 };
			int                 _shots{ 0 };
			int                 _hits{ 0 };
			bool                _hurried{ false };
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
