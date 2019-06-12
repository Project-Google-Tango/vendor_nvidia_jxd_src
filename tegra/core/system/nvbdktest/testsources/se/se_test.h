/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_DIAG_TESTS_SE_H
#define INCLUDED_DIAG_TESTS_SE_H

#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvError SeAesVerifyTest(NvU32 Instance);
NvError SeRngVerifyTest(NvU32 Instance);
#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_DIAG_TESTS_SE_H

