/*
* Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
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

#ifndef INCLUDED_NVMMLITE_H
#define INCLUDED_NVMMLITE_H

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
#include "nvrm_surface.h"
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
typedef struct NvMMLiteBlockRec * NvMMLiteBlockHandle;
typedef struct NvMMLiteBlockContextRec * NvMMLiteBlockContextHandle;

/** Maximum number of streams per block. */
#define NVMMLITEBLOCK_MAXSTREAMS 32

/** NvMMLiteBlockType defines various supported mmblocks (i.e. accessible via
 * NvMMLiteOpenMMBlock).
 *
 * @ingroup mmblock
 *
 */
typedef enum
{
    NvMMLiteBlockType_UnKnown =0x00,
    /** Image/video encoders */
    NvMMLiteBlockType_EncJPEG = 0x001,
    NvMMLiteBlockType_EncH263,
    NvMMLiteBlockType_EncMPEG4,
    NvMMLiteBlockType_EncH264,
    NvMMLiteBlockType_EncVC1,             /* not available yet */
    NvMMLiteBlockType_EncJPEG2000,        /* not available yet */
    NvMMLiteBlockType_EncVP8,

    /** Image/video decoders */
    NvMMLiteBlockType_DecVideoStart,

    NvMMLiteBlockType_DecJPEG = 0x100,    /* leave space for future expansion */
    NvMMLiteBlockType_DecSuperJPEG,
    NvMMLiteBlockType_DecJPEGProgressive,
    NvMMLiteBlockType_DecH263,
    NvMMLiteBlockType_DecMPEG4,
    NvMMLiteBlockType_DecH264,
    NvMMLiteBlockType_DecH264_2x,
    NvMMLiteBlockType_DecVC1,
    NvMMLiteBlockType_DecVC1_2x,
    NvMMLiteBlockType_DecMPEG2VideoVld,
    NvMMLiteBlockType_DecMPEG2VideoRecon,
    NvMMLiteBlockType_DecMPEG2,
    NvMMLiteBlockType_Dummy1,             /* padding for enum values */
    NvMMLiteBlockType_DecH264AVP,         /* for private purposes only */
    NvMMLiteBlockType_DecMPEG4AVP,        /* for private purposes only */
    NvMMLiteBlockType_Dummy2,
    NvMMLiteBlockType_Dummy3,
    NvMMLiteBlockType_Dummy4,
    NvMMLiteBlockType_Dummy5,
    NvMMLiteBlockType_Dummy6,
    NvMMLiteBlockType_Dummy7,
    NvMMLiteBlockType_DecMJPEG,
    NvMMLiteBlockType_DecVP8,
    NvMMLiteBlockType_DecH265,

    NvMMLiteBlockType_DecVideoEnd,

    /** Audio encoders */
    NvMMLiteBlockType_EncAMRNB = 0x200,   /* leave space for future expansion */
    NvMMLiteBlockType_EncAMRWB,
    NvMMLiteBlockType_EncAAC,
    NvMMLiteBlockType_EncMP3,
    NvMMLiteBlockType_EncSBC,
    NvMMLiteBlockType_EncWAV,
    NvMMLiteBlockType_EnciLBC,

    /** Audio decoders */
    NvMMLiteBlockType_DecAudioStart,

    NvMMLiteBlockType_DecAMRNB = 0x300,   /* leave space for future expansion */
    NvMMLiteBlockType_DecAMRWB,
    NvMMLiteBlockType_DecAAC,
    NvMMLiteBlockType_DecEAACPLUS,
    NvMMLiteBlockType_DecILBC,
    NvMMLiteBlockType_DecMP2,
    NvMMLiteBlockType_DecMP3,
    NvMMLiteBlockType_DecMP3SW,
    NvMMLiteBlockType_DecWMA,
    NvMMLiteBlockType_DecWMAPRO,
    NvMMLiteBlockType_DecWMALSL,
    NvMMLiteBlockType_DecRA8,
    NvMMLiteBlockType_DecWAV,
    NvMMLiteBlockType_DecSBC,
    NvMMLiteBlockType_DecOGG,
    NvMMLiteBlockType_DecBSAC,
    NvMMLiteBlockType_DecADTS,
    NvMMLiteBlockType_RingTone,
    NvMMLiteBlockType_DecWMASUPER,
    NvMMLiteBlockType_DecSUPERAUDIO,

    NvMMLiteBlockType_DecAudioEnd,

    /** Source blocks */
    NvMMLiteBlockType_SrcCamera = 0x400,
    NvMMLiteBlockType_SrcUSBCamera,
    /** Default Audio Input */
    NvMMLiteBlockType_SourceAudio = 0x480,   /* Please reserve 0x481-0x48F for specific sources */

    /** Miscellaneous */
    NvMMLiteBlockType_Mux = 0x500,        /* leave space for future expansion */
    NvMMLiteBlockType_Demux,
    NvMMLiteBlockType_FileReader,
    NvMMLiteBlockType_3gpFileReader,
    NvMMLiteBlockType_FileWriter,
    NvMMLiteBlockType_AudioMixer,
    NvMMLiteBlockType_AudioFX,
    NvMMLiteBlockType_DigitalZoom,

    /** Sink blocks */
    NvMMLiteBlockType_Sink = 0x700,       /* Replace this with first non-audio sink. */
    /** Default Audio Output */
    NvMMLiteBlockType_SinkAudio = 0x780,  /* Please Reserve 0x781-0x78F for Specific Sinks */
    NvMMLiteBlockType_SinkVideo,

    /* Parser Block */
    NvMMLiteBlockType_SuperParser, /* Represents Block Type for all types of file formats */

    /* Writer Blocks */
    NvMMLiteBlockType_3gppWriter, /* Represents Block Type for 3gpp Writer */

    NvMMLiteBlockType_Force32 = 0x7FFFFFFF
} NvMMLiteBlockType;

#define NVMMLITE_IS_BLOCK_AUDIO(x) ((x) > NvMMLiteBlockType_DecAudioStart && (x) < NvMMLiteBlockType_DecAudioEnd)

#define NVMMLITE_IS_BLOCK_VIDEO(x) ((x) > NvMMLiteBlockType_DecVideoStart && (x) < NvMMLiteBlockType_DecVideoEnd)

/**
 * @brief MMBlock Attribute enumerations.
 * These attributes are examples and need to be filled in by group
 * They should probably live in individual header files per mmblock
 */
enum {
    NvMMLiteAttributeVideoDec_Unknown = 0x1000,  // defined in nvmm_videodec.h
    NvMMLiteAttributeVideoEnc_Unknown = 0x2000,  // defined in nvmm_videoenc.h
    NvMMLiteAttributeAudioDec_Unknown = 0x3000,  // defined in nvmm_audiodec.h
    NvMMLiteAttributeAudioEnc_Unknown = 0x4000,  // defined in nvmm_audioenc.h
    NvMMLiteAttributeAudioMixer_Unknown = 0x5000,// defined in nvmm_mixer.h
    NvMMLiteAttributeParser_Unknown = 0x6000,    // defined in nvmm_parser.h
    NvMMLiteAttributeWriter_Unknown = 0x7000,    // defined in nvmm_writer.h
    /**
     * Stream position in bytes, see NvMMLiteAttrib_StreamPosition.
     */
    NvMMLiteAttribute_StreamPosition = 0x9000,

    /* reserve attribute space for previous individual group attributes */
    NvMMLiteAttributeGeneral_Unknown = 0xB000,

    /**
     * Used to get information from a block see NvMMLiteAttrib_BlockInfo
     */
    NvMMLiteAttribute_GetBlockInfo,

    /**
     * Used to get information about a stream see NvMMLiteAttrib_StreamInfo
     */
    NvMMLiteAttribute_GetStreamInfo,

    /** Set profiling triggers.
     *
     * NvMMLiteBlock::SetAttribute parameter \a pAttribute:
     * <pre>
     * NvU32 Bitmask of NvMMLiteProfileType bits
     * </pre>
     */
    NvMMLiteAttribute_Profile,

    /**
     * Set this attribute to enable/disable suspend mode
     */
    NvMMLiteAttribute_PowerState,

    /**
     * Set this attribute to enable/disable stream and block events
     */
    NvMMLiteAttribute_EventNotificationMode,
    /**
     * Use this attribute to Get ExifInfo from block
     */
    NvMMLiteAttribute_GetExifInfo,

    NvMMLiteAttribute_Force32 = 0x7FFFFFFF
};


/** NvMMLiteAttribute_StreamPosition parameter struct. */
typedef struct {
    NvU32 StreamIndex;  //!< [Input] Specifies stream to get position for
    NvU64 Position;     //!< [Output] Byte position relative to stream start
} NvMMLiteAttrib_StreamPosition;

/** NvMMLiteAttrib_BlockInfo parameter struct. */
typedef struct {
    NvU32      NumStreams;       //!< Number of streams in this block.
} NvMMLiteAttrib_BlockInfo;

/** NvMMLiteAttrib_StreamInfo parameter struct. */
typedef struct {
    NvU32 StreamIndex;       //!< [Input] Specifies stream to get position for
    NvU32 MixerStreamIndex;  //!< [Input] Specifies stream to get position for
    NvU32 NumBuffers;        //!< Number of negotiated buffers on the stream
    NvU8  Direction;         //!< NvMMLiteStreamDir stream direction
} NvMMLiteAttrib_StreamInfo;

typedef struct
{
    /* Enable/Disable Stream event notifications from the block*/
    NvBool bStreamEventNotification;
    /* Enable/Disable block event notifications from the block*/
    NvBool bBlockEventNotification;
} NvMMLiteAttrib_EventNotificationInfo;

/** NvMMLite Power States */
typedef enum
{
    NvMMLitePowerState_Unspecified = -1,
    NvMMLitePowerState_FullOn = 0,
    NvMMLitePowerState_LowOn,
    NvMMLitePowerState_Standby,
    NvMMLitePowerState_Sleep,
    NvMMLitePowerState_Off,
    NvMMLitePowerState_Num,
    NvMMLitePowerState_Force32 = 0x7FFFFFFF
} NvMMLitePowerState;

typedef struct
{
    NvMMLitePowerState PowerState;
} NvMMLiteAttrib_PowerStateInfo;

/**
 * @brief Defines the possible mmblock states.
 */
typedef enum {
    /// The mmblock is stopped: all internal settings populated with initial
    /// values, not processing data.
    NvMMLiteState_Stopped = 1,
    /// The mmblock is paused: internal settings are populated with current
    /// values, not processing data.
    NvMMLiteState_Paused,
    /// The mmblock is running: internal seeting are populated with current
    /// values, processing data.
    NvMMLiteState_Running,
    NvMMLiteState_Force32 = 0x7FFFFFFF
} NvMMLiteState;

/** Maximum length of block instance name, including 0-terminator. */
#define NVMMLITE_BLOCKNAME_LENGTH 16

/** Defines creation parameters for NvMMLiteOpenBlock.
 *
 * For upward compatibility this struct must be initialized to 0 before use:
 * <pre>
 * NvMMLiteCreationParameters CreationParameters;
 * NvOsMemset(&CreationParameters, 0, sizeof(NvMMLiteCreationParameters));
 * </pre>
 */
typedef struct
{
    NvU32         structSize;               //!< Size of this struct in bytes
    NvMMLiteBlockType Type;                 //!< Block type to instance
    NvU64         BlockSpecific;            //!< Block specific creation
                                            //!< parameters.  For instance a
                                            //!< GUID for a particular device
} NvMMLiteCreationParameters;

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
NvError NvMMLiteOpenBlock(NvMMLiteBlockHandle *phBlock,
                          NvMMLiteCreationParameters *pParams);

/**
 * @brief Global function to destroy a mmblock instance.
 *
 * @param hBlock A MMBLOCK handle.  If hBlock is NULL, this API does nothing.
 *
 * @ingroup mmblock
 */
void NvMMLiteCloseBlock(NvMMLiteBlockHandle hBlock);

/**
 * @brief flags for SetAttribute method.
 */
typedef enum {
    /** Don't apply the given setting until recieving a SetAttribute with this
        flag clear. Normally clear. */
    NvMMLiteSetAttrFlag_Deferred = 0x1,
    /** Send a notification when the attribute was set. Normally clear. */
    NvMMLiteSetAttrFlag_Notification = 0x02,

    NvMMLiteSetAttrFlag_Force32 = 0x7fffffff
} NvMMLiteSetAttrFlag;

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
 * @param pContext Private context pointer for the entity receiving this call.
 * The SetTransferBufferFunction establishes the context pointer associated
 * with a given TransferBufferFunction.
 *
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
typedef NvError (*NvMMLiteTransferBufferFunction)(void *pContext,
                                                  NvU32 StreamIndex,
                                                  NvMMBufferType BufferType,
                                                  NvU32 BufferSize,
                                                  void *pBuffer);

typedef NvMMLiteTransferBufferFunction LiteTransferBufferFunction;

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
typedef NvError (*NvMMLiteSendBlockEventFunction)(void *pContext,
                                                  NvU32 EventType,
                                                  NvU32 EventInfoSize,
                                                  void *pEventInfo);

typedef NvMMLiteSendBlockEventFunction LiteSendBlockEventFunction;

/**
 * NvMMLiteDoWorkCondition defines the condition for the blocks doWork function
 */
typedef enum
{
    /* Indicates that the block can only do critical work */
    NvMMLiteDoWorkCondition_Critical = 1,
    /* Indicates that the semaphore timed out */
    NvMMLiteDoWorkCondition_Timeout,
    /* Indicates to the block to do some work */
    NvMMLiteDoWorkCondition_SomeWork,

    NvMMLiteDoWorkCondition_Force32 = 0x7FFFFFFF
}NvMMLiteDoWorkCondition;

/**
 * @brief Function type for a block's "do work" function. This function is
 * called by the owner of the block's thread when to allow the block to do
 * work.
 *
 * @param [in] hBlock A block handle
 * @param [in] NvMMLiteDoWorkCondition
 *      Indicates the condition for the the type of work to do
 * @param [out] bMoreWorkPending
 *      True if and only if block still has work pending upon returning from
 *      the call.
 * @retval NvSuccess peration was successful.
 *
 * @ingroup mmblock
 *
 */
typedef NvError (*NvMMLiteDoWorkFunction)(NvMMLiteBlockHandle hBlock,
                                          NvMMLiteDoWorkCondition Flag,
                                          NvBool *pMoreWorkPending);

/**
 * @brief A mmblocks interface is a set of function pointers that provide
 * entrypoints for a given mmblock instance. Note that each instance may have
 * its own interface implementation (i.e. a different set of function
 * pointers).
 *
 * @ingroup mmblock
 */
typedef struct NvMMLiteBlockRec {
    NvU32 StructSize;

    NvMMLiteDoWorkFunction          pDoWork;
    /**
     * @brief CONTEXT
     *
     * Private context created and used by mmblock implementation
     *
     * @ingroup mmblock
     */
    void *pContext;

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
    NvError (*SetTransferBufferFunction)(NvMMLiteBlockHandle hBlock,
                                         NvU32 StreamIndex,
                                         NvMMLiteTransferBufferFunction TransferBuffer,
                                         void *pContextForTransferBuffer,
                                         NvU32 StreamIndexForTransferBuffer);

    /**
     * @brief Transfer a buffer to a block.
     *
     * @ingroup mmblock
     */
    NvMMLiteTransferBufferFunction TransferBufferToBlock;

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
    void (*SetSendBlockEventFunction)(NvMMLiteBlockHandle hBlock,
                                      void *ClientContext,
                                      NvMMLiteSendBlockEventFunction SendBlockEvent);

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
    NvError (*SetState)(NvMMLiteBlockHandle hBlock, NvMMLiteState State);

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
    NvError (*GetState)(NvMMLiteBlockHandle hBlock, NvMMLiteState *pState);

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
    NvError (*AbortBuffers)(NvMMLiteBlockHandle hBlock, NvU32 StreamIndex);

    /**
     * @brief Change some mmblock attribute (e.g. control, stream settings,
     * mmblock settings)
     *
     * @param hBlock A block handle
     * @param AttributeType identifier for the attribute to set.
     * @param SetAttrFlag bitmask of NvMMLiteSetAttrFlags
     * @param AttributeSize size of the attribute data to follow
     * @param pAttribute the attribute data
     *
     * @retval NvSuccess Pointer to transfer function was successfully returned
     *
     * @ingroup mmblock
     */
    NvError (*SetAttribute)(NvMMLiteBlockHandle hBlock,
                            NvU32 AttributeType, NvU32 SetAttrFlag,
                            NvU32 AttributeSize, void *pAttribute);

    /**
     * @brief Query some mmblock attribute (e.g. control, stream settings,
     * mmblock settings)
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
    NvError (*GetAttribute)(NvMMLiteBlockHandle hBlock,
                            NvU32 AttributeType, NvU32 AttributeSize,
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
    NvError (*Extension)(NvMMLiteBlockHandle hBlock,
                         NvU32 ExtensionIndex, NvU32 SizeInput,
                         void *pInputData, NvU32 SizeOutput,
                         void *pOutputData);

    /** Close the block.
     *
     * @param hBlock A block handle
     */
    void (*Close)(NvMMLiteBlockHandle hBlock);
} NvMMLiteBlock;

/******************************************************************************
 * Function definitions for block implementers. NvMMLite Transport will
 * call these functions. TBD: Should we move these to another header?
 *****************************************************************************/
/**
 * @brief Defines creation parameters for NvMMLiteOpenBlockFunction
 */
typedef struct
{
    /** Unique name unambiguously identifying this instance of the block */
    char InstanceName[16];
    NvU64 BlockSpecific;
} NvMMLiteInternalCreationParameters;

/*
 * @brief Each block library will expose an open entrypoint with the following
 * signature.
 *
 * @param [out] hBlock Pointer to a block handle to be created
 * @param [in] pParams creation parameters
 * @param [in] blockSemaphore Semaphore used for all triggers on block.
 * @param [out] pDoWorkFunction pointer to the NvMMLiteDoWorkFunction of this block
 *
 * @retval NvSuccess operation was successful.
 *
 */
typedef NvError (*NvMMLiteOpenBlockFunction)(
    NvMMLiteBlockType eBlockType,
    NvMMLiteBlockHandle *hBlock,
    NvMMLiteInternalCreationParameters *pParams,
    NvMMLiteDoWorkFunction *pDoWorkFunction);

/*
 * @brief Each block library will exopse an entrypoint with the following
 * signature to allow probing of what blocks that library supports.
 */
typedef NvMMLiteBlockType (*NvMMLiteQueryBlocksFunction)(int index);

/*
 * @brief Each block library will expose one function with below
 * signature to allow entry into blocks that library supports
 */
NvError NvMMLiteOpen(NvMMLiteBlockType eBlockType,
                     NvMMLiteBlockHandle *phBlock,
                     NvMMLiteInternalCreationParameters *pParams,
                     NvMMLiteDoWorkFunction *pDoWorkFunction);

/*
 * @brief Each block library will expose one function with below
 * signature to allow query of blocks supported by that library
 */
NvMMLiteBlockType NvMMLiteQueryBlocks(int index);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMMLITE_H


