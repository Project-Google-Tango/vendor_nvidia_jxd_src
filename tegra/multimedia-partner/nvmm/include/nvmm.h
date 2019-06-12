/*
* Copyright (c) 2007-2011 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** @file
* @brief <b>NVIDIA Driver Development Kit:
*           NvDDK Multimedia APIs</b>
*
* @b Description: Declares Interface for NvDDK multimedia blocks (mmblocks).
*/

#ifndef INCLUDED_NVMM_H
#define INCLUDED_NVMM_H

#define ENABLE_STREAMSHUTDOWN_CHANGES (1)

/**
*  @defgroup nvmm NVIDIA Multimedia APIs
*/


/**
*  @defgroup nvmm_modules Multimedia Codec APIs
*
*  @ingroup nvmm
* @{
*/

#include "nvcommon.h"
#include "nvos.h"
#include "nvmm_buffertype.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /**
    * @brief Define the notion of mmblock handle that represents one instance
    * of a particular type of mmblock. The handle is in fact a pointer to
    * a structure encapsulating the instance's context. This handle is
    * passed to every mmblock entrypoint to establish context.
    *
    * @ingroup mmblock
    */

    typedef struct NvMMBlockRec * NvMMBlockHandle;
    typedef struct NvMMBlockContextRec * NvMMBlockContextHandle;

    /** Maximum number of streams per block. */
#define NVMMBLOCK_MAXSTREAMS 32

    /** NvMMBlockType defines various supported mmblocks (i.e. accessible via
    * NvMMOpenMMBlock). Capabilities (e.g. profiles, levels, min/max bit rates,
    * min/max resolutions) for each mmblock can be queried by using GetCaps() for
    * each mmblock.
    *
    * @ingroup mmblock
    *
    */
    typedef enum
    {
        NvMMBlockType_UnKnown =0x00,
        /** Image/video encoders */
        NvMMBlockType_EncJPEG = 0x001,

        /** Image/video decoders */
        NvMMBlockType_DecVideoStart,

        NvMMBlockType_DecJPEG = 0x100,    /* leave space for future expansion */
        NvMMBlockType_DecSuperJPEG,
        NvMMBlockType_DecJPEGProgressive,
        NvMMBlockType_DecH263,
        NvMMBlockType_DecMPEG4,
        NvMMBlockType_DecH264,
        NvMMBlockType_DecH264_2x,
        NvMMBlockType_DecVC1,
        NvMMBlockType_DecVC1_2x,
        NvMMBlockType_DecMPEG2VideoVld,
        NvMMBlockType_DecMPEG2VideoRecon,
        NvMMBlockType_DecMPEG2,
        NvMMBlockType_Dummy1,             /* padding to keep enum values */
        NvMMBlockType_DecH264AVP,         /* for private purposes only */
        NvMMBlockType_DecMPEG4AVP,        /* for private purposes only */
        NvMMBlockType_DecVP8,

        NvMMBlockType_DecVideoEnd,

        /** Audio encoders */
        NvMMBlockType_EncAMRNB = 0x200,   /* leave space for future expansion */
        NvMMBlockType_EncAMRWB,
        NvMMBlockType_EncAAC,
        NvMMBlockType_EncMP3,
        NvMMBlockType_EncSBC,
        NvMMBlockType_EncWAV,
        NvMMBlockType_EnciLBC,

        /** Audio decoders */
        NvMMBlockType_DecAudioStart,

        NvMMBlockType_DecAMRNB = 0x300,   /* leave space for future expansion */
        NvMMBlockType_DecAMRWB,
        NvMMBlockType_DecAAC,
        NvMMBlockType_DecEAACPLUS,
        NvMMBlockType_DecILBC,
        NvMMBlockType_DecMP2,
        NvMMBlockType_DecMP3,
        NvMMBlockType_DecMP3SW,
        NvMMBlockType_DecWMA,
        NvMMBlockType_DecWMAPRO,
        NvMMBlockType_DecWMALSL,
        NvMMBlockType_DecRA8,
        NvMMBlockType_DecWAV,
        NvMMBlockType_DecSBC,
        NvMMBlockType_DecOGG,
        NvMMBlockType_DecBSAC,
        NvMMBlockType_DecADTS,
        NvMMBlockType_RingTone,
        NvMMBlockType_DecWMASUPER,
        NvMMBlockType_DecSUPERAUDIO,

        NvMMBlockType_DecAudioEnd,

        /** Source blocks */
        NvMMBlockType_SrcCamera = 0x400,
        NvMMBlockType_Dummy2,
        NvMMBlockType_SrcUSBCamera,
        /** Default Audio Input */
        NvMMBlockType_SourceAudio = 0x480,   /* Please reserve 0x481-0x48F for specific sources */

        /** Miscellaneous */
        NvMMBlockType_Mux = 0x500,        /* leave space for future expansion */
        NvMMBlockType_Demux,
        NvMMBlockType_FileReader,
        NvMMBlockType_3gpFileReader,
        NvMMBlockType_FileWriter,
        NvMMBlockType_AudioMixer,
        NvMMBlockType_AudioFX,
        NvMMBlockType_DigitalZoom,

        /** Sink blocks */
        NvMMBlockType_Sink = 0x700,       /* Replace this with first non-audio sink. */
        /** Default Audio Output */
        NvMMBlockType_SinkAudio = 0x780,  /* Please Reserve 0x781-0x78F for Specific Sinks */
        NvMMBlockType_SinkVideo,

        /* Parser Block */
        NvMMBlockType_SuperParser, /* Represents Block Type for all types of file formats */

        /* Writer Blocks */
        NvMMBlockType_3gppWriter, /* Represents Block Type for 3gpp Writer */

        NvMMBlockType_Force32 = 0x7FFFFFFF
    } NvMMBlockType;

#define NVMM_IS_BLOCK_AUDIO(x) ((x) > NvMMBlockType_DecAudioStart && (x) < NvMMBlockType_DecAudioEnd)

#define NVMM_IS_BLOCK_VIDEO(x) ((x) > NvMMBlockType_DecVideoStart && (x) < NvMMBlockType_DecVideoEnd)

    /* WILL BE DEPRICATED WHEN RPM IS IMPLEMENTED (begin) */

    /** @brief NvMMLocale selects whether mmblock is allocated on CPU or AVP.
    *  We may want to extend this as well to select other possible locales such
    *  as a dedicated hardware block,
    *
    * @ingroup mmblock
    *
    */
    typedef enum
    {
        /** Image/video encoders */
        NvMMLocale_CPU = 0x001,
        NvMMLocale_AVP,

        NvMMLocale_Force32 = 0x7FFFFFFF
    } NvMMLocale;

    /* WILL BE DEPRICATED WHEN RPM IS IMPLEMENTED (end) */


    /**
    * @brief MMBlock Attribute enumerations.
    * These attributes are examples and need to be filled in by group
    * They should probably live in individual header files per mmblock
    */

    enum {
        NvMMAttributeVideoDec_Unknown = 0x1000,  /* defined in nvmm_videodec.h */

        NvMMAttributeVideoEnc_Unknown = 0x2000,  /* defined in nvmm_videoenc.h */

        NvMMAttributeAudioDec_Unknown = 0x3000,  /* defined in nvmm_audiodnc.h */

        NvMMAttributeAudioEnc_Unknown = 0x4000,  /* defined in nvmm_audioenc.h */

        NvMMAttributeAudioMixer_Unknown = 0x5000,  /* defined in nvmm_mixer.h */

        NvMMAttributeParser_Unknown = 0x6000,   /* defined in nvmm_parser.h */

        NvMMAttributeWriter_Unknown = 0x7000,   /* defined in nvmm_writer.h */

        NvMMAttributeReference_Unknown = 0x8000, /* defined in nvmm_referenceblock.h */

        /** Stream position in bytes, see NvMMAttrib_StreamPosition.
        */
        NvMMAttribute_StreamPosition = 0x9000,

        NvMMAttributeVideoRend_Unknown = 0xA000, /* defined in nvmm_videorendererblock.h */

        /* defined to reserve attribute space for previous individual group attributes */
        NvMMAttributeGeneral_Unknown = 0xB000,

        /**
        * Used by client to get information from a block see NvMMAttrib_BlockInfo
        */
        NvMMAttribute_GetBlockInfo,

        /**
        * Used by client to get information of a stream from a block see NvMMAttrib_StreamInfo
        */
        NvMMAttribute_GetStreamInfo,

        /** Set profiling triggers.

        NvMMBlock::SetAttribute parameter \a pAttribute:
        <pre>
        NvU32 Bitmask of NvMMProfileType bits
        </pre>
        */
        NvMMAttribute_Profile,
        /**
        * Set this attribute to enable ULP mode
        */
        NvMMAttribute_UlpMode,

        /**
        * Set this attribute to enable/disable bugffer negotiation
        */
        NvMMAttribute_BufferNegotiationMode,

        /**
        * Set this attribute to enable ULP KPI log mode
        */
        NvMMAttribute_UlpKpiMode,

        /**
        * Set this attribute to enable/disable suspend mode
        */
        NvMMAttribute_PowerState,

        /**
        * Set this attribute to enable/disable stream and block events
        */
        NvMMAttribute_EventNotificationMode,

        /**
        * Set this attribute to update the trackindex
        */
        NvMMAttribute_UpdateTrackIndex,

        /**
        * Set this attribute to update Abnormal shutt down
        */
        NvMMAttribute_AbnormalTermination,

        /**
        * Query this attribute to get current track index
        */
        NvMMAttribute_GetCurrentTrackIndex,


        /**
        * Query this attribute to get metadata, see nvmm_metadata.h
        */
        NvMMAttribute_Metadata,

        /**
        * Query this attribute to get marker data, see nvmm_metadata.h
        */
        NvMMAttribute_MarkerData,

        /**
        * Query this attribute to get WMV Codecinfo data, see the structure NvMMWMVCodecInfo
        */

        NvMMAttribute_WMVCodecInfo,

        NvMMAttribute_GetExifInfo,

        /**
        * Query this attribute to get WMA Voice Codecinfo data (extra bytes other than WMA Format data, see NvMMWMAVoiceTrackInfo(nvmm_wma.h)
        */
        NvMMAttribute_WMAVoiceTrackInfo,

        /**
        * Query this attribute to get ASF DRM PLAY protection level info)
        */

        NvMMAttribute_AsfPlayProLvlInfo,
         /**
        * Query this attribute to get ASF DRM copy protection level info)
        */
        NvMMAttribute_AsfCopyProLvlInfo,
         /**
        * Query this attribute to get ASF DRM Incl protection level info)
        */
        NvMMAttribute_AsfInclProLvlInfo,

        /**
        * This attribute is set when direct tunnel is done for a stream
        */
        NvMMAttribute_SetTunnelMode,
        /**
        * This attribute is to get the video/audio  header for the stream from parser
        */
        NvMMAttribute_Header,
        /**
        * To enable MP3 TS from AVI :TOB: REMOVE
        */
        NvMMAttribute_Mp3EnableFlag,

        NvMMAttribute_Force32 = 0x7FFFFFFF
    };


    /** NvMMAttribute_StreamPosition parameter struct. */
    typedef struct {
        NvU32 StreamIndex;  //!< [Input] Specifies stream to get position for
        NvU64 Position;     //!< [Output] Byte position relative to stream start
    } NvMMAttrib_StreamPosition;

    /** NvMMAttrib_BlockInfo parameter struct. */
    typedef struct {
        NvMMLocale eLocale;  //!< Locale under which the block is running
        NvU32      NumStreams;       //!< Number of streams in this block.
    } NvMMAttrib_BlockInfo;

    /** NvMMAttrib_StreamInfo parameter struct. */
    typedef struct {
        NvU32 StreamIndex;       //!< [Input] Specifies stream to get position for
        NvU32 MixerStreamIndex;       //!< [Input] Specifies stream to get position for
        NvU32 NumBuffers;        //!< Number of negotiated buffers on the stream
        NvU8  Direction;         //!< NvMMStreamDir stream direction
        NvU8  bBlockIsAllocator; //!< NV_TRUE, if owning block is the allocator
    } NvMMAttrib_StreamInfo;

    typedef struct
    {
        /* Enable ULP mode for the block*/
        NvBool bEnableUlpMode;
        /* Enable ULP KPI mode for the block*/
        NvU32 ulpKpiMode;
    } NvMMAttrib_EnableUlpMode;

    typedef struct
    {
        /* Enable/Disable Stream event notifications from the block*/
        NvBool bStreamEventNotification;
        /* Enable/Disable block event notifications from the block*/
        NvBool bBlockEventNotification;
    } NvMMAttrib_EventNotificationInfo;

    typedef struct
    {
        NvU32 StreamIndex;
        NvU32 NumBuffers;
        NvBool bEnableBufferNegotiation;
    } NvMMAttrib_BufferNegotiationMode;

    /** NvMMAttrib_AbnormalTermInfo parameter struct. */
    typedef struct {
         NvBool bAbnormalTermination;
    } NvMMAttrib_AbnormalTermInfo;


    /** Profile operation options. */
    typedef enum {
        NvMMProfile_NullWork            = 0x1,   //!< Perform no work (decoding), just consume input and output meaningfull 'empty' data
        NvMMProfile_VideoDecode         = 0x2,
        NvMMProfile_AudioDecode         = 0x4,
        NvMMProfile_VideoFrameDecTime   = 0x8,
        NvMMProfile_BlockTransport      = 0x10,
        NvMMProfile_AVPKernel           = 0x20,
        NvMMProfile_VideoDecInputStarvation = 0x40
    } NvMMProfileType;

    typedef struct {
        NvU32 ProfileType; /* Bitfield of NvMMProfileType values */
    } NvMMAttrib_ProfileInfo;

    /** NvMM Power States */
    typedef enum
    {
        NvMMPowerState_Unspecified = -1,
        NvMMPowerState_FullOn = 0,
        NvMMPowerState_LowOn,
        NvMMPowerState_Standby,
        NvMMPowerState_Sleep,
        NvMMPowerState_Off,
        NvMMPowerState_Num,
        NvMMPowerState_Force32 = 0x7FFFFFFF
    } NvMMPowerState;

    typedef struct
    {
        NvMMPowerState PowerState;
    } NvMMAttrib_PowerStateInfo;

    typedef struct
    {
        NvU32 StreamIndex;
        NvBool bDirectTunnel;
    }NvMMAttrib_SetTunnelModeInfo;

    /**
    * @brief Defines the possible mmblock states.
    */
    typedef enum {
        /// The mmblock is stopped: all internal settings populated with initial
        /// values, not processing data.
        NvMMState_Stopped = 1,
        /// The mmblock is paused: internal settings are populated with current
        /// values, not processing data.
        NvMMState_Paused,
        /// The mmblock is running: internal seeting are populated with current
        /// values, processing data.
        NvMMState_Running,
        NvMMState_Force32 = 0x7FFFFFFF
    } NvMMState;

    /** Maximum length of block instance name, including 0-terminator. */
#define NVMM_BLOCKNAME_LENGTH 16

    /** Defines creation parameters for NvMMOpenBlock.

    For upward compatibility this struct must be initialized to 0 before use:
    <pre>
    NvMMCreationParameters CreationParameters;
    NvOsMemset(&CreationParameters, 0, sizeof(NvMMCreationParameters));
    </pre>
    */
    typedef struct
    {
        NvU32         structSize;               //!< Size of this struct in bytes
        NvMMBlockType Type;                     //!< Block type to instance
        NvMMLocale    Locale;                   //!< Locale under which the block will run
        NvU64         BlockSpecific;            //!< Block specific creation parameters.  For instance a GUID for a particular device
        NvU32         TransportSpecific;        //!< NvMM transport private
        NvBool        SetULPMode;               //!< enables ULP mode
    } NvMMCreationParameters;

    /**
    * @brief Global function to create a mmblock instance
    *
    * @param phBlock Pointer to location to hold created mmblock handle
    * @param pParams The creations parameters for this instance of the block.
    *
    * @retval NvSuccess MMBlock instance was successfully created
    * @retval NvError_BadParameter pParams contents are invalid
    *
    * @ingroup mmblock
    */
    NvError
        NvMMOpenBlock(
        NvMMBlockHandle *phBlock,
        NvMMCreationParameters *pParams
        );

    /**
    * @brief Global function to destroy a mmblock instance.
    *
    * @param hBlock A MMBLOCK handle.  If hBlock is NULL, this API does nothing.
    *
    * @ingroup mmblock
    */
    void
        NvMMCloseBlock(
        NvMMBlockHandle hBlock);

    typedef struct
    {
        NvU32 offset;
        NvU32 size;
        NvU64 TimeStamp;
        NvU32 BufferFlags;
        NvU32 MarkerNum;
        // please keep this structure a multiple of 8 bytes long
    } NvMMOffset;

#define MIN_OFFSETS 2
#define MAX_OFFSETS 1024
#define MIN_ULP_BUFFERS 6
#define MAX_ULP_BUFFERS 6
#define MAX_READBUFF_ADDRESSES 16

    typedef struct
    {
        NvU32 maxOffsets;
        NvU32 numOffsets;
        NvU32 tableIndex;
        NvU32 ConsumedSize; //This is useful on decoder side when we ran out of output buffers
        NvU32 VBase;
        NvU32 PBase;
        NvBool isOffsetListInUse;
        char* BuffMgrBaseAddr[MAX_READBUFF_ADDRESSES];
        NvU32 BuffMgrIndex;
        void *pParserCore;
        NvU32 padding; // 8-byte alignment
        // Please keep this structure a multiple of 8 bytes long.
    }NvMMOffsetList;

    /**
    * @brief flags for SetAttribute method.
    */
    typedef enum {
        /** Don't apply the given setting until recieving a SetAttribute with this
        flag clear. Normally clear. */
        NvMMSetAttrFlag_Deferred = 0x1,
        /** Send a notification when the attribute was set. Normally clear. */
        NvMMSetAttrFlag_Notification = 0x02,

        NvMMSetAttrFlag_Force32 = 0x7fffffff
    } NvMMSetAttrFlag;

    /**
    *
    * @brief Transfer buffer function
    *
    * All data transfer from or to a mmblock leverages this single function type
    * regardless of the caller/callee (shim/mmblock, mmblock/mmblock, or
    * mmblock/shim) or of the type of buffers transferred (full input buffers,
    * empty input buffer, full output buffers, empty output buffers). Abstraction
    * of this function type allows for simple and flexible routing of dataflow.
    *
    * This function also passes stream events which must be serialized with the
    * buffers.  If a call includes both a buffer and a stream event then the event
    * occurs before the buffer.
    *
    * @param pContext Private context pointer for the entity receiving this call. The
    * SetTransferBufferFunction establishes the context pointer associated with a given
    * TransferBufferFunction.
    * @param streamIndex The index of the stream on the MMBLOCK
    * @param BufferType The type of buffer being transferred.
    * @param BufferSize The total size of the buffer being transferred.
    * @param pBuffer Pointer to buffer descriptor object. This memory is owned
    * exclusively by the caller and is only guarenteed to be valid for the
    * duration of the call.  This field may be NULL if pStreamEvent is NON-NULL.
    *
    * @retval NvSuccess Transfer was successfully.
    *
    * @ingroup mmblock
    *
    */
    typedef
        NvError
        (*NvMMTransferBufferFunction)(
        void *pContext,
        NvU32 StreamIndex,
        NvMMBufferType BufferType,
        NvU32 BufferSize,
        void *pBuffer);

    typedef NvMMTransferBufferFunction TransferBufferFunction;

    /**
    *
    * @brief Block event callback
    *
    * All notifications from a mmblock to its client leverage the following
    * callback type:
    *
    * @param pContext Private context pointer for the client event call back.
    * @param EventType type of event passed
    * @param EventInfoSize the size of the event info buffer.
    * @param pEventInfo Pointer to event info. This memory is owned exclusively by
    * the caller and is only guarenteed to be valid for duration of the call.
    *
    * @retval NvSuccess Pointer to transfer function was successfully returned
    *
    * @ingroup mmblock
    *
    */
    typedef
        NvError
        (*NvMMSendBlockEventFunction)(
        void *pContext,
        NvU32 EventType,
        NvU32 EventInfoSize,
        void *pEventInfo);

    typedef NvMMSendBlockEventFunction SendBlockEventFunction;

    /**
    * @brief A mmblocks interface is a set of function pointers that provide
    * entrypoints for a given mmblock instance. Note that each instance may have
    * its own interface implementation (i.e. a different set of function
    * pointers).
    *
    * @ingroup mmblock
    */
    typedef struct NvMMBlockRec {
        NvU32 StructSize;

        /**
        * @brief CONTEXT
        *
        * Private context created and used by mmblock implementation
        *
        * @ingroup mmblock
        */
        void *pContext;

        /** Block semaphore timeout in ms.

        A block-side worker thread will wait on a semaphore until some
        event requires it to do work. This timeout specifies the timeout
        in ms, by which the wait will timeout and wake up the block.

        The NvMMBlock base class's default value is NV_WAIT_INFINITE.
        When a block is opened NvMM Transport uses this value. If value
        is zero, sets NV_WAIT_INFINITE
        */
        NvU32 BlockSemaphoreTimeout;

        /**
        * @brief Set buffer transfer function provided by the client.
        *
        * @param hBlock A block handle
        * @param streamIndex The index of the stream on the MMBLOCK
        * @param pTransferBuffer Pointer to the transfer buffer callback function
        * @param pContextForTransferBuffer Private context pointer for the entity
        * receiving the given transfer buffer function. If the entity is a block
        * this must be a block handle. If the entity is the client then this must be
        * client's context. The block receiving this call should not attempt to
        * dereference this pointer. It should provide this pointer as the first
        * parameter when the given TransferBufferFunction is called.
        * @param streamIndexForTransferBuffer Index of stream on destination
        * mmblock
        *
        * @retval NvSuccess Pointer to transfer function was successfully returned
        * @retval NvError_InvalidAddress pTransferBuffer is invalid.
        *
        * @ingroup mmblock
        */
        NvError
            (*SetTransferBufferFunction)(
            NvMMBlockHandle hBlock,
            NvU32 StreamIndex,
            NvMMTransferBufferFunction TransferBuffer,
            void *pContextForTransferBuffer,
            NvU32 StreamIndexForTransferBuffer);

        /**
        * @brief Get buffer transfer function of a block.
        *
        * @param hBlock A block handle
        * @param streamIndex The index of the stream on the MMBLOCK
        * @param pContextForTransferBuffer Context pointer for the block
        * to fill. Client may use this to set up tunnel with other blocks
        * or client itself
        *
        * If streamIndex is out of bounds, NULL is returned, otherwise
        * the must succeed. Only NvMM clients (e.g. tests, Dshow, OpenMAX IL)
        * will need to check for NULL.
        *
        * @ingroup mmblock
        */
        NvMMTransferBufferFunction
            (*GetTransferBufferFunction)(
            NvMMBlockHandle hBlock,
            NvU32 StreamIndex,
            void *pContextForTransferBuffer);

        /**
        * @brief Set buffer allocator.
        *
        * @param hCodec A block handle
        * @param StreamIndex The index of the stream on the MMBLOCK
        * @param bAllocateBuffers If true the block (the callee) has the obligation to allocate
        * the  buffers for the specified stream initially and the obligation to reallocate the
        * buffers any time new requirements imply a need to do so. The block may perform initial
        * allocation or dynamic reallocation as late as it wishes. (so long as the
        * block doesn't transfer the wrong type of buffer after a new buffer
        * requirements event). However, holding off allocation necessarily holds
        * off buffer transfer and consequently buffer processing (i.e. no allocation no processing).
        *
        * @retval NvSuccess function was successful
        *
        * @ingroup nvddk_codec
        */
        NvError
            (*SetBufferAllocator)(
            NvMMBlockHandle hBlock,
            NvU32 StreamIndex,
            NvBool AllocateBuffers);


        /**
        * @brief Set block notification callback given by client.
        *
        * @param hBlock A block handle
        * @param ClientContext Pointer to client context
        * @param SendBlockEvent Pointer to the notification callback function
        *
        * @retval NvSuccess Pointer to transfer function was successfully returned
        *
        * @ingroup mmblock
        */
        void
            (*SetSendBlockEventFunction)(
            NvMMBlockHandle hBlock,
            void *ClientContext,
            NvMMSendBlockEventFunction SendBlockEvent);

        /**
        * @brief Set block state.
        *
        * @param hBlock A block handle
        * @param State Enumeration of state to which to tansition
        *
        * @retval NvSuccess MMBlock state was successfully set
        *
        * @ingroup mmblock
        */
        NvError
            (*SetState)(
            NvMMBlockHandle hBlock,
            NvMMState State);

        /**
        * @brief Get block state.
        *
        * @param hBlock A block handle
        * @param pState Pointer to location to return block state
        *
        * @retval NvSuccess Pointer to transfer function was successfully returned
        * @retval NvError_InvalidAddress peState is invalid.
        *
        * @ingroup mmblock
        */
        NvError
            (*GetState)(
            NvMMBlockHandle hBlock,
            NvMMState *pState);

        /**
        * @brief Return all buffers of given stream to client.
        *
        * @param hBlock A block handle
        * @param StreamIndex The index of the stream on the block
        *
        * @retval NvSuccess Pointer to transfer function was successfully returned
        *
        * @ingroup mmblock
        */
        NvError
            (*AbortBuffers)(
            NvMMBlockHandle hBlock,
            NvU32 StreamIndex);



        /**
        * @brief Change some mmblock attribute (e.g. control, stream settings,
        * mmblock settings)
        *
        * The Attribute size is limited by the Size of the message that can be sent
        * across the RM transport. And so the max size of the attribute that can be
        * sent is 256-4-4-4-4 = 240 bytes.
        *
        * @param hBlock A block handle
        * @param AttributeType identifier for the attribute to set.
        * @param SetAttrFlag bitmask of NvMMSetAttrFlags
        * @param AttributeSize size of the attribute data to follow
        * @param pAttribute the attribute data
        *
        * @retval NvSuccess Pointer to transfer function was successfully returned
        *
        * @ingroup mmblock
        */
        NvError
            (*SetAttribute)(
            NvMMBlockHandle hBlock,
            NvU32 AttributeType,
            NvU32 SetAttrFlag,
            NvU32 AttributeSize,
            void *pAttribute);

        /**
        * @brief Query some mmblock attribute (e.g. control, stream settings,
        * mmblock settings)
        *
        * The Attribute size is limited by the Size of the message that can be sent
        * across the RM transport. And so the max size of the attribute that can be
        * extracted is 256-4-4-4 = 244 bytes.
        *
        * @param hBlock A block handle
        * @param AttributeType identifier for the attribute to get.
        * @param AttributeSize size of the attribute data to follow
        * @param pAttribute the attribute data
        *
        * @retval NvSuccess Pointer to transfer function was successfully returned
        *
        * @ingroup mmblock
        */
        NvError
            (*GetAttribute)(
            NvMMBlockHandle hBlock,
            NvU32 AttributeType,
            NvU32 AttributeSize,
            void *pAttribute);

        /** Use only when absolutely required. Prefer existing
        * interfaces/attributes first, new attributes second, and
        * extensions a distant third. Use only when there is some
        * operation (not setting) that is required on a block that
        * is not expressed in the existing API.
        *
        * @param hBlock A block handle
        * @param ExtensionIndex Implementation specific extension ID
        * @param SizeInput      Size of data passed in pInputData
        ignored if pInputData is NULL
        * @param pInputData     Buffer containing input parameters,
        can be NULL if extension does not use input
        * @param SizeOutput     Size of block passed in pOutputData,
        ignored if pOutputData is NULL
        * @param pOutputData    Buffer to receive output parameters,
        can be NULL if extension does not use output
        *
        * @retval NvSuccess Call was successfully processed
        *
        * @ingroup mmblock
        */
        NvError
            (*Extension)(
            NvMMBlockHandle hBlock,
            NvU32 ExtensionIndex,
            NvU32 SizeInput,
            void *pInputData,
            NvU32 SizeOutput,
            void *pOutputData);

    } NvMMBlock;

    /**
    * NvMMDoWorkCondition defines the condition for the blocks doWork function
    */
    typedef enum
    {
        /* Indicates that the block can only do critical work */
        NvMMDoWorkCondition_Critical = 1,
        /* Indicates that the semaphore timed out */
        NvMMDoWorkCondition_Timeout,
        /* Indicates to the block to do some work */
        NvMMDoWorkCondition_SomeWork,

        NvMMDoWorkCondition_Force32 = 0x7FFFFFFF
    }NvMMDoWorkCondition;

    /******************************************************************************
    * Function definitions for block implementers. NvMM Transport will
    * call these functions. TBD: Should we move these to another header?
    *****************************************************************************/
    /**
    * @brief Function type for a block's "do work" function. This function is
    * called by the owner of the block's thread when to allow the block to do
    * work.
    *
    * @param [in] hBlock A block handle
    * @param [in] NvMMDoWorkCondition
    *      Indicates the condition for the the type of work to do
    * @param [out] bMoreWorkPending
    *      True if and only if block still has work pending upon returning from
    *      the call.
    * @retval NvSuccess peration was successful.
    *
    * @ingroup mmblock
    *
    */
    typedef
        NvError
        (*NvMMDoWorkFunction)(
        NvMMBlockHandle hBlock,
        NvMMDoWorkCondition Flag,
        NvBool *pMoreWorkPending);

    /**
    * @brief Defines creation parameters for NvMMOpenBlockFunction
    */
    typedef struct
    {
        /** The locale under which the block will run */
        NvMMLocale Locale; /* WILL BE DEPRICATED WHEN RPM IS IMPLEMENTED */

        /** Unique name unambiguously identifying this instance of the block */
        char InstanceName[16];
        NvU64 BlockSpecific;
        NvBool SetULPMode;
        void *stackPtr;
        NvU32 stackSize;

        void *hService;

    } NvMMInternalCreationParameters;

    /*
    * @brief Each block implementation will expose an open entrypoint with the following
    * signature.
    *
    * @param [out] hBlock Pointer to a block handle to be created
    * @param [in] pParams creation parameters
    * @param [in] blockSemaphore Semaphore used for all triggers on block.
    * @param [out] pDoWorkFunction pointer to the NvMMDoWorkFunction of this block
    *
    * @retval NvSuccess operation was successful.
    *
    */
    typedef
        NvError
        (*NvMMOpenBlockFunction)(
        NvMMBlockHandle *hBlock,
        NvMMInternalCreationParameters *pParams,
        NvOsSemaphoreHandle blockSemaphore,
        NvMMDoWorkFunction *pDoWorkFunction);

    /**
    * @brief Defines destruction parameters for NvMMCloseBlockFunction
    */
    typedef struct {
        NvU32 structSize;
        /* TBD: exact contents */
    } NvMMInternalDestructionParameters;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_H


