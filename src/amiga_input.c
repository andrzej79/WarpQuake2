// WarpQuake2 - the AmigaOS side of input: the game window's IDCMP messages
// as rawkey events, and the mouse.
//
// The mouse comes, by default, from an input handler (priority 51, just in
// front of Intuition) that adds up the raw relative mouse counts and passes
// every event on.  IDCMP_MOUSEMOVE deltas are what Intuition makes of those
// counts: accelerated by the Input preferences, and only while the window's
// mouse queue has room (WarpQuake's findings).  The raw counts are neither
// accelerated nor ever dropped; without the handler, IDCMP's are used.

#ifndef __VBCC__
#define __reg(x)
#define __saveds
#endif

#include <exec/types.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>

#include "amiga_q2.h"

extern struct Window *qaWindow;        // amiga_screen.c

#define KEYQUEUE_SIZE 64   // power of two
static struct {
  UWORD code;
  UBYTE down;
} keyQueue[KEYQUEUE_SIZE];
static unsigned keyHead, keyTail;
static int mouseDx, mouseDy;
static volatile BOOL windowActive = TRUE;

// the input handler's counts since the last qaInputMouse (read and cleared
// under Forbid: the handler runs in input.device's task)
static volatile LONG rawDx, rawDy;
static struct MsgPort *inputPort;
static struct IOStdReq *inputReq;
static struct Interrupt inputHandler;
static BOOL inputHandlerOn;

static struct InputEvent *mouseHandler(__reg("a0") struct InputEvent *events, __reg("a1") APTR data)
{
  struct InputEvent *ev;

  (void)data;
  if(windowActive) {
    for(ev = events; ev != NULL; ev = ev->ie_NextEvent) {
      if(ev->ie_Class == IECLASS_RAWMOUSE && (ev->ie_Qualifier & IEQUALIFIER_RELATIVEMOUSE)) {
        rawDx += ev->ie_X;
        rawDy += ev->ie_Y;
      }
    }
  }
  return events;                        // everything goes on to Intuition
}

void qaInputStart(void)
{
  keyHead = keyTail = 0;
  mouseDx = mouseDy = 0;
  windowActive = TRUE;
  if(inputHandlerOn) {
    return;
  }
  inputPort = CreateMsgPort();
  if(inputPort == NULL) {
    return;
  }
  inputReq = (struct IOStdReq *)CreateIORequest(inputPort, sizeof(struct IOStdReq));
  if(inputReq == NULL || OpenDevice("input.device", 0, (struct IORequest *)inputReq, 0) != 0) {
    if(inputReq != NULL) {
      DeleteIORequest((struct IORequest *)inputReq);
      inputReq = NULL;
    }
    DeleteMsgPort(inputPort);
    inputPort = NULL;
    return;
  }
  inputHandler.is_Node.ln_Type = NT_INTERRUPT;
  inputHandler.is_Node.ln_Pri = 51;
  inputHandler.is_Node.ln_Name = "WarpQuake2 mouse";
  inputHandler.is_Data = NULL;
  inputHandler.is_Code = (void (*)())mouseHandler;
  inputReq->io_Command = IND_ADDHANDLER;
  inputReq->io_Data = &inputHandler;
  DoIO((struct IORequest *)inputReq);
  rawDx = rawDy = 0;
  inputHandlerOn = TRUE;
}

// Also from the atexit() cleanup: safe to call twice.
void qaInputStop(void)
{
  if(inputHandlerOn) {
    inputReq->io_Command = IND_REMHANDLER;
    inputReq->io_Data = &inputHandler;
    DoIO((struct IORequest *)inputReq);
    inputHandlerOn = FALSE;
  }
  if(inputReq != NULL) {
    CloseDevice((struct IORequest *)inputReq);
    DeleteIORequest((struct IORequest *)inputReq);
    inputReq = NULL;
  }
  if(inputPort != NULL) {
    DeleteMsgPort(inputPort);
    inputPort = NULL;
  }
}

int qaInputBreak(void)
{
  return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) ? 1 : 0;
}

static void queueKey(int code, int down)
{
  if(keyHead - keyTail >= KEYQUEUE_SIZE) {
    return;
  }
  keyQueue[keyHead % KEYQUEUE_SIZE].code = (UWORD)code;
  keyQueue[keyHead % KEYQUEUE_SIZE].down = (UBYTE)down;
  keyHead++;
}

// The window's messages into the key queue and the IDCMP mouse counts.
static void drainWindow(void)
{
  struct IntuiMessage *msg;

  if(qaWindow == NULL) {
    return;
  }
  while((msg = (struct IntuiMessage *)GetMsg(qaWindow->UserPort)) != NULL) {
    ULONG cls = msg->Class;
    UWORD code = msg->Code;
    WORD mx = msg->MouseX;
    WORD my = msg->MouseY;

    ReplyMsg((struct Message *)msg);

    switch(cls) {
    case IDCMP_RAWKEY:
      queueKey(code & 0x7F, !(code & IECODE_UP_PREFIX));
      break;

    case IDCMP_MOUSEBUTTONS:
      switch(code) {
      case SELECTDOWN: queueKey(QA_MOUSE1, 1); break;
      case SELECTUP: queueKey(QA_MOUSE1, 0); break;
      case MENUDOWN: queueKey(QA_MOUSE2, 1); break;
      case MENUUP: queueKey(QA_MOUSE2, 0); break;
      case MIDDLEDOWN: queueKey(QA_MOUSE3, 1); break;
      case MIDDLEUP: queueKey(QA_MOUSE3, 0); break;
      }
      break;

    case IDCMP_MOUSEMOVE:
      // IDCMP_DELTAMOVE: MouseX/Y are the motion, not a position
      if(windowActive) {
        mouseDx += mx;
        mouseDy += my;
      }
      break;

    case IDCMP_ACTIVEWINDOW:
      windowActive = TRUE;
      break;

    case IDCMP_INACTIVEWINDOW:
      windowActive = FALSE;
      mouseDx = mouseDy = 0;
      break;
    }
  }
}

int qaInputKey(int *code, int *down)
{
  if(keyTail == keyHead) {
    drainWindow();
  }
  if(keyTail == keyHead) {
    return 0;
  }
  *code = keyQueue[keyTail % KEYQUEUE_SIZE].code;
  *down = keyQueue[keyTail % KEYQUEUE_SIZE].down;
  keyTail++;
  return 1;
}

void qaInputMouse(int *dx, int *dy)
{
  drainWindow();
  if(inputHandlerOn) {
    Forbid();
    *dx = rawDx;
    *dy = rawDy;
    rawDx = rawDy = 0;
    Permit();
    mouseDx = mouseDy = 0;
    return;
  }
  *dx = mouseDx;
  *dy = mouseDy;
  mouseDx = mouseDy = 0;
}

int qaInputActive(void)
{
  return windowActive;
}
