// WarpQuake2 - the engine side of input: Amiga rawkeys as Quake II keys,
// and the mouse as view angles and movement (id's Win32 IN_MouseMove).

#include "../quake2/client/client.h"
#include "amiga_q2.h"

// NewMouse wheel rawkeys (no NDK header defines them)
#define RAWKEY_NM_WHEEL_UP 0x7A
#define RAWKEY_NM_WHEEL_DOWN 0x7B

// Positional: the rawkey is the key's place on the keyboard, so a keymap does
// not move WASD.  Quake wants lower-case ASCII for printable keys.  The
// numeric keypad gives Quake II's keypad keys.
static const unsigned char rawkeyToQuake[0x80] = {
  /* 0x00 */ '`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\\', 0, K_KP_INS,
  /* 0x10 */ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, K_KP_END, K_KP_DOWNARROW, K_KP_PGDN,
  /* 0x20 */ 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 0, 0, K_KP_LEFTARROW, K_KP_5, K_KP_RIGHTARROW,
  /* 0x30 */ '<', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, K_KP_DEL, K_KP_HOME, K_KP_UPARROW, K_KP_PGUP,
  /* 0x40 */ K_SPACE, K_BACKSPACE, K_TAB, K_KP_ENTER, K_ENTER, K_ESCAPE, K_DEL, K_INS, K_PGUP, K_PGDN, K_KP_MINUS,
  /*      */ K_F11, K_UPARROW, K_DOWNARROW, K_RIGHTARROW, K_LEFTARROW,
  /* 0x50 */ K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, '(', ')', K_KP_SLASH, '*', K_KP_PLUS, 0,
  /* 0x60 */ K_SHIFT, K_SHIFT, 0, K_CTRL, K_ALT, K_ALT, 0, 0, 0, 0, 0, 0, 0, 0, K_PAUSE, K_F12,
  /* 0x70 */ K_HOME, K_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

cvar_t *in_joystick;                   // the options menu's; no joystick yet
static cvar_t *m_filter;
static cvar_t *in_mouse;
static qboolean mlooking;
static qboolean inActive = true;
static int oldMouseX, oldMouseY;

static void IN_MLookDown(void)
{
  mlooking = true;
}

static void IN_MLookUp(void)
{
  mlooking = false;
  if(!freelook->value && lookspring->value) {
    IN_CenterView();
  }
}

void IN_Init(void)
{
  m_filter = Cvar_Get("m_filter", "0", 0);
  in_mouse = Cvar_Get("in_mouse", "1", CVAR_ARCHIVE);
  in_joystick = Cvar_Get("in_joystick", "0", CVAR_ARCHIVE);
  Cmd_AddCommand("+mlook", IN_MLookDown);
  Cmd_AddCommand("-mlook", IN_MLookUp);
}

void IN_Shutdown(void)
{
  Cmd_RemoveCommand("+mlook");
  Cmd_RemoveCommand("-mlook");
}

void IN_Commands(void)
{
}

void IN_Frame(void)
{
}

void IN_Activate(qboolean active)
{
  inActive = active;
}

// Sys_SendKeyEvents: the queued keys and buttons, as Key_Event calls.
void IN_SendKeyEvents(void)
{
  int code, down, key;
  unsigned t;

  if(qaInputBreak()) {
    Sys_Quit();
  }
  t = Sys_Milliseconds();
  while(qaInputKey(&code, &down)) {
    if(code >= QA_MOUSE1) {
      Key_Event(K_MOUSE1 + (code - QA_MOUSE1), down, t);
    } else if(code == RAWKEY_NM_WHEEL_UP || code == RAWKEY_NM_WHEEL_DOWN) {
      // a wheel click is one press, Quake wants a press and a release
      if(down) {
        key = (code == RAWKEY_NM_WHEEL_UP) ? K_MWHEELUP : K_MWHEELDOWN;
        Key_Event(key, true, t);
        Key_Event(key, false, t);
      }
    } else if((key = rawkeyToQuake[code & 0x7F]) != 0) {
      Key_Event(key, down, t);
    }
  }
}

void IN_Move(usercmd_t *cmd)
{
  int mx, my;
  float mouseX, mouseY;

  qaInputMouse(&mx, &my);
  if(!in_mouse->value || !inActive || !qaInputActive()) {
    return;
  }

  if(m_filter->value) {
    mouseX = (mx + oldMouseX) * 0.5f;
    mouseY = (my + oldMouseY) * 0.5f;
  } else {
    mouseX = (float)mx;
    mouseY = (float)my;
  }
  oldMouseX = mx;
  oldMouseY = my;

  mouseX *= sensitivity->value;
  mouseY *= sensitivity->value;

  // add mouse X/Y movement to cmd
  if((in_strafe.state & 1) || (lookstrafe->value && mlooking)) {
    cmd->sidemove += m_side->value * mouseX;
  } else {
    cl.viewangles[YAW] -= m_yaw->value * mouseX;
  }

  if((mlooking || freelook->value) && !(in_strafe.state & 1)) {
    cl.viewangles[PITCH] += m_pitch->value * mouseY;
  } else {
    cmd->forwardmove -= m_forward->value * mouseY;
  }
}
