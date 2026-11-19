#include "sim/Ball3D.h"

#include <algorithm>

namespace sim
{
	Quat Quat::operator*(const Quat& o) const
	{
		return { w * o.w - x * o.x - y * o.y - z * o.z, w * o.x + x * o.w + y * o.z - z * o.y, w * o.y - x * o.z + y * o.w + z * o.x,
			w * o.z + x * o.y - y * o.x + z * o.w };
	}

	void Quat::Normalize()
	{
		const double l = std::sqrt(w * w + x * x + y * y + z * z);
		if (l > 1e-12) {
			w /= l;
			x /= l;
			y /= l;
			z /= l;
		} else {
			*this = {};
		}
	}

	Quat Quat::FromAxisAngle(const Vec3& a_axis, double a_angle)
	{
		const Vec3   n = a_axis.Normalized();
		const double s = std::sin(a_angle * 0.5);
		return { std::cos(a_angle * 0.5), n.x * s, n.y * s, n.z * s };
	}

	void Quat::ToMatrix(double a_out[3][3]) const
	{
		a_out[0][0] = 1 - 2 * (y * y + z * z);
		a_out[0][1] = 2 * (x * y - w * z);
		a_out[0][2] = 2 * (x * z + w * y);
		a_out[1][0] = 2 * (x * y + w * z);
		a_out[1][1] = 1 - 2 * (x * x + z * z);
		a_out[1][2] = 2 * (y * z - w * x);
		a_out[2][0] = 2 * (x * z - w * y);
		a_out[2][1] = 2 * (y * z + w * x);
		a_out[2][2] = 1 - 2 * (x * x + y * y);
	}

	namespace
	{
		// closest point to p on the segment a-b
		Vec3 ClosestOnSegment(const Vec3& a_p, const Vec3& a_a, const Vec3& a_b)
		{
			const Vec3   ab = a_b - a_a;
			const double len2 = ab.Dot(ab);
			const double t = len2 > 1e-12 ? std::clamp((a_p - a_a).Dot(ab) / len2, 0.0, 1.0) : 0.0;
			return a_a + ab * t;
		}

		// closest point to p on a circle (centre c, axis n, radius r)
		Vec3 ClosestOnCircle(const Vec3& a_p, const Vec3& a_c, const Vec3& a_n, double a_r)
		{
			const Vec3 n = a_n.Normalized();
			Vec3       d = a_p - a_c;
			d -= n * d.Dot(n);  // into the circle's plane
			const double l = d.Length();
			if (l < 1e-9) {
				// on the axis: any point of the circle is as near; take one
				Vec3 any = std::abs(n.z) < 0.9 ? Vec3{ 0, 0, 1 }.Cross(n) : Vec3{ 1, 0, 0 }.Cross(n);
				return a_c + any.Normalized() * a_r;
			}
			return a_c + d * (a_r / l);
		}
	}

	bool World::AnyFlying() const
	{
		return std::ranges::any_of(balls, [](const Ball& a_b) { return a_b.state == BallState::kFlying; });
	}

	void World::Launch(int a_ball, const Vec3& a_pos, const Vec3& a_vel)
	{
		if (a_ball < 0 || a_ball >= static_cast<int>(balls.size())) {
			return;
		}
		auto& b = balls[a_ball];
		b.pos = a_pos;
		b.vel = a_vel;
		b.spin = {};
		b.state = BallState::kFlying;
		b.age = 0.0;
		b.still = 0.0;
		b.lastCatch = -1;
	}

	void World::Step(double a_dt, std::vector<Event>& a_events)
	{
		if (a_dt <= 0.0) {
			return;
		}
		a_dt = std::min(a_dt, 0.1);  // a hitch (a menu, a long frame): no more than a tenth of a second at once
		const int    n = std::max(1, static_cast<int>(std::ceil(a_dt / spec.substep)));
		const double h = a_dt / n;
		for (int i = 0; i < n; ++i) {
			SubStep(h, a_events);
		}
	}

	bool World::Contact(Ball& a_ball, int a_index, const Vec3& a_normal, double a_depth, double a_restitution, double a_friction,
		double a_rollDecel, int a_material, double a_h, std::vector<Event>& a_events)
	{
		const double r = spec.ballRadius;
		a_ball.pos += a_normal * a_depth;
		const double vn = a_ball.vel.Dot(a_normal);
		if (vn < 0.0) {
			const Vec3 vNormal = a_normal * vn;
			Vec3       vTangent = a_ball.vel - vNormal;
			// a bounce if it came in fast enough; otherwise it stays on the surface (rolling)
			const double bounce = -vn > 25.0 ? -vn * a_restitution : 0.0;
			if (-vn > 25.0) {
				vTangent = vTangent * (1.0 - a_friction);
				a_events.push_back({ Event::Kind::kHit, a_index, a_material, -vn, a_ball.pos - a_normal * r });
			}
			a_ball.vel = vTangent + a_normal * bounce;
		}
		// rolling: resistance along the surface, and the spin that goes with it
		Vec3         along = a_ball.vel - a_normal * a_ball.vel.Dot(a_normal);
		const double speed = along.Length();
		if (speed > 1e-9) {
			const double drop = std::min(speed, a_rollDecel * a_h);
			a_ball.vel -= along * (drop / speed);
			along = along * ((speed - drop) / speed);
		}
		a_ball.spin = a_normal.Cross(along) * (1.0 / r);
		return true;
	}

	void World::SubStep(double a_h, std::vector<Event>& a_events)
	{
		const double r = spec.ballRadius;
		_onSurface.assign(balls.size(), false);
		for (std::size_t i = 0; i < balls.size(); ++i) {
			auto& b = balls[i];
			if (b.state != BallState::kFlying) {
				continue;
			}
			b.age += a_h;
			const Vec3 before = b.pos;
			b.vel.z -= spec.gravity * a_h;
			b.vel = b.vel * std::max(0.0, 1.0 - spec.airDrag * a_h);
			const double sp = b.vel.Length();
			if (sp > spec.maxSpeed) {
				b.vel = b.vel * (spec.maxSpeed / sp);
			}
			b.pos += b.vel * a_h;

			// catches: the centre crossing the disc's plane against its normal, inside its ring
			for (const auto& c : catches) {
				const Vec3   n = c.normal.Normalized();
				const double d0 = (before - c.centre).Dot(n);
				const double d1 = (b.pos - c.centre).Dot(n);
				if (d0 >= 0.0 && d1 < 0.0) {
					const double t = d0 / (d0 - d1);
					const Vec3   hit = before + (b.pos - before) * t;
					Vec3         off = hit - c.centre;
					off -= n * off.Dot(n);
					const double rr = off.Length();
					if (rr <= c.radius - r * 0.25 && rr >= c.inner && b.lastCatch != c.id) {
						b.lastCatch = c.id;
						a_events.push_back({ Event::Kind::kCaught, static_cast<int>(i), c.id, b.vel.Length(), hit });
						if (!c.keep) {
							b.state = BallState::kCaught;
							break;
						}
					}
				}
			}
			if (b.state != BallState::kFlying) {
				continue;
			}

			bool supported = false;
			for (const auto& s : rects) {
				const Vec3   n = s.normal.Normalized();
				const Vec3   u = s.u.Normalized();
				const Vec3   v = n.Cross(u);
				const Vec3   d = b.pos - s.centre;
				const double du = d.Dot(u), dv = d.Dot(v);
				if (std::abs(du) > s.halfU + r * 0.5 || std::abs(dv) > s.halfV + r * 0.5) {
					continue;
				}
				bool open = false;
				for (const auto& [hc, hr] : s.holes) {
					Vec3 off = b.pos - hc;
					off -= n * off.Dot(n);
					if (off.Length() < hr) {
						open = true;
						break;
					}
				}
				if (open) {
					continue;
				}
				double dn = d.Dot(n);
				Vec3   face = n;
				if (s.twoSided && dn < 0.0) {
					dn = -dn;
					face = -n;
				}
				// in front within the radius, or its centre just through (a fast ball: no deeper than the radius; substeps
				// are short enough that it never goes further in one)
				if (dn < r && dn > -r) {
					Contact(b, static_cast<int>(i), face, r - dn, s.restitution, s.friction, s.rollDecel, s.material, a_h, a_events);
					if (face.z > 0.5) {
						supported = true;
					}
				}
			}
			for (const auto& rl : rails) {
				const Vec3   p = ClosestOnSegment(b.pos, rl.a, rl.b);
				const Vec3   d = b.pos - p;
				const double l = d.Length();
				const double min = r + rl.radius;
				if (l < min && l > 1e-9) {
					Contact(b, static_cast<int>(i), d * (1.0 / l), min - l, rl.restitution, 0.1, 4.0, rl.material, a_h, a_events);
				}
			}
			for (const auto& rm : rims) {
				const Vec3   p = ClosestOnCircle(b.pos, rm.centre, rm.axis, rm.radius);
				const Vec3   d = b.pos - p;
				const double l = d.Length();
				const double min = r + rm.tube;
				if (l < min && l > 1e-9) {
					const Vec3 n = d * (1.0 / l);
					Contact(b, static_cast<int>(i), n, min - l, rm.restitution, 0.15, 4.0, rm.material, a_h, a_events);
					if (n.z > 0.5) {
						supported = true;
					}
				}
			}
			_onSurface[i] = supported;

			// spin: integrate for the look of it
			const double w = b.spin.Length();
			if (w > 1e-9) {
				b.rot = Quat::FromAxisAngle(b.spin, w * a_h) * b.rot;
				b.rot.Normalize();
			}
			if (supported && b.vel.Length() < spec.restSpeed * 4.0) {
				b.still += a_h;
			} else {
				b.still = 0.0;
			}
		}

		// ball against ball
		for (std::size_t i = 0; i < balls.size(); ++i) {
			if (balls[i].state != BallState::kFlying) {
				continue;
			}
			for (std::size_t j = i + 1; j < balls.size(); ++j) {
				if (balls[j].state != BallState::kFlying) {
					continue;
				}
				auto&        a = balls[i];
				auto&        c = balls[j];
				const Vec3   d = c.pos - a.pos;
				const double l = d.Length();
				if (l >= 2.0 * r || l < 1e-9) {
					continue;
				}
				const Vec3 n = d * (1.0 / l);
				const double push = (2.0 * r - l) * 0.5;
				a.pos -= n * push;
				c.pos += n * push;
				const double vn = (c.vel - a.vel).Dot(n);
				if (vn < 0.0) {
					const double j2 = -(1.0 + spec.ballRestitution) * vn * 0.5;
					a.vel -= n * j2;
					c.vel += n * j2;
					if (-vn > 30.0) {
						a_events.push_back({ Event::Kind::kBalls, static_cast<int>(i), static_cast<int>(j), -vn, a.pos + n * r });
					}
				}
			}
		}
	}

	bool PredictToHeight(const Vec3& a_from, const Vec3& a_vel, double a_gravity, double a_z, Vec3& a_out)
	{
		// z(t) = z0 + vz t - g t^2 / 2 = a_z: the later root (on the way down)
		const double a = -0.5 * a_gravity, bq = a_vel.z, c = a_from.z - a_z;
		const double disc = bq * bq - 4.0 * a * c;
		if (disc < 0.0 || a_gravity <= 0.0) {
			return false;
		}
		const double t = (-bq - std::sqrt(disc)) / (2.0 * a);
		if (t <= 0.0) {
			return false;
		}
		a_out = { a_from.x + a_vel.x * t, a_from.y + a_vel.y * t, a_z };
		return true;
	}
}
