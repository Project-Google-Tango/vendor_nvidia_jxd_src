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
 *           NvMM Audio APIs</b>
 *
 * @b Description: Declares Interface for NvMM Audio APIs.
 */

#ifndef INCLUDED_NVMM_AUDIO_H
#define INCLUDED_NVMM_AUDIO_H

/**
 * @defgroup nvmm_audio Audio Multimedia API
 *
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif


#define NVMM_AUDIO_IO_DEFAULT_SAMPLE_RATE 44100
#define NVMM_AUDIO_IO_DEFAULT_MAX_BUFFER_SIZE 8192
#define NVMM_AUDIO_IO_DEFAULT_MIN_BUFFER_SIZE 8192
#define NVMM_AUDIO_IO_DEFAULT_MAX_BUFFERS 10
#define NVMM_AUDIO_IO_DEFAULT_MIN_BUFFERS 4



/**
 * @brief Audio Attribute enumerations.
   Following enum are used by audio for setting the attributes.
   They are used as 'eAttributeType' variable in SetAttribute()
   API.
 * @see SetAttribute
 */

typedef enum
{
    NvMMAudioAttribute_Default = (NvMMAttributeAudioDec_Unknown + 9000),
    NvMMAudioAttribute_Initialize,
    NvMMAudioAttribute_File,
    NvMMAudioAttribute_Position,
    NvMMAudioAttribute_Volume,
    NvMMAudioAttribute_MaxOutputChannels,
    NvMMAudioAttribute_DualMonoOutputMode,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAudioAttribute_Force32 = 0x7FFFFFFF

} NvMMAudioAttribute;

typedef enum
{
    NvMMAudioExtension_SetState = 0,
    NvMMAudioExtension_GetPin,
    NvMMAudioExtension_ReleasePin,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAudioExtension_Force32 = 0x7FFFFFFF

} NvMMAudioExtension;

typedef enum
{
    NVMM_AUDIO_PINTYPE_PLAYBACK,
    NVMM_AUDIO_PINTYPE_RECORD,

    NVMM_AUDIO_PINTYPE_Force32 = 0x7FFFFFFF

} NvMMAudioPinType;

typedef enum
{
    NVMM_AUDIO_DIRECTION_INPUT,
    NVMM_AUDIO_DIRECTION_OUTPUT,

    NVMM_AUDIO_DIRECTION_Force32 = 0x7FFFFFFF

} NvMMAudioPinDirection;

#define NVMM_AUDIO_PIN_BAD -1

//
// Structures
//

typedef struct NvMMAudioFileProperty_
{
    NvU32 structSize;

    NvU32 enable;
    #define NVMM_AUDIO_MAX_FILENAME_LENGTH 225
    NvU8 filename[NVMM_AUDIO_MAX_FILENAME_LENGTH];

} NvMMAudioFileProperty;


typedef struct NvMMAudioVolPanMute_
{
    NvS32 Volume;
    NvS32 Pan;
    NvBool Mute;

} NvMMAudioVolPanMute;


typedef struct NvMMAudioVolumePanMuteProperty_
{
    NvU32 structSize;

    #define NVMM_AUDIO_MAX_VOL_PAN_MUTE_INDEX  8
    NvU32 index;

    NvMMAudioVolPanMute volPanMute;

} NvMMAudioVolumePanMuteProperty;


typedef struct NvMMAudioIoStreamProperty_
{
    NvU32 structSize;

    /// Number of queues for each buffer, must be at least as large as largest of next two values
    NvU32 totalQueueSize;
    /// Total number of input buffers
    NvU32 totalInputBuffers;

} NvMMAudioIoStreamProperty;

typedef struct NvMMMixerOutputFormat_
{
    NvU32 structSize;
    /// Output sampler rate of stream in Hz
    NvU32 SampleRate;
    /// Number of valid bits in a sample
    NvU32 BitsPerSample;
    /// Number of output audio channels in stream
    NvU32 Channels;

} NvMMMixerOutputFormat;

typedef struct NvMMMixerStreamState_
{
    /// Status of call
    NvError status;         // Output
    /// Pin to control
    NvU32 pin;              // Input/Output
    /// Pin's new state
    NvMMState newState;     // Input
    /// Pin's previous state
    NvMMState prevState;    // Output

}NvMMMixerStreamState;

typedef struct NvMMMixerStreamProperty_
{
    NvU32 structSize;
    NvU32 StreamIndex; // TODO: no longer used, removed after WaveDev is repaired.
    /// Output sampler rate of stream in Hz
    NvU32 SampleRate;
    /// Number of valid bits in a sample
    NvU32 BitsPerSample;
    /** size of the sample container in byte
     * For example 24bit data in 32bit container would be a container size of 4
     */
    NvU32 ContainerSize;
    /// Block alignment in bytes of a sample block
    NvU32 BlockAlign;
    /// Size in bytes of frames of data used by decoder
    NvU32 FrameSize;
    /// Number of output audio channels in stream
    NvU32 Channels;

    /// Number of queues for each buffer, must be at least as large as largest of next two values
    NvU32 QueueSize;
    /// Total number of buffers
    NvU32 BufferCount;

} NvMMMixerStreamProperty;

typedef enum
{
    NvMMAudioMixerPort_Undefined    = 0x00000000,

    // Hardware Outputs
    NvMMAudioMixerPort_I2s1         = 0x00000001,
    NvMMAudioMixerPort_Spdif        = 0x00000002,

    // Virtual Hardware Outputs
    NvMMAudioMixerPort_Music        = 0x00000004,
    NvMMAudioMixerPort_Ringtone     = 0x00000008,

    // Hardware Inputs
    NvMMAudioMixerPort_BuiltinMic   = 0x00010000,
    NvMMAudioMixerPort_Mic          = 0x00020000,
    NvMMAudioMixerPort_Linein       = 0x00040000,

} NvMMAudioMixerPort;


typedef struct NvMMMixerGetPin_
{
    /// Status of call
    NvError status;                         // Output
    /// Pin to control
    NvU32 pin;                              // Output
    /// Pin Type requested
    NvMMAudioPinType pinType;               // Input
    /// Pin Direction requested
    NvMMAudioPinDirection pinDirection;     // Input
    /// property of output pin
    NvMMMixerStreamProperty mixerProperty;  // Input
    /// Ports to send audio for pin
    NvMMAudioMixerPort pinPort;               // Input

}NvMMMixerGetPin;

typedef struct NvMMMixerReleasePin_
{
    /// Status of call
    NvError status;                         // Output

    /// Pin to release
    NvU32 pin;                              // input

}NvMMMixerReleasePin;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_AUDIO_H
