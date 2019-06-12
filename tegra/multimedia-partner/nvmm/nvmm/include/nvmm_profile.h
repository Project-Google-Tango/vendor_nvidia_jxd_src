/*
* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/
#ifndef INCLUDED_NVMM_PROFILE_H
#define INCLUDED_NVMM_PROFILE_H

#include "nvassert.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NV_MM_PROFILE_INDEX0 0
#define NV_MM_PROFILE_INDEX1 1
#define NV_MM_PROFILE_INDEX2 2
#define NV_MM_PROFILE_INDEX3 3

#if !defined(ENABLE_PROFILING)
#define ENABLE_PROFILING 0
#endif

#if ENABLE_PROFILING
#define MAX_PROF_BLOCKS  4

typedef struct Profiling_t
{
    NvU32 accumulatedtime[MAX_PROF_BLOCKS];
    NvU32 no_of_times[MAX_PROF_BLOCKS];
    NvU32 last_run_time[MAX_PROF_BLOCKS];
    NvU32 ProfStats[MAX_PROF_BLOCKS];
    NvU32 inputStarvationCount[MAX_PROF_BLOCKS];
}Profiling;

static Profiling profile;

#define NvMMProfilingStart(index)\
do\
{\
    NV_ASSERT(index < MAX_PROF_BLOCKS);\
    profile.ProfStats[index] = (NvU32)NvOsGetTimeUS();\
}while(0);\

#define NvMMProfilingStarvationCount(index)\
do\
{\
    NV_ASSERT(index < MAX_PROF_BLOCKS);\
    profile.inputStarvationCount[index]++;\
}while(0);\



#define NvMMProfilingEnd(index) \
do\
{\
    NvU32 CounterVal;\
    NvU32 TotalTimeTaken;\
    NV_ASSERT(index < MAX_PROF_BLOCKS);\
    CounterVal = (NvU32)NvOsGetTimeUS();\
    if (profile.ProfStats[index] > CounterVal)\
    {\
    /* In case of timer overflow */\
        TotalTimeTaken = CounterVal + (0xFFFFFFFF - profile.ProfStats[index]);\
    }\
    else\
    {\
        TotalTimeTaken = CounterVal - profile.ProfStats[index];\
    }\
    profile.last_run_time[index] = TotalTimeTaken;\
    profile.accumulatedtime[index] += TotalTimeTaken;\
    profile.no_of_times[index]++;\
}while(0);\

#define NvMMProfilingPrint(N) \
do\
{\
    int i;\
    NV_ASSERT(N < MAX_PROF_BLOCKS);\
    for (i = 0;i <= N; i++)\
    {\
        NV_DEBUG_PRINTF(("[%d] (%d, %d, %d) \n", i, profile.no_of_times[i],\
                        profile.last_run_time[i], profile.accumulatedtime[i]));\
    }\
}while(0);\

#define NvMMProfilingInit()\
do\
{\
    int i;\
    for (i = 0;i < MAX_PROF_BLOCKS;i++)\
    {\
        profile.accumulatedtime[i] = 0;\
        profile.no_of_times[i] = 0;\
        profile.last_run_time[i] = 0;\
        profile.ProfStats[i] = 0;\
    }\
    NV_DEBUG_PRINTF(("[index] (no of times, last run time (us), total time taken (us)) : "));\
}while(0);\

#else
# define NvMMProfilingStart(index)
# define NvMMProfilingEnd(index)
# define NvMMProfilingPrint(N)
# define NvMMProfilingInit()
#define NvMMProfilingStarvationCount(index)
#endif // end of ENABLE_PROFILING

#ifdef __cplusplus
}
#endif

#endif // INCLUDED_NVMM_PROFILE_H
