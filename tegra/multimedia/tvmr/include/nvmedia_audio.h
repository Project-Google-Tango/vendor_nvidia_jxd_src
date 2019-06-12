/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.  All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 */

/**
 * \file nvmedia_audio.h
 * \brief The NvMedia Audio API
 *
 * This file contains the \ref api_audio "Audio API".
 */

#ifndef _NVMEDIA_AUDIO_H
#define _NVMEDIA_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmedia.h"

/**
 * \defgroup api_audio Audio API
 *
 * The Audio API encompasses all NvMedia audio related functionality
 *
 * @{
 */

/** \brief Major Version number */
#define NVMEDIA_AUDIO_VERSION_MAJOR   1
/** \brief Minor Version number */
#define NVMEDIA_AUDIO_VERSION_MINOR   6

/**
 * \defgroup audiodecoder_api Audio Decoder
 *
 * The NvMediaAudioDecoder object decodes compressed audio data, writing
 * the results to the specified buffer.
 *
 * A specific NvMedia implementation may support decoding multiple
 * types of compressed audio data. However, NvMediaAudioDecoder objects
 * are able to decode a specific type of compressed audio data.
 * This type must be specified during creation.
 *
 * @{
 */

/**
 * \brief A generic "audio configuration" type.
 *
 * This type serves solely to document the expected usage of a
 * generic (void *) function parameter. In actual usage, the
 * application is expected to physically provide a pointer to an
 * instance of one of the "real" NvMediaAudioDecoderConfig* structures,
 * picking the type appropriate for the audio decoder object in
 * question.
 */
typedef void NvMediaAudioConfigurationInfo;

/**
 * \brief Audio codec type
 */
typedef enum {
    /** \brief MP3 codec */
    NVMEDIA_AUDIO_CODEC_MP3,
    /** \brief WMA Pro codec */
    NVMEDIA_AUDIO_CODEC_WMAPRO,
    /** \brief WMA9 Std codec */
    NVMEDIA_AUDIO_CODEC_WMA9STD,
    /** \brief WMA9 Lsl codec */
    NVMEDIA_AUDIO_CODEC_WMA9LSL,
    /** \brief AAC codec */
    NVMEDIA_AUDIO_CODEC_AAC,
    /** \brief VORBIS codec */
    NVMEDIA_AUDIO_CODEC_VORBIS,
    /** \brief MP2 codec */
    NVMEDIA_AUDIO_CODEC_MP2
} NvMediaAudioCodec;

/**
 * \brief MP2 Audio Stream Format type
 */
typedef enum {
    /** \hideinitializer \brief Unused format */
    NvMediaAudioMP2StreamFormatUnused = 0,
    /** \brief MPEG 1 Layer 2 Stream format */
    NvMediaAudioMP2StreamFormatMP1Layer2,
    /** \brief MPEG 2 Layer 2 Stream format */
    NvMediaAudioMP2StreamFormatMP2Layer2
} NvMediaAudioMP2StreamFormatType;

/**
 * \brief MP3 Audio Stream Format type
 */
typedef enum {
    /** \hideinitializer \brief Unused format */
    NvMediaAudioMP3StreamFormatUnused = 0,
    /** \brief MPEG 1 Layer 3 Stream format */
    NvMediaAudioMP3StreamFormatMP1Layer3,
    /** \brief MPEG 2 Layer 3 Stream format */
    NvMediaAudioMP3StreamFormatMP2Layer3,
    /** \brief MPEG 2.5 Layer 3 Stream format */
    NvMediaAudioMP3StreamFormatMP2_5Layer3
} NvMediaAudioMP3StreamFormatType;

/**
 * \brief WMA Audio Stream Format type
 */
typedef enum {
    /** \hideinitializer \brief Unused format */
    NvMediaAudioWMAStreamFormatUnused = 0,
    /** \brief Windows Media Audio format 7 */
    NvMediaAudioWMAStreamFormat7,
    /** \brief Windows Media Audio format 8 */
    NvMediaAudioWMAStreamFormat8,
    /** \brief Windows Media Audio format 9 */
    NvMediaAudioWMAStreamFormat9,
    /** \brief Windows Media Audio format 10 */
    NvMediaAudioWMAStreamFormat10
} NvMediaAudioWMAStreamFormatType;

/**
 * \brief AAC Audio Stream Format type
 */
typedef enum {
    /** \hideinitializer \brief AAC Audio Data Transport Stream 2 format */
    NvMediaAudioAACStreamFormatMP2ADTS = 0,
    /** \brief AAC Audio Data Transport Stream 4 format */
    NvMediaAudioAACStreamFormatMP4ADTS,
    /** \brief AAC Low Overhead Audio Stream format */
    NvMediaAudioAACStreamFormatMP4LOAS,
    /** \brief AAC Low Overhead Audio Transport Multiplex */
    NvMediaAudioAACStreamFormatMP4LATM,
    /** \brief AAC Audio Data Interchange Format */
    NvMediaAudioAACStreamFormatADIF,
    /** \brief AAC Inside MPEG-4/ISO File Format */
    NvMediaAudioAACStreamFormatMP4FF,
    /** \brief AAC Raw Format */
    NvMediaAudioAACStreamFormatRAW,
} NvMediaAudioAACStreamFormatType;

/**
 * \brief MP3 Audio Channel Mode type
 */
typedef enum {
    /** \hideinitializer \brief Unused mode */
    NvMediaAudioMP3ChannelModeUnused = 0,
    /** \brief Stereo mode */
    NvMediaAudioMP3ChannelModeStereo,
    /** \brief Joint Stereo mode */
    NvMediaAudioMP3ChannelModeJointStereo,
    /** \brief Dual Mono mode */
    NvMediaAudioMP3ChannelModeDualMono,
    /** \brief Mono mode */
    NvMediaAudioMP3ChannelModeMono
} NvMediaAudioMP3ChannelModeType;

/**
 * \brief MP2 Audio Channel Mode type
 */
typedef enum {
    /** \hideinitializer \brief Unused mode */
    NvMediaAudioMP2ChannelModeUnused = 0,
    /** \brief Stereo mode */
    NvMediaAudioMP2ChannelModeStereo,
    /** \brief Joint Stereo mode */
    NvMediaAudioMP2ChannelModeJointStereo,
    /** \brief Dual Mono mode */
    NvMediaAudioMP2ChannelModeDualMono,
    /** \brief Mono mode */
    NvMediaAudioMP2ChannelModeMono
} NvMediaAudioMP2ChannelModeType;

/**
 * \brief AAC Audio Channel Mode type
 */
typedef enum {
    /** \hideinitializer \brief Stereo mode - 2 channels; bitrate allocation between the two
        channels varies according to the information carried by each channel */
    NvMediaAudioAACChannelModeStereo = 0,
    /** \brief Joint Stereo mode - takes advantage of what is common between the two channels for higher compression gain */
    NvMediaAudioAACChannelModeJointStereo,
    /** \brief Dual mode - 2 mono-channels, each encoded with half of the overall bitrate */
    NvMediaAudioAACChannelModeDual,
    /** \brief Mono mode */
    NvMediaAudioAACChannelModeMono,
    /** \hideinitializer \brief Unknown mode */
    NvMediaAudioAACChannelModeUnknown = 0x7FFFFFFF
} NvMediaAudioAACChannelModeType;

/**
 * \brief WMA Audio Profile type
 */
typedef enum {
    /** \hideinitializer \brief Unused profile */
    NvMediaAudioWMAProfileUnused = 0,
    /** \brief WMA L1 profile */
    NvMediaAudioWMAProfileL1,
    /** \brief WMA L2 profile */
    NvMediaAudioWMAProfileL2,
    /** \brief WMA L3 profile */
    NvMediaAudioWMAProfileL3,
    /** \brief WMA Std profile */
    NvMediaAudioWMAProfileL,
    /** \hideinitializer \brief WMA M0a profile */
    NvMediaAudioWMAProfileM0a = 0x10,
    /** \brief WMA M0b profile */
    NvMediaAudioWMAProfileM0b,
    /** \brief WMA M1 profile */
    NvMediaAudioWMAProfileM1,
    /** \brief WMA M2 profile */
    NvMediaAudioWMAProfileM2,
    /** \brief WMA M3 profile */
    NvMediaAudioWMAProfileM3,
    /** \brief WMA Pro profile */
    NvMediaAudioWMAProfileM,
    /** \hideinitializer \brief WMA N1 profile */
    NvMediaAudioWMAProfileN1 = 0x20,
    /** \brief WMA N2 profile */
    NvMediaAudioWMAProfileN2,
    /** \brief WMA N3 profile */
    NvMediaAudioWMAProfileN3,
    /** \brief WMA Lossless profile */
    NvMediaAudioWMAProfileN,
} NvMediaAudioWMAProfileType;

/**
 * \brief AAC Audio Profile type (note that the term 'profile' is used with MPEG-2 standard and the terms 'object-type' and 'profile' are used with MPEG-4 standard)
 */
typedef enum {
    /** \hideinitializer \brief Unused object type */
    NvMediaAudioAACObjectUnused = 0,
    /** \brief AAC Main object type */
    NvMediaAudioAACObjectMain,
    /** \brief AAC Low Complexity object type */
    NvMediaAudioAACObjectLC,
    /** \brief AAC Scalable Sample Rate object type */
    NvMediaAudioAACObjectSSR,
    /** \brief AAC Long Term Prediction object type */
    NvMediaAudioAACObjectLTP,
    /** \brief High Efficiency (HE) AAC Spectral Band Replication (SBR) object type */
    NvMediaAudioAACObjectHE,
    /** \brief AAC Scalable object type */
    NvMediaAudioAACObjectScalable,
    /** \brief Twin-VQ object type */
    NvMediaAudioAACObjectTwinVQ,
    /** \brief Code Excited Linear Prediction object type */
    NvMediaAudioAACObjectCELP,
    /** \brief Harmonic Vector eXcitation Coding object type */
    NvMediaAudioAACObjectHVXC,
    /** \hideinitializer \brief Text-To-Speech Interface object type */
    NvMediaAudioAACObjectTTSI = 12,
    /** \brief Main Synthesis object type */
    NvMediaAudioAACObjectMainSynthetic,
    /** \brief Wavetable Synthesis object type */
    NvMediaAudioAACObjectWavetableSynthesis,
    /** \brief General MIDI object type */
    NvMediaAudioAACObjectGeneralMIDI,
    /** \brief Algorithmic Synthesis And Audio Effects (FX) object type */
    NvMediaAudioAACObjectAlgorithmSynthesisAndAudioFX,
    /** \brief Error Resilient (ER) AAC Low Complexity object type */
    NvMediaAudioAACObjectERLC,
    /** \hideinitializer \brief ER-AAC Long Term Prediction object type */
    NvMediaAudioAACObjectERLTP = 19,
    /** \brief ER-AAC Scalable object type */
    NvMediaAudioAACObjectERScalable,
    /** \brief ER Twin-VQ object type */
    NvMediaAudioAACObjectERTwinVQ,
    /** \brief ER Bit-Sliced Arithmetic Coding object type */
    NvMediaAudioAACObjectERBSAC,
    /** \brief ER-AAC Low Delay object type */
    NvMediaAudioAACObjectLD,
    /** \brief ER Code Excited Linear Prediction object type */
    NvMediaAudioAACObjectERCELP,
    /** \brief ER Harmonic Vector eXcitation Coding object type */
    NvMediaAudioAACObjectERHVXC,
    /** \brief ER Harmonic and Individual Line plus Noise object type */
    NvMediaAudioAACObjectERHILN,
    /** \brief Parametric object type */
    NvMediaAudioAACObjectParametric,
    /** \hideinitializer \brief HE-AAC (v2) Parametric Stereo coding object type */
    NvMediaAudioAACObjectHE_PS = 29
} NvMediaAudioAACProfileType;

/**
 * \brief Audio decoder configuration for MP2 streams
 */
typedef struct {
    /** \brief Number of audio channels */
    unsigned int channels;
    /** \brief Input data bitrate */
    unsigned int bit_rate;
    /** \brief Sample rate in Hz */
    unsigned int sample_rate;
    /** \brief Stream format */
    NvMediaAudioMP2StreamFormatType format;
    /** \brief Channel mode */
    NvMediaAudioMP2ChannelModeType channel_mode;
} NvMediaAudioDecoderConfigMP2;

/**
 * \brief Audio decoder configuration for MP3 streams
 */
typedef struct {
    /** \brief Number of audio channels */
    unsigned int channels;
    /** \brief Input data bitrate */
    unsigned int bit_rate;
    /** \brief Sample rate in Hz */
    unsigned int sample_rate;
    /** \brief Stream format */
    NvMediaAudioMP3StreamFormatType format;
    /** \brief Channel mode */
    NvMediaAudioMP3ChannelModeType channel_mode;
} NvMediaAudioDecoderConfigMP3;

/**
 * \brief Audio decoder configuration for VORBIS streams
 */
typedef struct {
    /** \brief three Header(identification, comments and setup header) buffer pointer */
    char *pHeaderBuff[3];
    /** \brief three Header buffer size */
    unsigned int HeaderBufSize[3];
} NvMediaAudioDecoderConfigVORBIS;

/**
 * \brief WMA player information for WMA streams
 */
typedef struct {
    /** \brief Player options: HALFTRANSFORM = 0x0002, PAD2XTRANSFORM = 0x0008, DYNAMICRANGECOMPR = 0x0080, LTRT = 0x0100, IGNOREFREQEX = 0x0200, IGNORECX = 0x0400 */
    unsigned short int nPlayerOpt;
    /** \brief RGI mixdown matrix */
    int *rgiMixDownMatrix;
    /** \brief Peak value of reference amplitude */
    int iPeakAmplitudeRef;
    /** \brief RMS value of reference amplitude */
    int iRmsAmplitudeRef;
    /** \brief Peak value of target amplitude */
    int iPeakAmplitudeTarget;
    /** \brief RMS value of reference amplitude */
    int iRmsAmplitudeTarget;
    /** \brief Dynamic range control setting: high = 0, medium = 1, low = 2 */
    short int nDRCSetting;
    /** \brief Buffer size for left and right channels */
    int iLtRtBufsize;
    /** \brief Quality value for left and right channels */
    int iLtRtQuality;
} NvMediaAudioWMAPlayerInfo;

/**
 * \brief WMA format information for WMA streams
 */
typedef struct {
    /** \brief WMA format tag: WMAUDIO2_ES = 0x0165, WMAUDIO3_ES = 0x0166, WMAUDIO_LOSSLESS_ES = 0x0167 */
    unsigned short int wFormatTag;
    /** \brief Number of audio channels */
    unsigned short int nChannels;
    /** \brief Number of audio samples per second */
    unsigned int nSamplesPerSec;
    /** \brief Average number of bytes per second */
    unsigned int nAvgBytesPerSec;
    /** \brief Block alignment */
    unsigned short int nBlockAlign;
    /** \brief Size of an audio sample in bits */
    unsigned short int nValidBitsPerSample;
    /** \brief Channel mask */
    unsigned int nChannelMask;
    /** \brief Encoder options */
    unsigned short int wEncodeOpt;
    /** \brief Advanced encoder options */
    unsigned short int wAdvancedEncodeOpt;
    /** \brief  Advanced encoder options 2 */
    unsigned int dwAdvancedEncodeOpt2;
    /** \brief Stream has DRM content or not */
    NvMediaBool hasDRM;
    /** \brief DRM policy type */
    unsigned int policyType;
    /** \brief Metering time */
    unsigned int meteringTime;
    /** \brief License consume time */
    unsigned int licenseConsumeTime;
    /** \brief DRM context */
    unsigned int drmContext;
    /** \brief Core context */
    unsigned int coreContext;
} NvMediaAudioWMAFormatInfo;

/**
 * \brief PCM data type of decoded WMA streams
 */
typedef enum {
    /** \hideinitializer \brief PCM format */
    PCMDataType_PCM = 0,
    /** \brief IEE floating point format */
    PCMDataType_IEEE_FLOAT
} NvMediaAudioPCMDataType;

/**
 * \brief PCM format information for decoded WMA streams
 */
typedef struct {
    /** \brief Number of PCM samples per second */
    unsigned int nSamplesPerSec;
    /** \brief Number of audio channels */
    unsigned int nChannels;
    /** \brief Channel mask */
    unsigned int nChannelMask;
    /** \brief Size of a PCM sample in bits */
    unsigned int nValidBitsPerSample;
    /** \brief PCM container size */
    unsigned int cbPCMContainerSize;
    /** \brief PCM data type */
    NvMediaAudioPCMDataType pcmData;
} NvMediaAudioPCMFormatInfo;

/**
 * \brief WMA audio stream information
 */
typedef struct {
    /** \brief WMA player information */
    NvMediaAudioWMAPlayerInfo sPlayerInfo;
    /** \brief WMA format information for input stream */
    NvMediaAudioWMAFormatInfo sWmaFormat;
    /** \brief PCM format information for output stream */
    NvMediaAudioPCMFormatInfo sPcmFormat;
    /** \brief WMA stream format */
    unsigned short int wStreamType;
} NvMediaAudioWMAStreamInfo;

/**
 * \brief Audio decoder configuration for WMA streams
 */
typedef struct {
    /** \brief Stream format */
    NvMediaAudioWMAStreamFormatType eFormat;
    /** \brief WMA profile type */
    NvMediaAudioWMAProfileType eProfile;
    /** \brief Downmix or not */
    int Downmix;
    /** \brief Half-transform or not */
    int NoHalfTransform;
    /** \brief Superblock alignment */
    unsigned int nSuperBlockAlign;
    /** \brief Stream information */
    NvMediaAudioWMAStreamInfo WmaInfo;
} NvMediaAudioDecoderConfigWMA;

/**
 * \brief AAC tool usage (required for encoder configuration and optional as decoder information output)
 */
typedef enum {
    /** \hideinitializer \brief No AAC tools allowed (encoder configuration) or active (decoder information output) */
    NvMediaAudioAACToolNone = 0x00000000,
    /** \hideinitializer \brief Mid/Side (MS) joint coding tool allowed or active */
    NvMediaAudioAACToolMS   = 0x00000001,
    /** \hideinitializer \brief Intensity Stereo (IS) tool allowed or active */
    NvMediaAudioAACToolIS   = 0x00000002,
    /** \hideinitializer \brief Temporal Noise Shaping (TNS) tool allowed or active */
    NvMediaAudioAACToolTNS  = 0x00000004,
    /** \hideinitializer \brief MPEG-4 Perceptual Noise Substitution (PNS) tool allowed or active */
    NvMediaAudioAACToolPNS  = 0x00000008,
    /** \hideinitializer \brief MPEG-4 Long Term Prediction (LTP) tool allowed or active */
    NvMediaAudioAACToolLTP  = 0x00000010,
    /** \hideinitializer \brief All AAC tools allowed or active */
    NvMediaAudioAACToolAll  = 0x7FFFFFFF
} NvMediaAudioAACTools;

/**
 * \hideinitializer
 * \brief MPEG-4 Error-Resilient (ER) AAC tool usage (required for ER encoder configuration and optional as decoder information output)
 */
typedef enum {
    /** \hideinitializer \brief No ER-AAC tools allowed or used */
    NvMediaAudioAACERToolNone  = 0x00000000,
    /** \hideinitializer \brief Virtual Code Books (VCB11) for AAC section data allowed or used */
    NvMediaAudioAACERToolVCB11 = 0x00000001,
    /** \hideinitializer \brief Reversible Variable Length Coding (RVLC) allowed or used */
    NvMediaAudioAACERToolRVLC  = 0x00000002,
    /** \hideinitializer \brief Huffman Codeword Reordering (HCR) allowed or used */
    NvMediaAudioAACERToolHCR   = 0x00000004,
    /** \hideinitializer \brief All ER-AAC tools allowed or used */
    NvMediaAudioAACERToolAll   = 0x7FFFFFFF
}NvMediaAudioAACERTools;

/**
 * \brief AAC parameters
 */
typedef struct {
    /** \brief Number of audio channels */
    unsigned int nChannels;
    /** \brief Source data sampling rate (use 0 for variable or unknown sampling rate) */
    unsigned int nSampleRate;
    /** \brief Input data bitrate (use 0 for variable or unknown bitrate) */
    unsigned int nBitRate;
    /** \brief Audio bandwidth (in Hz) to which an encoder should limit the audio signal (use 0 to let encoder decide) */
    unsigned int nAudioBandWidth;
    /** \brief Frame length (in audio samples per channel) used by the codec; it can be 1024 or 960 (AAC-LC), 2048 (HE-AAC), 480 or 512 (AAC-LD) */
    unsigned int nFrameLength;
    /** \brief AAC tool usage */
    unsigned int nAACtools;
    /** \brief MPEG-4 Error-Resilient (ER) AAC tool usage */
    unsigned int nAACERtools;
    /** \brief AAC profile type */
    NvMediaAudioAACProfileType eAACProfile;
    /** \brief Stream format */
    NvMediaAudioAACStreamFormatType eAACStreamFormat;
    /** \brief Channel mode */
    NvMediaAudioAACChannelModeType eChannelMode;
} NvMediaAudioAACParams;

/**
 * \brief Audio track information for AAC streams
 */
typedef struct {
    /** \brief AAC object type */
    unsigned int objectType;
    /** \brief AAC profile type */
    unsigned int profile;
    /** \brief Source data sampling frequency index */
    unsigned int samplingFreqIndex;
    /** \brief Source data sampling frequency (use 0 for variable or unknown sampling frequencies) */
    unsigned int samplingFreq;
    /** \brief Number of audio channels */
    unsigned int noOfChannels;
    /** \brief Size of an audio sample in bits */
    unsigned int sampleSize;
    /** \brief Channel mode */
    unsigned int channelConfiguration;
    /** \brief Input data bitrate (use 0 for variable or unknown bitrate) */
    unsigned int bitRate;
    /** \brief Stream has DRM content or not */
    NvMediaBool HasDRM;
    /** \brief DRM policy type */
    unsigned int PolicyType;
    /** \brief Metering time */
    unsigned int MeteringTime;
    /** \brief License consume time */
    unsigned int LicenseConsumeTime;
    /** \brief DRM context */
    unsigned int DrmContext;
    /** \brief Core context */
    unsigned int CoreContext;
} NvMediaAudioAACTrackInfo;

/**
 * \brief Audio decoder configuration for AAC streams
 */
typedef struct {
    /** \brief AAC parameters */
    NvMediaAudioAACParams AACparams;
    /** \brief Audio track information */
    NvMediaAudioAACTrackInfo track_info;
    /** \brief Input buffers will have atleast one complete AAC frame
     *
     * If the flag is set to 1, decoder will assume partial frames in input and buffer appropriately
     * Flag is treated to be valid till next config
     */
    NvMediaBool EnablePartialFrameBuffering;
} NvMediaAudioDecoderConfigAAC;

/**
 * \brief A handle representing an audio decoder object.
 */
typedef struct {
    /** \brief Codec type */
    NvMediaAudioCodec codec;
} NvMediaAudioDecoder;

/** \brief Create an audio decoder object.
 *
 * Create a \ref NvMediaAudioDecoder object for the specified codec.  Each
 * decoder object may be accessed by a separate thread.  The object
 * is to be destroyed with \ref NvMediaAudioDecoderDestroy().
 *
 * \param[in] codec Codec type. The following are supported:
 * \n \ref NVMEDIA_AUDIO_CODEC_AAC
 * \n \ref NVMEDIA_AUDIO_CODEC_MP2
 * \n \ref NVMEDIA_AUDIO_CODEC_MP3
 * \n \ref NVMEDIA_AUDIO_CODEC_VORBIS
 * \n \ref NVMEDIA_AUDIO_CODEC_WMAPRO
 * \n \ref NVMEDIA_AUDIO_CODEC_WMA9STD
 * \n \ref NVMEDIA_AUDIO_CODEC_WMA9LSL
 * \return NvMediaAudioDecoder The new audio decoder's handle or NULL if unsuccesful.
 */
NvMediaAudioDecoder *
NvMediaAudioDecoderCreate(
    NvMediaAudioCodec codec
);

/** \brief Destroy an audio decoder object.
 * \param[in] decoder The decoder to be destroyed.
 * \return void
 */
void
NvMediaAudioDecoderDestroy(
   NvMediaAudioDecoder *decoder
);

/**
 * \brief Decode a compressed audio frame into the specifed output buffer.
 * \param[in] decoder The decoder object that will perform the
 *       decode operation.
 * \param[in] inBuffer Input buffer holding the compressed audio.
 * \param[in,out] inBufferSize Data size in inBuffer in bytes,
 *                returns amount of data bytes consumed by decoder from inBuffer
 * \param[in] outBuffer Buffer where the decoder places the uncompressed audio data.
 * \param[in,out] outBufferSize Input, the size of outBuffer in bytes. Output, if
 *                successful, size of the decoded data in bytes filled by the decoder.
 *                Output, if NvMediaStatus = NVMEDIA_STATUS_BAD_PARAMETER, expected
 *                mininum size of outBuffer in bytes.
 * \param[in] config A pointer to a structure containing
 *       information about the audio configuration the decoded frame. If the pointer
 *       is not NULL then the decoder fills the structure with configuration information
 *       for the decoder's type.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaAudioDecoderDecode(
    NvMediaAudioDecoder *decoder,
    void *inBuffer,
    unsigned int *inBufferSize,
    void *outBuffer,
    unsigned int *outBufferSize,
    NvMediaAudioConfigurationInfo *config
);

/** \brief Reset the decoder to it's initial state and flush out any pending bitstream.
 * \param[in] decoder The decoder to be flushed.
 * \return void
 */
void
NvMediaAudioDecoderFlush(
   NvMediaAudioDecoder *decoder
);

/**
 * \brief Retrieves the current audio decoder configuration.
 * \param[in] decoder Decoder object
 * \param[in] config A (pointer to a) structure containing
 *       information about the audio configuration. Note that
 *       the appropriate type of NvMediaAudioConfigurationInfo* structure must
 *       be provided to match the type that the decoder was
 *       created for.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaAudioDecoderConfigGet(
   NvMediaAudioDecoder *decoder,
   NvMediaAudioConfigurationInfo *config
);

/**
 * \brief Set the current audio decoder configuration.
 * \param[in] decoder Decoder object
 * \param[in] config A (pointer to a) structure containing
 *       information about the audio configuration. Note that
 *       the appropriate type of NvMediaAudioConfigurationInfo* structure must
 *       be provided to match the type that the decoder was
 *       created for.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 * \n \ref NVMEDIA_STATUS_NOT_SUPPORTED
 */
NvMediaStatus
NvMediaAudioDecoderConfigSet(
   NvMediaAudioDecoder *decoder,
   NvMediaAudioConfigurationInfo *config
);
/*@}*/

/**
 * \defgroup audioencoder_api Audio encoder
 *
 * The NvMediaAudioEncoder object encodes raw (pcm) audio data, writing
 * the results to the specified buffer.
 *
 * A specific NvMedia implementation may support encoding to multiple
 * types of compressed audio data. However, NvMediaAudioEncoder objects
 * are able to encode to a specific type of compressed audio data.
 * This type must be specified during creation.
 *
 * @{
 */

/**
 * \brief A generic "audio configuration" type.
 *
 * This type serves solely to document the expected usage of a
 * generic (void *) function parameter. In actual usage, the
 * application is expected to physically provide a pointer to an
 * instance of one of the "real" NvMediaAudioEncoderConfig* structures,
 * picking the type appropriate for the audio encoder object in
 * question.
 */

/**
 * \hideinitializer
 * \brief AAC encoding mode
 */
typedef enum {
    /** \hideinitializer \brief Low Complexity (AAC-LC) mode */
    NvMediaAudioAACEncoderMode_LC = 0,
    /** \brief Long Term Prediction (AAC-LTP) mode */
    NvMediaAudioAACEncoderMode_LTP,
    /** \brief High Efficiency (HE-AAC) mode */
    NvMediaAudioAACEncoderMode_PLUS,
    /** \brief High Efficiency version 2 (HE-AAC v2) mode  */
    NvMediaAudioAACEncoderMode_PLUS_ENHANCED,
    /** \hideinitializer \brief Enumeration boundary */
    NvMediaAudioAACEncoderMode_Force32 = 0x7FFFFFFF
} NvMediaAudioAACEncoderMode;

/**
 * \brief Audio encoder configuration for AAC output stream format
 */
typedef struct
{
    /** \brief Size of NvMediaAudioEncoderConfigAAC structure */
    unsigned int structSize;
    /** \brief Encoding mode */
    NvMediaAudioAACEncoderMode Mode;
    /** \brief Control for enabling/disabling CRC-check for error-resiliency */
    unsigned int CRCEnabled;
    /** \brief Input stream sampling rate in Hz */
    unsigned int SampleRate;
    /** \brief Input stream bitrate in bps */
    unsigned int BitRate;
    /** \brief Number of audio channels */
    unsigned int InputChannels;
    /** \brief Sample width of input audio */
    unsigned int InputSampleWidth;
    /** \brief Enable Temporal Noise Shaping (TNS) */
    unsigned int Tns_Mode;
    /** \brief Quantizer quality */
    unsigned int quantizerQuality;
    /** \brief Transport stream overhead in bits */
    unsigned int TsOverHeadBits;
} NvMediaAudioEncoderConfigAAC;

/**
 * \brief A handle representing an audio encoder object.
 */
typedef struct {
    /** \brief Codec type */
    NvMediaAudioCodec codec;
} NvMediaAudioEncoder;

/** \brief Create an audio encoder object.
 *
 * Create a \ref NvMediaAudioEncoder object for the specified codec.  Each
 * encoder object may be accessed by a separate thread.  The object
 * is to be destroyed with \ref NvMediaAudioEncoderDestroy().
 *
 * \param[in] codec Codec type. The following are supported:
 * \n \ref NVMEDIA_AUDIO_CODEC_AAC
 * \return NvMediaAudioEncoder The new audio encoder's handle or NULL if unsuccesful.
 */
NvMediaAudioEncoder *
NvMediaAudioEncoderCreate(
    NvMediaAudioCodec codec
);

/** \brief Destroy an audio encoder object.
 * \param[in] encoder The encoder to be destroyed.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaAudioEncoderDestroy(
   NvMediaAudioEncoder *encoder
);

/**
 * \brief Encode to a compressed audio frame in the specifed output buffer.
 * \param[in] encoder The encoder object that will perform the
 *       encode operation.
 * \param[in] inBuffer Input buffer holding the raw (pcm) audio.
 * \param[in] inBufferSize Size of the data in the input buffer in bytes.
 * \param[in] outBuffer Buffer where the encoder places the compressed audio data.
 * \param[in] outBufferSize Size of the encoded data in bytes filled by the encoder.
 * \param[in] config A pointer to a structure containing
 *       information about the audio configuration the encoded frame. If the pointer
 *       is not NULL then the encoder fills the structure with configuration information
 *       for the encoder's type.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaAudioEncoderEncode(
    NvMediaAudioEncoder *encoder,
    void *inBuffer,
    int *inBufferSize,
    void *outBuffer,
    int *outBufferSize,
    NvMediaAudioConfigurationInfo *config
);

/** \brief Reset the encoder to it's initial state and flush out any pending bitstream.
 * \param[in] encoder The encoder to be flushed.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaAudioEncoderFlush(
   NvMediaAudioEncoder *encoder
);

/**
 * \brief Retrieves the current audio encoder configuration.
 * \param[in] encoder Encoder object
 * \param[in] config A (pointer to a) structure containing
 *       information about the audio configuration. Note that
 *       the appropriate type of NvMediaAudioConfigurationInfo* structure must
 *       be provided to match the type that the encoder was
 *       created for.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaAudioEncoderConfigGet(
   NvMediaAudioEncoder *encoder,
   NvMediaAudioConfigurationInfo *config
);

/**
 * \brief Set the current audio encoder configuration.
 * \param[in] encoder Encoder object
 * \param[in] config A (pointer to a) structure containing
 *       information about the audio configuration. Note that
 *       the appropriate type of NvMediaAudioConfigurationInfo* structure must
 *       be provided to match the type that the encoder was
 *       created for.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 * \n \ref NVMEDIA_STATUS_NOT_SUPPORTED
 */
NvMediaStatus
NvMediaAudioEncoderConfigSet(
   NvMediaAudioEncoder *encoder,
   NvMediaAudioConfigurationInfo *config
);
/*@}*/

/**
 * \defgroup history_audio Audio History
 *
 * \section history_audio Audio API Version History
 *
 * <b> Version 1.7 </b> October 18, 2012
 * - Added EnablePartialFrameBuffering to \ref NvMediaAudioDecoderConfigAAC
 *
 * <b> Version 1.6 </b> June 26, 2012
 * - Split from nvmedia.h
 *
 * <b> Version 1.5 </b> May 24, 2012
 * - Added InputSampleWidth to \ref NvMediaAudioEncoderConfigAAC
 * - Changed return type of \ref NvMediaAudioEncoderFlush and \ref NvMediaAudioEncoderDestroy
 *
 * <b> Version 1.4 </b> February 20, 2012
 * - Added NVMEDIA_AUDIO_CODEC_WMA9LSL to \ref NvMediaAudioCodec enum
 * - Added \ref NvMediaAudioDecoderConfigWMA structure for all WMA profiles
 * - Added param description of \ref NvMediaAudioDecoderCreate API
 * - Removed NvMediaAudioDecoderConfigWMA9Std structure
 * - Removed NvMediaAudioDecoderConfigWMa9Lsl structure
 *
 * <b> Version 1.3 </b> January 30, 2012
 * - Added NVMEDIA_AUDIO_CODEC_MP2 to \ref NvMediaAudioCodec enum
 * - Added \ref NvMediaAudioDecoderConfigMP2 structure
 * - Added \ref NvMediaAudioMP2StreamFormatType structure
 * - Added \ref NvMediaAudioMP2ChannelModeType structure
 *
 * <b> Version 1.2 </b> December 16, 2011
 * - Added VORBIS decoding support
 * - Added NVMEDIA_AUDIO_CODEC_VORBIS to \ref NvMediaAudioCodec enum
 * - Added \ref NvMediaAudioDecoderConfigVORBIS structure
 *
 * <b> Version 1.1 </b> October 6, 2011
 * - Added Audio Encoder API definitions
 *
 * <b> Version 1.0 </b> May 25, 2011
 * - Added Audio Decoder API definitions
 *
 */
/*@}*/
/*@}*/

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* _NVMEDIA_AUDIO_H */
