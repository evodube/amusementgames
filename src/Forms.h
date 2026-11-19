#pragma once

#include "sim/Arcade.h"

// AmusementGames.esl's records (tools/make_esl.py) and the vanilla ones the plugin uses.
namespace ag::Forms
{
	inline constexpr std::string_view kPlugin = "AmusementGames.esl";
	inline constexpr std::string_view kNukaWorld = "DLCNukaWorld.esm";

	inline constexpr std::uint32_t kWorkshopMenuID = 0x800;  // KYWD AmusementGamesWorkshopMenu: the Amusement Games workshop category
	inline constexpr std::uint32_t kFirstMachineID = 0x810;  // ACTI the five machines, in sim::GameId order (0x810-0x814)
	inline constexpr std::uint32_t kFirstPieceID = 0x820;    // STAT the pieces (0x820-0x82A)

	// the pieces, by their ids in the .esl
	enum Piece : std::uint32_t
	{
		kSkeeBall = 0x820,
		kBasketball = 0x821,
		kCommieHead = 0x822,
		kZapperRocket = 0x823,
		kZapperDrop = 0x824,
		kGalleryDuck = 0x825,
		kGalleryBunny = 0x826,
		kGalleryPig = 0x827,
		kGalleryBottle = 0x828,
		kGalleryStar = 0x829,
		kZapperTarget = 0x82A
	};

	inline constexpr RE::TESFormID kFurnitureTables = 0x00118F59;  // WorkshopRecipeFilterFurniture02Tables
	inline constexpr RE::TESFormID kCaps = 0x0000000F;

	// DLCNukaWorld.esm's (ids within it)
	inline constexpr std::uint32_t kToken = 0x01998A;   // MISC DLC04_Nukacade_Token01
	inline constexpr std::uint32_t kTicket = 0x019988;  // MISC DLC04_Nukacade_Ticket01

	// a form from a plugin by its id within that plugin (light plugins too)
	inline RE::TESForm* From(std::string_view a_plugin, std::uint32_t a_rawID)
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		auto* file = dh ? dh->LookupModByName(a_plugin) : nullptr;
		if (!file || file->compileIndex == 0xFF) {
			return nullptr;
		}
		RE::TESFormID id;
		if (file->compileIndex == 0xFE) {
			id = (0xFEu << 24) | (static_cast<std::uint32_t>(file->smallFileCompileIndex) << 12) | (a_rawID & 0xFFF);
		} else {
			id = (static_cast<std::uint32_t>(file->compileIndex) << 24) | (a_rawID & 0xFFFFFF);
		}
		return RE::TESForm::GetFormByID(id);
	}

	inline RE::TESForm* Own(std::uint32_t a_rawID) { return From(kPlugin, a_rawID); }
	inline RE::TESForm* Nuka(std::uint32_t a_rawID) { return From(kNukaWorld, a_rawID); }

	inline RE::TESBoundObject* Bound(RE::TESForm* a_form)
	{
		return a_form && a_form->IsBoundObject() ? static_cast<RE::TESBoundObject*>(a_form) : nullptr;
	}

	inline RE::TESBoundObject* Token() { return Bound(Nuka(kToken)); }
	inline RE::TESBoundObject* Ticket() { return Bound(Nuka(kTicket)); }

	// which game a machine is (one of ours), or kCount
	inline sim::GameId GameOf(const RE::TESObjectREFR* a_ref)
	{
		auto* base = a_ref ? a_ref->GetObjectReference() : nullptr;
		if (!base) {
			return sim::GameId::kCount;
		}
		for (int i = 0; i < static_cast<int>(sim::GameId::kCount); ++i) {
			if (Own(kFirstMachineID + static_cast<std::uint32_t>(i)) == base) {
				return static_cast<sim::GameId>(i);
			}
		}
		return sim::GameId::kCount;
	}

	// the pieces a game shows, by the model index its sim::Piece gives
	inline std::vector<std::uint32_t> PiecesOf(sim::GameId a_game)
	{
		switch (a_game) {
		case sim::GameId::kAtomicRollers: return { kSkeeBall };
		case sim::GameId::kHoopShot: return { kBasketball };
		case sim::GameId::kWhacACommie: return { kCommieHead };
		case sim::GameId::kNukaZapperRace: return { kZapperRocket, kZapperDrop };
		case sim::GameId::kBanditRoundup: return { kGalleryDuck, kGalleryBunny, kGalleryPig, kGalleryBottle, kGalleryStar };
		default: return {};
		}
	}

	// The workshop menu's Amusement Games category: in the Furniture menu, right after Tables, when the game's data is
	// loaded. The menu's form lists aren't overridden in the .esl (that would undo other mods' changes to them): the
	// category's added here, each time the game starts. The Furniture menu is the form list (or lists, mods' copies
	// too) with Tables in it.
	inline void AddWorkshopMenu()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		auto* ours = Own(kWorkshopMenuID);
		auto* tables = RE::TESForm::GetFormByID(kFurnitureTables);
		if (!data || !ours || !tables) {
			log::warn("workshop menu: the Amusement Games category ({}) or the Tables one ({}) isn't there", ours != nullptr, tables != nullptr);
			return;
		}
		int added = 0;
		for (auto* list : data->GetFormArray<RE::BGSListForm>()) {
			if (!list) {
				continue;
			}
			auto& forms = list->arrayOfForms;
			const auto at = std::ranges::find(forms, tables);
			if (at == forms.end() || std::ranges::find(forms, ours) != forms.end()) {
				continue;
			}
			forms.insert(at + 1, ours);
			++added;
			log::info("workshop menu: Amusement Games added to {:08X} (the Furniture menu, after Tables)", list->GetFormID());
		}
		if (added == 0) {
			log::warn("workshop menu: the Furniture menu (the list with Tables in it) wasn't found: the Amusement Games category isn't in the workshop menu");
		}
	}
}
