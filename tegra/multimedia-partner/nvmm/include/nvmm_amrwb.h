/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: 
 *           NvMMBlock AMR-WB audio decoder specific structure</b>
 *
 * @b Description: Declares Interface for AMR-WB Audio decoder.
 */

#ifndef INCLUDED_NVMM_AMRWB_H
#define INCLUDED_NVMM_AMRWB_H

/**
 * @defgroup nvmm_amrwb AMR-WB Codec API
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
    NvMMBlockAmrWBStream_In = 0, //!< Input
    NvMMBlockAmrWBStream_Out,    //!< Output
    NvMMBlockAmrWBStreamCount

} NvMMBlockAmrWBStream;

enum
{
    NvMMAttributeAmrWB_ProcessingTimeInfo =
                    (NvMMAttributeReference_Unknown + 1),    /* uses  NvMMAmrWBProcessingTimeInfo*/
    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAttributeAmrWB_Force32 = 0x7FFFFFFF
};

typedef struct NvMMAmrWBProcessingTimeInfo_{
    NvU32 structSize;
    NvU32 usec;
}NvMMAmrWBProcessingTimeInfo;

typedef enum
{
    NvMMAudioAmrWBAttribute_Mode = 0,
    NvMMAudioAmrWBAttribute_DTxState,
    NvMMAudioAmrWBAttribute_Format,
    NvMMAudioAmrWBAttribute_Tx_Type,
    NvMMAudioAmrWBAttribute_Rx_Type,
    NvMMAudioAmrWBAttribute_MMS_Format,
    NvMMAudioAmrWBAttribute_MMS_Inp,
    NvMMAudioAmrWBAttribute_Used_Mode,

    /* Max should always be the last value, used to pad the enum to 32bits */
    NvMMAudioAmrWBAttribute_Force32 = 0x7FFFFFFF
} NvMMAudioAmrWBAttribute;

typedef enum
{
    NvMMAudioAmrWBMode_7k = 0,
    NvMMAudioAmrWBMode_9k,
    NvMMAudioAmrWBMode_12k,
    NvMMAudioAmrWBMode_14k,
    NvMMAudioAmrWBMode_16k,
    NvMMAudioAmrWBMode_18k,
    NvMMAudioAmrWBMode_20k,
    NvMMAudioAmrWBMode_23k,
    NvMMAudioAmrWBMode_24k,
    NvMMAudioAmrWBMode_SID,
    NvMMAudioAmrWBMode_Force32 = 0x7FFFFFFF
}NvMMAudioAmrWBMode;


typedef enum
{
    NvMMAudioAmrWBFormat_Default=0,
    NvMMAudioAmrWBFormat_ITU,
    NvMMAudioAmrWBFormat_MIME
}NvMMAudioAmrWBFormat;
/** 
* @brief AMR-WB codec Parameters
*/
typedef struct 
{
    NvU32 structSize;
    /// File format of source (MMS, IF1, IF2)
    NvU32   FileFormat;
    ///stream mode (This is an enum of bitrates)
    NvMMAudioAmrWBMode   Mode;
    /// DTx is on or Off
    NvU32 DTxState;
    //Enc format Default,ITU,MIME
    NvMMAudioAmrWBFormat Format;

} NvMMAudioEncPropertyAMRWB;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_AMRWB_H
