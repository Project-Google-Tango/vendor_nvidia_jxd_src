/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Memory barrier intrinsics - armcc version
 *   extern void NvMemBarrier(void);
 */
    AREA    NvBarriers, CODE, READONLY, ALIGN=4
    EXPORT  NvMemBarrier

NvMemBarrier
    // dsb instruction in ARM mode
    dcd 0xf57ff04f
    bx  lr

    END
