# WarpQuake2

Quake II for AmigaOS 3.x on a 68060 with an RTG (Picasso96) screen, written for the Warp
accelerators and meant to be fast on any 68060. The sibling of
[WarpQuake](https://github.com/andrzej79/WarpQuake).

The engine in `quake2/` is id Software's GPL release of Quake II (version 3.19, protocol 34,
which plays the 3.20 retail data), the directories this port needs, imported unchanged (the
first commit). Every change since is an ordinary diff. The game code and the software renderer
are linked into the program (`GAME_HARD_LINKED`, `REF_HARD_LINKED`) instead of loaded as DLLs.

The renderer's hot paths are 68060 assembly. Most of it is WarpQuake's, because Quake II's
software renderer is Quake's with few changes: the span, z-span and surface-cache drawers, the
alias model rasterizer, the edge-list scan, edge clipping, the face and BSP walks. The rest is
new: the alias vertex transform, the alias triangle loop and the particle drawer. Every asm
routine draws exactly what the C beside it draws, frame for frame (see *Checking*).

It runs on an 8-bit CLUT or a 16-bit RGB screen (`vid_bpp 16`). At 16 bpp the lighting is
computed per colour instead of rounded to the 256-colour palette.

WarpQuake2 is GPL v2, as id's code is (`LICENSE`). Quake II's game data is not included: copy
your own `baseq2/pak*.pak` next to the program.

## Speed

`timedemo demo1`, 320x240x8, 68060 at 104 MHz (Warp1260X, L2 early read ack on), no sound
(the default sound, below, adds about 1.8 ms a frame):

| build | ms/frame | fps |
|---|---:|---:|
| v0: id's C, built with vbcc | 127.99 | 7.81 |
| now, 8 bpp | 79.98 | 12.50 |
| now, 16 bpp (`vid_bpp 16`) | 87.86 | 11.38 |
| now, 8 bpp, HUD on a black bar (`scr_hudbar 1`, the default; below) | 75.89 | 13.18 |

The steps, each measured on its own:

| change | ms/frame after |
|---|---:|
| world spans in asm (Quake's `D_DrawSpans8`, which is Quake II's `D_DrawSpans16`) | 120.37 |
| surface-cache blocks in asm | 112.10 |
| alias span drawer and left-edge walk in asm | 109.26 |
| z spans in asm | 108.31 |
| alias triangle setup and gradients in asm | 107.04 |
| edge-list scan in asm | 101.94 |
| world vertex projection cache, edge clipping in asm | 100.50 |
| `R_RenderFace` in asm | 100.23 |
| alias: hidden triangles skipped early, light per normal | 99.58 |
| spans perspective-correct every 16 pixels (`sw_subdiv16 1`) | 93.56 |
| alias vertex transform in asm | 93.21 |
| entities sorted without vbcc's `qsort` | 91.63 |
| light recursions as loops | 91.11 |
| world BSP walk in asm | 90.31 |
| alias triangle loop in asm | 88.87 |
| HUD picture lookups cached, dead `skintable` rebuild dropped | 88.46 |
| particles in asm | 88.09 |
| light styles and particles added inline | 87.76 |
| frame copied to the screen with MOVE16 (`vid_blit -1` on a Warp) | 86.33 |
| a surface marked by a dynamic light rebuilt only if the light reaches it | 84.00 |
| alias models the z-buffer already hides skipped (`r_aliascull`, below) | 79.98 |

`sw_subdiv16 1` (the default) computes s and t exactly every 16 pixels, as id's x86 code did;
id's C did it every 8 (its own FIXME says so). `sw_subdiv16 0` is the C's 8 again.

What is left is mostly memory. On the Warp a load that misses the 060's 8 KB data cache costs
about 100 ns when the FPGA's 96 KB L2 has it and 600-800 ns when it comes from DDR3; the
frame (77 KB), the z-buffer (150 KB) and the surface cache's texels do not fit. Byte stores
to fast RAM run at about 21 ns, longwords at 15 ns a byte; to the RTG screen bytes take 67 ns,
so rendering straight into it would cost more than the 2.1 ms `WriteChunkyPixels` takes.

`r_aliascull 1` (the default) skips an alias model when every z-buffer entry in the screen
rectangle of its box is nearer than the box's nearest point: the drawers' own per-pixel test,
so nothing they would have drawn is lost, and the CRCs are unchanged. Quake II draws every model
in the potentially visible set, and in demo1 about one in five is behind a wall.

Tried and dropped: WarpQuake's z coverage (write z only where something reads it). Quake II's
weapon and particles cover so much of the view that it cut the z writes by a quarter, 1.3 ms,
for a pass that cost 2.4 ms.

## Building

```
make -j8            # build/WarpQuake2 (and copy to TARGET_DIR)
make app            # build only
make app ASM=0      # build/WarpQuake2_C: the all-C reference
make prof           # build/WarpQuake2_sym, unstripped, plus a link map
make ftp            # upload to WORK:Games/WarpQuake2 (AMIGA_FTP_HOST/USER/PASSWORD)
```

vbcc, `-cpu=68060 -fpu=68060 -O1` with `-lm060`, as WarpQuake (vbcc 0.9h miscompiles some loops
at -O2), and vasm. It needs vbcc and vasm on the PATH, the AmigaOS 3.2 NDK (`NDK_INC_C`), the
Picasso96 developer headers (`P96_SDK`) and AHI's (`AHI_SDK`, the ahidev archive), all
overridable on the `make` command line.

## Running

Put `WarpQuake2` next to a `baseq2` drawer holding the paks, or point it at an existing
installation and keep its own settings in a game directory of their own:

```
WarpQuake2 +set basedir Work:Games/Quake2 +set game warpq2
```

**Video options** (Options, Video): the mode, the colour depth, the screen size, the
brightness and the HUD bar, set on "apply". The modes are id's `sw_mode` 0-4 (320x240,
400x300, 512x384, 640x480, 800x600) and 5, 640x360; nothing larger, which a 68060 cannot render
at a playable rate. A mode the RTG board does not have exactly is centred on the nearest
larger one.

**`scr_hudbar`** (saved, default 1) stops the 3D view above the status bar's row (health, ammo,
armour) and fills that row black: 24 lines fewer to render, about 4 ms a frame at 320x240.
`scr_hudbar 0` draws the status bar over the view, as id's game does.

**16 bpp.** `vid_bpp 16` (saved; `+set vid_bpp 16` on the command line) opens a 16-bit RTG
mode instead of a CLUT one, falling back to 8 bits if there is none. The renderer then draws
RGB565 pixels: the lighting goes through a 16-bit colormap built with the formula id's
`colormap.pcx` was made with (brightness 2 - 2l/63), without rounding each lit colour to the
nearest of the 256, so it is smooth and keeps its hue. The surface cache, the sky, the
warping water and the sprites hold pixels; skins and pictures stay palette indexes, looked up
as they are drawn. The translucent surfaces, shells and particles are mixed per channel
(33/66%, as the 8-bit alphamap). The palette blend (damage and bonus flashes, under water,
powerups), which an 8-bit screen gets from its CLUT, is blended into the frame as it is
copied to the screen. The frame is big-endian RGB565, the Warp's 16-bit format; another
16-bit format gets it through `p96WritePixelArray`, which converts. `screenshot` is 8-bit only.

**`vid_blit`** (saved) picks how the frame reaches the screen: `0` WriteChunkyPixels (8 bpp) or
p96WritePixelArray (16), `1` the bitmap locked and the rows copied with CopyMemQuick, `2` the
same with MOVE16 bursts. `-1`, the default, is `2` on a Warp board (its RTG memory is uncached
and built for bursts: 1.46 ms a 320x240x8 frame against WriteChunkyPixels' 2.41) and `0` on any
other, since MOVE16 into Zorro III memory fails on some machines.

**Sound** goes through AHI: Quake II mixes into a ring that AHI plays as one looping
sample. `s_ahimode` (saved) is the AHI audio mode, by default the Warp's "16 bit mono"
(`0x00570004`), and AHI's default mode where that is not there (`0`: always AHI's default
mode); the ring is mono when the mode is, so Quake II writes half as much. `s_khz` (Quake II's,
default 11) is the rate: 11, 22 or 44 kHz. Both take effect at `snd_restart`. At the defaults
sound costs about 1.8 ms a frame (1.0 in Quake II's mixer, the rest in AHI's).

Engine bug fixed here: id's C mixer took the volume row of its scale table as `vol >> 11`
(always row 0, all zeros) where its x86 code used `vol >> 3`, so every 8-bit sound - all of
them, at the default `s_loadas8bit 1` - was silent (`snd_mix.c`). And `S_LoadSound` allocated
each sound at its file's sample width and channels, though it stores mono, one byte a sample
at `s_loadas8bit 1`: half of every 16-bit sound was never used, 3.6 MB at 22 kHz in base1
(`snd_mem.c`).

**Savegames** are written field by field, big-endian, with function pointers and monster
moves by name (`game/g_save.c`), so a save loads in any later build and on any CPU. id's
wrote each struct as the compiler laid it out and a function as its offset from `InitGame`,
checked only against the build date: a save from an earlier build of the same day jumped
into the wrong code. The name table, `game/g_savetab.c`, is generated from the game's sources
by `tools/gen_savetab.py` - run it after adding or renaming a game function or monster move
(`--check` says whether it is current). Saves from before this format are refused with a
message.

A timedemo (in the console: `timedemo 1`, then `demomap demo1.dm2`):

```
WarpQuake2 +set sw_mode 0 +set timedemo 1 +demomap demo1.dm2
```

## Profiling and benchmarks

Options (before the `+` commands):

| option | |
|---|---|
| `-benchmark` | quit when the timedemo ends, printing one `Q2BENCH frames=... ms_per_frame=... fps=... crc=...` line |
| `-prof` | phase timers: the report breaks the frame down (client, server, BSP, edges, surfaces, alias models, ...) |
| `-sample` | the on-machine sampling profiler over the timedemo, to `RAM:WarpQuake2.wprf` |
| `-crc` | a CRC-32 of every frame; `-crcfile <f>` one per frame, `-crcdump <n>[,<n>...]` raw frames, `-crcref <f>` dump the first that differs |
| `-headless` | no screen or input, for vamos |
| `-frames <n>` | end the timedemo after n frames |

With `+set fixedtime 100` every rendered frame advances one 100 ms server frame, so a
timedemo draws the same frames at any speed. The report goes to the console and
`RAM:WarpQuake2_prof.txt`.

`tools/bench.py` runs a timedemo on the Amiga through a remote-shell command (named by
`AMIGA_RUNNER` in `tools/ftp_config.local`; see the script), and returns as soon as it ends:
`--runs N`, `--bpp 16`, `--sound` (off otherwise), `--prof`, `--sample` (fetched and summarised with `tools/wprof.py`; build with
`make prof` first), `--crc`. Before each run it puts `tools/bench_config.cfg` into the game
directory, so every run starts from the defaults. `tools/pchist.py` turns a sample profile into
a per-instruction histogram of one asm function.

## Checking

`tools/vamos_crc.sh [binary] [options]` runs the timedemo headless under vamos (amitools) on the
host, in about 40 s: no task switches, no interrupts, and 80-bit floats computed exactly as the
060 does, so its CRCs are exactly reproducible (`DEMO=demo2` for the other demo). `ASM=0` and
`ASM=1` must print the same CRC. Now: `0380799c` (demo1, 320x240, `sw_subdiv16 1`) and
`2d5bb708` (demo2), with `scr_hudbar 0`, which the script sets; `sw_subdiv16 0` gives a different one, again the same for both builds.

**16 bpp** (`+set vid_bpp 16`) is checked with `-rgbtest`: the colour tables then hold the
8-bit renderer's palette indexes and the blends go through its alphamap, so the low byte of
each 16-bit pixel is the 8-bit frame, and `-crc`, which then hashes only those bytes, must
print the 8-bit references above. That checks every 16-bit drawer against the 8-bit renderer;
the `ASM=1` build does. In real colour demo1 gives `b13305d2`. The `ASM=0` build differs by a
pixel in some frames at 16 bpp, because vbcc rounds the floats of the C copies of the span
drawers differently (register allocation); the asm is the 8-bit code assembled again with
`PIX16`. At 16 bpp `-crcdump` also writes the pixels (`frame<n>_<w>x<h>.rgb565`), which
`tools/rgb565png.py` turns into a PNG.

On the bench 1260X the same build differs from run to run in some 10-20 frames of 689, by a
pixel or a float's last bit, even with interrupts off and with L2 early ack off; vamos does not.
The hardware agrees with vamos on about 92% of the frames.

Engine bugs fixed on the way: an instant particle's time was read uninitialised
(`cl_fx.c`); the z-span pairs were packed for little-endian (`r_scan.c`).

## Source layout

- `quake2/` - id's engine. `ref_soft/r_rgb.c` holds the 16 bpp tables; `*060rgb.s` assemble
  the drawers again with `PIX16`. The changes: what vbcc needs (field offsets with `offsetof`, linkage
  that matches its declarations, `strdup`/`putenv` declared), the base directory joined
  without a `/` after a `:` (`PROGDIR:baseq2`), the game's `maxclients` and `dedicated`
  renamed apart from the engine's, the instrumentation hooks, the portable savegames, and
  the renderer work above.
  `quake2/ref_soft/*060.s` are the 68060 routines; `Q2_ASM` (the Makefile's `ASM`) chooses
  them over the C.
- `src/` - the AmigaOS platform layer. `amiga_os.c`, `amiga_screen.c`, `amiga_input.c`,
  `amiga_ahi.c` and `q2sample.c` include the OS headers; `sys_amiga.c`, `vid_amiga.c`,
  `swimp_amiga.c`, `snddma_amiga.c`, `in_amiga.c`, `net_loop.c` and `q2prof.c` include the
  engine's. They meet only in
  `amiga_q2.h` and `q2prof.h`.
- `tools/` - uploads, downloads, the benchmark and CRC drivers, the profile readers.
