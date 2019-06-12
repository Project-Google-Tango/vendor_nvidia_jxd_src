/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Capture API</b>
 *
 * @b Description: This is the NVIDIA interface for capturing system
 * video/audio, encoding and multiplexing it, and then returning the resulting
 * transport stream packets to the client.
 *
 */

#ifndef NV_CAP_API_H
#define NV_CAP_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* NVCAP_CTX;

/**
 * @defgroup nvdisp_cap_group Capture Interface
 *

 * NvCap is a capture library that captures the system audio data
 *     from the device, as well as the screen contents being rendered on the
 *     device. The library multiplexes the audio and video data and provides
 *     it to the NvCap client application via a set of API functions; these
 *     are described in more detail later in this section.
 *
 * This section introduces the NVIDIA Capture library (NvCap). It provides
 *     a description of the high level architecture and usage model, as well
 *     as details about the APIs exposed by the library.
 *
 * <b>Section Overview</b>
 *
 * The following topics discuss the capture library architecture, usage flow,
 *     how to build NvCap, its current limitations, followed by API usage
 *     details:
 * - <a href="#nvcap_arch" class="el">Architecture</a>
 * - <a href="#nvcap_security" class="el">Security</a>
 * - <a href="#nvcap_usage" class="el">Expected Usage and Flow</a>
 * - <a href="#nvcap_build" class="el">Building NvCap</a>
 * - <a href="#nvcap_issues" class="el">Limitations</a>
 * - <a href="#nvcap_api" class="el">API Documentation</a>
 *
 * <a name="nvcap_arch"></a><h2>Architecture</h2>
 *
 * The following diagram shows the high-level architecture of the NVIDIA
 *     capture library.
 *
 * \image html api_nvcap_arch.jpg ""
 *
 * As illustrated in the diagram, the system audio data is routed from libaudio
 *     to the capture library via a shared buffer. The capture library reads
 *     this data and hands it over to the MPEG-2 TS multiplexer. The contents
 *     of the screen, which are accessible in gralloc, are routed to the H.264
 *     video encoder, and sent to NvCap via a socket connection. The contents,
 *     in turn, are fed to the MPEG-2 TS multiplexer.
 *
 * The multiplexer requests output buffers from the client, and so the client
 *     must perform all the buffer management of the TS buffers. When the
 *     multiplexer has data available, it writes the data to a buffer that it
 *     had previously acquired from the client. The multiplexer then sends
 *     the buffer back to the client via a callback.
 *
 * <a name="nvcap_security"></a><h2>Security</h2>
 *
 * Due to the inherent risks of exposing the framebuffer, NvCap is secured
 * using a default Android application permission:
 * - READ_FRAME_BUFFER
 *
 * This permission is enforced in Android code and can be given only
 * to applications that are:
 * - signed by the same key as the image that is on the device, or are
 * - run with the "system" UID.
 *
 * For applications that fail this permissions check,
 * the functions in this module return the ::NvCapStatus_PermissionsFailure.
 * This return signifies the lack of permissions to access the Frame Buffer.
 *
 * To ensure your application complies with the security required by the NvCap
 * interface, one of the following lines must be present in your application's
 * <tt>AndroidManifest.xml</tt>.
 *
 * Grant application "system" privileges by adding the following
 * attribute to the \c manifest element:
 *
 * <pre>
 *     android:sharedUserId="android.uid.system"
 * </pre>
 *
 * Grant application READ_FRAME_BUFFER permissions by including:
 * <pre>
 *     <uses-permission android:name="android.permission.READ_FRAME_BUFFER" />
 * </pre>
 *
 * For more information, see the the Android Developers website:
 * - <a href=
 *    "http://developer.android.com/reference/android/Manifest.permission.html#READ_FRAME_BUFFER"
 *    target="_blank">READ_FRAME_BUFFER</a>
 * - <a href=
 *    "http://developer.android.com/guide/topics/manifest/manifest-element.html#uid"
 *    target="_blank">android:sharedUserId</a>
 *
 * <a name="nvcap_usage"></a><h2>Expected Usage and Flow</h2>
 *
 * -# The client application initializes the capture library using
 *    NvCapInit(). It then provides the capture library with callback functions
 *    for getting and returning buffers and for event notifications. The library
 *    provides the client with a context handle that must be used in all
 *    subsequent API calls.
 * -# The client determines the capabilities of the source device using
 *    the NvCapGetCaps() function.
 * -# Depending on how the source must be configured, the client makes
 *    the required NvCapSetAttribute() calls (each call sets a particular
 *    attribute).
 * -# Once all the setup is complete, the client calls NvCapStart()
 *    to start the capture. When the NvCap library has data to return
 *    to the client, it requests a buffer from the client using the provided
 *    NvCapCallbackInfo::pfnGetBuffersCB callback.
 *     -# If there is no buffer available on the client side (in the event that
 *       there is back-pressure from the network stack), this call blocks.
 *     -# When the capture library has a buffer to which it can write, it fills
 *       the required data into this buffer, and returns it to the client using
 *       the asynchronous NvCapCallbackInfo::pfnSendBuffersCB callback. Any
 *       out-of-band events are signaled via the asynchronous
 *       NvCapCallbackInfo::pfnNotifyEventCB callback (currently this is not
 *       used).
 *
 * Clients can use the NvCapPause() function to pause the session if required.
 *     To resume the session, the client can call NvCapStart(). Once the client
 *     is done with the Wi-Fi Display session, it can tear down the capture
 *     library by using the NvCapRelease() function. The NvCap context can no
 *     longer be used after an NvCapRelease() has been called on it. To start
 *     capturing again, the client must repeat the set of steps outlined above,
 *     starting with another NvCapInit().
 *
 * <a name="nvcap_build"></a><h2>Building NvCap</h2>
 *
 * Follow the steps in this procedure to build NvCap.
 *
 * <b>To build NvCap</b>
 *
 * -# Because the capture library is not enabled in the build by default,
 *    you must enable it. Set and export the following environment variable
 *    before starting the build:<pre> export NVCAP_VIDEO_ENABLED=1 </pre>
 * -# If NvCap is being integrated into an Android application:
 *  -# Add the following line to the Android.mk file of the application:
 *     <pre> LOCAL_CERTIFICATE := platform </pre>
 *  -# Add the following to the AndroidManifest.xml file, under the
 *     "manifest" node: <pre> android:sharedUserId="android.uid.system" </pre>
 * -# Compile the application along with the rest of the image. This ensures
 *    that it has sufficient privileges to use the capture library.
 *
 * <a name="nvcap_issues"></a><h2>Limitations</h2>
 *
 * The following is a list of known issues to be taken into consideration.
 * - Currently, the bit rate specified by the client application is interpreted
 *   as being the bit rate that is to be used for video capture. Due to the
 *   additional overhead of audio, as well as transport stream packetization,
 *   the final bit rate that the client application sees will be slightly higher
 *   than what it requested. This is planned to be fixed in a later release.
 * - Currently, NvCap ignores the gralloc <em>protected usage</em> flag that is
 *   used to detect protected content. In a later release, capturing protected
 *   content will not be allowed.
 * - The output stream generated by NvCap is not standard-compliant but it
 *   can be played back by Gallery and other media players. For example, the
 *   H.264 bitstream does not contain Network Abstraction Layer (NAL) access
 *   unit delimiters. This is planned to be fixed in a later release.
 * - The H.264 elementary stream produced by NvCap does not contain visual
 *   usability information (VUI), buffering period SEI messages, and picture
 *   timing SEI messages.
 *
 * <a name="nvcap_api"><h2>API Documentation</h2>
 *
 * The following sections provide data and function definitions for the capture
 * library.
 * @{
 */

/**
 * Defines NvCap API return status codes.
 */
typedef enum _NvCapStatus
{
    NvCapStatus_Success = 0,
    NvCapStatus_NotImplemented = -1,
    NvCapStatus_GenericFailure = -2,
    NvCapStatus_InsufficientMemory = -3,
    NvCapStatus_InvalidParam = -4,
    NvCapStatus_InvalidContext = -5,
    NvCapStatus_InvalidAttribute = -6,
    NvCapStatus_LevelLimitViolated = -7,
    NvCapStatus_RuntimeUpdateNotAllowed = -8,
    NvCapStatus_InsufficientRate = -9,
    NvCapStatus_PermissionsFailure = -10,

} NvCapStatus;

/**
 * Defines the list of attribute types that can be used with an
 *     NvCapSetAttribute() or NvCapGetAttribute() call. Currently, only
 *     \a StreamFormat, \a VideoCodec, \a AudioCodec, \a Resolution,
 *     \a RefreshRate and \a BitRate are supported.
 */
typedef enum _NvCapAttrType
{
    NvCapAttrType_Undefined = 0,
    NvCapAttrType_StreamFormat,
    NvCapAttrType_VideoCodec,
    NvCapAttrType_AudioCodec,
    NvCapAttrType_StereoFormat,
    NvCapAttrType_Profile,
    NvCapAttrType_Level,
    NvCapAttrType_Resolution,
    NvCapAttrType_RefreshRate,
    NvCapAttrType_BitRate,
    NvCapAttrType_MultiSliceInfo,
    NvCapAttrType_FrameSkipInfo,
    NvCapAttrType_NetworkParams,
    NvCapAttrType_MuxerParams,
    NvCapAttrType_SinkInfo,
    NvCapAttrType_LatencyProfiling,
    NvCapAttrType_HdcpParams,
    NvCapAttrType_MuxerStats,

    /// Must be the last entry of this enumeration.
    NvCapAttrType_Last,
} NvCapAttrType;

/**
 * Defines the list of parameters for which the application can query
 *     capabilities. As currently the NvCapGetCaps() function is not supported,
 *     currently this enumeration is not used.
 */
typedef enum _NvCapCapsType
{
    NvCapCapsType_StreamFormat = 0,
    NvCapCapsType_VideoCodec,
    NvCapCapsType_AudioCodec,
    NvCapCapsType_StereoFormat,
    NvCapCapsType_Profile,
    NvCapCapsType_Level,
    NvCapCapsType_Resolution,
    NvCapCapsType_RefreshRate,
    NvCapCapsType_HdcpMode,
    NvCapCapsType_Last,
} NvCapCapsType;

/**
 * Defines the stream formats. This enumeration corresponds to
 *    the ::NvCapAttrType_StreamFormat and ::NvCapCapsType_StreamFormat
 *    enumerations. Currently, only the \a AV_SingleStream enumeration is
 *    supported.
 */
typedef enum _NvCapStreamFormat
{
    NvCapStreamFormat_None = 0,
    NvCapStreamFormat_MPEG2TS_AV_SingleStream,
    NvCapStreamFormat_MPEG2TS_AV_SeparateStreams, // Not supported
    NvCapStreamFormat_MPEG2TS_Video_Only,         // Not supported
    NvCapStreamFormat_MPEG2TS_Audio_Only,         // Not supported
} NvCapStreamFormat;

/**
 * Defines the video capture formats. This enumeration
 *     corresponds to the ::NvCapAttrType_VideoCodec and
 *     ::NvCapCapsType_VideoCodec enumerations.
 */
typedef enum _NvCapVideoCodec
{
    NvCapVideoCodec_None = 0,
    NvCapVideoCodec_H264,
} NvCapVideoCodec;

/**
 * Defines the audio capture formats. This corresponds to the
 *    ::NvCapAttrType_AudioCodec and ::NvCapCapsType_AudioCodec enumerations.
 *    Currently only \a NvCapAudioCodec_LPCM_16_48_2ch is supported.
 */
typedef enum _NvCapAudioCodec
{
    NvCapAudioCodec_None = 0,
    NvCapAudioCodec_LPCM_44_1,      // Not supported
    NvCapAudioCodec_LPCM_16_48_2ch,
    NvCapAudioCodec_AAC_LC_2,       // Not supported
    NvCapAudioCodec_AAC_LC_4,       // Not supported
    NvCapAudioCodec_AAC_LC_6,       // Not supported
    NvCapAudioCodec_AAC_LC_8,       // Not supported
    NvCapAudioCodec_AC3_2,          // Not supported
    NvCapAudioCodec_AC3_4,          // Not supported
    NvCapAudioCodec_AC3_6,          // Not supported
} NvCapAudioCodec;

/**
 * Defines stereo formats. This enumeration corresponds to the
 *     ::NvCapAttrType_StereoFormat and ::NvCapCapsType_StereoFormat
 *     enumerations.
 */
typedef enum _NvCapStereoFormat
{
    NvCapStereoFormat_None = 0,
    NvCapStereoFormat_TB_Half,
    NvCapStereoFormat_BT_Half,
    NvCapStereoFormat_LR_Half,
    NvCapStereoFormat_RL_Half,
    NvCapStereoFormat_Frame_Seq,
    NvCapStereoFormat_Frame_Pack,
} NvCapStereoFormat;

/**
 * Defines the list of profiles. This enumeration
 *     corresponds to the ::NvCapAttrType_Profile and ::NvCapCapsType_Profile
 *     enumerations.
 *
 * Currently, explicit specification of the profile through
 *     NvCapSetAttribute() is not supported, so this enumeration is not
 *     currently used.
 */
typedef enum _NvCapProfile
{
    NvCapProfile_Undefined = 0,
    NvCapProfile_CBP,
    NvCapProfile_CHP, /**< Not supported. */
} NvCapProfile;

/**
 * Defines the list of capture levels. This enumeration corresponds to the
 *     ::NvCapAttrType_Level and ::NvCapCapsType_Level enumerations.
 *     Currently, explicit specification of the level through NvCapSetAttribute()
 *     is not supported, so this enumeration is not currently used.
 */
typedef enum _NvCapLevel
{
    NvCapLevel_Undefined = 0,
    NvCapLevel_3_1,
    NvCapLevel_3_2,
    NvCapLevel_4_0,
    NvCapLevel_4_1,
    NvCapLevel_4_2,
    NvCapLevel_Max,
} NvCapLevel;

/**
 * Defines the list of HDCP protection modes. This enumeration corresponds to the
 *     ::NvCapAttrType_HdcpParams and ::NvCapCapsType_HdcpMode enumerations.
 */
typedef enum _NvCapHdcpProtectionMode
{
    NvCapHdcpProtectionNone = 0,
    NvCapHdcpProtection_2_0,
    NvCapHdcpProtection_2_1,
} NvCapHdcpProtectionMode;

/**
 * Defines audio stream descriptors.
 */
typedef enum _NvCapAudDesc
{
    NvCapAudDesc_WiFiDisplay = 0,
    NvCapAudDesc_Bluray,
} NvCapAudDesc;

/**
 * Defines nvcap latency profile levels.
 */
typedef enum _NvCapLatencyProfilingLevel
{
    NvCapLatencyProfilingLevel_None     = 0,
    NvCapLatencyProfilingLevel_Summary  = 1,
    NvCapLatencyProfilingLevel_Complete = 2,
} NvCapLatencyProfilingLevel;

/**
 * Holds the capture resolution. This structure corresponds to
 *     ::NvCapAttrType_Resolution and ::NvCapCapsType_Resolution enumerations.
 */
typedef struct
{
    unsigned int   uiWidth;  /**< Capture width. */
    unsigned int   uiHeight; /**< Capture height. */
} NvCapResolution;

/**
 * Holds the refresh rate in frames per second (fps) for the capture.
 *    It corresponds to the ::NvCapAttrType_RefreshRate and
 *    ::NvCapCapsType_RefreshRate enumerations.
 */
typedef struct
{
    unsigned int   uiFPSNum;  /**< Numerator for the frame rate. */
    unsigned int   uiFPSDen;  /**< Denominator for the frame rate. */
} NvCapRefreshRate;

/**
 * Holds the bit rate to be used for the capture and the transmission rate at which TS data is
 * returned to the client. It corresponds to the
 * ::NvCapAttrType_BitRate enumeration.
 */
typedef struct
{
    /// Bits per second. This is set as the video bit rate; audio rate is
    /// constant for 48 k, 2 ch, 16 bits per sample at 1536000 bps. TS mux
    /// output rate is the sum of audio and video bit rates with a 5% overhead
    /// for TS headers, etc.
    /// <pre> mux_rate = 1.05x(1536000 + uiBitRate) </pre>
    unsigned int   uiBitRate;

    /// Bits per second. This must be set greater than or equal to TS mux output rate
    /// based on available bandwidth. This is the rate at which data is returned to the client.
    /// \a uiTransmissionRate must be set at least equal to \a mux_rate.

    unsigned int   uiTransmissionRate;

} NvCapBitRate;

/**
 * Holds the multi-slice information as negotiated with the sink. This
 *     structure corresponds to the ::NvCapAttrType_MultiSliceInfo enumeration.
 */
typedef struct
{
    unsigned int uiEnableSliceIntraRefresh;  /**<  0 => disable, > 1 => enable. */
    unsigned int uiEnableSliceBuffers;       /**<  0 => disable, > 1 => enable. */
    unsigned int uiMinSliceSzMBs; /**<  If multi-slice encode is supported, this
                                        is the minimum slice size in MBs. */
    unsigned int uiMaxSliceSzMBs; /**<  If multi-slice encode is supported, this
                                        is the maximum slice size in MBs. */
    unsigned int uiMaxSliceNum;   /**<  If multi-slice encode is supported, this
                                        is the maximum number of slices allowed. */
} NvCapMultiSlice;

/**
 * Holds skip frame information as negotiated with the sink. This structure
 *    corresponds to the ::NvCapAttrType_FrameSkipInfo enumeration.
 */
typedef struct
{
    unsigned int uiEnable;     /**<  1 => enable, > 0 => disable. */
    unsigned int uiSkipFactor; /**<  If frame skipping is disabled, this is
                                     ignored; otherwise, this can be a value
                                     [0..7]--If 0, then there is no limit on the
                                     number of skipped frames; if non-zero
                                     (1 to 7), multiply this value by 0.5 seconds
                                     to get the skip duration. */
} NvCapFrameSkip;

/**
 * Holds parameters to be provided by the client application based on network
 *     feedback, messages from the sink, etc. Corresponds to the
 *     ::NvCapAttrType_NetworkParams enumeration.
 */
typedef struct
{
    unsigned int uiForceIDRPic; /**< Set by the client to non-zero value to
                                     force IDR picture--The library sets it to 0
                                     after the instantaneous decoding refresh
                                     (IDR) picture has been returned. */
} NvCapNetworkFeedback;

/**
 * Holds parameters for configuring the muxer. Corresponds to the
 *     ::NvCapAttrType_MuxerParams enumeration.
 */
typedef struct
{
    NvCapAudDesc eAudDesc;     /**< Specifies the audio stream descriptor
                                    that must be inserted by the muxer. */
} NvCapMuxerParams;

/**
 * Holds information about the sink side. If not specified, NVCAP assumes the
 *     default value (see details for each field below). Corresponds to the
 *     ::NvCapAttrType_SinkInfo enumeration.
 */
typedef struct
{
    /// Indicates stereo support on sink side. If sink is stereo-capable,
    /// NVCAP enables stereo rendering where applicable.
    /// [default] 0 => Indicates sink does not support stereo.
    /// 1 => Indicates sink supports stereo.
    unsigned int uiStereoCapable;
} NvCapSinkInfo;

/*
 * Holds parameters for controlling latency profiling. Corresponds to the
 *     ::NvCapAttrType_LatencyProfiling enumeration.
 */
typedef struct
{
    NvCapLatencyProfilingLevel   eLatencyProfileLvl;
} NvCapLatencyProfiling;

/**
 * Defines the values of the buffer type. Each instance of NvCapBuf
 *     specifies the type of data that the buffer contains. Currently,
 *     only \a NvCapBufType_AV is used. \a NvCapBufType_AV specifies that
 *     the buffer contains transport stream packets that contain audio and video
 *     data muxed together. \a NvCapBufType_Video and \a NvCapBufType_Audio
 *     are used to indicate that the buffer contains only video data or only
 *     audio data respectively.
 */
typedef enum _NvCapBufType
{
    NvCapBufType_None = 0,
    NvCapBufType_AV,
    NvCapBufType_Video,
    NvCapBufType_Audio,
} NvCapBufType;

/**
 * Holds the captured data to return to the client application.
 */
typedef struct
{
    void*          pPtr;    /**< Pointer to the data. */
    unsigned int   uiSize;  /**< Size of the buffer. */
    unsigned int   uiDataSize;  /**< Size of the data in returned buffer. */
    unsigned int   uID;     /**< Unique ID that the client can use
                               to identify this buffer. */
    NvCapBufType  eType;   /**< Specifies whether this buffer has A/V data,
                               only video data, or only audio data--
                               the list of possible values is specified in
                               the ::NvCapBufType enumeration. */
} NvCapBuf;


/**
 * Holds parameters for authenticating HDCP. Corresponds to the
 *     ::NvCapAttrType_HdcpParams enumeration.
 */
typedef struct
{
    NvCapHdcpProtectionMode eHdcpMode;     /**< Specifies the HDCP mode that
                                                must be authenticated. */
    unsigned int            uiIpAddr;      /**< Specifies the IP address of the
                                              TCP server setup by the sink.*/
    unsigned short          usPort;        /**< Specifies the port of the above
                                              TCP server to which NvCap must connect. */
} NvCapHdcpParams;


/**
 * Defines NvCap event notifications returned from the capture library to
 *     the client application.
 */
typedef enum _NvCapEventCode
{
    NvCapEventCode_None = 0,
    NvCapEventCode_Protection,
    NvCapEventCode_FormatChange,
    NvCapEventCode_FrameDrop,
    NvCapEventCode_DisplayStatus,
    NvCapEventCode_HdcpStatus,
    NvCapEventCode_AudioStatus,
    NvCapEventCode_VprStatus,
} NvCapEventCode;

// NvCap Event Data Structures

/**
 * Holds the event data for the NvCapEventCode_Protection event.
 */
typedef struct
{
    /// 0 => Success, Anything else => Failure.
    int iStatus;
} NvCapEventProtection;

/**
 * Holds the event data for the ::NvCapEventCode_FormatChange event.
 */
typedef struct
{
    unsigned int uiVideo;  /**< Indicates whether or not the data being
                               captured is video--Value 0 denotes non-video data,
                               and anything non-zero denotes video data. */
    NvCapStereoFormat eStereoFmt; /**< Holds the stereo format of the captured data. */
    unsigned int uiWidth;   /**< Holds the width of the captured data. */
    unsigned int uiHeight;  /**< Holds the height of the captured data. */
} NvCapEventFormatChange;

/**
 * Holds the event data for the ::NvCapEventCode_FrameDrop event.
 * Frame drop notifications aggregate over a specified amount of time and
 * are reported back to the application. If frame drop notifications get returned
 * for every frame drop that occurs, then \a uiTime is 0 and can be ignored, and
 * \a uiDrops is set to 1.
 */
typedef struct
{
    unsigned int uiDrops;  /**< Holds the number of frame drops in \a uiTime ms. */
    unsigned int uiTime;   /**< Holds the number of milliseconds in which \a uiDrops
                               frame drops were counted. */
} NvCapEventFrameDrop;

/**
 * Structure that holds the event data for the ::NvCapEventCode_DisplayStatus event.
 * Display status notifications notify the client when display becomes invalid
 * (e.g., internal LCD is off so that no valid content can be captured). The client
 * is advised to disconnect from NVCAP in such a situation.
 */
typedef struct
{
    /// 0 => Indicates success; Anything else => indicates failure.
    int iStatus;
} NvCapEventDisplayStatus;

/**
 * Holds the event data for the
 * ::NvCapEventCode_AudioStatus event. Audio
 * status notifications notify the client
 * when audio status changes either from
 * ON->OFF or from OFF->ON.
 */
typedef struct
{
    /// Audio ON<->OFF status indication. 0 => audio ON, !0 => audio OFF.
    int iStatus;
} NvCapEventAudioStatus;

/**
 * Defines the values of the HDCP event codes.
 */
typedef enum _NvCapEventHdcpType
{
    /// Indicates that authorization succeeded.
    NvCapEventHdcp_AuthSuccess = 0,
    /// Indicates authorization failed.
    NvCapEventHdcp_AuthFail,
    /// Indicates that an authenticated session was compromised or the receiver
    /// has been revoked.
    NvCapEventHdcp_Compromised,
} NvCapEventHdcpType;

/**
 * Holds the event data for the ::NvCapEventCode_HdcpStatus event.
 * HDCP status notifications notify the client when authentication succeeds, fails,
 * or is compromised.
 */
typedef struct
{
    NvCapEventHdcpType eEventHdcp;
} NvCapEventHdcpStatus;

/**
 * Defines the values of the VPR allocation event codes.
 */
typedef enum _NvCapEventVprType
{
    NvCapEventVpr_AllocFail = 0,
    NvCapEventVpr_AllocSuccess
} NvCapEventVprType;

/**
 * Holds the event data for the ::NvCapEventCode_VprStatus event.
 * VPR status notifications notify the client when VPR allocation succeeds or fails.
 */
typedef struct
{
    NvCapEventVprType eEventVpr;
}NvCapEventVprStatus;

/**
 * Holds muxer stats.
 */
typedef struct
{
    unsigned int uiVideoBitRate;     /**< Current video bit rate. */
    unsigned int uiAudioBitRate;     /**< Current audio bit rate. */
    unsigned int uiTotalStreamRate;  /**< Current muxer output rate. */
} NvCapMuxerStats;

/**
 * Provides the library with the NvCapBuf instances that
 *     Transport Stream packets are filled into. The first argument holds the NVCAP
 *     context handle of this callback. The second argument provides a pointer to
 *     an array of \a %NvCapBuf pointers. The third argument specifies how
 *     many instances of \a %NvCapBuf are being provided. If buffers are available
 *     to copy TS packets, the function returns \a %NvCapBuf pointers with the element
 *     uiSize >= one TS packet size to the caller. If uiSize < one TS packet size
 *     no TS packet data is copied to it. If no buffers are available, the function
 *     returns immediately with NULL in the second argument and 0 in the third
 *     argument.
 */
typedef void (*PFNCB_GETBUFFERS)(NVCAP_CTX ctx, NvCapBuf*** pppBuf, int* pCount);

/**
 * Sends filled buffers back to the client. The first argument holds the NVCAP context
 *     handle of this callback. The second argument contains a pointer to an array
 *     of NvCapBuf pointers. The third argument specifies how many instances of
 *     \a %NvCapBuf are being provided. The library returns a whole number of TS
 *     packets. No TS packet data are split across \c PFNCB_SENDBUFFERS calls.
 */
typedef void (*PFNCB_SENDBUFFERS)(NVCAP_CTX ctx, NvCapBuf** ppBuf, int count);

/**
 * Provides the client with notifications. The first argument holds the NVCAP
 *     context handle of this callback. The second argument is an event code.
 *     The third argument points to event data. If the event has no event data
 *     associated with it, then the third argument is NULL.
 */
typedef void (*PFNCB_NOTIFYEVENT)(NVCAP_CTX ctx,
                                  NvCapEventCode eEventCode,
                                  void* pData);

/**
 * Holds callback pointers.
 */
typedef struct _NvCapCallbackInfo
{
    /// [in] Provides the library with the NvCapBuf instances that the
    /// Transport Stream packets are filled into. The prototype is
    /// specified by ::PFNCB_GETBUFFERS. The first argument provides
    /// a pointer to an array of \a %NvCapBuf pointers, and the second
    /// argument specifies how many instances of \a %NvCapBuf are being
    /// provided. Each instance must have space for at least single transport
    /// stream packet.
    PFNCB_GETBUFFERS pfnGetBuffersCB;
    /// [in] Used by the library to send filled buffers back to the client.
    /// The prototype is specified by ::PFNCB_SENDBUFFERS. The first argument
    /// contains a pointer to an array of NvCapBuf pointers, and the second
    /// argument specifies how many instances of \a %NvCapBuf are provided.
    /// The Capture library always sends back only a whole number of TS
    /// packets in each \a %NvCapBuf instance in each call.
    PFNCB_SENDBUFFERS pfnSendBuffersCB;
    /// [in] Provides the client with notifications. The prototype is
    /// specified by ::PFNCB_NOTIFYEVENT. It takes an event code and
    /// event data as input arguments. If the event has no event data
    /// associated with it, then the second argument is NULL. Currently,
    /// this callback is not being used by the capture library.
    PFNCB_NOTIFYEVENT pfnNotifyEventCB;
} NvCapCallbackInfo;

/**
 * Initializes the NvCap library.
 *
 * Initializes function pointers for the NvCapCallbackInfo::pfnGetBuffersCB,
 *     NvCapCallbackInfo::pfnSendBuffersCB() and
 *     NvCapCallbackInfo::pfnNotifyEventCB() callbacks, and the list of
 *     attributes that we can modify at runtime without requiring the library
 *     to first be paused.
 *
 * @param[in] pCtx A pointer to the context handle to be updated by the library.
 * @param[in] pCBInfo A pointer to a structure containing callback function
 *     pointers that must be setup by the client.
 *
 * @return An error code as specified by the ::NvCapStatus enumeration.
 */
NvCapStatus
NvCapInit(
    NVCAP_CTX* pCtx,
    NvCapCallbackInfo* pCBInfo);

/**
 * Gets the capabilities of the source device.
 *
 * @warning Currently, this function is not implemented and should not be used
 *     by the client application.
 *
 * The client provides an array to hold the list of capabilities of the
 *     source. The library fills out this array in the order of
 *     decreasing priority (that is, the value preferred by the
 *     source is listed first). This function helps the client
 *     decide which params it needs to use while negotiating with the sink.
 *
 * @param[in] ctx NvCap context handle.
 * @param[in] eAttrType The type of attribute for which capabilities are
 *      to be queried.
 * @param[in] pCapsArr A pointer to an array of attributes. These are
 *      ordered in decreasing order of priority. For example,
 *      if the client asks for the level capabilities, we return
 *      all the capabilities supported (up to the count provided), with
 *      the capability preferred for use by the source listed first.
 * @param[in,out] pCount A pointer to the number of capability entries
 *      requested. The actual number returned is updated in the same location.
 * @param[in] size The array of attributes pointed to by \a pCapsArr are
 *      all of the same type. \a size refers to how big each of these array
 *      elements is, which depends on the type of data (enum, structure)
 *      associated with this capability.
 *
 * @return An error code as specified by the ::NvCapStatus enumeration.
 */
NvCapStatus NvCapGetCaps(NVCAP_CTX ctx, NvCapCapsType eAttrType, void* pCapsArr,
                int* pCount, int size);

/**
 * Gets the value of the specified attribute
 *
 * @param[in] ctx NvCap context handle.
 * @param[in] eAttrType The type of attribute to be fetched.
 * @param[in] pAttribute A pointer to the attribute structure. Based on
 *      the type of attribute, the client must provide the
 *      appropriate structure instance.
 * @param[in] size Size of the \a pAttribute buffer.
 *
 * @return An error code as specified by the ::NvCapStatus enumeration.
 */
NvCapStatus
NvCapGetAttribute(
    NVCAP_CTX ctx,
    NvCapAttrType eAttrType,
    void* pAttribute,
    int size);

/**
 * Sets the value of the specified attribute.
 *
 * @note Client applications must pause the session before calling
 *    \c NvCapSetAttribute, otherwise an error will be returned.
 *
 * @param[in] ctx NvCap context handle.
 * @param[in] eAttrType The type of attribute to be updated.
 * @param[in] pAttribute A pointer to the attribute structure. Based on
 *      the type of attribute, the client must provide the
 *      appropriate structure instance, and the library updates
 *      its internal state.
 * @param[in] size Size of the \a pAttribute buffer.
 *
 * @retval NvCapStatus_SessionNotPaused If the session is not paused
 *     before calling.
 * @return An error code as specified by the ::NvCapStatus enumeration.
 */
NvCapStatus
NvCapSetAttribute(
    NVCAP_CTX ctx,
    NvCapAttrType eAttrType,
    void* pAttribute,
    int size);

/**
 * Provides the caller with an estimate of the Presentation TimeStamp (PTS)
 * of the current frame.
 *
 * @param[in] ctx NvCap context handle.
 * @param[in] pqwPTS A pointer to the location where the PTS value is returned.
 *
 * @return An error code as specified by the ::NvCapStatus enumeration.
 */
NvCapStatus
NvCapGetCurrentPTS(
    NVCAP_CTX ctx,
    unsigned long long* pqwPTS);

/**
 * Starts the capture session for the specified context. Make this call
 * after the session has been properly configured via NvCapSetAttribute().
 *
 * @param[in] ctx NvCap context handle.
 *
 * @return An error code as specified by the ::NvCapStatus enumeration.
 */
NvCapStatus NvCapStart(NVCAP_CTX ctx);

/**
 * Pauses the capture session for the specified context. This should be used
 * when light-weight re-configuration must be performed, or if the sink sends a
 * Pause message to the source. This function does not cause the library to
 * tear down more internal resources than is essential.
 * The client must call NvCapStart() to resume the capture.
 *
 * @param[in] ctx NvCap context handle.
 *
 * @return An error code as specified by the ::NvCapStatus enumeration.
 */
NvCapStatus NvCapPause(NVCAP_CTX ctx);

/**
 * Releases NvCap library resources.
 *
 * @param[in] ctx NvCap context handle.
 *
 * @return An error code as specified by the ::NvCapStatus enumeration.
 */
NvCapStatus NvCapRelease(NVCAP_CTX ctx);

#ifdef __cplusplus
}
#endif

/** @} */
#endif // NV_CAP_API_H
