#include "Papyrus.h"

// The base class's destructor is declared in CommonLibF4 but not defined there; our functors need one to link.
RE::BSScript::IStackCallbackFunctor::~IStackCallbackFunctor() = default;

namespace ag::Papyrus
{
	namespace
	{
		constexpr auto kScript = "AmusementGamesNative";

		class Done final : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			Done(std::string a_what, std::function<void(std::int32_t)> a_then) :
				_what(std::move(a_what)),
				_then(std::move(a_then))
			{}
			void CallQueued() override {}
			void CallCanceled() override
			{
				static int told = 0;
				if (told++ < 5) {
					log::warn("papyrus {}: canceled (is AmusementGamesNative.pex installed?)", _what);
				}
			}
			void StartMultiDispatch() override {}
			void EndMultiDispatch() override {}
			void operator()(RE::BSScript::Variable a_result) override
			{
				if (_then) {
					_then(a_result.is<std::int32_t>() ? RE::BSScript::get<std::int32_t>(a_result) : 0);
				}
			}

		private:
			std::string                       _what;
			std::function<void(std::int32_t)> _then;
		};

		template <class... Args>
		bool Dispatch(const char* a_function, std::function<void(std::int32_t)> a_done, Args... a_args)
		{
			auto* game = RE::GameVM::GetSingleton();
			auto  vm = game ? game->GetVM() : nullptr;
			if (!vm) {
				return false;
			}
			const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new Done(a_function, std::move(a_done)) };
			const bool ok = vm->DispatchStaticCall(
				RE::BSFixedString(kScript), RE::BSFixedString(a_function),
				[&](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
					a_out = RE::BSScript::detail::PackVariables(a_args...);
					return true;
				},
				callback);
			if (!ok) {
				static int told = 0;
				if (told++ < 5) {
					log::warn("papyrus {}.{}: couldn't be called", kScript, a_function);
				}
			}
			return ok;
		}
	}

	bool Call(const char* a_function, std::vector<std::int32_t> a_ints, std::function<void(std::int32_t)> a_done)
	{
		switch (a_ints.size()) {
		case 1: return Dispatch(a_function, std::move(a_done), a_ints[0]);
		case 2: return Dispatch(a_function, std::move(a_done), a_ints[0], a_ints[1]);
		default:
			log::error("papyrus {}: {} arguments not handled", a_function, a_ints.size());
			return false;
		}
	}

	bool CallWithBools(const char* a_function, std::int32_t a_a, bool a_b, std::int32_t a_c, std::int32_t a_d, std::int32_t a_e,
		std::function<void(std::int32_t)> a_done)
	{
		return Dispatch(a_function, std::move(a_done), a_a, a_b, a_c, a_d, a_e);
	}
}
