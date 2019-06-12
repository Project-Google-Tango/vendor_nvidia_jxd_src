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
*           NvMMLite Image Encoder APIs</b>
*
* @b Description: Declares Interface for NvMMLite Image Encoder APIs.
*/

#ifndef INCLUDED_NVMMLITE_IMAGEENC_H
#define INCLUDED_NVMMLITE_IMAGEENC_H

/**
* @defgroup nvmmlite_imageenc Image Encode API
*
*
* @ingroup nvmmlite_modules
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
    //(NvMMLiteAttributeVideoEnc_JPEGEnc + 1),
    // uses NvMMLiteVideoEncBitStreamProperty
    NvMMLiteAttributeImageEnc_CommonStreamProperty = 0,
    NvMMLiteAttribtueImageDec_JPEGStreamProperty,
    NvMMLiteAttributeImageEnc_Quality,
    NvMMLiteAttributeImageEnc_ErrorResiliency,
    NvMMLiteAttributeImageEnc_InputFormat,
    NvMMLiteAttributeImageEnc_resolution,
    NvMMLiteAttributeImageEnc_ThumbnailSupport,
    /* OpenMax inspired attributes */
    NvMMLiteAttributeImageEnc_QFactor,
    NvMMLiteAttributeImageEnc_QuantizationTable,
    NvMMLiteAttributeImageEnc_HuffmanTable,
    /*RateControl Parameter*/
    NvMMLiteAttributeImageEnc_EncodedSize,
    /* EXIF parameters */
    NvMMLiteAttributeImageEnc_UserComment,
    NvMMLiteAttributeImageEnc_ImageDescrpt,
    NvMMLiteAttributeImageEnc_Make,
    NvMMLiteAttributeImageEnc_Model,
    NvMMLiteAttributeImageEnc_Copyright,
    NvMMLiteAttributeImageEnc_Artist,
    NvMMLiteAttributeImageEnc_Software,
    NvMMLiteAttributeImageEnc_DateTime,
    NvMMLiteAttributeImageEnc_DateTime_Original,
    NvMMLiteAttributeImageEnc_DateTime_Digitized,
    NvMMLiteAttributeImageEnc_FileSource,
    NvMMLiteAttributeImageEnc_GPSInfo,
    NvMMLiteAttributeImageEnc_InterOperabilityInfo,
    NvMMLiteAttributeImageEnc_Orientation,
    /* Custom QUANT Tables */
    NvMMLiteAttributeImageEnc_CustomQuantTables,

    NvMMLiteAttributeImageEnc_BlockHeightLog2,
    NvMMLiteAttributeImageEnc_Layout,
    /* GPS Processing Method*/
    NvMMLiteAttributeImageEnc_GPSInfo_ProcMethod,
    /* Custom Thumbnail Quantization Tables */
    NvMMLiteAttributeImageEnc_ThumbCustomQuantTables,
    /*
      Max should always be the last value, used to pad the enum to 32bits
    */
    NvMMLiteAttributeImageEnc_Force32 = 0x7FFFFFFF
} NvMMLiteImageEncAttribute;

typedef enum
{
    NvMMLiteImageRotatedPhysically = 0,

    NvMMLiteImageRotatedByExifTag,
    /*
      Max should always be the last value, used to pad the enum to 32bits
    */
    NvMMLiteImageRotated_Force32 = 0x7FFFFFFF

} NvMMLiteImageRotation;

/**
* @brief Defines the structure for holding encoder attributes
* related to image orientation, Indicates if image is really rotated
* or rotated with Exif Orientation tag
*/
typedef struct
{
    NvMMExif_Orientation orientation;
    NvMMLiteImageRotation    eRotation;
}NvMMLiteImageOrientation;

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
}NvMMLiteJpegEncAttributeLevel;

typedef enum
{
    NVMMLITE_JPEG_COLOR_FORMAT_420 = 0,
    NVMMLITE_JPEG_COLOR_FORMAT_422,
    NVMMLITE_JPEG_COLOR_FORMAT_422T,
    NVMMLITE_JPEG_COLOR_FORMAT_444,
    NVMMLITE_JPEG_COLOR_FORMAT_420_SEMIPLANAR,
    NVMMLITE_JPEG_COLOR_FORMAT_422_SEMIPLANAR,
    NVMMLITE_JPEG_COLOR_FORMAT_422T_SEMIPLANAR,
    NVMMLITE_JPEG_COLOR_FORMAT_444_SEMIPLANAR
} NVMMLITE_JPEG_COLOR_FORMAT;

typedef enum
{
    NVMMLITE_JPEG_ENC_QuantizationTable_Luma = 0,
    NVMMLITE_JPEG_ENC_QuantizationTable_Chroma,
    /*
    *  Max should always be the last value, used to pad the enum to 32bits
    */
    NVMMLITE_JPEG_ENC_QuantizationTable_Force32 = 0x7FFFFFFF
} NVMMLITE_JPEG_ENC_QUANTIZATIONTABLETYPE;

typedef struct
{
    NvU32   StructSize;
    NVMMLITE_JPEG_ENC_QUANTIZATIONTABLETYPE QTableType;
    NvU8    QuantTable[64];
}NvMMLiteJpegEncCustomQuantTables;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMMLITE_IMAGEENC_H
