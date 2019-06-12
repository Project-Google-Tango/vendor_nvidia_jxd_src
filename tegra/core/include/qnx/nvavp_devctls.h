/*
 * Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef _NVAVP_DEVCTLS_H
#define _NVAVP_DEVCTLS_H

#include <devctl.h>
#include "nvavp.h"

#define NVAVP_MAX_RECLOCS   64

/* avp submit flags */
#define NVAVP_FLAG_NONE            0x00000000
#define NVAVP_UCODE_EXT            0x00000001 /*use external ucode provided */

struct nvavp_submit_t {
   NvAvpCommandBuffer   cmdbuf;
   NvU32                num_relocs;
   NvAvpRelocationEntry relocs[NVAVP_MAX_RECLOCS];
   NvU32                fence_flag;
   NvRmFence            fence;
   NvU32                flags;
};

#define NVAVP_CMD_CODE                 0x60

#define NVAVP_DEVCTL_GET_SYNCPOINT_ID  __DIOF(_DCMD_MISC,  NVAVP_CMD_CODE + 0, unsigned int)
#define NVAVP_DEVCTL_SUBMIT            __DIOTF(_DCMD_MISC, NVAVP_CMD_CODE + 1, struct nvavp_submit_t)

#endif //_NVAVP_DEVCTL_H
