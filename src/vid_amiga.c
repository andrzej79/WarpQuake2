// WarpQuake2 - video, the client's side: id's direct-link glue to the
// software renderer (null/vid_null.c, REF_HARD_LINKED).  The renderer's
// platform calls are swimp_amiga.c's: the two sides each define a viddef_t.

#include "../quake2/client/client.h"

viddef_t viddef;                       // the client's view of the video state
refexport_t re;

refexport_t GetRefAPI(refimport_t rimp);

/*
==========================================================================
DIRECT LINK GLUE
==========================================================================
*/

#define MAXPRINTMSG 4096

void VID_Printf(int print_level, char *fmt, ...)
{
  va_list argptr;
  char msg[MAXPRINTMSG];

  va_start(argptr, fmt);
  vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  if(print_level == PRINT_ALL) {
    Com_Printf("%s", msg);
  } else {
    Com_DPrintf("%s", msg);
  }
}

void VID_Error(int err_level, char *fmt, ...)
{
  va_list argptr;
  char msg[MAXPRINTMSG];

  va_start(argptr, fmt);
  vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  Com_Error(err_level, "%s", msg);
}

void VID_NewWindow(int width, int height)
{
  viddef.width = width;
  viddef.height = height;
}

// id's mode list (sw_mode): each opens the closest RTG mode there is, whose
// own size the renderer then uses.
typedef struct vidmode_s {
  const char *description;
  int width, height;
  int mode;
} vidmode_t;

static vidmode_t vid_modes[] = {
  {"Mode 0: 320x240", 320, 240, 0},     {"Mode 1: 400x300", 400, 300, 1},
  {"Mode 2: 512x384", 512, 384, 2},     {"Mode 3: 640x480", 640, 480, 3},
  {"Mode 4: 800x600", 800, 600, 4},     {"Mode 5: 960x720", 960, 720, 5},
  {"Mode 6: 1024x768", 1024, 768, 6},   {"Mode 7: 1152x864", 1152, 864, 7},
  {"Mode 8: 1280x960", 1280, 960, 8},   {"Mode 9: 1600x1200", 1600, 1200, 9},
};
#define VID_NUM_MODES ((int)(sizeof(vid_modes) / sizeof(vid_modes[0])))

qboolean VID_GetModeInfo(int *width, int *height, int mode)
{
  if(mode < 0 || mode >= VID_NUM_MODES) {
    return false;
  }
  *width = vid_modes[mode].width;
  *height = vid_modes[mode].height;
  return true;
}

void VID_Init(void)
{
  refimport_t ri;

  viddef.width = 320;
  viddef.height = 240;

  ri.Cmd_AddCommand = Cmd_AddCommand;
  ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
  ri.Cmd_Argc = Cmd_Argc;
  ri.Cmd_Argv = Cmd_Argv;
  ri.Cmd_ExecuteText = Cbuf_ExecuteText;
  ri.Con_Printf = VID_Printf;
  ri.Sys_Error = VID_Error;
  ri.FS_LoadFile = FS_LoadFile;
  ri.FS_FreeFile = FS_FreeFile;
  ri.FS_Gamedir = FS_Gamedir;
  ri.Vid_NewWindow = VID_NewWindow;
  ri.Cvar_Get = Cvar_Get;
  ri.Cvar_Set = Cvar_Set;
  ri.Cvar_SetValue = Cvar_SetValue;
  ri.Vid_GetModeInfo = VID_GetModeInfo;

  re = GetRefAPI(ri);
  if(re.api_version != API_VERSION) {
    Com_Error(ERR_FATAL, "Re has incompatible api_version");
  }
  if(re.Init(NULL, NULL) == -1) {
    Com_Error(ERR_FATAL, "Couldn't start refresh");
  }
}

void VID_Shutdown(void)
{
  if(re.Shutdown) {
    re.Shutdown();
  }
}

void VID_CheckChanges(void)
{
}

void VID_MenuInit(void)
{
}

void VID_MenuDraw(void)
{
}

const char *VID_MenuKey(int k)
{
  return NULL;
}
