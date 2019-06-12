/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCMS_CMU_H
#define INCLUDED_NVCMS_CMU_H

typedef struct {
    NvU16 lut1[256];
    NvU16 csc[9];
    NvU8  lut2[960];
} NvCmsCmuBoardData;

#endif // INCLUDED_NVCMS_CMU_H
