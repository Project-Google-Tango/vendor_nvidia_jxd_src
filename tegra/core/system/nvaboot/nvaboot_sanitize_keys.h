/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVABOOT_SANITIZE_KEY_H
#define NVABOOT_SANITIZE_KEY_H

#include "nvcommon.h"
#include "nverror.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(CONFIG_TRUSTED_FOUNDATIONS) || defined(CONFIG_TRUSTED_LITTLE_KERNEL)
#include "nvaboot_tf.h"

/*
 * Prepare and "package" all the tegra2 resources forwarded to the Trusted Foundations.
 * Generate the TF_SSK, the TF_SBK and the seed (based on the PLL linear feedback shift register).
 */
NvError NvAbootPrivGenerateTFKeys(NvU8 *pTFBuff, NvU32 size);
#endif

NvError NvAbootPrivSanitizeKeys(void);

#ifdef __cplusplus
}
#endif

#endif
