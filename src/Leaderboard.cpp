#include "Leaderboard.h"

#include "Forms.h"
#include "Papyrus.h"

namespace ag::Leaderboard
{
	namespace
	{
		constexpr int kGames = static_cast<int>(sim::GameId::kCount);

		// names that aren't anyone's own (the game's generic ones: settlers, wanderers, jobs, factions, hired help,
		// default robots): these never go on a board. Matched ignoring case and full stops ("Mr. Handy" is "Mr Handy")
		constexpr std::array<std::string_view, 53> kGeneric{
			"Settler", "Provisioner", "Wastelander", "Wasteland Traveler", "Drifter", "Resident", "Neighbor", "Farmer",
			"Fisherman", "Harborman", "Hunter", "Moonshiner", "Scavenger", "Merchant", "Trader", "Caravan Guard",
			"Caravan Worker", "Guard", "Minuteman", "Railroad Agent", "Brotherhood Knight", "Brotherhood Scribe",
			"Child of Atom", "Gunner", "Raider", "Disciple", "Operator", "Pack Member", "Mercenary", "Hired Gun", "Bodyguard",
			"Synth Refugee", "Rebel Synth", "Escaping Synth", "Ghoul", "Prisoner", "Junkie", "Courier", "Soldier", "Mr Handy",
			"Mr Gutsy", "Protectron", "Assaultron", "Sentry Bot", "Robobrain", "Eyebot", "Robot", "Automatron", "Junkbot",
			"Scrapbot", "Swarmbot", "Tankbot", "Servitron"
		};

		struct Entry
		{
			RE::TESFormID actor{ 0 };
			std::int32_t  score{ 0 };
		};

		std::mutex                                       g_lock;
		std::array<std::vector<Entry>, kGames>           g_boards;

		bool Generic(std::string_view a_name)
		{
			auto lower = [](std::string_view a_s) {
				std::string out;
				for (const unsigned char c : a_s) {
					if (c != '.') {
						out += static_cast<char>(std::tolower(c));
					}
				}
				while (!out.empty() && out.back() == ' ') {
					out.pop_back();
				}
				return out;
			};
			const auto name = lower(a_name);
			return name.empty() || std::ranges::any_of(kGeneric, [&](std::string_view g) { return lower(g) == name; });
		}

		constexpr int kRace = static_cast<int>(sim::GameId::kNukaZapperRace);

		// whoever's named (not one of the generic names): their name, or null
		const char* Named(RE::TESFormID a_actor)
		{
			auto* form = RE::TESForm::GetFormByID(a_actor);
			auto* actor = form ? form->As<RE::Actor>() : nullptr;
			const char* name = actor ? actor->GetDisplayFullName() : nullptr;
			return name && !Generic(name) ? name : nullptr;
		}

		// a board onto the counter: each place's score into its global (0: empty, its line hidden), its player into
		// its alias (AmusementGamesNative.TopAlias: 0 clears it)
		void Publish(int a_game, const std::vector<Entry>& a_board)
		{
			for (int place = 0; place < kPlaces; ++place) {
				const Entry e = place < static_cast<int>(a_board.size()) ? a_board[place] : Entry{};
				const auto  id = kFirstScoreID + static_cast<std::uint32_t>(a_game * kPlaces + place);
				if (auto* g = Forms::Own(id); g && g->Is(RE::ENUM_FORM_ID::kGLOB)) {
					static_cast<RE::TESGlobal*>(g)->value = static_cast<float>(e.score);
				}
				Papyrus::Call("TopAlias", { kFirstAlias + a_game * kPlaces + place, static_cast<std::int32_t>(e.actor) });
			}
		}
	}

	void Record(sim::GameId a_game, RE::TESFormID a_actor, int a_score)
	{
		const int game = static_cast<int>(a_game);
		if (game < 0 || game >= kGames || game == kRace || a_actor == 0 || a_score <= 0) {
			return;
		}
		const char* name = Named(a_actor);
		if (!name) {
			return;
		}
		std::vector<Entry> board;
		{
			std::scoped_lock l(g_lock);
			auto& b = g_boards[game];
			auto  it = std::ranges::find_if(b, [&](const Entry& e) { return e.actor == a_actor; });
			if (it != b.end()) {
				if (a_score <= it->score) {
					return;  // (their best's already up)
				}
				it->score = a_score;
			} else {
				b.push_back({ a_actor, a_score });
			}
			std::ranges::stable_sort(b, std::greater<>{}, &Entry::score);
			const bool placed = std::ranges::any_of(b | std::views::take(kPlaces), [&](const Entry& e) { return e.actor == a_actor; });
			if (b.size() > static_cast<std::size_t>(kPlaces)) {
				b.resize(kPlaces);
			}
			if (!placed) {
				return;
			}
			board = b;
		}
		log::info("high scores: {} ({:08X}) {} at {}", name, a_actor, a_score, sim::GameName(a_game));
		Publish(game, board);
	}

	void RecordWin(RE::TESFormID a_actor)
	{
		const char* name = a_actor ? Named(a_actor) : nullptr;
		if (!name) {
			return;
		}
		std::vector<Entry> board;
		int                wins = 0;
		{
			// (everyone who's won kept, not just the ten: someone further down can still climb on)
			std::scoped_lock l(g_lock);
			auto& b = g_boards[kRace];
			auto  it = std::ranges::find_if(b, [&](const Entry& e) { return e.actor == a_actor; });
			if (it != b.end()) {
				wins = ++it->score;
			} else {
				b.push_back({ a_actor, 1 });
				wins = 1;
			}
			std::ranges::stable_sort(b, std::greater<>{}, &Entry::score);
			board.assign(b.begin(), b.begin() + std::min<std::size_t>(b.size(), kPlaces));
		}
		log::info("high scores: {} ({:08X}) has won {} Nuka-Zapper Race(s)", name, a_actor, wins);
		Publish(kRace, board);
	}

	int Wins(RE::TESFormID a_actor)
	{
		std::scoped_lock l(g_lock);
		const auto& b = g_boards[kRace];
		const auto  it = std::ranges::find_if(b, [&](const Entry& e) { return e.actor == a_actor; });
		return it != b.end() ? it->score : 0;
	}

	void Save(const F4SE::SerializationInterface* a_intfc)
	{
		std::scoped_lock l(g_lock);
		if (!a_intfc->OpenRecord(kSaveType, kSaveVersion)) {
			return;
		}
		for (const auto& b : g_boards) {
			const std::uint32_t n = static_cast<std::uint32_t>(b.size());
			a_intfc->WriteRecordData(&n, sizeof(n));
			for (const auto& e : b) {
				a_intfc->WriteRecordData(&e.actor, sizeof(e.actor));
				a_intfc->WriteRecordData(&e.score, sizeof(e.score));
			}
		}
	}

	void Load(const F4SE::SerializationInterface* a_intfc, std::uint32_t, std::uint32_t)
	{
		std::scoped_lock l(g_lock);
		for (auto& b : g_boards) {
			b.clear();
			std::uint32_t n = 0;
			if (!a_intfc->ReadRecordData(&n, sizeof(n))) {
				return;
			}
			for (std::uint32_t i = 0; i < n; ++i) {
				Entry e;
				if (!a_intfc->ReadRecordData(&e.actor, sizeof(e.actor)) || !a_intfc->ReadRecordData(&e.score, sizeof(e.score))) {
					return;
				}
				if (const auto now = a_intfc->ResolveFormID(e.actor)) {
					e.actor = *now;
					b.push_back(e);
				}
			}
		}
	}

	void Revert()
	{
		std::scoped_lock l(g_lock);
		for (auto& b : g_boards) {
			b.clear();
		}
	}
}
