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
		// the lane (a little rolling resistance: wood), the hump that throws the ball up, the walls either side
		a_world.rects.push_back(Slope(L.laneFrontY, L.laneFrontZ, L.rampStartY, L.laneBackZ, L.laneHalfWidth, 0.2, 0.05, 9.0, 0));
		a_world.rects.push_back(Slope(L.rampStartY, L.laneBackZ, L.rampEndY, L.rampTopZ, L.laneHalfWidth, 0.2, 0.0, 6.0, 0));
		a_world.rects.push_back(SideWall(-L.laneHalfWidth - r, L.laneFrontY, L.rampEndY, L.laneFrontZ - 5.0, L.rampTopZ + 25.0, 0.45));
		a_world.rects.push_back(SideWall(L.laneHalfWidth + r, L.laneFrontY, L.rampEndY, L.laneFrontZ - 5.0, L.rampTopZ + 25.0, 0.45));
		a_world.rects.push_back(SideWall(-L.boardHalfWidth - r, L.rampEndY, L.boardTopY, L.laneBackZ - 20.0, L.boardTopZ + 40.0, 0.45));
		a_world.rects.push_back(SideWall(L.boardHalfWidth + r, L.rampEndY, L.boardTopY, L.laneBackZ - 20.0, L.boardTopZ + 40.0, 0.45));
		// the board: a slope with the rings let into it; under each opening, a catch for its points
		Rect         board = Slope(L.boardBottomY, L.boardBottomZ, L.boardTopY, L.boardTopZ, L.boardHalfWidth, 0.35, 0.1, 8.0, 1);
		const Vec3   n = board.normal.Normalized();
		const Vec3   up = n.Cross(board.u).Normalized();  // up the board
		const Vec3   bottom = board.centre - up * board.halfV;
		const Vec3   centre = bottom + up * (2.0 * board.halfV * L.ringCentreU);
		board.holes.emplace_back(centre, L.ringRadii.front());
		std::vector<Vec3> corners;
		for (double side : { -1.0, 1.0 }) {
			const Vec3 c = bottom + up * (2.0 * board.halfV * L.cornerHoleU) + Vec3{ side * L.cornerHoleX, 0.0, 0.0 };
			board.holes.emplace_back(c, L.cornerHoleRadius);
			corners.push_back(c);
		}
		a_world.rects.push_back(board);
		// the back stop above the board
		a_world.rects.push_back(BackWall(L.boardTopY, L.boardHalfWidth + r, L.boardTopZ - 10.0, L.boardTopZ + 40.0, 0.3, false));
		// the rings' outer rim standing proud of the board (a ball clipping it rattles off), and each ring's cup below:
		// a ball's points are the ring it drops in through
		for (std::size_t k = 0; k < L.ringRadii.size(); ++k) {
			if (k == 0) {
				Rim rim;
				rim.centre = centre + n * 0.4;
				rim.axis = n;
				rim.radius = L.ringRadii[k];
				rim.tube = 0.7;
				rim.restitution = 0.4;
				rim.material = 1;
				a_world.rims.push_back(rim);
			}
			Catch c;
			c.centre = centre - n * (r + 2.0);
			c.normal = n;
			c.radius = L.ringRadii[k] + r;
			c.inner = k + 1 < L.ringRadii.size() ? L.ringRadii[k + 1] : 0.0;
			c.id = L.ringPoints[k];
			a_world.catches.push_back(c);
		}
		for (const auto& c : corners) {
			Rim rim;
			rim.centre = c + n * 0.4;
			rim.axis = n;
			rim.radius = L.cornerHoleRadius;
			rim.tube = 0.6;
			rim.restitution = 0.4;
			rim.material = 1;
			a_world.rims.push_back(rim);
			Catch k;
			k.centre = c - n * (r + 2.0);
			k.normal = n;
			k.radius = L.cornerHoleRadius + r;
			k.id = 100;
			a_world.catches.push_back(k);
		}
		// the gutter: anything that drops off the bottom of the board, no points
		Catch gutter;
		gutter.centre = { 0.0, (L.rampEndY + L.boardBottomY) * 0.5, L.boardBottomZ - 18.0 };
		gutter.normal = { 0.0, 0.0, 1.0 };
		gutter.radius = 90.0;
		gutter.id = 0;
		a_world.catches.push_back(gutter);
	}

	void BuildHoop(World& a_world)
	{
		const auto&  L = Hoop();
		const double r = L.ballRadius;
		Rim          rim;
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
		basket.centre = L.rim - Vec3{ 0.0, 0.0, 3.0 };
		basket.normal = { 0.0, 0.0, 1.0 };
		basket.radius = L.rimRadius;
		basket.id = kHoopBasket;
		basket.keep = true;
		a_world.catches.push_back(basket);
		// the net: a few cords from the rim narrowing down, slowing what goes through
		for (int k = 0; k < 8; ++k) {
			const double a = k * 3.14159265358979 / 4.0;
			const Vec3   top = L.rim + Vec3{ std::cos(a) * L.rimRadius, std::sin(a) * L.rimRadius, 0.0 };
			const Vec3   bottom = L.rim + Vec3{ std::cos(a) * L.rimRadius * 0.7, std::sin(a) * L.rimRadius * 0.7, -14.0 };
			a_world.rails.push_back({ top, bottom, 0.3, 0.1, 2 });
		}
		a_world.rects.push_back(BackWall(L.boardY, L.boardHalfWidth, L.boardZ - L.boardHalfHeight, L.boardZ + L.boardHalfHeight, 0.6, true));
		a_world.rects.back().material = 0;
		// the return ramp, down to the front; the cage's sides and top
		a_world.rects.push_back(Slope(L.floorFrontY, L.floorFrontZ, L.boardY, L.floorBackZ, L.sideX, 0.45, 0.1, 10.0, 3));
		a_world.rects.push_back(SideWall(-L.sideX - r, L.floorFrontY, L.boardY, L.floorFrontZ, L.cageTopZ, 0.4));
		a_world.rects.push_back(SideWall(L.sideX + r, L.floorFrontY, L.boardY, L.floorFrontZ, L.cageTopZ, 0.4));
		a_world.rects.push_back(BackWall(L.boardY + 2.0, L.sideX, L.floorBackZ, L.cageTopZ, 0.4, false));
		// back to the front: the ball's done
		Catch back;
		back.centre = { 0.0, L.floorFrontY + 15.0, L.floorFrontZ + r };
		back.normal = { 0.0, 1.0, 0.0 };
		back.radius = L.sideX * 2.0;
		back.id = kHoopReturn;
		a_world.catches.push_back(back);
	}
}
