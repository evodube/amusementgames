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
	inline constexpr std::uint32_t kPrizeCounterID = 0x838;  // TERM AmusementGamesPrizeCounter: tickets for prizes
	inline constexpr std::uint32_t kTicketsCarriedID = 0x845;  // GLOB AmusementGamesTicketsCarried: the tickets on you
	inline constexpr std::uint32_t kFirstBestID = 0x840;      // GLOB AmusementGamesBest*: your bests, sim::GameId order

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
		kZapperTarget = 0x82A,
		kCommieWhackerProp = 0x82B,  // the Nuka-Cade's mallet, resting on Whac-a-Commie
		kIncomePerSettler = 0x80C    // GLOB: (unused since 0.9.7: the rates are in Settings, the Income page)
	};

	inline constexpr RE::TESFormID kFurnitureTables = 0x00118F59;  // WorkshopRecipeFilterFurniture02Tables
	inline constexpr RE::TESFormID kCaps = 0x0000000F;

	// DLCNukaWorld.esm's (ids within it)
	inline constexpr std::uint32_t kToken = 0x01998A;   // MISC DLC04_Nukacade_Token01
	inline constexpr std::uint32_t kTicket = 0x019988;  // MISC DLC04_Nukacade_Ticket01
	inline constexpr std::uint32_t kCommieWhacker = 0x052926;   // WEAP DLC04_CommieWhacker: Whac-a-Commie's mallet
	inline constexpr std::uint32_t kThirstZapper = 0x007BC8;    // WEAP DLC04_ThirstZapper: the zapper race's water gun
	inline constexpr std::uint32_t kLeverGun = 0x02C8B0;        // WEAP DLC03_LeverGun (DLCCoast.esm): Bandit Roundup's lever-action rifle...
	inline constexpr RE::TESFormID kHuntingRifle = 0x0004F46A;  // WEAP HuntingRifle (Fallout4.esm): ...or this, without Far Harbor
	inline constexpr std::uint32_t kSettlerLeverGun = 0x833;    // WEAP AmusementGamesSettlerLeverGun (ours): a settler's
	inline constexpr std::uint32_t kLeverGunIronSights = 0x042E6F;      // OMOD DLC03_mod_LeverGun_Scope_SightsIron: no scope
	inline constexpr RE::TESFormID kHuntingRifleIronSights = 0x00148418;  // OMOD mod_HuntingRifle_Scope_SightsIron
	inline constexpr std::uint32_t kNumbers = 0x04E138;       // ACTI DLC04_Nukacade_Numbers: a scoreboard digit
	inline constexpr std::uint32_t kNumbersLarge = 0x053653;  // ACTI DLC04_Nukacade_Numbers_Large: the zapper's and gallery's

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

	// a workshop item's link to its settlement's workshop
	inline RE::BGSKeyword* WorkshopItemKeyword()
	{
		static RE::BGSKeyword* kw = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>("WorkshopItemKeyword");
		return kw;
	}

	inline constexpr RE::TESFormID kXMarkerHeading = 0x00000034;  // STAT XMarkerHeading

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
		case sim::GameId::kNukaZapperRace: return { kZapperRocket };
		case sim::GameId::kBanditRoundup: return { kGalleryDuck, kGalleryBunny, kGalleryPig, kGalleryBottle, kGalleryStar };
		default: return {};
		}
	}

	// the weapon you play a game with (Nuka-World's), or none
	inline RE::TESObjectWEAP* WeaponOf(sim::GameId a_game)
	{
		RE::TESForm* form = nullptr;
		switch (a_game) {
		case sim::GameId::kWhacACommie: form = Nuka(kCommieWhacker); break;
		case sim::GameId::kNukaZapperRace: form = Nuka(kThirstZapper); break;
		case sim::GameId::kBanditRoundup:
			form = From("DLCCoast.esm", kLeverGun);
			if (!form) {
				form = RE::TESForm::GetFormByID(kHuntingRifle);
			}
			break;
		default: return nullptr;
		}
		return form ? form->As<RE::TESObjectWEAP>() : nullptr;
	}

	// a settler's: as yours, but Bandit Roundup's lever-action is ours (AmusementGamesSettlerLeverGun): Far Harbor's, played
	// with the Hunting Rifle's animations (Far Harbor's own had them lower it and drop into the floor at each shot)
	inline RE::TESObjectWEAP* SettlerWeaponOf(sim::GameId a_game)
	{
		if (a_game == sim::GameId::kBanditRoundup) {
			if (auto* own = Own(kSettlerLeverGun); own && own->As<RE::TESObjectWEAP>()) {
				return own->As<RE::TESObjectWEAP>();
			}
		}
		return WeaponOf(a_game);
	}

	// the mod fitted to a lent weapon: the rifles' iron sights (no scope)
	inline RE::TESForm* LentModFor(const RE::TESObjectWEAP* a_weapon)
	{
		if (!a_weapon) {
			return nullptr;
		}
		if (a_weapon == From("DLCCoast.esm", kLeverGun) || a_weapon == Own(kSettlerLeverGun)) {
			return From("DLCCoast.esm", kLeverGunIronSights);
		}
		if (a_weapon->GetFormID() == kHuntingRifle) {
			return RE::TESForm::GetFormByID(kHuntingRifleIronSights);
		}
		return nullptr;
	}

	// The workshop menu: Furniture > Evodube Games > Amusement Games. Evodube Games is a submenu shared by evodube's mods
	// (Wasteland Wagers, Amusement Games, Mini Golf): a form list whose first entry is the keyword naming it (as
	// vanilla's own submenus are), each mod carrying its own copy with the same editor ids. Here, when the game's data
	// is loaded: in each Furniture menu (the form list, or lists, with Tables in it), the Evodube Games already there
	// (another mod put it in) is found by its keyword's editor id and our category added to it; with none there yet,
	// ours goes in, right after Tables. The menu's lists aren't overridden in the .esl (that would undo other mods'
	// changes to them).
	inline constexpr std::uint32_t    kGamesMenuListID = 0x8B5;  // FLST EvodubeGamesWorkshopMenuList
	inline constexpr std::string_view kGamesMenuEditorID = "EvodubeGamesWorkshopMenu";

	inline RE::BGSListForm* GamesSubmenuIn(RE::BGSListForm* a_menu)
	{
		for (auto* f : a_menu->arrayOfForms) {
			auto* sub = f ? f->As<RE::BGSListForm>() : nullptr;
			if (!sub || sub->arrayOfForms.empty() || !sub->arrayOfForms[0]) {
				continue;
			}
			auto* kw = sub->arrayOfForms[0]->As<RE::BGSKeyword>();
			if (kw && std::string_view(kw->formEditorID.c_str() ? kw->formEditorID.c_str() : "") == kGamesMenuEditorID) {
				return sub;
			}
		}
		return nullptr;
	}

	inline void AddWorkshopMenu()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		auto* ours = Own(kWorkshopMenuID);
		auto* gamesForm = Own(kGamesMenuListID);
		auto* games = gamesForm ? gamesForm->As<RE::BGSListForm>() : nullptr;
		auto* tables = RE::TESForm::GetFormByID(kFurnitureTables);
		if (!data || !ours || !games || !tables) {
			log::warn("workshop menu: the Amusement Games category ({}), Evodube Games ({}) or Tables ({}) isn't there", ours != nullptr,
				games != nullptr, tables != nullptr);
			return;
		}
		int added = 0;
		for (auto* list : data->GetFormArray<RE::BGSListForm>()) {
			if (!list || list == games) {
				continue;
			}
			auto& forms = list->arrayOfForms;
			if (std::ranges::find(forms, tables) == forms.end()) {
				continue;
			}
			// (an older version put its category straight in Furniture: out of there, into Evodube Games)
			if (auto old = std::ranges::find(forms, ours); old != forms.end()) {
				forms.erase(old);
			}
			auto* sub = GamesSubmenuIn(list);
			if (!sub) {
				sub = games;
				forms.insert(std::ranges::find(forms, tables) + 1, games);
				log::info("workshop menu: Evodube Games added to {:08X} (the Furniture menu, after Tables)", list->GetFormID());
			}
			if (std::ranges::find(sub->arrayOfForms, ours) == sub->arrayOfForms.end()) {
				sub->arrayOfForms.push_back(ours);
			}
			log::info("workshop menu: Amusement Games in Evodube Games ({:08X}) in {:08X}", sub->GetFormID(), list->GetFormID());
			++added;
		}
		if (added == 0) {
			log::warn("workshop menu: the Furniture menu (the list with Tables in it) wasn't found: Evodube Games isn't in the workshop menu");
		}
	}
}
