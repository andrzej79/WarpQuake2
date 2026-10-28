// WarpQuake2 - video, the client's side: id's direct-link glue to the
// software renderer (null/vid_null.c, REF_HARD_LINKED).  The renderer's
// platform calls are swimp_amiga.c's: the two sides each define a viddef_t.

#include "../quake2/client/client.h"
#include "../quake2/client/qmenu.h"

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

// id's numbers for 0-4; nothing past 800x600, which this hardware cannot
// render at a playable rate
static vidmode_t vid_modes[] = {
  {"Mode 0: 320x240", 320, 240, 0}, {"Mode 1: 400x300", 400, 300, 1}, {"Mode 2: 512x384", 512, 384, 2},
  {"Mode 3: 640x480", 640, 480, 3}, {"Mode 4: 800x600", 800, 600, 4}, {"Mode 5: 640x360", 640, 360, 5},
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

/*
==========================================================================
VIDEO MENU - id's software half of win32/vid_menu.c: mode, colour depth,
screen size, brightness, the HUD bar; "apply" sets them
==========================================================================
*/

extern cvar_t *scr_viewsize;
extern cvar_t *scr_hudbar;

void M_PopMenu(void);
const char *Default_MenuKey(menuframework_s *m, int key);

// the menu's order: by size
static const int menu_modes[] = {0, 1, 2, 5, 3, 4};
static const char *mode_names[] = {"[320 240]", "[400 300]", "[512 384]", "[640 360]", "[640 480]", "[800 600]", NULL};
static const char *bpp_names[] = {"8-bit", "16-bit", NULL};
static const char *yesno_names[] = {"no", "yes", NULL};

static menuframework_s s_video_menu;
static menulist_s s_mode_list;
static menulist_s s_bpp_list;
static menuslider_s s_screensize_slider;
static menuslider_s s_brightness_slider;
static menulist_s s_hudbar_box;
static menuaction_s s_apply_action;
static menuaction_s s_defaults_action;

static void ScreenSizeCallback(void *s)
{
  Cvar_SetValue("viewsize", ((menuslider_s *)s)->curvalue * 10);
}

static void BrightnessCallback(void *s)
{
  (void)s;
}

static void ResetDefaults(void *unused)
{
  (void)unused;
  VID_MenuInit();
}

static void ApplyChanges(void *unused)
{
  float gamma = (0.8f - (s_brightness_slider.curvalue / 10.0f - 0.5f)) + 0.5f;

  (void)unused;
  Cvar_SetValue("vid_gamma", gamma);
  Cvar_SetValue("vid_bpp", s_bpp_list.curvalue ? 16 : 8);
  Cvar_SetValue("scr_hudbar", s_hudbar_box.curvalue);
  Cvar_SetValue("sw_mode", menu_modes[s_mode_list.curvalue]);
  M_ForceMenuOff();
}

void VID_MenuInit(void)
{
  int i, mode = (int)Cvar_Get("sw_mode", "0", CVAR_ARCHIVE)->value;

  memset(&s_video_menu, 0, sizeof(s_video_menu));
  s_video_menu.x = viddef.width * 0.50;
  s_video_menu.nitems = 0;

  s_mode_list.generic.type = MTYPE_SPINCONTROL;
  s_mode_list.generic.name = "video mode";
  s_mode_list.generic.x = 0;
  s_mode_list.generic.y = 0;
  s_mode_list.itemnames = mode_names;
  s_mode_list.curvalue = 0;
  for(i = 0; i < (int)(sizeof(menu_modes) / sizeof(menu_modes[0])); i++) {
    if(menu_modes[i] == mode) {
      s_mode_list.curvalue = i;
    }
  }

  s_bpp_list.generic.type = MTYPE_SPINCONTROL;
  s_bpp_list.generic.name = "colour depth";
  s_bpp_list.generic.x = 0;
  s_bpp_list.generic.y = 10;
  s_bpp_list.itemnames = bpp_names;
  s_bpp_list.curvalue = Cvar_VariableValue("vid_bpp") == 16;

  s_screensize_slider.generic.type = MTYPE_SLIDER;
  s_screensize_slider.generic.x = 0;
  s_screensize_slider.generic.y = 20;
  s_screensize_slider.generic.name = "screen size";
  s_screensize_slider.minvalue = 4;
  s_screensize_slider.maxvalue = 10;
  s_screensize_slider.generic.callback = ScreenSizeCallback;
  s_screensize_slider.curvalue = scr_viewsize->value / 10;

  s_brightness_slider.generic.type = MTYPE_SLIDER;
  s_brightness_slider.generic.x = 0;
  s_brightness_slider.generic.y = 30;
  s_brightness_slider.generic.name = "brightness";
  s_brightness_slider.generic.callback = BrightnessCallback;
  s_brightness_slider.minvalue = 5;
  s_brightness_slider.maxvalue = 13;
  s_brightness_slider.curvalue = (1.3f - Cvar_VariableValue("vid_gamma") + 0.5f) * 10;

  s_hudbar_box.generic.type = MTYPE_SPINCONTROL;
  s_hudbar_box.generic.x = 0;
  s_hudbar_box.generic.y = 40;
  s_hudbar_box.generic.name = "HUD bar";
  s_hudbar_box.itemnames = yesno_names;
  s_hudbar_box.curvalue = scr_hudbar->value != 0;
  s_hudbar_box.generic.statusbar = "a black bar under the HUD: fewer pixels to draw";

  s_defaults_action.generic.type = MTYPE_ACTION;
  s_defaults_action.generic.name = "reset to current";
  s_defaults_action.generic.x = 0;
  s_defaults_action.generic.y = 60;
  s_defaults_action.generic.callback = ResetDefaults;

  s_apply_action.generic.type = MTYPE_ACTION;
  s_apply_action.generic.name = "apply";
  s_apply_action.generic.x = 0;
  s_apply_action.generic.y = 70;
  s_apply_action.generic.callback = ApplyChanges;

  Menu_AddItem(&s_video_menu, &s_mode_list);
  Menu_AddItem(&s_video_menu, &s_bpp_list);
  Menu_AddItem(&s_video_menu, &s_screensize_slider);
  Menu_AddItem(&s_video_menu, &s_brightness_slider);
  Menu_AddItem(&s_video_menu, &s_hudbar_box);
  Menu_AddItem(&s_video_menu, &s_defaults_action);
  Menu_AddItem(&s_video_menu, &s_apply_action);

  Menu_Center(&s_video_menu);
  s_video_menu.x -= 8;
}

void VID_MenuDraw(void)
{
  int w, h;

  // the banner, as id's menus have it
  re.DrawGetPicSize(&w, &h, "m_banner_video");
  re.DrawPic(viddef.width / 2 - w / 2, viddef.height / 2 - 110, "m_banner_video");
  Menu_AdjustCursor(&s_video_menu, 1);
  Menu_Draw(&s_video_menu);
}

const char *VID_MenuKey(int key)
{
  if(key == K_ESCAPE) {
    M_PopMenu();
    return NULL;
  }
  return Default_MenuKey(&s_video_menu, key);
}
