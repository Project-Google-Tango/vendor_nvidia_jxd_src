/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvflash_fuse_bypass.h"
#include "nvos.h"
#include "nvutil.h"
#include "nvcommon.h"
#include "nv3p.h"
#include "nvflash_commands.h"
#include "nvusbhost.h"
#include "nvflash_parser.h"
#include "nvddk_fuse.h"
#include "nvapputil.h"

extern Nv3pSocketHandle s_hSock;
extern Nv3pCmdGetPlatformInfo s_PlatformInfo;
extern Nv3pBoardId s_BoardId;
extern Nv3pBoardDetails g_BoardDetails;

extern char* nvflash_status_string(Nv3pStatus s);
extern NvBool nvflash_wait_status(void);

#define VERIFY( exp, code ) \
    if ( !(exp) ) {         \
        code;               \
    }

#define CHECK_RANGE(val, min, max)                  \
    if (!((val) >= (min) && (val) <= (max)) &&      \
       ((min) | (max)))                             \
    {                                               \
        Ret = NV_FALSE;                             \
    }

#define MAX_SKUS 20
#define FUSE_PRODUCTION_MODE_0 0x100

typedef struct NvFlashSkuNameToIdMappingRec
{
    char SkuName[50];
    NvU32 SkuId;
}NvFlashSkuNameIdMapping;

typedef struct NvFuseBypassCallbackArgRec
{
    NvFuseBypassInfo FuseBypassInfo[MAX_SKUS];
    NvU32 NumSkus;
    NvU32 DefaultSku;
    NvU32 SupportedSkus[MAX_SKUS];
    NvU32 NumSupportedSkus;
    NvBoardInfo *pBoardInfo;
    parse_callback *pSub;
} NvFuseBypassCallbackArg;

NvFuseBypassInfo s_FuseBypassInfo;
static NvFlashSkuNameIdMapping s_Mappings[MAX_SKUS];

char* NvFlashGetSkuName(NvU32 SkuId)
{
    NvU32 i;

    for(i = 0; i < MAX_SKUS; i++)
    {
        if (s_Mappings[i].SkuId == SkuId)
            return s_Mappings[i].SkuName;
    }

    return "Unknown SKU";
}

NvU32 NvFlashGetSkuId(const char *pSkuName)
{
    NvU32 i;
    char *pEnd;

    for(i = 0; i < MAX_SKUS; i++)
    {
        if (NvFlashIStrcmp(s_Mappings[i].SkuName, pSkuName) == 0)
            return s_Mappings[i].SkuId;
    }

    i = NvUStrtoul(pSkuName, &pEnd, 0);
    if (*pEnd != '\0')
        i = 0;

    return i;
}

/**
 * Converts the comma separated string into integer array
 *
 * @param pString   Source string
 * @param pArray    Target array
 * @param MaxLen    Maximum length of input array
 *
 * @return          Number of integers retrieved from source string
 */
static NvU32 NvFlashStringToIntArray(char *pString, NvU32 *pArray, NvU32 MaxLen)
{
    char *pStart = pString;
    char *pEnd = pStart;
    NvU32 NumValues = 0;

    while (*pStart != '\0')
    {
        pArray[NumValues] = NvUStrtoul(pStart, &pEnd, 0);
        NumValues++;

        if (MaxLen <= NumValues)
            break;

        pStart = pEnd;

        while (*pStart != '\0' &&
               (*pStart == ' ' || *pStart == ',') &&
               pStart++)
            ;
    }

    return NumValues;
}

/**
 * Checks if he comma separated string contains a specified integer value
 *
 * @param pString   Source string
 * @param Value     Value to be searched in target string
 *
 * @return          Number of integers retrieved from source string
 */
static NvBool NvFlashStringContains(char *pString, NvU32 Value)
{
    char *pStart = pString;
    char *pEnd = pStart;
    NvBool Found = NV_FALSE;

    while (*pStart != '\0')
    {
        if (NvUStrtoul(pStart, &pEnd, 0) == Value)
        {
            Found = NV_TRUE;
            break;
        }
        pStart = pEnd;

        while (*pStart != '\0' &&
               (*pStart == ' ' || *pStart == ',') &&
               pStart++)
            ;
    }

    return Found;
}

/**
 * Callback function for parsing supported sku.
 *
 * @param pRec Vector of <key, value> pairs parsed by parser
 * @param pAux Callback Argument
 *
 * @return     Status to parser whether to continue or stop parsing
 *             and any error.
 */
static NvFlashParserStatus
NvFlashGetSupportedSkus(
    NvFlashVector *pRec,
    void *pAux)
{
    NvU32 i = 0;
    NvU32 NumPairs = 0;
    NvFlashSectionPair *pPairs  = NULL;
    NvFuseBypassCallbackArg *pArg = NULL;

    NumPairs = pRec->n;
    pPairs  = pRec->data;
    pArg = (NvFuseBypassCallbackArg *) pAux;

    for (i = 0; i < NumPairs; i++)
    {
        if (NvFlashIStrcmp(pPairs[i].key, "/policy") == 0 &&
            NvFlashIStrcmp(pPairs[i].value, "supported skus") == 0)
        {
            pArg->pSub = NULL;
            goto end;
        }

        if (pArg->NumSupportedSkus != 0)
            goto end;

        if (NvFlashIStrcmp(pPairs[i].key, "board") == 0)
        {
            if (NvUStrtoul(pPairs[i].value, NULL, 0) !=
                pArg->pBoardInfo->BoardId)
            {
                goto end;
            }
        }
        else if (NvFlashIStrcmp(pPairs[i].key, "board sku") == 0)
        {
            NvBool Found = NvFlashStringContains(pPairs[i].value,
                                                 pArg->pBoardInfo->Sku);
            if (Found == NV_FALSE)
                goto end;
        }
        else if (NvFlashIStrcmp(pPairs[i].key, "skus") == 0)
        {
            pArg->NumSupportedSkus = NvFlashStringToIntArray(pPairs[i].value,
                                                            pArg->SupportedSkus,
                                                            MAX_SKUS);
        }
        else
        {
            NvAuPrintf("Unknown token %s\n", pPairs[i].key);
            return P_ERROR;
        }
    }

end:
    return P_CONTINUE;
}

/**
 * Callback function for parsing default sku.
 *
 * @param pRec  Vector of <key, value> pairs parsed by parser
 * @param pAux  Callback Argument
 *
 * @return      Status to parser whether to continue or stop parsing
 *              and any error.
 */
static NvFlashParserStatus
NvFlashGetDefaultSkus(
    NvFlashVector *pRec,
    void *pAux)
{
    NvU32 i = 0;
    NvU32 NumPairs = 0;
    NvFlashSectionPair *pPairs  = NULL;
    NvFuseBypassCallbackArg *pArg = NULL;

    NumPairs = pRec->n;
    pPairs  = pRec->data;
    pArg = (NvFuseBypassCallbackArg *) pAux;

    for (i = 0; i < NumPairs; i++)
    {
        if (NvFlashIStrcmp(pPairs[i].key, "/policy") == 0 &&
            NvFlashIStrcmp(pPairs[i].value, "default skus") == 0)
        {
            pArg->pSub = NULL;
            goto end;
        }

        if (pArg->DefaultSku != 0)
            goto end;

        if (NvFlashIStrcmp(pPairs[i].key, "board") == 0)
        {
            if (NvUStrtoul(pPairs[i].value, NULL, 0) !=
                pArg->pBoardInfo->BoardId)
            {
                goto end;
            }
        }
        else if (NvFlashIStrcmp(pPairs[i].key, "board sku") == 0)
        {
            NvBool Found = NvFlashStringContains(pPairs[i].value,
                                                 pArg->pBoardInfo->Sku);
            if (Found == NV_FALSE)
                goto end;
        }
        else if (NvFlashIStrcmp(pPairs[i].key, "default sku") == 0)
        {
            pArg->DefaultSku = NvUStrtoul(pPairs[i].value, NULL, 0);
        }
        else
        {
            NvAuPrintf("Unknown token %s\n", pPairs[i].key);
            return P_ERROR;
        }
    }

end:
    return P_CONTINUE;
}

/**
 * Information like cpu_speedo_min, cpu_speedo_high etc. are filled from
 * a string of comma seperated values.
 *
 * @param pArray Array in which values to be filled.
 * @param pPair  <key, value> pair of nvflash parser
 */
static void
NvFlashUpdateSpeedoIddqValues(
    NvU32 *pArray,
    NvFlashSectionPair *pPair)
{
    NvS32 i;
    NvS32 j;

    j = 0;
    i = 0;
    j = NvOsStrlen(pPair->key) - 1;
    i = NvUStrtoul(pPair->key + j, NULL, 10) - 1;

    i = i > 0 ? i : 0;

    NvFlashStringToIntArray(pPair->value, pArray + i, 8 - i);
}

/**
 * Callback function for updating ubersku and fuse info.
 *
 * @param pRec  Vector of <key, value> pairs parsed by parser
 * @param pAux  Callback Argument having uber Sku structure which is to be
 *              filled and a boolean value indicating whether the valid sku
 *              entry is found or not.
 *
 * @return      Status to parser whether to continue or stop parsing
 *              and any error.
 */
static NvFlashParserStatus
NvFlashFuseBypassParserCallback(
    NvFlashVector *pRec,
    void *pAux)
{
    NvU32 i = 0;
    NvU32 NumPairs = 0;
    NvS32 CurFuse = -1;
    NvFlashParserStatus Ret = P_CONTINUE;
    NvFlashSectionPair *pPairs  = NULL;
    NvFuseBypassCallbackArg *pArg = NULL;
    NvFuseBypassInfo *pCurBypassInfo = NULL;

    pArg = (NvFuseBypassCallbackArg *) pAux;

    if (pArg->NumSkus >= MAX_SKUS)
    {
        NvAuPrintf("More than 20 skus\n");
        goto fail;
    }

    if (pArg->NumSkus > 0)
        pCurBypassInfo = &(pArg->FuseBypassInfo[pArg->NumSkus -1]);

    /* If any sub section to be parsed */
    if (pArg->pSub != NULL)
        return (*(pArg->pSub))(pRec, pAux);

    NumPairs = pRec->n;
    pPairs  = pRec->data;

    /* Parse the mapping section of bypass file */
    if (NumPairs > 0 && NvFlashIStrcmp(pPairs[0].key, "sku name") == 0)
    {
        if (NvFlashIStrcmp(pPairs[0].value, "sku id") != 0)
        {
            NvAuPrintf("Invalid token %s\n", pPairs[0].value);
            return P_ERROR;
        }

        for (i = 1; i < NumPairs; i++)
        {
            NvOsStrncpy(s_Mappings[i - 1].SkuName, pPairs[i].key, 50);
            s_Mappings[i - 1].SkuId = NvUStrtoul(pPairs[i].value, NULL, 0);
        }
        return P_CONTINUE;
    }

    for (i = 0; i < NumPairs; i++)
    {
        if (NvFlashIStrcmp(pPairs[i].key, "policy") == 0)
        {
            if (NvFlashIStrcmp(pPairs[i].value, "default skus") == 0)
            {
                pArg->pSub = &NvFlashGetDefaultSkus;
            }
            else if (NvFlashIStrcmp(pPairs[i].value, "supported skus") == 0)
            {
                pArg->pSub = &NvFlashGetSupportedSkus;
            }
            else
            {
                NvAuPrintf("Invalid sku fuse bypass policy\n");
                return P_ERROR;
            }
            break;
        }
        else if (NvFlashIStrcmp(pPairs[i].key, "sku") == 0)
        {
            NvU32 SkuType = 0;

            SkuType = NvUStrtoul(pPairs[i].value, NULL, 0);
            if (SkuType == 0)
            {
                NvAuPrintf("Unknown sku in config file %s\n", pPairs[i].value);
                goto fail;
            }

            pArg->FuseBypassInfo[pArg->NumSkus].sku_type = SkuType;
            pArg->NumSkus++;
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "cpu_speedo_min", 14) == 0)
        {
            NvFlashUpdateSpeedoIddqValues(
                pCurBypassInfo->cpu_speedo_min, &pPairs[i]
            );
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "cpu_speedo_max", 14) == 0)
        {
            NvFlashUpdateSpeedoIddqValues(
                pCurBypassInfo->cpu_speedo_max, &pPairs[i]
            );
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "cpu_iddq_min", 12) == 0)
        {
            NvFlashUpdateSpeedoIddqValues(
                pCurBypassInfo->cpu_iddq_min, &pPairs[i]
            );
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "cpu_iddq_max", 12) == 0)
        {
            NvFlashUpdateSpeedoIddqValues(
                pCurBypassInfo->cpu_iddq_max, &pPairs[i]
            );
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "soc_speedo_min", 14) == 0)
        {
            NvFlashUpdateSpeedoIddqValues(
                pCurBypassInfo->soc_speedo_min, &pPairs[i]
            );
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "soc_speedo_max", 14) == 0)

        {
            NvFlashUpdateSpeedoIddqValues(
                pCurBypassInfo->soc_speedo_max, &pPairs[i]
            );
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "soc_iddq_min", 12) == 0)
        {
            NvFlashUpdateSpeedoIddqValues(
                pCurBypassInfo->soc_iddq_min, &pPairs[i]
            );
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "soc_iddq_max", 12) == 0)
        {
            NvFlashUpdateSpeedoIddqValues(
                pCurBypassInfo->soc_iddq_max, &pPairs[i]
            );
        }
        else if (NvFlashIStrncmp(pPairs[i].key, "fuse", 4) == 0)
        {
            CurFuse = pCurBypassInfo->num_fuses;
            if (pCurBypassInfo->num_fuses > 8)
            {
                NvAuPrintf("More than 8 fuses\n");
                goto fail;
            }

            pCurBypassInfo->num_fuses++;
        }
        else if (NvFlashIStrcmp(pPairs[i].key, "value") == 0)
        {
            if (CurFuse == -1)
            {
                NvAuPrintf("fuse token is missing\n");
                goto fail;
            }

            pCurBypassInfo->fuses[CurFuse].fuse_value[0] =
                                    NvUStrtoul(pPairs[i].value, NULL, 0);
        }
        else if (NvFlashIStrcmp(pPairs[i].key, "offset") == 0)
        {
            if (CurFuse == -1)
            {
                NvAuPrintf("fuse token is missing\n");
                goto fail;
            }

            pCurBypassInfo->fuses[CurFuse].offset =
                                    NvUStrtoul(pPairs[i].value, NULL, 0);
        }
        else
        {
            NvAuPrintf("Uknown Token %s\n", pPairs[i].key);
            goto fail;
        }
    }
    goto clean;

fail:
    Ret = P_STOP;
clean:
    return Ret;
}

/**
 * Checks the speedo limits
 *
 * @param pBoardDetails Structure containing speedo/iddq values of board
 * @param pBypassInfo   Structure containing speedo/iddq requirement of sku
 *
 * @return              NV_TRUE of board can be bypassed with given sku info
 */
static NvBool
NvFlashCheckSpeedoLimits(
    Nv3pBoardDetails *pBoardDetails,
    NvFuseBypassInfo *pBypassInfo)
{
    NvBool Ret = NV_TRUE;

    CHECK_RANGE(pBoardDetails->CpuSpeedo[0], pBypassInfo->cpu_speedo_min[0],
                pBypassInfo->cpu_speedo_max[0]);
    CHECK_RANGE(pBoardDetails->CpuSpeedo[1], pBypassInfo->cpu_speedo_min[1],
                pBypassInfo->cpu_speedo_max[1]);
    CHECK_RANGE(pBoardDetails->CpuSpeedo[2], pBypassInfo->cpu_speedo_min[2],
                pBypassInfo->cpu_speedo_max[2]);
    CHECK_RANGE(pBoardDetails->CpuIddq, pBypassInfo->cpu_iddq_min[0],
                pBypassInfo->cpu_iddq_max[0]);
    CHECK_RANGE(pBoardDetails->SocSpeedo[0], pBypassInfo->soc_speedo_min[0],
                pBypassInfo->soc_speedo_max[0]);
    CHECK_RANGE(pBoardDetails->SocSpeedo[1], pBypassInfo->soc_speedo_min[1],
                pBypassInfo->soc_speedo_max[1]);
    CHECK_RANGE(pBoardDetails->SocSpeedo[2], pBypassInfo->soc_speedo_min[2],
                pBypassInfo->soc_speedo_max[2]);
    CHECK_RANGE(pBoardDetails->SocIddq, pBypassInfo->soc_iddq_min[0],
                pBypassInfo->soc_iddq_max[0]);

    return Ret;
}

NvError NvFlashParseFusebypass(void)
{
    NvError e = NvSuccess;
    NvOsFileHandle hFile = 0;
    NvFuseBypassCallbackArg CallBackArg;
    NvFlashFuseBypass FusebypassOpt;
    char *pErrStr = NULL;
    NvFuseBypassInfo *pBypassInfo = NULL;
    NvU32 TargetSku = 0;
    NvBool PrintSpeedos = NV_FALSE;
    char *pEnd;

    /* Retrieve the fusebypass options */
    e = NvFlashCommandGetOption(NvFlashOption_FuseBypass,
                                (void *)&FusebypassOpt);
    VERIFY(e == NvSuccess, pErrStr = "Fuse bypass option failed"; goto fail);

    /* If sku_to_bypass option is not provided in command line
     * then do not take any actions for sku bypassing
     */
    if (FusebypassOpt.pSkuId == NULL)
        return NvSuccess;

    /* if 0 is given for sku_to_bypass then do not take any
     * actions for sku bypassing
     */
    TargetSku = NvUStrtoul(FusebypassOpt.pSkuId, &pEnd, 0);
    if (TargetSku == 0 && *pEnd == '\0')
        return NvSuccess;

    /* If board is not pre production device then return success and skip
     * selection if force download is not specified
     */
    if (s_PlatformInfo.OperatingMode != NvDdkFuseOperatingMode_Preproduction &&
        FusebypassOpt.ForceDownload == NV_FALSE)
    {
        NvAuPrintf("Warning: Device is not a pre-production device, "
                   "skipping sku bypass\n");
        return NvSuccess;
    }

    /* Use default fuse bypass config file if not provided */
    if (FusebypassOpt.config_file == NULL)
    {
        FusebypassOpt.config_file = "fuse_bypass.txt";
        NvAuPrintf("Using %s as fuse bypass config file\n",
                   FusebypassOpt.config_file);
    }

    NvOsMemset(&CallBackArg, 0, sizeof(CallBackArg));
    e = NvOsFopen(FusebypassOpt.config_file, NVOS_OPEN_READ, &hFile);
    VERIFY(e == NvSuccess,
           pErrStr = "Error: Fuse bypass file open failed"; goto fail);

    /* Parse the fuse_bypass.txt */
    CallBackArg.pBoardInfo = &g_BoardDetails.BoardInfo;
    e = nvflash_parser(hFile, &NvFlashFuseBypassParserCallback,
                       NULL, &CallBackArg);
    VERIFY(e == NvSuccess, goto fail);

    /* If chip's SKU_INFO is fused then return error and skip selection
     * if force download is not specified.
     */
    if (g_BoardDetails.ChipSku != 0 && FusebypassOpt.ForceDownload == NV_FALSE)
    {
        NvAuPrintf("ERROR: SKU_INFO is already fused with %s: 0x%02x\n",
                   NvFlashGetSkuName(g_BoardDetails.ChipSku),
                   g_BoardDetails.ChipSku);
        e = NvError_BadParameter;
        goto fail;
    }

    /* If sku auto selection is used then iterate over the supported
     * skus and select the sku which board is capable of
     */
    if (NvFlashIStrcmp(FusebypassOpt.pSkuId, "auto") == 0)
    {
        NvU32 i;
        NvU32 j;

        if (CallBackArg.NumSupportedSkus == 0)
        {
            NvAuPrintf("ERROR: Board %d does not support any skus\n\n",
                        CallBackArg.pBoardInfo->BoardId);
            e = NvError_BadParameter;
            goto fail;
        }

        for (i = 0; i < CallBackArg.NumSupportedSkus; i++)
        {
            for (j = 0; j < CallBackArg.NumSkus; j++)
            {
                pBypassInfo = &CallBackArg.FuseBypassInfo[j];

                if (CallBackArg.SupportedSkus[i] ==
                    pBypassInfo->sku_type)
                {
                    break;
                }
            }

            if (j < CallBackArg.NumSkus)
            {
                NvBool ValidSku;

                ValidSku = NvFlashCheckSpeedoLimits(&g_BoardDetails,
                                                    pBypassInfo);
                if (ValidSku)
                    break;
            }
        }

        if (i >= CallBackArg.NumSupportedSkus)
        {
            pBypassInfo = NULL;
            /* If no board is not capable of any skus and forcebypass is
             * provided with auto then force bypass with default sku.
             */
            if (FusebypassOpt.force_bypass)
            {
                TargetSku = CallBackArg.DefaultSku;
            }
            else
            {
                NvAuPrintf("ERROR: Board is not capable of any skus\n\n");
                PrintSpeedos = NV_TRUE;
                e = NvError_BadParameter;
                goto fail;
            }
        }
    }
    else
    {
        TargetSku = NvFlashGetSkuId(FusebypassOpt.pSkuId);
        if (TargetSku == 0)
        {
            NvAuPrintf("Error: Unsupported SKU: %s\n", FusebypassOpt.pSkuId);
            e = NvError_BadParameter;
            goto fail;
        }
    }

    if (TargetSku != 0)
    {
        NvU32 i;
        NvBool ValidSku = NV_FALSE;

        for (i = 0; i < CallBackArg.NumSkus; i++)
        {
            pBypassInfo = &CallBackArg.FuseBypassInfo[i];

            if (TargetSku == pBypassInfo->sku_type)
                break;
        }

        if (i < CallBackArg.NumSkus)
        {
            ValidSku = NvFlashCheckSpeedoLimits(&g_BoardDetails, pBypassInfo);
        }
        else
        {
            NvAuPrintf("ERROR: Sku %d for Board %d is not "
                       "present in %s\n", TargetSku,
                       g_BoardDetails.BoardInfo.BoardId,
                       FusebypassOpt.config_file);
            e = NvError_BadParameter;
            goto fail;
        }

        if (ValidSku == NV_FALSE)
        {
            if (FusebypassOpt.force_bypass)
            {
                NvAuPrintf("WARNING: Board %d does not meet the speedo/iddq "
                           "requirements of SKU 0x%02x\n",
                            CallBackArg.pBoardInfo->BoardId, TargetSku);
            }
            else
            {
                NvAuPrintf("ERROR: Board %d does not meet the speedo/iddq "
                           "requirements of SKU 0x%02x\n\n",
                            CallBackArg.pBoardInfo->BoardId, TargetSku);
                PrintSpeedos = NV_TRUE;
                e = NvError_BadParameter;
                goto fail;
            }
        }
    }

    if (pBypassInfo)
    {
        NvAuPrintf("Bypassing SKU 0x%02x\n", pBypassInfo->sku_type);
        pBypassInfo->force_bypass = FusebypassOpt.force_bypass;
        pBypassInfo->force_download = FusebypassOpt.ForceDownload;
        NvOsMemcpy(&s_FuseBypassInfo, pBypassInfo, sizeof(s_FuseBypassInfo));

        g_BoardDetails.ChipSku = pBypassInfo->sku_type;
    }

fail:

    if (PrintSpeedos)
    {
        NvAuPrintf("CPU SPEEDO : %d, %d, %d\n", g_BoardDetails.CpuSpeedo[0],
                   g_BoardDetails.CpuSpeedo[1], g_BoardDetails.CpuSpeedo[2]);
        NvAuPrintf("CPU IDDQ   : %d\n", g_BoardDetails.CpuIddq);
        NvAuPrintf("SOC SPEEDO : %d, %d, %d\n", g_BoardDetails.SocSpeedo[0],
                   g_BoardDetails.SocSpeedo[1], g_BoardDetails.SocSpeedo[2]);
        NvAuPrintf("SOC IDDQ   : %d\n", g_BoardDetails.CpuIddq);
    }

    if (pErrStr)
        NvAuPrintf("%s\n", pErrStr);

    if (hFile)
        NvOsFclose(hFile);

    return e;
}

NvBool NvFlashSendFuseBypassInfo(NvU32 ParitionId, NvU64 PartitionSize)
{
    NvError e = NvSuccess;
    NvBool Ret  = NV_TRUE;
    const char *pErrStr = NULL;
    NvU32 Size = 0;
    Nv3pCmdStatus *pStatus = NULL;
    Nv3pCommand Cmd;
    Nv3pCmdQueryPartition QueryPartArg;
    Nv3pCmdDownloadPartition DownloadPartArg;

    if (s_FuseBypassInfo.sku_type == 0)
    {
        return NV_TRUE;
    }

    Size = sizeof(s_FuseBypassInfo);

    if (!PartitionSize)
    {
        QueryPartArg.Id = ParitionId;
        Cmd = Nv3pCommand_QueryPartition;
        NV_CHECK_ERROR_CLEANUP(
            Nv3pCommandSend(s_hSock, Cmd, &QueryPartArg, 0)
        );

        /* wait for status */
        Ret = nvflash_wait_status();
        VERIFY(Ret, pErrStr = "failed to query partition"; goto fail);
        PartitionSize = QueryPartArg.Size;
    }

    if (Size > PartitionSize)
    {
        pErrStr = "Fuse partition is smaller than structure Size";
        goto fail;
    }

    NvAuPrintf("sending fuse bypass information\n");

    Cmd = Nv3pCommand_DownloadPartition;
    DownloadPartArg.Id = ParitionId;
    DownloadPartArg.Length = Size;

    NV_CHECK_ERROR_CLEANUP(
        Nv3pCommandSend(s_hSock, Cmd, &DownloadPartArg, 0)
    );

    e = Nv3pDataSend(s_hSock, (NvU8 *) &s_FuseBypassInfo, Size, 0);
    VERIFY(e == NvSuccess, pErrStr = "Fuse data send failed"; goto fail);

    /* Get the status from bootloader to check for any errors/warnings */
    e = Nv3pCommandReceive(s_hSock, &Cmd, (void **)&pStatus, 0 );
    VERIFY(e == NvSuccess, goto fail);
    VERIFY(Cmd == Nv3pCommand_Status, goto fail);

    e = Nv3pCommandComplete(s_hSock, Cmd, pStatus, 0);
    VERIFY(e == NvSuccess, goto fail);

    if (pStatus && pStatus->Code != Nv3pStatus_Ok)
    {
        if (pStatus->Code != Nv3pStatus_FuseBypassSpeedoIddqCheckFailure)
        {
            pErrStr = nvflash_status_string(pStatus->Code);
            goto fail;
        }
    }

    NvAuPrintf("Fuses bypassed successfully\n");
    goto clean;

fail:
    Ret = NV_FALSE;
    if (pErrStr)
    {
        NvAuPrintf("%s\n", pErrStr);
    }

clean:
    return Ret;
}

void NvFlashWriteFusesToWarmBoot(NvU32 *pFuseWriteStart)
{
    NvU32 i = 0;
    NvS32 ProductionModeFuseIndex = -1;

    *pFuseWriteStart = s_FuseBypassInfo.num_fuses;
    pFuseWriteStart++;

    for (i = 0; i < s_FuseBypassInfo.num_fuses; i++)
    {
        if (s_FuseBypassInfo.fuses[i].offset == FUSE_PRODUCTION_MODE_0)
        {
            ProductionModeFuseIndex = i;
            continue;
        }
        pFuseWriteStart[0] = s_FuseBypassInfo.fuses[i].offset;
        pFuseWriteStart[1] = s_FuseBypassInfo.fuses[i].fuse_value[0];
        pFuseWriteStart += 2;
    }

    if (ProductionModeFuseIndex != -1)
    {
        i = (NvU32)ProductionModeFuseIndex;
        pFuseWriteStart[0] = s_FuseBypassInfo.fuses[i].offset;
        pFuseWriteStart[1] = s_FuseBypassInfo.fuses[i].fuse_value[0];
    }
}

