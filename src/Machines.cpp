#include "Machines.h"

#include "Forms.h"

namespace ag::machines
{
	namespace
	{
		std::mutex                        g_mutex;
		std::unordered_set<RE::TESFormID> g_known;

		bool Ours(RE::TESObjectREFR* a_ref)
		{
			return a_ref && Forms::GameOf(a_ref) != sim::GameId::kCount;
		}

		RE::TESObjectREFR* RefOf(RE::TESFormID a_id)
		{
			auto* f = a_id ? RE::TESForm::GetFormByID(a_id) : nullptr;
			return f ? f->As<RE::TESObjectREFR>() : nullptr;
		}

		class Sink : public RE::BSTEventSink<RE::TESObjectLoadedEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent& a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
			{
				auto* ref = RefOf(a_event.formID);
				if (Ours(ref)) {
					std::scoped_lock lock(g_mutex);
					if (a_event.loaded) {
						g_known.insert(a_event.formID);
					} else {
						g_known.erase(a_event.formID);
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// every loaded cell's objects, once (the reseed)
		template <class F>
		void ForEachLoadedRef(F&& a_fn)
		{
			auto* tes = RE::TES::GetSingleton();
			auto  visit = [&](RE::TESObjectCELL* a_cell) {
				if (!a_cell) {
					return;
				}
				auto* player = RE::PlayerCharacter::GetSingleton();
				a_cell->ForEachReferenceInRange(player->data.location, 1.0e9f, [&](RE::TESObjectREFR* a_ref) {
					a_fn(a_ref);
					return RE::BSContainer::ForEachResult::kContinue;
				});
			};
			if (tes && tes->interiorCell) {
				visit(tes->interiorCell);
			} else if (tes && tes->gridCells && tes->gridCells->gridCell) {
				const auto dim = std::min<std::uint32_t>(tes->gridCells->dimension, 15);
				for (std::uint32_t i = 0; i < dim * dim; ++i) {
					auto* cell = tes->gridCells->gridCell[i].cell;
					if (cell && cell->IsAttached()) {
						visit(cell);
					}
				}
			} else if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				visit(player->GetParentCell());
			}
		}
	}

	void Listen()
	{
		static Sink sink;
		static bool done = false;
		if (done) {
			return;
		}
		done = true;
		if (auto* source = RE::TESObjectLoadedEvent::GetEventSource()) {
			source->RegisterSink(&sink);
			log::info("machines: watching models load (no scanning for machines)");
		} else {
			log::warn("machines: no object-loaded event source");
		}
	}

	void Reseed()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->GetParentCell()) {
			return;
		}
		std::vector<RE::TESFormID> found;
		ForEachLoadedRef([&](RE::TESObjectREFR* a_ref) {
			if (Ours(a_ref) && a_ref->Get3D()) {
				found.push_back(a_ref->GetFormID());
			}
		});
		std::scoped_lock lock(g_mutex);
		g_known.clear();
		for (auto id : found) {
			g_known.insert(id);
		}
		log::info("machines: {} of ours loaded", g_known.size());
	}

	bool Empty()
	{
		std::scoped_lock lock(g_mutex);
		return g_known.empty();
	}

	std::vector<RE::TESObjectREFR*> Near(const RE::NiPoint3& a_pos, float a_range)
	{
		std::vector<RE::TESFormID> ids;
		{
			std::scoped_lock lock(g_mutex);
			ids.assign(g_known.begin(), g_known.end());
		}
		std::vector<RE::TESObjectREFR*> out;
		std::vector<RE::TESFormID>      gone;
		for (auto id : ids) {
			auto* ref = RefOf(id);
			if (!ref || ref->IsDeleted()) {
				gone.push_back(id);
				continue;
			}
			if ((ref->data.location - a_pos).Length() <= a_range) {
				out.push_back(ref);
			}
		}
		if (!gone.empty()) {
			std::scoped_lock lock(g_mutex);
			for (auto id : gone) {
				g_known.erase(id);
			}
		}
		return out;
	}
}
