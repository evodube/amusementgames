#include "SettlerSkill.h"

#include "Settings.h"

namespace ag::SettlerSkill
{
	namespace
	{
		constexpr int kGames = static_cast<int>(sim::GameId::kCount);

		// what goes into it: their talent, then practice; their form on the day either way (no stats: settlers have
		// none, every SPECIAL 0 in their records; only companions' are set)
		constexpr double kTalent = 0.85;
		constexpr double kPractice = 0.15;  // the most practice adds (about half after 10 games there, most by 30)
		constexpr double kPracticeGames = 15.0;
		constexpr double kForm = 0.08;

		struct Record
		{
			float                               talent{ 0.5f };
			std::array<std::uint16_t, kGames>   played{};
			std::array<std::int32_t, kGames>    best{};  // their best score at each (the reactions: a new best cheered)
		};

		std::mutex                                 g_lock;
		std::unordered_map<RE::TESFormID, Record>  g_settlers;
	}

	double Roll(RE::TESFormID a_id, sim::GameId a_game, std::mt19937& a_rng)
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
		const double practice = kPractice * (1.0 - std::exp(-played / kPracticeGames));
		const double form = std::uniform_real_distribution<double>(-kForm, kForm)(a_rng);
		const double shift = Settings::Get().settlersSkill / 100.0;  // (the Settlers page: -50 to +50)
		const double skill = std::clamp(kTalent * talent + practice + form + shift, 0.0, 1.0);
		log::info("settler skill: {:08X} at {}: {:.2f} (talent {:.2f}, {} games before: +{:.2f}, form {:+.2f}, setting {:+.2f})",
			a_id, sim::GameName(a_game), skill, talent, played, practice, form, shift);
		return skill;
	}

	int Scored(RE::TESFormID a_id, sim::GameId a_game, int a_score)
	{
		const int game = static_cast<int>(a_game);
		if (game < 0 || game >= kGames || !a_id) {
			return 0;
		}
		std::scoped_lock l(g_lock);
		auto&     r = g_settlers[a_id];
		const int before = r.best[game];
		r.best[game] = std::max(before, a_score);
		return before;
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
			a_intfc->WriteRecordData(r.best.data(), sizeof(r.best));
		}
	}

	void Load(const F4SE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t)
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
				!a_intfc->ReadRecordData(r.played.data(), sizeof(r.played)) ||
				(a_version >= 2 && !a_intfc->ReadRecordData(r.best.data(), sizeof(r.best)))) {
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
