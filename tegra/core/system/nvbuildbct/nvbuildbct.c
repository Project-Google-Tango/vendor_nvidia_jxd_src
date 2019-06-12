/**
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvbuildbct.c - Implementation of the nvbuildbct.
 */

/*
 * TODO / Notes
 * - Add doxygen commentary
 * - Add support for different allocation modes/strategies
 */

#include "nvbuildbct.h"
#include "nvassert.h"
#include "nvapputil.h"
#include "parse.h"
#include "parse_hal.h"

void InitContext(BuildBctContext *Context);
void Usage(void);
void ProcessCommandLine(int argc, char *argv[], BuildBctContext *Context);
void AllocateBctAndSetValues(BuildBctContext *Context);

BuildBctParseInterface *g_pBctParseInterf = NULL;

NvError
OpenFile(const char *Filename, NvU32 Flags, NvOsFileHandle *File)
{
    NvError         Error;

    if (Filename == NULL) return NvError_BadParameter;
    if (File     == NULL) return NvError_BadParameter;

    Error = NvOsFopen(Filename, Flags, File);
    if (Error != NvSuccess)
        NvAuPrintf("Error opening %s with flags=%d.\n", Filename, Flags);

    return Error;
}

void
InitContext(BuildBctContext *Context)
{
    NvOsMemset(Context, 0, sizeof(BuildBctContext));

    /* Set defaults */
    Context->ConfigFile            = NULL;
    Context->RawFile            = NULL;
    Context->RandomBlockType       = RandomAesBlockType_Zeroes;

    Context->BctWritten            = NV_FALSE; /* Not yet written */
    Context->NvBCT = NULL;

    Context->LogMemoryOps        = NV_FALSE;
}

void AllocateBctAndSetValues(BuildBctContext *Context)
{

    /* Allocate space for the BCT */
    Context->NvBCT = NvOsAlloc(Context->NvBCTLength);

    NV_ASSERT(Context->NvBCT != NULL);
    NvOsMemset(&(Context->NvBCT[0]), 0, Context->NvBCTLength);

    g_pBctParseInterf->SetValue(Context, Token_BlockSize,     16384);
    g_pBctParseInterf->SetValue(Context, Token_PageSize,      512);
    g_pBctParseInterf->SetValue(Context, Token_PartitionSize, 134217728);
    g_pBctParseInterf->SetValue(Context, Token_Redundancy,    1);
    g_pBctParseInterf->SetValue(Context, Token_Version,       1);
    g_pBctParseInterf->SetValue(Context, Token_CustomerDataVersion,
                                        NV_CUSTOMER_DATA_VERSION);
}

NvError
WriteImageFile(BuildBctContext *Context)
{
    NvError Error = NvSuccess;

    NV_ASSERT(Context != NULL);

    if (Context->BctWritten == NV_FALSE)
        g_pBctParseInterf->UpdateBct(Context, UpdateEndState_Complete);

        return (int)Error;
}

NvError NvBuildBct(BuildBctArg *pArg)
{
    NvError Error = NvSuccess;
    NvU32 FileSize = 0;
    NvU32 ChipId;
    NvS64 Offset;
    const char *pCfgFile;
    const char *pBctFile;
    Nv3pCmdGetPlatformInfo *pInfo;
    BuildBctContext Context;
    NvOsStatType Stats;

    if (pArg == NULL)
    {
        Error = NvError_BadParameter;
        goto clean;
    }

    pInfo = (Nv3pCmdGetPlatformInfo *)pArg->pPlatformInfo;
    pCfgFile = pArg->pCfgFile;
    pBctFile = pArg->pBctFile;
    ChipId = pArg->ChipId;
    Offset = pArg->Offset;

    InitContext(&Context);

    if (pInfo != NULL)
        Context.SecondaryBootDevice = pInfo->SecondaryBootDevice;

    /* Open the configuration file. */
    Error = OpenFile(pCfgFile, NVOS_OPEN_READ,  &(Context.ConfigFile));
    if (Error != NvSuccess)
    {
        NvAuPrintf("Error opening cfg file %s Error=%d\n",
                   pCfgFile, Error);
        Error = NvError_FileOperationFailed;
        goto clean;
    }

    /* Move to the offset */
    if (Offset != 0)
    {
        Error = NvOsFseek((Context.ConfigFile), Offset, NvOsSeek_Set);
        if (Error != NvSuccess)
        {
            NvAuPrintf("Error Seek cfg file %s Error=%d\n",
                       pCfgFile, Error);
            Error = NvError_FileOperationFailed;
            goto clean;
        }
    }

    NvOsStrncpy(Context.CfgFilename, pCfgFile, MAX_BUFFER);
    /* Record the output filename */
    if (pBctFile)
        NvOsStrncpy(Context.ImageFilename, pBctFile, MAX_BUFFER);

    g_pBctParseInterf = NvOsAlloc(sizeof(BuildBctParseInterface));
    if (g_pBctParseInterf == NULL)
    {
        NvAuPrintf("Insufficient memory to proceed.\n");
        Error = NvError_InsufficientMemory;
        goto clean;
    }
    NvOsMemset(g_pBctParseInterf, 0, sizeof(BuildBctParseInterface));

    switch (ChipId)
    {
        case 0x30:
            t30GetBuildBctInterf(g_pBctParseInterf);
            break;
        case 0x35:
            t11xGetBuildBctInterf(g_pBctParseInterf);
            break;
        case 0x40:
            t12xGetBuildBctInterf(g_pBctParseInterf);
            break;
        default:
            Error = NvError_BadValue;
            goto clean;
    }
    // Get the size of the bct and preserve it as a part of the context.
    g_pBctParseInterf->GetBctSize(&Context);

    /* Allocation of BCT memory is dependent on getting the size. Hence
     * this has been decoupled from basic initialization of context
     */
    AllocateBctAndSetValues(&Context);

    if ((pBctFile != NULL) &&
        !NvOsStrncmp(Context.CfgFilename, Context.ImageFilename, MAX_BUFFER))
    {
        NvAuPrintf("\nError: Configuration and Image file "
                   "names cannot be same.\n");
        Error = NvError_BadValue;
        goto clean;
    }

    /* Parse & process the contents of the config file. */
    g_pBctParseInterf->ProcessConfigFile(&Context);

    if (pBctFile != NULL)
    {
        /* Open the raw output file. */
        Error = OpenFile(Context.ImageFilename,
                     NVOS_OPEN_WRITE | NVOS_OPEN_CREATE,
                     &(Context.RawFile));
        if (Error != NvSuccess)
        {
            NvAuPrintf("Error opening raw file %s.  Error=%d\n",
                       Context.ImageFilename, Error);
            Error = NvError_FileOperationFailed;
            goto clean;
        }
    }

    /* Write the image file. */
    if (Context.LogMemoryOps == NV_FALSE)
    {
        /* The image hasn't been written yet. */
        Error = WriteImageFile(&Context);
        if (Error != NvSuccess)
        {
            NvAuPrintf("Error writing image file.\n");
            goto clean;
        }
    }

    if (pBctFile != NULL)
    {
        Error = NvOsStat(Context.ImageFilename, &Stats);
        if (Error != NvSuccess)
        {
            NvAuPrintf("Error: Unable to query info on bct file path %s\n",
                        Context.ImageFilename);
            goto clean;
        }
        FileSize = (NvU32)Stats.size;

        if (FileSize != 0)
        {
            NvAuPrintf("BuildBct: %s file created successfully. \n", pBctFile);
        }
    }
    else
    {
        if (Context.pBuffer == NULL)
        {
            NvAuPrintf("BuildBct buffer is not initialized!");
            Error = NvError_NotInitialized;
            goto clean;
        }

        pArg->pBctBuffer = Context.pBuffer;
        pArg->BctSize = Context.NvBCTLength;
    }

clean:
    if (Context.ConfigFile != NULL)
        NvOsFclose(Context.ConfigFile);
    if (Context.RawFile != NULL)
        NvOsFclose(Context.RawFile);

    if (Context.NvBCT != NULL)
    {
        NvOsFree(Context.NvBCT);
        Context.NvBCT = NULL;
    }
    if (g_pBctParseInterf != NULL)
    {
        NvOsFree(g_pBctParseInterf);
        g_pBctParseInterf = NULL;
    }

    return Error;
}

