/*
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvtest.h"
#include "nvsecuretool.h"

static NvError
ComputeCodeSize(
    NvOsFileHandle  File,
    NvU32          *Size,
    NvU32          *NumSegments,
    NvU32          *NumSegsLoaded);

static NvError
CopyCodeToBuffer(
    NvU8               *CodePtr,
    NvOsFileHandle      File,
    NvU32               NumSegments,
    SegHeaderAndOffset *SortBuf,
    NvU8              **NextPtr);

static NvError
SortSegments(
    NvOsFileHandle       File,
    NvU32                NumSegments,
    SegHeaderAndOffset **pSortBuf);
/*
 * ComputeCodeSize() - Compute the size of the code to be sent to the target.
 *    Increases the size and counts based on the data in File.
 */
static NvError
ComputeCodeSize(
    NvOsFileHandle  File,
    NvU32          *Size,
    NvU32          *NumSegments,
    NvU32          *NumSegsLoaded)
{
    ELFHDR  HeaderData;
    NvU16   Nsegs;
    NvU32   Offset;
    NvU32   i;
    NvError e;

    // Read the number of segments.
    e = NvElfGetNumSegments(File, &Nsegs, &Offset);
    if (e)
        return NvError_BLServerFileReadFailed;

    (*NumSegments) += (NvU32) Nsegs;

    NvAuPrintf("nsegs=%d\n", Nsegs);

    // Add in the size of each segment's data.
    for (i = 0; i < Nsegs; i++)
    {
        e = NvOsFseek(File, Offset + i*sizeof(ELFHDR), NvOsSeek_Set);
        if (e) return NvError_BLServerFileReadFailed;

        e = NvOsFread(File, &HeaderData, sizeof(ELFHDR), NULL);
        if (e) return NvError_BLServerFileReadFailed;

        // Add the size of the segment's data and its table entry.
        if (HeaderData.loadType == PT_LOAD)
        {
            *Size += HeaderData.size;
            (*NumSegsLoaded)++;
        }
    }

    return NvSuccess;
}

/*
 * Copy segment code to buffer
 * Note: NextPtr can be NULL
 */
static NvError
CopyCodeToBuffer(
    NvU8               *CodePtr,
    NvOsFileHandle      File,
    NvU32               NumSegments,
    SegHeaderAndOffset *SortBuf,
    NvU8              **NextPtr)
{
    NvError            e = NvSuccess;
    NvU32              i;

    // Copy the segment data
    for (i = 0; i < NumSegments; i++)
    {
        // Read the segment data into the buffer.
        e = NvOsFseek(File, SortBuf[i].Offset, NvOsSeek_Set);
        if (e)
        {
            e = NvError_BLServerFileReadFailed;
            goto fail;
        }
        NvAuPrintf("address: 0x%08X, 0x%08X\n",
                        SortBuf[i].SegHeader.data.address,
                        SortBuf[i].SegHeader.data.size );

        e = NvOsFread(File, CodePtr, SortBuf[i].SegHeader.data.size, NULL);
        if (e)
        {
            e = NvError_BLServerFileReadFailed;
            goto fail;
        }

        CodePtr += SortBuf[i].SegHeader.data.size;
    }

fail:
    /* Cleanup and exit. */
    if(NextPtr)
        *NextPtr = CodePtr;

    return e;
}

/*
 * Sort segment by address offset
 */
static NvError
SortSegments(
    NvOsFileHandle       File,
    NvU32                NumSegments,
    SegHeaderAndOffset **pSortBuf)
{
    NvError e = NvSuccess;
    NvU32   i, j;
    SegHeaderAndOffset *SortBuf;
    SegHeaderAndOffset Tmp;

    NV_ASSERT(pSortBuf);

    SortBuf = NvOsAlloc(NumSegments * sizeof(SegHeaderAndOffset));
    *pSortBuf = SortBuf;
    if (SortBuf == NULL)
        return NvError_InsufficientMemory;

    // Grab the metadata about each segment in the ELF file
    for (i = 0; i < NumSegments; i++)
    {
        e = NvElfComputeSegmentHeader(File, i,
                                      &(SortBuf[i].SegHeader),
                                      &(SortBuf[i].Offset));
        if (e)
        {
            NvAuPrintf("nvblserver: Invalid ELF file (SortSegments)\n");
            e = NvError_BLServerInvalidElfFile;
            goto fail;
        }
    }

    // sort the segments by load address.  As the number of segments
    // is probably quite small, a simple O(n^2) sort should be okay
    for (i = 0; i < NumSegments - 1; i++)
    {
        for (j = i + 1; j < NumSegments; j++)
        {
            if (SortBuf[i].Offset > SortBuf[j].Offset)
            {
                NvOsMemmove(&Tmp,&(SortBuf[i]),sizeof(Tmp));
                NvOsMemmove(&(SortBuf[i]),&(SortBuf[j]),sizeof(Tmp));
                NvOsMemmove(&(SortBuf[j]),&Tmp,sizeof(Tmp));
            }
        }
    }
    return e;

fail:
    NvOsFree(SortBuf);
    return e;
}

/*
 * Load NvTest file to buffer
 */
NvError LoadNvTest(
    const char *Filename,
    NvU32 *pEntryPoint,
    NvU8 **MsgBuffer,
    NvU32 *Length)
{
    NvOsFileHandle  File;
    NvU32  NumSegments   = 0;
    NvU32  NumSegsLoaded = 0;
    NvU32  PayloadLength = 0;
    SegHeaderAndOffset *SortBuf=NULL;
    NvError e = NvSuccess;

    NvOsFopen(Filename, NVOS_OPEN_READ, &File);

    NV_CHECK_ERROR(ComputeCodeSize(File, &PayloadLength, &NumSegments, &NumSegsLoaded));
    *Length = PayloadLength;
    /** Allocate message buffer */
    *MsgBuffer = (NvU8*) NvOsAlloc(PayloadLength);

    if (*MsgBuffer == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    /** Fill the message buffer */
    if (NvElfGetEntryPointAddress(File, pEntryPoint) != NvSuccess)
    {
        NvAuPrintf("Invalid ELF file (m).\n");
        e = NvError_BLServerInvalidElfFile;
        goto fail;
    }
    /** Sort the segments in the ELF file by load address **/
    NV_CHECK_ERROR_CLEANUP(SortSegments(File, NumSegments, &SortBuf));
    /* Copy into buffer */
    NV_CHECK_ERROR_CLEANUP(CopyCodeToBuffer(*MsgBuffer, File, NumSegments, SortBuf, NULL));
    NvOsFree(SortBuf);
fail:
    NvOsFclose(File);
    NvAuPrintf("Raw test binary length = %d\n", *Length);

    return e;
}

/*
 * Save singend NvTest file
 */
NvError SaveSignedNvTest(
    NvU32 Length,
    NvU8 *BlobBuf,
    const char *Filename,
    const char *ExtStr)
{
    NvError e = NvSuccess;
    NvOsFileHandle hFile = NULL;
    NvU32 Flags;
    char *err_str = 0;
    char SignedFilename[100];
    char *p;

    NvAuPrintf("Signed test binary length = %d $$$\n", Length);
    if ((NvOsStrlen(Filename) + NvOsStrlen(ExtStr)) > 100)
    {
        NvAuPrintf("\nNvTest file name is too long, max is 100 \n");
        return e;
    }
    NvOsStrcpy(SignedFilename, Filename);
    p = SignedFilename + NvOsStrlen(SignedFilename);
    NvOsStrcpy(p, ExtStr);

    Flags = NVOS_OPEN_CREATE | NVOS_OPEN_WRITE;
    e = NvOsFopen(SignedFilename, Flags, &hFile);
    VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);
    e = NvOsFseek(hFile, 0, NvOsSeek_Set);
    VERIFY(e == NvSuccess, err_str = "file seek failed"; goto fail);
    e = NvOsFwrite(hFile, BlobBuf, Length);
    VERIFY(e == NvSuccess, err_str = "file write failed"; goto fail);

fail:
    if (err_str)
        NvAuPrintf("\n%s NvError 0x%x \n", err_str, e);
    if (hFile)
        NvOsFclose(hFile);
    NvAuPrintf("NvTest file %s saved as %s\n", Filename, SignedFilename);
    return e;
}
