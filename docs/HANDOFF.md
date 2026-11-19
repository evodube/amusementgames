# AmusementGames: project handoff

**Amusement Games** (repo `evodube/amusementgames`): the Nuka-Cade's five arcade games (and its token dispenser),
buildable in settlements under Furniture > Amusement Games. F4SE plugin + generated ESL, OG 1.10.163 first (NG/AE
built too). Set up like Wasteland Wagers (`evodube/WastelandWagers`): same CommonLibF4 commit, xmake, CI.

## Where things live

- `tools/make_esl.py`: AmusementGames.esl (masters Fallout4.esm, DLCNukaWorld.esm). The form ids are in its docstring
  and `src/Forms.h`.
- `src/Forms.h` `AddWorkshopMenu`: puts the category (KYWD xx000800) in the Furniture menu after Tables at game data
  ready, in every form list that has Tables (no FLST override in the .esl).

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
