// WarpQuake2 - instrumentation: per-frame phase timers, per-stage work
// counters, frame CRCs, the sampling profiler and the benchmark mode
// (WarpQuake's wq_prof, for Quake II's client, server and renderer).
//
// Plain C, no engine types: the client, the server and the renderer each
// include it next to their own headers.
//
// The timers read the E-clock at phase boundaries only, a few dozen reads a
// frame; in the hot loops there are only counter increments.  A read is not
// free (some microseconds), so the phase timers run only with -prof; the
// frame timer always runs, for the report's ms/frame.  Where a hot
// function's time is wanted, the sampling profiler (-sample) answers that
// without disturbing it.
//
// Command line (all with a timedemo, see q2prof.c):
//   -benchmark      quit when the timedemo ends; one "Q2BENCH ..." line on stdout
//   -prof           phase timers
//   -crc            CRC-32 of every frame shown (needs +set fixedtime 100)
//   -crcfile <f>    ... one line per frame into f
//   -crcdump <n>[,<n>...]  those frames as raw 8-bit pixels to RAM:frame<n>_<w>x<h>.raw
//   -crcdumpdir <d> where those go instead of RAM: (vamos has none: work:)
//   -crcref <f>     dump the first frame whose CRC differs from a -crcfile f
//   -sample         the sampling profiler over the timedemo (RAM:WarpQuake2.wprf)
//   -proflog <f>    where the report goes (RAM:WarpQuake2_prof.txt)

#ifndef Q2PROF_H
#define Q2PROF_H

#include "amiga_q2.h"

typedef enum {
  Q2P_FRAME,        // Qcommon_Frame
  Q2P_SERVER,       // SV_Frame: the game, and the demo read
  Q2P_CLIENT,       // CL_ReadPackets: parse the server's messages
  Q2P_PREDICT,      // CL_PredictMovement
  Q2P_SCREEN,       // SCR_UpdateScreen, all of it
  Q2P_CLVIEW,       // V_RenderView: entity lists, effects, and the 3D view
  Q2P_RENDER,       // R_RenderFrame
  Q2P_R_SETUP,      // R_SetupFrame, R_MarkLeaves, R_PushDlights
  Q2P_R_WORLD,      // R_RenderWorld: BSP walk, clipping, edge emission
  Q2P_R_BMODELS,    // R_DrawBEntitiesOnList: doors, lifts, ...
  Q2P_R_SCAN,       // R_ScanEdges: edge sort, span generation, and D_DrawSurfaces
  Q2P_D_SURFS,      // D_DrawSurfaces: texture mapping, z, sky, water
  Q2P_D_CACHE,      // building surface-cache blocks (cache misses only)
  Q2P_R_ENTS,       // R_DrawEntitiesOnList: alias models (the weapon too), sprites
  Q2P_R_PARTICLES,  // R_DrawParticles
  Q2P_R_ALPHA,      // R_DrawAlphaSurfaces: translucent surfaces
  Q2P_R_WARP,       // D_WarpScreen, underwater
  Q2P_R_PALETTE,    // R_CalcPalette: the damage/bonus flash
  Q2P_BLIT,         // SWimp_EndFrame: frame to the display
  Q2P_SOUND,        // S_Update
  Q2P_CRC,          // -crc checksumming: ours, not the game's
  Q2P_COUNT
} q2p_phase_t;

typedef enum {
  Q2C_SPAN_PIXELS,  // D_DrawSpans16: textured world pixels written
  Q2C_SPANS,        // ... spans
  Q2C_TURB_PIXELS,  // Turbulent8: water/slime/lava pixels
  Q2C_ZSPAN_PIXELS, // D_DrawZSpans: z-buffer words written
  Q2C_POLY_PIXELS,  // R_PolysetDrawSpans8: alias model pixels considered
  Q2C_CACHE_BUILDS, // surface-cache blocks built
  Q2C_CACHE_BYTES,  // ... and their size
  Q2C_PARTICLES,    // particles drawn
  Q2C_ALIAS_MODELS, // alias models drawn
  Q2C_ALIAS_VERTS,  // ... their vertices transformed
  Q2C_ALIAS_TRIS,   // ... their triangles not completely clipped
  Q2C_ALIAS_DRAWN,  // ... of those, unclipped and front-facing (to R_DrawTriangle)
  Q2C_ALIAS_CLIPPED,// ... and partly clipped (to R_AliasClipTriangle)
  Q2C_ALIAS_HIDDEN, // models skipped: the z-buffer covers all of their box
  Q2C_COUNT
} q2p_counter_t;

extern unsigned long q2p_ticks[Q2P_COUNT];
extern unsigned long q2p_calls[Q2P_COUNT];
extern unsigned long q2c_count[Q2C_COUNT];
extern int q2p_phases;  // -prof: the phase timers run
extern int q2p_crcon;   // -crc

#define Q2P_BEGIN(p) unsigned long q2p_t0_##p = q2p_phases ? qaTicks() : 0
#define Q2P_END(p) ((q2p_phases ? (q2p_ticks[p] += qaTicks() - q2p_t0_##p) : 0), q2p_calls[p]++)
// the frame timer: always on
#define Q2P_BEGIN_FRAME unsigned long q2p_t0_frame = qaTicks()
#define Q2P_END_FRAME (q2p_ticks[Q2P_FRAME] += qaTicks() - q2p_t0_frame, q2p_calls[Q2P_FRAME]++)
#define Q2C_ADD(c, n) (q2c_count[c] += (unsigned long)(n))

void Q2P_Init(void);           // after the command system: registers "prof", "profreset", "profsample"
void Q2P_FrameStart(void);     // runs a reset asked for mid-frame
void Q2P_FrameCRC(const unsigned char *buf, int width, int height, int rowbytes, int pixbytes);  // rowbytes in bytes
// -crcfile: what the renderer was given, by kind (0 entities, 1 particles, 2 lights, 3 view)
void Q2P_InputCRC(int kind, const void *data, int bytes);
void Q2P_ServerData(void);     // a server (or demo) starts: before anything it sends is parsed
void Q2P_TimedemoStart(void);  // the timedemo's first frame
int Q2P_TimedemoDone(int frames);  // -frames <n>: true once n frames are drawn
void Q2P_TimedemoEnd(int frames, int msec);  // ... and its end: report, samples, -benchmark quits

#endif
