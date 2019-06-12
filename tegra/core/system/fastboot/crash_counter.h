/**
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_CRASH_COUNTER_H
#define INCLUDED_CRASH_COUNTER_H

#include "fastboot.h"

#ifdef __cplusplus
extern "C" {
#endif

// Number of times Google TV fails to boot before a
// "crash recovery" boot is forced.
#define MAX_GTV_CRASH_COUNTER 10

/**
 * Set the crash counter to value. Stored in CTR partition.
 * @param crashCounter: Set crash counter to this value
 * @retval Nv_Error_Success:  No error
 * @retval Any other value:   Internal error
 */
NvError setCrashCounter(NvU32 crashCounter);

/**
 * Read existing crash counter from CTR partition.
 * @param crashCounter: Number of attempted boots without success.
 * @retval Nv_Error_Success:  No error
 * @retval Any other value:   Internal error
 */
NvError getCrashCounter(NvU32 * crashCounter);

/**
 * Write "crash recovery" bootloader message to MSC partition,
 * to force a recovery boot.
 * @retval Nv_Error_Success:  No error
 * @retval Any other value:   Internal error
 */
NvError forceRecoveryBoot(NvAbootHandle hAboot);

#ifdef __cplusplus
}
#endif

#endif // INCLUDED_CRASH_COUNTER_H
