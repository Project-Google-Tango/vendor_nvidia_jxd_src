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
 *           NvMM Audio Mixer APIs</b>
 *
 * @b Description: Declares Interface for NvMM Audio Mixer APIs.
 */

#ifndef INCLUDED_NVMM_MIXER_H
#define INCLUDED_NVMM_MIXER_H

/**
 * @defgroup nvmm_mixer Audio Mixer Multimedia API
 *
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"

#include "nvddk_geq.h"
#include "nvmm.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define MAX_CONFIGURABLE_BUFFERS          20
#define MAX_MIXER_INPUT_PLAYBACK_STREAMS  7
#define MAX_MIXER_INPUT_RECORD_STREAMS    1
#define MAX_MIXER_INPUT_STREAMS           (MAX_MIXER_INPUT_PLAYBACK_STREAMS + MAX_MIXER_INPUT_RECORD_STREAMS)
#define MAX_MIXER_OUTPUT_PLAYBACK_STREAMS 1
#define MAX_MIXER_OUTPUT_RECORD_STREAMS   1
#define MAX_MIXER_OUTPUT_STREAMS          (MAX_MIXER_OUTPUT_PLAYBACK_STREAMS + MAX_MIXER_OUTPUT_RECORD_STREAMS)
#define MAX_MIXER_TOTAL_STREAMS           (MAX_MIXER_INPUT_STREAMS + MAX_MIXER_OUTPUT_STREAMS)

#define MIXER_NUM_PARA_EQ_FILTERS         4
#define MIXER_BANDPASS_IIR_FILTER         1
#define MIXER_HIGHPASS_IIR_FILTER         2
#define MIXER_LOWPASS_IIR_FILTER          3


enum NV_MIXER_PIN_TYPE
{
    PLAYBACK_PIN = 0,
    RECORD_PIN = 1
};


/**
 * @brief Audio Mixer Attribute enumerations.
 * Following enum are used by audio mixer for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * @see SetAttribute
 */
typedef enum
{
    NvMMMixerAttribute_CommonStreamProperty = (NvMMAttributeAudioMixer_Unknown + 1),
    NvMMMixerAttribute_MixerCaps,
    NvMMMixerAttribute_Position,
    NvMMMixerAttribute_Volume,
    NvMMMixerAttribute_SampleRate,
    NvMMMixerAttribute_DRC,
    NvMMMixerAttribute_AGC,
    NvMMMixerAttribute_SPD,
    NvMMMixerAttribute_EQ,
    NvMMMixerAttribute_ParaEQFilter,
    NvMMMixerAttribute_SetState,
    NvMMMixerAttribute_TimeStamp,
    NvMMMixerAttribute_TraceTimeStamps,
    NvMMMixerAttribute_SendTimeStampEvents,
    NvMMMixerAttribute_SetULPMode,
    NvMMMixerAttribute_TraceGaplessTime,
    NvMMMixerAttribute_LRVolume,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMMixerAttribute_Force32 = 0x7FFFFFFF

}NvMMAudioMixerAttribute;

typedef enum
{
    NvMMMixerStream_Undefined = 0,
    NvMMMixerStream_Input = 1,
    NvMMMixerStream_Output,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMMixerStream_Force32 = 0x7FFFFFFF

} NvMMMixerStreamDirection;


typedef struct MixerVolumePanMute_
{
    NvS32   Volume;
    NvS32   Pan;
    NvBool  Mute;

}MixerVolPanMute;

typedef struct MixerVolumeMute_
{
    NvS32   LeftVolume;
    NvS32   RightVolume;
    NvBool  Mute;

}MixerVolMute;

typedef struct NvMMMixerStreamSetVolumePanMuteProperty_
{
    NvU32   structSize;
    /// Index of stream with given properties
    NvU32   StreamIndex;

    MixerVolPanMute volPanMute;

} NvMMMixerStreamSetVolumePanMuteProperty;

typedef struct NvMMMixerStreamGetVolumePanMuteProperty_
{
    NvU32   structSize;
    /// Index of stream with given properties

    MixerVolPanMute volPanMute[MAX_MIXER_TOTAL_STREAMS];

} NvMMMixerStreamGetVolumePanMuteProperty;

typedef struct NvMMMixerStreamSetVolumeMuteProperty_
{
    NvU32   structSize;
    /// Index of stream with given properties
    NvU32   StreamIndex;

    MixerVolMute volMute;

} NvMMMixerStreamSetVolumeMuteProperty;

typedef struct NvMMMixerStreamGetVolumeMuteProperty_
{
    NvU32   structSize;
    /// Index of stream with given properties

    MixerVolMute volMute[MAX_MIXER_TOTAL_STREAMS];

} NvMMMixerStreamGetVolumeMuteProperty;

typedef struct NvMMMixerStreamSetSampleRateProperty_
{
    NvU32   structSize;
    /// Index of stream with given properties
    NvU32   StreamIndex;

    NvS32   sampleRate;

} NvMMMixerStreamSetSampleRateProperty;

typedef struct NvMMMixerStreamGetSampleRateProperty_
{
    NvU32   structSize;
    /// Index of stream with given properties

    NvS32   sampleRate[MAX_MIXER_TOTAL_STREAMS];

} NvMMMixerStreamGetSampleRateProperty;


//#include "nvddk_GEQ.h"

typedef struct NvMMMixerStreamParaEQFilterProperty_
{
    NvU32   structSize;
    NvU32   bEnable;
    NvU32   bUpdate;
    NvMMMixerStreamDirection Direction;
    NvU32   FilterType[MIXER_NUM_PARA_EQ_FILTERS];
    NvS32   F0[MIXER_NUM_PARA_EQ_FILTERS];
    NvS32   BndWdth[MIXER_NUM_PARA_EQ_FILTERS];
    NvS32   dBGain[MIXER_NUM_PARA_EQ_FILTERS];
} NvMMMixerStreamParaEQFilterProperty;

typedef struct NvMMMixerStreamDrcProperty_
{
    NvU32   structSize;
    NvS32   EnableDrc;
    NvS32   NoiseGateTh;
    NvS32   LowerCompTh;
    NvS32   UpperCompTh;
    NvS32   ClippingTh;
} NvMMMixerStreamDrcProperty;

typedef struct NvMMMixerStreamSpdProperty_
{
    NvU32   structSize;
    NvU32   SpeakerWidth;
}NvMMMixerStreamSpdProperty;

/**
 * @brief Defines the structure for holding the configuartion for the decoder
 * capabilities. These are stream independent properties. Decoder fills this
 * structure on NvMMGetMMBlockCapabilities() function being called by Client.
 * @see NvMMGetMMBlockCapabilities
 */
typedef struct
{
    NvU32 structSize;
    /// Holds minimum sample rate supported by mixer
    NvU32 MinSampleRate;
    /// Holds maximum sample rate supported by mixer
    NvU32 MaxSampleRate;
     /// Holds minimum bits per sample supported by mixer
    NvU32 MinBitsPerSample;
    /// Holds maximum bits per sample supported by mixer
    NvU32 MaxBitsPerSample;
    /// Holds maximum number of channels supported by mixer
    NvU32 Channels;
    /// Holds maximum number of input streams supported by mixer
    NvU32 MaxInputStreams;

} NvMMMixerCapabilities;

typedef struct
{
    // Holds the current position of all streams
    NvU32 position[MAX_MIXER_TOTAL_STREAMS];

} NvMMMixerPosition;

typedef struct NvMMMixerSetState_
{
    /// Pin to control
    NvU32 pin;              // Input/Output
    /// Pin's new state
    NvMMState newState;     // Input

}NvMMMixerSetState;

typedef struct NvMMMixerTimeStamp_
{
    /// Pin to control
    NvU32 pin;
    /// Pin's timestamp
    NvU64 TimeStamp;

}NvMMMixerTimeStamp;

typedef struct NvMMMixerTraceTimeStamps_
{
    /// Enable or Disable this function
    NvBool TraceTimeStamps;

}NvMMMixerTraceTimeStamps;

typedef struct NvMMMixerSendTimeStampEvents_
{
    /// Enable or Disable this function
    NvU32  SendTimeStamps;
    NvU32  TimeStampFrequency;
}NvMMMixerSendTimeStampEvents;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_MIXER_H
