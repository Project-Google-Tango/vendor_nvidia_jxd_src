/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

#include <string.h>
#include "socket.h"
#include "logs.h"

//-------------------------------------------------------------------------------------------
Socket::Socket()
{
   socket_handle = IC_SOCKET_INVALID;
#if defined (_WIN32)
   WSAStartup(MAKEWORD(2,2), &m_wsaData);
#endif
}

//-------------------------------------------------------------------------------------------
Socket::~Socket()
{
#if defined (_WIN32)
   WSACleanup();
#endif
}

//-------------------------------------------------------------------------------------------
void Socket::Close(void)
{
   if (IsOpen() == true)
   {
   #if defined (__linux__) || defined (__APPLE__)
      shutdown(socket_handle, SHUT_RDWR);
      ::close (socket_handle);
   #else
      shutdown(socket_handle, SD_BOTH);
      closesocket(socket_handle);
   #endif
   }
   socket_handle = IC_SOCKET_INVALID;
}
//-------------------------------------------------------------------------------------------
/** Open socket as a client. */
bool Socket::OpenClient(int port_num, int rxBufLen, int txBufLen)
{
    // Create socket
#if defined (__linux__) || defined (__APPLE__)
    socket_handle = socket(PF_INET, SOCK_STREAM, 0);
#else
    socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    if (IsOpen(socket_handle) == true)
    {
        // Connect to socket
        sockaddr_in tempAddr;
        memset(&tempAddr, 0, sizeof(sockaddr_in));
        tempAddr.sin_port        = htons((unsigned short)port_num);
        tempAddr.sin_family      = AF_INET;
        tempAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(socket_handle, (struct sockaddr *)&tempAddr, sizeof(tempAddr)) != 0)
        {   // Connect failed
#if defined (__linux__) || defined (__APPLE__)
            ::close(socket_handle);
#else
            closesocket(socket_handle);
#endif
            socket_handle = IC_SOCKET_INVALID;
            /* Failed to connect to socket */
        }
        else
        {
            int ret = 0;

            // Set any buffer lengths
            if (rxBufLen != 0)
                ret = setsockopt(socket_handle, SOL_SOCKET, SO_RCVBUF, (char*)&rxBufLen, sizeof(int));
            if (txBufLen != 0)
                ret = setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF, (char*)&txBufLen, sizeof(int));

            if (ret == -1)
            {
#if defined (__linux__) || defined (__APPLE__)
                ::close(socket_handle);
#else
                closesocket(socket_handle);
#endif
                socket_handle = IC_SOCKET_INVALID;
            }
        }
    }

    return (IsOpen());
}

//-------------------------------------------------------------------------------------------
int Socket::Read(char *buff, size_t len)
{
    int returnVal = 0;

    if (IsOpen())
    {
        returnVal = recv(socket_handle, buff, len, 0);
    }

    return (returnVal);
}

//-------------------------------------------------------------------------------------------
