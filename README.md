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

WarpQuake2 is GPL v2, as id's code is (`LICENSE`). Quake II's game data is not included: copy
your own `baseq2/pak*.pak` next to the program.

## Speed

`timedemo demo1`, 320x240x8, 68060 at 104 MHz (Warp1260X, L2 early read ack on), no sound:

| build | ms/frame | fps |
|---|---:|---:|
| v0: id's C, built with vbcc | 127.99 | 7.81 |
| now | 87.76 | 11.39 |

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

`sw_subdiv16 1` (the default) computes s and t exactly every 16 pixels, as id's x86 code did;
id's C did it every 8 (its own FIXME says so). `sw_subdiv16 0` is the C's 8 again.

What is left is mostly memory. On the Warp a load that misses the 060's 8 KB data cache costs
about 100 ns when the FPGA's 96 KB L2 has it and 600-800 ns when it comes from DDR3; the
frame (77 KB), the z-buffer (150 KB) and the surface cache's texels do not fit. Byte stores
to fast RAM run at about 21 ns, longwords at 15 ns a byte; to the RTG screen bytes take 67 ns,
so rendering straight into it would cost more than the 2.1 ms `WriteChunkyPixels` takes.

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
at -O2), and vasm. It needs vbcc and vasm on the PATH, the AmigaOS 3.2 NDK (`NDK_INC_C`) and the
Picasso96 developer headers (`P96_SDK`), all overridable on the `make` command line.

## Running

Put `WarpQuake2` next to a `baseq2` drawer holding the paks, or point it at an existing
installation and keep its own settings in a game directory of their own:

```
WarpQuake2 +set basedir Work:Games/Quake2 +set game warpq2
```

A timedemo:

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
`AMIGA_RUNNER` in `tools/ftp_config.local`; see the script), and returns as soon as it ends: `--runs N`, `--prof`, `--sample` (fetched and summarised with `tools/wprof.py`; build with
`make prof` first), `--crc`. Before each run it puts `tools/bench_config.cfg` into the game
directory, so every run starts from the defaults. `tools/pchist.py` turns a sample profile into
a per-instruction histogram of one asm function.

## Checking

`tools/vamos_crc.sh [binary]` runs the timedemo headless under vamos (amitools) on the host, in
about 40 s: no task switches, no interrupts, and 80-bit floats computed exactly as the 060 does,
so its CRCs are exactly reproducible. `ASM=0` and `ASM=1` must print the same CRC. Now:
`0380799c` (demo1, 320x240, `sw_subdiv16 1`); `sw_subdiv16 0` gives a different one, again the
same for both builds.

On the bench 1260X the same build differs from run to run in some 10-20 frames of 689, by a
pixel or a float's last bit, even with interrupts off and with L2 early ack off; vamos does not.
The hardware agrees with vamos on about 92% of the frames.

Engine bugs fixed on the way: an instant particle's time was read uninitialised
(`cl_fx.c`); the z-span pairs were packed for little-endian (`r_scan.c`).

## Source layout

- `quake2/` - id's engine. The changes: what vbcc needs (field offsets with `offsetof`, linkage
  that matches its declarations, `strdup`/`putenv` declared), the base directory joined
  without a `/` after a `:` (`PROGDIR:baseq2`), the game's `maxclients` and `dedicated`
  renamed apart from the engine's, the instrumentation hooks, and the renderer work above.
  `quake2/ref_soft/*060.s` are the 68060 routines; `Q2_ASM` (the Makefile's `ASM`) chooses
  them over the C.
- `src/` - the AmigaOS platform layer. `amiga_os.c`, `amiga_screen.c`, `amiga_input.c` and
  `q2sample.c` include the OS headers; `sys_amiga.c`, `vid_amiga.c`, `swimp_amiga.c`,
  `in_amiga.c`, `net_loop.c` and `q2prof.c` include the engine's. They meet only in
  `amiga_q2.h` and `q2prof.h`.
- `tools/` - uploads, downloads, the benchmark and CRC drivers, the profile readers.
