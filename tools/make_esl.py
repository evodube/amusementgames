#!/usr/bin/env python3
"""Writes AmusementGames.esl: the Nuka-Cade's arcade machines, buildable in settlements under
Furniture > Amusement Games, and a Prize Counter terminal that trades the tickets for Nuka-World prizes.

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
                                               AmusementGamesTokenStand (papyrus/) sells 5 at a press of F
    xx000809  (unused: was the stand's how-many menu)
    xx00080A  MESG AmusementGamesNotEnoughCapsMessage  the corner message when you can't pay
    xx00080B  GLOB AmusementGamesTokenPrice    caps a token (5)
    xx00080D  KYWD AmusementGamesLinkSpot      a settler's linked ref: the spot at a machine they're to walk to
    xx00080E  PACK AmusementGamesWalkToSpot    Travel to that linked ref, walking, facing the marker at the end (as
                                               Wasteland Wagers' WastelandWagersWalkToSpot)
    xx00080F  QUST AmusementGamesQuest         priority 101, start game enabled; collection alias 0 Players carrying the
                                               package (alias packages go by quest priority: it outranks settler
                                               sandboxes); collection alias 1 Shooters carrying
                                               AmusementGamesHoldAtSpot (xx00082C: the spot held once
                                               there, aiming at the targets without firing (UseWeapon, as
                                               MinRecruit00AimWeapon), no interruptions; xx00082D KYWD
                                               AmusementGamesLinkTarget, what they aim at); scripts AmusementGamesQuest,
                                               AmusementGamesGroupAlias
    xx00080C  GLOB AmusementGamesIncomePerSettler  unused since 0.9.7 (was the caps rate; the plugin's settings have it now)

    xx000810  ACTI AmusementGamesAtomicRollers  the five machines: Nuka-World's own models (DLCNukaWorld's
    xx000811  ACTI AmusementGamesHoopShot       DLC04_Nukacade_* activators', bounds and workshop previews), no script:
    xx000812  ACTI AmusementGamesWhacACommie    activating one is what the plugin listens for (it takes a token and
    xx000813  ACTI AmusementGamesNukaZapperRace runs the game)
    xx000814  ACTI AmusementGamesBanditRoundup
    xx000820+ STAT AmusementGamesPiece*         what the games move about, each a Nuka-World model: 820 the skee-ball,
                                               (822, the commie's head, an ACTI: hit by your Commie Whacker, its hit
                                               event is the whack)
                                               821 the basketball (the base game's, BasketBall01), 822 a commie's head, 823 a zapper rocket, 824 a
                                               drop of the zapper's water, 825-829 the gallery's duck, bunny, pig,
                                               bottle and star, 82A the zapper's target, 82B the Nuka-Cade's mallet
                                               resting on Whac-a-Commie (its whacker01 node)

    xx000838  TERM AmusementGamesPrizeCounter  the Prize Counter: Nuka-World's own prize terminal (its console), a
                                               menu of its sections; xx000839-83C TERM its sections (Weapons,
                                               Ammo, Drinks, Souvenirs), each prize shown buyable when you've its
                                               tickets (GetItemCount on the player) and "not enough" when not; buying
                                               runs a fragment of AmusementGamesPrizeCounterFragments (papyrus/,
                                               written by --psc from PRIZES), which calls AmusementGamesNative.BuyPrize
    xx00083D  COBJ workshop_co_AmusementGamesPrizeCounter -> xx000838
    xx00083E  GLOB AmusementGamesTicketsCurrent   tickets redeemed into the counter, unspent (one account, every
                                               counter: as the Nuka-Cade's DLC04NukacadeTicketsRedeemedCurrent)
    xx00083F  GLOB AmusementGamesTicketsRedeemed  tickets redeemed, all told
    xx000845  GLOB AmusementGamesTicketsCarried   the tickets on you (the plugin sets it as you use a counter)
    xx000846  TERM AmusementGamesPrizeCounterRedeem  Redeem Tickets: all you carry into the counter
    xx000847  TERM AmusementGamesPrizeCounterSpend   Spend Tickets: the sections
    xx000840+ GLOB AmusementGamesBest*         your best score at each game (840-844, sim::GameId order; the plugin)
              (AmusementGamesQuest's ref aliases 4 Player, 5 PrizeCounter: the counter's welcome, <Alias=Player>)

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

COAST = 0x01000000  # DLCCoast.esm (Far Harbor), our second master: Bandit Roundup's lever-action rifle
NUKA = 0x02000000   # DLCNukaWorld.esm, our third master
OWN = 0x03000000    # our records come after the three masters

# components (Fallout4.esm)
WOOD, STEEL, SCREWS, CIRCUITRY, COPPER, GEARS = 0x0001FAC2, 0x0001FABD, 0x0003D294, 0x0001FA9B, 0x0001FA9C, 0x0001FAB0
WORKBENCH_FURNITURE = 0x0005B5E3  # WorkshopWorkbenchTypeFurniture (the settlement workbench)
ARCADE_SOUNDS = (NUKA | 0x05710E, NUKA | 0x05710F)  # DLC04UIWorkshopModeItemPickUp/PutDownArcadeMachinex
METAL_SOUNDS = (0x000196B0, 0x000196BA)             # the token dispenser's, as Nuka-World's recipe has them
CAPS = 0x0000000F
LEVER_GUN = COAST | 0x02C8B0                         # DLC03_LeverGun: Bandit Roundup's rifle
ANIMS_LEVER_ACTION = COAST | 0x02F834                # its animations' keyword (Far Harbor's, for the lever)
ANIMS_HUNTING_RIFLE = 0x00058E37                     # the Hunting Rifle's
SETTLER_LEVER_GUN = OWN | (FIRST_ID + 0x33)          # ours: the lever-action, the Hunting Rifle's animations
TOKEN = NUKA | 0x01998A                              # DLC04_Nukacade_Token01
TOKEN_SOUND = NUKA | 0x04E5E4                        # DLC04OBJArcadeTokenExchangeDispense
TOKEN_PRICE = 5.0                                    # caps a token
INCOME_PER_SETTLER = 1.0                             # caps a machine earns a day, a settler

# ---- the Prize Counter: a terminal, tickets in, Nuka-World's prizes out
TICKET = NUKA | 0x019988                             # DLC04_Nukacade_Ticket01 (BuyPrize / RedeemTickets take it by id)
PRIZE_COUNTER = OWN | (FIRST_ID + 0x38)              # TERM: the top page (its sections)
PRIZE_PAGE_FIRST = FIRST_ID + 0x39                   # TERM: a section each (0x839-0x83C)
PRIZE_RECIPE = FIRST_ID + 0x3D                       # COBJ: built at the workbench
PRIZE_FRAGMENTS = "AmusementGamesPrizeCounterFragments"  # papyrus/: one function a prize (written by --psc)
RUBBER, ALUMINUM = 0x0001FAB9, 0x0001FA91
TERMINAL_COMPONENTS = [(RUBBER, 2), (COPPER, 2), (CIRCUITRY, 2), (ALUMINUM, 4)]  # as workshop_co_Terminal
NOT_ENOUGH = 100                                     # a prize's "not enough tickets" line: its menu id + 100
TICKETS_CURRENT = FIRST_ID + 0x3E                    # GLOB: the tickets you've redeemed into the counter, unspent
TICKETS_REDEEMED = FIRST_ID + 0x3F                   # GLOB: tickets you've redeemed into it, all told
TICKETS_CARRIED = FIRST_ID + 0x45                    # GLOB: the tickets on you (the plugin sets it as you use it)
REDEEM_PAGE = FIRST_ID + 0x46                        # TERM: Redeem Tickets (all you carry, into the counter)
SPEND_PAGE = FIRST_ID + 0x47                         # TERM: Spend Tickets (the sections)
BEST_FIRST = FIRST_ID + 0x40                         # GLOB: your best score at each game, sim::GameId order (0x840-0x844)
BEST_GAMES = ["AtomicRollers", "HoopShot", "WhacACommie", "NukaZapperRace", "BanditRoundup"]
BEST_NAMES = ["Atomic Rollers", "Hoop Shot", "Whac-a-Commie", "Nuka-Zapper Race", "Bandit Roundup"]
PLAYER_ALIAS, COUNTER_ALIAS = 4, 5                   # AmusementGamesQuest's: you (stores text) and a counter (uses it)
RULE = "=================================================="  # (as wide as the Nuka-Cade's screens)

# the catalogue: (section, [(what the terminal says, DLCNukaWorld.esm id, how many, tickets)]). Only Nuka-World's
# own things (its prize counter's Paddle Ball and Acid Soaker, its souvenirs, its sodas ice-cold, its 7.62), at the
# Nuka-Cade's own prices: an item's caps value x 50 (DLC04NukacadePriceMult), a bundle that times how many
PRIZES = [
    ("Weapons", [
        ("Paddle Ball", 0x00A6C0, 1, 500),
        ("Thirst Zapper", 0x007BC8, 1, 500),
        ("Commie Whacker", 0x052926, 1, 1250),          # DLC04_CommieWhacker: Whac-a-Commie's mallet
        ("Acid Soaker", 0x007BC6, 1, 6250),
    ]),
    ("Ammo", [
        ("Paddle Balls x10", 0x00A6C2, 10, 500),         # DLC04_Ammo_PaddleBall
        ("Water Ammo x20", 0x04C0E4, 20, 1000),          # DLC04_Ammo_ThirstZapper_Water (worth 0: priced as 1 cap)
        ("Nuka-Cola Ammo x20", 0x02BDC2, 20, 1000),      # DLC04_Ammo_ThirstZapper_NukaCola
        ("Nuka-Cherry Ammo x20", 0x00A6C9, 20, 1000),    # DLC04_Ammo_ThirstZapper_Cherry
        ("Nuka-Quantum Ammo x20", 0x00A6C6, 20, 1000),   # DLC04_Ammo_ThirstZapper_Quantum
        ("Acid Concentrate x20", 0x030076, 20, 2000),    # DLC04_Ammo_AcidSoaker: the Acid Soaker's (a load)
        ("7.62 Rounds x20", 0x037897, 20, 2000),         # DLC04_Ammo_HandmadeRound
    ]),
    ("Drinks", [
        ("Ice-Cold Nuka-Cola Orange", 0x02EDAD, 1, 1000),
        ("Ice-Cold Nuka-Grape", 0x02EDAB, 1, 1000),
        ("Ice-Cold Nuka-Cola Wild", 0x02EDB1, 1, 1000),
        ("Ice-Cold Nuka-Cola Victory", 0x02EDB0, 1, 2000),
        ("Ice-Cold Nuka-Cola Quartz", 0x02EDAF, 1, 2000),
        ("Ice-Cold Nuka-Cola Dark", 0x02EDA9, 1, 2500),
    ]),
    ("Souvenirs", [
        ("Souvenir Teddy Bear", 0x02689C, 1, 150),
        ("Souvenir Sloth Toy", 0x02A185, 1, 150),
        ("Souvenir Magnet Cow", 0x0268A1, 1, 150),       # DLC04_SouvenirMagnet01
    ]),
]



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


def keyword(form_id: int, editor_id: str) -> bytes:
    # colour (white), type 0 (none): as vanilla link keywords (RELinkTravel)
    return record(
        b"KYWD", 0, form_id,
        subrecord(b"EDID", zstring(editor_id)) + subrecord(b"CNAM", bytes.fromhex("ffffff00")) + subrecord(b"TNAM", struct.pack("<I", 0)),
    )


TRAVEL_TEMPLATE = 0x00002CB0  # Fallout4.esm "Travel"


def travel_package(form_id: int, editor_id: str, link_keyword: int, radius: int = 10, general: int = 0x00002000,
                   interrupts: int = 0x00FD) -> bytes:
    """A Travel package to the actor's linked ref (with link_keyword), like vanilla
    VRWorkshopShared_WorkshopSummonedByBellPackage (and Wasteland Wagers' walk): same template, data inputs and event
    blocks, walking."""
    no_event = subrecord(b"INAM", struct.pack("<I", 0)) + subrecord(b"PDTO", bytes(8))
    body = (
        subrecord(b"EDID", zstring(editor_id))
        + subrecord(b"PKDT", struct.pack("<IBBBBHH", general, 18, 0, 0, 0x4D, interrupts, 0))
        + subrecord(b"PSDT", bytes.fromhex("00ff00ffff000000") + struct.pack("<i", 0))
        + subrecord(b"PKCU", struct.pack("<III", 4, TRAVEL_TEMPLATE, 1))
        + subrecord(b"ANAM", zstring("Location")) + subrecord(b"PLDT", struct.pack("<IIiI", 6, link_keyword, radius, 0))
        + subrecord(b"ANAM", zstring("Bool")) + subrecord(b"CNAM", b"\x00")  # Prefer Preferred Path?
        + subrecord(b"ANAM", zstring("Bool")) + subrecord(b"CNAM", b"\x00")  # Use "soft radius"?
        + subrecord(b"ANAM", zstring("Bool")) + subrecord(b"CNAM", b"\x00")  # Ignore marker orientation? (no: face it)
        + subrecord(b"UNAM", b"\x01") + subrecord(b"UNAM", b"\x03") + subrecord(b"UNAM", b"\x05") + subrecord(b"UNAM", b"\x07")
        + subrecord(b"XNAM", b"\x08")
        + subrecord(b"POBA", b"") + no_event
        + subrecord(b"POEA", b"") + no_event
        + subrecord(b"POCA", b"") + no_event
    )
    return record(b"PACK", 0, form_id, body)


USE_WEAPON_TEMPLATE = 0x0008F677  # Fallout4.esm's UseWeapon package template (as MinRecruit00AimWeapon uses it)


def aim_package(form_id: int, editor_id: str, spot_keyword: int, target_keyword: int, weapon: int = 0) -> bytes:
    """Stand at the actor's linked ref (spot_keyword) with the weapon drawn, aiming at their other linked ref
    (target_keyword) without firing (the plugin fires the shots): laid out as Fallout4.esm's MinRecruit00AimWeapon
    (the UseWeapon template, its 23 data inputs in its order), its location and target our linked refs, any time, and
    no interruptions (no hellos, chatter, idle chatter)."""
    no_event = subrecord(b"INAM", struct.pack("<I", 0)) + subrecord(b"PDTO", bytes(8))
    def b(v):
        return subrecord(b"ANAM", zstring("Bool")) + subrecord(b"CNAM", bytes([v]))
    def fl(v):
        return subrecord(b"ANAM", zstring("Float")) + subrecord(b"CNAM", struct.pack("<f", v))
    def it(v):
        return subrecord(b"ANAM", zstring("Int")) + subrecord(b"CNAM", struct.pack("<i", v))
    body = (
        subrecord(b"EDID", zstring(editor_id))
        # general flags 0x00800000 (weapon drawn), type 18, interrupt override 0, preferred speed 2, unknown 0, interrupt
        # flags 0 (none)
        + subrecord(b"PKDT", struct.pack("<IBBBBHH", 0x00800000, 18, 0, 2, 0, 0, 0))
        + subrecord(b"PSDT", bytes.fromhex("00ff00ffff000000") + struct.pack("<i", 0))
        + subrecord(b"PKCU", struct.pack("<III", 23, USE_WEAPON_TEMPLATE, 11))
        # 00 Location: near the linked ref (spot), radius 10
        + subrecord(b"ANAM", zstring("Location")) + subrecord(b"PLDT", struct.pack("<IIiI", 6, spot_keyword, 10, 0))
        # 01 Weapon to Use: as vanilla (an object type: the best they have of it), or (weapon) that very weapon (object
        # ID): left to choose, a settler took up their own gun at the zapper race and fired bullets
        + subrecord(b"ANAM", zstring("TargetSelector"))
        + subrecord(b"PTDA", struct.pack("<IiI", 1, weapon, 0) if weapon else bytes.fromhex("020000001400000000000000"))
        # 02 Target: the linked ref (target)
        + subrecord(b"ANAM", zstring("SingleRef")) + subrecord(b"PTDA", struct.pack("<IiI", 3, target_keyword, 0))
        + b(0)   # 1f Move while aiming?
        + b(0)   # 03 Always Hit?
        + b(1)   # 04 Head-track the Target?
        + b(0)   # 05 Volley Fire?
        + b(0)   # 06 Crouch Reload?
        + b(1)   # 07 Do No Damage?
        + b(0)   # 08 Hold When Blocked? (no: with the counter in front, read as cover, they crouched behind it)
        + b(0)   # 0f Allow Combat to Start?
        + b(1)   # 10 Aim Without Firing?
        + fl(0.0) + fl(0.0) + fl(25.0)   # 09 Volley Wait Min, 0a Max, 14 Power Attack Percent
        + b(0)   # 11 (deprecated)
        + it(0) + it(0) + it(0)          # 0b Volleys To Fire, 0c Volley Shots Min, 0d Max
        + fl(0.0)  # 13 Block Percent
        + b(1) + b(0)  # 18 True, 19 False
        + subrecord(b"ANAM", zstring("SingleRef")) + subrecord(b"PTDA", bytes.fromhex("060000000000000000000000"))  # 21 self
        + b"".join(subrecord(b"UNAM", bytes([u])) for u in (0x00, 0x01, 0x02, 0x1F, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0F, 0x10,
                                                           0x09, 0x0A, 0x14, 0x11, 0x0B, 0x0C, 0x0D, 0x13, 0x18, 0x19, 0x21))
        + subrecord(b"XNAM", b"\x22")
        + subrecord(b"POBA", b"") + no_event
        + subrecord(b"POEA", b"") + no_event
        + subrecord(b"POCA", b"") + no_event
    )
    return record(b"PACK", 0, form_id, body)


# DLCCoast.esm's DLC03_LeverGun (0102C8B0) record, its subrecords as they are there (decompressed): what an override
# of it in xEdit would copy
LEVER_GUN_RECORD = bytes.fromhex(
    "454449440f00444c4330335f4c6576657247756e004f424e440c000000000000000000000000005054524e0400161c030046554c4c0400b5"
    "2500004d4f444c3100576561706f6e735c48756e74696e675269666c655c48756e74696e675269666c6544756d6d79506172656e742e6e69"
    "66004d4f445414000400000000000000000000000000000000000000455459500400453f0100424944530400e53e1200594e414d04002349"
    "03005a4e414d0400c3f501004b53495a0400090000004b5744412400ea4a0f00a1a00400cb9c040185c41000b4ad0300862a090034f80201"
    "122b0001789e1c0044455343040000000000494e52440400cf772300415050520c0004400200a0980400c8321e004f42544504000b000000"
    "4f425446000046554c4c04009d3300004f4254533c00060000000000000000000000ffff0100000043a60401000001702e04010000016d2e"
    "04010000016e2e04010000016f2e04010000011df204000000014f425446000046554c4c0400e24100004f42545340000600000000000000"
    "00000000ffff0001a7421600000043a60401000001702e04010000016d2e04010000016e2e04010000016f2e04010000011df20400000001"
    "4f425446000046554c4c04004e4100004f4254531600000000000000000000000000010000012df4050000004f425446000046554c4c0400"
    "ad4100004f4254534000060000000000000000000000ffff0001a6421600000043a6040100000149a6040100000147a6040100000144a604"
    "010000014aa6040100000148a604010000014f425446000046554c4c04003f4100004f425453160000000000000000000000000003000001"
    "2df4050000004f425446000046554c4c0400bb4100004f4254534000060000000000000000000000ffff0001989c0d00000043a604010000"
    "01702e04010000016d2e04010000016e2e04010000016f2e04010000011df204000000014f425446000046554c4c0400934100004f425453"
    "4000060000000000000000000000050000012df40500000043a6040100000149a604010000016e2e040100000146a604010000014da60401"
    "00000148a604010000014f425446000046554c4c0400444100004f4254534000060000000000000000000000ffff000193f70c00000043a6"
    "0401000001702e0401000001c99c04010000016e2e04010000016f2e04010000011df204000000014f425446000046554c4c0400aa410000"
    "4f4254534000060000000000000000000000070000012df40500000043a6040100000149a6040100000147a6040100000145a60401000001"
    "4aa6040100000148a604010000014f425446000046554c4c0400ee4100004f4254534000060000000000000000000000ffff000197f70c00"
    "000043a60401000001702e0401000001c99c0401000001c79c0401000001c29c04010000011df204000000014f425446000046554c4c0400"
    "c44100004f4254534000060000000000000000000000090000012df40500000043a6040100000149a6040100000147a6040100000145a604"
    "010000014ca6040100000148a6040100000153544f500000444e414d8400b1c802010000803f0000803f0000803f00008043000080430000"
    "003f000000000000003f00000000000000000000000000014000050009000080400000000096000000230000000000000000000000000000"
    "000000000000000000000000000000ac2615000000000000abaa2a3f00000000d041000000000000000001000000ffff7f7f464e414d2900"
    "acc527370000003f0000803fc3f5a83e5e3373400000803fcdcc4c3e01000000000000000000000000435244540c00000000400000803f00"
    "000000494e414d0400374b00004c4e414d0400722e040157414d440400cc9c0401575a4d44040046b20400")


def settler_lever_gun(form_id: int, editor_id: str) -> bytes:
    """Far Harbor's lever-action as a record of ours: its form IDs kept as they are (its masters, Fallout4.esm then
    itself, are our first two), its animations' keyword the Hunting Rifle's, and its names written out (DLCCoast's are
    localized)."""
    body = LEVER_GUN_RECORD
    out = b""
    i = 0
    first_full = True
    while i < len(body):
        sig = body[i:i + 4]
        n = struct.unpack_from("<H", body, i + 4)[0]
        v = body[i + 6:i + 6 + n]
        i += 6 + n
        if sig == b"EDID":
            v = zstring(editor_id)
        elif sig == b"FULL":
            v = zstring("Lever-Action Rifle" if first_full else "Default")
            first_full = False
        elif sig == b"DESC":
            v = zstring("")
        elif sig == b"KWDA":
            ids = [ANIMS_HUNTING_RIFLE if k == ANIMS_LEVER_ACTION else k for k in struct.unpack("<%dI" % (n // 4), v)]
            v = struct.pack("<%dI" % len(ids), *ids)
        out += subrecord(sig, v)
    return record(b"WEAP", 0, form_id, out)


def quest(form_id: int, editor_id: str, package: int, hold: int, hold_drawn: int, hold_zapper: int, priority: int = 100) -> bytes:
    """AmusementGamesQuest: start game enabled, priority 100, collection alias 0 'Players' with the walk package.
    Scripts: AmusementGamesQuest on the quest (property Players), AmusementGamesGroupAlias on the alias (papyrus/).
    Laid out as Wasteland Wagers' WastelandWagersQuest (itself as Fallout4.esm's DialogueGlowingSeaAtom)."""
    def entry(name: str, props: list[bytes]) -> bytes:
        return wstring(name) + b"\x00" + struct.pack("<H", len(props)) + b"".join(props)
    aliases = [(0, "Players", package), (1, "Shooters", hold), (2, "Holders", hold_drawn), (3, "Squirters", hold_zapper)]
    props = [wstring(n) + bytes([1, 1]) + struct.pack("<HhI", 0, i, form_id) for i, n, _ in aliases]
    vmad = (struct.pack("<HHH", 6, 2, 1) + entry("AmusementGamesQuest", props)
            + b"\x03" + struct.pack("<H", 0) + wstring("")
            + struct.pack("<H", len(aliases))
            + b"".join(struct.pack("<HhI", 0, i, form_id) + struct.pack("<HHH", 6, 2, 1) + entry("AmusementGamesGroupAlias", [])
                       for i, _, _ in aliases))
    def collection(alias_id: int, name: str, pack: int) -> bytes:
        return (
            subrecord(b"ALCS", struct.pack("<I", alias_id))
            + subrecord(b"ALMI", b"\x00")
            + subrecord(b"ALST", struct.pack("<I", alias_id))
            + subrecord(b"ALID", zstring(name))
            + subrecord(b"FNAM", struct.pack("<I", 0x0000020A))  # optional, allow reuse in quest, allow reserved
            + subrecord(b"ALPC", struct.pack("<I", pack))
            + subrecord(b"VTCK", struct.pack("<I", 0))
            + subrecord(b"ALED", b"")
        )
    body = (
        subrecord(b"EDID", zstring(editor_id))
        + subrecord(b"VMAD", vmad)
        + subrecord(b"DNAM", struct.pack("<HBBfI", 0x0011, priority, 0, 0.0, 0))
        + subrecord(b"NEXT", b"")
        + subrecord(b"ANAM", struct.pack("<I", COUNTER_ALIAS + 1))
        + b"".join(collection(i, n, p) for i, n, p in aliases)
        # the Prize Counter's: you, whose name it stores (stores text, optional), and the counter you're at, which uses
        # it (uses stored text, optional), as DLCNukaWorld.esm's DLC04NukaCade has its Player and NukacadeTerminal:
        # its welcome, "<Alias=Player>". Both filled by the counter's script (AmusementGamesPrizeCounter)
        + subrecord(b"ALST", struct.pack("<I", PLAYER_ALIAS)) + subrecord(b"ALID", zstring("Player"))
        + subrecord(b"FNAM", struct.pack("<I", 0x00000102)) + subrecord(b"ALED", b"")
        + subrecord(b"ALST", struct.pack("<I", COUNTER_ALIAS)) + subrecord(b"ALID", zstring("PrizeCounter"))
        + subrecord(b"FNAM", struct.pack("<I", 0x00004002)) + subrecord(b"ALED", b"")
    )
    return record(b"QUST", 0, form_id, body)


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
        + subrecord(b"ATTX", zstring("Buy 5 Tokens"))
        + subrecord(b"FNAM", struct.pack("<H", 0)),
    )


def machine(form_id: int, editor_id: str, name: str, mesh: str, bounds: tuple, preview: int, prompt: str = "Play") -> bytes:
    """An arcade machine: an activator with no script (activating it is what the plugin listens for), Nuka-World's
    model, bounds and workshop preview; its prompt (Bandit Roundup's says a hold is a 1v1)."""
    return record(
        b"ACTI", 0, form_id,
        subrecord(b"EDID", zstring(editor_id))
        + subrecord(b"OBND", struct.pack("<6h", *bounds))
        + subrecord(b"PTRN", struct.pack("<I", preview))
        + subrecord(b"FULL", zstring(name))
        + subrecord(b"MODL", zstring(mesh))
        + subrecord(b"ATTX", zstring(prompt))
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


def idle(form_id: int, editor_id: str, animation: str) -> bytes:
    """A dynamic idle playing our own .hkx, laid out as Nuka-World's DLC04LooseSmokeBombThrow: the human behavior graph,
    event dyn_Activation, the file in GNAM (Meshes-relative, .hkt as the game's own spell it), no conditions."""
    return record(b"IDLE", 0, form_id,
                  subrecord(b"EDID", zstring(editor_id))
                  + subrecord(b"DNAM", zstring("actors\\Character\\Behaviors\\RaiderRootBehavior.hkx"))
                  + subrecord(b"ENAM", zstring("dyn_Activation"))
                  + subrecord(b"ANAM", struct.pack("<II", 0, 0))
                  + subrecord(b"DATA", bytes((0, 0, 1, 0x53, 0, 0)))
                  + subrecord(b"GNAM", zstring(f"Actors/Character/Animations/AmusementGames/{animation}.hkt")))


def global_float(form_id: int, editor_id: str, value: float) -> bytes:
    return record(b"GLOB", 0, form_id,
                  subrecord(b"EDID", zstring(editor_id)) + subrecord(b"FNAM", b"f") + subrecord(b"FLTV", struct.pack("<f", value)))


def prize_number(page: int, row: int) -> int:
    """a prize's number across the catalogue (1 up): its menu item id and its fragment's"""
    return sum(len(items) for _, items in PRIZES[:page]) + row + 1


def tickets_condition(at_least: bool, tickets: int) -> bytes:
    """the tickets redeemed into the counter (GetGlobalValue, function 74, of AmusementGamesTicketsCurrent: as
    Nuka-World's prize pages check DLC04NukacadeTicketsRedeemedCurrent) >= or < tickets: comparison, value, function,
    param 1, param 2, run on (subject), reference, param 3"""
    op = 0x60 if at_least else 0x80  # (>= is 3, < is 4, in the top three bits)
    return subrecord(b"CTDA", struct.pack("<B3xfHxxIIIIi", op, float(tickets), 74, OWN | TICKETS_CURRENT, 0, 0, 0, -1))


def terminal_fragments(fragments: list[tuple[int, str]]) -> bytes:
    """a terminal's script fragments, laid out as DLCNukaWorld.esm's DLC04NukacadeSubTerminalTicketSpendConfirm: no
    scripts of its own; then the fragment script (bind data version 3, its name, flags 0, no properties) and each
    fragment: the menu item id it runs for, 1, the script, the function"""
    out = struct.pack("<HHH", 6, 2, 0) + b"\x03" + wstring(PRIZE_FRAGMENTS) + b"\x00" + struct.pack("<H", 0)
    out += struct.pack("<H", len(fragments))
    for item_id, function in fragments:
        out += struct.pack("<IB", item_id, 1) + wstring(PRIZE_FRAGMENTS) + wstring(function)
    return out


# what a page of the counter has besides its menu: Nuka-World's subterminals' (DLC04NukacadeSubTerminalTicketSpend01)
# keywords, display settings and marker parameters
SUBTERMINAL_KEYWORDS = (0x000A56D7, 0x0006D5BB, 0x000C2022)  # FurnitureForce1stPerson, PlayerPathToFurniture, AnimFurnWallTerminal


def terminal_frame(mesh: str, keywords: tuple, marker: bytes, xmarker: str = "") -> bytes:
    return (subrecord(b"MODL", zstring(mesh))
            + subrecord(b"KSIZ", struct.pack("<I", len(keywords)))
            + subrecord(b"KWDA", b"".join(struct.pack("<I", k) for k in keywords))
            + subrecord(b"PNAM", struct.pack("<I", 0x00334CCC))
            + subrecord(b"FNAM", struct.pack("<H", 0))
            + subrecord(b"COCT", struct.pack("<I", 0))
            + subrecord(b"MNAM", bytes.fromhex("01000040"))
            + subrecord(b"WBDT", b"\x00")
            + (subrecord(b"XMRK", zstring(xmarker)) if xmarker else b"")  # (where you stand at it, in the editor)
            + subrecord(b"SNAM", marker))


def subterminal(form_id: int, editor_id: str, text: str, items: list[bytes], fragments: list[tuple[int, str]]) -> bytes:
    """a page of the counter, as Nuka-World's subterminals: their frame, its text, its menu, its fragments"""
    return record(b"TERM", 0, form_id,
                  subrecord(b"EDID", zstring(editor_id))
                  + (subrecord(b"VMAD", terminal_fragments(fragments)) if fragments else b"")
                  + subrecord(b"OBND", struct.pack("<6h", -18, -52, 86, 21, 0, 135))
                  + terminal_frame("Furniture\\Terminals\\TerminalWall01.nif", SUBTERMINAL_KEYWORDS,
                                   bytes.fromhex("000000000000acc2000000000000000000000000ffffffff"))
                  + subrecord(b"BSIZ", struct.pack("<I", 1))
                  + subrecord(b"BTXT", zstring(text))
                  + subrecord(b"ISIZ", struct.pack("<I", len(items)))
                  + b"".join(items))


def menu_item(text: str, kind: int, item_id: int, rest: bytes = b"") -> bytes:
    """a menu item: its text, its type (4 a page, 5 back to the top, 6 run and redraw, 8 display text), its id, then
    its page / display text / conditions"""
    return subrecord(b"ITXT", zstring(text)) + subrecord(b"ANAM", bytes([kind])) + subrecord(b"ITID", struct.pack("<H", item_id)) + rest


TICKETS_LINE = f"               Current Tickets: <Token.ValueInt=CurrentTickets>\r\n{RULE}\r\n"  # (atop each page)


def prize_page(form_id: int, page: int) -> bytes:
    """a section of the Prize Counter: each prize twice, one shown when you've the tickets redeemed (it runs its
    fragment, which takes them and gives the prize, then redraws: type 6, as Nuka-World's main terminal's), one when
    you haven't (type 8, display text: how many it takes)"""
    title, items = PRIZES[page]
    menu = []
    fragments = []
    for row, (name, _, _, tickets) in enumerate(items):
        n = prize_number(page, row)
        menu.append(menu_item(f"{tickets} Tickets - {name}", 6, n, tickets_condition(True, tickets)))
        menu.append(menu_item(f"{tickets} Tickets - {name} (not enough)", 8, NOT_ENOUGH + n,
                              subrecord(b"UNAM", zstring(f"{RULE}\r\n\r\n{name}: {tickets} Tickets\r\n\r\n"
                                                         "     You have insufficient tickets at this time.\r\n"
                                                         "  Redeem more tickets, or win more at the machines!\r\n"))
                              + tickets_condition(False, tickets)))
        fragments.append((n, f"Fragment_Terminal_{n:02d}"))
    return subterminal(form_id, f"AmusementGamesPrizeCounter{title.replace('-', '')}",
                       f"{TICKETS_LINE}               {title}: Please Select a Prize\r\n", menu, fragments)


def redeem_page(form_id: int) -> bytes:
    """Redeem Tickets, as Nuka-World's DLC04NukacadeSubTerminalTicketDeposit: all the tickets you carry into the
    counter (Yes runs its fragment, AmusementGamesNative.RedeemTickets, then back to the top), or not"""
    text = ("Nuka-Cade tickets can be won from the games here in your settlements.\r\n\r\n"
            "You are carrying <Token.ValueInt=CarriedTickets> Nuka-Cade Tickets.\r\n"
            "You currently have redeemed <Token.ValueInt=CurrentTickets> Nuka-Cade Tickets.\r\n\r\n"
            "Would you like to redeem all of your Nuka-Cade Tickets?\r\n")
    return subterminal(form_id, "AmusementGamesPrizeCounterRedeem", text,
                       [menu_item("Yes", 5, 1, subrecord(b"RNAM", zstring("Redeeming Tickets..."))), menu_item("No", 5, 2)],
                       [(1, "Fragment_Terminal_Redeem")])


def spend_page(form_id: int) -> bytes:
    """Spend Tickets, as Nuka-World's DLC04NukacadeSubTerminalTicketSpendMain: the sections"""
    items = [menu_item(title, 4, page + 1, subrecord(b"TNAM", struct.pack("<I", OWN | (PRIZE_PAGE_FIRST + page))))
             for page, (title, _) in enumerate(PRIZES)]
    return subterminal(form_id, "AmusementGamesPrizeCounterSpend",
                       f"{TICKETS_LINE}            Please Select a Prize Category\r\n", items, [])


def prize_counter(form_id: int) -> bytes:
    """the Prize Counter: Nuka-World's own prize terminal in the Nuka-Cade (DLC04NukacadeTerminal: the console, its
    bounds, furniture keywords, marker and its parameters; none of its script or menus), with our menus as its:
    Redeem Tickets, Spend Tickets (the sections), View High Scores.
    Its workshop preview the base game's buildable Terminal's (workshop_Terminal): Nuka-World's has none, not being
    buildable. Needs no power. Its screen as the Nuka-Cade's (its header, welcome and high scores, copied): your
    tickets, redeemed and best scores are tokens the script puts on it (AddTextReplacementData, as Nuka-World's), from
    globals the plugin keeps current"""
    scores = "".join(f"   {n + ':':<18}<Token.ValueInt=HighScore{g}>\r\n" for g, n in zip(BEST_GAMES, BEST_NAMES))
    menu = [menu_item("Redeem Tickets", 4, 1, subrecord(b"TNAM", struct.pack("<I", OWN | REDEEM_PAGE))),
            menu_item("Spend Tickets", 4, 2, subrecord(b"TNAM", struct.pack("<I", OWN | SPEND_PAGE))),
            menu_item("View High Scores", 8, 3,
                      subrecord(b"UNAM", zstring(f"{RULE}\r\n               Personal High Scores\r\n{RULE}\r\n\r\n{scores}")))]
    keywords = (0x000286E6, 0x0006D5BB, 0x000A56D7, 0x000FCB12)  # AnimFurnDeskTerminal, PlayerPathToFurniture,
    # FurnitureForce1stPerson, AnimFurnDeskTerminalNoChair (DLC04NukacadeTerminal's)
    body = (subrecord(b"EDID", zstring("AmusementGamesPrizeCounter"))
            # (its script: the screen's tokens, and you and it into the quest's aliases for your name)
            + subrecord(b"VMAD", object_vmad("AmusementGamesPrizeCounter", []))
            + subrecord(b"OBND", struct.pack("<6h", -20, -34, 0, 19, 16, 97))
            + subrecord(b"PTRN", struct.pack("<I", 0x0017B540))  # workshop_Terminal
            + subrecord(b"NAM0", zstring("               === Nuka-Cade ==="))  # (the header, as the Nuka-Cade's)
            + subrecord(b"FULL", zstring("Prize Counter"))
            + terminal_frame("Furniture\\Terminals\\TerminalConsoleOn.nif", keywords,
                             bytes.fromhex("00000000000088c2000000000000000000000000ffffffff"),
                             "Markers\\MarkerDeskTerminal01.nif")
            + subrecord(b"BSIZ", struct.pack("<I", 1))
            + subrecord(b"BTXT", zstring("Welcome to the Nuka-Cade, <Alias=Player>\r\n\r\n"
                                         "Exchange your tickets for fun and exciting prizes!\r\n\r\n"
                                         "Current Tickets: <Token.ValueInt=CurrentTickets>\r\n"
                                         "Total Tickets Redeemed: <Token.ValueInt=TotalRedeemed>\r\n\r\n"))
            + subrecord(b"ISIZ", struct.pack("<I", len(menu)))
            + b"".join(menu))
    return record(b"TERM", 0, form_id, body)


def fragments_psc() -> str:
    """papyrus/AmusementGamesPrizeCounterFragments.psc: a function a prize, each buying it (AmusementGamesNative.BuyPrize)"""
    lines = [";BEGIN FRAGMENT CODE - written by tools/make_esl.py --psc: the Prize Counter's prizes",
             f"Scriptname {PRIZE_FRAGMENTS} Extends Terminal Hidden Const", ""]
    for page, (_, items) in enumerate(PRIZES):
        for row, (name, item, count, tickets) in enumerate(items):
            n = prize_number(page, row)
            lines += [f";BEGIN FRAGMENT Fragment_Terminal_{n:02d}",
                      f"Function Fragment_Terminal_{n:02d}(ObjectReference akTerminalRef)",
                      ";BEGIN CODE",
                      f"AmusementGamesNative.BuyPrize(0x{item:06X}, {count}, {tickets})  ; {name}",
                      ";END CODE",
                      "EndFunction",
                      ";END FRAGMENT", ""]
    lines += [";BEGIN FRAGMENT Fragment_Terminal_Redeem",
              "Function Fragment_Terminal_Redeem(ObjectReference akTerminalRef)",
              ";BEGIN CODE",
              "AmusementGamesNative.RedeemTickets()  ; Redeem Tickets: Yes",
              ";END CODE",
              "EndFunction",
              ";END FRAGMENT", ""]
    lines.append(";END FRAGMENT CODE - Do not edit anything between this and the begin comment")
    return "\n".join(lines) + "\n"


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
    (0x21, "Basketball", "SetDressing\\SportsEquipment\\BasketBall01.nif", (-12, -12, -12, 12, 12, 12)),  # the base game's
    (0x22, "CommieHead", GAMES_DIR + "DLC04_Nukacade_Commie01.nif", (-5, -4, -18, 5, 5, 2)),  # the still one: origin at its top
    (0x23, "ZapperRocket", GAMES_DIR + "DLC04NukacadeNukaZapperRocket01\\DLC04NukacadeNukaZapperRocket01.nif", (-9, -9, -23, 9, 9, 22)),
    (0x24, "ZapperDrop", "DLC04\\Effects\\DLC04ProjectileThirstZapperWater.nif", (-2, -2, -2, 2, 2, 2)),
    (0x25, "GalleryDuck", GAMES_DIR + "DLC04_Nukacade_ShootingGalleryDuck01.nif", (-15, 0, -4, 15, 0, 32)),
    (0x26, "GalleryBunny", GAMES_DIR + "DLC04_Nukacade_ShootingGalleryBunny01.nif", (-18, 0, -5, 18, 0, 36)),
    (0x27, "GalleryPig", GAMES_DIR + "DLC04_Nukacade_ShootingGalleryPig01.nif", (-15, 0, -5, 15, 0, 26)),
    (0x28, "GalleryBottle", GAMES_DIR + "DLC04_Nukacade_ShootingGallerybottle01.nif", (-9, 0, -6, 9, 1, 48)),
    (0x29, "GalleryStar", GAMES_DIR + "DLC04_Nukacade_ShootingGalleryStar01.nif", (-13, 0, -6, 13, 0, 28)),
    (0x2A, "ZapperTarget", GAMES_DIR + "DLC04_Nukacade_NukaZapperTarget01.nif", (-15, -8, -21, 15, 9, 21)),
    (0x2B, "CommieWhacker", GAMES_DIR + "DLC04_Nukacade_CommieWhacker01.nif", (-20, -20, -20, 20, 20, 20)),  # the machine's own mallet
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
    # settlers walked to a machine: the link keyword, the walk package, the quest whose alias carries it
    link = OWN | (FIRST_ID + 0xD)
    walk = OWN | (FIRST_ID + 0xE)
    keywords.append(keyword(link, "AmusementGamesLinkSpot"))
    # ...and once they're there with the rifle: the same spot held, weapon drawn (general flag 0x01000000), and none of
    # the interruptions a package allows (no hellos, chatter, idle chatter, world interactions: interrupt flags 0), so
    # they stand with the rifle up and shoot, nothing else
    # (aiming at their second linked ref, keyword AmusementGamesLinkTarget: the middle of the targets)
    hold = OWN | (FIRST_ID + 0x2C)
    target = OWN | (FIRST_ID + 0x2D)
    keywords.append(keyword(target, "AmusementGamesLinkTarget"))
    # (and for a melee game, Whac-a-Commie: kept at the spot with the mallet drawn, weapon drawn 0x00800000, no
    # interruptions)
    hold_drawn = OWN | (FIRST_ID + 0x2E)
    hold_zapper = OWN | (FIRST_ID + 0x2F)  # (the zapper race's: aiming the Thirst Zapper itself)
    packages = [travel_package(walk, "AmusementGamesWalkToSpot", link),
                # (Bandit Roundup's: that very rifle, the settlers' lever-action, as the zapper race's names the Thirst
                # Zapper)
                aim_package(hold, "AmusementGamesHoldAtSpot", link, target, weapon=SETTLER_LEVER_GUN),
                travel_package(hold_drawn, "AmusementGamesHoldDrawnAtSpot", link, general=0x00802000, interrupts=0),
                aim_package(hold_zapper, "AmusementGamesHoldZapperAtSpot", link, target, weapon=NUKA | 0x007BC8)]  # DLC04_ThirstZapper
    # (priority 101: above Wasteland Wagers' 100, so a roulette bettor called over isn't pulled back mid-walk)
    quests = [quest(OWN | (FIRST_ID + 0xF), "AmusementGamesQuest", walk, hold, hold_drawn, hold_zapper, priority=101)]
    # the category's picture: its first item, Whac-a-Commie, framed as its own workshop preview
    # (DLC04workshop_NukaCade_WhackACommie01) with its bounds
    art = [menu_art(OWN | (FIRST_ID + 1), "AmusementGamesWorkshopMenuArt", menu,
                    "DLC04\\SetDressing\\Games\\DLC04_Nukacade_WhackaCommie01.nif", NUKA | 0x055807,
                    (-48, -40, 0, 48, 55, 179))]
    recipes = [workshop_recipe(OWN | (FIRST_ID + off), f"workshop_co_AmusementGames{name}", made, parts, menu, sounds)
               for off, name, made, parts, sounds in MACHINES]

    # ---- the Token Stand: caps in, tokens out
    broke = OWN | (FIRST_ID + 0xA)
    price = OWN | (FIRST_ID + 0xB)
    # (xx000809, the old how-many menu, is gone: F buys 5 at a time)
    acti = [token_stand(OWN | (FIRST_ID + 8), [("Caps", CAPS), ("Token", TOKEN), ("TokenPrice", price),
                                                ("NotEnoughCapsMessage", broke), ("DispenseSound", TOKEN_SOUND)])]
    messages = [message(broke, "AmusementGamesNotEnoughCapsMessage", "Not enough caps: %.0f needed, you have %.0f.")]
    globs = [global_float(price, "AmusementGamesTokenPrice", TOKEN_PRICE),
             global_float(OWN | (FIRST_ID + 0xC), "AmusementGamesIncomePerSettler", INCOME_PER_SETTLER),
             global_float(OWN | TICKETS_CURRENT, "AmusementGamesTicketsCurrent", 0.0),
             global_float(OWN | TICKETS_REDEEMED, "AmusementGamesTicketsRedeemed", 0.0),
             global_float(OWN | TICKETS_CARRIED, "AmusementGamesTicketsCarried", 0.0)]
    globs += [global_float(OWN | (BEST_FIRST + i), f"AmusementGamesBest{g}", 0.0) for i, g in enumerate(BEST_GAMES)]

    # ---- the machines and their pieces
    for off, name, title, mesh, bounds, preview in GAMES:
        acti.append(machine(OWN | (FIRST_ID + off), f"AmusementGames{name}", title, GAMES_DIR + mesh, bounds, preview,
                            "Play (hold: 1v1)" if name == "BanditRoundup" else "Play"))
    stats = [stat(OWN | (FIRST_ID + off), f"AmusementGamesPiece{name}", mesh, bounds) for off, name, mesh, bounds in PIECES
             if name != "CommieHead"]
    # the commie's head: an activator (no script, nothing to activate), as Nuka-World's own heads are: it gets hit events
    for off, name, mesh, bounds in PIECES:
        if name == "CommieHead":
            acti.append(record(
                b"ACTI", 0, OWN | (FIRST_ID + off),
                subrecord(b"EDID", zstring(f"AmusementGamesPiece{name}"))
                + subrecord(b"OBND", struct.pack("<6h", *bounds))
                + subrecord(b"MODL", zstring(mesh))
                + subrecord(b"FNAM", struct.pack("<H", 0))))

    # ---- the throws settlers make at Hoop Shot and Atomic Rollers: our own animations (30 fps, Blender + PyNifly)
    # (each a second and a third take: its own stance, crouch, turn and lean, its hands the first's; one of the three at
    # random each throw)
    idles = [idle(OWN | (FIRST_ID + 0x30), "AmusementGamesBasketballShot", "AGBasketballShot"),
             idle(OWN | (FIRST_ID + 0x31), "AmusementGamesSkeeballRoll", "AGSkeeballRoll"),
             idle(OWN | (FIRST_ID + 0x34), "AmusementGamesBasketballShot2", "AGBasketballShot2"),
             idle(OWN | (FIRST_ID + 0x35), "AmusementGamesSkeeballRoll2", "AGSkeeballRoll2"),
             idle(OWN | (FIRST_ID + 0x36), "AmusementGamesBasketballShot3", "AGBasketballShot3"),
             idle(OWN | (FIRST_ID + 0x37), "AmusementGamesSkeeballRoll3", "AGSkeeballRoll3")]

    # ---- the settlers' lever-action at Bandit Roundup: Far Harbor's own (yours), with the Hunting Rifle's animations.
    # Far Harbor's for it (third person) had them lower it and drop into the floor at each shot; a weapon's animations
    # are picked by its keyword. (0.8.0's mod changing the keyword on their lent rifle didn't take)
    weapons = [settler_lever_gun(SETTLER_LEVER_GUN, "AmusementGamesSettlerLeverGun")]

    # ---- the Prize Counter: tickets in, Nuka-World's prizes out (its sections, then the page you open)
    terminals = ([prize_counter(PRIZE_COUNTER), redeem_page(OWN | REDEEM_PAGE), spend_page(OWN | SPEND_PAGE)]
                 + [prize_page(OWN | (PRIZE_PAGE_FIRST + i), i) for i in range(len(PRIZES))])
    recipes.append(workshop_recipe(OWN | PRIZE_RECIPE, "workshop_co_AmusementGamesPrizeCounter", PRIZE_COUNTER,
                                   TERMINAL_COMPONENTS, menu, METAL_SOUNDS))

    count = len(terminals) + len(weapons) + len(idles) + len(keywords) + len(art) + len(recipes) + len(acti) + len(messages) + len(globs) + len(stats) + len(packages) + len(quests)
    header = record(
        b"TES4",
        FLAG_MASTER | FLAG_LIGHT,
        0,
        subrecord(b"HEDR", struct.pack("<fiI", 1.0, count, FIRST_ID + 0x48))
        + subrecord(b"CNAM", zstring("evodube"))
        + subrecord(b"SNAM", zstring("Amusement Games: the Nuka-Cade's arcade machines in your settlements"))
        + subrecord(b"MAST", zstring("Fallout4.esm"))
        + subrecord(b"DATA", struct.pack("<Q", 0))
        + subrecord(b"MAST", zstring("DLCCoast.esm"))
        + subrecord(b"DATA", struct.pack("<Q", 0))
        + subrecord(b"MAST", zstring("DLCNukaWorld.esm"))
        + subrecord(b"DATA", struct.pack("<Q", 0)),
    )
    # groups in the order the game's own master has them
    return (header + group(b"GLOB", b"".join(globs)) + group(b"KYWD", b"".join(keywords))
            + group(b"ACTI", b"".join(acti)) + group(b"STAT", b"".join(stats)) + group(b"WEAP", b"".join(weapons))
            + group(b"TERM", b"".join(terminals)) + group(b"QUST", b"".join(quests))
            + group(b"IDLE", b"".join(idles)) + group(b"PACK", b"".join(packages)) + group(b"MESG", b"".join(messages)) + group(b"ARTO", b"".join(art))
            + group(b"COBJ", b"".join(recipes)))


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--psc" in sys.argv:  # the Prize Counter's fragment script, from the catalogue above
        out = args[0] if args else f"papyrus/{PRIZE_FRAGMENTS}.psc"
        with open(out, "w", newline="\n") as f:
            f.write(fragments_psc())
        print(f"wrote {out}")
        return 0
    out = args[0] if args else "AmusementGames.esl"
    data = build()
    with open(out, "wb") as f:
        f.write(data)
    print(f"wrote {out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
