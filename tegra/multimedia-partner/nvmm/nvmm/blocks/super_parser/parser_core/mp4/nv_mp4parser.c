/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nv_mp4parser.h"
#include "nvmm_contentpipe.h"
#include "nvmm_sock.h"

typedef struct NvMp4Id3V2HeaderRec
{
    NvS8 Tag[3];
    NvU8 VersionMajor;
    NvU8 VersionRevision;
    NvU8 Flags;
    NvU8 Size[4];
} NvMp4Id3V2Header;

typedef struct NvMp4Id3V2_34FrameHeader
{
    NvS8  Tag[4];
    NvU8  Size[4];
    NvU8  Flags[2];
} NvMp4Id3V2_34FrameHeader;

typedef struct NvMp4Id3V2_2FrameHeaderRec
{
    NvS8  Tag[3];
    NvU8  Size[3];
} NvMp4Id3V2_2FrameHeader;

typedef enum
{
    ID3_Ver2_2 = 2,
    ID3_Ver2_3,
    ID3_Ver2_4,
}Mp4Id3V2Version;

typedef enum
{
    NvMp4Id3MetadataType_ARTIST,
    NvMp4Id3MetadataType_ALBUM,
    NvMp4Id3MetadataType_GENRE,
    NvMp4Id3MetadataType_TITLE,
    NvMp4Id3MetadataType_TRACKNUM,
    NvMp4Id3MetadataType_YEAR,
    NvMp4Id3MetadataType_COMPOSER,
    NvMp4Id3MetadataType_ALBUMARTIST,
    NvMp4Id3MetadataType_COVERART,
    NvMp4Id3MetadataType_COPYRIGHT,
    // This is be the last entry representing the max metadata.
    // Any new metadata type should be added just before this not in between.
    NvMp4Id3MetadataType_MAXMETADATA,
    NvMp4Id3MetadataType_Force32 = 0x7FFFFFFF
} NvMp4Id3MetadataType;

typedef enum Mp4ReadBulkDataTypeRec
{
    Mp4ReadBulkDataType_STSZ = 1,
    Mp4ReadBulkDataType_STCO,
    Mp4ReadBulkDataType_CTTS,
    Mp4ReadBulkDataType_STTS
} Mp4ReadBulkDataType;

typedef struct MP4ParserStreamBufferRec
{
    // Buffer to hold the bits from the  stream
    NvU8  *ReadBuffer;
    //Size of header
    NvU32 NBits;
    // Carry word
    NvU32 CWord;
    //Byte count
    NvU32 ByteCount;
} MP4ParserStreamBuffer ;

static NvError
NvMp4ParserGetSINFAtom (
    NvMp4Parser * pNvMp4Parser,
    NvU32 *pAtomSize,
    NvU8 *pAtom)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (pNvMp4Parser->IsSINF);

    pAtom = pNvMp4Parser->pSINF;
    *pAtomSize = pNvMp4Parser->SINFSize;

cleanup:
    return Status;
}

NvError
NvMp4ParserGetAtom (
    NvMp4Parser *pNvMp4Parser,
    NvU32 AtomType,
    NvU8 *pAtom,
    NvU32 *pAtomSize)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (pNvMp4Parser);
    if (AtomType == MP4_FOURCC('s', 'i', 'n', 'f'))
    {
        Status = NvMp4ParserGetSINFAtom (pNvMp4Parser, pAtomSize, pAtom);
    }
    else
        return NvError_ParserFailure;
cleanup:
    return Status;

}

static NvError
Mp4ParserATOMH (
    NvMp4Parser *pNvMp4Parser,
    NvU32 *pAtomType,
    NvU64 *pAtomSize,
    NvU8 *pBytesRead)
{
    NvError Status = NvSuccess;
    NvU8 ReadBuffer[8];
    NvU32 LowInt, HighInt;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;

    *pAtomType = 0;
    *pAtomSize = 0;
    *pBytesRead = 0;

    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, sizeof (ReadBuffer)));

    *pAtomType = NV_BE_TO_INT_32 (&ReadBuffer[4]);
    *pAtomSize = NV_BE_TO_INT_32 (&ReadBuffer[0]);
    *pBytesRead = 8;

    if (*pAtomSize == 1)
    {
        if (*pAtomType == MP4_FOURCC('m', 'd', 'a', 't'))
        {
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, sizeof (ReadBuffer)));
            LowInt = NV_BE_TO_INT_32 (&ReadBuffer[0]);
            HighInt = NV_BE_TO_INT_32 (&ReadBuffer[4]);
            *pAtomSize = ( (NvU64) HighInt | (NvU64) LowInt << 32);
            *pBytesRead += 8;
        }
        else
        {
            Status = NvError_ParserFailure;
        }
    }
    else if (*pAtomSize < 8)
    {
        Status =  NvError_ParserFailure;
    }

cleanup:
    return Status;
}

static NvError
Mp4ParserATOM (
    NvMp4Parser* pNvMp4Parser,
    NvU32 ParentAtomType,
    NvU64 ParentAtomSize,
    NvU64 *pBytesRead,
    void *pData,
    NvError (*pAtomHandler) (NvMp4Parser * s, NvU32 a, NvU64 i, NvU64* p, void *d))
{
    NvError Status = NvSuccess;
    NvU8 HdrBytesRead = 0;
    NvU64 DataBytesRead = 0;
    NvU64 BytesRead = 0;
    NvU64 FilePos = 0;
    NvS64 SeekOffset = 0;
    NvU32 AtomType;
    NvU64 AtomSize = 0;

    while ( (BytesRead + 8) <= ParentAtomSize)
    {
        HdrBytesRead = 0;
        DataBytesRead = 0;
        NVMM_CHK_ERR (Mp4ParserATOMH (pNvMp4Parser, &AtomType, &AtomSize, &HdrBytesRead));
        NVMM_CHK_ERR (pAtomHandler (pNvMp4Parser, AtomType, AtomSize - HdrBytesRead, &DataBytesRead, pData));
        SeekOffset = AtomSize - HdrBytesRead - DataBytesRead;
        NVMM_CHK_ARG (SeekOffset >= 0);
        if (SeekOffset > 0)
        {
            NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
            NVMM_CHK_ARG (FilePos + SeekOffset <= pNvMp4Parser->FileSize);
            NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) SeekOffset, CP_OriginCur));
        }
        BytesRead += AtomSize;
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvU32
Mp4GetExpandableParam (
    NvU8 *pBuffer,
    NvU32 *pParam)
{
    NvU32 ByteIndex = 0;
    NvU32 SizeOfInstance = 0;
    NvU32 NextByte;
    NvU8 SizeByte;

    NextByte = (pBuffer[ByteIndex] >> 7) & 1;
    SizeOfInstance = (pBuffer[ByteIndex] & 0x7f);

    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "nextByte = %d -- sizeOfInstance=%d\n", NextByte, SizeOfInstance));
    while (NextByte)
    {
        ByteIndex++;
        NextByte = (pBuffer[ByteIndex] >> 7) & 1;
        SizeByte = (pBuffer[ByteIndex] & 0x7f);
        SizeOfInstance = (SizeOfInstance << 7) | SizeByte;

        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "nextByte = %d -- sizeByte = %d -- sizeOfInstance=%d\n", NextByte, SizeByte, SizeOfInstance));
    }
    *pParam = SizeOfInstance;

    return (ByteIndex + 1);
}


static NvU32
Mp4GetStreamBits (
    MP4ParserStreamBuffer *pInput,
    NvU32 NoOfBits)
{
    NvU32 c1;

    while (pInput->NBits < NoOfBits)
    {
        c1 = * (pInput->ReadBuffer) ++;
        pInput->ByteCount++;
        pInput->CWord = (pInput->CWord << 8) | c1;

        pInput->NBits += 8;
    }

    pInput->NBits -= NoOfBits;

    return (pInput->CWord >> pInput->NBits) & ( (1 << NoOfBits) - 1) ;
}

static NvError
Mp4ParserHeaderEntry (
    NvMp4Parser *pNvMp4Parser,
    NvMp4TrackInformation*pTrackInfo,
    NvU32 ESDSSize,
    NvU32 *pSamplesRead)
{
    NvError Status = NvSuccess;
    NvU32 FileBytesRead = 0;
    NvU32 ReadCount;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;
    NvU32 count = pTrackInfo->nDecSpecInfoCount;

    if (ESDSSize > MAX_ESDS_ATOM_SIZE)
        ESDSSize = MAX_ESDS_ATOM_SIZE;

    if(count >= MAX_DECSPECINFO_COUNT)
    {
        /*boundary check*/
        return NvSuccess;
    }

    if (pTrackInfo->pESDS)
        NvOsFree (pTrackInfo->pESDS);

    ReadCount = ESDSSize - 8;
    pTrackInfo->pDecSpecInfo[count] = NvOsAlloc(ReadCount * sizeof(NvU8));
    NVMM_CHK_MEM (pTrackInfo->pDecSpecInfo[count]);

    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*)
            pTrackInfo->pDecSpecInfo[count], ReadCount));

    FileBytesRead += ReadCount;
    pTrackInfo->DecInfoSize[count] = ReadCount;
    pTrackInfo->nDecSpecInfoCount++;

    *pSamplesRead = FileBytesRead;

cleanup:
    if (Status != NvSuccess)
    {
        if (pTrackInfo->pESDS)
        {
            NvOsFree (pTrackInfo->pESDS);
            pTrackInfo->pESDS = NULL;
        }
    }
    return Status;
}


static NvU32
Mp4ParserAudioSampleEntry (
    NvMp4Parser * pNvMp4Parser,
    NvMp4TrackInformation *pAudioTrackInfo)
{
    NvError Status = NvSuccess;
    NvU8 TmpBuffer[20];
    NvU8 *ReadBuffer;
    NvU32 BytesRead = 0;
    NvU8 identified = 0;
    NvU32 AtomType;
    NvU32 AtomSize;
    NvU32 ESDSSize;
    NvU32 Version;
    NvU32 Size;
    NvU32 tag;
    NvU32 TmpBytesRead, param;
    NvU32 flag;
    NvU32 FileBytesRead = 0;
    NvU32 StreamDependenceFlag;
    NvU32 URLFlag;
    NvU32 URLLength;
    NvU32 StreamType;
    NvU32 ReadCount;
    NvU64 FileOffset;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;
    NvU32 DecSpecInfoCount = 0;
    NvU32 DecSpecInfoSize = 0;

#ifdef MPEG4_CALLBACK
    NvS8 *amsBuff;
#endif
    ReadBuffer = TmpBuffer;
    FileBytesRead += 8;
    Size = pAudioTrackInfo->AtomSize;
    AtomType = pAudioTrackInfo->AtomType;
    if (AtomType == MP4_FOURCC('m', 'p', '4', 'a'))
    {
        // TODO: We can combine these multiple reads into one
        // NvMp4StreamType_Audio Sample Entry starts, skip first 6 bytes: reserved==0
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        FileBytesRead += 8;
        // reserved data
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        FileBytesRead += 8;
        Version = ( (ReadBuffer[0] << 8) | ReadBuffer[1]);
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        FileBytesRead += 8;
        ReadCount = 4;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        FileBytesRead += 4;
        // 4 more 32-bit fields in Version 1
        if (Version >= 1)
        {
            ReadCount = 16;
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
            FileBytesRead += 16;
        }
        // 5 more 32-bit fields in Version 2
        if (Version == 2)
        {
            ReadCount = 20;
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
            FileBytesRead += 20;
        }
        // ESDS AtomType starts here (14496-1, page 28)
        while (identified != 1)
        {
            ReadCount = 8;
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
            FileBytesRead += 8;
            AtomType = NV_BE_TO_INT_32 (&ReadBuffer[4]);
            AtomSize = NV_BE_TO_INT_32 (&ReadBuffer[0]);
            if (AtomSize < 8)
            {
                goto cleanup;
            }
            if (AtomType == MP4_FOURCC('w', 'a', 'v', 'e')) // ESDS may be inside of a WAVE AtomType
            {
                // do nothing, we want to look inside
            }
            else if (AtomType == MP4_FOURCC('e', 's', 'd', 's'))
            {
                ESDSSize = AtomSize - 8;
                if (ESDSSize > MAX_ESDS_BYTES)
                {
                    goto cleanup;
                }
                if (pAudioTrackInfo->pESDS)
                    NvOsFree (pAudioTrackInfo->pESDS);

                pAudioTrackInfo->pESDS = (NvU8*) NvOsAlloc (ESDSSize * sizeof (NvU8));
                if (!pAudioTrackInfo->pESDS)
                {
                    goto cleanup;
                }
                ReadCount = ESDSSize;
                NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) &pAudioTrackInfo->pESDS[0], ReadCount));
                FileBytesRead += ESDSSize;
                ReadBuffer = &pAudioTrackInfo->pESDS[0];
                Version = (NvU32) ReadBuffer[0];
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "AudioSampleEntry - ESDS AtomType -- Version=%d\n", Version));
                if (Version != 0)
                {
                    goto cleanup;
                }
                ReadBuffer += 4;
                BytesRead = 4;
                tag = ReadBuffer[0];
                BytesRead += 1;
                ReadBuffer += 1;
                if (tag == ES_DESC_TAG)
                {
                    TmpBytesRead = Mp4GetExpandableParam (&ReadBuffer[0], &param);
                    BytesRead += TmpBytesRead;
                    ReadBuffer += TmpBytesRead;
                    // ES_ID = (ReadBuffer[0] << 8) | (ReadBuffer[1]);
                    ReadBuffer += 2;
                    BytesRead += 2;
                    flag = ReadBuffer[0];
                    // TODO: Replace Magic numbers with appropriate named macros
                    StreamDependenceFlag = flag >> 7;
                    URLFlag = (flag >> 6) & 1;
                    ReadBuffer += 1;
                    BytesRead += 1;
                    if (StreamDependenceFlag)
                    {
                        ReadBuffer += 2;
                        BytesRead += 2;
                    }
                    if (URLFlag)
                    {
                        URLLength = ReadBuffer[0];

                        /* The string is not read and the URL is ignored here */
                        ReadBuffer += (URLLength + 1);
                        BytesRead += (URLLength + 1);
                    }

                    /**
                     *  Assuming that DecoderConfigDescriptior, SLConfigDescriptor,
                     *  IPI_DescrPoNvS32er, IP_IdentificationDataSet, IPMP_Descriptor,
                     *  LanguageDescriptor, QoS_descriptor, RegistrationDescriptor and
                     *  ExtensionDescriptor instantiations occur in a sequential manner.
                     *  Ignore all expcept DecoderConfigDescriptor
                     */
                    tag = ReadBuffer[0];
                    ReadBuffer += 1;
                    BytesRead += 1;
                    if (tag == DECODER_CONGIF_DESCR_TAG)
                    {
                        TmpBytesRead = Mp4GetExpandableParam (&ReadBuffer[0], &param);
                        BytesRead += TmpBytesRead;
                        ReadBuffer += TmpBytesRead;
                        pAudioTrackInfo->ObjectType = ReadBuffer[0];
                        StreamType = ReadBuffer[1] >> 2;
                        // TODO: Parser should not decide on which codec to be supported.
                        if (( (pAudioTrackInfo->ObjectType != MPEG4_QCELP) && (pAudioTrackInfo->ObjectType != MPEG4_EVRC) &&
                                (pAudioTrackInfo->ObjectType != MPEG4_AUDIO) &&
                                (pAudioTrackInfo->ObjectType != MPEG2AACLC_AUDIO) /*&&  //Ignore mp3 tracks
                            (pAudioTrackInfo->objectType != MP3_AUDIO)*/) ||
                                (StreamType != AUDIO_STREAM))
                        {
                            goto cleanup;
                        }

                        pAudioTrackInfo->MaxBitRate = NV_BE_TO_INT_32 (&ReadBuffer[5]);
                        pAudioTrackInfo->AvgBitRate = NV_BE_TO_INT_32 (&ReadBuffer[9]);
                        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "AAC  Audio: MaxBitRate = %d - AvgBitRate = %d\n",
                                            pAudioTrackInfo->MaxBitRate, pAudioTrackInfo->AvgBitRate));
                        BytesRead += 13;
                        ReadBuffer += 13;
                        tag = ReadBuffer[0];
                        if (tag == DECODER_SPECIFIC_INFO_TAG)
                        {
                            identified = 1;
                            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "AUDIO --- DECODER_SPECIFIC_INFO_TAG"));
                            ReadBuffer += 1;
                            BytesRead += 1;
                            TmpBytesRead = Mp4GetExpandableParam (&ReadBuffer[0], &param);
                            BytesRead += TmpBytesRead;
                            ReadBuffer += TmpBytesRead;
                            if (param > ESDSSize)
                            {
                                goto cleanup;
                            }
                            DecSpecInfoCount = pAudioTrackInfo->nDecSpecInfoCount;
                            DecSpecInfoSize = param;
                            if(DecSpecInfoCount >= MAX_DECSPECINFO_COUNT)
                            {
                                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_ERROR,
                                    "AUDIO --- exceeds MAX_DECSPECINFO_COUNT.\n"));
                                goto cleanup;
                            }
                            pAudioTrackInfo->pDecSpecInfo[DecSpecInfoCount]
                                    = NvOsAlloc(DecSpecInfoSize * sizeof(NvU8));
                            NVMM_CHK_MEM (pAudioTrackInfo->pDecSpecInfo[DecSpecInfoCount]);
                            NvOsMemcpy(pAudioTrackInfo->pDecSpecInfo[DecSpecInfoCount],
                                    &pAudioTrackInfo->pESDS[BytesRead], DecSpecInfoSize);
                            pAudioTrackInfo->DecInfoSize[DecSpecInfoCount] = DecSpecInfoSize;
                            pAudioTrackInfo->nDecSpecInfoCount++;

                            ReadBuffer += param;
                            BytesRead += param;
                        }
                    }
                }
            }
            else
            {
                // skip NvMp4TrackTypes_OTHER atoms
                FileOffset = AtomSize - 8;
                NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginCur));
                FileBytesRead += (AtomSize - 8);
                identified = (FileBytesRead >= Size);
            }
        }
    }
    else
    {
        ReadCount = Size - 8;
        if (ReadCount > MAX_ESDS_ATOM_SIZE)
        {
            goto cleanup;
        }
        if (pAudioTrackInfo->pESDS)
            NvOsFree (pAudioTrackInfo->pESDS);

        DecSpecInfoCount = pAudioTrackInfo->nDecSpecInfoCount;
        if(DecSpecInfoCount >= MAX_DECSPECINFO_COUNT)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_ERROR,
                "AUDIO --- exceeds MAX_DECSPECINFO_COUNT.\n"));
            goto cleanup;
        }

        pAudioTrackInfo->pDecSpecInfo[DecSpecInfoCount]
                = NvOsAlloc(ReadCount * sizeof(NvU8));
        NVMM_CHK_MEM (pAudioTrackInfo->pDecSpecInfo[DecSpecInfoCount]);
        NVMM_CHK_CP(pPipe->Read(pNvMp4Parser->hContentPipe, (CPbyte*)
                pAudioTrackInfo->pDecSpecInfo[DecSpecInfoCount], ReadCount));
        FileBytesRead += ReadCount ;
        pAudioTrackInfo->DecInfoSize[DecSpecInfoCount] = ReadCount;
        pAudioTrackInfo->nDecSpecInfoCount++;

    }
    return FileBytesRead;
cleanup:
    if (pAudioTrackInfo->pESDS)
    {
        NvOsFree (pAudioTrackInfo->pESDS);
        pAudioTrackInfo->pESDS = NULL;
    }
    return FileBytesRead;
}


static NvError
Mp4ParserVideoSampleEntry (
    NvMp4Parser * pNvMp4Parser,
    NvMp4TrackInformation *pVideoTrackInfo,
    NvU32 *pSamplesRead)
{
    NvError Status = NvSuccess;
    NvU8 TmpBuffer[16];
    NvU8 *ReadBuffer;
    NvU32 BytesRead = 0;
    NvU8 identified = 0, flag;
    NvU32 AtomType;
    NvU32 AtomSize;
    NvU32 ESDSSize;
    NvU32 Version;
    NvU32 tag;
    NvU32 TmpBytesRead, param;
    NvU32 FileBytesRead = 0;
    NvU32 StreamDependenceFlag;
    NvU32 URLFlag;
    NvU32 URLLength;
    NvU32 StreamType;
    NvU32 ReadCount;
    NvU64 FileOffset;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;
    NvU32 DecSpecInfoCount = 0;
    NvU32 DecSpecInfoSize = 0;
    /* The first two NvS32s may be
        inside or outside this routine.... For now, it
        does not matter because we can support only one
        NvMp4StreamType_Audio entry so far */
    ReadBuffer = TmpBuffer;

    /* The actual NvMp4StreamType_Video Sample Entry starts */
    /* Skipping first 6 bytes: reserved==0 */
    // TODO: We can combine these multiple reads into one    
    ReadCount = 8;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
    FileBytesRead += 8;
    /* reserved data */
    ReadCount = 16;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
    FileBytesRead += 16;
    ReadCount = 16;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
    pVideoTrackInfo->Width = (ReadBuffer[0] << 8) | (ReadBuffer[1]);
    pVideoTrackInfo->Height = (ReadBuffer[2] << 8) | (ReadBuffer[3]);
    FileBytesRead += 16;
    ReadCount = 10;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
    FileBytesRead += 10;
    ReadCount = 16;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
    FileBytesRead += 16;
    ReadCount = 12;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
    FileBytesRead += 12;

    /* ESDS AtomType starts here (14496-1, page 28) */
    while (identified != 1)
    {
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        FileBytesRead += 8;
        AtomType = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        AtomSize = NV_BE_TO_INT_32 (&ReadBuffer[0]);
        if (AtomSize < 8)
        {
            goto cleanup;
        }
        if (AtomType == MP4_FOURCC('e', 's', 'd', 's'))
        {
            identified = 1;
            ESDSSize = AtomSize - 8;
            if (ESDSSize > MAX_ESDS_BYTES)
            {
                goto cleanup;
            }
            if (pVideoTrackInfo->pESDS)
                NvOsFree (pVideoTrackInfo->pESDS);

            pVideoTrackInfo->pESDS = (NvU8*) NvOsAlloc (ESDSSize * sizeof (NvU8));
            if (!pVideoTrackInfo->pESDS)
            {
                goto cleanup;
            }
            ReadCount = ESDSSize;
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) &pVideoTrackInfo->pESDS[0], ReadCount));
            FileBytesRead += ESDSSize;
            ReadBuffer = &pVideoTrackInfo->pESDS[0];
            Version = (NvU32) ReadBuffer[0];
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "VideoSampleEntry - ESDS AtomType -- Version=%d\n", Version));
            if (Version != 0)
            {
                goto cleanup;
            }
            ReadBuffer += 4;
            BytesRead = 4;
            tag = ReadBuffer[0];
            BytesRead += 1;
            ReadBuffer += 1;
            if (tag == ES_DESC_TAG)
            {
                TmpBytesRead = Mp4GetExpandableParam (&ReadBuffer[0], &param);

                BytesRead += TmpBytesRead;
                ReadBuffer += TmpBytesRead;
                ReadBuffer += 2;
                BytesRead += 2;
                flag = ReadBuffer[0];
                // TODO: Replace Magic numbers with appropriate named macros
                StreamDependenceFlag = flag >> 7;
                URLFlag = (flag >> 6) & 1;
                ReadBuffer += 1;
                BytesRead += 1;
                if (StreamDependenceFlag)
                {
                    ReadBuffer += 2;
                    BytesRead += 2;
                }
                if (URLFlag)
                {
                    URLLength = ReadBuffer[0];

                    /* The string is not read and the URL is ignored here */
                    ReadBuffer += (URLLength + 1);
                    BytesRead += (URLLength + 1);
                }

                /* Assuming that DecoderConfigDescriptior, SLConfigDescriptor,
                    IPI_DescrPoNvS32er, IP_IdentificationDataSet, IPMP_Descriptor,
                    LanguageDescriptor, QoS_descriptor, RegistrationDescriptor and
                    ExtensionDescriptor instantiations occur in a sequential manner.
                    Ignore all expcept DecoderConfigDescriptor */

                tag = ReadBuffer[0];
                ReadBuffer += 1;
                BytesRead += 1;
                if (tag == DECODER_CONGIF_DESCR_TAG)
                {
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "VIDEO --- DECODER_CONGIF_DESCR_TAG"));
                    TmpBytesRead = Mp4GetExpandableParam (&ReadBuffer[0], &param);
                    BytesRead += TmpBytesRead;
                    ReadBuffer += TmpBytesRead;
                    pVideoTrackInfo->ObjectType = ReadBuffer[0];
                    StreamType = ReadBuffer[1] >> 2;
                    if ( (pVideoTrackInfo->ObjectType != MPEG4_VIDEO) || (StreamType != VIDEO_STREAM))
                    {
                        goto cleanup;
                    }
                    pVideoTrackInfo->MaxBitRate = NV_BE_TO_INT_32 (&ReadBuffer[5]);
                    pVideoTrackInfo->AvgBitRate = NV_BE_TO_INT_32 (&ReadBuffer[9]);
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Mpeg4 video: VaxBitRate = %d - AvgBitRate = %d\n",
                                        pVideoTrackInfo->MaxBitRate, pVideoTrackInfo->AvgBitRate));
                    BytesRead += 13;
                    ReadBuffer += 13;
                    tag = ReadBuffer[0];
                    if (tag == DECODER_SPECIFIC_INFO_TAG)
                    {
                        // Field :DecoderSpecificInfo(video)   //value
                        //(8) DecoderSpecificInfo tag         //0x05
                        //(8) descriptor size                 //---
                        //(8*) DecoderSpecificInfo data       //---

                        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "VIDEO --- DECODER_SPECIFIC_INFO_TAG"));
                        ReadBuffer += 1;
                        BytesRead += 1;
                        TmpBytesRead = Mp4GetExpandableParam (&ReadBuffer[0], &param);
                        BytesRead += TmpBytesRead;
                        ReadBuffer += TmpBytesRead;
                        if (param > ESDSSize)
                        {
                            goto cleanup;
                        }
                        DecSpecInfoCount = pVideoTrackInfo->nDecSpecInfoCount;
                        DecSpecInfoSize = param;
                        if(DecSpecInfoCount >= MAX_DECSPECINFO_COUNT)
                        {
                            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_ERROR,
                                "VIDEO --- exceeds MAX_DECSPECINFO_COUNT.\n"));
                            goto cleanup;
                        }

                        pVideoTrackInfo->pDecSpecInfo[DecSpecInfoCount]
                                = NvOsAlloc(DecSpecInfoSize * sizeof(NvU8));
                        NVMM_CHK_MEM (pVideoTrackInfo->pDecSpecInfo[DecSpecInfoCount]);

                        NvOsMemcpy(pVideoTrackInfo->pDecSpecInfo[DecSpecInfoCount],
                                &pVideoTrackInfo->pESDS[BytesRead], DecSpecInfoSize);

                        pVideoTrackInfo->DecInfoSize[DecSpecInfoCount] = DecSpecInfoSize;
                        pVideoTrackInfo->nDecSpecInfoCount++;

                        ReadBuffer += param;
                        BytesRead += param;
                    }
                }
            }
        }
        else
        {
            /* For any unidentified AtomType (NvMp4TrackTypes_OTHER than ESDS) */
            FileOffset = AtomSize - 8;
            NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginCur));
            FileBytesRead += (AtomSize - 8);
        }
    }
    *pSamplesRead = FileBytesRead;
    return Status;
cleanup:
    if (pVideoTrackInfo->pESDS)
    {
        NvOsFree (pVideoTrackInfo->pESDS);
        pVideoTrackInfo->pESDS = NULL;
    }
    return NvError_ParserFailure;
}

static NvError
Mp4IdentifyMDAT (
    NvMp4Parser * pNvMp4Parser)
{
    NvError Status = NvSuccess;
    NvU8 HdrBytesRead = 0;
    NvU32 AtomType = 0;
    NvU64 AtomSize = 0;
    NvU32 MP4_FOURCC_MDAT= MP4_FOURCC('m', 'd', 'a', 't');

    //Check if mdat size is already calculated
    if (pNvMp4Parser->MDATSize > 1)
        return NvSuccess;

    NVMM_CHK_CP(pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64)0, CP_OriginBegin));

    while (Status == NvSuccess && AtomType !=  MP4_FOURCC_MDAT)
    {
        Status = Mp4ParserATOMH (pNvMp4Parser, &AtomType, &AtomSize, &HdrBytesRead);

        if (Status == NvSuccess && AtomType !=  MP4_FOURCC_MDAT)
        {
            Status = pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) (AtomSize - HdrBytesRead), CP_OriginCur);
        }
    }

    if (AtomType == MP4_FOURCC_MDAT)
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Detected MDAT AtomType, Size - %x", AtomSize));
        pNvMp4Parser->MDATSize = AtomSize;
    }

cleanup:
    return Status;
}

/*
 ******************************************************************************
 */

static NvError
Mp4ParserMVHD (
    NvMp4Parser * pNvMp4Parser,
    NvU64 mvhdSize,
    NvU64* pBytesRead)
{
    NvError Status = NvSuccess;
    NvU8 ReadBuffer[112];
    NvU32 VersionFlag;
    NvU32 LowInt, HighInt, ReadCount;
    NvU64 BytesRead = 0;
    NvMp4MovieData * MvHdData = &pNvMp4Parser->MvHdData;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;

    ReadCount = 4;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
    BytesRead += ReadCount;

    VersionFlag = ReadBuffer[0];
    if (VersionFlag == 0)
        ReadCount = 96;
    else if (VersionFlag == 1)
        ReadCount = 108;
    else
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MPEGVERSION_ERROR = %d\n", VersionFlag));
        Status = NvError_UnSupportedStream;
        goto cleanup;
    }

    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
    BytesRead += ReadCount;

    if (VersionFlag == 0)
    {
        MvHdData->CreationTime = NV_BE_TO_INT_32 (&ReadBuffer[0]);
        MvHdData->ModifiedTime = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        MvHdData->TimeScale = NV_BE_TO_INT_32 (&ReadBuffer[8]);
        MvHdData->Duration = NV_BE_TO_INT_32 (&ReadBuffer[12]);
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MPEG_VERSION_0 = %d\n", VersionFlag));
    }
    else if (VersionFlag == 1)
    {
        LowInt = NV_BE_TO_INT_32 (&ReadBuffer[0]); //creationTime_low
        HighInt = NV_BE_TO_INT_32 (&ReadBuffer[4]); //creationTime_high
        MvHdData->CreationTime = ( (NvU64) LowInt << 32) | (NvU64) HighInt; //creationTime_high
        LowInt = NV_BE_TO_INT_32 (&ReadBuffer[8]); //ModificationTime_low
        HighInt = NV_BE_TO_INT_32 (&ReadBuffer[12]); //ModificationTime_high
        MvHdData->ModifiedTime = ( (NvU64) LowInt << 32) | (NvU64) HighInt; //Modification Time
        MvHdData->TimeScale = NV_BE_TO_INT_32 (&ReadBuffer[16]);
        LowInt = NV_BE_TO_INT_32 (&ReadBuffer[20]); //duration_low
        HighInt = NV_BE_TO_INT_32 (&ReadBuffer[24]); //duration_high
        MvHdData->Duration = ( (NvU64) LowInt << 32) | (NvU64) HighInt; //duration
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MPEG_VERSION_1 = %d\n", VersionFlag));
    }

    if (MvHdData->TimeScale == 0 || MvHdData->Duration == 0)
    {
        Status = NvError_ParserFailure;
        goto cleanup;
    }

    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MVHD: Duration = %d, TimeScale = %d, MediaTime = %d\n", MvHdData->Duration, MvHdData->TimeScale, (NvU64) MvHdData->Duration / MvHdData->TimeScale));

    if (VersionFlag == 0)
        MvHdData->NextTrackID = NV_BE_TO_INT_32 (&ReadBuffer[92]);
    else if (VersionFlag == 1)
        MvHdData->NextTrackID = NV_BE_TO_INT_32 (&ReadBuffer[104]);

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

/*
 ******************************************************************************
 */


static NvError
Mp4ParserTKHD (
    NvMp4Parser * pNvMp4Parser,
    NvU64 TKHDSize,
    NvU64 *pBytesRead,
    NvU64 *pTrackHdrInfo)
{
    NvError Status = NvSuccess;
    NvU8 ReadBuffer[104]; // track header is 92 bytes Version 0 92bytes: ver-1: 104bytes
    NvU64 ReadCount = 0;
    NvU32 LowInt, HighInt, tmp = 0;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;

    //Track header is (92 bytes - Version 0) : (104 bytes - ver-1)
    NVMM_CHK_ARG(TKHDSize <=104);

    ReadCount = TKHDSize;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) ReadCount));

    pTrackHdrInfo[0] = ReadBuffer[0]; //Version
    if (pTrackHdrInfo[0] == 0)
    {
        pTrackHdrInfo[1] = NV_BE_TO_INT_32 (&ReadBuffer[4]); //Creation Time
        pTrackHdrInfo[2] = NV_BE_TO_INT_32 (&ReadBuffer[8]); // Modification Time
        pTrackHdrInfo[3] = (NvU32) NV_BE_TO_INT_32 (&ReadBuffer[12]); // Track ID
        pTrackHdrInfo[4] = NV_BE_TO_INT_32 (&ReadBuffer[20]); // Duration
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "TKHD: MPEG_VERSION_0 = %d -- TrackDuration = %d\n", ReadBuffer[0], pTrackHdrInfo[4]));
    }
    else if (pTrackHdrInfo[0]  == 1)
    {
        LowInt = NV_BE_TO_INT_32 (&ReadBuffer[4]); //Creation Time_low
        HighInt = NV_BE_TO_INT_32 (&ReadBuffer[8]); //Creation Time_high
        pTrackHdrInfo[1] = ( (NvU64) LowInt << 32) | (NvU64) HighInt; //creation time
        LowInt = NV_BE_TO_INT_32 (&ReadBuffer[12]); //Modification Time_low
        HighInt = NV_BE_TO_INT_32 (&ReadBuffer[16]); //Modification Time_high
        pTrackHdrInfo[2] = ( (NvU64) LowInt << 32) | (NvU64) HighInt; //Modification time
        pTrackHdrInfo[3] = (NvU32) NV_BE_TO_INT_32 (&ReadBuffer[20]); // Track ID
        LowInt = NV_BE_TO_INT_32 (&ReadBuffer[28]); //Duration Time_low
        HighInt = NV_BE_TO_INT_32 (&ReadBuffer[32]); //Duration Time_high
        pTrackHdrInfo[4] = ( (NvU64) LowInt << 32) | (NvU64) HighInt; //Duration
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MPEG_VERSION_1 = %d\n", ReadBuffer[0]));
    }
    else
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MPEGVERSION_ERROR = %d\n", ReadBuffer[0]));
        Status = NvError_UnSupportedStream;
        goto cleanup;
    }

    // TODO: tmp and the magic numbers needs to replaced with proper names

    if (pTrackHdrInfo[0] == 0)
        tmp = NV_BE_TO_INT_32 (&ReadBuffer[36]); //int(16)volume = {if track_is_audio 0x0100 else  0};
    else if (pTrackHdrInfo[0] == 1)
        tmp = NV_BE_TO_INT_32 (&ReadBuffer[48]); //int(16)volume = {if track_is_audio 0x0100 else  0};
        
    pTrackHdrInfo[5] = 0;  // Track info  (NvMp4StreamType_Audio / NvMp4StreamType_Video)

    if ( (tmp == 0x01000000))
    {
        pTrackHdrInfo[5] = AUDIO_TRACK;
    }

    if (pTrackHdrInfo[0] == 0)
    {
        tmp = NV_BE_TO_INT_32 (&ReadBuffer[76]);
    }
    else if (pTrackHdrInfo[0] == 1)
    {
        tmp = NV_BE_TO_INT_32 (&ReadBuffer[88]);
    }

    if ( (tmp == 0x01400000)) //320
    {
        pTrackHdrInfo[5] = VIDEO_TRACK; //unsigned int(32) width in 16.16
    }

    if (pTrackHdrInfo[0] == 0)
    {
        tmp = NV_BE_TO_INT_32 (&ReadBuffer[80]);
    }
    else if (pTrackHdrInfo[0] == 1)
    {
        tmp = NV_BE_TO_INT_32 (&ReadBuffer[92]);
    }

    if ( (tmp == 0x00F00000)) //240
    {
        pTrackHdrInfo[5] = VIDEO_TRACK;//unsigned int(32) height in 16.16
    }

cleanup:
    *pBytesRead = ReadCount;

    return Status;
}

static NvError
Mp4ParserUDTA3GP (
    NvMp4Parser *pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64 *pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 AtomBytesLeft = AtomSize;
    NvU8 *pUdata = NULL;
    NvU8 *pUdataTemp = NULL;
    NvU64 UdataSize = 0;
    NvMMMetaDataCharSet EncodeType;
    NvU32 StringSize;
    NvU16 Year;
    NvS32 RC =0;
    NvS8 *pTextString = NULL;
    NvU32 *pTextSize = NULL;
    NvMMMetaDataCharSet *pTextEncoding = NULL;

    NVMM_CHK_ARG(pNvMp4Parser);
    NVMM_CHK_ARG(pBytesRead);

    if (AtomBytesLeft < 4)
    {
        goto cleanup;
    }

    /*
     * BoxHeader.Size       Unsigned int(32)
     * BoxHeader.Type       Unsigned int(32)    'xxxx'
     * BoxHeader.Version    Unsigned int(8)     0
     * BoxHeader.Flags      Bit(24)             0
     *
     * The Box (or Atom) handled in this function is user data defined in .3pg
     * file format specification. Those boxes are grouped because they have
     * common structure (BoxHeader). When this function is called, file pointer
     * is at BoxHeader.Version byte, we skip 4 bytes to the beginning of box
     * contain.
     * 4 = 1 for Version + 3 for Flags
     */
    NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(pNvMp4Parser->hContentPipe,
            4, CP_OriginCur));
    AtomBytesLeft -= 4;

    /*
     * pUdata points to the first data byte of this User Data box, and UdataSize
     * indicates the number of valid data.
     */
    pUdata = NvOsAlloc(AtomBytesLeft);
    NVMM_CHK_MEM(pUdata);
    NVMM_CHK_ERR(pNvMp4Parser->pPipe->cpipe.Read(pNvMp4Parser->hContentPipe,
            (CPbyte *)pUdata, AtomBytesLeft));
    UdataSize = AtomBytesLeft;
    AtomBytesLeft = 0;

    if (AtomType == MP4_FOURCC('t', 'i', 't', 'l'))
    {
        /*
         * Pad                  Bit(1)              0
         * Language             Unsigned int(5)[3]  Packed ISO-639-2/T language code
         * Title                String              Text of title
         *
         */
        pTextString = &(pNvMp4Parser->MediaMetadata.Title[0]);
        pTextSize = &pNvMp4Parser->MediaMetadata.TitleSize;
        pTextEncoding = &pNvMp4Parser->MediaMetadata.TitleEncoding;
    }
    else if(AtomType == MP4_FOURCC('c', 'p', 'r', 't'))
    {
        /*
         * Pad                  Bit(1)                  0
         * Language             Unsigned int(5)[3]      Packed ISO-639-2/T language code
         * Performer            String                  Text of copyright notice
         */
        pTextString = &(pNvMp4Parser->MediaMetadata.Copyright[0]);
        pTextSize = &pNvMp4Parser->MediaMetadata.CopyrightSize;
        pTextEncoding = &pNvMp4Parser->MediaMetadata.CopyrightEncoding;

    }
    else if(AtomType == MP4_FOURCC('p', 'e', 'r', 'f'))
    {
        /*
         * Pad                  Bit(1)                  0
         * Language             Unsigned int(5)[3]      Packed ISO-639-2/T language code
         * Performer            String                  Text of performer
         */
        pTextString = &(pNvMp4Parser->MediaMetadata.Artist[0]);
        pTextSize = &pNvMp4Parser->MediaMetadata.ArtistSize;
        pTextEncoding = &pNvMp4Parser->MediaMetadata.ArtistEncoding;

    }
    else if(AtomType == MP4_FOURCC('g', 'n', 'r', 'e'))
    {
        /*
         * Pad                  Bit(1)                  0
         * Language             Unsigned int(5)[3]      Packed ISO-639-2/T language code
         * Performer            String                  Text of genre
         */
        pTextString = &(pNvMp4Parser->MediaMetadata.Genre[0]);
        pTextSize = &pNvMp4Parser->MediaMetadata.GenreSize;
        pTextEncoding = &pNvMp4Parser->MediaMetadata.GenreEncoding;
    }
    else if(AtomType == MP4_FOURCC('a', 'u', 't', 'h'))
    {
        /*
         * Pad                  Bit(1)                  0
         * Language             Unsigned int(5)[3]      Packed ISO-639-2/T language code
         * Performer            String                  Text of author
         */
        pTextString = &(pNvMp4Parser->MediaMetadata.AlbumArtist[0]);
        pTextSize = &pNvMp4Parser->MediaMetadata.AlbumArtistSize;
        pTextEncoding = &pNvMp4Parser->MediaMetadata.AlbumArtistEncoding;
    }
    else if(AtomType == MP4_FOURCC('a', 'l', 'b', 'm'))
    {
        /*
         * Pad                  Bit(1)                  0
         * Language             Unsigned int(5)[3]      Packed ISO-639-2/T language code
         * Performer            String                  Text of album title
         * TrackNumber          Unsigned int(8)         Optional integer with track number
         */
        pTextString = &(pNvMp4Parser->MediaMetadata.Album[0]);
        pTextSize = &pNvMp4Parser->MediaMetadata.AlbumSize;
        pTextEncoding = &pNvMp4Parser->MediaMetadata.AlbumEncoding;

        /* the last byte might be a TrackNumber, let's test it */
        if (0x00 != pUdata[UdataSize -1])
        {
            /*none-zero, it's track number */
            pNvMp4Parser->MediaMetadata.TrackNumber = pUdata[UdataSize - 1];
            UdataSize -= 1;
        }
    }
    else if(AtomType == MP4_FOURCC('y', 'r', 'r', 'c'))
    {
        /*
         * RecordingYear        Unsigned int(16)        Integer value of recording year
         */
        if (UdataSize != 2 )
        {
            /* invalid BOX, recording year box must be exact two bytes */
            goto cleanup;
        }

        Year = (pUdata[0] << 8) | (pUdata[1]);
        RC = NvOsSnprintf((char *)pNvMp4Parser->MediaMetadata.Year,
                MP4_MAX_METADATA, "%u", Year);
        if (RC <= -1)
        {
            /* NvOsSnprintf failed */
            goto cleanup;
        }
        pNvMp4Parser->MediaMetadata.YearSize = RC + 1; /* add '\0' byte */
        pNvMp4Parser->MediaMetadata.YearEncoding = NvMMMetaDataEncode_Utf8;
    }
    else
    {
        /* unknown Box, skip copy data */
        goto cleanup;
    }

    /* handle text string type BOX */
    if (NULL != pTextString)
    {
        if (UdataSize < 2 )
        {
            /*
             * invalid BOX, text string type BOX should have at least Pad(1 bit)
             * and Language code (15 bits).
             */
            goto cleanup;
        }

        /* pUdataTemp points to first byte of string */
        pUdataTemp = &(pUdata[2]);
        StringSize = UdataSize - 2;

        if (StringSize >= 4)
        {
            /* make sure we won't access invalid data */
            if ((0xFE == pUdataTemp[0]) && (0xFF == pUdataTemp[1]))
            {
                /* we don't support UTF16-BE (big endian unicode) */
                goto cleanup;
            }
            if ((0xFF == pUdataTemp[0]) && (0xFE == pUdataTemp[1]))
            {
                /* UTF16-LE */
                EncodeType = NvMMMetaDataEncode_Utf16;
            }
            else
            {
                /* UTF-8 */
                EncodeType = NvMMMetaDataEncode_Utf8;
            }
        }
        else
        {
            /* size < 4, it has to be UTF-8 for string */
            EncodeType = NvMMMetaDataEncode_Utf8;
        }

        /* save text string */
        StringSize = (StringSize > MP4_MAX_METADATA) ? MP4_MAX_METADATA : StringSize;
        NvOsMemcpy(pTextString, pUdataTemp, StringSize);
        *pTextSize = StringSize;
        *pTextEncoding = EncodeType;
        /* make sure null-terminated */
        pTextString[StringSize - 1] = 0;
        if (NvMMMetaDataEncode_Utf16 == EncodeType)
        {
            pTextString[StringSize - 2] = 0;
        }
    }

cleanup:
    if (pUdata)
    {
        NvOsFree(pUdata);
        pUdata = NULL;
    }
    *pBytesRead = AtomSize - AtomBytesLeft;
    return Status;
}

static NvError
Mp4ParserDATA (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 FilePos;
    NvU64 BytesRead = 0;
    NvU8 ReadBuffer[8];
    NvU32 Size = MP4_MAX_METADATA - 1;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;
    NvU32 ParentAtom = * (NvU32 *) pData;
    NvS8* MetaBuf = NULL;
    NvU32* MetaSize = NULL;
    NvMMMetaDataCharSet *MetaEncoding = NULL;

    if (AtomType == MP4_FOURCC('d', 'a', 't', 'a'))
    {
        // 1byte Version + 3bytes AtomType flag + 4 byte NULL space
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) 8));
        BytesRead += 8;

        if (ParentAtom == MP4_FOURCC(0xa9, 'n', 'a', 'm'))
        {
            MetaBuf = pNvMp4Parser->MediaMetadata.Title;
            MetaSize = &pNvMp4Parser->MediaMetadata.TitleSize;
            MetaEncoding = &pNvMp4Parser->MediaMetadata.TitleEncoding;
        }
        else if (ParentAtom == MP4_FOURCC(0xa9, 'A', 'R', 'T'))
        {
            MetaBuf = pNvMp4Parser->MediaMetadata.Artist;
            MetaSize = &pNvMp4Parser->MediaMetadata.ArtistSize;
            MetaEncoding = &pNvMp4Parser->MediaMetadata.ArtistEncoding;
        }
        else if (ParentAtom == MP4_FOURCC('a', 'A', 'R', 'T'))
        {
            MetaBuf = pNvMp4Parser->MediaMetadata.AlbumArtist;
            MetaSize = &pNvMp4Parser->MediaMetadata.AlbumArtistSize;
            MetaEncoding = &pNvMp4Parser->MediaMetadata.AlbumArtistEncoding;
        }
        else if (ParentAtom == MP4_FOURCC(0xa9, 'a', 'l', 'b'))
        {
            MetaBuf = pNvMp4Parser->MediaMetadata.Album;
            MetaSize = &pNvMp4Parser->MediaMetadata.AlbumSize;
            MetaEncoding = &pNvMp4Parser->MediaMetadata.AlbumEncoding;
        }
        else if (ParentAtom == MP4_FOURCC(0xa9, 'g', 'e', 'n'))
        {
            MetaBuf = pNvMp4Parser->MediaMetadata.Genre;
            MetaSize = &pNvMp4Parser->MediaMetadata.GenreSize;
            MetaEncoding = &pNvMp4Parser->MediaMetadata.GenreEncoding;
        }
        else if (ParentAtom == MP4_FOURCC(0xa9, 'd', 'a', 'y'))
        {
            MetaBuf = pNvMp4Parser->MediaMetadata.Year;
            MetaSize = &pNvMp4Parser->MediaMetadata.YearSize;
            MetaEncoding = &pNvMp4Parser->MediaMetadata.YearEncoding;
        }
        else if (ParentAtom == MP4_FOURCC(0xa9, 'w', 'r', 't'))
        {
            MetaBuf = pNvMp4Parser->MediaMetadata.Composer;
            MetaSize = &pNvMp4Parser->MediaMetadata.ComposerSize;
            MetaEncoding = &pNvMp4Parser->MediaMetadata.ComposerEncoding;
        }

        if (MetaBuf)
        {
            if ( (Size + 8) > AtomSize)
            {
                Size = (NvU32) AtomSize - 8;
                if (Size != 0)
                {
                    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) MetaBuf, (CPuint) Size));
                }
            }
            else
            {
                NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) MetaBuf, (CPuint) Size));
            }
            BytesRead += Size;
            MetaBuf[Size] = '\0';
            *MetaSize = Size + 1;
            *MetaEncoding = NvMMMetaDataEncode_Utf8;
        }
        else if (ParentAtom == MP4_FOURCC('t', 'r', 'k', 'n'))
        {
            // trkn is eight bytes long: two zeros, 2 bytes of track number (big-endian, always), 2 bytes of total tracks, and two more zeros
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) 8));
            BytesRead += 8;
            pNvMp4Parser->MediaMetadata.TrackNumber = (ReadBuffer[2] << 8) | ReadBuffer[3];
            pNvMp4Parser->MediaMetadata.TotalTracks = (ReadBuffer[4] << 8) | ReadBuffer[5];
        }
        else if (ParentAtom == MP4_FOURCC('c', 'o', 'v', 'r'))
        {
            NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
            pNvMp4Parser->MediaMetadata.CoverArtOffset = FilePos;
            pNvMp4Parser->MediaMetadata.CoverArtSize = (NvU32) AtomSize - 8; // excluding Version etc info
        }
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserILST (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 BytesRead = 0;

    if (AtomType == MP4_FOURCC(0xa9, 'n', 'a', 'm') || 
        AtomType == MP4_FOURCC(0xa9, 'A', 'R', 'T') ||
        AtomType == MP4_FOURCC('a', 'A', 'R', 'T') ||
        AtomType == MP4_FOURCC(0xa9, 'a', 'l', 'b') ||
        AtomType == MP4_FOURCC(0xa9, 'g', 'e', 'n') ||
        AtomType == MP4_FOURCC(0xa9, 'd', 'a', 'y') || 
        AtomType == MP4_FOURCC(0xa9, 'w', 'r', 't') ||
        AtomType == MP4_FOURCC('t', 'r', 'k', 'n') || 
        AtomType == MP4_FOURCC('c', 'o', 'v', 'r'))
    {
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, &AtomType, Mp4ParserDATA));
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvS8*
Mp4ParserID32GetMetadataName (
    NvMp4Id3MetadataType MetadataType,
    Mp4Id3V2Version Id3V2Major)
{

    NvBool IsV2_2 = (ID3_Ver2_2 == Id3V2Major) ? NV_TRUE : NV_FALSE;
    switch (MetadataType)
    {
        case NvMp4Id3MetadataType_ARTIST:
            return (NvS8*)((IsV2_2) ? "TP1" : "TPE1");
        case NvMp4Id3MetadataType_ALBUM:
            return (NvS8*)((IsV2_2) ? "TAL" : "TALB");
        case NvMp4Id3MetadataType_GENRE:
            return (NvS8*)((IsV2_2) ? "TCO" : "TCON");
        case NvMp4Id3MetadataType_TITLE:
            return (NvS8*)((IsV2_2) ? "TT2" : "TIT2");
        case NvMp4Id3MetadataType_TRACKNUM:
            return (NvS8*)((IsV2_2) ? "TRK" : "TRCK");
        case NvMp4Id3MetadataType_COMPOSER:
            return (NvS8*)((IsV2_2) ? "TCM" : "TCOM");
        case NvMp4Id3MetadataType_YEAR:
            return (NvS8*)((IsV2_2) ? "TYE" : "TYER");
        case NvMp4Id3MetadataType_ALBUMARTIST:
            return (NvS8*)((IsV2_2) ? "TP2" : "TPE2");
        case NvMp4Id3MetadataType_COVERART:
            return (NvS8*)((IsV2_2) ? "PIC" : "APIC");
        case NvMp4Id3MetadataType_COPYRIGHT:
            return (NvS8*)((IsV2_2) ? "TCR" : "TCOP");
        default:
            return NULL;
    }
}

static NvBool
Mp4ParserID32GetTextSize (
    NvU8 *pData,
    NvU32 DataSize,
    NvMMMetaDataCharSet EncodeType,
    NvU32 *TextSize)
{
    NvU32 RunningZeros = 0;
    NvU32 i;

    if ( (!pData) || (!TextSize) )
    {
        return NV_FALSE;
    }

    if ( (NvMMMetaDataEncode_Utf8 != EncodeType) &&
            (NvMMMetaDataEncode_Utf16 != EncodeType) )
    {
        return NV_FALSE;
    }

    for (i = 0; i < DataSize; i++)
    {
        if (0x00 == pData[i])
        {
            RunningZeros++;
        }
        else
        {
            /* clear running zeros */
            RunningZeros = 0;
        }
        if ( ((NvMMMetaDataEncode_Utf8 == EncodeType) && (1 == RunningZeros)) ||
                ((NvMMMetaDataEncode_Utf8 == EncodeType) && (2 == RunningZeros)))
        {
            /* total size of the text string (including null termination) */
            *TextSize = (i + 1);
            return NV_TRUE;
        }
    }

    /* termination not found */
    return NV_FALSE;
}

static NvError
Mp4ParserID32FrameGetTrackNumber (
    NvMp4Parser *pNvMp4Parser,
    NvMp4Id3MetadataType MetadataType,
    Mp4Id3V2Version Id3V2Major,
    NvU8 *pData,
    NvU32 DataSize,
    NvMMMetaDataCharSet EncodeType)
{
    NvError Status = NvSuccess;
    NvU32 i;
    NvBool DividerFound = NV_FALSE;
    NvU32 divider = 0;

    NVMM_CHK_ARG(pNvMp4Parser);
    NVMM_CHK_ARG(NvMp4Id3MetadataType_TRACKNUM == MetadataType);
    pNvMp4Parser->MediaMetadata.TotalTracks = 0;
    pNvMp4Parser->MediaMetadata.TrackNumber = 0;

    /*
     * The 'Track number/Position in set' frame is a numeric string containing
     * the order number of the audio-file on its original recording. This may be
     *  extended with a "/" character and a numeric string containing the total
     *  numer of tracks/elements on the original recording. E.g. "4/9".
     */
    if (NvMMMetaDataEncode_Utf8 != EncodeType)
    {
        /* numeric string can't be unicode, skip this frame */
        return NvSuccess;
    }

    /* find divider '/' */
    for (i = 0; i < DataSize; i++)
    {
        if ('/' == pData[i])
        {
            divider = i;
            DividerFound = NV_TRUE;
            break;
        }
    }

    if (!DividerFound)
    {
        /*
         * if didn't find a divider '/', there is no total track information
         * the numeric string is track number, set the last byte as divider
         */
        divider = DataSize - 1;
    }

    /* parse track number */
    for (i = 0; i < divider; i++)
    {
        if ((pData[i] >= '0') && (pData[i] <= '9'))
        {
            pNvMp4Parser->MediaMetadata.TrackNumber += (pData[i]-'0');
            if (i + 1 < divider)
            {
                /* has one or more digit */
                pNvMp4Parser->MediaMetadata.TrackNumber =
                        pNvMp4Parser->MediaMetadata.TrackNumber * 10;
            }
        }
        else
        {
            /* non-numeric, invalid data, clear track number */
            pNvMp4Parser->MediaMetadata.TotalTracks = 0;
            pNvMp4Parser->MediaMetadata.TrackNumber = 0;
            return NvSuccess;
        }
    }

    /* parse total track number */
    if (DividerFound)
    {
        /* starts from the byte next to '/' */
        for (i = divider + 1; i < DataSize; i++)
        {
            if ((pData[i] >= '0') && (pData[i] <= '9'))
            {
                pNvMp4Parser->MediaMetadata.TotalTracks += (pData[i]-'0');
                if (i + 1 < DataSize)
                {
                    /* has one or more digit */
                    pNvMp4Parser->MediaMetadata.TotalTracks =
                            pNvMp4Parser->MediaMetadata.TotalTracks * 10;
                }
            }
            else
            {
                /* non-numeric, invalid data, clear track number */
                pNvMp4Parser->MediaMetadata.TotalTracks = 0;
                pNvMp4Parser->MediaMetadata.TrackNumber = 0;
                return NvSuccess;
            }
        }
    }
cleanup:
    return Status;
}

static NvError
Mp4ParserID32FrameGetCoverArt (
    NvMp4Parser *pNvMp4Parser,
    NvMp4Id3MetadataType MetadataType,
    Mp4Id3V2Version Id3V2Major,
    NvU8 *pData,
    NvU32 DataSize,
    NvMMMetaDataCharSet EncodeType)
{
    NvError Status = NvSuccess;
    NvU8 *pDataTemp = pData;
    NvU64 Pos = 0;
    NvU32 MimeSize = 0;
    NvU32 DescSize = 0;
    NvU32 BytesLeft = DataSize;

    NVMM_CHK_ARG(pNvMp4Parser);
    NVMM_CHK_ARG(NvMp4Id3MetadataType_COVERART == MetadataType);

    /*
     * Whole frame has been read by caller. Pos points to the first byte of next
     * frame
     */
    NVMM_CHK_ERR(pNvMp4Parser->pPipe->GetPosition64(pNvMp4Parser->hContentPipe,
            (CPuint64 *)&Pos));

    if (ID3_Ver2_2 == Id3V2Major)
    {
        /*
         * v2.2
         * Attached picture   "PIC"
         * Frame size         $xx xx xx
         * Text encoding      $xx
         * Image format       $xx xx xx
         * Picture type       $xx
         * Description        <textstring> $00 (00)
         * Picture data       <binary data>
         */

        /* skip 4 bytes (3 for image format and 1 for picture type) */
        if (BytesLeft < 4)
        {
            return NvSuccess;
        }
        pDataTemp = (pData + 4);
        BytesLeft = (DataSize - 4);

        /* find Description text size */
        if (!Mp4ParserID32GetTextSize(pDataTemp, BytesLeft, EncodeType, &DescSize))
        {
            /* can't determine Description text size, skip this frame */
            return NvSuccess;
        }
        pDataTemp += DescSize;
        BytesLeft -= DescSize;

        pNvMp4Parser->MediaMetadata.CoverArtOffset = (Pos - BytesLeft);
        pNvMp4Parser->MediaMetadata.CoverArtSize = BytesLeft;
    }
    else /* v2.3, v2.4 */
    {
        /*
         * v2.3 and v2.4
         * <Header for 'Attached picture', ID: "APIC">
         * Text encoding   $xx
         * MIME type       <text string> $00
         * Picture type    $xx
         * Description     <text string according to encoding> $00 (00)
         * Picture data    <binary data>
         *
         */
        pDataTemp = pData;
        BytesLeft = DataSize;
        /* find MIME text size */
        if (!Mp4ParserID32GetTextSize(pDataTemp, BytesLeft, EncodeType, &MimeSize))
        {
            /* can't determine MIME text size, skip this frame */
            return NvSuccess;
        }
        pDataTemp += MimeSize;
        BytesLeft -= MimeSize;

        /* skip one byte for picture type */
        if (BytesLeft < 1)
        {
            return NvSuccess;
        }
        pDataTemp += 1;
        BytesLeft -= 1;

        /* find Description text size */
        if (!Mp4ParserID32GetTextSize(pDataTemp, BytesLeft, EncodeType, &DescSize))
        {
            /* can't determine Description text size, skip this frame */
            return NvSuccess;
        }
        pDataTemp += DescSize;
        BytesLeft -= DescSize;

        pNvMp4Parser->MediaMetadata.CoverArtOffset = (Pos - BytesLeft);
        pNvMp4Parser->MediaMetadata.CoverArtSize = BytesLeft;
    }

cleanup:
    return Status;
}

static NvError
Mp4ParserID32FrameGetText (
    NvMp4Parser *pNvMp4Parser,
    NvMp4Id3MetadataType MetadataType,
    NvU8 Id3V2Major,
    NvU8 *pData,
    NvU32 DataSize,
    NvMMMetaDataCharSet EncodeType)
{
    NvError Status = NvSuccess;
    NvS8 *pTextString = NULL;
    NvU32 *pTextSize = NULL;
    NvMMMetaDataCharSet *pTextEncoding = NULL;
    NvU32 CopySize = 0;

    NVMM_CHK_ARG(pNvMp4Parser);
    NVMM_CHK_ARG((EncodeType == NvMMMetaDataEncode_Utf8) ||
            (EncodeType == NvMMMetaDataEncode_Utf16));
    NVMM_CHK_ARG(pData);

    switch (MetadataType)
    {
        /* simple text encoding frames */
        case NvMp4Id3MetadataType_ARTIST:
            pTextString = &(pNvMp4Parser->MediaMetadata.Artist[0]);
            pTextSize = &pNvMp4Parser->MediaMetadata.ArtistSize;
            pTextEncoding = &pNvMp4Parser->MediaMetadata.ArtistEncoding;
            break;
        case NvMp4Id3MetadataType_ALBUM:
            pTextString = &(pNvMp4Parser->MediaMetadata.Album[0]);
            pTextSize = &pNvMp4Parser->MediaMetadata.AlbumSize;
            pTextEncoding = &pNvMp4Parser->MediaMetadata.AlbumEncoding;
            break;
        case NvMp4Id3MetadataType_GENRE:
            pTextString = &(pNvMp4Parser->MediaMetadata.Genre[0]);
            pTextSize = &pNvMp4Parser->MediaMetadata.GenreSize;
            pTextEncoding = &pNvMp4Parser->MediaMetadata.GenreEncoding;
            break;
        case NvMp4Id3MetadataType_TITLE:
            pTextString = &(pNvMp4Parser->MediaMetadata.Title[0]);
            pTextSize = &pNvMp4Parser->MediaMetadata.TitleSize;
            pTextEncoding = &pNvMp4Parser->MediaMetadata.TitleEncoding;
            break;
        case NvMp4Id3MetadataType_YEAR:
            pTextString = &(pNvMp4Parser->MediaMetadata.Year[0]);
            pTextSize = &pNvMp4Parser->MediaMetadata.YearSize;
            pTextEncoding = &pNvMp4Parser->MediaMetadata.YearEncoding;
            break;
        case NvMp4Id3MetadataType_COMPOSER:
            pTextString = &(pNvMp4Parser->MediaMetadata.Composer[0]);
            pTextSize = &pNvMp4Parser->MediaMetadata.ComposerSize;
            pTextEncoding = &pNvMp4Parser->MediaMetadata.ComposerEncoding;
            break;
        case NvMp4Id3MetadataType_ALBUMARTIST:
            pTextString = &(pNvMp4Parser->MediaMetadata.AlbumArtist[0]);
            pTextSize = &pNvMp4Parser->MediaMetadata.AlbumArtistSize;
            pTextEncoding = &pNvMp4Parser->MediaMetadata.AlbumArtistEncoding;
            break;
        case NvMp4Id3MetadataType_COPYRIGHT:
            pTextString = &(pNvMp4Parser->MediaMetadata.Copyright[0]);
            pTextSize = &pNvMp4Parser->MediaMetadata.CopyrightSize;
            pTextEncoding = &pNvMp4Parser->MediaMetadata.CopyrightEncoding;
            break;
        default:
            return NvSuccess;
    }

    if ( (NvMMMetaDataEncode_Utf16 == EncodeType) &&
            !((0xFF == pData[0]) && (0xFE == pData[1])) )
    {
        /*
         * unicode encoding but not little-endian, skip it because
         * we support only little-endian type unicode.
         */
        return NvSuccess;
    }

    CopySize = (DataSize < MP4_MAX_METADATA) ? DataSize : MP4_MAX_METADATA;
    NvOsMemcpy(pTextString, pData, CopySize);
    /* make sure string is null-terminated */
    pTextString[CopySize - 1] = 0;
    if (NvMMMetaDataEncode_Utf16 == EncodeType)
    {
        pTextString[CopySize - 2] = 0;
    }
    *pTextSize = CopySize;
    *pTextEncoding = EncodeType;

cleanup:
    return Status;
}

static NvError
Mp4ParserID32ParseFrameData(
    NvMp4Parser *pNvMp4Parser,
    Mp4Id3V2Version Id3V2Major,
    NvMp4Id3MetadataType MetadataType,
    NvU64  FrameSize)
{
    NvError Status = NvSuccess;
    NvMMMetaDataCharSet EncodeType;
    NvU8 *pFrameData = NULL;
    NvU8 Encode;
    NvU32 FrameBytesLeft = FrameSize;

    NVMM_CHK_ARG(pNvMp4Parser);

    if (FrameSize < sizeof(NvU8))
    {
        /* frame should be at least one byte long */
        return NvSuccess;
    }
    NVMM_CHK_ERR(pNvMp4Parser->pPipe->cpipe.Read(pNvMp4Parser->hContentPipe,
            (CPbyte *)&Encode, sizeof(NvU8)));
    FrameBytesLeft -= sizeof(NvU8);

    switch (Encode)
    {
        case 0x00:
            /* non-unicode */
            EncodeType = NvMMMetaDataEncode_Utf8;
            break;

        case 0x01:
            /* unicode 16 */
            EncodeType = NvMMMetaDataEncode_Utf16;
            break;

        default:
            /* non-documented encoding type, skip this frame */
            NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(
                    pNvMp4Parser->hContentPipe, FrameBytesLeft, CP_OriginCur));
            return NvSuccess;
    }

    if (0 == FrameBytesLeft)
    {
        /* an empty frame */
        return NvSuccess;
    }

    /* parse frame data */
    pFrameData = (NvU8 *) NvOsAlloc(FrameBytesLeft);
    NVMM_CHK_MEM(pFrameData);
    NVMM_CHK_ERR(pNvMp4Parser->pPipe->cpipe.Read(pNvMp4Parser->hContentPipe,
            (CPbyte *)pFrameData, FrameBytesLeft));

    if (NvMp4Id3MetadataType_COVERART == MetadataType)
    {
        NVMM_CHK_ERR(Mp4ParserID32FrameGetCoverArt(pNvMp4Parser,
                MetadataType,
                Id3V2Major,
                pFrameData,
                FrameBytesLeft,
                EncodeType));
    }
    else if (NvMp4Id3MetadataType_TRACKNUM == MetadataType)
    {
        NVMM_CHK_ERR(Mp4ParserID32FrameGetTrackNumber(pNvMp4Parser,
                MetadataType,
                Id3V2Major,
                pFrameData,
                FrameBytesLeft,
                EncodeType));
    }
    else
    {
        NVMM_CHK_ERR(Mp4ParserID32FrameGetText(pNvMp4Parser,
                MetadataType,
                Id3V2Major,
                pFrameData,
                FrameBytesLeft,
                EncodeType));
    }

cleanup:
    if (pFrameData)
    {
        NvOsFree(pFrameData);
        pFrameData = 0;
    }
    return Status;
}

/*
 * A test for ID3 frame identifiers. This function tests whether a ID3 V2 frame
 * identifier is supported by Mp4 parser.
 *
 * params:
 * Id3V2Version - Major number of ID3 v2.
 * *pTestFrameID - The frame identifier for test.
 * *pMetadataType - OUT, will carry the metadata type.
 * return:
 * NV_TRUE - success, the identifier needs to be extracted.
 * NV_FALSE - failure, invalid identifier or no need to extract the identifier.
 *
 */
static NvBool
Mp4ParserID32IsSupportedFrame (
    Mp4Id3V2Version Id3V2Version,
    NvS8 *pTestFrameID,
    NvMp4Id3MetadataType *pMetadataType)
{
    NvU8 FrameIDLen = 0;
    NvU32 i;
    NvS8 *pFrameID = NULL;

    if (NULL == pTestFrameID)
    {
        return NV_FALSE;
    }

    if (ID3_Ver2_2 == Id3V2Version)
    {
        FrameIDLen = 3;
    }
    else if ((ID3_Ver2_3 == Id3V2Version) || (ID3_Ver2_4 == Id3V2Version))
    {
        FrameIDLen = 4;
    }
    else
    {
        /* unknown major number */
        return NV_FALSE;
    }

    for (i = 0; i < NvMp4Id3MetadataType_MAXMETADATA; i++)
    {
        pFrameID = Mp4ParserID32GetMetadataName((NvMp4Id3MetadataType) i, Id3V2Version);
        if (!NvOsStrncmp((char *)pTestFrameID, (char *)pFrameID, FrameIDLen))
        {
            /* supported metadata */
            *pMetadataType = (NvMp4Id3MetadataType) i;
            return NV_TRUE;;
        }
    }

    /* unsupported */
    return NV_FALSE;
}

/*
 * This function identifies and parses a ID3 V2 frame. When is function is
 * called, file pointer must be parked at the first byte of a ID3 V2 frame.
 * After this function is completed, the file pointer will be parked at:
 *      1. the position of a content pipe access error, return NvError
 *      2. the first byte of next frame, return NvSuccess
 * Mp4ParserID32DecodeFrame exits gracefully (with return code NvSuccess) even
 * if there is integrity check failure in frame. So a corrupted frame won't stop
 * parser. Invalid ID32 frame data will only lead to lack of metadata.
 * Caller must ensure that Id3V2Major is in one of (ID3_Ver2_2, ID3_Ver2_3,
 *  ID3_Ver2_4). A given MaxBytesRead will limit the byte count this function
 * will read from file. Mp4ParserID32DecodeFrame should never reads more bytes
 * than MaxBytesRead indicates. This ensures that ID32 parsing won't read across
 * ID32 atom.
 *
 */
static NvError
Mp4ParserID32DecodeFrame (
    NvMp4Parser *pNvMp4Parser,
    Mp4Id3V2Version Id3V2Major,
    NvU64 MaxBytesRead,
    NvU64 *pBytesRead)

{
    NvError Status = NvSuccess;
    NvMp4Id3V2_34FrameHeader FrameV2_34;
    NvMp4Id3V2_2FrameHeader FrameV2_2;
    NvU32 ReadSize = 0;
    NvU64 FrameSize = 0;
    NvMp4Id3MetadataType MetadataType;
    NvS8 *pTestFrameID = NULL;
    NvU64 BytesLeft = MaxBytesRead;

    NVMM_CHK_ARG(pNvMp4Parser);
    NVMM_CHK_ARG(pBytesRead);
    *pBytesRead = 0;

    if (ID3_Ver2_2 == Id3V2Major)
    {
        ReadSize = sizeof(NvMp4Id3V2_2FrameHeader);
        if (BytesLeft < ReadSize)
        {
            /* don't have enough data to read a ID3 v2.2 frame header */
            NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(
                    pNvMp4Parser->hContentPipe, BytesLeft, CP_OriginCur));
            *pBytesRead += BytesLeft;
            return NvSuccess;
        }

        NVMM_CHK_CP(pNvMp4Parser->pPipe->cpipe.Read(pNvMp4Parser->hContentPipe,
                (CPbyte *)&FrameV2_2, ReadSize));
        *pBytesRead += ReadSize;
        BytesLeft -= ReadSize;

        FrameSize = FrameV2_2.Size[2] | (FrameV2_2.Size[1] << 8) |
                (FrameV2_2.Size[0] << 16);
        pTestFrameID = FrameV2_2.Tag;
    }
    else /* ID3 v2.3 or ID3 v2.4 */
    {
        ReadSize = sizeof(NvMp4Id3V2_34FrameHeader);
        if (BytesLeft < ReadSize)
        {
            /* don't have enough data to read a ID3 v2.3/v2.4 frame header */
            NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(
                    pNvMp4Parser->hContentPipe, BytesLeft, CP_OriginCur));
            *pBytesRead += BytesLeft;
            return NvSuccess;
        }

        NVMM_CHK_CP(pNvMp4Parser->pPipe->cpipe.Read(pNvMp4Parser->hContentPipe,
                (CPbyte *)&FrameV2_34, ReadSize));
        *pBytesRead += ReadSize;
        BytesLeft -= ReadSize;

        if (Id3V2Major == ID3_Ver2_3)
        {
            FrameSize = NV_BE_TO_INT_32 (FrameV2_34.Size);
        }
        else /* v2.4 */
        {
            FrameSize = (FrameV2_34.Size[3] & 0x7F) |
                    ((FrameV2_34.Size[2] & 0x7F) << 7) |
                    ((FrameV2_34.Size[1] & 0x7F) << 14) |
                    ((FrameV2_34.Size[0] & 0x7F) << 21);
        }
        pTestFrameID = FrameV2_34.Tag;
    }

    if (BytesLeft < FrameSize)
    {
        /* don't have enough data to read a ID32 frame */
        NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(
                pNvMp4Parser->hContentPipe, BytesLeft, CP_OriginCur));
        *pBytesRead += BytesLeft;
        return NvSuccess;
    }

    if (!Mp4ParserID32IsSupportedFrame(Id3V2Major, pTestFrameID, &MetadataType))
    {
        /* unsupported or invalid frame identifier */
        if (Id3V2Major == ID3_Ver2_2)
        {
            NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                    "Skipping ID3 v2.2 frame %c%c%c (=0x%02X%02X%02X)\n",
                    pTestFrameID[0], pTestFrameID[1], pTestFrameID[2],
                    pTestFrameID[0], pTestFrameID[1], pTestFrameID[2]));
        }
        else /* v2.3 or v2.4 */
        {
            NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                    "Skipping ID3 v2.3+ frame %c%c%c%c (=0x%02X%02X%02X%02X)\n",
                    pTestFrameID[0], pTestFrameID[1], pTestFrameID[2],
                    pTestFrameID[3], pTestFrameID[0], pTestFrameID[1],
                    pTestFrameID[2], pTestFrameID[3]));
        }
        /* unsupported frame, just skip it */
        NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(
                pNvMp4Parser->hContentPipe, FrameSize, CP_OriginCur));
        *pBytesRead += FrameSize;
        Status = NvSuccess;
    }
    else
    {
        /* supported frame, starts to parse frame data */
        NVMM_CHK_ERR(Mp4ParserID32ParseFrameData(pNvMp4Parser,
                Id3V2Major,
                MetadataType,
                FrameSize));
        *pBytesRead += FrameSize;
    }

cleanup:
    return Status;
}

/*
 * This function identifies and parses "ID32" box (atom) according to ID3 v2 tag
 * format. When is function is called, file pointer must be parked at the byte
 * right after atom type field. After this function is completed, the file
 * pointer will be parked at:
 *      1. the position of a content pipe access error, return NvError
 *      2. the first byte of next box (atom), return NvSuccess
 * Mp4ParserID32 exits gracefully (with return code NvSuccess) even if there is
 * integrity check failure in ID32 data. So a corrupted ID32 box won't stop
 * parser. Invalid ID32 data will only lead to lack of metadata.
 *
 */
static NvError
Mp4ParserID32 (
    NvMp4Parser *pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64 *pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvMp4Id3V2Header Id3V2Hdr;
    NvU32 ID32Size;
    NvU32 ExtHdrSize;
    NvU64 FrameBytesRead;
    NvU64 AtomBytesLeft = AtomSize;

    NVMM_CHK_ARG(pNvMp4Parser);
    NVMM_CHK_ARG(pBytesRead);

    /*
     * Box Type: 'ID32'
     * Container: Meta box ('meta')
     * Mandatory: No
     * Quantity: Zero or more
     * The ID3v2 box contains a complete ID3 version 2.x.x data. It should be
     * parsed according to http://www.id3.org/ specifications for v.2.x.x tags.
     * There may be multiple ID3v2 boxes using different language codes.
     *
     * Syntax
     * aligned(8) class ID3v2Box extends FullBox('ID32', version=0, 0) {
     * const bit(1) pad = 0;
     * unsigned int(5)[3] language; // ISO-639-2/T language code
     * unsigned int(8) ID3v2data [];
     * }
     *
     */

    /*
     * ID32 Box extends a FullBox Class, there are 4 bytes of zero and 2 bytes
     * language code (we don't have to extract), skip 6 (= 4+2) bytes in total.
     */
    if (AtomBytesLeft < 6)
    {
        /* don't have enough bytes left*/
        goto complete_atom;
    }
    NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(pNvMp4Parser->hContentPipe,
            6, CP_OriginCur));
    AtomBytesLeft -= 6;

    if (AtomBytesLeft < sizeof(NvMp4Id3V2Header))
    {
        /* don't have enough bytes left for reading a ID3 V2 header */
        goto complete_atom;
    }
    NVMM_CHK_ERR(pNvMp4Parser->pPipe->cpipe.Read(pNvMp4Parser->hContentPipe,
            (CPbyte *) &Id3V2Hdr, sizeof(NvMp4Id3V2Header)));
    AtomBytesLeft -= sizeof(NvMp4Id3V2Header);

    /* ID3 V2 header format
     * ID3v2/file identifier      "ID3"
     * ID3v2 version              $0v 00
     * ID3v2 flags                %abcd0000
     * ID3v2 size             4 * %0xxxxxxx
     * The ID3v2 tag size is encoded with four bytes where the most significant
     * bit (bit 7) is set to zero in every byte, making a total of 28 bits.
     */
    if (NvOsStrncmp((char*) Id3V2Hdr.Tag, "ID3", 3))
    {
        NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4ParserID32: invalid ID3 V2 header tag=0x%02X%02X%02X\n",
                Id3V2Hdr.Tag[0], Id3V2Hdr.Tag[1], Id3V2Hdr.Tag[2]));
        /* invalid ID32 identifier */
        goto complete_atom;
    }

    /* check for supported ID3 V2 major, only supports 2.2, 2.3 or 2.4 */
    if (!( (ID3_Ver2_2 == Id3V2Hdr.VersionMajor) ||
            (ID3_Ver2_3 == Id3V2Hdr.VersionMajor) ||
            (ID3_Ver2_4 == Id3V2Hdr.VersionMajor) ))
    {
        NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4ParserID32: invalid ID3 V2 major version=%u\n",
                Id3V2Hdr.VersionMajor));
        /* unsupported ID3 version */
        goto complete_atom;
    }

    ID32Size = (Id3V2Hdr.Size[3] & 0x7F) |
            ((Id3V2Hdr.Size[2] & 0x7F) << 7) |
            ((Id3V2Hdr.Size[1] & 0x7F) << 14) |
            ((Id3V2Hdr.Size[0] & 0x7F) << 21);

    if (ID32Size > AtomBytesLeft)
    {
        /* ID32 occupies more than AtomSize, invalid ID32 box (atom) */
        NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4ParserID32: invalid ID32 size=%u\n", ID32Size));
        goto complete_atom;
    }

    /* for v2.3 and v2.4, if flags bit#6 is set, there is an extended header */
    if (((ID3_Ver2_3 == Id3V2Hdr.VersionMajor) || (ID3_Ver2_4 == Id3V2Hdr.VersionMajor))
        && (Id3V2Hdr.Flags & (1 << 6)))
    {
        /*
         * Extended header size   $xx xx xx xx
         * Extended Flags         $xx xx
         * Size of padding        $xx xx xx xx
         */
        /* don't have to extract, just skip whole extension header */
        if (AtomBytesLeft < sizeof(NvU32))
        {
            /* invalid ID32 box (atom) */
            goto complete_atom;
        }
        NVMM_CHK_ERR(pNvMp4Parser->pPipe->cpipe.Read(pNvMp4Parser->hContentPipe,
                (CPbyte *)&ExtHdrSize, sizeof(NvU32)));
        AtomBytesLeft -= sizeof(NvU32);

        if (AtomBytesLeft < ExtHdrSize)
        {
            /* invalid ID32 box (atom) */
            goto complete_atom;
        }
        NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(pNvMp4Parser->hContentPipe,
                ExtHdrSize, CP_OriginCur));
        AtomBytesLeft -= ExtHdrSize;
    }

    /* ID32 header integrity check passed, starts to decode ID32 frames */
    while (AtomBytesLeft)
    {
        NVMM_CHK_ERR(Mp4ParserID32DecodeFrame(pNvMp4Parser,
                Id3V2Hdr.VersionMajor, AtomBytesLeft, &FrameBytesRead));

        if (AtomBytesLeft >= FrameBytesRead)
        {
            AtomBytesLeft -= FrameBytesRead;
        }
        else
        {
            /*
             * Mp4ParserID32DecodeFrame() reads more bytes than AtomBytesLeft,
             * there is a bug in Mp4ParserID32DecodeFrame().
             */
            return NvError_ParserFailure;
        }
    }

complete_atom:
    if (0 != AtomBytesLeft)
    {
        NVMM_CHK_ERR(pNvMp4Parser->pPipe->SetPosition64(pNvMp4Parser->hContentPipe,
                AtomBytesLeft, CP_OriginCur));
    }
    *pBytesRead = AtomSize - AtomBytesLeft;
cleanup:
    return Status;
}

static NvError
Mp4ParserMETA (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 BytesRead = 0;

    if (AtomType == MP4_FOURCC('i', 'l', 's', 't'))
    {
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, NULL, Mp4ParserILST));
    }
    else if (AtomType == MP4_FOURCC('I', 'D', '3', '2'))
    {
        NVMM_CHK_ERR(Mp4ParserID32(pNvMp4Parser, AtomType, AtomSize, &BytesRead, NULL));
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}


static NvError
Mp4ParserUDTA (
    NvMp4Parser *pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU8 ReadBuffer[4];
    NvU64 BytesRead = 0, metaBytesRead = 0;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;

    if (AtomType == MP4_FOURCC('m', 'e', 't', 'a'))
    {
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) 4));
        BytesRead += 4;
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &metaBytesRead, NULL, Mp4ParserMETA));
        BytesRead += metaBytesRead;
    } else if( (AtomType == MP4_FOURCC('t', 'i', 't', 'l')) ||
            (AtomType == MP4_FOURCC('c', 'p', 'r', 't')) ||
            (AtomType == MP4_FOURCC('p', 'e', 'r', 'f')) ||
            (AtomType == MP4_FOURCC('g', 'n', 'r', 'e')) ||
            (AtomType == MP4_FOURCC('a', 'u', 't', 'h')) ||
            (AtomType == MP4_FOURCC('y', 'r', 'r', 'c')) ||
            (AtomType == MP4_FOURCC('a', 'l', 'b', 'm')) )
    {
        NVMM_CHK_ERR(Mp4ParserUDTA3GP(pNvMp4Parser, AtomType, AtomSize,
                &metaBytesRead, NULL));
        BytesRead += metaBytesRead;
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserRMDA (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64 *pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 BytesRead = 0;
    NvU32 RefType = 0, URLLen = 0;
    NvU8 ReadBuffer[8];
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;

    if (AtomType == MP4_FOURCC('r', 'd', 'r', 'f'))
    {
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) 8));
        BytesRead += 8;
        RefType = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        if ((RefType == MP4_FOURCC('u', 'r', 'l', 'r')) || (RefType == MP4_FOURCC('u', 'r', 'l', ' ')))
        {
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) 4));
            BytesRead += 4;
            URLLen = NV_BE_TO_INT_32 (&ReadBuffer[0]);
            pNvMp4Parser->URIList[pNvMp4Parser->URICount] = NvOsAlloc (URLLen + 1);
            NVMM_CHK_MEM (pNvMp4Parser->URIList[pNvMp4Parser->URICount]);
            NvOsMemset (pNvMp4Parser->URIList[pNvMp4Parser->URICount], 0, URLLen + 1);
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) pNvMp4Parser->URIList[pNvMp4Parser->URICount], URLLen));
            BytesRead += URLLen;
            pNvMp4Parser->URICount++;
        }
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserRMRA (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 BytesRead = 0;

    if (AtomType == MP4_FOURCC('r', 'm', 'd', 'a'))
    {
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, NULL, Mp4ParserRMDA));
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserDINF(
    NvMp4Parser *pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 BytesRead = 0;    
    NvU32 NumEntries=0;
    NvU8 ReadBuffer[32];    
    NvU32 ReadCount;
    NvU32 Type;
    NvU32 SubType;    
    NvU32 URLLen = 0;    
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;
    
    if (AtomType == MP4_FOURCC('d', 'r', 'e', 'f'))
    {
        /**
          * qtff.pdf page 230
          */
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint)ReadCount));     
        //versionFlags = NV_BE_TO_LE_32 (&ReadBuffer[0]);
        NumEntries = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        BytesRead += ReadCount;
        
        while(NumEntries > 0)
        {
            ReadCount = 8;
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint)ReadCount));     
            BytesRead += ReadCount;        
            
            Type = NV_BE_TO_INT_32(&ReadBuffer[4]); 

            switch (Type)
            {
                case MP4_FOURCC('h', 'n', 'd', 'l'):
                    /* version 4 bytes, flags 4 bytes*/
                    ReadCount = 8;
                    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint)ReadCount));     
                    BytesRead += ReadCount;      

                    /*  subtype size -4 bytes, subtype -4 bytes*/
                    ReadCount = 9;                    
                    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint)ReadCount));     
                    BytesRead += ReadCount;      

                    SubType = NV_BE_TO_INT_32(&ReadBuffer[5]); 

                    if( SubType == MP4_FOURCC('d', 'a', 't', 'a') && pNvMp4Parser->EmbeddedURL == NULL)
                    {
                        URLLen = NV_BE_TO_INT_32 (&ReadBuffer[1]) - 8;
                        if(pNvMp4Parser->EmbeddedURL)
                        NvOsFree(pNvMp4Parser->EmbeddedURL);
                        pNvMp4Parser->EmbeddedURL = NvOsAlloc (URLLen + 1);
                        NVMM_CHK_MEM (pNvMp4Parser->EmbeddedURL);
                        NvOsMemset (pNvMp4Parser->EmbeddedURL, 0, URLLen + 1);
                        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) pNvMp4Parser->EmbeddedURL, URLLen));
                        BytesRead += URLLen;
                    }
                   
                    break;
                    
                default:
                    break;
            }
            NumEntries--;
        }
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}
static NvError
Mp4ParserSTBL (
    NvMp4Parser *pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU8 ReadBuffer[100];
    NvU8 HdrBytesRead = 0;
    NvU8 SampleEntryFound = 0;
    NvU64 TempAtomSize;
    NvU32 Version;
    NvU32 SampleBytesRead = 0;
    NvU64 FilePos, BytesRead = 0;
    NvU32 SampleCount;
    NvU32 SampleIndex;
    NvU32 ReadCount;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;
    NvMp4TrackInformation *pTrackInfo = (NvMp4TrackInformation *) pData;
    NvU32 SyncCount = 0;

    if (AtomType == MP4_FOURCC('s', 't', 's', 'd'))
    {
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) ReadCount));
        Version = (NvU32) ReadBuffer[0];
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "STSD AtomType -- Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;;
        }
        BytesRead += 8;
        SampleCount = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        for (SampleIndex = 0; SampleIndex < SampleCount; SampleIndex++)
        {
            switch (pTrackInfo->HandlerType)
            {
                case MP4_FOURCC('s', 'o', 'u', 'n'):
                    NVMM_CHK_ERR (Mp4ParserATOMH (pNvMp4Parser, &pTrackInfo->AtomType, &TempAtomSize, &HdrBytesRead));
                    pTrackInfo->AtomSize = (NvU32) TempAtomSize;
                    SampleBytesRead = Mp4ParserAudioSampleEntry (pNvMp4Parser, pTrackInfo);
                    if (!SampleBytesRead)
                    {
                        Status = NvError_ParserFailure;
                        goto cleanup;
                    }
                    BytesRead +=  SampleBytesRead;
                    SampleEntryFound += 1;
                    break;

                case MP4_FOURCC('v', 'i', 'd', 'e'):
                    NVMM_CHK_ERR (Mp4ParserATOMH (pNvMp4Parser, &pTrackInfo->AtomType, &TempAtomSize, &HdrBytesRead));
                    pTrackInfo->AtomSize = (NvU32) TempAtomSize;
                    SampleBytesRead = 0;
                    if ( (pTrackInfo->AtomType == MP4_FOURCC('m', 'p', '4', 'v')) || (pTrackInfo->AtomType == MP4_FOURCC('m', 'p', '4', '2')))
                    {
                        NVMM_CHK_ERR (Mp4ParserVideoSampleEntry (pNvMp4Parser, pTrackInfo, &SampleBytesRead));
                    }
                    else if ( (pTrackInfo->AtomType == MP4_FOURCC('m', 'j', 'p', 'a')) || 
                                    (pTrackInfo->AtomType == MP4_FOURCC('m', 'j', 'p', 'b')) || 
                                    (pTrackInfo->AtomType == MP4_FOURCC('j', 'p', 'e', 'g')) || 
                                    (pTrackInfo->AtomType == MP4_FOURCC('a', 'v', 'c', '1')) || 
                                    (pTrackInfo->AtomType == MP4_FOURCC('s', '2', '6', '3')) || 
                                    (pTrackInfo->AtomType == MP4_FOURCC('H', '2', '6', '3')) || 
                                    (pTrackInfo->AtomType == MP4_FOURCC('h', '2', '6', '3')))
                    {

                        NVMM_CHK_ERR (Mp4ParserHeaderEntry (pNvMp4Parser, pTrackInfo, pTrackInfo->AtomSize,  &SampleBytesRead));

                    }
                    BytesRead +=  SampleBytesRead + 8;
                    SampleEntryFound += 1;
                    break;
                case MP4_FOURCC('s', 't', 'r', 'm'):
                    break;
                default:
                    return NvSuccess;
            }
            if (SampleEntryFound >= SampleCount)
                break;
        }
    }
    else if (AtomType == MP4_FOURCC('s', 't', 't', 's'))
    {
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        pTrackInfo->STTSOffset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        Version = (NvU32) ReadBuffer[0];
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "STTS AtomType-- Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;
        }
        pTrackInfo->STTSEntries = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        BytesRead += ReadCount;
    }
    else if (AtomType == MP4_FOURCC('c', 't', 't', 's'))
    {
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        pTrackInfo->CTTSOffset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        Version = (NvU32) ReadBuffer[0];
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "CTTS AtomType-- Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;
        }
        pTrackInfo->CTTSEntries = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        BytesRead += ReadCount;
    }
    else if (AtomType == MP4_FOURCC('s', 't', 's', 'c'))
    {
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        pTrackInfo->STSCOffset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        pTrackInfo->STSCAtomSize = (NvU32) AtomSize + MP4_DEFAULT_ATOM_HDR_SIZE;
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        Version = (NvU32) ReadBuffer[0];
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "STSC AtomType-- Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;
        }
        pTrackInfo->STSCEntries = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        BytesRead += ReadCount;
    }
    else if (AtomType == MP4_FOURCC('s', 't', 's', 'z'))
    {
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        pTrackInfo->STSZOffset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        /* 8 bytes for the header */
        ReadCount = 12;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        Version = (NvU32) ReadBuffer[0];
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "STSZ AtomType-- Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;
        }
        pTrackInfo->STSZSampleSize = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        pTrackInfo->STSZSampleCount = NV_BE_TO_INT_32 (&ReadBuffer[8]);
        BytesRead += ReadCount;
    }
    else if (AtomType == MP4_FOURCC('s', 't', 'c', 'o'))
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Atom Type: STCO -- Container: stbl\n"));
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        pTrackInfo->STCOOffset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        /* 8 bytes for the header */
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        Version = (NvU32) ReadBuffer[0];
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "STCO AtomType-- Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;
        }
        pTrackInfo->STCOEntries = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        if (pTrackInfo->StreamType == NvMp4StreamType_Stream && pTrackInfo->STCOEntries == 1)
        {
            NvU32 offset =0;
            ReadCount = 4;
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
            offset=NV_BE_TO_INT_32 (&ReadBuffer[0]);
            if(!pNvMp4Parser->EmbeddedURL && offset != 0 && pTrackInfo->STSZSampleSize != 0)
            {
                pNvMp4Parser->EmbeddedURL=NvOsAlloc(pTrackInfo->STSZSampleSize+1);
                NVMM_CHK_MEM (pNvMp4Parser->EmbeddedURL);
                NvOsMemset(pNvMp4Parser->EmbeddedURL,0,pTrackInfo->STSZSampleSize+1);
                NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
                NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) offset, CP_OriginBegin));
                NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) pNvMp4Parser->EmbeddedURL, pTrackInfo->STSZSampleSize));
                NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FilePos, CP_OriginBegin));
            }
            ReadCount=12;
        }
        BytesRead += ReadCount;
    }
    else if (AtomType == MP4_FOURCC('c', 'o', '6', '4'))
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Atom Type: CO64 -- Container: STBL\n"));
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        pTrackInfo->CO64Offset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        pTrackInfo->STCOOffset = pTrackInfo->CO64Offset;
        /* 8 bytes for the header */
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        Version = (NvU32) ReadBuffer[0];
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "CO64 AtomType-- Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;
        }
        pTrackInfo->CO64Entries = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        pTrackInfo->STCOEntries = pTrackInfo->CO64Entries;
        pTrackInfo->IsCo64bitOffsetPresent = NV_TRUE;
        BytesRead += ReadCount;
    }
    else if (AtomType == MP4_FOURCC('s', 't', 's', 's'))
    {
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        pTrackInfo->STSSOffset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        /* 8 bytes for the header */
        ReadCount = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        Version = (NvU32) ReadBuffer[0];
        BytesRead += ReadCount;
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "STSS AtomType-- Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;
        }
        pTrackInfo->STSSEntries = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        if(pTrackInfo->STSSEntries == 0)
        {
            pTrackInfo->IsValidSeekTable = NV_FALSE;
        }
        else
        {
            pTrackInfo->IsValidSeekTable = NV_TRUE;
             /*Reading the first 20 Sync entries or Max STSSEntries if less than 20. This is required to generate max I frame size value for thumb nail attribute request*/
            pTrackInfo->MaxScanSTSSEntries = (pTrackInfo->STSSEntries > NV_MAX_SYNC_SCAN_FOR_THUMB_NAIL) ? NV_MAX_SYNC_SCAN_FOR_THUMB_NAIL : pTrackInfo->STSSEntries;
            /*Reading 20 entries for now. If NV_MAX_SYNC_SCAN_FOR_THUMB_NAIL exceeds 20 needs allocatation as required*/
            NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, pTrackInfo->MaxScanSTSSEntries*(sizeof(NvU32))));
            for (SyncCount= 0; SyncCount< pTrackInfo->MaxScanSTSSEntries; SyncCount++)
            {
                pTrackInfo->IFrameNumbers[SyncCount] =  NV_BE_TO_INT_32 (&ReadBuffer[SyncCount*4]);
            }
            BytesRead += pTrackInfo->MaxScanSTSSEntries*(sizeof(NvU32));
        }
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserMINF (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 BytesRead = 0, FilePos = 0;
    NvMp4TrackInformation *pTrackInfo = (NvMp4TrackInformation *) pData;

    if (AtomType == MP4_FOURCC('s', 't', 'b', 'l'))
    {
     
        if (pTrackInfo->StreamType == NvMp4StreamType_Audio || pTrackInfo->StreamType == NvMp4StreamType_Video
             || pTrackInfo->StreamType == NvMp4StreamType_Stream)
        {
            pTrackInfo->STSCOffset = pTrackInfo->STCOOffset = pTrackInfo->STSZOffset = -1;
            pTrackInfo->STTSOffset = - 1;
            pTrackInfo->CTTSOffset = (NvU64)-1;
            NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, pTrackInfo, Mp4ParserSTBL));
        }
        else /* if the stream type is not determined by this time, store the STBL FileOffset for future use */
        {
            NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
            pTrackInfo->STBLOffset = FilePos;
            pTrackInfo->STBLSize = AtomSize;
        }
    }
    else if (AtomType == MP4_FOURCC('d', 'i', 'n', 'f'))
    {
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, NULL, Mp4ParserDINF));        
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}


/*
 ******************************************************************************
 */

static NvError
Mp4ParserMDIA (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64 *pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU8 ReadBuffer[32];
    NvU64 BytesRead = 0, TmpRead = 0, FilePos = 0;
    NvU32 Version;
    NvU32 LowInt, HighInt, ReadCount;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;
    NvMp4TrackInformation *pTrackInfo = (NvMp4TrackInformation *) pData;

    if (AtomType == MP4_FOURCC('m', 'd', 'h', 'd'))
    {
        ReadCount = 4;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        BytesRead = ReadCount;
        Version = ReadBuffer[0];
        if (Version == 0)
        {
            ReadCount = 16;
        }
        else if (Version == 1)
        {
            ReadCount = 28;
        }
        else
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MDHD_VERSION_ERROR = %d\n", Version));
            return NvError_UnSupportedStream;
        }
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        BytesRead += ReadCount;
        if (Version == 0)
        {
            pTrackInfo->TimeScale = (NvU64) NV_BE_TO_INT_32 (&ReadBuffer[8]);
            pTrackInfo->Duration = (NvU64) NV_BE_TO_INT_32 (&ReadBuffer[12]);
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MDHD_VERSION_0 = %d\n", Version));
        }
        else if (Version == 1)
        {
            pTrackInfo->TimeScale = (NvU64) NV_BE_TO_INT_32 (&ReadBuffer[16]);
            LowInt = NV_BE_TO_INT_32 (&ReadBuffer[20]);
            HighInt = NV_BE_TO_INT_32 (&ReadBuffer[24]);
            pTrackInfo->Duration = ( (NvU64) LowInt << 32) | (NvU64) HighInt;
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MDHD_VERSION_1 = %d\n", Version));
        }
    }
    else if (AtomType == MP4_FOURCC('h', 'd', 'l', 'r'))
    {
        ReadCount = 16;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, ReadCount));
        BytesRead = ReadCount;
        Version = ReadBuffer[0];
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "HDLR Version=%d\n", Version));
        if (Version != 0)
        {
            return NvError_UnSupportedStream;
        }
        pTrackInfo->HandlerType = NV_BE_TO_INT_32 (&ReadBuffer[8]);

        // Handler type gives the nature of the track, NvMp4StreamType_Audio/NvMp4StreamType_Video
        switch (pTrackInfo->HandlerType)
        {

            case MP4_FOURCC('s', 'o', 'u', 'n'):
                pTrackInfo->StreamType = NvMp4StreamType_Audio;
                break;

            case MP4_FOURCC('v', 'i', 'd', 'e'):
                pTrackInfo->StreamType = NvMp4StreamType_Video;
                break;

            case MP4_FOURCC('s', 't', 'r', 'm'):
                pTrackInfo->StreamType = NvMp4StreamType_Stream;
                break;

            default:
                pTrackInfo->StreamType = NvMp4TrackTypes_OTHER;
                break;
        }

        /*
         * This is a special corner case where HDLR AtomType is wrongly placed after MINF AtomType in the file
         */
        if (pTrackInfo->StreamType == NvMp4StreamType_Audio || pTrackInfo->StreamType == NvMp4StreamType_Video
            || pTrackInfo->StreamType == NvMp4StreamType_Stream )
        {
            if (pTrackInfo->STBLOffset != MP4_INVALID_NUMBER)
            {
                NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
                NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) pTrackInfo->STBLOffset, CP_OriginBegin));
                NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, MP4_FOURCC('s', 't', 'b', 'l'), pTrackInfo->STBLSize, &TmpRead, pTrackInfo, Mp4ParserSTBL));
                NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FilePos, CP_OriginBegin));
            }
        }

    }
    else if (AtomType == MP4_FOURCC('m', 'i', 'n', 'f'))
    {
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, pTrackInfo, Mp4ParserMINF));
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserEDTS (
    NvMp4Parser *pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64 *pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU8 ReadBuffer[32];
    NvU32 EntryCount = 0;
    NvU64 FilePos, BytesRead = 0;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;
    NvMp4TrackInformation *pTrackInfo = (NvMp4TrackInformation *) pData;

    if (AtomType == MP4_FOURCC('e', 'l', 's', 't'))
    {
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        pTrackInfo->ELSTOffset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        BytesRead = 8;
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) BytesRead));
        EntryCount = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        pTrackInfo->ELSTEntries = EntryCount;
        if (EntryCount)
            pTrackInfo->IsELSTPresent = NV_TRUE;
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserTRAK (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64 *pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64  BytesRead = 0;
    NvU64 TrackHdrInfo[TRK_INFO_NELEM];
    NvMp4TrackInformation *pTrackInfo = (NvMp4TrackInformation *) pData;

    if (AtomType == MP4_FOURCC('e', 'd', 't', 's'))
    {
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, pTrackInfo, Mp4ParserEDTS));
    }
    else if (AtomType == MP4_FOURCC('t', 'k', 'h', 'd'))
    {
        NvOsMemset (TrackHdrInfo, 0, sizeof (TrackHdrInfo));
        NVMM_CHK_ERR (Mp4ParserTKHD (pNvMp4Parser, AtomSize, &BytesRead, (NvU64 *)TrackHdrInfo)); // Parse the track header Atom
        pTrackInfo->TrackID = (NvU32)TrackHdrInfo[3];
    }
    else if (AtomType == MP4_FOURCC('m', 'd', 'i', 'a'))
    {
        pTrackInfo->STBLOffset = MP4_INVALID_NUMBER;
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, pTrackInfo, Mp4ParserMDIA));
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserELST (
    NvMp4Parser *pNvMp4Parser,
    NvMp4TrackInformation *pTrackInfo)
{
    NvError Status = NvSuccess;
    NvU8 ReadBuffer[32];
    NvU8 HdrBytesRead = 0;
    NvU32 AtomType;
    NvU64 AtomSize;
    NvU32 Version = 0;
    NvS64 pELSTMediaTime;
    NvU32 i, EntryCount, LowInt, HighInt, AtomSizeCount;
    NvU64 FileOffset = 0;
    NvU64 SegmentDuration = 0;
    CP_PIPETYPE *pPipe = &pNvMp4Parser->pPipe->cpipe;

    FileOffset = pTrackInfo->ELSTOffset;
    NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));
    NVMM_CHK_ERR (Mp4ParserATOMH (pNvMp4Parser, &AtomType, &AtomSize, &HdrBytesRead));

    if (AtomType == MP4_FOURCC('e', 'l', 's', 't'))
    {
        AtomSizeCount = (NvU32) (AtomSize - HdrBytesRead); //28 - ver1 and 20-ver0
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, 8));
        Version = (NvU32) ReadBuffer[0];
        EntryCount = pTrackInfo->ELSTEntries;
        if (!EntryCount)
        {
            // as good as no pELST present.
            // No effect so just return ok and let playback continue.
            Status = NvSuccess;
            goto cleanup;
        }
        pTrackInfo->SegmentTable = (NvU64 *) NvOsAlloc (sizeof (NvU64) * EntryCount);
        NVMM_CHK_MEM (pTrackInfo->SegmentTable);
        NvOsMemset (pTrackInfo->SegmentTable, 0, sizeof (NvU64) *EntryCount);

        pTrackInfo->pELSTMediaTime = NvOsAlloc (sizeof (NvU64) * EntryCount);
        NVMM_CHK_MEM (pTrackInfo->pELSTMediaTime);
        NvOsMemset (pTrackInfo->pELSTMediaTime, 0, sizeof (NvU64) *EntryCount);

        for (i = 0; i < EntryCount; i++)
        {
            if (Version == 0)
            {
                if (AtomSizeCount < 20)
                {
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ELST_Err_ver_0\n"));
                }
                NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, 12));
                SegmentDuration = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                pTrackInfo->SegmentTable[i] = (NvU64) SegmentDuration; //SegmentDuration/mvhdData->timeScale;
                pELSTMediaTime = (NvS32) NV_BE_TO_INT_32 (&ReadBuffer[4]);
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "EntryCount = %d - SegmentDuration = %d\n", EntryCount, SegmentDuration));
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "pELSTMediaTime = %d\n", pELSTMediaTime));
                //update ReadBuffer: 4(Segment Duration) + 4(media time table above) + 2(media rate)+ 2(reserved)
                if (pTrackInfo->TimeScale != 0)
                {
                    if (pELSTMediaTime == -1)
                    {
                        pTrackInfo->pELSTMediaTime[i]  = -1;
                    }
                    else
                    {
                        pTrackInfo->pELSTMediaTime[i] = pELSTMediaTime;
                    }
                }
                else
                {
                    pTrackInfo->pELSTMediaTime[i]  = 0;
                }
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ELST_VERSION_0 = %d\n", Version));
            }
            else if (Version == 1)
            {
                if (AtomSizeCount < 28)
                {
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ELST_Err_ver_1\n"));
                }
                NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, 20));
                LowInt = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                HighInt = NV_BE_TO_INT_32 (&ReadBuffer[4]);
                SegmentDuration = ( (NvU64) LowInt << 32) | (NvU64) HighInt;
                pTrackInfo->SegmentTable[i] = (NvU64) SegmentDuration;
                LowInt = (NvU32) NV_BE_TO_INT_32 (&ReadBuffer[8]);
                HighInt = (NvU32) NV_BE_TO_INT_32 (&ReadBuffer[12]);
                pELSTMediaTime = ( (NvS64) LowInt << 32) | (NvS64) HighInt;
                if (pTrackInfo->TimeScale != 0)
                {
                    if (pELSTMediaTime == -1)
                    {
                        pTrackInfo->pELSTMediaTime[i]  = -1;
                    }
                    else
                    {
                        pTrackInfo->pELSTMediaTime[i] = pELSTMediaTime;
                    }
                }
                else
                {
                    pTrackInfo->pELSTMediaTime[i]  = 0;
                }
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ELST_VERSION_1 = %d\n", Version));
            }
            else
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ELST_VERSION_ERROR = %d\n", Version));
                Status = NvError_UnSupportedStream;
                goto cleanup;
            }
        }
    }
    return Status;
cleanup:
    pTrackInfo->IsELSTPresent = NV_FALSE;
    return Status;
}

static NvError
Mp4ParserAVTrack (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 BytesRead = 0;
    NvU32 BitsLeftToDecode = 0;
    NvBool IsAudio, IsVideo;
    NvU32 SyncExtensionType, ExtensionObjType;
    NvU8 *pDecDataPtr;
    MP4ParserStreamBuffer ParserStreamBuffer;
    NvU32 AVCount = pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks;
    NvMp4AacConfigurationData *pAACConfigData = &pNvMp4Parser->AACConfigData;

    IsAudio = IsVideo = NV_FALSE;

    pNvMp4Parser->TrackInfo[AVCount].StreamType = NvMp4TrackTypes_OTHER;
    pNvMp4Parser->TrackInfo[AVCount].TrackOffset = * (NvU64 *) pData;

    //Parse the track and get the content type for NvMp4StreamType_Audio track
    NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize,
                           &BytesRead, &pNvMp4Parser->TrackInfo[AVCount], Mp4ParserTRAK));

    // TODO: Decompose this code into small APIs
    if (pNvMp4Parser->TrackInfo[AVCount].StreamType == NvMp4StreamType_Audio)
    {
        switch (pNvMp4Parser->TrackInfo[AVCount].AtomType)
        {
            case MP4_FOURCC('m', 'p', '4', 'a'):
                if (pNvMp4Parser->TrackInfo[AVCount].ObjectType == MPEG4_QCELP)
                {
                    pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_QCELP;
                    IsAudio = NV_TRUE;
                }
                else if (pNvMp4Parser->TrackInfo[AVCount].ObjectType == MPEG4_EVRC)
                {
                    pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_EVRC;
                    IsAudio = NV_TRUE;
                }
                else
                {
                    //check the MPEG4 Audio object type
                    ParserStreamBuffer.ReadBuffer = pNvMp4Parser->TrackInfo[AVCount].pDecSpecInfo[0];
                    if (ParserStreamBuffer.ReadBuffer == NULL)
                    {
                        pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_UnsupportedAudio;
                        goto cleanup;
                    }
                    ParserStreamBuffer.NBits = 0 ;
                    ParserStreamBuffer.CWord = 0 ;
                    ParserStreamBuffer.ByteCount = 0;
                    /*
                     * The MPEG-4 audio codec value is the 5 bits of the AudioObjectType found in
                     * the AudioSpecificInfo (a DecoderSpecificInfo). See specification ISO/IEC 14496
                     * , subclause 1.6. The AudioSpecificInfo is found in the MPEG-4 Elementary
                     * Stream Descriptor Atom within the siDecompressionParam AtomType of the audio
                     * sample description for the QuickTime audio codec of type 'mp4a'.
                     */
                    pAACConfigData->ObjectType =  Mp4GetStreamBits (&ParserStreamBuffer, 5);
                    pDecDataPtr = pNvMp4Parser->TrackInfo[AVCount].pDecSpecInfo[0];
                    switch (pAACConfigData->ObjectType)
                    {
                        case 1: // AAC main object
                        case 2: // AAC_LC object
                            pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_AAC;
                            IsAudio = NV_TRUE;
                            break;
                        case 5: // SBR object
                        case 29:
                            pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_AACSBR;
                            IsAudio = NV_TRUE;
                            break;
                        case 22: //ER BSAC object
                            pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_BSAC;
                            IsAudio = NV_TRUE;
                            break;
                        default:
                            pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_UnsupportedAudio;
                            break;
                    }
                    pAACConfigData->SamplingFreqIndex = Mp4GetStreamBits (&ParserStreamBuffer, 4);
                    pDecDataPtr++;
                    if (pAACConfigData->SamplingFreqIndex == 0xf)
                    {
                        /* extracting 24 bits */
                        pAACConfigData->SamplingFreq = Mp4GetStreamBits (&ParserStreamBuffer, 24);
                        pDecDataPtr += 3;
                    }
                    else
                    {
                        pAACConfigData->SamplingFreq = FreqIndexTable[pAACConfigData->SamplingFreqIndex ];
                    }
                    pAACConfigData->ChannelConfiguration = Mp4GetStreamBits (&ParserStreamBuffer, 4);
                    pAACConfigData->EpConfig = Mp4GetStreamBits (&ParserStreamBuffer, 2);
                    pAACConfigData->DirectMapping = Mp4GetStreamBits (&ParserStreamBuffer, 1);

                    pAACConfigData->SbrPresentFlag = -1;
                    if (pAACConfigData->ObjectType == 2)
                    {
                        BitsLeftToDecode =
                            (pNvMp4Parser->TrackInfo[AVCount].DecInfoSize[0] * 8)
                            - (ParserStreamBuffer.ByteCount * 8)
                            - (ParserStreamBuffer.NBits);
                        if (BitsLeftToDecode >= 16)
                        {
                            SyncExtensionType = Mp4GetStreamBits (&ParserStreamBuffer, 11);
                            if (SyncExtensionType == 0x2b7)
                            {
                                ExtensionObjType = Mp4GetStreamBits (&ParserStreamBuffer, 5); /* from AudioSpecificConfig */
                                if (ExtensionObjType == 5)
                                {
                                    pAACConfigData->SbrPresentFlag = Mp4GetStreamBits (&ParserStreamBuffer, 1);
                                    if (pAACConfigData->SbrPresentFlag)
                                    {
                                        pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_AACSBR;
                                        pAACConfigData->ExtensionSamplingFreqIndex = Mp4GetStreamBits (&ParserStreamBuffer, 4);
                                        if (pAACConfigData->ExtensionSamplingFreqIndex == 0xf)
                                        {
                                            /* extracting 24 bits */
                                            pAACConfigData->ExtensionSamplingFreq = Mp4GetStreamBits (&ParserStreamBuffer, 24);
                                        }
                                        else
                                        {
                                            pAACConfigData->ExtensionSamplingFreq = FreqIndexTable[pAACConfigData->ExtensionSamplingFreqIndex ];
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else if (pAACConfigData->ObjectType == 5)
                    {
                        // SBR object
                        pAACConfigData->SbrPresentFlag = 1;
                        pAACConfigData->ExtensionSamplingFreqIndex = Mp4GetStreamBits (&ParserStreamBuffer, 4);
                        if (pAACConfigData->ExtensionSamplingFreqIndex == 0xf)
                        {
                            /* extracting 24 bits */
                            pAACConfigData->ExtensionSamplingFreqIndex = Mp4GetStreamBits (&ParserStreamBuffer, 24);
                        }
                        else
                        {
                            pAACConfigData->ExtensionSamplingFreq = FreqIndexTable[pAACConfigData->ExtensionSamplingFreqIndex];
                        }
                    }
                }
                break;
            case MP4_FOURCC('s', 'a', 'm', 'r'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_NAMR;
                IsAudio = NV_TRUE;
                break;
            case MP4_FOURCC('s', 'q', 'c', 'p'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_QCELP;
                IsAudio = NV_TRUE;
                break;
            case MP4_FOURCC('s', 'e', 'v', 'c'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_EVRC;
                IsAudio = NV_TRUE;
                break;
            case MP4_FOURCC('s', 'a', 'w', 'b'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_WAMR;
                IsAudio = NV_TRUE;
                break;
            case MP4_FOURCC('r', 'a', 'w', ' '):
                /*  pNvMp4Parser->allTrackInfo[AVCount].TrackType = WAV;
                  break; */
            case MP4_FOURCC('t', 'w', 'o', 's'):
                /* pNvMp4Parser->allTrackInfo[AVCount].TrackType = WAV;
                 break;*/
            case MP4_FOURCC('u', 'l', 'a', 'w'):
                /* pNvMp4Parser->allTrackInfo[AVCount].TrackType = U_LAW;
                 break; */
            case MP4_FOURCC('a', 'l', 'a', 'w'):
                /*pNvMp4Parser->allTrackInfo[AVCount].TrackType = A_LAW;
                break; */
            default:
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_UnsupportedAudio;
                break;
        }
        pNvMp4Parser->MvHdData.AudioTracks++;
        if (IsAudio)
        {
            pNvMp4Parser->AudioIndex = AVCount;
        }
    }
    else if (pNvMp4Parser->TrackInfo[AVCount].StreamType == NvMp4StreamType_Video)
    {
        switch (pNvMp4Parser->TrackInfo[AVCount].AtomType)
        {
            case MP4_FOURCC('m', 'p', '4', 'v'):
            case MP4_FOURCC('m', 'p', '4', '2'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_MPEG4;
                IsVideo = NV_TRUE;
                break;
            case MP4_FOURCC('a', 'v', 'c', '1'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_AVC;
                IsVideo = NV_TRUE;
                break;
            case MP4_FOURCC('s', '2', '6', '3'):
            case MP4_FOURCC('H', '2', '6', '3'):
            case MP4_FOURCC('h', '2', '6', '3'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_S263;
                IsVideo = NV_TRUE;
                break;
            case MP4_FOURCC('m', 'j', 'p', 'a'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_MJPEGA;
                IsVideo = NV_TRUE;
                break;
            case MP4_FOURCC('m', 'j', 'p', 'b'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_MJPEGB;
                IsVideo = NV_TRUE;
                break;
            case MP4_FOURCC('j', 'p', 'e', 'g'):
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_MJPEGA;
                IsVideo = NV_TRUE;
                break;
            default:
                pNvMp4Parser->TrackInfo[AVCount].TrackType = NvMp4TrackTypes_UnsupportedVideo;
                break;
        }
        pNvMp4Parser->MvHdData.VideoTracks++;
        if (IsVideo && pNvMp4Parser->VideoIndex == 0xFFFFFFFF)
        {
            pNvMp4Parser->VideoIndex = AVCount;
        }
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

static NvError
Mp4ParserMOOV (
    NvMp4Parser* pNvMp4Parser,
    NvU32 AtomType,
    NvU64 AtomSize,
    NvU64* pBytesRead,
    void *pData)
{
    NvError Status = NvSuccess;
    NvU64 TrackOffset = 0, BytesRead = 0;
    NvU8 ReadBuffer[4];
    NvU64 FilePos;
    NvMp4MovieData *pMvHdData = &pNvMp4Parser->MvHdData;

    if (AtomType == MP4_FOURCC('m', 'v', 'h', 'd'))
    {
        /**
         * Parse the MVHD Atom and get creationtime, modification time,
         * next trackid and the bytes for mvhd header
         */
        NVMM_CHK_ERR (Mp4ParserMVHD (pNvMp4Parser, AtomSize, &BytesRead));
    }
    else if (AtomType == MP4_FOURCC('t', 'r', 'a', 'k'))
    {
        NVMM_CHK_CP (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64 *) &FilePos));
        TrackOffset = FilePos - MP4_DEFAULT_ATOM_HDR_SIZE;
        NVMM_CHK_ERR (Mp4ParserAVTrack (pNvMp4Parser, AtomType, AtomSize, &BytesRead, &TrackOffset));
        pMvHdData->TrackCount++;
        if (pMvHdData->TrackCount >= MAX_TRACKS)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "EXCESSTRACKS_ERROR = %d\n", pMvHdData->TrackCount));
            Status = NvError_ParserCorruptedStream;
            goto cleanup;
        }
    }
    else if (AtomType == MP4_FOURCC('u', 'd', 't', 'a'))
    {
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, NULL, Mp4ParserUDTA));
    }
    else if (AtomType == MP4_FOURCC('m', 'e', 't', 'a'))
    {
        /* "meta" is full box, skip 4 more bytes (version and flags) */
        NVMM_CHK_CP (pNvMp4Parser->pPipe->cpipe.Read (pNvMp4Parser->hContentPipe, (CPbyte*) ReadBuffer, (CPuint) 4));
        if (0 == NV_BE_TO_INT_32(&ReadBuffer[0]))
        {
            /* parse meta only when we see correct version and flags (all zeros)*/
            NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, NULL, Mp4ParserMETA));
        }
        BytesRead += 4;
    }
    else if (AtomType == MP4_FOURCC('r', 'm', 'r', 'a'))
    {
        NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize, &BytesRead, NULL, Mp4ParserRMRA));
    }

cleanup:
    *pBytesRead = BytesRead;
    return Status;
}

/*
 ******************************************************************************
 */
static NvError
Mp4IdentifyMOOV (
    NvMp4Parser * pNvMp4Parser)
{
    NvError Status = NvSuccess;
    NvU8 HdrBytesRead = 0;
    NvU64 BytesRead = 0;
    NvU32 AtomType;
    NvU64 FileOffset, AtomSize = 0;
    NvMp4MovieData *pMvHdData = &pNvMp4Parser->MvHdData;
    NvS32 checktimes = 0;
    CP_CHECKBYTESRESULTTYPE eResult;

    // Make sure we are parsing from the start of the file.
    NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) 0, CP_OriginBegin));

    // Set MOOV FileOffset to an invalid value in the beginning
    FileOffset = MP4_INVALID_NUMBER;

    /**
     * This code assumes that MOOV header is there in the file.
     * Partial files will return an NvError_ParserFailure after scanning through the
     * entire file.
     */
    while (FileOffset == MP4_INVALID_NUMBER)
    {
        NVMM_CHK_ERR (Mp4ParserATOMH (pNvMp4Parser, &AtomType, &AtomSize, &HdrBytesRead));
        if (AtomType  ==  MP4_FOURCC('m', 'o', 'o', 'v'))
        {
            FileOffset = BytesRead;
        }
        else
        {
            if (AtomType  ==  MP4_FOURCC('m', 'd', 'a', 't'))
            {
                pNvMp4Parser->MDATSize = AtomSize;
            }
            NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) (AtomSize - HdrBytesRead), CP_OriginCur));
            BytesRead +=  AtomSize;
        }
    }

    if (AtomType  ==  MP4_FOURCC('m', 'o', 'o', 'v'))
    {
        checktimes = 100;
        while (checktimes >= 0 && !NvMMSockGetBlockActivity())
        {
            pNvMp4Parser->pPipe->cpipe.CheckAvailableBytes(pNvMp4Parser->hContentPipe,
                                                       AtomSize,
                                                       &eResult);
            if (eResult == CP_CheckBytesNotReady)
            {
                checktimes--;
                NvOsSleepMS(10);
            }
            else
                break;
        }
    }

    // Initialize number of tracks count
    pNvMp4Parser->MvHdData.AudioTracks = 0;
    pNvMp4Parser->MvHdData.VideoTracks = 0;
    pMvHdData->TrackCount = 0;

    NVMM_CHK_ERR (Mp4ParserATOM (pNvMp4Parser, AtomType, AtomSize - HdrBytesRead, &BytesRead, NULL, Mp4ParserMOOV));


cleanup:
    return Status;

}


static NvError
Mp4ParserAVCSpecificConfig (
    NvMp4Parser * pNvMp4Parser,
    NvU32 TrackIndex,
    NvAVCConfigData *pAVCConfigData,
    NvU8*pDecSpecInfo,
    NvU32 Size)
{
    NvError Status = NvSuccess;
    NvU8 *ReadBuffer, *ReadBuffer1, *ReadBuffer2;
    NvU8 identified = 0;
    NvU32 AtomType;
    NvU32 AtomSize, AvgBitRate = 0, MaxBitRate = 0;
    NvU32 i, j;
    NvU32 FileBytesRead = 0;
    NvU32 AVCBytesRead = 0;

    ReadBuffer = pDecSpecInfo;
    ReadBuffer += 8;
    FileBytesRead += 8;

    pAVCConfigData->DataRefIndex = (ReadBuffer[6] << 8) | (ReadBuffer[7]);
    /* Version + Revision Level and Vendor */
    ReadBuffer += 8;
    FileBytesRead += 8;

    /* Temporal quality and Spatial Quality */
    ReadBuffer += 8;
    FileBytesRead += 8;
    FileBytesRead += 4;

    pAVCConfigData->Width = (ReadBuffer[0] << 8) | (ReadBuffer[1]);
    pAVCConfigData->Height = (ReadBuffer[2] << 8) | (ReadBuffer[3]);

    ReadBuffer += 4;
    FileBytesRead += 8;

    pAVCConfigData->HorizontalResolution = NV_BE_TO_INT_32 (&ReadBuffer[0]);
    pAVCConfigData->VerticalResolution = NV_BE_TO_INT_32 (&ReadBuffer[4]);

    ReadBuffer += 8;
    FileBytesRead += 6;

    pAVCConfigData->FrameCount = (ReadBuffer[4] << 8) | (ReadBuffer[5]);

    ReadBuffer += 6;
    FileBytesRead += 32;
    ReadBuffer += 32;
    FileBytesRead += 4;

    pAVCConfigData->Depth = (ReadBuffer[0] << 8) | (ReadBuffer[1]);
    pAVCConfigData->ColorTableID = (ReadBuffer[2] << 8) | (ReadBuffer[3]);

    ReadBuffer += 4;

    identified = (Size > (FileBytesRead + 4) ? 0 : 1);

    while (identified != 1)
    {
        FileBytesRead += 8;
        AtomType = NV_BE_TO_INT_32 (&ReadBuffer[4]);
        AtomSize = NV_BE_TO_INT_32 (&ReadBuffer[0]);
        ReadBuffer += 8;

        if (AtomType == MP4_FOURCC('f', 'i', 'e', 'l'))
        {
            FileBytesRead += 2;
            pAVCConfigData->Field1 = ReadBuffer[0];
            pAVCConfigData->Field2 = ReadBuffer[1];

            ReadBuffer += 2;
        }
        else if (AtomType == MP4_FOURCC('g', 'a', 'm', 'a'))
        {
            FileBytesRead += 4;
            pAVCConfigData->Gamma = NV_BE_TO_INT_32 (&ReadBuffer[0]);
            ReadBuffer += 4;
        }
        else if (AtomType == MP4_FOURCC('a', 'v', 'c', 'C'))
        {
            pAVCConfigData->ConfigVersion = ReadBuffer[0];
            pAVCConfigData->ProfileIndication = ReadBuffer[1];

            pAVCConfigData->ProfileCompatibility = ReadBuffer[2];

            pAVCConfigData->LevelIndication = ReadBuffer[3]; // 10 times the Table A-1 profile from specs

            pAVCConfigData->Length = (ReadBuffer[4]) & 0x3;
            pNvMp4Parser->NALSize = pAVCConfigData->Length;

            pAVCConfigData->NumSequenceParamSets = (ReadBuffer[5]) & 0x1f;

            AVCBytesRead = 6;
            FileBytesRead += 6;
            ReadBuffer1 = &ReadBuffer[6];

            pAVCConfigData->SeqParamSetLength[0] = 0;
            for (i = 0; i < (pAVCConfigData->NumSequenceParamSets); i++)
            {
                pAVCConfigData->SeqParamSetLength[i] = (ReadBuffer1[0] << 8) | (ReadBuffer1[1]);
                ReadBuffer1 += 2;

                FileBytesRead += (pAVCConfigData->SeqParamSetLength[i] + 2);
                AVCBytesRead   += (pAVCConfigData->SeqParamSetLength[i] + 2);
                if (FileBytesRead > Size || AVCBytesRead > (AtomSize - 8))
                {
                    Status = NvError_ParserFailure;
                    goto ExitParseAVC;
                }
                for (j = 0; j < pAVCConfigData->SeqParamSetLength[i]; j++)
                {
                    pAVCConfigData->SpsNALUnit[i][j] = ReadBuffer1[j];
                }

                ReadBuffer1 += (pAVCConfigData->SeqParamSetLength[i]);
            }
            pAVCConfigData->NumOfPictureParamSets = (ReadBuffer1[0]);

            ReadBuffer1 += 1;
            FileBytesRead += 1;
            AVCBytesRead   += 1;

            pAVCConfigData->PicParamSetLength[0] = 0;
            for (i = 0; i < (pAVCConfigData->NumOfPictureParamSets); i++)
            {
                pAVCConfigData->PicParamSetLength[i] = (ReadBuffer1[0] << 8) | (ReadBuffer1[1]);
                ReadBuffer1 += 2;
                FileBytesRead += (pAVCConfigData->PicParamSetLength[i] + 2);
                AVCBytesRead   += (pAVCConfigData->PicParamSetLength[i] + 2);
                if (FileBytesRead > Size || AVCBytesRead > (AtomSize - 8))
                {
                    Status = NvError_ParserFailure;
                    goto ExitParseAVC;
                }
                for (j = 0; j < pAVCConfigData->PicParamSetLength[i]; j++)
                {
                    pAVCConfigData->PpsNALUnit[i][j] = ReadBuffer1[j];
                }
                pAVCConfigData->EntropyType = NvMp4ParserParsePPS (pAVCConfigData->PpsNALUnit[i], pAVCConfigData->PicParamSetLength[i]);
                ReadBuffer1 += (pAVCConfigData->PicParamSetLength[i]);
            }

            ReadBuffer2 = &ReadBuffer1[0];
            FileBytesRead += 8;
            if (FileBytesRead > Size)
            {
                identified = 1;
                goto ExitParseAVC;
            }
            AtomType = NV_BE_TO_INT_32 (&ReadBuffer2[4]);
            ReadBuffer2 += 8;
            //20bytes: btrt_atom size(4bytes), btrt(4bytes), BufferFullness(4bytes), avg bitrate(4byts),
            //max bitrate(4 bytes)
            if (AtomType == MP4_FOURCC('b', 't', 'r', 't'))
            {
                pNvMp4Parser->TrackInfo[TrackIndex].MaxBitRate = MaxBitRate = NV_BE_TO_INT_32 (&ReadBuffer2[4]);
                pNvMp4Parser->TrackInfo[TrackIndex].AvgBitRate = AvgBitRate = NV_BE_TO_INT_32 (&ReadBuffer2[8]);
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "H264: AvgBitRate = %d - MaxBitRate\n", AvgBitRate, MaxBitRate));
                ReadBuffer2 += 12;
                FileBytesRead += 12;
            }
            identified = 1;
            break;
        }
        else
        {
            FileBytesRead += (AtomSize - 8);
            if (FileBytesRead > Size)
            {
                Status = NvError_ParserFailure;
                break;
            }

            /* For any unidentified AtomType */
            ReadBuffer += (AtomSize - 8);
        }
        identified = (Size > (FileBytesRead + 4) ? 0 : 1);
    }

ExitParseAVC:
    return Status;
}


static NvError
MP4ParserAllocFramingInfo (
    NvMp4FramingInfo *pFramingInfo)
{
    NvError Status = NvSuccess;
    
    pFramingInfo->OffsetTable = (NvU64 *) NvOsAlloc (sizeof (NvU64) * pFramingInfo->MaxFramesPerBlock);
    NVMM_CHK_MEM (pFramingInfo->OffsetTable);
    NvOsMemset (pFramingInfo->OffsetTable, 0, sizeof (NvU64) * pFramingInfo->MaxFramesPerBlock);

    pFramingInfo->SizeTable = (NvU32 *) NvOsAlloc (sizeof (NvU32) * pFramingInfo->MaxFramesPerBlock);
    NVMM_CHK_MEM (pFramingInfo->SizeTable);
    NvOsMemset (pFramingInfo->SizeTable, 0, sizeof (NvU32) * pFramingInfo->MaxFramesPerBlock);

    pFramingInfo->DTSTable = (NvU64 *) NvOsAlloc (sizeof (NvU64) * pFramingInfo->MaxFramesPerBlock);
    NVMM_CHK_MEM (pFramingInfo->DTSTable);
    NvOsMemset (pFramingInfo->DTSTable, 0, sizeof (NvU64) * pFramingInfo->MaxFramesPerBlock);

    pFramingInfo->CTSTable = (NvU32 *) NvOsAlloc (sizeof (NvU32) * pFramingInfo->MaxFramesPerBlock);
    NVMM_CHK_MEM (pFramingInfo->CTSTable);
    NvOsMemset (pFramingInfo->CTSTable, 0, sizeof (NvU32) * pFramingInfo->MaxFramesPerBlock);

cleanup:
    return Status;
}

static NvError
Mp4ReadBulkData (
    NvMp4Parser *pNvMp4Parser,
    Mp4ReadBulkDataType ReadType,
    NvU64 FileOffset,
    NvU64 EntryCount,
    NvU32 EntrySize,
    NvU8 *pParam1,
    NvU8 *pParam2)
{
    NvError Status = NvSuccess;
    NvU8* ReadBuffer = NULL;
    NvU64 i = 0, j = 0, TotalReadCount = 0, ReadCount = 0;
    NvU64 FrameCount = 0, TimeIncrement = 0;
    NvU32 k = 0, SampleCount = 0, SampleOffset = 0, SampleDelta = 0;
    NvMp4FramingInfo* pFramingInfo = NULL;
    NvU32 BlockNum = 0;
    NvU32 ActualFramesInBlock = 0;
                
    NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));

    TotalReadCount = EntryCount * EntrySize;
    for (i = 0; i <= TotalReadCount / READ_BULK_DATA_SIZE; i++)
    {
        ReadBuffer = pNvMp4Parser->pTempBuffer;
        ReadCount = (i < TotalReadCount / READ_BULK_DATA_SIZE) ? READ_BULK_DATA_SIZE : (TotalReadCount % READ_BULK_DATA_SIZE);
        if (ReadCount > 0)
            NVMM_CHK_CP (pNvMp4Parser->pPipe->cpipe.Read (pNvMp4Parser->hContentPipe, (CPbyte *) ReadBuffer, (CPuint) ReadCount));
        for (j = 0; j < ReadCount; j += EntrySize, ReadBuffer += EntrySize)
        {
            if (Mp4ReadBulkDataType_STSZ == ReadType)
            {
                * (NvU32 *) pParam1 += NV_BE_TO_INT_32 (ReadBuffer); // this is CumFrameSize
            }
            else if (Mp4ReadBulkDataType_STCO == ReadType)
            {
                if (EntrySize == sizeof (NvU64))
                {
                    NvU32 LowInt = 0, HighInt = 0;
                    LowInt = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                    HighInt = NV_BE_TO_INT_32 (&ReadBuffer[4]);
                    * (NvU64*) pParam1 = ( (NvU64) LowInt << 32) | (NvU64) HighInt; // this is pChunkOffsetArray
                }
                else
                {
                    * (NvU64*) pParam1 = NV_BE_TO_INT_32 (ReadBuffer); // this is pChunkOffsetArray
                }
                pParam1 += sizeof (NvU64);
            }
            else if (Mp4ReadBulkDataType_CTTS == ReadType)
            {
                // sample table is:
                // 4-byte: # of frames
                // 4-byte: sample-FileOffset # of frames
                //
                // So, parse from the beginning, find the current frame, and start updating
                // from there
                pFramingInfo = (NvMp4FramingInfo *) pParam1;
                BlockNum = * (NvU32 *) pParam2;

                SampleCount = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                SampleOffset = NV_BE_TO_INT_32 (&ReadBuffer[4]);

                for (k = 0; k < SampleCount; k++)
                {
                    if (FrameCount == ( (BlockNum + 1) * pFramingInfo->MaxFramesPerBlock))
                    {
                        return NvSuccess; // we are done
                    }
                    else if (FrameCount >= (BlockNum * pFramingInfo->MaxFramesPerBlock) &&
                             FrameCount < ( (BlockNum + 1) * pFramingInfo->MaxFramesPerBlock))
                    {
                        pFramingInfo->CTSTable[FrameCount - (BlockNum * pFramingInfo->MaxFramesPerBlock) ] = SampleOffset;
                    }
                    FrameCount++;
                }
            }
            else if (Mp4ReadBulkDataType_STTS == ReadType)
            {
                pFramingInfo = (NvMp4FramingInfo *) pParam1;
                BlockNum = * (NvU32 *) pParam2;

                if (EntryCount == 1)
                { // indicates all frames are have constant incrementing time stamp
                    SampleCount = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                    SampleDelta = NV_BE_TO_INT_32 (&ReadBuffer[4]);
                    TimeIncrement = (NvU64) ( (NvU64) SampleDelta * (BlockNum * pFramingInfo->MaxFramesPerBlock));

                    ActualFramesInBlock = (BlockNum < pFramingInfo->TotalNoBlocks - 1) ? (pFramingInfo->MaxFramesPerBlock) : (pFramingInfo->TotalNoFrames % pFramingInfo->MaxFramesPerBlock);

                    for (k = 0; k < ActualFramesInBlock; k++)
                    {
                        pFramingInfo->DTSTable[k] = TimeIncrement;
                        TimeIncrement += SampleDelta;
                    }
                }
                else
                {
                    // sample table is:
                    // 4-byte: # of frames
                    // 4-byte: duration for the given # of frames
                    //
                    // So, parse from the beginning, find the current frame, and start updating
                    // from there
                    SampleCount = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                    SampleDelta = NV_BE_TO_INT_32 (&ReadBuffer[4]);

                    for (k = 0; k < SampleCount; k++)
                    {
                        if (FrameCount == ( (BlockNum + 1) * pFramingInfo->MaxFramesPerBlock))
                        {
                            return NvSuccess; // we are done
                        }
                        else if (FrameCount >= (BlockNum * pFramingInfo->MaxFramesPerBlock) &&
                                 FrameCount < ( (BlockNum + 1) * pFramingInfo->MaxFramesPerBlock))
                        {
                            pFramingInfo->DTSTable[FrameCount - (BlockNum * pFramingInfo->MaxFramesPerBlock) ] = TimeIncrement;
                        }
                        FrameCount++;
                        TimeIncrement += SampleDelta;
                    }
                }

            }
        }
    }

cleanup:
    return Status;
}

static void
NvMp4ParserFreeSTSCList (
    NvMp4Parser* pNvMp4Parser,
    NvU32 StreamIndex)
{
    NvStscEntry *stsc = NULL, *stsc_temp = NULL;

    if(!pNvMp4Parser->FramingInfo[StreamIndex].IsValidStsc) {
        return;
    }

    while(NV_TRUE)
    {
        stsc = pNvMp4Parser->FramingInfo[StreamIndex].StscEntryListHead;
        /*find and free the last entry*/
        while(stsc->next)
        {
            stsc_temp = stsc;
            stsc = stsc_temp->next;
        }
        /*stsc points to last entry, stsc_temp->next points to stsc*/
        NvOsFree(stsc);

        if(stsc == pNvMp4Parser->FramingInfo[StreamIndex].StscEntryListHead)
        {
            pNvMp4Parser->FramingInfo[StreamIndex].StscEntryListHead = NULL;
            break; /*freeing the list head*/
        }
        else
        {
            /*making stsc_temp the last entry*/
            stsc_temp->next = NULL;
        }
    }
}

NvError
NvMp4ParserGetAudioProps (
    NvMp4Parser* pNvMp4Parser,
    NvMMStreamInfo* pCurrentStream,
    NvU32 TrackIndex,
    NvU32* pMaxBitRate)
{
    NvError Status = NvSuccess;
    void *pHeaderData = NULL ;
    NvU32 HeaderSize = 0;
    NvU8 *ReadBuffer;
    NvU8 SamplingFreqIndex;
    NvU32 FreqTable[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, -1, -1, 0 };

    pHeaderData = NvMp4ParserGetDecConfig (pNvMp4Parser, TrackIndex, &HeaderSize);
    ReadBuffer = (NvU8*) pHeaderData;
    SamplingFreqIndex = ( ( (ReadBuffer[0]) & (0x07)) << 1) | ( ( (ReadBuffer[1]) & (0x80)) >> 7);
    if (SamplingFreqIndex == 0xf)
    {
        pCurrentStream->NvMMStream_Props.AudioProps.SampleRate = ( ( (ReadBuffer[1]) & (0x7f)) << 17) | (ReadBuffer[2] << 9) | (ReadBuffer[3] << 1) | ( (ReadBuffer[4] & 0x80) >> 7);
        pCurrentStream->NvMMStream_Props.AudioProps.NChannels = ( ( (ReadBuffer[4]) & (0x78)) >> 3);
    }
    else
    {
        pCurrentStream->NvMMStream_Props.AudioProps.SampleRate = FreqTable[SamplingFreqIndex];
        pCurrentStream->NvMMStream_Props.AudioProps.NChannels = ( ( (ReadBuffer[1]) & (0x78)) >> 3);
    }

    Status = NvMp4ParserGetBitRate(pNvMp4Parser, TrackIndex,
                                    (NvU32*) &pCurrentStream->NvMMStream_Props.AudioProps.BitRate,
                                    (NvU32*) pMaxBitRate);
    return Status;
}

NvError
NvMp4ParserInit (
    NvMp4Parser *pNvMp4Parser)
{
    NvError Status = NvSuccess;
    NvU32 i;

    NVMM_CHK_ARG (pNvMp4Parser);

    /* Initialize  state */
    for (i = 0; i < MAX_TRACKS; i++)
    {
        pNvMp4Parser->FramingInfo[i].CurrentBlock = -1;
        pNvMp4Parser->FramingInfo[i].CurrentFrameIndex = -1;
        pNvMp4Parser->FramingInfo[i].FrameSize = 1;
        pNvMp4Parser->FramingInfo[i].IsCo64bitOffsetPresent = NV_FALSE;
        pNvMp4Parser->FramingInfo[i].IsCorruptedFile = NV_FALSE;
        pNvMp4Parser->TrackInfo[i].IsCo64bitOffsetPresent = NV_FALSE;
        pNvMp4Parser->TrackInfo[i].IsELSTPresent = NV_FALSE;
        pNvMp4Parser->TrackInfo[i].EndOfStream = NV_FALSE;
    }

    pNvMp4Parser->AACConfigData.SamplingFreqIndex = 4;
    pNvMp4Parser->AACConfigData.SampleSize = 16;
    pNvMp4Parser->AACConfigData.SamplingFreq = 44100;
    pNvMp4Parser->AACConfigData.SbrPresentFlag = -1;
    pNvMp4Parser->VideoIndex = 0xFFFFFFFF;

    NvmmGetFileContentPipe (&pNvMp4Parser->pPipe);
    NVMM_CHK_ARG (pNvMp4Parser->pPipe);

    NVMM_CHK_ERR (pNvMp4Parser->pPipe->cpipe.Open (&pNvMp4Parser->hContentPipe, (CPstring) pNvMp4Parser->URIList[0], CP_AccessRead));
    NVMM_CHK_ARG (pNvMp4Parser->hContentPipe);

    NVMM_CHK_ERR (pNvMp4Parser->pPipe->GetSize (pNvMp4Parser->hContentPipe, (CPuint64 *) &pNvMp4Parser->FileSize));
    NVMM_CHK_ARG (pNvMp4Parser->FileSize);

    if (pNvMp4Parser->pPipe->IsStreaming (pNvMp4Parser->hContentPipe))
    {
        pNvMp4Parser->IsStreaming = NV_TRUE;
    }

    pNvMp4Parser->VideoSyncData.IsReady = NV_FALSE;

    pNvMp4Parser->MediaMetadata.TrackNumber = MP4_INVALID_NUMBER;
    pNvMp4Parser->MediaMetadata.TotalTracks = MP4_INVALID_NUMBER;

    pNvMp4Parser->AudioIndex = MP4_INVALID_NUMBER;
    pNvMp4Parser->VideoIndex = MP4_INVALID_NUMBER;

    pNvMp4Parser->IsSINF = NV_FALSE;
    pNvMp4Parser->DrmInterface.pNvDrmBindContent = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmBindLicense = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmClkGenerateChallenge = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmClkProcessResponse = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmCreateContext = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmDecrypt = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmDestroyContext = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmGenerateLicenseChallenge = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmGetPetetionURL = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmProcessLicenseResponse = NULL;
    pNvMp4Parser->DrmInterface.pNvDrmUpdateMeteringInfo = NULL;

cleanup:
    return Status;
}

void
NvMp4ParserDeInit (
    NvMp4Parser * pNvMp4Parser)
{
    NvU32 i = 0;
    NvU32 index = 0;

    if (pNvMp4Parser)
    {
        if (pNvMp4Parser->hContentPipe)
        {
            (void) pNvMp4Parser->pPipe->cpipe.Close (pNvMp4Parser->hContentPipe);
            pNvMp4Parser->hContentPipe = 0;
        }

        for (i = 0 ; i < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks); i++)
        {
            if (pNvMp4Parser->FramingInfo[i].OffsetTable)
            {
                NvOsFree (pNvMp4Parser->FramingInfo[i].OffsetTable);
                pNvMp4Parser->FramingInfo[i].OffsetTable = NULL;
            }

            if (pNvMp4Parser->FramingInfo[i].SizeTable)
            {
                NvOsFree (pNvMp4Parser->FramingInfo[i].SizeTable);
                pNvMp4Parser->FramingInfo[i].SizeTable = NULL;
            }

            if (pNvMp4Parser->FramingInfo[i].DTSTable)
            {
                NvOsFree (pNvMp4Parser->FramingInfo[i].DTSTable);
                pNvMp4Parser->FramingInfo[i].DTSTable = NULL;
            }

            if (pNvMp4Parser->FramingInfo[i].CTSTable)
            {
                NvOsFree (pNvMp4Parser->FramingInfo[i].CTSTable);
                pNvMp4Parser->FramingInfo[i].CTSTable = NULL;
            }

            if (pNvMp4Parser->TrackInfo[i].SegmentTable)
            {
                NvOsFree (pNvMp4Parser->TrackInfo[i].SegmentTable);
                pNvMp4Parser->TrackInfo[i].SegmentTable = 0;
            }
            if (pNvMp4Parser->TrackInfo[i].pELSTMediaTime)
            {
                NvOsFree (pNvMp4Parser->TrackInfo[i].pELSTMediaTime);
                pNvMp4Parser->TrackInfo[i].pELSTMediaTime = 0;
            }

            if (pNvMp4Parser->TrackInfo[i].pESDS)
            {
                NvOsFree (pNvMp4Parser->TrackInfo[i].pESDS);
                pNvMp4Parser->TrackInfo[i].pESDS = NULL;
            }

            if(pNvMp4Parser->FramingInfo[i].IsValidStsc)
            {
                NvMp4ParserFreeSTSCList(pNvMp4Parser, i);
                pNvMp4Parser->FramingInfo[i].IsValidStsc = NV_FALSE;
            }

            for(index = 0 ; index < pNvMp4Parser->nAVCConfigCount; index++)
            {
                if(pNvMp4Parser->pAVCConfigData[index])
                {
                    NvOsFree(pNvMp4Parser->pAVCConfigData[index]);
                    pNvMp4Parser->pAVCConfigData[index] = NULL;
                }
            }
            pNvMp4Parser->nAVCConfigCount = 0;

            for(index = 0 ;
                index < pNvMp4Parser->TrackInfo[i].nDecSpecInfoCount; index++)
            {
                if(pNvMp4Parser->TrackInfo[i].pDecSpecInfo[index])
                {
                    NvOsFree(pNvMp4Parser->TrackInfo[i].pDecSpecInfo[index]);
                    pNvMp4Parser->TrackInfo[i].pDecSpecInfo[index] = NULL;
                    pNvMp4Parser->TrackInfo[i].DecInfoSize[index] = 0;
                }
            }
            pNvMp4Parser->TrackInfo[i].nDecSpecInfoCount = 0;
        }

        if (pNvMp4Parser->IsSINF)
        {
            pNvMp4Parser->SINFSize = 0;
            if (pNvMp4Parser->pSINF)
            {
                NvOsFree (pNvMp4Parser->pSINF);
                pNvMp4Parser->pSINF = NULL;
            }
            pNvMp4Parser->IsSINF = NV_FALSE;
            pNvMp4Parser->DrmInterface.pNvDrmBindContent = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmBindLicense = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmClkGenerateChallenge = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmClkProcessResponse = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmCreateContext = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmDecrypt = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmDestroyContext = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmGenerateLicenseChallenge = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmGetPetetionURL = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmProcessLicenseResponse = NULL;
            pNvMp4Parser->DrmInterface.pNvDrmUpdateMeteringInfo = NULL;
        }

    }

}

NvError
NvMp4ParserParse (
    NvMp4Parser *pNvMp4Parser)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (pNvMp4Parser);

    // Parse the MOOV AtomType
    NVMM_CHK_ERR (Mp4IdentifyMOOV (pNvMp4Parser));

    // Find the MDAT header and save the mdat size
    if (pNvMp4Parser->IsStreaming)
        (void) Mp4IdentifyMDAT (pNvMp4Parser);

cleanup:
    return Status;
}

NvU32
NvMp4ParserGetNumTracks (
    NvMp4Parser *pNvMp4Parser)
{
    NvError Status = NvSuccess;
    NvMp4TrackTypes Count = 0;

    NVMM_CHK_ARG (pNvMp4Parser);

    Count = pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks;

cleanup:
    return Count;
}

NvMp4TrackTypes
NvMp4ParserGetTracksInfo (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex)
{
    NvError Status = NvSuccess;
    NvMp4TrackTypes type = NvMp4TrackTypes_OTHER;

    NVMM_CHK_ARG (pNvMp4Parser);

    type = pNvMp4Parser->TrackInfo[TrackIndex].TrackType;

cleanup:
    return type;
}

NvError
NvMp4ParserSetTrack (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex)
{
    NvError Status = NvSuccess;
    NvU32 i = 0;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));

    if (pNvMp4Parser->TrackInfo[TrackIndex].IsELSTPresent)
    {
        Status = Mp4ParserELST (pNvMp4Parser, &pNvMp4Parser->TrackInfo[TrackIndex]);
    }

    pNvMp4Parser->FramingInfo[TrackIndex].TotalNoFrames = pNvMp4Parser->TrackInfo[TrackIndex].STSZSampleCount;

    if (pNvMp4Parser->IsStreaming && pNvMp4Parser->FramingInfo[TrackIndex].TotalNoFrames > NUM_FRAMES_IN_BLOCK)
    {
        pNvMp4Parser->FramingInfo[TrackIndex].MaxFramesPerBlock = pNvMp4Parser->FramingInfo[TrackIndex].TotalNoFrames;
    }
    else
    {
        pNvMp4Parser->FramingInfo[TrackIndex].MaxFramesPerBlock = NUM_FRAMES_IN_BLOCK;
    }
    pNvMp4Parser->FramingInfo[TrackIndex].TotalNoBlocks = (pNvMp4Parser->FramingInfo[TrackIndex].TotalNoFrames /
            pNvMp4Parser->FramingInfo[TrackIndex].MaxFramesPerBlock) + 1;

    NVMM_CHK_ERR (MP4ParserAllocFramingInfo (&pNvMp4Parser->FramingInfo[TrackIndex]));
    pNvMp4Parser->FramingInfo[TrackIndex].BitRate = pNvMp4Parser->TrackInfo[TrackIndex].AvgBitRate;

    /**
     * Initialize the framing info variables such that when the first frame is
     * to be process, a call is made to prepareFramingInfo.
     */
    pNvMp4Parser->FramingInfo[TrackIndex].CurrentBlock = -1;
    pNvMp4Parser->FramingInfo[TrackIndex].CurrentFrameIndex = -1;
    pNvMp4Parser->FramingInfo[TrackIndex].SkipFrameCount = 0;
    pNvMp4Parser->FramingInfo[TrackIndex].ValidFrameCount = 0;


    if (pNvMp4Parser->TrackInfo[TrackIndex].TrackType == NvMp4TrackTypes_AVC)
    {
        pNvMp4Parser->nAVCConfigCount = pNvMp4Parser->TrackInfo[TrackIndex].nDecSpecInfoCount;
        for(i = 0 ; i < pNvMp4Parser->nAVCConfigCount ; i++)
        {
            /*parse Decoder Specific Information and save to avcC*/
            pNvMp4Parser->pAVCConfigData[i] = NvOsAlloc(sizeof(NvAVCConfigData));
            NVMM_CHK_MEM(pNvMp4Parser->pAVCConfigData[i]);
            NvOsMemset(pNvMp4Parser->pAVCConfigData[i], 0, sizeof(NvAVCConfigData));
            NVMM_CHK_ERR (Mp4ParserAVCSpecificConfig(pNvMp4Parser, TrackIndex,
                pNvMp4Parser->pAVCConfigData[i],
                (void *) pNvMp4Parser->TrackInfo[TrackIndex].pDecSpecInfo[i],
                pNvMp4Parser->TrackInfo[TrackIndex].DecInfoSize[i]));
        }
        pNvMp4Parser->FramingInfo[TrackIndex].BitRate  = pNvMp4Parser->TrackInfo[TrackIndex].AvgBitRate;
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "AVC: videoBitRate = %d\n", pNvMp4Parser->FramingInfo[TrackIndex].BitRate));
    }

cleanup:
    return Status;
}

void *
NvMp4ParserGetDecConfig (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvU32 *pDecInfoSize)
{
    NvError Status = NvSuccess;
    void *pDecSpecInfo = NULL;

    NVMM_CHK_ARG (pNvMp4Parser);

    pDecSpecInfo = pNvMp4Parser->TrackInfo[TrackIndex].pDecSpecInfo[0];
    *pDecInfoSize = pNvMp4Parser->TrackInfo[TrackIndex].DecInfoSize[0];

cleanup:
    return pDecSpecInfo;
}

NvU64
NvMp4ParserGetMediaTimeScale (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex)
{
    NvError Status = NvSuccess;
    NvU64 TimeScale = 0;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));
    NVMM_CHK_ARG (pNvMp4Parser->TrackInfo[TrackIndex].TrackType != NvMp4TrackTypes_OTHER);

    TimeScale = pNvMp4Parser->TrackInfo[TrackIndex].TimeScale;

cleanup:
    return TimeScale;
}

NvError
NvMp4ParserSetMediaDuration (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvU64 Duration)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));
    NVMM_CHK_ARG (pNvMp4Parser->TrackInfo[TrackIndex].TrackType != NvMp4TrackTypes_OTHER);

    if (NVMP4_ISTRACKAUDIO (TrackIndex))
    {
        pNvMp4Parser->TrackInfo[TrackIndex].Duration = Duration;
    }

cleanup:
    return Status;
}

NvU64
NvMp4ParserGetMediaDuration (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex)
{
    NvError Status = NvSuccess;
    NvU64 Duration = 0;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));
    NVMM_CHK_ARG (pNvMp4Parser->TrackInfo[TrackIndex].TrackType != NvMp4TrackTypes_OTHER);

    Duration = pNvMp4Parser->TrackInfo[TrackIndex].Duration;

cleanup:
    return Duration;
}

NvU32
NvMp4ParserGetTotalMediaSamples (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex)
{

    NvError Status = NvSuccess;
    NvU32 SampleCount = 0;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));
    NVMM_CHK_ARG (pNvMp4Parser->TrackInfo[TrackIndex].TrackType != NvMp4TrackTypes_OTHER);

    SampleCount = pNvMp4Parser->TrackInfo[TrackIndex].STSZSampleCount;

cleanup:
    return SampleCount;
}

NvError
NvMp4ParserGetBitRate (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvU32 *pAvgBitRate,
    NvU32 *pMaxBitRate)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (pNvMp4Parser && pAvgBitRate && pMaxBitRate);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));
    NVMM_CHK_ARG (pNvMp4Parser->TrackInfo[TrackIndex].TrackType != NvMp4TrackTypes_OTHER);

    *pAvgBitRate =  pNvMp4Parser->TrackInfo[TrackIndex].AvgBitRate;
    *pMaxBitRate =  pNvMp4Parser->TrackInfo[TrackIndex].MaxBitRate;

cleanup:
    return Status;
}

NvError
NvMp4ParserGetNALSize (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex ,
    NvU32 *pNALSize)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (pNALSize);

    if (pNvMp4Parser->TrackInfo[TrackIndex].TrackType == NvMp4TrackTypes_AVC)
    {
        *pNALSize =  pNvMp4Parser->NALSize;
    }
    else
    {
        Status = NvError_ParserFailure;
    }

cleanup:
    return Status;
}

NvError
NvMp4ParserGetCoverArt (
    NvMp4Parser *pNvMp4Parser,
    void *pBuffer)
{
    NvError Status = NvSuccess;
    CP_PIPETYPE *pPipe = NULL;
    NvU64 FileOffset = 0;

    NVMM_CHK_ARG (pNvMp4Parser && pBuffer);

    pPipe = &pNvMp4Parser->pPipe->cpipe;
    FileOffset = pNvMp4Parser->MediaMetadata.CoverArtOffset;
    NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) pBuffer, pNvMp4Parser->MediaMetadata.CoverArtSize));

cleanup:
    return Status;
}

NvS32
NvMp4ParserGetNextSyncUnit (
    NvMp4Parser * pNvMp4Parser,
    NvS32 Reference,
    NvBool Direction)
{
    NvError Status = NvSuccess;
    NvU32 i = 0;
    NvU32 ReadCount = 0;
    NvU64 FileOffset;
    NvBool IsPreviousSyncUnit;
    CP_PIPETYPE *pPipe;
    NvMp4TrackInformation *pTrackInfo = NULL;
    NvMp4SynchUnitData * pSyncData = NULL;

    NVMM_CHK_ARG (pNvMp4Parser && Reference >=0);

    if (Reference == 0)
        return Reference;

    pPipe = &pNvMp4Parser->pPipe->cpipe;

    pTrackInfo = &pNvMp4Parser->TrackInfo[pNvMp4Parser->VideoIndex];

    pSyncData = &pNvMp4Parser->VideoSyncData;
    // populate the data structure if this is the first call
    if (!pSyncData->IsReady)
    {
        if (pTrackInfo->STSSEntries <= 0)
        {
            return (Reference + 1);
        }
        FileOffset = pTrackInfo->STSSOffset;
        FileOffset += 16; /* 16 bytes = 4 size + 4 AtomType + 4 Version-flag + 4 entry*/
        NVMM_CHK_ERR (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));
        pSyncData->FirstEntryIndex = 0;
        ReadCount = STSS_BUFF_SIZE;
        if (pTrackInfo->STSSEntries < STSS_BUFF_SIZE)
        {
            ReadCount = pTrackInfo->STSSEntries;
        }

        NVMM_CHK_ERR (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) pSyncData->ReadBuffer, ReadCount * 4));

        for (i = 0; i < ReadCount; i++)
        {
            pSyncData->ReadBuffer[i] = NV_BE_TO_INT_32 ( (NvU8 *) & pSyncData->ReadBuffer[i]);
        }
        pSyncData->ValidCount = ReadCount;
        pSyncData->IsReady = NV_TRUE;
    }

    // check if the index we are looking for is in the ReadBuffer
    if (0 < pSyncData->ValidCount)
    {
        if ( (Reference > pSyncData->ReadBuffer[0]) && (Reference <= pSyncData->ReadBuffer[pSyncData->ValidCount-1]))
        {
            //this is a hit, get the index
            // TODO: binary search would be faster
            for (i = 0; i < (pSyncData->ValidCount - 1); i++)
            {
                if ( (Reference <= pSyncData->ReadBuffer[i+1]) && (Reference >= pSyncData->ReadBuffer[i]))
                {
                    if (Direction)
                    {
                        if ( (Reference == pSyncData->ReadBuffer[i]) && (pNvMp4Parser->rate == 0 || pNvMp4Parser->rate == 1000))
                        {
                            return pSyncData->ReadBuffer[i];
                        }
                        else
                        {
                            return pSyncData->ReadBuffer[i+1];
                        }
                    }
                    else
                    {
                        if (Reference > pSyncData->ReadBuffer[i])
                        {
                            return pSyncData->ReadBuffer[i];
                        }
                        else
                        {
                            // means equal
                            return pSyncData->ReadBuffer[i-1];
                        }
                    }
                }
            }
        }
        //check the corner cases
        else if (Direction && (Reference == pSyncData->ReadBuffer[0]))
        {
            return pSyncData->ReadBuffer[1];
        }
        else if (!Direction && (Reference == pSyncData->ReadBuffer[pSyncData->ValidCount-1]))
        {
            return pSyncData->ReadBuffer[pSyncData->ValidCount-2];
        }
    }
    //no match, check if reloading the ReadBuffer would help
    //first find out if need to load previous or later records
    IsPreviousSyncUnit = NV_FALSE;
    if (Reference < pSyncData->ReadBuffer[0])
    {
        IsPreviousSyncUnit = NV_TRUE;
    }

    // check the corner cases first
    if (IsPreviousSyncUnit && (0 == pSyncData->FirstEntryIndex))
    {
        NVMM_CHK_ARG(Direction);
        return pSyncData->ReadBuffer[0];
    }
    if (!IsPreviousSyncUnit && ( (pSyncData->FirstEntryIndex + pSyncData->ValidCount) >= pTrackInfo->STSSEntries))
        return pSyncData->ReadBuffer[pSyncData->ValidCount-1];

    //update the ReadBuffer
    if (IsPreviousSyncUnit)
    {
        pSyncData->FirstEntryIndex = pSyncData->FirstEntryIndex - (STSS_BUFF_SIZE - 16);
    }
    else
    {
        pSyncData->FirstEntryIndex += pSyncData->ValidCount;
    }

    ReadCount = ((pSyncData->FirstEntryIndex + STSS_BUFF_SIZE) > pTrackInfo->STSSEntries) ?
                            pTrackInfo->STSSEntries-pSyncData->FirstEntryIndex : STSS_BUFF_SIZE;

    FileOffset = pTrackInfo->STSSOffset;
    FileOffset += 16; /* 16 bytes = 4 size + 4 AtomType + 4 Version-flag + 4 entry*/
    FileOffset += (pSyncData->FirstEntryIndex * 4);
    NVMM_CHK_ERR (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));
    NVMM_CHK_ERR (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) pSyncData->ReadBuffer, ReadCount * 4));

    for (i = 0; i < ReadCount; i++)
    {
        pSyncData->ReadBuffer[i] = NV_BE_TO_INT_32 ( (NvU8 *) & pSyncData->ReadBuffer[i]);
    }
    pSyncData->ValidCount = ReadCount;
    // TODO: Remove the recursion and implement a simple loop
    // do the search again
    return NvMp4ParserGetNextSyncUnit (pNvMp4Parser , Reference , Direction);

cleanup:
    return -1;
}

/*
1. Read the frame sizes directly from stsz entry.
2. Skip to required chunk and find the number of frames to skip in the chunk
3. Find the FileOffset within the chunk for the starting frame
4. While not done reading all the required frames
4a. find the chunk FileOffset from stco entry
4b. Get the frame FileOffset within the chunk
*/
static NvError
MP4PrepareFramingInfo (
    NvMp4Parser *pNvMp4Parser,
    const NvMp4TrackInformation *pTrackInfo,
    NvU32 BlockNum,
    NvMp4FramingInfo *pFramingInfo)
{
    NvError Status = NvSuccess;
    NvU8 *ReadBuffer;
    NvU32 ActualFramesInBlock = 0;
    NvU32 CurrentChunkIndex = 0, CurrentChunkNFrames = 0;
    NvU32 NextChunkIndex = 0, NextChunkNFrames = 0;
    NvU32 ChunkFramesToSkip = 0, CumFrameSize = 0;
    NvU64 *pChunkOffsetArray = NULL, ChunkArrayCount = 0;
    NvU32 ChunkIndexCount = 0;
    NvU32 CumNFramesCount = 0, ChunkFramesRead = 0;
    NvU64 i, FileOffset, ReadCount, STSCBytesRead;
    NvU32 Factor;
    CP_PIPETYPE *pPipe;

    NVMM_CHK_ARG (pNvMp4Parser && pTrackInfo && pFramingInfo);
//    CHKARG (block < pFramingInfo->totalNoBlocks); /* Some Tracks fail this sanity check */

    pPipe = &pNvMp4Parser->pPipe->cpipe;

    pFramingInfo->IsCo64bitOffsetPresent = pTrackInfo->IsCo64bitOffsetPresent;

    Factor = (pTrackInfo->IsCo64bitOffsetPresent) ? 2 : 1;

    ActualFramesInBlock = (BlockNum < pFramingInfo->TotalNoBlocks - 1) ?
                          (pFramingInfo->MaxFramesPerBlock) :
                          (pFramingInfo->TotalNoFrames % pFramingInfo->MaxFramesPerBlock);

    /******* STEP 1 ********/
    /***********************/
    /* read frame sizes */
    /* 20 bytes = 4 size + 4 AtomType + 4 Version-flag + 4 entry + 4 sample_size*/
    /* need to skip blocks according to loop */
    /* 4-bytes a block*/
    if (pTrackInfo->STSZSampleSize != 0)
    {
        // indicates all frames are of same size
        for (i = 0; i < ActualFramesInBlock; i++)
        {
            pFramingInfo->SizeTable[i] = pTrackInfo->STSZSampleSize;
        }
    }
    else
    {
        FileOffset = pTrackInfo->STSZOffset + 20 + sizeof (NvU32) * (BlockNum * pFramingInfo->MaxFramesPerBlock);
        NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));
        ReadCount = ActualFramesInBlock * sizeof (NvU32);
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) &pFramingInfo->SizeTable[0], (CPuint) ReadCount));
        for (i = 0; i < ActualFramesInBlock; i++)
        {
            pFramingInfo->SizeTable[i] = NV_BE_TO_INT_32 ( (NvU8 *) & pFramingInfo->SizeTable[i]);
        }
    }


    /******* STEP 2 ********/
    /***********************/
    /* read decoding and composition time stamp */
    /* 16 bytes = 4 size + 4 AtomType + 4 Version-flag + 4 entry count */
    /* need to skip blocks according to loop */
    /* 8-bytes a block*/

    // read STTS
    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "STSS : STTSEntries = %d\n", pTrackInfo->STTSEntries));
    FileOffset = pTrackInfo->STTSOffset + 16;
    NVMM_CHK_ERR (Mp4ReadBulkData (pNvMp4Parser, Mp4ReadBulkDataType_STTS, FileOffset, pTrackInfo->STTSEntries, sizeof (NvU32) *2, (NvU8 *) pFramingInfo, (NvU8 *) &BlockNum));


    //read CTTS
    if (pTrackInfo->CTTSOffset != (NvU64)-1)
    {
        /* 16 bytes = 4 size + 4 AtomType + 4 Version-flag + 4 entry count */
        FileOffset = pTrackInfo->CTTSOffset + 16 ;
        NVMM_CHK_ERR (Mp4ReadBulkData (pNvMp4Parser, Mp4ReadBulkDataType_CTTS, FileOffset, pTrackInfo->CTTSEntries, sizeof (NvU32) *2, (NvU8 *) pFramingInfo, (NvU8 *) &BlockNum));
    }

    /******* STEP 3 ********/
    /***********************/
    FileOffset = pTrackInfo->STSCOffset + 16;/* 16 bytes = 4 size + 4 AtomType + 4 Version-flag + 4 entry*/
    NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));

    // Read STSC data
    CumNFramesCount = 0;
    STSCBytesRead = 16;
    ReadBuffer = pNvMp4Parser->pTempBuffer;
    ReadCount = 12;
    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) ReadBuffer, (CPuint) ReadCount));
    STSCBytesRead += ReadCount;
    CurrentChunkIndex = NV_BE_TO_INT_32 (&ReadBuffer[0]);
    CurrentChunkNFrames = NV_BE_TO_INT_32 (&ReadBuffer[4]);

    for (i = 1; i < pTrackInfo->STSCEntries; i++)
    {
        NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) ReadBuffer, (CPuint) ReadCount));
        STSCBytesRead += ReadCount;
        NextChunkIndex = NV_BE_TO_INT_32 (&ReadBuffer[0]);
        NextChunkNFrames = NV_BE_TO_INT_32 (&ReadBuffer[4]);

        while (CurrentChunkIndex < NextChunkIndex)
        {
            if ( (CumNFramesCount + CurrentChunkNFrames) > (NvU32) BlockNum*pFramingInfo->MaxFramesPerBlock)
                break;

            CumNFramesCount += CurrentChunkNFrames;
            CurrentChunkIndex++;
        }

        if (CurrentChunkIndex < NextChunkIndex)
            break;

        CurrentChunkIndex = NextChunkIndex;
        CurrentChunkNFrames = NextChunkNFrames;
    }
        
    /* This  condition can  be true when the chunk index falls after the last STSC entry */
    if (i == pTrackInfo->STSCEntries)
    {
        ChunkIndexCount = (NvU32) (BlockNum * pFramingInfo->MaxFramesPerBlock-CumNFramesCount) / CurrentChunkNFrames;
        CurrentChunkIndex += ChunkIndexCount;
        CumNFramesCount += ChunkIndexCount*CurrentChunkNFrames;            
    }

    if (CurrentChunkIndex > pTrackInfo->STCOEntries) /* requested block is not in the file */
    {
        Status = NvError_ParserEndOfStream;
        goto cleanup;
    }

    /******* STEP 4 ********/
    /***********************/
    CumFrameSize = 0;
    ChunkFramesToSkip = BlockNum * pFramingInfo->MaxFramesPerBlock - CumNFramesCount;
    if (pTrackInfo->STSZSampleSize != 0)
    { /* if this entry is zero, it indicates all frames are of same size */
        CumFrameSize = pTrackInfo->STSZSampleSize * ChunkFramesToSkip;
    }
    else
    {
        /* 20 bytes = 4 size + 4 AtomType + 4 Version-flag + 4 entry + 4 sample_size + 4 bytes per frame*/
        NvU64 FileOffset = pTrackInfo->STSZOffset + 20 + sizeof (NvU32) * CumNFramesCount;
        NVMM_CHK_ERR (Mp4ReadBulkData (pNvMp4Parser, Mp4ReadBulkDataType_STSZ, FileOffset, ChunkFramesToSkip, sizeof (NvU32), (NvU8 *) &CumFrameSize, NULL));
    }

    /******* STEP 5 ********/
    /***********************/
    // In order to reduce file seek, we store the chunk offsets for later use
    if ( (pTrackInfo->STCOEntries - CurrentChunkIndex) < ( (NvU32) (pFramingInfo->MaxFramesPerBlock)))
    {
        ChunkArrayCount = pTrackInfo->STCOEntries - CurrentChunkIndex + 1;
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "stco-chunkIdx < NFRANMES: STCOEntries =%d - ChunkArrayCount = %lld\n", pTrackInfo->STCOEntries, ChunkArrayCount));
    }
    else
    {
        //allocate space for maximum number of chunk FileOffset values that can be requested in one call
        // worst sceneriao is when each chunk has one frame
        // hence max total number of chunk FileOffset required at that time would be NOVERnvMp4FramingInfo->nOverlapFramesLAPFRAMES + pFramingInfo->nFramesInBlock
        ChunkArrayCount = pFramingInfo->MaxFramesPerBlock + 1;
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "stco-chunkIdx > NFRANMES: ChunkArrayCount = %lld\n", ChunkArrayCount));
    }
    pChunkOffsetArray = (NvU64*) NvOsAlloc ( (size_t) (sizeof (NvU64) * ChunkArrayCount));     //Allocate space for max number of entires left in the chunk FileOffset table
    NVMM_CHK_MEM (pChunkOffsetArray);
    // Read Chunk Offset Array
    FileOffset =   pTrackInfo->STCOOffset + 16 + sizeof (NvU32) * Factor * (CurrentChunkIndex - 1); // we will seek wrt to CurrentChunkIndex.
    NVMM_CHK_ERR (Mp4ReadBulkData (pNvMp4Parser, Mp4ReadBulkDataType_STCO, FileOffset, ChunkArrayCount, sizeof (NvU32) *Factor, (NvU8 *) pChunkOffsetArray, NULL));

    // Populate the Offset Table
    FileOffset = pTrackInfo->STSCOffset + STSCBytesRead;
    NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));
    ReadBuffer = pNvMp4Parser->pTempBuffer;
    for (i = 0, ChunkIndexCount = 0, ChunkFramesRead = ChunkFramesToSkip; i < ActualFramesInBlock; i++, ChunkFramesRead++)
    {
        if (ChunkFramesRead >= CurrentChunkNFrames)
        {
            CurrentChunkIndex++;
            ChunkIndexCount++;
            if (ChunkIndexCount >= ChunkArrayCount)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ChunkIndexCount >= ChunkArrayCount\n"));
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ChunkIndexCount = %d - ChunkArrayCount = %lld\n", ChunkIndexCount, ChunkArrayCount));
                pFramingInfo->IsCorruptedFile = NV_TRUE;
                break;
            }
            if (CurrentChunkIndex == NextChunkIndex)
            {
                CurrentChunkNFrames = NextChunkNFrames;
                if (STSCBytesRead != pTrackInfo->STSCAtomSize)
                {
                    ReadCount = 12;
                    NVMM_CHK_CP (pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) ReadBuffer, (CPuint) ReadCount));
                    STSCBytesRead += ReadCount;
                    NextChunkIndex = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                    NextChunkNFrames = NV_BE_TO_INT_32 (&ReadBuffer[4]);
                }
            }
            ChunkFramesRead = 0;
            CumFrameSize = 0;
        }
        pFramingInfo->OffsetTable[i] = pChunkOffsetArray[ChunkIndexCount] + CumFrameSize;
        CumFrameSize += pFramingInfo->SizeTable[i];
    }

    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "NvMp4FramingInfo-IsCorruptedFile = %d\n", pFramingInfo->IsCorruptedFile));
    pFramingInfo->ValidFrameCount = (BlockNum * pFramingInfo->MaxFramesPerBlock) + (NvU32) i;
    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ValidFrameCount = %d - ActualFramesInBlock = %d\n", pFramingInfo->ValidFrameCount, ActualFramesInBlock));

    /******* STEP 6 ********/
    /***********************/
    /* store stsc, need "sample_desc_index" to switch between stsd entries
     * Although we store stsc for both audio and video tracks, switching is only
     * supported for AVC decoder currently. For other audio/video decoders, we
     * will have to talk to decoder owners about how to send/switch decoder
     * specific configuration. The mechanism for AVC decoder is described in
     * comments of bug#724650 */
    if(!pFramingInfo->IsValidStsc)
    {
        NvStscEntry *stsc = NULL;
        NvStscEntry *stsc_last = NULL;
        /* 16 bytes = 4 size + 4 AtomType + 4 Version-flag + 4 entry*/
        FileOffset = pTrackInfo->STSCOffset + 16;
        NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64(pNvMp4Parser->hContentPipe,
                (CPint64) FileOffset, CP_OriginBegin));

        // Read STSC data
        ReadBuffer = pNvMp4Parser->pTempBuffer;
        ReadCount = 12; /*12=size of a stsc entry*/
        for (i = 0; i < pTrackInfo->STSCEntries; i++)
        {
            stsc = (NvStscEntry*) NvOsAlloc((size_t)(sizeof(NvStscEntry)));
            NVMM_CHK_MEM (stsc);
            NvOsMemset(stsc, 0, sizeof(NvStscEntry));
            if(0 == i)
            {
                /*creating list head*/
                pFramingInfo->StscEntryListHead = stsc;
            }
            else
            {
                /*append to the last one*/
                stsc_last->next = stsc;
            }

            NVMM_CHK_CP (pPipe->Read(pNvMp4Parser->hContentPipe,
                    (CPbyte *) ReadBuffer, (CPuint) ReadCount));

            stsc->first_chunk = NV_BE_TO_INT_32 (&ReadBuffer[0]);
            stsc->samples_per_chunk = NV_BE_TO_INT_32 (&ReadBuffer[4]);
            stsc->sample_desc_index = NV_BE_TO_INT_32 (&ReadBuffer[8]);
            stsc_last = stsc;
        }
        pFramingInfo->IsValidStsc = NV_TRUE;
    }
cleanup:
    if (pChunkOffsetArray)
    {
        NvOsFree (pChunkOffsetArray);
        pChunkOffsetArray = NULL;
    }

    return Status;
}


static NvError
Mp4PrepareFramingInfoPLAY (
    NvMp4Parser* pNvMp4Parser,
    NvU32 FrameNumber,
    NvMp4TrackInformation *pTrackInfo,
    NvMp4FramingInfo *pNvMp4FramingInfo)
{
    NvError Status = NvSuccess;
    NvU32 BlockNum = 0;
    NvBool IsFrameValid = NV_FALSE;

    if (FrameNumber > pNvMp4FramingInfo->CurrentFrameIndex + 1) pNvMp4Parser->PlayState = NvMp4ParserPlayState_FF;
    else if (FrameNumber < pNvMp4FramingInfo->CurrentFrameIndex) pNvMp4Parser->PlayState = NvMp4ParserPlayState_REW;
    else pNvMp4Parser->PlayState = NvMp4ParserPlayState_PLAY;

    pNvMp4FramingInfo->CurrentFrameIndex = FrameNumber;

    NVMM_CHK_ARG (pNvMp4FramingInfo->MaxFramesPerBlock);
    BlockNum = FrameNumber / pNvMp4FramingInfo->MaxFramesPerBlock;

    switch (pNvMp4Parser->PlayState)
    {
        case NvMp4ParserPlayState_PLAY:
            if (FrameNumber >= ( (pNvMp4FramingInfo->CurrentBlock + 1) *pNvMp4FramingInfo->MaxFramesPerBlock))
            {
                NVMM_CHK_ARG (BlockNum == (pNvMp4FramingInfo->CurrentBlock + 1));  /* In play mode, if a block is requested, it should be the next block */
                IsFrameValid = NV_TRUE;
            }
            break;
        case NvMp4ParserPlayState_REW:
            if (FrameNumber < pNvMp4FramingInfo->CurrentBlock*pNvMp4FramingInfo->MaxFramesPerBlock)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "REW PrepFramingInfo Start- \n"));
                IsFrameValid = NV_TRUE;
            }
            break;
        case NvMp4ParserPlayState_FF:
            if (FrameNumber >= ( (pNvMp4FramingInfo->CurrentBlock + 1) *pNvMp4FramingInfo->MaxFramesPerBlock))
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "FF PrepFramingInfo Start- \n"));
                IsFrameValid = NV_TRUE;
            }
            break;
        default:
            Status = NvError_ParserFailure;
            break;
    }

    if (IsFrameValid == NV_TRUE)
    {
        NVMM_CHK_ERR (MP4PrepareFramingInfo (pNvMp4Parser,  pTrackInfo,  BlockNum,  pNvMp4FramingInfo));
        pNvMp4FramingInfo->CurrentBlock = BlockNum;
    }
cleanup:
    return Status;
}


static NvError
Mp4ReadMultipleFrames (
    NvMp4Parser * pNvMp4Parser,
    NvU32 TrackIndex,
    NvMMBuffer* pOffsetListBuffer,
    NvU32 VBase,
    NvU32 PBase)
{
    NvError Status = NvSuccess;
    CP_PIPETYPE *pPipe;
    NvMp4FramingInfo *pNvMp4FramingInfo = NULL;
    NvMp4TrackInformation *pTrackInfo = NULL;
    NvU32 FrameNumber = 0;
    NvU32 i = 0, CurrentFrameIndex = 0;
    NvU64 MediaTimeScale = 0;
    NvU64 CurrentFrameOffset = 0;
    NvU64 CurrentFilePosition = 0;
    NvU32 CurrentFrameSize = 0, ReadBytes = 0, Count = 0;
    NvU32 k = 0, CurrentFrameCount = 0;
    CPbyte* pFrameBuffer = NULL;
    CPuint64 SizeOfBytesAvailable = 0;
    NvU32 MaxNoFrames = 0, NumContinuosFrames = 0;
    NvU32 CurrentFrameMaxCount = 0;
    NvS32 cts = 0;
    NvU64 dts = 0, ts = 0;
    NvU32 CurrentAccumulatedSize = 0;
    NvMMPayloadMetadata Payload;
    NvU32 MaxOffsetListCount = 0;
    NvU64 TimeStamp = 0;
    NvBool IsVideoStream = NV_FALSE;
    NvU32 TotalSizePerOffsetList = 0;
    CP_CHECKBYTESRESULTTYPE CPResult = CP_CheckBytesOk;
    NvU32 MaxSize = 0;

    NVMM_CHK_ARG (pNvMp4Parser && pOffsetListBuffer);

    pPipe = &pNvMp4Parser->pPipe->cpipe;

    pNvMp4FramingInfo = &pNvMp4Parser->FramingInfo[TrackIndex];
    pTrackInfo = &pNvMp4Parser->TrackInfo[TrackIndex];
    MediaTimeScale =  pTrackInfo->TimeScale;
    FrameNumber = pNvMp4FramingInfo->FrameCounter;
    MaxSize = pNvMp4FramingInfo->MaxFrameSize;

    if (FrameNumber >=  pNvMp4FramingInfo->TotalNoFrames)
    {
        pTrackInfo->EndOfStream = NV_TRUE;
        Status = NvError_ParserEndOfStream;
        goto cleanup;
    }

    switch (pNvMp4Parser->TrackInfo[TrackIndex].TrackType)
    {
        case NvMp4TrackTypes_AAC:
        case NvMp4TrackTypes_AACSBR:
            if (pNvMp4Parser->VideoIndex != MP4_INVALID_NUMBER)
                MaxOffsetListCount  = 256;
            else
            {
                switch (FrameNumber)
                {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                        MaxOffsetListCount  = 8;
                        pNvMp4FramingInfo->SkipFrameCount = 0;
                        break;
                    case 4:
                        MaxOffsetListCount  = 16;
                        break;
                    case 5:
                        MaxOffsetListCount  = 32;
                        break;
                    case 6:
                        MaxOffsetListCount  = 64;
                        break;
                    case 7:
                        MaxOffsetListCount  = 128;
                        break;
                    case 8:
                        MaxOffsetListCount  = 256;
                        break;
                    default:
                        MaxOffsetListCount  = MAX_OFFSETS;
                }
            }
            break;

        case NvMp4TrackTypes_MPEG4:
        case NvMp4TrackTypes_S263:
        case NvMp4TrackTypes_AVC:
        case NvMp4TrackTypes_MJPEGA:
        case NvMp4TrackTypes_MJPEGB:
            MaxOffsetListCount  = 256;
            IsVideoStream = NV_TRUE;
            break;

        default:
            return NvError_ParserFailure;
    }


    NVMM_CHK_ERR (Mp4PrepareFramingInfoPLAY (pNvMp4Parser, FrameNumber, pTrackInfo, pNvMp4FramingInfo))

    /*     For ex: Calculate the number of frames we can get from 64K
    *     contigous data.
    *
    *     Case 1:
    *    ----------------------------------------------------------
    *    |   F1  |        |  F2  |      | F3   |        |          F4              |
    *    ----------------------------------------------------------
    *    <----------------------------------->
    *                                   64K
    *    In the above example, in 64K contigous chunk we take F1, F2, F3.
    *    F4 is not considered as the F4 crosses the 64K boundary.
    *
    *     Case 2:
    *    ----------------------------------------------------------
    *    |   F3  |        |  F1  |      | F2   |        |      F4       |
    *    ----------------------------------------------------------
    *                        <----------------------------------->
    *                                                                64K
    *    In the above example, in 64K contigous chunk we take F1, F2 only.
    *    F4 is not considered as the F4 crosses the 64K boundary. F3 is also not
    *    considered as F3 comes before F1.
    *
    */

    // Calculates the max frames present in the block
    if ( (pNvMp4FramingInfo->CurrentBlock + 1) *pNvMp4FramingInfo->MaxFramesPerBlock > pNvMp4FramingInfo->TotalNoFrames)
    {
        MaxNoFrames = pNvMp4FramingInfo->TotalNoFrames;
    }
    else
    {
        MaxNoFrames = (pNvMp4FramingInfo->CurrentBlock + 1) * pNvMp4FramingInfo->MaxFramesPerBlock;
    }
    CurrentFrameMaxCount = MaxNoFrames - pNvMp4FramingInfo->CurrentBlock * pNvMp4FramingInfo->MaxFramesPerBlock;

    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "rate= %d\n", pNvMp4Parser->rate));
    if ( (pNvMp4Parser->rate > 2000) || (pNvMp4Parser->rate < 0)) MaxOffsetListCount = 1;

    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "\n\n"));
    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "- Start Offset List Preparation - \n"));

    //Calculating number of frames within 64K
    while ( (FrameNumber <  MaxNoFrames) && (CurrentFrameCount < MaxOffsetListCount) && (TotalSizePerOffsetList < ReadBufferSize_2MB))
    {
        pFrameBuffer = NULL;
        CurrentFrameIndex = FrameNumber % pNvMp4FramingInfo->MaxFramesPerBlock;
        CurrentFrameOffset = pNvMp4FramingInfo->OffsetTable[CurrentFrameIndex];
        CurrentFrameSize = pNvMp4FramingInfo->SizeTable[CurrentFrameIndex];
        if (CurrentFrameOffset == 0 || CurrentFrameOffset > pNvMp4Parser->FileSize || CurrentFrameSize > pNvMp4FramingInfo->MaxFrameSize)
        {
            FrameNumber++;
            CurrentFrameIndex++;
            continue;
        }

        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "OuterLoop: FrameNumber = %d - CurrentFrameIndex = %d\n", FrameNumber, CurrentFrameIndex));
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "OuterLoop: Size = %d - FileOffset = %d\n", CurrentFrameSize, CurrentFrameOffset));
        //Seek to the current FileOffset in the file
        NVMM_CHK_ERR (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) CurrentFrameOffset, CP_OriginBegin));
        CurrentFilePosition = CurrentFrameOffset;
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Set Offsets EQ: CurrentFilePosition = %d - CurrentFrameOffset = %ld\n", CurrentFilePosition, CurrentFrameOffset));

        for (i = CurrentFrameIndex,
                k = CurrentFrameCount,
                CurrentAccumulatedSize = 0,
                NumContinuosFrames = 0; (i < CurrentFrameMaxCount) &&
                (CurrentAccumulatedSize + pNvMp4FramingInfo->SizeTable[i] < ReadBufferSize_256KB) &&
                (k <  MaxOffsetListCount) &&
                (FrameNumber + NumContinuosFrames < MaxNoFrames) &&
                (CurrentFrameOffset == pNvMp4FramingInfo->OffsetTable[i]);
                i++, k++, NumContinuosFrames++)
        {

            if (pNvMp4FramingInfo->OffsetTable[i] == 0 ||
                    pNvMp4FramingInfo->OffsetTable[i] > pNvMp4Parser->FileSize ||
                    pNvMp4FramingInfo->SizeTable[i] > pNvMp4FramingInfo->MaxFrameSize)
            {
                pNvMp4FramingInfo->SkipFrameCount++;
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Continue2- CurrentFrameSize > MaxSize\n"));
                if ( (pNvMp4FramingInfo->SkipFrameCount > 50) || (CurrentFrameOffset > pNvMp4Parser->FileSize))
                {
                    FrameNumber = pNvMp4Parser->FramingInfo[pNvMp4Parser->AudioIndex].FrameCounter = pNvMp4FramingInfo->TotalNoFrames;
                    Status = NvError_ParserEndOfStream;
                    goto cleanup;
                }
                continue;
            }
            CurrentAccumulatedSize += pNvMp4FramingInfo->SizeTable[i];
            CurrentFrameOffset += pNvMp4FramingInfo->SizeTable[i];
        }

        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "NumContinuosFrames = %d - CurrentAccumulatedSize = %d\n", NumContinuosFrames, CurrentAccumulatedSize));
        ReadBytes = CurrentAccumulatedSize;

        NVMM_CHK_ARG (CurrentAccumulatedSize < MAX_MP4_SPAREAREA_SIZE);

        // Check if requested size is available in the file.
        Status = pPipe->CheckAvailableBytes (pNvMp4Parser->hContentPipe, CurrentAccumulatedSize, &CPResult);
        // Check if we reached end of stream.
        if (CPResult == CP_CheckBytesAtEndOfStream)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "CP_CheckBytesAtEndOfStream\n"));
            Status = NvError_ParserEndOfStream;
            goto cleanup;
        }
        else if (CPResult == CP_CheckBytesNotReady)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "CP_CheckBytesNotReady\n"));
            Status = NvError_ContentPipeNotReady;
            goto cleanup;
        }
        else if (CPResult == CP_CheckBytesInsufficientBytes)
        {
            pNvMp4Parser->pPipe->GetAvailableBytes (pNvMp4Parser->hContentPipe, &SizeOfBytesAvailable);
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "CP_CheckBytesInsufficientBytes: REQ = %d --Avail = %d\n", CurrentFrameSize, SizeOfBytesAvailable));
            //Since this is partial frame. Decoder may not pNvMp4ReaderHandle it. So ReadBuffer
            //Not issued on this. Send so far packed valid lists count to decoder.

            //ReadBytes = CurrentFrameSize = (NvU32)SizeOfBytesAvailable;
            if (SizeOfBytesAvailable + CurrentFilePosition == pNvMp4Parser->FileSize)
            {
                CurrentAccumulatedSize = (NvU32) SizeOfBytesAvailable;
                ReadBytes = CurrentAccumulatedSize;
            }
            else
            {
                Status = NvError_InsufficientMemory;
                goto cleanup;
            }
        }
        else if (CPResult == CP_CheckBytesOutOfBuffers)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "CP_CheckBytesOutOfBuffers\n"));
            Status = NvError_ContentPipeNoFreeBuffers;
            goto cleanup;
        }
        else if (CPResult != CP_CheckBytesOk)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "CPResult != CP_CheckBytesOk\n"));
            Status = NvError_ContentPipeNotReady;
            goto cleanup;
        }

        Status = pPipe->ReadBuffer (pNvMp4Parser->hContentPipe, &pFrameBuffer, (CPuint*) & CurrentAccumulatedSize, 0);
        CurrentFilePosition += CurrentAccumulatedSize;

        if (CurrentAccumulatedSize != ReadBytes)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Size MisMatch- partial frame: CurrentFrameSize != ReadBytes\n"));
            pPipe->ReleaseReadBuffer (pNvMp4Parser->hContentPipe, pFrameBuffer);
            //setting back the the currentFilePosition.
            CurrentFilePosition -= CurrentAccumulatedSize;
            Status  = NvError_InsufficientMemory;
            goto cleanup;
        }

        if (Status != NvSuccess)
        {
            Status  = NvError_ParserFailure;
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ReadBuffer Status ! = NvSuccess\n"));
            if ( (Status == NvError_NotInitialized) || (Status == NvError_EndOfFile))
            {
                Status = NvError_ParserEndOfStream;
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "ReadBuffer EOS\n"));

                pNvMp4Parser->TrackInfo[TrackIndex].EndOfStream = NV_TRUE;
                if ( (Status == NvError_EndOfFile) && NumContinuosFrames)
                {
                    goto mp4PushOffsetsLoop;
                }
            }
            CurrentFilePosition -= CurrentAccumulatedSize;
            goto cleanup;
        }

mp4PushOffsetsLoop:

        //Status = NvmmSetReadBuffBaseAddress(pOffsetListBuffer, pFrameBuffer);
        for (i = 0; i < NumContinuosFrames; i++)
        {
            // Setting Base Adress
            Status = NvmmSetReadBuffBaseAddress (pOffsetListBuffer, pFrameBuffer);
            dts = pNvMp4FramingInfo->DTSTable[CurrentFrameIndex];
            cts = pNvMp4FramingInfo->CTSTable[CurrentFrameIndex];
            CurrentFrameSize = pNvMp4FramingInfo->SizeTable[CurrentFrameIndex];
            CurrentFrameOffset = pNvMp4FramingInfo->OffsetTable[CurrentFrameIndex];
            if (CurrentFrameSize > MaxSize || CurrentFrameOffset <= 0 || CurrentFrameOffset > pNvMp4Parser->FileSize)
            {
                FrameNumber++;
                CurrentFrameIndex++;
                CurrentFrameCount ++;
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Continue- CurrentFrameSize > MaxSize\n"));
                continue;
            }
            ts = cts + dts;
            if (MediaTimeScale != 0)
                Payload.TimeStamp = ( ( (NvU64) ts * TICKS_PER_SECOND) / MediaTimeScale) * 10;
            else
                Payload.TimeStamp = 0;
            Payload.BufferFlags = 0;

            //Mark _last_ N audio buffers to enable silence detection by decoder
            if (FrameNumber >= (pNvMp4FramingInfo->TotalNoFrames - N_LAST_AUDIOBUFFERS))
                Payload.BufferFlags |= NvMMBufferFlag_Gapless;

            if (i == 0)
            {
                Payload.BufferFlags |= NvMMBufferFlag_CPBuffer;
            }
            if (pNvMp4Parser->IsSINF)
            {
                if (pNvMp4Parser->DrmInterface.pNvDrmDecrypt)
                {
                    Status = pNvMp4Parser->DrmInterface.pNvDrmDecrypt (pNvMp4Parser->pDrmContext, (NvU8 *) pFrameBuffer, CurrentFrameSize);
                    if (NvError_Success != Status)
                    {
                        pNvMp4Parser->DrmError = Status;
                        return NvError_ParserDRMFailure;
                    }
                }
            }
            NvmmPushToOffsetList (pOffsetListBuffer, (NvU32) pFrameBuffer, CurrentFrameSize, Payload);
            pFrameBuffer += CurrentFrameSize;
            TotalSizePerOffsetList += CurrentFrameSize;
            CurrentFrameIndex++;
            CurrentFrameCount ++;
            FrameNumber ++;
            if (FrameNumber >=  pNvMp4FramingInfo->TotalNoFrames)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "FrameNumber >= pNvMp4FramingInfo->TotalNoFrames\n"));
                break;
            }
        }
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "End: FrameNumber = %d - i = %d - CurrentFrameIndex = %d\n",
                            FrameNumber, i , CurrentFrameIndex));
    };

    if ( ( (pNvMp4Parser->rate > 2000) || (pNvMp4Parser->rate < 0)) && IsVideoStream)
    {
        if (MediaTimeScale != 0)
            TimeStamp = ( ( (NvU64) ts * TICKS_PER_SECOND) / MediaTimeScale) * 10;
        else
            TimeStamp = 0;

        pNvMp4Parser->FramingInfo[pNvMp4Parser->AudioIndex].FrameCounter = (NvU32) (pNvMp4Parser->AudioFPS * ( (TimeStamp) / 10000000));
    }

cleanup:
    if (Status == NvError_EndOfFile)
        Status = NvError_ParserEndOfStream;

    pNvMp4Parser->FramingInfo[TrackIndex].FrameCounter = FrameNumber;
    pNvMp4FramingInfo->CurrentFrameIndex = FrameNumber;
    NvmmGetNumOfValidOffsets (pOffsetListBuffer, &Count);
    if (Count > 0)
    {
        NvmmSetOffsetListStatus (pOffsetListBuffer, NV_TRUE);
    }
    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Status = %d -- -- Count = %d\n", Status, Count));

    return Status;
}


NvError
NvMp4ParserGetMultipleFrames (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvMMBuffer*pOffsetListBuffer ,
    NvU32 VBase,
    NvU32 PBase)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));

    if (NVMP4_ISTRACKVIDEO (pNvMp4Parser->TrackInfo[TrackIndex].TrackType))
    {
        if ( (pNvMp4Parser->rate > 2000) || (pNvMp4Parser->rate < 0))
        {
            NVMM_CHK_ERR (NvMp4ParserSetRate (pNvMp4Parser));
        }
    }

    Status = Mp4ReadMultipleFrames (pNvMp4Parser, TrackIndex,  pOffsetListBuffer , VBase, PBase);

cleanup:
    return Status;
}

static NvError
Mp4ReadAudioStreamingData (
    NvMp4Parser * pNvMp4Parser,
    NvU32 TrackIndex,
    void *ReadBuffer,
    NvU32 * pMaxFrameBytes,
    NvU64 *pDTS,
    NvS32 *pCTS,
    NvU64 *pELST)
{
    NvMp4FramingInfo *pNvMp4FramingInfo = NULL;
    NvMp4TrackInformation *pTrackInfo = NULL;
    NvU32 CurrentFrameIndex = 0, MaxNoFrames = 0, NumContinuosFrames = 0;
    NvU32 ReadCount = 0, CurrentAccumulatedSize = 0, CurrentFrameSize = 0;
    NvU32 i, j = 0;
    NvS64 DeltaOffset = 0;
    NvU64 FileOffset = 0, CurrentFrameOffset = 0, CurrentFilePosition = 0;
    CP_PIPETYPE *pPipe;
    NvError Status = NvSuccess;
    NvU32 RequestedBytes = *pMaxFrameBytes;
    NvU8 *pMemBufferptr = &pNvMp4Parser->pTempBuffer[0];

    *pMaxFrameBytes = 0;
    pPipe = &pNvMp4Parser->pPipe->cpipe;

    pNvMp4FramingInfo = &pNvMp4Parser->FramingInfo[TrackIndex];
    pTrackInfo = &pNvMp4Parser->TrackInfo[TrackIndex];

    if (pNvMp4FramingInfo->FrameCounter >=  pNvMp4FramingInfo->TotalNoFrames)
    {
        pTrackInfo->EndOfStream = NV_TRUE;
        return (NvError_ParserEndOfStream);
    }


    if (pNvMp4FramingInfo->FrameCounter == 0)
    {
        pNvMp4Parser->IsFirstFrame = NV_TRUE;
        if (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, 0, CP_OriginBegin) != NvSuccess)
        {
            return NvError_ParserFailure;
        }
    }

    Mp4PrepareFramingInfoPLAY (pNvMp4Parser, pNvMp4FramingInfo->FrameCounter, pTrackInfo, pNvMp4FramingInfo);

    CurrentAccumulatedSize = 0;
    NumContinuosFrames = 0;
    //Get the current frame number.
    CurrentFrameIndex = pNvMp4FramingInfo->FrameCounter - (pNvMp4FramingInfo->CurrentBlock * pNvMp4FramingInfo->MaxFramesPerBlock);
    // Calculates the max frames present in the block
    MaxNoFrames = (pNvMp4FramingInfo->CurrentBlock + 1) * pNvMp4FramingInfo->MaxFramesPerBlock;
    FileOffset = pNvMp4FramingInfo->OffsetTable[CurrentFrameIndex];
    if (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64*) &CurrentFilePosition) != NvSuccess)
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO GetPosition64 Failure\n"));
        *pMaxFrameBytes = 0;
        return NvError_ParserFailure;
    }
    ReadCount = *pMaxFrameBytes = pNvMp4FramingInfo->SizeTable[CurrentFrameIndex];
    if (*pMaxFrameBytes > RequestedBytes)
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO: MAX BYTES GT REQ\n"));
        *pMaxFrameBytes = 0;
        return NvSuccess;
    }
    if ( (FileOffset > pNvMp4Parser->CurrentFrameOffset) || (FileOffset < pNvMp4Parser->PreviousFrameOffset) || (pNvMp4Parser->IsFirstFrame))
    {
        pNvMp4Parser->PreviousFrameOffset = FileOffset;
        NvOsMemset (pMemBufferptr, 0, ReadBufferSize_256KB);
        if ( (CurrentFilePosition != FileOffset) || (pNvMp4Parser->IsFirstFrame))
        {
            DeltaOffset = (NvS64) (FileOffset - CurrentFilePosition);
            if (CurrentFilePosition == pNvMp4Parser->FileSize)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "CFilePos == FileSize:CFP = %lld", CurrentFilePosition));
                Status = pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin);
            }
            else
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_VERBOSE, "SEEK Issued:Delta_offset = %d\n", DeltaOffset));
                Status = pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) DeltaOffset, CP_OriginCur);
            }
            if (Status != NvSuccess)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4:AUDIO SetPosition64 Failure: Status = %d\n", Status));
                if (Status == NvError_FileOperationFailed)
                { 
                    Status = NvError_ParserEndOfStream;
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO SetPosition64 Failure - FILE_OPERATION_FAILED\n"));
                    return Status;
                }
                else if (Status == NvError_EndOfFile)
                {
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO SetPosition64 Failure - NvError_EndOfFile\n"));
                    return NvError_ParserEndOfStream;
                }
                else if (Status == NvError_ContentPipeNoData)
                {
                    *pMaxFrameBytes = 0;
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO SetPosition64 Failure - NvError_ContentPipeNoData\n"));
                    return NvSuccess;
                }
                else
                {
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO SetPosition64 Failure - OtherError\n"));
                    *pMaxFrameBytes = 0;
                    return Status;
                }
            }
            CurrentFilePosition = CurrentFrameOffset = FileOffset;
            pNvMp4Parser->IsFirstFrame = NV_FALSE;
        }
        i =  CurrentFrameIndex;
        j = pNvMp4FramingInfo->FrameCounter;
        CurrentFilePosition = CurrentFrameOffset = FileOffset;
        CurrentFrameSize = ReadCount;
        while ( (CurrentAccumulatedSize < ReadBufferSize_256KB) && (j < MaxNoFrames))
        {
            if (CurrentFrameOffset != CurrentFilePosition)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_VERBOSE, "CurrentFrameOffset != CurrentFrameOffset: pNvMp4FramingInfo->FrameCounter = %d\n", pNvMp4FramingInfo->FrameCounter));
                break;
            }
            else
            {
                NumContinuosFrames++;
                CurrentAccumulatedSize += CurrentFrameSize;
            }
            i++;
            j++;
            pNvMp4Parser->CurrentFrameOffset = CurrentFrameOffset;
            CurrentFilePosition += CurrentFrameSize;
            CurrentFrameOffset = pNvMp4FramingInfo->OffsetTable[i];
            CurrentFrameSize = pNvMp4FramingInfo->SizeTable[i];
            if ( ( (pNvMp4FramingInfo->FrameCounter + NumContinuosFrames) >=  pNvMp4FramingInfo->TotalNoFrames))
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_VERBOSE, " (pNvMp4FramingInfo->FrameCounter+NumContinuosFrames) >= pNvMp4FramingInfo->TotalNoFrames\n"));
                break;
            }
            else if ( (CurrentAccumulatedSize + CurrentFrameSize) >= ReadBufferSize_256KB)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_VERBOSE, " Accumulated Size > ReadBufferSize_4KB\n"));
                break;
            }
        };
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_VERBOSE, "FileOffset = %lld - prevOffset = %lld - CurrentFrameOffset = %lld\n", FileOffset,
                            pNvMp4Parser->PreviousFrameOffset, pNvMp4Parser->CurrentFrameOffset));
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_VERBOSE, "pNvMp4FramingInfo->FrameCounter = %d - NumContinuosFrames = %d - CurrentAccumulatedSize = %d\n",
                            pNvMp4FramingInfo->FrameCounter , NumContinuosFrames, CurrentAccumulatedSize));
        Status = pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) pMemBufferptr, CurrentAccumulatedSize);
        if (Status != NvSuccess)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO Read Err: Status = %d\n", Status));
            if (Status == NvError_EndOfFile)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO Read NvError_EndOfFile\n"));
                return NvError_ParserEndOfStream;
            }
            else
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: AUDIO Read NvError_Other\n"));
                *pMaxFrameBytes = 0;
                return NvError_ParserFailure;
            }
        }
        NvOsMemcpy (ReadBuffer, (NvU8*) pMemBufferptr, ReadCount);
    }
    else
    {
        NvOsMemcpy (ReadBuffer, (NvU8*) pMemBufferptr + (FileOffset - pNvMp4Parser->PreviousFrameOffset), ReadCount);
    }

    (*pDTS) = pNvMp4FramingInfo->DTSTable[CurrentFrameIndex];
    (*pCTS) = pNvMp4FramingInfo->CTSTable[CurrentFrameIndex];
    if (pTrackInfo->IsELSTPresent)
    {
        if (pTrackInfo->pELSTMediaTime[0] == -1)
            (*pELST) = pTrackInfo->SegmentTable[0];
        else
            (*pELST) = 0;
    }

    return (NvSuccess);
}

static NvError
Mp4ReadData (
    NvMp4Parser * pNvMp4Parser,
    NvU32 TrackIndex,
    void * ReadBuffer,
    NvU32 * pMaxFrameBytes,
    NvU64 *pDTS,
    NvS32 *pCTS,
    NvU64 *pELST)
{
    NvError Status = NvSuccess;
    NvMp4FramingInfo *pNvMp4FramingInfo = NULL;
    NvMp4TrackInformation *pTrackInfo = NULL;
    NvU64 FileOffset = 0;
    NvU64 CurrentFilePosition = 0;
    NvU32 ReadCount = 0;
    NvU64 TempBufferOffset = 0;
    CP_PIPETYPE *pPipe;
    NvU32 RequestedBytes = 0;

    NVMM_CHK_ARG (pNvMp4Parser && pMaxFrameBytes);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));
    NVMM_CHK_ARG (pNvMp4Parser->TrackInfo[TrackIndex].TrackType != NvMp4TrackTypes_OTHER);

    pNvMp4FramingInfo = &pNvMp4Parser->FramingInfo[TrackIndex];
    pTrackInfo = &pNvMp4Parser->TrackInfo[TrackIndex];

    RequestedBytes = *pMaxFrameBytes;
    *pMaxFrameBytes = 0;
    pPipe = &pNvMp4Parser->pPipe->cpipe;

    if (pNvMp4FramingInfo->FrameCounter >=  pNvMp4FramingInfo->TotalNoFrames ||
            (pNvMp4FramingInfo->IsCorruptedFile && pNvMp4FramingInfo->FrameCounter >= pNvMp4FramingInfo->ValidFrameCount))
    {
        pTrackInfo->EndOfStream = NV_TRUE;
        return (NvError_ParserEndOfStream);
    }

    NVMM_CHK_ERR (Mp4PrepareFramingInfoPLAY (pNvMp4Parser, pNvMp4FramingInfo->FrameCounter, pTrackInfo, pNvMp4FramingInfo));

    TempBufferOffset  = pNvMp4FramingInfo->OffsetTable[pNvMp4FramingInfo->FrameCounter - (pNvMp4FramingInfo->CurrentBlock*pNvMp4FramingInfo->MaxFramesPerBlock) ];
    (*pMaxFrameBytes) = pNvMp4FramingInfo->SizeTable[pNvMp4FramingInfo->FrameCounter - (pNvMp4FramingInfo->CurrentBlock*pNvMp4FramingInfo->MaxFramesPerBlock) ];
    (*pDTS) = pNvMp4FramingInfo->DTSTable[pNvMp4FramingInfo->FrameCounter - (pNvMp4FramingInfo->CurrentBlock*pNvMp4FramingInfo->MaxFramesPerBlock) ];
    (*pCTS) = pNvMp4FramingInfo->CTSTable[pNvMp4FramingInfo->FrameCounter - (pNvMp4FramingInfo->CurrentBlock*pNvMp4FramingInfo->MaxFramesPerBlock) ];
    if (pTrackInfo->IsELSTPresent)
    {
        if (pTrackInfo->pELSTMediaTime && (pTrackInfo->pELSTMediaTime[0] == -1))
            (*pELST) = pTrackInfo->SegmentTable[0];
        else
            (*pELST) = 0;
    }

    FileOffset = TempBufferOffset;

    if ( (*pMaxFrameBytes > RequestedBytes) || (FileOffset > pNvMp4Parser->FileSize))
    {
        pTrackInfo->SyncFrameSize = *pMaxFrameBytes;
        if (RequestedBytes != 0)
        {
            pNvMp4FramingInfo->SkipFrameCount++;
        }
        *pMaxFrameBytes = 0;
        if ( (pNvMp4FramingInfo->SkipFrameCount > 30) || (FileOffset > pNvMp4Parser->FileSize && RequestedBytes != 0))
        {
            pNvMp4FramingInfo->FrameCounter = pNvMp4FramingInfo->TotalNoFrames;
            return NvError_ParserEndOfStream;
        }
        //Returning NvMp4ParserStatus_NOERROR as INSUFFECIENT_MEMORY error is not pNvMp4ReaderHandled outside parser yet
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Mp4_INSUFFECIENT_MEMORY_\n"));
        return NvSuccess;
    }

    if (pNvMp4Parser->pPipe->GetPosition64 (pNvMp4Parser->hContentPipe, (CPuint64*) &CurrentFilePosition) != NvSuccess)
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Mp4: GetPosition64 err:CurrentFilePosition = %lld\n", CurrentFilePosition));
        *pMaxFrameBytes = 0;
        return NvError_ParserFailure;
    }

    if (NVMP4_ISTRACKVIDEO (pTrackInfo->TrackType))
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_VERBOSE, "\n\nMP4:vfc = %d - VideoRead: Size = %d - FileOffset = %lld", pNvMp4FramingInfo->FrameCounter, *pMaxFrameBytes, FileOffset));
    else if (NVMP4_ISTRACKAUDIO (pTrackInfo->TrackType))
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_VERBOSE, "\n\nMP4:afc = %d - AudioRead: Size = %d - FileOffset = %lld", pNvMp4FramingInfo->FrameCounter, *pMaxFrameBytes, FileOffset));

    if (CurrentFilePosition != FileOffset)
    {
        Status = pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin);
        if (Status != NvSuccess)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: SetPosition64 err: Status = %d - curPos = %lld - FileOffset = %lld\n", Status, CurrentFilePosition, FileOffset));
            if (Status == NvError_FileOperationFailed)
            {
                Status = NvError_ParserEndOfStream;
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Mp4 SetPos NvError_FileOperationFailed err\n"));
                return  Status;
            }
            else if (Status == NvError_EndOfFile)
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Mp4 set FilePos EndOfFile\n"));
                return NvError_ParserEndOfStream;
            }
            else
            {
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Mp4 SetPosition64 Other err\n"));
                *pMaxFrameBytes = 0;
                return Status;
            }
        }
    }
    ReadCount = *pMaxFrameBytes;
    if (ReadCount)
        Status = pPipe->Read (pNvMp4Parser->hContentPipe, (CPbyte *) ReadBuffer, ReadCount);
    if (Status != NvSuccess)
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: Read err: ReadCount = %d\n", ReadCount));
        if (Status == NvError_EndOfFile)
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Mp4: Read NvError_EndOfFile\n"));
            pNvMp4FramingInfo->FrameCounter = pNvMp4FramingInfo->TotalNoFrames;
            return NvError_ParserEndOfStream;
        }
        else
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MP4: Read NvError_Other"));
            *pMaxFrameBytes = 0;
            return NvError_ParserFailure;
        }
    }

    if (pNvMp4Parser->IsSINF)
    {
        if (pNvMp4Parser->DrmInterface.pNvDrmDecrypt)
        {
            Status = pNvMp4Parser->DrmInterface.pNvDrmDecrypt (pNvMp4Parser->pDrmContext, (NvU8 *) ReadBuffer, ReadCount);
            if (NvError_Success != Status)
            {
                pNvMp4Parser->DrmError = Status;
                return NvError_ParserDRMFailure;
            }
        }
    }

cleanup:
    return (NvSuccess);
}

NvError
NvMp4ParserGetNextAccessUnit (
    NvMp4Parser * pNvMp4Parser,
    NvU32 TrackIndex ,
    void *pBuffer ,
    NvU32 *pFrameSize,
    NvU64 *pDTS,
    NvS32 *pCTS,
    NvU64 *pELST)
{
    NvError Status = NvSuccess;
    NvU64 ts = 0;
    NvU64 MediaTimeScale = 0;
    NvU64 TimeStamp = 0;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));
    NVMM_CHK_ARG (pNvMp4Parser->TrackInfo[TrackIndex].TrackType != NvMp4TrackTypes_OTHER);

    if (NVMP4_ISTRACKAUDIO (pNvMp4Parser->TrackInfo[TrackIndex].TrackType) && (TrackIndex == pNvMp4Parser->AudioIndex))
    {
        if (pNvMp4Parser->IsStreaming)
        {
            Status = Mp4ReadAudioStreamingData (pNvMp4Parser, TrackIndex, pBuffer , pFrameSize, pDTS,
                                                pCTS, pELST);
        }
        else
        {
            Status = Mp4ReadData (pNvMp4Parser, TrackIndex, pBuffer , pFrameSize, pDTS,
                                  pCTS, pELST);
        }

        pNvMp4Parser->FramingInfo[TrackIndex].FrameCounter++;
    }
    else if (NVMP4_ISTRACKVIDEO (pNvMp4Parser->TrackInfo[TrackIndex].TrackType) && (TrackIndex == pNvMp4Parser->VideoIndex))
    {
        if ( ( (pNvMp4Parser->rate > 2000) || (pNvMp4Parser->rate < 0)) &&
                (pNvMp4Parser->SeekVideoFrameCounter != pNvMp4Parser->FramingInfo[TrackIndex].FrameCounter))
        {
            Status = NvMp4ParserSetRate (pNvMp4Parser);
        }

        if (pNvMp4Parser->SeekVideoFrameCounter != 0) pNvMp4Parser->SeekVideoFrameCounter = 0;
        if (Status != NvSuccess) return Status;

        Status = Mp4ReadData (pNvMp4Parser, TrackIndex, pBuffer , pFrameSize, pDTS,
                              pCTS, pELST) ;
        if ( (pNvMp4Parser->rate > 2000) || (pNvMp4Parser->rate < 0))
        {
            ts = (*pCTS) + (*pDTS);
            MediaTimeScale = NvMp4ParserGetMediaTimeScale (pNvMp4Parser, TrackIndex);
            if (MediaTimeScale != 0)
                TimeStamp = ( ( ( (NvU64) ts + (*pELST)) * TICKS_PER_SECOND) / MediaTimeScale) * 10;
            else
                TimeStamp = 0;

            pNvMp4Parser->FramingInfo[pNvMp4Parser->AudioIndex].FrameCounter = (NvU32) (pNvMp4Parser->AudioFPS * ( (TimeStamp) / 10000000));
        }
        if (pNvMp4Parser->rate < 0)
            pNvMp4Parser->FramingInfo[TrackIndex].FrameCounter--;
        else
            pNvMp4Parser->FramingInfo[TrackIndex].FrameCounter++;
    }

cleanup:
    return Status;
}

NvError
NvMp4ParserGetCurrentAccessUnit (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    void *pBuffer,
    NvU32 *pFrameSize,
    NvU64 *pDTS,
    NvS32 *pCTS,
    NvU64 *pELST)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (pNvMp4Parser);
    NVMM_CHK_ARG (TrackIndex < (pNvMp4Parser->MvHdData.AudioTracks + pNvMp4Parser->MvHdData.VideoTracks));
    NVMM_CHK_ARG (pNvMp4Parser->TrackInfo[TrackIndex].TrackType != NvMp4TrackTypes_OTHER);


    if (NVMP4_ISTRACKAUDIO (pNvMp4Parser->TrackInfo[TrackIndex].TrackType) && pNvMp4Parser->IsStreaming)
    {
        Status = Mp4ReadAudioStreamingData (pNvMp4Parser, TrackIndex, pBuffer, pFrameSize, pDTS, pCTS, pELST);
    }
    else
    {
        Status = Mp4ReadData (pNvMp4Parser, TrackIndex, pBuffer, pFrameSize, pDTS, pCTS, pELST) ;
    }

cleanup:
    return Status;
}

NvError
NvMp4ParserSetRate (
    NvMp4Parser * pNvMp4Parser)
{
    NvError Status = NvSuccess;
    NvU32 FrameCounter = 0;
    NvMp4FramingInfo *pVideoFramingInfo = NULL;
    NvMp4FramingInfo *pAudioFramingInfo = NULL;

    NVMM_CHK_ARG (pNvMp4Parser);

    pVideoFramingInfo = &pNvMp4Parser->FramingInfo[pNvMp4Parser->VideoIndex];
    pAudioFramingInfo = &pNvMp4Parser->FramingInfo[pNvMp4Parser->AudioIndex];

    if (pNvMp4Parser->rate > 2000) //seek to next I frame
    {
        FrameCounter = NvMp4ParserGetNextSyncUnit (pNvMp4Parser, pVideoFramingInfo->FrameCounter, NV_TRUE);
        if ( (FrameCounter == (NvU32)-1) || (FrameCounter == (NvU32)NvMp4SyncUnitStatus_STSSENTRIES_DONE)) //error returned by the above API
        {
            //Go to Last frame
            pAudioFramingInfo->FrameCounter = pAudioFramingInfo->TotalNoFrames;
            pVideoFramingInfo->FrameCounter = pVideoFramingInfo->TotalNoFrames;
            Status = NvError_ParserEndOfStream;
            goto cleanup;
        }

    }
    else if (pNvMp4Parser->rate < 0) //seek to previous I frame
    {
        if ( (pVideoFramingInfo->FrameCounter == 1) || (pVideoFramingInfo->FrameCounter == 0))
        {
            //reverse I frame scan mode reached beginning of the track return EOS
            pAudioFramingInfo->FrameCounter = 0;
            pVideoFramingInfo->FrameCounter = 0;
            Status = NvError_ParserEndOfStream;
            goto cleanup;
        }
        FrameCounter = NvMp4ParserGetNextSyncUnit (pNvMp4Parser, pVideoFramingInfo->FrameCounter , NV_FALSE);

        if (FrameCounter == (NvU32)-1) //error returned by the above API
        {
            //Go to Last frame
            pAudioFramingInfo->FrameCounter = pAudioFramingInfo->TotalNoFrames;
            pVideoFramingInfo->FrameCounter = pVideoFramingInfo->TotalNoFrames;
            Status = NvError_ParserEndOfStream;
            goto cleanup;
        }
    }
    if (FrameCounter != 0)
    {
        FrameCounter--;//The actual I frame is one less than the frame returned by the getNextSyncUnit
    }
    pVideoFramingInfo->FrameCounter = FrameCounter;

cleanup:
    return Status;
}

/*
 * given current frame count (pNvMp4FramingInfo->FrameCount) and stsc box,
 * return with corresponding AVC specific configuration
 */
NvError
NvMp4ParserGetAVCConfigData (
    NvMp4Parser *pNvMp4Parser,
    NvU32 StreamIndex,
    NvAVCConfigData **ppAVCConfigData)
{
    NvMp4FramingInfo *pNvMp4FramingInfo = NULL;
    NvMp4TrackTypes TrackType;
    NvU32 FrameStart = 0;
    NvU32 FrameEnd = 0;
    NvStscEntry *stsc;
    NvBool found = NV_FALSE;

    TrackType = NvMp4ParserGetTracksInfo (pNvMp4Parser, StreamIndex);

    if (NvMp4TrackTypes_AVC != TrackType)
    {
        return NvError_BadParameter;
    }

    if(0 == pNvMp4Parser->nAVCConfigCount)
    {
        /*sanity check*/
        return NvError_BadParameter;
    }

    if(1 == pNvMp4Parser->nAVCConfigCount)
    {
        /*this should be true for most media files, we can save executing time*/
        *ppAVCConfigData = pNvMp4Parser->pAVCConfigData[0];
        return NvSuccess;
    }

    pNvMp4FramingInfo = &pNvMp4Parser->FramingInfo[StreamIndex];
    /*walking through the stsc linked-list, start from list head*/
    stsc = pNvMp4FramingInfo->StscEntryListHead;
    do
    {
        if(NULL == stsc->next)
        {
            /*last entry in stsc, set FrameEnd to maximum value of NvU32*/
            /*doing this in cases of seeing bad stsc*/
            FrameEnd = (NvU32)-1;
        }
        else
        {
            /*refer ISO/IEC 14496-12 "Sample To Chunk Box" for this calculation*/
            FrameEnd = FrameStart +
                (stsc->next->first_chunk - stsc->first_chunk)*stsc->samples_per_chunk;
        }

        if ((pNvMp4FramingInfo->FrameCounter >= FrameStart) &&
                (pNvMp4FramingInfo->FrameCounter < FrameEnd))
        {
            if(stsc->sample_desc_index > pNvMp4Parser->nAVCConfigCount)
            {
                /* sanity check, stsc->sample_dec_index points to a non-existing
                 * avcC box, either stsc or stsd is corrupted */
                NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_ERROR,
                        "AVC: stsc->sample_desc_index invalid (=%d)\n",
                        stsc->sample_desc_index));
                return NvError_BadParameter;
            }

            /*stsc->sample_desc_index starts from 1*/
            *ppAVCConfigData =
                pNvMp4Parser->pAVCConfigData[stsc->sample_desc_index-1];
            found = NV_TRUE;
            break; /*found*/
        }

        stsc = stsc->next;
        FrameStart = FrameEnd;
    } while(stsc);


    if(!found)
    {
        /*shouldn't be here*/
        NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_ERROR,
                "AVC: can't find avcC FrameCounter = %d\n",
                pNvMp4FramingInfo->FrameCounter));
        return NvError_BadParameter;
    }

    return NvSuccess;
}

static void InitBits(NvU8 *p, NvU32 size, NvMp4Bitstream *pBitStream)
{
    pBitStream->size = size;
    pBitStream->rdptr = p;
    pBitStream->shift_reg = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
    pBitStream->shift_reg_bits = 32;
    pBitStream->IsError = NV_FALSE;
}

static void FillBuffer(NvMp4Bitstream *pBitStream, NvU8 ToUpdate)
{
    NvU8 *p = pBitStream->rdptr + 4;
    pBitStream->shift_reg = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
    pBitStream->shift_reg_bits = 32;
    if (ToUpdate)
    {
        pBitStream->size -= 4;
        pBitStream->rdptr += 4;
    }
}

static NvU32 ReadBits(NvMp4Bitstream *pBitStream, NvU32 bits)
{
    NvU32 result;
    if (pBitStream->shift_reg_bits >= bits)
    {
        result = GETBITS(pBitStream->shift_reg, bits);
        pBitStream->shift_reg_bits -= bits;
        pBitStream->shift_reg <<= bits;
    }
    else
    {
        bits -= pBitStream->shift_reg_bits;
        result = GETBITS(pBitStream->shift_reg, pBitStream->shift_reg_bits) << bits;
        FillBuffer(pBitStream, 1);
        result |= GETBITS(pBitStream->shift_reg, bits);
    }
    return result;
}

static NvU32 ScanBits(NvMp4Bitstream *pBitStream, NvU32 bits)
{
    NvU32 result;
    if (pBitStream->shift_reg_bits >= bits)
    {
        result = GETBITS (pBitStream->shift_reg, bits);
    }
    else
    {
        bits -= pBitStream->shift_reg_bits;
        result = GETBITS (pBitStream->shift_reg, pBitStream->shift_reg_bits) << bits;
        FillBuffer(pBitStream, 0);
        result |= GETBITS (pBitStream->shift_reg, bits);
    }
    return result;
}

static NvU32 LeadingZero(NvMp4Bitstream *pBitStream, NvU32 n)
{
    NvU32 i;
    NvU32 value;
    NvU32 nextdata;
    i = 0;
    value = 0;
    do
    {
        i += 8;
        nextdata = ScanBits(pBitStream, i);
        value += NvLeadingZerosTable[nextdata & 0x000000FF] - 3;
        if (value != i)
            break;
    }while (i < n);

    if (value > n)
        value = n;

    return value;
}

static NvU32 ReadBits_ue(NvMp4Bitstream *pBitStream, NvS32 min, NvS32 max)
{
    int codeNum, leadingZeroBits;
    leadingZeroBits = LeadingZero(pBitStream, 24);
    ReadBits(pBitStream, leadingZeroBits + 1);
    if (leadingZeroBits == 0)
        codeNum = 0;
    else
        codeNum = (1 << leadingZeroBits) - 1 + ReadBits(pBitStream, leadingZeroBits);
    if ((max != -1) && ((codeNum > max) || (codeNum < min)))
    {
        pBitStream->IsError = NV_TRUE;
    }
    return codeNum;
}

static  NvMp4AvcEntropy Parse(NvMp4Bitstream *pBitStream)
{
    NvU32 entropy_coding_mode_flag;
    // ignore 1 byte for NAL unit type since we know this is PPS
    ReadBits(pBitStream, 8);
    // pic_parameter_set_id
    ReadBits_ue(pBitStream, 0, 255);
    // seq_parameter_set_id
    ReadBits_ue(pBitStream, 0, 31);
    if (pBitStream->IsError)
    {
        NvOsDebugPrintf("Out of range parameter\n");
        return AvcEntropy_Invalid;
    }
    entropy_coding_mode_flag = ReadBits(pBitStream, 1);
    if (entropy_coding_mode_flag)
        return AvcEntropy_Cabac;
    else
        return AvcEntropy_Cavlc;
}

/* Parse PPS data to identify if it's CABAC or CAVLC */
NvMp4AvcEntropy NvMp4ParserParsePPS (NvU8 *p, NvU32 size)
{
    NvMp4Bitstream BitStream;
    // init bitstream
    InitBits(p, size, &BitStream);
    // parse
    return Parse(&BitStream);
}
