#pragma once

// Calls into AmusementGamesNative.psc's global functions (forms passed as their ids). a_done, if given, gets the
// function's Int result on Papyrus' thread when it's done.
namespace ag::Papyrus
{
	bool Call(const char* a_function, std::vector<std::int32_t> a_ints, std::function<void(std::int32_t)> a_done = {});
	bool CallWithBools(const char* a_function, std::int32_t a_a, bool a_b, std::int32_t a_c, std::int32_t a_d, std::int32_t a_e,
		std::function<void(std::int32_t)> a_done = {});

	inline std::int32_t Id(const RE::TESForm* a_form) { return a_form ? static_cast<std::int32_t>(a_form->GetFormID()) : 0; }
}
