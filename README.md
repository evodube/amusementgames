# Amusement Games

The Nuka-Cade's arcade games for your settlements, in Fallout 4: build them from the workshop menu under
**Furniture > Amusement Games**.

- **Whac-a-Commie**
- **Nuka-Zapper Race**
- **Atomic Rollers**
- **Hoop Shot**
- **Bandit Roundup**
- **Token Dispenser** (pre-war money in, Nuka-Cade tokens out: the machines take tokens)

No meshes of its own: every machine is Nuka-World's own model. Unlike Nuka-World's own recipes
(Decorations > Nuka-World > Nuka-Cade, after the first Nuka-World quest), these are there from the start.

**Status: 0.1.0.** The machines are Nuka-World's own for now; each game is to be revamped in turn.

## Requirements

- Fallout 4 1.10.163 ("Old Gen"; also built for Next-Gen and Anniversary).
- The Nuka-World DLC (`DLCNukaWorld.esm` is a master).
- F4SE and Address Library for F4SE Plugins (the plugin puts the Amusement Games category in the Furniture menu).

## Building

CI (`.github/workflows/build.yml`) builds the plugin with xmake against Dear-Modding CommonLibF4 (`lib/commonlibf4`)
and writes the .esl with `python tools/make_esl.py`. Each push to `main` publishes the `Data` folder to the `builds`
branch.
