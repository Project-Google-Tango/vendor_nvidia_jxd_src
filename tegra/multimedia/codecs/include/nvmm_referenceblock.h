/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_REFERENCEBLOCK_H
#define INCLUDED_NVMM_REFERENCEBLOCK_H

#include "nvcommon.h"
#include "nvmm.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** Reference block's stream indices.
    Stream indices must start at 0.
*/
typedef enum
{
    NvMMBlockRefStream_In = 0, //!< Input
    NvMMBlockRefStream_Out,    //!< Output
    NvMMBlockRefStreamCount

} NvMMBlockRefStream;

/**
 * @brief Reference Attribute enumerations.
 * Following enum are used to set the reference block attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * @see SetAttribute
 */
enum
{
    NvMMAttributeReference_ProcessingTimeInfo =
                    (NvMMAttributeReference_Unknown + 1),    /* uses  NvMMReferenceProcessingTimeInfo*/
    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAttributeReference_Force32 = 0x7FFFFFFF
};

typedef struct NvMMReferenceProcessingTimeInfo_{
    NvU32 structSize;
    NvU32 usec;
}NvMMReferenceProcessingTimeInfo;

#if defined(__cplusplus)
}
#endif

#endif
