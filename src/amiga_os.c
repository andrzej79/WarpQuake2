// WarpQuake2 - the AmigaOS side of the system layer: libraries, the E-clock
// timer, hunk memory, directory scans, mkdir, shell variables, fatal errors.

#ifndef __VBCC__
#define __reg(x)
#define __saveds
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/timer.h>

#include "amiga_q2.h"
#include "q2sample.h"

struct IntuitionBase *IntuitionBase = NULL;
struct Device *TimerBase = NULL;

static struct MsgPort *timerPort = NULL;
static struct timerequest *timerReq = NULL;
static ULONG eclockFreq;
static double eclockBase;
static BOOL fromWorkbench;

/*
===============================================================================
TIMER - E-clock, opened once, read without I/O (ReadEClock is a plain call)
===============================================================================
*/

static BOOL timerOpen(void)
{
  struct EClockVal ev;

  timerPort = CreateMsgPort();
  if(timerPort == NULL) {
    return FALSE;
  }
  timerReq = (struct timerequest *)CreateIORequest(timerPort, sizeof(struct timerequest));
  if(timerReq == NULL || OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)timerReq, 0) != 0) {
    return FALSE;
  }
  TimerBase = timerReq->tr_node.io_Device;
  eclockFreq = ReadEClock(&ev);
  eclockBase = (double)ev.ev_hi * 4294967296.0 + (double)ev.ev_lo;
  return TRUE;
}

static void timerClose(void)
{
  if(TimerBase != NULL) {
    CloseDevice((struct IORequest *)timerReq);
    TimerBase = NULL;
  }
  if(timerReq != NULL) {
    DeleteIORequest((struct IORequest *)timerReq);
    timerReq = NULL;
  }
  if(timerPort != NULL) {
    DeleteMsgPort(timerPort);
    timerPort = NULL;
  }
}

unsigned qaMilliseconds(void)
{
  struct EClockVal ev;

  ReadEClock(&ev);
  return (unsigned)(((double)ev.ev_hi * 4294967296.0 + (double)ev.ev_lo - eclockBase) * 1000.0 / eclockFreq);
}

unsigned long qaTicks(void)
{
  struct EClockVal ev;

  ReadEClock(&ev);
  return ev.ev_lo;
}

unsigned long qaTickRate(void)
{
  return eclockFreq;
}

/*
===============================================================================
SYSTEM
===============================================================================
*/

int qaInit(int headless)
{
  // Workbench start: vbcc's startup code leaves the WBStartup message in
  // _WBenchMsg; the program runs from its own directory either way, since
  // the base directory is PROGDIR:.
  extern struct WBStartup *_WBenchMsg;

  fromWorkbench = (_WBenchMsg != NULL);
  qaHeadless = headless;
  IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
  if(IntuitionBase == NULL && !headless) {
    printf("WarpQuake2 needs AmigaOS 3.0 or newer\n");
    return 0;
  }
  if(!timerOpen()) {
    printf("WarpQuake2: cannot open timer.device\n");
    return 0;
  }
  return 1;
}

void qaShutdown(void)
{
  // the sampler task first: it must not outlive the code it runs in
  wprofStop();
  wprofFree();
  qaInputStop();
  qaScreenShutdown();
  qaFindClose();
  timerClose();
  if(IntuitionBase != NULL) {
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
  }
}

int qaFromWorkbench(void)
{
  return fromWorkbench;
}

void qaMkdir(const char *path)
{
  BPTR lock = CreateDir((STRPTR)path);

  if(lock != 0) {
    UnLock(lock);
  }
}

void qaShowError(const char *msg)
{
  printf("WarpQuake2 error: %s\n", msg);
  if(fromWorkbench && IntuitionBase != NULL) {
    struct EasyStruct es = {sizeof(struct EasyStruct), 0, (UBYTE *)"WarpQuake2", (UBYTE *)"%s", (UBYTE *)"Quit"};
    EasyRequest(NULL, &es, NULL, (ULONG)msg);
  }
}

int qaSetEnv(const char *name, const char *value)
{
  return SetVar((STRPTR)name, (STRPTR)value, -1, GVF_LOCAL_ONLY) ? 1 : 0;
}

/*
===============================================================================
HUNK MEMORY

A model's hunk is reserved at its largest (16 MB for a world) and cut down
once loaded: exec lets the tail of an AllocMem block be freed on its own, in
MEM_BLOCKSIZE units.  Cleared, as id's mmap'ed hunks were.
===============================================================================
*/

#define HUNK_ROUND(n) (((n) + MEM_BLOCKSIZE - 1) & ~(ULONG)(MEM_BLOCKSIZE - 1))

void *qaHunkAlloc(unsigned maxsize)
{
  return AllocMem(HUNK_ROUND(maxsize), MEMF_ANY | MEMF_CLEAR);
}

unsigned qaHunkTrim(void *base, unsigned maxsize, unsigned used)
{
  ULONG keep = HUNK_ROUND(used), all = HUNK_ROUND(maxsize);

  if(qaHeadless) {
    return all;
  }
  if(keep < all) {
    FreeMem((UBYTE *)base + keep, all - keep);
  }
  return keep;
}

void qaHunkFree(void *base, unsigned size)
{
  FreeMem(base, HUNK_ROUND(size));
}

/*
===============================================================================
DIRECTORY SCAN
===============================================================================
*/

static BPTR findLock;
static struct FileInfoBlock *findFib;
static char findPattern[2 * 128 + 2];

// A Quake glob as an AmigaDOS pattern: '*' becomes "#?"; '?' means the same.
static void globToPattern(const char *glob, char *out, int size)
{
  int n = 0;

  for(; *glob && n < size - 3; glob++) {
    if(*glob == '*') {
      out[n++] = '#';
      out[n++] = '?';
    } else {
      out[n++] = *glob;
    }
  }
  out[n] = 0;
}

int qaFindOpen(const char *dir, const char *pattern)
{
  char amigaPattern[128];

  qaFindClose();
  globToPattern(*pattern ? pattern : "*", amigaPattern, sizeof(amigaPattern));
  if(ParsePatternNoCase((STRPTR)amigaPattern, (STRPTR)findPattern, sizeof(findPattern)) < 0) {
    return 0;
  }
  findFib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
  if(findFib == NULL) {
    return 0;
  }
  findLock = Lock((STRPTR)dir, ACCESS_READ);
  if(findLock == 0 || !Examine(findLock, findFib) || findFib->fib_DirEntryType <= 0) {
    qaFindClose();
    return 0;
  }
  return 1;
}

const char *qaFindNext(int *isDir)
{
  if(findLock == 0) {
    return NULL;
  }
  while(ExNext(findLock, findFib)) {
    if(MatchPatternNoCase((STRPTR)findPattern, (STRPTR)findFib->fib_FileName)) {
      *isDir = (findFib->fib_DirEntryType > 0);
      return (const char *)findFib->fib_FileName;
    }
  }
  return NULL;
}

void qaFindClose(void)
{
  if(findLock != 0) {
    UnLock(findLock);
    findLock = 0;
  }
  if(findFib != NULL) {
    FreeDosObject(DOS_FIB, findFib);
    findFib = NULL;
  }
}
