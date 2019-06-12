/* Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file
 * <b>NVIDIA Tegra: OpenMAX Encoder Extension Interface</b>
 *
 */

/**
 * @defgroup nv_omx_il_encoder Encoder
 *
 * This is the NVIDIA OpenMAX encoder class extensions interface.
 *
 * These extensions include ultra low power (ULP) mode, video de-interlacing, JPEG EXIF info,
 * thumbnail generation and more.
 *
 * @ingroup nvomx_encoder_extension
 * @{
 */

#ifndef NVOMX_EncoderExtensions_h_
#define NVOMX_EncoderExtensions_h_

#define MAX_EXIF_STRING_IN_BYTES                   (200)
#define MAX_GPS_STRING_IN_BYTES                    (32)
#define GPS_DATESTAMP_LENGTH                       (11)
#define MAX_INTEROP_STRING_IN_BYTES                (32)
#define MAX_GPS_PROCESSING_METHOD_IN_BYTES         (40)

/* General encoder extensions */

/* Audio encoder extensions */

/* Video encoder extensions */

/* JPEG encoder extensions */

/** Config extension index to set EXIF information (image encoder classes only).
 *  See ::NVX_CONFIG_ENCODEEXIFINFO
 */
#define NVX_INDEX_CONFIG_ENCODEEXIFINFO "OMX.Nvidia.index.config.encodeexifinfo"
/** Holds information to set EXIF information. */
typedef struct NVX_CONFIG_ENCODEEXIFINFO
{
    OMX_U32 nSize;                          /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;               /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                     /**< Port that this struct applies to */

    OMX_S8  ImageDescription[MAX_EXIF_STRING_IN_BYTES];     /**< String describing image */
    OMX_S8  Make[MAX_EXIF_STRING_IN_BYTES];                 /**< String of image make */
    OMX_S8  Model[MAX_EXIF_STRING_IN_BYTES];                /**< String of image model */
    OMX_S8  Copyright[MAX_EXIF_STRING_IN_BYTES];            /**< String of image copyright */
    OMX_S8  Artist[MAX_EXIF_STRING_IN_BYTES];               /**< String of image artist */
    OMX_S8  Software[MAX_EXIF_STRING_IN_BYTES];             /**< String of software creating image */
    OMX_S8  DateTime[MAX_EXIF_STRING_IN_BYTES];             /**< String of date and time */
    OMX_S8  DateTimeOriginal[MAX_EXIF_STRING_IN_BYTES];     /**< String of original date and time */
    OMX_S8  DateTimeDigitized[MAX_EXIF_STRING_IN_BYTES];    /**< String of digitized date and time */
    OMX_U16 filesource;                                     /**< File source */
    OMX_S8  UserComment[MAX_EXIF_STRING_IN_BYTES];          /**< String user comments */
    OMX_U16 Orientation;                                    /**< Orientation of the image: 0,90,180,270*/
} NVX_CONFIG_ENCODEEXIFINFO;

/** OMX  Encode GpsBitMap Type
  * Enable and disable the exif gps fields individually
  * See also NVX_CONFIG_ENCODEGPSINFO.GPSBitMapInfo
  */
typedef enum OMX_ENCODE_GPSBITMAPTYPE {
    OMX_ENCODE_GPSBitMapLatitudeRef =      0x01,
    OMX_ENCODE_GPSBitMapLongitudeRef =     0x02,
    OMX_ENCODE_GPSBitMapAltitudeRef =      0x04,
    OMX_ENCODE_GPSBitMapTimeStamp =        0x08,
    OMX_ENCODE_GPSBitMapSatellites =       0x10,
    OMX_ENCODE_GPSBitMapStatus =           0x20,
    OMX_ENCODE_GPSBitMapMeasureMode =      0x40,
    OMX_ENCODE_GPSBitMapDOP =              0x80,
    OMX_ENCODE_GPSBitMapImgDirectionRef =  0x100,
    OMX_ENCODE_GPSBitMapMapDatum =         0x200,
    OMX_ENCODE_GPSBitMapProcessingMethod = 0x400,
    OMX_ENCODE_GPSBitMapDateStamp =        0x800,
    OMX_ENCODE_GPSBitMapMax =              0x7FFFFFFF
} OMX_ENCODE_GPSBITMAPTYPE;



/** Config extension index to set GPS information (image encoder classes only).
 *  See ::NVX_CONFIG_ENCODEGPSINFO
 */
#define NVX_INDEX_CONFIG_ENCODEGPSINFO "OMX.Nvidia.index.config.encodegpsinfo"
/** Holds information to set GPS information. */
typedef struct NVX_CONFIG_ENCODEGPSINFO
{
    OMX_U32 nSize;                          /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;               /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                     /**< Port that this struct applies to */

    OMX_U32 GPSBitMapInfo;
    OMX_U32 GPSVersionID;                   /**< Version identifier */
    OMX_S8  GPSLatitudeRef[2];              /**< Latitude reference */
    OMX_U32 GPSLatitudeNumerator[3];        /**< Latitude numerator */
    OMX_U32 GPSLatitudeDenominator[3];      /**< Latitude denominator */
    OMX_S8  GPSLongitudeRef[2];             /**< Longitude reference */
    OMX_U32 GPSLongitudeNumerator[3];       /**< Longitude numerator */
    OMX_U32 GPSLongitudeDenominator[3];     /**< Longitude denominator */
    OMX_U8  GPSAltitudeRef;                 /**< Altitude reference */
    OMX_U32 GPSAltitudeNumerator;           /**< Altitude numerator */
    OMX_U32 GPSAltitudeDenominator;         /**< Altitude denominator */
    OMX_U32 GPSTimeStampNumerator[3];       /**< Timestamp numerator */
    OMX_U32 GPSTimeStampDenominator[3];     /**< Timestamp denominator */
    OMX_S8  GPSSatellites[MAX_GPS_STRING_IN_BYTES];
    OMX_S8  GPSStatus[2];
    OMX_S8  GPSMeasureMode[2];
    OMX_U32 GPSDOPNumerator;
    OMX_U32 GPSDOPDenominator;
    OMX_S8  GPSImgDirectionRef[2];
    OMX_U32 GPSImgDirectionNumerator;
    OMX_U32 GPSImgDirectionDenominator;
    OMX_S8  GPSMapDatum[MAX_GPS_STRING_IN_BYTES];
    OMX_S8  GPSDateStamp[GPS_DATESTAMP_LENGTH];
    OMX_S8  GPSProcessingMethod[MAX_GPS_PROCESSING_METHOD_IN_BYTES];
} NVX_CONFIG_ENCODEGPSINFO;


/** Config extension index to set Interoperability IFD information (image encoder classes only).
 *  See ::NVX_CONFIG_ENCODE_INTEROPERABILITYINFO
 */
#define NVX_INDEX_CONFIG_ENCODE_INTEROPINFO "OMX.Nvidia.index.config.encodeinteropinfo"
/** Holds information to set GPS information. */

typedef struct NVX_CONFIG_ENCODE_INTEROPINFO
{
    OMX_U32 nSize;                          /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;               /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                     /**< Port that this struct applies to */

    OMX_S8 Index[MAX_INTEROP_STRING_IN_BYTES];
} NVX_CONFIG_ENCODE_INTEROPINFO;

/** Config extension to mirror ::OMX_IndexParamQuantizationTable.
 * See: ::OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE
 */
#define NVX_INDEX_CONFIG_ENCODE_QUANTIZATIONTABLE \
    "OMX.NVidia.index.config.encodequantizationtable"

/** Config extension index to set thumbnail quality factor for JPEG encoder (image encoder classes only).
 */
#define NVX_INDEX_CONFIG_THUMBNAILQF  "OMX.Nvidia.index.config.thumbnailqf"

/** Param extension index to set/get quantization table (luma and chroma) for thumbnail image.
 * (jpeg image encoder class only)
 * See OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE
 */
#define NVX_INDEX_PARAM_THUMBNAILIMAGEQUANTTABLE "OMX.Nvidia.index.param.thumbnailquanttable"

typedef enum NVX_VIDEO_VP8PROFILETYPE {
    NVX_VIDEO_VP8ProfileMain                = 0x01,
    NVX_VIDEO_VP8ProfileMax                 = 0x7FFFFFFF
} NVX_VIDEO_VP8PROFILETYPE;

typedef enum NVX_VIDEO_VP8LEVELTYPE {
    NVX_VIDEO_VP8Level_Version0             = 0x0,
    NVX_VIDEO_VP8Level_Version1             = 0x1,
    NVX_VIDEO_VP8Level_Version2             = 0x2,
    NVX_VIDEO_VP8Level_Version3             = 0x3,
    NVX_VIDEO_VP8LevelMax                   = 0x7FFFFFFF
} NVX_VIDEO_VP8LEVELTYPE;

/** Config extension index to set VP8 encoding parameters
 */
#define NVX_INDEX_PARAM_VP8TYPE "OMX.Nvidia.index.param.vp8type"

typedef struct NVX_VIDEO_PARAM_VP8TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    NVX_VIDEO_VP8PROFILETYPE eProfile;
    NVX_VIDEO_VP8LEVELTYPE eLevel;
    OMX_U32 nDCTPartitions;
    OMX_BOOL bErrorResilientMode;
    OMX_U32 nPFrames;
    OMX_U32 filter_level;
    OMX_U32 sharpness_level;
} NVX_VIDEO_PARAM_VP8TYPE;

/* OMX extension index to configure encoder to send - P frame with all skipped MBs */
/**< reference: NVX_INDEX_PARAM_VIDENC_GEN_SKIP_MB_FRAMES
 * Use OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_PARAM_VIDENC_SKIP_FRAME "OMX.Nvidia.index.param.videncskipframe"

#endif
/** @} */
/* File EOF */


