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
 *           NvMMBlock baseclass APIs</b>
 *
 * @b Description: Common functionality for NvMMBlocks.
 */

#ifndef INCLUDED_NVMM_BLOCK_H
#define INCLUDED_NVMM_BLOCK_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvmm_queue.h"
#include "nvmm_bufferprofile.h"
#include "nvmm_event.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NV_USE_COMMON_BUFFER_SIZE   1

#define NVMM_COMMON_MAX_INPUT_BUFFER_SIZE       24 * 1024
#define NVMM_COMMON_MIN_INPUT_BUFFER_SIZE       24 * 1024

#define NVMM_COMMON_MAX_OUTPUT_BUFFER_SIZE      8 * 1024
#define NVMM_COMMON_MIN_OUTPUT_BUFFER_SIZE      8 * 1024

#define NVMM_COMMON_MAX_OUTPUT_BUFFERS          10
#define NVMM_COMMON_MIN_OUTPUT_BUFFERS          10

#define NVMM_COMMON_MIN_MIXER_BUFFER_SIZE        8 * 1024
#define NVMM_COMMON_MAX_MIXER_BUFFER_SIZE        8 * 1024

#define NVMM_COMMON_MIN_MIXER_BUFFERS            10
#define NVMM_COMMON_MAX_MIXER_BUFFERS            10

/** Maximum number of supported buffers per NvMMStream. */
#define NVMMSTREAM_MAXBUFFERS 32

/** NvMMStream direction IDs. */
typedef enum
{
    NvMMStreamDir_In  = 0,  //!< In, block receives full buffers.
    NvMMStreamDir_Out = 1   //!< Out, block receives empty buffers.

} NvMMStreamDir;

/** Stream control container for a stream managed by an NvMMBlock. */
typedef struct NvMMStreamRec
{
    NvU32                   StreamIndex;                 //!< Index of this stream on the owning NvMMBlock
    NvMMQueueHandle         BufQ;                        //!< Buffer queue, full buffers for incoming, empty buffers for outgoing streams
    TransferBufferFunction  TransferBufferFromBlock;     //!< TransferBuffer function of other side, NULL if not connected
    NvU32                   OutgoingStreamIndex;         //!< Stream index for a NvMMStream::TransferBuffer (index on the other side)
    void*                   pOutgoingTBFContext;         //!< Context for \a NvMMStream::TransferBuffer
    NvU32                   NumBuffers;                  //!< Number of negotiated buffers on the stream
    NvU64                   Position;                    //!< Current stream position in bytes
    NvU32                   EndOfStreamBufferCount;
    NvU8                    Direction;                   //!< NvMMStreamDir stream direction
    NvU8                    bBlockIsAllocator;           //!< NV_TRUE, if owning block is the allocator
    NvU8                    bTBFSet;                     //!< NV_TRUE, if TBF is set.
    NvU8                    bNewConfigurationReceived;   //!< NV_TRUE, if new buffer configuration received
    NvU8                    bNewBufferRequirementsReceived;   //!< NV_TRUE, if new NewBufferRequirements received
    NvU8                    bStreamBuffersReturned;      //!< NV_TRUE, if stream shutting down and all buffers are returned
    NvU8                    bStreamShuttingDown;         //!< NV_TRUE, if stream received NvMMEvent_StreamShutdown
    NvU8                    bEndOfStream;                //!< NV_TRUE, if stream received NvMMEvent_StreamEnd
    NvU8                    bEndOfTrack;                 //!< NV_TRUE, if stream received TrackEnd buffer flag
    NvBool                  bEnableBufferNegotiation;    //!< NV_TRUE, if buffer negotiation is required which is default value
    NvBool                  bWaitToRenegotiate;          //!< NV_TRUE, if waiting for the buffers to come back before renegotiating.
    NvBool                  bBuffersAllocated;           //!< NV_TRUE, if buffers are allocated
    NvBool                  bBuffersNegotiationCompleteEventSent; //!< NV_TRUE, if buffers Negotiation complete event sent
    NvBool                  bDirectTunnel;           //!< NV_TRUE, if buffers are allocated
    NvU8                    bStreamShuttingDownEventSent;
    NvU8                    bEndOfStreamEventSent;       //!< NV_TRUE, if NvMMEvent_StreamEnd sent
    NvU8                    ConfigRetry;                 /**< Buffer config retry count, which was send to NvMMBlockContext::GetBufferRequirements().
                                                              If block is allocator, this specifies the retry count,
                                                              for which NvMMBlockNegotiateBufferConfig() succeeded.
                                                              If block is non-allocator, this specifies the currently
                                                              communicated requirements set.
                                                         */
                                                         
    NvBool                  bBufferAllocationFailed;
    NvS64                   LastTimeStamp;                    //!< Current stream position in bytes
    // large members go last for performance
    NvMMBuffer*             pBuf[NVMMSTREAM_MAXBUFFERS]; /**< Buffer headers for buffers allocated by owning block.
                                                              Array contains heap-block pointers.
                                                              Actually used array size is NvMMStream::NumBuffers.
                                                         */
    NvMMNewBufferRequirementsInfo  OutgoingBufReqs;      //!< defined, if outgoingRequirements.structSize != 0, used only by the allocator
    NvMMNewBufferConfigurationInfo BufConfig;            //!< negotiated buffer config, valid if bufferConfig.structSize != 0, used only by the allocator

} NvMMStream;

/** Test if NvMMStream is connected.
    @param (NvMMStream*) Stream instance
    @return (NvBool) NV_TRUE if connected, NV_FALSE otherwise
*/
#define NvMMStreamIsConnected(pStream) ((pStream)->TransferBufferFromBlock != NULL)

/** Test if NvMMStream has reached end of stream.
    @param (NvMMStream*) Stream instance
    @return (NvBool) NV_TRUE if connected, NV_FALSE otherwise
*/
#define NvMMStreamIsEOS(pStream) ((int)(pStream)->bEndOfStream | (int)(pStream)->bEndOfStreamEventSent)

/** NvMMBlockContext::PrivateClose callback.
    @param hBlock Block handle
*/
typedef void (*NvMMPrivateCloseFunction)(NvMMBlockHandle hBlock);

/** NvMMBlockContext::ProcessData callback.
    This function may be called from multiple threads.
    @param hBlock Block handle
    @return NvSuccess indicates success with no more work pending. NvError_Busy indicates success with
            more work pending, the framework will call the function again. Any other error code
            is interpreted as serious runtime error causing block shutdown.
*/
typedef NvError (*NvMMBlockDoWorkFunction)(NvMMBlockHandle hBlock, NvMMDoWorkCondition condition);

/** NvMMBlockContext::GetBufferRequirements callback.
    This function is called from the baseclass to enquire new or changed buffer requirements from the derived block.

    An implementation must check the \a Retry parameter and provide a uniqueue and constant requirements set
    for each setting, and return NV_TRUE. If \a Retry exceeds the number of supported configurations, the
    implementation must return NV_FALSE. \a Retry 0 must always succeed.

    This callback will be called with the block mutex locked, so it should return quickly.

    This callback can be called multiple times during a single negotiation to enquire alternate buffer configurations.
    The \a Retry parameter will count the retries, starting at 0.

    If a block supports alternate buffer configs, it will usually return the most favourable one
    on Retry 0, and fallback to more relaxed ones for higher retry counts.

    @param hBlock      Block handle
    @param StreamIndex Stream index to get requirements for
    @param Retry       Retry count, starting at 0
    @param pBufReq     Uninitialized info structure to be filled by the block implementation
    @return NV_TRUE if a requirement set was returned, or NV_FALSE if \a Retry exceeded the number of
            supported configurations.
*/
typedef NvBool (*NvMMGetBufferRequirementsFunction)(NvMMBlockHandle hBlock, NvU32 StreamIndex, NvU32 Retry, NvMMNewBufferRequirementsInfo *pBufReq);

/**
 *
 * @brief Transfer buffer event function - Event handler for derived blocks if needed.
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
 * @param bEventHandled Returns true if event is handled by derived block.
 * @retval NvSuccess Transfer was successfully.
 *
 * @ingroup mmblock
 *
 */
typedef
NvError
(*NvMMTransferBufferEventFunction)(
    void *pContext,
    NvU32 StreamIndex,
    NvMMBufferType BufferType,
    NvU32 BufferSize,
    void *pBuffer,
    NvBool *bEventHandled);

/** NvMMBlock base class.

    Provides common functionality for NvMMBlock implementations:
    - state handling
    - stream and buffer handling

    <b>Object derivation model</b>

    If an NvMMBlock implementation derives from this class, it must
    include struct NvMMBlockContext as first member of its block context:

    <pre>
    typedef struct RefBlockContextRec
    {
        NvMMBlockContext block;

        <other block specific context data>

    } RefBlockContext;
    </pre>

    By doing so, the NvMMBlockContext methods can down-cast a derived block context
    pointer from NvMMBlock::pContext (e.g. RefBlockContext*) to NvMMBlockContext*.
*/
typedef struct NvMMBlockContextRec
{
    //
    // Data members
    //

    NvMMStream**            pStreams;               /**< Pointer to stream index in heap block.
                                                         StreamIndex is index into array. Array items
                                                         can be NULL for non-instantiated streams.
                                                    */
    NvU32                   StreamCount;            //!< mpStreams[] array size (not number of instanciated streams)
    NvMMState               State;                  //!< Current block state
    NvRmDeviceHandle        hRmDevice;              //!< RM handle, can be NULL if no RM functions needed
    NvOsMutexHandle         hBlockMutex;            //!< Mutex for multi-threaded block access
    NvOsMutexHandle         hBlockCloseMutex;       //!< Mutex for synchronizing block closing   
    NvOsSemaphoreHandle     hBlockEventSema;
    NvU8                    ProfileStatus;          //!< Bitfield of NvMMProfileType values
    NvU8                    bDoBlockClose;          //!< NV_TRUE if delayed block destruction is initiated
    NvU8                    bBlockProcessingData;   //!< NV_TRUE if ProccessData function is processing (to prevent recursion)
    void*                   pSendEventContext;      //!< Context to be passed to NvMMBlockContext::SendEvent calls

    NvMMInternalCreationParameters  params;         //!< Copy of block internal creation parameters
    NvMMPowerState          PowerState;             //!< Holds the value of powerstate

    NvMMBlockType           BlockType;              //!< BlockType of the derived block
    NvMMLocale              eLocale;                //!< Locale under which the block will run
    NvBool                  bDelayedAlloc;          //!< NV_TRUE for delayed (not at time of buffer negotiation) block allocation
#if NVMM_BUFFER_PROFILING_ENABLED
    NvError (*NvMMBlockTransferBufferToBlockCallBack) //!< Callback function of the derived block when TBF is called
            (void *pContext, NvU32 StreamIndex);
#endif
    NvError (*NvMMBlockTBTBCallBack) //!< Callback function to the derived function to pass NvMM buffer along with it
            (void *pContext, NvU32 StreamIndex, NvMMBuffer* buffer);

    //
    // Interface
    //

    /** Block process data callback. */
    NvMMBlockDoWorkFunction DoWork;

    /** Block event callback, registered by block client via NvMMBlock::SetSendBlockEventFunction(). */
    NvMMSendBlockEventFunction SendEvent;

    /** Private block close callback for delayed block destrution from within NvMMBlockTransferBufferToBlock() */
    NvMMPrivateCloseFunction PrivateClose;

    /** Get new buffer requirements callback. */
    NvMMGetBufferRequirementsFunction GetBufferRequirements;

    /** Handle block specific events */
    NvMMTransferBufferEventFunction TransferBufferEventFunction;

    /* status signifying ULP mode */
    NvBool                  bEnableUlpMode;

   /* Flag to disable stream events */
    NvBool                  bSendStreamEventNotifications;

     /* Flag to disable stream events */
    NvBool                  bSendBlockEventNotifications;

    /* Flag to set  Abnormal Termination */
    NvBool bAbnormalTermination;

} NvMMBlockContext;


/** NvMMBlock baseclass open function.

    This function must be called at the beginning of a derived NvMMBlock's open function.

    It performs the following operations:
    - Allocate memory for struct NvMMBlock and return handle to that struct in \a *phBlock
    - Populate NvMMBlock with NvMMBlockContext function pointers. The functions are the
      NvMMBlock default implementations. If a derivated block wishes to override these
      implementations, it can overwrite the function pointers with its own implemenations.
    - Allocate memory for derived block context (\a SizeOfBlockContext bytes) and store
      pointer to that struct into \a phBlock->pContext
    - Initialize NvMMBlockContext members with default values and NvMMBlockOpen() function parameters

    On function failure no memory is allocated.
  
    @param phBlock              Returns block handle, not changed on failure
    @param SizeOfBlockContext   Size of derived block context struct in bytes
    @param hSemaphore           Block's semaphore used as a trigger when for any external event
                                which requires the block to do more work, e.g. an interupt fired
                                upon the hardware's completion of work.
    @param DoWorkFunction       Process data callback
    @param PrivateCloseFunction Delayed block close callback
*/ 
NvError NvMMBlockOpen(NvMMBlockHandle *phBlock,
                      NvU32 SizeOfBlockContext,
                      NvMMInternalCreationParameters* pParams,
                      NvOsSemaphoreHandle hSemaphore,
                      NvMMBlockDoWorkFunction DoWorkFunction,
                      NvMMPrivateCloseFunction PrivateCloseFunction,
                      NvMMGetBufferRequirementsFunction GetBufferRequirements);

/** NvMMBlock baseclass close function.

    This function must be called at the end of a derived NvMMBlock's close function.

    It performs the following operations:
    - free memory for hBlock->pContext and hBlock structs

    This function is safe to call on an already closed block.

    @param hBlock Block handle, can be NULL
*/
void NvMMBlockClose(NvMMBlockHandle hBlock);

/** NvMMBlock help to initiate delayed block close.

    Try to close the block. If possible block will be closed here,
    otherwise it will be marked for close. Block will be closed whenever 
    possible in future

    Call this from a block implementations Close function.

    @param hBlock Block handle
*/
void NvMMBlockTryClose(NvMMBlockHandle hBlock);

/** Default NvMMBlock::SetState implementation.

    This function implements the NvMMBlock::SetState for the NvMMBlock pointer
    handed back on the block open call. This function allows a client to
    change the state of the block. Aside from saving the state (which other
    parts of the block will use to decide on behavior, e.g. to process data
    or not) a block need only reinitialize internal state when transitioning
    into stopped.

    The default implementation saves the new state.
    If the state is changed to NvMMState_Stopped, all instanciated streams are set to position 0.
    If the state is changed to NvMMState_Running, NvMMBlockContext::BlockEventSema will be signalled. xxx clarify

    @param hBlock Block handle
    @param eState New block state
*/
NvError NvMMBlockSetState(NvMMBlockHandle hBlock, NvMMState eState);

/** Default NvMMBlock::GetState implementation.

    This function implements NvMMBlock::GetState for the NvMMBlock pointer
    handed back on the block open call. This function queries the current
    block state.

    A custom block is unlikely to change this much. The block
    has nothing to do here other than to retrieve the block state and return
    it. Block can not change the state by itself so block will not receive
    getState function call from the client.

    @param hBlock Block handle
    @param peState Returns state
*/
NvError NvMMBlockGetState(NvMMBlockHandle hBlock, NvMMState *peState);

/** Default NvMMBlock::SetAttribute implementation.

    This function sets the value of attribute supported by the block. This equates
    to a mapping of the passed value to internal state. The block should apply this
    attribute to internal state immediately given that this call is serialized with
    other calls in the block's context. The list of supported attributes is block
    specific.

    @param hBlock Block handle
*/
NvError NvMMBlockSetAttribute(NvMMBlockHandle hBlock, NvU32 AttributeType, NvU32 SetAttrFlag, NvU32 AttributeSize, void *pAttribute);

/** Default NvMMBlock::GetAttribute implementation.

    This function queries the value of attribute supported by the block.
    This equates to a look up based on the requested attribute. The list
    of supported attributes is block specific.

    @param hBlock Block handle
*/
NvError NvMMBlockGetAttribute(NvMMBlockHandle hBlock, NvU32 AttributeType, NvU32 AttributeSize, void *pAttribute);

/** Default NvMMBlock::SetBufferAllocator implementation.

    NvMMBlockCreateStream() initializes the block not to be the allocator.

    If bAllocateBuffers is true then the block is the allocator.
    See also NvMMBlock::SetBufferAllocator().

    @param hBlock Block handle
    @param StreamIndex Stream index to operate on

    @retval NvSuccess Success
    @retval NvErrorBadParameter StreamIndex out of range
*/ 
NvError NvMMBlockSetBufferAllocator(NvMMBlockHandle hBlock, NvU32 StreamIndex, NvBool bAllocateBuffers);

/** Default NvMMBlock::SetBufferAllocator implementation.

    A method to get transferbuffer fuction from block. This will be used to
    make connection between block/client.

    This default implementation just returns NvMMBlockTransferBufferToBlock.

    @param hBlock Block handle
    @param StreamIndex Stream index to get transfer function for
    @param pContextForTranferBuffer Context pointer for the block to fill.
                                    The client may use this to set up tunnel with other blocks or client itself.
*/
NvMMTransferBufferFunction NvMMBlockGetTransferBufferFunction(NvMMBlockHandle hBlock, NvU32 StreamIndex, void *pContextForTranferBuffer);

/** Default NvMMBlock::SetTransferBufferFunction implementation.

    A custom block should at a minimum cache the values passed. It can, however, defer the
    communication of the buffer requirements to a later if it isn't yet aware
    of them. Nevertheless, the block should be aware that buffers may not be
    allocated until requirements are known so requirements should be
    communicated as soon as they are known. It is the obligation of the block
    to *eventually* send buffer requirements once SetTransferBufferFunction
    is called. If the block doesn't care much about requirements then it can
    simply send very liberal requirements back.                               

    @param hBlock Block handle
    @param StreamIndex Stream index to set transfer function for
    @param TransferBuffer New transfer buffer function
    @param pContextForTranferBuffer User data to pass on \a TransferBuffer calls
    @param StreamIndexForTransferBuffer Stream index of other side
*/
NvError NvMMBlockSetTransferBufferFunction(NvMMBlockHandle hBlock, 
                                           NvU32 StreamIndex,
                                           NvMMTransferBufferFunction TransferBuffer,
                                           void* pContextForTranferBuffer,
                                           NvU32 StreamIndexForTransferBuffer);

/** Default NvMMBlock::SendBlockEventFunction implementation.

    This default implementation just stores context and function pointer into
    the NvMMBlockContext structure.

    Note that there are two types of events, stream events sent via TransferBufferFunction
    calls, and block events sent from the block to the client via the
    callback registered here. A custom block is unlikely to change this much.
    The block has little to do here other than save the new value for the
    event handler.

    @param hBlock Block handle
*/
void NvMMBlockSetSendBlockEventFunction(NvMMBlockHandle hBlock, void* ClientContext, SendBlockEventFunction SendEvent);

/** Default implementation for buffer receive function.

    This function implements the NvMMBlock.TransferBufferToBlock for the
    NvMMBlock pointer handed back on the block open call. The client uses
    this call to send the block a buffer or an event associated with a
    stream. All incoming data can be processed immediately (if the block
    state is right). We queue the data just in case we don't yet have
    sufficient input and output data to do some work but process that data 
    as soon as we do.

    Some possible variations a custom block might implement include:
    - giving buffers to hardware acceleration without queuing in software
    - repacking data into one large buffer (e.g. if data does not come in
     intact frames yet that's the way a codec expects it)
*/
NvError NvMMBlockTransferBufferToBlock(void* hBlock, NvU32 StreamIndex, NvMMBufferType BufferType, NvU32 BufferSize, void* pBuffer);

/** Default NvMMBlock::AbortBuffers implementation.

    This function triggers the block to return all buffers to its connected block/client

    @param hBlock Block handle
    @param StreamIndex Stream index to operate on
*/
NvError NvMMBlockAbortBuffers(NvMMBlockHandle hBlock, NvU32 StreamIndex);

/** Default implementation for block DoWork function.

    xxx

    @param hBlock Block handle
    @param Flag xxx
    @param pMoreWorkPending xxx
*/
NvError NvMMBlockDoWork(NvMMBlockHandle hBlock, NvMMDoWorkCondition Flag, NvBool *pMoreWorkPending);

/** Create and open a stream on the block.
    Will allocate and initialize a NvMMStream struct for given StreamIndex in NvMMBlockContext::pStreams[].
    @param hBlock Block handle
    @param StreamIndex Stream index to assign to created stream
    @param Direction Stream direction
    @param MaxBufferCount Maximum number of supported buffers
*/
NvError NvMMBlockCreateStream(NvMMBlockHandle hBlock, NvU32 StreamIndex, NvMMStreamDir Direction, NvU32 MaxBufferCount);

/** Destroy stream on the block.
    @param hBlock Block handle
    @param StreamIndex Stream index to destroy, can be invalid
*/
void NvMMBlockDestroyStream(NvMMBlockHandle hBlock, NvU32 StreamIndex);

/** Get stream by index.
    This function can also be used to test, if a given stream is created.
    @param pContext    (NvMMBlockContext*) Block context pointer
    @param StreamIndex (NvU32) Stream index
    @return            (NvMMStream*) Pointer to stream, or NULL if \a StreamIndex was invalid
*/
#define NvMMBlockGetStream(pContext, StreamIndex) \
    (((StreamIndex)<(pContext)->StreamCount) ? (pContext)->pStreams[StreamIndex] : NULL)

/*
 * Allocate the buffers for the given stream. Only called when the block
 * recieves an affirmative reply to the suggested buffer config for the
 * entity it is sharing the stream with. A custom block wouldn't allocate
 * the buffers much differently since the config has now been established
 * on both sides. However, a custom block is free to allocate or transfer
 * buffers as late as it wants. Note, of course, that holding off buffer
 * allocation results in effectively holding off block operation.

    @param hBlock Block handle
    @param pStream Stream to allocate buffers for
 */
NvError NvMMBlockAllocateBuffers(NvMMBlockHandle hBlock, NvMMStream *pStream);

/** Try to delete all buffers allocated by block.
    This method should be called only if block is an allocator.
    It will dequeue buffers from queue and delete it.

    Note that this will not delete all, but only those buffers which are in the
    incoming stream queue. This function needs to be called over and over again,
    until NvMMBlockAreAllBuffersDeleted returns NV_TRUE.  xxx: not effective, fix this

    @param hBlock Block handle
    @param StreamIndex Stream index 
 */
void NvMMBlockDeallocateBuffers(NvMMBlockHandle hBlock, NvU32 StreamIndex);

/** Return all buffers to the allocator.
    This method should be called only if block is an allocator.
    It will return buffers from queue to allocator through transfer buffer function.
    @param hBlock Block handle
    @param StreamIndex Stream index 
 */
NvError NvMMBlockReturnBuffersToAllocator(NvMMBlockHandle hBlock, NvU32 StreamIndex);

/** Check whether all buffers are deleted, if we are the allocator.
    This method should be called only if block is an allocator.
    @param hBlock Block handle
    @param StreamIndex Stream index to check
 */
NvBool NvMMBlockAreAllBuffersDeleted(NvMMBlockHandle hBlock, NvU32 StreamIndex);

/** Check whether all buffers are returned to the allocator.
    This method should be called only if block is not an allocator.
    @param hBlock Block handle
    @param StreamIndex Stream index to check
 */
NvBool NvMMBlockAreAllBuffersReturned(NvMMBlockHandle hBlock, NvU32 StreamIndex);

/** Send end of stream event (NvMMEvent_StreamEnd).
    @param hBlock  Block handle
    @param pStream Stream to send event on
*/
NvError NvMMBlockSendEOS(NvMMBlockHandle hBlock, NvMMStream* pStream);

/** This routine is called to send audio data to the mixer.

    It is a generic routine and should not contain any variables that are codec specific.
    All codecs should use this routine to send data to the Mixer module.

    @param pNextOutBuf    pointer to the buffer structure to send
    @param pOutStream     pointer to the stream structure
    @param rate           rate to adjust the sample rate by
    @param entriesOutput  number of output entries available
    @param sampleRate     sample rate of the audio data, 8kHz thru 48kHz usually
    @param numChannels    number of channels of the audio data 1 - mono, 2 - stereo
    @param bitPerSample   number of bits of audio data per sample, 8 or 16 only
    @param pAdjSampleRate pointer to a variable to store the calculated adjusted sample rate, can be zero
    @param deQueueBuffer  flag, set to true to de-queue current buffer (optional depending on codec)
*/
NvError NvMMSendDataToMixer(NvMMBuffer *pNextOutBuf, NvMMStream *pOutStream, NvU32 rate, NvU32 entriesOutput, NvU32 sampleRate, NvU32 numChannels, NvU32 bitsPerSample, NvS32 *pAdjSampleRate, NvBool deQueueBuffer);


#if defined(__cplusplus)
}
#endif

/** @} */

#endif /* INCLUDED_NVMM_BLOCK_H */

