#include "Scoreboard.h"

#include "Arcade.h"
#include "Forms.h"
#include "Income.h"
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
			RE::NiTransform     placed;  // the machine where it stood when the board went up (moved: put up again)
			Row                 score;
			Row                 high;
			double              refresh{ 0.0 };
			struct Prop
			{
				RE::ObjectRefHandle ref;
				RE::TESForm*        mod{ nullptr };   // (a weapon's: fitted once it's loaded)
				RE::NiPoint3        at;               // where it goes (world)...
				RE::NiMatrix3       rot;              // ...turned so (world: rows its axes)
				bool                weapon{ false };
				bool                settled{ false }; // frozen in place (and, a weapon, made not to be picked up)
				std::shared_ptr<std::atomic<std::uint32_t>> placed;  // a weapon: its ref's id once Papyrus has placed it
				// Atomic Rollers' balls in the pocket: one slot each from the front (home); as balls are taken from the
				// front, the rest roll down the pocket, slot by slot
				bool                rolls{ false };
				RE::NiPoint3        step;             // a slot back up the pocket (world)
				RE::NiMatrix3       machine;          // the machine's turn
				float               home{ 0.0f };
				float               slot{ 0.0f };     // where it is now
				float               turnPerSlot{ 0.0f };  // how far it turns rolling a slot (radians)
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
		RE::ObjectRefHandle Place(RE::TESObjectREFR* a_machine, RE::TESBoundObject* a_form, const RE::NiPoint3& a_at)
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
		// Hoop Shot's at the foot of its ramp, behind the front shelf, the mallet on Whac-a-Commie, a Thirst Zapper on the zapper race's counter in
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
			constexpr float kAlongCounter = 2.0f;
			constexpr float kInHolder = 3.0f;
			// a_side: laid on its side (turned a quarter about its length's crosswise axis: up becomes a_side along x)
			const auto add = [&](RE::TESBoundObject* a_form, const RE::NiPoint3& a_pos, float a_side, RE::TESForm* a_mod, bool a_weapon) {
				if (!a_form) {
					return;
				}
				Board::Prop p;
				if (a_weapon) {
					// placed whole by Papyrus (PlaceAtMe), then laid down here once it's in
					p.placed = std::make_shared<std::atomic<std::uint32_t>>(0);
					auto into = p.placed;
					Papyrus::Call("PlaceWeapon", { Papyrus::Id(a_machine), Papyrus::Id(a_form), Papyrus::Id(a_mod) },
						[into](std::int32_t a_id) { *into = static_cast<std::uint32_t>(a_id); });
				} else {
					p.ref = Place(a_machine, a_form, a_pos);
				}
				p.mod = a_mod;
				p.weapon = a_weapon;
				p.at = a_pos;
				// rows: the prop's axes in the machine's space (a quarter turn about y: z -> (a_side, 0, 0)), then the
				// machine's own turn
				// kAlongCounter: lying across the counter, its barrel to the machine's left, its top toward the targets
				// (lever and trigger toward you): x -> down, y (the barrel) -> left, z (its top) -> into the machine
				// kInHolder: the mallet as Nuka-World leaves it (the machine's whacker01 node): its handle in the holder at
				// the front left corner, its head lying back on the top
				const RE::NiMatrix3 local = a_side == kInHolder ? RE::NiMatrix3(0.9991f, 0.0074f, -0.0422f, 0, -0.0428f, 0.1735f, -0.9839f, 0, 0.0f, 0.9848f, 0.1736f, 0) :
				                            a_side == 0.0f      ? RE::NiMatrix3(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0) :
				                            a_side == kAlongCounter ? RE::NiMatrix3(0, 0, -1, 0, -1, 0, 0, 0, 0, 1, 0, 0) :
				                                                  RE::NiMatrix3(0, 0, -a_side, 0, 0, 1, 0, 0, a_side, 0, 0, 0);
				p.rot = local * w.rotate;
				a_b.props.push_back(p);
			};
			constexpr float kOnItsSide = 1.0f;   // up to the machine's right (a gun lies on its left side)
			switch (a_b.game) {
			case sim::GameId::kAtomicRollers:
				{
					const auto& L = sim::layout::Rollers();
					auto*       ball = Forms::Bound(Forms::Own(Forms::kSkeeBall));
					const RE::NiPoint3 step = w.rotate.Transpose() * (RE::NiPoint3{ 0.0f, static_cast<float>(L.pocketStep), 0.0f } * scale);
					for (int k = 0; k < L.balls; ++k) {
						add(ball, at(L.pocket + sim::Vec3{ 0.0, L.pocketStep * k, 0.0 }), 0.0f, nullptr, false);
						auto& p = a_b.props.back();
						p.rolls = true;
						p.step = step;
						p.machine = w.rotate;
						p.home = p.slot = static_cast<float>(k);
						p.turnPerSlot = static_cast<float>(L.pocketStep / L.ballRadius);
					}
				}
				break;
			case sim::GameId::kHoopShot:
				{
					const auto& L = sim::layout::Hoop();
					auto*       ball = Forms::Bound(Forms::Own(Forms::kBasketball));
					for (double x : { -L.restBallSpacing, 0.0, L.restBallSpacing }) {
						add(ball, at({ x, L.restBallY, L.restBallZ }), 0.0f, nullptr, false);
					}
				}
				break;
			case sim::GameId::kWhacACommie:
				add(Forms::Bound(Forms::Own(Forms::kCommieWhackerProp)), at(sim::layout::Whac().malletRest), kInHolder, nullptr, false);
				break;
			case sim::GameId::kNukaZapperRace:
				add(Forms::WeaponOf(a_b.game), at(sim::layout::Zapper().gunRest), kOnItsSide, nullptr, true);
				break;
			case sim::GameId::kBanditRoundup:
				{
					auto* rifle = Forms::WeaponOf(a_b.game);
					add(rifle, at(sim::layout::Gallery().rifleRest), kAlongCounter, Forms::LentModFor(rifle), true);
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

		bool Moved(const RE::NiTransform& a_now, const RE::NiTransform& a_then)
		{
			if ((a_now.translate - a_then.translate).Length() > 0.5f) {
				return true;
			}
			for (int r = 0; r < 3; ++r) {
				for (int c = 0; c < 3; ++c) {
					if (std::abs(a_now.rotate.entry[r][c] - a_then.rotate.entry[r][c]) > 0.001f) {
						return true;
					}
				}
			}
			return false;
		}

		std::unordered_map<RE::TESFormID, RE::NiTransform> g_lastSeen;  // each machine where it was at the last look

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
				bool keep = m && !m->IsDisabled() && !m->IsDeleted() && m->Get3D() && (m->data.location - me).Length() < kFar;
				if (keep && Moved(m->Get3D()->world, a_b.placed)) {
					keep = false;  // (moved in build mode: its board, digits and things taken down, put up again where it is)
					log::info("{} {:08X}: moved: its board and things to go up where it is now", sim::GameName(a_b.game), a_b.id);
				}
				if (!keep) {
					Remove(a_b);
				}
				return !keep;
			});
			// their takings: each pays its settlement what it's earned (whole days since it last did)
			for (auto& b : g_boards) {
				if (auto m = b.machine.get()) {
					Income::Check(m.get());
				}
			}
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
					// still being moved (where it is differs from the last look): wait till it's put down
					const auto& now = a_ref->Get3D()->world;
					auto [seen, fresh] = g_lastSeen.try_emplace(a_ref->GetFormID(), now);
					if (!fresh && Moved(now, seen->second)) {
						seen->second = now;
						return RE::BSContainer::ForEachResult::kContinue;
					}
					seen->second = now;
					Board b;
					b.machine = a_ref->GetHandle();
					b.id = a_ref->GetFormID();
					b.game = game;
					b.placed = now;
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

	std::vector<RE::ObjectRefHandle> Machines(sim::GameId a_game)
	{
		std::vector<RE::ObjectRefHandle> out;
		for (const auto& b : g_boards) {
			if (b.game == a_game) {
				out.push_back(b.machine);
			}
		}
		return out;
	}

	constexpr float kRollSlotsPerSecond = 5.0f;  // the pocket's balls rolling down

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
			const int taken = static_cast<int>(b.props.size()) - shown;  // (Atomic Rollers: from the front of the pocket)
			for (int k = 0; k < static_cast<int>(b.props.size()); ++k) {
				auto& p = b.props[k];
				if (!p.ref && p.placed && *p.placed != 0) {
					if (auto* form = RE::TESForm::GetFormByID(*p.placed); form && form->As<RE::TESObjectREFR>()) {
						p.ref = form->As<RE::TESObjectREFR>()->GetHandle();
					}
					*p.placed = 0;
				}
				auto  ref = p.ref.get();
				auto* root = ref ? ref->Get3D() : nullptr;
				if (!root) {
					continue;
				}
				// exactly where and how it lies
				const auto lay = [root](const RE::NiPoint3& a_at, const RE::NiMatrix3& a_rot) {
					RE::NiTransform t;
					if (auto* parent = root->parent) {
						const auto& pw = parent->GetWorldTransform();
						const float ps = pw.scale != 0.0f ? pw.scale : 1.0f;
						t.rotate = a_rot * pw.rotate.Transpose();
						t.translate = (pw.rotate * (a_at - pw.translate)) * (1.0f / ps);
						t.scale = 1.0f / ps;
					} else {
						t.rotate = a_rot;
						t.translate = a_at;
						t.scale = 1.0f;
					}
					root->SetLocalTransform(t);
					RE::NiUpdateData update;
					root->Update(update);
					// and where it was last frame, too: a static's isn't updated by the game, so left as it was the
					// renderer sees it forever moving from its old place to its new (a ghost smeared between the
					// two, till something like the pause menu updates everything)
					RE::BSVisit::TraverseScenegraphObjects(root, [](RE::NiAVObject* a_o) {
						a_o->previousWorld = a_o->world;
						return RE::BSVisitControl::kContinue;
					});
				};
				bool moved = false;
				if (p.rolls) {
					// taken from the front; the rest roll down to fill the gap (back up to their places for a new game)
					const float want = static_cast<float>(k - taken);
					if (want >= p.slot) {
						moved = want != p.slot;
						p.slot = want;
					} else {
						p.slot = std::max(want, p.slot - static_cast<float>(a_dt) * kRollSlotsPerSecond);
						moved = true;
					}
				}
				if (!p.settled) {
					p.settled = true;
					moved = true;
					RE::BSVisit::TraverseScenegraphCollision(root, [](RE::bhkNPCollisionObject* a_c) {
						a_c->SetMotionType(RE::hknpMotionPropertiesId::Preset::kKeyframed);
						return RE::BSVisitControl::kContinue;
					});
				}
				if (moved) {
					if (p.rolls) {
						// rolled toward you: turned about the machine's x, its top toward you
						const float a = (p.home - p.slot) * p.turnPerSlot;
						const float c = std::cos(a), sn = std::sin(a);
						const RE::NiMatrix3 local(1, 0, 0, 0, 0, c, sn, 0, 0, -sn, c, 0);
						lay(p.at + p.step * (p.slot - p.home), local * p.machine);
					} else {
						lay(p.at, p.rot);
					}
				}
				const bool hide = p.rolls ? k < taken : k >= shown;
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
