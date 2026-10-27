// WarpQuake2 - the RTG screen: an 8-bit CLUT Picasso96 mode as close as
// there is to the size the renderer asks for, a borderless backdrop window on
// it for input, and the frame copied in with WriteChunkyPixels.

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

static struct Screen *screen = NULL;
static UWORD *blankPointer = NULL;     // chip RAM, for SetPointer
static ULONG paletteTable[1 + 256 * 3 + 1];
static ULONG modeId = INVALID_ID;
static int screenWidth, screenHeight;
static char info[64];

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

int qaScreenOpen(int width, int height)
{
  ULONG id;

  qaScreenClose();
  if(!openLibraries()) {
    printf("WarpQuake2: needs graphics.library V40 and Picasso96API.library V2 (RTG)\n");
    return 0;
  }
  id = p96BestModeIDTags(P96BIDTAG_NominalWidth, width, P96BIDTAG_NominalHeight, height, P96BIDTAG_Depth, 8,
                         P96BIDTAG_FormatsAllowed, RGBFF_CLUT, TAG_DONE);
  if(id == INVALID_ID) {
    return 0;
  }
  screenWidth = (int)p96GetModeIDAttr(id, P96IDA_WIDTH);
  screenHeight = (int)p96GetModeIDAttr(id, P96IDA_HEIGHT);

  screen = OpenScreenTags(NULL, SA_DisplayID, id, SA_Width, screenWidth, SA_Height, screenHeight, SA_Depth, 8,
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
  sprintf(info, "mode 0x%08lx %dx%d", modeId, screenWidth, screenHeight);
  return 1;
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

void qaScreenBlit(const unsigned char *pixels, int rowBytes, int width, int height)
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
  WriteChunkyPixels(qaWindow->RPort, 0, 0, width - 1, height - 1, (UBYTE *)pixels, rowBytes);
}

void qaScreenPalette(const unsigned char *rgba)
{
  int i;

  if(screen == NULL) {
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
