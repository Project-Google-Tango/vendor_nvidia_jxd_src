/*
* Copyright (c) 2012-2014 NVIDIA CORPORATION.  All rights reserved.
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
	/* UUID : {23457890-8572-4a6f-a1f1-03aa9b05f9ff} */
	{ 0x23457890, 0x8572, 0x4a6f,
		{ 0xa1, 0xf1, 0x03, 0xaa, 0x9b, 0x05, 0xf9, 0xff } },

	/* optional configuration options here */
	{
		/* eight pages for heap */
		OTE_CONFIG_MIN_HEAP_SIZE(8 * 4096),

		/* request two I/O mappings */
		OTE_CONFIG_MAP_MEM(1, 0x70000000, 0x1000),
		OTE_CONFIG_MAP_MEM(2, 0x70000804, 0x4)
	},
};
