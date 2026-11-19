#include "Sounds.h"

#include "Forms.h"

// The base class's destructor is declared in CommonLibF4 but not defined there; our functors need one to link.
RE::BSScript::IStackCallbackFunctor::~IStackCallbackFunctor() = default;

namespace ag::Sounds
{
	namespace
	{
		using sim::Cue;
		using sim::GameId;

		// DLCNukaWorld.esm's sound descriptors (ids within it)
		namespace S
		{
			constexpr std::uint32_t kSledBallStart = 0x056B82, kSledBallMusic = 0x056B81, kSledBallFastStart = 0x056B80, kSledBallFast = 0x056B7F;
			constexpr std::uint32_t kSledBallTrackH = 0x04E4C6, kSledBallTrackL = 0x04E4C7;
			constexpr std::uint32_t kPoints[6]{ 0x04E4C9, 0x04E4CA, 0x04E4CB, 0x04E4CC, 0x04E4CD, 0x04E4CE };  // 10 .. 50, 100
			constexpr std::uint32_t kHoopStart = 0x04F13D, kHoopMusic = 0x056C6A, kHoopFastStart = 0x056C6B, kHoopFast = 0x056C6C;
			constexpr std::uint32_t kHoopNet = 0x04F140, kHoopBell = 0x04F13E, kHoopHigh = 0x04F13F;
			constexpr std::uint32_t kBasketballH = 0x04F086, kBasketballL = 0x04F087;
			constexpr std::uint32_t kWhacBell = 0x04F63E, kWhacLaugh = 0x04F63D, kWhacHit = 0x04F63C, kWhacFinish = 0x04F63B;
			constexpr std::uint32_t kWhacMusic = 0x04F63A, kWhacFastStart = 0x05619E, kWhacFast = 0x0560FC;
			constexpr std::uint32_t kZapStart = 0x04EA33, kZapStop = 0x04EA34, kZapWinMusic = 0x04EA31, kZapWinMech = 0x04EA32;
			constexpr std::uint32_t kZapMusic = 0x04EA2F, kZapMech = 0x04EA30, kZapSquirt = 0x052025;
			constexpr std::uint32_t kBanditBell = 0x056C61, kBanditMusic = 0x0561A1, kBanditFastStart = 0x0561A0, kBanditFast = 0x05619F;
			constexpr std::uint32_t kBanditHit = 0x04EEAD, kBanditWin = 0x04EEAC, kBanditShot = 0x0520B0;
			constexpr std::uint32_t kTicketDispense = 0x0504FD;
		}

		RE::TESForm* Sound(std::uint32_t a_id) { return Forms::Nuka(a_id); }

		// Sound.Play's answer: the playing instance, for the music to be stopped later
		class Started final : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			explicit Started(std::atomic<std::int32_t>* a_into) :
				_into(a_into)
			{}
			void CallQueued() override {}
			void CallCanceled() override {}
			void StartMultiDispatch() override {}
			void EndMultiDispatch() override {}
			void operator()(RE::BSScript::Variable a_result) override
			{
				if (_into && a_result.is<std::int32_t>()) {
					*_into = RE::BSScript::get<std::int32_t>(a_result);
				}
			}

		private:
			std::atomic<std::int32_t>* _into;
		};

		bool PlayId(std::uint32_t a_id, RE::TESObjectREFR* a_at, std::atomic<std::int32_t>* a_instance = nullptr)
		{
			auto* form = Sound(a_id);
			auto* game = RE::GameVM::GetSingleton();
			auto  vm = game ? game->GetVM() : nullptr;
			if (!form || !a_at || !vm) {
				return false;
			}
			auto&      handles = vm->GetObjectHandlePolicy();
			const auto handle = handles.GetHandleForObject(RE::BSScript::GetVMTypeID<RE::BGSSoundDescriptorForm>(), form);
			if (handle == handles.EmptyHandle()) {
				return false;
			}
			const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new Started(a_instance) };
			return vm->DispatchMethodCall(
				handle, RE::BSFixedString("Sound"), RE::BSFixedString("Play"),
				[&](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
					a_out = RE::BSScript::detail::PackVariables(a_at);
					return true;
				},
				callback);
		}

		void Stop(std::atomic<std::int32_t>& a_instance)
		{
			const std::int32_t id = a_instance.exchange(0);
			auto*              game = RE::GameVM::GetSingleton();
			auto               vm = game ? game->GetVM() : nullptr;
			if (id == 0 || !vm) {
				return;
			}
			const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none{ new Started(nullptr) };
			vm->DispatchStaticCall(
				RE::BSFixedString("Sound"), RE::BSFixedString("StopInstance"),
				[&](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
					a_out = RE::BSScript::detail::PackVariables(id);
					return true;
				},
				none);
		}

		std::atomic<std::int32_t> g_music{ 0 };
		std::atomic<std::int32_t> g_layer{ 0 };  // the zapper's running machinery under its music

		// the same sound not more than so often (a ball rattling about: a few clicks, not a buzz)
		bool Spaced(std::uint32_t a_id, double a_seconds)
		{
			static std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point> last;
			const auto now = std::chrono::steady_clock::now();
			auto&      t = last[a_id];
			if (std::chrono::duration<double>(now - t).count() < a_seconds) {
				return false;
			}
			t = now;
			return true;
		}
	}

	void Play(GameId a_game, Cue a_cue, RE::TESObjectREFR* a_at, double a_gain)
	{
		std::vector<std::uint32_t> ids;
		double                     spacing = 0.0;
		switch (a_cue) {
		case Cue::kStart:
			switch (a_game) {
			case GameId::kAtomicRollers: ids = { S::kSledBallStart }; break;
			case GameId::kHoopShot: ids = { S::kHoopStart }; break;
			case GameId::kWhacACommie: ids = { S::kWhacBell, S::kWhacLaugh }; break;
			case GameId::kNukaZapperRace: ids = { S::kZapStart }; break;
			case GameId::kBanditRoundup: ids = { S::kBanditBell }; break;
			default: break;
			}
			break;
		case Cue::kEnd:
			ids = { a_game == GameId::kNukaZapperRace ? S::kZapStop : a_game == GameId::kBanditRoundup ? S::kBanditWin : S::kWhacFinish };
			break;
		case Cue::kWin:
			ids = a_game == GameId::kNukaZapperRace ? std::vector<std::uint32_t>{ S::kZapWinMusic, S::kZapWinMech } : std::vector<std::uint32_t>{ S::kHoopHigh };
			break;
		case Cue::kBallHitHard:
			ids = { S::kSledBallTrackH };
			spacing = 0.08;
			break;
		case Cue::kBallHitSoft:
			if (a_gain < 0.15) {
				return;
			}
			ids = { S::kSledBallTrackL };
			spacing = 0.12;
			break;
		case Cue::kPoints10: ids = { S::kPoints[0] }; break;
		case Cue::kPoints20: ids = { S::kPoints[1] }; break;
		case Cue::kPoints30: ids = { S::kPoints[2] }; break;
		case Cue::kPoints40: ids = { S::kPoints[3] }; break;
		case Cue::kPoints50: ids = { S::kPoints[4] }; break;
		case Cue::kPoints100: ids = { S::kPoints[5] }; break;
		case Cue::kBasket: ids = { S::kHoopNet, S::kHoopBell }; break;
		case Cue::kBounce:
			if (a_gain < 0.1) {
				return;
			}
			ids = { a_gain > 0.5 ? S::kBasketballH : S::kBasketballL };
			spacing = 0.1;
			break;
		case Cue::kWhack: ids = { S::kWhacHit }; break;
		case Cue::kSquirt:
			ids = { S::kZapSquirt };
			spacing = 0.35;
			break;
		case Cue::kTargetHit: ids = { S::kBanditHit }; break;
		case Cue::kShot: ids = { S::kBanditShot }; break;
		case Cue::kTicket: ids = { S::kTicketDispense }; break;
		case Cue::kHurry: Music(a_game, a_at, true, true); return;
		default: return;
		}
		for (const auto id : ids) {
			if (spacing > 0.0 && !Spaced(id, spacing)) {
				continue;
			}
			PlayId(id, a_at);
		}
	}

	void Music(GameId a_game, RE::TESObjectREFR* a_at, bool a_on, bool a_fast)
	{
		Stop(g_music);
		Stop(g_layer);
		if (!a_on) {
			return;
		}
		std::uint32_t loop = 0, start = 0;
		switch (a_game) {
		case GameId::kAtomicRollers:
			loop = a_fast ? S::kSledBallFast : S::kSledBallMusic;
			start = a_fast ? S::kSledBallFastStart : 0;
			break;
		case GameId::kHoopShot:
			loop = a_fast ? S::kHoopFast : S::kHoopMusic;
			start = a_fast ? S::kHoopFastStart : 0;
			break;
		case GameId::kWhacACommie:
			loop = a_fast ? S::kWhacFast : S::kWhacMusic;
			start = a_fast ? S::kWhacFastStart : 0;
			break;
		case GameId::kNukaZapperRace:
			loop = S::kZapMusic;
			PlayId(S::kZapMech, a_at, &g_layer);
			break;
		case GameId::kBanditRoundup:
			loop = a_fast ? S::kBanditFast : S::kBanditMusic;
			start = a_fast ? S::kBanditFastStart : 0;
			break;
		default: return;
		}
		if (start) {
			PlayId(start, a_at);
		}
		PlayId(loop, a_at, &g_music);
	}

	void Tickets(RE::TESObjectREFR* a_at)
	{
		PlayId(S::kTicketDispense, a_at);
	}
}
