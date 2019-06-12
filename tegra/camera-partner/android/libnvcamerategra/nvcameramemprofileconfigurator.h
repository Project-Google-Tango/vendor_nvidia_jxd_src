/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NV_CAMERA_HAL_MEM_CONFIGURATOR_H
#define NV_CAMERA_HAL_MEM_CONFIGURATOR_H

#include <nvcommon.h>
#include <nverror.h>
#include <utils/threads.h>
#include <utils/Log.h>

typedef enum {
    NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW = 0,   //Camera block preview output
    NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,       //Camera block capture output
    NVCAMERA_BUFFERCONFIG_DZ_PREVIEW,           //DZ block preview output
    NVCAMERA_BUFFERCONFIG_DZ_STILL,             //DZ block still output
    NVCAMERA_BUFFERCONFIG_DZ_VIDEO,             //DZ block video output
    NVCAMERA_BUFFERCONFIG_VSTAB,                //VSTAB backward frames
    NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT,          //Jpeg output buffer number

    NVCAMERA_BUFFERCONFIG_NUM,
} NvCameraBufferConfigPortID;

typedef enum {
    NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH = 0,
    NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
    NVCAMERA_BUFFERCONFIG_STAGE_EXIT,
    NVCAMERA_BUFFERCONFIG_STAGE_LEAN_STILL,     //Only used in LEAN perf scheme
    NVCAMERA_BUFFERCONFIG_STAGE_LEAN_VIDEO,     //Only used in LEAN perf scheme

    NVCAMERA_BUFFERCONFIG_STAGE_NUM,
} NvCameraBufferConfigStage;

typedef enum {
    NVCAMERA_BUFFER_FOOTPRINT_LEAN = 0,         //Min memory footprint
    NVCAMERA_BUFFER_FOOTPRINT_MIXED,            //Intermediate memory footprint
    NVCAMERA_BUFFER_FOOTPRINT_PERF,             //Max memory footprint

    NVCAMERA_BUFFER_FOOTPRINT_NUM,
} NvCameraBufferFootprintScheme;

typedef enum {
    NVCAMERA_BUFFER_PERF_LEAN = 0,              //LEAN performance scheme
    NVCAMERA_BUFFER_PERF_MIXED,                 //MIXED performance scheme
    NVCAMERA_BUFFER_PERF_PERF,                  //PERF performance scheme

    NVCAMERA_BUFFER_PERF_NUM,
} NvCameraBufferPerfScheme;

namespace android {

class NvCameraMemProfileConfigurator
{
public:
    NvCameraMemProfileConfigurator();
    ~NvCameraMemProfileConfigurator();

    static const NvU32 MAX_NUMBER_OF_BUFFERS = 20;
    static const NvU32 INVALID_NUMBER_OF_BUFFERS = -1;

    // Set current buffer configuration stage
    NvError SetBufferConfigStage(NvCameraBufferConfigStage stage);

    // Get current buffer configuration perf scheme
    NvCameraBufferPerfScheme GetBufferConfigPerfScheme();

    // Get current buffer configuration stage
    NvCameraBufferConfigStage GetBufferConfigStage();

    NvCameraBufferFootprintScheme GetBufferFootprintScheme();

    // Set the min/max amount of buffer for one ID at one stage.
    // We can set both min and max amount here. If we don't want to
    // change either value, we should pass in
    // NvCameraMemProfileConfigurator::INVALID_NUMBER_OF_BUFFERS
    // We should query the stage before calling this function.
    NvError SetBufferAmount(
        NvCameraBufferConfigStage stage,
        NvCameraBufferConfigPortID id,
        NvU32 minAmount, NvU32 maxAmount);

    // Reset the min/max amount of buffer for one ID at one stage.
    NvError ResetBufferAmount(
        NvCameraBufferConfigStage stage,
        NvCameraBufferConfigPortID id);

    // Get the min/max amount of buffer for one ID at one stage.
    // We should query the stage before calling this function.
    // We can pass in NULL pointer if we don't need the number
    NvError GetBufferAmount(
        NvCameraBufferConfigStage stage,
        NvCameraBufferConfigPortID id,
        NvU32* minAmount, NvU32* maxAmount);

    // Get the min/max initial amount of buffer for one ID at one
    // stage. We should query the stage before calling this function.
    // We can pass in NULL pointer if we don't need the number
    NvError GetInitBufferAmount(
        NvCameraBufferConfigStage stage,
        NvCameraBufferConfigPortID id,
        NvU32* minAmount, NvU32* maxAmount);

private:

    // Decide which memory configuration to use based on memory size
    void DecideMemConfigScheme();

    // Footprint config should be setup first before
    // perf config because perf config relies on the footprint config!!!

    // To populate an array for footprint config based on selected scheme
    void GenerateFootprintSchemeConfig();

    // Helper functions to populate the perf config array
    void GenerateLeanPerfSchemeArray();
    void GenerateMixedPerfSchemeArray();
    void GeneratePerfPerfSchemeArray();

    // To populate an array for perf config based on selected perf scheme
    // and input footprint array
    void GeneratePerfSchemeConfig();

    void PrintBufferConfig(char *msg,
        NvU32 array[NVCAMERA_BUFFERCONFIG_NUM][NVCAMERA_BUFFERCONFIG_NUM]);

private:
    Mutex mLock;

    NvCameraBufferConfigStage mConfigStage;

    // Max number of buffer at each ports
    NvU32 mMaxBufNumAtPort[NVCAMERA_BUFFERCONFIG_NUM];

    NvU32 mInitBufferMaxAmount[NVCAMERA_BUFFERCONFIG_STAGE_NUM][NVCAMERA_BUFFERCONFIG_NUM];
    NvU32 mInitBufferMinAmount[NVCAMERA_BUFFERCONFIG_STAGE_NUM][NVCAMERA_BUFFERCONFIG_NUM];

    NvU32 mBufferMaxAmount[NVCAMERA_BUFFERCONFIG_STAGE_NUM][NVCAMERA_BUFFERCONFIG_NUM];
    NvU32 mBufferMinAmount[NVCAMERA_BUFFERCONFIG_STAGE_NUM][NVCAMERA_BUFFERCONFIG_NUM];

    NvCameraBufferFootprintScheme mFootprintScheme;
    NvCameraBufferPerfScheme mPerfScheme;
};

} // namespace android

#endif
