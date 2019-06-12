/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVHDCP_UP_H
#define INCLUDED_NVHDCP_UP_H

#include "nvhdcp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* macros for accessing bit fields */
#define FLD_BASE(fld)           (0?fld)
#define FLD_EXTENT(fld)         (1?fld)
#define FLD64_SHIFTMASK(fld) \
    (0xFFFFFFFFFFFFFFFFULL >> (63 - (FLD_EXTENT(fld) - FLD_BASE(fld))) \
        << FLD_BASE(fld))
#define FLD32_SHIFTMASK(fld) \
    (0xFFFFFFFF >> (31 - (FLD_EXTENT(fld) - FLD_BASE(fld))) << FLD_BASE(fld))

/* 64-bit get and set accessors */
#define FLD64_SET(n,type,part,v) \
    (((NvU64)(v) << FLD_BASE(type##_##part)) | \
        ((~FLD64_SHIFTMASK(type##_##part)) & (n)))
#define FLD64_GET(n,type,part) \
    ((FLD64_SHIFTMASK(type##_##part) & (n)) >> FLD_BASE(type##_##part))

/* 32-bit get and set accessors */
#define FLD32_SET(n,type,part,v) \
    (((NvU32)(v) << FLD_BASE(type##_##part)) | \
        ((~FLD32_SHIFTMASK(type##_##part)) & (n)))
#define FLD32_GET(n,type,part) \
    ((FLD32_SHIFTMASK(type##_##part) & (n)) >> FLD_BASE(type##_##part))

typedef NvU64 NvU40;
typedef NvU64 NvU56;

#define NV_HDCP_GLOB_SIZE (((3 * 64) + (1 * 64) + (40 * 56)) / 8)
#define NV_HDCP_KEY_OFFSET (((3 * 64) + (1 * 64)) / 8)

/**
 * Returns a pointer to a decrypted glob for NVIDIA Upstream authentication. The
 * pointer must be de-allocated with NvOdmDispHdcpReleaseGlob.
 *
 * If this is called multiple times without first releasing the glob, then
 * this will fail (return NULL).
 *
 * This must decrypt the key glob into secure memory. If the memory cannot be
 * mapped, then this must return NULL.
 *
 * @param size Pointer to the size of the glob
 */
typedef void * (*NvOdmDispHdcpGetGlobType)( NvU32 *size );

/**
 * De-allocates/clears glob memory.
 */
typedef void (*NvOdmDispHdcpReleaseGlobType)( void *glob );

/**
 * Gets a signed S and Cs to allow authentication of the graphics hardware
 * with the player software.
 *
 * @param pDisplayStream Pointer to display context
 * @param phpPacket A pointer to a hdcp packet with the following filled in:
 *      - uidHDCP (verMaj = 1, revMin = 1 )
 *      - packetSize
 *      - apIndex (NV_HDCP_FLAGS_APINDEX must be set as well)
 *      - version (NV_HDCP_PACKET_VERSION)
 *      - cmdCommand (NV_HDCP_CMD_READ_LINK_STATUS)
 *      - required flags/fields: NV_HDCP_FLAGS_CN, _CKSV
 * @retval NV_HDCP_STATUS_SUCCESS on success, one of NV_HDCP_RET_STATUS
        otherwise
 * @returns phpPacket with the following flFlags and their associated fields:
 *      - NV_HDCP_FLAGS_BSTATUS, _BKSVLIST, _AN, _S, _CS, _KP, _DKSV
 */
NV_HDCP_RET_STATUS
GetLinkStatus(
    void *pDisplayStream,
    NV_HDCP_PACKET *phpPacket );

/**
 * Performs the necessary operations to validate links (repeaters) for the
 * attach-point. See the NVIDIA Upstream Sepcification for more details.
 *
 * @param pDisplayStream Pointer to display context
 * @param phpPacket A pointer to a hdcp packet with the following filled in:
 *      - uidHDCP (verMaj = 1, revMin = 1 )
 *      - packetSize
 *      - apIndex (NV_HDCP_FLAGS_APINDEX must be set as well)
 *      - version (NV_HDCP_PACKET_VERSION)
 *      - cmdCommand (NV_HDCP_CMD_VALIDATE_LINK)
 *      - required flags/fields: NV_HDCP_FLAGS_CN, _CKSV
 * @retval NV_HDCP_STATUS_SUCCESS on success, one of NV_HDCP_RET_STATUS
        otherwise
 * @returns phpPacket with the following flFlags and their associated fields:
 *      - NV_HDCP_FLAGS_BSTATUS, _BKSVLIST, _V, _MP, _DKSV
 */
NV_HDCP_RET_STATUS
ValidateLink(
    void *pDisplayStream,
    NV_HDCP_PACKET *phpPacket );

#if defined(__cplusplus)
}
#endif

#endif
