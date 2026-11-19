#include "Scoreboard.h"

#include "Arcade.h"
#include "Forms.h"
#include "Papyrus.h"
#include "sim/Layouts.h"

namespace ag::Scoreboards
{
	namespace
	{
		constexpr float kNear = 1500.0f;     // boards go up within this of you...
		constexpr float kFar = 2500.0f;      // ...and come down past this
		constexpr int   kDigits = 8;

		struct Row
		{
			std::array<RE::ObjectRefHandle, kDigits> refs{};
			std::array<int, kDigits>                 shown{};  // the digit each shows (-1 hidden, -2 not set yet)
		};

		struct Board
		{
			RE::ObjectRefHandle machine;
			RE::TESFormID       id{ 0 };
			sim::GameId         game{ sim::GameId::kCount };
			Row                 score;
			Row                 high;
			double              refresh{ 0.0 };
			struct Prop
			{
				RE::ObjectRefHandle ref;
				RE::TESForm*        mod{ nullptr };   // (a weapon's: fitted once it's loaded)
				bool                weapon{ false };
				bool                settled{ false }; // frozen in place (and, a weapon, made not to be picked up)
			};
			std::vector<Prop> props;  // the machine's things: the balls, the mallet, the gun on the counter
		};

		std::vector<Board> g_boards;
		double             g_scan{ 0.0 };

		RE::TESBoundObject* DigitForm(sim::GameId a_game)
		{
			const bool large = a_game == sim::GameId::kNukaZapperRace || a_game == sim::GameId::kBanditRoundup;
			return Forms::Bound(Forms::Nuka(large ? Forms::kNumbersLarge : Forms::kNumbers));
		}

		void Remove(RE::ObjectRefHandle& a_handle)
		{
			if (auto ref = a_handle.get()) {
				ref->Disable();
				ref->SetWantsDelete(true);
			}
			a_handle = {};
		}

		void Remove(Board& a_b)
		{
			for (auto& p : a_b.props) {
				Remove(p.ref);
			}
			a_b.props.clear();
			for (auto* row : { &a_b.score, &a_b.high }) {
				for (auto& h : row->refs) {
					Remove(h);
				}
			}
		}

		// a digit set: PlayAnimation("Num0" + digit) on it, as DLC04ArcadeScoreNumber does it (through
		// AmusementGamesNative.ShowDigit: a ref of ours has no script to call it on directly)
		bool ShowDigit(RE::TESObjectREFR* a_ref, int a_digit)
		{
			return a_ref && a_ref->Get3D() && Papyrus::Call("ShowDigit", { Papyrus::Id(a_ref), a_digit });
		}

		// a digit's ref at the machine's node: where the node is, turned as the machine is
		RE::ObjectRefHandle Place(RE::TESObjectREFR* a_machine, RE::TESBoundObject* a_form, const RE::NiPoint3& a_at, float a_tilt = 0.0f)
		{
			auto*              dh = RE::TESDataHandler::GetSingleton();
			RE::TESObjectCELL* cell = a_machine->GetParentCell();
			if (!dh || !cell || !a_form) {
				return {};
			}
			// a digit left over in the same spot (saved before boards came down for saves): cleared off
			cell->ForEachReferenceInRange(a_at, 2.0f, [&](RE::TESObjectREFR* a_ref) {
				if (a_ref && a_ref->GetObjectReference() == a_form && !a_ref->IsDisabled()) {
					a_ref->Disable();
					a_ref->SetWantsDelete(true);
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			RE::NEW_REFR_DATA data;
			data.location = a_at;
			data.direction = a_machine->data.angle;
			data.direction.y += a_tilt;  // (a weapon laid on its side)
			data.object = a_form;
			if (cell->IsInterior()) {
				data.interior = cell;
			} else {
				data.world = cell->worldSpace;
			}
			data.initializeScripts = false;  // (Nuka-World's own number script stays out of it: we set the digits)
			return dh->CreateReferenceAtLocation(data);
		}

		// the machine's things, to make it look ready to play: Atomic Rollers' balls waiting in its right-hand pocket,
		// Hoop Shot's on the front shelf, the mallet on Whac-a-Commie, a Thirst Zapper on the zapper race's counter in
		// front of your lane, the rifle on the gallery's counter
		void PutUpProps(Board& a_b, RE::TESObjectREFR* a_machine)
		{
			auto* root = a_machine->Get3D();
			if (!root) {
				return;
			}
			const auto& w = root->world;
			const float scale = w.scale != 0.0f ? w.scale : 1.0f;
			const auto  at = [&](const sim::Vec3& a_local) {
				const RE::NiPoint3 l{ static_cast<float>(a_local.x), static_cast<float>(a_local.y), static_cast<float>(a_local.z) };
				return w.translate + w.rotate.Transpose() * (l * scale);
			};
			const auto add = [&](RE::TESBoundObject* a_form, const RE::NiPoint3& a_pos, float a_tilt, RE::TESForm* a_mod, bool a_weapon) {
				if (!a_form) {
					return;
				}
				Board::Prop p;
				p.ref = Place(a_machine, a_form, a_pos, a_tilt);
				p.mod = a_mod;
				p.weapon = a_weapon;
				a_b.props.push_back(p);
			};
			constexpr float kOnItsSide = 1.5707963f;
			switch (a_b.game) {
			case sim::GameId::kAtomicRollers:
				{
					const auto& L = sim::layout::Rollers();
					auto*       ball = Forms::Bound(Forms::Own(Forms::kSkeeBall));
					for (int k = 0; k < L.balls; ++k) {
						add(ball, at(L.pocket + sim::Vec3{ 0.0, L.pocketStep * k, 0.0 }), 0.0f, nullptr, false);
					}
				}
				break;
			case sim::GameId::kHoopShot:
				{
					const auto& L = sim::layout::Hoop();
					auto*       ball = Forms::Bound(Forms::Own(Forms::kBasketball));
					for (double x : { -24.0, 0.0, 24.0 }) {
						add(ball, at({ x, L.shelfBallY, 57.9 + L.ballRadius }), 0.0f, nullptr, false);
					}
				}
				break;
			case sim::GameId::kWhacACommie:
				if (auto* node = root->GetObjectByName(RE::BSFixedString("whacker01"))) {
					add(Forms::Bound(Forms::Own(Forms::kCommieWhackerProp)), node->world.translate, 0.0f, nullptr, false);
				}
				break;
			case sim::GameId::kNukaZapperRace:
				add(Forms::WeaponOf(a_b.game), at(sim::layout::Zapper().gunRest), kOnItsSide, nullptr, true);
				break;
			case sim::GameId::kBanditRoundup:
				{
					auto* rifle = Forms::WeaponOf(a_b.game);
					add(rifle, at(sim::layout::Gallery().rifleRest), kOnItsSide, Forms::LentModFor(rifle), true);
				}
				break;
			default:
				break;
			}
		}

		bool PutUp(Board& a_b, RE::TESObjectREFR* a_machine)
		{
			auto* root = a_machine->Get3D();
			auto* form = DigitForm(a_b.game);
			if (!root || !form) {
				return false;
			}
			int placed = 0;
			for (int i = 0; i < kDigits; ++i) {
				for (auto [row, prefix] : { std::pair{ &a_b.score, "Score" }, std::pair{ &a_b.high, "HighScore" } }) {
					auto* node = root->GetObjectByName(RE::BSFixedString(std::format("{}{:02}", prefix, i + 1)));
					if (!node) {
						continue;
					}
					row->refs[i] = Place(a_machine, form, node->world.translate);
					row->shown[i] = -2;
					++placed;
				}
			}
			PutUpProps(a_b, a_machine);
			log::info("{} {:08X}: its scoreboard up ({} digits)", sim::GameName(a_b.game), a_machine->GetFormID(), placed);
			return placed > 0;
		}

		// a number on a row: right-aligned, no leading zeros (one 0 for nothing)
		void Show(Row& a_row, int a_value)
		{
			a_value = std::clamp(a_value, 0, 99999999);
			const std::string text = std::to_string(a_value);
			const int         start = kDigits - static_cast<int>(text.size());
			for (int i = 0; i < kDigits; ++i) {
				const int want = i < start ? -1 : text[i - start] - '0';
				if (want == a_row.shown[i]) {
					continue;
				}
				auto ref = a_row.refs[i].get();
				auto* root = ref ? ref->Get3D() : nullptr;
				if (!root) {
					continue;  // not loaded yet: next time
				}
				if (want < 0) {
					root->SetAppCulled(true);
					a_row.shown[i] = -1;
				} else {
					root->SetAppCulled(false);
					if (ShowDigit(ref.get(), want)) {
						a_row.shown[i] = want;
					}
				}
			}
		}

		void Scan()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			const auto me = player->data.location;
			// down: gone, unloaded or far
			std::erase_if(g_boards, [&](Board& a_b) {
				auto m = a_b.machine.get();
				const bool keep = m && !m->IsDisabled() && !m->IsDeleted() && m->Get3D() && (m->data.location - me).Length() < kFar;
				if (!keep) {
					Remove(a_b);
				}
				return !keep;
			});
			// up: our machines near you
			auto* tes = RE::TES::GetSingleton();
			auto  visit = [&](RE::TESObjectCELL* a_cell) {
				if (!a_cell) {
					return;
				}
				a_cell->ForEachReferenceInRange(me, kNear, [&](RE::TESObjectREFR* a_ref) {
					const auto game = Forms::GameOf(a_ref);
					if (game == sim::GameId::kCount || a_ref->IsDisabled() || a_ref->IsDeleted() || !a_ref->Get3D()) {
						return RE::BSContainer::ForEachResult::kContinue;
					}
					if (std::ranges::any_of(g_boards, [&](const Board& a_b) { return a_b.id == a_ref->GetFormID(); })) {
						return RE::BSContainer::ForEachResult::kContinue;
					}
					Board b;
					b.machine = a_ref->GetHandle();
					b.id = a_ref->GetFormID();
					b.game = game;
					if (PutUp(b, a_ref)) {
						g_boards.push_back(std::move(b));
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
			};
			if (tes && tes->interiorCell) {
				visit(tes->interiorCell);
			} else if (tes && tes->gridCells && tes->gridCells->gridCell) {
				const auto dim = std::min<std::uint32_t>(tes->gridCells->dimension, 15);
				for (std::uint32_t i = 0; i < dim * dim; ++i) {
					auto* cell = tes->gridCells->gridCell[i].cell;
					if (cell && cell->IsAttached()) {
						visit(cell);
					}
				}
			} else {
				visit(player->GetParentCell());
			}
		}
	}

	void Tick(double a_dt)
	{
		g_scan -= a_dt;
		if (g_scan <= 0.0) {
			g_scan = 1.0;
			Scan();
		}
		const auto& arcade = Arcade::Get();
		for (auto& b : g_boards) {
			// the props: settled once loaded; on show unless you're playing (then the game says how many: Atomic
			// Rollers' balls still waiting in the pocket)
			const int shown = arcade.PropsShown(b.id, static_cast<int>(b.props.size()));
			for (int k = 0; k < static_cast<int>(b.props.size()); ++k) {
				auto& p = b.props[k];
				auto  ref = p.ref.get();
				auto* root = ref ? ref->Get3D() : nullptr;
				if (!root) {
					continue;
				}
				if (!p.settled) {
					p.settled = true;
					RE::BSVisit::TraverseScenegraphCollision(root, [](RE::bhkNPCollisionObject* a_c) {
						a_c->SetMotionType(RE::hknpMotionPropertiesId::Preset::kKeyframed);
						return RE::BSVisitControl::kContinue;
					});
					if (p.weapon) {
						Papyrus::Call("Prop", { Papyrus::Id(ref.get()), Papyrus::Id(p.mod) });
					}
				}
				const bool hide = k >= shown;
				if (root->GetAppCulled() != hide) {
					root->SetAppCulled(hide);
				}
			}
			// every few seconds every digit's set again (a digit whose animation didn't take: its 3D not ready, a
			// Papyrus call dropped)
			b.refresh -= a_dt;
			if (b.refresh <= 0.0) {
				b.refresh = 4.0;
				b.score.shown.fill(-2);
				b.high.shown.fill(-2);
			}
			Show(b.score, arcade.ScoreOn(b.id));
			Show(b.high, arcade.Best(b.game));
		}
	}

	void Clear()
	{
		for (auto& b : g_boards) {
			Remove(b);
		}
		g_boards.clear();
		g_scan = 0.0;
	}
}
