#include "Arcade.h"

#include "Income.h"

#include "Forms.h"
#include "Hud.h"
#include "Scoreboard.h"
#include "Settings.h"
#include "MenuWatch.h"
#include "Papyrus.h"
#include "Sounds.h"
#include "sim/Layouts.h"

#include <cctype>

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

		// the Activate button, as the game reads it (whatever it's bound to): how long it's held, watched and never
		// handled (the game's activation goes on as ever)
		struct ActivateWatch final : RE::BSInputEventUser
		{
			bool ShouldHandleEvent(const RE::InputEvent* a_event) override
			{
				if (const auto* b = a_event ? a_event->As<RE::ButtonEvent>() : nullptr) {
					static const RE::BSFixedString activate{ "Activate" };
					if (b->QRawUserEvent() == activate) {
						Arcade::Get().OnActivateButton(b->QPressed(), b->QHeldDownSecs());
					}
				}
				return false;
			}
		};

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
				if (a_event.target && a_event.cause) {
					auto       handle = a_event.target->GetHandle();
					const bool you = a_event.cause.get() == player;
					const auto cause = a_event.cause->GetFormID();
					if (auto* tasks = F4SE::GetTaskInterface()) {
						tasks->AddTask([handle, you, cause]() {
							if (auto ref = handle.get()) {
								if (you) {
									Arcade::Get().OnHit(ref.get());
								} else {
									Arcade::Get().OnSettlerHit(ref.get(), cause);  // (a settler's mallet on a head)
								}
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
		constexpr std::uint32_t kSwingType = 0x41475357;  // 'AGSW': the swing quickening on when saved
		constexpr std::uint32_t kHeldType = 0x4147484C;  // 'AGHL': settlers held at a machine when saved
	}

	// ------------------------------------------------------------------------ listening

	void Arcade::OnActivateButton(bool a_down, float a_held)
	{
		static bool told = false;
		if (!told) {
			told = true;
			log::info("the Activate button's coming through (a hold at Bandit Roundup: a 1v1)");
		}
		_activateDown = a_down;
		_activateHeld = static_cast<double>(a_held);
	}

	void Arcade::Listen()
	{
		static auto*        watch = new ActivateWatch();
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
		if (auto* controls = RE::MenuControls::GetSingleton()) {
			controls->RegisterHandler(watch);  // (the Activate button watched: Bandit Roundup tapped or held)
		} else {
			log::warn("no menu controls: Bandit Roundup can't tell a hold from a tap (always alone)");
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

	// a settler's swing landing on one of their game's pieces (Whac-a-Commie's heads): the only way they score there
	void Arcade::OnSettlerHit(RE::TESObjectREFR* a_target, RE::TESFormID a_cause)
	{
		for (auto& sh : _shows) {
			if (sh.phase != Show::Phase::kPlaying || sh.actorID != a_cause || !sh.game || !sh.whacker) {
				continue;
			}
			for (std::size_t i = 0; i < sh.pieces.size(); ++i) {
				if (auto ref = sh.pieces[i].get(); ref && ref.get() == a_target) {
					sh.game->OnPieceHit(static_cast<int>(i));
					++sh.hits;
					if (sh.hits == 1) {
						log::info("{}: {}'s mallet is landing on the heads", sim::GameName(sh.gameId), sh.name);
					}
					return;
				}
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
		// waiting for a settler to take you on at Bandit Roundup: activated again, you leave (token back)
		if (_duel.on && !_duel.started) {
			if (_duel.machineID == a_machine->GetFormID()) {
				EndDuel("Bandit Roundup: you've left. Your token's back.", true);
			} else {
				Notify("Bandit Roundup: you're waiting for a settler to take you on. Activate it again to leave.");
			}
			return;
		}
		for (auto& sh : _shows) {
			if (sh.phase == Show::Phase::kNone || sh.machineID != a_machine->GetFormID()) {
				continue;
			}
			if (sh.phase == Show::Phase::kPlaying) {
				Notify(std::format("{}: someone's playing. Wait your turn.", sim::GameName(game)));
				return;
			}
			EndShow(sh, "you got there first");
		}
		Race* race = RaceOn(a_machine->GetFormID());
		if (race && race->phase == 2) {
			Notify(std::format("{}: a race is on. Wait your turn.", sim::GameName(game)));
			return;
		}
		if (game == sim::GameId::kNukaZapperRace) {
			// no racing on your own: you race settlers. Your token in, you wait for one to be ready; 10 seconds after,
			// the race starts (activate again to leave, your token back)
			if (race && race->youIn) {
				race->youIn = false;
				GiveToken();
				Notify("Nuka-Zapper Race: you've left the race. Your token's back.");
				return;
			}
			if (!TakeToken()) {
				Notify(std::format("{}: insert a Nuka-Cade token. Buy them at a Token Stand.", sim::GameName(game)));
				return;
			}
			if (!race) {
				Race r;
				r.phase = 1;
				r.machine = a_machine->GetHandle();
				r.machineID = a_machine->GetFormID();
				_races.push_back(std::move(r));
				race = &_races.back();
				InviteRacer(*race);
			}
			race->youIn = true;
			if (race->countdown < 0.0 && std::ranges::any_of(race->racers, [](const Racer& a_q) { return a_q.state == 2; })) {
				race->countdown = sim::layout::Zapper().gatherSeconds;
			}
			Notify(race->countdown >= 0.0 ? std::format("Nuka-Zapper Race: the race starts in {:.0f} seconds. Get behind your gun.", race->countdown) :
			                                "Nuka-Zapper Race: racers are on their way. The race starts 10 seconds after one's ready.");
			log::info("NUKA-ZAPPER RACE {:08X}: you're in, waiting for racers", race->machineID);
			return;
		}
		if (game == sim::GameId::kBanditRoundup) {
			// tapped or held? (held: a settler takes you on). Known once it's let go, or held long enough
			_pending.machine = a_machine->GetHandle();
			_pending.age = 0.0;
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
		_lastDuel.erase(_machineID);
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
		if (int left = 0, right = 0; sim::GalleryVersus(*_game) && DuelScoresOn(_machineID, left, right)) {
			_lastDuel[_machineID] = { left, right };  // (on the board, each on its side, till the next game there)
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
			{
				sim::HudInfo info;
				_game->Hud(info);
				_hud.verdict = info.verdict;
			}
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
		_duel = {};
		_pending = {};
		if (!_shows.empty() || !_letGo.empty() || !_races.empty()) {
			_shows.clear();
			_races.clear();
			_rest.clear();
			_letGo.clear();
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
		if (_duel.on && _duel.started) {
			EndDuel(nullptr, false);  // (your 1v1 over: your rival let go)
		}
		if (Race* race = RaceOn(_machineID); race && race->withYou) {
			EndRace(*race, nullptr);  // (your race with settlers over: they're let go)
		}
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
		ResolvePending(dt);
		if (!menus::AnyBlocking()) {
			Scoreboards::Tick(dt);
			TickDuel();
			TickShow(dt);
			TickRaces(dt);
			TickRedress(dt);
		} else if (menus::Open(menus::kWorkshop) && !menus::Open(menus::kPause) && !menus::Open(menus::kPipboy) &&
				   !menus::Open(menus::kLoading) && !menus::Open(menus::kMain) && !menus::Open(menus::kConsole)) {
			Scoreboards::Tick(dt);  // (build mode: a machine moved gets its board and things back up where it's put)
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
		if (Race* race = _gameId == sim::GameId::kNukaZapperRace ? RaceOn(_machineID) : nullptr; race && race->withYou) {
			RaceShots(*race, *_game, dt);  // (settlers racing you: their squirts shown)
		}
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
		std::vector<RE::ObjectRefHandle>& a_pieces, std::vector<int>& a_models, std::vector<RE::NiAVObject*>& a_frozen,
		const std::vector<bool>* a_keepHidden)
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
			const bool hide = pieces[i].hidden || (a_keepHidden && i < a_keepHidden->size() && (*a_keepHidden)[i]);
			PlaceNode(ref.get(), ToWorld(a_f, pieces[i].pos), rot, static_cast<float>(pieces[i].scale) * a_f.scale, hide);
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

	void Arcade::PlaceNode(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_pos, const RE::NiMatrix3& a_rot, float a_scale, bool a_hidden, bool a_moveHidden)
	{
		auto* root = a_ref ? a_ref->Get3D() : nullptr;
		if (!root) {
			return;
		}
		if (root->GetAppCulled() != a_hidden) {
			root->SetAppCulled(a_hidden);
		}
		if (a_hidden && !a_moveHidden) {
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
		// where it was last frame kept as such (the renderer's motion: a static's isn't kept by the game, and left at
		// where it was spawned it would smear a ghost)
		RE::BSVisit::TraverseScenegraphObjects(root, [](RE::NiAVObject* a_o) {
			a_o->previousWorld = a_o->world;
			return RE::BSVisitControl::kContinue;
		});
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
			// (a modular gun's base record can say 0 rounds, its magazine coming from its mods, as the Thirst Zapper's
			// seems to: then a full-looking load all the same, kept so)
			_lent.clip = weapon->weaponData.ammoCapacity > 0 ? weapon->weaponData.ammoCapacity : 20u;
		} else if (weapon->weaponData.type == RE::WEAPON_TYPE::kGun) {
			_lent.clip = weapon->weaponData.ammoCapacity > 0 ? weapon->weaponData.ammoCapacity : 20u;  // (no ammo of its own)
		}
		auto had = _lent.had;
		Papyrus::CallWithBools("ArmPlayer", Papyrus::Id(weapon), _lent.added, Papyrus::Id(_lent.ammo), _lent.ammoGiven,
			Papyrus::Id(Forms::LentModFor(weapon)),
			[had](std::int32_t a_had) { *had = a_had; });
		log::info("{}: {} in your hands{} (ammo {:08X}, kept loaded with {} rounds; its record says {})", sim::GameName(a_game),
			weapon->GetFormEditorID() ? weapon->GetFormEditorID() : "its weapon", _lent.added ? " (lent)" : "",
			_lent.ammo ? _lent.ammo->GetFormID() : 0, _lent.clip, weapon->weaponData.ammoCapacity);
		if (a_game == sim::GameId::kWhacACommie) {
			Quicken(true);
		}
	}

	namespace
	{
		const RE::ActorValueInfo* AV(const char* a_editorID)
		{
			return RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>(a_editorID);
		}

	}

	void Arcade::Quicken(bool a_on)
	{
		auto*       player = RE::PlayerCharacter::GetSingleton();
		const auto* speed = AV("WeaponSpeedMult");
		if (!player || !speed) {
			if (a_on && !speed) {
				log::warn("Whac-a-Commie: no WeaponSpeedMult actor value found: your swing can't be quickened");
			}
			return;
		}
		if (!a_on) {
			if (_swingBoost != 0.0f) {
				player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *speed, -_swingBoost);
				_swingBoost = 0.0f;
			}
			return;
		}
		const auto& set = Settings::Get();
		if (!set.swingOn) {
			return;
		}
		const auto* agility = AV("Agility");
		const float agi = agility ? player->GetActorValue(*agility) : 1.0f;
		const float mult = std::clamp(1.0f + set.swingPerAgility / 100.0f * (agi - 1.0f), 1.0f, set.swingMost);
		// (0: the game's default, as quick as normal)
		const float now = player->GetActorValue(*speed);
		const float base = now > 0.0f ? now : 1.0f;
		_swingBoost = base * mult - now;
		if (_swingBoost != 0.0f) {
			player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *speed, _swingBoost);
		}
		log::info("Whac-a-Commie: Agility {}: your swing x{:.2f} (WeaponSpeedMult {:.2f} -> {:.2f})", agi, mult, now,
			player->GetActorValue(*speed));
	}

	// unlimited ammo while you play: the weapon's magazine is put back to full after every shot, so it's never
	// reloaded (the rounds come off the lent ammo, all taken back after: your own ammo's never touched)
	void Arcade::TopUpAmmo()
	{
		if (!_lent.weapon || _lent.clip == 0) {
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
		Quicken(false);
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

	void Arcade::GiveToken()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (auto* token = Forms::Token(); player && token) {
			player->AddObjectToContainer(token, nullptr, 1, nullptr, RE::ITEM_REMOVE_REASON::kNone);
		}
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
		constexpr double    kShowNextTry = 5.0;                // the one asked couldn't come: someone else this soon
		constexpr double    kShowTurnAbout = 120.0;            // a settler who's played a game: not asked to it again for this long
		constexpr double    kShowWalk = 90.0;                  // not there by then: near enough, or called off
		constexpr float     kShowArrived = 12.0f;              // this near the spot: there
		constexpr float     kShowAtSpot = 40.0f;               // ...or this near and no closer for a moment
		constexpr double    kShowStallResend = 6.0;            // no closer for this long: the walk sent again
		constexpr float     kShowStuckNear = 300.0f;           // ...stuck this near after two of those: near enough
		constexpr int       kShowStuckResends = 3;             // ...further off, after this many: someone else
		constexpr float     kShowReach = 3000.0f;              // settlers this near the machine
		constexpr float     kShowYouAway = 3000.0f;            // you this far from it: the game's called off
		constexpr double    kGalleryEyeHeight = 115.0;         // their eye (the rifle's sights) above it
		constexpr double    kSteadyBeforeShot = 0.4;           // the gun drawn this long before a shot (out, then fire)
		constexpr float     kHandJump = 25.0f;                 // a settler's hand moved further than this in a frame: a jump
		constexpr double    kDuelWait = 90.0;                  // a 1v1's settler, ready: their rival not ready by then, called off
		constexpr double    kHoldToDuel = 0.5;                 // Bandit Roundup's Activate held this long: a 1v1
		constexpr double    kDuelMiddle = 20.0;                // you this near the counter's middle: your side picked at random
		constexpr int       kDuelTries = 3;                    // settlers called to take you on (one couldn't come: another)

		// a name as the HUD shows it
		std::string Upper(std::string a_s)
		{
			for (auto& c : a_s) {
				c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			}
			return a_s;
		}
		constexpr float     kWhacMalletReach = 30.0f;          // a settler's mallet head this near an up head's middle: a hit
		                                                       // (they swing straight ahead, still: the holes are 48 across)
		constexpr float     kWhacHeadMiddle = 8.0f;            // ...its middle above where it's placed
		constexpr double    kWhacSwingLong = 1.6;              // ...a swing followed this long (the Papyrus call's wait, the
		                                                       // turn and the swing's wind-up before it comes down)
		constexpr double    kWhacSwingDown = 0.6;              // ...by then it's come down
		constexpr double    kZapperAimUnder = 59.0;            // a zapper racer's aim point, under their target's centre...
		constexpr double    kZapperAimRight = -12.0;           // ...and to its right (their stream went in above and right)
	}

	std::vector<Arcade::Worn> Arcade::WornBy(RE::Actor* a_actor)
	{
		std::vector<Worn> out;
		auto*             list = a_actor ? a_actor->inventoryList : nullptr;
		if (!list) {
			return out;
		}
		RE::BSAutoReadLock lock{ list->rwLock };
		for (auto& item : list->data) {
			if (!item.object || !item.object->Is(RE::ENUM_FORM_ID::kARMO)) {
				continue;
			}
			std::uint32_t id = 0;
			for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++id) {
				if (stack->IsEquipped()) {
					Worn w;
					w.object = item.object;
					w.stackID = id;
					if (auto* extra = stack->extra.get()) {
						if (auto* inst = extra->GetByType<RE::ExtraInstanceData>()) {
							w.instance = inst->data;
						}
					}
					out.push_back(std::move(w));
				}
			}
		}
		return out;
	}

	void Arcade::Snapshot(RE::Actor* a_actor)
	{
		if (a_actor && !_worn.contains(a_actor->GetFormID())) {
			_worn[a_actor->GetFormID()] = WornBy(a_actor);
		}
	}

	// what they wore put back on: anything of it not on them now
	void Arcade::TickRedress(double a_dt)
	{
		std::scoped_lock lock(_redressMutex);
		for (auto it = _redress.begin(); it != _redress.end();) {
			it->in -= a_dt;
			if (it->in > 0.0) {
				++it;
				continue;
			}
			auto* actor = RE::TESForm::GetFormByID<RE::Actor>(it->actor);
			auto* equip = RE::ActorEquipManager::GetSingleton();
			if (actor && equip && !actor->IsDead(false)) {
				const auto now = WornBy(actor);
				int        put = 0;
				for (const auto& w : it->worn) {
					const bool on = std::ranges::any_of(now, [&](const Worn& n) { return n.object == w.object; });
					if (!on && actor->GetInventoryObjectCount(w.object) > 0) {
						equip->EquipObject(actor, RE::BGSObjectInstance(w.object, w.instance.get()), w.stackID, 1, nullptr, false, false, false, true, false);
						++put;
					}
				}
				if (put > 0) {
					log::info("{:08X}: {} piece(s) of what they wore put back on", it->actor, put);
				}
			}
			if (--it->times > 0) {
				it->in = 1.0;
				++it;
			} else {
				it = _redress.erase(it);
			}
		}
	}

	void Arcade::Release(RE::TESFormID a_actor, RE::TESFormID a_weapon, bool a_added)
	{
		_swept[a_actor] = 5.0;  // (being let go: not swept up again while Papyrus does it)
		std::vector<Worn> worn;
		if (auto found = _worn.find(a_actor); found != _worn.end()) {
			worn = std::move(found->second);
			_worn.erase(found);
		}
		Papyrus::Call("NpcEnd", { static_cast<std::int32_t>(a_actor), static_cast<std::int32_t>(a_weapon), a_added ? 1 : 0 },
			[this, a_actor, worn = std::move(worn)](std::int32_t) mutable {
				if (!worn.empty()) {
					std::scoped_lock lock(_redressMutex);
					_redress.push_back({ a_actor, std::move(worn) });
				}
			});
	}

	void Arcade::ReleaseHeld()
	{
		Papyrus::Call("NpcClearAll", { 0 });  // (anyone left in the walk to a machine)
		for (const auto& h : _heldFromSave) {
			log::info("a settler ({:08X}) held at a game when saved: let go", h.actor);
			Release(h.actor, h.weapon, h.added);
		}
		_heldFromSave.clear();
		// a save made mid Whac-a-Commie: its swing quickening taken off
		if (_swingFromSave != 0.0f) {
			if (auto* player = RE::PlayerCharacter::GetSingleton(); player && AV("WeaponSpeedMult")) {
				player->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, *AV("WeaponSpeedMult"), -_swingFromSave);
				log::info("Whac-a-Commie: a save's swing quickening ({:+.2f}) taken off", _swingFromSave);
			}
			_swingFromSave = 0.0f;
		}
	}

	void Arcade::TickShow(double a_dt)
	{
		// settlers on their way when a game was called off: let go once they get there
		std::erase_if(_letGo, [&](const Held& a_h) {
			const auto r = a_h.ready ? a_h.ready->load() : 1;
			if (r < 0) {
				return false;
			}
			if (r <= 2) {
				Release(a_h.actor, a_h.weapon, r == 2);  // (in the quest, or at the spot with the rifle)
			}
			return true;
		});

		// machines resting after a settler's game there, and settlers who've just played
		const auto count = [a_dt](auto& a_map) {
			for (auto it = a_map.begin(); it != a_map.end();) {
				it->second -= a_dt;
				it = it->second <= 0.0 ? a_map.erase(it) : std::next(it);
			}
		};
		count(_rest);
		count(_played);
		// a settler to a free machine now and then (another soon after, while there are machines free)
		if (Settings::Get().settlersOn) {
			_nextShow -= a_dt;
			if (_nextShow <= 0.0) {
				_nextShow = StartShow() ? kShowNextTry : kShowRetry;
			}
		}
		for (std::size_t i = 0; i < _shows.size(); ++i) {
			TickOne(_shows[i], a_dt);
		}
		SweepStragglers(a_dt);
		std::erase_if(_shows, [](const Show& a_s) { return a_s.phase == Show::Phase::kNone; });
	}

	void Arcade::TickOne(Show& a_s, double a_dt)
	{
		if (a_s.phase == Show::Phase::kNone) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto machine = a_s.machine.get();
		auto  actorRef = a_s.actor.get();
		auto* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
		if (!machine || machine->IsDisabled() || machine->IsDeleted() || !player ||
			(machine->data.location - player->data.location).Length() > kShowYouAway) {
			EndShow(a_s, "the machine's gone or you've left");
			return;
		}
		if (!actor || actor->IsDead(false) || !actor->Get3D() || actor->IsInCombat()) {
			EndShow(a_s, "the settler's gone, down or fighting");
			return;
		}
		if (_playing && _machineID == a_s.machineID && !a_s.vsYou) {
			EndShow(a_s, "you're at it");
			return;
		}
		Frame f;
		if (!FrameOf(machine.get(), f)) {
			EndShow(a_s, "the machine's not loaded");
			return;
		}

		if (a_s.phase == Show::Phase::kWalking) {
			const auto r = a_s.ready->load();
			if (r == 0 || r >= 10) {
				const char* why = r == 10 ? "a form's missing" : r == 11 ? "dead" : r == 12 ? "fighting" : r == 13 ? "following you" :
				                  r == 14 ? "knocked out" : r == 15 ? "doing something for you" : r == 17 ? "bleeding out" :
				                  r == 18 ? "the walk couldn't be given them" : r == 20 ? "not a person (a dog, brahmin or robot)" :
				                  r == 21 ? "sat at a Wasteland Wagers table" : r == 22 ? "dealing at a Wasteland Wagers table" : r >= 160 ? "sitting, or at a workstation" : "Papyrus didn't say";
				log::info("{}: {} ({:08X}) couldn't come: {} ({})", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, why, r);
				_refused.push_back(a_s.actorID);
				a_s.ready.reset();  // (not in the quest: nothing to let go)
				EndShow(a_s, nullptr);
				_nextShow = kShowNextTry;  // (someone else, soon)
				return;
			}
			if (r < 0) {
				return;  // (Papyrus not back yet)
			}
			// walking (their package): there, close and no closer for a moment, or near enough after a while; the walk
			// sent again when they stall (as Wasteland Wagers walks its players)
			auto marker = a_s.marker.get();
			if (!marker) {
				EndShow(a_s, "the spot's gone");
				return;
			}
			const float d = std::hypot(actor->data.location.x - marker->data.location.x, actor->data.location.y - marker->data.location.y);
			a_s.walked += a_dt;
			if (d < a_s.best - 10.0f) {
				a_s.best = d;
				a_s.stall = 0.0;
				a_s.resends = 0;
			} else {
				a_s.stall += a_dt;
			}
			bool there = d < kShowArrived || (d < kShowAtSpot && a_s.stall > 1.5);
			if (!there && a_s.walked > kShowWalk) {
				if (d < 120.0f) {
					there = true;
					log::info("{}: {} ({:08X}) {:.0f} from the spot after a while: near enough", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, d);
				} else {
					log::info("{}: {} ({:08X}) took too long, {:.0f} from the spot", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, d);
					EndShow(a_s, nullptr);
					return;
				}
			}
			if (!there) {
				if (a_s.stall > kShowStallResend && d < kShowStuckNear && a_s.resends >= 2) {
					// a couple of metres off and stuck there (the machine, or something put by it, in the way of their path):
					// near enough, the counter puts them on the spot
					there = true;
					log::info("{}: {} ({:08X}) stuck {:.0f} from the spot: near enough", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, d);
				} else if (a_s.stall > kShowStallResend && a_s.resends >= kShowStuckResends) {
					// further off and not a step closer for a while: no way through from where they are; someone else
					log::info("{}: {} ({:08X}) can't find a way ({:.0f} away, no closer in {:.0f}s): someone else", sim::GameName(a_s.gameId),
						a_s.name, a_s.actorID, d, kShowStallResend * (a_s.resends + 1));
					_refused.push_back(a_s.actorID);
					EndShow(a_s, nullptr);
					_nextShow = kShowNextTry;
					return;
				} else if (a_s.stall > kShowStallResend) {
					++a_s.resends;
					log::info("{}: {} ({:08X}) no closer in {:.0f}s ({:.0f} away): sending the walk again", sim::GameName(a_s.gameId), a_s.name,
						a_s.actorID, a_s.stall, d);
					a_s.stall = 0.0;
					a_s.best = d;
					Papyrus::Call("NpcWalk", { static_cast<std::int32_t>(a_s.actorID), Papyrus::Id(marker.get()), 1 });
				}
				if (!there) {
					return;
				}
			}
			a_s.phase = Show::Phase::kArming;
			Snapshot(actor);  // (what they wear: put back on after)
			a_s.armed = std::make_shared<std::atomic<std::int32_t>>(-1);
			auto armed = a_s.armed;
			auto aimAt = a_s.aimAt.get();
			Papyrus::Call("NpcTakeUp", { static_cast<std::int32_t>(a_s.actorID), Papyrus::Id(a_s.weapon), Papyrus::Id(marker.get()),
										   // (the aim package only at the gallery: Whac-a-Commie's marker is just what they face)
										   a_s.gameId == sim::GameId::kBanditRoundup ? Papyrus::Id(aimAt.get()) : 0,
										   Papyrus::Id(a_s.weapon ? a_s.weapon->weaponData.ammo : nullptr) },
				[armed](std::int32_t a_r) { *armed = a_r; });
			log::info("{}: {} ({:08X}) at the counter after {:.0f}s", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, a_s.walked);
			return;
		}

		if (a_s.phase == Show::Phase::kArming) {
			const auto r = a_s.armed->load();
			if (r < 0) {
				return;
			}
			if (r == 0) {
				EndShow(a_s, "couldn't take up the game");
				return;
			}
			// at it (the gun or mallet up): the game starts, its pieces put up
			a_s.added = r == 2;
			const auto seed = static_cast<std::uint32_t>(_showRng());
			const double skill = std::uniform_real_distribution<double>(0.0, 1.0)(_showRng);  // how good they are
			a_s.skill = skill;
			const double spread = 0.03 - 0.024 * skill;  // (a shot's spread at the gallery: 0.006 to 0.03)
			switch (a_s.gameId) {
			case sim::GameId::kWhacACommie:
				// (none hopeless with a mallet; their score only from their mallet's real hits on the heads)
				a_s.whacker = std::make_unique<sim::WhacPlayer>(seed ^ 0x5EED, 0.45 + 0.55 * skill, false);
				break;
			case sim::GameId::kAtomicRollers:
			case sim::GameId::kHoopShot:
				{
					a_s.thrower = std::make_unique<sim::Thrower>(seed ^ 0x5EED, skill, a_s.gameId);
					// our own throw animations let go at frame 24 (Hoop Shot) and 22 (Atomic Rollers), at 30 fps
					a_s.thrower->SetLead(a_s.gameId == sim::GameId::kHoopShot ? 24.0 / 30.0 : 22.0 / 30.0);
					// the ball in their hand
					const auto ids = Forms::PiecesOf(a_s.gameId);
					if (auto* ball = ids.empty() ? nullptr : Forms::Bound(Forms::Own(ids[0]))) {
						a_s.handBall = Spawn(ball, machine.get(), actor->data.location);
					}
				}
				break;
			default: a_s.shooter = std::make_unique<sim::GalleryShooter>(seed ^ 0x5EED, spread); break;
			}
			if (a_s.side >= 0) {
				// a 1v1: the rifle up, waiting for their rival (you, or the other settler)
				a_s.phase = Show::Phase::kWaiting;
				a_s.waited = 0.0;
				log::info("{}: {} ({:08X}) ready (skill {:.2f}), waiting for {}", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, skill,
					a_s.vsYou ? "you" : "their rival");
				return;
			}
			a_s.game = sim::MakeGame(a_s.gameId);
			a_s.game->Start(seed);
			BeginGame(a_s, machine.get(), f);
			log::info("{}: {} ({:08X}) playing (skill {:.2f})", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, skill);
			return;
		}

		if (a_s.phase == Show::Phase::kWaiting) {
			// a 1v1 waiting: yours starts when you're both ready (TickDuel); two settlers', when the host's rival is
			a_s.waited += a_dt;
			if (a_s.vsYou || a_s.guest) {
				if (a_s.waited > kDuelWait) {
					EndShow(a_s, "their rival never came");
				}
				return;
			}
			Show* other = PartnerOf(a_s);
			if (!other) {
				EndShow(a_s, "their rival's gone");
				return;
			}
			if (other->phase != Show::Phase::kWaiting) {
				if (a_s.waited > kDuelWait) {
					EndShow(a_s, "their rival never came");
				}
				return;
			}
			a_s.game = sim::MakeGame(a_s.gameId);
			a_s.game->Start(static_cast<std::uint32_t>(_showRng()));
			sim::SetGalleryVersus(*a_s.game, Upper(a_s.name), Upper(other->name));
			BeginGame(a_s, machine.get(), f);
			other->phase = Show::Phase::kPlaying;
			log::info("{}: {} and {} playing each other ({:08X})", sim::GameName(a_s.gameId), a_s.name, other->name, a_s.machineID);
			return;
		}

		// playing the other games: the mallet's swings, the balls' throws (each with its animation), the ball in their hand
		if (!a_s.shooter) {
			sim::Input in;
			if (a_s.whacker) {
				// a swing over (it missed): their spot moved by most of how far the mallet came down short of (or past, or
				// beside) the head they were after, flat on the ground, so the next swings land (no further than 80 from
				// where the machine puts them)
				// a swing over that missed: their spot moved by most of how far the mallet came down short of (or past, or
				// beside) the head they were after, flat on the ground, so the next swings land (no further than 80 from
				// where the machine puts them). Not off a swing cut short before it could come down
				const auto missed = [&]() {
					if (a_s.swingBest < 1e8f && a_s.swingT >= kWhacSwingDown && a_s.misses++ < 6) {
						log::info("WHAC-A-COMMIE: {}'s swing missed, its mallet {:.0f} from the nearest head up", a_s.name, a_s.swingBest);
					}
					a_s.swingT = -1.0;
				};
				if (a_s.whacker->Update(*a_s.game, a_dt)) {
					if (a_s.swingT >= 0.0) {
						missed();  // (the last swing not over yet: settled first)
					}
					auto spot = a_s.marker.get();
					auto at = a_s.aimAt.get();
					// (they face the middle of the holes, still, the whole game: turned to each head, and their spot moved
					// after misses, they shuffled about all game)
					Papyrus::Call("NpcSwing", { static_cast<std::int32_t>(a_s.actorID), Papyrus::Id(spot.get()), Papyrus::Id(at.get()), Papyrus::Id(a_s.weapon) });
					a_s.swingT = 0.0;
					a_s.swingBest = 1e9f;
					a_s.swingTarget = a_s.whacker->Target();
				}
				// the swing's mallet coming down: a head it gets to is hit (a settler's melee hits on the heads never come
				// through as hit events, so their mallet's head is followed instead). Its head: the far end of what's in
				// their hand (the Weapon node's grip, through the middle of the mallet's bounds, as far again)
				if (a_s.swingT >= 0.0) {
					a_s.swingT += a_dt;
					auto* root = actor->Get3D();
					auto* grip = root ? root->GetObjectByName("Weapon") : nullptr;
					if (grip && grip->worldBound.fRadius > 1.0f) {
						const RE::NiPoint3 g = grip->world.translate;
						const RE::NiPoint3 c = grip->worldBound.center;
						const RE::NiPoint3 head = g + (c - g) * 2.0f;
						std::vector<sim::Piece> pieces;
						a_s.game->Pieces(pieces);
						for (std::size_t i = 0; i < pieces.size() && i < a_s.pieces.size(); ++i) {
							auto  ref = a_s.pieces[i].get();
							auto* h3 = ref ? ref->Get3D() : nullptr;
							if (!h3) {
								continue;
							}
							const RE::NiPoint3 top = h3->world.translate + RE::NiPoint3{ 0.0f, 0.0f, kWhacHeadMiddle };
							const float        d = (head - top).Length();
							if (pieces[i].hidden) {
								continue;
							}
							a_s.swingBest = std::min(a_s.swingBest, d);
							if (d < kWhacMalletReach) {
								a_s.game->OnPieceHit(static_cast<int>(i));
								if (++a_s.hits == 1) {
									log::info("WHAC-A-COMMIE: {}'s mallet is landing on the heads", a_s.name);
								}
								a_s.swingT = -1.0;
								break;
							}
						}
					}
					if (a_s.swingT > kWhacSwingLong) {
						missed();
					}
				}
			} else if (a_s.thrower) {
				in = a_s.thrower->Next(a_dt, a_s.game->Ready());
				a_s.sinceThrow += a_dt;
				if (in.released) {
					a_s.sinceThrow = 0.0;
					a_s.holding = false;
					a_s.ballShown = false;
					a_s.handShown = false;
				}
				if (a_s.thrower->TakeCue()) {
					a_s.holding = true;
					a_s.handShown = false;
					a_s.throwFrames = 0;
					a_s.pickupLog = a_s.pickups++ < 3 ? 0 : -1;
					*a_s.throwOn = false;
					auto throwOn = a_s.throwOn;
					static std::atomic<int> told{ -1 };  // (how the throws go: logged once each way)
					const std::string       name = a_s.name;
					const auto              game = a_s.gameId;
					auto spot = a_s.marker.get();
					Papyrus::Call(a_s.gameId == sim::GameId::kHoopShot ? "NpcToss" : "NpcRoll", { static_cast<std::int32_t>(a_s.actorID), Papyrus::Id(spot.get()) },
						[name, game, throwOn](std::int32_t a_r) {
							*throwOn = true;
							if (told.exchange(a_r) != a_r) {
								log::info("{}: {}'s throws: {}", sim::GameName(game), name,
									a_r == 4 ? (game == sim::GameId::kHoopShot ? "our basketball shot" : "our skee-ball roll") :
									a_r == 1 ? (game == sim::GameId::kHoopShot ? "the grenade throw" : "the mine throw") :
									a_r == 3 ? "Nuka-World's smoke bomb toss (the throw wouldn't play)" : "no animation would play");
							}
						});
				}
			}
			a_s.game->Update(a_dt, in);
			for (const auto& snd : a_s.game->TakeSounds()) {
				Sounds::Play(a_s.gameId, snd.cue, machine.get(), snd.gain);
			}
			// the game's own ball waiting to be thrown: Hoop Shot's (its last piece, by your eye) never shown; Atomic
			// Rollers' on the lane's start till they take it up for a throw. Thrown balls leave from her hands and show at once.
			// (kept hidden as it's placed: shown and hidden again each frame, the renderer caught it shown now and then,
			// and the ball on the lane flickered while she held hers)
			std::vector<bool> waiting;
			if (a_s.thrower) {
				const sim::Vec3 held = sim::layout::Rollers().release;
				std::vector<sim::Piece> pieces;
				a_s.game->Pieces(pieces);
				waiting.resize(pieces.size());
				for (std::size_t i = 0; i < pieces.size(); ++i) {
					// (Atomic Rollers': hidden once hers is shown in her hand - till then it's still on the lane)
					// (this frame's: the hand's ball shown below once the throw's two frames on)
					const bool inHand = a_s.handShown || (a_s.holding && *a_s.throwOn && a_s.throwFrames + 1 >= 2);
					waiting[i] = a_s.gameId == sim::GameId::kHoopShot ? i + 1 == pieces.size() :
					                                                    inHand && (pieces[i].pos - held).Length() < 12.0;
				}
			}
			PlacePieces(f, *a_s.game, a_s.gameId, machine.get(), a_s.pieces, a_s.pieceModel, a_s.frozen, &waiting);
			if (a_s.thrower) {
				// ...theirs in their hand(s) while the throw's on: Hoop Shot's out in front of the right palm (on the ball's back),
				// Atomic Rollers' cupped in the right palm (out from the palm by its radius, not through the hand)
				if (auto ball = a_s.handBall.get()) {
					auto* root = actor->Get3D();
					RE::NiPoint3 at;
					bool         show = false;
					bool         placed = false;  // (where it'd be in her hand, shown or not)
					// (the throw playing a couple of frames: her turn and any step made, the hand where the throw has it)
					if (a_s.holding && *a_s.throwOn) {
						++a_s.throwFrames;
					}
					const bool throwing = a_s.throwFrames >= 2;
					{
						auto* h = root ? root->GetObjectByName("RArm_Hand") : nullptr;
						auto* mid = root ? root->GetObjectByName("RArm_Finger31") : nullptr;
						auto* index = root ? root->GetObjectByName("RArm_Finger21") : nullptr;
						auto* pinky = root ? root->GetObjectByName("RArm_Finger51") : nullptr;
						auto* thumb = root ? root->GetObjectByName("RArm_Finger13") : nullptr;
						if (h && mid && index && pinky) {
							const RE::NiPoint3 wrist = h->world.translate;
							const RE::NiPoint3 along = mid->world.translate - wrist;
							const RE::NiPoint3 across = index->world.translate - pinky->world.translate;
							RE::NiPoint3       n{ along.y * across.z - along.z * across.y, along.z * across.x - along.x * across.z,
                                along.x * across.y - along.y * across.x };
							const float len = n.Length();
							if (len > 0.001f) {
								n = n * (1.0f / len);
								// (the palm's side: the thumb's tip is on it)
								if (thumb) {
									const RE::NiPoint3 t = thumb->world.translate - wrist;
									if (t.x * n.x + t.y * n.y + t.z * n.z < 0.0f) {
										n = n * -1.0f;
									}
								}
								float radius = a_s.gameId == sim::GameId::kHoopShot ? 12.0f : 4.5f;
								if (auto* b3 = ball->Get3D(); b3 && b3->worldBound.fRadius > 0.5f) {
									radius = b3->worldBound.fRadius;
								}
								if (a_s.gameId == sim::GameId::kHoopShot) {
									// the set shot's right palm on the back of the ball, facing the hoop (the left hand at its
									// side): shown once the palm's turned to face forward, the ball out in front of it
									const float        heading = actor->data.angle.z;
									const RE::NiPoint3 forward{ std::sin(heading), std::cos(heading), 0.0f };
									// (once shown, kept till it's let go: the palm's turn hovers about the line through the set
									// point, and the ball blinked in and out there)
									at = wrist + along * 0.75f + n * (radius + 4.0f);  // (clear of the hand: on its palm, not round its fingers)
									placed = true;
									if (a_s.holding && throwing && (a_s.ballShown || n.x * forward.x + n.y * forward.y > 0.45f)) {
										show = true;
										a_s.ballShown = true;
									}
								} else {
									// (in the fingers, out from them by its radius: from the palm or the wrist it sat in the hand)
									auto* tip = root ? root->GetObjectByName("RArm_Finger33") : nullptr;
									const RE::NiPoint3 knuckle = mid->world.translate;
									const RE::NiPoint3 into = tip ? (tip->world.translate - knuckle) * 0.3f : RE::NiPoint3{};
									at = knuckle + into + n * (radius + 2.0f);
									placed = true;
									show = a_s.holding && throwing;
								}
							}
						}
					}
					// kept in her hand while hidden too: taken up, it shows where it is already (it was left where it was
					// last let go, and shown in her hand it flickered a moment: the renderer's motion across from there)
					const RE::NiPoint3 rawAt = at, prevAt = a_s.lastAt;
					// (a frame's jump of the hand - the actor turned or moved under it - ignored: the ball kept where it was)
					if (placed && a_s.lastAtOk && (at - a_s.lastAt).Length() > kHandJump && a_s.jumps < 2) {
						++a_s.jumps;
						at = a_s.lastAt;
					} else if (placed) {
						a_s.jumps = 0;
						a_s.lastAt = at;
						a_s.lastAtOk = true;
					}
					if (show) {
						a_s.handShown = true;
					}
					if (placed) {
						Freeze(ball.get(), a_s.frozen);
						PlaceNode(ball.get(), at, f.rot, f.scale, !show, true);
					} else if (auto* b3 = ball->Get3D()) {
						b3->SetAppCulled(true);
					}
					// (a pickup frame by frame, the first three a game: what's shown, the hand, her own moves)
					if (a_s.pickupLog >= 0 && a_s.pickupLog < 40) {
						bool laneShown = false;
						for (std::size_t i = 0; i < waiting.size() && i < a_s.pieces.size(); ++i) {
							if (auto r = a_s.pieces[i].get(); r && r->Get3D() && !r->Get3D()->GetAppCulled()) {
								std::vector<sim::Piece> pcs;
								a_s.game->Pieces(pcs);
								if (i < pcs.size() && (pcs[i].pos - sim::layout::Rollers().release).Length() < 12.0) {
									laneShown = true;
								}
							}
						}
						const RE::NiPoint3 me = actor->data.location;
						log::info("{} PICKUP {} f{}: on {} hand {} lane ball {} | hand ({:.1f},{:.1f},{:.1f}) moved {:.1f}{} | her moved {:.1f}, turned {:.1f}",
							sim::GameName(a_s.gameId), a_s.name, a_s.pickupLog, *a_s.throwOn ? 1 : 0, show ? "SHOWN" : "hidden",
							laneShown ? "SHOWN" : "hidden", rawAt.x, rawAt.y, rawAt.z, (rawAt - prevAt).Length(), at == rawAt ? "" : " (a jump: held)",
							a_s.pickupLog > 0 ? (me - a_s.lastActorAt).Length() : 0.0f,
							a_s.pickupLog > 0 ? (actor->data.angle.z - a_s.lastHeading) * 57.2958f : 0.0f);
						a_s.lastActorAt = me;
						a_s.lastHeading = actor->data.angle.z;
						++a_s.pickupLog;
					}
				}
			}
			if (a_s.game->Over()) {
				_lastScores[a_s.machineID] = a_s.game->Score();
				_played[Played(a_s.actorID, a_s.gameId)] = kShowTurnAbout;
				log::info("{}: {} ({:08X}) scored {}", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, a_s.game->Score());
				EndShow(a_s, nullptr);
			}
			return;
		}

		// playing Bandit Roundup: the shooter aims and fires; the game plays it
		// (how they stand, every 2 s for their first 30: their height on the ground against where they are, how low
		// their pelvis is over it, and what the game says the rifle's doing - to find what sinks them)
		a_s.diag += a_dt;
		if (a_s.diag >= 2.0 && a_s.diagT < 30.0) {
			a_s.diagT += a_s.diag;
			a_s.diag = 0.0;
			auto*       root = actor->Get3D();
			auto*       pelvis = root ? root->GetObjectByName("Pelvis") : nullptr;
			const float z = actor->data.location.z;
			log::info("BANDIT ROUNDUP: {} at {:.0f}s: z {:.1f} (spot {:.1f}), 3D z {:+.1f}, pelvis {:.1f} up, gun state {}, drawn {}, sneaking {}",
				a_s.name, a_s.diagT, z, a_s.marker.get() ? a_s.marker.get()->data.location.z : 0.0f, root ? root->world.translate.z - z : 0.0f,
				pelvis ? pelvis->world.translate.z - z : 0.0f, static_cast<int>(actor->gunState), actor->GetWeaponMagicDrawn(), actor->IsSneaking());
		}
		// (a 1v1's rival, yours or another settler's: their shots go into that game, no time passing; its owner plays it)
		sim::Game* game = a_s.game.get();
		const bool owner = !a_s.vsYou && !a_s.guest;
		if (a_s.vsYou) {
			game = _playing && _machineID == a_s.machineID ? _game.get() : nullptr;
		} else if (a_s.guest) {
			Show* host = PartnerOf(a_s);
			game = host && host->phase == Show::Phase::kPlaying ? host->game.get() : nullptr;
		}
		if (!game) {
			EndShow(a_s, a_s.vsYou ? nullptr : "their rival's game is over");
			return;
		}
		sim::Input in = a_s.shooter->Next(*game, a_s.eye, a_dt);
		// a shot only once they've turned onto where it's aimed (their real bullet goes where the rifle points: on a
		// fast target their turn lags), or after a moment however they stand
		if (in.pressed && std::abs(in.dir.y) > 0.05) {
			const double       t = (sim::layout::Gallery().aimAt.y - in.eye.y) / in.dir.y;
			const RE::NiPoint3 w = ToWorld(f, in.eye + in.dir * t);
			const float        want = std::atan2(w.x - actor->data.location.x, w.y - actor->data.location.y);
			float              off = want - actor->data.angle.z;
			while (off > 3.14159265f) off -= 6.2831853f;
			while (off < -3.14159265f) off += 6.2831853f;
			if (std::abs(off) > 0.05f && a_s.onTarget < 0.8) {
				a_s.onTarget += a_dt;
				in.pressed = false;
				a_s.shooter->Again();
			} else {
				a_s.onTarget = 0.0;
			}
		}
		in.who = owner ? 0 : 1;
		game->Update(owner ? a_dt : 0.0, in);
		// what they aim at follows their shot: the marker their hold package aims at put where the shooter's looking,
		// out at the targets' tracks (so they swing from target to target, and each shot goes where it's aimed)
		if (auto aimAt = a_s.aimAt.get(); aimAt && std::abs(in.dir.y) > 0.05) {
			const double t = (sim::layout::Gallery().aimAt.y - in.eye.y) / in.dir.y;
			if (t > 0.0) {
				const RE::NiPoint3 w = ToWorld(f, in.eye + in.dir * t);
				aimAt->data.location.x = w.x;
				aimAt->data.location.y = w.y;
				aimAt->data.location.z = w.z;
			}
		}
		// ...and not till the rifle's been up and steady a while: taking it up they raise it, lower it as their hold
		// package takes over and raise it again, and the first shots of that went into the ground
		if (actor->GetWeaponMagicDrawn()) {
			a_s.steady += a_dt;
			a_s.drawAsked = 0.0;
			a_s.everDrawn = true;
		} else if (a_s.everDrawn) {  // (not before their package has first raised it: that's its job)
			// never put away while they play: if it is, straight out again
			if (a_s.steady > 0.0) {
				log::info("BANDIT ROUNDUP: {} put the rifle away mid-game: drawn again", a_s.name);
			}
			a_s.steady = 0.0;
			a_s.drawAsked -= a_dt;
			if (a_s.drawAsked <= 0.0) {
				a_s.drawAsked = 1.0;
				Papyrus::Call("NpcDraw", { static_cast<std::int32_t>(a_s.actorID) });
			}
		}  // (as at the zapper race: their package draws it first)
		if (in.pressed && a_s.steady < kSteadyBeforeShot) {
			in.pressed = false;
			a_s.shooter->Again();
		}
		// the magazine filled once, as the rifle comes up: it's handed over empty, and their first shots were reloads till
		// one filled it. (Kept full after every shot, as yours is, every other shot of theirs was a reload)
		if (a_s.weapon && !a_s.loaded && actor->GetWeaponMagicDrawn()) {
			a_s.loaded = true;
			const RE::BGSEquipIndex hand{ 0 };
			const auto              full = a_s.weapon->weaponData.ammoCapacity > 0 ? a_s.weapon->weaponData.ammoCapacity : 10u;
			if (const auto now = actor->GetCurrentAmmoCount(hand); now < full) {
				actor->SetCurrentAmmoCount(hand, full);
				log::info("BANDIT ROUNDUP: {}'s magazine filled ({} of {})", a_s.name, now, full);
			}
		}
		if (in.pressed && a_s.weapon && CanFire(actor, a_s.name)) {
			static std::atomic<int> told{ -1 };  // (how the shots go: logged once each way)
			const std::string       name = a_s.name;
			Papyrus::Call("NpcShoot", { static_cast<std::int32_t>(a_s.actorID), Papyrus::Id(a_s.weapon), Papyrus::Id(a_s.weapon->weaponData.ammo) },
				[name](std::int32_t a_r) {
					if (told.exchange(a_r) != a_r) {
						log::info("BANDIT ROUNDUP: {}'s shots: {}", name,
							a_r == 3 ? "the rifle's fire event (weaponFire)" :
							a_r == 1 ? "the shot idle (RaiderFireSingle)" :
							a_r == 2 ? "an attack (attackStart)" : a_r == 4 ? "none (the shot idle wouldn't play)" : "nobody to fire it");
					}
				});
		}
		if (!owner) {
			return;  // (the game's owner plays it: its sounds, pieces and end)
		}
		for (const auto& s : a_s.game->TakeSounds()) {
			Sounds::Play(a_s.gameId, s.cue, machine.get(), s.gain);
		}
		PlacePieces(f, *a_s.game, a_s.gameId, machine.get(), a_s.pieces, a_s.pieceModel, a_s.frozen);
		if (a_s.game->Over()) {
			_lastScores[a_s.machineID] = a_s.game->Score();
			_played[Played(a_s.actorID, sim::GameId::kBanditRoundup)] = kShowTurnAbout;
			if (int left = 0, right = 0; sim::GalleryVersus(*a_s.game) && DuelScoresOn(a_s.machineID, left, right)) {
				_lastDuel[a_s.machineID] = { left, right };  // (on the board, each on its side, till the next game there)
			}
			if (const Show* other = sim::GalleryVersus(*a_s.game) ? PartnerOf(a_s) : nullptr) {
				_played[Played(other->actorID, sim::GameId::kBanditRoundup)] = kShowTurnAbout;
				const int mine = sim::GalleryScore(*a_s.game, 0), theirs = sim::GalleryScore(*a_s.game, 1);
				log::info("{}: {} ({:08X}) {}, {} ({:08X}) {}: {}", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, mine, other->name,
					other->actorID, theirs, mine == theirs ? std::string("a tie") : std::format("{} won", mine > theirs ? a_s.name : other->name));
			} else {
				log::info("{}: {} ({:08X}) scored {}", sim::GameName(a_s.gameId), a_s.name, a_s.actorID, a_s.game->Score());
			}
			EndShow(a_s, nullptr);
		}
	}

	bool Arcade::StartShow()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* lists = RE::ProcessLists::GetSingleton();
		auto* kw = Forms::WorkshopItemKeyword();
		auto* markerForm = Forms::Bound(RE::TESForm::GetFormByID(Forms::kXMarkerHeading));
		if (!player || !lists || !kw || !markerForm) {
			return false;
		}
		// (the games a settler plays on their own; the zapper race is TickRaces')
		std::vector<std::pair<RE::ObjectRefHandle, sim::GameId>> machines;
		for (const auto g : { sim::GameId::kBanditRoundup, sim::GameId::kWhacACommie, sim::GameId::kAtomicRollers, sim::GameId::kHoopShot }) {
			for (auto& m : Scoreboards::Machines(g)) {
				machines.emplace_back(m, g);
			}
		}
		std::shuffle(machines.begin(), machines.end(), _showRng);
		for (auto& [h, gameId] : machines) {
			auto* weapon = Forms::SettlerWeaponOf(gameId);  // (none: Atomic Rollers, Hoop Shot)
			if (gameId != sim::GameId::kAtomicRollers && gameId != sim::GameId::kHoopShot && !weapon) {
				continue;
			}
			auto machine = h.get();
			if (!machine || (_playing && _machineID == machine->GetFormID()) || ShowOn(machine->GetFormID()) ||
				_rest.contains(machine->GetFormID()) || (_duel.on && _duel.machineID == machine->GetFormID())) {
				continue;  // (yours, a settler's already, or resting after one)
			}
			auto* workshop = machine->GetLinkedRef(kw);
			if (!workshop) {
				continue;
			}
			// the settlers of its settlement near it (people: not its turrets, dogs, brahmin, robots)
			static auto*            npcType = RE::TESForm::GetFormByID<RE::BGSKeyword>(0x00013794);  // ActorTypeNPC
			std::vector<RE::Actor*> settlers;
			int                     elsewhere = 0, farOff = 0;
			std::string             others;
			for (auto& ah : lists->highActorHandles) {
				auto a = ah.get();
				if (!a || a.get() == player || a->IsDead(false) || a->IsDisabled() || !a->Get3D() || a->IsInCombat() ||
					(npcType && !a->HasKeywordHelper(npcType, nullptr))) {
					continue;
				}
				if ((a->data.location - machine->data.location).Length() > kShowReach) {
					++farOff;
					continue;
				}
				if (a->GetLinkedRef(kw) != workshop) {
					++elsewhere;  // (not of this settlement: a visitor, a companion, a trader)
					if (others.size() < 200) {
						others += std::format("{}{}", others.empty() ? "" : ", ", a->GetDisplayFullName() ? a->GetDisplayFullName() : "?");
					}
					continue;
				}
				if (std::ranges::find(_refused, a->GetFormID()) != _refused.end() ||
					std::ranges::any_of(_shows, [&](const Show& a_s) { return a_s.actorID == a->GetFormID(); }) ||
					std::ranges::any_of(_races, [&](const Race& a_r) {
						return std::ranges::any_of(a_r.racers, [&](const Racer& a_q) { return a_q.actorID == a->GetFormID(); });
					}) ||
					_played.contains(Played(a->GetFormID(), gameId))) {
					continue;  // (couldn't come lately, at another machine, or had a go lately: someone else's turn)
				}
				settlers.push_back(a.get());
			}
			if (settlers.empty()) {
				log::info("{}: no settler of its settlement free to ask ({} people not of it near: {})", sim::GameName(gameId),
					elsewhere, others.empty() ? "none" : others);
				if (!_refused.empty()) {
					log::info("{}: no settler free just now ({} asked couldn't come)", sim::GameName(sim::GameId::kBanditRoundup), _refused.size());
					_refused.clear();  // (they may be free by the next try)
				}
				continue;
			}
			auto* who = settlers[std::uniform_int_distribution<std::size_t>(0, settlers.size() - 1)(_showRng)];
			log::info("{}: {} settler(s) of its settlement about (and {} people not of it near: {}; {} further off)",
				sim::GameName(gameId), settlers.size(), elsewhere, others.empty() ? "none" : others, farOff);
			// Bandit Roundup: now and then two settlers playing each other, one each side
			if (gameId == sim::GameId::kBanditRoundup && settlers.size() >= 2 &&
				std::uniform_real_distribution<double>(0.0, 100.0)(_showRng) < Settings::Get().settlersDuel) {
				RE::Actor* rival = who;
				while (rival == who) {
					rival = settlers[std::uniform_int_distribution<std::size_t>(0, settlers.size() - 1)(_showRng)];
				}
				const int side = std::uniform_int_distribution<int>(0, 1)(_showRng);
				if (CallToPlay(machine.get(), who, gameId, side, false, false, rival->GetFormID())) {
					if (CallToPlay(machine.get(), rival, gameId, 1 - side, false, true, who->GetFormID())) {
						log::info("{}: a 1v1 ({:08X} and {:08X})", sim::GameName(gameId), who->GetFormID(), rival->GetFormID());
						return true;
					}
					for (auto& o : _shows) {
						if (o.actorID == who->GetFormID() && o.phase != Show::Phase::kNone) {
							o.partner = 0;
							EndShow(o, "their rival couldn't be called");
						}
					}
				}
				continue;
			}
			if (CallToPlay(machine.get(), who, gameId, -1, false, false, 0)) {
				return true;
			}
		}
		return false;
	}

	// a settler called to play a game on a machine: their spot (alone, or a 1v1's side), the rifle's aim marker, the walk
	bool Arcade::CallToPlay(RE::TESObjectREFR* a_machine, RE::Actor* a_who, sim::GameId a_game, int a_side, bool a_vsYou, bool a_guest,
		RE::TESFormID a_partner)
	{
		auto* markerForm = Forms::Bound(RE::TESForm::GetFormByID(Forms::kXMarkerHeading));
		Frame f;
		if (!a_machine || !a_who || !markerForm || !FrameOf(a_machine, f)) {
			return false;
		}
		auto*           weapon = Forms::SettlerWeaponOf(a_game);
		const sim::Vec3 stand = a_side >= 0 ? sim::layout::SettlerStand1v1(a_side) : sim::layout::SettlerStand(a_game);
		Show            sh;
		sh.marker = Spawn(markerForm, a_machine, ToWorld(f, stand));
		if (a_game == sim::GameId::kBanditRoundup) {
			sh.aimAt = Spawn(markerForm, a_machine, ToWorld(f, sim::layout::Gallery().aimAt));
		} else if (a_game == sim::GameId::kWhacACommie) {
			sh.aimAt = Spawn(markerForm, a_machine, ToWorld(f, sim::Vec3{ 0.0, 0.0, 67.0 }));  // (the middle of the holes: what they face)
		}
		auto marker = sh.marker.get();
		if (!marker) {
			Remove(sh.aimAt);
			return false;
		}
		sh.phase = Show::Phase::kWalking;
		sh.actor = a_who->GetHandle();
		sh.actorID = a_who->GetFormID();
		if (const char* n = a_who->GetDisplayFullName(); n && *n) {
			sh.name = n;
		}
		sh.machine = a_machine->GetHandle();
		sh.machineID = a_machine->GetFormID();
		sh.gameId = a_game;
		sh.eye = stand + sim::Vec3{ 0.0, 0.0, kGalleryEyeHeight };
		sh.weapon = weapon;
		sh.side = a_side;
		sh.vsYou = a_vsYou;
		sh.guest = a_guest;
		sh.partner = a_partner;
		sh.ready = std::make_shared<std::atomic<std::int32_t>>(-1);
		auto ready = sh.ready;
		Papyrus::Call("NpcStartGallery", { static_cast<std::int32_t>(sh.actorID), Papyrus::Id(weapon), Papyrus::Id(marker.get()) },
			[ready](std::int32_t a_r) { *ready = a_r; });
		log::info("{}: {} ({:08X}) coming to play ({:08X}){}", sim::GameName(sh.gameId), sh.name, sh.actorID, sh.machineID,
			a_vsYou ? ", against you" : a_side == 0 ? ", on the left" : a_side == 1 ? ", on the right" : "");
		_shows.push_back(std::move(sh));
		return true;
	}

	// a settler's game under way: its pieces put up, its music on
	void Arcade::BeginGame(Show& a_s, RE::TESObjectREFR* a_machine, const Frame& a_f)
	{
		_lastDuel.erase(a_s.machineID);
		std::vector<sim::Piece> pieces;
		a_s.game->Pieces(pieces);
		const auto models = Forms::PiecesOf(a_s.gameId);
		for (const auto& p : pieces) {
			auto* form = p.model >= 0 && p.model < static_cast<int>(models.size()) ? Forms::Bound(Forms::Own(models[p.model])) : nullptr;
			a_s.pieces.push_back(form ? Spawn(form, a_machine, ToWorld(a_f, p.pos)) : RE::ObjectRefHandle{});
			a_s.pieceModel.push_back(p.model);
		}
		Sounds::Music(a_s.gameId, a_machine, true);
		a_s.phase = Show::Phase::kPlaying;
	}

	Arcade::Show* Arcade::PartnerOf(const Show& a_s)
	{
		if (a_s.partner == 0) {
			return nullptr;
		}
		for (auto& o : _shows) {
			if (o.phase != Show::Phase::kNone && o.actorID == a_s.partner && o.machineID == a_s.machineID) {
				return &o;
			}
		}
		return nullptr;
	}

	bool Arcade::DuelScoresOn(RE::TESFormID a_machine, int& a_left, int& a_right) const
	{
		const sim::Game* g = nullptr;
		int              side = 0;  // the game's own player's (its score 0)
		if (_playing && _machineID == a_machine && _game && sim::GalleryVersus(*_game)) {
			g = _game.get();
			side = _duel.yourSide;
		} else if (const auto* s = ShowOn(a_machine); s && s->phase == Show::Phase::kPlaying && s->game && sim::GalleryVersus(*s->game)) {
			g = s->game.get();
			side = s->side;
		}
		if (!g) {
			// (the last game there a 1v1: its final scores, each on its side, till the next game)
			if (const auto it = _lastDuel.find(a_machine); it != _lastDuel.end()) {
				a_left = it->second.first;
				a_right = it->second.second;
				return true;
			}
			return false;
		}
		const int own = sim::GalleryScore(*g, 0), rival = sim::GalleryScore(*g, 1);
		a_left = side == 1 ? rival : own;
		a_right = side == 1 ? own : rival;
		return true;
	}

	// ------------------------------------------------------------------------ your Bandit Roundup 1v1

	// a Bandit Roundup activated: tapped, you play alone; held, a settler's called to take you on
	void Arcade::ResolvePending(double a_dt)
	{
		if (!_pending.machine) {
			return;
		}
		_pending.age += a_dt;
		const bool   down = _activateDown.load();
		const double held = _activateHeld.load();
		if (down && held < kHoldToDuel && _pending.age < 1.5) {
			return;  // (still down: not known yet)
		}
		auto machine = _pending.machine.get();
		_pending = {};
		if (!machine || _playing || _duel.on) {
			return;
		}
		log::info("BANDIT ROUNDUP: activated, the button {} ({:.2f}s): {}", down ? "still down" : "let go", held,
			held >= kHoldToDuel ? "held, a 1v1" : "tapped, alone");
		if (held >= kHoldToDuel) {
			StartDuel(machine.get());
			return;
		}
		if (!TakeToken()) {
			Notify(std::format("{}: insert a Nuka-Cade token. Buy them at a Token Stand.", sim::GameName(sim::GameId::kBanditRoundup)));
			return;
		}
		Start(machine.get(), sim::GameId::kBanditRoundup);
	}

	void Arcade::StartDuel(RE::TESObjectREFR* a_machine)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		Frame f;
		if (!player || !FrameOf(a_machine, f)) {
			return;
		}
		if (!TakeToken()) {
			Notify("Bandit Roundup: insert a Nuka-Cade token. Buy them at a Token Stand.");
			return;
		}
		// your side: where you are at the counter (near its middle, either); the settler's the other
		const double x = ToLocal(f, player->data.location).x;
		_duel = {};
		_duel.on = true;
		_duel.machine = a_machine->GetHandle();
		_duel.machineID = a_machine->GetFormID();
		_duel.yourSide = x < -kDuelMiddle ? 0 : x > kDuelMiddle ? 1 : std::uniform_int_distribution<int>(0, 1)(_showRng);
		log::info("BANDIT ROUNDUP {:08X}: you held it: a 1v1, you on the {} ({:.0f} across)", _duel.machineID,
			_duel.yourSide == 0 ? "left" : "right", x);
		if (!CallRival()) {
			EndDuel("Bandit Roundup: no settler's free to take you on. Your token's back.", true);
		}
	}

	bool Arcade::CallRival()
	{
		auto machine = _duel.machine.get();
		while (machine && _duel.tries < kDuelTries) {
			++_duel.tries;
			auto* who = PickSettler(machine.get(), sim::GameId::kBanditRoundup);
			if (!who) {
				return false;
			}
			if (CallToPlay(machine.get(), who, sim::GameId::kBanditRoundup, 1 - _duel.yourSide, true, false, 0)) {
				const char* n = who->GetDisplayFullName();
				Notify(std::format("Bandit Roundup: {} is coming to take you on. Activate it again to leave.", n && *n ? n : "a settler"));
				return true;
			}
		}
		return false;
	}

	// your 1v1 waiting: your rival ready (the rifle up), the game starts; your rival gone, another called
	void Arcade::TickDuel()
	{
		if (!_duel.on || _duel.started) {
			return;
		}
		Show* rival = nullptr;
		for (auto& sh : _shows) {
			if (sh.vsYou && sh.phase != Show::Phase::kNone) {
				rival = &sh;
			}
		}
		if (!rival) {
			if (!CallRival()) {
				EndDuel("Bandit Roundup: no settler could come to take you on. Your token's back.", true);
			}
			return;
		}
		if (rival->phase != Show::Phase::kWaiting) {
			return;
		}
		auto machine = _duel.machine.get();
		if (!machine) {
			EndDuel(nullptr, true);
			return;
		}
		Start(machine.get(), sim::GameId::kBanditRoundup);
		if (!_game) {
			EndDuel(nullptr, true);
			return;
		}
		sim::SetGalleryVersus(*_game, "YOU", Upper(rival->name));
		_duel.started = true;
		rival->phase = Show::Phase::kPlaying;
		log::info("BANDIT ROUNDUP {:08X}: you and {} ({:08X})", _duel.machineID, rival->name, rival->actorID);
	}

	void Arcade::EndDuel(const char* a_notify, bool a_tokenBack)
	{
		const bool was = _duel.on;
		_duel = {};  // (first: the rival's EndShow doesn't come back here)
		for (auto& sh : _shows) {
			if (sh.vsYou && sh.phase != Show::Phase::kNone) {
				EndShow(sh, nullptr);
			}
		}
		if (was && a_tokenBack) {
			GiveToken();
		}
		if (a_notify) {
			Notify(a_notify);
		}
	}

	// a settler still linked to a game's spot when no game has them (one let go while walking that kept on, or stood
	// there after): let go now, once a second at most
	void Arcade::SweepStragglers(double a_dt)
	{
		for (auto it = _swept.begin(); it != _swept.end();) {
			it->second -= a_dt;
			it = it->second <= 0.0 ? _swept.erase(it) : std::next(it);
		}
		_sweep -= a_dt;
		if (_sweep > 0.0) {
			return;
		}
		_sweep = 1.0;
		auto* lists = RE::ProcessLists::GetSingleton();
		auto* link = Forms::Own(0x80D) ? Forms::Own(0x80D)->As<RE::BGSKeyword>() : nullptr;  // AmusementGamesLinkSpot
		if (!lists || !link) {
			return;
		}
		for (auto& ah : lists->highActorHandles) {
			auto a = ah.get();
			if (!a || a->IsDead(false) || !a->GetLinkedRef(link)) {
				continue;
			}
			const auto id = a->GetFormID();
			const bool busy = _swept.contains(id) ||
			                  std::ranges::any_of(_shows, [&](const Show& a_s) { return a_s.phase != Show::Phase::kNone && a_s.actorID == id; }) ||
			                  std::ranges::any_of(_letGo, [&](const Held& a_h) { return a_h.actor == id; }) ||
			                  std::ranges::any_of(_heldFromSave, [&](const Held& a_h) { return a_h.actor == id; }) ||
			                  std::ranges::any_of(_races, [&](const Race& a_r) {
								  return std::ranges::any_of(a_r.racers, [&](const Racer& a_q) { return a_q.actorID == id; });
							  });
			if (busy) {
				continue;
			}
			const char* n = a->GetDisplayFullName();
			log::info("{} ({:08X}) still headed for a game's spot, no game theirs: let go", n && *n ? n : "a settler", id);
			Release(id, 0, false);
		}
	}

	void Arcade::EndShow(Show& a_s, const char* a_why)
	{
		if (a_s.phase == Show::Phase::kNone) {
			return;
		}
		if (a_why) {
			log::info("{}: {}'s game called off ({})", sim::GameName(a_s.gameId), a_s.name.empty() ? "the settler" : a_s.name, a_why);
		}
		const auto weaponID = a_s.weapon ? a_s.weapon->GetFormID() : 0;
		const auto partner = a_s.partner;
		const auto machineID = a_s.machineID;
		if (a_s.phase == Show::Phase::kPlaying || a_s.phase == Show::Phase::kWaiting) {
			Release(a_s.actorID, weaponID, a_s.added);
			if (auto machine = a_s.machine.get(); machine && a_s.game) {
				Sounds::Music(a_s.gameId, machine.get(), false);  // (its owner's: not a rival's, yours playing on)
			}
		} else if (a_s.phase == Show::Phase::kArming && a_s.armed) {
			_letGo.push_back({ a_s.armed, a_s.actorID, weaponID, false });  // (let go once they've the rifle up)
		} else if (a_s.ready) {
			_letGo.push_back({ a_s.ready, a_s.actorID, weaponID, false });  // (let go once Papyrus has them walking)
		}
		for (auto& h : a_s.pieces) {
			Remove(h);
		}
		Remove(a_s.marker);
		Remove(a_s.aimAt);
		Remove(a_s.handBall);
		// the machine rests a while before the next settler's game on it (after a game played; a few seconds after one
		// that never got going, someone else asked)
		const auto& set = Settings::Get();
		_rest[a_s.machineID] = a_s.phase == Show::Phase::kPlaying ?
		                           std::uniform_real_distribution<double>(set.settlersGapShortest, std::max(set.settlersGapShortest, set.settlersGapLongest))(_showRng) :
		                           kShowNextTry;
		a_s = {};
		// a 1v1 between settlers: the other's game over too
		if (partner != 0) {
			for (auto& o : _shows) {
				if (o.phase != Show::Phase::kNone && o.actorID == partner && o.machineID == machineID) {
					EndShow(o, a_why ? "their rival's game was called off" : nullptr);
				}
			}
		}
	}


	// ------------------------------------------------------------------------ settlers racing (Nuka-Zapper Race)

	RE::Actor* Arcade::PickSettler(RE::TESObjectREFR* a_machine, sim::GameId a_game)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* lists = RE::ProcessLists::GetSingleton();
		auto* kw = Forms::WorkshopItemKeyword();
		auto* workshop = a_machine && kw ? a_machine->GetLinkedRef(kw) : nullptr;
		if (!player || !lists || !workshop) {
			return nullptr;
		}
		static auto*            npcType = RE::TESForm::GetFormByID<RE::BGSKeyword>(0x00013794);  // ActorTypeNPC
		std::vector<RE::Actor*> settlers;
		for (auto& ah : lists->highActorHandles) {
			auto a = ah.get();
			if (!a || a.get() == player || a->IsDead(false) || a->IsDisabled() || !a->Get3D() || a->IsInCombat() ||
				(npcType && !a->HasKeywordHelper(npcType, nullptr)) || a->GetLinkedRef(kw) != workshop ||
				(a->data.location - a_machine->data.location).Length() > kShowReach) {
				continue;
			}
			const auto id = a->GetFormID();
			const bool busy = std::ranges::find(_refused, id) != _refused.end() || _played.contains(Played(id, a_game)) ||
			                  std::ranges::any_of(_shows, [&](const Show& a_s) { return a_s.actorID == id; }) ||
			                  std::ranges::any_of(_races, [&](const Race& a_r) {
								  return std::ranges::any_of(a_r.racers, [&](const Racer& a_q) { return a_q.actorID == id; });
							  });
			if (!busy) {
				settlers.push_back(a.get());
			}
		}
		if (settlers.empty()) {
			_refused.clear();  // (those who couldn't come may be free by the next try)
			return nullptr;
		}
		return settlers[std::uniform_int_distribution<std::size_t>(0, settlers.size() - 1)(_showRng)];
	}

	void Arcade::LetGo(Racer& a_q)
	{
		const auto weaponID = Forms::WeaponOf(sim::GameId::kNukaZapperRace) ? Forms::WeaponOf(sim::GameId::kNukaZapperRace)->GetFormID() : 0;
		if (a_q.state == 2) {
			Release(a_q.actorID, weaponID, a_q.added);
		} else if (a_q.state == 1 && a_q.armed) {
			_letGo.push_back({ a_q.armed, a_q.actorID, weaponID, false });
		} else if (a_q.ready) {
			_letGo.push_back({ a_q.ready, a_q.actorID, weaponID, false });
		}
		Remove(a_q.marker);
		Remove(a_q.aimAt);
	}

	bool Arcade::StartRace()
	{
		for (auto& h : Scoreboards::Machines(sim::GameId::kNukaZapperRace)) {
			auto machine = h.get();
			if (!machine || (_playing && _machineID == machine->GetFormID()) || RaceOn(machine->GetFormID()) ||
				_rest.contains(machine->GetFormID())) {
				continue;
			}
			Race r;
			r.phase = 1;
			r.machine = h;
			r.machineID = machine->GetFormID();
			if (!InviteRacer(r)) {
				continue;
			}
			log::info("NUKA-ZAPPER RACE {:08X}: settlers gathering for a race", r.machineID);
			_races.push_back(std::move(r));
			return true;
		}
		return false;
	}

	bool Arcade::InviteRacer(Race& a_r)
	{
		const auto& L = sim::layout::Zapper();
		auto        machine = a_r.machine.get();
		auto*       weapon = Forms::WeaponOf(sim::GameId::kNukaZapperRace);
		auto*       markerForm = Forms::Bound(RE::TESForm::GetFormByID(Forms::kXMarkerHeading));
		Frame       f;
		if (!machine || !weapon || !markerForm || !FrameOf(machine.get(), f)) {
			return false;
		}
		int lane = -1;
		for (int l : L.settlerLanes) {
			if (l >= 0 && l < static_cast<int>(L.lanes.size()) &&
				std::ranges::none_of(a_r.racers, [&](const Racer& a_q) { return a_q.lane == l; })) {
				lane = l;
				break;
			}
		}
		auto* who = lane >= 0 ? PickSettler(machine.get(), sim::GameId::kNukaZapperRace) : nullptr;
		if (!who) {
			return false;
		}
		Racer q;
		q.lane = lane;
		q.actor = who->GetHandle();
		q.actorID = who->GetFormID();
		if (const char* n = who->GetDisplayFullName(); n && *n) {
			q.name = n;
		}
		const auto& ln = L.lanes[lane];
		q.marker = Spawn(markerForm, machine.get(), ToWorld(f, sim::Vec3{ ln.gun.x, L.standY, 0.0 }));
		// what they aim at: their lane's target, put this far under its centre - aimed at the centre itself, their
		// stream went over it, into the Nuka-Cola bottle above (an NPC aims at a point this much over what it's given)
		q.aimAt = Spawn(markerForm, machine.get(), ToWorld(f, ln.target + sim::Vec3{ kZapperAimRight, 0.0, -kZapperAimUnder }));
		auto marker = q.marker.get();
		if (!marker) {
			Remove(q.aimAt);
			return false;
		}
		q.skill = std::uniform_real_distribution<double>(0.0, 1.0)(_showRng);
		q.ready = std::make_shared<std::atomic<std::int32_t>>(-1);
		auto ready = q.ready;
		Papyrus::Call("NpcStartGallery", { static_cast<std::int32_t>(q.actorID), Papyrus::Id(weapon), Papyrus::Id(marker.get()) },
			[ready](std::int32_t a_r) { *ready = a_r; });
		log::info("NUKA-ZAPPER RACE {:08X}: {} ({:08X}) coming to race (lane {})", a_r.machineID, q.name, q.actorID, lane + 1);
		a_r.racers.push_back(std::move(q));
		return true;
	}

	void Arcade::TickRaces(double a_dt)
	{
		if (Settings::Get().settlersOn) {
			_nextRace -= a_dt;
			if (_nextRace <= 0.0) {
				_nextRace = StartRace() ? kShowNextTry : kShowRetry;
			}
		}
		for (std::size_t i = 0; i < _races.size(); ++i) {
			TickRace(_races[i], a_dt);
		}
		std::erase_if(_races, [](const Race& a_r) { return a_r.phase == 0; });
	}

	void Arcade::TickRace(Race& a_r, double a_dt)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto  machine = a_r.machine.get();
		Frame f;
		if (!machine || machine->IsDisabled() || machine->IsDeleted() || !player || !FrameOf(machine.get(), f) ||
			(machine->data.location - player->data.location).Length() > kShowYouAway) {
			EndRace(a_r, "the machine's gone or you've left");
			return;
		}
		auto* weapon = Forms::WeaponOf(sim::GameId::kNukaZapperRace);
		// each settler: on their way, taking up the gun, ready
		for (auto& q : a_r.racers) {
			auto  ref = q.actor.get();
			auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
			if (!actor || actor->IsDead(false) || !actor->Get3D() || actor->IsInCombat()) {
				log::info("NUKA-ZAPPER RACE {:08X}: {} dropped out (gone, down or fighting)", a_r.machineID, q.name);
				LetGo(q);
				q.lane = -1;
				continue;
			}
			if (q.state == 0) {
				const auto r = q.ready->load();
				if (r == 0 || r >= 10) {
					log::info("NUKA-ZAPPER RACE {:08X}: {} couldn't come ({})", a_r.machineID, q.name, r);
					_refused.push_back(q.actorID);
					q.ready.reset();
					LetGo(q);
					q.lane = -1;
					continue;
				}
				if (r < 0) {
					continue;
				}
				auto marker = q.marker.get();
				if (!marker) {
					LetGo(q);
					q.lane = -1;
					continue;
				}
				const float d = std::hypot(actor->data.location.x - marker->data.location.x, actor->data.location.y - marker->data.location.y);
				q.walked += a_dt;
				if (d < q.best - 10.0f) {
					q.best = d;
					q.stall = 0.0;
				} else {
					q.stall += a_dt;
				}
				bool there = d < kShowArrived || (d < kShowAtSpot && q.stall > 1.5) || (q.walked > kShowWalk && d < 120.0f);
				if (!there && q.walked > kShowWalk) {
					log::info("NUKA-ZAPPER RACE {:08X}: {} took too long ({:.0f} away)", a_r.machineID, q.name, d);
					LetGo(q);
					q.lane = -1;
					continue;
				}
				if (!there) {
					if (q.stall > kShowStallResend) {
						q.stall = 0.0;
						q.best = d;
						Papyrus::Call("NpcWalk", { static_cast<std::int32_t>(q.actorID), Papyrus::Id(marker.get()), 1 });
					}
					continue;
				}
				q.state = 1;
				Snapshot(actor);  // (what they wear: put back on after)
				q.armed = std::make_shared<std::atomic<std::int32_t>>(-1);
				auto armed = q.armed;
				auto aimAt = q.aimAt.get();
				Papyrus::Call("NpcTakeUp", { static_cast<std::int32_t>(q.actorID), Papyrus::Id(weapon), Papyrus::Id(marker.get()), Papyrus::Id(aimAt.get()),
											   Papyrus::Id(weapon ? weapon->weaponData.ammo : nullptr) },
					[armed](std::int32_t a_r2) { *armed = a_r2; });
				continue;
			}
			if (q.state == 1) {
				const auto r = q.armed->load();
				if (r == 0) {
					LetGo(q);
					q.lane = -1;
				} else if (r > 0) {
					q.state = 2;
					q.added = r == 2;
					log::info("NUKA-ZAPPER RACE {:08X}: {} ready in lane {}", a_r.machineID, q.name, q.lane + 1);
				}
			}
		}
		std::erase_if(a_r.racers, [](const Racer& a_q) { return a_q.lane < 0; });

		if (a_r.phase == 1) {
			a_r.gathered += a_dt;
			const auto ready = std::ranges::count_if(a_r.racers, [](const Racer& a_q) { return a_q.state == 2; });
			// more settlers, a lane each (all but yours), while they gather
			a_r.invite -= a_dt;
			if (a_r.invite <= 0.0) {
				a_r.invite = kShowNextTry;
				if (a_r.racers.size() < sim::layout::Zapper().settlerLanes.size()) {
					InviteRacer(a_r);
				}
			}
			if (ready + (a_r.youIn ? 1 : 0) >= 2 && a_r.countdown < 0.0) {
				a_r.countdown = sim::layout::Zapper().gatherSeconds;
				log::info("NUKA-ZAPPER RACE {:08X}: two ready: the race starts in {:.0f}s", a_r.machineID, a_r.countdown);
				if (a_r.youIn) {
					Notify(std::format("Nuka-Zapper Race: the race starts in {:.0f} seconds. Get behind your gun.", a_r.countdown));
				}
			}
			if (a_r.countdown >= 0.0) {
				a_r.countdown -= a_dt;
				if (a_r.countdown <= 0.0) {
					BeginRace(a_r);
				}
			} else if (a_r.gathered > sim::layout::Zapper().gatherLongest) {
				EndRace(a_r, "not two settlers ready to race");
			} else if (a_r.racers.empty() && a_r.gathered > 20.0 && !a_r.youIn) {
				EndRace(a_r, "nobody came");
			}
			return;
		}
		if (a_r.phase == 2 && a_r.withYou) {
			return;  // (your game runs the race: its shots shown from your Tick)
		}
		if (a_r.phase == 2 && a_r.game) {
			a_r.game->Update(a_dt, sim::Input{});
			RaceShots(a_r, *a_r.game, a_dt);
			for (const auto& s : a_r.game->TakeSounds()) {
				Sounds::Play(sim::GameId::kNukaZapperRace, s.cue, machine.get(), s.gain);
			}
			PlacePieces(f, *a_r.game, sim::GameId::kNukaZapperRace, machine.get(), a_r.pieces, a_r.pieceModel, a_r.frozen);
			const auto& lanes = sim::layout::Zapper().lanes;
			for (std::size_t i = 0; i < a_r.fixtures.size() && i < lanes.size(); ++i) {
				if (auto ref = a_r.fixtures[i].get()) {
					Freeze(ref.get(), a_r.frozen);
					PlaceNode(ref.get(), ToWorld(f, lanes[i].target), f.rot, f.scale, false);
				}
			}
			if (a_r.game->Over()) {
				const int w = sim::ZapperWinner(*a_r.game);
				const auto it = std::ranges::find_if(a_r.racers, [&](const Racer& a_q) { return a_q.lane == w; });
				_lastScores[a_r.machineID] = a_r.game->Score();
				log::info("NUKA-ZAPPER RACE {:08X}: {} won (lane {})", a_r.machineID, it != a_r.racers.end() ? it->name : "?", w + 1);
				EndRace(a_r, nullptr);
			}
		}
	}

	bool Arcade::CanFire(RE::Actor* a_actor, const std::string& a_name)
	{
		if (!a_actor) {
			return false;
		}
		auto* ammo = a_actor->GetCurrentAmmo(RE::BGSEquipIndex{ 0 });
		const char* from = "loaded";
		if (!ammo) {
			// not loaded yet as far as the process knows (a settler's Thirst Zapper never was): the ammo its equipped
			// instance takes, or failing that the gun's own, in their pockets (what it fires is the gun's: the shot's its
			// animation, nothing calls Weapon.Fire)
			from = "the gun's own";
			if (auto* list = a_actor->inventoryList) {
				RE::BSAutoReadLock lock{ list->rwLock };
				for (auto& item : list->data) {
					auto* weap = item.object ? item.object->As<RE::TESObjectWEAP>() : nullptr;
					if (!weap) {
						continue;
					}
					for (auto* stack = item.stackData.get(); stack && !ammo; stack = stack->nextStack.get()) {
						if (!stack->IsEquipped()) {
							continue;
						}
						auto* extra = stack->extra.get();
						auto* inst = extra ? extra->GetByType<RE::ExtraInstanceData>() : nullptr;
						auto* data = inst && inst->data ? static_cast<RE::TESObjectWEAP::InstanceData*>(inst->data.get()) : nullptr;
						ammo = data && data->ammo ? data->ammo : weap->weaponData.ammo;
						from = data && data->ammo ? "its instance's" : "the gun's own";
					}
					if (ammo) {
						break;
					}
				}
			}
		}
		if (!ammo) {
			static std::unordered_set<RE::TESFormID> told;
			if (told.insert(a_actor->GetFormID()).second) {
				log::warn("{}'s gun has nothing to fire just now (no ammo for it): no shot from it till it has", a_name);
			}
			return false;
		}
		{
			static std::unordered_set<RE::TESFormID> told;
			if (told.insert(a_actor->GetFormID()).second) {
				log::info("{}'s gun fires ammo {:08X} ({})", a_name, ammo->GetFormID(), from);
			}
		}
		if (_loaded.insert(a_actor->GetFormID() ^ ammo->GetFormID()).second) {
			Papyrus::Call("AddTo", { static_cast<std::int32_t>(a_actor->GetFormID()), static_cast<std::int32_t>(ammo->GetFormID()), 50 });
		}
		return true;
	}

	void Arcade::RaceShots(Race& a_r, sim::Game& a_game, double a_dt)
	{
		auto* weapon = Forms::WeaponOf(sim::GameId::kNukaZapperRace);
		for (auto& q : a_r.racers) {
			q.sinceShot += a_dt;
			auto  r = q.actor.get();
			auto* a = r ? r->As<RE::Actor>() : nullptr;
			q.steady = a && a->GetWeaponMagicDrawn() ? q.steady + a_dt : 0.0;  // (as at the gallery: drawn a while first)
			q.everDrawn = q.everDrawn || (a && a->GetWeaponMagicDrawn());
			if (a && q.state == 2 && q.everDrawn && !a->GetWeaponMagicDrawn()) {
				q.drawAsked -= a_dt;  // (never put away while they race: out again)
				if (q.drawAsked <= 0.0) {
					q.drawAsked = 1.0;
					Papyrus::Call("NpcDraw", { static_cast<std::int32_t>(q.actorID) });
				}
			}
		}
		for (const auto& [lane, hit] : sim::TakeZapperShotsHit(a_game)) {
			auto it = std::ranges::find_if(a_r.racers, [&](const Racer& a_q) { return a_q.lane == lane && a_q.state == 2; });
			if (it == a_r.racers.end() || it->sinceShot < 0.35 || it->steady < kSteadyBeforeShot || !weapon) {
				continue;  // (a squirt's animation still going: the next one shown)
			}
			auto  ref = it->actor.get();
			if (!CanFire(ref ? ref->As<RE::Actor>() : nullptr, it->name)) {
				continue;
			}
			// where this squirt goes, as the race counts it: on the bullseye for a hit, a little off its rim for a miss
			// (what they aim at moved there; their aim follows it)
			if (auto aimAt = it->aimAt.get(); aimAt && lane >= 0 && lane < static_cast<int>(sim::layout::Zapper().lanes.size())) {
				Frame f;
				auto  machine = a_r.machine.get();
				if (machine && FrameOf(machine.get(), f)) {
					sim::Vec3 p = sim::layout::Zapper().lanes[lane].target + sim::Vec3{ kZapperAimRight, 0.0, -kZapperAimUnder };
					if (!hit) {
						const double a = std::uniform_real_distribution<double>(0.0, 6.2831853)(_showRng);
						const double r = std::uniform_real_distribution<double>(14.0, 24.0)(_showRng);
						p = p + sim::Vec3{ std::cos(a) * r, 0.0, std::sin(a) * r };
					}
					const RE::NiPoint3 w = ToWorld(f, p);
					aimAt->data.location = RE::NiPoint3A{ w.x, w.y, w.z };
				}
			}
			it->sinceShot = 0.0;
			static std::atomic<int> told{ -1 };  // (how the squirts go: logged once each way)
			const std::string       name = it->name;
			Papyrus::Call("NpcShoot", { static_cast<std::int32_t>(it->actorID), Papyrus::Id(weapon), Papyrus::Id(weapon->weaponData.ammo) },
				[name](std::int32_t a_r) {
					if (told.exchange(a_r) != a_r) {
						log::info("NUKA-ZAPPER RACE: {}'s squirts: {}", name,
							a_r == 1 ? "the gun's own shot animation (RaiderFireSingle)" :
							a_r == 2 ? "the shot animation wouldn't play: fired without it" : "nobody to fire it");
					}
				});
		}
	}

	void Arcade::BeginRace(Race& a_r)
	{
		auto machine = a_r.machine.get();
		Frame f;
		if (!machine || !FrameOf(machine.get(), f)) {
			EndRace(a_r, "the machine's not loaded");
			return;
		}
		// those not ready by now: not in this one
		for (auto& q : a_r.racers) {
			if (q.state != 2) {
				LetGo(q);
				q.lane = -1;
			}
		}
		std::erase_if(a_r.racers, [](const Racer& a_q) { return a_q.lane < 0; });
		if (a_r.racers.size() + (a_r.youIn ? 1 : 0) < 2 || (a_r.youIn && _playing)) {
			a_r.countdown = -1.0;  // (back to gathering)
			return;
		}
		const auto&              L = sim::layout::Zapper();
		std::vector<double>      skill(L.lanes.size(), -1.0);
		std::vector<std::string> names(L.lanes.size());
		for (const auto& q : a_r.racers) {
			skill[q.lane] = q.skill;
			names[q.lane] = q.name;
		}
		if (a_r.youIn) {
			// you and the settlers: your game is the race, you in your lane, theirs theirs, the rest empty
			Start(machine.get(), sim::GameId::kNukaZapperRace);
			if (!_game) {
				EndRace(a_r, "your game couldn't start");
				return;
			}
			sim::SetZapperRacers(*_game, L.playerLane, skill, names);
			a_r.youIn = false;
			a_r.withYou = true;
			a_r.phase = 2;
			log::info("NUKA-ZAPPER RACE {:08X}: you and {} settlers racing", a_r.machineID, a_r.racers.size());
			return;
		}
		a_r.game = sim::MakeGame(sim::GameId::kNukaZapperRace);
		a_r.game->Start(static_cast<std::uint32_t>(_showRng()));
		sim::SetZapperRacers(*a_r.game, -1, skill, names);
		std::vector<sim::Piece> pieces;
		a_r.game->Pieces(pieces);
		const auto models = Forms::PiecesOf(sim::GameId::kNukaZapperRace);
		for (const auto& p : pieces) {
			auto* form = p.model >= 0 && p.model < static_cast<int>(models.size()) ? Forms::Bound(Forms::Own(models[p.model])) : nullptr;
			a_r.pieces.push_back(form ? Spawn(form, machine.get(), ToWorld(f, p.pos)) : RE::ObjectRefHandle{});
			a_r.pieceModel.push_back(p.model);
		}
		if (auto* target = Forms::Bound(Forms::Own(Forms::kZapperTarget))) {
			for (const auto& lane : L.lanes) {
				a_r.fixtures.push_back(Spawn(target, machine.get(), ToWorld(f, lane.target)));
			}
		}
		Sounds::Music(sim::GameId::kNukaZapperRace, machine.get(), true);
		a_r.phase = 2;
		log::info("NUKA-ZAPPER RACE {:08X}: {} settlers racing", a_r.machineID, a_r.racers.size());
	}

	void Arcade::EndRace(Race& a_r, const char* a_why)
	{
		if (a_r.phase == 0) {
			return;
		}
		if (a_why) {
			log::info("NUKA-ZAPPER RACE {:08X}: called off ({})", a_r.machineID, a_why);
		}
		const bool raced = a_r.phase == 2;
		if (a_r.youIn) {
			GiveToken();
			Notify("Nuka-Zapper Race: nobody came to race. Your token's back.");
		}
		for (auto& q : a_r.racers) {
			if (raced && q.state == 2) {
				_played[Played(q.actorID, sim::GameId::kNukaZapperRace)] = kShowTurnAbout;
			}
			LetGo(q);
		}
		if (raced && !a_r.withYou) {
			if (auto machine = a_r.machine.get()) {
				Sounds::Music(sim::GameId::kNukaZapperRace, machine.get(), false);
			}
		}
		for (auto& h : a_r.pieces) {
			Remove(h);
		}
		for (auto& h : a_r.fixtures) {
			Remove(h);
		}
		const auto& set = Settings::Get();
		_rest[a_r.machineID] = raced ? std::uniform_real_distribution<double>(set.settlersGapShortest, std::max(set.settlersGapShortest, set.settlersGapLongest))(_showRng) :
		                               kShowRetry;
		a_r = {};
	}

	// ------------------------------------------------------------------------ saves

	void Arcade::Save(const F4SE::SerializationInterface* a_intfc)
	{
		auto& self = Get();
		Income::Save(a_intfc);
		if (self._swingBoost != 0.0f && a_intfc->OpenRecord(kSwingType, 1)) {
			a_intfc->WriteRecordData(&self._swingBoost, sizeof(self._swingBoost));
		}
		// settlers held at a machine (AI off): let go after a load of this save
		{
			std::vector<Held> held;
			for (const auto& r : self._races) {
				const auto* zw = Forms::WeaponOf(sim::GameId::kNukaZapperRace);
				for (const auto& q : r.racers) {
					held.push_back({ nullptr, q.actorID, zw ? zw->GetFormID() : 0, q.state == 2 && q.added });
				}
			}
			for (const auto& sh : self._shows) {
				if (sh.phase == Show::Phase::kPlaying) {
					held.push_back({ nullptr, sh.actorID, sh.weapon ? sh.weapon->GetFormID() : 0, sh.added });
				} else if (sh.phase == Show::Phase::kWalking || sh.phase == Show::Phase::kArming) {
					held.push_back({ nullptr, sh.actorID, sh.weapon ? sh.weapon->GetFormID() : 0, false });
				}
			}
			for (const auto& h : self._letGo) {
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
		self._swingFromSave = 0.0f;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type == kSwingType) {
				a_intfc->ReadRecordData(&self._swingFromSave, sizeof(self._swingFromSave));
				continue;
			}
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
