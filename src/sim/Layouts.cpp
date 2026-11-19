#include "sim/Layouts.h"

#include "sim/Collision.inc"

#include <cmath>

namespace sim::layout
{
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
		const auto& L = Rollers();
		// the machine's own collision (DLC04_Nukacade_SkeeBall01.nif's): lane, hump, gutter, the board with its ring
		// walls and holes, the cups under them, the cabinet
		Mesh m;
		for (const auto& t : collision::kRollers) {
			m.Add(t);
		}
		m.restitution = 0.3;
		m.friction = 0.08;
		m.rollDecel = 7.0;
		m.material = 0;
		a_world.meshes.push_back(std::move(m));
		// the cups: a ball down in one is scored (Nuka-World's BallHole01-07 nodes, at the cups' bottoms); the gutter
		// before the board (BallHole00) and the trough over its top score nothing
		for (const auto& h : L.cups) {
			Catch c;
			c.centre = h.at + Vec3{ 0.0, 0.0, 6.0 };
			c.box = { 7.0, 7.0, 9.0 };
			c.id = h.points;
			a_world.catches.push_back(c);
		}
		Catch gutter;
		gutter.centre = L.gutter;
		gutter.box = { 42.0, 9.0, 6.0 };
		gutter.id = 0;
		a_world.catches.push_back(gutter);
		Catch trough;
		trough.centre = L.trough;
		trough.box = { 64.0, 9.0, 22.0 };
		trough.id = 0;
		a_world.catches.push_back(trough);
	}

	void BuildHoop(World& a_world)
	{
		const auto& L = Hoop();
		// the machine's own collision (DLC04_Nukacade_BasketBall01.nif's: the cage, backboard, return ramp and front),
		// all but its rim: a round one of ours (the model's is eight flat plates)
		Mesh m;
		for (const auto& t : collision::kHoop) {
			m.Add(t);
		}
		m.restitution = L.bounce;
		m.friction = 0.15;
		m.rollDecel = 10.0;
		m.material = 0;
		a_world.meshes.push_back(std::move(m));
		Rim rim;
		rim.centre = L.rim;
		rim.axis = { 0.0, 0.0, 1.0 };
		rim.radius = L.rimRadius;
		rim.tube = L.rimTube;
		rim.restitution = L.bounce;
		rim.material = 1;
		a_world.rims.push_back(rim);
		// the bracket from the rim's back to the board
		a_world.rails.push_back({ L.rim + Vec3{ 0.0, L.rimRadius, 0.0 }, Vec3{ 0.0, L.boardY, L.rim.z }, 0.8, L.bounce, 1 });
		// a basket: down through the hoop (the ball falls on through the net)
		Catch basket;
		basket.centre = L.rim - Vec3{ 0.0, 0.0, 6.0 };
		basket.normal = { 0.0, 0.0, 1.0 };
		basket.radius = L.rimRadius + 1.0;
		basket.id = kHoopBasket;
		basket.keep = true;
		a_world.catches.push_back(basket);
		// the net: cords from the rim narrowing down, slowing what goes through
		for (int k = 0; k < 8; ++k) {
			const double a = k * 3.14159265358979 / 4.0;
			const Vec3   top = L.rim + Vec3{ std::cos(a) * L.rimRadius, std::sin(a) * L.rimRadius, 0.0 };
			const Vec3   bottom = L.rim + Vec3{ std::cos(a) * L.rimRadius * 0.7, std::sin(a) * L.rimRadius * 0.7, -20.0 };
			a_world.rails.push_back({ top, bottom, 0.3, 0.05, 2 });
		}
		// back down at the front: the ball's done
		Catch back;
		back.centre = { 0.0, L.rampFrontY + 10.0, L.rampFrontZ + 20.0 };
		back.normal = { 0.0, 1.0, 0.0 };
		back.radius = L.sideX * 1.5;
		back.id = kHoopReturn;
		a_world.catches.push_back(back);
	}
}
