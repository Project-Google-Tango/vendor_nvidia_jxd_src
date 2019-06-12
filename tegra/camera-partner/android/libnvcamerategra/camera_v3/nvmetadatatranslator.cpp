/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include <math.h>
#include "nvmetadatatranslator.h"
#include "nvcamerahal3common.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"

#define JPEG_FRAME_DURATION 200000000
#define FPS_30 33333334LL
#define MAX_DIGITAL_ZOOM 8.0f

namespace android
{

static NvS32 translateIntentToTemplate(NvU32 intent);

struct NvAndroidCvt
{
    uint8_t anVal;
    int32_t nvVal;
};

static const NvCamPropertyU64List kAvailableJpegMinDurations =
{
    {
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION,
        JPEG_FRAME_DURATION
    },
    19
};

static const NvCamPropertySizeList kAvailableJpegSizes =
{
    {
        {320, 240},     //qvga
        {640, 480},     //vga
        {1024, 768},    //1M
        {1280, 720},    //720p
        {1280, 960},    //1.3M
        {1440, 1080},   //1.5M
        {1600, 1200},   //2M
        {1920, 1080},   //2M
        {2048, 1536},   //3M
        {2104, 1560},   //half-res (max-sensor)
        {2688, 1520},   //4M
        {2592, 1458},   //3.7M (16:9)
        {2592, 1944},   //5M
        {3264, 1836},   //6M (16:9)
        {3264, 2448},   //8M
        {3896, 2192},   //8.5M
        {4128, 3096},   //12.8M
        {4208, 2368},   //10M (16:9)
        {4208, 3120}    //13M
    },
    19
};

static const NvCamPropertySizeList kAvailableThumbnailSizes =
{
    {
        {320, 240},
        {160, 120},
        {0, 0}
    },
    3
};

static const NvCamPropertyList kScalerAvailableFormats = {
    {
        HAL_PIXEL_FORMAT_RAW_SENSOR,
        HAL_PIXEL_FORMAT_BLOB,
        HAL_PIXEL_FORMAT_RGBA_8888,
        HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
        // These are handled by YCbCr_420_888
        HAL_PIXEL_FORMAT_YV12,
        HAL_PIXEL_FORMAT_YCrCb_420_SP,
        HAL_PIXEL_FORMAT_YCbCr_420_888
    },
    7
};

static const NvAndroidCvt AE_ANTIBANDING_MODE_TABLE[] =
{
    {
        ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF,
        NvCamAeAntibandingMode_Off
    },
    {
        ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ,
        NvCamAeAntibandingMode_50Hz
    },
    {
        ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ,
        NvCamAeAntibandingMode_60Hz
    },
    {
        ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO,
        NvCamAeAntibandingMode_AUTO
    }
};

static const NvAndroidCvt COLOR_CORRECTION_MODE_TABLE[] =
{
    {
        ANDROID_COLOR_CORRECTION_MODE_TRANSFORM_MATRIX,
        NvCamColorCorrectionMode_Matrix
    },
    {
        ANDROID_COLOR_CORRECTION_MODE_FAST,
        NvCamColorCorrectionMode_Fast
    },
    {
        ANDROID_COLOR_CORRECTION_MODE_HIGH_QUALITY,
        NvCamColorCorrectionMode_HighQuality,
    }
};

static const NvAndroidCvt AE_MODE_TABLE[] =
{
    {
        ANDROID_CONTROL_AE_MODE_OFF,
        NvCamAeMode_Off
    },
    {
        ANDROID_CONTROL_AE_MODE_ON,
        NvCamAeMode_On
    },
    {
        ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH,
        NvCamAeMode_OnAutoFlash
    },
    {
        ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH,
        NvCamAeMode_OnAlwaysFlash
    },
    {
        ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE,
        NvCamAeMode_On_AutoFlashRedEye
    }
};

static const NvAndroidCvt AE_PRECAPTURE_TRIGGER_TABLE[] =
{
    {
        ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE,
        NvCamAePrecaptureTrigger_Idle
    },
    {
        ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START,
        NvCamAePrecaptureTrigger_Start
    }
};

static const NvAndroidCvt AF_TRIGGER_TABLE[] =
{
    {
        ANDROID_CONTROL_AF_TRIGGER_IDLE,
        NvCamAfTrigger_Idle
    },
    {
        ANDROID_CONTROL_AF_TRIGGER_START,
        NvCamAfTrigger_Start
    },
    {
        ANDROID_CONTROL_AF_TRIGGER_CANCEL,
        NvCamAfTrigger_Cancel
    }
};

static const NvAndroidCvt AF_MODE_TABLE[] =
{
    {
        ANDROID_CONTROL_AF_MODE_OFF,
        NvCamAfMode_Off
    },
    {
        ANDROID_CONTROL_AF_MODE_AUTO,
        NvCamAfMode_Auto
    },
    {
        ANDROID_CONTROL_AF_MODE_MACRO,
        NvCamAfMode_Macro
    },
    {
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO,
        NvCamAfMode_ContinuousVideo
    },
    {
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
        NvCamAfMode_ContinuousPicture
    },
    {
        ANDROID_CONTROL_AF_MODE_EDOF,
        NvCamAfMode_ExtDepthOfField
    }
};

static const NvAndroidCvt AWB_MODE_TABLE[] =
{
    {
        ANDROID_CONTROL_AWB_MODE_OFF,
        NvCamAwbMode_Off
    },
    {
        ANDROID_CONTROL_AWB_MODE_AUTO,
        NvCamAwbMode_Auto
    },
    {
        ANDROID_CONTROL_AWB_MODE_INCANDESCENT,
        NvCamAwbMode_Incandescent
    },
    {
        ANDROID_CONTROL_AWB_MODE_FLUORESCENT,
        NvCamAwbMode_Fluorescent
    },
    {
        ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT,
        NvCamAwbMode_WarmFluorescent
    },
    {
        ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
        NvCamAwbMode_Daylight
    },
    {
        ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
        NvCamAwbMode_CloudyDaylight
    },
    {
        ANDROID_CONTROL_AWB_MODE_TWILIGHT,
        NvCamAwbMode_Twilight
    },
    {
        ANDROID_CONTROL_AWB_MODE_SHADE,
        NvCamAwbMode_Shade
    }
};

static const NvAndroidCvt AE_LOCK_TABLE[] =
{
    {
        ANDROID_CONTROL_AE_LOCK_OFF,
        NV_FALSE
    },
    {
        ANDROID_CONTROL_AE_LOCK_ON,
        NV_TRUE
    }
};

static const NvAndroidCvt AWB_LOCK_TABLE[] =
{
    {
        ANDROID_CONTROL_AWB_LOCK_OFF,
        NV_FALSE
    },
    {
        ANDROID_CONTROL_AWB_LOCK_ON,
        NV_TRUE
    }
};

static const NvAndroidCvt EFFECT_MODE_TABLE[] =
{
    {
        ANDROID_CONTROL_EFFECT_MODE_OFF,
        NvCamColorEffectsMode_Off
    },
    {
        ANDROID_CONTROL_EFFECT_MODE_MONO,
        NvCamColorEffectsMode_Mono
    },
    {
        ANDROID_CONTROL_EFFECT_MODE_NEGATIVE,
        NvCamColorEffectsMode_Negative
    },
    {
        ANDROID_CONTROL_EFFECT_MODE_SOLARIZE,
        NvCamColorEffectsMode_Solarize
    },
    {
        ANDROID_CONTROL_EFFECT_MODE_SEPIA,
        NvCamColorEffectsMode_Sepia
    },
    {
        ANDROID_CONTROL_EFFECT_MODE_POSTERIZE,
        NvCamColorEffectsMode_Posterize
    },
    {
        ANDROID_CONTROL_EFFECT_MODE_AQUA,
        NvCamColorEffectsMode_Aqua
    }
};

static const NvAndroidCvt CONTROL_MODE_TABLE[] =
{
    {
        ANDROID_CONTROL_MODE_OFF,
        NvCamControlMode_Off
    },
    {
        ANDROID_CONTROL_MODE_AUTO,
        NvCamControlMode_Auto
    },
    {
        ANDROID_CONTROL_MODE_USE_SCENE_MODE,
        NvCamControlMode_UseSceneMode
    }
};

static const NvAndroidCvt SCENE_MODE_TABLE[] =
{
    {
        ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED,
        NvCamSceneMode_Unsupported
    },
    {
        ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY,
        NvCamSceneMode_FacePriority
    },
    {
        ANDROID_CONTROL_SCENE_MODE_ACTION,
        NvCamSceneMode_Action
    },
    {
        ANDROID_CONTROL_SCENE_MODE_PORTRAIT,
        NvCamSceneMode_Portrait
    },
    {
        ANDROID_CONTROL_SCENE_MODE_LANDSCAPE,
        NvCamSceneMode_Landscape
    },
    {
        ANDROID_CONTROL_SCENE_MODE_NIGHT,
        NvCamSceneMode_Night
    },
    {
        ANDROID_CONTROL_SCENE_MODE_NIGHT_PORTRAIT,
        NvCamSceneMode_NightPortrait
    },
    {
        ANDROID_CONTROL_SCENE_MODE_THEATRE,
        NvCamSceneMode_Theatre
    },
    {
        ANDROID_CONTROL_SCENE_MODE_BEACH,
        NvCamSceneMode_Beach
    },
    {
        ANDROID_CONTROL_SCENE_MODE_SNOW,
        NvCamSceneMode_Snow
    },
    {
        ANDROID_CONTROL_SCENE_MODE_SUNSET,
        NvCamSceneMode_Sunset
    },
    {
        ANDROID_CONTROL_SCENE_MODE_STEADYPHOTO,
        NvCamSceneMode_SteadyPhoto
    },
    {
        ANDROID_CONTROL_SCENE_MODE_FIREWORKS,
        NvCamSceneMode_Fireworks
    },
    {
        ANDROID_CONTROL_SCENE_MODE_SPORTS,
        NvCamSceneMode_Sports
    },
    {
        ANDROID_CONTROL_SCENE_MODE_PARTY,
        NvCamSceneMode_Party
    },
    {
        ANDROID_CONTROL_SCENE_MODE_CANDLELIGHT,
        NvCamSceneMode_CandleLight
    },
    {
        ANDROID_CONTROL_SCENE_MODE_BARCODE,
        NvCamSceneMode_Barcode
    }
};

static const NvAndroidCvt DEMOSAIC_MODE_TABLE[] =
{
    {
        ANDROID_DEMOSAIC_MODE_FAST,
        NvCamDemosaicMode_Fast
    },
    {
        ANDROID_DEMOSAIC_MODE_HIGH_QUALITY,
        NvCamDemosaicMode_HighQuality
    }
};

static const NvAndroidCvt EDGE_MODE_TABLE[] =
{
    {
        ANDROID_EDGE_MODE_OFF,
        NvCamEdgeEnhanceMode_Off
    },
    {
        ANDROID_EDGE_MODE_FAST,
        NvCamEdgeEnhanceMode_Fast
    },
    {
        ANDROID_EDGE_MODE_HIGH_QUALITY,
        NvCamEdgeEnhanceMode_HighQuality
    }
};

static const NvAndroidCvt GEOMETRIC_MODE_TABLE[] =
{
    {
        ANDROID_GEOMETRIC_MODE_OFF,
        NvCamGeometricMode_Off
    },
    {
        ANDROID_GEOMETRIC_MODE_FAST,
        NvCamGeometricMode_Fast
    },
    {
        ANDROID_GEOMETRIC_MODE_HIGH_QUALITY,
        NvCamGeometricMode_HighQuality
    }
};

static const NvAndroidCvt HOTPIXEL_MODE_TABLE[] =
{
    {
        ANDROID_HOT_PIXEL_MODE_OFF,
        NvCamHotPixelMode_Off
    },
    {
        ANDROID_HOT_PIXEL_MODE_FAST,
        NvCamHotPixelMode_Fast
    },
    {
        ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY,
        NvCamHotPixelMode_HighQuality
    }
};

static const NvAndroidCvt NOISE_REDUCTION_MODE_TABLE[] =
{
    {
        ANDROID_NOISE_REDUCTION_MODE_OFF,
        NvCamNoiseReductionMode_Off
    },
    {
        ANDROID_NOISE_REDUCTION_MODE_FAST,
        NvCamNoiseReductionMode_Fast,
    },
    {
        ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY,
        NvCamNoiseReductionMode_HighQuality
    }
};

static const NvAndroidCvt SHADING_MODE_TABLE[] =
{
    {
        ANDROID_SHADING_MODE_OFF,
        NvCamShadingMode_Off
    },
    {
        ANDROID_SHADING_MODE_FAST,
        NvCamShadingMode_Fast
    },
    {
        ANDROID_SHADING_MODE_HIGH_QUALITY,
        NvCamShadingMode_HighQuality
    }
};

static const NvAndroidCvt FLASH_MODE_TABLE[] =
{
    {
        ANDROID_FLASH_MODE_OFF,
        NvCamFlashMode_Off
    },
    {
        ANDROID_FLASH_MODE_SINGLE,
        NvCamFlashMode_Single
    },
    {
        ANDROID_FLASH_MODE_TORCH,
        NvCamFlashMode_Torch
    }
};

static const NvAndroidCvt FACE_DETECT_MODE_TABLE[] =
{
    {
        ANDROID_STATISTICS_FACE_DETECT_MODE_OFF,
        NvCamFaceDetectMode_Off
    },
    {
        ANDROID_STATISTICS_FACE_DETECT_MODE_SIMPLE,
        NvCamFaceDetectMode_Simple
    },
    {
        ANDROID_STATISTICS_FACE_DETECT_MODE_FULL,
        NvCamFaceDetectMode_Full
    }
};

static const NvAndroidCvt VSTAB_MODE_TABLE[] =
{
    {
        ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF,
        NV_FALSE
    },
    {
        ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON,
        NV_TRUE
    }
};

static const NvAndroidCvt AWB_STATE_TABLE[] =
{
    {
        ANDROID_CONTROL_AWB_STATE_INACTIVE,
        NvCamAwbState_Inactive
    },
    {
        ANDROID_CONTROL_AWB_STATE_SEARCHING,
        NvCamAwbState_Searching
    },
    {
        ANDROID_CONTROL_AWB_STATE_CONVERGED,
        NvCamAwbState_Converged
    },
    {
        ANDROID_CONTROL_AWB_STATE_LOCKED,
        NvCamAwbState_Locked
    },
    {
        //TODO: ??? NvCamAwbState_Timeout
        //Is this correct
        ANDROID_CONTROL_AWB_STATE_SEARCHING,
        NvCamAwbState_Timeout
    }
};

static const NvAndroidCvt AE_STATE_TABLE[] =
{
    {
        ANDROID_CONTROL_AE_STATE_INACTIVE,
        NvCamAeState_Inactive
    },
    {
        ANDROID_CONTROL_AE_STATE_SEARCHING,
        NvCamAeState_Searching
    },
    {
        ANDROID_CONTROL_AE_STATE_CONVERGED,
        NvCamAeState_Converged
    },
    {
        ANDROID_CONTROL_AE_STATE_LOCKED,
        NvCamAeState_Locked
    },
    {
        ANDROID_CONTROL_AE_STATE_FLASH_REQUIRED,
        NvCamAeState_FlashRequired
    },
    {
        ANDROID_CONTROL_AE_STATE_PRECAPTURE,
        NvCamAeState_Precapture
    }
};

static const NvAndroidCvt AF_STATE_TABLE[] =
{
    {
        ANDROID_CONTROL_AF_STATE_INACTIVE,
        NvCamAfState_Inactive
    },
    {
        ANDROID_CONTROL_AF_STATE_PASSIVE_SCAN,
        NvCamAfState_PassiveScan
    },
    {
        ANDROID_CONTROL_AF_STATE_PASSIVE_FOCUSED,
        NvCamAfState_PassiveFocused
    },
    {
        ANDROID_CONTROL_AF_STATE_ACTIVE_SCAN,
        NvCamAfState_ActiveScan
    },
    {
        ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED,
        NvCamAfState_FocusLocked
    },
    {
        ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED,
        NvCamAfState_NotFocusLocked
    }
};

static const NvAndroidCvt FLASH_STATE_TABLE[] =
{
    {
        ANDROID_FLASH_STATE_UNAVAILABLE,
        NvCamFlashState_Unavailable
    },
    {
        ANDROID_FLASH_STATE_CHARGING,
        NvCamFlashState_Charging
    },
    {
        ANDROID_FLASH_STATE_READY,
        NvCamFlashState_Ready
    },
    {
        ANDROID_FLASH_STATE_FIRED,
        NvCamFlashState_Fired
    }
};

static const NvAndroidCvt CAPTURE_INTENT_TABLE[] =
{
    {
        ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW,
        NvCameraCoreUseCase_Preview
    },
    {
        ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE,
        NvCameraCoreUseCase_Still
    },
    {
        ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD,
        NvCameraCoreUseCase_Video
    },
    {
        ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT,
        NvCameraCoreUseCase_VideoSnapshot
    },
    {
        ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG,
        NvCameraCoreUseCase_ZSL
    }
};

static const NvAndroidCvt METADATA_MODE_TABLE[] =
{
    {
        ANDROID_REQUEST_METADATA_MODE_NONE,
        NvCamMetadataMode_None
    },
    {
        ANDROID_REQUEST_METADATA_MODE_FULL,
        NvCamMetadataMode_Full
    }
};

static const NvAndroidCvt REQUEST_TYPE_TABLE[] =
{
    {
        ANDROID_REQUEST_TYPE_CAPTURE,
        NvCamRequestType_Capture
    },
    {
        ANDROID_REQUEST_TYPE_REPROCESS,
        NvCamRequestType_Reprocess
    }
};

static const NvAndroidCvt OPT_STAB_MODE_TABLE[] =
{
    {
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF,
        NV_FALSE
    },
    {
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_ON,
        NV_FALSE
    }
};

static const NvAndroidCvt STAT_HISTOGRAM_MODE_TABLE[] =
{
    {
        ANDROID_STATISTICS_HISTOGRAM_MODE_OFF,
        NV_FALSE
    },
    {
        ANDROID_STATISTICS_HISTOGRAM_MODE_ON,
        NV_TRUE
    }
};

static const NvAndroidCvt STAT_SHARPNESS_MAP_MODE_TABLE[] =
{
    {
        ANDROID_STATISTICS_SHARPNESS_MAP_MODE_OFF,
        NV_FALSE
    },
    {
        ANDROID_STATISTICS_SHARPNESS_MAP_MODE_ON,
        NV_TRUE
    }
};

typedef enum
{
    NvPropBool,
    NvPropEnum,
    NvPropByte,
    NvPropInt32
} NvPropType;

static NvBool convertToNV(const NvAndroidCvt* table, int size,
    uint8_t anVal, void *pNvVal, NvPropType type);

static NvBool fillAndroidTags(
    const NvCamPropertyList &prop, Vector<uint8_t>& tags,
    const char* name, const NvAndroidCvt* table, int size);

#define AE_COMP_STEP_NUMERATOR 1
#define AE_COMP_STEP_DENOMINATOR 10
#define AE_COMP_RANGE_LOW -20
#define AE_COMP_RANGE_HIGH 20

#define CONVERT_TO_NV(table, anVal, nvVal, type) \
    convertToNV(table, (sizeof(table)/sizeof(NvAndroidCvt)), anVal, nvVal, type)

#define CONVERT_TO_ANDROID(table, nvVal, anVal) \
    convertToAndroid(table, (sizeof(table)/sizeof(NvAndroidCvt)), nvVal, anVal)

#define FILL_ANDROID_TAGS(prop, tags, table) \
    fillAndroidTags(prop, tags, #table, table, (sizeof(table)/sizeof(NvAndroidCvt)))

#define DEBUG_TAG

#ifdef DEBUG_TAG
#define TAG_UPDATE(mData, name, value, size) \
    do \
    { \
        if (OK != mData.update(name, value, size)) \
        { \
            NV_LOGE("%s, failed to update TAG %s", \
                __FUNCTION__, #name); \
        } \
    } while (0)
#else
#define TAG_UPDATE(mData, name, value, size) \
    mData.update(name, value, size)
#endif

#define TAG_LIST_UPDATE(mData, modes, tag, table) \
    do \
    { \
        Vector<uint8_t> temp; \
        if (FILL_ANDROID_TAGS(modes, temp, table)) \
        { \
            TAG_UPDATE(mData, tag, temp.array(), temp.size()); \
        } \
    } while (0)

#define TAG_UPDATE_NV(mData, table, nvVal, tagname) \
    do \
    { \
        uint8_t anTag; \
        if (CONVERT_TO_ANDROID(table, nvVal, anTag)) \
        { \
            TAG_UPDATE(mData, tagname, &anTag, 1); \
        } \
    } while (0)

#define TAG_UPDATE_NV_D(mData, table, nvVal, tagname) \
    do \
    { \
        uint8_t anTag; \
        if (CONVERT_TO_ANDROID(table, nvVal, anTag)) \
        { \
            TAG_UPDATE(mData, tagname, &anTag, 1); \
        } \
        else \
        { \
            NV_LOGE("%s, failed to update TAG %s (enum: %d)", \
                __FUNCTION__, #tagname, nvVal); \
        } \
    } while (0)

static NvBool convertToNV(const NvAndroidCvt* table, int size,
    uint8_t anVal, void *pNvVal, NvPropType type)
{
    for (int i = 0; i < size; i++)
    {
        if (table[i].anVal == anVal)
        {
            switch (type)
            {
                case NvPropBool:
                    *((NvBool*)pNvVal) = (NvBool)table[i].nvVal;
                    break;
                case NvPropByte:
                    *((NvU8*)pNvVal) = (NvU8)table[i].nvVal;
                    break;
                case NvPropInt32:
                    *((NvU32*)pNvVal) = (NvU32)table[i].nvVal;
                    break;
                case NvPropEnum:
                    *((NvU32*)pNvVal) = (NvU32)table[i].nvVal;
                    break;
                default:
                    NV_LOGE("%s Invalid type", __FUNCTION__);
                    return NV_FALSE;
            }
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}

static NvBool convertToNV(const NvAndroidCvt* table, int size,
    uint8_t anVal, NvBool& nvData);

static NvBool convertToNV(const NvAndroidCvt* table, int size,
    uint8_t anVal, NvS32& nvData);

static NvBool convertToNV(const NvAndroidCvt* table, int size,
    uint8_t anVal, NvBool& nvData)
{
    return convertToNV(table, size, anVal, &nvData, NvPropBool);
}

static NvBool convertToNV(const NvAndroidCvt* table, int size,
    uint8_t anVal, NvS32& nvData)
{
    return convertToNV(table, size, anVal, &nvData, NvPropInt32);
}

static NvBool convertToAndroid(const NvAndroidCvt* table, int size,
    int32_t nvVal, uint8_t& anVal)
{
    for (int i = 0; i < size; i++)
    {
        if (table[i].nvVal == nvVal)
        {
            anVal = table[i].anVal;
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}

static NvBool fillAndroidTags(
    const NvCamPropertyList &prop,
    Vector<uint8_t>& vals, const char* name,
    const NvAndroidCvt* table, int size)
{
    uint8_t anVal;
    NvBool ret = NV_FALSE;

    vals.resize(prop.Size);
    for (NvU32 i = 0; i < prop.Size; i++)
    {
        ret = convertToAndroid(table, size, prop.Property[i], anVal);
        if (NV_TRUE == ret)
        {
            NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "%s: %s, %d %d", __FUNCTION__, name,
                prop.Property[i], anVal);
            vals.editItemAt(i) = anVal;
        }
        else
        {
            NV_LOGE("%s failed to find tag: %d in table: %s",
                __FUNCTION__, prop.Property[i], name);
            return ret;
        }
    }
    return ret;
}

static uint32_t getSize(uint8_t type)
{
    int ret = 0;
    switch (type)
    {
        case TYPE_BYTE:
            ret = sizeof(char);
            break;
        case TYPE_INT32:
            ret = sizeof(int32_t);
            break;
        case TYPE_FLOAT:
            ret = sizeof(float);
            break;
        case TYPE_INT64:
            ret = sizeof(int64_t);
            break;
        case TYPE_DOUBLE:
            ret = sizeof(double);
            break;
        case TYPE_RATIONAL:
            ret = sizeof(camera_metadata_rational_t);
            break;
        default:
            NV_LOGE("%s Invalid Tag type", __FUNCTION__);
            return NV_FALSE;
    }
    return ret;
}

static void convertNvRegionsToAndroid(const NvCamRegions& region,
    Vector<int32_t>& data)
{
    data.resize(region.numOfRegions*5);
    for (int i = 0; i < (int)region.numOfRegions; i++)
    {
        data.editItemAt(5*i) = region.regions[i].left;
        data.editItemAt((5*i)+1) = region.regions[i].top;
        data.editItemAt((5*i)+2) = region.regions[i].right;
        data.editItemAt((5*i)+3) = region.regions[i].bottom;
        //TODO:verify this???
        data.editItemAt((5*i)+4) = ceil(region.weights[i]);
    }
}

//Enable the below line to debug the control properties
//#define DEBUG_SET_CONTROLS

#define TAG_TO_NV_EN_D(table, tagname, anVal, nvVal) \
    do \
    { \
        if (!convertToNV(table, (sizeof(table)/sizeof(NvAndroidCvt)), anVal, nvVal)) \
        { \
            NV_LOGE("Failed to convert TAG %s", #tagname); \
        } \
        else \
        { \
            NV_LOGE("%s: %d %d", #tagname, anVal, nvVal); \
        } \
    } while (0)

#ifndef DEBUG_SET_CONTROLS
#define TAG_TO_NV_EN(table, tagname, anVal, nvVal) \
    do \
    { \
        if (!convertToNV(table, (sizeof(table)/sizeof(NvAndroidCvt)), anVal, nvVal)) \
        { \
            NV_LOGE("Failed to convert TAG %s", #tagname); \
        } \
    } while (0)
#else
#define TAG_TO_NV_EN TAG_TO_NV_EN_D
#endif

static void convertToNvRegions(const camera_metadata_entry_t& entry,
    NvCamRegions& camRegion)
{
    camRegion.numOfRegions = entry.count/5;
    for (uint32_t i = 0; i < camRegion.numOfRegions; i++)
    {
        camRegion.regions[i].left = entry.data.i32[(5*i) + 0];
        camRegion.regions[i].top = entry.data.i32[(5*i) + 1];
        camRegion.regions[i].right = entry.data.i32[(5*i) + 2];
        camRegion.regions[i].bottom = entry.data.i32[(5*i) + 3];
        camRegion.weights[i] = entry.data.i32[(5*i) + 4];
    }
}

#define TAG_TO_NV_REGIONS_D(tagname, entry, camRegion) \
    do { \
        convertToNvRegions(entry, camRegion); \
        for (uint32_t i = 0; i < camRegion.numOfRegions; i++) \
        { \
            NV_LOGE("%s: %d %d %d %d %d", #tagname, \
                entry.data.i32[(5*i) + 0], \
                entry.data.i32[(5*i) + 1], \
                entry.data.i32[(5*i) + 2], \
                entry.data.i32[(5*i) + 3], \
                entry.data.i32[(5*i) + 4]); \
        } \
    } while (0)

#ifndef DEBUG_SET_CONTROLS
#define TAG_TO_NV_REGIONS(tagname, entry, camRegion) \
    do { \
        convertToNvRegions(entry, camRegion); \
    } while(0)
#else
#define TAG_TO_NV_REGIONS TAG_TO_NV_REGIONS_D
#endif

template <typename Type>
static NvBool convertToNvData(const camera_metadata_entry_t& entry,
    Type* data, uint32_t max_count, uint32_t& count)
{
    if (max_count < entry.count)
    {
        NV_LOGE("%s tag: %u, out of range count (%d, max:%d)",
                __FUNCTION__, entry.tag, entry.count, max_count);
        return NV_FALSE;
    }
    count = entry.count;

    if (getSize(entry.type) != sizeof(Type))
    {
        NV_LOGE("%s tag: %u, types don't match %d",
            __FUNCTION__, entry.tag, entry.type);
        return NV_FALSE;
    }

    NvOsMemcpy(data, entry.data.u8, count*sizeof(Type));
    return NV_TRUE;
}

static void printToString(const camera_metadata_entry_t& entry,
    char* str, int count)
{
    strcpy(str, "");
    #define PRINT_DATA(fmt, V) \
        do { \
            for (uint32_t i = 0; i < entry.count; i++) \
            { \
                snprintf(str, count, fmt, str, entry.data.V[i]); \
            } \
        } while (0)

    switch (entry.type)
    {
        case TYPE_BYTE:
            PRINT_DATA("%s %d", u8);
            break;
        case TYPE_INT32:
            PRINT_DATA("%s %d", i32);
            break;
        case TYPE_FLOAT:
            PRINT_DATA("%s %f", f);
            break;
        case TYPE_INT64:
            PRINT_DATA("%s %lld", i64);
            break;
        case TYPE_DOUBLE:
            PRINT_DATA("%s %lf", d);
            break;
        case TYPE_RATIONAL:
            for (uint32_t i = 0; i < entry.count; i++)
            {
                snprintf(str, count, "%s %d/%d", str,
                    entry.data.r[i].numerator,
                    entry.data.r[i].denominator);
            }
            break;
        default:
            NV_LOGE("%s Invalid Tag type", __FUNCTION__);
    }
}

#define TAG_TO_NV_DATA_D(tagname, entry, item, max_count) \
    do { \
        uint32_t count; \
        if (!convertToNvData(entry, item, max_count, count)) \
        { \
            NV_LOGE("Failed to convert TAG %s", #tagname); \
        } \
        else \
        { \
            char str[256]; \
            printToString(entry, str, 256); \
            NV_LOGE("%s: %s", #tagname, str); \
        } \
    } while (0)

#ifndef DEBUG_SET_CONTROLS
#define TAG_TO_NV_DATA(tagname, entry, item, max_count) \
    do { \
        uint32_t count; \
        if (!convertToNvData(entry, item, max_count, count)) \
        { \
            NV_LOGE("Failed to convert TAG %s", #tagname); \
        } \
    } while (0)
#else
#define TAG_TO_NV_DATA TAG_TO_NV_DATA_D
#endif

#define TAG_TO_NV_DATA_COUNT_D(tagname, entry, item, max_count, count) \
    do { \
        if (!convertToNvData(entry, item, max_count, count)) \
        { \
            NV_LOGE("Failed to convert TAG %s", #tagname); \
        } \
        else \
        { \
            char str[256]; \
            printToString(entry, str, 256); \
            NV_LOGE("%s: %s", #tagname, str); \
        } \
    } while (0)

#ifndef DEBUG_SET_CONTROLS
#define TAG_TO_NV_DATA_COUNT(tagname, entry, item, max_count, count) \
    do { \
        if (!convertToNvData(entry, item, max_count, count)) \
        { \
            NV_LOGE("Failed to convert TAG %s", #tagname); \
        } \
    } while (0)
#else
#define TAG_TO_NV_DATA_COUNT TAG_TO_NV_DATA_COUNT_D
#endif

#define TAG_TO_ISPRANGEF32_D(tagname, entry, range) \
    do { \
        if (entry.count == 2 && entry.type == TYPE_INT32) \
        { \
            range.low = entry.data.i32[0]; \
            range.high = entry.data.i32[1]; \
            NV_LOGE("%s: %d %d", #tagname, \
                entry.data.i32[0], entry.data.i32[1]); \
        } \
        else \
        { \
            NV_LOGE("%s entry count (%d) doesn't mach 2", \
                #tagname, entry.count); \
        } \
    } while (0)

#ifndef DEBUG_SET_CONTROLS
#define TAG_TO_ISPRANGEF32(tagname, entry, range) \
    do { \
        if (entry.count == 2 && entry.type == TYPE_INT32) \
        { \
            range.low = entry.data.i32[0]; \
            range.high = entry.data.i32[1]; \
        } \
        else \
        { \
            NV_LOGE("%s entry count (%d) doesn't mach 2", \
                #tagname, entry.count); \
        } \
    } while (0)
#else
#define TAG_TO_ISPRANGEF32 TAG_TO_ISPRANGEF32_D
#endif


NvS32 translateIntentToTemplate(NvU32 intent)
{
    NvS32 type = -1;

    switch (intent)
    {
        case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
            type = CAMERA2_TEMPLATE_PREVIEW;
            break;
        case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
            type = CAMERA2_TEMPLATE_STILL_CAPTURE;
            break;
        case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
            type = CAMERA2_TEMPLATE_VIDEO_RECORD;
            break;
        case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
            type = CAMERA2_TEMPLATE_VIDEO_SNAPSHOT;
            break;
        case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
            type = CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG;
            break;
        case ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM:
            type = -1;
            break;
        default:
            type = -1;
            break;
    }

    return type;
}

int NvMetadataTranslator::captureIntent(const CameraMetadata& mData)
{
    camera_metadata_ro_entry_t entry = mData.find(ANDROID_CONTROL_CAPTURE_INTENT);
    if (entry.count != 0)
    {
        return translateIntentToTemplate(entry.data.u8[0]);
    }
    return -1;
}

NvError NvMetadataTranslator::translateToNvCamProperty(
        CameraMetadata& metaData,
        NvCameraHal3_Public_Controls &hal3_prop,
        const NvCamProperty_Public_Static& statProp,
        const NvMMCameraSensorMode& sensorMode)
{
    int64_t val_i64;

    camera_metadata_entry_t entry;
    NvCamProperty_Public_Controls &prop = hal3_prop.CoreCntrlProps;
    NvOsMemset(&hal3_prop.JpegControls, 0, sizeof(NvCameraHal3_JpegControls));

    camera_metadata_t* mData = (camera_metadata_t*)metaData.getAndLock();

    NvSize sensorActiveArraySize;
    // Active array size
    sensorActiveArraySize.width = (int)ceil(statProp.SensorActiveArraySize.right -
            statProp.SensorActiveArraySize.left + 1);
    sensorActiveArraySize.height = (int)ceil(statProp.SensorActiveArraySize.bottom -
            statProp.SensorActiveArraySize.top + 1);

    size_t count = get_camera_metadata_entry_count(mData);

    for (size_t i = 0; i < count; i++)
    {
        if (OK != get_camera_metadata_entry(mData, i, &entry))
        {
            NV_LOGE("%s Failed to find metadata at index: %d", __FUNCTION__, i);
            continue;
        }

        if (entry.count == 0)
        {
            NV_LOGE("%s TAG: %d, index: %d is empty", __FUNCTION__,
                entry.tag, i);
            continue;
        }

        switch (entry.tag)
        {
            case ANDROID_CONTROL_AE_LOCK:
                TAG_TO_NV_EN(AE_LOCK_TABLE, ANDROID_CONTROL_AE_LOCK,
                    entry.data.u8[0], (NvBool&)prop.AeLock);
                break;
            case ANDROID_CONTROL_AE_MODE:
                TAG_TO_NV_EN(AE_MODE_TABLE, ANDROID_CONTROL_AE_MODE,
                    entry.data.u8[0], (NvS32&)prop.AeMode);
                break;
            case ANDROID_CONTROL_AE_TARGET_FPS_RANGE:
                TAG_TO_ISPRANGEF32(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                    entry, prop.AeTargetFpsRange);
                //
                // If App sets fps above 30fps then its an indication that
                // High fps recording mode is selected. This is the only way
                // Hal can identify High fps mode selected and do the following.
                // If High fps recording mode is selected, then..
                // a). Set both high and low fps values equal to disable VFR.
                // b). Set property which pushes ISP to perf mode.
                //
                // Note : Similar operation can be done from App, But..
                // a). It fails CTS. Because, for App to set [120 120], we have
                //     notify the fps ranges as [120 120]. With fps ranges
                //     being [120 120], when CTS is ran, with lower limit being
                //     120.0 anything less would fail CTS, as we can reach 120
                //     mode only when High fps mode is selected, and CTS can not
                //     select High fps rec mode.
                //
                if (prop.CaptureIntent == NvCameraCoreUseCase_Video && prop.AeTargetFpsRange.high > 30.0)
                {
                    prop.AeTargetFpsRange.low = prop.AeTargetFpsRange.high;
                    prop.highFpsRecording = NV_TRUE;
                }
                else
                {
                    prop.highFpsRecording = NV_FALSE;
                }
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT:
                TAG_TO_NV_EN(CAPTURE_INTENT_TABLE,
                    ANDROID_CONTROL_CAPTURE_INTENT,
                    entry.data.u8[0], (NvS32&)prop.CaptureIntent);
                break;
            case ANDROID_SENSOR_SENSITIVITY:
                TAG_TO_NV_DATA(ANDROID_SENSOR_SENSITIVITY, entry,
                    &prop.SensorSensitivity, 1);
                break;
            case ANDROID_SENSOR_EXPOSURE_TIME:
                TAG_TO_NV_DATA(ANDROID_SENSOR_EXPOSURE_TIME, entry,
                    &val_i64, 1);
                    prop.SensorExposureTime = (NvF32)val_i64 / 1e9;
                break;
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE:
                TAG_TO_NV_EN(AE_ANTIBANDING_MODE_TABLE,
                    ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                    entry.data.u8[0], (NvS32&)prop.AeAntibandingMode);
                break;
            case ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION:
            {
                int32_t ExpoComp;
                TAG_TO_NV_DATA(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
                    entry, &ExpoComp, 1);
                prop.AeExposureCompensation = ((NvF32) ExpoComp *
                                                AE_COMP_STEP_NUMERATOR) /
                                                AE_COMP_STEP_DENOMINATOR;
            }
                break;
            case ANDROID_CONTROL_AE_REGIONS:
                TAG_TO_NV_REGIONS(ANDROID_CONTROL_AE_REGIONS, entry,
                    prop.AeRegions);
                NvMetadataTranslator::MapRegionsToValidRange(
                    prop.AeRegions, sensorActiveArraySize, sensorMode.Resolution);
                break;
            case ANDROID_CONTROL_AE_PRECAPTURE_ID:
                TAG_TO_NV_DATA(ANDROID_CONTROL_AE_PRECAPTURE_ID, entry,
                    &hal3_prop.AePrecaptureId, 1);
                break;
            case ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER:
                TAG_TO_NV_EN(AE_PRECAPTURE_TRIGGER_TABLE,
                    ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                    entry.data.u8[0], (NvS32&)prop.AePrecaptureTrigger);
                break;
            case ANDROID_CONTROL_AF_MODE:
                TAG_TO_NV_EN(AF_MODE_TABLE,
                    ANDROID_CONTROL_AF_MODE,
                    entry.data.u8[0], (NvS32&)prop.AfMode);
                break;
            case ANDROID_CONTROL_AF_REGIONS:
                TAG_TO_NV_REGIONS(ANDROID_CONTROL_AF_REGIONS, entry,
                    prop.AfRegions);
                NvMetadataTranslator::MapRegionsToValidRange(
                    prop.AfRegions, sensorActiveArraySize, sensorMode.Resolution);
                break;
            case ANDROID_CONTROL_AF_TRIGGER:
                TAG_TO_NV_EN(AF_TRIGGER_TABLE,
                    ANDROID_CONTROL_AF_TRIGGER,
                    entry.data.u8[0], (NvS32&)prop.AfTrigger);
                break;
            case ANDROID_CONTROL_AF_TRIGGER_ID:
                TAG_TO_NV_DATA(ANDROID_CONTROL_AF_TRIGGER_ID, entry,
                    &prop.AfTriggerId, 1);
                break;
            case ANDROID_CONTROL_AWB_LOCK:
                TAG_TO_NV_EN(AWB_LOCK_TABLE,
                    ANDROID_CONTROL_AWB_LOCK,
                    entry.data.u8[0], (NvBool&)prop.AwbLock);
                break;
            case ANDROID_CONTROL_AWB_MODE:
                TAG_TO_NV_EN(AWB_MODE_TABLE,
                    ANDROID_CONTROL_AWB_MODE,
                    entry.data.u8[0], (NvS32&)prop.AwbMode);
                break;
            case ANDROID_CONTROL_AWB_REGIONS:
                TAG_TO_NV_REGIONS(ANDROID_CONTROL_AWB_REGIONS, entry,
                    prop.AwbRegions);
                NvMetadataTranslator::MapRegionsToValidRange(
                    prop.AwbRegions, sensorActiveArraySize, sensorMode.Resolution);
                break;
            case ANDROID_REQUEST_FRAME_COUNT:
                TAG_TO_NV_DATA(ANDROID_REQUEST_FRAME_COUNT, entry,
                    &prop.RequestFrameCount, 1);
                break;
            case ANDROID_REQUEST_ID:
                TAG_TO_NV_DATA(ANDROID_REQUEST_ID, entry,
                    &prop.RequestId, 1);
                break;
            case ANDROID_REQUEST_INPUT_STREAMS:
                TAG_TO_NV_DATA(ANDROID_REQUEST_INPUT_STREAMS, entry,
                    &prop.RequestInputStreams, 1);
                break;
            case ANDROID_REQUEST_METADATA_MODE:
                TAG_TO_NV_EN(METADATA_MODE_TABLE,
                    ANDROID_REQUEST_METADATA_MODE,
                    entry.data.u8[0], (NvS32&)prop.RequestMetadataMode);
                break;
            case ANDROID_REQUEST_OUTPUT_STREAMS:
                TAG_TO_NV_DATA(ANDROID_REQUEST_OUTPUT_STREAMS, entry,
                    &prop.RequestOutputStreams, 1);
                break;
            case ANDROID_REQUEST_TYPE:
                TAG_TO_NV_EN(REQUEST_TYPE_TABLE,
                    ANDROID_REQUEST_TYPE,
                    entry.data.u8[0], (NvS32&)prop.RequestType);
                break;
            case ANDROID_SENSOR_FRAME_DURATION:
                {
                    float FrameRate = 0;
                    TAG_TO_NV_DATA(ANDROID_SENSOR_FRAME_DURATION, entry,
                    &val_i64, 1);
                    FrameRate = 1e9/(NvF32)val_i64;
                    if(val_i64 > 0)
                    {
                        prop.AeTargetFpsRange.low = FrameRate;
                        prop.AeTargetFpsRange.high = FrameRate;
                    }
                }
                break;
            case ANDROID_COLOR_CORRECTION_MODE:
                TAG_TO_NV_EN(COLOR_CORRECTION_MODE_TABLE,
                    ANDROID_COLOR_CORRECTION_MODE,
                    entry.data.u8[0], (NvS32&)prop.ColorCorrectionMode);
                break;
            case ANDROID_COLOR_CORRECTION_TRANSFORM:
                //TODO:
                break;
            case ANDROID_CONTROL_EFFECT_MODE:
                TAG_TO_NV_EN(EFFECT_MODE_TABLE,
                    ANDROID_CONTROL_EFFECT_MODE,
                    entry.data.u8[0], (NvS32&)prop.ColorEffectsMode);
                break;
            case ANDROID_CONTROL_MODE:
                TAG_TO_NV_EN(CONTROL_MODE_TABLE,
                    ANDROID_CONTROL_MODE,
                    entry.data.u8[0], (NvS32&)prop.ControlMode);
                break;
            case ANDROID_CONTROL_SCENE_MODE:
                TAG_TO_NV_EN(SCENE_MODE_TABLE,
                    ANDROID_CONTROL_SCENE_MODE,
                    entry.data.u8[0], (NvS32&)prop.SceneMode);
                break;
            case ANDROID_CONTROL_VIDEO_STABILIZATION_MODE:
                TAG_TO_NV_EN(VSTAB_MODE_TABLE,
                    ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                    entry.data.u8[0], (NvS32&)prop.VideoStabalizationMode);
                break;
            case ANDROID_DEMOSAIC_MODE:
                TAG_TO_NV_EN(DEMOSAIC_MODE_TABLE,
                    ANDROID_DEMOSAIC_MODE,
                    entry.data.u8[0], (NvS32&)prop.DemosaicMode);
                break;
            case ANDROID_EDGE_MODE:
                TAG_TO_NV_EN(EDGE_MODE_TABLE,
                    ANDROID_EDGE_MODE,
                    entry.data.u8[0], (NvS32&)prop.EdgeEnhanceMode);
                break;
            case ANDROID_EDGE_STRENGTH:
                {
                    uint8_t strength;
                    TAG_TO_NV_DATA(ANDROID_EDGE_STRENGTH, entry,
                        &strength, 1);
                    prop.EdgeEnhanceStrength = (NvF32)strength/10.0f;
                }
                break;
            case ANDROID_FLASH_FIRING_POWER:
                {
                    uint8_t power;
                    TAG_TO_NV_DATA(ANDROID_FLASH_FIRING_POWER, entry,
                        &power, 1);
                    prop.FlashFiringPower = (NvF32)power;
                }
                break;
            case ANDROID_FLASH_FIRING_TIME:
                TAG_TO_NV_DATA(ANDROID_FLASH_FIRING_TIME, entry,
                        &val_i64, 1);
                prop.FlashFiringTime = (NvF32)val_i64 / 1e9;
                break;
            case ANDROID_FLASH_MODE:
                TAG_TO_NV_EN(FLASH_MODE_TABLE,
                    ANDROID_FLASH_MODE,
                    entry.data.u8[0], (NvS32&)prop.FlashMode);
                break;
            case ANDROID_GEOMETRIC_MODE:
                TAG_TO_NV_EN(GEOMETRIC_MODE_TABLE,
                    ANDROID_GEOMETRIC_MODE,
                    entry.data.u8[0], (NvS32&)prop.GeometricMode);
                break;
            case ANDROID_GEOMETRIC_STRENGTH:
                {
                    uint8_t strength;
                    TAG_TO_NV_DATA(ANDROID_GEOMETRIC_STRENGTH, entry,
                        &strength, 1);
                    prop.GeometricStrength = (NvF32)strength;
                }
                break;
            case ANDROID_HOT_PIXEL_MODE:
                TAG_TO_NV_EN(HOTPIXEL_MODE_TABLE,
                    ANDROID_HOT_PIXEL_MODE,
                    entry.data.u8[0], (NvS32&)prop.HotPixelMode);
                break;
            case ANDROID_LENS_APERTURE:
                TAG_TO_NV_DATA(ANDROID_LENS_APERTURE, entry,
                        &prop.LensAperture, 1);
                break;
            case ANDROID_LENS_FILTER_DENSITY:
                TAG_TO_NV_DATA(ANDROID_LENS_FILTER_DENSITY, entry,
                        &prop.LensFilterDensity, 1);
                break;
            case ANDROID_LENS_FOCAL_LENGTH:
                TAG_TO_NV_DATA(ANDROID_LENS_FOCAL_LENGTH, entry,
                        &prop.LensFocalLength, 1);
                break;
            case ANDROID_LENS_FOCUS_DISTANCE:
                TAG_TO_NV_DATA(ANDROID_LENS_FOCUS_DISTANCE, entry,
                        &prop.LensFocusDistance, 1);
                break;
            case ANDROID_LENS_OPTICAL_STABILIZATION_MODE:
                TAG_TO_NV_EN(OPT_STAB_MODE_TABLE,
                    ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
                    entry.data.u8[0], (NvS32&)prop.LensOpticalStabalizationMode);
                break;
            case ANDROID_NOISE_REDUCTION_MODE:
                TAG_TO_NV_EN(NOISE_REDUCTION_MODE_TABLE,
                    ANDROID_NOISE_REDUCTION_MODE,
                    entry.data.u8[0], (NvS32&)prop.NoiseReductionMode);
                break;
            case ANDROID_NOISE_REDUCTION_STRENGTH:
                {
                    uint8_t strength;
                    TAG_TO_NV_DATA(ANDROID_NOISE_REDUCTION_STRENGTH, entry,
                        &strength, 1);
                    prop.NoiseReductionStrength = (NvF32)strength/10.0f;
                }
                break;
            case ANDROID_SCALER_CROP_REGION:
                {
                    // cropRegion: (x, y, width, height)
                    int32_t cropRegion[4];
                    NvSize ActiveArraySize;
                    // Active array size
                    ActiveArraySize.width =
                        (int)ceil(statProp.SensorActiveArraySize.right
                        - statProp.SensorActiveArraySize.left + 1);
                    ActiveArraySize.height =
                        (int)ceil(statProp.SensorActiveArraySize.bottom
                        - statProp.SensorActiveArraySize.top + 1);

                    TAG_TO_NV_DATA(ANDROID_SCALER_CROP_REGION, entry,
                        cropRegion, 4);

                    prop.ScalerCropRegion.left = cropRegion[0];
                    prop.ScalerCropRegion.top = cropRegion[1];
                    prop.ScalerCropRegion.right = cropRegion[0] + cropRegion[2];
                    prop.ScalerCropRegion.bottom = cropRegion[1] + cropRegion[3];

                    // clamp right or bottom if any of them exceeds ActiveArraySize.
                    if (prop.ScalerCropRegion.right > ActiveArraySize.width)
                    {
                        NV_LOGE("%s: CropRegion.right(%d) is clamped to %d",
                            __FUNCTION__, prop.ScalerCropRegion.right,
                            ActiveArraySize.width);
                        prop.ScalerCropRegion.right = ActiveArraySize.width;
                    }
                    if (prop.ScalerCropRegion.bottom > ActiveArraySize.height)
                    {
                        NV_LOGE("%s: CropRegion.bottom(%d) is clamped to %d",
                            __FUNCTION__, prop.ScalerCropRegion.bottom,
                            ActiveArraySize.height);
                        prop.ScalerCropRegion.bottom = ActiveArraySize.height;
                    }
                    // width and height shouldn't be less than
                    // activearraysize.(width or height)/maxDigitalZoom
                    NvSize size;
                    size.width = prop.ScalerCropRegion.right -
                        prop.ScalerCropRegion.left;
                    size.height = prop.ScalerCropRegion.bottom -
                        prop.ScalerCropRegion.top;
                    if (size.width <
                            floor(((NvF32)ActiveArraySize.width) / MAX_DIGITAL_ZOOM))
                    {
                        size.width = ceil(((NvF32)ActiveArraySize.width) /
                                MAX_DIGITAL_ZOOM);
                        NV_LOGE("%s: CropRegion.right(%d) is clamped to %d",
                            __FUNCTION__, prop.ScalerCropRegion.right,
                            prop.ScalerCropRegion.left + size.width);
                        prop.ScalerCropRegion.right = prop.ScalerCropRegion.left
                            + size.width;
                    }
                    if (size.height <
                            floor(((NvF32)ActiveArraySize.height) / MAX_DIGITAL_ZOOM))
                    {
                        size.height = ceil(((NvF32)ActiveArraySize.height) /
                                MAX_DIGITAL_ZOOM);
                        NV_LOGE("%s: CropRegion.bottom(%d) is clamped to %d",
                            __FUNCTION__, prop.ScalerCropRegion.bottom,
                            prop.ScalerCropRegion.bottom + size.height);
                        prop.ScalerCropRegion.bottom = prop.ScalerCropRegion.top
                            + size.height;
                    }
                }
                break;
            case ANDROID_SHADING_MODE:
                TAG_TO_NV_EN(SHADING_MODE_TABLE,
                    ANDROID_SHADING_MODE,
                    entry.data.u8[0], (NvS32&)prop.ShadingMode);
                break;
            case ANDROID_SHADING_STRENGTH:
                {
                    uint8_t strength;
                    TAG_TO_NV_DATA(ANDROID_SHADING_STRENGTH, entry,
                        &strength, 1);
                    prop.ShadingStrength = (NvF32)strength;
                }
                break;
            case ANDROID_STATISTICS_FACE_DETECT_MODE:
                TAG_TO_NV_EN(FACE_DETECT_MODE_TABLE,
                    ANDROID_STATISTICS_FACE_DETECT_MODE,
                    entry.data.u8[0], (NvS32&)prop.StatsFaceDetectMode);
                break;
            case ANDROID_STATISTICS_HISTOGRAM_MODE:
                TAG_TO_NV_EN(STAT_HISTOGRAM_MODE_TABLE,
                    ANDROID_STATISTICS_HISTOGRAM_MODE,
                    entry.data.u8[0], (NvS32&)prop.StatsHistogramMode);
                break;
            case ANDROID_STATISTICS_SHARPNESS_MAP_MODE:
                TAG_TO_NV_EN(STAT_SHARPNESS_MAP_MODE_TABLE,
                    ANDROID_STATISTICS_SHARPNESS_MAP_MODE,
                    entry.data.u8[0], (NvS32&)prop.StatsSharpnessMode);
                break;
            case ANDROID_JPEG_ORIENTATION:
                TAG_TO_NV_DATA(ANDROID_JPEG_ORIENTATION, entry,
                    (int32_t*)&hal3_prop.JpegControls.jpegOrientation, 1);
                break;
            case ANDROID_JPEG_QUALITY:
                {
                    uint8_t qual;
                    TAG_TO_NV_DATA(ANDROID_JPEG_QUALITY, entry,
                        &qual, 1);
                    hal3_prop.JpegControls.jpegQuality = qual;
                }
                break;
            case ANDROID_JPEG_THUMBNAIL_SIZE:
                TAG_TO_NV_DATA(ANDROID_JPEG_THUMBNAIL_SIZE, entry,
                    (int32_t*)&hal3_prop.JpegControls.thumbnailSize, 2);
                break;
            case ANDROID_JPEG_THUMBNAIL_QUALITY:
                {
                    uint8_t qual;
                    TAG_TO_NV_DATA(ANDROID_JPEG_THUMBNAIL_QUALITY, entry,
                        &qual, 1);
                    hal3_prop.JpegControls.thumbnailQuality = qual;
                }
                break;
            case ANDROID_JPEG_GPS_TIMESTAMP:
                TAG_TO_NV_DATA(ANDROID_JPEG_GPS_TIMESTAMP, entry,
                    (int64_t*)&hal3_prop.JpegControls.gpsTimeStamp, 1);
                break;
            case ANDROID_JPEG_GPS_COORDINATES:
                TAG_TO_NV_DATA(ANDROID_JPEG_GPS_COORDINATES, entry,
                    hal3_prop.JpegControls.gpsCoordinates, 3);
                break;
            case ANDROID_JPEG_GPS_PROCESSING_METHOD:
                {
                    uint32_t count;
                    TAG_TO_NV_DATA_COUNT(ANDROID_JPEG_GPS_COORDINATES, entry,
                        (uint8_t *)hal3_prop.JpegControls.gpsProcessingMethod,
                        32, count);
                    hal3_prop.JpegControls.gpsProcessingMethod[count] = '\0';
                }
                break;
            case ANDROID_CONTROL_AE_STATE:
            case ANDROID_CONTROL_AF_STATE:
                //Nothing to do
                break;
            case ANDROID_LED_TRANSMIT:
                //TODO:
                break;
            default:
                NV_LOGE("%s index: %d, TAG: %d not handled", __FUNCTION__, i, entry.tag);
        }
    }
    metaData.unlock(mData);
    return NvSuccess;
}

NvError NvMetadataTranslator::translateFromNvCamProperty(
        NvCameraHal3_Public_Controls &hal3_prop,
        CameraMetadata& mData)
{
    int64_t val_i64;
    NvCamProperty_Public_Controls &prop =
        hal3_prop.CoreCntrlProps;

    // maps to android.control.aeLock
    TAG_UPDATE_NV_D(mData, AE_LOCK_TABLE, prop.AeLock,
        ANDROID_CONTROL_AE_LOCK);

    // maps to android.control.aeMode
    TAG_UPDATE_NV_D(mData, AE_MODE_TABLE, prop.AeMode,
        ANDROID_CONTROL_AE_MODE);

    // android.control.aeTargetFpsRange
    {
        int32_t fps[] = {(int32_t)ceil(prop.AeTargetFpsRange.low),
                         (int32_t)ceil(prop.AeTargetFpsRange.high)};
        TAG_UPDATE(mData, ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps, 2);
    }

    // maps to android.control.captureIntent
    TAG_UPDATE_NV_D(mData, CAPTURE_INTENT_TABLE, prop.CaptureIntent,
        ANDROID_CONTROL_CAPTURE_INTENT);

    // maps to android.sensor.sensitivity
    // In ISO arithmetic units.
    TAG_UPDATE(mData, ANDROID_SENSOR_SENSITIVITY,
        (int32_t*)&prop.SensorSensitivity, 1);

    // maps to android.sensor.exposureTime
    // Value in nanoseconds.
    val_i64 = prop.SensorExposureTime * 1e9;
    TAG_UPDATE(mData, ANDROID_SENSOR_EXPOSURE_TIME,
        (int64_t*)&val_i64, 1);

    // maps to android.control.aeAntibandingMode
    TAG_UPDATE_NV_D(mData, AE_ANTIBANDING_MODE_TABLE,
        prop.AeAntibandingMode, ANDROID_CONTROL_AE_ANTIBANDING_MODE);

    // maps to android.control.aeExposureCompensation
    {
        int32_t ExpoComp;
        ExpoComp = (int32_t) ((prop.AeExposureCompensation *
                               AE_COMP_STEP_DENOMINATOR) /
                               AE_COMP_STEP_NUMERATOR);
        TAG_UPDATE(mData, ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
            &ExpoComp, 1);
    }
    // maps to android.control.aeRegions
    {
        Vector<int32_t> data;
        convertNvRegionsToAndroid(prop.AeRegions, data);
        if (prop.AeRegions.numOfRegions > 0)
        {
            TAG_UPDATE(mData, ANDROID_CONTROL_AE_REGIONS,
                data.array(), data.size());
        }
    }

    // maps to android.control.aePrecaptureTrigger
    TAG_UPDATE_NV_D(mData, AE_PRECAPTURE_TRIGGER_TABLE,
        prop.AePrecaptureTrigger,
        ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);

    // maps to android.control.afMode
    TAG_UPDATE_NV_D(mData, AF_MODE_TABLE, prop.AfMode,
        ANDROID_CONTROL_AF_MODE);

    // maps to android.control.afRegions
    {
        Vector<int32_t> data;
        convertNvRegionsToAndroid(prop.AfRegions, data);
        if (prop.AfRegions.numOfRegions > 0)
        {
            TAG_UPDATE(mData, ANDROID_CONTROL_AF_REGIONS,
                data.array(), data.size());
        }
    }

    // maps to android.control.afTrigger
    TAG_UPDATE_NV_D(mData, AF_TRIGGER_TABLE, prop.AfTrigger,
        ANDROID_CONTROL_AF_TRIGGER);

    // maps to android.control.AePrecaptureId
    TAG_UPDATE(mData, ANDROID_CONTROL_AE_PRECAPTURE_ID,
        &hal3_prop.AePrecaptureId, 1);

    TAG_UPDATE(mData, ANDROID_CONTROL_AF_TRIGGER_ID,
        &prop.AfTriggerId, 1);

    // maps to android.control.awbLock
    TAG_UPDATE_NV_D(mData, AWB_LOCK_TABLE,
        prop.AwbLock, ANDROID_CONTROL_AWB_LOCK);

    // maps to android.control.awbMode
    TAG_UPDATE_NV_D(mData, AWB_MODE_TABLE, prop.AwbMode,
        ANDROID_CONTROL_AWB_MODE);

    // maps to android.control.awbRegions
    {
        Vector<int32_t> data;
        convertNvRegionsToAndroid(prop.AwbRegions, data);
        if (prop.AwbRegions.numOfRegions > 0)
        {
            TAG_UPDATE(mData, ANDROID_CONTROL_AWB_REGIONS,
                data.array(), data.size());
        }
    }

    // android.request.frameCount
    // A frame counter set by the framework. Must be maintained
    // unchanged in output frame
    TAG_UPDATE(mData, ANDROID_REQUEST_FRAME_COUNT,
        (int32_t *)&prop.RequestFrameCount, 1);

    // maps to android.request.id
    // An application-specified ID for the current request.
    // Must be maintained unchanged in output frame
    TAG_UPDATE(mData, ANDROID_REQUEST_ID,
        (int32_t *)&prop.RequestId, 1);

    // maps to android.request.inputStreams
    // List which camera reprocess stream is used for the source
    // of reprocessing data.
    //NvU32 RequestInputStreams;

    // maps to android.request.metadataMode
    TAG_UPDATE_NV_D(mData, METADATA_MODE_TABLE,
        prop.RequestMetadataMode, ANDROID_REQUEST_METADATA_MODE);

    // maps to android.request.outputStreams
    // Lists which camera output streams image data from this capture
    // must be sent to.
    //NvU32 RequestOutputStreams;

    // maps to android.request.type
    TAG_UPDATE_NV_D(mData, REQUEST_TYPE_TABLE, prop.RequestType,
        ANDROID_REQUEST_TYPE);

    // maps to android.sensor.frameDuration
    val_i64 =  0;
    TAG_UPDATE(mData, ANDROID_SENSOR_FRAME_DURATION,
        (int64_t*)&val_i64, 1);

    // Pull these properties out as they are implemented.
    // ######################### NOT IMPLEMENTED ################################

    // maps to android.colorCorrection.mode
    TAG_UPDATE_NV_D(mData, COLOR_CORRECTION_MODE_TABLE,
        prop.ColorCorrectionMode, ANDROID_COLOR_CORRECTION_MODE);

    // maps to android.colorCorrection.transform
    //NvIspMathF32Matrix ColorCorrectionTransform;

    // maps to android.control.effectMode
    TAG_UPDATE_NV_D(mData, EFFECT_MODE_TABLE, prop.ColorEffectsMode,
        ANDROID_CONTROL_EFFECT_MODE);

    // maps to android.control.mode
    TAG_UPDATE_NV_D(mData, CONTROL_MODE_TABLE, prop.ControlMode,
        ANDROID_CONTROL_MODE);

    // maps to android.control.sceneMode
    TAG_UPDATE_NV_D(mData, SCENE_MODE_TABLE, prop.SceneMode,
        ANDROID_CONTROL_SCENE_MODE);

    // maps to android.control.videoStabilizationMode
    //NvBool VideoStabalizationMode;

    // maps to android.demosaic.mode
    TAG_UPDATE_NV_D(mData, DEMOSAIC_MODE_TABLE, prop.DemosaicMode,
        ANDROID_DEMOSAIC_MODE);

    // maps to android.edge.mode
    TAG_UPDATE_NV_D(mData, EDGE_MODE_TABLE, prop.EdgeEnhanceMode,
        ANDROID_EDGE_MODE);

    // maps to android.edge.strength. Range: [1,10]
    {
        uint8_t strength = prop.EdgeEnhanceStrength * 10;
        TAG_UPDATE(mData, ANDROID_EDGE_STRENGTH, &strength, 1);
    }

    //TODO: Is this right???
    // maps to android.flash.firingPower. Range [1,10]
    {
        uint8_t power = ceil(prop.FlashFiringPower);
        TAG_UPDATE(mData, ANDROID_FLASH_FIRING_POWER, &power, 1);
    }

    // maps to android.flash.firingTime in Nanoseconds.ANDROID_CONTROL_EFFECT_MODE
    // Firing time is relative to start of the exposure.
    val_i64 = prop.FlashFiringTime * 1e9;
    TAG_UPDATE(mData, ANDROID_FLASH_FIRING_TIME,
        (int64_t *)&val_i64, 1);

    // maps to android.flash.mode
    TAG_UPDATE_NV_D(mData, FLASH_MODE_TABLE, prop.FlashMode,
        ANDROID_FLASH_MODE);

    // maps to android.geometric.mode
    //NvCamGeometricMode  GeometricMode;
    TAG_UPDATE_NV_D(mData, GEOMETRIC_MODE_TABLE, prop.GeometricMode,
        ANDROID_GEOMETRIC_MODE);

    // maps to android.geometric.strength. Range [1,10]
    //NvF32 GeometricStrength;

    // maps to android.hotPixel.mode
    TAG_UPDATE_NV_D(mData, HOTPIXEL_MODE_TABLE, prop.HotPixelMode,
        ANDROID_HOT_PIXEL_MODE);

    // TODO: Define the JPEG controls later.
    // android.jpeg.gpsCoordinates (controls)
    // android.jpeg.gpsProcessingMethod (controls)
    // android.jpeg.gpsTimestamp (controls)
    TAG_UPDATE(mData, ANDROID_JPEG_QUALITY,
            (uint8_t *) &hal3_prop.JpegControls.jpegQuality, 1);

    TAG_UPDATE(mData, ANDROID_JPEG_ORIENTATION,
            (int32_t *) &hal3_prop.JpegControls.jpegOrientation, 1);

    TAG_UPDATE(mData, ANDROID_JPEG_THUMBNAIL_QUALITY,
            (uint8_t *) &hal3_prop.JpegControls.thumbnailQuality, 1);

    TAG_UPDATE(mData, ANDROID_JPEG_THUMBNAIL_SIZE,
            (int32_t *) &hal3_prop.JpegControls.thumbnailSize, 2);

    // maps to android.lens.aperture
    // Size of the lens aperture.
    TAG_UPDATE(mData, ANDROID_LENS_APERTURE, &prop.LensAperture, 1);

    // maps to android.lens.filterDensity
    TAG_UPDATE(mData, ANDROID_LENS_FILTER_DENSITY,
        &prop.LensFilterDensity, 1);

    // maps to android.lens.focalLength
    TAG_UPDATE(mData, ANDROID_LENS_FOCAL_LENGTH,
        &prop.LensFocalLength, 1);

    // maps to android.lens.focusDistance
    TAG_UPDATE(mData, ANDROID_LENS_FOCUS_DISTANCE,
        &prop.LensFocusDistance, 1);

    // maps to android.lens.opticalStabilizationMode
    TAG_UPDATE_NV(mData, OPT_STAB_MODE_TABLE,
        prop.LensOpticalStabalizationMode,
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE);

    // maps to android.noiseReduction.mode
    TAG_UPDATE_NV_D(mData, NOISE_REDUCTION_MODE_TABLE,
        prop.NoiseReductionMode, ANDROID_NOISE_REDUCTION_MODE);

    // maps to android.noiseReduction.strength. Range[1,10]
    {
        uint8_t strength = (uint8_t)ceil(prop.NoiseReductionStrength) * 10;
        TAG_UPDATE(mData, ANDROID_NOISE_REDUCTION_STRENGTH, &strength, 1);
    }

    // android.scaler.cropRegion
    {
        // cropRegion: (x, y, width, height)
        // Default should be (0, 0, width, height). Core sets it to
        // SensorActiveArray.left, top, right, bottom.
        int32_t cropRegion[4];
        cropRegion[0] = 0;
        cropRegion[1] = 0;
        cropRegion[2] = prop.ScalerCropRegion.right -
            prop.ScalerCropRegion.left + 1;
        cropRegion[3] = prop.ScalerCropRegion.bottom -
            prop.ScalerCropRegion.top + 1;
        TAG_UPDATE(mData, ANDROID_SCALER_CROP_REGION, cropRegion, 4);
    }
    // maps to android.shading.mode
    TAG_UPDATE_NV_D(mData, SHADING_MODE_TABLE,
        prop.ShadingMode, ANDROID_SHADING_MODE);

    // maps to android.shading.strength. Range [1,10]
    //NvF32 ShadingStrength;

    // maps to android.statistics.faceDetectMode
    TAG_UPDATE_NV_D(mData, FACE_DETECT_MODE_TABLE,
        prop.StatsFaceDetectMode,
        ANDROID_STATISTICS_FACE_DETECT_MODE);

    // maps to android.statistics.histogramMode
    TAG_UPDATE_NV_D(mData, STAT_HISTOGRAM_MODE_TABLE,
        prop.StatsHistogramMode,
        ANDROID_STATISTICS_HISTOGRAM_MODE);

    // maps to android.statistics.sharpnessMapMode
    TAG_UPDATE_NV_D(mData, STAT_SHARPNESS_MAP_MODE_TABLE,
        prop.StatsSharpnessMode,
        ANDROID_STATISTICS_SHARPNESS_MAP_MODE);

    // TODO: Define tonemap data items later. Needs some discussion.
    // android.tonemap.curveBlue
    // android.tonemap.curveGreen (controls)
    // android.tonemap.curveRed (controls)
    // android.tonemap.mode (controls)

    // maps to android.led.transmit
    // This LED is nominally used to indicate to the user that the camera
    // is powered on and may be streaming images back to the Application Processor.
    // In certain rare circumstances, the OS may disable this when video is processed
    // locally and not transmitted to any untrusted applications.In particular, the LED
    // *must* always be on when the data could be transmitted off the device. The LED
    // *should* always be on whenever data is stored locally on the device. The LED *may*
    // be off if a trusted application is using the data that doesn't violate the above rules.
    //NvBool LedTransmit;

    return NvSuccess;
}

static NvBool IsAeModeSupported(
        const NvCamProperty_Public_Static& sprop,
        NvCamAeMode mode)
{
    NvBool IsSupported = NV_FALSE;

    for (int i = 0; i < (int)sprop.AeAvailableModes.Size; i++)
    {
        if (sprop.AeAvailableModes.Property[i] == mode)
            IsSupported = NV_TRUE;
    }

    return IsSupported;
}

static NvBool IsAfModeSupported(
        const NvCamProperty_Public_Static& sprop,
        NvCamAfMode mode)
{
    NvBool IsSupported = NV_FALSE;

    for (int i = 0; i < (int)sprop.AfAvailableModes.Size; i++)
    {
        if (sprop.AfAvailableModes.Property[i] == mode)
            IsSupported = NV_TRUE;
    }

    return IsSupported;
}

static NvBool IsAwbModeSupported(
        const NvCamProperty_Public_Static& sprop,
        NvCamAwbMode mode)
{
    NvBool IsSupported = NV_FALSE;

    for (int i = 0; i < (int)sprop.AwbAvailableModes.Size; i++)
    {
        if (sprop.AwbAvailableModes.Property[i] == mode)
            IsSupported = NV_TRUE;
    }

    return IsSupported;
}

static void updateSceneModeOverrides_FacePriority(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    // set all to 0, since they will be ignored anyways
    sceneOverrides.editItemAt(idx) = 0;
    sceneOverrides.editItemAt(idx+1) = 0;
    sceneOverrides.editItemAt(idx+2) = 0;
}

static void updateSceneModeOverrides_Action(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    // set the focus position to Infinity
    sceneOverrides.editItemAt(idx+2) = NvCamAfMode_Off;
}

static void updateSceneModeOverrides_Portrait(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    //ignore
}

static void updateSceneModeOverrides_Landscape(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    // TODO: set the focus position to hyperfocal
    sceneOverrides.editItemAt(idx+2) = NvCamAfMode_Off;
}

static void updateSceneModeOverrides_Night(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_OnAlwaysFlash))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_OnAlwaysFlash;
    }

    if (IsAfModeSupported(sprop, NvCamAfMode_Auto))
    {
        sceneOverrides.editItemAt(idx+2)  = NvCamAfMode_Auto;
    }
}

static void updateSceneModeOverrides_NightPortrait(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_On_AutoFlashRedEye))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_On_AutoFlashRedEye;
    }

    if (IsAfModeSupported(sprop, NvCamAfMode_Auto))
    {
        sceneOverrides.editItemAt(idx+2)  = NvCamAfMode_Auto;
    }
}

static void updateSceneModeOverrides_Theatre(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_On))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_On;
    }

    if (IsAwbModeSupported(sprop, NvCamAwbMode_Auto))
    {
        sceneOverrides.editItemAt(idx+1)  = NvCamAwbMode_Auto;
    }

    sceneOverrides.editItemAt(idx+2) = NvCamAfMode_Off;
}

static void updateSceneModeOverrides_Beach(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAwbModeSupported(sprop, NvCamAwbMode_Daylight))
    {
        sceneOverrides.editItemAt(idx+1)  = NvCamAwbMode_Daylight;
    }
}

static void updateSceneModeOverrides_Snow(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAwbModeSupported(sprop, NvCamAwbMode_Daylight))
    {
        sceneOverrides.editItemAt(idx+1)  = NvCamAwbMode_Daylight;
    }
}

static void updateSceneModeOverrides_Sunset(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_On))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_On;
    }

    if (IsAwbModeSupported(sprop, NvCamAwbMode_Twilight))
    {
        sceneOverrides.editItemAt(idx+1) = NvCamAwbMode_Twilight;
    }
}

static void updateSceneModeOverrides_SteadyPhoto(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_On))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_On;
    }
}

static void updateSceneModeOverrides_Fireworks(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_On))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_On;
    }

    // TODO: set the focus to hyperfocal
    sceneOverrides.editItemAt(idx+2) = NvCamAfMode_Off;
}

static void updateSceneModeOverrides_Sports(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_On))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_On;
    }

    sceneOverrides.editItemAt(idx+2) = NvCamAfMode_Off;
}

static void updateSceneModeOverrides_Party(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    //ignore
}

static void updateSceneModeOverrides_CandleLight(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_On))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_On;
    }
}

static void updateSceneModeOverrides_Barcode(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    if (IsAeModeSupported(sprop, NvCamAeMode_On))
    {
        sceneOverrides.editItemAt(idx) = NvCamAeMode_On;
    }

    if (IsAfModeSupported(sprop, NvCamAfMode_Macro))
    {
        sceneOverrides.editItemAt(idx+2)  = NvCamAfMode_Macro;
    }
}

static void updateSceneModeOverrides_Default(
        const NvU32 idx,
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    NvU32 AeIndex  = idx;
    NvU32 AwbIndex = idx+1;
    NvU32 AfIndex  = idx+2;

    // default aeMode override
    if (IsAeModeSupported(sprop, NvCamAeMode_OnAutoFlash))
    {
        sceneOverrides.editItemAt(AeIndex) = NvCamAeMode_OnAutoFlash;
    }
    else
    {
        sceneOverrides.editItemAt(AeIndex) = NvCamAeMode_On;
    }

    // default awbMode override
    if (IsAwbModeSupported(sprop, NvCamAwbMode_Auto))
    {
        sceneOverrides.editItemAt(AwbIndex) = NvCamAwbMode_Auto;
    }
    else
    {
        sceneOverrides.editItemAt(AwbIndex) = NvCamAwbMode_Off;
    }

    // default afMode override
    if (IsAfModeSupported(sprop, NvCamAfMode_ContinuousPicture))
    {
        sceneOverrides.editItemAt(AfIndex)  = NvCamAfMode_ContinuousPicture;
    }
    else
    {
       sceneOverrides.editItemAt(AfIndex) = NvCamAfMode_Off;
    }
}

static NvBool updateSceneModeOverrides(
        const NvCamProperty_Public_Static& sprop,
        Vector<uint8_t>& sceneOverrides)
{
    const int kModesPerSceneMode = 3;
    sceneOverrides.resize(sprop.AvailableSceneModes.Size * kModesPerSceneMode);

    //Populate the appropriate mode overrides for each supported scene mode
    for(int i=0, j=0; i<(int)sprop.AvailableSceneModes.Size; i++, j+=kModesPerSceneMode) {

        updateSceneModeOverrides_Default(j, sprop, sceneOverrides);

        switch (sprop.AvailableSceneModes.Property[i])
        {
            case NvCamSceneMode_FacePriority:
                updateSceneModeOverrides_FacePriority(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Action:
                updateSceneModeOverrides_Action(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Portrait:
                updateSceneModeOverrides_Portrait(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Landscape:
                updateSceneModeOverrides_Landscape(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Night:
                updateSceneModeOverrides_Night(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_NightPortrait:
                updateSceneModeOverrides_NightPortrait(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Theatre:
                updateSceneModeOverrides_Theatre(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Beach:
                updateSceneModeOverrides_Beach(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Snow:
                updateSceneModeOverrides_Snow(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Sunset:
                updateSceneModeOverrides_Sunset(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_SteadyPhoto:
                updateSceneModeOverrides_SteadyPhoto(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Fireworks:
                updateSceneModeOverrides_Fireworks(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Sports:
                updateSceneModeOverrides_Sports(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Party:
                updateSceneModeOverrides_Party(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_CandleLight:
                updateSceneModeOverrides_CandleLight(j, sprop, sceneOverrides);
                break;
            case NvCamSceneMode_Barcode:
                updateSceneModeOverrides_Barcode(j, sprop, sceneOverrides);
                break;
            default:
                NV_LOGE("%s:%d: Error: Unrecognized scene mode\n", __FUNCTION__, __LINE__);
                return NV_FALSE;
        }
    }

    return NV_TRUE;
}

static NvBool filterJpegResolutions(
    const NvCamPropertySizeList& data,
    const NvCamPropertyU64List& duration,
    NvS32 maxWidth, NvS32 maxHeight,
    NvS32 maxWidth30fps, NvS32 maxHeight30fps,
    Vector<int32_t>& fsize,
    Vector<int64_t>& fduration)
{
    NvF32 targetRatio_4_3 = 1.333f;
    NvF32 targetRatio_16_9 = 1.777f;

    fsize.resize(0);
    fduration.resize(0);
    if (data.Size != duration.Size)
    {
        NV_LOGE("%s: JPEG Data size[%d], and duration size[%d] do not match!",
                __FUNCTION__, data.Size, duration.Size);
        return NV_FALSE;
    }
    else
    {
        for (int i = 0; i < (int)data.Size; i++)
        {
            NvF32 ratio =
                (NvF32)data.Dimensions[i].width / (NvF32)data.Dimensions[i].height;
            NvF32 diff_4_3 = fabs(ratio - targetRatio_4_3);
            NvF32 diff_16_9 = fabs(ratio - targetRatio_16_9);

            // Only keep the sizes that maintain a strict 4:3 or 16:9 ratio
            if ((diff_4_3  > ASPECT_TOLERANCE) &&
                (diff_16_9 > ASPECT_TOLERANCE))
            {
                // half/quarter of max res are exceptions, and
                // must be included in the list of supported sizes
                if (!(((maxWidth == (data.Dimensions[i].width)) &&
                       (maxHeight == (data.Dimensions[i].height))) ||
                      ((maxWidth == (data.Dimensions[i].width * 2)) &&
                       (maxHeight == (data.Dimensions[i].height * 2))) ||
                      ((maxWidth == (data.Dimensions[i].width * 4)) &&
                       (maxHeight == (data.Dimensions[i].height * 4)))))
                {
                    NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "%s: aspect ratio for %dx%d lies outside the tolerance range",
                        __FUNCTION__,
                        data.Dimensions[i].width, data.Dimensions[i].height);
                    continue;
                }
            }
            // filter out any 16:9 sizes that don't support 30fps
            else if (diff_16_9 <= ASPECT_TOLERANCE)
            {
                NvS32 max_area = maxWidth30fps * maxHeight30fps;
                NvS32 area = data.Dimensions[i].width * data.Dimensions[i].height;

                if(area > max_area)
                {
                    continue;
                }
            }

            if (data.Dimensions[i].width <= maxWidth &&
                data.Dimensions[i].height <= maxHeight)
            {
                fsize.push(data.Dimensions[i].width);
                fsize.push(data.Dimensions[i].height);
                fduration.push(duration.Values[i]);
            }
        }
    }

    return NV_TRUE;
}

static NvBool filterResolutions(
    const NvCamPropertySizeList& data,
    const NvCamPropertyU64List& duration,
    NvS32 width, NvS32 height, Vector<int32_t>& fsize,
    Vector<int64_t>& fduration)
{
    fsize.resize(0);
    fduration.resize(0);
    if (data.Size != duration.Size)
    {
        return NV_FALSE;
    }
    else
    {
        for (int i = 0; i < (int)data.Size; i++)
        {
            if (data.Dimensions[i].width <= width &&
                data.Dimensions[i].height <= height)
            {
                fsize.push(data.Dimensions[i].width);
                fsize.push(data.Dimensions[i].height);
                fduration.push(duration.Values[i]);
            }
        }
    }
    return NV_TRUE;
}

/*
 * compare the fps ranges based on range.high
 * if equal, then compare range.low
 */
static int compareSizes(const void *pSize1, const void *pSize2)
{
    NvSize *size1 = ( NvSize *) pSize1;
    NvSize *size2 = ( NvSize *) pSize2;

    NvS32 area1 = size1->width * size1->height;
    NvS32 area2 = size2->width * size2->height;
    return (area1 >= area2) ? 1 : -1;
}

/*
 * Get the max processed size that supports 30fps.
 */
static NvBool getMax30FPSSize(
    const Vector<int32_t>& availSizes,
    const Vector<int64_t>& availMinDurations,
    NvS32 &maxWidth, NvS32 &maxHeight)
{
    Vector<NvSize> sortedSizes;
    sortedSizes.resize(0);

    if (availSizes.size() != (availMinDurations.size() * 2))
    {
        return NV_FALSE;
    }

    for (int i=0, j=0; i<(int)availSizes.size()-1; i+=2, j++)
    {
        if (availMinDurations[j] <= FPS_30) {
            NvSize size = {availSizes[i], availSizes[i+1]};
            sortedSizes.push_back(size);
        }
    }

    qsort(sortedSizes.editArray(), sortedSizes.size(),
        sizeof(NvSize), compareSizes);

    size_t n = sortedSizes.size();
    if (n > 0)
    {
        //pick the last element.
        maxWidth = sortedSizes[n-1].width;
        maxHeight = sortedSizes[n-1].height;
        return NV_TRUE;
    }

    return NV_FALSE;
}

/*
 * compare the fps ranges based on range.high
 * if equal, then compare range.low
 */
static int compareFpsRange(const void *fpsRange1, const void *fpsRange2)
{
    NvCameraIspRangeF32 *range1 = ( NvCameraIspRangeF32 *) fpsRange1;
    NvCameraIspRangeF32 *range2 = ( NvCameraIspRangeF32 *) fpsRange2;

    return ((range1->high > range2->high) ||
            ((range1->high == range2->high) && (range1->low > range2->low))) ? 1 : -1;
}

/*
 * filter available fps ranges with the assumption that NvCameraCore
 * populates the list with unique values.
 */
static NvBool filterFpsRanges(
    const NvCamPropertyRangeList& data,
    Vector<int32_t>& fRanges)
{
    Vector<NvCameraIspRangeF32> sortedfRanges;
    fRanges.resize(0);
    sortedfRanges.resize(0);

    for (int i = 0; i < (int)data.Size; i++)
    {
        sortedfRanges.push_back(
            data.Range[i]);
    }
    qsort(sortedfRanges.editArray(), sortedfRanges.size(),
        sizeof(NvCameraIspRangeF32), compareFpsRange);

    for (int i = 0; i < (int)sortedfRanges.size(); i++)
    {
        // WAR for Bug 1435570
        // TargetFPSRange is a static metadata and it is reported before
        // selecting active sensor mode. CTS tests the ranges provided
        // without associating the frame duration with the active mode.
        // This is problematic for sensors that do not support 30 FPS at
        // the highest resolution, but support it at lower resolutions.
        // To avoid this, report a wider range if range.low == range.high.
        if (floor(sortedfRanges[i].low) != floor(sortedfRanges[i].high))
        {
            fRanges.push_back((int32_t)floor(sortedfRanges[i].low));
            fRanges.push_back((int32_t)floor(sortedfRanges[i].high));
        }
    }

    return NV_TRUE;
}

NvError NvMetadataTranslator::translateFromNvCamProperty(
        const NvCamProperty_Public_Static &prop,
        CameraMetadata& mData)
{
    NvError e = NvSuccess;

    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: ++", __FUNCTION__);

    TAG_LIST_UPDATE(mData, prop.AeAvailableAntibandingModes,
        ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
        AE_ANTIBANDING_MODE_TABLE);

    TAG_LIST_UPDATE(mData, prop.AeAvailableModes,
        ANDROID_CONTROL_AE_AVAILABLE_MODES, AE_MODE_TABLE);

    {
        Vector<int32_t> availableFpsRanges;
        if (filterFpsRanges(prop.AeAvailableTargetFpsRanges,
            availableFpsRanges))
        {
            TAG_UPDATE(mData, ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
                (int32_t *)availableFpsRanges.array(), availableFpsRanges.size());
        }
    }

    {
        int32_t wVals[2] = {AE_COMP_RANGE_LOW, AE_COMP_RANGE_HIGH};
        TAG_UPDATE(mData, ANDROID_CONTROL_AE_COMPENSATION_RANGE,
            wVals, 2);
    }

    {
        camera_metadata_rational_t step;
        step.numerator = AE_COMP_STEP_NUMERATOR;
        step.denominator = AE_COMP_STEP_DENOMINATOR;
        TAG_UPDATE(mData, ANDROID_CONTROL_AE_COMPENSATION_STEP, &step, 1);
    }

    TAG_LIST_UPDATE(mData, prop.AfAvailableModes,
        ANDROID_CONTROL_AF_AVAILABLE_MODES, AF_MODE_TABLE);

    TAG_LIST_UPDATE(mData, prop.AvailableEffects,
        ANDROID_CONTROL_AVAILABLE_EFFECTS, EFFECT_MODE_TABLE);

    TAG_LIST_UPDATE(mData, prop.AvailableSceneModes,
        ANDROID_CONTROL_AVAILABLE_SCENE_MODES, SCENE_MODE_TABLE);

    TAG_LIST_UPDATE(mData, prop.AvailableVideoStabilizationModes,
        ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
        VSTAB_MODE_TABLE);

    TAG_LIST_UPDATE(mData, prop.AwbAvailableModes,
        ANDROID_CONTROL_AWB_AVAILABLE_MODES, AWB_MODE_TABLE);

    TAG_UPDATE(mData, ANDROID_CONTROL_MAX_REGIONS, (int32_t*)&prop.MaxRegions, 1);

    {
        Vector<uint8_t> sceneOverrides;
        if (updateSceneModeOverrides(prop, sceneOverrides))
        {
            const int kModesPerSceneMode = 3;
            // convert overrides to framework compatible tags
            for (int i=0; i<=(int)sceneOverrides.size()-kModesPerSceneMode; i+=kModesPerSceneMode)
            {
                CONVERT_TO_ANDROID(
                        AE_MODE_TABLE,
                        sceneOverrides[i],
                        sceneOverrides.editItemAt(i));
                CONVERT_TO_ANDROID(
                        AWB_MODE_TABLE,
                        sceneOverrides[i+1],
                        sceneOverrides.editItemAt(i+1));
                CONVERT_TO_ANDROID(
                        AF_MODE_TABLE,
                        sceneOverrides[i+2],
                        sceneOverrides.editItemAt(i+2));
            }

            TAG_UPDATE(mData, ANDROID_CONTROL_SCENE_MODE_OVERRIDES,
                    (uint8_t*)sceneOverrides.array(), sceneOverrides.size());
        }
        else
        {
            NV_LOGE("Failed TAG ANDROID_CONTROL_SCENE_MODE_OVERRIDES");
        }
    }

    {
        uint8_t flash = (uint8_t)prop.FlashAvailable;
        TAG_UPDATE(mData, ANDROID_FLASH_INFO_AVAILABLE, &flash, 1);
    }
    TAG_UPDATE(mData, ANDROID_FLASH_INFO_CHARGE_DURATION,
        (int64_t *)&prop.FlashChargeDuration, 1);

    TAG_UPDATE(mData, ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
        (int32_t*)&kAvailableThumbnailSizes, kAvailableThumbnailSizes.Size*2);

    int32_t maxJpegSize = MAX_JPEG_SIZE;
    TAG_UPDATE(mData, ANDROID_JPEG_MAX_SIZE, (int32_t*)&maxJpegSize, 1);

    TAG_UPDATE(mData, ANDROID_LENS_INFO_AVAILABLE_APERTURES,
        (float*)&prop.LensAvailableApertures.Values,
        prop.LensAvailableApertures.Size);

    TAG_UPDATE(mData, ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES,
        (float*)&prop.LensAvailableFilterDensities.Values,
        prop.LensAvailableFilterDensities.Size);

    TAG_UPDATE(mData, ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
        (float*)&prop.LensAvailableFocalLengths.Values,
        prop.LensAvailableFocalLengths.Size);

    //TODO:
    {
        uint8_t stabModes = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
        TAG_UPDATE(mData, ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
            &stabModes, 1);
    }

    TAG_UPDATE(mData, ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE,
        (float*)&prop.LensHyperfocalDistance, 1);

    TAG_UPDATE(mData, ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE,
        (float*)&prop.LensMinimumFocusDistance, 1);
    {
        uint8_t lf = prop.LensFacing?
                        ANDROID_LENS_FACING_BACK:
                        ANDROID_LENS_FACING_FRONT;
        TAG_UPDATE(mData, ANDROID_LENS_FACING, &lf, 1);
    }

    TAG_UPDATE(mData, ANDROID_LENS_POSITION, prop.LensPosition, 3);

    TAG_UPDATE(mData, ANDROID_SCALER_AVAILABLE_FORMATS,
        (int32_t*)&kScalerAvailableFormats.Property,
        kScalerAvailableFormats.Size);

    NvF32 scalerAvailableMaxDigitalZoom = MAX_DIGITAL_ZOOM;
    TAG_UPDATE(mData, ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM,
        (float*)&scalerAvailableMaxDigitalZoom, 1);

    {
        int32_t actPixSize[] =
            {prop.SensorActiveArraySize.left, prop.SensorActiveArraySize.top,
             (int)ceil(prop.SensorActiveArraySize.right - prop.SensorActiveArraySize.left + 1),
             (int)ceil(prop.SensorActiveArraySize.bottom - prop.SensorActiveArraySize.top + 1)};

        TAG_UPDATE(mData, ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE,
            actPixSize, 4);

        Vector<int32_t> availSizes;
        Vector<int64_t> availMinDurations;

        if (filterResolutions(prop.ScalerAvailableRawSizes,
            prop.ScalerAvailableRawMinDurations, actPixSize[2],
            actPixSize[3], availSizes, availMinDurations))
        {
            TAG_UPDATE(mData, ANDROID_SCALER_AVAILABLE_RAW_SIZES,
                (int32_t*)availSizes.array(), availSizes.size());

            TAG_UPDATE(mData, ANDROID_SCALER_AVAILABLE_RAW_MIN_DURATIONS,
                (int64_t*)availMinDurations.array(), availMinDurations.size());
        }
        else
        {
            NV_LOGE("Failed TAGS ANDROID_SCALER_AVAILABLE_RAW_SIZES,"
                "ANDROID_SCALER_AVAILABLE_RAW_MIN_DURATIONS");
        }

        if (filterResolutions(prop.ScalerAvailableProcessedSizes,
            prop.ScalerAvailableProcessedMinDurations, actPixSize[2],
            actPixSize[3], availSizes, availMinDurations))
        {
            TAG_UPDATE(mData, ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES,
                (int32_t*)availSizes.array(), availSizes.size());

            TAG_UPDATE(mData, ANDROID_SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS,
                (int64_t*)availMinDurations.array(), availMinDurations.size());
        }
        else
        {
            NV_LOGE("Failed TAGS ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES,"
                "ANDROID_SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS");
        }

        // Get the max procesed size that can support 30 fps
        NvS32 maxWidth30fps;
        NvS32 maxHeight30fps;
        if (getMax30FPSSize(availSizes, availMinDurations,
            maxWidth30fps, maxHeight30fps) == NV_FALSE)
        {
            // Assign the largest size if the query fails
            maxWidth30fps = actPixSize[2];
            maxHeight30fps = actPixSize[3];
        }

        if (filterJpegResolutions(kAvailableJpegSizes,
            kAvailableJpegMinDurations, actPixSize[2],
            actPixSize[3], maxWidth30fps, maxHeight30fps,
            availSizes, availMinDurations))
        {
            TAG_UPDATE(mData, ANDROID_SCALER_AVAILABLE_JPEG_SIZES,
                (int32_t*)availSizes.array(), availSizes.size());

            TAG_UPDATE(mData, ANDROID_SCALER_AVAILABLE_JPEG_MIN_DURATIONS,
                (int64_t*)availMinDurations.array(), availMinDurations.size());
        }
        else
        {
            NV_LOGE("Failed TAGS ANDROID_SCALER_AVAILABLE_JPEG_SIZES,"
                "ANDROID_SCALER_AVAILABLE_JPEG_MIN_DURATIONS");
        }
    }

// This flag enables High fps recording feature only in non-partner builds.
#if (NV_SUPPORT_HIGH_FPS_RECORDING == 1)
    {
        // Filter out the high fps (above 30fps) from the list and update.
        Vector<int32_t> highFpsModesData;
        int32_t index = 0;
        while (index < MAX_NUM_SENSOR_MODES)
        {
            if (prop.SensorModeList[index].FrameRate > 30.0)
            {
                highFpsModesData.push(prop.SensorModeList[index].Resolution.width);
                highFpsModesData.push(prop.SensorModeList[index].Resolution.height);
                highFpsModesData.push((int32_t)prop.SensorModeList[index].FrameRate);
                NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "Supported High fps recording modes : %dx%d@%d\n", \
                    prop.SensorModeList[index].Resolution.width,
                    prop.SensorModeList[index].Resolution.height,
                    (int32_t)prop.SensorModeList[index].FrameRate);
            }
            index++;
        }
        if (index)
        {
            TAG_UPDATE(mData, ANDROID_CONTROL_VIDEO_HIGH_FPS_RECORDING_MODE,
                highFpsModesData.array(), highFpsModesData.size());
        }
    }
#endif

#ifdef PLATFORM_IS_KITKAT
    TAG_UPDATE(mData, ANDROID_SENSOR_INFO_SENSITIVITY_RANGE,
#else
    TAG_UPDATE(mData, ANDROID_SENSOR_INFO_AVAILABLE_SENSITIVITIES,
#endif
        (int32_t*)prop.SensorAvailableSensitivities.Property,
        prop.SensorAvailableSensitivities.Size);

    //TODO:
    {
        uint8_t colorFilter = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
        TAG_UPDATE(mData, ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT,
            &colorFilter, 1);
            //(int32_t*)prop.SensorColorFilterArrangement.Property,
            //prop.SensorColorFilterArrangement.Size);
    }
    TAG_UPDATE(mData, ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE,
        (int64_t*)prop.SensorExposureTimeRange, 2);

    TAG_UPDATE(mData, ANDROID_SENSOR_INFO_MAX_FRAME_DURATION,
        (int64_t*)&prop.SensorMaxFrameDuration, 1);

    TAG_UPDATE(mData, ANDROID_SENSOR_INFO_PHYSICAL_SIZE,
        (float*)&prop.SensorPhysicalSize, 2);

    TAG_UPDATE(mData, ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE,
        (int32_t*)&prop.SensorPixelArraySize, 2);

    TAG_UPDATE(mData, ANDROID_SENSOR_INFO_WHITE_LEVEL,
        (int32_t*)&prop.SensorWhiteLevel, 1);

    TAG_UPDATE(mData, ANDROID_SENSOR_BLACK_LEVEL_PATTERN,
        (int32_t*)&prop.BlackLevelPattern, 4);

    TAG_UPDATE(mData, ANDROID_SENSOR_ORIENTATION,
        (int32_t*)&prop.SensorOrientation, 1);

    TAG_LIST_UPDATE(mData, prop.StatsAvailableFaceDetectModes,
        ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
        FACE_DETECT_MODE_TABLE);

    TAG_UPDATE(mData, ANDROID_STATISTICS_INFO_HISTOGRAM_BUCKET_COUNT,
        (int32_t*)&prop.StatsHistogramBucketCount, 1);

    TAG_UPDATE(mData, ANDROID_STATISTICS_INFO_MAX_FACE_COUNT,
        (int32_t*)&prop.StatsMaxFaceCount, 1);

    TAG_UPDATE(mData, ANDROID_STATISTICS_INFO_MAX_HISTOGRAM_COUNT,
        (int32_t*)&prop.StatsMaxHistogramCount, 1);

    TAG_UPDATE(mData, ANDROID_STATISTICS_INFO_MAX_SHARPNESS_MAP_VALUE,
        (int32_t*)&prop.StatsMaxSharpnessMapValue, 1);

    TAG_UPDATE(mData, ANDROID_STATISTICS_INFO_SHARPNESS_MAP_SIZE,
        (int32_t*)&prop.StatsSharpnessMapSize, 2);

    TAG_UPDATE(mData, ANDROID_TONEMAP_MAX_CURVE_POINTS,
        (int32_t*)&prop.TonemapMaxCurvePoints, 1);

    {
        uint8_t hwLevel = (prop.SupportedHardwareLevel == NvCamSupportedHardwareLevel_Full)?
            ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL:ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED;
        TAG_UPDATE(mData, ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL, &hwLevel, 1);
    }

    TAG_UPDATE(mData, ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY,
        (int32_t*)&prop.SensorMaxAnalogSensitivity, 1);

    //TODO:
    //ANDROID_LENS_OPTICAL_AXIS_ANGLE
    //ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS
    //ANDROID_REQUEST_MAX_NUM_REPROCESS_STREAMS
    //ANDROID_SENSOR_BASE_GAIN_FACTOR
    //ANDROID_SENSOR_CALIBRATION_TRANSFORM1
    //ANDROID_SENSOR_CALIBRATION_TRANSFORM2
    //ANDROID_SENSOR_COLOR_TRANSFORM1
    //ANDROID_SENSOR_COLOR_TRANSFORM2
    //ANDROID_SENSOR_FORWARD_MATRIX1
    //ANDROID_SENSOR_FORWARD_MATRIX2
    //ANDROID_SENSOR_NOISE_MODEL_COEFFICIENTS
    //ANDROID_SENSOR_REFERENCE_ILLUMINANT1
    //ANDROID_SENSOR_REFERENCE_ILLUMINANT2
    //ANDROID_LED_AVAILABLE_LEDS
    uint8_t usePartial = 0;
    if (prop.NoOfResultsPerFrame > 1)
    {
        usePartial = 1;
    }
    TAG_UPDATE(mData, ANDROID_QUIRKS_USE_PARTIAL_RESULT,
           (uint8_t *)&usePartial, 1);

    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: --", __FUNCTION__);
    return e;

fail:
    NV_LOGE("%s: --: Failed (error: 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvMetadataTranslator::translateFromNvCamProperty(
            const NvCameraHal3_Public_Dynamic &hal3Prop,
            const NvCameraHal3_Public_Controls &propDef,
            CameraMetadata& mData,
            const NvCamProperty_Public_Static& statProp)
{
    int64_t val_i64;
    uint8_t anTag;
    NvError e = NvSuccess;
    const NvCamProperty_Public_Dynamic& prop = hal3Prop.CoreDynProps;
    NvCamRegions regions;
    NvMMCameraSensorMode sensorMode = hal3Prop.sensorMode;
    NvSize sensorActiveArraySize;
    // Active array size
    sensorActiveArraySize.width = (int)ceil(statProp.SensorActiveArraySize.right -
            statProp.SensorActiveArraySize.left + 1);
    sensorActiveArraySize.height = (int)ceil(statProp.SensorActiveArraySize.bottom -
            statProp.SensorActiveArraySize.top + 1);

    // The ID sent with the latest Camera2 trigger
    // precapture metering call
    if (prop.Ae.PartialResultUpdated || hal3Prop.send3AResults)
    {
        TAG_UPDATE(mData, ANDROID_CONTROL_AE_PRECAPTURE_ID,
                &hal3Prop.AePrecaptureId, 1);

        // maps to android.control.aeRegions
        {
            Vector<int32_t> data;
            regions = prop.Ae.AeRegions;
            NvMetadataTranslator::MapRegionsToValidRange(regions,
                    sensorMode.Resolution, sensorActiveArraySize);
            convertNvRegionsToAndroid(regions, data);
            if (prop.Ae.AeRegions.numOfRegions > 0)
            {
                TAG_UPDATE(mData, ANDROID_CONTROL_AE_REGIONS,
                        data.array(), data.size());
            }
        }

        //android.control.aeState
        TAG_UPDATE_NV(mData, AE_STATE_TABLE, prop.Ae.AeState,
                ANDROID_CONTROL_AE_STATE);

        // maps to android.sensor.exposureTime
        // Value in nanoseconds.
        val_i64 = prop.Ae.SensorExposureTime * 1e9;
        TAG_UPDATE(mData, ANDROID_SENSOR_EXPOSURE_TIME,
                (int64_t*)&val_i64, 1);

        // maps to android.sensor.sensitivity
        // In ISO arithmetic units.
        TAG_UPDATE(mData, ANDROID_SENSOR_SENSITIVITY,
                (int32_t*)&prop.Ae.SensorSensitivity, 1);
    }

    if (prop.Af.PartialResultUpdated || hal3Prop.send3AResults)
    {
        TAG_UPDATE(mData, ANDROID_CONTROL_AF_TRIGGER_ID,
                &prop.Af.AfTriggerId, 1);

        // maps to android.control.afMode
        TAG_UPDATE_NV(mData, AF_MODE_TABLE, prop.Af.AfMode,
                ANDROID_CONTROL_AF_MODE);

        // maps to android.control.afRegions
        {
            Vector<int32_t> data;
            regions = prop.Af.AfRegions;
            NvMetadataTranslator::MapRegionsToValidRange(regions,
                    sensorMode.Resolution, sensorActiveArraySize);
            convertNvRegionsToAndroid(regions, data);
            if (prop.Af.AfRegions.numOfRegions > 0)
            {
                TAG_UPDATE(mData, ANDROID_CONTROL_AF_REGIONS,
                        data.array(), data.size());
            }
        }

        //android.control.afstate
        TAG_UPDATE_NV(mData, AF_STATE_TABLE, prop.Af.AfState,
                ANDROID_CONTROL_AF_STATE);
    }

    if (prop.Awb.PartialResultUpdated || hal3Prop.send3AResults)
    {
        // maps to android.control.awbMode
        TAG_UPDATE_NV(mData, AWB_MODE_TABLE, prop.Awb.AwbMode,
                ANDROID_CONTROL_AWB_MODE);

        //android.control.awbState
        TAG_UPDATE_NV(mData, AWB_STATE_TABLE, prop.Awb.AwbState,
                ANDROID_CONTROL_AWB_STATE);

        // maps to android.control.awbRegions
        {
            Vector<int32_t> data;
            regions = prop.Awb.AwbRegions;
            NvMetadataTranslator::MapRegionsToValidRange(regions,
                    sensorMode.Resolution, sensorActiveArraySize);
            convertNvRegionsToAndroid(regions, data);
            if (prop.Awb.AwbRegions.numOfRegions > 0)
            {
                TAG_UPDATE(mData, ANDROID_CONTROL_AWB_REGIONS,
                        data.array(), data.size());
            }
        }
#if (NV_HALV3_METADATA_ENHANCEMENT_WAR == 1)
        TAG_UPDATE(mData, NVIDIA_AWB_CCT, (int32_t *)&prop.Awb.AwbCCT, 1);
#endif
    }

    // maps to android.control.mode
    TAG_UPDATE_NV(mData, CONTROL_MODE_TABLE, prop.ControlMode,
        ANDROID_CONTROL_MODE);

    // maps to android.edge.mode
    TAG_UPDATE_NV(mData, EDGE_MODE_TABLE, prop.EdgeEnhanceMode,
        ANDROID_EDGE_MODE);

    //TODO: Is this right?
    // maps to android.flash.firingPower. Range [1,10]
    {
        uint8_t power = ceil(prop.FlashFiringPower);
        TAG_UPDATE(mData, ANDROID_FLASH_FIRING_POWER, &power, 1);
    }

    // maps to android.flash.firingTime in Nanoseconds.
    val_i64 = prop.FlashFiringTime * 1e9;
    TAG_UPDATE(mData, ANDROID_FLASH_FIRING_TIME,
        (int64_t *)&val_i64, 1);

    // maps to android.flash.mode
    TAG_UPDATE_NV(mData, FLASH_MODE_TABLE, prop.FlashMode,
        ANDROID_FLASH_MODE);

    // maps to android.flash.state
    TAG_UPDATE_NV(mData, FLASH_STATE_TABLE, prop.FlashState,
        ANDROID_FLASH_STATE);

    // maps to android.hotPixel.mode
    TAG_UPDATE_NV(mData, HOTPIXEL_MODE_TABLE, prop.HotPixelMode,
        ANDROID_HOT_PIXEL_MODE);

    // TODO: Define the JPEG controls later.
    // android.jpeg.gpsCoordinates (controls)
    // android.jpeg.gpsProcessingMethod (controls)
    // android.jpeg.gpsTimestamp (controls)
    // android.jpeg.orientation (controls)
    // android.jpeg.quality (controls)
    // android.jpeg.thumbnailQuality (controls)
    // android.jpeg.thumbnailSize (controls)

    // maps to android.lens.aperture
    // Size of the lens aperture.
    TAG_UPDATE(mData, ANDROID_LENS_APERTURE, &prop.LensAperture, 1);

    // maps to android.lens.filterDensity
    TAG_UPDATE(mData, ANDROID_LENS_FILTER_DENSITY,
        &prop.LensFilterDensity, 1);

    // maps to android.lens.focalLength
    TAG_UPDATE(mData, ANDROID_LENS_FOCAL_LENGTH,
        &prop.LensFocalLength, 1);

    // maps to android.lens.focusDistance
    TAG_UPDATE(mData, ANDROID_LENS_FOCUS_DISTANCE,
        &prop.LensFocusDistance, 1);

    // maps to android.lens.opticalStabilizationMode
    TAG_UPDATE_NV(mData, OPT_STAB_MODE_TABLE,
        prop.LensOpticalStabalizationMode,
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE);

    // maps to android.lens.focusrange
    {
        float focusRange[] = {prop.LensFocusRange.high, prop.LensFocusRange.low};
        TAG_UPDATE(mData, ANDROID_LENS_FOCUS_RANGE, focusRange, 2);
    }

    //TODO: Check this ????
    // maps to android.lens.state
    {
        uint8_t state = ANDROID_LENS_STATE_STATIONARY;
        TAG_UPDATE(mData, ANDROID_LENS_STATE, &state, 1);
    }

    // maps to android.noiseReduction.mode
    TAG_UPDATE_NV(mData, NOISE_REDUCTION_MODE_TABLE,
        prop.NoiseReductionMode, ANDROID_NOISE_REDUCTION_MODE);

    // maps to android.request.metadataMode
    TAG_UPDATE_NV(mData, METADATA_MODE_TABLE,
        prop.RequestMetadataMode, ANDROID_REQUEST_METADATA_MODE);

    // maps to android.request.outputStreamsnager(
    // Lists which camera output streams image data from this capture
    // must be sent to.
    //NvU32 RequestOutputStreams;

    // android.scaler.cropRegion
    {
        // cropRegion: (x, y, width, height)
        int32_t cropRegion[4];
        cropRegion[0] = prop.ScalerCropRegion.left;
        cropRegion[1] = prop.ScalerCropRegion.top;
        cropRegion[2] = prop.ScalerCropRegion.right - prop.ScalerCropRegion.left;
        cropRegion[3] = prop.ScalerCropRegion.bottom - prop.ScalerCropRegion.top;
        TAG_UPDATE(mData, ANDROID_SCALER_CROP_REGION, cropRegion, 4);
    }

    // maps to android.sensor.frameDuration,
    // if frameDuration is non zero and set by app,
    // AeTargetFpsRange will have same values for
    // low and high, so populate it in frameDuration.
    {
        if(propDef.CoreCntrlProps.AeTargetFpsRange.high ==
            propDef.CoreCntrlProps.AeTargetFpsRange.low)
        {
            val_i64 =  1e9 / (propDef.CoreCntrlProps.AeTargetFpsRange.low);
        }
        else
        {
            val_i64 = 0;
        }

        TAG_UPDATE(mData, ANDROID_SENSOR_FRAME_DURATION,
            (int64_t*)&val_i64, 1);
    }
    // maps to android.sensor.timestamp
    TAG_UPDATE(mData, ANDROID_SENSOR_TIMESTAMP,
        (int64_t*)&prop.SensorTimestamp, 1);

    // maps to android.shading.mode
    TAG_UPDATE_NV(mData, SHADING_MODE_TABLE,
        prop.ShadingMode, ANDROID_SHADING_MODE);

    // maps to android.statistics.faceDetectMode
    TAG_UPDATE_NV(mData, FACE_DETECT_MODE_TABLE,
        prop.StatsFaceDetectMode,
        ANDROID_STATISTICS_FACE_DETECT_MODE);

    // maps to android.statistics.histogramMode
    TAG_UPDATE_NV(mData, STAT_HISTOGRAM_MODE_TABLE,
        prop.StatsHistogramMode,
        ANDROID_STATISTICS_HISTOGRAM_MODE);

    // maps to android.statistics.sharpnessMapMode
    TAG_UPDATE_NV(mData, STAT_SHARPNESS_MAP_MODE_TABLE,
        prop.StatsSharpnessMode,
        ANDROID_STATISTICS_SHARPNESS_MAP_MODE);

    // maps to android.statistics.faceIds
    TAG_UPDATE(mData, ANDROID_STATISTICS_FACE_IDS,
        (int32_t *)prop.FaceIds.Property, prop.FaceIds.Size);

    // maps to android.statistics.faceLandmarks
    TAG_UPDATE(mData, ANDROID_STATISTICS_FACE_LANDMARKS,
        (int32_t *)prop.FaceLandmarks.Landmarks,
        6*prop.FaceLandmarks.Size);

    // maps to android.statistics.faceRectangles
    if(prop.FaceRectangles.Size > 0)
    {
        // translate to active array size co-ordinate system for frameworks
        NvCamPropertyRectList faceRectangles = prop.FaceRectangles;
        for (uint32_t i = 0; i < faceRectangles.Size ; i++)
        {
            MapRectToValidRange(faceRectangles.Rects[i], sensorMode.Resolution,
                    sensorActiveArraySize);
        }
        TAG_UPDATE(mData, ANDROID_STATISTICS_FACE_RECTANGLES,
                (int32_t *)faceRectangles.Rects, 4*faceRectangles.Size);
    }

    // maps to android.statistics.faceScores
    if (prop.FaceScores.Size > 0)
    {
        Vector<uint8_t> data;
        data.resize(prop.FaceScores.Size);
        for (NvU32 i = 0; i < prop.FaceScores.Size; i++)
        {
            data.editItemAt(i) = prop.FaceScores.Property[i];
        }
        TAG_UPDATE(mData, ANDROID_STATISTICS_FACE_SCORES,
            (uint8_t *)data.array(), data.size());
    }

    // TODO: Define these data items later. Need to evaluate
    // implementation.
    // android.statistics.histogram

    // android.statistics.sharpnessMap
    if (prop.StatsSharpnessMode)
    {
        TAG_UPDATE(mData, ANDROID_STATISTICS_SHARPNESS_MAP,
            (int32_t *)prop.StatsSharpnessMap, MAX_FM_WINDOWS * 3);
    }

    // TODO: Define tonemap data items later. Needs some discussion.
    // android.tonemap.curveBlue
    // android.tonemap.curveGreen (controls)
    // android.tonemap.curveRed (controls)
    // android.tonemap.mode (controls)

    // Extensions to Google metadata until vendor tags support is available
#if (NV_HALV3_METADATA_ENHANCEMENT_WAR == 1)

    TAG_UPDATE(mData, NVIDIA_DNG_WHITEBALANCE,
        (float*)&prop.RawCaptureCCT, 1);

    TAG_UPDATE(mData, NVIDIA_DNG_WBNEUTRAL,
        (float*)&prop.RawCaptureWB, 4);

    TAG_UPDATE(mData, NVIDIA_DNG_GAIN,
        (float*)&prop.RawCaptureGain, 1);

    TAG_UPDATE(mData, NVIDIA_DNG_RAWTORGB3000K,
        (float*)&prop.RawCaptureCCM3000, 16);

    TAG_UPDATE(mData, NVIDIA_DNG_RAWTORGB6500K,
        (float*)&prop.RawCaptureCCM6500, 16);
#endif

    // maps to android.led.transmit
    // This LED is nominally used to indicate to the user that the camera
    // is powered on and may be streaming images back to the Application Processor.
    // In certain rare circumstances, the OS may disable this when video is processed
    // locally and not transmitted to any untrusted applications.In particular, the LED
    // *must* always be on when the data could be transmitted off the device. The LED
    // *should* always be on whenever data is stored locally on the device. The LED *may*
    // be off if a trusted application is using the data that doesn't violate the above rules.
    //NvBool LedTransmit;

    // Final metadata, set the partial quirk to FINAL
    uint8_t partial = ANDROID_QUIRKS_PARTIAL_RESULT_FINAL;
    TAG_UPDATE(mData, ANDROID_QUIRKS_PARTIAL_RESULT,
        (uint8_t *)&partial, 1);

    return e;

fail:
    NV_LOGE("%s: Failed (error: 0x%x)", __FUNCTION__, e);
    return NvSuccess;
}

NvError NvMetadataTranslator::MapRegionsToValidRange(
        NvCamRegions& nvCamRegions,
        const NvSize& fromResolution,
        const NvSize& toResolution)
{
    NvU32 i = 0;

    for (i = 0; i < nvCamRegions.numOfRegions; i++)
    {
        MapRectToValidRange(nvCamRegions.regions[i], fromResolution, toResolution);
    }
    return NvSuccess;
}


void NvMetadataTranslator::MapRectToValidRange(NvRect& rect,
        const NvSize& fromResolution,
        const NvSize& toResolution)
{
    // translate as per dest sensor resolution -- toResolution
    rect.top =
        (int)(floor((double(rect.top)/
                        double(fromResolution.height)) * toResolution.height));
    rect.bottom =
        (int)(floor((double(rect.bottom)/
                        double(fromResolution.height)) * toResolution.height));
    rect.left =
        (int)(floor((double(rect.left)/
                        double(fromResolution.width)) * toResolution.width));
    rect.right =
        (int)(floor((double(rect.right)/
                        double(fromResolution.width)) * toResolution.width));

}


NvError NvMetadataTranslator::translatePartialFromNvCamProperty(
        const NvCameraHal3_Public_Dynamic& hal3Prop,
        CameraMetadata& mData, const NvMMCameraSensorMode& sensorMode,
        const NvCamProperty_Public_Static& statProp)
{
    const NvCamProperty_Public_Dynamic& prop = hal3Prop.CoreDynProps;
    int64_t val_i64 = 0;
    NvCamRegions regions;
    NvSize sensorActiveArraySize;
    // Active array size
    sensorActiveArraySize.width = (int)ceil(statProp.SensorActiveArraySize.right -
            statProp.SensorActiveArraySize.left + 1);
    sensorActiveArraySize.height = (int)ceil(statProp.SensorActiveArraySize.bottom -
            statProp.SensorActiveArraySize.top + 1);

    // check if Ae changed
    if (prop.Ae.PartialResultUpdated)
    {
        TAG_UPDATE(mData, ANDROID_CONTROL_AE_PRECAPTURE_ID,
            &hal3Prop.AePrecaptureId, 1);
        // maps to android.control.aeRegions
        {
            Vector<int32_t> data;
            regions = prop.Ae.AeRegions;
            NvMetadataTranslator::MapRegionsToValidRange(regions,
                    sensorMode.Resolution, sensorActiveArraySize);
            convertNvRegionsToAndroid(regions, data);
            if (prop.Ae.AeRegions.numOfRegions > 0)
            {
                TAG_UPDATE(mData, ANDROID_CONTROL_AE_REGIONS,
                        data.array(), data.size());
            }
        }
        //android.control.aeState
        TAG_UPDATE_NV(mData, AE_STATE_TABLE, prop.Ae.AeState,
                ANDROID_CONTROL_AE_STATE);
        // maps to android.sensor.exposureTime
        // Value in nanoseconds.
        val_i64 = prop.Ae.SensorExposureTime * 1e9;
        TAG_UPDATE(mData, ANDROID_SENSOR_EXPOSURE_TIME,
                (int64_t*)&val_i64, 1);
        // maps to android.sensor.sensitivity
        // In ISO arithmetic units.
        TAG_UPDATE(mData, ANDROID_SENSOR_SENSITIVITY,
            (int32_t*)&prop.Ae.SensorSensitivity, 1);
   }
    // check if Awb changed
    if (prop.Awb.PartialResultUpdated)
    {
        // maps to android.control.awbMode
        TAG_UPDATE_NV(mData, AWB_MODE_TABLE, prop.Awb.AwbMode,
                ANDROID_CONTROL_AWB_MODE);

        //android.control.awbState
        TAG_UPDATE_NV(mData, AWB_STATE_TABLE, prop.Awb.AwbState,
                ANDROID_CONTROL_AWB_STATE);

        // maps to android.control.awbRegions
        {
            Vector<int32_t> data;
            regions = prop.Awb.AwbRegions;
            NvMetadataTranslator::MapRegionsToValidRange(regions,
                    sensorMode.Resolution, sensorActiveArraySize);
            convertNvRegionsToAndroid(regions, data);
            if (prop.Awb.AwbRegions.numOfRegions > 0)
            {
                TAG_UPDATE(mData, ANDROID_CONTROL_AWB_REGIONS,
                        data.array(), data.size());
            }
        }
#if (NV_HALV3_METADATA_ENHANCEMENT_WAR == 1)
        TAG_UPDATE(mData, NVIDIA_AWB_CCT, (int32_t *)&prop.Awb.AwbCCT, 1);
#endif
    }
    // check if Af changed
    if (prop.Af.PartialResultUpdated)
    {
        TAG_UPDATE(mData, ANDROID_CONTROL_AF_TRIGGER_ID,
                &prop.Af.AfTriggerId, 1);

        // maps to android.control.afMode
        TAG_UPDATE_NV(mData, AF_MODE_TABLE, prop.Af.AfMode,
                ANDROID_CONTROL_AF_MODE);

        // maps to android.control.afRegions
        {
            Vector<int32_t> data;
            regions = prop.Af.AfRegions;
            NvMetadataTranslator::MapRegionsToValidRange(regions,
                    sensorMode.Resolution, sensorActiveArraySize);
            convertNvRegionsToAndroid(regions, data);
            if (prop.Af.AfRegions.numOfRegions > 0)
            {
                TAG_UPDATE(mData, ANDROID_CONTROL_AF_REGIONS,
                        data.array(), data.size());
            }
        }

        //android.control.afstate
        TAG_UPDATE_NV(mData, AF_STATE_TABLE, prop.Af.AfState,
                ANDROID_CONTROL_AF_STATE);
    }

    uint8_t partial = ANDROID_QUIRKS_PARTIAL_RESULT_PARTIAL;
    TAG_UPDATE(mData, ANDROID_QUIRKS_PARTIAL_RESULT,
        (uint8_t *)&partial, 1);

    return NvSuccess;
}

} //namespace android
