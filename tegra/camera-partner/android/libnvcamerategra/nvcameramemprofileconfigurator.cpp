/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcameramemprofileconfigurator.h"
#include <sys/sysconf.h>
#include <cutils/properties.h>

/*
 * There are two schemes/parts which define the behavior of NvCameraMemProfileConfigurator.
 * 1. Footprint scheme (indicated by mFootprintScheme)
 *      This scheme defines the static part of memory configurator which specifies the max memory
 *      footprint when camera is fully loaded (after first preview frame is received).
 *      This macro should be set to NVCAMERA_BUFFER_FOOTPRINT_LEAN for platforms with limited
 *      memory such as Pluto 1GB Ram + 1080p Panel and NVCAMERA_BUFFER_FOOTPRINT_PERF for platforms
 *      with sufficient memory such as Dalmore 2GB Ram to get better performance. In cases where
 *      memory is only slightly limited, NVCAMERA_BUFFER_FOOTPRINT_MIXED can be used.
 *
 * 2. Performance scheme (indicated by mPerfScheme)
 *      This scheme defines the dynamic part of memory configurator which specifies how the memory
 *      is allocated across different stages of camera. Right now, we defined three stages:
 *          camera launch - when camera HAL constructor is called.
 *          after 1st preview - when first preview frame is received.
 *          camera_exit - when camera HAL destructor is called.
 *      With the increase of perf level, the camera hal driver will leave bigger memory footprint
 *      after exit, but will be faster to launch/switch.
 *
 * Currently, both of these schemes are configured at runtime (in NvCameraMemProfileConfigurator
 *  constructor). We simply choose the scheme based on the memory size. The typical memory sizes
 *  are 1GB and 2GB at this moment. And follows are the equations:
 *  Memory Size < Threshold (1.5GB)
 *      mFootprintScheme = NVCAMERA_BUFFER_FOOTPRINT_LEAN
 *      mPerfScheme = NVCAMERA_BUFFER_PERF_MIXED
 *  Memory Size >= Threshold (1.5GB)
 *      mFootprintScheme = NVCAMERA_BUFFER_FOOTPRINT_PERF
 *      mPerfScheme = NVCAMERA_BUFFER_PERF_PERF
 *
 * Follows are some example numbers for different configurations. B represents the set of numbers
 *  for different buffers which correlates with the max memory footprint. For the buffer definition,
 *  one can refer to NvCameraBufferConfigPortID. B is defined by mFootprintScheme.
 *
 *      level                    camera_start        after 1st preview        still/video               camera_exit
 * NVCAMERA_BUFFER_PERF_LEAN   {2, 0, 4, 0, 0, 4}   {2, 0, 4, 0, 0, 4}    {2, 2, 4, B/0, 1/B, 4}    {2, 0, 0, 0, 0, 4}
 * NVCAMERA_BUFFER_PERF_MIXED  {2, 1, 4, 1, 1, 4}          B                     B                  {2, 0, 0, 0, 0, 4}
 * NVCAMERA_BUFFER_PERF_PERF          B                    B                     B                          B
 *
 * The difference between LEAN and MIXED perf scheme is that in MIXED perf scheme, B is allocated
 *  after 1st preview and remain static after that. In comparison, in LEAN perf scheme, we allocate
 *  still/video related buffer only when we capture images or start recording and deallocate when we
 *  finish recording.
 *
 * Please note that the above scheme indicates that the footprint scheme should be selected
 *  prior to performance scheme.
 *
 * The default selected schemes can be overwritten by
 *  adb shell setprop camera.memconfig.override 1       #Enable schemes overwritten
 *  adb shell setprop camera.memconfig.footprint 0/1/2  #0-LEAN footprint; 1-MIXED footprint; 2-PERF footprint
 *  adb shell setprop camera.memconfig.perf 0/1/2       #0-LEAN perf; 1-MIXED perf; 2-PERF perf
 */

// The scheme selection threshold 1.5GB
#define FOOTPRINT_SCHEME_SELECT_THRESHOLD   1.5*1024*1024*1024
#define PERF_SCHEME_SELECT_THRESHOLD        1.5*1024*1024*1024

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define CHECK_STAGE_VALUE(stage) \
{ \
    if (stage < NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH || \
        stage >= NVCAMERA_BUFFERCONFIG_STAGE_NUM) \
    { \
        ALOGE("%s Stage value out of range. Value is %d, valid range [%d , %d)", \
            __FUNCTION__, stage, \
            NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH, NVCAMERA_BUFFERCONFIG_STAGE_NUM); \
        return NvError_BadParameter; \
    } \
}

#define CHECK_ID_VALUE(id) \
{ \
    if (id < NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW || \
        id >= NVCAMERA_BUFFERCONFIG_NUM) \
    { \
        ALOGE("%s ID value out of range. Value is %d, valid range [%d , %d)", \
            __FUNCTION__, id, \
            NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW, NVCAMERA_BUFFERCONFIG_NUM); \
        return NvError_BadParameter; \
    } \
}

namespace android {

static const NvU32 LEAN_FOOTPRINT_INIT_ARRAY[NVCAMERA_BUFFERCONFIG_NUM] =
{
    6, //NVCAMERA_BUFFERCONFIG_CAPTURE_PREVIEW
    2, //NVCAMERA_BUFFERCONFIG_CAPTURE_STILL
#if NV_CAMERA_V3
    10, //NVCAMERA_BUFFERCONFIG_DZ_PREVIEW
#else
    2, //NVCAMERA_BUFFERCONFIG_DZ_PREVIEW
#endif
    3, //NVCAMERA_BUFFERCONFIG_DZ_STILL
    6, //NVCAMERA_BUFFERCONFIG_DZ_VIDEO, MAX(NUM_BUFFERS_REQUIRED_FOR_VSTAB, 6)
    6, //NVCAMERA_BUFFERCONFIG_VSTAB
    4, //NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT
};

static const NvU32 MIXED_FOOTPRINT_INIT_ARRAY[NVCAMERA_BUFFERCONFIG_NUM] =
{
    8, //NVCAMERA_BUFFERCONFIG_CAPTURE_PREVIEW
    2, //NVCAMERA_BUFFERCONFIG_CAPTURE_STILL
#if NV_CAMERA_V3
    10, //NVCAMERA_BUFFERCONFIG_DZ_PREVIEW
#else
    2, //NVCAMERA_BUFFERCONFIG_DZ_PREVIEW
#endif
    3, //NVCAMERA_BUFFERCONFIG_DZ_STILL
    6, //NVCAMERA_BUFFERCONFIG_DZ_VIDEO, MAX(NUM_BUFFERS_REQUIRED_FOR_VSTAB, 6)
    6, //NVCAMERA_BUFFERCONFIG_VSTAB
    4, //NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT
};

static const NvU32 PERF_FOOTPRINT_INIT_ARRAY[NVCAMERA_BUFFERCONFIG_NUM] =
{
    8, //NVCAMERA_BUFFERCONFIG_CAPTURE_PREVIEW
    5, //NVCAMERA_BUFFERCONFIG_CAPTURE_STILL
#if NV_CAMERA_V3
    40, //NVCAMERA_BUFFERCONFIG_DZ_PREVIEW
#else
    2, //NVCAMERA_BUFFERCONFIG_DZ_PREVIEW
#endif
    4, //NVCAMERA_BUFFERCONFIG_DZ_STILL
    12,//NVCAMERA_BUFFERCONFIG_DZ_VIDEO, MAX(NUM_BUFFERS_REQUIRED_FOR_VSTAB, 6)
    12,//NVCAMERA_BUFFERCONFIG_VSTAB
    8, //NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT
};

void NvCameraMemProfileConfigurator::PrintBufferConfig(
    char *msg,
    NvU32 array[NVCAMERA_BUFFERCONFIG_NUM][NVCAMERA_BUFFERCONFIG_NUM])
{
#define PRINT_ONE_STAGE(stage, array) \
    ALOGD("  %d         %d         %d          %d          %d          %d \
         %d          %d", stage, \
        array[stage][NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW], \
        array[stage][NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE], \
        array[stage][NVCAMERA_BUFFERCONFIG_DZ_PREVIEW], \
        array[stage][NVCAMERA_BUFFERCONFIG_DZ_STILL], \
        array[stage][NVCAMERA_BUFFERCONFIG_DZ_VIDEO], \
        array[stage][NVCAMERA_BUFFERCONFIG_VSTAB], \
        array[stage][NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT])

    NvU32 stage;
    ALOGD("%s", msg);
    ALOGD("stage  | CAP_PREV | CAP_STILL | DZ_PREV | DZ_STILL | DZ_VIDEO | DZ_VSTAB | HAL_JPEG");
    stage = NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH;
    PRINT_ONE_STAGE(stage, array);
    stage = NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW;
    PRINT_ONE_STAGE(stage, array);
    stage = NVCAMERA_BUFFERCONFIG_STAGE_EXIT;
    PRINT_ONE_STAGE(stage, array);
    stage = NVCAMERA_BUFFERCONFIG_STAGE_LEAN_STILL;
    PRINT_ONE_STAGE(stage, array);
    stage = NVCAMERA_BUFFERCONFIG_STAGE_LEAN_VIDEO;
    PRINT_ONE_STAGE(stage, array);
}

NvCameraMemProfileConfigurator::NvCameraMemProfileConfigurator()
{
    Mutex::Autolock lock(&mLock);

    mConfigStage = NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH;

    DecideMemConfigScheme();

    //Populate footprint array first
    GenerateFootprintSchemeConfig();

    //Setup perf scheme
    GeneratePerfSchemeConfig();

    PrintBufferConfig((char *)"Buffer Congiuration", mBufferMaxAmount);
}

void NvCameraMemProfileConfigurator::DecideMemConfigScheme()
{
    // Check the total size of memory
    NvS64 pageNum = sysconf(_SC_PHYS_PAGES);
    NvS64 pageSize = sysconf(_SC_PAGE_SIZE);
    NvS64 memSize = pageNum * pageSize;

    char value[PROPERTY_VALUE_MAX];
    property_get("camera.memconfig.override", value, 0);

    ALOGV("Camera Buffer Configurator: Page Number: %lld, Page Size %lld, Total Memory Size %lld",
        pageNum, pageSize, memSize);

    if (pageNum <= 0 || pageSize <= 0)
    {
        ALOGE("%s: Error in getting page size/number!", __FUNCTION__);
        mFootprintScheme = NVCAMERA_BUFFER_FOOTPRINT_LEAN;
        mPerfScheme = NVCAMERA_BUFFER_PERF_LEAN;
    }
    else if (atoi(value) != 0)
    {
        int overrideScheme;

        //Read footprint scheme
        property_get("camera.memconfig.footprint", value, 0);
        overrideScheme = atoi(value);
        if (overrideScheme < NVCAMERA_BUFFER_FOOTPRINT_LEAN) {
            overrideScheme = NVCAMERA_BUFFER_FOOTPRINT_LEAN;
        }
        else if (overrideScheme > NVCAMERA_BUFFER_FOOTPRINT_PERF) {
            overrideScheme = NVCAMERA_BUFFER_FOOTPRINT_PERF;
        }
        mFootprintScheme = (NvCameraBufferFootprintScheme) overrideScheme;

        //Read perf scheme
        property_get("camera.memconfig.perf", value, 0);
        overrideScheme = atoi(value);
        if (overrideScheme < NVCAMERA_BUFFER_PERF_LEAN) {
            overrideScheme = NVCAMERA_BUFFER_PERF_LEAN;
        }
        else if (overrideScheme > NVCAMERA_BUFFER_PERF_PERF) {
            overrideScheme = NVCAMERA_BUFFER_PERF_PERF;
        }
        mPerfScheme = (NvCameraBufferPerfScheme) overrideScheme;

        ALOGW("Camera: Memory config is overwritten. Footprint scheme %d, Perf scheme %d",
            mFootprintScheme, mPerfScheme);
    }
    else
    {
        // Select footprint scheme
        if (memSize < FOOTPRINT_SCHEME_SELECT_THRESHOLD)
        {
            ALOGV("%s: Select LEAN footprint profile", __FUNCTION__);
            mFootprintScheme = NVCAMERA_BUFFER_FOOTPRINT_LEAN;
        }
        else
        {
            ALOGV("%s: Select PERF footprint profile", __FUNCTION__);
            mFootprintScheme = NVCAMERA_BUFFER_FOOTPRINT_PERF;
        }

        // Select performance scheme
        if (memSize < PERF_SCHEME_SELECT_THRESHOLD)
        {
            ALOGV("%s: Select LEAN perf scheme", __FUNCTION__);
            mPerfScheme = NVCAMERA_BUFFER_PERF_MIXED;
        }
        else
        {
            ALOGV("%s: Select PERF perf scheme", __FUNCTION__);
            mPerfScheme = NVCAMERA_BUFFER_PERF_PERF;
        }
    }
}

// To populate an array for footprint config based on selected scheme
void NvCameraMemProfileConfigurator::GenerateFootprintSchemeConfig()
{
    //Copy values from the corresponding scheme array
    if (mFootprintScheme == NVCAMERA_BUFFER_FOOTPRINT_PERF)
    {
        memcpy(mMaxBufNumAtPort, PERF_FOOTPRINT_INIT_ARRAY,
            NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
    }
    else if (mFootprintScheme == NVCAMERA_BUFFER_FOOTPRINT_LEAN)
    {
        memcpy(mMaxBufNumAtPort, LEAN_FOOTPRINT_INIT_ARRAY,
            NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
    }
    else
    {
        memcpy(mMaxBufNumAtPort, MIXED_FOOTPRINT_INIT_ARRAY,
            NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
    }
}

void NvCameraMemProfileConfigurator::GenerateLeanPerfSchemeArray()
{
    const NvU32
    InitBufferAmount[NVCAMERA_BUFFERCONFIG_STAGE_NUM][NVCAMERA_BUFFERCONFIG_NUM] =
    {
        //Launch stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            0,
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            0,
            0,
            //Others
            1,
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //After 1st preview stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            0,
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            0,
            0,
            //Others
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_VSTAB],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //Exit stage
        {
            //Camera block
            0,
            0,
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            0,
            0,
            //Others
            0,
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //LEAN still stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE], //used in this stage
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_STILL],       //used in this stage
            0,
            //Others
            1,
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //LEAN video stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE], //used in this stage
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            1,                                                      //used for video snapshot
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_VIDEO],       //used in this stage
            //Others
            1,
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        }
    };

    //Copy values to the array
    memcpy(mInitBufferMinAmount, InitBufferAmount,
        NVCAMERA_BUFFERCONFIG_STAGE_NUM*NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
    memcpy(mInitBufferMaxAmount, InitBufferAmount,
        NVCAMERA_BUFFERCONFIG_STAGE_NUM*NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
}

void NvCameraMemProfileConfigurator::GenerateMixedPerfSchemeArray()
{
    const NvU32
    InitBufferAmount[NVCAMERA_BUFFERCONFIG_STAGE_NUM][NVCAMERA_BUFFERCONFIG_NUM] =
    {
        //Launch stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE],
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            1,
            1,
            //Others
            1,
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //After 1st preview stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE],
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_STILL],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_VIDEO],
            //Others
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_VSTAB],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //Exit stage
        {
            //Camera block
            0,
            0,
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            0,
            0,
            //Others
            0,
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //LEAN still stage
        {
            0
        },
        //LEAN video stage
        {
            0
        }
    };

    //Copy values to the array
    memcpy(mInitBufferMinAmount, InitBufferAmount,
        NVCAMERA_BUFFERCONFIG_STAGE_NUM*NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
    memcpy(mInitBufferMaxAmount, InitBufferAmount,
        NVCAMERA_BUFFERCONFIG_STAGE_NUM*NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
}

void NvCameraMemProfileConfigurator::GeneratePerfPerfSchemeArray()
{
    const NvU32
    InitBufferAmount[NVCAMERA_BUFFERCONFIG_STAGE_NUM][NVCAMERA_BUFFERCONFIG_NUM] =
    {
        //Launch stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE],
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_STILL],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_VIDEO],
            //Others
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_VSTAB],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //After 1st preview stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE],
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_STILL],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_VIDEO],
            //Others
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_VSTAB],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //Exit stage
        {
            //Camera block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE],
            //DZ block
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_PREVIEW],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_STILL],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_DZ_VIDEO],
            //Others
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_VSTAB],
            mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT]
        },
        //LEAN still stage
        {
            0
        },
        //LEAN video stage
        {
            0
        }
    };

    //Copy values to the array
    memcpy(mInitBufferMinAmount, InitBufferAmount,
        NVCAMERA_BUFFERCONFIG_STAGE_NUM*NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
    memcpy(mInitBufferMaxAmount, InitBufferAmount,
        NVCAMERA_BUFFERCONFIG_STAGE_NUM*NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
}

// Currently, we set the same values for min and max amounts. But this can
// be changed in the future.
// To populate an array for perf config based on selected perf scheme
// and input footprint array
void NvCameraMemProfileConfigurator::GeneratePerfSchemeConfig()
{
    //PERF scheme
    if (mPerfScheme == NVCAMERA_BUFFER_PERF_PERF)
    {
        ALOGV("%s: Select PERF footprint profile");
        GeneratePerfPerfSchemeArray();
    }
    //MIXED scheme
    else if (mPerfScheme == NVCAMERA_BUFFER_PERF_MIXED)
    {
        ALOGV("%s: Select MIXED footprint profile");
        GenerateMixedPerfSchemeArray();
    }
    //LEAN scheme
    else
    {
        ALOGV("%s: Select LEAN footprint profile");
        GenerateLeanPerfSchemeArray();
    }

    memcpy(mBufferMinAmount, mInitBufferMinAmount,
        NVCAMERA_BUFFERCONFIG_STAGE_NUM*NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
    memcpy(mBufferMaxAmount, mInitBufferMaxAmount,
        NVCAMERA_BUFFERCONFIG_STAGE_NUM*NVCAMERA_BUFFERCONFIG_NUM*sizeof(NvU32));
}

NvError NvCameraMemProfileConfigurator::SetBufferConfigStage(
    NvCameraBufferConfigStage stage)
{
    Mutex::Autolock lock(&mLock);

    CHECK_STAGE_VALUE(stage);

    ALOGV("NvCameraMemProfileConfigurator changes to stage %d", stage);

    mConfigStage = stage;
    return NvSuccess;
}

NvCameraBufferPerfScheme NvCameraMemProfileConfigurator::GetBufferConfigPerfScheme()
{
    Mutex::Autolock lock(&mLock);

    return mPerfScheme;
}

NvCameraBufferConfigStage NvCameraMemProfileConfigurator::GetBufferConfigStage()
{
    Mutex::Autolock lock(&mLock);

    return mConfigStage;
}

NvCameraBufferFootprintScheme NvCameraMemProfileConfigurator::GetBufferFootprintScheme()
{
    Mutex::Autolock lock(&mLock);

    return mFootprintScheme;
}

NvError NvCameraMemProfileConfigurator::ResetBufferAmount(
    NvCameraBufferConfigStage stage,
    NvCameraBufferConfigPortID id)
{
    Mutex::Autolock lock(&mLock);

    CHECK_STAGE_VALUE(stage);
    CHECK_ID_VALUE(id);

    mBufferMinAmount[stage][id] =
        mInitBufferMinAmount[stage][id];

    mBufferMaxAmount[stage][id] =
        mInitBufferMaxAmount[stage][id];

    return NvSuccess;
}

NvError NvCameraMemProfileConfigurator::SetBufferAmount(
    NvCameraBufferConfigStage stage,
    NvCameraBufferConfigPortID id,
    NvU32 minAmount, NvU32 maxAmount)
{
    Mutex::Autolock lock(&mLock);

    CHECK_STAGE_VALUE(stage);
    CHECK_ID_VALUE(id);

    if (minAmount != INVALID_NUMBER_OF_BUFFERS)
    {
        mBufferMinAmount[stage][id] = minAmount;
    }

    if (maxAmount != INVALID_NUMBER_OF_BUFFERS)
    {
        mBufferMaxAmount[stage][id] = maxAmount;
    }
    return NvSuccess;
}

NvError NvCameraMemProfileConfigurator::GetBufferAmount(
    NvCameraBufferConfigStage stage,
    NvCameraBufferConfigPortID id,
    NvU32* minAmount, NvU32* maxAmount)
{
    Mutex::Autolock lock(&mLock);

    CHECK_STAGE_VALUE(stage);
    CHECK_ID_VALUE(id);

    if (minAmount)
    {
        minAmount[0] = mBufferMinAmount[stage][id];
    }

    if (maxAmount)
    {
        maxAmount[0] = mBufferMaxAmount[stage][id];
    }
    return NvSuccess;
}

NvError NvCameraMemProfileConfigurator::GetInitBufferAmount(
    NvCameraBufferConfigStage stage,
    NvCameraBufferConfigPortID id,
    NvU32* minAmount, NvU32* maxAmount)
{
    Mutex::Autolock lock(&mLock);

    CHECK_STAGE_VALUE(stage);
    CHECK_ID_VALUE(id);

    if (minAmount)
    {
        minAmount[0] = mInitBufferMinAmount[stage][id];
    }

    if (maxAmount)
    {
        maxAmount[0] = mInitBufferMaxAmount[stage][id];
    }
    return NvSuccess;
}

} // namespace android
