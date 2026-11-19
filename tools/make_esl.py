#!/usr/bin/env python3
"""Writes AmusementGames.esl: the Nuka-Cade's arcade machines, buildable in settlements under
Furniture > Amusement Games.

    python tools/make_esl.py [out.esl]

Masters: Fallout4.esm, DLCNukaWorld.esm. No meshes of our own: every machine is Nuka-World's own (its model in
DLCNukaWorld - Main.ba2). The machines are activators of ours with Nuka-World's
models; the plugin runs the games (src/Arcade.cpp).

Form IDs (src/Forms.h expects these):
    xx000800  KYWD AmusementGamesWorkshopMenu  the workshop menu's Amusement Games category: the plugin puts it in the
                                               Furniture menu (after Tables) when the game's data is loaded
    xx000801  ARTO AmusementGamesWorkshopMenuArt  the category's picture: Whac-a-Commie, framed as its own preview
    xx000802  COBJ workshop_co_AmusementGamesWhacACommie    -> xx000812
    xx000803  COBJ workshop_co_AmusementGamesNukaZapperRace -> xx000813
    xx000804  COBJ workshop_co_AmusementGamesAtomicRollers  -> xx000810
    xx000805  COBJ workshop_co_AmusementGamesHoopShot       -> xx000811
    xx000806  COBJ workshop_co_AmusementGamesBanditRoundup  -> xx000814
    xx000807  COBJ workshop_co_AmusementGamesTokenStand     -> xx000808, the Token Stand
    xx000808  ACTI AmusementGamesTokenStand    Token Stand: caps in, Nuka-Cade tokens out (DLCNukaWorld 0001998A, what
                                               the machines take). Nuka-World's token dispenser model; its script
                                               AmusementGamesTokenStand (papyrus/) asks how many
    xx000809  MESG AmusementGamesTokenStandMessage  the stand's message box: the price, your caps and tokens; buy 1, 5,
                                               10 or 25, or cancel
    xx00080A  MESG AmusementGamesNotEnoughCapsMessage  the corner message when you can't pay
    xx00080B  GLOB AmusementGamesTokenPrice    caps a token (5)

    xx000810  ACTI AmusementGamesAtomicRollers  the five machines: Nuka-World's own models (DLCNukaWorld's
    xx000811  ACTI AmusementGamesHoopShot       DLC04_Nukacade_* activators', bounds and workshop previews), no script:
    xx000812  ACTI AmusementGamesWhacACommie    activating one is what the plugin listens for (it takes a token and
    xx000813  ACTI AmusementGamesNukaZapperRace runs the game)
    xx000814  ACTI AmusementGamesBanditRoundup
    xx000820+ STAT AmusementGamesPiece*         what the games move about, each a Nuka-World model: 820 the skee-ball,
                                               821 the basketball, 822 a commie's head, 823 a zapper rocket, 824 a
                                               drop of the zapper's water, 825-829 the gallery's duck, bunny, pig,
                                               bottle and star, 82A the zapper's target

Tokens and tickets are Nuka-World's own items (DLC04_Nukacade_Token01 0001998A, DLC04_Nukacade_Ticket01 00019988),
so the Nuka-Cade's machines and prize terminal take ours and ours take theirs.

The game machines' recipes copy Nuka-World's own (DLC04workshop_co_Nukacade*, Decorations > Nuka-World > Nuka-Cade): same
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
CAPS = 0x0000000F
TOKEN = NUKA | 0x01998A                              # DLC04_Nukacade_Token01
TOKEN_SOUND = NUKA | 0x04E5E4                        # DLC04OBJArcadeTokenExchangeDispense
TOKEN_PRICE = 5.0                                    # caps a token


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


def wstring(text: str) -> bytes:
    data = text.encode("cp1252")
    return struct.pack("<H", len(data)) + data


def object_property(name: str, form_id: int) -> bytes:
    # type 1 (object), flags 1 (edited), object v2: unused, alias id (-1 none), form id
    return wstring(name) + bytes([1, 1]) + struct.pack("<HhI", 0, -1, form_id)


def object_vmad(script: str, properties: list[tuple[str, int]]) -> bytes:
    """One script on a base object, laid out as DLCNukaWorld.esm's token dispenser: version 6, object format 2, one
    script (name, flags 0, its properties)."""
    props = [object_property(n, f) for n, f in properties]
    return struct.pack("<HHH", 6, 2, 1) + wstring(script) + b"\x00" + struct.pack("<H", len(props)) + b"".join(props)


def token_stand(form_id: int, properties: list[tuple[str, int]]) -> bytes:
    """The Token Stand: Nuka-World's token dispenser (DLC04_Nukacade_TokenDispenser01: its model, bounds and workshop
    preview) with our script instead of its pre-war money one."""
    return record(
        b"ACTI", 0, form_id,
        subrecord(b"EDID", zstring("AmusementGamesTokenStand"))
        + subrecord(b"VMAD", object_vmad("AmusementGamesTokenStand", properties))
        + subrecord(b"OBND", struct.pack("<6h", -21, -32, 0, 21, 19, 135))
        + subrecord(b"PTRN", struct.pack("<I", NUKA | 0x055806))  # DLC04workshop_NukaCade_TokenDispenser01
        + subrecord(b"FULL", zstring("Token Stand"))
        + subrecord(b"MODL", zstring("DLC04\\SetDressing\\Games\\DLC04_Nukacade_TokenDispenser01.nif"))
        + subrecord(b"ATTX", zstring("Buy Tokens"))
        + subrecord(b"FNAM", struct.pack("<H", 0)),
    )


def machine(form_id: int, editor_id: str, name: str, mesh: str, bounds: tuple, preview: int) -> bytes:
    """An arcade machine: an activator with no script (activating it is what the plugin listens for), Nuka-World's
    model, bounds and workshop preview."""
    return record(
        b"ACTI", 0, form_id,
        subrecord(b"EDID", zstring(editor_id))
        + subrecord(b"OBND", struct.pack("<6h", *bounds))
        + subrecord(b"PTRN", struct.pack("<I", preview))
        + subrecord(b"FULL", zstring(name))
        + subrecord(b"MODL", zstring(mesh))
        + subrecord(b"ATTX", zstring("Play"))
        + subrecord(b"FNAM", struct.pack("<H", 0)),
    )


def stat(form_id: int, editor_id: str, mesh: str, bounds: tuple) -> bytes:
    """A piece the plugin places and moves (laid out as Fallout4.esm's statics)."""
    return record(
        b"STAT", 0, form_id,
        subrecord(b"EDID", zstring(editor_id))
        + subrecord(b"OBND", struct.pack("<6h", *bounds))
        + subrecord(b"MODL", zstring(mesh))
        # max angle, material (none), leaf amplitude, leaf frequency
        + subrecord(b"DNAM", struct.pack("<fIff", 90.0, 0, 1.0, 1.0)),
    )


def message(form_id: int, editor_id: str, text: str, title: str = "", buttons: list[str] | None = None) -> bytes:
    """A message, laid out as DLCNukaWorld.esm's: with buttons a message box (DNAM 1, as
    DLC04WorkshopRedRocketPickGangMessage), without a corner message shown 2 seconds (as
    DLC04_Nukacade_TokenNeededMessage). Its %.0f are the numbers the script passes to Show."""
    body = subrecord(b"EDID", zstring(editor_id)) + subrecord(b"DESC", zstring(text))
    if title:
        body += subrecord(b"FULL", zstring(title))
    body += subrecord(b"INAM", struct.pack("<I", 0))
    if buttons:
        body += subrecord(b"DNAM", struct.pack("<I", 1)) + b"".join(subrecord(b"ITXT", zstring(b)) for b in buttons)
    else:
        body += subrecord(b"DNAM", struct.pack("<I", 0)) + subrecord(b"TNAM", struct.pack("<I", 2))
    return record(b"MESG", 0, form_id, body)


def global_float(form_id: int, editor_id: str, value: float) -> bytes:
    return record(b"GLOB", 0, form_id,
                  subrecord(b"EDID", zstring(editor_id)) + subrecord(b"FNAM", b"f") + subrecord(b"FLTV", struct.pack("<f", value)))


GAMES_DIR = "DLC04\\SetDressing\\Games\\"

# the machines: (form id offset, editor id suffix, name, Nuka-World's model, its bounds, its workshop preview TRNS)
GAMES = [
    (0x10, "AtomicRollers", "Atomic Rollers", "DLC04_Nukacade_SkeeBall01.nif", (-66, -162, -1, 67, 185, 176), NUKA | 0x055802),
    (0x11, "HoopShot", "Hoop Shot", "DLC04_Nukacade_BasketBall01.nif", (-78, -127, 0, 76, 186, 235), NUKA | 0x055804),
    (0x12, "WhacACommie", "Whac-a-Commie", "DLC04_Nukacade_WhackaCommie01.nif", (-48, -40, 0, 48, 55, 179), NUKA | 0x055807),
    (0x13, "NukaZapperRace", "Nuka-Zapper Race", "DLC04_Nukacade_NukaZapperRace.nif", (-260, -47, -28, 258, 220, 295), NUKA | 0x055805),
    (0x14, "BanditRoundup", "Bandit Roundup", "DLC04_Nukacade_ShootingGallery01.nif", (-257, -47, -64, 262, 476, 294), NUKA | 0x055803),
]

# what the games move about: (form id offset, editor id suffix, Nuka-World's model, bounds)
PIECES = [
    (0x20, "SkeeBall", GAMES_DIR + "DLC04_Nukacade_Ball01.nif", (-4, -4, -4, 4, 4, 4)),
    (0x21, "Basketball", "DLC04\\Props\\DLC04_Basketball01.nif", (-12, -12, -12, 12, 12, 12)),
    (0x22, "CommieHead", GAMES_DIR + "DLC04_Nukacade_Commie01.nif", (-5, -4, -18, 5, 5, 2)),  # the still one: origin at its top
    (0x23, "ZapperRocket", GAMES_DIR + "DLC04NukacadeNukaZapperRocket01\\DLC04NukacadeNukaZapperRocket01.nif", (-9, -9, -23, 9, 9, 22)),
    (0x24, "ZapperDrop", "DLC04\\Effects\\DLC04ProjectileThirstZapperWater.nif", (-2, -2, -2, 2, 2, 2)),
    (0x25, "GalleryDuck", GAMES_DIR + "DLC04_Nukacade_ShootingGalleryDuck01.nif", (-15, 0, -4, 15, 0, 32)),
    (0x26, "GalleryBunny", GAMES_DIR + "DLC04_Nukacade_ShootingGalleryBunny01.nif", (-18, 0, -5, 18, 0, 36)),
    (0x27, "GalleryPig", GAMES_DIR + "DLC04_Nukacade_ShootingGalleryPig01.nif", (-15, 0, -5, 15, 0, 26)),
    (0x28, "GalleryBottle", GAMES_DIR + "DLC04_Nukacade_ShootingGallerybottle01.nif", (-9, 0, -6, 9, 1, 48)),
    (0x29, "GalleryStar", GAMES_DIR + "DLC04_Nukacade_ShootingGalleryStar01.nif", (-13, 0, -6, 13, 0, 28)),
    (0x2A, "ZapperTarget", GAMES_DIR + "DLC04_Nukacade_NukaZapperTarget01.nif", (-15, -8, -21, 15, 9, 21)),
]

# (recipe form id offset, editor id suffix, what it makes, components as Nuka-World's recipe, sounds)
MACHINES = [
    (0x02, "WhacACommie", OWN | (FIRST_ID + 0x12), [(WOOD, 1), (STEEL, 5), (SCREWS, 1), (CIRCUITRY, 1)], ARCADE_SOUNDS),
    (0x03, "NukaZapperRace", OWN | (FIRST_ID + 0x13), [(WOOD, 3), (STEEL, 8), (SCREWS, 2), (CIRCUITRY, 2)], ARCADE_SOUNDS),
    (0x04, "AtomicRollers", OWN | (FIRST_ID + 0x10), [(SCREWS, 2), (STEEL, 6), (CIRCUITRY, 2), (COPPER, 1)], ARCADE_SOUNDS),
    (0x05, "HoopShot", OWN | (FIRST_ID + 0x11), [(WOOD, 3), (STEEL, 8), (SCREWS, 2), (CIRCUITRY, 2)], ARCADE_SOUNDS),
    (0x06, "BanditRoundup", OWN | (FIRST_ID + 0x14), [(SCREWS, 2), (STEEL, 8), (CIRCUITRY, 2), (WOOD, 3)], ARCADE_SOUNDS),
    (0x07, "TokenStand", OWN | (FIRST_ID + 8), [(STEEL, 4), (SCREWS, 1), (GEARS, 1)], METAL_SOUNDS),
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

    # ---- the Token Stand: caps in, tokens out
    box = OWN | (FIRST_ID + 9)
    broke = OWN | (FIRST_ID + 0xA)
    price = OWN | (FIRST_ID + 0xB)
    acti = [token_stand(OWN | (FIRST_ID + 8), [("Caps", CAPS), ("Token", TOKEN), ("TokenPrice", price),
                                                ("BuyMessage", box), ("NotEnoughCapsMessage", broke),
                                                ("DispenseSound", TOKEN_SOUND)])]
    messages = [message(box, "AmusementGamesTokenStandMessage",
                        "Nuka-Cade tokens, %.0f caps each.\nYou have %.0f caps and %.0f tokens.",
                        "Token Stand", ["1 Token", "5 Tokens", "10 Tokens", "25 Tokens", "Cancel"]),
                message(broke, "AmusementGamesNotEnoughCapsMessage", "Not enough caps: %.0f needed, you have %.0f.")]
    globs = [global_float(price, "AmusementGamesTokenPrice", TOKEN_PRICE)]

    # ---- the machines and their pieces
    for off, name, title, mesh, bounds, preview in GAMES:
        acti.append(machine(OWN | (FIRST_ID + off), f"AmusementGames{name}", title, GAMES_DIR + mesh, bounds, preview))
    stats = [stat(OWN | (FIRST_ID + off), f"AmusementGamesPiece{name}", mesh, bounds) for off, name, mesh, bounds in PIECES]

    count = len(keywords) + len(art) + len(recipes) + len(acti) + len(messages) + len(globs) + len(stats)
    header = record(
        b"TES4",
        FLAG_MASTER | FLAG_LIGHT,
        0,
        subrecord(b"HEDR", struct.pack("<fiI", 1.0, count, FIRST_ID + 0x2B))
        + subrecord(b"CNAM", zstring("evodube"))
        + subrecord(b"SNAM", zstring("Amusement Games: the Nuka-Cade's arcade machines in your settlements"))
        + subrecord(b"MAST", zstring("Fallout4.esm"))
        + subrecord(b"DATA", struct.pack("<Q", 0))
        + subrecord(b"MAST", zstring("DLCNukaWorld.esm"))
        + subrecord(b"DATA", struct.pack("<Q", 0)),
    )
    # groups in the order the game's own master has them
    return (header + group(b"GLOB", b"".join(globs)) + group(b"KYWD", b"".join(keywords))
            + group(b"ACTI", b"".join(acti)) + group(b"STAT", b"".join(stats)) + group(b"MESG", b"".join(messages)) + group(b"ARTO", b"".join(art))
            + group(b"COBJ", b"".join(recipes)))


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
