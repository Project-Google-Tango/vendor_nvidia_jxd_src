/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: 
 *           NvMMBlock iLBC speech decoder specific structure</b>
 *
 * @b Description: Declares Interface for AMR-WB Audio decoder.
 */

#ifndef INCLUDED_NVMM_ILBC_H
#define INCLUDED_NVMM_ILBC_H

/**
 * @defgroup nvmm_ilbc iLBC Codec API
 * 
 * 
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"
#include "nvmm.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    NvMMBlockILBCStream_In = 0, //!< Input
    NvMMBlockILBCStream_Out,    //!< Output
    NvMMBlockILBCStreamCount

} NvMMBlockILBCStream;

enum
{
    NvMMAttributeILBC_ProcessingTimeInfo =
                    (NvMMAttributeReference_Unknown + 1),    /* uses  NvMMILBCProcessingTimeInfo*/
    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAttributeILBC_Force32 = 0x7FFFFFFF
};

typedef struct NvMMILBCProcessingTimeInfo_{
    NvU32 structSize;
    NvU32 usec;
}NvMMILBCProcessingTimeInfo;

typedef enum
{
    NvMMAudioILBCAttribute_Mode = 0,

    /* Max should always be the last value, used to pad the enum to 32bits */
    NvMMAudioILBCAttribute_Force32 = 0x7FFFFFFF
} NvMMAudioILBCAttribute;

typedef enum
{
    NvMMAudioILBCMode_20ms = 20,
    NvMMAudioILBCMode_30ms = 30,
    NvMMAudioILBCMode_Force32 = 0x7FFFFFFF
}NvMMAudioILBCMode;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_ILBC_H
