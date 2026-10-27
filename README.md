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

## Building

```
make -j8            # build/WarpQuake2 (and copy to TARGET_DIR)
make app            # build only
make prof           # build/WarpQuake2_sym, unstripped, plus a link map
make ftp            # upload to WORK:Games/WarpQuake2 (AMIGA_FTP_HOST/USER/PASSWORD)
```

vbcc, `-cpu=68060 -fpu=68060 -O1` with `-lm060`, as WarpQuake (vbcc 0.9h miscompiles some loops
at -O2). It needs vbcc and vasm on the PATH, the AmigaOS 3.2 NDK (`NDK_INC_C`) and the
Picasso96 developer headers (`P96_SDK`), all overridable on the `make` command line.

## Running

Put `WarpQuake2` next to a `baseq2` drawer holding the paks, or point it at an existing
installation and keep its own settings in a game directory of their own:

```
WarpQuake2 +set basedir Work:Games/Quake2 +set game warpq2
```

`timedemo demo1` at 320x240x8, 68060 at 104 MHz, v0 (id's C renderer, no sound): 7.9 fps.

```
WarpQuake2 +set sw_mode 0 +set timedemo 1 +demomap demo1.dm2
```

## Source layout

- `quake2/` - id's engine. Changes so far: what vbcc needs (field offsets with `offsetof`,
  linkage that matches its declarations, `strdup`/`putenv` declared), the base directory
  joined without a `/` after a `:` (`PROGDIR:baseq2`), and the game's `maxclients` and
  `dedicated` renamed, as the engine has globals of those names.
- `src/` - the AmigaOS platform layer. `amiga_os.c`, `amiga_screen.c` and `amiga_input.c`
  include the OS headers; `sys_amiga.c`, `vid_amiga.c`, `swimp_amiga.c`, `in_amiga.c` and
  `net_loop.c` include the engine's. They meet only in `amiga_q2.h`.
