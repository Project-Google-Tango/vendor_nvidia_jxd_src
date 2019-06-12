/*
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Definition of SparseHeaderRec and ChunkHeaderRec
 * are derived from software provided under the following
 * license:
 *
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nvcommon.h"
#include "nverror.h"
#include "nvsystem_utils.h"
#include "nvstormgr.h"
#include "nvfsmgr.h"
#include "nverror.h"
#include "crc32.h"

#define HASH_LEN  16
#define CHUNK_LEN 65536

#define UNSPARSE_MAGIC 0xed26ff3a

#define CHUNK_HEADER_LEN sizeof(ChunkHeader)
#define SPARSE_HEADER_MAJOR_VER 1

#define SPARSE_HEADER_MAGIC  0xed26ff3a
#define CHUNK_TYPE_RAW    0xCAC1
#define CHUNK_TYPE_FILL   0xCAC2
#define CHUNK_TYPE_DONT_CARE  0xCAC3
#define CHUNK_TYPE_CRC32  0xCAC4
static NvBool IsStart = NV_TRUE;

typedef struct SparseHeaderRec {
    NvU32  magic;           /* 0xed26ff3a */
    NvU16  major_version;   /* (0x1) - reject images with higher major versions */
    NvU16  minor_version;   /* (0x0) - allow images with higer minor versions */
    NvU16  file_hdr_sz;     /* 28 bytes for first revision of the file format */
    NvU16  chunk_hdr_sz;    /* 12 bytes for first revision of the file format */
    NvU32  blk_sz;          /* block size in bytes, must be a multiple of 4 (4096) */
    NvU32  total_blks;      /* total blocks in the non-sparse output image */
    NvU32  total_chunks;    /* total chunks in the sparse input image */
    NvU32  image_checksum;  /* CRC32 checksum of the original data, counting "don't care" */
                            /* as 0. Standard 802.3 polynomial, use a Public Domain */
                            /* table implementation */
} SparseHeader;


typedef struct ChunkHeaderRec {
    NvU16  chunk_type;   /* 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care */
    NvU16  reserved;
    NvU32  chunk_sz;    /* in blocks in output image */
    NvU32  total_sz;    /* in bytes of chunk input file including chunk header and data */
} ChunkHeader;

typedef enum{
    Unsparse_ChunkHeaderDownloading = 1,
    Unsparse_RawDataDownloading = 2,
    Unsparse_CRCDataDownloading = 3,
    Unsparse_FillDataDownloading = 4,
}UnsparseState;


typedef struct SparseStateRec {
    UnsparseState state;
    ChunkHeader *pChunk;
    SparseHeader *pSparse;
    NvBool dummy;
    NvU8 dummycount;
    NvU64 BytesRemaining;
    NvU32 filecrc;
}UnSparseFileState;

static
NvError
NvSysUtilSignUnSparse(
    const NvU8 *InBuffer,
    NvU8 *LeftBuffer,
    NvU8 *LeftOut,
    NvU32 WriteBytes,
    NvBool IsSparseStart,
    NvBool IsEnd,
    NvU8 *pHash,
    NvDdkFuseOperatingMode *pOpMode);

NvBool NvSysUtilCheckSparseImage(const NvU8 *Buffer, NvU32 size)
{
    NvBool ret = NV_FALSE;
    NvU32 magic = *(NvU32*)Buffer;
    if(size < 28)
        return ret;
    if(magic == UNSPARSE_MAGIC)
    {
        ret = NV_TRUE;
    }
    return ret;
}

NvError NvSysUtilSignUnSparse(
             const NvU8 *InBuffer,
             NvU8 *LeftBuffer,
             NvU8 *LeftOut,
             NvU32 WriteBytes,
             NvBool IsSparseStart,
             NvBool IsEnd,
             NvU8 *pHash,
             NvDdkFuseOperatingMode *pOpMode)
{
    NvError e = NvSuccess;
    NvU8 Left = *(NvU8*)LeftOut;
    NvU32 BytesToSign = WriteBytes + Left;
    NvU8 *lPtr = LeftBuffer + Left;
    NvU8 Move = 0;

    if (BytesToSign >= HASH_LEN)
    {
        if (Left)
        {
            Move = HASH_LEN - Left;
            NvOsMemcpy(lPtr, InBuffer, Move);
            e = NvSysUtilSignData(
                       LeftBuffer,
                       HASH_LEN,
                       IsStart,
                       NV_FALSE,
                       pHash,
                       pOpMode);
            if (e != NvSuccess)
                 goto fail;

            Left = 0;
            BytesToSign -= HASH_LEN;
        }
        if (BytesToSign != 0)
        {
            if ((BytesToSign % HASH_LEN) == 0)
            {
                e = NvSysUtilSignData(
                        InBuffer + Move,
                        BytesToSign,
                        IsStart,
                        IsEnd,
                        pHash,
                        pOpMode);
                if (e != NvSuccess)
                    goto fail;
                Left = 0;
                BytesToSign = 0;
                if (IsStart)
                    IsStart = NV_FALSE;
            }
            else
            {
                Left = BytesToSign % HASH_LEN;
                e = NvSysUtilSignData(
                        InBuffer + Move,
                        BytesToSign - Left,
                        IsStart,
                        IsEnd,
                        pHash,
                        pOpMode);
                if (e != NvSuccess)
                    goto fail;

                NvOsMemmove(LeftBuffer, InBuffer + WriteBytes - Left, Left);
                if (IsStart)
                    IsStart = NV_FALSE;
            }
        }
    }
    else
    {
        NvOsMemcpy(lPtr, InBuffer, WriteBytes);
        Left += WriteBytes;
    }
    *(NvU8*)LeftOut=Left;

fail:
    return e;
}

NvError NvSysUtilUnSparse(
            NvStorMgrFileHandle hFile,
            const NvU8 *Buffer,
            NvU32 Size,
            NvBool IsSparseStart,
            NvBool IsLastBuffer,
            NvBool IsSignRequired,
            NvBool IsCRC32Required,
            NvU8 *Hash,
            NvDdkFuseOperatingMode *OpMode)
{
    static UnSparseFileState *hSparse = NULL;
    NvU32 offset = 0;
    NvU32 BytesToDecode = Size;
    const NvU8 *pBuff = Buffer;
    NvU32 *FillBuff = NULL;
    static NvU8 *FillData = NULL;
    static NvU32 NumFillBlocks = 0;
    static NvU8 *crc = NULL;
    static NvU8 *lBuff = NULL;
    static NvU8 LeftOut = 0;
    NvError e = NvSuccess;


    if (IsSparseStart)
    {
        if (IsSignRequired)
        {
            lBuff = NvOsAlloc(HASH_LEN);
            if (lBuff == NULL)
            {
                e = NvError_InsufficientMemory;
                goto fail;
            }
        }
        hSparse = NvOsAlloc(sizeof(UnSparseFileState));
        if (hSparse == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        hSparse->pSparse = NvOsAlloc(sizeof(SparseHeader));
        if (hSparse->pSparse == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        hSparse->pChunk = NvOsAlloc(sizeof(ChunkHeader));
        if (hSparse->pChunk == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        FillData = NvOsAlloc(sizeof(NvU32));
        if (FillData == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        *FillData = 0;

        crc = NvOsAlloc(sizeof(NvU32));
        if (crc == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        *crc = 0;
        hSparse->dummy = NV_FALSE;
        hSparse->dummycount = 0;
        if(BytesToDecode < sizeof(SparseHeader))
        {
            e = NvError_InvalidSize;
            goto fail;
        }
        NvOsMemcpy(hSparse->pSparse, pBuff, sizeof(SparseHeader));
        offset += sizeof(SparseHeader);
        BytesToDecode -= sizeof(SparseHeader);
        hSparse->state = Unsparse_ChunkHeaderDownloading;
        hSparse->BytesRemaining = CHUNK_HEADER_LEN;
        if(hSparse->pSparse->magic != SPARSE_HEADER_MAGIC)
        {
            e = NvError_NotSupported;
            goto fail;
        }
        if(hSparse->pSparse->major_version != SPARSE_HEADER_MAJOR_VER)
        {
            e = NvError_NotSupported;
            goto fail;
        }

        if((hSparse->pSparse->file_hdr_sz > sizeof(SparseHeader)))
        {
            hSparse->dummy = NV_TRUE;
            hSparse->dummycount = (hSparse->pSparse->file_hdr_sz - sizeof(SparseHeader));
        }
        if (IsSignRequired)
            NvOsMemset(lBuff, 0, HASH_LEN);
    }
    while(BytesToDecode)
    {
        if(hSparse->dummy)
        {
            if(BytesToDecode >= hSparse->dummycount)
            {
                offset = offset + hSparse->dummycount;
                BytesToDecode -= hSparse->dummycount;
                hSparse->dummy= NV_FALSE;
                hSparse->dummycount = 0;
            }
            else
            {
                hSparse->dummycount -= BytesToDecode;
                return NvSuccess;
            }
        }
        if(hSparse->state == Unsparse_RawDataDownloading)
        {
            if((BytesToDecode) >= hSparse->BytesRemaining)
            {
                while (hSparse->BytesRemaining)
                {
                    NvU32 BytesWritten = 0;
                    e = NvStorMgrFileWrite(hFile,
                                            (void *)(pBuff + offset),
                                            hSparse->BytesRemaining,
                                            &BytesWritten);
                    if (e != NvSuccess)
                        goto fail;
                    // If Verify part is enabled for sparsed image, Hash calculation is
                    // is done here
                    if (IsSignRequired)
                    {
                        e = NvSysUtilSignUnSparse(
                                   pBuff + offset,
                                   lBuff,
                                   &LeftOut,
                                   BytesWritten,
                                   IsSparseStart,
                                   NV_FALSE,
                                   Hash,
                                   OpMode);
                        if (e != NvSuccess)
                            goto fail;
                     }
                     if (IsCRC32Required)
                             hSparse->filecrc = NvComputeCrc32(
                                                       hSparse->filecrc,
                                                       pBuff + offset,
                                                       BytesWritten);
                     hSparse->BytesRemaining -= BytesWritten;
                     offset += BytesWritten;
                     BytesToDecode -= BytesWritten;
                }
                hSparse->state = Unsparse_ChunkHeaderDownloading;
                hSparse->BytesRemaining = CHUNK_HEADER_LEN;
            }
            else
            {
                while(BytesToDecode)
                {
                    NvU32 BytesWritten = 0;
                    e = NvStorMgrFileWrite(hFile, (void *)(pBuff + offset), BytesToDecode, &BytesWritten);
                    if (e != NvSuccess)
                        goto fail;

                    if (IsSignRequired)
                    {
                         e = NvSysUtilSignUnSparse(
                                    pBuff + offset,
                                    lBuff,
                                    &LeftOut,
                                    BytesWritten,
                                    IsSparseStart,
                                    IsLastBuffer,
                                    Hash,
                                    OpMode);
                        if (e != NvSuccess)
                            goto fail;
                    }
                    if (IsCRC32Required)
                        hSparse->filecrc = NvComputeCrc32(
                                                      hSparse->filecrc,
                                                      pBuff + offset,
                                                      BytesWritten);
                    hSparse->BytesRemaining -= BytesWritten;
                    BytesToDecode -= BytesWritten;
                    offset += BytesWritten;
                }
                return NvSuccess;
            }
        }
        else if(hSparse->state == Unsparse_CRCDataDownloading)
        {
            if((BytesToDecode) >= hSparse->BytesRemaining)
            {
                NvOsMemcpy(crc + 4 - hSparse->BytesRemaining,
                            pBuff + offset,
                            hSparse->BytesRemaining);

                offset += hSparse->BytesRemaining;
                BytesToDecode -= hSparse->BytesRemaining;
                if(*crc != hSparse->filecrc)
                {
                    e= NvError_CountMismatch;
                    goto fail;
                }
                hSparse->state = Unsparse_ChunkHeaderDownloading;
                hSparse->BytesRemaining = CHUNK_HEADER_LEN;
            }
            else
            {
                NvOsMemcpy(crc + 4 - hSparse->BytesRemaining, pBuff + offset, BytesToDecode);
                hSparse->BytesRemaining -= BytesToDecode;
                return NvSuccess;
            }
        }
        else if(hSparse->state == Unsparse_FillDataDownloading)
        {
            if((BytesToDecode) >= hSparse->BytesRemaining)
            {
                NvU32 i;
                NvU32 BlocksToWrite;
                NvU32 BytesWritten = 0;
                NvU32 NumBlocksInChunk = 1 << 8;

                NvOsMemcpy(FillData + sizeof(NvU32) - hSparse->BytesRemaining,
                            pBuff + offset,
                            hSparse->BytesRemaining);

                BlocksToWrite = (NvU32)NV_MIN(NumBlocksInChunk, NumFillBlocks);
                FillBuff = NvOsAlloc(hSparse->pSparse->blk_sz * BlocksToWrite);
                if (FillBuff == NULL)
                {
                    e = NvError_InsufficientMemory;
                    goto fail;
                }

                i = 0;
                while (i < (hSparse->pSparse->blk_sz * BlocksToWrite) /
                            sizeof(NvU32))
                {
                    FillBuff[i++] = *((NvU32 *)FillData);
                }

                while (NumFillBlocks)
                {
                    BlocksToWrite = (NvU32)NV_MIN(NumBlocksInChunk, NumFillBlocks);
                    NumFillBlocks -= BlocksToWrite;
                    e = NvStorMgrFileWrite(hFile, (void *)FillBuff,
                                    BlocksToWrite * hSparse->pSparse->blk_sz,
                                    &BytesWritten);
                    if (BytesWritten != BlocksToWrite * hSparse->pSparse->blk_sz)
                        e = NvError_EndOfFile;
                    if (e != NvSuccess)
                        goto fail;
                }

                NvOsFree(FillBuff);
                FillBuff = NULL;

                offset += hSparse->BytesRemaining;
                BytesToDecode -= hSparse->BytesRemaining;
                hSparse->state = Unsparse_ChunkHeaderDownloading;
                hSparse->BytesRemaining = CHUNK_HEADER_LEN;
            }
            else
            {
                NvOsMemcpy(FillData + sizeof(NvU32) - hSparse->BytesRemaining,
                                pBuff + offset, BytesToDecode);
                hSparse->BytesRemaining -= BytesToDecode;
                return NvSuccess;
            }
        }
        else
        {
            if (hSparse->BytesRemaining <= BytesToDecode)
            {
                NvOsMemcpy(
                    ((NvU8*)(hSparse->pChunk)) + (CHUNK_HEADER_LEN - hSparse->BytesRemaining),
                                pBuff + offset, hSparse->BytesRemaining);
                offset += hSparse->BytesRemaining;
                BytesToDecode -= hSparse->BytesRemaining;

                if(hSparse->pSparse->chunk_hdr_sz > CHUNK_HEADER_LEN)
                {
                    hSparse->dummy = NV_TRUE;
                    hSparse->dummycount = hSparse->pSparse->chunk_hdr_sz - CHUNK_HEADER_LEN;
                }
            }

            else
            {
                NvOsMemcpy(
                    ((NvU8*)(hSparse->pChunk)) + (CHUNK_HEADER_LEN - hSparse->BytesRemaining),
                                    pBuff + offset, BytesToDecode);
                hSparse->BytesRemaining -= BytesToDecode;
                return NvSuccess;
            }


            switch(hSparse->pChunk->chunk_type)
            {
            case CHUNK_TYPE_RAW:
                if(hSparse->pChunk->total_sz !=
                    hSparse->pSparse->chunk_hdr_sz + \
                    hSparse->pChunk->chunk_sz * hSparse->pSparse->blk_sz)
                {
                    e= NvError_BadParameter;
                    goto fail;
                }
                hSparse->BytesRemaining = hSparse->pChunk->chunk_sz * hSparse->pSparse->blk_sz;
                hSparse->state = Unsparse_RawDataDownloading;
                break;
            case CHUNK_TYPE_DONT_CARE:
                hSparse->BytesRemaining= (NvU64) (hSparse->pChunk->chunk_sz) * (NvU64) (hSparse->pSparse->blk_sz);
                if (IsSignRequired)
                {
                    NvU64 BytesToSeek = hSparse->BytesRemaining;
                    NvU8 *ZeroBuffer = NULL;
                    NvBool IsEnd = NV_FALSE;

                    ZeroBuffer = NvOsAlloc(CHUNK_LEN);
                    if (ZeroBuffer == NULL)
                    {
                        e = NvError_InsufficientMemory;
                        goto fail;
                    }
                    NvOsMemset(ZeroBuffer, 0, CHUNK_LEN);
                    while(BytesToSeek)
                    {
                        if (BytesToSeek <= CHUNK_LEN && IsLastBuffer &&
                           (!BytesToDecode))
                            IsEnd = NV_TRUE;
                        if (BytesToSeek > CHUNK_LEN)
                        {
                            e = NvSysUtilSignUnSparse(
                                       ZeroBuffer,
                                       lBuff,
                                       &LeftOut,
                                       CHUNK_LEN,
                                       IsSparseStart,
                                       IsEnd,
                                       Hash,
                                       OpMode);
                            if(e != NvSuccess)
                            {
                                NvOsFree(ZeroBuffer);
                                goto fail;
                            }
                            BytesToSeek-=CHUNK_LEN;
                        }
                        else
                        {
                            e = NvSysUtilSignUnSparse(
                                       ZeroBuffer,
                                       lBuff,
                                       &LeftOut,
                                       BytesToSeek,
                                       IsSparseStart,
                                       IsEnd,
                                       Hash,
                                       OpMode);
                            if (e  != NvSuccess)
                            {
                                NvOsFree(ZeroBuffer);
                                goto fail;
                            }
                            BytesToSeek =0;
                            if (IsEnd)
                                LeftOut=0;
                        }
                    }
                    NvOsFree(ZeroBuffer);
                }
                NvStorMgrFileSeek(hFile, hSparse->BytesRemaining, NvOsSeek_Cur);
                hSparse->state = Unsparse_ChunkHeaderDownloading;
                hSparse->BytesRemaining = CHUNK_HEADER_LEN;
                break;

            case CHUNK_TYPE_CRC32:
                if (IsCRC32Required)
                {
                    hSparse->BytesRemaining = 4;
                    hSparse->state = Unsparse_CRCDataDownloading;
                    break;
                }
                else
                {
                    e = NvError_BadParameter;
                    goto fail;
                }

            case CHUNK_TYPE_FILL:
                hSparse->BytesRemaining = sizeof(NvU32);
                hSparse->state = Unsparse_FillDataDownloading;
                NumFillBlocks = hSparse->pChunk->chunk_sz;
                break;
            default:
                e = NvError_BadParameter;
                goto fail;
            }
        }
    }
fail:
    if(IsLastBuffer || e)
    {
        if (IsSignRequired)
        {
            // If LeftOut data then generate signature here
            if (LeftOut)
            {
                e = NvSysUtilSignData(
                           lBuff,
                           LeftOut,
                           IsSparseStart,
                           IsLastBuffer,
                           Hash,
                           OpMode);
            }
            NvOsFree(lBuff);
        }
        NvOsFree(hSparse->pChunk);
        NvOsFree(hSparse->pSparse);
        NvOsFree(hSparse);
        NvOsFree(crc);
        NvOsFree(FillData);
        IsStart = NV_TRUE;
    }
    if (FillBuff)
        NvOsFree(FillBuff);

    return e;
}

