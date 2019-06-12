/*
 * Copyright (c) 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvaudioalsa.h"

//*****************************************************************************
// createAudioHardware
//*****************************************************************************
//
android_audio_legacy::AudioHardwareInterface* android_audio_legacy::createAudioHardware(void)
{
    NvAudioALSADevice* dev = 0;
    dev = new NvAudioALSADevice();

    // Android expects AudioHardwareInterface allocation to succeed
    // even if the init fails. Soon after returning, if initCheck()
    // says that init failed, this instance gets deleted and a stub
    // implementation is allocated instead.
    // See frameworks/base/libs/audioflinger/AudioHardwareInterface.cpp,
    // function create().
    return dev;
}


