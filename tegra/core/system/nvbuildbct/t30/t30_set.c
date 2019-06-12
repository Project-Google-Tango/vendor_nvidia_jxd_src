/**
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "math.h"


#include "nvapputil.h"
#include "nvassert.h"
#include "t30_set.h"
#include "t30/nvboot_bct.h"
#include "t30/nvboot_sdram_param.h"
#include "../nvbuildbct.h"
#include "t30_data_layout.h"
#include "nvbct_customer_platform_data.h"
#include "../nvbuildbct_util.h"

#define NV_MAX(a, b) (((a) > (b)) ? (a) : (b))

void
t30ComputeRandomAesBlock(BuildBctContext *Context)
{
    NvU32 i;

    for (i = 0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
    {
        ((NvBootHash*)(Context->RandomBlock))->hash[i] = rand();
    }
}

int
t30SetRandomAesBlock(BuildBctContext *Context, NvU32 Value, NvU8* AesBlock)
{
    Context->RandomBlockType = (RandomAesBlockType)Value;

    switch (Context->RandomBlockType)
    {
    case RandomAesBlockType_Zeroes:
        NvOsMemset(&(Context->RandomBlock[0]), 0, CMAC_HASH_LENGTH_BYTES);
        break;

    case RandomAesBlockType_Literal:
        NvOsMemcpy(&(Context->RandomBlock[0]), AesBlock, CMAC_HASH_LENGTH_BYTES);
        break;

    case RandomAesBlockType_Random:
        /* This type will be recomputed whenever needed. */
        break;

    case RandomAesBlockType_RandomFixed:
        /* Compute this once now. */
        t30ComputeRandomAesBlock(Context);
        break;

    default:
        NV_ASSERT(!"Unknown random block type.");
        return 1;
    }

    return 0;
}

int
t30SetBootLoader(BuildBctContext *Context,
              NvU8              *Filename,
              NvU32              LoadAddress,
              NvU32              EntryPoint)
{
    NvOsMemcpy(Context->NewBlFilename, Filename, MAX_BUFFER);
    Context->NewBlLoadAddress = LoadAddress;
    Context->NewBlEntryPoint = EntryPoint;
    Context->NewBlStartBlk = 0;
    Context->NewBlStartPg = 0;
    t30UpdateBl(Context, (UpdateEndState)0);

    return 0;
}

int
t30SetSdmmcParam(BuildBctContext *Context,
                 NvU32              Index,
                 ParseToken         Token,
                 NvU32              Value)
{
    NvBootSdmmcParams *Params;
    NvBootConfigTable *Bct = NULL;
    Bct = (NvBootConfigTable*)(Context->NvBCT);

    NV_ASSERT(Context      != NULL);
    NV_ASSERT(Bct != NULL);

    Params = &(Bct->DevParams[Index].SdmmcParams);

    Bct->NumParamSets = NV_MAX(Bct->NumParamSets, Index + 1);

    /* The result of the TODO processing is to get a token & value. */
    switch (Token)
    {
    case Token_ClockDivider:
        Params->ClockDivider = Value;
        break;

    case Token_DataWidth:
        if((Value != 0) && (Value != 1) && (Value != 2)
            && (Value != 5) && (Value != 6))
        {
            NvAuPrintf("\nError: Value entered for DeviceParam[%d].SdmmcParams."
            "DataWidth is not valid. Valid values are 0, 1, 2, 5 & 6. \n "
            ,Index);
            NvAuPrintf("\nBCT file was not created. \n");
            exit(1);
        }
        Params->DataWidth = Value;
        break;
    case Token_MaxPowerClassSupported:
        Params->MaxPowerClassSupported = Value;
        break;
    case Token_SdController:
        if((Value != 0) && (Value != 1))
        {
            NvAuPrintf("\nError: Value entered for DeviceParam[%d].SdmmcParams."
            "SdController is not valid. Valid values are 0 & 1. \n ",Index);
            NvAuPrintf("\nBCT file was not created. \n");
            exit(1);
        }
        Params->SdController = Value;
        break;
    default:
        NvAuPrintf("Unexpected token %d at line %d\n",
                   Token, __LINE__);

    return 1;
    }

    return 0;
}

int
t30SetNandParam(BuildBctContext *Context,
             NvU32              Index,
             ParseToken         Token,
             NvU32              Value)
{
    NvBootNandParams *Params;
    NvBootConfigTable *Bct = NULL;

    Bct = (NvBootConfigTable*)(Context->NvBCT);

    NV_ASSERT(Context      != NULL);
    NV_ASSERT(Bct != NULL);

    Params = &(Bct->DevParams[Index].NandParams);

    Bct->NumParamSets = NV_MAX(Bct->NumParamSets, Index + 1);

    /* The result of the TODO processing is to get a token & value. */
    switch (Token)
    {
    case Token_ClockDivider:
        Params->ClockDivider = (NvU8)Value;
        break;

    case Token_NandAsyncTiming0:
        Params->NandAsyncTiming0 = Value;
        break;

    case Token_NandAsyncTiming1:
        Params->NandAsyncTiming1 = Value;
        break;

    case Token_NandAsyncTiming2:
        Params->NandAsyncTiming2 = Value;
        break;

    case Token_NandAsyncTiming3:
        Params->NandAsyncTiming3 = Value;
        break;

    case Token_NandSDDRTiming0:
        Params->NandSDDRTiming0 = Value;
        break;

    case Token_NandSDDRTiming1:
        Params->NandSDDRTiming1 = Value;
        break;

    case Token_NandTDDRTiming0:
        Params->NandTDDRTiming0 = Value;
        break;

    case Token_NandTDDRTiming1:
        Params->NandTDDRTiming1 = Value;
        break;

    case Token_DisableSyncDDR:
        Params->DisableSyncDDR = Value;
        break;

    case Token_NandFbioDqsibDlyByte:
        Params->NandFbioDqsibDlyByte = Value;
        break;

    case Token_NandFbioQuseDlyByte:
        Params->NandFbioQuseDlyByte = Value;
        break;

    case Token_NandFbioCfgQuseLate:
        Params->NandFbioCfgQuseLate = Value;
        break;

    case Token_BlockSizeLog2:
        Params->BlockSizeLog2 = (NvU8)Value;
        break;

    case Token_PageSizeLog2:
        Params->PageSizeLog2 = (NvU8)Value;
        break;

    default:
        NvAuPrintf("Unexpected token %d at line %d\n",
                   Token, __LINE__);

    return 1;
    }

    return 0;
}

int
t30SetNorParam(BuildBctContext *Context,
            NvU32              Index,
            ParseToken         Token,
            NvU32              Value)
{
    //NvBootSnorParams *Params;
    NvBootConfigTable *Bct = NULL;
    Bct = (NvBootConfigTable*)(Context->NvBCT);

    NV_ASSERT(Context      != NULL);
    NV_ASSERT(Bct != NULL);

    //Params = &(Bct->DevParams[Index].SnorParams);

    Bct->NumParamSets = NV_MAX(Bct->NumParamSets, Index + 1);

    /* The result of the TODO processing is to get a token & value. */
    switch (Token)
    {
    case Token_NorTiming:
//        Params->NorTiming = Value;
        break;

    default:
        NvAuPrintf("Unexpected token %d at line %d\n",
                   Token, __LINE__);

        return 1;
    }

    return 0;
}

int
t30SetSpiFlashParam(BuildBctContext *Context,
                 NvU32              Index,
                 ParseToken         Token,
                 NvU32              Value)
{
    NvBootSpiFlashParams *Params;
    NvBootConfigTable *Bct = NULL;
    Bct = (NvBootConfigTable*)(Context->NvBCT);

    NV_ASSERT(Context      != NULL);
    NV_ASSERT(Bct != NULL);

    Params = &(Bct->DevParams[Index].SpiFlashParams);

    Bct->NumParamSets = NV_MAX(Bct->NumParamSets, Index + 1);

    /* The result of the TODO processing is to get a token & value. */
    switch (Token)
    {

    case Token_ClockSource:
        Params->ClockSource = (NvBootSpiClockSource)Value;
        break;

    case Token_ClockDivider:
        if((Value != 9) && (Value != 11) && (Value != 16) && (Value != 22))
        {
            NvAuPrintf("\nError: Value entered for DeviceParam[%d].SpiFlashParams"
                        ".ClockDivider is not valid. Valid values are 9, 11, 16 &"
                        "22. \n ",Index);
            NvAuPrintf("\nBCT file was not created. \n");
            exit(1);
        }
        Params->ClockDivider = (NvU8)Value;
        break;

    case Token_ReadCommandTypeFast:
        Params->ReadCommandTypeFast = (NvBool)Value;
        break;

    default:
        NvAuPrintf("Unexpected token %d at line %d\n",
                   Token, __LINE__);

        return 1;
    }

    return 0;
}

int
t30SetSataParam(BuildBctContext *Context,
             NvU32              Index,
             ParseToken         Token,
             NvU32              Value)
{
    NvBootSataParams *Params;
    NvBootConfigTable *Bct = NULL;

    Bct = (NvBootConfigTable*)(Context->NvBCT);

    NV_ASSERT(Context      != NULL);
    NV_ASSERT(Bct != NULL);

    Params = &(Bct->DevParams[Index].SataParams);

    Bct->NumParamSets = NV_MAX(Bct->NumParamSets, Index + 1);

    /* The result of the TODO processing is to get a token & value. */
    switch (Token)
    {
    case Token_SataClockSource:
        if (Value >= NvBootSataClockSource_Num)
        {
            NvAuPrintf("\nError: Value entered for DeviceParam[%d].SataParams"
                         ".SataClockSource is not valid. Valid values are 1,2 &"
                         " 3. \n ",Index);
            NvAuPrintf("\nBCT file was not created. \n");
           exit(1);
        }
        Params->SataClockSource = Value;
        break;

    case Token_SataClockDivider:
        Params->SataClockDivider = Value;
        break;

    case Token_SataOobClockSource:
        if (Value >= NvBootSataClockSource_Num)
        {
            NvAuPrintf("\nError: Value entered for DeviceParam[%d].SataParams"
                         ".SataOobClockSource is not valid. Valid values are 1,2 &"
                         " 3. \n ", Index);
            NvAuPrintf("\nBCT file was not created. \n");
            exit(1);
        }
        Params->SataOobClockSource = Value;
        break;

    case Token_SataOobClockDivider:
        Params->SataOobClockDivider = Value;
        break;

    case Token_SataMode:
        if (Value >= NvBootSataMode_Num)
        {
            NvAuPrintf("\nError: Value entered for DeviceParam[%d].SataParams"
                         ".SataMode is not valid. Valid values are 1 & 2. \n ", Index);
            NvAuPrintf("\nBCT file was not created. \n");
            exit(1);
        }
        Params->SataMode = Value;
        break;

    case Token_TransferMode:
        if (Value >= NvBootSataTransferMode_Num)
        {
            NvAuPrintf("\nError: Value entered for DeviceParam[%d].SataParams"
                        ".TransferMode is not valid. Valid values are 1 & 2. \n ", Index);
            NvAuPrintf("\nBCT file was not created. \n");
            exit(1);
        }
        Params->TransferMode = Value;
        break;

    case Token_isSdramInitialized:
        Params->isSdramInitialized = Value;
        break;

    case Token_SataBuffersBase:
        Params->SataBuffersBase = Value;
        break;

    case Token_DataBufferBase:
        Params->DataBufferBase = Value;
        break;

    case Token_PlleDivM:
        Params->PlleDivM = Value;
        break;

    case Token_PlleDivN:
        Params->PlleDivN = Value;
        break;

    case Token_PlleDivPl:
        Params->PlleDivPl = Value;
        break;

    case Token_PlleDivPlCml:
        Params->PlleDivPlCml = Value;
        break;

    case Token_PlleSSCoeffSscmax:
        Params->PlleSSCoeffSscmax = Value;
        break;

    case Token_PlleSSCoeffSscinc:
        Params->PlleSSCoeffSscinc = Value;
        break;

    case Token_PlleSSCoeffSscincintrvl:
        Params->PlleSSCoeffSscincintrvl = Value;
        break;

    default:
        NvAuPrintf("Unexpected token %d at line %d\n",
                   Token, __LINE__);

    return 1;
    }

    return 0;
}

#define SET_PARAM_U32(z) case Token_##z: Params->z = Value; break
#define SET_PARAM_ENUM(z,t) case Token_##z: Params->z = (t)Value; break
#define SET_PARAM_BOOL(z) case Token_##z: Params->z = (NvBool)Value; break

int
t30SetSdramParam(BuildBctContext *Context,
              NvU32              Index,
              ParseToken         Token,
              NvU32              Value)
{
    NvBootSdramParams *Params;
    NvBootConfigTable *Bct = NULL;
    Bct = (NvBootConfigTable*)(Context->NvBCT);

    NV_ASSERT(Context      != NULL);
    NV_ASSERT(Bct != NULL);

    Params = &(Bct->SdramParams[Index]);

    Bct->NumSdramSets = NV_MAX(Bct->NumSdramSets, Index + 1);

    switch (Token)
    {
        SET_PARAM_U32(MemoryType);
        SET_PARAM_U32(PllMChargePumpSetupControl);
        SET_PARAM_U32(PllMLoopFilterSetupControl);
        SET_PARAM_U32(PllMInputDivider);
        SET_PARAM_U32(PllMFeedbackDivider);
        SET_PARAM_U32(PllMPostDivider);
        SET_PARAM_U32(PllMStableTime);
        SET_PARAM_U32(EmcClockDivider);
        SET_PARAM_U32(EmcClockSource);
        SET_PARAM_U32(EmcClockUsePllMUD);
        SET_PARAM_U32(EmcAutoCalInterval);
        SET_PARAM_U32(EmcAutoCalConfig);
        SET_PARAM_U32(EmcAutoCalWait);
        SET_PARAM_U32(EmcAdrCfg);
        SET_PARAM_U32(EmcPinProgramWait);
        SET_PARAM_U32(EmcPinExtraWait);
        SET_PARAM_U32(EmcTimingControlWait);
        SET_PARAM_U32(EmcRc);
        SET_PARAM_U32(EmcRfc);
        SET_PARAM_U32(EmcRas);
        SET_PARAM_U32(EmcRp);
        SET_PARAM_U32(EmcR2w);
        SET_PARAM_U32(EmcW2r);
        SET_PARAM_U32(EmcR2p);
        SET_PARAM_U32(EmcW2p);
        SET_PARAM_U32(EmcRdRcd);
        SET_PARAM_U32(EmcWrRcd);
        SET_PARAM_U32(EmcRrd);
        SET_PARAM_U32(EmcRext);
        SET_PARAM_U32(EmcWext);
        SET_PARAM_U32(EmcWdv);
        SET_PARAM_U32(EmcQUse);
        SET_PARAM_U32(EmcQRst);
        SET_PARAM_U32(EmcQSafe);
        SET_PARAM_U32(EmcRdv);
        SET_PARAM_U32(EmcCtt);
        SET_PARAM_U32(EmcCttDuration);
        SET_PARAM_U32(EmcRefresh);
        SET_PARAM_U32(EmcBurstRefreshNum);
        SET_PARAM_U32(EmcPreRefreshReqCnt);
        SET_PARAM_U32(EmcPdEx2Wr);
        SET_PARAM_U32(EmcPdEx2Rd);
        SET_PARAM_U32(EmcPChg2Pden);
        SET_PARAM_U32(EmcAct2Pden);
        SET_PARAM_U32(EmcAr2Pden);
        SET_PARAM_U32(EmcRw2Pden);
        SET_PARAM_U32(EmcTxsr);
        SET_PARAM_U32(EmcTxsrDll);
        SET_PARAM_U32(EmcTcke);
        SET_PARAM_U32(EmcTfaw);
        SET_PARAM_U32(EmcTrpab);
        SET_PARAM_U32(EmcTClkStable);
        SET_PARAM_U32(EmcTClkStop);
        SET_PARAM_U32(EmcTRefBw);
        SET_PARAM_U32(EmcQUseExtra);
        SET_PARAM_U32(EmcFbioCfg5);
        SET_PARAM_U32(EmcFbioCfg6);
        SET_PARAM_U32(EmcFbioSpare);
        SET_PARAM_U32(EmcCfgRsv);
        SET_PARAM_U32(EmcMrs);
        SET_PARAM_U32(EmcEmrs);
        SET_PARAM_U32(EmcMrw1);
        SET_PARAM_U32(EmcMrw2);
        SET_PARAM_U32(EmcMrw3);
        SET_PARAM_U32(EmcMrwExtra);
        SET_PARAM_U32(EmcWarmBootMrw1);
        SET_PARAM_U32(EmcWarmBootMrw2);
        SET_PARAM_U32(EmcWarmBootMrw3);
        SET_PARAM_U32(EmcWarmBootMrwExtra);
        SET_PARAM_U32(EmcWarmBootExtraModeRegWriteEnable);
        SET_PARAM_U32(EmcExtraModeRegWriteEnable);
        SET_PARAM_U32(EmcMrwResetCommand);
        SET_PARAM_U32(EmcMrwResetNInitWait);
        SET_PARAM_U32(EmcMrsWaitCnt);
        SET_PARAM_U32(EmcCfg);
        SET_PARAM_U32(EmcCfg2);
        SET_PARAM_U32(EmcDbg);
        SET_PARAM_U32(EmcCmdQ);
        SET_PARAM_U32(EmcMc2EmcQ);
        SET_PARAM_U32(EmcDynSelfRefControl);
        SET_PARAM_U32(AhbArbitrationXbarCtrlMemInitDone);
        SET_PARAM_U32(EmcCfgDigDll);
        SET_PARAM_U32(EmcCfgDigDllPeriod);
        SET_PARAM_U32(EmcDevSelect);
        SET_PARAM_U32(EmcSelDpdCtrl);
        SET_PARAM_U32(EmcDllXformDqs0);
        SET_PARAM_U32(EmcDllXformDqs1);
        SET_PARAM_U32(EmcDllXformDqs2);
        SET_PARAM_U32(EmcDllXformDqs3);
        SET_PARAM_U32(EmcDllXformDqs4);
        SET_PARAM_U32(EmcDllXformDqs5);
        SET_PARAM_U32(EmcDllXformDqs6);
        SET_PARAM_U32(EmcDllXformDqs7);
        SET_PARAM_U32(EmcDllXformQUse0);
        SET_PARAM_U32(EmcDllXformQUse1);
        SET_PARAM_U32(EmcDllXformQUse2);
        SET_PARAM_U32(EmcDllXformQUse3);
        SET_PARAM_U32(EmcDllXformQUse4);
        SET_PARAM_U32(EmcDllXformQUse5);
        SET_PARAM_U32(EmcDllXformQUse6);
        SET_PARAM_U32(EmcDllXformQUse7);
        SET_PARAM_U32(EmcDliTrimTxDqs0);
        SET_PARAM_U32(EmcDliTrimTxDqs1);
        SET_PARAM_U32(EmcDliTrimTxDqs2);
        SET_PARAM_U32(EmcDliTrimTxDqs3);
        SET_PARAM_U32(EmcDliTrimTxDqs4);
        SET_PARAM_U32(EmcDliTrimTxDqs5);
        SET_PARAM_U32(EmcDliTrimTxDqs6);
        SET_PARAM_U32(EmcDliTrimTxDqs7);
        SET_PARAM_U32(EmcDllXformDq0);
        SET_PARAM_U32(EmcDllXformDq1);
        SET_PARAM_U32(EmcDllXformDq2);
        SET_PARAM_U32(EmcDllXformDq3);
        SET_PARAM_U32(WarmBootWait);
        SET_PARAM_U32(EmcCttTermCtrl);
        SET_PARAM_U32(EmcOdtWrite);
        SET_PARAM_U32(EmcOdtRead);
        SET_PARAM_U32(EmcZcalInterval);
        SET_PARAM_U32(EmcZcalWaitCnt);
        SET_PARAM_U32(EmcZcalMrwCmd);
        SET_PARAM_U32(EmcMrsResetDll);
        SET_PARAM_U32(EmcZcalInitDev0);
        SET_PARAM_U32(EmcZcalInitDev1);
        SET_PARAM_U32(EmcZcalInitWait);
        SET_PARAM_U32(EmcZcalColdBootEnable);
        SET_PARAM_U32(EmcZcalWarmBootEnable);
        SET_PARAM_U32(EmcMrwLpddr2ZcalWarmBoot);
        SET_PARAM_U32(EmcZqCalDdr3WarmBoot);
        SET_PARAM_U32(EmcZcalWarmBootWait);
        SET_PARAM_U32(EmcMrsWarmBootEnable);
        SET_PARAM_U32(EmcMrsResetDllWait);
        SET_PARAM_U32(EmcEmrsEmr2);
        SET_PARAM_U32(EmcEmrsEmr3);
        SET_PARAM_U32(EmcMrsExtra);
        SET_PARAM_U32(EmcWarmBootMrs);
        SET_PARAM_U32(EmcWarmBootEmrs);
        SET_PARAM_U32(EmcWarmBootEmr2);
        SET_PARAM_U32(EmcWarmBootEmr3);
        SET_PARAM_U32(EmcWarmBootMrsExtra);
        SET_PARAM_U32(EmcEmrsDdr2DllEnable);
        SET_PARAM_U32(EmcMrsDdr2DllReset);
        SET_PARAM_U32(EmcEmrsDdr2OcdCalib);
        SET_PARAM_U32(EmcDdr2Wait);
        SET_PARAM_U32(EmcClkenOverride);
        SET_PARAM_U32(EmcExtraRefreshNum);
        SET_PARAM_U32(EmcClkenOverrideAllWarmBoot);
        SET_PARAM_U32(McClkenOverrideAllWarmBoot);
        SET_PARAM_U32(EmcCfgDigDllPeriodWarmBoot);
        SET_PARAM_U32(PmcVddpSel);
        SET_PARAM_U32(PmcDdrPwr);
        SET_PARAM_U32(PmcDdrCfg);
        SET_PARAM_U32(PmcIoDpdReq);
        SET_PARAM_U32(PmcENoVttGen);
        SET_PARAM_U32(PmcNoIoPower);
        SET_PARAM_U32(EmcXm2CmdPadCtrl);
        SET_PARAM_U32(EmcXm2CmdPadCtrl2);
        SET_PARAM_U32(EmcXm2DqsPadCtrl);
        SET_PARAM_U32(EmcXm2DqsPadCtrl2);
        SET_PARAM_U32(EmcXm2DqsPadCtrl3);
        SET_PARAM_U32(EmcXm2DqPadCtrl);
        SET_PARAM_U32(EmcXm2DqPadCtrl2);
        SET_PARAM_U32(EmcXm2ClkPadCtrl);
        SET_PARAM_U32(EmcXm2CompPadCtrl);
        SET_PARAM_U32(EmcXm2VttGenPadCtrl);
        SET_PARAM_U32(EmcXm2VttGenPadCtrl2);
        SET_PARAM_U32(EmcXm2QUsePadCtrl);
        SET_PARAM_U32(McEmemAdrCfg);
        SET_PARAM_U32(McEmemAdrCfgDev0);
        SET_PARAM_U32(McEmemAdrCfgDev1);
        SET_PARAM_U32(McEmemCfg);
        SET_PARAM_U32(McEmemArbCfg);
        SET_PARAM_U32(McEmemArbOutstandingReq);
        SET_PARAM_U32(McEmemArbTimingRcd);
        SET_PARAM_U32(McEmemArbTimingRp);
        SET_PARAM_U32(McEmemArbTimingRc);
        SET_PARAM_U32(McEmemArbTimingRas);
        SET_PARAM_U32(McEmemArbTimingFaw);
        SET_PARAM_U32(McEmemArbTimingRrd);
        SET_PARAM_U32(McEmemArbTimingRap2Pre);
        SET_PARAM_U32(McEmemArbTimingWap2Pre);
        SET_PARAM_U32(McEmemArbTimingR2R);
        SET_PARAM_U32(McEmemArbTimingW2W);
        SET_PARAM_U32(McEmemArbTimingR2W);
        SET_PARAM_U32(McEmemArbTimingW2R);
        SET_PARAM_U32(McEmemArbDaTurns);
        SET_PARAM_U32(McEmemArbDaCovers);
        SET_PARAM_U32(McEmemArbMisc0);
        SET_PARAM_U32(McEmemArbMisc1);
        SET_PARAM_U32(McEmemArbRing1Throttle);
        SET_PARAM_U32(McEmemArbOverride);
        SET_PARAM_U32(McEmemArbRsv);
        SET_PARAM_U32(McClkenOverride);

        default:
            NvAuPrintf("Unexpected token %d at line %d\n",
                       Token, __LINE__);
            return 1;
    }

    return 0;
}

int t30SetValue(BuildBctContext *Context, ParseToken Token, NvU32 Value)
{
    NV_ASSERT(Context != NULL);

    switch (Token)
    {

    case Token_Attribute:
        Context->NewBlAttribute = Value;
        break;

    case Token_BlockSize:
        Context->BlockSize     = Value;
        Context->BlockSizeLog2 = Log2(Value);

        if (Context->Memory != NULL)
        {
            NvAuPrintf("Error: Too late to change block size.\n");
            return 1;
        }

        if (Value != (NvU32)(1 << Context->BlockSizeLog2))
        {
            NvAuPrintf("Error: Block size must be a power of 2.\n");
            return 1;
        }

        Context->PagesPerBlock = 1 << (Context->BlockSizeLog2 -
                                       Context->PageSizeLog2);

        break;

    case Token_EnableJtag:
        NV_ASSERT(Context->NvBCT != NULL);
        break;

    case Token_PageSize:
        Context->PageSize     = Value;
        Context->PageSizeLog2 = Log2(Value);

        if (Context->Memory != NULL)
        {
            NvAuPrintf("Error: Too late to change block size.\n");
            return 1;
        }

        if (Value != (NvU32)(1 << Context->PageSizeLog2))
        {
            NvAuPrintf("Error: Page size must be a power of 2.\n");
            return 1;
        }

        Context->PagesPerBlock = 1 << (Context->BlockSizeLog2 -
                                       Context->PageSizeLog2);

        break;

    case Token_PartitionSize:
        if (Context->Memory != NULL)
        {
            NvAuPrintf("Error: Too late to change block size.\n");
            return 1;
        }

        Context->PartitionSize = Value;
        break;

    case Token_Redundancy:
        Context->Redundancy = Value;
        break;

    case Token_Version:
        Context->Version = Value;
        Context->VersionSetBeforeBL = NV_TRUE;
        break;

    case Token_CustomerDataVersion:
        {
            NvBctCustomerPlatformData* pCustomerPlatformData;
            pCustomerPlatformData = (NvBctCustomerPlatformData *)
                            ((NvBootConfigTable*)Context->NvBCT)->CustomerData;
            pCustomerPlatformData->CustomerDataVersion = NV_CUSTOMER_DATA_VERSION;
        }
        break;

    default:
        NvAuPrintf("Unknown token %d at line %d\n", Token, __LINE__);
        return 1;
    }

    return 0;
}

