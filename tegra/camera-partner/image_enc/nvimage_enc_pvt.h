/*
* Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NV_IMAGE_ENC_PVT_H
#define INCLUDED_NV_IMAGE_ENC_PVT_H

#include "nvimage_enc.h"
#include "nvimage_enc_exifheader.h"

typedef void (*pfnImageEncSelect)(NvImageEncHandle hEncoderType);
typedef NvError (*pfnEncOpen) (NvImageEncHandle hImageEnc);
typedef void (*pfnEncClose) (NvImageEncHandle hImageEnc);
typedef NvError (*pfnEncSetParameter) (NvImageEncHandle hImageEnc,
                                        NvJpegEncParameters *params);
typedef NvError (*pfnEncGetParameter) (NvImageEncHandle hImageEnc,
                                        NvJpegEncParameters *params);
typedef NvError (*pfnImageEncStart) (NvImageEncHandle hImageEnc,
                                        NvMMBuffer *InputBuffer,
                                        NvMMBuffer *OutPutBuffer,
                                        NvBool  IsThumbnail);

#define MAX_WIDTH       (8 * 1024)
#define MAX_HEIGHT      (8 * 1024)

#define MAX_QUEUE_SIZE 10
#define MAX_BITSTREAM_SIZE (10 * 1024 * 1024)
#define MAX_OUTPUT_BUFFERING 2

#define JPEG_ENC_START   (0)
#define PRIMARY_ENC_DONE    PRIMARY_ENC
#define THUMBNAIL_ENC_DONE  THUMBNAIL_ENC
#define JPEG_ENC_COMPLETE   (PRIMARY_ENC_DONE | THUMBNAIL_ENC_DONE)

#define JPEG_FRAME_ENC_MAX_HEADER_LENGTH 4096    //4k Size Normally it takes 3148 Bytes

#define JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET   0x10003    //0xFFFF + 4
#define JPEG_FRAME_ENC_MAX_THUMBNAIL_SIZE   JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET
#define JPEG_FRAME_ENC_MAX_APP_MARKER_SIZE  JPEG_FRAME_ENC_PRIMARY_MAX_OFFSET



#define CAPTURE_PROFILING       0

typedef struct JPEGHeaderParamsRec {
    NvU32               NumofExifTags;
    NvU32               NoOf_0IFD_Fields;
    NvRmMemHandle       hExifInfo;
    NvRmMemHandle       hMakernoteExtension;
    NvU8                *pbufHeader;
    NvU32               ThumbNailSize;
    NvU32               WH_offset;
    NvU32               headeroffset;
    NvU32               JpegIFLengthLocation;
    NvU32               lenImageDescription;
    NvU32               lenMake;
    NvU32               lenModel;
    NvU32               lenCopyright;
    NvU32               lenArtist;
    NvU32               lenSoftware;
    NvU32               defaultLen;
    NvU32               lenMakerNote;
    NvU32               lenUserComment;
    NvU32               lenImageUniqueId;
    NvU32               lenSubjectArea;
    NvU32               NoOf_GPS_Tags;
    NvU32               Length_Of_GPS_IFD;
    NvBool              bGPSLatitudeTag_Present;
    NvBool              bGPSLongitudeTag_Present;
    NvBool              bGPSAltitudeTag_Present;
    NvBool              bGPSTimeStampTag_Present;
    NvBool              bGPSSatellitesTag_Present;
    NvBool              bGPSStatusTag_Present;
    NvBool              bGPSMeasureModeTag_Present;
    NvBool              bGPSDOPTag_Present;
    NvBool              bGPSImgDirectionTag_Present;
    NvBool              bGPSMapDatumTag_Present;
    NvBool              bGPSDateStampTag_Present;
    NvBool              bGPSProcessingMethodTag_Present;
    NvU32               GPSTagLength_Satellites;
    NvU32               GPSTagLength_MapDatum;
    NvU32               GPSTagLength_GPSProcessingMethod;
    NvU32               GPSTagLength_DateStamp;
} JPEGHeaderParams;

typedef struct NvEncoderPrivateRec
{
    NvU32                   SupportLevel;
    pfnEncOpen              pfnOpen;
    pfnEncClose             pfnClose;
    pfnEncSetParameter      pfnSetParam;
    pfnEncGetParameter      pfnGetParam;
    pfnImageEncStart        pfnStartEncoding;

    void*                   pPrivateContext;
    JPEGHeaderParams        HeaderParams;
    NvJpegEncParameters     PriParams;
    NvJpegEncParameters     ThumbParams;
    NvU32                   PrimarySize;

} NvEncoderPrivate;

typedef struct NvImageEncRec {
    void                        *pClientContext;
    //NvJpegEncParameters         *pEncParams;
    NvImageEncBufferCB          ClientCB;
    NvEncoderPrivate            *pEncoder;
    NvU32                       EncStartState;
} NvImageEnc;

#endif // INCLUDED_NV_IMAGE_ENC_PVT_H
