/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVFLASH_USAGE_H
#define INCLUDED_NVFLASH_USAGE_H

#include "nvflash_commands.h"


#if defined(__cplusplus)
extern "C"
{
#endif

void
nvflash_usage(void);
void
nvflash_cmd_usage(NvFlashCmdHelp *a);

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NVFLASH_USAGE_H
