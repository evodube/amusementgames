#include "Arcade.h"

#include "Forms.h"
#include "Hud.h"
#include "Scoreboard.h"
#include "MenuWatch.h"
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
	}

	// ------------------------------------------------------------------------ listening

	void Arcade::Listen()
	{
		static ActivateSink activate;
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

	// ------------------------------------------------------------------------ starting and ending

	void Arcade::OnActivated(RE::TESObjectREFR* a_machine)
	{
		const auto game = Forms::GameOf(a_machine);
		if (game == sim::GameId::kCount || _playing) {
			return;
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
		BlockControls(true);
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
		if (!_playing && _pieces.empty()) {
			return;
		}
		log::info("{}: dropped ({})", _game ? sim::GameName(_gameId) : "a game", a_why);
		if (auto machine = _machine.get()) {
			Sounds::Music(_gameId, machine.get(), false);
		}
		_playing = false;
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
		PublishHud();
	}

	void Arcade::PlacePieces(const Frame& a_f)
	{
		std::vector<sim::Piece> pieces;
		_game->Pieces(pieces);
		for (std::size_t i = 0; i < pieces.size() && i < _pieces.size(); ++i) {
			auto ref = _pieces[i].get();
			if (!ref) {
				continue;
			}
			// a gallery target's kind changes as it comes round: its ref swapped for the new kind's
			if (pieces[i].model != _pieceModel[i]) {
				const auto models = Forms::PiecesOf(_gameId);
				if (auto* form = pieces[i].model < static_cast<int>(models.size()) ? Forms::Bound(Forms::Own(models[pieces[i].model])) : nullptr) {
					auto machine = _machine.get();
					Remove(_pieces[i]);
					_pieces[i] = Spawn(form, machine.get(), ToWorld(a_f, pieces[i].pos));
					_pieceModel[i] = pieces[i].model;
					ref = _pieces[i].get();
					if (!ref) {
						continue;
					}
				}
			}
			Freeze(ref.get());
			const RE::NiMatrix3 rot = FromRows(pieces[i].rot) * a_f.rot;
			PlaceNode(ref.get(), ToWorld(a_f, pieces[i].pos), rot, static_cast<float>(pieces[i].scale) * a_f.scale, pieces[i].hidden);
		}
		// the fixtures (the zapper's targets): where the layout has them, facing you
		if (_gameId == sim::GameId::kNukaZapperRace) {
			const auto& lanes = sim::layout::Zapper().lanes;
			for (std::size_t i = 0; i < _fixtures.size() && i < lanes.size(); ++i) {
				if (auto ref = _fixtures[i].get()) {
					Freeze(ref.get());
					PlaceNode(ref.get(), ToWorld(a_f, lanes[i].target), a_f.rot, a_f.scale, false);
				}
			}
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

	void Arcade::Freeze(RE::TESObjectREFR* a_ref)
	{
		// the vanilla models have Havok (the balls are physics props): keyframed, so only we move them
		auto* root = a_ref ? a_ref->Get3D() : nullptr;
		if (!root || std::ranges::find(_frozen, root) != _frozen.end()) {
			return;
		}
		_frozen.push_back(root);
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
		a_ref->data.location = a_pos;
	}

	// ------------------------------------------------------------------------ you

	void Arcade::BlockControls(bool a_block)
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
		const auto flags = static_cast<Flag>(
			std::to_underlying(Flag::kMovement) | std::to_underlying(Flag::kActivate) | std::to_underlying(Flag::kPOVSwitch) |
			std::to_underlying(Flag::kFighting) | std::to_underlying(Flag::kSneaking) | std::to_underlying(Flag::kWheelZoom) |
			std::to_underlying(Flag::kJumping) | std::to_underlying(Flag::kVATS));
		manager->EnableUserEvent(_inputLayer->layerID, flags, !a_block, RE::UserEvents::SENDER_ID::kGameplay);
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

	void Arcade::PublishHud()
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
		st.best = _gameId != sim::GameId::kCount ? _best[static_cast<int>(_gameId)] : 0;
		st.tokens = Count(Forms::Token());
		st.tickets = Count(Forms::Ticket());
		std::scoped_lock lock(_hudMutex);
		_hud = std::move(st);
	}

	// ------------------------------------------------------------------------ saves

	void Arcade::Save(const F4SE::SerializationInterface* a_intfc)
	{
		auto& self = Get();
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
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
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
		Get()._best.fill(0);
	}
}
