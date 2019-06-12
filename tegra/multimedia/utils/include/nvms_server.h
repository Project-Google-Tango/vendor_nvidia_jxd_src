/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _NVMS_SERVER_H_
#define _NVMS_SERVER_H_

#include <nvms_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Opaque Handle type for Media Streaming Server
 */
typedef void* NvMS_Server;

/**
 * \brief Opaque Handle type for Media Streaming Server Source
 */
typedef void* NvMS_ServerSource;

/**
 * \brief Opaque Handle type for Media Streaming Application
 */
typedef void* NvMS_ServerApp;

/**
 * \brief Server message type enumeration
 */
typedef enum {
    /*! This is used by server to send out information strings */
    NVMS_SERVER_MSG_INFO,
    /*! This is used by server to indicate a fatal error */
    NVMS_SERVER_MSG_ERROR,
    /*! This is used by server to indicate error for a source */
    NVMS_SERVER_MSG_SOURCE_ERROR,
    /*! This is used by server to indicate sources death */
    NVMS_SERVER_MSG_SOURCE_GONE,
    /*! This is used by server to indicate connection availablility at port */
    NVMS_SERVER_MSG_PORT_AVAILABLE,
    /*! This is used by server to indicate error for a port */
    NVMS_SERVER_MSG_PORT_ERROR,

    NVMS_SERVER_MSG_UND = 0x7FFFFFFF
} NvMS_ServerMsgType;

/**
 * \brief Server message structure
 */
typedef struct {
    /*! Generic pointer for message from server
        - can be interpreted based on message type */
    void *pMsg;
    /*! Application handle */
    NvMS_ServerApp hApp;
    /*! Message type */
    NvMS_ServerMsgType eType;
} NvMS_ServerMsg;

/**
 *  \brief Callback for messages from server implementation
 *
 *  \param[in] pMsg Callback message pointer
 *  \return void
 */
typedef NvMS_Status
(*NvMS_ServerCb)(NvMS_ServerMsg *pMsg);

/**
 * \brief Protocol used for data channel for media streaming
 */
typedef enum {
    /*! TCP */
    NVMS_DATA_PROTOCOL_TYPE_TCP,
    /*! UDP unicast */
    NVMS_DATA_PROTOCOL_TYPE_UDP_UCAST,
    /*! UDP multicast */
    NVMS_DATA_PROTOCOL_TYPE_UDP_MCAST,

    NVMS_DATA_PROTOCOL_TYPE_UND = 0x7FFFFFFF
} NvMS_DataProtocol;

/**
 * \brief Protocol used for session channel media streaming
 */
typedef enum {
    /*! Miracast */
    NVMS_SESSION_PROTOCOL_TYPE_MIRACAST,
    /*! Undefined - This is used to indicate that no
      * server is not supposed to use any session channel */
    NVMS_SESSION_PROTOCOL_TYPE_UND = 0x7FFFFFFF
} NvMS_SessionProtocol;

/**
 * \brief Data Channel Configuration
 */
typedef struct {
    /*! Port for media streaming */
    unsigned int uPort;
    /*! Source IP address(unicast) /
        Membership address(multicast)
        to stream the data to */
    char *pClientAddress;
    /*! Protocol to be used for data channel */
    NvMS_DataProtocol eProtocol;
} NvMS_DataConfig;

/**
 * \brief Session Channel Configuration
 */
typedef struct {
    /*! Port for media streaming */
    unsigned int uPort;
    /*! Client IP address for session management */
    char *pClientAddress;
    /*! Protocol to be used for session channel */
    NvMS_SessionProtocol eProtocol;
} NvMS_SessionConfig;

/**
 * \brief Network Configuration for a port
 */
typedef struct {
    /*! Data Channel Configuration */
    NvMS_DataConfig oDataConfig;
    /*! Session Channel Configuration */
    NvMS_SessionConfig oSessionConfig;
} NvMS_NetworkConfig;

/**
 * \brief Configuration for transport stream muxer
 */
typedef struct {
    unsigned int uPidPCR;
    unsigned int uPidPMT;
    unsigned int uPidStream[NVMS_MAX_NUM_STREAMS];
    unsigned int bDisableMux;
} NvMS_MuxConfig;

/**
 * \brief Port Configuration
 */
typedef struct {
    /*! Stream Configuration */
    NvMS_StreamConfig oStreamConfig;
    /*! Network Configuration */
    NvMS_NetworkConfig oNetworkConfig;
    /*! Muxer Configuration */
    NvMS_MuxConfig oMuxConfig;
} NvMS_PortConfig;

/**
 * \brief Streamer Server Configuration
 */
typedef struct {
    /*! Max number of ports allowed */
    unsigned int uMaxNumPorts;
    /*! Max number of sources allowed */
    unsigned int uMaxNumSources;
    /*! Base path for all IPC communication */
    char *pBasePath;
    /*! Callback handler */
    NvMS_ServerCb hCb;
    /*! Application handle */
    NvMS_ServerApp hApp;
} NvMS_ServerConfig;

/**
 * \brief Create a server instance
 *
 * \param[out] phServer Pointer to server handle
 * \param[in] pConfig Pointer to server configuration
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerOpen(NvMS_Server *phServer,
                NvMS_ServerConfig *pConfig);

/**
 * \brief Destroys a server instance
 *
 * \param[in] phServer pointer to server handle
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerClose(NvMS_Server *phServer);

/**
 * \brief Create a port instance
 *
 * \param[in] hServer server handle
 * \param[in] pConfig pointer to port configuration
 * \param[out] phPort pointer to port handle
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerCreatePort(NvMS_Server hServer,
                      NvMS_Port *phPort,
                      NvMS_PortConfig *pConfig);

/**
 * \brief Destroy a port instance
 *
 * \param[in] hServer Server handle
 * \param[in] phPort Pointer to port handle
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerDestroyPort(NvMS_Server hServer,
                       NvMS_Port *phPort);

/**
 * \brief Set routing table
 *
 * \param[in] hServer server handle
 * \param[in] pRoutes pointer to route table
 * \param[in] uNumRoutes number of entries in route table
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerSetRoute(NvMS_Server hServer,
                    NvMS_Route *pRoutes,
                    unsigned int uNumRoutes);

/**
 * \brief Start streaming on a port
 *
 * \param[in] hServer Server handle
 * \param[in] hPort Port handle
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerStartStreaming(NvMS_Server hServer,
                          NvMS_Port hPort);

/**
 * \brief Stop streaming on a port
 *
 * \param[in] hServer Server handle
 * \param[in] hPort Port handle
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerStopStreaming(NvMS_Server hServer,
                         NvMS_Port hPort);

/**
 * \brief Create a data source
 *
 * \param[in] hServer Server handle
 * \param[out] phSource Pointer to source handle
 * \param[in] pConfig Pointer to source config
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerCreateSource(NvMS_Server hServer,
                        NvMS_ServerSource *phSource,
                        NvMS_BufferConfig *pConfig);

/**
 * \brief destroy a data source
 *
 * \param[in] hServer Server handle
 * \param[out] phSource Pointer to source handle
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerDestroySource(NvMS_Server hServer,
                        NvMS_ServerSource *phSource);

/**
 * \brief destroy a data source
 *
 * \param[in] hServer Server handle
 * \param[out] phSource Pointer to source handle
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_ServerConfigureSource(NvMS_Server hServer,
                           NvMS_ServerSource hSource,
                           NvMS_BufferConfig *pConfig);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* _NVMS_SERVER_H_ */

