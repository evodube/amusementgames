#include "Reactions.h"

#include "Forms.h"
#include "Papyrus.h"

namespace ag::Reactions
{
	namespace
	{
		struct Gesture
		{
			bool          nuka;  // DLCNukaWorld.esm's (else Fallout4.esm's)
			std::uint32_t id;
		};

		// the game's own: Fallout4.esm's IdleCheeringStanding (its crowd cheers, a few takes), Nuka-World's
		// DLC04_Raider_WildCheering1-3; IdleClapping, Nuka-World's DLC04_Raider_ClapStanding1; HeadShakeYes, HeadShakeNo
		constexpr std::array<Gesture, 4> kCheers{ { { false, 0x10BA3A }, { true, 0x019E8F }, { true, 0x019E90 }, { true, 0x019E91 } } };
		constexpr std::array<Gesture, 2> kClaps{ { { false, 0x141F3C }, { true, 0x019E8C } } };
		constexpr Gesture                kNodIdle{ false, 0x038C7B };
		constexpr Gesture                kShakeIdle{ false, 0x038C7A };

		RE::TESForm* Form(const Gesture& a_g)
		{
			return a_g.nuka ? Forms::Nuka(a_g.id) : RE::TESForm::GetFormByID(a_g.id);
		}
	}

	Kind Judge(int a_score, int a_bestBefore, int a_place)
	{
		if (a_score <= 0) {
			return Kind::kShake;
		}
		if ((a_place >= 0 && a_place < 3) || (a_bestBefore > 0 && a_score > a_bestBefore)) {
			return Kind::kCheer;  // (a new best of theirs, or near the top of the high scores)
		}
		if (a_bestBefore > 0 && a_score * 2 < a_bestBefore) {
			return Kind::kShake;  // (well under what they can do)
		}
		return Kind::kNod;
	}

	void Play(RE::TESFormID a_actor, Kind a_kind, double a_delay, std::mt19937& a_rng)
	{
		Gesture g{};
		int     hold = 0;  // ms the cheer or clap goes on before it's stopped (the nods and shakes end themselves)
		switch (a_kind) {
		case Kind::kCheer:
			g = kCheers[std::uniform_int_distribution<std::size_t>(0, kCheers.size() - 1)(a_rng)];
			hold = 4000;
			break;
		case Kind::kClap:
			g = kClaps[std::uniform_int_distribution<std::size_t>(0, kClaps.size() - 1)(a_rng)];
			hold = 3000;
			break;
		case Kind::kNod: g = kNodIdle; break;
		case Kind::kShake: g = kShakeIdle; break;
		}
		auto* idle = Form(g);
		if (!a_actor || !idle) {
			return;
		}
		static constexpr const char* kNames[]{ "cheers", "claps", "nods", "shakes their head" };
		log::info("reaction: {:08X} {} (in {:.1f}s)", a_actor, kNames[static_cast<int>(a_kind)], a_delay);
		Papyrus::Call("NpcReact", { static_cast<std::int32_t>(a_actor), Papyrus::Id(idle), static_cast<std::int32_t>(a_delay * 1000.0), hold });
	}
}
