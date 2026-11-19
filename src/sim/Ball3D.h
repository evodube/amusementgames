#pragma once

// Ball physics for the arcade machines: Atomic Rollers' and Hoop Shot's balls. Plain C++ with no game dependencies,
// so it can be tested on its own (tests/ball3d_test.cpp), as Playable Pool's table physics is.
//
// Machine space: origin at the machine model's origin (its footprint centre on the floor), x to the player's right,
// y away from the player (into the machine), z up. Game units (about 70 a metre) and seconds.
//
// The machine is a few simple shapes: flat rectangles (lanes, ramps, the backboard, walls), rails (capsules: a lane's
// edge, a hoop's net frame), rims (circles of tube: a hoop, a ring on the skee-ball board) and catch discs (a hole or
// the hoop: a ball dropping through one is caught and scored). Fixed substeps; each contact pushes the ball out, takes
// some of its speed into the surface (restitution), rubs some of its sliding off (friction) and spins it to roll.

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace sim
{
	struct Vec3
	{
		double x{ 0.0 };
		double y{ 0.0 };
		double z{ 0.0 };

		constexpr Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
		constexpr Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
		constexpr Vec3 operator-() const { return { -x, -y, -z }; }
		constexpr Vec3 operator*(double s) const { return { x * s, y * s, z * s }; }
		Vec3&          operator+=(const Vec3& o)
		{
			x += o.x;
			y += o.y;
			z += o.z;
			return *this;
		}
		Vec3& operator-=(const Vec3& o)
		{
			x -= o.x;
			y -= o.y;
			z -= o.z;
			return *this;
		}
		constexpr double Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
		constexpr Vec3   Cross(const Vec3& o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
		double           Length() const { return std::sqrt(Dot(*this)); }
		Vec3             Normalized() const
		{
			const double l = Length();
			return l > 1e-12 ? *this * (1.0 / l) : Vec3{};
		}
	};

	// unit quaternion: a ball's orientation (for its spin on screen)
	struct Quat
	{
		double w{ 1.0 };
		double x{ 0.0 };
		double y{ 0.0 };
		double z{ 0.0 };

		Quat operator*(const Quat& o) const;
		void Normalize();
		static Quat FromAxisAngle(const Vec3& a_axis, double a_angle);
		// row-major 3x3 rotation matrix (rows: the ball's axes in machine space)
		void ToMatrix(double a_out[3][3]) const;
	};

	// A flat rectangle, solid on its front (the side its normal faces). centre, normal, u (one in-plane axis; the other is
	// normal x u), half sizes along u and the other axis.
	struct Rect
	{
		Vec3   centre;
		Vec3   normal{ 0.0, 0.0, 1.0 };
		Vec3   u{ 1.0, 0.0, 0.0 };
		double halfU{ 10.0 };
		double halfV{ 10.0 };
		double restitution{ 0.5 };
		double friction{ 0.2 };      // tangential speed lost per bounce (0..1)
		double rollDecel{ 6.0 };     // rolling resistance while resting on it, units/s^2
		int    material{ 0 };        // for the sounds
		bool   twoSided{ false };    // a thin board: solid both ways
		// round openings in it (a skee-ball board's rings): no surface within radius of each centre
		std::vector<std::pair<Vec3, double>> holes;
	};

	// A rod from a to b of the given radius (a rail, an edge, a net frame).
	struct Rail
	{
		Vec3   a;
		Vec3   b;
		double radius{ 0.5 };
		double restitution{ 0.5 };
		int    material{ 0 };
	};

	// A circle of tube: a hoop or a ring's rim. centre, the circle's axis, its radius and the tube's.
	struct Rim
	{
		Vec3   centre;
		Vec3   axis{ 0.0, 0.0, 1.0 };
		double radius{ 9.0 };
		double tube{ 0.4 };
		double restitution{ 0.55 };
		int    material{ 0 };
	};

	// A disc a ball is caught by when its centre goes through it against the normal (down through a hoop, into a hole),
	// within radius - margin of the centre (inner: the hole in the middle of a ring, 0 for a whole disc).
	struct Catch
	{
		Vec3   centre;
		Vec3   normal{ 0.0, 0.0, 1.0 };
		double radius{ 5.0 };
		double inner{ 0.0 };
		int    id{ 0 };
		bool   keep{ false };  // the ball stays in play after (the hoop: it falls on through the net); otherwise it's taken
	};

	enum class BallState : std::uint8_t
	{
		kOff,      // not in play
		kHeld,     // in the player's hand (placed by the game)
		kFlying,   // in play
		kCaught    // taken by a catch (the game hides it or puts it back)
	};

	struct Ball
	{
		Vec3      pos;
		Vec3      vel;
		Vec3      spin;  // angular velocity, radians/s about this axis
		Quat      rot;
		BallState state{ BallState::kOff };
		double    age{ 0.0 };   // seconds in play
		double    still{ 0.0 }; // seconds slow
		int       lastCatch{ -1 };
	};

	struct Event
	{
		enum class Kind : std::uint8_t
		{
			kHit,    // a ball hit something: material, speed into it
			kCaught, // a ball went through a catch: id
			kBalls   // two balls knocked together
		};
		Kind   kind{ Kind::kHit };
		int    ball{ -1 };
		int    other{ -1 };    // the catch id, the other ball, or the material
		double speed{ 0.0 };   // speed into the surface
		Vec3   where;
	};

	struct Spec
	{
		double ballRadius{ 2.0 };
		double gravity{ 686.0 };      // 9.8 m/s^2 at 70 units a metre
		double ballRestitution{ 0.6 };
		double substep{ 1.0 / 480.0 };
		double maxSpeed{ 2000.0 };
		double restSpeed{ 1.0 };      // below this, on a surface, for a while: at rest
		double airDrag{ 0.02 };       // fraction of speed lost a second in the air
	};

	class World
	{
	public:
		Spec               spec;
		std::vector<Rect>  rects;
		std::vector<Rail>  rails;
		std::vector<Rim>   rims;
		std::vector<Catch> catches;
		std::vector<Ball>  balls;

		// runs a_dt seconds of the world, in substeps; the events since the last call in a_events
		void Step(double a_dt, std::vector<Event>& a_events);

		// a ball into play at a_pos with a_vel
		void Launch(int a_ball, const Vec3& a_pos, const Vec3& a_vel);

		// any ball in play
		bool AnyFlying() const;

	private:
		void SubStep(double a_h, std::vector<Event>& a_events);
		bool Contact(Ball& a_ball, int a_index, const Vec3& a_normal, double a_depth, double a_restitution, double a_friction, double a_rollDecel,
			int a_material, double a_h, std::vector<Event>& a_events);
		std::vector<bool> _onSurface;
	};

	// Where a ball thrown from a_from at a_speed toward a_dir lands its first time down to height a_z (for a guide).
	// false if it never gets there.
	bool PredictToHeight(const Vec3& a_from, const Vec3& a_vel, double a_gravity, double a_z, Vec3& a_out);
}
