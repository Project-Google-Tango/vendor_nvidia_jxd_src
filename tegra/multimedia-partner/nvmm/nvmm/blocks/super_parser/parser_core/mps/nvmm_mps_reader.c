
/**
* @nvmm_mps_reader.c
* @brief IMplementation of mps reader
*
* @b Description: Implementation of mps reader
*
*/

/*
* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#include "nvmm_mps_reader.h"
//#include "nvos.h"

/* Macro functions used by reader */
#define NV_MPS_CHECK_MARKER_BITS(uOffset)   if (((pReader->pScratchBufPtr[uOffset] & 0x01) != 0x01) || \
                                                ((pReader->pScratchBufPtr[uOffset+2] & 0x01) != 0x01) || \
                                                ((pReader->pScratchBufPtr[uOffset+4] & 0x01) != 0x01)) { \
                                                    *peRes = NV_MPS_RESULT_BOGUS_HEADER; \
                                                    return NvSuccess; \
                                            }

#define NV_MPS_READ_PTS(pts, uOffset)       pts =   (((NvU64)(pReader->pScratchBufPtr[uOffset] & 0x0E)) << 29) | \
                                                    (((NvU64)(pReader->pScratchBufPtr[uOffset+1])) << 22) | \
                                                    (((NvU64)(pReader->pScratchBufPtr[uOffset+2] & 0xFE)) << 14) | \
                                                    (((NvU64)(pReader->pScratchBufPtr[uOffset+3])) << 7) | \
                                                    (((NvU64)(pReader->pScratchBufPtr[uOffset+4] & 0xFE)) >> 1);

static NvError NvMpsReaderMarkBytesParsed(NvMpsReader *pReader, NvU32 bytes)
{
    if (bytes <= pReader->uUnParsedScratchSize)
    {
        pReader->uUnParsedScratchSize -= bytes;
        pReader->pScratchBufPtr += bytes;
        pReader->lCurrParseOffSet += bytes;

        return NvSuccess;
    }
    NvOsDebugPrintf("Fatal Error in MPEG-PS parser: NvMpsReaderMarkBytesParsed!\n"); //This should never occur
    return NvError_ParserFailure;
}


static NvError NvMpsReaderReadForward(NvMpsReader *pReader, eNvMpsResult *peRes, NvU32 requested, NvU32 *read)
{
    NvU32 lReadSize;
    NvU32 lAvailableSize;
    NvError status;

    if (read)
        *read = 0;

    lAvailableSize = pReader->lFileSize - pReader->lCurrFileOffSet;
    if (lAvailableSize == 0)
    {
        *peRes = NV_MPS_RESULT_END_OF_FILE;
        return NvSuccess;
    }

    NvOsMemmove(pReader->pScratchBufBase, pReader->pScratchBufPtr, pReader->uUnParsedScratchSize);
    pReader->pScratchBufPtr = pReader->pScratchBufBase;
    lReadSize = pReader->uScratchBufMaxSize - pReader->uUnParsedScratchSize;

    if (lReadSize == 0)
    {
        NvOsDebugPrintf("Fatal error in MPEG-PS parser NvMpsReaderReadForward!\n"); //This should never occur
        return NvError_ParserFailure;
    }

    if (lReadSize >  lAvailableSize)
    {
        lReadSize = lAvailableSize;
    }

    if ((requested != 0) && (lReadSize >  requested))
    {
        lReadSize = requested;
    }

    status = pReader->pPipe->Read(pReader->cphandle,
                (CPbyte*)pReader->pScratchBufBase + pReader->uUnParsedScratchSize,
                (CPuint)lReadSize);
    if (status != NvSuccess)
    {
        return status;
    }

    pReader->uUnParsedScratchSize += lReadSize;
    pReader->uScratchBufCurrentSize = pReader->uUnParsedScratchSize;
    pReader->lCurrFileOffSet += lReadSize;

    if (read)
        *read = lReadSize;
    *peRes = NV_MPS_RESULT_OK;

    return NvSuccess;
}

static NvError NvMpsReaderEnsureDataAvailability(NvMpsReader *pReader, eNvMpsResult *peRes, NvU32 uRequested)
{
    NvError status;

    while (pReader->uUnParsedScratchSize < uRequested)
    {
        status = pReader->ReadForward(pReader, peRes, 0, NULL);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }

    return NvSuccess;
}

static NvError NvMpsReaderSeekNextSyncCodeInScratch(NvMpsReader *pReader, eNvMpsResult *peRes, NvU32 uLimit)
{
    NvU32 uRemaining;
    NvU8 *pBuf;
    NvU32 uBytesParsed;
    NvBool bFound = NV_FALSE;

    *peRes = NV_MPS_RESULT_OK;

    uRemaining = pReader->uUnParsedScratchSize;

    if (uRemaining <= NV_MPS_SYNC_MARKER_SIZE)
    {
        *peRes = NV_MPS_RESULT_NEED_MORE_DATA;
        return NvSuccess;
    }

    if (uRemaining > uLimit)
    {
        uRemaining = uLimit;
    }

    pBuf = pReader->pScratchBufPtr;
    uRemaining -= NV_MPS_SYNC_MARKER_SIZE;
    uBytesParsed = 0;

    while(uBytesParsed < uRemaining)
    {
        if (pBuf[0] == 0 && pBuf[1] == 0 && pBuf[2] == 1)
        {
            bFound = NV_TRUE;
            break;
        }
        pBuf++;
        uBytesParsed++;
    }

    uBytesParsed += NV_MPS_SYNC_MARKER_SIZE - 1;
    pReader->pScratchBufPtr += uBytesParsed;
    pReader->lCurrParseOffSet += uBytesParsed;
    pReader->uUnParsedScratchSize -= uBytesParsed;

    if (bFound == NV_FALSE)
    {
        if ((uRemaining + NV_MPS_SYNC_MARKER_SIZE) == uLimit)
        {
            *peRes = NV_MPS_RESULT_SYNC_NOT_FOUND;
        }
        else
        {
            *peRes = NV_MPS_RESULT_NEED_MORE_DATA;
        }
    }

    return NvSuccess;
}

static NvError NvMpsReaderSetReaderPosition(NvMpsReader *pReader, eNvMpsResult *peRes, NvU64 lOffset)
{
    NvError status;
    *peRes = NV_MPS_RESULT_OK;

    if (lOffset > pReader->lFileSize)
    {
        *peRes = NV_MPS_RESULT_END_OF_FILE;
        return NvSuccess;
    }

    /* Check if requested position is within current scratch buffer */
    if ((pReader->lCurrFileOffSet > lOffset) &&
        ((pReader->lCurrFileOffSet - pReader->uScratchBufCurrentSize) <= lOffset))
    {
        pReader->lCurrParseOffSet = lOffset;
        lOffset = pReader->lCurrFileOffSet - lOffset;
        pReader->pScratchBufPtr = pReader->pScratchBufBase + pReader->uScratchBufCurrentSize - lOffset;
        pReader->uUnParsedScratchSize = lOffset;
        return NvSuccess;
    }

    /* Need to flush scratch buffer and set file position */
    status = pReader->pPipe->SetPosition(pReader->cphandle, lOffset, CP_OriginBegin);
    if(status != NvSuccess)
    {
        return status;
    }

    pReader->lCurrFileOffSet = lOffset;
    pReader->lCurrParseOffSet = lOffset;
    pReader->pScratchBufPtr = pReader->pScratchBufBase;
    pReader->uScratchBufCurrentSize = 0;
    pReader->uUnParsedScratchSize = 0;

    return NvSuccess;
}


static NvError NvMpsReaderCopyDataToExternalBuffer(NvMpsReader *pReader, NvU8 *pBuf,
                                            NvU64 lOffset, NvU32 *pSize)
{
    NvU8 *pStart;
    NvU32 uPartialData = 0;
    NvError status;
    CPuint pos;

    /*Check if lOffset is within scratch buffer */
    if ((pReader->lCurrFileOffSet > lOffset) &&
        ((pReader->lCurrFileOffSet - pReader->uScratchBufCurrentSize) <= lOffset))
    {
        pStart = pReader->pScratchBufBase + pReader->uScratchBufCurrentSize -
                        (pReader->lCurrFileOffSet - lOffset);
        if ((pReader->pScratchBufBase + pReader->uScratchBufCurrentSize) > (pStart + *pSize))
        {
            /* Entire data is in scratch buffer. mem copy */
            NvOsMemcpy(pBuf, pStart, *pSize);
            /* Adjust pScratchBufPtr and uScratchBufCurrentSize */
            pReader->pScratchBufPtr = pStart + *pSize;
            pReader->uUnParsedScratchSize = pReader->pScratchBufBase +
                pReader->uScratchBufCurrentSize - pReader->pScratchBufPtr;
            pReader->lCurrParseOffSet = lOffset + *pSize;
            return NvSuccess;
        }
        else
        {
            /* Only part of data is available in scratch. Copy it first */
            uPartialData = (pReader->lCurrFileOffSet - lOffset);
            NvOsMemcpy(pBuf, pStart, uPartialData);
            *pSize -= uPartialData;
            pBuf += uPartialData;
            lOffset += uPartialData;
        }
    }
    else
    {
        /* Need to flush scratch buffer and set file position */
        status = pReader->pPipe->SetPosition(pReader->cphandle, lOffset, CP_OriginBegin);
        if(status != NvSuccess)
        {
            return status;
        }
    }

    status = pReader->pPipe->Read(pReader->cphandle,
                (CPbyte*)pBuf, (CPuint)(*pSize));
    if (status != NvSuccess)
    {
        return status;
    }

    *pSize += uPartialData;

    status = pReader->pPipe->GetPosition(pReader->cphandle, &pos);
    if (status != NvSuccess)
    {
        return status;
    }
    pReader->lCurrFileOffSet = (NvU64)(pos);
    pReader->lCurrParseOffSet = pReader->lCurrFileOffSet;
    pReader->pScratchBufPtr = pReader->pScratchBufBase;
    pReader->uScratchBufCurrentSize = 0;
    pReader->uUnParsedScratchSize = 0;

    return NvSuccess;
}

static NvError NvMpsReaderSeekNextSyncCode(NvMpsReader *pReader, eNvMpsResult *peRes, NvU32 uLimit)
{
    NvU32 uRead;
    NvError status;
    NvU64 lLimitParseOffSet = pReader->lCurrParseOffSet + uLimit;

    while(1)
    {
        status = pReader->SeekNextSyncCodeInScratch(pReader, peRes, uLimit);
        /*Don't return if *peRes == NV_MPS_RESULT_NEED_MORE_DATA */
        if ((status != NvSuccess) || (*peRes == NV_MPS_RESULT_OK) || (*peRes == NV_MPS_RESULT_SYNC_NOT_FOUND))
        {
            return status;
        }

        if (pReader->lCurrParseOffSet >= lLimitParseOffSet)
        {
            break;
        }

        uLimit = (lLimitParseOffSet - pReader->lCurrParseOffSet);

        status = pReader->ReadForward(pReader, peRes, uLimit, &uRead);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }

    return NvSuccess; //*peRes == NV_MPS_RESULT_NEED_MORE_DATA
}


static NvError NvMpsReaderSeekNextPackCode(NvMpsReader *pReader, eNvMpsResult *peRes, NvU32 uLimit)
{
    NvU64 lLimitParseOffSet = pReader->lCurrParseOffSet + uLimit;
    NvError status = NvSuccess;

    while (1)
    {
        status = pReader->SeekNextSyncCode(pReader, peRes, uLimit);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            break;
        }

        if (pReader->pScratchBufPtr[0] == NV_MPS_PACK_HEADER_CODE)
        {
            *peRes = NV_MPS_RESULT_OK;
            return NvSuccess;
        }

        if (pReader->lCurrParseOffSet >= lLimitParseOffSet)
        {
            *peRes = NV_MPS_RESULT_SYNC_NOT_FOUND;
            return NvSuccess;
        }

        uLimit = (lLimitParseOffSet - pReader->lCurrParseOffSet);
    }

    return status;
}


static NvError NvMpsReaderSeekNextPESCode(NvMpsReader *pReader, eNvMpsResult *peRes, NvU32 uLimit)
{
    NvU64 lLimitParseOffSet = pReader->lCurrParseOffSet + uLimit;
    NvError status = NvSuccess;

    while (1)
    {
        status = pReader->SeekNextSyncCode(pReader, peRes, uLimit);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            break;
        }

        /*else sync is found. Need to find type */
        if (pReader->pScratchBufPtr[0] == NV_MPS_PACK_HEADER_CODE)
        {
            status = pReader->PeekParsePackHead(pReader, peRes, NV_TRUE);
            if ((status != NvSuccess) || (*peRes == NV_MPS_RESULT_END_OF_FILE))
            {
                return status;
            }
        }
        else if (pReader->pScratchBufPtr[0] == NV_MPS_SYS_HEADER_CODE)
        {
            status = pReader->PeekParseSysHead(pReader, peRes, NV_TRUE);
            if ((status != NvSuccess) || (*peRes == NV_MPS_RESULT_END_OF_FILE))
            {
                return status;
            }
        }
        else if ((pReader->pScratchBufPtr[0] == NV_MPS_STREAM_ID_AUDIO) ||
            (pReader->pScratchBufPtr[0] == NV_MPS_STREAM_ID_AUDIO_EXT) ||
            (pReader->pScratchBufPtr[0] == NV_MPS_STREAM_ID_VIDEO) ||
            (pReader->pScratchBufPtr[0] == NV_MPS_STREAM_ID_PSM) ||
            (pReader->pScratchBufPtr[0] == NV_MPS_STREAM_ID_PRIV_1) ||
            (pReader->pScratchBufPtr[0] == NV_MPS_STREAM_ID_PAD) ||
            (pReader->pScratchBufPtr[0] == NV_MPS_STREAM_ID_PRIV_2) ||
            ((pReader->pScratchBufPtr[0] >= NV_MPS_STREAM_ID_OTHER_MIN) &&
             (pReader->pScratchBufPtr[0] <= NV_MPS_STREAM_ID_OTHER_MAX)))
        {
            *peRes = NV_MPS_RESULT_OK;
            return NvSuccess;
        }

        if (pReader->lCurrParseOffSet >= lLimitParseOffSet)
        {
            *peRes = NV_MPS_RESULT_SYNC_NOT_FOUND;
            return NvSuccess;
        }

        uLimit = (lLimitParseOffSet - pReader->lCurrParseOffSet);
    }

    return status;
}

static NvError NvMpsReaderPeekParsePackHead(NvMpsReader *pReader, eNvMpsResult *peRes, NvBool bParse)
{
    NvU32 uSizeRequired = 0;
    NvError status;
    eNvMpsMpegType eMpgType;
    NvU32 uPaddingLength;
    NvMpsPackHead *pHead = pReader->pPackHead;

    if (pReader->pScratchBufPtr[0] != NV_MPS_PACK_HEADER_CODE)
    {
        *peRes = NV_MPS_RESULT_BOGUS_HEADER;
        return NvSuccess;
    }

    /*At least 2 bytes are required to find MPEG type*/
    uSizeRequired = 2;
    if (pReader->uUnParsedScratchSize < uSizeRequired)
    {
        status = pReader->EnsureDataAvailability(pReader, peRes, uSizeRequired);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }

    if ((pReader->pScratchBufPtr[1] & 0xF0) == 0x20)
    {
        eMpgType = NV_MPS_MPEG_1;
        uSizeRequired = 9;
    }
    else
    {
        eMpgType = NV_MPS_MPEG_2;
        uSizeRequired = 11;
    }

    if (pReader->uUnParsedScratchSize < uSizeRequired)
    {
        status = pReader->EnsureDataAvailability(pReader, peRes, uSizeRequired);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }

    if (eMpgType == NV_MPS_MPEG_1)
    {
        /*Check all marker bits first */
        if (((pReader->pScratchBufPtr[1] & 1) != 1) ||
            ((pReader->pScratchBufPtr[3] & 1) != 1) ||
            ((pReader->pScratchBufPtr[5] & 1) != 1) ||
            ((pReader->pScratchBufPtr[6] & 0x80) != 0x80) ||
            ((pReader->pScratchBufPtr[8] & 1) != 1))
        {
            *peRes = NV_MPS_RESULT_BOGUS_HEADER;
            return NvSuccess;
        }
        pHead->lScrBase = 0;
        pHead->lScrExt = 0;
        pHead->lScr = ((((NvU64)(pReader->pScratchBufPtr[1] & 0x0E)) << 29) | //[32...30]
                       (((NvU64)(pReader->pScratchBufPtr[2] << 22))) |  //[29...22]
                       (((NvU64)(pReader->pScratchBufPtr[3] & 0xFE)) << 14) | //[21...15]
                       (((NvU64)(pReader->pScratchBufPtr[4] << 7))) | //[14...7]
                       (((NvU64)(pReader->pScratchBufPtr[5] & 0xFE)) >> 1)); // [6...0]

        pHead->uMuxRate = ((((NvU64)(pReader->pScratchBufPtr[6] & 0x7F)) << 15) | //[22...16]
                       (((NvU64)(pReader->pScratchBufPtr[7] << 8))) | //[15...8]
                       (((NvU64)(pReader->pScratchBufPtr[8] & 0xFE)) >> 1)); //[7...0]
        uPaddingLength = 0;
    }
    else
    {
        //Add padding length
        uPaddingLength = (pReader->pScratchBufPtr[10] & 0x7);
        uSizeRequired += uPaddingLength;
        if (pReader->uUnParsedScratchSize < uSizeRequired)
        {
            status = pReader->EnsureDataAvailability(pReader, peRes, uSizeRequired);
            if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
            {
                return status;
            }
        }

        /*Check all marker bits */
        if (((pReader->pScratchBufPtr[1] & 0x4) != 0x4) ||
            ((pReader->pScratchBufPtr[3] & 0x4) != 0x4) ||
            ((pReader->pScratchBufPtr[5] & 0x4) != 0x4) ||
            ((pReader->pScratchBufPtr[6] & 0x1) != 0x1) ||
            ((pReader->pScratchBufPtr[9] & 0x3) != 0x3))
        {
            *peRes = NV_MPS_RESULT_BOGUS_HEADER;
            return NvSuccess;
        }

        /*Read SCR Base */
        pHead->lScrBase = ((((NvU64)(pReader->pScratchBufPtr[1] & 0x38)) << 27) | //[32...30]
                           (((NvU64)(pReader->pScratchBufPtr[1] & 3)) << 28) | //[29...28]
                           ((NvU64)(pReader->pScratchBufPtr[2]) << 20) | //[27...20]
                           (((NvU64)(pReader->pScratchBufPtr[3] & 0xF8)) << 12) | //[19...15]
                           (((NvU64)(pReader->pScratchBufPtr[3] & 3)) << 13) | //[14...13]
                           ((NvU64)(pReader->pScratchBufPtr[4]) << 5) | //[12...5]
                           (((NvU64)(pReader->pScratchBufPtr[5] & 0xF8)) >> 3)); //[4...0]

        /*Read SCR Extension */
        pHead->lScrExt = ((((NvU64)(pReader->pScratchBufPtr[5] & 0x3)) << 7) | //[8...7]
                           (((NvU64)(pReader->pScratchBufPtr[6] & 0xFE)) >> 1)); //[6...0]

        /*Read mux rate */
        pHead->uMuxRate = ((((NvU64)(pReader->pScratchBufPtr[7])) << 15) | //[22...15]
                           (((NvU64)(pReader->pScratchBufPtr[8])) << 7) | //[14...7]
                           (((NvU64)(pReader->pScratchBufPtr[9] & 0xFE)) >> 1)); //[6...0]

        pHead->lScr = (pHead->lScrBase * 300) + pHead->lScrExt;
    }

    pHead->eType = eMpgType;
    pHead->uPaddingLength = uPaddingLength;
    if (bParse)
    {
        pReader->MarkBytesParsed(pReader, uSizeRequired);
    }

    *peRes = NV_MPS_RESULT_OK;
    return NvSuccess;
}


static NvError NvMpsReaderPeekParseSysHead(NvMpsReader *pReader, eNvMpsResult *peRes, NvBool bParse)
{
    NvU32 uSizeRequired = 0;
    NvError status;
    NvU32 uHeadLength;
    NvMpsSysHead *pHead = pReader->pSysHead;

    if (pReader->pScratchBufPtr[0] != NV_MPS_SYS_HEADER_CODE)
    {
        *peRes = NV_MPS_RESULT_BOGUS_HEADER;
        return NvSuccess;
    }

    /*Atleast 3 bytes are required */
    uSizeRequired = 3;
    if (pReader->uUnParsedScratchSize < uSizeRequired)
    {
        status = pReader->EnsureDataAvailability(pReader, peRes, uSizeRequired);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }

    uHeadLength = ((NvU32)(pReader->pScratchBufPtr[1])) << 8 | pReader->pScratchBufPtr[2];
    uSizeRequired += uHeadLength;

    /*Atleast uHeadLength + 3 bytes are required */
    if (pReader->uUnParsedScratchSize < uSizeRequired)
    {
        status = pReader->EnsureDataAvailability(pReader, peRes, uSizeRequired);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }

    /* Now check for marker bits */
    if (((pReader->pScratchBufPtr[3] & 0x80) == 0) ||
        ((pReader->pScratchBufPtr[5] & 0x01) == 0) ||
        ((pReader->pScratchBufPtr[7] & 0x20) == 0))
    {
        *peRes = NV_MPS_RESULT_BOGUS_HEADER;
        return NvSuccess;
    }

    pHead->uHeadLength = uHeadLength;

    if (bParse)
    {
        pReader->MarkBytesParsed(pReader, uSizeRequired);
    }

    *peRes = NV_MPS_RESULT_OK;
    return NvSuccess;
}


static NvError NvMpsReaderPeekParsePESHead (NvMpsReader *pReader, eNvMpsResult *peRes, NvBool bParse)
{
    NvU32 uSizeRequired = 0;
    NvU32 uOffset = 0;
    NvU32 uMpeg2HeadLen = 0;
    NvError status;
    NvU32 uPacketLength;
    NvU32 uStreamID;
    NvBool bPtsValidity = NV_FALSE;
    NvBool bDtsValidity = NV_FALSE;
    NvU64 lPts = 0;
    NvU64 lDts = 0;
    NvMpsPESHead *pHead = pReader->pPESHead;
    eNvMpsMpegType eType = pReader->pPackHead->eType;


    if ((pReader->pScratchBufPtr[0] != NV_MPS_STREAM_ID_AUDIO) &&
        (pReader->pScratchBufPtr[0] != NV_MPS_STREAM_ID_AUDIO_EXT) &&
        (pReader->pScratchBufPtr[0] != NV_MPS_STREAM_ID_VIDEO) &&
        (pReader->pScratchBufPtr[0] != NV_MPS_STREAM_ID_PSM) &&
        (pReader->pScratchBufPtr[0] != NV_MPS_STREAM_ID_PRIV_1) &&
        (pReader->pScratchBufPtr[0] != NV_MPS_STREAM_ID_PAD) &&
        (pReader->pScratchBufPtr[0] != NV_MPS_STREAM_ID_PRIV_2) &&
        ((pReader->pScratchBufPtr[0] < NV_MPS_STREAM_ID_OTHER_MIN) ||
         (pReader->pScratchBufPtr[0] > NV_MPS_STREAM_ID_OTHER_MAX)))
    {
        *peRes = NV_MPS_RESULT_BOGUS_HEADER;
        return NvSuccess;
    }

    /*Atleast 3 bytes are required */
    uSizeRequired = 3;
    if (pReader->uUnParsedScratchSize < uSizeRequired)
    {
        status = pReader->EnsureDataAvailability(pReader, peRes, uSizeRequired);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }

    uPacketLength = ((NvU32)(pReader->pScratchBufPtr[1])) << 8 | pReader->pScratchBufPtr[2];
    if ((uPacketLength == 0) && (pReader->pScratchBufPtr[0] != NV_MPS_STREAM_ID_PAD))
    {
        *peRes = NV_MPS_RESULT_BOGUS_HEADER;
        return NvSuccess; //This is a transport stream;
    }

    uStreamID = pReader->pScratchBufPtr[0];
    *peRes = NV_MPS_RESULT_OK;

    if ((uStreamID != NV_MPS_STREAM_ID_AUDIO) &&
        (uStreamID != NV_MPS_STREAM_ID_AUDIO_EXT) &&
        (uStreamID != NV_MPS_STREAM_ID_VIDEO))
    {
        pHead->eType = eType;
        pHead->uStreamID = uStreamID;
        pHead->bAVStream = NV_FALSE;
        pHead->uPacketLength = uPacketLength;
        if (bParse)
        {
            pReader->MarkBytesParsed(pReader, uSizeRequired);
            uSizeRequired = 0;
        }

        /*Set Info about PES Data */
        pHead->lFileOffsetForData = pReader->lCurrParseOffSet + uSizeRequired;
        pHead->uDataSize = pHead->uPacketLength;
        pHead->lFileOffsetForHead = pHead->lFileOffsetForData - 6; // 6= 4 sync marker + 2 length
        return NvSuccess;
    }

    /*Make sure we have atleast 31 more bytes in scratch buffer
     * This is maximum header size for MPEG-1.
     * 31 = 1 Sync marker last byte + 2 Length + 16 stuffing + 2 STD + 5 pts + 5 dts
     */
    uSizeRequired = 31;
    if (pReader->uUnParsedScratchSize < uSizeRequired)
    {
        status = pReader->EnsureDataAvailability(pReader, peRes, uSizeRequired);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }


    uOffset = 3; // 1 sync marker last byte + 2 header length

    if (eType == NV_MPS_MPEG_1)
    {
        /* skip stuffing bytes */
        while((uOffset < 16) && (pReader->pScratchBufPtr[uOffset] == 0xFF))
        {
            uOffset++;
        }

        if ((pReader->pScratchBufPtr[uOffset] & 0xC0) == 0x40)
        {
            /* skip STD buffer size (2 bit tag + 1 bit scale + 13 bit value => 16 bits) */
            uOffset += 2;
        }

        if ((pReader->pScratchBufPtr[uOffset] & 0xE0) == 0x20)
        {
            /* Check Marker bits */
            NV_MPS_CHECK_MARKER_BITS(uOffset);

            /*Now read actual Pts */
            bPtsValidity = NV_TRUE;
            NV_MPS_READ_PTS(lPts, uOffset);

            if ((pReader->pScratchBufPtr[uOffset] & 0xF0) == 0x30)
            {
                /* Check Marker bits for dts */
                NV_MPS_CHECK_MARKER_BITS(uOffset+5);

                /*Now read actual Dts */
                bDtsValidity = NV_TRUE;
                NV_MPS_READ_PTS(lDts, uOffset+5);

                uOffset +=  5;
            }
            uOffset +=  5;
        }
        else if (pReader->pScratchBufPtr[uOffset] == 0x0F)
        {
            uOffset +=  1;
        }
        else
        {
            *peRes = NV_MPS_RESULT_BOGUS_HEADER;
            return NvSuccess;
        }
    }
    else //NV_MPS_MPEG_2
    {
        if ((pReader->pScratchBufPtr[uOffset] & 0xC0) != 0x80)
        {
                *peRes = NV_MPS_RESULT_BOGUS_HEADER;
                return NvSuccess;
        }

        uMpeg2HeadLen = pReader->pScratchBufPtr[uOffset+2];

        if ((pReader->pScratchBufPtr[uOffset+1] & 0xC0) >= 0x80)
        {   //First read pts

            /* Check Marker bits */
            NV_MPS_CHECK_MARKER_BITS(uOffset+3);

            /*Now read actual Pts */
            bPtsValidity = NV_TRUE;
            NV_MPS_READ_PTS(lPts, uOffset+3);

            if ((pReader->pScratchBufPtr[uOffset+1] & 0xC0) == 0xC0)
            { //Now read dts
                /* Check Marker bits */
                NV_MPS_CHECK_MARKER_BITS(uOffset+8);

                /*Now read actual Dts */
                bDtsValidity = NV_TRUE;
                NV_MPS_READ_PTS(lDts, uOffset+8);
            }
        }

        uOffset +=  uMpeg2HeadLen + 3; // 3 = uMpeg2HeadLen byte offset
    }

    if (uPacketLength <= (uOffset - 3))
    {
        *peRes = NV_MPS_RESULT_BOGUS_HEADER;
        return NvSuccess; //uHeadLength field is corrupted
    }

    /*Make sure we have atleast uOffset more bytes in scratch buffer */
    if (pReader->uUnParsedScratchSize < uOffset)
    {
        status = pReader->EnsureDataAvailability(pReader, peRes, uOffset);
        if ((status != NvSuccess) || (*peRes != NV_MPS_RESULT_OK))
        {
            return status;
        }
    }

    pHead->eType = eType;
    pHead->uStreamID = uStreamID;
    pHead->bAVStream = NV_TRUE;
    pHead->uPacketLength = uPacketLength;
    if ((pHead->bPtsValid = bPtsValidity)) // use of = is intentional here
    {
        pHead->lPts = lPts;
    }
    if ((pHead->bDtsValid = bDtsValidity)) // use of = is intentional here
    {
        pHead->lDts = lDts;
    }

    uSizeRequired = uOffset;
    if (bParse)
    {
        pReader->MarkBytesParsed(pReader, uOffset);
        uSizeRequired = 0;
    }

    /*Set Info about PES Data */
    pHead->lFileOffsetForData = pReader->lCurrParseOffSet + uSizeRequired;
    pHead->uDataSize = uPacketLength - uOffset + 3; //3 = 1byte for lasy byte of sync and 2 bytes for length
    pHead->lFileOffsetForHead = pHead->lFileOffsetForData - uOffset - 3; //3 = First 3 bytes of sync

    *peRes = NV_MPS_RESULT_OK;
    return NvSuccess;
}

void NvMpsReaderDestroy(NvMpsReader *pReader)
{
    if (pReader == NULL)
    {
        return;
    }

    if (pReader->pPESHead)
    {
        NvOsFree(pReader->pPESHead);
    }

    if (pReader->pSysHead)
    {
        NvOsFree(pReader->pSysHead);
    }

    if (pReader->pPackHead)
    {
        NvOsFree(pReader->pPackHead);
    }

    if (pReader->pScratchBufBase)
    {
        NvOsFree(pReader->pScratchBufBase);
    }

    NvOsFree(pReader);
}

static NvError NvMpsReaderInit (NvMpsReader *pReader, CP_PIPETYPE *pPipe, CPhandle cphandle)
{
    NvError status = NvSuccess;
    CPuint pos;

    if ((pReader == NULL) || (pPipe == NULL))
    {
        return NvError_ParserFailure;
    }

    pReader->pPipe = pPipe;
    pReader->cphandle = cphandle;

    status = pReader->pPipe->SetPosition(pReader->cphandle, 0, CP_OriginEnd);
    if(status != NvSuccess)
    {
        return status;
    }
    status = pReader->pPipe->GetPosition(pReader->cphandle, &pos);
    if (status != NvSuccess)
    {
        return status;
    }
    pReader->lFileSize = (NvU64)(pos);
    status = pReader->pPipe->SetPosition(pReader->cphandle, 0, CP_OriginBegin);
    if(status != NvSuccess)
    {
        return status;
    }

    return NvSuccess;
}

NvMpsReader *NvMpsReaderCreate(NvU32 uScratchBufMaxSize)
{
    NvMpsReader *pReader;

    pReader = (NvMpsReader *)NvOsAlloc(sizeof(NvMpsReader));
    if (pReader == NULL)
        goto NvMpsReaderMemFailure;
    NvOsMemset(pReader, 0, sizeof(NvMpsReader));


    pReader->pScratchBufBase = (NvU8 *)NvOsAlloc(uScratchBufMaxSize);
    if (pReader->pScratchBufBase == NULL)
        goto NvMpsReaderMemFailure;

    pReader->pPackHead = (NvMpsPackHead *)NvOsAlloc(sizeof(NvMpsPackHead));
    if (pReader->pPackHead == NULL)
        goto NvMpsReaderMemFailure;

    pReader->pSysHead = (NvMpsSysHead *)NvOsAlloc(sizeof(NvMpsSysHead));
    if (pReader->pSysHead == NULL)
        goto NvMpsReaderMemFailure;

    pReader->pPESHead = (NvMpsPESHead *)NvOsAlloc(sizeof(NvMpsPESHead));
    if (pReader->pPESHead == NULL)
        goto NvMpsReaderMemFailure;

    pReader->pScratchBufPtr = pReader->pScratchBufBase;
    pReader->uScratchBufMaxSize = uScratchBufMaxSize;

    pReader->Init = NvMpsReaderInit;
    pReader->MarkBytesParsed = NvMpsReaderMarkBytesParsed;
    pReader->ReadForward = NvMpsReaderReadForward;
    pReader->EnsureDataAvailability = NvMpsReaderEnsureDataAvailability;
    pReader->SetReaderPosition = NvMpsReaderSetReaderPosition;
    pReader->SeekNextSyncCodeInScratch = NvMpsReaderSeekNextSyncCodeInScratch;
    pReader->SeekNextSyncCode = NvMpsReaderSeekNextSyncCode;
    pReader->SeekNextPackCode = NvMpsReaderSeekNextPackCode;
    pReader->SeekNextPESCode = NvMpsReaderSeekNextPESCode;
    pReader->PeekParsePackHead = NvMpsReaderPeekParsePackHead;
    pReader->PeekParseSysHead = NvMpsReaderPeekParseSysHead;
    pReader->PeekParsePESHead = NvMpsReaderPeekParsePESHead;
    pReader->CopyDataToExternalBuffer = NvMpsReaderCopyDataToExternalBuffer;

    return pReader;

NvMpsReaderMemFailure:
    NvMpsReaderDestroy(pReader);
    return NULL;
}
