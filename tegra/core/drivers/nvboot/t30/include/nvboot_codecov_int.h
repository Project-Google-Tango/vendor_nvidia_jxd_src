/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVBOOT_CODECOV_INT_H
#define INCLUDED_NVBOOT_CODECOV_INT_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * To facilitate postprocessing tools in locating the code coverage array,
 * we store a pointer to the array at a well-known "magic" address, as
 * defined here.
 */
#define NVBOOT_CODECOV_MAGIC_ADDRESS 0x40005E00

/*
 * The maximum number of profile points is determined by the size of the
 * code coverage array, one bit per profile.  The code coverage array is
 * allocated in units of 32-bit words.
 *
 * Note that the actual (global) code coverage array declaration must be
 * manually inserted into the code-under-test.  Then by including this
 * file, all profiled code can gain access to the global array.
 */
#define NVBOOT_CODECOV_ARRAY_WORDS 100

#ifdef ENABLE_CODECOV
extern NvU32 NvBootCodeCoverageArray[NVBOOT_CODECOV_ARRAY_WORDS];
#endif

/*
 * The actual PROFILE macro is shown here.  Each PROFILE point has a
 * unique id which is used to index into the global code coverage array,
 * which is a bit array.  The array is initially loaded with zeroes, and
 * whenever a PROFILE point is encountered then it's id is used to index
 * into the array an change the appropriate bit to a one.
 *
 * When ENABLE_CODECOV is *NOT* defined, then the PROFILE macro becomes
 * an empty statement.
 */

#ifdef ENABLE_CODECOV
#define PROFILE(val)\
{\
    NvU16 Index = val / (sizeof(NvU32)*8);\
    NvU16 BitPosition = val % (sizeof(NvU32)*8);\
    NvBootCodeCoverageArray[Index] |= 1 << BitPosition;\
}
#else
#define PROFILE()\
{\
    do\
    {\
    }while(0);\
}
#endif

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_CODECOV_INT_H
