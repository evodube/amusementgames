#include "sim/Layouts.h"

#include <cmath>

namespace sim::layout
{
	namespace
	{
		// a rectangle across the machine (x from -a_halfX to a_halfX) from (y0, z0) to (y1, z1): a lane, a ramp, a
		// board. Its solid side faces up (and toward you, when it climbs away from you)
		Rect Slope(double a_y0, double a_z0, double a_y1, double a_z1, double a_halfX, double a_restitution, double a_friction,
			double a_rollDecel, int a_material)
		{
			Rect         r;
			const Vec3   along = Vec3{ 0.0, a_y1 - a_y0, a_z1 - a_z0 };
			const double len = along.Length();
			const Vec3   d = along * (1.0 / len);
			r.centre = { 0.0, (a_y0 + a_y1) * 0.5, (a_z0 + a_z1) * 0.5 };
			r.normal = Vec3{ 0.0, -d.z, d.y };
			if (r.normal.z < 0.0) {
				r.normal = -r.normal;
			}
			r.u = { 1.0, 0.0, 0.0 };
			r.halfU = a_halfX;
			r.halfV = len * 0.5;
			r.restitution = a_restitution;
			r.friction = a_friction;
			r.rollDecel = a_rollDecel;
			r.material = a_material;
			return r;
		}

		// an upright two-sided wall at x = a_x, from y0 to y1 and z0 to z1
		Rect SideWall(double a_x, double a_y0, double a_y1, double a_z0, double a_z1, double a_restitution)
		{
			Rect r;
			r.centre = { a_x, (a_y0 + a_y1) * 0.5, (a_z0 + a_z1) * 0.5 };
			r.normal = { a_x < 0.0 ? 1.0 : -1.0, 0.0, 0.0 };
			r.u = { 0.0, 1.0, 0.0 };
			r.halfU = std::abs(a_y1 - a_y0) * 0.5;
			r.halfV = std::abs(a_z1 - a_z0) * 0.5;
			r.restitution = a_restitution;
			r.friction = 0.1;
			r.twoSided = true;
			return r;
		}

		// an upright wall across the machine at y = a_y, facing you
		Rect BackWall(double a_y, double a_halfX, double a_z0, double a_z1, double a_restitution, bool a_twoSided)
		{
			Rect r;
			r.centre = { 0.0, a_y, (a_z0 + a_z1) * 0.5 };
			r.normal = { 0.0, -1.0, 0.0 };
			r.u = { 1.0, 0.0, 0.0 };
			r.halfU = a_halfX;
			r.halfV = std::abs(a_z1 - a_z0) * 0.5;
			r.restitution = a_restitution;
			r.friction = 0.15;
			r.twoSided = a_twoSided;
			return r;
		}
	}

	const RollersLayout& Rollers()
	{
		static const RollersLayout l;
		return l;
	}

	const HoopLayout& Hoop()
	{
		static const HoopLayout l;
		return l;
	}

	const WhacLayout& Whac()
	{
		static const WhacLayout l;
		return l;
	}

	const ZapperLayout& Zapper()
	{
		static const ZapperLayout l;
		return l;
	}

	const GalleryLayout& Gallery()
	{
		static const GalleryLayout l;
		return l;
	}

	void BuildRollers(World& a_world)
	{
		const auto&  L = Rollers();
		const double r = L.ballRadius;
		// the lane (flat, then rising), the hump's ramp and flat top: wood, a little rolling resistance
		a_world.rects.push_back(Slope(L.laneFrontY, L.laneFrontZ, L.laneBendY, L.laneBendZ, L.laneHalfWidth, 0.2, 0.05, 9.0, 0));
		a_world.rects.push_back(Slope(L.laneBendY, L.laneBendZ, L.rampStartY, L.rampStartZ, L.laneHalfWidth, 0.2, 0.05, 9.0, 0));
		a_world.rects.push_back(Slope(L.rampStartY, L.rampStartZ, L.rampEndY, L.rampEndZ, L.laneHalfWidth, 0.2, 0.0, 6.0, 0));
		a_world.rects.push_back(Slope(L.rampEndY, L.rampEndZ, L.humpEndY, L.rampEndZ, L.laneHalfWidth, 0.2, 0.0, 6.0, 0));
		// the gutter's floor between the hump and the board
		a_world.rects.push_back(Slope(L.humpEndY, L.gutterZ, L.boardStartY, L.gutterZ, L.boardHalfWidth, 0.2, 0.2, 20.0, 0));
		// the walls: the lane's, then the wider board's
		a_world.rects.push_back(SideWall(-L.laneHalfWidth, L.laneFrontY, L.humpEndY, L.laneFrontZ - 5.0, L.rampEndZ + 40.0, 0.45));
		a_world.rects.push_back(SideWall(L.laneHalfWidth, L.laneFrontY, L.humpEndY, L.laneFrontZ - 5.0, L.rampEndZ + 40.0, 0.45));
		a_world.rects.push_back(SideWall(-L.boardHalfWidth, L.humpEndY, L.boardEndY + 6.0, L.gutterZ - 5.0, L.boardEndZ + 60.0, 0.45));
		a_world.rects.push_back(SideWall(L.boardHalfWidth, L.humpEndY, L.boardEndY + 6.0, L.gutterZ - 5.0, L.boardEndZ + 60.0, 0.45));
		// the board, with its holes let in; each hole's edge (a ball clipping it rattles) and the cup under it
		Rect         board = Slope(L.boardStartY, L.boardStartZ, L.boardEndY, L.boardEndZ, L.boardHalfWidth, 0.35, 0.1, 8.0, 1);
		const Vec3   n = board.normal.Normalized();
		const double slope = (L.boardEndZ - L.boardStartZ) / (L.boardEndY - L.boardStartY);
		for (const auto& h : L.holes) {
			const Vec3 at{ h.x, h.y, L.boardStartZ + slope * (h.y - L.boardStartY) };
			board.holes.emplace_back(at, h.radius);
			Rim edge;
			edge.centre = at;
			edge.axis = n;
			edge.radius = h.radius;
			edge.tube = 0.4;
			edge.restitution = 0.35;
			edge.material = 1;
			a_world.rims.push_back(edge);
			Catch c;
			c.centre = at - n * (r + 2.0);
			c.normal = n;
			c.radius = h.radius;
			c.id = h.points;
			a_world.catches.push_back(c);
		}
		a_world.rects.push_back(board);
		// the scoreboard's face above the board's top: high balls come back down
		a_world.rects.push_back(BackWall(L.boardEndY + 6.0, L.boardHalfWidth, L.boardEndZ, L.boardEndZ + 70.0, 0.3, false));
		// no points: down in the gutter, or over the board's top into the trough
		Catch gutter;
		gutter.centre = { 0.0, (L.humpEndY + L.boardStartY) * 0.5, L.gutterZ + r + 2.0 };
		gutter.normal = { 0.0, 0.0, 1.0 };
		gutter.radius = L.boardHalfWidth + 2.0;
		gutter.id = 0;
		a_world.catches.push_back(gutter);
		Catch trough;
		trough.centre = { 0.0, L.boardEndY + 2.0, L.boardEndZ };
		trough.normal = { 0.0, -1.0, 0.0 };
		trough.radius = 80.0;
		trough.id = 0;
		a_world.catches.push_back(trough);
	}

	void BuildHoop(World& a_world)
	{
		const auto& L = Hoop();
		Rim         rim;
		rim.centre = L.rim;
		rim.axis = { 0.0, 0.0, 1.0 };
		rim.radius = L.rimRadius;
		rim.tube = L.rimTube;
		rim.restitution = 0.55;
		rim.material = 1;
		a_world.rims.push_back(rim);
		// the bracket from the rim's back to the board
		a_world.rails.push_back({ L.rim + Vec3{ 0.0, L.rimRadius, 0.0 }, Vec3{ 0.0, L.boardY, L.rim.z }, 0.8, 0.5, 1 });
		// a basket: down through the hoop (the ball falls on through the net)
		Catch basket;
		basket.centre = L.rim - Vec3{ 0.0, 0.0, 6.0 };
		basket.normal = { 0.0, 0.0, 1.0 };
		basket.radius = L.rimRadius;
		basket.id = kHoopBasket;
		basket.keep = true;
		a_world.catches.push_back(basket);
		// the net: cords from the rim narrowing down, slowing what goes through
		for (int k = 0; k < 8; ++k) {
			const double a = k * 3.14159265358979 / 4.0;
			const Vec3   top = L.rim + Vec3{ std::cos(a) * L.rimRadius, std::sin(a) * L.rimRadius, 0.0 };
			const Vec3   bottom = L.rim + Vec3{ std::cos(a) * L.rimRadius * 0.7, std::sin(a) * L.rimRadius * 0.7, -20.0 };
			a_world.rails.push_back({ top, bottom, 0.3, 0.1, 2 });
		}
		a_world.rects.push_back(BackWall(L.boardY, L.boardHalfWidth, L.boardZ - L.boardHalfHeight, L.boardZ + L.boardHalfHeight, 0.6, true));
		a_world.rects.back().material = 0;
		a_world.rects.push_back(BackWall(L.boardY + 5.0, L.sideX, L.rampBackZ, L.boardZ - L.boardHalfHeight, 0.4, false));
		// the return ramp, down to the front; the cage's sides
		a_world.rects.push_back(Slope(L.rampFrontY, L.rampFrontZ, L.rampBackY, L.rampBackZ, L.sideX, 0.45, 0.1, 10.0, 3));
		a_world.rects.push_back(SideWall(-L.sideX, L.shelfFrontY, L.boardY, L.rampFrontZ - 5.0, L.sideTopZ, 0.4));
		a_world.rects.push_back(SideWall(L.sideX, L.shelfFrontY, L.boardY, L.rampFrontZ - 5.0, L.sideTopZ, 0.4));
		// back down at the front: the ball's done
		Catch back;
		back.centre = { 0.0, L.rampFrontY + 10.0, L.rampFrontZ + 20.0 };
		back.normal = { 0.0, 1.0, 0.0 };
		back.radius = L.sideX * 1.5;
		back.id = kHoopReturn;
		a_world.catches.push_back(back);
	}
}
