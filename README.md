# Amusement Games

The Nuka-Cade's arcade games, playable in your settlements, in Fallout 4: build them from the workshop menu under
**Furniture > Amusement Games**, buy tokens at the **Token Stand** with caps, play, win tickets.

- **Atomic Rollers** (skee-ball): hold the left mouse button to charge, let go to roll; look where it goes. Nine
  balls; the rings are 10 to 50, the top corners 100.
- **Hoop Shot**: hold to charge, let go to shoot, as many baskets as you can in 45 seconds (3 points in the last 10).
- **Whac-a-Commie**: look at a commie popping up and click to whack it, for 30 seconds; quicker whacks score more.
- **Nuka-Zapper Race**: hold the button with your sights on your target to squirt it and send your rocket up; first
  to the top wins (the other lanes race you).
- **Bandit Roundup**: shoot the cut-outs running along the gallery for 40 seconds: duck 10, bunny 20, pig 30,
  bottle 40, star 50.
- **Token Stand**: caps in, Nuka-Cade tokens out (5 caps a token). Each game takes a token.

Tickets come out by your score (Nuka-Cade tickets: the Nuka-Cade's prize terminal takes them too). Your best score
at each game is kept in your save. X leaves a game early, paid for the score so far.

Every model, texture and sound is the game's own (Nuka-World's): the mod ships none. The physics (the balls' runs,
bounces and rims) is the mod's own, in plain C++ (`src/sim`), tested on its own like Playable Pool's.

**Status: 0.1.0.**

## Requirements

- Fallout 4 1.10.163 ("Old Gen"; also built for Next-Gen and Anniversary).
- The Nuka-World DLC (`DLCNukaWorld.esm` is a master).
- F4SE and Address Library for F4SE Plugins.
- [F4SE Menu Framework](https://www.nexusmods.com/fallout4/mods/105090) for the HUD (without it, results come as
  corner messages).

## Building

CI (`.github/workflows/build.yml`) runs the games' tests (`tests/games_test.cpp`), builds the plugin with xmake
against Dear-Modding CommonLibF4 (`lib/commonlibf4`), compiles the token stand's script with Caprica and writes the
.esl with `python tools/make_esl.py`. Each push to `main` publishes the `Data` folder to the `builds`
branch.
