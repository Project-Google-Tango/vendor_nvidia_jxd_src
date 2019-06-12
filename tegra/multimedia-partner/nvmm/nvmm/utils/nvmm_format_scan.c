/*
 * Copyright (c) 2009 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* MOSTLY PORTED FROM THE //sw/multimedia/ NvFormatScan.cpp */

#include <ctype.h>
#include <string.h>

#include "nvos.h"
#include "nvcommon.h"

#include "nvmm_file_util.h"

typedef struct _NvCPR {
    NV_CUSTOM_PROTOCOL *pProto;
    NvCPRHandle hFile;
} NvCPR;

static NvBool ScanForWav(NvCPR *oProto)
{
    NvU8 buff[12];
    size_t size = 0;

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    size = oProto->pProto->Read(oProto->hFile, buff, 12);
    if (size != 12)
        return NV_FALSE;

    if (NvOsStrncmp("RIFF", (char *)buff, 4))
        return NV_FALSE;

    if (NvOsStrncmp("WAVE", (char *)(buff + 8), 4))
        return NV_FALSE;

    return NV_TRUE;
}

static NvBool ScanForAvi(NvCPR *oProto)
{
    NvU8 buff[12];
    size_t size = 0;

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    size = oProto->pProto->Read(oProto->hFile, buff, 12);
    if (size != 12)
        return NV_FALSE;
    
    if (NvOsStrncmp("RIFF", (char *)buff, 4))
        return NV_FALSE;

    if (NvOsStrncmp("AVI ", (char *)(buff + 8), 4) &&
        NvOsStrncmp("AVIX", (char *)(buff + 8), 4))
    {
        return NV_FALSE;
    }

    return NV_TRUE;
} 

static NvU8 ASF_Header_GUID[16] = 
    { 0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C };

static NvBool ScanForAsf(NvCPR *oProto)
{
    NvU8 buff[16];
    size_t size = 0;

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    size = oProto->pProto->Read(oProto->hFile, buff, 16);
    if (size != 16)
        return NV_FALSE;

    if (NvOsMemcmp(buff, &ASF_Header_GUID, 16))
        return NV_FALSE;

    return NV_TRUE;
}

static void CapitalizeToken(NvU32 *pulToken)
{
    char *pC = (char *)pulToken;
    int i;

    for (i = 0; i < 4; i++)
    {
        if (pC[i] >= 'a' && pC[i] <= 'z')
        {
            pC[i] = toupper(pC[i]);
        }
    }
}

static void InvertU32(NvU32 *pulValue)
{
    *pulValue =     ((*pulValue & 0xff)<<24) |
                    ((*pulValue & 0xff00)<<8) |
                    ((*pulValue & 0xff0000)>>8) |
                    ((*pulValue & 0xff000000)>>24);
}

#define NVFS_MOOV 0x00000001
#define NVFS_VMHD 0x00000002
#define NVFS_SMHD 0x00000004
#define NVFS_MHLR 0x00000008
#define NVFS_S263 0x00000010
#define NVFS_MP4V 0x00000020
#define NVFS_SAMR 0x00000040
#define NVFS_MP4A 0x00000080
#define NVFS_FTYP 0x00000100
#define NVFS_MKV_EBML  0xA3DF451A

#define FourCC(a, b, c, d) (a | (b << 8) | (c << 16) | (d << 24))

static NvBool ReadMP4Atom(NvCPR *oProto, NvU32 *pulAtomsFound, 
                          NvU32 *pulFtyp)
{
    NvU32 ulData[2];
    NvU8 ucData;
    NvU32 tag;
    NvU32 size;
    NvU64 llPosNextTag = 0;
    NvU64 llPosCurrent = 0;
    NvU32 ulSearchLength = 0x1000;
    size_t readsize;

    if (NvSuccess != oProto->pProto->GetPosition(oProto->hFile, &llPosCurrent))
        return NV_FALSE;
    readsize = oProto->pProto->Read(oProto->hFile, (NvU8*)&(ulData[0]), 4);
    if (readsize != 4)
        return NV_FALSE;
    readsize = oProto->pProto->Read(oProto->hFile, (NvU8*)&(ulData[1]), 4);
    if (readsize != 4)
        return NV_FALSE;

    ulSearchLength -= 8;

    do 
    {
        size = ulData[0];
        InvertU32(&size);
        llPosNextTag = (NvS64)(llPosCurrent + (NvU64)size);

        tag = ulData[1];
        CapitalizeToken(&tag);

        switch (tag)
        {
        case FourCC('F','T','Y','P'):
            *pulAtomsFound |= NVFS_FTYP;
            readsize = oProto->pProto->Read(oProto->hFile, (NvU8*)pulFtyp, 4);
            if (readsize != 4)
                return NV_FALSE;

            return NV_FALSE; // just need this

        case FourCC('M','O','O','V'):
            *pulAtomsFound |= NVFS_MOOV;
            return NV_FALSE;

            // fall through
        case FourCC('M','D','I','A'):
        case FourCC('T','R','A','K'):
        case FourCC('M','I','N','F'):
        case FourCC('S','T','B','L'):
        case FourCC('S','T','S','D'):
        case FourCC('H','D','L','R' ):
        {
            NvU32 ulTimesCalled = 0;
            do 
            {
                ReadMP4Atom(oProto, pulAtomsFound, pulFtyp);
                oProto->pProto->GetPosition(oProto->hFile, &llPosCurrent);
                ulTimesCalled++;
            } while (llPosCurrent < llPosNextTag && ulTimesCalled < 10000);
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
        }
        case FourCC('M','P','4','V'):
            *pulAtomsFound |= NVFS_MP4V;
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
        case FourCC('S','A','M','R'):
            *pulAtomsFound |= NVFS_SAMR;
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
        case FourCC('M','P','4','A'):
            *pulAtomsFound |= NVFS_MP4A;
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
        case FourCC('S','2','6','3'):
            *pulAtomsFound |= NVFS_S263;
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
        case FourCC( 'M', 'H', 'L', 'R' ):
            *pulAtomsFound |= NVFS_MHLR;
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
        case FourCC( 'S', 'M', 'H', 'D' ):
            *pulAtomsFound |= NVFS_SMHD;
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
        case FourCC( 'V', 'M', 'H', 'D' ):
            *pulAtomsFound |= NVFS_VMHD;
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
            // SKIP OVER ALL OTHER KNOWN TAGS
        case FourCC( 'C', 'O', '6', '4' ):
        case FourCC( 'S', 'T', 'C', 'O' ):
        case FourCC( 'C', 'R', 'H', 'D' ):
        case FourCC( 'C', 'T', 'T', 'S' ):
        case FourCC( 'C', 'P', 'R', 'T' ):
        case FourCC( 'U', 'R', 'L', ' ' ):
        case FourCC( 'U', 'R', 'N', ' ' ):
        case FourCC( 'D', 'I', 'N', 'F' ):
        case FourCC( 'D', 'R', 'E', 'F' ):
        case FourCC( 'S', 'T', 'D', 'P' ):
        case FourCC( 'E', 'S', 'D', 'S' ):
        case FourCC( 'E', 'D', 'T', 'S' ):
        case FourCC( 'E', 'L', 'S', 'T' ):
        case FourCC( 'U', 'U', 'I', 'D' ):
        case FourCC( 'F', 'R', 'E', 'E' ):
        case FourCC( '!', 'G', 'N', 'R' ):
        case FourCC( 'H', 'M', 'H', 'D' ):
        case FourCC( 'H', 'I', 'N', 'T' ):
        case FourCC( 'N', 'M', 'H', 'D' ):
        case FourCC( 'M', 'P', '4', 'S' ):
        case FourCC( 'M', 'D', 'A', 'T' ):
        case FourCC( 'M', 'D', 'H', 'D' ):
        case FourCC( 'M', 'V', 'H', 'D' ):
        case FourCC( 'I', 'O', 'D', 'S' ):
        case FourCC( 'O', 'D', 'H', 'D' ):
        case FourCC( 'M', 'P', 'O', 'D' ):
        case FourCC( 'S', 'T', 'S', 'Z' ):
        case FourCC( 'S', 'T', 'S', 'C' ):
        case FourCC( 'S', 'D', 'H', 'D' ):
        case FourCC( 'S', 'T', 'S', 'H' ):
        case FourCC( 'S', 'K', 'I', 'P' ):
        case FourCC( 'D', 'P', 'N', 'D' ):
        case FourCC( 'S', 'T', 'S', 'S' ):
        case FourCC( 'T', 'K', 'H', 'D' ):
        case FourCC( 'T', 'R', 'E', 'F' ):
        case FourCC( 'U', 'D', 'T', 'A' ):
            return (NvSuccess == oProto->pProto->SetPosition(oProto->hFile, llPosNextTag, NvCPR_OriginBegin));
        }

        // read another byte and try again
        ulData[0] >>= 8;
        ulData[0] |= (ulData[1]&0xff)<<24;
        ulData[1] >>=8;

        readsize = oProto->pProto->Read(oProto->hFile, &ucData, 1);
        if (readsize != 1)
            return NV_FALSE;

        llPosCurrent++;
        ulSearchLength--;
        ulData[1] |= (NvU32)ucData<<24;

    } while (ulSearchLength);

    return NV_FALSE;
}

static NvBool ScanForMp4(NvCPR *oProto)
{
    NvU32 ulFtyp, ulAtomsFound;
    NvU32 ulTimesCalled = 0;

    ulFtyp = ulAtomsFound = 0;

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    while (ReadMP4Atom(oProto, &ulAtomsFound, &ulFtyp))
    {
        ulTimesCalled++;
        if (ulTimesCalled > 10000)
            break;
    }

    if (ulAtomsFound & NVFS_MOOV ||
        ulAtomsFound & NVFS_FTYP)
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

static NvBool FindMp3Header(NvCPR *oProto, NvU32 ulSearchLength, NvU32 *pulBits)
{
    NvError eErr;
    NvU8 bits;
    NvU32 ulNewBits, ulBitrateIndex, ulLayer, ulSamplingFreq;
    size_t readsize;
    NvU32 ulVersion;
    NvU32 ulBytesRead = 0;
    NvU32 layerZeroCnt = 0;

    do 
    {
        //look for 0xFF's of syncword
        do
        {
            readsize = oProto->pProto->Read(oProto->hFile, &bits, 1);
            ulBytesRead++;
            if (readsize != 1 || ulBytesRead >= ulSearchLength)
            {
                return NV_FALSE;
            }
        } while (bits != 0xFF);

        //discard leading 0xFF's of syncword
        do
        {
            readsize = oProto->pProto->Read(oProto->hFile, &bits, 1);
            ulBytesRead++;
            if (readsize != 1 || ulBytesRead >= ulSearchLength)
            {
                return NV_FALSE;
            }
        } while (bits == 0xFF);

        if ((bits & 0xE0) != 0xE0)
        {
            continue;
        }

        ulVersion = 4 - ((bits & 0x18) >> 3);
        if (ulVersion == 3) // reserved
        {
            continue;
        }

        ulLayer = 4 - ((bits&0x6)>>1);
        if (ulLayer == 4)
        {
            layerZeroCnt++;
            if(layerZeroCnt > 4)
                return NV_FALSE;
            continue;
        }
        ulNewBits = bits << 4;

        readsize = oProto->pProto->Read(oProto->hFile, &bits, 1);
        ulBytesRead++;

        ulBitrateIndex = bits >> 4;
        if (ulBitrateIndex == 15){ // != 15
            continue;
        }
        ulSamplingFreq = (bits & 0xc) >> 2;
        if (ulSamplingFreq == 3){ // != 3
            continue;
        }
        ulNewBits |= ulSamplingFreq;

        if ((0 == *pulBits) || (ulNewBits == *pulBits))
        {
            NvU32 uBitrate, uFrequency, uPadBit, uFrameSize, versionidx = 0;

            static const NvS32 s_MygaSampleRate[3][4] =
            {
                {44100, 48000, 32000, 0}, // MPEG-1
                {22050, 24000, 16000, 0}, // MPEG-2
                {11025, 12000,  8000, 0}, // MPEG-2.5
            };
            static const NvS32 s_MygaBitrate[3][3][15] =
            {
                {
                    // MPEG-1
                    {  0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448}, // Layer 1
                    {  0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384}, // Layer 2
                    {  0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}, // Layer 3
                },
                {
                    // MPEG-2, MPEG-2.5
                    {  0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256}, // Layer 1
                    {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, // Layer 2
                    {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, // Layer 3
                },
                {
                    // MPEG-2, MPEG-2.5
                    {  0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256}, // Layer 1
                    {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, // Layer 2
                    {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, // Layer 3
                },
            };

            *pulBits = ulNewBits;
            if (ulVersion == 1)
                versionidx = 0;
            else if (ulVersion == 2)
                versionidx = 1;
            else if (ulVersion == 4)
                versionidx = 2;

            uBitrate = s_MygaBitrate[versionidx][ulLayer-1][ulBitrateIndex];
            uFrequency = s_MygaSampleRate[versionidx][ulSamplingFreq];

            uPadBit = ((bits & 0x2) >> 1);

            if (ulLayer == 1) // Layer 1
            {
                if (uPadBit)
                {
                    uFrameSize = uFrequency == 0 ? 417 : ( ( (uBitrate * 1000 * 12) / uFrequency + 1)) * 4;
                }
                else
                {
                    uFrameSize = uFrequency == 0 ? 417 : ( (uBitrate * 1000 * 12) / uFrequency) * 4;
                }
            }
            else //Layer2 & Layer3
            {
                if (uPadBit)
                {
                    uFrameSize = (uFrequency == 0) ? 417 : ((((versionidx == 2)||(versionidx == 1)) && (ulLayer == 3)) ?
                        ((uBitrate * 1000 * 72)/uFrequency + 1) : ((uBitrate * 1000 * 144)/uFrequency + 1));
                }
                else
                {
                    uFrameSize = (uFrequency == 0) ? 417 : ((((versionidx == 2)||(versionidx == 1)) && (ulLayer == 3)) ?
                        ((uBitrate * 1000 * 72)/uFrequency) : ((uBitrate * 1000 * 144)/uFrequency));
                }
            }
            if (uFrameSize - uPadBit)
            {
                NvU32 uNextFrameOffset = uFrameSize - 3;
                eErr = oProto->pProto->SetPosition(oProto->hFile, uNextFrameOffset, NvCPR_OriginCur);
                if (NvSuccess != eErr) 
                {
                    return NV_FALSE;
                }

                ulBytesRead += uNextFrameOffset;
            }
            return NV_TRUE;
        }
    } while (ulBytesRead < ulSearchLength);

    return NV_FALSE;
}

#define MAXIMUM_ID3_TAG_COUNT 5
#define ID3_HEADER_SIZE 10

static NvBool SkipId3Header(NvCPR *oProto, NvBool *pbFoundId3)
{
    NvError eErr;
    NvU8 uTag[ID3_HEADER_SIZE];
    size_t sRead;
    NvU32 ulHeaderSize=0, id3Count=0;

    *pbFoundId3 = NV_FALSE;

    for (id3Count=0; id3Count < MAXIMUM_ID3_TAG_COUNT; id3Count++)
    {
        sRead = oProto->pProto->Read(oProto->hFile, uTag, 10);
        if (sRead != 10)
            return NV_FALSE;

        if ( uTag[0] != 'I'
             || uTag[1] != 'D'
             || uTag[2] != '3'
             || uTag[3] == 0xFF
             || uTag[4] == 0xFF
             || uTag[6] > 0x7F
             || uTag[7] > 0x7F
             || uTag[8] > 0x7F
             || uTag[9] > 0x7F
            )
        {
            eErr = oProto->pProto->SetPosition(oProto->hFile, -10, NvCPR_OriginCur);
            if (NvSuccess != eErr)
            {
                return NV_FALSE;
            }
            return NV_TRUE;
        }
        else
        {
            ulHeaderSize += ((uTag[6] << 21) +
                (uTag[7] << 14) +
                (uTag[8] << 7) +
                (uTag[9]));
            *pbFoundId3 = NV_TRUE;
            eErr = oProto->pProto->SetPosition(oProto->hFile, ulHeaderSize, NvCPR_OriginCur);
            if (NvSuccess != eErr)
            {
                return NV_FALSE;
            }
        }
    }
    return NV_TRUE;
}

static NvBool ScanForMp3(NvCPR *oProto)
{
    NvU32 ulMaxBytesScanned = 0x1000 /*:0x100000 */;
    NvU32 ulBits = 0, ulNewBits = 0, uMatchCount = 0, uCount = 0;
    NvBool bFoundId3;
    NvBool eRet;

    // read a few headers (first few frames may be short)
    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    eRet = SkipId3Header(oProto, &bFoundId3);
    if (!eRet)
    {
        if (!bFoundId3)
            oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);
    }

    do {
        if (ulNewBits != 0)
            ulBits = ulNewBits;
        ulNewBits = 0;

        if(!FindMp3Header(oProto, ulMaxBytesScanned, &ulNewBits)) // find another header with matching bits
            return NV_FALSE;

        if ((ulBits == ulNewBits) && (ulNewBits != 0))
            uMatchCount++;
        else
            uMatchCount = 0;
    }
    while (uCount++ < 15 && uMatchCount < 10);

    if (uMatchCount > 9)
        return NV_TRUE;

    return NV_FALSE;
}

static NvBool CheckForOggAudio(NvCPR *oProto)
{
    NvBool bos = NV_FALSE;
    NvU8 buf[100];
    NvU32 noofsegments = 0;
    NvS64 datalength = 0;
    NvBool foundvideo = NV_FALSE;
    NvU32 i = 0;
 
    // read 27 bytes initially
    NvOsMemset(buf, 0, sizeof(buf));
    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);    
    oProto->pProto->Read(oProto->hFile, buf, 27);
 
    do
    {
        // check for bos
        if (buf[5] & 0x02)
        {
            bos = NV_TRUE;
            noofsegments = buf[26];
 
            // read noofsegments bytes
            NvOsMemset(buf, 0, sizeof(buf));
            oProto->pProto->Read(oProto->hFile, buf, noofsegments);
 
            // calculate the actual datalength
            for (i = 0; i < noofsegments; i++)
            {
                datalength += buf[i];
            }
 
            NvOsMemset(buf, 0, sizeof(buf));
            oProto->pProto->Read(oProto->hFile, buf,8);
 
            if (!NvOsStrncmp("theora", (char *)(buf + 1), 6))
            {
                foundvideo = NV_TRUE;
                break;
            }
 
            oProto->pProto->SetPosition(oProto->hFile, (datalength - 8), NvCPR_OriginCur);    
            NvOsMemset(buf, 0, sizeof(buf));
            oProto->pProto->Read(oProto->hFile, buf, 27);
 
            datalength = 0;
        }
        else
        {
            bos = NV_FALSE;
        }
 
    }while (bos);
 
    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);    
 
    return (!foundvideo);
}

static NvBool ScanForOgg(NvCPR *oProto)
{
    NvU8 buff[20];
    size_t size = 0;

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    size = oProto->pProto->Read(oProto->hFile, buff, 20);
    if (size != 20)
        return NV_FALSE;

    if (NvOsStrncmp("OggS", (char *)buff, 4))
        return NV_FALSE;

    return NV_TRUE;
}

static NvBool ScanForAmrAwb(NvCPR *oProto)
{
    size_t size = 0;
    NvU8 amrBuf[8];

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    size = oProto->pProto->Read(oProto->hFile, amrBuf, 8);
    if (size != 8)
        return NV_FALSE;

    if ((!NvOsStrncmp("#!AMR-WB", (char *)amrBuf, 8)) || 
        (!NvOsStrncmp("#!AMR",    (char *)amrBuf, 5)))
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

static NvBool ScanForM2v(NvCPR *oProto)
{
    NvU8 StartCodeBuf[4];
    size_t size = 0;

    // Detect MPEG2 ES start code here: 000001B3

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    size = oProto->pProto->Read(oProto->hFile,StartCodeBuf,4);

    if (size != 4)
        return NV_FALSE;

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    if ((StartCodeBuf[0] == 0) && (StartCodeBuf[1] == 0) &&
        (StartCodeBuf[2] == 1))
    {
        if (StartCodeBuf[3] == 0xB3)
        {
           return NV_TRUE;
        }
    }

    return NV_FALSE;
}

#define MAX_AAC_FRAME_SINGLE_CH_SIZE 768
#define MAX_AAC_NUMBER_OF_CH  6
#define MAX_AAC_SIZE (MAX_AAC_FRAME_SINGLE_CH_SIZE * MAX_AAC_NUMBER_OF_CH)

static NvBool ScanForAac(NvCPR *oProto)
{
    size_t size = 0;
    NvU32 tagsize = 0;
    NvU32 syncWordCount = 0;
    NvU8 aacBuf[MAX_AAC_SIZE];
    NvU8 *adtsPtr = NULL;

    oProto->pProto->SetPosition(oProto->hFile, 0, NvCPR_OriginBegin);

    size = oProto->pProto->Read(oProto->hFile, aacBuf, MAX_AAC_SIZE);
    if (size != MAX_AAC_SIZE)
        return NV_FALSE;

    /* Checking for ID3, If ID3 found then metadata size can be computed using below equation */
    if (!NvOsMemcmp(aacBuf, "ID3", 3))
    {
        tagsize = (aacBuf[6] << 21) | (aacBuf[7] << 14) | 
                  (aacBuf[8] << 7) | aacBuf[9];
        tagsize += 10;

        //to calculate accurate syncword
        if(tagsize > MAX_AAC_SIZE)
        {
            oProto->pProto->SetPosition(oProto->hFile, tagsize, NvCPR_OriginBegin);
            size = oProto->pProto->Read(oProto->hFile, aacBuf, MAX_AAC_SIZE);
            if (size != MAX_AAC_SIZE)
                return NV_FALSE;
            tagsize = 0;
        }
    }
    adtsPtr = aacBuf + tagsize;
    syncWordCount = tagsize+1;

    if (syncWordCount >= (MAX_AAC_SIZE - 1))
    {
        return NV_FALSE;
    }
    /* Searching for the sync word with in MaxAdtsAacBufferSize buffer */
    while ((syncWordCount < (MAX_AAC_SIZE - 1)) && ((adtsPtr[0] != 0xFF) || ((adtsPtr[1] & 0xF6) != 0xF0)))
    {
        adtsPtr++;
        syncWordCount++;
    }

    if ((adtsPtr[0] == 0xFF && ((adtsPtr[1] & 0xF6) == 0xF0)))
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}


NvError NvMMUtilGuessFormatFromFile(char *szURI, NV_CUSTOM_PROTOCOL *pProtocol,
                                    NvParserCoreType *eParserType, 
                                    NvMMSuperParserMediaType *eMediaType)
{
    NvCPR oProto;
    NvS32 nProtoVer = 1;
    NvError err;

    *eParserType = NvParserCoreType_UnKnown;
    *eMediaType = NvMMSuperParserMediaType_Force32;

    if (!szURI || !eParserType || !eMediaType)
        return NvSuccess;

    oProto.pProto = pProtocol;
    oProto.hFile = NULL;

    oProto.pProto->GetVersion(&nProtoVer);
    if (nProtoVer < 2)
        return NvSuccess;

    err = oProto.pProto->Open(&oProto.hFile, szURI, NvCPR_AccessRead);
    if (err != NvSuccess)
    {
        return err;
    }

    if (ScanForWav(&oProto))
    {
        *eParserType = NvParserCoreType_WavParser;
        *eMediaType = NvMMSuperParserMediaType_Audio;
    }
    else if (ScanForAvi(&oProto))
    {
        *eParserType = NvParserCoreType_AviParser;
    }
    else if (ScanForAsf(&oProto))
    {
        *eParserType = NvParserCoreType_AsfParser;
    }
    else if (ScanForMp4(&oProto))
    {
        *eParserType = NvParserCoreType_Mp4Parser;
    }
    else if (ScanForMp3(&oProto))
    {
        *eParserType = NvParserCoreType_Mp3Parser;
        *eMediaType = NvMMSuperParserMediaType_Audio;
    }
    else if (ScanForOgg(&oProto))
    {
      if (CheckForOggAudio(&oProto))
      {
        *eParserType = NvParserCoreType_OggParser;
        *eMediaType = NvMMSuperParserMediaType_Audio;
      }
    }
    else if(ScanForAmrAwb(&oProto))
    {
        *eParserType = NvParserCoreType_AmrParser;
    }
    else if(ScanForM2v(&oProto))
    {
        *eParserType = NvParserCoreType_M2vParser;
    }
    else if(ScanForAac(&oProto))
    {
        *eParserType = NvParserCoreType_AacParser;
    }

    oProto.pProto->Close(oProto.hFile);

    return NvSuccess;
}

