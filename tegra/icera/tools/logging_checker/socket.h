/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

#ifndef _CHECKER_SOCKET_H_
#define _CHECKER_SOCKET_H_

#if defined (__linux__) || defined (__APPLE__)
# include <errno.h>
# include <arpa/inet.h>
# include <unistd.h>
#endif
#if defined (_WIN32)
# include <winsock2.h>
#endif
#include <stdio.h>


 /** Socket handle. */
#if defined (__linux__) || defined (__APPLE__)
typedef  int    IcSocketHdl;
#define IC_SOCKET_INVALID  (-1)
#else
typedef  SOCKET IcSocketHdl;
#define IC_SOCKET_INVALID  (INVALID_SOCKET)
#endif


class Socket
{
private:
#if defined (_WIN32)
   WSADATA  m_wsaData;
#endif
   IcSocketHdl  socket_handle;

public:
   Socket();
   ~Socket();
   bool OpenClient(int port_num, int rxBufLen, int txBufLen);
#if defined (__linux__) || defined (__APPLE__)
    static bool   IsOpen(IcSocketHdl socket) { return (socket >= 0); }
#elif defined (_WIN32)
    static bool   IsOpen(IcSocketHdl socket) { return (socket != INVALID_SOCKET); }
#endif
   bool   IsOpen(void) { return (IsOpen(socket_handle)); }
   void   Close(void);
   int    Read(char *buff, size_t len);
};

#endif
