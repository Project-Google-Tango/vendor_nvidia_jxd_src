/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * This NvMM block is provided as a reference to block writers. As such it
 * is a demonstration of correct semantics. Implementation of so called
 * "custom blocks" may vary.
 * Note that the calls on the reference block are serviced synchronously.
 * This is by design. A layer leveraging the Transport API operates between
 * the client and the block. Transparently to the block implementation, this
 * layer provides a block worker thread and makes all calls to the block
 * on this thread. Synchronous operation, e.g. without the Transport API as
 * a middleman, is also possible using the same code. The block need not
 * concern itself with the context under which it is run, any threading, or
 * messages. The block need only service calls synchronously and the rest
 * will be taken care of by whatever has created it, e.g. the transport API.
 */


#include "nvmm.h"
#include "nvmm_event.h"
#include "nvrm_transport.h"
#include "nvmm_block.h"
#include "nvmm_queue.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvmm_debug.h"
#include "nvmm_util.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm_basewriterblock.h"
#include "nvmm_mtswriterblock.h"
#include "nv_mts_writer.h"


#define NVMM_MTSWRITER_MAX_TRACKS 2
#define NVMM_MTSWRITER_WORK_BUFFER_SIZE (400*1024)
#define TICKS_PER_SECOND 1000000
// Buffer Sizes
#define MAX_CHANNELS          6    /* can be set to 1 for a mono encoder */

#define VIDEOENC_MAX_OUTPUT_BUFFERS 5
#define VIDEOENC_MIN_OUTPUT_BUFFERS 3

typedef enum NvMMmtsWriterState
{
    WRITER_INIT,
    WRITER_DATA,
    WRITER_EOS
} NvMMmtsWriterState;


typedef struct NvMMMTSWriterStreamInfo
{
  NvMMWriterStreamConfig streamConfig;
  NvMMmtsWriterState streamState;
} NvMMMTSWriterStreamInfo;


/** Context for MTS writer block.

*/
typedef struct
{
    NvMMBaseWriterBlockContext *block; //!< NvMMBlock base class, must be first member to allow down-casting
    NvString szFilename;
    //NvMM3GPWriterStreamInfo streamInfo[NVMM_3GPWRITER_MAX_TRACKS];
    NvS32 streamIndex;
    CPhandle cphandle;
    CP_PIPETYPE_EXTENDED *pPipe;
    // Params for MTS core library
    structInpParam oInputParam;
    tmem           omtsMem;
    structSVParam  oSVParam;
    // First Video frame Timestamp
    NvU64     uFirstFrameTimeStamp;
    NvU32 nBufferSize;
    NvU32 nPageSize;
    NvU32 nBufferDataSize;
    NvU8 *pmtsBufferAlloc;
    NvU8 *pmtsBuffer;
    NvBool bmtsInitialized;
    NvBool audioPresent;
    NvBool videoPresent;
} NvMMmtsWriterBlockContext;

/*
 * INTERNAL TYPEDEFS/DEFINES
 *
 * A custom block may use these values to the extent
 * that its implementation resembles the reference.
 * Even so, the values will likely differ in a custom
 * block.
 */

// prototypes for  implementation of mts writer functions
NvError NvMMmtsWriterSetStreamConfig(NvMMBlockHandle hBlock, NvMMWriterStreamConfig *pConfig);
NvError NvMMmtsWriterSetUserData(NvMMBlockHandle hBlock, NvMMWriteUserDataConfig *pConfig);
NvError NvMMmtsWriterAddMediaSample(NvMMBlockHandle hBlock, NvU32 streamIndex, NvMMBuffer *pBuffer);
NvError NvMMmtsOpenWriter(NvMMBlockHandle hBlock, NvString pURI);
NvError NvMMmtsCloseWriter(NvMMBlockHandle hBlock);
NvBool  NvMMmtsWriterBlockGetBufferRequirements( NvMMBlockHandle hBlock,
                                                 NvU32 StreamIndex,
                                                 NvU32 Retry,
                                                 NvMMNewBufferRequirementsInfo *pBufReq);



/**
 *  Constructor for the block which is used by NvMMOpenBlock.
 */
NvError NvMMmtsWriterBlockOpen(NvMMBlockHandle *phBlock,
                               NvMMInternalCreationParameters *pParams,
                               NvOsSemaphoreHandle BlockEventSema,
                               NvMMDoWorkFunction *pDoWorkFunction)
{
    NvError status = NvSuccess;
    return status;
}

NvBool  NvMMmtsWriterBlockGetBufferRequirements(NvMMBlockHandle hBlock,
                                                NvU32 StreamIndex,
                                                NvU32 Retry,
                                                NvMMNewBufferRequirementsInfo *pBufReq)
{
    return NV_TRUE;
}


/** Close function which is called by NvMMCloseBlock().

    If possible block will be closed here, otherwise it will be marked for close.
    Block will be closed whenever possible in future.
*/
void NvMMmtsWriterBlockClose(NvMMBlockHandle hBlock,
                             NvMMInternalDestructionParameters *pParams)
{
     NvMMBlockTryClose(hBlock);
}

NvError NvMMmtsOpenWriter(NvMMBlockHandle hBlock, NvString szURI)
{
    NvError status = NvSuccess;
    return status;
}


NvError NvMMmtsWriterSetStreamConfig(NvMMBlockHandle hBlock, NvMMWriterStreamConfig *pConfig)
{
    NvError status = NvSuccess;
    return status;
}


NvError NvMMmtsWriterAddMediaSample(NvMMBlockHandle hBlock, NvU32 streamIndex, NvMMBuffer *pBuffer)
{
    NvError status = NvSuccess;
    return status;
}





