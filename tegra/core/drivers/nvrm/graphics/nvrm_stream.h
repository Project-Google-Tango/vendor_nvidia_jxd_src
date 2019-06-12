/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_STREAM_H
#define INCLUDED_NVRM_STREAM_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvrm_channel.h"
#include "nvrm_channel_priv.h"

/* hack up some defines that look like the register spec. there's no general
 * sync point header -- each module has it's own namespace prefix for the
 * sync point registers.
 */
#define NVRM_INCR_SYNCPT_0                   (0x0)
#define NVRM_INCR_SYNCPT_0_COND_RANGE        15:8
#define NVRM_INCR_SYNCPT_0_COND_IMMEDIATE    (0x0UL)
#define NVRM_INCR_SYNCPT_0_COND_OP_DONE      (0x1UL)
#define NVRM_INCR_SYNCPT_0_COND_RD_DONE      (0x2UL)
#define NVRM_INCR_SYNCPT_0_COND_REG_WR_SAFE  (0x3UL)
#define NVRM_INCR_SYNCPT_0_INDX_RANGE        7:0

/* per-chip state for the channel implementation */
typedef struct NvRmChHwRec *NvRmChHwHandle;

/*
 * Placeholder context which is assigned to a channel during unit powerdown.
 */
#define NVRM_FAKE_POWERDOWN_CONTEXT ((NvRmContextHandle)-1)

/* Cheesy associative array for two elements: NvRmModuleID_3D and everything
 * else.
 */

/* NVRM_CHANNEL_CONTEXT_GET retrieves a last stored hardware context with the
 * given module ID.
 */
#define NVRM_CHANNEL_CONTEXT_GET(hChannel, Engine) \
    ((Engine) == NvRmModuleID_3D ? \
     (hChannel)->hContext3D : (hChannel)->hContextOther)

/* NVRM_CHANNEL_CONTEXT_SET stores this context using the Engine field as a
 * key.
 */
#define NVRM_CHANNEL_CONTEXT_SET(hChannel, Engine, hContext) \
    do { \
        NvRmContextHandle *h = (Engine) == NvRmModuleID_3D ? \
            &(hChannel)->hContext3D : &(hChannel)->hContextOther; \
        *h = hContext; \
        if (hContext != NVRM_FAKE_POWERDOWN_CONTEXT) \
            (hContext)->hChannel = hChannel; \
    } while (0)

/* NVRM_CHANNEL_CONTEXT_RESET removes the context with this module ID if it is
 * the current one.
 */
#define NVRM_CHANNEL_CONTEXT_RESET(hChannel, hContext) \
    do { \
        NvRmContextHandle *h = (hContext) && \
            (hContext)->Engine == NvRmModuleID_3D ? \
            &(hChannel)->hContext3D : &(hChannel)->hContextOther; \
        if (*h == hContext) \
            *h = NULL; \
    } while (0)

typedef struct NvRmRmcParserRec
{
    NvU32 data_count;
    NvU32 opcode;
    NvU32 offset;
    NvU32 mask;
} NvRmRmcParser;

/**
 * parses a pushbuffer word and emits to the gobal RMC file. must be
 * implemented by the per-chip channel implementation.
 *
 * @param hChannel The channel handle
 * @param data The data word
 */
void
NvRmChParseCmd( NvRmDeviceHandle hRm, NvRmRmcParser *parser, NvU32 data );

#endif // INCLUDED_NVRM_STREAM_H
