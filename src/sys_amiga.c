// WarpQuake2 - the engine side of the system layer: main(), the Sys_* and
// Hunk_* calls id's code makes, and the C library functions vbcc lacks.

#include "../quake2/qcommon/qcommon.h"
#include "../quake2/game/game.h"
#include "amiga_q2.h"

// The software renderer recurses (R_RecursiveWorldNode) and so does the
// game (the AI, physics); vbcc gives every nested block its own stack slot.
size_t __stack = 1024 * 1024;
static const char stackCookie[] = "$STACK: 1048576";
#define VERSION_STRING "WarpQuake2 0.1 (28.10.2026)"
static const char versionTag[] = "$VER: " VERSION_STRING;

int curtime;
unsigned sys_frame_time;

/*
===============================================================================
C LIBRARY
===============================================================================
*/

char *strdup(const char *s)
{
  char *d = malloc(strlen(s) + 1);

  if(d != NULL) {
    strcpy(d, s);
  }
  return d;
}

// "NAME=value", as the setenv console command builds it; "NAME=" unsets
int putenv(char *string)
{
  char name[256];
  char *eq = strchr(string, '=');
  int len;

  if(eq == NULL) {
    return -1;
  }
  len = (int)(eq - string);
  if(len <= 0 || len >= (int)sizeof(name)) {
    return -1;
  }
  memcpy(name, string, len);
  name[len] = 0;
  return qaSetEnv(name, eq + 1) ? 0 : -1;
}

/*
===============================================================================
SYSTEM
===============================================================================
*/

void Sys_Init(void)
{
}

void Sys_Error(char *error, ...)
{
  va_list argptr;
  char msg[1024];

  CL_Shutdown();
  Qcommon_Shutdown();

  va_start(argptr, error);
  vsnprintf(msg, sizeof(msg), error, argptr);
  va_end(argptr);

  // the screen first: a requester behind a game screen is invisible
  qaShutdown();
  qaShowError(msg);
  exit(20);
}

void Sys_Quit(void)
{
  // after Com_Quit has shut the client and server down
  exit(0);
}

int Sys_Milliseconds(void)
{
  curtime = (int)qaMilliseconds();
  return curtime;
}

void Sys_Mkdir(char *path)
{
  qaMkdir(path);
}

char *Sys_ConsoleInput(void)
{
  return NULL;
}

void Sys_ConsoleOutput(char *string)
{
  if(!qaFromWorkbench()) {
    fputs(string, stdout);
  }
}

void Sys_SendKeyEvents(void)
{
  // in_amiga.c turns the window's events into Key_Event calls
  extern void IN_SendKeyEvents(void);

  IN_SendKeyEvents();
  sys_frame_time = Sys_Milliseconds();
}

void Sys_AppActivate(void)
{
}

void Sys_CopyProtect(void)
{
}

char *Sys_GetClipboardData(void)
{
  return NULL;
}

/*
===============================================================================
GAME - linked in (GAME_HARD_LINKED): GetGameAPI is game/g_main.c's
===============================================================================
*/

game_export_t *GetGameAPI(game_import_t *import);

void Sys_UnloadGame(void)
{
}

void *Sys_GetGameAPI(void *parms)
{
  return GetGameAPI((game_import_t *)parms);
}

/*
===============================================================================
HUNK - one at a time, as on id's platforms.  The block starts with its size.
===============================================================================
*/

static byte *hunkBase;
static int hunkMax, hunkUsed;

#define HUNK_HEADER 32   // the size, padded to keep the data cache-line aligned

void *Hunk_Begin(int maxsize)
{
  hunkMax = maxsize + HUNK_HEADER;
  hunkUsed = 0;
  hunkBase = qaHunkAlloc(hunkMax);
  if(hunkBase == NULL) {
    Sys_Error("Hunk_Begin: cannot allocate %d bytes", maxsize);
  }
  *(int *)hunkBase = hunkMax;
  return hunkBase + HUNK_HEADER;
}

void *Hunk_Alloc(int size)
{
  byte *buf;

  // round to cacheline
  size = (size + 31) & ~31;
  if(HUNK_HEADER + hunkUsed + size > hunkMax) {
    Sys_Error("Hunk_Alloc overflow");
  }
  buf = hunkBase + HUNK_HEADER + hunkUsed;
  hunkUsed += size;
  return buf;
}

int Hunk_End(void)
{
  *(int *)hunkBase = qaHunkTrim(hunkBase, hunkMax, HUNK_HEADER + hunkUsed);
  return hunkUsed;
}

void Hunk_Free(void *base)
{
  byte *m;

  if(base != NULL) {
    m = (byte *)base - HUNK_HEADER;
    qaHunkFree(m, *(int *)m);
  }
}

/*
===============================================================================
FILE SEARCH - "dir/pattern", as id's platforms take it
===============================================================================
*/

static char findBase[MAX_OSPATH];
static char findPath[MAX_OSPATH];

// A base and a name joined; no '/' after a device or assign ("PROGDIR:").
static void joinPath(char *out, const char *dir, const char *name)
{
  int len = strlen(dir);

  Com_sprintf(out, MAX_OSPATH, (len > 0 && dir[len - 1] != ':') ? "%s/%s" : "%s%s", dir, name);
}

static char *findMatch(unsigned musthave, unsigned canthave)
{
  const char *name;
  int isDir;

  while((name = qaFindNext(&isDir)) != NULL) {
    if(isDir && (canthave & SFF_SUBDIR)) {
      continue;
    }
    if((musthave & SFF_SUBDIR) && !isDir) {
      continue;
    }
    joinPath(findPath, findBase, name);
    return findPath;
  }
  return NULL;
}

char *Sys_FindFirst(char *path, unsigned musthave, unsigned canthave)
{
  char pattern[MAX_OSPATH];
  char *p;

  strcpy(findBase, path);
  p = strrchr(findBase, '/');
  if(p == NULL) {
    p = strrchr(findBase, ':');
  }
  if(p != NULL) {
    strcpy(pattern, p + 1);
    // keep the ':' of a device, drop the '/' of a directory
    p[*p == ':' ? 1 : 0] = 0;
  } else {
    strcpy(pattern, findBase);
    findBase[0] = 0;
  }
  if(strcmp(pattern, "*.*") == 0) {
    strcpy(pattern, "*");
  }
  if(!qaFindOpen(findBase, pattern)) {
    return NULL;
  }
  return findMatch(musthave, canthave);
}

char *Sys_FindNext(unsigned musthave, unsigned canthave)
{
  return findMatch(musthave, canthave);
}

void Sys_FindClose(void)
{
  qaFindClose();
}

/*
===============================================================================
MAIN
===============================================================================
*/

int main(int argc, char **argv)
{
  int time, oldtime, newtime;

  (void)stackCookie;
  (void)versionTag;

  // Workbench start: argc 0 and argv is the WBStartup message; the command
  // line is the icon's tooltypes then
  if(argc == 0) {
    argc = qaWorkbenchArgs(&argv);
  }
  atexit(qaShutdown);
  {
    int i, headless = 0;

    for(i = 1; i < argc; i++) {
      if(!strcmp(argv[i], "-headless")) {
        headless = 1;
      }
    }
    if(!qaInit(headless)) {
      return 20;
    }
  }
  printf(VERSION_STRING "\n");

  Qcommon_Init(argc, argv);

  oldtime = Sys_Milliseconds();
  for(;;) {
    // at least a millisecond a frame, as id's Linux loop
    do {
      newtime = Sys_Milliseconds();
      time = newtime - oldtime;
    } while(time < 1);
    Qcommon_Frame(time);
    oldtime = newtime;
  }
  return 0;
}
