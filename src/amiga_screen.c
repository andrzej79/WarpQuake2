// WarpQuake2 - the RTG screen: an 8-bit CLUT or a 16-bit RGB Picasso96 mode
// as close as there is to the size the renderer asks for, a borderless
// backdrop window on it for input, and the frame copied in.
//
// A 16-bit frame is big-endian RGB565, the Warp's 16-bit format.  On a
// screen of that format it can be copied straight in; any other 16-bit
// format (5-5-5, PC byte order) gets it through p96WritePixelArray, which
// converts.  The palette blend of a 16-bit screen (the damage and bonus
// flashes, under water: the 8-bit screen's CLUT does it for free) is done
// here, as the frame is copied (blend16.s).

#ifndef __VBCC__
#define __reg(x)
#define __saveds
#endif

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

/* Picasso96: the structures from the Picasso96 developer headers (P96_SDK in
   the Makefile), the call stubs from vbcc's own inline/ tree, whose
   prototypes need stdargs for the tag-list functions. */
#include <libraries/Picasso96.h>
#pragma stdargs-on
#include <clib/Picasso96_protos.h>
#pragma stdargs-off
#include <inline/Picasso96_protos.h>

#include "amiga_q2.h"

struct GfxBase *GfxBase = NULL;
struct Library *P96Base = NULL;
struct Window *qaWindow = NULL;        // amiga_input.c reads its port
int qaHeadless;

static struct Screen *screen = NULL;
static UWORD *blankPointer = NULL;     // chip RAM, for SetPointer
static ULONG paletteTable[1 + 256 * 3 + 1];
static ULONG modeId = INVALID_ID;
static int screenWidth, screenHeight;
static int screenDepth;                // 8 or 16: the frame's
static RGBFTYPE screenFormat;          // the screen's own
static BOOL onWarp;                    // the screen is on a Warp board (csgfx.card)
static char info[80];

// the blend: qaBlend16's parameters (mask, spread(c) * a, 32 - a), on while
// its alpha is not 0
static ULONG blendParams[3];
static BOOL blendOn;
static UWORD *blendBuffer;             // 16 bpp through p96WritePixelArray
static ULONG blendBufferBytes;

void qaBlend16(const UWORD *src, UWORD *dst, ULONG pixels, const ULONG *params);
void qaMove16Rows(const UBYTE *src, UBYTE *dst, ULONG width, ULONG height, ULONG dstBytesPerRow);

#define RGBFF_16BIT (RGBFF_R5G6B5 | RGBFF_R5G5B5 | RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B5G6R5PC | RGBFF_B5G5R5PC)

static BOOL openLibraries(void)
{
  if(GfxBase == NULL) {
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 40);
  }
  if(P96Base == NULL) {
    P96Base = OpenLibrary("Picasso96API.library", 2);
  }
  if(blankPointer == NULL) {
    // a 1x1 transparent sprite: the pointer is hidden, the mouse still reports
    blankPointer = AllocVec(6 * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
  }
  return GfxBase != NULL && P96Base != NULL;
}

int qaScreenOpen(int width, int height, int depth)
{
  ULONG id = INVALID_ID;

  qaScreenClose();
  blendOn = FALSE;
  if(qaHeadless) {
    screenWidth = width;
    screenHeight = height;
    screenDepth = depth == 16 ? 16 : 8;
    screenFormat = screenDepth == 16 ? RGBFB_R5G6B5 : RGBFB_CLUT;
    sprintf(info, "headless %dx%dx%d", width, height, screenDepth);
    return screenDepth;
  }
  if(!openLibraries()) {
    printf("WarpQuake2: needs graphics.library V40 and Picasso96API.library V2 (RTG)\n");
    return 0;
  }
  if(depth == 16) {
    // big-endian 5-6-5 first, the frame's own format; any 16-bit one else
    id = p96BestModeIDTags(P96BIDTAG_NominalWidth, width, P96BIDTAG_NominalHeight, height, P96BIDTAG_Depth, 16,
                           P96BIDTAG_FormatsAllowed, RGBFF_R5G6B5, TAG_DONE);
    if(id == INVALID_ID) {
      id = p96BestModeIDTags(P96BIDTAG_NominalWidth, width, P96BIDTAG_NominalHeight, height, P96BIDTAG_Depth, 16,
                             P96BIDTAG_FormatsAllowed, RGBFF_16BIT, TAG_DONE);
    }
    if(id == INVALID_ID) {
      printf("WarpQuake2: no 16-bit RTG mode for %dx%d, using 8 bits\n", width, height);
    }
  }
  if(id == INVALID_ID) {
    depth = 8;
    id = p96BestModeIDTags(P96BIDTAG_NominalWidth, width, P96BIDTAG_NominalHeight, height, P96BIDTAG_Depth, 8,
                           P96BIDTAG_FormatsAllowed, RGBFF_CLUT, TAG_DONE);
  }
  if(id == INVALID_ID) {
    return 0;
  }
  screenWidth = (int)p96GetModeIDAttr(id, P96IDA_WIDTH);
  screenHeight = (int)p96GetModeIDAttr(id, P96IDA_HEIGHT);
  screenDepth = depth;
  screenFormat = (RGBFTYPE)p96GetModeIDAttr(id, P96IDA_RGBFORMAT);
  {
    const char *board = (const char *)p96GetModeIDAttr(id, P96IDA_BOARDNAME);

    onWarp = (board != NULL && strncmp(board, "csgfx", 5) == 0);
  }

  screen = OpenScreenTags(NULL, SA_DisplayID, id, SA_Width, screenWidth, SA_Height, screenHeight, SA_Depth, depth,
                          SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN, SA_Exclusive, TRUE,
                          SA_Draggable, FALSE, SA_Title, (ULONG) "WarpQuake2", TAG_DONE);
  if(screen == NULL) {
    return 0;
  }
  qaWindow = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)screen, WA_Left, 0, WA_Top, 0, WA_Width, screenWidth,
                            WA_Height, screenHeight, WA_Borderless, TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE,
                            WA_RMBTrap, TRUE, WA_ReportMouse, TRUE, WA_NoCareRefresh, TRUE, WA_SimpleRefresh, TRUE,
                            WA_IDCMP,
                            IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_DELTAMOVE |
                              IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW,
                            TAG_DONE);
  if(qaWindow == NULL) {
    CloseScreen(screen);
    screen = NULL;
    return 0;
  }
  if(blankPointer != NULL) {
    SetPointer(qaWindow, blankPointer, 1, 1, 0, 0);
  }
  SetRast(qaWindow->RPort, 0);
  modeId = id;
  sprintf(info, "mode 0x%08lx %dx%dx%d%s%s", modeId, screenWidth, screenHeight, depth,
          (depth == 16 && screenFormat != RGBFB_R5G6B5) ? " (converted)" : "", onWarp ? ", Warp" : "");
  return depth;
}

// Also from the atexit() cleanup: safe to call twice, and with nothing open.
void qaScreenClose(void)
{
  if(qaWindow != NULL) {
    ClearPointer(qaWindow);
    CloseWindow(qaWindow);
    qaWindow = NULL;
  }
  if(screen != NULL) {
    CloseScreen(screen);
    screen = NULL;
  }
}

int qaScreenWidth(void)
{
  return screenWidth;
}

int qaScreenHeight(void)
{
  return screenHeight;
}

const char *qaScreenInfo(void)
{
  return info;
}

// Straight into the screen's memory, row by row: CopyMemQuick, or MOVE16
// bursts (mode 2; Warp's RTG memory is uncached and built for them), or
// blended (16 bpp, while a blend is on).  FALSE if the bitmap cannot be
// locked or is not in the frame's format: the caller falls back to the OS
// call.  MOVE16 needs 16-byte aligned rows, CopyMemQuick longword ones;
// CopyMem otherwise.
static BOOL blitLocked(const UBYTE *src, int rowBytes, int width, int height, int mode)
{
  struct BitMap *bm = qaWindow->RPort->BitMap;
  struct RenderInfo ri;
  LONG lock;
  UBYTE *dst;
  ULONG bytes = (ULONG)width * (screenDepth / 8);
  int y;

  lock = p96LockBitMap(bm, (UBYTE *)&ri, sizeof(ri));
  if(lock == 0) {
    return FALSE;
  }
  if(ri.RGBFormat != (screenDepth == 16 ? RGBFB_R5G6B5 : RGBFB_CLUT) || ri.Memory == NULL) {
    p96UnlockBitMap(bm, lock);
    return FALSE;
  }
  dst = (UBYTE *)ri.Memory;
  if(blendOn) {
    for(y = 0; y < height; y++) {
      qaBlend16((const UWORD *)src, (UWORD *)dst, (ULONG)width & ~1UL, blendParams);
      src += rowBytes;
      dst += ri.BytesPerRow;
    }
  } else if(mode == 2 && (((ULONG)src | (ULONG)dst | (ULONG)rowBytes | (ULONG)ri.BytesPerRow | bytes) & 15) == 0 &&
            (ULONG)rowBytes == bytes) {
    qaMove16Rows(src, dst, bytes, (ULONG)height, (ULONG)ri.BytesPerRow);
  } else if((((ULONG)src | (ULONG)dst | (ULONG)rowBytes | (ULONG)ri.BytesPerRow | bytes) & 3) == 0) {
    for(y = 0; y < height; y++) {
      CopyMemQuick((APTR)src, dst, bytes);
      src += rowBytes;
      dst += ri.BytesPerRow;
    }
  } else {
    for(y = 0; y < height; y++) {
      CopyMem((APTR)src, dst, bytes);
      src += rowBytes;
      dst += ri.BytesPerRow;
    }
  }
  p96UnlockBitMap(bm, lock);
  return TRUE;
}

// 16 bpp through P96, which converts to the screen's format: the frame as
// a RenderInfo, blended first into blendBuffer while a blend is on.
static void blitPixelArray(const UBYTE *src, int rowBytes, int width, int height)
{
  struct RenderInfo ri;
  int y;

  if(blendOn) {
    ULONG need = (ULONG)rowBytes * height;

    if(blendBufferBytes < need) {
      if(blendBuffer != NULL) {
        FreeVec(blendBuffer);
      }
      blendBuffer = AllocVec(need, MEMF_ANY);
      blendBufferBytes = blendBuffer != NULL ? need : 0;
    }
    if(blendBuffer != NULL) {
      for(y = 0; y < height; y++) {
        qaBlend16((const UWORD *)(src + y * rowBytes), (UWORD *)((UBYTE *)blendBuffer + y * rowBytes),
                  (ULONG)width & ~1UL, blendParams);
      }
      src = (const UBYTE *)blendBuffer;
    }
  }
  memset(&ri, 0, sizeof(ri));
  ri.Memory = (APTR)src;
  ri.BytesPerRow = (WORD)rowBytes;
  ri.RGBFormat = RGBFB_R5G6B5;
  p96WritePixelArray(&ri, 0, 0, qaWindow->RPort, 0, 0, width, height);
}

void qaScreenBlit(const unsigned char *pixels, int rowBytes, int width, int height, int mode)
{
  if(qaWindow == NULL) {
    return;
  }
  if(width > screenWidth) {
    width = screenWidth;
  }
  if(height > screenHeight) {
    height = screenHeight;
  }
  if(mode < 0) {
    // auto: MOVE16 into a Warp's RTG memory (uncached, built for bursts:
    // 1.5 ms where WriteChunkyPixels takes 2.4 at 320x240x8), the OS call
    // anywhere else (MOVE16 into Zorro III memory fails on some machines)
    mode = onWarp ? 2 : 0;
  }
  if((mode != 0 || blendOn) && blitLocked(pixels, rowBytes, width, height, mode)) {
    return;
  }
  if(screenDepth == 16) {
    blitPixelArray(pixels, rowBytes, width, height);
    return;
  }
  WriteChunkyPixels(qaWindow->RPort, 0, 0, width - 1, height - 1, (UBYTE *)pixels, rowBytes);
}

// The 16 bpp blend: each pixel moved toward r, g, b (0..255) by alpha/256,
// in 1/32 steps (qaBlend16); alpha 0 is off.
void qaScreenBlend(int r, int g, int b, int alpha)
{
  ULONG c, a = ((ULONG)alpha * 32 + 128) >> 8;

  if(screenDepth != 16 || a == 0) {
    blendOn = FALSE;
    return;
  }
  if(a > 32) {
    a = 32;
  }
  c = ((ULONG)(r >> 3) << 11) | ((ULONG)(g >> 2) << 5) | (ULONG)(b >> 3);
  blendParams[0] = 0x07e0f81fUL;
  blendParams[1] = ((c | (c << 16)) & blendParams[0]) * a;
  blendParams[2] = 32 - a;
  blendOn = TRUE;
}

void qaScreenPalette(const unsigned char *rgba)
{
  int i;

  if(screen == NULL || screenDepth != 8) {
    return;
  }
  paletteTable[0] = (256UL << 16) | 0;
  for(i = 0; i < 256; i++) {
    paletteTable[1 + i * 3 + 0] = (ULONG)rgba[i * 4 + 0] * 0x01010101UL;
    paletteTable[1 + i * 3 + 1] = (ULONG)rgba[i * 4 + 1] * 0x01010101UL;
    paletteTable[1 + i * 3 + 2] = (ULONG)rgba[i * 4 + 2] * 0x01010101UL;
  }
  paletteTable[1 + 256 * 3] = 0;
  LoadRGB32(&screen->ViewPort, paletteTable);
}

// The screen and the libraries: at exit
void qaScreenShutdown(void)
{
  qaScreenClose();
  if(blendBuffer != NULL) {
    FreeVec(blendBuffer);
    blendBuffer = NULL;
    blendBufferBytes = 0;
  }
  if(blankPointer != NULL) {
    FreeVec(blankPointer);
    blankPointer = NULL;
  }
  if(P96Base != NULL) {
    CloseLibrary(P96Base);
    P96Base = NULL;
  }
  if(GfxBase != NULL) {
    CloseLibrary((struct Library *)GfxBase);
    GfxBase = NULL;
  }
}
