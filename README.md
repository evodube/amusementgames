# Amusement Games

The Nuka-Cade's arcade games, playable in your settlements, in Fallout 4: build them from the workshop menu under
**Furniture > Amusement Games**, buy tokens at the **Token Stand** with caps, play, win tickets.

- **Atomic Rollers** (skee-ball): hold the left mouse button to charge, let go to roll; look where it goes. Nine
  balls over the hump and up the board, its ring walls funnelling them into the holes: 100 to 500 up the middle,
  700 in the top corners, nothing in the gutter or over the top.
- **Hoop Shot**: hold to charge, let go to lob it at the hoop, as many baskets as you can in 45 seconds.
- **Whac-a-Commie**: seven holes; swing the Commie Whacker at the commies popping up, for 30 seconds: a point is
  your mallet landing on a commie (its hit), not a click. Just the clock on screen while you play.
- **Nuka-Zapper Race**: six lanes. Hold the button with your Thirst Zapper's stream on your target and your rocket
  climbs; ten steps up and you've won (the other lanes race you, streams and all).
- **Bandit Roundup**: shoot the cut-outs running along the gallery's two tracks with Far Harbor's Lever-Action Rifle
  (the Hunting Rifle without Far Harbor), for 60 seconds. Unlimited ammo while you play: it never needs reloading.
- **Token Stand**: caps in, Nuka-Cade tokens out (5 caps a token). Each game takes a token.

A point a hit (a basket, a whack, a target, a step up the zapper race; a race won is 10 more); Atomic Rollers' holes are
worth what they say. Tickets come out at Nuka-World's own rates: a ticket every 100 at Atomic Rollers, every 2
points at the rest. They're Nuka-Cade tickets, so the Nuka-Cade's prize terminal takes them too. The weapon's lent
for the game (with ammo) if you haven't one, and you get back what you had out after. Each machine's own scoreboard
shows the game on it and your best at that game, kept in your save. X leaves a game early, paid for the score so far.

Every model, texture and sound is the game's own (Nuka-World's): the mod ships none. The physics (the balls' runs,
bounces and rims) is the mod's own, in plain C++ (`src/sim`), tested on its own like Playable Pool's.

**Status: 0.1.1.**

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
