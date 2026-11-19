#pragma once

#include "sim/Arcade.h"

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
			const auto it = _lastScores.find(a_machine);
			return it != _lastScores.end() ? it->second : 0;
		}
		int Best(sim::GameId a_game) const { return a_game != sim::GameId::kCount ? _best[static_cast<int>(a_game)] : 0; }

		// something of the game's hit by you (on the game's thread): Whac-a-Commie's heads are whacked this way
		void OnHit(RE::TESObjectREFR* a_target);

		// a machine activated by you (on the game's thread)
		void OnActivated(RE::TESObjectREFR* a_machine);

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
		RE::ObjectRefHandle Spawn(RE::TESBoundObject* a_form, RE::TESObjectREFR* a_near, const RE::NiPoint3& a_pos);
		void                Remove(RE::ObjectRefHandle& a_handle);
		void                PlaceNode(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_pos, const RE::NiMatrix3& a_rot, float a_scale, bool a_hidden);
		void                Freeze(RE::TESObjectREFR* a_ref);
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

		std::array<int, static_cast<int>(sim::GameId::kCount)> _best{};
		RE::TESFormID                                         _machineID{ 0 };
		std::unordered_map<RE::TESFormID, int>                _lastScores;  // each machine's last game (this session)

		mutable std::mutex _hudMutex;
		HudState           _hud;
	};
}
