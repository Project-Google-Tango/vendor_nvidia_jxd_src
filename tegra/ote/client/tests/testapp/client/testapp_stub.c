/*
* Copyright (c) 2013-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>

#include "nvos.h"

int main(void)
{
	NvOsDebugPrintf("Non-TLK build. Running stub test.\n");
	NvOsDebugPrintf("SUCCESS\n");
	return 0;
}
