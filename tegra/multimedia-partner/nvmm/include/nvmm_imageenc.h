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
*           NvMM Image Encoder APIs</b>
*
* @b Description: Declares Interface for NvMM Image Encoder APIs.
*/

#ifndef INCLUDED_NVMM_IMAGEENC_H
#define INCLUDED_NVMM_IMAGEENC_H

/**
* @defgroup nvmm_imageenc Image Encode API
* 
* 
* @ingroup nvmm_modules
* @{
*/

#include "nvcommon.h"
#include "nvmm_exif.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /**
    * @brief Image Encoder Attribute enumerations.
    * Following enum are used by image encoders for setting the attributes.
    * They are used as 'eAttributeType' variable in SetAttribute() API.
    * @see SetAttribute
    */
    typedef enum
    {
        //(NvMMAttributeVideoEnc_JPEGEnc + 1),
        //      /* uses NvMMVideoEncBitStreamProperty */
        NvMMAttributeImageEnc_CommonStreamProperty = 0,
        NvMMAttribtueImageDec_JPEGStreamProperty,
        NvMMAttributeImageEnc_Quality,
        NvMMAttributeImageEnc_ErrorResiliency,
        NvMMAttributeImageEnc_InputFormat,
        NvMMAttributeImageEnc_resolution,
        NvMMAttributeImageEnc_ThumbnailSupport,
        /* OpenMax inspired attributes */
        NvMMAttributeImageEnc_QFactor,
        NvMMAttributeImageEnc_QuantizationTable,
        NvMMAttributeImageEnc_HuffmanTable,
        /*RateControl Parameter*/
        NvMMAttributeImageEnc_EncodedSize,
        /* EXIF parameters */
        NvMMAttributeImageEnc_UserComment,
        NvMMAttributeImageEnc_ImageDescrpt,
        NvMMAttributeImageEnc_Make,
        NvMMAttributeImageEnc_Model,
        NvMMAttributeImageEnc_Copyright,
        NvMMAttributeImageEnc_Artist,
        NvMMAttributeImageEnc_Software,
        NvMMAttributeImageEnc_DateTime,
        NvMMAttributeImageEnc_DateTime_Original,
        NvMMAttributeImageEnc_DateTime_Digitized,
        NvMMAttributeImageEnc_FileSource,
        NvMMAttributeImageEnc_GPSInfo,
        NvMMAttributeImageEnc_InterOperabilityInfo,
        NvMMAttributeImageEnc_Orientation,
        /* Custom QUANT Tables */
        NvMMAttributeImageEnc_CustomQuantTables,

        /* GPS Processing Method */
        NvMMAttributeImageEnc_GPSInfo_ProcMethod,
        /*
          Max should always be the last value, used to pad the enum to 32bits
        */
        NvMMAttributeImageEnc_Force32 = 0x7FFFFFFF
    } NvMMImageEncAttribute;

    typedef enum
    {
        NvMMImageRotatedPhysically = 0,

        NvMMImageRotatedByExifTag,
        /*
          Max should always be the last value, used to pad the enum to 32bits
        */
        NvMMImageRotated_Force32 = 0x7FFFFFFF

    } NvMMImageRotation;

    /**
    * @brief Defines the structure for holding encoder attributes
    * related to image orientation, Indicates if image is really rotated
    * or rotated with Exif Orientation tag
    */
    typedef struct
    {
        NvMMExif_Orientation orientation;
        NvMMImageRotation    eRotation;
    }NvMMImageOrientation;

    /**
    * @brief Defines the structure for holding encoder attributes
    * related to Quality , Error resiliency, chroma format and resolution
    */
    typedef struct
    {    
        NvU32   StructSize;
        /// Quality/Error resiliency level
        NvU32   AttributeLevel1;
        NvU32   AttributeLevel2;
        NvBool   AttributeLevel_ThumbNailFlag;
#define MAX_EXIF_STRING_IN_BYTES       (200)
        char    AttributeLevel_Exifstring[MAX_EXIF_STRING_IN_BYTES];
    }NvMMJpegEncAttributeLevel;

    typedef enum
    {
        NV_JPEG_COLOR_FORMAT_420 = 0,
        NV_JPEG_COLOR_FORMAT_422,
        NV_JPEG_COLOR_FORMAT_422T,
        NV_JPEG_COLOR_FORMAT_444
    }NV_JPEG_COLOR_FORMAT;

    typedef enum
    {
        NV_JPEG_ENC_QuantizationTable_Luma = 0,
        NV_JPEG_ENC_QuantizationTable_Chroma,
        /*
        *  Max should always be the last value, used to pad the enum to 32bits
        */
        NV_JPEG_ENC_QuantizationTable_Force32 = 0x7FFFFFFF
    } NV_JPEG_ENC_QUANTIZATIONTABLETYPE;

    typedef struct
    {
        NvU32   StructSize;
        NV_JPEG_ENC_QUANTIZATIONTABLETYPE QTableType;
        NvU8    QuantTable[64];
    }NvMMJpegEncCustomQuantTables;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_IMAGEENC_H
