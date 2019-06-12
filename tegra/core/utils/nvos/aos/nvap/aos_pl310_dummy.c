/*
 * Copyright 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

// Most of the PL310 functions are nulled out via #define on chips that
// don't support them, but this file provides some additional stubs for
// those functions that are called directly from assembly code.
#include "aos_pl310.h"

void nvaosPl310InvalidateDisable(void)
{
}
