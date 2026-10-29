// WarpQuake2 - the network: UDP sockets on bsdsocket.library (Roadshow, or
// whatever TCP/IP stack is running), for net_udp.c's NET_* calls.
//
// The library is opened only when a game goes multiplayer (NET_Config), so a
// single player game never needs a TCP/IP stack.  Sockets are non-blocking:
// Quake polls them once a frame.

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <netinet/in.h>
#include <netdb.h>

#include "amiga_q2.h"

#define MAX_SOCKETS 4                  // Quake opens two: the server's and the client's

struct Library *SocketBase = NULL;

static LONG sockets[MAX_SOCKETS];
static int nSockets;

int qaNetStart(void)
{
  if(SocketBase == NULL) {
    SocketBase = OpenLibrary("bsdsocket.library", 4);
  }
  return SocketBase != NULL;
}

// the stack's own name (Roadshow, warpSOCKET, ...), its id string less the line end
const char *qaNetInfo(void)
{
  static char info[80];
  int n;

  if(SocketBase == NULL || SocketBase->lib_IdString == NULL) {
    return "bsdsocket.library";
  }
  strncpy(info, (const char *)SocketBase->lib_IdString, sizeof(info) - 1);
  info[sizeof(info) - 1] = 0;
  for(n = (int)strlen(info); n > 0 && (info[n - 1] == '\n' || info[n - 1] == '\r'); n--) {
    info[n - 1] = 0;
  }
  return info;
}

int qaNetOpen(int port)
{
  struct sockaddr_in a;
  LONG s, one = 1;

  if(SocketBase == NULL || nSockets == MAX_SOCKETS) {
    return -1;
  }
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s < 0) {
    printf("UDP: no socket (error %ld)\n", Errno());
    return -1;
  }
  IoctlSocket(s, FIONBIO, (char *)&one);
  setsockopt(s, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = INADDR_ANY;
  a.sin_port = (UWORD)(port < 0 ? 0 : port);
  if(bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
    printf("UDP: cannot bind port %d (error %ld)\n", port, Errno());
    CloseSocket(s);
    return -1;
  }
  sockets[nSockets++] = s;
  return (int)s;
}

void qaNetClose(int s)
{
  int i;

  for(i = 0; i < nSockets; i++) {
    if(sockets[i] == s) {
      CloseSocket(s);
      sockets[i] = sockets[--nSockets];
      return;
    }
  }
}

int qaNetSend(int s, const unsigned char ip[4], int port, const void *data, int len)
{
  struct sockaddr_in a;

  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  memcpy(&a.sin_addr.s_addr, ip, 4);
  a.sin_port = (UWORD)port;
  return (int)sendto(s, (APTR)data, len, 0, (struct sockaddr *)&a, sizeof(a));
}

int qaNetRecv(int s, unsigned char ip[4], int *port, void *buf, int max)
{
  struct sockaddr_in a;
  socklen_t alen = sizeof(a);
  LONG n = recvfrom(s, buf, max, 0, (struct sockaddr *)&a, &alen);

  if(n < 0) {
    LONG e = Errno();

    // nothing waiting, or an ICMP "unreachable" for an earlier send
    if(e != EWOULDBLOCK && e != ECONNREFUSED) {
      printf("UDP: receive error %ld\n", e);
    }
    return -1;
  }
  memcpy(ip, &a.sin_addr.s_addr, 4);
  *port = a.sin_port;
  return (int)n;
}

int qaNetResolve(const char *name, unsigned char ip[4])
{
  struct hostent *h;

  if(!qaNetStart()) {
    return 0;
  }
  h = gethostbyname((STRPTR)name);
  if(h == NULL || h->h_addrtype != AF_INET || h->h_addr_list[0] == NULL) {
    return 0;
  }
  memcpy(ip, h->h_addr_list[0], 4);
  return 1;
}

void qaNetWait(int s, int msec)
{
  struct timeval tv;
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(s, &fds);
  tv.tv_sec = msec / 1000;
  tv.tv_usec = (msec % 1000) * 1000;
  WaitSelect(s + 1, &fds, NULL, NULL, (void *)&tv, NULL);   // its prototype names the type in its own scope
}

// Also from the atexit() cleanup: safe to call twice.
void qaNetStop(void)
{
  while(nSockets > 0) {
    CloseSocket(sockets[--nSockets]);
  }
  if(SocketBase != NULL) {
    CloseLibrary(SocketBase);
    SocketBase = NULL;
  }
}
