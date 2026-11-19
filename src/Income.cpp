#include "Income.h"

#include "Forms.h"
#include "Papyrus.h"

namespace ag::Income
{
	namespace
	{
		constexpr float kMostDays = 30.0f;  // a long time away pays at most this many days

		std::mutex                                g_lock;
		std::unordered_map<RE::TESFormID, float> g_paid;  // each machine: the day it was last paid up to

		float Today()
		{
			auto* cal = RE::Calendar::GetSingleton();
			return cal && cal->gameDaysPassed ? cal->gameDaysPassed->GetValue() : -1.0f;
		}

		RE::BGSKeyword* WorkshopItemKeyword()
		{
			static RE::BGSKeyword* kw = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>("WorkshopItemKeyword");
			return kw;
		}

		// the settlement's population (the workshop's rating; its editor ID looked up, the way WorkshopParent names it)
		const RE::ActorValueInfo* Population()
		{
			static const RE::ActorValueInfo* av = [] {
				for (const char* name : { "Population", "WorkshopRatingPopulation" }) {
					if (auto* f = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>(name)) {
						log::info("income: settlers counted by {} ({:08X})", name, f->GetFormID());
						return f;
					}
				}
				log::warn("income: no Population actor value found; the games won't earn");
				return static_cast<RE::ActorValueInfo*>(nullptr);
			}();
			return av;
		}
	}

	void Check(RE::TESObjectREFR* a_machine)
	{
		const float today = Today();
		if (!a_machine || today < 0.0f) {
			return;
		}
		const auto id = a_machine->GetFormID();
		float      days = 0.0f;
		{
			std::scoped_lock l(g_lock);
			auto [it, fresh] = g_paid.try_emplace(id, today);
			if (fresh) {
				return;  // (new here: earning from now)
			}
			days = std::floor(today - it->second);
			if (days < 1.0f) {
				return;
			}
			it->second += days;
		}
		auto* kw = WorkshopItemKeyword();
		auto* workshop = kw ? a_machine->GetLinkedRef(kw) : nullptr;
		const auto* pop = Population();
		auto* rate = Forms::Own(Forms::kIncomePerSettler);
		if (!workshop || !pop || !rate || !rate->As<RE::TESGlobal>()) {
			return;
		}
		const float settlers = workshop->GetActorValue(*pop);
		const float each = rate->As<RE::TESGlobal>()->GetValue();
		const int   caps = static_cast<int>(std::lround(settlers * each * std::min(days, kMostDays)));
		if (caps <= 0) {
			return;
		}
		Papyrus::Call("AddTo", { Papyrus::Id(workshop), static_cast<std::int32_t>(Forms::kCaps), caps });
		log::info("income: {:08X} earned {} caps over {} day(s) ({} settlers)", id, caps, days, settlers);
	}

	void Save(const F4SE::SerializationInterface* a_intfc)
	{
		std::scoped_lock l(g_lock);
		if (!a_intfc->OpenRecord(kSaveType, kSaveVersion)) {
			return;
		}
		const std::uint32_t n = static_cast<std::uint32_t>(g_paid.size());
		a_intfc->WriteRecordData(&n, sizeof(n));
		for (const auto& [id, day] : g_paid) {
			a_intfc->WriteRecordData(&id, sizeof(id));
			a_intfc->WriteRecordData(&day, sizeof(day));
		}
	}

	void Load(const F4SE::SerializationInterface* a_intfc, std::uint32_t, std::uint32_t)
	{
		std::scoped_lock l(g_lock);
		g_paid.clear();
		std::uint32_t n = 0;
		if (!a_intfc->ReadRecordData(&n, sizeof(n))) {
			return;
		}
		for (std::uint32_t i = 0; i < n; ++i) {
			RE::TESFormID id = 0;
			float         day = 0.0f;
			if (!a_intfc->ReadRecordData(&id, sizeof(id)) || !a_intfc->ReadRecordData(&day, sizeof(day))) {
				break;
			}
			if (const auto now = a_intfc->ResolveFormID(id)) {
				g_paid[*now] = day;
			}
		}
	}

	void Revert()
	{
		std::scoped_lock l(g_lock);
		g_paid.clear();
	}
}
