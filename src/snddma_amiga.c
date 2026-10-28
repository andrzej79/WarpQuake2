// WarpQuake2 - sound, the engine's side: the SNDDMA_* calls of Quake II's
// mixer (client/snd_dma.c) on amiga_ahi.c's ring.
//
// s_khz (Quake II's, default 11: 11025 Hz) is the mixing rate; s_ahimode
// (saved) the AHI audio mode, by default the Warp's "16 bit mono"
// (0x00570004; AHI's default mode where there is none, 0 for AHI's default
// mode always).  Either takes effect at snd_restart.

#include "../quake2/client/client.h"
#include "../quake2/client/snd_loc.h"
#include "amiga_q2.h"

#define WARP_AHI_16BIT_MONO "0x00570004"

qboolean SNDDMA_Init(void)
{
  cvar_t *mode = Cvar_Get("s_ahimode", WARP_AHI_16BIT_MONO, CVAR_ARCHIVE);
  int frames, rate, channels, khz = (int)s_khz->value;
  void *ring;

  rate = khz >= 44 ? 44100 : khz >= 22 ? 22050 : 11025;
  ring = qaSoundInit(strtoul(mode->string, NULL, 0), rate, &frames, &rate, &channels);
  if(ring == NULL) {
    return false;
  }
  Com_Printf("Sound: %s, %d-frame ring\n", qaSoundInfo(), frames);
  dma.channels = channels;
  dma.samples = frames * channels;
  dma.submission_chunk = 1;
  dma.samplepos = 0;
  dma.samplebits = 16;
  dma.speed = rate;
  dma.buffer = ring;
  return true;
}

// in mono samples (frames times channels), as snd_dma.c counts
int SNDDMA_GetDMAPos(void)
{
  dma.samplepos = (qaSoundPos() * dma.channels) & (dma.samples - 1);
  return dma.samplepos;
}

void SNDDMA_Shutdown(void)
{
  qaSoundShutdown();
  dma.buffer = NULL;
}

void SNDDMA_BeginPainting(void)
{
}

void SNDDMA_Submit(void)
{
}
