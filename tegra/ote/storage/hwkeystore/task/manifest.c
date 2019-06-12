/*
* Copyright (c) 2013-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stddef.h>
#include <stdio.h>

#include <service/ote_manifest.h>

OTE_MANIFEST OTE_MANIFEST_ATTRS ote_manifest =
{
	/* UUID of hwkeystore_task */
	{ 0x23457890, 0x1234, 0x4a6f, \
		{ 0xa1, 0xf1, 0x03, 0xaa, 0x9b, 0x05, 0xf9, 0xee } },

	/* optional configuration options here */
	{
		/* pages for heap allocations */
		OTE_CONFIG_MIN_HEAP_SIZE(32 * 4096),

		/* pages for stack allocations */
		OTE_CONFIG_MIN_STACK_SIZE(16 * 4096),

	},
};
