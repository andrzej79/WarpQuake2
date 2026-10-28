// WarpQuake2 - instrumentation, see q2prof.h

#include "../quake2/qcommon/qcommon.h"
#include "q2prof.h"

unsigned long q2p_ticks[Q2P_COUNT];
unsigned long q2p_calls[Q2P_COUNT];
unsigned long q2c_count[Q2C_COUNT];
int q2p_phases;
int q2p_crcon;

// -crc: a CRC-32 over every frame shown.  With +set fixedtime 100 every
// frame advances one server frame, so a timedemo renders the same frames on
// any machine at any speed: two builds that print the same CRC drew the
// same pixels.
static unsigned long crcTable[256];
static unsigned long crcAll;           // CRC of the frames' CRCs
static unsigned long crcFrames;
static FILE *crcFile;                  // -crcfile: one line per frame
#define MAX_DUMPS 16
static unsigned long crcDump[MAX_DUMPS]; // -crcdump <n>[,<n>...]: those frames to files
static int numDumps;
static unsigned long inputCRC[4];      // Q2P_InputCRC's, this frame
static FILE *crcRef;                   // -crcref <f>: a -crcfile to compare with

static qboolean resetPending;
static qboolean measuring;             // between the timedemo's start and end
static qboolean sampling;
static qboolean benchmark;
static double timerCostTicks;          // one qaTicks() call, in ticks
static int frameWidth, frameHeight;

extern cvar_t *fixedtime;              // common.c

#define DEFAULT_LOG "RAM:WarpQuake2_prof.txt"
#define DEFAULT_SAMPLES "RAM:WarpQuake2.wprf"

// The report tree: depth is the indent.  A row with a "minus" list is
// derived: its own phase minus the listed ones (what is left inside it).
typedef struct {
  int depth;
  const char *name;
  int phase;
  int minus[10];                       // up to 10; -1 ends a shorter list
} q2p_row_t;

#define NONE {-1}

static const q2p_row_t rows[] = {
  {0, "server (game + demo read)", Q2P_SERVER, NONE},
  {0, "client (parse messages)", Q2P_CLIENT, NONE},
  {0, "client prediction", Q2P_PREDICT, NONE},
  {0, "screen", Q2P_SCREEN, NONE},
  {1, "view (V_RenderView)", Q2P_CLVIEW, NONE},
  {2, "entities, effects, lights", Q2P_CLVIEW, {Q2P_RENDER, -1}},
  {2, "3D view (R_RenderFrame)", Q2P_RENDER, NONE},
  {3, "setup, leaves, dlights", Q2P_R_SETUP, NONE},
  {3, "world BSP + edges", Q2P_R_WORLD, NONE},
  {3, "brush models", Q2P_R_BMODELS, NONE},
  {3, "scan edges (all)", Q2P_R_SCAN, NONE},
  {4, "edge sort + span gen", Q2P_R_SCAN, {Q2P_D_SURFS, -1}},
  {4, "draw surfaces", Q2P_D_SURFS, NONE},
  {5, "surface cache build", Q2P_D_CACHE, NONE},
  {5, "spans, z, sky, water", Q2P_D_SURFS, {Q2P_D_CACHE, -1}},
  {3, "alias models + sprites", Q2P_R_ENTS, NONE},
  {3, "particles", Q2P_R_PARTICLES, NONE},
  {3, "translucent surfaces", Q2P_R_ALPHA, NONE},
  {3, "underwater warp", Q2P_R_WARP, NONE},
  {3, "palette (flashes)", Q2P_R_PALETTE, NONE},
  {3, "rest of R_RenderFrame", Q2P_RENDER,
   {Q2P_R_SETUP, Q2P_R_WORLD, Q2P_R_BMODELS, Q2P_R_SCAN, Q2P_R_ENTS, Q2P_R_PARTICLES, Q2P_R_ALPHA, Q2P_R_WARP,
    Q2P_R_PALETTE, -1}},
  {1, "2D: HUD, console, menu", Q2P_SCREEN, {Q2P_CLVIEW, Q2P_BLIT, Q2P_CRC, -1}},
  {1, "blit to display", Q2P_BLIT, NONE},
  {1, "frame CRC (-crc)", Q2P_CRC, NONE},
  {0, "sound", Q2P_SOUND, NONE},
  {0, "rest of the frame", Q2P_FRAME, {Q2P_SERVER, Q2P_CLIENT, Q2P_PREDICT, Q2P_SCREEN, -1}},
};

static void CRCInit(void)
{
  unsigned long c;
  int i, k;

  for(i = 0; i < 256; i++) {
    c = i;
    for(k = 0; k < 8; k++) {
      c = (c & 1) ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
    }
    crcTable[i] = c;
  }
}

static const char *ArgValue(const char *name, const char *def);

static unsigned long CRCUpdate(unsigned long crc, const unsigned char *p, int n)
{
  while(n-- > 0) {
    crc = crcTable[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
  }
  return crc;
}

void Q2P_InputCRC(int kind, const void *data, int bytes)
{
  int i;

  inputCRC[kind] = CRCUpdate(inputCRC[kind], (const unsigned char *)data, bytes);
  // -crcdump: the input of a dumped frame too, raw
  for(i = 0; i < numDumps && crcDump[i] != crcFrames + 1; i++) {
  }
  if(measuring && i < numDumps) {
    FILE *f = fopen(va("%sin%lu_%d.bin", ArgValue("-crcdumpdir", "RAM:"), crcFrames + 1, kind), "ab");

    if(f) {
      fwrite(data, 1, bytes, f);
      fclose(f);
    }
  }
}

void Q2P_FrameCRC(const unsigned char *buf, int width, int height, int rowbytes)
{
  unsigned long crc = 0xFFFFFFFFUL;
  unsigned char be[4];
  int y;

  frameWidth = width;
  frameHeight = height;
  // only the timedemo's frames: loading screens depend on the disk's speed
  if(!q2p_crcon || !measuring) {
    return;
  }
  for(y = 0; y < height; y++) {
    crc = CRCUpdate(crc, buf + y * rowbytes, width);
  }
  crc ^= 0xFFFFFFFFUL;

  be[0] = crc >> 24;
  be[1] = crc >> 16;
  be[2] = crc >> 8;
  be[3] = crc;
  crcAll = CRCUpdate(crcAll, be, 4);
  crcFrames++;
  if(crcFile) {
    fprintf(crcFile, "%lu %08lx  ents %08lx part %08lx dl %08lx view %08lx\n", crcFrames, crc, inputCRC[0],
            inputCRC[1], inputCRC[2], inputCRC[3]);
  }
  inputCRC[0] = inputCRC[1] = inputCRC[2] = inputCRC[3] = 0;

  // the first frame that differs from the reference run is dumped
  if(crcRef) {
    unsigned long n, ref;

    if(fscanf(crcRef, "%lu %lx", &n, &ref) == 2 && n == crcFrames && ref != crc) {
      Com_Printf("crcref: frame %lu differs (%08lx, reference %08lx)\n", crcFrames, crc, ref);
      if(numDumps < MAX_DUMPS) {
        crcDump[numDumps++] = crcFrames;
      }
      fclose(crcRef);
      crcRef = NULL;
    }
  }

  // the raw 8-bit frame, to find what differs where two CRCs disagree
  for(y = 0; y < numDumps && crcDump[y] != crcFrames; y++) {
  }
  if(y < numDumps) {
    FILE *f = fopen(va("%sframe%lu_%dx%d.raw", ArgValue("-crcdumpdir", "RAM:"), crcFrames, width, height), "wb");

    if(f) {
      for(y = 0; y < height; y++) {
        fwrite(buf + y * rowbytes, 1, width, f);
      }
      fclose(f);
    }
  }
}

static void Reset(void)
{
  memset(q2p_ticks, 0, sizeof(q2p_ticks));
  memset(q2p_calls, 0, sizeof(q2p_calls));
  memset(q2c_count, 0, sizeof(q2c_count));
  crcAll = 0xFFFFFFFFUL;
  crcFrames = 0;
  if(crcFile) {
    rewind(crcFile);
  }
}

// What one timer read costs, so the report can say how much it disturbs.
static void CalibrateTimer(void)
{
  unsigned long t0, t1;
  int i;

  t0 = qaTicks();
  for(i = 0; i < 1000; i++) {
    qaTicks();
  }
  t1 = qaTicks();
  timerCostTicks = (double)(t1 - t0) / 1000.0;
}

static double PhaseTicks(const q2p_row_t *r)
{
  double t;
  int i;

  t = (double)q2p_ticks[r->phase];
  for(i = 0; i < 10 && r->minus[i] >= 0; i++) {
    t -= (double)q2p_ticks[r->minus[i]];
  }
  // "rest of the frame" also leaves out the sound
  if(r->phase == Q2P_FRAME) {
    t -= (double)q2p_ticks[Q2P_SOUND];
  }
  return t;
}

static void Out(FILE *f, char *fmt, ...)
{
  va_list argptr;
  char text[256];

  va_start(argptr, fmt);
  vsnprintf(text, sizeof(text), fmt, argptr);
  va_end(argptr);
  Com_Printf("%s", text);
  if(f) {
    fputs(text, f);
  }
}

static void Report(const char *logpath)
{
  FILE *f = NULL;
  unsigned long frames, reads;
  double rate, frameMs, msPerTick, t, pixels;
  const q2p_row_t *r;
  int i;

  frames = q2p_calls[Q2P_FRAME];
  if(!frames) {
    Com_Printf("prof: no frames measured\n");
    return;
  }
  if(logpath) {
    f = fopen(logpath, "w");
  }

  rate = (double)qaTickRate();
  msPerTick = 1000.0 / rate;
  frameMs = (double)q2p_ticks[Q2P_FRAME] * msPerTick / frames;
  pixels = (double)frameWidth * frameHeight;

  Out(f, "---- WarpQuake2 profile: %lu frames, %.2f ms/frame (%.2f fps) ----\n", frames, frameMs, 1000.0 / frameMs);
  Out(f, "frame %dx%d, %s\n", frameWidth, frameHeight, qaScreenInfo());
  if(!q2p_phases) {
    Out(f, "(phase timers off: run with -prof for the breakdown)\n");
  } else {
    Out(f, "%-32s %8s %6s %7s\n", "phase", "ms/frm", "%", "calls");
  }
  for(r = rows; q2p_phases && r < rows + sizeof(rows) / sizeof(rows[0]); r++) {
    char name[48];

    t = PhaseTicks(r) * msPerTick / frames;
    sprintf(name, "%*s%s", r->depth * 2, "", r->name);
    if(r->minus[0] >= 0 || r->phase == Q2P_FRAME) {
      Out(f, "%-32s %8.2f %5.1f%%\n", name, t, 100.0 * t / frameMs);
    } else {
      Out(f, "%-32s %8.2f %5.1f%% %7.1f\n", name, t, 100.0 * t / frameMs, (double)q2p_calls[r->phase] / frames);
    }
  }

  Out(f, "-- work per frame\n");
  Out(f, "world span pixels   %8.0f  (%.2fx the frame)\n", (double)q2c_count[Q2C_SPAN_PIXELS] / frames,
      pixels > 0 ? (double)q2c_count[Q2C_SPAN_PIXELS] / frames / pixels : 0.0);
  Out(f, "world spans         %8.0f  (%.1f pixels each)\n", (double)q2c_count[Q2C_SPANS] / frames,
      q2c_count[Q2C_SPANS] ? (double)q2c_count[Q2C_SPAN_PIXELS] / q2c_count[Q2C_SPANS] : 0.0);
  Out(f, "z-buffer words      %8.0f\n", (double)q2c_count[Q2C_ZSPAN_PIXELS] / frames);
  Out(f, "water/lava pixels   %8.0f\n", (double)q2c_count[Q2C_TURB_PIXELS] / frames);
  Out(f, "alias span pixels   %8.0f  (tested against z; not all are written)\n",
      (double)q2c_count[Q2C_POLY_PIXELS] / frames);
  Out(f, "alias models        %8.1f\n", (double)q2c_count[Q2C_ALIAS_MODELS] / frames);
  Out(f, "surface cache       %8.1f blocks, %.1f KB built\n", (double)q2c_count[Q2C_CACHE_BUILDS] / frames,
      q2c_count[Q2C_CACHE_BYTES] / 1024.0 / frames);
  Out(f, "particles           %8.1f\n", (double)q2c_count[Q2C_PARTICLES] / frames);

  reads = 2 * frames;
  if(q2p_phases) {
    for(i = 0; i < Q2P_COUNT; i++) {
      if(i != Q2P_FRAME) {
        reads += 2 * q2p_calls[i];
      }
    }
  }
  Out(f, "-- timer: %.0f Hz, %.2f us/read, %.1f reads/frame = %.3f ms/frame (%.2f%%) of overhead\n", rate,
      timerCostTicks * 1000.0 * msPerTick, (double)reads / frames, timerCostTicks * reads / frames * msPerTick,
      100.0 * timerCostTicks * reads / frames * msPerTick / frameMs);

  if(q2p_crcon) {
    Out(f, "-- frame CRC-32: %08lx over %lu frames%s\n", crcAll ^ 0xFFFFFFFFUL, crcFrames,
        fixedtime->value > 0 ? "" : " (fixedtime is 0: not reproducible)");
  }

  if(f) {
    fclose(f);
    Com_Printf("prof: written to %s\n", logpath);
  }
}

static const char *ArgValue(const char *name, const char *def)
{
  int i = COM_CheckParm((char *)name);

  if(i && i < COM_Argc() - 1) {
    return COM_Argv(i + 1);
  }
  return def;
}

/*
===============================================================================
SAMPLING PROFILER - src/q2sample.c, read by tools/wprof.py
===============================================================================
*/

int qaSampleStart(void);
unsigned long qaSampleStop(const char *path);

static void SampleStart(void)
{
  if(sampling) {
    return;
  }
  sampling = qaSampleStart();
  Com_Printf(sampling ? "profsample: sampling\n" : "profsample: could not start\n");
}

static void SampleStop(void)
{
  const char *path;
  unsigned long n;

  if(!sampling) {
    return;
  }
  sampling = false;
  path = ArgValue("-samplefile", DEFAULT_SAMPLES);
  n = qaSampleStop(path);
  Com_Printf("profsample: %lu samples written to %s\n", n, path);
}

static void Prof_f(void)
{
  Report(Cmd_Argc() > 1 ? Cmd_Argv(1) : NULL);
}

static void ProfReset_f(void)
{
  resetPending = true;
}

static void ProfSample_f(void)
{
  if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "stop")) {
    SampleStop();
  } else {
    SampleStart();
  }
}

void Q2P_Init(void)
{
  Cmd_AddCommand("prof", Prof_f);
  Cmd_AddCommand("profreset", ProfReset_f);
  Cmd_AddCommand("profsample", ProfSample_f);
  CalibrateTimer();
  CRCInit();
  benchmark = COM_CheckParm("-benchmark") != 0;
  q2p_crcon = COM_CheckParm("-crc") != 0;
  q2p_phases = COM_CheckParm("-prof") != 0;
  if(q2p_crcon && COM_CheckParm("-crcfile") && COM_CheckParm("-crcfile") < COM_Argc() - 1) {
    crcFile = fopen(COM_Argv(COM_CheckParm("-crcfile") + 1), "w");
  }
  {
    const char *p = ArgValue("-crcdump", "");

    while(*p && numDumps < MAX_DUMPS) {
      crcDump[numDumps++] = strtoul(p, (char **)&p, 10);
      if(*p == ',') {
        p++;
      } else {
        break;
      }
    }
  }
  if(q2p_crcon && COM_CheckParm("-crcref") && COM_CheckParm("-crcref") < COM_Argc() - 1) {
    crcRef = fopen(COM_Argv(COM_CheckParm("-crcref") + 1), "r");
  }
  Reset();
}

void Q2P_FrameStart(void)
{
  if(resetPending) {
    resetPending = false;
    Reset();
    measuring = true;
  }
}

void Q2P_ServerData(void)
{
  // The effects draw from rand(), and the server calls it once a frame,
  // while loading too, so how many calls came before depends on the disk's
  // speed: reseed before the demo's first message spawns anything.
  if(q2p_crcon) {
    srand(0);
  }
}

void Q2P_TimedemoStart(void)
{
  // mid-frame: the reset waits for the next frame to begin, and so do the CRCs
  resetPending = true;
  if(COM_CheckParm("-sample")) {
    SampleStart();
  }
}

int Q2P_TimedemoDone(int frames)
{
  static int limit = -1;

  if(limit < 0) {
    limit = atoi(ArgValue("-frames", "0"));
  }
  return limit > 0 && frames >= limit;
}

void Q2P_TimedemoEnd(int frames, int msec)
{
  measuring = false;
  SampleStop();
  Report(ArgValue("-proflog", DEFAULT_LOG));
  if(crcFile) {
    fclose(crcFile);
    crcFile = NULL;
  }
  if(benchmark) {
    // one line for scripts; stdout is the Shell's (or a remote shell's) output
    printf("Q2BENCH frames=%d ms=%d ms_per_frame=%.3f fps=%.3f crc=%08lx\n", frames, msec,
           frames > 0 ? (double)msec / frames : 0.0, msec > 0 ? frames * 1000.0 / msec : 0.0,
           q2p_crcon ? crcAll ^ 0xFFFFFFFFUL : 0UL);
    fflush(stdout);
    Cbuf_AddText("quit\n");
  }
}

