/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * The authors make NO WARRANTY or representation, either express or implied,
 * with respect to this software, its quality, accuracy, merchantability, or
 * fitness for a particular purpose.  This software is provided "AS IS", and you,
 * its user, assume the entire risk as to its quality and accuracy.
 *
 * This software is copyright (C) 1991-1998, Thomas G. Lane.
 * All Rights Reserved except as specified below.
 *
 * Permission is hereby granted to use, copy, modify, and distribute this
 * software (or portions thereof) for any purpose, without fee, subject to these
 * conditions:
 * (1) If any part of the source code for this software is distributed, then this
 * README file must be included, with this copyright and no-warranty notice
 * unaltered; and any additions, deletions, or changes to the original files
 * must be clearly indicated in accompanying documentation.
 * (2) If only executable code is distributed, then the accompanying
 * documentation must state that "this software is based in part on the work of
 * the Independent JPEG Group".
 * (3) Permission for use of this software is granted only if the user accepts
 * full responsibility for any undesirable consequences; the authors accept
 * NO LIABILITY for damages of any kind.
 *
 * These conditions apply to any software derived from or based on the IJG code,
 * not just to the unmodified library.  If you use our work, you ought to
 * acknowledge us.
 *
 * Permission is NOT granted for the use of any IJG author's name or company name
 * in advertising or publicity relating to this software or products derived from
 * it.  This software may be referred to only as "the Independent JPEG Group's
 * software".
 *
 * We specifically permit and encourage the use of this software as the basis of
 * commercial products, provided that all warranty or liability claims are
 * assumed by the product vendor.
 */

#ifndef INCLUDED_NV_IMAGE_ENC_EXIFHEADER_H
#define INCLUDED_NV_IMAGE_ENC_EXIFHEADER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "nvimage_enc_pvt.h"
#include "nvimage_enc.h"


NvError JPEGEncWriteHeader(NvImageEncHandle hImageEnc,
                    NvU8 *Header);

NvError GetHeaderLen(NvImageEncHandle hImageEnc,
                    NvRmMemHandle hExifInfo,
                    NvU32 *pDataLen);

#define LEN_VALUE_FIT_IN_TAG 4

#define JPEG_QUANT_SCALE_BITS   11
#define NO_OF_QUANT_TABLES      2
#define RESTART_MARKER_OFFSET   7
#define RESTART_MARKER_OFFSET_RESET 0
#define SIZE_OF_JFIF_HEADER     (6*1024)    // 200 * 10 strings, max maker note = 2000 and remaining for tags + delta
#define DEFAULT_SUBJECT_AREA_LENGTH 8

#define NA_STRING "-NA-"

    enum JPEG_MARKER {      /* JPEG marker codes */

        M_SOF0  = 0xc0,
        M_SOF1  = 0xc1,
        M_SOF2  = 0xc2,
        M_SOF3  = 0xc3,

        M_SOF5  = 0xc5,
        M_SOF6  = 0xc6,
        M_SOF7  = 0xc7,

        M_JPG   = 0xc8,
        M_SOF9  = 0xc9,
        M_SOF10 = 0xca,
        M_SOF11 = 0xcb,

        M_SOF13 = 0xcd,
        M_SOF14 = 0xce,
        M_SOF15 = 0xcf,

        M_DHT   = 0xc4,

        M_DAC   = 0xcc,

        M_RST0  = 0xd0,
        M_RST1  = 0xd1,
        M_RST2  = 0xd2,
        M_RST3  = 0xd3,
        M_RST4  = 0xd4,
        M_RST5  = 0xd5,
        M_RST6  = 0xd6,
        M_RST7  = 0xd7,

        M_SOI   = 0xd8,
        M_EOI   = 0xd9,
        M_SOS   = 0xda,
        M_DQT   = 0xdb,
        M_DNL   = 0xdc,
        M_DRI   = 0xdd,
        M_DHP   = 0xde,
        M_EXP   = 0xdf,

        M_APP0  = 0xe0,
        M_APP1  = 0xe1,
        M_APP2  = 0xe2,
        M_APP3  = 0xe3,
        M_APP4  = 0xe4,
        M_APP5  = 0xe5,
        M_APP6  = 0xe6,
        M_APP7  = 0xe7,
        M_APP8  = 0xe8,
        M_APP9  = 0xe9,
        M_APP10 = 0xea,
        M_APP11 = 0xeb,
        M_APP12 = 0xec,
        M_APP13 = 0xed,
        M_APP14 = 0xee,
        M_APP15 = 0xef,

        M_JPG0  = 0xf0,
        M_JPG13 = 0xfd,
        M_COM   = 0xfe,

        M_MARKER= 0xff,

        M_TEM   = 0x01,

        M_ERROR = 0x100

    };
    /*
    * IFD Tag type
    */
    typedef enum
    {
        IfdTypes_Byte = 1,
        IfdTypes_Ascii, IfdTypes_Short, IfdTypes_Long, IfdTypes_Rational,
        IfdTypes_Sbyte, IfdTypes_Undefined, IfdTypes_Sshort, IfdTypes_Slong, IfdTypes_Srational,
        IfdTypes_Float, IfdTypes_Double
    } IfdTypes;

#define EMIT_TAG_WITH_OFFSET(p, tag_hi, tag_lo, tag_type, tag_len, offset) \
    do\
    {\
    *(p++) = (tag_hi); \
    *(p++) = (tag_lo);\
    *(p++) = 0x00;\
    *(p++) = (tag_type);\
    *(p++) = (NvU8)((tag_len) >> 24);\
    *(p++) = (NvU8)(((tag_len) >> 16) & 0xFF);\
    *(p++) = (NvU8)(((tag_len) >> 8) & 0xFF);\
    *(p++) = (NvU8)((tag_len) & 0xFF);\
    *(p++) = (NvU8)((offset) >> 24); \
    *(p++) = (NvU8)(((offset) >> 16) & 0xFF);\
    *(p++) = (NvU8)(((offset) >> 8) & 0xFF);\
    *(p++) = (NvU8)((offset) & 0xFF);\
        }\
        while(0)

#define EMIT_TAG_NO_OFFSET(p, tag_hi, tag_lo, tag_type, tag_len, v1, v2, v3, v4) \
    do\
    {\
    *(p++) = (tag_hi); \
    *(p++) = (tag_lo);\
    *(p++) = 0x00;\
    *(p++) = (tag_type);\
    *(p++) = 0x00; \
    *(p++) = 0x00;\
    *(p++) = 0x00;\
    *(p++) = (tag_len);\
    *(p++) = (v1);\
    *(p++) = (v2);\
    *(p++) = (v3);\
    *(p++) = (v4);\
        }\
        while(0)

#define TAG_SAVE_VAL_STR(p, p_src, len ) \
    do\
    {\
    if ((p_src))\
    {\
    /*p++ ??*/\
    NvOsStrncpy((char*)(p), (p_src), (len));\
    (p) += (len);\
        }\
        else\
    {\
    NvOsStrncpy((char*)(p), (NA_STRING), (len));\
    (p) += (len);\
        }\
        }\
        while(0)

#define  TAG_SAVE_VAL_NUM_DEN(p, numerator, denominator )\
    do \
    {\
    *(p++) = (NvU8)((numerator) >> 24); \
    *(p++) = (NvU8)(((numerator) >> 16) & 0xFF);\
    *(p++) = (NvU8)(((numerator) >> 8) & 0xFF);\
    *(p++) = (NvU8)((numerator) & 0xFF); \
    *(p++) = (NvU8)((denominator) >> 24); \
    *(p++) = (NvU8)(((denominator) >> 16) & 0xFF);\
    *(p++) = (NvU8)(((denominator) >> 8) & 0xFF);\
    *(p++) = (NvU8)((denominator) & 0xFF); \
        }\
        while(0)


#define TAG_SAVE_VAL_RESOLUTION(p) \
    {\
    /* hardcoding to 72 dpi need to change */\
    *(p++) = 0x00; /*X Resolution */\
    *(p++) = 0x00;\
    *(p++) = 0x00;\
    *(p++) = 0x48;\
    *(p++) = 0x00;\
    *(p++) = 0x00;\
    *(p++) = 0x00;\
    *(p++) = 0x01;\
    *(p++) = 0x00; /* Y Resolution */\
    *(p++) = 0x00;\
    *(p++) = 0x00;\
    *(p++) = 0x48;\
    *(p++) = 0x00;\
    *(p++) = 0x00;\
    *(p++) = 0x00;\
    *(p++) = 0x01;\
        }\
        while(0)

    /*
    * TAGS in IFD entry
    */
    typedef enum
    {
        Tag_ImageTitle_High = 0x01, Tag_ImageTitle_Low = 0x0E,
        Tag_Make_High = 0x01, Tag_Make_Low = 0x0F,
        Tag_Model_High = 0x01, Tag_Model_Low = 0x10,
        Tag_Orientation_High = 0x01, Tag_Orientation_Low= 0x12,
        Tag_XResolution_High = 0x01, Tag_XResolution_Low = 0x1A,
        Tag_YResolution_High = 0x01, Tag_YResolution_Low = 0x1B,
        Tag_ResolutionUnit_High = 0x01, Tag_ResolutionUnit_Low = 0x28,
        Tag_Software_High = 0x01, Tag_Software_Low = 0x31,
        Tag_DateTime_High = 0x01, Tag_DateTime_Low = 0x32,
        Tag_Artist_High = 0x01, Tag_Artist_Low = 0x3B,
        Tag_YCbCrPosition_High = 0x02, Tag_YCbCrPosition_Low = 0x13,
        Tag_Copyright_High = 0x82, Tag_Copyright_Low = 0x98,
        Tag_ExifIfdPointer_High = 0x87, Tag_ExifIfdPointer_Low = 0x69,
        Tag_GPSIFDOffset_High = 0x88 ,Tag_GPSIFDOffset_Low=0x25,

        Tag_ExposureTime_High= 0x82, Tag_ExposureTime_Low= 0x9A,
        Tag_FNumber_High= 0x82, Tag_FNumber_Low= 0x9D,
        Tag_ExposureProgram_High= 0x88, Tag_ExposureProgram_Low= 0x22,
        Tag_GainISO_High= 0x88, Tag_GainISO_Low= 0x27,
        Tag_ExifVersion_High = 0x90, Tag_ExifVersion_Low = 0x00,
        Tag_DateTimeOriginal_High = 0x90,Tag_DateTimeOriginal_Low = 0x03,
        Tag_DateTimeDigitized_High = 0x90,Tag_DateTimeDigitized_Low = 0x04,
        Tag_ComponentConfig_High = 0x91, Tag_ComponentConfig_Low = 0x01,
        Tag_CompressedBitsPerPixel_High = 0x91, Tag_CompressedBitsPerPixel_Low = 0x02,
        Tag_ShutterSpeed_High= 0x92, Tag_ShutterSpeed_Low= 0x01,
        Tag_Aperture_High= 0x92, Tag_Aperture_Low= 0x02,
        Tag_Brightness_High= 0x92, Tag_Brightness_Low= 0x03,
        Tag_ExposureBias_High= 0x92, Tag_ExposureBias_Low= 0x04,
        Tag_MaxAperture_High= 0x92, Tag_MaxAperture_Low= 0x05,
        Tag_SubjectDistanceRange_High = 0x92,Tag_SubjectDistanceRange_Low = 0x06,
        Tag_MeteringMode_High= 0x92, Tag_MeteringMode_Low= 0x07,
        Tag_LightSource_High = 0x92,Tag_LightSource_Low = 0x08,
        Tag_Flash_High= 0x92, Tag_Flash_Low= 0x09,
        Tag_FocalLength_High= 0x92, Tag_FocalLength_Low= 0x0A,
        Tag_FalshpixVersion_High = 0xA0, Tag_FalshpixVersion_Low = 0x00,
        Tag_ColorSpace_High = 0xA0, Tag_ColorSpace_Low = 0x01,
        Tag_PixelXDim_High = 0xA0, Tag_PixelXDim_Low = 0x02,
        Tag_PixelYDim_High = 0xA0, Tag_PixelYDim_Low = 0x03,
        Tag_InterOperabilityIFD_High = 0xA0, Tag_InterOperabilityIFD_Low = 0x05,
        Tag_SensingMethod_High = 0xA2,Tag_SensingMethod_Low = 0x17,
        Tag_FileSource_High = 0xA3, Tag_FileSource_Low = 0x00,
        Tag_CustomRendered_High = 0xA4,Tag_CustomRendered_Low = 0x01,
        Tag_ExposureMode_High = 0xA4,Tag_ExposureMode_Low = 0x02,
        Tag_WhiteBalance_High = 0xA4,Tag_WhiteBalance_Low = 0x03,
        Tag_DigitalZoomRatio_High = 0xA4,Tag_DigitalZoomRatio_Low = 0x04,
        Tag_SceneCaptureType_High = 0xA4,Tag_SceneCaptureType_Low = 0x06,
        Tag_Sharpness_High = 0xA4,Tag_Sharpness_Low = 0x0A,
        Tag_ImageUniqueId_High = 0xA4,Tag_ImageUniqueId_Low = 0x20,
        Tag_SubjectArea_High = 0x92,Tag_SubjectArea_Low = 0x14,
        Tag_MakerNote_High = 0x92,Tag_MakerNote_Low = 0x7C,
        Tag_UserComment_High = 0x92,Tag_UserComment_Low = 0x86,

        Tag_Compression_High = 0x01, Tag_Compression_Low = 0x03,
        Tag_JpegInterchangeFormat_High = 0x02, Tag_JpegInterchangeFormat_Low = 0x01,
        Tag_JpegInterchangeFormatLength_High = 0x02, Tag_JpegInterchangeFormatLength_Low = 0x02,

        //tagid's of GPSInfo
        Tag_GPSVersionID_High=0x00,Tag_GPSVersionID_Low=0x00,
        Tag_GPSLatitudeRef_High=0x00,Tag_GPSLatitudeRef_Low=0x01,
        Tag_GPSLatitude_High=0x00,Tag_GPSLatitude_Low=0x02,
        Tag_GPSLongitudeRef_High=0x00,Tag_GPSLongitudeRef_Low=0x03,
        Tag_GPSLongitude_High=0x00,Tag_GPSLongitude_Low=0x04,
        Tag_GPSAltitudeRef_High=0x00,Tag_GPSAltitudeRef_Low=0x05,
        Tag_GPSAltitude_High=0x00,Tag_GPSAltitude_Low=0x06,
        Tag_GPSTimeStamp_High=0x00,Tag_GPSTimeStamp_Low=0x07,
        Tag_GPSSatellites_High=0x00,Tag_GPSSatellites_Low=0x08,
        Tag_GPSStatus_High=0x00,Tag_GPSStatus_Low=0x09,
        Tag_GPSMeasureMode_High=0x00,Tag_GPSMeasureMode_Low=0x0A,
        Tag_GPSDOP_High=0x00,Tag_GPSDOP_Low=0x0B,
        Tag_GPSImgDirectionRef_High=0x00,Tag_GPSImgDirectionRef_Low=0x10,
        Tag_GPSImgDirection_High=0x00,Tag_GPSImgDirection_Low=0x11,
        Tag_GPSMapDatum_High=0x00,Tag_GPSMapDatum_Low=0x12,
        Tag_GPSProcessingMethod_High=0x00,Tag_GPSProcessingMethod_Low=0x1B,
        Tag_GPSDateStamp_High=0x00,Tag_GPSDateStamp_Low=0x1D,

        //TagID's of InterOperability IFD
        Tag_InteroperabilityIndex_High=0x00,Tag_InteroperabilityIndex_Low=0x01,
        Tag_InteroperabilityVersion_High=0x00,Tag_InteroperabilityVersion_Low=0x02,


    }Tag;

    typedef enum
    {
        ExifVariable_TitleLength = 10,  ExifVariable_CameraManufacturer = 15,
        ExifVariable_CameraModel = 7,  ExifVariable_XResolution = 8,
        ExifVariable_YResolution = 8, ExifVariable_SoftwareLength = 10,
        ExifVariable_DateandTime = 20, ExifVariable_ArtistNameLength = 12,
        ExifVariable_CopyrightLength = 10,  ExifVariable_ExposureTime = 8,
        ExifVariable_CompressedBitsPerPixel = 8, ExifVariable_ExposureBias = 8,
        ExifVariable_FNumber = 8, ExifVariable_Focallength = 8,
        ExifVariable_MaxAperture = 8, ExifVariable_SubjectDistanceRange = 8,
        ExifVariable_ZoomFactor = 8,ExifVariable_UserComment = 32, ExifVariable_MakerNote = 32,
        ExifVariable_ShutterSpeedValue = 8, ExifVariable_ApertureValue = 8,
        ExifVariable_BrightnessValue =8,
    }ExifVariable;

    typedef enum
    {
        //GPStagVariableValues which have length greater than "FOUR" bytes
        GPSTagLength_Latitude=24, GPSTagLength_Longitude=24,
        GPSTagLength_Altitude=8, GPSTagLength_TimeStamp=24,
        GPSTagLength_DOP=8, GPSTagLength_ImgDirection = 8,

        //GPStagVariableValues which have length less than/Equals"FOUR" bytes
        GPSTagLength_ImgDirectionRef = 2,
    }GPSTagLength;

    typedef enum
    {
        InterOperabilityTagLength_Index=32,
    }InterOperabilityTagLength;

#define MIN_NO_OF_GPS_TAGS (1) //GPSVersionID
    typedef enum
    {
        // TODO: TagCount_EXIFIFD should not be hardcoded. Can use enum
        // trick to update it if we have more IFD tags ?
        TagCount_0thIFD=13,TagCount_EXIFIFD=36,
        TagCount_GPSIFD=17,TagCount_1stIFD=8,TagCount_InterOperabilityIFD=2,

    }TagCount;


#ifdef __cplusplus
}
#endif

#endif // INCLUDED_NV_IMAGE_ENC_EXIFHEADER_H
