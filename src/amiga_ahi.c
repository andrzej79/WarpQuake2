// WarpQuake2 - sound output through AHI (snddma_amiga.c's SNDDMA_* calls).
//
// Quake II mixes into a ring buffer and asks where the hardware is reading
// (SNDDMA_GetDMAPos), as with a DMA sound card.  Here AHI plays that ring as
// one looping dynamic sample (AHIST_DYNAMICSAMPLE: AHI reads the memory as
// it mixes, so what Quake writes ahead of the play position is heard).  The
// play position is not something AHI reports, so it is measured: AHI calls
// the sound hook each time the sample (re)starts, the hook notes the E-clock,
// and the position is the time since then times the mixing rate.  (WarpQuake's
// amiga_ahi.c, the same scheme.)
//
// The ring is mono when the audio mode is (the Warp's "16 bit mono", the
// default): Quake II then mixes and writes half as much.

#ifndef __VBCC__
#define __reg(x)
#define __saveds
#endif

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <devices/ahi.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/ahi.h>

#include "amiga_q2.h"

#define RING_SECONDS_MIN 0.3           // ring length: at least this, a power of 2 frames

struct Library *AHIBase = NULL;
extern struct Device *TimerBase;       // amiga_os.c, the E-clock

static struct MsgPort *ahiPort = NULL;
static struct AHIRequest *ahiReq = NULL;
static BOOL ahiDeviceOpen = FALSE;
static struct AHIAudioCtrl *ahiCtrl = NULL;
static BOOL soundLoaded = FALSE;

static WORD *ring = NULL;              // 16-bit, big-endian, mono or stereo
static ULONG ringFrames;
static ULONG mixFreq;                  // what AHI actually mixes at
static double framesPerTick;           // mixFreq / E-clock rate

static struct Hook soundHook;
static volatile ULONG loopStart;       // E-clock (low word) when the ring last started
static volatile ULONG loopCount;

static char soundInfo[96];

static BOOL ahiOpen(void)
{
  if(AHIBase != NULL) {
    return TRUE;
  }
  ahiPort = CreateMsgPort();
  if(ahiPort == NULL) {
    return FALSE;
  }
  ahiReq = (struct AHIRequest *)CreateIORequest(ahiPort, sizeof(struct AHIRequest));
  if(ahiReq == NULL) {
    return FALSE;
  }
  ahiReq->ahir_Version = 4;
  if(OpenDevice((STRPTR)AHINAME, AHI_NO_UNIT, (struct IORequest *)ahiReq, 0) != 0) {
    return FALSE;
  }
  ahiDeviceOpen = TRUE;
  AHIBase = (struct Library *)ahiReq->ahir_Std.io_Device;
  return TRUE;
}

static void ahiClose(void)
{
  if(ahiDeviceOpen) {
    CloseDevice((struct IORequest *)ahiReq);
    ahiDeviceOpen = FALSE;
  }
  AHIBase = NULL;
  if(ahiReq != NULL) {
    DeleteIORequest((struct IORequest *)ahiReq);
    ahiReq = NULL;
  }
  if(ahiPort != NULL) {
    DeleteMsgPort(ahiPort);
    ahiPort = NULL;
  }
}

// Called by AHI each time the ring sample starts over (also the first time).
// From AHI's mixing context: only notes the time.
static ULONG __saveds soundFunc(__reg("a0") struct Hook *hook, __reg("a2") struct AHIAudioCtrl *ctrl,
                                __reg("a1") struct AHISoundMessage *msg)
{
  struct EClockVal ev;

  (void)hook;
  (void)ctrl;
  (void)msg;
  ReadEClock(&ev);
  loopStart = ev.ev_lo;
  loopCount++;
  return 0;
}

static struct AHIAudioCtrl *allocAudio(ULONG mode, ULONG freq)
{
  return AHI_AllocAudio(AHIA_AudioID, mode, AHIA_MixFreq, freq, AHIA_Channels, 1, AHIA_Sounds, 1, AHIA_SoundFunc,
                        (ULONG)&soundHook, TAG_DONE);
}

void *qaSoundInit(unsigned long mode, int freq, int *frames, int *rate, int *channels)
{
  struct EClockVal ev;
  struct AHISampleInfo sample;
  ULONG eclock, id = AHI_INVALID_ID, stereo = 0;
  char name[64];

  if(qaHeadless || TimerBase == NULL) {
    return NULL;
  }
  if(!ahiOpen()) {
    printf("Sound: no ahi.device V4\n");
    qaSoundShutdown();
    return NULL;
  }

  memset(&soundHook, 0, sizeof(soundHook));
  soundHook.h_Entry = (HOOKFUNC)soundFunc;
  ahiCtrl = allocAudio(mode, (ULONG)freq);
  if(ahiCtrl == NULL && mode != AHI_DEFAULT_ID) {
    printf("Sound: no AHI mode 0x%08lx at %d Hz, using the default mode\n", mode, freq);
    mode = AHI_DEFAULT_ID;
    ahiCtrl = allocAudio(mode, (ULONG)freq);
  }
  if(ahiCtrl == NULL) {
    printf("Sound: cannot allocate AHI at %d Hz\n", freq);
    qaSoundShutdown();
    return NULL;
  }
  mixFreq = 0;
  AHI_ControlAudio(ahiCtrl, AHIC_MixFreq_Query, (ULONG)&mixFreq, TAG_DONE);
  if(mixFreq == 0) {
    mixFreq = (ULONG)freq;
  }
  // the mode the control really got (the default mode resolves to one)
  name[0] = 0;
  AHI_GetAudioAttrs(AHI_INVALID_ID, ahiCtrl, AHIDB_AudioID, (ULONG)&id, TAG_DONE);
  AHI_GetAudioAttrs(id, NULL, AHIDB_Name, (ULONG)name, AHIDB_BufferLen, sizeof(name), AHIDB_Stereo,
                    (ULONG)&stereo, TAG_DONE);
  *channels = stereo ? 2 : 1;

  // the ring: a power of 2 frames (Quake masks positions), >= RING_SECONDS_MIN
  ringFrames = 1024;
  while(ringFrames < (ULONG)(mixFreq * RING_SECONDS_MIN)) {
    ringFrames <<= 1;
  }
  ring = AllocVec(ringFrames * 2 * *channels, MEMF_PUBLIC | MEMF_CLEAR);
  if(ring == NULL) {
    qaSoundShutdown();
    return NULL;
  }
  sample.ahisi_Type = stereo ? AHIST_S16S : AHIST_M16S;
  sample.ahisi_Address = ring;
  sample.ahisi_Length = ringFrames;
  if(AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, ahiCtrl) != AHIE_OK) {
    printf("Sound: AHI_LoadSound failed\n");
    qaSoundShutdown();
    return NULL;
  }
  soundLoaded = TRUE;

  eclock = ReadEClock(&ev);
  framesPerTick = (double)mixFreq / (double)eclock;
  loopStart = ev.ev_lo;
  loopCount = 0;

  if(AHI_ControlAudio(ahiCtrl, AHIC_Play, TRUE, TAG_DONE) != AHIE_OK) {
    printf("Sound: AHI won't play\n");
    qaSoundShutdown();
    return NULL;
  }
  AHI_Play(ahiCtrl, AHIP_BeginChannel, 0, AHIP_Freq, mixFreq, AHIP_Vol, 0x10000, AHIP_Pan, 0x8000, AHIP_Sound, 0,
           AHIP_EndChannel, 0, TAG_DONE);

  sprintf(soundInfo, "AHI 0x%08lx %s, %lu Hz, %s", id, name[0] ? name : "?", mixFreq, stereo ? "stereo" : "mono");
  *frames = (int)ringFrames;
  *rate = (int)mixFreq;
  return ring;
}

// The frame AHI is playing now: the time since the ring last started.
int qaSoundPos(void)
{
  struct EClockVal ev;
  ULONG start, count;
  double f;

  if(ahiCtrl == NULL) {
    return 0;
  }
  do {                                  // the hook may run between the reads
    count = loopCount;
    start = loopStart;
    ReadEClock(&ev);
  } while(count != loopCount);
  f = (double)(ULONG)(ev.ev_lo - start) * framesPerTick;
  if(f >= (double)(ringFrames - 1)) {
    return (int)ringFrames - 1;          // the hook is due: hold at the end
  }
  return (int)f;
}

const char *qaSoundInfo(void)
{
  return ahiCtrl != NULL ? soundInfo : "none";
}

// Also from the atexit() cleanup: safe to call twice.
void qaSoundShutdown(void)
{
  if(ahiCtrl != NULL) {
    AHI_ControlAudio(ahiCtrl, AHIC_Play, FALSE, TAG_DONE);
    if(soundLoaded) {
      AHI_UnloadSound(0, ahiCtrl);
      soundLoaded = FALSE;
    }
    AHI_FreeAudio(ahiCtrl);
    ahiCtrl = NULL;
  }
  if(ring != NULL) {
    FreeVec(ring);
    ring = NULL;
  }
  ahiClose();
}
