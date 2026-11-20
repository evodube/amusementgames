#pragma once

#include <set>

#include "sim/Arcade.h"

#include <unordered_set>

// The arcade in the game: the machines you activate, the game you're playing (sim::Game) and what it shows, the
// tokens it takes and the tickets it pays, your best scores, and what the HUD draws.
//
// You play standing at the machine in first person: you look where you aim and the left mouse button throws, rolls,
// whacks, squirts or shoots (held: charges a throw). Your movement, attacks and activation are held while you play
// (an input layer); X leaves early (paid for the score so far).
namespace ag
{
	class Arcade
	{
	public:
		static Arcade& Get()
		{
			static Arcade instance;
			return instance;
		}

		// the activation listener (once)
		void Listen();

		// every frame, on the render thread (the Present hook): the keys read, the game's tick queued for the game's thread
		void OnPresent();

		// leaving a game the hard way: a load, the main menu
		void Abort(const char* a_why);

		// what the HUD draws (a copy, for the render thread)
		struct HudState
		{
			bool                       playing{ false };   // at a machine
			bool                       results{ false };   // the game just ended: its banner
			sim::GameId                game{ sim::GameId::kCount };
			int                        score{ 0 };
			int                        best{ 0 };
			int                        tokens{ 0 };
			int                        tickets{ 0 };       // you have
			sim::HudInfo               info;
			int                        lastPoints{ 0 };
			double                     sinceLastPoints{ 99.0 };
			int                        won{ 0 };           // tickets this game paid
			bool                       newBest{ false };
			std::string                verdict;      // the zapper race: won or lost (and who won)
			double                     resultsAge{ 0.0 };
			std::string                leaveKey{ "X" };
			std::vector<std::array<float, 2>> guide;  // the aim line on the screen (0..1), from the ball out
		};
		HudState Hud() const
		{
			std::scoped_lock lock(_hudMutex);
			return _hud;
		}

		// F4SE serialization: the best scores
		static void Save(const F4SE::SerializationInterface* a_intfc);
		static void Load(const F4SE::SerializationInterface* a_intfc);
		static void Revert(const F4SE::SerializationInterface* a_intfc);

		// what a machine's scoreboard shows: the score of the game being played on it, or of the last one; your best at a
		// game
		int ScoreOn(RE::TESFormID a_machine) const
		{
			if (_playing && _game && _machineID == a_machine) {
				return _game->Score();
			}
			if (const auto* s = ShowOn(a_machine); s && s->phase == Show::Phase::kPlaying && s->game) {
				return s->game->Score();  // (a settler's game)
			}
			for (const auto& r : _races) {
				if (r.phase == 2 && r.machineID == a_machine && r.game) {
					return r.game->Score();  // (settlers racing: the leader's)
				}
			}
			const auto it = _lastScores.find(a_machine);
			return it != _lastScores.end() ? it->second : 0;
		}
		bool PlayingOn(RE::TESFormID a_machine) const { return _playing && _machineID == a_machine; }
		// Bandit Roundup 1v1 on a machine (yours with a settler, or two settlers'): the left and right shooters' scores
		bool DuelScoresOn(RE::TESFormID a_machine, int& a_left, int& a_right) const;
		// how many of a machine's props are on show: all of them, or while you play on it what the game says
		int PropsShown(RE::TESFormID a_machine, int a_all) const
		{
			if (const auto* s = ShowOn(a_machine); s && s->phase == Show::Phase::kPlaying) {
				// (a settler's playing: the gun or mallet's in their hands; Atomic Rollers' pocket as the game has it)
				if (!s->game) {
					return 0;
				}
				sim::HudInfo h;
				s->game->Hud(h);
				return h.waiting < 0 ? 0 : std::min(a_all, h.waiting);
			}
			if (!PlayingOn(a_machine) || !_game) {
				return a_all;
			}
			sim::HudInfo h;
			_game->Hud(h);
			return h.waiting < 0 ? 0 : std::min(a_all, h.waiting);
		}
		int Best(sim::GameId a_game) const { return a_game != sim::GameId::kCount ? _best[static_cast<int>(a_game)] : 0; }
		// the Prize Counter's screen: your tickets and best scores into its globals (the game's thread: as you activate
		// it, before its screen's drawn, and after each game)
		void RefreshCounter() const;

		// something of the game's hit by you (on the game's thread): Whac-a-Commie's heads are whacked this way
		void OnHit(RE::TESObjectREFR* a_target);
		void OnSettlerHit(RE::TESObjectREFR* a_target, RE::TESFormID a_cause);

		// a machine activated by you (on the game's thread)
		void OnActivated(RE::TESObjectREFR* a_machine);
		// the Activate button down or up, and how long it's been held (the game's input)
		void OnActivateButton(bool a_down, float a_held);

		// after a load: settlers held at a machine when the game was saved let go (their AI back on, the lent rifle
		// taken back)
		void ReleaseHeld();

	private:
		Arcade() = default;

		struct Frame
		{
			RE::NiPoint3  origin;
			RE::NiMatrix3 rot;  // the machine's world rotation
			float         scale{ 1.0f };
		};

		void Tick();
		bool FrameOf(RE::TESObjectREFR* a_machine, Frame& a_out) const;
		RE::NiPoint3 ToWorld(const Frame& a_f, const sim::Vec3& a_p) const;
		sim::Vec3    ToLocal(const Frame& a_f, const RE::NiPoint3& a_p) const;
		sim::Vec3    DirToLocal(const Frame& a_f, const RE::NiPoint3& a_d) const;

		void Start(RE::TESObjectREFR* a_machine, sim::GameId a_game);
		void Finish(bool a_early);
		void Cleanup();
		void PlacePieces(const Frame& a_f);
		void PlacePieces(const Frame& a_f, const sim::Game& a_game, sim::GameId a_id, RE::TESObjectREFR* a_machine,
			std::vector<RE::ObjectRefHandle>& a_pieces, std::vector<int>& a_models, std::vector<RE::NiAVObject*>& a_frozen,
			const std::vector<bool>* a_keepHidden = nullptr);
		RE::ObjectRefHandle Spawn(RE::TESBoundObject* a_form, RE::TESObjectREFR* a_near, const RE::NiPoint3& a_pos);
		void                Remove(RE::ObjectRefHandle& a_handle);
		void                PlaceNode(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_pos, const RE::NiMatrix3& a_rot, float a_scale, bool a_hidden, bool a_moveHidden = false);
		void Freeze(RE::TESObjectREFR* a_ref, std::vector<RE::NiAVObject*>& a_frozen);
		void                BlockControls(bool a_block, bool a_allowFighting = false);
		void                Arm(sim::GameId a_game);
		void                Disarm();
		void                PublishHud(const Frame* a_frame = nullptr);
		void                Notify(const std::string& a_text);

		int  Count(RE::TESBoundObject* a_item) const;
		bool TakeToken();

		// render thread -> game thread
		std::atomic<bool> _tickPending{ false };
		std::atomic<bool> _lmb{ false };
		std::atomic<bool> _leavePressed{ false };     // the leave key went down
		bool              _leaveWas{ false };
		// the Activate button (the game's own button events): a machine tapped or held. Bandit Roundup tapped you play
		// alone, held (kHoldToDuel) a settler's called to take you on
		std::atomic<bool>   _activateDown{ false };
		std::atomic<double> _activateHeld{ 0.0 };     // ...down this long (now, or the last press)
		struct PendingActivation
		{
			RE::ObjectRefHandle machine;
			double              age{ 0.0 };
		};
		PendingActivation _pending;
		void              ResolvePending(double a_dt);

		// your Bandit Roundup 1v1: your token in, a settler called (a Show, vsYou) to the other side; the game starts
		// once they've the rifle up. Activate it again while waiting to leave, token back
		struct Duel
		{
			bool                on{ false };
			bool                started{ false };
			RE::ObjectRefHandle machine;
			RE::TESFormID       machineID{ 0 };
			int                 yourSide{ 0 };  // 0 left, 1 right (the settler the other)
			int                 tries{ 0 };     // settlers called (one couldn't come: another)
		};
		Duel _duel;
		void StartDuel(RE::TESObjectREFR* a_machine);
		bool CallRival();
		void TickDuel();
		void EndDuel(const char* a_notify, bool a_tokenBack);

		// the game (game thread)
		std::unique_ptr<sim::Game>           _game;
		sim::GameId                          _gameId{ sim::GameId::kCount };
		RE::ObjectRefHandle                  _machine;
		std::vector<RE::ObjectRefHandle>     _pieces;
		std::vector<int>                     _pieceModel;
		std::vector<RE::ObjectRefHandle>     _fixtures;  // the zapper's targets: put up for the game
		std::vector<RE::NiAVObject*>         _frozen;
		std::chrono::steady_clock::time_point _last{};
		bool                                 _playing{ false };
		bool                                 _held{ false };       // the fire button, as the game saw it last
		bool                                 _hurry{ false };
		double                               _resultsLeft{ 0.0 };
		bool                                 _controlsBlocked{ false };
		RE::BSTSmartPointer<RE::BSInputEnableLayer> _inputLayer;
		bool                                 _wasThird{ false };

		// the game's weapon in your hands (lent if you hadn't one) and the ammo lent with it
		struct Lent
		{
			RE::TESObjectWEAP*        weapon{ nullptr };
			bool                      added{ false };
			RE::TESAmmo*              ammo{ nullptr };
			int                       ammoBefore{ 0 };
			int                       ammoGiven{ 0 };
			std::shared_ptr<std::atomic<std::int32_t>> had{ std::make_shared<std::atomic<std::int32_t>>(0) };  // what you had out
			std::uint32_t             clip{ 0 };  // the most rounds seen in its magazine: kept topped up to this
		};
		void TopUpAmmo();
		Lent _lent;

		// Whac-a-Commie's Agility swing, gone (0.9.6): nothing's quickened now. What an older version raised
		// WeaponSpeedMult by (kept in the cosave, a save made mid-game) is taken off still
		void  Quicken(bool a_on);
		float _swingBoost{ 0.0f };
		float _swingFromSave{ 0.0f };

		std::array<int, static_cast<int>(sim::GameId::kCount)> _best{};
		RE::TESFormID                                         _machineID{ 0 };
		std::unordered_map<RE::TESFormID, int>                _lastScores;  // each machine's last game (this session)

		mutable std::mutex _hudMutex;
		HudState           _hud;

		// a settler playing a game (Bandit Roundup) while you're about: one at a time, on a machine you're not at. They
		// walk to the spot in front of it, take up the rifle (lent if they've none), and are held there (their AI off)
		// while the game plays them; then let go
		struct Show
		{
			enum class Phase
			{
				kNone,
				kWalking,  // on their way (AmusementGamesQuest's walk package: AmusementGamesNative.NpcStartGallery)
				kArming,   // there: the rifle being taken up (NpcTakeUp)
				kWaiting,  // a 1v1: the rifle up, waiting for their rival
				kPlaying
			};
			Phase                                  phase{ Phase::kNone };
			RE::ObjectRefHandle                    actor;
			RE::TESFormID                          actorID{ 0 };
			std::string                            name;    // (for the log)
			RE::ObjectRefHandle                    machine;
			RE::TESFormID                          machineID{ 0 };
			sim::GameId                            gameId{ sim::GameId::kCount };
			RE::ObjectRefHandle                    marker;  // where they stand
			RE::ObjectRefHandle                    aimAt;   // ...and what they aim at (the middle of the targets)
			sim::Vec3                              eye;     // their eye, in the machine's space
			RE::TESObjectWEAP*                     weapon{ nullptr };
			bool                                   added{ false };  // the rifle lent (taken back after)
			double                                 walked{ 0.0 };   // seconds on the way
			double                                 onTarget{ 0.0 }; // a shot held back this long, turning onto the target
			double                                 steady{ 0.0 };   // their weapon drawn this long
			double                                 drawAsked{ 0.0 };  // ...put away: till it's asked out again
			double                                 diag{ 0.0 };
			double                                 diagT{ 0.0 };
			bool                                   everDrawn{ false };
			bool                                   loaded{ false };  // (their magazine first filled: logged)
			// a throw's ball in her hand: shown once the throw's actually playing (NpcRoll / NpcToss done: her turn to the
			// machine and any step back to her spot made, which jumped the hand), and a frame's jump of the hand ignored
			std::shared_ptr<std::atomic<bool>>     throwOn{ std::make_shared<std::atomic<bool>>(false) };
			int                                    throwFrames{ 0 };
			bool                                   handShown{ false };
			RE::NiPoint3                           lastAt{};
			bool                                   lastAtOk{ false };
			int                                    jumps{ 0 };
			int                                    pickups{ 0 };    // (logged frame by frame: the first few)
			int                                    shotsLogged{ 0 };  // Bandit Roundup: the first shots' magazine logged
			double                                 magCheckIn{ -1.0 };
			std::uint32_t                          magBefore{ 0 };
			int                                    pickupLog{ -1 }; // frames of this one logged so far (-1: none)
			RE::NiPoint3                           lastActorAt{};
			float                                  lastHeading{ 0.0f };
			// Bandit Roundup 1v1: their side (-1 alone, 0 left, 1 right); the rival of yours, or of another settler (the
			// guest: their shots go into the host's game, the host's plays it); the other settler
			int                                    side{ -1 };
			bool                                   vsYou{ false };
			bool                                   guest{ false };
			RE::TESFormID                          partner{ 0 };
			double                                 waited{ 0.0 };
			double                                 skill{ 0.5 };
			double                                 stall{ 0.0 };    // ...no closer for this long
			float                                  best{ 1e9f };    // ...the closest they've been
			int                                    resends{ 0 };    // ...walks sent again with no step closer between
			std::shared_ptr<std::atomic<std::int32_t>> ready;  // NpcStartGallery: -1 not yet, 1 walking, 10+ not coming
			std::shared_ptr<std::atomic<std::int32_t>> armed;  // NpcTakeUp: -1 not yet, 0 didn't, 1 ready, 2 ready (lent)
			std::unique_ptr<sim::Game>             game;
			std::unique_ptr<sim::GalleryShooter>   shooter;  // Bandit Roundup's
			std::unique_ptr<sim::WhacPlayer>       whacker;  // Whac-a-Commie's
			std::unique_ptr<sim::Thrower>          thrower;  // Atomic Rollers' and Hoop Shot's
			RE::ObjectRefHandle                    handBall; // ...the ball in their hand
			double                                 sinceThrow{ 9.0 };
			bool                                   holding{ false };
			bool                                   ballShown{ false };  // ...the ball in their hands shown this throw
			int                                    hits{ 0 };  // ...their mallet's hits on the heads
			int                                    misses{ 0 };
			double                                 swingT{ -1.0 };  // ...the swing coming down this long (-1: none)
			float                                  swingBest{ 1e9f };  // ...its mallet the nearest it's been to the head they're after
			int                                    swingTarget{ -1 };
			float                                  swingHighZ{ -1e9f };  // ...the mallet head's highest yet (the wind-up)...
			bool                                   swingJudged{ false };
			float                                  swingLowZ{ 1e9f };  // ...its lowest since, and there
			RE::NiPoint3                           swingLowAt{};
			int                                    swingsLogged{ 0 };
			RE::NiPoint3                           swingMiss;          // ...from the mallet then to that head  // ...the throw's animation on, the ball in their hand(s)
			std::vector<RE::ObjectRefHandle>       pieces;
			std::vector<int>                       pieceModel;
			std::vector<RE::NiAVObject*>           frozen;
		};
		struct Held  // a settler to let go once Papyrus is done with them
		{
			std::shared_ptr<std::atomic<std::int32_t>> ready;  // (null: known held)
			RE::TESFormID                          actor{ 0 };
			RE::TESFormID                          weapon{ 0 };
			bool                                   added{ false };
		};
		void TickShow(double a_dt);
		void TickOne(Show& a_s, double a_dt);
		bool StartShow();
		bool CallToPlay(RE::TESObjectREFR* a_machine, RE::Actor* a_who, sim::GameId a_game, int a_side, bool a_vsYou, bool a_guest,
			RE::TESFormID a_partner);
		void BeginGame(Show& a_s, RE::TESObjectREFR* a_machine, const Frame& a_f);
		Show* PartnerOf(const Show& a_s);
		void EndShow(Show& a_s, const char* a_why);
		void Release(RE::TESFormID a_actor, RE::TESFormID a_weapon, bool a_added);
		// the settler's game on a machine (a 1v1's host: not your rival, or the other settler's)
		const Show* ShowOn(RE::TESFormID a_machine) const
		{
			for (const auto& s : _shows) {
				if (s.phase != Show::Phase::kNone && s.machineID == a_machine && !s.guest && !s.vsYou) {
					return &s;
				}
			}
			return nullptr;
		}
		std::vector<Show>  _shows;  // settlers' games: one a machine, as many as there are machines free
		// a 1v1's final scores (left, right), kept on its machine's board till the next game there
		std::unordered_map<RE::TESFormID, std::pair<int, int>> _lastDuel;
		// settlers still linked to a spot no game's using (let go late, or a walk that outlived its game): let go
		void   SweepStragglers(double a_dt);
		void   CallOffAtGamesInPlay();
		std::set<std::pair<RE::TESFormID, RE::TESFormID>> _bystanders;  // (machine, actor): logged about it mid-game
		int                                               _bystanderTick{ 0 };  // (the sweep's seconds: looked for every 5th)
		double _sweep{ 1.0 };
		std::unordered_map<RE::TESFormID, double> _swept;  // (let go lately: not again while Papyrus does it)

		// settlers racing at Nuka-Zapper Race: up to one a lane but yours. They gather (each walks to the spot behind
		// their lane's gun and takes up a Thirst Zapper); 10 s after the second is ready the race starts. You can
		// join while they gather: the race starts then, you in your lane
		struct Racer
		{
			int                                    lane{ 0 };
			RE::ObjectRefHandle                    actor;
			RE::TESFormID                          actorID{ 0 };
			std::string                            name;
			RE::ObjectRefHandle                    marker;  // where they stand
			RE::ObjectRefHandle                    aimAt;   // their lane's target
			int                                    state{ 0 };  // 0 walking, 1 taking up the gun, 2 ready
			double                                 walked{ 0.0 };
			double                                 stall{ 0.0 };
			float                                  best{ 1e9f };
			double                                 skill{ 0.5 };
			double                                 sinceShot{ 9.0 };
			double                                 steady{ 0.0 };   // ...their gun drawn this long
			double                                 drawAsked{ 0.0 };
			bool                                   everDrawn{ false };
			bool                                   added{ false };
			std::shared_ptr<std::atomic<std::int32_t>> ready;
			std::shared_ptr<std::atomic<std::int32_t>> armed;
		};
		struct Race
		{
			int                                    phase{ 0 };  // 0 none, 1 gathering, 2 racing
			RE::ObjectRefHandle                    machine;
			RE::TESFormID                          machineID{ 0 };
			std::vector<Racer>                     racers;
			double                                 gathered{ 0.0 };  // seconds gathering
			double                                 countdown{ -1.0 };
			double                                 invite{ 0.0 };
			bool                                   withYou{ false };  // you're racing too (your game is the race)
			bool                                   youIn{ false };    // ...or will be: waiting with your token in for a racer
			std::unique_ptr<sim::Game>             game;     // (settlers only)
			std::vector<RE::ObjectRefHandle>       pieces;
			std::vector<int>                       pieceModel;
			std::vector<RE::ObjectRefHandle>       fixtures;
			std::vector<RE::NiAVObject*>           frozen;
		};
		void   TickRaces(double a_dt);
		void   TickRace(Race& a_r, double a_dt);
		bool   StartRace();
		bool   InviteRacer(Race& a_r);
		void   GiveToken();
		void   BeginRace(Race& a_r);
		void   EndRace(Race& a_r, const char* a_why);
		void   LetGo(Racer& a_q);
		void   RaceShots(Race& a_r, sim::Game& a_game, double a_dt);
		Race*  RaceOn(RE::TESFormID a_machine)
		{
			for (auto& r : _races) {
				if (r.phase != 0 && r.machineID == a_machine) {
					return &r;
				}
			}
			return nullptr;
		}
		RE::Actor* PickSettler(RE::TESObjectREFR* a_machine, sim::GameId a_game);
		// a settler's gun can fire: what it fires (its current ammo) known, a pocketful given once; none: no shot (firing a
		// gun with nothing to fire crashes the game)
		bool CanFire(RE::Actor* a_actor, const std::string& a_name);
		std::unordered_set<RE::TESFormID> _loaded;
		std::vector<Race>  _races;
		double             _nextRace{ 20.0 };
		std::unordered_map<RE::TESFormID, double> _rest;  // a machine's rest after a settler's game there (seconds)
		// a settler who's just played a game: not asked to that game again for this long (other games they may play)
		std::unordered_map<std::uint64_t, double> _played;
		static std::uint64_t Played(RE::TESFormID a_actor, sim::GameId a_game)
		{
			return (static_cast<std::uint64_t>(a_actor) << 8) | static_cast<std::uint64_t>(a_game);
		}
		std::vector<Held>  _letGo;  // (on their way when called off)
		double             _nextShow{ 20.0 };
		std::mt19937       _showRng{ std::random_device{}() };
		std::vector<Held>  _heldFromSave;  // (read from the cosave: let go after the load)
		// what a settler wore when they took up a game's weapon: put back on after (their inventory changed, the game
		// had them dress in whatever it picked)
		struct Worn
		{
			RE::TESBoundObject*                   object{ nullptr };
			RE::BSTSmartPointer<RE::TBO_InstanceData> instance;
			std::uint32_t                         stackID{ 0 };
		};
		struct Redress
		{
			RE::TESFormID     actor{ 0 };
			std::vector<Worn> worn;
			double            in{ 1.0 };  // seconds till it's done (the game's own re-dress first)
			int               times{ 2 }; // ...done this many times, a second after another
		};
		static std::vector<Worn> WornBy(RE::Actor* a_actor);
		void                     Snapshot(RE::Actor* a_actor);
		void                     TickRedress(double a_dt);
		std::unordered_map<RE::TESFormID, std::vector<Worn>> _worn;
		std::mutex                 _redressMutex;
		std::vector<Redress>       _redress;
		std::vector<RE::TESFormID> _refused;  // settlers who couldn't come lately (asked again once everyone has)
	};
}
