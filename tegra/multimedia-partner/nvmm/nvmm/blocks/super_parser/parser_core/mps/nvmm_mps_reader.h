/**
 * @file nvmm_mps_reader.h
 * @brief <b>NVIDIA Media Parser Package:mps reader/b>
 *
 * @b Description:   This file implements mps reader
 */

/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all NvS32ellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVMM_MPS_READER_H
#define INCLUDED_NVMM_MPS_READER_H

#include "nvmm.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm_mps_defines.h"

/* Pack header */
typedef struct NvMpsPackHeadRec
{
    eNvMpsMpegType eType;
    NvU64 lScr;
    NvU64 lScrBase;
    NvU64 lScrExt;
    NvU64 uMuxRate;
    NvU32 uPaddingLength;
} NvMpsPackHead;

/* System header */
typedef struct NvMpsSysHeadRec
{
    NvU32 uHeadLength;
} NvMpsSysHead;

/* PES header */
typedef struct NvMpsPESHeadRec
{
    eNvMpsMpegType eType;
    NvU32 uStreamID;
    NvU32 uPacketLength;

    NvBool bAVStream;
    NvBool bPtsValid;
    NvU64 lPts;
    NvBool bDtsValid;
    NvU64 lDts;

    NvU64 lFileOffsetForData;
    NvU32 uDataSize;
    NvU64 lFileOffsetForHead;
} NvMpsPESHead;

/* NvMpsReader uses a scratch buffer to handle all File related and sync search opearations */
typedef struct NvMpsReaderRec{

    NvU8 * pScratchBufBase;
    NvU8 * pScratchBufPtr;
    NvU32 uScratchBufCurrentSize;
    NvU32 uScratchBufMaxSize;
    NvU32 uUnParsedScratchSize;

    CPhandle cphandle;
    CP_PIPETYPE *pPipe;
    NvU64 lFileSize;
    NvU64 lCurrFileOffSet;
    NvU64 lCurrParseOffSet;

    NvMpsPackHead *pPackHead;
    NvMpsSysHead  *pSysHead;
    NvMpsPESHead  *pPESHead;

    /*************These are low level functions*****************/

    /* Mark bytes size data as parsed */
    NvError (*MarkBytesParsed)(struct NvMpsReaderRec *pReader, NvU32 bytes);

    /* Read next data into scratch buffer.
     * Possible values of peRes
       NV_MPS_RESULT_OK
       NV_MPS_RESULT_END_OF_FILE
       If uRequested = 0, maximum possible size is read.
       pointer read could be NULL
     */
    NvError (*ReadForward)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvU32 uRequested, NvU32 *puRead);

    /*Ensure that scratch buffer has requested size */
    NvError (*EnsureDataAvailability)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvU32 uRequested);

    NvError (*SeekNextSyncCodeInScratch)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvU32 uLimit);


    /*************These high level functions used by NvMpsParser *****************/

    NvError (*Init) (struct NvMpsReaderRec *pReader, CP_PIPETYPE *pPipe, CPhandle cphandle);

    /* offset = Absolute offset from the beginning of the file. */
    NvError (*SetReaderPosition)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvU64 lOffset);

    /* These peekparse functions do actual parsing only if bParse is set to NV_TRUE.
     * Possible return values of peRes are:
     *  NV_MPS_RESULT_OK
     *  NV_MPS_RESULT_END_OF_FILE (EOF reached before peeking/parsing header completely)
     *  NV_MPS_RESULT_BOGUS_HEADER (Some legal header requirements (like marker bits) were not met)
     */
    NvError (*PeekParsePackHead)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvBool bParse);
    NvError (*PeekParseSysHead)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvBool bParse);
    NvError (*PeekParsePESHead)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvBool bParse);

    /* These seek functions are used to seek to next sync code from current position.
     * Possible return values of peRes are:
     * NV_MPS_RESULT_OK
     * NV_MPS_RESULT_SYNC_NOT_FOUND  (within specifid limit uLimit)
     * NV_MPS_RESULT_END_OF_FILE (EOF reached before limit and sync is not found)
     */
    NvError (*SeekNextSyncCode)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvU32 uLimit);
    NvError (*SeekNextPackCode)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvU32 uLimit);
    NvError (*SeekNextPESCode)(struct NvMpsReaderRec *pReader, eNvMpsResult *peRes, NvU32 uLimit);

    /* This function copies data directly to external buffer without using internal scratch.
     * This function is used to copy actual PES data
     */
    NvError (*CopyDataToExternalBuffer)(struct NvMpsReaderRec *pReader, NvU8 *pBuf, NvU64 lOffset, NvU32 *pSize);

}NvMpsReader;

NvMpsReader *NvMpsReaderCreate(NvU32 uScratchBufMaxSize);
void NvMpsReaderDestroy(NvMpsReader *pReader);

#endif //INCLUDED_NVMM_MPS_READER_H
