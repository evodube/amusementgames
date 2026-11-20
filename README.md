# Amusement Games

The Nuka-Cade's arcade games, playable in your settlements, in Fallout 4: build them from the workshop menu under
**Furniture > Amusement Games**, buy tokens at the **Token Stand** with caps, play, win tickets.

- **Atomic Rollers** (skee-ball): hold the left mouse button to charge, let go to roll; look where it goes. Nine
  balls over the hump and up the board, its ring walls funnelling them into the holes: 100 to 500 up the middle,
  700 in the top corners, nothing in the gutter or over the top. A ball that scores nothing (the gutter, back down
  the lane, stuck on the board) comes back and isn't one of the nine, up to nine a game.
- **Hoop Shot**: hold to charge, let go to lob it at the hoop, as many baskets as you can in 45 seconds.
- **Whac-a-Commie**: seven holes; swing the Commie Whacker at the commies popping up, for 30 seconds: a point is
  your mallet landing on a commie (its hit), not a click.
- **Nuka-Zapper Race**: six lanes; yours is the one with a Thirst Zapper lying on the counter, so stand behind it.
  Squirt your target with the Thirst Zapper: each hit sends your rocket a step up, ten and you've won (the other
  lanes race you). Unlimited ammo while you play.
- **Bandit Roundup**: shoot the cut-outs running along the gallery's two tracks with Far Harbor's Lever-Action Rifle
  (iron sights, no scope), for 60 seconds. 100 points a hit, 300 for dead centre of a
  bullseye, either row. Unlimited ammo while you play: it never needs reloading.
  Tap F to play alone; hold F for a 1v1 against a settler (**F) Play (hold: 1v1)**): your token in, a settler's
  called over and the game starts once they've the rifle up (activate it again while waiting to leave, token back).
  You both shoot at the same targets, any of them: whoever hits one first has its points. You're the left shooter if
  you're left of the counter's middle when you hold it, the right if right (near the middle, either); the settler
  takes the other side. Both scores on the machine's boards (the left shooter's on the left one, the right's on the right) and on screen; the winner gets 1000 more.
- **Settings** in F4SE Menu Framework (Amusement Games > Settlers, Income): settlers playing (on/off, how often, how
  often two play each other at Bandit Roundup, their skill), the takings and ticket rates; kept in
  `Data\F4SE\Plugins\AmusementGames_user.ini`.
- **Settlers play too**, one settler to each machine: Bandit Roundup (rifle to the shoulder; now and then two settlers
  play each other, one each side: how often in the settings), Whac-a-Commie (the
  Commie Whacker in hand, one-handed swings), Atomic Rollers (the red ball in hand, rolled with a low underhand
  throw) and Hoop Shot (the basketball in hand, a two-handed set shot): each throw one of three takes at random,
  the same hands on the ball, their stance, crouch, turn and lean their own, their score on the machine's board as they go; at Nuka-Zapper Race, settlers gather at the lanes (up to five: every lane but yours), and 10
  seconds after the second is ready they race. No racing on your own: put your token in and you wait for a settler
  to be ready (10 seconds after, you're off), racing only the settlers who came; the HUD shows who's racing and their
  rockets, and at the end whether you won or lost (and who won). Activate it again while waiting to leave, token back.
- **Settlers react**, with the game's own animations, a moment after a game: a new best of theirs or a place near the
  top of the high scores gets a cheer (the base game's crowd cheers, Nuka-World's wild ones), an ordinary game a nod,
  nothing scored or well under their best a head shake; a 1v1's or a race's winner cheers, the rest shake their heads.
  Up to three settlers about who aren't playing clap (or cheer) when someone wins a 1v1 or a race, sets a new top
  score, or when you set a new best.
- **Settlers' skill**: each settler has a talent of their own (rolled the first time they play, theirs for good), gets
  better the more they play a machine (about half the most practice gives after 10 games
  there, nearly all by 30), and has good and bad days; the Settlers page's skill setting shifts everyone (-50 to +50).
  Kept in your save.
- **Happiness**: each game machine adds +5 happiness to its settlement (as bars and stores add theirs; no settler or
  power needed).
- **Takings**: each machine earns its settlement 1 cap and 2 Nuka-Cade tickets a day for every settler there, put in
  the workbench when you're back (up to 30 days' worth). Both rates are on the Income page of the mod's settings
  (F4SE Menu Framework), or `[Income] fCapsPerSettler` / `fTicketsPerSettler` in `AmusementGames_user.ini`.
- **Tickets from playing**: each game's rate below, doubled by default; the multiplier is on the same Income page
  (`fPlayTicketsMult`, 0.5x to 5x).
- **Token Stand**: press F to buy 5 Nuka-Cade tokens (5 caps a token, 25 a press). Each game takes a token.
- **Prize Counter**: the Nuka-Cade's own prize terminal (no power needed), working as the Nuka-Cade's does. Its first
  screen welcomes you by name and shows your Current Tickets and Total Tickets Redeemed, then:
  - **Redeem Tickets**: puts all the tickets you carry into the counter (one account, shared by every counter).
  - **Spend Tickets**: Weapons, Ammo, Drinks, Souvenirs, paid from what you've redeemed; a prize you can't afford
    yet says "(not enough)".
  - **View High Scores**: each game's top 10 across all your settlements, you and your settlers alike (each one's
    best score; generic names like Settler, Provisioner, Wastelander, Raider or Protectron left off, so it's people
    with names of their own; anyone since deleted shows as "..." (the dead keep their names); the
    Nuka-Zapper Race's is most races won, every win scoring the same), and your Personal High Scores.
  Prices are the Nuka-Cade's own: an item's caps value x 50.
  - Weapons: Paddle Ball 500, Thirst Zapper 500, Commie Whacker 1250, Acid Soaker 6250
  - Ammo: Paddle Balls x10 500; Thirst Zapper ammo x20 (Water, Nuka-Cola, Nuka-Cherry, Nuka-Quantum) 1000 each;
    Acid Concentrate x20 2000; 7.62 Rounds x20 2000
  - Drinks (ice-cold Nuka-Cola): Orange, Grape, Wild 1000 each; Victory, Quartz 2000 each; Dark 2500
  - Souvenirs: Teddy Bear, Sloth Toy, Magnet Cow, 150 each

  The catalogue is `PRIZES` in `tools/make_esl.py`; after changing it, `python tools/make_esl.py --psc` rewrites the
  terminal's fragment script (`papyrus/AmusementGamesPrizeCounterFragments.psc`).

On screen while you play: just the clock (Atomic Rollers: the ball you're on; the zapper race: its countdown), and
the power meter while you charge a throw; Atomic Rollers also shows each ball's points as they come and a green aim
line up the lane to where you look (as fo4bowling's). At the end, the result. The machine's own scoreboard has the score.

Points in hundreds, as Nuka-World's machines count them: 100 a hit (a basket, a whack, a target, a step up the zapper
race; a race won is 1000 more); Atomic Rollers' holes are worth what they say. Tickets come out at Nuka-World's own rates, doubled (the Income page's multiplier): a ticket every 100 at Atomic Rollers,
every 200 at Hoop Shot and Whac-a-Commie, every 400 at the zapper race, and at Bandit Roundup by what you hit, as
Nuka-World's gallery pays (the bunny half a ticket, the pig 3/4, the duck 1, the star 1 1/4, the bottle 1 1/2; a
bullseye's more points, not more tickets). They're Nuka-Cade tickets, so the Nuka-Cade's prize terminal takes them too. The weapon's lent
for the game (with ammo) if you haven't one, and you get back what you had out after. Each machine's own scoreboard
shows the game on it and your best at that game, kept in your save. X leaves a game early, paid for the score so far.

**Controllers** work too: the games' fire button is the game's own attack (the left mouse button, or a controller's
right trigger, or whatever you've bound it to), you aim with the right stick as you look, Activate (A) starts a game
(hold it at Bandit Roundup for a 1v1), and holding Activate for a second while playing leaves (as X does).

Each machine looks ready to play: Atomic Rollers' balls wait in its right-hand pocket (fewer as you play), Hoop
Shot's at the foot of its ramp, the Nuka-Cade's mallet rests on Whac-a-Commie, a Thirst Zapper lies on the zapper race's
counter in front of your lane and the rifle on the gallery's counter (not to be picked up: you're handed one to play).

Every model, texture and sound is the game's own (Nuka-World's): the mod ships none. The physics (the balls' runs,
bounces and rims) is the mod's own, in plain C++ (`src/sim`), tested on its own like Playable Pool's.

**Status: 0.9.11.**

## Requirements

- Fallout 4 1.10.163 ("Old Gen"; also built for Next-Gen and Anniversary).
- The Nuka-World and Far Harbor DLCs (`DLCNukaWorld.esm` and `DLCCoast.esm` are masters: Far Harbor for Bandit
  Roundup's lever-action rifle).
- F4SE and Address Library for F4SE Plugins.
- [F4SE Menu Framework](https://www.nexusmods.com/fallout4/mods/105090) for the HUD (without it, results come as
  corner messages).

## Building

CI (`.github/workflows/build.yml`) runs the games' tests (`tests/games_test.cpp`), builds the plugin with xmake
against Dear-Modding CommonLibF4 (`lib/commonlibf4`), compiles the token stand's script with Caprica and writes the
.esl with `python tools/make_esl.py`. Each push to `main` publishes the `Data` folder to the `builds`
branch.
