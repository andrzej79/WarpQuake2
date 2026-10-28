// WarpQuake2 - the interface between the two halves of the platform layer.
//
// The files that include the AmigaOS headers (amiga_os.c, amiga_screen.c,
// amiga_input.c) and the ones that include the engine's (sys_amiga.c,
// vid_amiga.c, in_amiga.c, net_loop.c) never include each other's: the two
// define clashing names (TRUE/true, BYTE/byte, MENUUP, ...).  They meet only
// here, in plain C types.
#ifndef AMIGA_Q2_H
#define AMIGA_Q2_H

// ---- amiga_os.c: system ----

int qaInit(int headless);              // libraries and timer; 0 on failure (message printed)
void qaShutdown(void);                 // also the atexit() cleanup: safe to call twice
int qaFromWorkbench(void);
unsigned qaMilliseconds(void);         // since qaInit()
unsigned long qaTicks(void);           // raw E-clock count, low word
unsigned long qaTickRate(void);        // E-clock ticks per second
void qaMkdir(const char *path);
void qaShowError(const char *msg);     // after the screen is closed: a requester from Workbench
int qaSetEnv(const char *name, const char *value);  // a local shell variable; 0 on failure

// Memory for a hunk: maxsize bytes, cleared, then cut down to what was used
// (the tail goes back to the system); qaHunkTrim returns the size it kept,
// which qaHunkFree is given.  Headless (vamos, which cannot free part of a
// block) a hunk keeps its whole reservation.
void *qaHunkAlloc(unsigned maxsize);
unsigned qaHunkTrim(void *base, unsigned maxsize, unsigned used);
void qaHunkFree(void *base, unsigned size);

// Directory scan, one at a time: the entries of dir whose names match pattern
// (a Quake glob: '*' and '?', case-insensitive).  qaFindNext returns NULL at
// the end; *isDir tells a directory.
int qaFindOpen(const char *dir, const char *pattern);
const char *qaFindNext(int *isDir);
void qaFindClose(void);

// ---- amiga_screen.c: the RTG screen ----

// -headless (vamos, which has no graphics): no screen, no input; the frame is
// only rendered, at the size asked for
extern int qaHeadless;
// A screen in the closest RTG mode of that depth (8 or 16; 8 if there is no
// 16-bit one): the depth it opened, 0 on failure.  A 16-bit frame is
// big-endian RGB565.
int qaScreenOpen(int width, int height, int depth);
void qaScreenClose(void);
void qaScreenShutdown(void);                // the screen and the libraries, at exit
int qaScreenWidth(void);                   // the open screen's (may exceed the frame's)
int qaScreenHeight(void);
// mode 0: WriteChunkyPixels (8 bpp) or p96WritePixelArray (16); 1: the
// bitmap locked and the rows copied with CopyMemQuick; 2: the same with
// MOVE16 (falls back to 0 if it cannot be locked); -1: 2 on a Warp board,
// 0 on any other.  rowBytes in bytes.
void qaScreenBlit(const unsigned char *pixels, int rowBytes, int width, int height, int mode);
void qaScreenBlend(int r, int g, int b, int alpha);  // 16 bpp: alpha 0..256, 0 = off
void qaScreenPalette(const unsigned char *rgba);  // 256 entries of R, G, B, unused
const char *qaScreenInfo(void);

// ---- amiga_ahi.c: sound ----

// AHI playing a looping ring of 16-bit samples, mono or stereo as the mode
// is: the ring (NULL if there is no sound; never headless), its length in
// frames, the rate and the channels AHI really uses.  mode 0 is AHI's
// default mode, which is also taken when the one asked for is not there.
void *qaSoundInit(unsigned long mode, int freq, int *frames, int *rate, int *channels);
int qaSoundPos(void);                  // the frame AHI is playing
const char *qaSoundInfo(void);
void qaSoundShutdown(void);            // also the atexit() cleanup: safe to call twice

// ---- amiga_input.c: the game window's input ----

void qaInputStart(void);               // after qaScreenOpen
void qaInputStop(void);
int qaInputBreak(void);                // Ctrl-C to the program
// The next key event: an Amiga rawkey code (0x00-0x7f, wheel 0x7a/0x7b) or a
// mouse button (QA_MOUSE1..3).  0 when there are none left.
#define QA_MOUSE1 0x100
#define QA_MOUSE2 0x101
#define QA_MOUSE3 0x102
int qaInputKey(int *code, int *down);
void qaInputMouse(int *dx, int *dy);   // counts since the last call
int qaInputActive(void);               // the game window is the active one

#endif
