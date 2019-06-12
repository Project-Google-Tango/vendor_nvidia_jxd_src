/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMU_SOCKET_H__
#define __NVMU_SOCKET_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmu_types.h"

#define NVMU_SOCKET_MAX_PATH_LEN (100)

/**
 * \brief Socket server handle type
 */
typedef void* NvMU_SocketServer;

/**
 * \brief Socket client handle type
 */
typedef void* NvMU_SocketClient;

/**
 * \brief Socket application context type
 */
typedef void* NvMU_SocketAppCtx;

/**
 * \brief Socket application connnection handle for a client
 */
typedef void* NvMU_SocketAppConn;

/**
 * \brief Socket implementation connnection handle for a client
 */
typedef void* NvMU_SocketImplConn;

/**
 * \brief Socket callback message type
 */
typedef enum {
    /*! Indicates: client has requested for connection
     *  Application is expected to set configuration to create a client
     *  handler
     */
    NVMU_SOCKET_SERVER_MSG_CLIENT_AVAILABLE,
    /*! Indicates: A client handler thread for client has been created
     *  Application is expected to set application side connection handle
     */
    NVMU_SOCKET_SERVER_MSG_CLIENT_HANDLE,
    /*! Indicates: Message from client is available */
    NVMU_SOCKET_SERVER_MSG_CLIENT_MESSAGE,
    /*! Indicates: Client communication has run into an error */
    NVMU_SOCKET_SERVER_MSG_CLIENT_ERROR,
    /*! Indicates: Client has closed connection */
    NVMU_SOCKET_SERVER_MSG_CLIENT_GONE,
    /*! Indicates: Server has run into an internal error */
    NVMU_SOCKET_SERVER_MSG_SERVER_ERROR,

    /*! Indicates: Message from server is available for client */
    NVMU_SOCKET_CLIENT_MSG_SERVER_MESSAGE,
    /*! Indicates: Communication with server has run into an error */
    NVMU_SOCKET_CLIENT_MSG_SERVER_ERROR,
    /*! Indicates: Server has terminated connection */
    NVMU_SOCKET_CLIENT_MSG_SERVER_GONE,
    /*! Indicates: Client has run into an internal error */
    NVMU_SOCKET_CLIENT_MSG_CLIENT_ERROR,
    NVMU_SOCKET_MSG_UND = 0x7FFFFFFF
} NvMU_SocketMsgType;

/**
 * \brief Message type for \ref NVMU_SOCKET_SERVER_MSG_CLIENT_AVAILABLE
 */
typedef struct {
    /*! Handle to application context */
    NvMU_SocketAppCtx   hCtx;
    /*! Handle to client configuration */
    void *pClientConfig;
} NvMU_SocketMsgClientAvail;

/**
 * \brief Message type for \ref NVMU_SOCKET_SERVER_MSG_CLIENT_HANDLE
 */
typedef struct {
    /*! Handle to application context */
    NvMU_SocketAppCtx   hCtx;
    /*! Handle to implementation's connection handle */
    NvMU_SocketImplConn hConnImpl;
    /*! Handle to application's connection handle */
    NvMU_SocketAppConn *phConnApp;
} NvMU_SocketMsgClientHandle;

/**
 * \brief Message type for \ref NVMU_SOCKET_SERVER_MSG_CLIENT_MESSAGE
 */
typedef struct {
    /*! Message data */
    void *pData;
    /*! Message data size */
    U32  uDataSize;
    /*! Handle to application context */
    NvMU_SocketAppCtx   hCtx;
    /*! Handle to implementation's connection handle */
    NvMU_SocketImplConn hConnImpl;
    /*! Handle to application's connection handle */
    NvMU_SocketAppConn  hConnApp;
} NvMU_SocketMsgPacket;

/**
 * \brief Socket callback message type
 */
typedef struct {
    /*! void pointer for various message types */
    void *pData;
    /*! Message type */
    NvMU_SocketMsgType eType;
} NvMU_SocketCbMsg;

/**
 * \brief Socket callback prototype
 */
typedef NvMU_Status
(*NvMU_SocketCb)(NvMU_SocketCbMsg *pMsg);

/**
 * \brief Socket server configuration
 */
typedef struct {
    /*! IPC path for communication */
    char *pPath;
    /*! (Optional) Diagnostics path */
    char *pDiagPath;
    /*! Maximum number of clients allowed */
    U32 uMaxNumClients;
    /*! Application context handle */
    NvMU_SocketAppCtx hApp;
    /*! Application callback */
    NvMU_SocketCb hCb;
} NvMU_SocketServerConfig;

/**
 * \brief Socket client configuration
 */
typedef struct {
    /*! IPC path for communication */
    char *pPath;
    /*! (Optional) Diagnostics path */
    char *pDiagPath;
    /*! Maximum size of expected message */
    U32 uRecvMsgSize;
    /*! Set to NVMU_TRUE if client wants to be notified of server messages */
    Bool bNeedListnerThread;
    /*! Application context handle */
    NvMU_SocketAppCtx hApp;
    /*! Application callback */
    NvMU_SocketCb hCb;
    /*! Verbosity control */
    Bool bVerbose;
} NvMU_SocketClientConfig;

/**
 * \brief Open a socket server instance
 *
 * \param[out] phServer pointer to server handle
 * \param[in] pConfig pointer to server config
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketServerOpen(NvMU_SocketServer *phServer, NvMU_SocketServerConfig *pConfig);

/**
 * \brief Close a socket server instance
 *
 * \param[out] phServer pointer to server handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketServerClose(NvMU_SocketServer *phServer);

/**
 * \brief Suspend server listener thread and all client handler threads
 *
 * \param[in] hServer pointer to server handle
 * \param[in] uTimeoutMs Time to wait for in milliseconds for operation to complete
 *            This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketServerSuspend(NvMU_SocketServer hServer, U32 uTimeoutMs);

/**
 * \brief Resume server listener thread and all client handlers
 *
 * \param[in] hServer server handle
 * \param[in] uTimeoutMs Time to wait for in milliseconds for operation to complete
 *            This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketServerResume(NvMU_SocketServer hServer, U32 uTimeoutMs);

/**
 * \brief Send a message packet to a client
 *
 * \param[in] hServer server handle
 * \param[in] pMsg pointer to message container. Contains client handle to point to client
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketServerSend(NvMU_SocketServer hServer, NvMU_SocketMsgPacket *pMsg);

/**
 * \brief Open a client instance
 *
 * \param[in] phClient pointer to client handle
 * \param[in] pConfig pointer to client configuration
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketClientOpen(NvMU_SocketClient *phClient, NvMU_SocketClientConfig *pConfig);

/**
 * \brief Close a client instance
 *
 * \param[in] phClient pointer to client handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketClientClose(NvMU_SocketClient *phClient);

/**
 * \brief Send a message to server
 *
 * \param[in] hClient Client handle
 * \param[in] pMsg pointer to message container
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketClientSend(NvMU_SocketClient hClient, NvMU_SocketMsgPacket *pMsg);

/**
 * \brief Recieve message from server. This fails if client thread is setup to recieve
 * messages froms server.
 *
 * \param[in] hClient Client handle
 * \param[in] pMsg pointer to message container
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SocketClientRecv(NvMU_SocketClient hClient, NvMU_SocketMsgPacket *pMsg);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* #ifndef __NVMU_SOCKET_H__ */
