#pragma once

// AmusementGames.esl's records (tools/make_esl.py) and the vanilla ones the plugin uses.
namespace ag::Forms
{
	inline constexpr std::string_view kPlugin = "AmusementGames.esl";

	inline constexpr std::uint32_t kWorkshopMenuID = 0x800;  // KYWD AmusementGamesWorkshopMenu: the Amusement Games workshop category

	inline constexpr RE::TESFormID kFurnitureTables = 0x00118F59;  // WorkshopRecipeFilterFurniture02Tables

	// one of our records by its id in the .esl (light plugin: 0xFE, its light index, then the low 12 bits)
	inline RE::TESForm* Own(std::uint32_t a_rawID)
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		auto* file = dh ? dh->LookupModByName(kPlugin) : nullptr;
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
