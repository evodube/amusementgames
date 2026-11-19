#!/usr/bin/env python3
"""Writes AmusementGames.esl: the Nuka-Cade's arcade machines, buildable in settlements under
Furniture > Amusement Games.

    python tools/make_esl.py [out.esl]

Masters: Fallout4.esm, DLCNukaWorld.esm. No meshes of our own: every machine is Nuka-World's own (its model in
DLCNukaWorld - Main.ba2). For now the recipes build the Nuka-Cade's own scripted machines (playable as in the
Nuka-Cade, with Nuka-Cade tokens); as each game is revamped its recipe moves to a form of ours.

Form IDs (src/Forms.h expects these):
    xx000800  KYWD AmusementGamesWorkshopMenu  the workshop menu's Amusement Games category: the plugin puts it in the
                                               Furniture menu (after Tables) when the game's data is loaded
    xx000801  ARTO AmusementGamesWorkshopMenuArt  the category's picture: Whac-a-Commie, framed as its own preview
    xx000802  COBJ workshop_co_AmusementGamesWhacACommie    -> DLCNukaWorld 0004BD36 DLC04_Nukacade_WhackaCommie01Red
    xx000803  COBJ workshop_co_AmusementGamesNukaZapperRace -> DLCNukaWorld 0004CBDC DLC04_Nukacade_NukaZapperRace
    xx000804  COBJ workshop_co_AmusementGamesAtomicRollers  -> DLCNukaWorld 0004BD34 DLC04_Nukacade_AtomicRollers01Blue
    xx000805  COBJ workshop_co_AmusementGamesHoopShot       -> DLCNukaWorld 0004BD31 DLC04_Nukacade_HoopShot
    xx000806  COBJ workshop_co_AmusementGamesBanditRoundup  -> DLCNukaWorld 0004BD30 DLC04_Nukacade_BanditRoundup
    xx000807  COBJ workshop_co_AmusementGamesTokenDispenser -> DLCNukaWorld 0004BD38 DLC04_Nukacade_TokenDispenser01
                                               (pre-war money in, Nuka-Cade tokens out: what the machines take)

The recipes copy Nuka-World's own (DLC04workshop_co_Nukacade*, Decorations > Nuka-World > Nuka-Cade): same
components and sounds, but no condition (Nuka-World's need DLC04MQ01 done), so they're there from the start.
Record layouts follow Wasteland Wagers' tools/make_esl.py (laid out as Fallout4.esm's own records).
"""

import struct
import sys

RECORD_VERSION = 131  # Fallout 4
FLAG_MASTER = 0x1
FLAG_LIGHT = 0x200
FIRST_ID = 0x800

NUKA = 0x01000000  # DLCNukaWorld.esm, our second master
OWN = 0x02000000   # our records come after the two masters

# components (Fallout4.esm)
WOOD, STEEL, SCREWS, CIRCUITRY, COPPER, GEARS = 0x0001FAC2, 0x0001FABD, 0x0003D294, 0x0001FA9B, 0x0001FA9C, 0x0001FAB0
WORKBENCH_FURNITURE = 0x0005B5E3  # WorkshopWorkbenchTypeFurniture (the settlement workbench)
ARCADE_SOUNDS = (NUKA | 0x05710E, NUKA | 0x05710F)  # DLC04UIWorkshopModeItemPickUp/PutDownArcadeMachinex
METAL_SOUNDS = (0x000196B0, 0x000196BA)             # the token dispenser's, as Nuka-World's recipe has them


def subrecord(sig: bytes, data: bytes) -> bytes:
    if len(data) > 0xFFFF:
        raise ValueError(f"{sig!r} too long")
    return sig + struct.pack("<H", len(data)) + data


def zstring(text: str) -> bytes:
    return text.encode("cp1252") + b"\0"


def record(sig: bytes, flags: int, form_id: int, body: bytes) -> bytes:
    # type, data size, flags, form id, version control info, form version, unknown
    return sig + struct.pack("<IIIIHH", len(body), flags, form_id, 0, RECORD_VERSION, 0) + body


def group(label: bytes, contents: bytes) -> bytes:
    # type, group size (header included), label, group type 0 (top), timestamp, vc info, unknown
    return b"GRUP" + struct.pack("<I", 24 + len(contents)) + label + struct.pack("<iHHI", 0, 0, 0, 0) + contents


def menu_keyword(form_id: int, editor_id: str, name: str) -> bytes:
    """a workshop menu category (a recipe filter, as Fallout4.esm's WorkshopRecipeFilterFurniture02Tables): its name
    is what the menu shows. Not put in the menu's form list here (that would undo other mods' changes to the list):
    the plugin adds it when the game's data is loaded (Forms.h AddWorkshopMenu)"""
    return record(
        b"KYWD", 0, form_id,
        subrecord(b"EDID", zstring(editor_id)) + subrecord(b"CNAM", bytes.fromhex("ffffff00"))
        + subrecord(b"TNAM", struct.pack("<I", 9))  # type 9: recipe filter
        + subrecord(b"FULL", zstring(name)),
    )


def menu_art(form_id: int, editor_id: str, keyword_id: int, mesh: str, preview: int, bounds: tuple) -> bytes:
    """the picture on a workshop menu tab: an art object with the tab's keyword (the menu finds a tab's art by it)"""
    return record(
        b"ARTO", 0, form_id,
        subrecord(b"EDID", zstring(editor_id))
        + subrecord(b"OBND", struct.pack("<6h", *bounds))
        + subrecord(b"PTRN", struct.pack("<I", preview))
        + subrecord(b"KSIZ", struct.pack("<I", 1))
        + subrecord(b"KWDA", struct.pack("<I", keyword_id))
        + subrecord(b"MODL", zstring(mesh))
        + subrecord(b"DNAM", struct.pack("<I", 0)),
    )


def workshop_recipe(form_id: int, editor_id: str, created: int, components: list[tuple[int, int]], category: int,
                    sounds: tuple[int, int] = ARCADE_SOUNDS) -> bytes:
    """A workshop menu entry, laid out as DLCNukaWorld.esm's DLC04workshop_co_NukacadeHoopShot (its conditions left
    out): pick-up and put-down sounds, components, no description, the object, the settlement workbench, the
    category, makes one."""
    return record(
        b"COBJ", 0, form_id,
        subrecord(b"EDID", zstring(editor_id))
        + subrecord(b"YNAM", struct.pack("<I", sounds[0]))
        + subrecord(b"ZNAM", struct.pack("<I", sounds[1]))
        + subrecord(b"FVPA", b"".join(struct.pack("<II", c, n) for c, n in components))
        + subrecord(b"DESC", zstring(""))
        + subrecord(b"CNAM", struct.pack("<I", created))
        + subrecord(b"BNAM", struct.pack("<I", WORKBENCH_FURNITURE))
        + subrecord(b"FNAM", struct.pack("<I", category))
        + subrecord(b"INTV", struct.pack("<HH", 1, 10)),  # makes one; priority as Nuka-World's
    )


# (form id offset, editor id suffix, Nuka-World's machine, components as Nuka-World's recipe, sounds)
MACHINES = [
    (0x02, "WhacACommie", NUKA | 0x04BD36, [(WOOD, 1), (STEEL, 5), (SCREWS, 1), (CIRCUITRY, 1)], ARCADE_SOUNDS),
    (0x03, "NukaZapperRace", NUKA | 0x04CBDC, [(WOOD, 3), (STEEL, 8), (SCREWS, 2), (CIRCUITRY, 2)], ARCADE_SOUNDS),
    (0x04, "AtomicRollers", NUKA | 0x04BD34, [(SCREWS, 2), (STEEL, 6), (CIRCUITRY, 2), (COPPER, 1)], ARCADE_SOUNDS),
    (0x05, "HoopShot", NUKA | 0x04BD31, [(WOOD, 3), (STEEL, 8), (SCREWS, 2), (CIRCUITRY, 2)], ARCADE_SOUNDS),
    (0x06, "BanditRoundup", NUKA | 0x04BD30, [(SCREWS, 2), (STEEL, 8), (CIRCUITRY, 2), (WOOD, 3)], ARCADE_SOUNDS),
    (0x07, "TokenDispenser", NUKA | 0x04BD38, [(STEEL, 4), (SCREWS, 1), (GEARS, 1)], METAL_SOUNDS),
]


def build() -> bytes:
    menu = OWN | FIRST_ID
    keywords = [menu_keyword(menu, "AmusementGamesWorkshopMenu", "Amusement Games")]
    # the category's picture: its first item, Whac-a-Commie, framed as its own workshop preview
    # (DLC04workshop_NukaCade_WhackACommie01) with its bounds
    art = [menu_art(OWN | (FIRST_ID + 1), "AmusementGamesWorkshopMenuArt", menu,
                    "DLC04\\SetDressing\\Games\\DLC04_Nukacade_WhackaCommie01.nif", NUKA | 0x055807,
                    (-48, -40, 0, 48, 55, 179))]
    recipes = [workshop_recipe(OWN | (FIRST_ID + off), f"workshop_co_AmusementGames{name}", made, parts, menu, sounds)
               for off, name, made, parts, sounds in MACHINES]

    count = len(keywords) + len(art) + len(recipes)
    header = record(
        b"TES4",
        FLAG_MASTER | FLAG_LIGHT,
        0,
        subrecord(b"HEDR", struct.pack("<fiI", 1.0, count, FIRST_ID + 8))
        + subrecord(b"CNAM", zstring("evodube"))
        + subrecord(b"SNAM", zstring("Amusement Games: the Nuka-Cade's arcade machines in your settlements"))
        + subrecord(b"MAST", zstring("Fallout4.esm"))
        + subrecord(b"DATA", struct.pack("<Q", 0))
        + subrecord(b"MAST", zstring("DLCNukaWorld.esm"))
        + subrecord(b"DATA", struct.pack("<Q", 0)),
    )
    # groups in the order the game's own master has them
    return header + group(b"KYWD", b"".join(keywords)) + group(b"ARTO", b"".join(art)) + group(b"COBJ", b"".join(recipes))


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out = args[0] if args else "AmusementGames.esl"
    data = build()
    with open(out, "wb") as f:
        f.write(data)
    print(f"wrote {out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
