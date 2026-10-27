# WarpQuake2

Quake II for AmigaOS 3.x on a 68060 with an RTG (Picasso96) screen, written for the Warp
accelerators and meant to be fast on any 68060. The sibling of
[WarpQuake](https://github.com/andrzej79/WarpQuake).

The engine in `quake2/` is id Software's GPL release of Quake II (version 3.19, protocol 34,
which plays the 3.20 retail data), the directories this port needs, imported unchanged (the
first commit). Every change since is an ordinary diff. The game code and the software renderer
are linked into the program (`GAME_HARD_LINKED`, `REF_HARD_LINKED`) instead of loaded as DLLs.

WarpQuake2 is GPL v2, as id's code is (`LICENSE`). Quake II's game data is not included: copy
your own `baseq2/pak*.pak` next to the program.

## Roadmap

- **v0**: runs at all. 320x240x8, null sound, single player (loopback network only).
- **v1**: instrumentation, as in WarpQuake: phase timers, the sampling profiler, frame CRCs.
- **v2**: optimise what v1 measures, starting from WarpQuake's 68060 renderer routines.
- Later: sound through AHI, more screen modes, the mission packs.
