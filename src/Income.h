#pragma once

namespace ag::Income
{
	// a machine of ours, near you: pays its settlement's workshop what it's earned since it last did (whole days; a
	// day's takings for each settler there: Settings' incomeCaps caps and incomeTickets Nuka-Cade tickets)
	void Check(RE::TESObjectREFR* a_machine);

	void Save(const F4SE::SerializationInterface* a_intfc);
	void Load(const F4SE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);
	void Revert();

	inline constexpr std::uint32_t kSaveType = 0x4147494E;  // 'AGIN': when each machine was last paid
	inline constexpr std::uint32_t kSaveVersion = 1;
}
