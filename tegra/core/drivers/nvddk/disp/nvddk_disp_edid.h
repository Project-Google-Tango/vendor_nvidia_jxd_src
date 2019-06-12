/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_DISP_EDID_H
#define INCLUDED_NVDDK_DISP_EDID_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvrm_init.h"
#include "nvddk_disp.h"
#include "nvodm_disp.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

typedef enum
{
    NvEdidEstablishedTiming1_800_600_60 = 1,
    NvEdidEstablishedTiming1_800_600_56 = 2,
    NvEdidEstablishedTiming1_640_480_75 = 4,
    NvEdidEstablishedTiming1_640_480_72 = 8,
    NvEdidEstablishedTiming1_640_480_67 = 16,
    NvEdidEstablishedTiming1_640_480_60 = 32,
    NvEdidEstablishedTiming1_720_400_88 = 64,
    NvEdidEstablishedTiming1_720_400_70 = 128,
} NvEdidEstablishedTiming1;

typedef enum
{
    NvEdidEstablishedTiming2_1280_1024_75 = 1,
    NvEdidEstablishedTiming2_1024_768_75 = 2,
    NvEdidEstablishedTiming2_1024_768_70 = 4,
    NvEdidEstablishedTiming2_1024_768_60 = 8,
    NvEdidEstablishedTiming2_1024_768_87 = 16,
    NvEdidEstablishedTiming2_832_624_75 = 32,
    NvEdidEstablishedTiming2_800_600_75 = 64,
    NvEdidEstablishedTiming2_800_600_72 = 128,
} NvEdidEstablishedTiming2;

typedef enum
{
    NvEdidEst3Res0_1152_864_75 = 1,
    NvEdidEst3Res0_1024_768_85 = 2,
    NvEdidEst3Res0_800_600_85 = 4,
    NvEdidEst3Res0_848_480_60 = 8,
    NvEdidEst3Res0_640_480_85 = 16,
    NvEdidEst3Res0_720_400_85 = 32,
    NvEdidEst3Res0_640_400_85 = 64,
    NvEdidEst3Res0_640_350_85 = 128,
} NvEdidEst3Res0;

typedef enum
{
    NvEdidEst3Res1_1280_1024_85 = 1,
    NvEdidEst3Res1_1280_1024_60 = 2,
    NvEdidEst3Res1_1280_960_85 = 4,
    NvEdidEst3Res1_1280_960_60 = 8,
    NvEdidEst3Res1_1280_768_85 = 16,
    NvEdidEst3Res1_1280_768_75 = 32,
    NvEdidEst3Res1_1280_768_60 = 64,
    NvEdidEst3Res1_1280_768_60_RB = 128, /* RB= reduced blanking */
} NvEdidEst3Res1;

typedef enum
{
    NvEdidEst3Res2_1400_1050_75 = 1,
    NvEdidEst3Res2_1400_1050_60 = 2,
    NvEdidEst3Res2_1400_1050_60_RB = 4,
    NvEdidEst3Res2_1440_900_85 = 8,
    NvEdidEst3Res2_1440_900_75 = 16,
    NvEdidEst3Res2_1440_900_60 = 32,
    NvEdidEst3Res2_1440_900_60_RB = 64,
    NvEdidEst3Res2_1360_768_60 = 128,
} NvEdidEst3Res2;

typedef enum
{
    NvEdidEst3Res3_1600_1200_70 = 1,
    NvEdidEst3Res3_1600_1200_65 = 2,
    NvEdidEst3Res3_1600_1200_60 = 4,
    NvEdidEst3Res3_1680_1050_85 = 8,
    NvEdidEst3Res3_1680_1050_75 = 16,
    NvEdidEst3Res3_1680_1050_60 = 32,
    NvEdidEst3Res3_1680_1050_60_RB = 64,
    NvEdidEst3Res3_1400_1050_85 = 128,
} NvEdidEst3Res3;

typedef enum
{
    NvEdidEst3Res4_1920_1200_60 = 1,
    NvEdidEst3Res4_1920_1200_60_RB = 2,
    NvEdidEst3Res4_1856_1392_75 = 4,
    NvEdidEst3Res4_1856_1392_60 = 8,
    NvEdidEst3Res4_1792_1344_75 = 16,
    NvEdidEst3Res4_1792_1344_60 = 32,
    NvEdidEst3Res4_1600_1200_85 = 64,
    NvEdidEst3Res4_1600_1200_75 = 128,
} NvEdidEst3Res4;

typedef enum
{
    NvEdidEst3Res5_1920_1440_75 = 16,
    NvEdidEst3Res5_1920_1440_60 = 32,
    NvEdidEst3Res5_1920_1200_85 = 64,
    NvEdidEst3Res5_1920_1200_75 = 128,
} NvEdidEst3Res5;


typedef struct NvEdidEstablisedTiming3Rec
{
    NvEdidEst3Res0 Res0;
    NvEdidEst3Res1 Res1;
    NvEdidEst3Res2 Res2;
    NvEdidEst3Res3 Res3;
    NvEdidEst3Res4 Res4;
    NvEdidEst3Res5 Res5;
} NvEdidEstablishedTiming3;

/* only exposing supported resolutions, this is from Table 3 of the
 * CEA-861-D spec. The format of the enum is
 * NvEdidVideoId_Resolution_<CEA Format>
 */
typedef enum
{
    NvEdidVideoId_640_480_1 = 1,
    NvEdidVideoId_720_480_2 = 2,
    NvEdidVideoId_720_480_3 = 3,
    NvEdidVideoId_1280_720_4 = 4,
    NvEdidVideoId_1920_1080_16 = 16,
    NvEdidVideoId_720_576_17 = 17,
    NvEdidVideoId_720_576_18 = 18,
    NvEdidVideoId_1280_720_19 = 19,
    NvEdidVideoId_1920_1080_31 = 31,
    NvEdidVideoId_1920_1080_32 = 32,
    NvEdidVideoId_1920_1080_33 = 33,
    NvEdidVideoId_1920_1080_34 = 34,

    NvEdidVideoId_Force32 = 0x7FFFFFFF,
} NvEdidVideoId;

typedef struct NvEdidStandardTimingIdentificationRec
{
    /* actual horizontal and vertical values calculated from the edid's
     * aspect ratio value
     */

    NvU32 HorizActive;
    NvU32 VertActive;
    NvU32 RefreshRate;
} NvEdidStandardTimingIdentification;

typedef struct NvEdidDetailedTimingDescriptionRec
{
    NvU32 PixelClock; // KHz
    NvU16 HorizActive;
    NvU16 HorizBlank;
    NvU16 VertActive;
    NvU16 VertBlank;
    NvU16 HorizOffset;
    NvU16 VertOffset;
    NvU16 HorizPulse;
    NvU16 VertPulse;
    NvU8 HorizBoarder;
    NvU8 VertBoarder;
    NvU16 HorizImageSize;
    NvU16 VertImageSize;
    NvU8 Flags; // bit 7: interlacing
    NvBool bNative;
} NvEdidDetailedTimingDescription;

typedef struct NvEdidShortVideoDescriptorRec
{
    NvEdidVideoId Id;
    NvBool Native;
} NvEdidShortVideoDescriptor;

#define NVDDK_DISP_EDID_MAX_STI         (15)
#define NVDDK_DISP_EDID_MAX_DTD         (10)
#define NVDDK_DISP_EDID_MAX_SVD         (16)
#define NVDDK_DISP_EDID_VSDB_BASE       (8)
#define NVDDK_DISP_EDID_VSDB_PAYLOAD    (0)
#define NVDDK_DISP_EDID_BLOCK_SIZE      (128)
#define NVDDK_DISP_EDID_VSDB_MAX \
    (NVDDK_DISP_EDID_VSDB_BASE + NVDDK_DISP_EDID_VSDB_PAYLOAD)

/* this is in the VSDB (vendor specific data block) for HDMI devices */
#define NVDDK_DISP_EDID_IEEE_REG_ID     (0x000C03)

#define NVDDK_DISP_EDID_STI_TAG             (0xFA)
#define NVDDK_DISP_EDID_EST_TIMING_3_TAG    (0xF7)

typedef struct NvEdidRec
{
    NvEdidEstablishedTiming1 EstTiming1;
    NvEdidEstablishedTiming2 EstTiming2;
    NvU32 nSTIs;
    NvU32 nDTDs;
    NvU32 nSVDs;
    NvEdidStandardTimingIdentification STI[NVDDK_DISP_EDID_MAX_STI];
    NvEdidDetailedTimingDescription DTD[NVDDK_DISP_EDID_MAX_DTD];
    NvEdidShortVideoDescriptor SVD[NVDDK_DISP_EDID_MAX_SVD];

    /* see Table 8-6 in the HDMI spec */
    NvU8 VendorSpecificDataBlock[NVDDK_DISP_EDID_VSDB_MAX];

    NvU8 VideoInputDefinition;
    NvU8 Version;
    NvU8 Revision;

    NvBool bHasCeaExtension;
    NvBool bHas3dSupport;
    NvEdidEstablishedTiming3  EstTiming3;
} NvDdkDispEdid;

/**
 * Reads an EDID from the display device.
 */
NvError NvDdkDispEdidRead( NvRmDeviceHandle hRm,
    NvDdkDispDisplayHandle hDisplay, NvDdkDispEdid *edid );

/**
 * Converts an EDID to display modes.
 */
void NvDdkDispEdidExport( NvDdkDispEdid *edid, NvU32 *nModes,
    NvOdmDispDeviceMode *modes, NvU32 flags );
#define NVDDK_DISP_EDID_EXPORT_FLAG_VESA    (0x1)
#define NVDDK_DISP_EDID_EXPORT_FLAG_ALL     (0x2)

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
