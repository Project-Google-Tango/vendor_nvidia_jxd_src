/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_RUNNER_H
#define INCLUDED_RUNNER_H

#include "nvcommon.h"
#include "nvos.h"
#include "nv3p.h"
#include "error_handler.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Director or core function which initiates test entry , runs test entry and reports the result
 *
 * @param Suite The test entry module pointer(all or module name)
 * @param Argument The test entry module run type( all , testname, testtype)
.* @param PrivData The common private test input data structure
.* @param h3p The sockethandle for Nv3p communication
 *
 * @return NvSuccess if there were not failures
 */
NvError NvBDKTest_RunInputTestPlan(const char* Suite,
                                   const char* Argument,
                                   NvBDKTest_TestPrivData PrivData,
                                   Nv3pSocketHandle h3p);

NvError NvBDKTest_SetBDKTestEnv(NvBool stopOnFail, NvU32 timeout);

#if defined(__cplusplus)
                     }
#endif

#endif // INCLUDED_RUNNER_H

