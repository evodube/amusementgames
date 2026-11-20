#include "SettlerSkill.h"

#include "Settings.h"

namespace ag::SettlerSkill
{
	namespace
	{
		constexpr int kGames = static_cast<int>(sim::GameId::kCount);

		// what goes into it: talent most, their stat, then practice; their form on the day either way
		constexpr double kTalent = 0.55;
		constexpr double kStat = 0.30;
		constexpr double kPractice = 0.15;  // the most practice adds (about half after 10 games there, most by 30)
		constexpr double kPracticeGames = 15.0;
		constexpr double kForm = 0.08;

		struct Record
		{
			float                               talent{ 0.5f };
			std::array<std::uint16_t, kGames>   played{};
		};

		std::mutex                                 g_lock;
		std::unordered_map<RE::TESFormID, Record>  g_settlers;

		const RE::ActorValueInfo* Value(const char* a_name)
		{
			auto* av = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>(a_name);
			if (!av) {
				log::warn("settler skill: no {} actor value found", a_name);
			}
			return av;
		}

		// their stat for the game, 1 to 10 as 0 to 1 (0.5 if it can't be read)
		double Stat(RE::Actor* a_actor, sim::GameId a_game)
		{
			static const RE::ActorValueInfo* per = Value("Perception");
			static const RE::ActorValueInfo* agi = Value("Agility");
			auto norm = [&](const RE::ActorValueInfo* a_av) {
				return a_actor && a_av ? std::clamp((a_actor->GetActorValue(*a_av) - 1.0) / 9.0, 0.0, 1.0) : 0.5;
			};
			switch (a_game) {
			case sim::GameId::kBanditRoundup:
			case sim::GameId::kNukaZapperRace: return norm(per);  // (aiming)
			case sim::GameId::kWhacACommie: return norm(agi);     // (quick hands)
			default: return 0.5 * (norm(per) + norm(agi));        // (a throw: aim and touch)
			}
		}
	}

	double Roll(RE::Actor* a_actor, RE::TESFormID a_id, sim::GameId a_game, std::mt19937& a_rng)
	{
		const int game = static_cast<int>(a_game);
		if (game < 0 || game >= kGames) {
			return std::uniform_real_distribution<double>(0.0, 1.0)(a_rng);
		}
		double talent = 0.5;
		int    played = 0;
		{
			std::scoped_lock l(g_lock);
			auto [it, fresh] = g_settlers.try_emplace(a_id);
			if (fresh) {
				it->second.talent = static_cast<float>(std::uniform_real_distribution<double>(0.0, 1.0)(a_rng));
			}
			talent = it->second.talent;
			played = it->second.played[game];
			if (it->second.played[game] < 0xFFFF) {
				++it->second.played[game];
			}
		}
		const double stat = Stat(a_actor, a_game);
		const double practice = kPractice * (1.0 - std::exp(-played / kPracticeGames));
		const double form = std::uniform_real_distribution<double>(-kForm, kForm)(a_rng);
		const double shift = Settings::Get().settlersSkill / 100.0;  // (the Settlers page: -50 to +50)
		const double skill = std::clamp(kTalent * talent + kStat * stat + practice + form + shift, 0.0, 1.0);
		log::info("settler skill: {:08X} at {}: {:.2f} (talent {:.2f}, stat {:.2f}, {} games before: +{:.2f}, form {:+.2f}, setting {:+.2f})",
			a_id, sim::GameName(a_game), skill, talent, stat, played, practice, form, shift);
		return skill;
	}

	void Save(const F4SE::SerializationInterface* a_intfc)
	{
		std::scoped_lock l(g_lock);
		if (!a_intfc->OpenRecord(kSaveType, kSaveVersion)) {
			return;
		}
		const std::uint32_t n = static_cast<std::uint32_t>(g_settlers.size());
		a_intfc->WriteRecordData(&n, sizeof(n));
		for (const auto& [id, r] : g_settlers) {
			a_intfc->WriteRecordData(&id, sizeof(id));
			a_intfc->WriteRecordData(&r.talent, sizeof(r.talent));
			a_intfc->WriteRecordData(r.played.data(), sizeof(r.played));
		}
	}

	void Load(const F4SE::SerializationInterface* a_intfc, std::uint32_t, std::uint32_t)
	{
		std::scoped_lock l(g_lock);
		g_settlers.clear();
		std::uint32_t n = 0;
		if (!a_intfc->ReadRecordData(&n, sizeof(n))) {
			return;
		}
		for (std::uint32_t i = 0; i < n; ++i) {
			RE::TESFormID id = 0;
			Record        r;
			if (!a_intfc->ReadRecordData(&id, sizeof(id)) || !a_intfc->ReadRecordData(&r.talent, sizeof(r.talent)) ||
				!a_intfc->ReadRecordData(r.played.data(), sizeof(r.played))) {
				return;
			}
			if (const auto now = a_intfc->ResolveFormID(id)) {
				g_settlers[*now] = r;
			}
		}
	}

	void Revert()
	{
		std::scoped_lock l(g_lock);
		g_settlers.clear();
	}
}
