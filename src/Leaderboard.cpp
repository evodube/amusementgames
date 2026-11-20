#include "Leaderboard.h"

#include "Forms.h"
#include "Papyrus.h"

namespace ag::Leaderboard
{
	namespace
	{
		constexpr int kGames = static_cast<int>(sim::GameId::kCount);

		// names that aren't anyone's own (the game's generic ones: settlers, wanderers, jobs, factions, hired help,
		// default robots, and their ranks and kinds): these never go on a board. A whole name only, ignoring case and full
		// stops ("Mr. Handy" is "Mr Handy"): a name after a rank ("Scribe Haylen", "Supervisor White") still goes on
		constexpr std::string_view kGeneric[]{
			// settlers, wanderers, jobs, factions, hired help
			"Settler", "Provisioner", "Wastelander", "Wasteland Traveler", "Drifter", "Resident", "Neighbor", "Farmer",
			"Fisherman", "Harborman", "Hunter", "Moonshiner", "Scavenger", "Merchant", "Trader", "Caravan Guard",
			"Caravan Worker", "Guard", "Minuteman", "Railroad Agent", "Brotherhood Knight", "Brotherhood Scribe",
			"Child of Atom", "Gunner", "Raider", "Disciple", "Operator", "Pack Member", "Mercenary", "Hired Gun", "Bodyguard",
			"Synth Refugee", "Rebel Synth", "Escaping Synth", "Ghoul", "Prisoner", "Junkie", "Courier", "Soldier",
			// default robots
			"Mr Handy", "Mr Gutsy", "Protectron", "Assaultron", "Sentry Bot", "Robobrain", "Eyebot", "Robot", "Automatron",
			"Junkbot", "Scrapbot", "Swarmbot", "Tankbot", "Servitron",
			// ranks and kinds: raiders, Gunners, the Brotherhood, Nuka-World's gangs, Children of Atom, synths and the
			// Institute, super mutants, trappers, mercs, jobs and places
			"Raider Psycho", "Raider Scavver", "Raider Scum", "Raider Survivalist", "Raider Veteran", "Raider Waster",
			"Raider Man", "Raider Woman", "Berserk Raider", "Sinner's Raider", "Gunner Conscript", "Gunner Private",
			"Gunner Corporal", "Gunner Sergeant", "Gunner Lieutenant", "Gunner Captain", "Gunner Major", "Gunner Colonel",
			"Gunner Commander", "Gunner Brigadier", "Gunner Mercenary", "Brotherhood Aspirant", "Brotherhood Initiate",
			"Brotherhood Soldier Initiate", "Brotherhood Knight-Sergeant", "Brotherhood Knight-Captain",
			"Brotherhood Knight-Commander", "Brotherhood Lancer", "Brotherhood Lancer-Initiate", "Brotherhood Lancer-Knight",
			"Brotherhood Lancer-Sergeant", "Brotherhood Lancer-Captain", "Brotherhood Paladin",
			"Brotherhood Paladin-Commander", "Brotherhood Star Paladin", "Brotherhood Scribe Initiate",
			"Brotherhood Senior Scribe", "Head Knight", "Rogue Knight", "Initiate", "Disciple Scavver", "Disciple Waster",
			"Disciple Survivalist", "Disciple Veteran", "Disciple Pillager", "Disciple Tormentor", "Disciple Butcher",
			"Operator Scavver", "Operator Waster", "Operator Survivalist", "Operator Veteran", "Operator Pillager",
			"Operator Tormentor", "Operator Butcher", "Pack Scavver", "Pack Waster", "Pack Survivalist", "Pack Veteran",
			"Pack Pillager", "Pack Tormentor", "Pack Butcher", "Pack Captive", "Child of Atom Cultist",
			"Child of Atom Preacher", "Child of Atom Reborn", "Child of Atom Zealot", "Synth", "Gen 2 Synth", "Synth Leader",
			"Synth Trooper", "Synth Patroller", "Synth Seeker", "Synth Stormer", "Synth Strider", "Synth Assaulter",
			"Synth Eradicator", "Institute Scientist", "Institute Worker", "Institute Courser", "Institute Division Head",
			"Super Mutant", "Super Mutant Brute", "Super Mutant Butcher", "Super Mutant Enforcer", "Super Mutant Master",
			"Super Mutant Overlord", "Super Mutant Primus", "Super Mutant Skirmisher", "Super Mutant Warlord",
			"Super Mutant Suicider", "SuperMutant Skirmisher", "Trapper", "Grim Trapper", "Stalking Trapper",
			"Ruthless Trapper", "Relentless Trapper", "Scrounging Trapper", "Triggerman", "Rust Devil", "Combatant",
			"Railroad Heavy", "Pilot", "Missionary", "Preacher", "Compound Guard", "Diamond City Security", "Vault Security",
			"Vault-Tec Security", "Vault 81 Resident", "Tunnel Worker", "Chem Lab Worker", "Caravan Prisoner", "Concierge",
			"Midshipman", "Sentry", "Virtual Settler",
			// robot families: Mr. Handy, Gutsy, Protectron, Sentry Bot, Assaultron, Automatron's, Nuka-World's, Gunners'
			"Mr Handy Greeter", "Nurse Handy", "Assistant Manager Handy", "Mr Frothy", "Mr Frothy Chef", "Mr Frothy Waiter",
			"Mr. Handy Annihilator", "Mr. Handy Destroyer", "Mr. Handy Dominator", "Mr. Handy War Machine",
			"Mr. Handy Wrecker", "Guard Gutsy", "Lieutenant Gutsy", "Major Gutsy", "Colonel Gutsy", "Brotherhood Mr Gutsy",
			"Cadet Astro-Gutsy", "Lieutenant Astro-Gutsy", "Captain Astro-Gutsy", "Commander Astro-Gutsy", "Police Protectron",
			"Utility Protectron", "Doom Protectron", "Protectron Sheriff", "Protectron Steward", "Protectron Watcher",
			"Protectron Guardian", "Protectron Golem", "Protectron Basilisk", "Protectron Scourge", "Protectron Devil",
			"Annihilator Sentry Bot", "Annihilator Sentry Bot Mk.II", "Siege Breaker Sentry Bot", "Sentry Bot Butcher",
			"Sentry Bot Carnage", "Sentry Bot Lament", "Sentry Bot Reaper", "Sentry Bot Ripper", "Space Sentry",
			"Space Sentry Annihilator", "Space Sentry Breaker", "Space Sentry Obliterator", "Assaultron Dominator",
			"Assaultron Invader", "Assaultron Demon", "Assaultron Devil", "Assaultron Gorgon", "Assaultron Hag",
			"Assaultron Succubus", "Unstable Assaultron", "Integrated Junkbot", "Integrated Scrapbot", "Integrated Swarmbot",
			"Integrated Tankbot", "Integrated Robobrain", "Servomech Junkbot", "Servomech Scrapbot", "Servomech Swarmbot",
			"Servomech Tankbot", "Servomech Robobrain", "Servomech Eyebot", "Quantum Junkbot", "Quantum Scrapbot",
			"Quantum Swarmbot", "Quantum Tankbot", "Quantum Robobrain", "Cybermech Junkbot", "Cybermech Scrapbot",
			"Cybermech Swarmbot", "Cybermech Tankbot", "Cybermech Robobrain", "Cybermech Eyebot", "Galactron",
			"Galactron Defender", "Galactron Guardian", "Galactron Sentinel", "Galactron Technician", "Galactron Usher",
			"Galactron Watcher", "Nukatron", "Nukatron Defender", "Nukatron Guardian", "Nukatron Sentinel", "Nukatron Watcher",
			"Novatron", "Novatron Dominator", "Novatron Eliminator", "Novatron Initiator", "Novatron Invader", "Cannery Robot",
			"TrashBot", "TurretBot", "Gunner Annihilator Sentry Bot", "Gunner Annihilator Sentry Bot Mk.II",
			"Gunner Assaultron", "Gunner Assaultron Dominator", "Gunner Assaultron Invader", "Gunner Colonel Gutsy",
			"Gunner Lt. Gutsy", "Gunner Major Gutsy", "Gunner Mr Handy", "Gunner Mr. Gutsy", "Gunner Protectron",
			"Gunner Protectron Chef", "Gunner Sentry Bot", "Gunner Siege Breaker Sentry Bot", "Combat Sentry Prototype MKIV"
		};

		struct Entry
		{
			RE::TESFormID actor{ 0 };  // (0 once gone)
			std::int32_t  score{ 0 };
			std::uint8_t  gone{ 0 };   // deleted: the place kept, "..." for the name
		};

		std::mutex                                       g_lock;
		std::array<std::vector<Entry>, kGames>           g_boards;
		bool                                             g_published{ false };  // every board's aliases filled this session

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
				const auto gone = kFirstGoneID + static_cast<std::uint32_t>(a_game * kPlaces + place);
				if (auto* g = Forms::Own(gone); g && g->Is(RE::ENUM_FORM_ID::kGLOB)) {
					static_cast<RE::TESGlobal*>(g)->value = e.gone ? 1.0f : 0.0f;
				}
				Papyrus::Call("TopAlias", { kFirstAlias + a_game * kPlaces + place, static_cast<std::int32_t>(e.actor) });
			}
		}
	}

	int Record(sim::GameId a_game, RE::TESFormID a_actor, int a_score)
	{
		const int game = static_cast<int>(a_game);
		if (game < 0 || game >= kGames || game == kRace || a_actor == 0 || a_score <= 0) {
			return -1;
		}
		const char* name = Named(a_actor);
		if (!name) {
			return -1;
		}
		std::vector<Entry> board;
		int                place = -1;
		{
			std::scoped_lock l(g_lock);
			auto& b = g_boards[game];
			auto  it = std::ranges::find_if(b, [&](const Entry& e) { return e.actor == a_actor; });
			if (it != b.end()) {
				if (a_score <= it->score) {
					return -1;  // (their best's already up)
				}
				it->score = a_score;
			} else {
				b.push_back({ a_actor, a_score });
			}
			std::ranges::stable_sort(b, std::greater<>{}, &Entry::score);
			const auto at = std::ranges::find_if(b, [&](const Entry& e) { return e.actor == a_actor; });
			place = static_cast<int>(at - b.begin());
			const bool placed = place < kPlaces;
			if (b.size() > static_cast<std::size_t>(kPlaces)) {
				b.resize(kPlaces);
			}
			if (!placed) {
				return -1;
			}
			board = b;
		}
		log::info("high scores: {} ({:08X}) {} at {}", name, a_actor, a_score, sim::GameName(a_game));
		Publish(game, board);
		return place;
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

	void Refresh()
	{
		std::array<std::vector<Entry>, kGames> changed;
		std::array<bool, kGames>               publish{};
		{
			std::scoped_lock l(g_lock);
			if (!g_published) {
				g_published = true;
				publish.fill(true);
			}
			for (int game = 0; game < kGames; ++game) {
				bool any = false;
				auto& b = g_boards[game];
				for (auto& e : b) {
					if (e.gone) {
						continue;
					}
					auto* form = RE::TESForm::GetFormByID(e.actor);
					if (!form || form->IsDeleted()) {
						log::info("high scores: {:08X} ({} at {}) is gone: \"...\"", e.actor, e.score, sim::GameName(static_cast<sim::GameId>(game)));
						e.gone = 1;
						e.actor = 0;  // (its id may be someone new's later)
						any = true;
					}
				}
				if (any || publish[game]) {
					publish[game] = true;
					changed[game].assign(b.begin(), b.begin() + std::min<std::size_t>(b.size(), kPlaces));
				}
			}
		}
		for (int game = 0; game < kGames; ++game) {
			if (publish[game]) {
				Publish(game, changed[game]);
			}
		}
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
				a_intfc->WriteRecordData(&e.gone, sizeof(e.gone));
			}
		}
	}

	void Load(const F4SE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t)
	{
		std::scoped_lock l(g_lock);
		g_published = false;
		for (auto& b : g_boards) {
			b.clear();
		}
		for (int game = 0; game < kGames; ++game) {
			auto& b = g_boards[game];
			// (saved before points went to hundreds: scores x 100; Atomic Rollers' always were, the zapper's are wins)
			const bool hundred = a_version < 2 && game != kRace && game != static_cast<int>(sim::GameId::kAtomicRollers);
			std::uint32_t n = 0;
			if (!a_intfc->ReadRecordData(&n, sizeof(n))) {
				return;
			}
			for (std::uint32_t i = 0; i < n; ++i) {
				Entry e;
				if (!a_intfc->ReadRecordData(&e.actor, sizeof(e.actor)) || !a_intfc->ReadRecordData(&e.score, sizeof(e.score)) ||
					!a_intfc->ReadRecordData(&e.gone, sizeof(e.gone))) {
					return;
				}
				// (not there any more: their place kept, "..." for the name)
				std::optional<std::uint32_t> now;
				if (!e.gone) {
					now = a_intfc->ResolveFormID(e.actor);
				}
				if (now) {
					e.actor = *now;
				} else {
					e.actor = 0;
					e.gone = 1;
				}
				if (hundred) {
					e.score *= 100;
				}
				b.push_back(e);
			}
		}
	}

	void Revert()
	{
		std::scoped_lock l(g_lock);
		g_published = false;
		for (auto& b : g_boards) {
			b.clear();
		}
	}
}
