// WarpQuake2 - the network: id's loopback buffers between the local client
// and server, and UDP over IPv4 for multiplayer (linux/net_udp.c's NET_*
// calls on amiga_net.c's sockets).
//
// The sockets open when a game goes multiplayer (NET_Config: a server with
// maxclients above 1, or a connect to another machine) and close when it goes
// back to single player; until then no TCP/IP stack is needed.  As id's, the
// server listens on "port" (27910) and the client on "clientport" (27901, or
// any port if that one is taken).  netadr_t keeps the port in network order.

#include "../quake2/qcommon/qcommon.h"
#include "amiga_q2.h"

#define MAX_LOOPBACK 4

typedef struct {
  byte data[MAX_MSGLEN];
  int datalen;
} loopmsg_t;

typedef struct {
  loopmsg_t msgs[MAX_LOOPBACK];
  int get, send;
} loopback_t;

static loopback_t loopbacks[2];
static netadr_t net_local_adr;

static int ip_sockets[2] = {-1, -1};   // NS_CLIENT, NS_SERVER
static qboolean net_multiplayer;

qboolean NET_CompareAdr(netadr_t a, netadr_t b)
{
  if(a.type != b.type) {
    return false;
  }
  if(a.type == NA_LOOPBACK) {
    return true;
  }
  return (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
           ? true
           : false;
}

qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b)
{
  if(a.type != b.type) {
    return false;
  }
  if(a.type == NA_LOOPBACK) {
    return true;
  }
  return (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3]) ? true : false;
}

char *NET_AdrToString(netadr_t a)
{
  static char s[64];

  if(a.type == NA_LOOPBACK) {
    Com_sprintf(s, sizeof(s), "loopback");
  } else {
    Com_sprintf(s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], (unsigned short)BigShort(a.port));
  }
  return s;
}

// "localhost", "a.b.c.d" or a host name, each with an optional ":port"
// (port 0 when there is none: the callers put in the default)
qboolean NET_StringToAdr(char *s, netadr_t *a)
{
  char name[128], *colon;
  int i, n, part[4];
  qboolean dotted = true;

  memset(a, 0, sizeof(*a));
  if(!strcmp(s, "localhost")) {
    a->type = NA_LOOPBACK;
    return true;
  }
  strncpy(name, s, sizeof(name) - 1);
  name[sizeof(name) - 1] = 0;
  colon = strchr(name, ':');
  if(colon != NULL) {
    *colon = 0;
    a->port = BigShort((short)atoi(colon + 1));
  }
  // dotted quads by hand: a name needs the stack's resolver
  n = 0;
  part[0] = 0;
  for(i = 0; name[i] != 0 && dotted; i++) {
    if(name[i] >= '0' && name[i] <= '9') {
      part[n] = part[n] * 10 + name[i] - '0';
      dotted = part[n] < 256;
    } else if(name[i] == '.' && n < 3 && i > 0 && name[i - 1] != '.') {
      part[++n] = 0;
    } else {
      dotted = false;
    }
  }
  if(dotted && n == 3 && i > 0 && name[i - 1] != '.') {
    for(i = 0; i < 4; i++) {
      a->ip[i] = (byte)part[i];
    }
  } else if(!qaNetResolve(name, a->ip)) {
    return false;
  }
  a->type = NA_IP;
  return true;
}

qboolean NET_IsLocalAddress(netadr_t adr)
{
  return adr.type == NA_LOOPBACK ? true : false;
}

static qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
  int i;
  loopback_t *loop;

  loop = &loopbacks[sock];

  if(loop->send - loop->get > MAX_LOOPBACK) {
    loop->get = loop->send - MAX_LOOPBACK;
  }
  if(loop->get >= loop->send) {
    return false;
  }

  i = loop->get & (MAX_LOOPBACK - 1);
  loop->get++;

  memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
  net_message->cursize = loop->msgs[i].datalen;
  *net_from = net_local_adr;
  return true;
}

static void NET_SendLoopPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
  int i;
  loopback_t *loop;

  loop = &loopbacks[sock ^ 1];

  i = loop->send & (MAX_LOOPBACK - 1);
  loop->send++;

  memcpy(loop->msgs[i].data, data, length);
  loop->msgs[i].datalen = length;
}

qboolean NET_GetPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
  int n, port;

  if(NET_GetLoopPacket(sock, net_from, net_message)) {
    return true;
  }
  if(ip_sockets[sock] < 0) {
    return false;
  }
  memset(net_from, 0, sizeof(*net_from));
  n = qaNetRecv(ip_sockets[sock], net_from->ip, &port, net_message->data, net_message->maxsize);
  if(n < 0) {
    return false;
  }
  net_from->type = NA_IP;
  net_from->port = BigShort((short)port);
  if(n == net_message->maxsize) {
    Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
    return false;
  }
  net_message->cursize = n;
  return true;
}

void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
  static const byte broadcast[4] = {255, 255, 255, 255};

  if(to.type == NA_LOOPBACK) {
    NET_SendLoopPacket(sock, length, data, to);
    return;
  }
  if(ip_sockets[sock] < 0) {
    return;
  }
  if(to.type == NA_BROADCAST) {
    qaNetSend(ip_sockets[sock], broadcast, (unsigned short)BigShort(to.port), data, length);
  } else if(to.type == NA_IP) {
    qaNetSend(ip_sockets[sock], to.ip, (unsigned short)BigShort(to.port), data, length);
  }
  // IPX: there is none
}

static void NET_OpenIP(void)
{
  cvar_t *port;

  if(!qaNetStart()) {
    Com_Printf("No TCP/IP stack (bsdsocket.library): local games only\n");
    return;
  }
  Com_Printf("UDP: %s\n", qaNetInfo());
  if(ip_sockets[NS_SERVER] < 0) {
    port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET);
    ip_sockets[NS_SERVER] = qaNetOpen((int)port->value);
  }
  if(ip_sockets[NS_CLIENT] < 0) {
    port = Cvar_Get("clientport", va("%i", PORT_CLIENT), CVAR_NOSET);
    ip_sockets[NS_CLIENT] = qaNetOpen((int)port->value);
    if(ip_sockets[NS_CLIENT] < 0) {
      ip_sockets[NS_CLIENT] = qaNetOpen(PORT_ANY);
    }
  }
}

// multiplayer opens the sockets, single player closes them (and the stack)
void NET_Config(qboolean multiplayer)
{
  if(multiplayer == net_multiplayer) {
    return;
  }
  net_multiplayer = multiplayer;
  if(multiplayer) {
    NET_OpenIP();
  } else {
    qaNetStop();
    ip_sockets[NS_CLIENT] = ip_sockets[NS_SERVER] = -1;
  }
}

void NET_Init(void)
{
  net_local_adr.type = NA_LOOPBACK;
}

void NET_Shutdown(void)
{
  NET_Config(false);
}

// A dedicated server's idle wait: until a packet comes or msec pass.
void NET_Sleep(int msec)
{
  extern cvar_t *dedicated;

  if(ip_sockets[NS_SERVER] < 0 || (dedicated && !dedicated->value)) {
    return;
  }
  qaNetWait(ip_sockets[NS_SERVER], msec);
}
