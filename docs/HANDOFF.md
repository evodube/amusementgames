# AmusementGames: project handoff

**Amusement Games** (repo `evodube/amusementgames`): the Nuka-Cade's five arcade games, playable in settlements
(Furniture > Amusement Games), a Token Stand (caps for tokens) and tickets by score. F4SE plugin + generated ESL, OG 1.10.163 first (NG/AE
built too). Set up like Wasteland Wagers (`evodube/WastelandWagers`): same CommonLibF4 commit, xmake, CI.

## Where things live

- `tools/make_esl.py`: AmusementGames.esl (masters Fallout4.esm, DLCNukaWorld.esm). The form ids are in its docstring
  and `src/Forms.h`.
- `src/Forms.h` `AddWorkshopMenu`: puts the category (KYWD xx000800) in the Furniture menu after Tables at game data
  ready, in every form list that has Tables (no FLST override in the .esl).

- `src/sim/`: the games, no game dependencies. `Ball3D` (sphere physics: rects, rails, rims, catch discs; fixed
  substeps), `Arcade` (the five games' rules: sim::Game; what's shown, heard and on the HUD; tickets), `Layouts`
  (each machine's geometry in its model's space). Tests: `tests/games_test.cpp` (CI's Linux job).
- `src/Arcade.cpp`: the machines in game: activation (a token taken), first person + an input layer holding
  movement/attacks/activation, the look ray into the machine's space each frame, the pieces (refs of our STATs with
  Nuka-World's models, Havok keyframed) moved by the sim, sounds, tickets paid, best scores (F4SE serialization).
- `src/Sounds.cpp`: Nuka-World's Nuka-Cade sound descriptors through Papyrus Sound.Play / StopInstance.
- `src/Hud.cpp`: the terminal HUD (F4SE Menu Framework), fonts in `dist/Data/F4SE/Plugins/AmusementGames/hud`.
- `papyrus/AmusementGamesTokenStand.psc`: the stand's message box (compiled by Caprica in CI).

## Measurements

`src/sim/Layouts.h` holds the machines' geometry, measured off Nuka-World's models (their Havok collision and named
nodes) with Blender + PyNifly on the PC: the BA2 entries were pulled out to `C:\Users\Dan\Desktop\amusementgames\extract`
(a GNRL BA2 reader in Python run in Blender), then rays cast at the imported collision. Scoring and tickets follow
Nuka-World's scripts' properties (ScoreEvents PointValue, ScorePerTicket): Atomic Rollers holes 0/100-500/700 at
100 a ticket; Hoop Shot 100 a basket, Whac-a-Commie 100 a whack, Bandit Roundup 100-300 a target, all 200 a ticket.

## Hoop Shot's throws

No two alike (HoopLayout handWobble / aimWobble / paceWobble): a dead-on aim at the best power scores about half the
time, not every time.

## Collision

Atomic Rollers and Hoop Shot run on the models' own Havok collision triangles (src/sim/Collision.inc, written by
tools/collision_to_cpp.py from tools/collision/*.json, exported in Blender from the extracted models). Hoop Shot's
rim plates are left out for our round rim. Skee-ball scoring: a ball inside a box at a BallHole node's cup.

## Papyrus (papyrus/AmusementGamesNative.psc)

Sounds, scoreboard digits and the games' weapons go through its global functions, forms passed as ids: calling a
method on a form or ref with no script bound (a sound descriptor, a digit made with initializeScripts false)
silently did nothing in 0.1.0 (no sounds, no digits). If the digits still don't change, the other way: make them
with their scripts on and call DLC04:DLC04ArcadeScoreNumber.DisplayNumber, as Nuka-World does.

## Whac-a-Commie's whacks

The heads are an ACTI of ours (0x822, the still DLC04_Nukacade_Commie01.nif, which has collision), as Nuka-World's
own heads are activators scored OnHit. A TESHitEvent with you as the cause and a head as the target is the whack
(Arcade::OnHit -> sim::Game::OnPieceHit); clicks score nothing. The log says when the first one comes through.

## Weapons

Whac-a-Commie: DLC04_CommieWhacker; Nuka-Zapper Race: DLC04_ThirstZapper (fired for real, a hit each
click on target, thirty to win (the bottles climb to the top of the cabinet); your lane is ZapperLayout::playerLane, marked by a Thirst Zapper model (STAT 82B) lying
on the counter, put up with the scoreboard and hidden while you play); Bandit Roundup:
DLC03_LeverGun (DLCCoast.esm 0002C8B0, looked up by name so Far Harbor isn't a master), else HuntingRifle
(Fallout4.esm); shots counted a second apart. Arcade::TopUpAmmo keeps its magazine at the most rounds seen in it
(Actor::SetCurrentAmmoCount, hand 0), so it's never reloaded; the rounds spent come off the 200 lent. ArmPlayer lends it (if you have none) with 200 of its ammo, equips and draws it; DisarmPlayer takes
back the lent weapon and what's left of the lent ammo and re-equips what you had out. Fighting isn't held for those
games (your swing or shot is the real one; the game reads the click and where you look).

## Props (src/Scoreboard.cpp PutUpProps)

Put up with the scoreboard: Atomic Rollers' 9 balls in the right-hand pocket (RollersLayout::pocket, back along its
channel), Hoop Shot's 3 basketballs at the foot of its ramp, just behind the front shelf, Whac-a-Commie's mallet (STAT 82B, the Nuka-Cade's own
DLC04_Nukacade_CommieWhacker01.nif) at its whacker01 node, the Thirst Zapper and the rifle as real weapon refs laid
on their counters: placed by AmusementGamesNative.PlaceWeapon (PlaceAtMe: a ref made from the plugin got only the
modular rifle's first piece, its lever), BlockActivation, the rifle's iron sights attached, then laid down by the
plugin. Havok
keyframed once loaded. While you play they're hidden, but for the balls still waiting in Atomic Rollers' pocket
(HudInfo::waiting).

## Scoreboards (src/Scoreboard.cpp)

Nuka-World's digits (DLC04_Nukacade_Numbers 0004E138; the _Large 00053653 on the zapper and gallery) are placed at
each machine's Score01-08 / HighScore01-08 nodes (read off the loaded 3D) for machines within 1500 of you, and set
with PlayAnimation("Num0" + digit) as DLC04ArcadeScoreNumber does. Top row: your best at that game; bottom: the
game on that machine (live while playing). Taken down before each save and when you're far; a stray digit in a
slot is cleared when a board goes up.

## Still to do

- Settlers coming to play (as Wasteland Wagers' bettors); the ticket store.

## Nuka-World's machines (DLCNukaWorld.esm)

| Game | ACTI | Model (DLC04\SetDressing\Games\) | Script | Workshop preview (TRNS) |
|---|---|---|---|---|
| Whac-a-Commie | 0004BD36 DLC04_Nukacade_WhackaCommie01Red | DLC04_Nukacade_WhackaCommie01.nif | DLC04:DLC04ArcadeWhackAMole | 00055807 |
| Nuka-Zapper Race | 0004CBDC DLC04_Nukacade_NukaZapperRace | DLC04_Nukacade_NukaZapperRace.nif | DLC04:DLC04ArcadeNukaZapperRaceScript | 00055805 |
| Atomic Rollers | 0004BD34 DLC04_Nukacade_AtomicRollers01Blue | DLC04_Nukacade_SkeeBall01.nif | DLC04:DLC04ArcadeAtomicRollers | 00055802 |
| Hoop Shot | 0004BD31 DLC04_Nukacade_HoopShot | DLC04_Nukacade_BasketBall01.nif | DLC04:DLC04ArcadeHoopShot | 00055804 |
| Bandit Roundup | 0004BD30 DLC04_Nukacade_BanditRoundup | DLC04_Nukacade_ShootingGallery01.nif | DLC04:DLC04ArcadeShootingGallery | 00055803 |
| Token Dispenser | 0004BD38 DLC04_Nukacade_TokenDispenser01 | DLC04_Nukacade_TokenDispenser01.nif | DLC04:DLC04ArcadeTokenMachine | 00055806 |

The game machines carry BlockPlayerActivation and a scoreboard (number models placed on nodes Score01..08 of the
machine's model). Their parts: Whac-a-Commie targets 0004E13B, Zapper Race rockets 00051FC8 / targets 00051FC9,
Atomic Rollers ball MISC 00019987 and hole triggers 00051E44/45, Hoop Shot trigger 00051E42 and ball MISC 000503B2,
Bandit Roundup targets 0004D68B-90 (left to right) and 00051E47-4B (right to left), numbers 0004E138 / 00053653.
Static copies of each model exist too (STAT 0001997C etc.).

Nuka-World's own recipes: DLC04workshop_co_Nukacade* (000517BE-C5), category Fallout4.esm 0024A077
WorkshopRecipeFilterDecor02NukaWorld03NukaCade, conditions GetStageDone DLC04MQ01 1000 or 1500. Ours copy their
components and sounds, without the conditions.

## Next

Revamp each game: its recipe moves to a form of ours (the same model) that the plugin drives.
