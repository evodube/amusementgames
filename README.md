# Amusement Games

The Nuka-Cade's arcade games, playable in your settlements, in Fallout 4: build them from the workshop menu under
**Furniture > Amusement Games**, buy tokens at the **Token Stand** with caps, play, win tickets.

- **Atomic Rollers** (skee-ball): hold the left mouse button to charge, let go to roll; look where it goes. Nine
  balls over the hump and up the board, its ring walls funnelling them into the holes: 100 to 500 up the middle,
  700 in the top corners, nothing in the gutter or over the top.
- **Hoop Shot**: hold to charge, let go to lob it at the hoop, as many baskets as you can in 45 seconds.
- **Whac-a-Commie**: seven holes; swing the Commie Whacker at the commies popping up, for 30 seconds: a point is
  your mallet landing on a commie (its hit), not a click. The more Agility you have, the quicker you swing (3% a
  point past 1, up to twice as quick; both in the settings).
- **Nuka-Zapper Race**: six lanes; yours is the one with a Thirst Zapper lying on the counter, so stand behind it.
  Squirt your target with the Thirst Zapper: each hit sends your rocket a step up, ten and you've won (the other
  lanes race you). Unlimited ammo while you play.
- **Bandit Roundup**: shoot the cut-outs running along the gallery's two tracks with Far Harbor's Lever-Action Rifle
  (the Hunting Rifle without Far Harbor; iron sights, no scope), for 60 seconds. A point a hit, 3 for dead centre of a
  bullseye, either row. Unlimited ammo while you play: it never needs reloading.
- **Settings** in F4SE Menu Framework (Amusement Games > Whac-a-Commie, Settlers): the Agility swing (on/off, how much,
  with your Agility and swing speed shown live) and settlers playing (on/off, how often); kept in
  `Data\F4SE\Plugins\AmusementGames_user.ini`.
- **Settlers play too** (Bandit Roundup, for now): while you're about, now and then one of the settlement's settlers
  walks up to a free gallery, takes up the rifle and plays a round; the scoreboard shows their score as they shoot.
- **Takings**: each machine earns its settlement 1 cap a day for every settler there, put in the workshop when you're
  back (up to 30 days' worth). The rate is the global `AmusementGamesIncomePerSettler` (`set AmusementGamesIncomePerSettler to 2`).
- **Token Stand**: press F to buy 5 Nuka-Cade tokens (5 caps a token, 25 a press). Each game takes a token.

On screen while you play: just the clock (Atomic Rollers: the ball you're on; the zapper race: its countdown), and
the power meter while you charge a throw; Atomic Rollers also shows each ball's points as they come and a green aim
line up the lane to where you look (as fo4bowling's). At the end, the result. The machine's own scoreboard has the score.

A point a hit (a basket, a whack, a target, a step up the zapper race; a race won is 10 more); Atomic Rollers' holes are
worth what they say. Tickets come out at Nuka-World's own rates: a ticket every 100 at Atomic Rollers, every 2
points at the rest. They're Nuka-Cade tickets, so the Nuka-Cade's prize terminal takes them too. The weapon's lent
for the game (with ammo) if you haven't one, and you get back what you had out after. Each machine's own scoreboard
shows the game on it and your best at that game, kept in your save. X leaves a game early, paid for the score so far.

Each machine looks ready to play: Atomic Rollers' balls wait in its right-hand pocket (fewer as you play), Hoop
Shot's at the foot of its ramp, the Nuka-Cade's mallet rests on Whac-a-Commie, a Thirst Zapper lies on the zapper race's
counter in front of your lane and the rifle on the gallery's counter (not to be picked up: you're handed one to play).

Every model, texture and sound is the game's own (Nuka-World's): the mod ships none. The physics (the balls' runs,
bounces and rims) is the mod's own, in plain C++ (`src/sim`), tested on its own like Playable Pool's.

**Status: 0.4.3.**

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
