/**
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


/**
 * This file manages the locations of data in the boot device.  As such, it
 * serves as a prototype of sorts for logic that would prove useful in a
 * recovery mode app that updates the structures in the boot device.
 */

#ifndef INCLUDED_t30_DATA_LAYOUT_H
#define INCLUDED_t30_DATA_LAYOUT_H

#include "../nvbuildbct_config.h"
#include "../nvbuildbct.h"
#include "../data_layout.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void t30UpdateBct(struct BuildBctContextRec *Context, UpdateEndState EndState);
void t30UpdateBl(struct BuildBctContextRec *Context, UpdateEndState EndState);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_DATA_LAYOUT_H */

