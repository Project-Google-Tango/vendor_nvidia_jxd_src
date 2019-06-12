/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Audio Codec Interface</b>
 *
 * @b Description: Defines the interface to the audio codec controller.
 */

#ifndef INCLUDED_NVODM_AUDIOCODEC_H
#define INCLUDED_NVODM_AUDIOCODEC_H

/**
 * @defgroup nvodm_audiocodec Audio Codec ODM Driver API
 * The audio codec driver has two types of ports:
 * - Input port--analog input signals are converted to digital
 * - Output port--digital signals are converted to analog
 *
 * There may be multiple input and output ports inside the audio codec.
 *
 * <b>Input Ports</b>
 *
 * Input ports are responsible for converting analog signals to digital
 * format. There are many input analog lines to the input (ADC), like
 * line in, mic, and digital output that can be routed to a different
 * digital path.
 *
 * There are diffrent configurations for the input port, such as:
 * - Signal cofigurations to select the input and output path
 * - PCM configurations to select the sampling rate and PCM size
 * - Configurations for gain/boost level to boost the input signal
 * - Configurations for side tone attenuation to suppress noise.
 *
 * <b>Output Port</b>
 *
 * The output port is responsible for converting all digital signals
 * to analog format. There are many digital input paths to the output
 * (DAC) and many analog outputs, like line out and headphone out. There
 * are diffrent configurations for the DAC port, such as:
 * - Signal cofiguration to select the input and output path
 * - PCM configuration to select the sampling rate and PCM size
 * - Volume level to control the volume and short circut current limitor
 *
 * This is the interface to control and configure the audio codec.
 * @ingroup nvodm_adaptation
 * @{
 */

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** Defines opaque registers for clients. */
typedef struct NvOdmAudioCodecRec *NvOdmAudioCodecHandle;

/**
 * Defines the signal name for the audio codec. This includes the signal
 * name and its identifier. For example, the signal names are LineIn,
 * LineOut, HeadPhone, Mic, and digital signals like I2S, etc.
 */
#define NVODMAUDIO_SIGNALNAME(SignalType, SignalId) (((SignalType) << 16) | (SignalId))

/**
 * Defines the port name of the audio codec. The ports are
 * ADC and DAC with corrsponding ID. @see NvOdmAudioPortName, NvOdmAudioPortType
 */
#define NVODMAUDIO_PORTNAME(PortType, PortId) (((PortType) << 16) | (PortId))

/**
 * Extract ::NvOdmAudioPortType from a 32-bit word of type ::NvOdmAudioPortName.
 */
#define NVODMAUDIO_GET_PORT_TYPE(PortName) ((NvOdmAudioPortType)((PortName) >> 16))

/**
 * Extract port ID from a 32-bit word of type ::NvOdmAudioPortName.
 */
#define NVODMAUDIO_GET_PORT_ID(PortName) ((PortName) & 0xFFFFU)

/**
 * Defines the ::NvOdmSignal port name, which includes the ::NvOdmAudioPortType in high
 * and port ID in low 16-bit.
 * @see NVODMAUDIO_PORTNAME, NVODMAUDIO_GET_PORT_TYPE, NVODMAUDIO_GET_PORT_ID
 */
typedef NvU32 NvOdmAudioPortName;

/**
 * Defines the different types of signals in the audio codec.
 */
typedef NvU32 NvOdmAudioSignalType;

/// Specifies the signal name to none.
#define NvOdmAudioSignalType_None                   0x00000000

/// Specifies the signal name to line in.
#define NvOdmAudioSignalType_LineIn                 0x00000001

/// Specifies the signal name to mic in.
#define NvOdmAudioSignalType_MicIn                  0x00000002

/// Specifies the signal name to the CD.
#define NvOdmAudioSignalType_Cd                     0x00000004

/// Specifies the signal name to video.
#define NvOdmAudioSignalType_Video                  0x00000008

/// Specifies the signal name to phone in, which is mono type.
#define NvOdmAudioSignalType_PhoneIn                0x00000010

/// Specifies the signal name to auxiliary.
#define NvOdmAudioSignalType_Aux                    0x00000020

/// Specifies the signal name to line out.
#define NvOdmAudioSignalType_LineOut                0x00000040

/// Specifies the signal name to headphone.
#define NvOdmAudioSignalType_HeadphoneOut           0x00000080

/// Specifies the signal name to speaker output.
#define NvOdmAudioSignalType_Speakers               0x00000100

/// Specifies the signal type to the PCM format.
#define NvOdmAudioSignalType_Digital_Pcm            0x00000200

/// Specifies the signal name to I2S digital signal.
#define NvOdmAudioSignalType_Digital_I2s            0x00000400

/// Specifies the signal name to S/PDIF digital signal.
#define NvOdmAudioSignalType_Digital_Spdif          0x00000800

/// Specifies the signal name to AC97 digital signal.
#define NvOdmAudioSignalType_Digital_AcLink         0x00001000

/// Specifies the signal name to Rxn analog  signal.
#define NvOdmAudioSignalType_Analog_Rxn             0x00002000

/// Specifies the signal name to the Bluetooth signal.
#define NvOdmAudioSignalType_Digital_BlueTooth      0x00004000

/// Specifies the signal name to the baseband signal.
#define NvOdmAudioSignalType_Digital_BaseBand       0x00008000

/// Specifies the signal name to the analog In signal.
#define NvOdmAudioSignalType_AnalogIn               0x00010000

/// Specifies the signal name to the mono out.
#define NvOdmAudioSignalType_MonoOut                0x00020000




/**
 * Defines the port types for the audio codec.
 */
typedef enum
{
    /// Specifies port to the input (ADC) port.
    NvOdmAudioPortType_Input = 0x1,

    /// Specifies port to the output (DAC) port.
    NvOdmAudioPortType_Output,

    NvOdmAudioPortType_Force32 = 0x7FFFFFFF
} NvOdmAudioPortType;



/**
 * Defines the audio channel types.
 */
typedef NvU32 NvOdmAudioSpeakerType;

/// Specifies a codec channel to none or unused.
#define NvOdmAudioSpeakerType_None                  0x00000000

/// Specifies a front left channel.
#define NvOdmAudioSpeakerType_FrontLeft             0x00000001

/// Specifies a front right channel.
#define NvOdmAudioSpeakerType_FrontRight            0x00000002

/// Specifies a front center channel.
#define NvOdmAudioSpeakerType_FrontCenter           0x00000004

/// Specifies a front low frequency channel.
#define NvOdmAudioSpeakerType_LowFrequency          0x00000008

/// Specifies a back left channel.
#define NvOdmAudioSpeakerType_BackLeft              0x00000010

/// Specifies a back right channel.
#define NvOdmAudioSpeakerType_BackRight             0x00000020

/// Specifies a front left-of-center channel.
#define NvOdmAudioSpeakerType_FrontLeftOfCenter     0x00000040

/// Specifies a front right-of-center channel.
#define NvOdmAudioSpeakerType_FrontRightOfCenter    0x00000080

/// Specifies a back center channel.
#define NvOdmAudioSpeakerType_BackCenter            0x00000100

/// Specifies a side left channel.
#define NvOdmAudioSpeakerType_SideLeft              0x00000200

/// Specifies a side right channel.
#define NvOdmAudioSpeakerType_SideRight             0x00000400

/// Specifies a top center channel.
#define NvOdmAudioSpeakerType_TopCenter             0x00000800

/// Specifies a top left channel.
#define NvOdmAudioSpeakerType_TopFrontLeft          0x00001000

/// Specifies a top front center channel.
#define NvOdmAudioSpeakerType_TopFrontCenter        0x00002000

/// Specifies a top front right channel.
#define NvOdmAudioSpeakerType_TopFrontRight         0x00004000

/// Specifies a top back left channel.
#define NvOdmAudioSpeakerType_TopBackLeft           0x00008000

/// Specifies a top back center channel.
#define NvOdmAudioSpeakerType_TopBackCenter         0x00010000

/// Specifies a top back right channel.
#define NvOdmAudioSpeakerType_TopBackRight          0x00020000

/// Specifies all possible channels.
#define NvOdmAudioSpeakerType_All                   0xFFFFFFFF


/**
 * Defines the audio wave format.
 */
typedef enum
{
    /// Specifies linear pulse code modulation (PCM) encoded data.
    NvOdmAudioWaveFormat_Pcm = 0x0,

    /// Specifies IEEE floating format.
    NvOdmAudioWaveFormat_IeeeFloat,

    /// Specifies DRM format.
    NvOdmAudioWaveFormat_Drm,

    /// Specifies adaptive differential PCM (ADPCM) format.
    NvOdmAudioWaveFormat_AdPcm,

    /// Specifies a-law PCM encoded data.
    NvOdmAudioWaveFormat_ALaw,

    /// Specifies mu-law PCM encoded data.
    NvOdmAudioWaveFormat_MuLaw,

    NvOdmAudioWaveFormat_Force32 = 0x7FFFFFFF
} NvOdmAudioWaveFormat;


/**
 * Defines the audio wave parameter for the port.
 */
typedef struct
{
    /// Specifies the number of channels, such as 2 for stereo.
    NvU32 NumberOfChannels;

    /// Indicates whether or not the PCM data is signed.
    NvBool IsSignedData;

    /// Indicates whether or not PCM data is little endian.
    NvBool IsLittleEndian;

    /// Indicates whether or not the PCM data is interleaved.
    /// NV_TRUE specifies normal interleaved data, NV_FALSE
    /// non-interleaved data such as block data.
    NvBool IsInterleaved;

    /// Specifies the number of bits per sample, such as 8 or 16 bits.
    NvU32 NumberOfBitsPerSample;

    /// Specifies the sampling rate in Hz, such as 8000 for 8 kHz, 44100
    /// for 44.1 kHz.
    NvU32 SamplingRateInHz;

    /// Specifies the expected BCLK data bits per LRCLK with codec as slave.
    NvU32 DatabitsPerLRCLK;

    /// Specifies the I2S data format, see ::NvOdmQueryI2sDataCommFormat for possible values.
    NvU32 DataFormat;

    /// Specifies the wave format mode.
    NvOdmAudioWaveFormat FormatType;

} NvOdmAudioWave;

/**
 * Defines the structure to the volume/gain/boost.
 */
typedef struct
{

    /// Specifies the signal type for which volume/gain is selected.
    NvOdmAudioSignalType SignalType;

    /// Specifies the signalID
    NvU32 SignalId;

    /// Specifies the Channel/Speaker ID, see ::NvOdmAudioSpeakerType.
    NvU32 ChannelMask;

    /// Indicates whether or not volume is to be set using a linear
    /// (0-100) scale, rather than a logarithmic (mB) scale
    /// (millibels = 1/100 dB).
    /// NV_TRUE specifies linear.
    NvBool IsLinear;

    /// Specifies the volume linear setting in the 0-100 range, or
    /// the volume logarithmic setting for this port. The values
    /// for volume are in mB relative to a gain of 1 (e.g. the output
    /// is the same as the input level). Values are in mB from
    /// MaxVolumeInMilliBel (maximum volume) to MinVolumeInMilliBel mB
    /// (typically negative). If a component cannot accurately set the
    /// volume to the requested value, it must set the volume to the
    /// closest value @b below the requested value. When getting the
    /// volume setting, the current actual volume must be returned.
    NvS32 VolumeLevel;
} NvOdmAudioVolume;

/**
 * Defines the mute parameters of the channel.
 */
typedef struct
{
    /// Specifies the signal type.
    NvOdmAudioSignalType  SignalType;

    /// Specifies the signal ID
    NvU32 SignalId;

    /// Specifies the Channel/Speaker ID, see ::NvOdmAudioSpeakerType.
    NvU32 ChannelMask;

    /// Indicates whether or not the channel is muted.
    NvBool IsMute;

} NvOdmAudioMuteData;


/**
 * Defines the different power modes of the audio codec.
 */
typedef enum
{
    /// Specifies the audio codec is in normal functional mode.
    /// The codec driver is expected to be in this state after NvOdmAudioCodecOpen().
    NvOdmAudioCodecPowerMode_Normal = 1,

    /// Specifies the audio codec is in the standby mode.
    NvOdmAudioCodecPowerMode_Standby,

    /// Specifies the audio codec is in the shutdown mode.
    NvOdmAudioCodecPowerMode_Shutdown,

    NvOdmAudioCodecPowerMode_Force32 = 0x7FFFFFFF
} NvOdmAudioCodecPowerMode;


/**
 * Defines the different attributes of the audio codec.
 */
typedef enum
{
    /// Specifies the initialize value.
    NvOdmAudioConfigure_None = 0x0,

    /// Specifies the PCM configuartion, like the sampling rate,
    /// bits per sample, etc.
    NvOdmAudioConfigure_Pcm,

    /// Specifies the bypass configuration of the audio codec.
    /// Takes parameter struct of type ::NvOdmAudioConfigBypass.
    NvOdmAudioConfigure_ByPass,

    /// Specifies the side tone attenuation configuration of the
    /// audio codec.
    /// Takes parameter struct of type ::NvOdmAudioConfigSideToneAttn.
    NvOdmAudioConfigure_SideToneAtt,

    /// Specifies the short circuit detection configuration of the
    /// audio codec.
    /// Takes parameter struct of type ::NvOdmAudioConfigShortCircuit.
    NvOdmAudioConfigure_ShortCircuit,

    /// Specifies to select the input and output signals of the given
    /// port. This selects the input to the port and output from the
    /// port, like for ADC line 1 to the I2S 2 line.
    /// Takes parameter struct of type ::NvOdmAudioConfigInOutSignal.
    NvOdmAudioConfigure_InOutSignal,

    /// Set or get ODM specific parameters.
    /// Implementation is ODM driver implementation specific.
    /// Takes parameter struct of type ::NvOdmAudioConfigODM.
    NvOdmAudioConfigure_ODM,

    /// Specifies a callback to signal dynamic IO changes.
    NvOdmAudioConfigure_InOutSignalCallback,

    /// Specifies to configure voice dac
    NvOdmAudioConfigure_Vdac,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmAudioConfigure_Force32 = 0x7FFFFFFF
} NvOdmAudioConfigure;

/**
 * Defines the bypass configuratiion of the audio codec.
 */
typedef struct
{
    /// Specifies to enable or disable control.
    NvBool IsEnable;

    /// Specifies the input signal type.
    NvOdmAudioSignalType InSignalType;

    /// Specifies the the input signal ID.
    NvU32 InSignalId;

    /// Specifies the output signal type.
    NvOdmAudioSignalType OutSignalType;

    /// Specifies the output signal ID.
    NvU32 OutSignalId;

} NvOdmAudioConfigBypass, NvOdmAudioConfigInOutSignal;

/**
 * Defines the side tone attenuation configuratiion of the audio codec.
 */
typedef struct
{
    /// Specifies whether or not to enable side tone attenuation.
    NvBool IsEnable;

    /// Specifies the signal type.
    NvOdmAudioSignalType SignalType;

    /// Specifies the signal ID on which side tone attenuation is to
    /// be applied.
    NvU32 SignalId;

    /// Specifies the side tone attenuation value.
    NvU32 SideToneAttnValue;

} NvOdmAudioConfigSideToneAttn;

/**
 * Defines the short circuit configuration of the audio codec.
 */
typedef struct
{
    /// Specifies whether or not to enable short circuit.
    NvBool IsEnable;

    /// Specifies the signal type.
    NvOdmAudioSignalType SignalType;

    /// Specifies the signal ID on which to apply short circuit
    /// detection.
    NvU32 SignalId;

    /// Specifies the short circuit current limit.
    NvU32 ShortCircuitCurrentLimit;

    /// Specifies whether or not the short circuit current is detected.
    NvBool IsShortCircuitDetected;

} NvOdmAudioConfigShortCircuit;

/**
 * Defines the ODM function-specific parameters of the audio codec.
 */
typedef struct
{
    /// Byte size of block referenced by pData.
    ///
    /// For NvOdmAudioCodecSetConfiguration() this is the size of the passed in data.
    ///
    /// For NvOdmAudioCodecGetConfiguration() on entry this is the maximum size of the buffer to receive data,
    /// on exit it should be updated with the number of bytes written, or 0 in case of failure.
    NvU32 Size;

    /// ODM function specific data.
    /// Interpreted by ODM driver implementation. Buffer is owned by the
    /// calling context and valid only for the duration of the call of
    /// NvOdmAudioCodecSetConfiguration() / NvOdmAudioCodecGetConfiguration().
    ///
    /// For NvOdmAudioCodecGetConfiguration() on entry this buffer can hold some input data,
    /// such as which type of configuration to get.
    NvU8* pData;

} NvOdmAudioConfigODM;

/**
 * Defines the callback prototype for dynamic signal changes.
 */
typedef void (*NvOdmAudioConfigInOutSignalFxn)(void* pContext, NvOdmAudioConfigInOutSignal* Signal);

/**
 * Defines the structure used to register the callback
 * interface.
 */
typedef struct
{
    /// Specifies the function to call on an IO signal change.
    NvOdmAudioConfigInOutSignalFxn Callback;

    /// Specifies the context to pass the callback on an IO signal change.
    void* pContext;

} NvOdmAudioConfigInOutSignalCallback;

/**
 * Defines the ODM function-specific parameters of the audio codec.
 */
typedef struct
{
    /// Byte size of block referenced by pData.
    ///
    /// For NvOdmAudioCodecSetConfiguration() this is the size of the passed in data.
    ///
    /// For NvOdmAudioCodecGetConfiguration() on entry this is the maximum size of the buffer to receive data,
    /// on exit it should be updated with the number of bytes written, or 0 in case of failure.
    NvU32 Size;

    /// ODM function specific data.
    /// Interpreted by ODM driver implementation. Buffer is owned by the
    /// calling context and valid only for the duration of the call of
    /// NvOdmAudioCodecSetConfiguration() / NvOdmAudioCodecGetConfiguration().
    ///
    /// For NvOdmAudioCodecGetConfiguration() on entry this buffer can hold some input data,
    /// such as which type of configuration to get.
    NvU8* pData;

} NvOdmAudioConfigNotify;


/**
 * Defines the structure for holding the maximum number of
 * ADC and DAC ports inside the audio codec.
 */
typedef struct
{
    /// Holds the maximum number of input ports (ADC) supported
    /// by the codec.
    NvU32 MaxNumberOfInputPort;

    /// Holds the maximum number of output ports (DAC) supported
    /// by the codec.
    NvU32 MaxNumberOfOutputPort;
} NvOdmAudioPortCaps;

/**
 * Defines the structure for holding the capabilty on the ADC port.
 */
typedef struct
{
    /// Holds the maximum number of line ins supported by the codec.
    NvU32 MaxNumberOfLineIn;

    /// Holds the maximum number of microphone line ins supported by
    /// the codec.
    NvU32 MaxNumberOfMicIn;

    /// Holds the maximum number of CD ins supported by the codec.
    NvU32 MaxNumberOfCdIn;

    /// Holds the maximum number of video ins supported by the codec.
    NvU32 MaxNumberOfVideoIn;

    /// Holds the maxmimum number of the mono type input signals.
    NvU32 MaxNumberOfMonoInput;

    /// Holds the maximum number of the outputs.
    NvU32 MaxNumberOfOutput;

    /// Holds a value indicating whether or not side tone attenuation
    /// is supported.
    NvBool IsSideToneAttSupported;

    /// Holds a value indicating whether or not the nonlinear gain
    /// control is supported.
    NvBool IsNonLinerGainSupported;

    /// Holds the maximum gain in millibel (mB) if nonlinear volume
    /// control is supported.
    NvS32 MaxVolumeInMilliBel;

    /// Holds the minimum gain in mB if nonlinear volume control
    /// is supported.
    NvS32 MinVolumeInMilliBel;
}  NvOdmAudioInputPortCaps;

/**
 * Defines the structure for holding the capabilty on the DAC port.
 */
typedef struct
{
    /// Holds the maximum number of the digital inputs.
    NvU32 MaxNumberOfDigitalInput;

    /// Holds the maximum number of line outs supported by the codec.
    NvU32 MaxNumberOfLineOut;

    /// Holds the maximum number of headphone outs supported the
    /// by codec.
    NvU32 MaxNumberOfHeadphoneOut;

    /// Holds the maximum number for speakers per output.
    NvU32 MaxNumberOfSpeakers;

    /// Holds a value indicating whether or not short circuit
    /// current limit is supported.
    NvBool IsShortCircuitCurrLimitSupported;

    /// Holds a value indicating whether or not the nonlinear volume
    /// control is supported.
    NvBool IsNonLinerVolumeSupported;

    /// Holds the maximum volume in millibel (mB) if nonlinear volume
    /// control is supported.
    NvS32 MaxVolumeInMilliBel;

    /// Holds the minimum volume in mB if nonlinear volume control
    /// is supported.
    NvS32 MinVolumeInMilliBel;
} NvOdmAudioOutputPortCaps;

/**
 * Gets the audio codec capabilities.
 * This tells the maxmimum number of the ADC and DAC ports available
 * in the codec.
 *
 * @param InstanceId The audio codec instance ID for which capabilities
 * are required.
 *
 * @return A pointer to the returned structure holding the audio codec
 * capabilities.
 */
const NvOdmAudioPortCaps * NvOdmAudioCodecGetPortCaps(NvU32 InstanceId);

/**
 * Gets the output port capabilities. This function returns the capability
 *  of the DAC port in the audio codec.
 *
 * @param ACInstanceId The audio codec instance ID for which capabilities are
 * required.
 * @param OutputPortId The output port ID.
 *
 * @return A pointer to the returned structure holding the output (DAC)
 * capabilities.
 */
const NvOdmAudioOutputPortCaps *
NvOdmAudioCodecGetOutputPortCaps(
    NvU32 ACInstanceId,
    NvU32 OutputPortId);

/**
 * Gets the input port capabilities. This function returns the capability
 * of the input (ADC) port in the audio codec.
 *
 * @param ACInstanceId The audio codec instance ID for which capabilities are
 * required.
 * @param InputPortId The input port ID of the audio codec.
 *
 * @return A pointer to the returned structure holding the input (ADC)
 * capabilities.
 */
const NvOdmAudioInputPortCaps *
NvOdmAudioCodecGetInputPortCaps(
    NvU32 ACInstanceId,
    NvU32 InputPortId);

/**
 * Opens the audio codec. This function allocates the handle for
 * the audio codec. The allocated handle should be passed when calling
 * other audio codec APIs.
 *
 * The codec device should be powered up and put into the
 * ::NvOdmAudioCodecPowerMode_Normal state.
 *
 * @see NvOdmAudioCodecClose
 *
 * @param InstanceId The audio codec instance ID.
 * @param hCodecNotifySem A pointer to the semaphore handle to signal
 * when there is a device change notification, like headphone detection, mic, etc.
 *
 * @return The audio codec handle.
 */
NvOdmAudioCodecHandle NvOdmAudioCodecOpen(NvU32 InstanceId, void* hCodecNotifySem);

/**
 * @brief Releases the audio codec handle.
 *
 * @pre NvOdmAudioCodecOpen()
 *
 * @post This frees the allocated handle, so clients must call
 * \c NvOdmAudioCodecOpen again to use the audio codec API.
 *
 * @param hAudioCodec The audio codec handle to release. If
 *     NULL, this API has no effect.
 */
void NvOdmAudioCodecClose(NvOdmAudioCodecHandle hAudioCodec);

/**
 * Gets the list of the port PCM properties, which holds the different
 * digital signal properties.
 *
 * With this function, clients can get the different sampling rates and
 * PCM sizes supported by this port.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PortName The port name, which is comprised of the port type
 * and port ID.
 * @param pValidListCount A pointer to the variable holding the valid
 * list in the list array.
 * @return A pointer to the list of the structure containing the
 * supported properties for the port.
 */
const NvOdmAudioWave *
NvOdmAudioCodecGetPortPcmCaps(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvU32 *pValidListCount);

/**
 * Sets the PCM property of the port.
 * With this function, clients can set the different sampling rate
 * and PCM size for this port.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PortName The port name, which is comprised of the port type and
 * port ID.
 * @param pWaveParams A pointer to the structure containing the property
 * of the PCM samples, like bits per sample, sampling rate, etc.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmAudioCodecSetPortPcmProps(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioWave *pWaveParams);


/**
 * @brief  Sets the volume/gain for requested channel of the given port.
 * This controls the analog line's volume/gain.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PortName The port name, which is comprised of the port type and
 * port ID.
 * @param pVolume A pointer to the structure containing the volume to set.
 * @param ListCount The number of the element in the list.
 */
void
NvOdmAudioCodecSetVolume(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioVolume *pVolume,
    NvU32 ListCount);

/**
 * Mutes/unmutes the selected devices and its selected channels.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PortName The port name, which is comprised of the port type and
 * port ID.
 * @param pMuteParam A pointer to the structure that contains the mute
 * parameter.
 * @param ListCount The number of the element in the list.
 */
 void
 NvOdmAudioCodecSetMuteControl(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioMuteData *pMuteParam,
    NvU32 ListCount);

/**
 * Set the different audio codec configurations, like the bypass,
 * short circuit current, or side tone attenuation on different line.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PortName The port name, which is comprised of the port type
 * and port ID.
 * @param ConfigType The configuration type, like bypass, sidetone
 * attenuation, or short circuit current detection.
 * @param pConfigData A pointer to the structure containing the related
 * configuration parameter.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
 NvBool
 NvOdmAudioCodecSetConfiguration(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData);

/**
 * Gets the different audio codec configurations, like the bypass,
 * short circuit current, side tone attenuation on different line,
 * and input/output signals.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PortName The port name, which is comprised of the port type
 * and port ID.
 * @param ConfigType The configuration type, like bypass, sidetone
 * attenuation, or short circuit current detection.
 * @param pConfigData A pointer to the returned structure containing
 * the related parameters.
 */
void
 NvOdmAudioCodecGetConfiguration(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData);

/**
 * @brief  Sets the power state to on or off for the ports.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PortName The port name, which is comprised of the port type and
 * port ID.
 * @param SignalType The signal type which contains the input or output
 * to/from the ADC/DAC.
 * @param SignalId The ID of the corresponding line.
 * @param IsPowerOn Powers on or off the module.
 * NV_TRUE powers on, otherwise it is powered off.
 */
void
NvOdmAudioCodecSetPowerState(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn);

/**
 * Sets the power mode of the audio codec, such as standby, shutdown,
 * or normal mode.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PowerMode The power mode to set.
 */
void
NvOdmAudioCodecSetPowerMode(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioCodecPowerMode PowerMode);

/**
 * Sets the codec mode as master or slave.
 *
 * @param hAudioCodec The audio codec handle allocated by the audio codec
 * driver in a call to NvOdmAudioCodecOpen().
 * @param PortName The port name, which is comprised of the port type and
 * port ID.
 * @param IsMaster Specify NV_TRUE to set the codec as master.
 */
void
NvOdmAudioCodecSetOperationMode(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvBool IsMaster);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif  // INCLUDED_NVODM_AUDIOCODEC_H
