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
	/* UUID : {32456890-8572-4a6f-a1f1-03aa9b05f9ff} */
	{ 0x32456890, 0x8572, 0x4a6f,
		{ 0xa1, 0xf1, 0x03, 0xaa, 0x9b, 0x05, 0xf9, 0xff } },

	/* optional configuration options here */
	{
	},
};
