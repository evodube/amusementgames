#include "Arcade.h"

#include "Income.h"

#include "Forms.h"
#include "Hud.h"
#include "Scoreboard.h"
#include "MenuWatch.h"
#include "Papyrus.h"
#include "Sounds.h"
#include "sim/Layouts.h"

namespace ag
{
	namespace
	{
		constexpr int kLeaveVk = 'X';

		bool GameHasFocus()
		{
			auto*      data = RE::BSGraphics::GetRendererData();
			const HWND window = data ? reinterpret_cast<HWND>(data->renderWindow[0].hwnd) : nullptr;
			return !window || ::GetForegroundWindow() == window;
		}

		RE::NiMatrix3 FromRows(const double a_m[3][3])
		{
			return RE::NiMatrix3(
				static_cast<float>(a_m[0][0]), static_cast<float>(a_m[0][1]), static_cast<float>(a_m[0][2]), 0.0f,
				static_cast<float>(a_m[1][0]), static_cast<float>(a_m[1][1]), static_cast<float>(a_m[1][2]), 0.0f,
				static_cast<float>(a_m[2][0]), static_cast<float>(a_m[2][1]), static_cast<float>(a_m[2][2]), 0.0f);
		}

		struct ActivateSink final : RE::BSTEventSink<RE::TESActivateEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent& a_event, RE::BSTEventSource<RE::TESActivateEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (a_event.objectActivated && a_event.actionRef && a_event.actionRef.get() == player &&
					Forms::GameOf(a_event.objectActivated.get()) != sim::GameId::kCount) {
					auto handle = a_event.objectActivated->GetHandle();
					if (auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddTask([handle]() {
							if (auto ref = handle.get()) {
								Arcade::Get().OnActivated(ref.get());
							}
						});
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// you hitting something: one of the game's pieces (a commie's head, by your Commie Whacker)
		struct HitSink final : RE::BSTEventSink<RE::TESHitEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (a_event.target && a_event.cause && a_event.cause.get() == player) {
					auto handle = a_event.target->GetHandle();
					if (auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddTask([handle]() {
							if (auto ref = handle.get()) {
								Arcade::Get().OnHit(ref.get());
							}
						});
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// a load or the main menu while you're playing: the game's dropped
		struct MenuSink final : RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
			struct Data
			{
				RE::BSFixedString menuName;
				bool              opening;
			};
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				const auto& e = reinterpret_cast<const Data&>(a_event);
				if (e.opening && e.menuName.c_str()) {
					const std::string_view name = e.menuName.c_str();
					if (name == "LoadingMenu" || name == "MainMenu") {
						if (auto* tasks = F4SE::GetTaskInterface()) {
							tasks->AddTask([]() { Arcade::Get().Abort("a loading screen"); });
						}
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		constexpr std::uint32_t kSaveType = 0x41474253;  // 'AGBS': the best scores
		constexpr std::uint32_t kSaveVersion = 1;
		constexpr std::uint32_t kHeldType = 0x4147484C;  // 'AGHL': settlers held at a machine when saved
	}

	// ------------------------------------------------------------------------ listening

	void Arcade::Listen()
	{
		static ActivateSink activate;
		static HitSink      hits;
		static MenuSink     menus;
		static bool         registered = false;
		if (registered) {
			return;
		}
		registered = true;
		if (auto* source = RE::TESActivateEvent::GetEventSource()) {
			source->RegisterSink(&activate);
		} else {
			log::warn("no activate event source: the machines can't be played");
		}
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->RegisterSink<RE::MenuOpenCloseEvent>(&menus);
		}
		if (auto* source = RE::TESHitEvent::GetEventSource()) {
			source->RegisterSink(&hits);
		} else {
			log::warn("no hit event source: Whac-a-Commie's whacks won't count");
		}
		log::info("listening for the machines");
	}

	void Arcade::OnPresent()
	{
		const bool focus = GameHasFocus() && !menus::AnyBlocking();
		_lmb = focus && (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		const bool leave = focus && (::GetAsyncKeyState(kLeaveVk) & 0x8000) != 0;
		if (leave && !_leaveWas) {
			_leavePressed = true;
		}
		_leaveWas = leave;
		if (_tickPending.exchange(true)) {
			return;
		}
		auto* tasks = F4SE::GetTaskInterface();
		if (!tasks) {
			_tickPending = false;
			return;
		}
		tasks->AddTask([this]() {
			Tick();
			_tickPending = false;
		});
	}

	void Arcade::OnHit(RE::TESObjectREFR* a_target)
	{
		if (!_playing || !_game || !a_target) {
			return;
		}
		for (std::size_t i = 0; i < _pieces.size(); ++i) {
			if (auto ref = _pieces[i].get(); ref && ref.get() == a_target) {
				static bool told = false;
				if (!told) {
					told = true;
					log::info("{}: your weapon's hits on its pieces are coming through", sim::GameName(_gameId));
				}
				_game->OnPieceHit(static_cast<int>(i));
				return;
			}
		}
	}

	// ------------------------------------------------------------------------ starting and ending

	void Arcade::OnActivated(RE::TESObjectREFR* a_machine)
	{
		const auto game = Forms::GameOf(a_machine);
		if (game == sim::GameId::kCount || _playing) {
			return;
		}
		if (_show.phase != Show::Phase::kNone && _show.machineID == a_machine->GetFormID()) {
			if (_show.phase == Show::Phase::kPlaying) {
				Notify(std::format("{}: someone's playing. Wait your turn.", sim::GameName(game)));
				return;
			}
			EndShow("you got there first");
		}
		if (!TakeToken()) {
			Notify(std::format("{}: insert a Nuka-Cade token. Buy them at a Token Stand.", sim::GameName(game)));
			return;
		}
		Start(a_machine, game);
	}

	void Arcade::Start(RE::TESObjectREFR* a_machine, sim::GameId a_game)
	{
		Cleanup();
		_game = sim::MakeGame(a_game);
		if (!_game) {
			return;
		}
		_gameId = a_game;
		_machine = a_machine->GetHandle();
		_machineID = a_machine->GetFormID();
		_game->Start(static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
		_playing = true;
		_held = _lmb.load();  // (a button already down when you start doesn't throw)
		_leavePressed = false;
		_hurry = false;
		_resultsLeft = 0.0;
		_last = std::chrono::steady_clock::now();
		// first person: you look where you aim
		if (auto* camera = RE::PlayerCamera::GetSingleton()) {
			_wasThird = camera->QCameraEquals(RE::CameraState::k3rdPerson);
			if (_wasThird) {
				if (auto* first = camera->cameraStates[RE::CameraStates::kFirstPerson].get()) {
					camera->SetState(first);
				}
			}
		}
		Arm(a_game);
		BlockControls(true, _lent.weapon != nullptr);  // (with a weapon: its swing or shot is yours to make)
		// the pieces: one ref for each the game shows (they're moved, shown and hidden every frame)
		std::vector<sim::Piece> pieces;
		_game->Pieces(pieces);
		const auto models = Forms::PiecesOf(a_game);
		Frame      f;
		FrameOf(a_machine, f);
		for (const auto& p : pieces) {
			auto* form = p.model >= 0 && p.model < static_cast<int>(models.size()) ? Forms::Bound(Forms::Own(models[p.model])) : nullptr;
			_pieces.push_back(form ? Spawn(form, a_machine, ToWorld(f, p.pos)) : RE::ObjectRefHandle{});
			_pieceModel.push_back(p.model);
		}
		// the zapper's targets, one a lane
		if (a_game == sim::GameId::kNukaZapperRace) {
			if (auto* target = Forms::Bound(Forms::Own(Forms::kZapperTarget))) {
				for (const auto& lane : sim::layout::Zapper().lanes) {
					_fixtures.push_back(Spawn(target, a_machine, ToWorld(f, lane.target)));
				}
			}
		}
		Sounds::Music(a_game, a_machine, true);
		log::info("{}: a game started ({} pieces)", sim::GameName(a_game), _pieces.size());
		PublishHud();
	}

	void Arcade::Finish(bool a_early)
	{
		if (!_game) {
			return;
		}
		auto       machine = _machine.get();
		const int  score = _game->Score();
		const int  tickets = _game->TicketsFor(score);
		auto&      best = _best[static_cast<int>(_gameId)];
		const bool newBest = score > best && score > 0;
		if (newBest) {
			best = score;
		}
		_lastScores[_machineID] = score;
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (auto* ticket = Forms::Ticket(); player && ticket && tickets > 0) {
			player->AddObjectToContainer(ticket, nullptr, tickets, nullptr, RE::ITEM_REMOVE_REASON::kNone);
		}
		if (machine) {
			Sounds::Music(_gameId, machine.get(), false);
			Sounds::Tickets(machine.get());
			if (newBest && _gameId != sim::GameId::kNukaZapperRace) {
				Sounds::Play(_gameId, sim::Cue::kWin, machine.get(), 1.0);
			}
		}
		log::info("{}: {} with {} points, {} tickets{}", sim::GameName(_gameId), a_early ? "left early" : "game over", score, tickets,
			newBest ? " (a new best)" : "");
		if (!Hud::Showing()) {
			Notify(std::format("{}: {} points, {} tickets{}", sim::GameName(_gameId), score, tickets, newBest ? ". A new best!" : ""));
		}
		{
			std::scoped_lock lock(_hudMutex);
			_hud.results = true;
			_hud.won = tickets;
			_hud.newBest = newBest;
			_hud.score = score;
			_hud.best = best;
			_hud.resultsAge = 0.0;
			_hud.playing = false;
		}
		_resultsLeft = 4.0;
		_playing = false;
		Disarm();
		BlockControls(false);
		if (_wasThird) {
			if (auto* camera = RE::PlayerCamera::GetSingleton()) {
				if (auto* third = camera->cameraStates[RE::CameraStates::k3rdPerson].get()) {
					camera->SetState(third);
				}
			}
			_wasThird = false;
		}
		Cleanup();
	}

	void Arcade::Abort(const char* a_why)
	{
		// a load: a settler's game just dropped (the save being loaded says who to let go)
		if (_show.phase != Show::Phase::kNone || !_held.empty()) {
			_show = {};
			_held.clear();
			_nextShow = 20.0;
		}
		if (!_playing && _pieces.empty()) {
			return;
		}
		log::info("{}: dropped ({})", _game ? sim::GameName(_gameId) : "a game", a_why);
		if (auto machine = _machine.get()) {
			Sounds::Music(_gameId, machine.get(), false);
		}
		_playing = false;
		Disarm();
		BlockControls(false);
		Cleanup();
		std::scoped_lock lock(_hudMutex);
		_hud = {};
	}

	void Arcade::Cleanup()
	{
		for (auto& h : _pieces) {
			Remove(h);
		}
		for (auto& h : _fixtures) {
			Remove(h);
		}
		_pieces.clear();
		_pieceModel.clear();
		_fixtures.clear();
		_frozen.clear();
		_game.reset();
	}

	// ------------------------------------------------------------------------ every frame

	void Arcade::Tick()
	{
		const auto   now = std::chrono::steady_clock::now();
		const double dt = std::clamp(std::chrono::duration<double>(now - _last).count(), 0.0, 0.1);
		_last = now;
		if (!menus::AnyBlocking()) {
			Scoreboards::Tick(dt);
			TickShow(dt);
		}
		if (!_playing) {
			if (_resultsLeft > 0.0) {
				_resultsLeft -= dt;
				std::scoped_lock lock(_hudMutex);
				_hud.resultsAge += dt;
				if (_resultsLeft <= 0.0) {
					_hud = {};
				}
			}
			_leavePressed = false;
			return;
		}
		if (menus::AnyBlocking()) {
			return;  // paused: the game waits
		}
		auto machine = _machine.get();
		Frame f;
		if (!machine || machine->IsDisabled() || machine->IsDeleted() || !FrameOf(machine.get(), f)) {
			Abort("the machine's gone");
			return;
		}
		if (_leavePressed.exchange(false)) {
			Finish(true);
			return;
		}
		// where you look, in the machine's space
		sim::Input in;
		auto*      camera = RE::PlayerCamera::GetSingleton();
		auto*      player = RE::PlayerCharacter::GetSingleton();
		if (!camera || !camera->cameraRoot || !player) {
			return;
		}
		const float  pitch = player->data.angle.x;  // + looks down
		const float  yaw = player->data.angle.z;    // 0 north (+y), clockwise
		RE::NiPoint3 dir{ std::sin(yaw) * std::cos(pitch), std::cos(yaw) * std::cos(pitch), -std::sin(pitch) };
		in.eye = ToLocal(f, camera->cameraRoot->world.translate);
		in.dir = DirToLocal(f, dir).Normalized();
		const bool down = _lmb.load();
		in.pressed = down && !_held;
		in.released = !down && _held;
		in.held = down;
		_held = down;

		TopUpAmmo();
		_game->Update(dt, in);
		for (const auto& s : _game->TakeSounds()) {
			if (s.cue == sim::Cue::kHurry) {
				_hurry = true;
			}
			Sounds::Play(_gameId, s.cue, machine.get(), s.gain);
		}
		PlacePieces(f);
		if (_game->Over()) {
			Finish(false);
			return;
		}
		PublishHud(&f);
	}

	void Arcade::PlacePieces(const Frame& a_f)
	{
		auto machine = _machine.get();
		PlacePieces(a_f, *_game, _gameId, machine.get(), _pieces, _pieceModel, _frozen);
		// the fixtures (the zapper's targets): where the layout has them, facing you
		if (_gameId == sim::GameId::kNukaZapperRace) {
			const auto& lanes = sim::layout::Zapper().lanes;
			for (std::size_t i = 0; i < _fixtures.size() && i < lanes.size(); ++i) {
				if (auto ref = _fixtures[i].get()) {
					Freeze(ref.get(), _frozen);
					PlaceNode(ref.get(), ToWorld(a_f, lanes[i].target), a_f.rot, a_f.scale, false);
				}
			}
		}
	}

	void Arcade::PlacePieces(const Frame& a_f, const sim::Game& a_game, sim::GameId a_id, RE::TESObjectREFR* a_machine,
		std::vector<RE::ObjectRefHandle>& a_pieces, std::vector<int>& a_models, std::vector<RE::NiAVObject*>& a_frozen)
	{
		std::vector<sim::Piece> pieces;
		a_game.Pieces(pieces);
		for (std::size_t i = 0; i < pieces.size() && i < a_pieces.size(); ++i) {
			auto ref = a_pieces[i].get();
			if (!ref) {
				continue;
			}
			// a gallery target's kind changes as it comes round: its ref swapped for the new kind's
			if (pieces[i].model != a_models[i]) {
				const auto models = Forms::PiecesOf(a_id);
				if (auto* form = pieces[i].model < static_cast<int>(models.size()) ? Forms::Bound(Forms::Own(models[pieces[i].model])) : nullptr) {
					Remove(a_pieces[i]);
					a_pieces[i] = Spawn(form, a_machine, ToWorld(a_f, pieces[i].pos));
					a_models[i] = pieces[i].model;
					ref = a_pieces[i].get();
					if (!ref) {
						continue;
					}
				}
			}
			Freeze(ref.get(), a_frozen);
			const RE::NiMatrix3 rot = FromRows(pieces[i].rot) * a_f.rot;
			PlaceNode(ref.get(), ToWorld(a_f, pieces[i].pos), rot, static_cast<float>(pieces[i].scale) * a_f.scale, pieces[i].hidden);
		}
	}

	// ------------------------------------------------------------------------ the machine's space

	bool Arcade::FrameOf(RE::TESObjectREFR* a_machine, Frame& a_out) const
	{
		auto* root = a_machine ? a_machine->Get3D() : nullptr;
		if (!root) {
			return false;
		}
		const auto& w = root->world;
		a_out.origin = w.translate;
		a_out.rot = w.rotate;
		a_out.scale = w.scale != 0.0f ? w.scale : 1.0f;
		return true;
	}

	RE::NiPoint3 Arcade::ToWorld(const Frame& a_f, const sim::Vec3& a_p) const
	{
		const RE::NiPoint3 local{ static_cast<float>(a_p.x), static_cast<float>(a_p.y), static_cast<float>(a_p.z) };
		return a_f.origin + a_f.rot.Transpose() * (local * a_f.scale);
	}

	sim::Vec3 Arcade::ToLocal(const Frame& a_f, const RE::NiPoint3& a_p) const
	{
		const RE::NiPoint3 l = (a_f.rot * (a_p - a_f.origin)) * (1.0f / a_f.scale);
		return { l.x, l.y, l.z };
	}

	sim::Vec3 Arcade::DirToLocal(const Frame& a_f, const RE::NiPoint3& a_d) const
	{
		const RE::NiPoint3 l = a_f.rot * a_d;
		return { l.x, l.y, l.z };
	}

	// ------------------------------------------------------------------------ refs

	RE::ObjectRefHandle Arcade::Spawn(RE::TESBoundObject* a_form, RE::TESObjectREFR* a_near, const RE::NiPoint3& a_pos)
	{
		auto*              dh = RE::TESDataHandler::GetSingleton();
		RE::TESObjectCELL* cell = a_near ? a_near->GetParentCell() : nullptr;
		if (!cell) {
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				cell = player->GetParentCell();
			}
		}
		if (!dh || !cell || !a_form) {
			return {};
		}
		RE::NEW_REFR_DATA data;
		data.location = a_pos;
		data.direction = a_near ? a_near->data.angle : RE::NiPoint3{};
		data.object = a_form;
		if (cell->IsInterior()) {
			data.interior = cell;
		} else {
			data.world = cell->worldSpace;
		}
		data.initializeScripts = false;
		return dh->CreateReferenceAtLocation(data);
	}

	void Arcade::Remove(RE::ObjectRefHandle& a_handle)
	{
		if (auto ref = a_handle.get()) {
			ref->Disable();
			ref->SetWantsDelete(true);
		}
		a_handle = {};
	}

	void Arcade::Freeze(RE::TESObjectREFR* a_ref, std::vector<RE::NiAVObject*>& a_frozen)
	{
		// the vanilla models have Havok (the balls are physics props): keyframed, so only we move them
		auto* root = a_ref ? a_ref->Get3D() : nullptr;
		if (!root || std::ranges::find(a_frozen, root) != a_frozen.end()) {
			return;
		}
		a_frozen.push_back(root);
		RE::BSVisit::TraverseScenegraphCollision(root, [&](RE::bhkNPCollisionObject* a_collision) {
			a_collision->SetMotionType(RE::hknpMotionPropertiesId::Preset::kKeyframed);
			return RE::BSVisitControl::kContinue;
		});
	}

	void Arcade::PlaceNode(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_pos, const RE::NiMatrix3& a_rot, float a_scale, bool a_hidden)
	{
		auto* root = a_ref ? a_ref->Get3D() : nullptr;
		if (!root) {
			return;
		}
		if (root->GetAppCulled() != a_hidden) {
			root->SetAppCulled(a_hidden);
		}
		if (a_hidden) {
			return;
		}
		RE::NiTransform local;
		if (auto* parent = root->parent) {
			const auto& p = parent->GetWorldTransform();
			const float ps = p.scale != 0.0f ? p.scale : 1.0f;
			local.rotate = a_rot * p.rotate.Transpose();
			local.translate = (p.rotate * (a_pos - p.translate)) * (1.0f / ps);
			local.scale = a_scale / ps;
		} else {
			local.rotate = a_rot;
			local.translate = a_pos;
			local.scale = a_scale;
		}
		root->SetLocalTransform(local);
		RE::NiUpdateData update;
		root->Update(update);
		a_ref->data.location.x = a_pos.x;
		a_ref->data.location.y = a_pos.y;
		a_ref->data.location.z = a_pos.z;
	}

	// ------------------------------------------------------------------------ you

	void Arcade::Arm(sim::GameId a_game)
	{
		_lent = {};
		auto* weapon = Forms::WeaponOf(a_game);
		if (!weapon) {
			return;
		}
		_lent.weapon = weapon;
		_lent.added = Count(weapon) == 0;
		_lent.ammo = weapon->weaponData.ammo;
		if (_lent.ammo) {
			_lent.ammoBefore = Count(_lent.ammo);
			_lent.ammoGiven = 200;
			// loaded from the start: a weapon put in your hands comes with its magazine empty, so it's filled to what it
			// holds (and kept there)
			_lent.clip = weapon->weaponData.ammoCapacity;
		}
		auto had = _lent.had;
		Papyrus::CallWithBools("ArmPlayer", Papyrus::Id(weapon), _lent.added, Papyrus::Id(_lent.ammo), _lent.ammoGiven,
			Papyrus::Id(Forms::LentModFor(weapon)),
			[had](std::int32_t a_had) { *had = a_had; });
		log::info("{}: {} in your hands{}", sim::GameName(a_game), weapon->GetFormEditorID() ? weapon->GetFormEditorID() : "its weapon",
			_lent.added ? " (lent)" : "");
	}

	// unlimited ammo while you play: the weapon's magazine is put back to full after every shot, so it's never
	// reloaded (the rounds come off the lent ammo, all taken back after: your own ammo's never touched)
	void Arcade::TopUpAmmo()
	{
		if (!_lent.weapon || !_lent.ammo) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		const RE::BGSEquipIndex hand{ 0 };
		const auto              now = player->GetCurrentAmmoCount(hand);
		if (now > _lent.clip) {
			_lent.clip = now;
		} else if (now < _lent.clip) {
			player->SetCurrentAmmoCount(hand, _lent.clip);
		}
	}

	void Arcade::Disarm()
	{
		if (!_lent.weapon) {
			return;
		}
		// the ammo lent back: what's left of it (yours, from before, stays)
		const int back = _lent.ammo ? std::clamp(Count(_lent.ammo) - _lent.ammoBefore, 0, _lent.ammoGiven) : 0;
		Papyrus::CallWithBools("DisarmPlayer", Papyrus::Id(_lent.weapon), _lent.added, Papyrus::Id(_lent.ammo), back, _lent.had->load());
		_lent = {};
	}

	void Arcade::BlockControls(bool a_block, bool a_allowFighting)
	{
		if (a_block == _controlsBlocked) {
			return;
		}
		auto* manager = RE::BSInputEnableManager::GetSingleton();
		if (!manager) {
			return;
		}
		if (!_inputLayer && !manager->AllocateNewLayer(_inputLayer, "AmusementGames machine")) {
			log::warn("could not get an input layer; controls stay on at the machine");
			return;
		}
		using Flag = RE::UserEvents::USER_EVENT_FLAG;
		// (fighting's left alone with a weapon in your hands: putting it back on when you're done would be no change)
		static bool fightingHeld = false;
		const bool  holdFighting = a_block ? !a_allowFighting : fightingHeld;
		const auto  flags = static_cast<Flag>(
            std::to_underlying(Flag::kMovement) | std::to_underlying(Flag::kActivate) | std::to_underlying(Flag::kPOVSwitch) |
            (holdFighting ? std::to_underlying(Flag::kFighting) : 0) | std::to_underlying(Flag::kSneaking) |
            std::to_underlying(Flag::kWheelZoom) | std::to_underlying(Flag::kJumping) | std::to_underlying(Flag::kVATS));
		manager->EnableUserEvent(_inputLayer->layerID, flags, !a_block, RE::UserEvents::SENDER_ID::kGameplay);
		fightingHeld = a_block && holdFighting;
		_controlsBlocked = a_block;
	}

	int Arcade::Count(RE::TESBoundObject* a_item) const
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		return player && a_item ? static_cast<int>(player->GetInventoryObjectCount(a_item)) : 0;
	}

	bool Arcade::TakeToken()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* token = Forms::Token();
		if (!player || !token || Count(token) < 1) {
			return false;
		}
		RE::TESObjectREFR::RemoveItemData data(token, 1);
		player->RemoveItem(data);
		return true;
	}

	void Arcade::Notify(const std::string& a_text)
	{
		RE::SendHUDMessage::ShowHUDMessage(a_text.c_str(), nullptr, false, false);
	}

	void Arcade::PublishHud(const Frame* a_frame)
	{
		HudState st;
		st.playing = _playing && _game;
		st.game = _gameId;
		if (_game) {
			st.score = _game->Score();
			_game->Hud(st.info);
			st.lastPoints = _game->lastPoints;
			st.sinceLastPoints = _game->sinceLastPoints;
		}
		// the aim line onto the screen, as fo4bowling does it
		if (a_frame && !st.info.guide.empty()) {
			if (auto* camera = RE::Main::WorldRootCamera()) {
				for (const auto& p : st.info.guide) {
					float x = 0.0f, y = 0.0f, z = 0.0f;
					if (camera->WorldPtToScreenPt3(ToWorld(*a_frame, p), x, y, z, 1e-5f) && z > 0.0f) {
						st.guide.push_back({ x, 1.0f - y });
					}
				}
			}
		}
		st.best = _gameId != sim::GameId::kCount ? _best[static_cast<int>(_gameId)] : 0;
		st.tokens = Count(Forms::Token());
		st.tickets = Count(Forms::Ticket());
		std::scoped_lock lock(_hudMutex);
		_hud = std::move(st);
	}


	// ------------------------------------------------------------------------ settlers at the games

	namespace
	{
		constexpr double    kShowFirst = 20.0;                 // the first chance after you're about
		constexpr double    kShowRetry = 20.0;                 // no one free: try again in this long
		constexpr double    kShowGapShortest = 45.0, kShowGapLongest = 120.0;  // between games
		constexpr double    kShowWalk = 45.0;                  // not there by then: called off
		constexpr float     kShowReach = 3000.0f;              // settlers this near the machine
		constexpr float     kShowYouAway = 3000.0f;            // you this far from it: the game's called off
		const sim::Vec3     kGalleryStand{ 0.0, -75.0, 0.0 };  // where they stand (the counter's front at -32)
		constexpr double    kGalleryEyeHeight = 115.0;         // their eye (the rifle's sights) above it
	}

	void Arcade::Release(RE::TESFormID a_actor, RE::TESFormID a_weapon, bool a_added)
	{
		Papyrus::Call("NpcEnd", { static_cast<std::int32_t>(a_actor), static_cast<std::int32_t>(a_weapon), a_added ? 1 : 0 });
	}

	void Arcade::ReleaseHeld()
	{
		for (const auto& h : _heldFromSave) {
			log::info("a settler ({:08X}) held at a game when saved: let go", h.actor);
			Release(h.actor, h.weapon, h.added);
		}
		_heldFromSave.clear();
	}

	void Arcade::TickShow(double a_dt)
	{
		// settlers on their way when a game was called off: let go once they get there
		std::erase_if(_held, [&](const Held& a_h) {
			const auto r = a_h.ready ? a_h.ready->load() : 1;
			if (r < 0) {
				return false;
			}
			if (r > 0) {
				Release(a_h.actor, a_h.weapon, r == 2);
			}
			return true;
		});

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (_show.phase == Show::Phase::kNone) {
			_nextShow -= a_dt;
			if (_nextShow <= 0.0) {
				_nextShow = StartShow() ? 0.0 : kShowRetry;
			}
			return;
		}
		auto machine = _show.machine.get();
		auto actor = _show.actor.get();
		if (!machine || machine->IsDisabled() || machine->IsDeleted() || !player ||
			(machine->data.location - player->data.location).Length() > kShowYouAway) {
			EndShow("the machine's gone or you've left");
			return;
		}
		if (!actor || actor->IsDead(false) || !actor->Get3D() || actor->IsInCombat()) {
			EndShow("the settler's gone, down or fighting");
			return;
		}
		if (_playing && _machineID == _show.machineID) {
			EndShow("you're at it");
			return;
		}
		Frame f;
		if (!FrameOf(machine.get(), f)) {
			EndShow("the machine's not loaded");
			return;
		}

		if (_show.phase == Show::Phase::kWalking) {
			const auto r = _show.ready->load();
			if (r == 0) {
				EndShow("the settler couldn't come");
				return;
			}
			if (r < 0) {
				_show.walkLeft -= a_dt;
				if (_show.walkLeft <= 0.0) {
					EndShow("the settler took too long");
				}
				return;
			}
			// there, rifle up: the game starts, its targets put up
			_show.added = r == 2;
			_show.game = sim::MakeGame(_show.gameId);
			const auto seed = static_cast<std::uint32_t>(_showRng());
			_show.game->Start(seed);
			const double spread = std::uniform_real_distribution<double>(0.006, 0.03)(_showRng);  // how good a shot
			_show.shooter = std::make_unique<sim::GalleryShooter>(seed ^ 0x5EED, spread);
			std::vector<sim::Piece> pieces;
			_show.game->Pieces(pieces);
			const auto models = Forms::PiecesOf(_show.gameId);
			for (const auto& p : pieces) {
				auto* form = p.model >= 0 && p.model < static_cast<int>(models.size()) ? Forms::Bound(Forms::Own(models[p.model])) : nullptr;
				_show.pieces.push_back(form ? Spawn(form, machine.get(), ToWorld(f, p.pos)) : RE::ObjectRefHandle{});
				_show.pieceModel.push_back(p.model);
			}
			Sounds::Music(_show.gameId, machine.get(), true);
			_show.phase = Show::Phase::kPlaying;
			log::info("{}: a settler ({:08X}) playing (spread {:.3f})", sim::GameName(_show.gameId), _show.actorID, spread);
			return;
		}

		// playing: the shooter aims and fires; the game plays it
		const sim::Input in = _show.shooter->Next(*_show.game, _show.eye, a_dt);
		_show.game->Update(a_dt, in);
		if (in.pressed && _show.weapon) {
			Papyrus::Call("NpcShoot", { static_cast<std::int32_t>(_show.actorID), Papyrus::Id(_show.weapon), Papyrus::Id(_show.weapon->weaponData.ammo) });
		}
		for (const auto& s : _show.game->TakeSounds()) {
			Sounds::Play(_show.gameId, s.cue, machine.get(), s.gain);
		}
		PlacePieces(f, *_show.game, _show.gameId, machine.get(), _show.pieces, _show.pieceModel, _show.frozen);
		if (_show.game->Over()) {
			_lastScores[_show.machineID] = _show.game->Score();
			log::info("{}: the settler ({:08X}) scored {}", sim::GameName(_show.gameId), _show.actorID, _show.game->Score());
			EndShow(nullptr);
		}
	}

	bool Arcade::StartShow()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* lists = RE::ProcessLists::GetSingleton();
		auto* kw = Forms::WorkshopItemKeyword();
		auto* weapon = Forms::WeaponOf(sim::GameId::kBanditRoundup);
		auto* markerForm = Forms::Bound(RE::TESForm::GetFormByID(Forms::kXMarkerHeading));
		if (!player || !lists || !kw || !weapon || !markerForm) {
			return false;
		}
		for (auto& h : Scoreboards::Machines(sim::GameId::kBanditRoundup)) {
			auto machine = h.get();
			if (!machine || (_playing && _machineID == machine->GetFormID())) {
				continue;
			}
			auto* workshop = machine->GetLinkedRef(kw);
			if (!workshop) {
				continue;
			}
			// the settlers of its settlement near it
			std::vector<RE::Actor*> settlers;
			for (auto& ah : lists->highActorHandles) {
				auto a = ah.get();
				if (!a || a.get() == player || a->IsDead(false) || a->IsDisabled() || !a->Get3D() || a->IsInCombat() ||
					a->GetLinkedRef(kw) != workshop || (a->data.location - machine->data.location).Length() > kShowReach) {
					continue;
				}
				settlers.push_back(a.get());
			}
			if (settlers.empty()) {
				continue;
			}
			auto* who = settlers[std::uniform_int_distribution<std::size_t>(0, settlers.size() - 1)(_showRng)];
			Frame f;
			if (!FrameOf(machine.get(), f)) {
				continue;
			}
			_show = {};
			_show.marker = Spawn(markerForm, machine.get(), ToWorld(f, kGalleryStand));
			auto marker = _show.marker.get();
			if (!marker) {
				_show = {};
				return false;
			}
			_show.phase = Show::Phase::kWalking;
			_show.actor = RE::ActorHandle(who);
			_show.actorID = who->GetFormID();
			_show.machine = h;
			_show.machineID = machine->GetFormID();
			_show.gameId = sim::GameId::kBanditRoundup;
			_show.eye = kGalleryStand + sim::Vec3{ 0.0, 0.0, kGalleryEyeHeight };
			_show.weapon = weapon;
			_show.walkLeft = kShowWalk;
			_show.ready = std::make_shared<std::atomic<std::int32_t>>(-1);
			auto ready = _show.ready;
			Papyrus::Call("NpcStartGallery", { static_cast<std::int32_t>(_show.actorID), Papyrus::Id(weapon), Papyrus::Id(marker.get()) },
				[ready](std::int32_t a_r) { *ready = a_r; });
			log::info("{}: a settler ({:08X}) coming to play", sim::GameName(_show.gameId), _show.actorID);
			return true;
		}
		return false;
	}

	void Arcade::EndShow(const char* a_why)
	{
		if (_show.phase == Show::Phase::kNone) {
			return;
		}
		if (a_why) {
			log::info("{}: the settler's game called off ({})", sim::GameName(_show.gameId), a_why);
		}
		const auto weaponID = _show.weapon ? _show.weapon->GetFormID() : 0;
		if (_show.phase == Show::Phase::kPlaying) {
			Release(_show.actorID, weaponID, _show.added);
			if (auto machine = _show.machine.get()) {
				Sounds::Music(_show.gameId, machine.get(), false);
			}
		} else if (_show.ready) {
			_held.push_back({ _show.ready, _show.actorID, weaponID, false });  // (let go once they're there)
		}
		for (auto& h : _show.pieces) {
			Remove(h);
		}
		Remove(_show.marker);
		_show = {};
		_nextShow = std::uniform_real_distribution<double>(kShowGapShortest, kShowGapLongest)(_showRng);
	}

	// ------------------------------------------------------------------------ saves

	void Arcade::Save(const F4SE::SerializationInterface* a_intfc)
	{
		auto& self = Get();
		Income::Save(a_intfc);
		// settlers held at a machine (AI off): let go after a load of this save
		{
			std::vector<Held> held;
			if (self._show.phase == Show::Phase::kPlaying) {
				held.push_back({ nullptr, self._show.actorID, self._show.weapon ? self._show.weapon->GetFormID() : 0, self._show.added });
			} else if (self._show.phase == Show::Phase::kWalking) {
				held.push_back({ nullptr, self._show.actorID, self._show.weapon ? self._show.weapon->GetFormID() : 0, false });
			}
			for (const auto& h : self._held) {
				held.push_back({ nullptr, h.actor, h.weapon, h.added });
			}
			if (!held.empty() && a_intfc->OpenRecord(kHeldType, 1)) {
				const std::uint32_t n = static_cast<std::uint32_t>(held.size());
				a_intfc->WriteRecordData(&n, sizeof(n));
				for (const auto& h : held) {
					const std::uint32_t added = h.added ? 1 : 0;
					a_intfc->WriteRecordData(&h.actor, sizeof(h.actor));
					a_intfc->WriteRecordData(&h.weapon, sizeof(h.weapon));
					a_intfc->WriteRecordData(&added, sizeof(added));
				}
			}
		}
		if (!a_intfc->OpenRecord(kSaveType, kSaveVersion)) {
			return;
		}
		const std::uint32_t n = static_cast<std::uint32_t>(self._best.size());
		a_intfc->WriteRecordData(&n, sizeof(n));
		for (const auto b : self._best) {
			const std::int32_t v = b;
			a_intfc->WriteRecordData(&v, sizeof(v));
		}
	}

	void Arcade::Load(const F4SE::SerializationInterface* a_intfc)
	{
		auto&         self = Get();
		std::uint32_t type = 0, version = 0, length = 0;
		self._best.fill(0);
		Income::Revert();
		self._heldFromSave.clear();
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type == kHeldType) {
				std::uint32_t n = 0;
				a_intfc->ReadRecordData(&n, sizeof(n));
				for (std::uint32_t i = 0; i < n; ++i) {
					RE::TESFormID actor = 0, weapon = 0;
					std::uint32_t added = 0;
					if (!a_intfc->ReadRecordData(&actor, sizeof(actor)) || !a_intfc->ReadRecordData(&weapon, sizeof(weapon)) ||
						!a_intfc->ReadRecordData(&added, sizeof(added))) {
						break;
					}
					const auto a = a_intfc->ResolveFormID(actor);
					const auto w = a_intfc->ResolveFormID(weapon);
					if (a) {
						self._heldFromSave.push_back({ nullptr, *a, w ? *w : 0, added != 0 });
					}
				}
				continue;
			}
			if (type == Income::kSaveType) {
				Income::Load(a_intfc, version, length);
				continue;
			}
			if (type != kSaveType) {
				continue;
			}
			std::uint32_t n = 0;
			if (!a_intfc->ReadRecordData(&n, sizeof(n))) {
				break;
			}
			for (std::uint32_t i = 0; i < n; ++i) {
				std::int32_t v = 0;
				if (!a_intfc->ReadRecordData(&v, sizeof(v))) {
					break;
				}
				if (i < self._best.size()) {
					self._best[i] = v;
				}
			}
		}
	}

	void Arcade::Revert(const F4SE::SerializationInterface*)
	{
		Income::Revert();
		Get()._best.fill(0);
	}
}
