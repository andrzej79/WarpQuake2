// WarpQuake2 - video, the renderer's side: the SWimp_* platform calls,
// which put the frame on the RTG screen (amiga_screen.c).

#include "../quake2/ref_soft/r_local.h"
#include "amiga_q2.h"
#include "q2prof.h"

/*
==========================================================================
SWIMP - the renderer's platform calls
==========================================================================
*/

static byte *frameAlloc;               // vid.buffer, 16-byte aligned inside it

static void freeFrame(void)
{
  if(frameAlloc != NULL) {
    free(frameAlloc);
    frameAlloc = NULL;
  }
  vid.buffer = NULL;
}

int SWimp_Init(void *hInstance, void *wndProc)
{
  return true;
}

void SWimp_BeginFrame(float camera_separation)
{
}

void SWimp_EndFrame(void)
{
  {
    Q2P_BEGIN(Q2P_CRC);
    Q2P_FrameCRC(vid.buffer, vid.width, vid.height, vid.rowbytes * r_pixbytes, r_pixbytes);
    Q2P_END(Q2P_CRC);
  }
  {
    Q2P_BEGIN(Q2P_BLIT);
    static cvar_t *vid_blit;

    if(vid_blit == NULL) {
      vid_blit = ri.Cvar_Get("vid_blit", "-1", CVAR_ARCHIVE);  // auto (amiga_q2.h)
    }
    qaScreenBlit(vid.buffer, vid.rowbytes * r_pixbytes, vid.width, vid.height, (int)vid_blit->value);
    Q2P_END(Q2P_BLIT);
  }
}

// 16 bpp: the palette blend, done as the frame is shown (r_rgb.c)
void SWimp_SetBlend(int r, int g, int b, int alpha)
{
  qaScreenBlend(r, g, b, alpha);
}

void SWimp_SetPalette(const unsigned char *palette)
{
  if(palette == NULL) {
    palette = (const unsigned char *)sw_state.currentpalette;
  }
  qaScreenPalette(palette);
}

void SWimp_Shutdown(void)
{
  qaInputStop();
  qaScreenClose();
  freeFrame();
}

rserr_t SWimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
  int w, h, depth;
  cvar_t *bpp = ri.Cvar_Get("vid_bpp", "8", CVAR_ARCHIVE);

  ri.Con_Printf(PRINT_ALL, "setting mode %d:", mode);
  if(!ri.Vid_GetModeInfo(&w, &h, mode)) {
    ri.Con_Printf(PRINT_ALL, " invalid mode\n");
    return rserr_invalid_mode;
  }
  ri.Con_Printf(PRINT_ALL, " %d %d\n", w, h);

  qaInputStop();
  freeFrame();
  depth = qaScreenOpen(w, h, bpp->value >= 15 ? 16 : 8);
  if(!depth) {
    ri.Con_Printf(PRINT_ALL, "no RTG screen mode for %dx%d\n", w, h);
    return rserr_invalid_mode;
  }
  if(depth != 16 && bpp->value >= 15) {
    ri.Cvar_Set("vid_bpp", "8");       // there is no 16-bit mode
  }
  r_pixbytes = depth / 8;
  // the frame is the screen's own size, within the renderer's limits
  w = qaScreenWidth() < MAXWIDTH ? qaScreenWidth() : MAXWIDTH;
  h = qaScreenHeight() < MAXHEIGHT ? qaScreenHeight() : MAXHEIGHT;
  ri.Con_Printf(PRINT_ALL, "RTG %s, frame %dx%d\n", qaScreenInfo(), w, h);

  frameAlloc = malloc(w * h * r_pixbytes + 15);
  if(frameAlloc == NULL) {
    qaScreenClose();
    return rserr_invalid_mode;
  }
  vid.buffer = (pixel_t *)(((unsigned long)frameAlloc + 15) & ~15UL);
  vid.rowbytes = w;
  *pwidth = w;
  *pheight = h;
  ri.Vid_NewWindow(w, h);
  qaInputStart();

  R_GammaCorrectAndSetPalette((const unsigned char *)d_8to24table);
  return rserr_ok;
}

void SWimp_AppActivate(qboolean active)
{
}
