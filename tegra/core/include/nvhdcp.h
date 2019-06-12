/*
 * Copyright (c) 2010 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA HDCP Upstream Library</b>
 *
 * @b Description: Defines the API for the HDCP Upstream Library.
 *
 */

#ifndef INCLUDED_NVHDCP_H
#define INCLUDED_NVHDCP_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvhdcp_group HDCP Upstream Library
 *
 * This API describes an example Upstream library for High-Bandwidth
 * Digital Content Protection (HDCP):
 * - Cn-Client session ID (64-bit random value)
 * - Cksv-Client KSV
 * - Dksv-Server KSV
 *
 * @{
 */

/// Maximum number of ports/attach points supported in HDCP connection state.
#define NV_MAX_NUM_AP               1
/// Maximum number of receiver and repeater devices.
#define NV_MAX_NUM_DEVICES          127

typedef NvU64 NV_HDCP_CN;
#define NV_HDCP_CN_SESSION_ID       36:0
#define NV_HDCP_CN_DISPLAY          39:37
#define NV_HDCP_CN_RESERVED         63:40

/// Structure of the data returned from the monitor, as defined in HDCP spec.
typedef NvU32 NV_HDCP_BSTATUS;
#define NV_HDCP_BSTATUS_DEVICE_COUNT            6:0
#define NV_HDCP_BSTATUS_MAX_DEVICES_EXCEEDED    7:7
#define NV_HDCP_BSTATUS_REPEATER_DEPTH          10:8
#define NV_HDCP_BSTATUS_MAX_CASCADE_EXCEEDED    11:11
#define NV_HDCP_BSTATUS_HDMI_MODE               12:12
#define NV_HDCP_BSTATUS_RESERVED                31:13

/// The connection state.
typedef NvU64 NV_HDCP_CS;
#define NV_HDCP_CS_ATTACH_POINTS                15:0
#define NV_HDCP_CS_NON_HDCP                     16:16
#define NV_HDCP_CS_HEAD_INDEX                   20:17
#define NV_HDCP_CS_RFU_PLANES                   28:21
#define NV_HDCP_CS_NUMBER_OF_ACTIVE_HEADS       30:29
#define NV_HDCP_CS_RESERVED_2                   39:31
#define NV_HDCP_CS_ATTACHED_PLANES              47:40
#define NV_HDCP_CS_CLONE_MODE                   48:48
#define NV_HDCP_CS_SPAN_MODE                    49:49
#define NV_HDCP_CS_PADDING                      63:50

/// The status of the attachment point (HDCP-capable or not).
#define NV_HDCP_STATUS_ENCRYPTING               0:0
#define NV_HDCP_STATUS_REPEATER                 1:1
#define NV_HDCP_STATUS_USER_ACCESSIBLE          2:2
#define NV_HDCP_STATUS_EXT_UNPROTECTED          3:3
#define NV_HDCP_STATUS_PORT_INDEX               7:4
#define NV_HDCP_STATUS_NUM_PORTS                11:8
#define NV_HDCP_STATUS_INTERNAL_PANEL           12:12
#define NV_HDCP_STATUS_WIDE_SCOPE               13:13
#define NV_HDCP_STATUS_HAS_CS                   14:14
#define NV_HDCP_STATUS_READ_Z                   15:15
#define NV_HDCP_STATUS_RESERVED                 39:16
#define NV_HDCP_STATUS_DUAL_LINK_EVEN           40:40
#define NV_HDCP_STATUS_DUAL_LINK_ODD            41:41
#define NV_HDCP_STATUS_DUAL_LINK_CAPABLE        42:42
#define NV_HDCP_STATUS_PADDING                  63:43

/// Holds the status of the attachment point.
typedef struct
{
    NvU64 status;
    NvU32 DisplayId;
} NV_HDCP_STATUS;

/// Defines flags used for indicating active member elements.
typedef enum
{
    NV_HDCP_FLAGS_NULL          = 0x00000000, /**< Gets the attach point status.
                                              */
    NV_HDCP_FLAGS_APINDEX       = 0x00000001, /**< Index of attach point. */
    NV_HDCP_FLAGS_AN            = 0x00000010, /**< Downstream session ID. */
    NV_HDCP_FLAGS_AKSV          = 0x00000020, /**< Downstream/Xmtr KSV. */
    NV_HDCP_FLAGS_BKSV          = 0x00000040, /**< Downstream/Rcvr KSV. */
    NV_HDCP_FLAGS_BSTATUS       = 0x00000080, /**< Link/repeater status. */
    NV_HDCP_FLAGS_CN            = 0x00000100, /**< Upstream session ID. */
    NV_HDCP_FLAGS_CKSV          = 0x00000200, /**< Upstream client application
                                              KSV. */
    NV_HDCP_FLAGS_DKSV          = 0x00000400, /**< Upstream/Xmtr KSV. */
    NV_HDCP_FLAGS_KP            = 0x00001000, /**< Signature. */
    NV_HDCP_FLAGS_S             = 0x00002000, /**< Status. */
    NV_HDCP_FLAGS_CS            = 0x00004000, /**< Connection state. */
    NV_HDCP_FLAGS_V             = 0x00010000, /**< V of the KSVList. */
    NV_HDCP_FLAGS_MP            = 0x00020000, /**< Encrypted initializer for KSV
                                              list. */
    NV_HDCP_FLAGS_BKSVLIST      = 0x00040000, /**< NumKsvs & BksvList[NumKsvs]
                                              */
    NV_HDCP_FLAGS_DUAL_LINK     = 0x00100000, /**< Two sets of An, Aksv, Kp,
                                              Bksv, Dksv. */
    NV_HDCP_FLAGS_HDCP_AVAILABLE= (int)0x80000000, /**< Display supports HDCP.
                                                   */
} NV_HDCP_FLAGS;

/// Specifies the HDCP commands.
typedef enum
{
    NV_HDCP_CMD_NULL                    = 0x00, /**< Null command. */
    NV_HDCP_CMD_READ_LINK_STATUS        = 0x02, /**< Gets the status. */
    NV_HDCP_CMD_VALIDATE_LINK           = 0x03, /**< Gets M & V. */
} NV_HDCP_COMMANDS;

/// Specifies the HDCP return status values.
typedef enum
{
    /// Indicates the function completed successfully.
    NV_HDCP_STATUS_SUCCESS      = (int)(0x00000000L),

    /// Indicates the function failed.
    NV_HDCP_STATUS_UNSUCCESSFUL = (int)(0xC0000001L),

    /// Indicates renegotiation is not complete; check status later.
    NV_HDCP_STATUS_PENDING      = (0x00000103),

    /// Indicates renegotiation could not complete.
    NV_HDCP_STATUS_LINK_FAILED  = (int)(0xC000013EL),

    /// Indicates one or more of the calling parameters was invalid.
    NV_HDCP_STATUS_INVALID_PARAMETER = (int)(0xC000000DL),

    /// Indicates the combination of \a flFlags was invalid.
    NV_HDCP_STATUS_INVALID_PARAMETER_MIX = (int)(0xC0000030),

    /// Insufficient buffer space was allocated; re-allocate using the size
    /// returned in the \a dwSize member.
    NV_HDCP_STATUS_NO_MEMORY    = (int)(0xC0000017),

    /// Indicates the session ID &/or KSV supplied were rejected.
    NV_HDCP_STATUS_BAD_TOKEN_TYPE = (int)(0xC00000A8),
} NV_HDCP_RET_STATUS;

/// Holds the HDCP packet.
typedef struct
{
    NV_HDCP_COMMANDS    cmdCommand;         /**< [IN] */
    NV_HDCP_FLAGS       flFlags;            /**< [IN/OUT] */
    NV_HDCP_RET_STATUS  hdcpPacketStatus;   /**< [OUT] */

    /// [IN] Holds the attach point index.
    NvU32               apIndex;

    /// [IN] Holds the client session ID.
    NV_HDCP_CN          cN;
    NvU64               cKsv;               /**< [IN] */

    NV_HDCP_BSTATUS     bStatus[NV_MAX_NUM_AP];     /**< [OUT] */
    NV_HDCP_STATUS      hdcpStatus[NV_MAX_NUM_AP];  /**< [OUT] */

    /// [OUT] Holds the connection state.
    NV_HDCP_CS          cS;

    /// [OUT] Holds the KPRIME value.
    NvU64              kP[2];
    NvU64              aN[2];               /**< [OUT] */
    NvU64              aKsv[2];             /**< [OUT] */
    NvU64              bKsv[2];             /**< [OUT] */
    NvU64              dKsv[2];             /**< [OUT] */

    /// [OUT] Holds the VPRIME value.
    NvU8               vP[20];
    /// [OUT] Holds the MPRIME value.
    NvU64              mP;

    /// [OUT] Holds valid KSVs in the \a bKsvList; maximum is 127 devices.
    NvU32              numBKSVs;

    /// [OUT] Holds up to 127 receivers and repeaters.
    NvU64              bKsvList[NV_MAX_NUM_DEVICES];
} NV_HDCP_PACKET;

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
