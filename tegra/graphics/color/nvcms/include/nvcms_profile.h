/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCMS_PROFILE_H
#define INCLUDED_NVCMS_PROFILE_H

#include "nvcms.h"
#include "nvrm_surface.h"
#include "nverror.h"

/*
 * Utility macros
 */

/* Homogenize a 3x3 matrix to 4x4. */
#define NVCMS_MX_HGEN(m00, m01, m02,              \
                      m10, m11, m12,              \
                      m20, m21, m22)              \
                         { m00,  m01,  m02, 0.0f, \
                           m10,  m11,  m12, 0.0f, \
                           m20,  m21,  m22, 0.0f, \
                           0.0f, 0.0f, 0.0f, 1.0f }

/* This matrix translation macro makes it easier to
 * write and verify standard matrices found in literature,
 * that are typically in row-major order.
 */
#define NVCMS_MX_TRANS(m00, m01, m02, m03,        \
                       m10, m11, m12, m13,        \
                       m20, m21, m22, m23,        \
                       m30, m31, m32, m33)        \
                         { m00, m10, m20, m30,    \
                           m01, m11, m21, m31,    \
                           m02, m12, m22, m32,    \
                           m03, m13, m23, m33 }

/* Homogenize and translate. */
#define NVCMS_MX_HGTR(m00, m01, m02,              \
                      m10, m11, m12,              \
                      m20, m21, m22)              \
                          NVCMS_MX_HGEN(m00, m10, m20, \
                                        m01, m11, m21, \
                                        m02, m12, m22) \

/*
 * Constants
 */

#define NVCMS_WHITE_POINT_D65 {NvCmsStandardWhitePoint_D65, {{0.9505f, 1.0000f, 1.0890f, 1.0000f}}}
#define NVCMS_TRC_sRGB {NvCmsTrcType_sRGB, 2.2f, NULL, 0}
#define NVCMS_TRC_SET_sRGB { NVCMS_TRC_sRGB, NVCMS_TRC_sRGB, NVCMS_TRC_sRGB }
#define NVCMS_TRC_REC709 {NvCmsTrcType_Rec709, 2.2f, NULL, 0}
#define NVCMS_TRC_SET_REC709 { NVCMS_TRC_REC709, NVCMS_TRC_REC709, NVCMS_TRC_REC709 }

/*
 * Profile data types
 */

typedef enum
{
    NvCmsStandardWhitePoint_None    = 0,

    NvCmsStandardWhitePoint_Other,
    NvCmsStandardWhitePoint_D50,
    NvCmsStandardWhitePoint_D65,

    NvCmsStandardWhitePoint_Force32 = 0x7FFFFFFF
} NvCmsStandardWhitePoint;

typedef struct
{
    NvCmsStandardWhitePoint std;
    NvCmsVector XYZ;
} NvCmsWhitePoint;

typedef enum
{
    NvCmsChromaticAdaptationType_None           = 0,

    NvCmsChromaticAdaptationType_Bradford,
    NvCmsChromaticAdaptationType_vonKries,
    NvCmsChromaticAdaptationType_XYZScaling,

    NvCmsChromaticAdaptationType_Force32        = 0x7FFFFFFF
} NvCmsChromaticAdaptationType;

typedef enum
{
    NvCmsRenderingIntent_None                   = 0,

    NvCmsRenderingIntent_RelativeColorimetric,
    NvCmsRenderingIntent_Perceptual,
    NvCmsRenderingIntent_Saturation,
    NvCmsRenderingIntent_AbsoluteColorimetric,

    NvCmsRenderingIntent_Force32                = 0x7FFFFFFF
} NvCmsRenderingIntent;

// TODO [TaCo]: Separate interpolated LUT and parametric TRCs.
typedef enum
{
    NvCmsTrcType_None       = 0,

    NvCmsTrcType_Linear,
    NvCmsTrcType_Gamma,
    NvCmsTrcType_sRGB,
    NvCmsTrcType_Rec709,

    NvCmsTrcType_LUT,

    NvCmsTrcType_Force32    = 0x7FFFFFFF
} NvCmsTrcType;

typedef enum
{
    NvCmsLutType_None       = 0,
    NvCmsLutType_1D,
    NvCmsLutType_2D,
    NvCmsLutType_3D,

    NvCmsLutType_Force32    = 0x7FFFFFFF
} NvCmsLutType;

// TODO: Support more than 8bit LUT gamma
typedef struct
{
    NvCmsTrcType type;
    NvF32        gamma;
    NvU8*        lutData;
    NvU32        lutLength;
} NvCmsTrc;

typedef struct
{
    NvCmsTrc r;
    NvCmsTrc g;
    NvCmsTrc b;
} NvCmsTrcSet;

typedef struct
{
    /* dev to PCS */
    NvCmsMatrix transform;

    /* PCS to dev */
    NvCmsMatrix inverse;
} NvCmsChromaticAdaptation;

typedef struct
{
    NvCmsLutType type;
    NvRmSurface  lutsurf;
} NvCmsLut;

typedef enum
{
    NvCmsProfileType_None       = 0,

    NvCmsProfileType_Matrix,
    NvCmsProfileType_LUT,

    NvCmsProfileType_Force32    = 0x7FFFFFFF
} NvCmsProfileType;

typedef enum
{
    NvCmsProfileClass_None      = 0,

    /* PCS <-> display conversion */
    NvCmsProfileClass_Display,

    /* profile to profile conversion */
    NvCmsProfileClass_DeviceLink,

    NvCmsProfileClass_Force32   = 0x7FFFFFFF
} NvCmsProfileClass;

struct NvCmsProfileRec
{
    NvCmsProfileType     type;
    NvCmsProfileClass    pclass;
    NvCmsRenderingIntent intent;
};

struct NvCmsDisplayProfileRec
{
    NvCmsProfile                    base;

    NvCmsAbsColorSpace              colorSpace;
    NvCmsAbsColorSpace              pcs;

    /* TODO [TaCo]: White point and adaptation defs are redundant,
     * unless we support PCS white points other than D65.
     */
    NvCmsWhitePoint                 whitePoint;

    /*
     * TODO [TaCo]: Adding NvCmsChromaticAdaptation struct with pre-
     * calculated adaptation matrices is required, if adaptation
     * between different PCS white points is to be supported.
     */
    NvCmsChromaticAdaptationType    adaptation;

    NvCmsTrcSet                     trcs;

    NvBool                          toPCS;

    /* column-major order */
    NvCmsMatrix                     matrix;
};

struct NvCmsDeviceLinkProfileRec
{
    NvCmsProfile                    base;

    NvU32                           refCount;

    NvCmsAbsColorSpace              inputColorSpace;
    NvCmsAbsColorSpace              outputColorSpace;

    NvCmsTrcSet                     inputTrcs;
    NvCmsTrcSet                     outputTrcs;

    /* column-major order */
    NvCmsMatrix                     matrix;

    /* index and CSC LUTs */
    NvCmsLut                        indexlut;
    NvCmsLut                        lut;
};

/*
 * Standard profile definitions
 */

static const NvCmsChromaticAdaptation NvCmsChromaticAdaptation_Bradford =
{
    {NVCMS_MX_HGTR( 0.8951000f,  0.2664000f, -0.1614000f,
                    -0.7502000f,  1.7135000f,  0.0367000f,
                     0.0389000f, -0.0685000f,  1.0296000f)},

    {NVCMS_MX_HGTR( 0.9869929f, -0.1470543f, 0.1599627f,
                     0.4323053f,  0.5183603f, 0.0492912f,
                    -0.0085287f,  0.0400428f, 0.9684867f)}
};

static const NvCmsDisplayProfile NvCmsMatrixProfile_sRGB =
{
    {NvCmsProfileType_Matrix,
     NvCmsProfileClass_Display,
     NvCmsRenderingIntent_AbsoluteColorimetric},

    NvCmsAbsColorSpace_sRGB,
    NvCmsAbsColorSpace_CIEXYZ,

    NVCMS_WHITE_POINT_D65,
    NvCmsChromaticAdaptationType_None,

    NVCMS_TRC_SET_sRGB,
    NV_TRUE,
    {NVCMS_MX_HGTR( 0.4124f, 0.3576f, 0.1805f,
                     0.2126f, 0.7152f, 0.0722f,
                     0.0193f, 0.1192f, 0.9505f)}
};

static const NvCmsDisplayProfile NvCmsMatrixProfile_AdobeRGB =
{
    {NvCmsProfileType_Matrix,
     NvCmsProfileClass_Display,
     NvCmsRenderingIntent_AbsoluteColorimetric},

    NvCmsAbsColorSpace_AdobeRGB,
    NvCmsAbsColorSpace_CIEXYZ,

    NVCMS_WHITE_POINT_D65,
    NvCmsChromaticAdaptationType_None,

    NVCMS_TRC_SET_sRGB,
    NV_TRUE,
    {NVCMS_MX_HGTR( 0.5767f, 0.1856f, 0.1882f,
                     0.2974f, 0.6273f, 0.0753f,
                     0.0270f, 0.0707f, 0.9911f)}
};

static const NvCmsDisplayProfile NvCmsMatrixProfile_WideGamutRGB =
{
    {NvCmsProfileType_Matrix,
     NvCmsProfileClass_Display,
     NvCmsRenderingIntent_AbsoluteColorimetric},

    NvCmsAbsColorSpace_WideGamutRGB,
    NvCmsAbsColorSpace_CIEXYZ,

    NVCMS_WHITE_POINT_D65,
    NvCmsChromaticAdaptationType_None,

    NVCMS_TRC_SET_sRGB,
    NV_TRUE,
    {NVCMS_MX_HGTR( 0.7164f, 0.1010f, 0.1468f,
                     0.2587f, 0.7247f, 0.0166f,
                     0.0000f, 0.0512f, 0.7740f)}
};

static const NvCmsDisplayProfile NvCmsMatrixProfile_Rec709 =
{
    {NvCmsProfileType_Matrix,
     NvCmsProfileClass_Display,
     NvCmsRenderingIntent_AbsoluteColorimetric},

    NvCmsAbsColorSpace_Rec709,
    NvCmsAbsColorSpace_CIEXYZ,

    NVCMS_WHITE_POINT_D65,
    NvCmsChromaticAdaptationType_None,

    NVCMS_TRC_SET_REC709,
    NV_TRUE,
    {NVCMS_MX_HGTR( 0.4124f, 0.3576f, 0.1805f,
                     0.2126f, 0.7152f, 0.0722f,
                     0.0193f, 0.1192f, 0.9505f)}
};

/*
 * Profile conversion functions
 */

static NV_INLINE const NvCmsDisplayProfile*
NvCmsAbsColorSpaceToDisplayMatrixProfile(NvCmsAbsColorSpace cs)
{
    switch(cs)
    {
    case NvCmsAbsColorSpace_sRGB:
        return &NvCmsMatrixProfile_sRGB;
    case NvCmsAbsColorSpace_AdobeRGB:
        return &NvCmsMatrixProfile_AdobeRGB;
    case NvCmsAbsColorSpace_WideGamutRGB:
        return &NvCmsMatrixProfile_WideGamutRGB;
    case NvCmsAbsColorSpace_Rec709:
        return &NvCmsMatrixProfile_Rec709;
    default:
        return NULL;
    }
}

/*
 * Profile functions
 */

NvError NvCmsDisplayProfileCreate(NvCmsDisplayProfile** prof, int dev);
void NvCmsDisplayProfileDestroy(NvCmsDisplayProfile* prof);
void NvCmsDisplayProfileInit(NvCmsDisplayProfile* pp);

void NvCmsDeviceLinkProfileInit(NvCmsDeviceLinkProfile* pp);
void NvCmsDeviceLinkProfileDeinit(NvCmsDeviceLinkProfile* profile);
void NvCmsDeviceLinkProfileRef(NvCmsDeviceLinkProfile* pp);
void NvCmsDeviceLinkProfileUnref(NvCmsDeviceLinkProfile* pp);

NvError NvCmsDeviceLinkProfileCreate(NvCmsContext* ctx,
                                     const NvCmsDisplayProfile* input,
                                     const NvCmsDisplayProfile* output,
                                     NvBool invert,
                                     NvCmsProfileType type,
                                     NvRmDeviceHandle RmDevice,
                                     NvCmsDeviceLinkProfile** prof);

#endif // INCLUDED_NVCMS_PROFILE_H
