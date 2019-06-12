/**
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Parsing support for the nvbuildbct for different parameters
 */


#include "nvapputil.h"
#include "nvassert.h"
#include "../parse.h"
#include "../parse_hal.h"
#include "t11x/nvboot_bct.h"
#include "t11x/nvboot_sdram_param.h"
#include "t11x_data_layout.h"
#include "t11x_set.h"
#include "t11x_parse.h"
#include "nvbct_customer_platform_data.h"

static NvBool              DeviceIndexZero= NV_FALSE;
static NvBool              DeviceIndexOne= NV_FALSE;
static NvBool              DeviceIndexTwo= NV_FALSE;
static NvBool              DeviceIndexThree= NV_FALSE;
static NvBool              SdramIndexZero= NV_FALSE;
static NvBool              SdramIndexOne= NV_FALSE;
static NvBool              SdramIndexTwo= NV_FALSE;
static NvBool              SdramIndexThree= NV_FALSE;

#ifndef DONT_INTEGRATE_LEGACY_DRAM_TYPES
#define NvBootMemoryType_Ddr    0
#define NvBootMemoryType_LpDdr  0
#define NvBootMemoryType_Ddr2   0
#endif

/*
 * Function prototypes
 * ParseXXX() parses XXX in the input
 * A ParseXXX() function may call other parse functions and set functions.
 */

static int SetArray(BuildBctContext *Context, NvU32 Index,ParseToken Token,NvU32 Value);
static char *ParseNvU32   (char *statement, NvU32 *val);
static char *ParseNvU8    (char *statement, NvU32 *val);
static char *ParseEnum    (char *Statement, EnumItem *Table, NvU32 *val);
static char *ParseBool    (char *Statement, NvBool *val);
static char *ParseFieldName(char *Rest, FieldItem *FieldTable, FieldItem **Field);
static char *ParseFieldValue(char *Rest, FieldItem *Field, NvU32 *Value);
static int ParseValueNvU32 (BuildBctContext *Context, ParseToken Token, char *Rest, char *Prefix);
static int ParseSdramParam (BuildBctContext *Context, ParseToken Token, char *Rest, char *Prefix);
static int ParseDevParam   (BuildBctContext *Context, ParseToken Token, char *Rest, char *Prefix);
static int ParseArray(BuildBctContext *Context, ParseToken Token, char *Rest, char *Prefix);
static int ProcessStatement(BuildBctContext *Context, char *Statement);
static int EndConfig(BuildBctContext *Context, ParseToken Token, char *Rest, char *Prefix);

/*
 * Static data
 */

static EnumItem s_BoolTable[] =
{
    { "NV_TRUE",  NV_TRUE  },
    { "NV_FALSE", NV_FALSE },
    { "True",     NV_TRUE  },
    { "False",    NV_FALSE },
    { NULL,       0 }
};

static EnumItem s_SdmmcDataWidthTable[] =
{
    { "NvBootSdmmcDataWidth_1Bit",     NvBootSdmmcDataWidth_1Bit },
    { "NvBootSdmmcDataWidth_4Bit",     NvBootSdmmcDataWidth_4Bit },
    { "NvBootSdmmcDataWidth_8Bit",     NvBootSdmmcDataWidth_8Bit },
    { "NvBootSdmmcDataWidth_Ddr_4Bit", NvBootSdmmcDataWidth_Ddr_4Bit },
    { "NvBootSdmmcDataWidth_Ddr_8Bit", NvBootSdmmcDataWidth_Ddr_8Bit },
    { "1Bit",                          NvBootSdmmcDataWidth_1Bit },
    { "4Bit",                          NvBootSdmmcDataWidth_4Bit },
    { "8Bit",                          NvBootSdmmcDataWidth_8Bit },
    { "Ddr_4Bit",                      NvBootSdmmcDataWidth_Ddr_4Bit },
    { "Ddr_8Bit",                      NvBootSdmmcDataWidth_Ddr_8Bit },
    { NULL, 0 }
};

/* Macros to simplify the field definitions */
#define DESC_ENUM_FIELD(a,t) { #a, Token_##a, FieldType_Enum, t ,NV_FALSE ,NV_FALSE ,NV_FALSE ,NV_FALSE }
#define DESC_U32_FIELD(a) { #a, Token_##a, FieldType_U32, NULL , NV_FALSE ,NV_FALSE ,NV_FALSE ,NV_FALSE }
#define DESC_BOOL_FIELD(a) { #a, Token_##a, FieldType_Bool, NULL , NV_FALSE ,NV_FALSE ,NV_FALSE ,NV_FALSE }

// Note: Prefixes must appear after the items that start with them.
static EnumItem s_NvBootMemoryTypeTable[] =
{
    { "None",                    NvBootMemoryType_None },
    { "Ddr3",                    NvBootMemoryType_Ddr3 },
    { "Ddr2",                    NvBootMemoryType_Ddr2 },
    { "Ddr",                     NvBootMemoryType_Ddr },
    { "LpDdr2",                  NvBootMemoryType_LpDdr2 },
    { "LpDdr",                   NvBootMemoryType_LpDdr },
    { "NvBootMemoryType_None",   NvBootMemoryType_None },
    { "NvBootMemoryType_Ddr3",   NvBootMemoryType_Ddr3 },
    { "NvBootMemoryType_Ddr2",   NvBootMemoryType_Ddr2 },
    { "NvBootMemoryType_Ddr",    NvBootMemoryType_Ddr },
    { "NvBootMemoryType_LpDdr2", NvBootMemoryType_LpDdr2 },
    { "NvBootMemoryType_LpDdr",  NvBootMemoryType_LpDdr },
    { NULL, 0}
};

static EnumItem s_SpiClockSourceTable[] =
{
    { "NvBootSpiClockSource_PllPOut0", NvBootSpiClockSource_PllPOut0 },
    { "NvBootSpiClockSource_ClockM",   NvBootSpiClockSource_ClockM },
    { "PllPOut0",                      NvBootSpiClockSource_PllPOut0 },
    { "ClockM",                        NvBootSpiClockSource_ClockM },
    { NULL, 0 }
};

static EnumItem s_SnorClockSourceTable[] =
{
    { "NvBootSnorClockSource_PllPOut0", NvBootSnorClockSource_PllPOut0 },
    { "NvBootSnorClockSource_PllMOut0", NvBootSnorClockSource_PllMOut0 },
    { "NvBootSnorClockSource_ClockM",   NvBootSnorClockSource_ClockM },
    { "NvBootSnorClockSource_PllCOut0", NvBootSnorClockSource_PllCOut0 },
    { "PllPOut0",                       NvBootSnorClockSource_PllPOut0 },
    { "PllMOut0",                       NvBootSnorClockSource_PllMOut0 },
    { "ClockM",                         NvBootSnorClockSource_ClockM },
    { "PllCOut0",                       NvBootSnorClockSource_PllCOut0 },
    { NULL, 0 }
};

static EnumItem s_SnorDataXferMode[] =
{
    { "SnorDataXferMode_Pio", SnorDataXferMode_Pio },
    { "SnorDataXferMode_Dma", SnorDataXferMode_Dma },
    { "SnorDataXferMode_Max", SnorDataXferMode_Max },
    { "Pio",                  SnorDataXferMode_Pio },
    { "Dma",                  SnorDataXferMode_Dma },
    { "Max",                  SnorDataXferMode_Max },
    { NULL, 0 }
};

static FieldItem s_SdramFieldTable[] =
{
    DESC_ENUM_FIELD(MemoryType, s_NvBootMemoryTypeTable),
    DESC_U32_FIELD(PllMInputDivider),
    DESC_U32_FIELD(PllMFeedbackDivider),
    DESC_U32_FIELD(PllMStableTime),
    DESC_U32_FIELD(PllMSetupControl),
    DESC_U32_FIELD(PllMSelectDiv2),
    DESC_U32_FIELD(PllMPDLshiftPh45),
    DESC_U32_FIELD(PllMPDLshiftPh90),
    DESC_U32_FIELD(PllMPDLshiftPh135),
    DESC_U32_FIELD(PllMKCP),
    DESC_U32_FIELD(PllMKVCO),
    DESC_U32_FIELD(EmcBctSpare0),
    DESC_U32_FIELD(EmcClockSource),
    DESC_U32_FIELD(EmcAutoCalInterval),
    DESC_U32_FIELD(EmcAutoCalConfig),
    DESC_U32_FIELD(EmcAutoCalConfig2),
    DESC_U32_FIELD(EmcAutoCalConfig3),
    DESC_U32_FIELD(EmcAutoCalWait),
    DESC_U32_FIELD(EmcAdrCfg),
    DESC_U32_FIELD(EmcPinProgramWait),
    DESC_U32_FIELD(EmcPinExtraWait),
    DESC_U32_FIELD(EmcTimingControlWait),
    DESC_U32_FIELD(EmcRc),
    DESC_U32_FIELD(EmcRfc),
    DESC_U32_FIELD(EmcRfcSlr),
    DESC_U32_FIELD(EmcRas),
    DESC_U32_FIELD(EmcRp),
    DESC_U32_FIELD(EmcR2r),
    DESC_U32_FIELD(EmcW2w),
    DESC_U32_FIELD(EmcR2w),
    DESC_U32_FIELD(EmcW2r),
    DESC_U32_FIELD(EmcR2p),
    DESC_U32_FIELD(EmcW2p),
    DESC_U32_FIELD(EmcRdRcd),
    DESC_U32_FIELD(EmcWrRcd),
    DESC_U32_FIELD(EmcRrd),
    DESC_U32_FIELD(EmcRext),
    DESC_U32_FIELD(EmcWext),
    DESC_U32_FIELD(EmcWdv),
    DESC_U32_FIELD(EmcWdvMask),
    DESC_U32_FIELD(EmcQUse),
    DESC_U32_FIELD(EmcIbdly),
    DESC_U32_FIELD(EmcEInput),
    DESC_U32_FIELD(EmcEInputDuration),
    DESC_U32_FIELD(EmcPutermExtra),
    DESC_U32_FIELD(EmcCdbCntl1),
    DESC_U32_FIELD(EmcCdbCntl2),
    DESC_U32_FIELD(EmcQRst),
    DESC_U32_FIELD(EmcQSafe),
    DESC_U32_FIELD(EmcRdv),
    DESC_U32_FIELD(EmcRdvMask),
    DESC_U32_FIELD(EmcCtt),
    DESC_U32_FIELD(EmcCttDuration),
    DESC_U32_FIELD(EmcRefresh),
    DESC_U32_FIELD(EmcBurstRefreshNum),
    DESC_U32_FIELD(EmcPreRefreshReqCnt),
    DESC_U32_FIELD(EmcPdEx2Wr),
    DESC_U32_FIELD(EmcPdEx2Rd),
    DESC_U32_FIELD(EmcPChg2Pden),
    DESC_U32_FIELD(EmcAct2Pden),
    DESC_U32_FIELD(EmcAr2Pden),
    DESC_U32_FIELD(EmcRw2Pden),
    DESC_U32_FIELD(EmcTxsr),
    DESC_U32_FIELD(EmcTxsrDll),
    DESC_U32_FIELD(EmcTcke),
    DESC_U32_FIELD(EmcTckesr),
    DESC_U32_FIELD(EmcTpd),
    DESC_U32_FIELD(EmcTfaw),
    DESC_U32_FIELD(EmcTrpab),
    DESC_U32_FIELD(EmcTClkStable),
    DESC_U32_FIELD(EmcTClkStop),
    DESC_U32_FIELD(EmcTRefBw),
    DESC_U32_FIELD(EmcQUseExtra),
    DESC_U32_FIELD(EmcFbioCfg5),
    DESC_U32_FIELD(EmcFbioCfg6),
    DESC_U32_FIELD(EmcFbioSpare),
    DESC_U32_FIELD(EmcCfgRsv),
    DESC_U32_FIELD(EmcMrs),
    DESC_U32_FIELD(EmcEmrs),
    DESC_U32_FIELD(EmcEmrs2),
    DESC_U32_FIELD(EmcEmrs3),
    DESC_U32_FIELD(EmcMrw1),
    DESC_U32_FIELD(EmcMrw2),
    DESC_U32_FIELD(EmcMrw3),
    DESC_U32_FIELD(EmcMrw4),
    DESC_U32_FIELD(EmcMrwExtra),
    DESC_U32_FIELD(EmcWarmBootMrwExtra),
    DESC_U32_FIELD(EmcWarmBootExtraModeRegWriteEnable),
    DESC_U32_FIELD(EmcExtraModeRegWriteEnable),
    DESC_U32_FIELD(EmcMrwResetCommand),
    DESC_U32_FIELD(EmcMrwResetNInitWait),
    DESC_U32_FIELD(EmcMrsWaitCnt),
    DESC_U32_FIELD(EmcMrsWaitCnt2),
    DESC_U32_FIELD(EmcCfg),
    DESC_U32_FIELD(EmcCfg2),
    DESC_U32_FIELD(EmcDbg),
    DESC_U32_FIELD(EmcCmdQ),
    DESC_U32_FIELD(EmcMc2EmcQ),
    DESC_U32_FIELD(EmcDynSelfRefControl),
    DESC_U32_FIELD(AhbArbitrationXbarCtrlMemInitDone),
    DESC_U32_FIELD(EmcCfgDigDll),
    DESC_U32_FIELD(EmcCfgDigDllPeriod),
    DESC_U32_FIELD(EmcDevSelect),
    DESC_U32_FIELD(EmcSelDpdCtrl),
    DESC_U32_FIELD(EmcDllXformDqs0),
    DESC_U32_FIELD(EmcDllXformDqs1),
    DESC_U32_FIELD(EmcDllXformDqs2),
    DESC_U32_FIELD(EmcDllXformDqs3),
    DESC_U32_FIELD(EmcDllXformDqs4),
    DESC_U32_FIELD(EmcDllXformDqs5),
    DESC_U32_FIELD(EmcDllXformDqs6),
    DESC_U32_FIELD(EmcDllXformDqs7),
    DESC_U32_FIELD(EmcDllXformQUse0),
    DESC_U32_FIELD(EmcDllXformQUse1),
    DESC_U32_FIELD(EmcDllXformQUse2),
    DESC_U32_FIELD(EmcDllXformQUse3),
    DESC_U32_FIELD(EmcDllXformQUse4),
    DESC_U32_FIELD(EmcDllXformQUse5),
    DESC_U32_FIELD(EmcDllXformQUse6),
    DESC_U32_FIELD(EmcDllXformQUse7),
    DESC_U32_FIELD(EmcDllXformAddr0),
    DESC_U32_FIELD(EmcDllXformAddr1),
    DESC_U32_FIELD(EmcDllXformAddr2),
    DESC_U32_FIELD(EmcDliTrimTxDqs0),
    DESC_U32_FIELD(EmcDliTrimTxDqs1),
    DESC_U32_FIELD(EmcDliTrimTxDqs2),
    DESC_U32_FIELD(EmcDliTrimTxDqs3),
    DESC_U32_FIELD(EmcDliTrimTxDqs4),
    DESC_U32_FIELD(EmcDliTrimTxDqs5),
    DESC_U32_FIELD(EmcDliTrimTxDqs6),
    DESC_U32_FIELD(EmcDliTrimTxDqs7),
    DESC_U32_FIELD(EmcDllXformDq0),
    DESC_U32_FIELD(EmcDllXformDq1),
    DESC_U32_FIELD(EmcDllXformDq2),
    DESC_U32_FIELD(EmcDllXformDq3),
    DESC_U32_FIELD(WarmBootWait),
    DESC_U32_FIELD(EmcCttTermCtrl),
    DESC_U32_FIELD(EmcOdtWrite),
    DESC_U32_FIELD(EmcOdtRead),
    DESC_U32_FIELD(EmcZcalInterval),
    DESC_U32_FIELD(EmcZcalWaitCnt),
    DESC_U32_FIELD(EmcZcalMrwCmd),
    DESC_U32_FIELD(EmcMrsResetDll),
    DESC_U32_FIELD(EmcZcalInitDev0),
    DESC_U32_FIELD(EmcZcalInitDev1),
    DESC_U32_FIELD(EmcZcalInitWait),
    DESC_U32_FIELD(EmcZcalWarmColdBootEnables),
    DESC_U32_FIELD(EmcMrwLpddr2ZcalWarmBoot),
    DESC_U32_FIELD(EmcZqCalDdr3WarmBoot),
    DESC_U32_FIELD(EmcZcalWarmBootWait),
    DESC_U32_FIELD(EmcMrsWarmBootEnable),
    DESC_U32_FIELD(EmcMrsResetDllWait),
    DESC_U32_FIELD(EmcMrsExtra),
    DESC_U32_FIELD(EmcWarmBootMrsExtra),
    DESC_U32_FIELD(EmcEmrsDdr2DllEnable),
    DESC_U32_FIELD(EmcMrsDdr2DllReset),
    DESC_U32_FIELD(EmcEmrsDdr2OcdCalib),
    DESC_U32_FIELD(EmcDdr2Wait),
    DESC_U32_FIELD(EmcClkenOverride),
    DESC_U32_FIELD(EmcExtraRefreshNum),
    DESC_U32_FIELD(EmcClkenOverrideAllWarmBoot),
    DESC_U32_FIELD(McClkenOverrideAllWarmBoot),
    DESC_U32_FIELD(EmcCfgDigDllPeriodWarmBoot),
    DESC_U32_FIELD(PmcVddpSel),
    DESC_U32_FIELD(PmcDdrPwr),
    DESC_U32_FIELD(PmcDdrCfg),
    DESC_U32_FIELD(PmcIoDpdReq),
    DESC_U32_FIELD(PmcIoDpd2Req),
    DESC_U32_FIELD(PmcRegShort),
    DESC_U32_FIELD(PmcENoVttGen),
    DESC_U32_FIELD(PmcNoIoPower),
    DESC_U32_FIELD(EmcXm2CmdPadCtrl),
    DESC_U32_FIELD(EmcXm2CmdPadCtrl2),
    DESC_U32_FIELD(EmcXm2CmdPadCtrl3),
    DESC_U32_FIELD(EmcXm2CmdPadCtrl4),
    DESC_U32_FIELD(EmcXm2DqsPadCtrl),
    DESC_U32_FIELD(EmcXm2DqsPadCtrl2),
    DESC_U32_FIELD(EmcXm2DqsPadCtrl3),
    DESC_U32_FIELD(EmcXm2DqsPadCtrl4),
    DESC_U32_FIELD(EmcXm2DqPadCtrl),
    DESC_U32_FIELD(EmcXm2DqPadCtrl2),
    DESC_U32_FIELD(EmcXm2ClkPadCtrl),
    DESC_U32_FIELD(EmcXm2ClkPadCtrl2),
    DESC_U32_FIELD(EmcXm2CompPadCtrl),
    DESC_U32_FIELD(EmcXm2VttGenPadCtrl),
    DESC_U32_FIELD(EmcXm2VttGenPadCtrl2),
    DESC_U32_FIELD(EmcAcpdControl),
    DESC_U32_FIELD(EmcSwizzleRank0ByteCfg),
    DESC_U32_FIELD(EmcSwizzleRank0Byte0),
    DESC_U32_FIELD(EmcSwizzleRank0Byte1),
    DESC_U32_FIELD(EmcSwizzleRank0Byte2),
    DESC_U32_FIELD(EmcSwizzleRank0Byte3),
    DESC_U32_FIELD(EmcSwizzleRank1ByteCfg),
    DESC_U32_FIELD(EmcSwizzleRank1Byte0),
    DESC_U32_FIELD(EmcSwizzleRank1Byte1),
    DESC_U32_FIELD(EmcSwizzleRank1Byte2),
    DESC_U32_FIELD(EmcSwizzleRank1Byte3),
    DESC_U32_FIELD(EmcAddrSwizzleStack1a),
    DESC_U32_FIELD(EmcAddrSwizzleStack1b),
    DESC_U32_FIELD(EmcAddrSwizzleStack2a),
    DESC_U32_FIELD(EmcAddrSwizzleStack2b),
    DESC_U32_FIELD(EmcAddrSwizzleStack3),
    DESC_U32_FIELD(EmcDsrVttgenDrv),
    DESC_U32_FIELD(EmcTxdsrvttgen),
    DESC_U32_FIELD(McEmemAdrCfg),
    DESC_U32_FIELD(McEmemAdrCfgDev0),
    DESC_U32_FIELD(McEmemAdrCfgDev1),
    DESC_U32_FIELD(McEmemAdrCfgChannelMask),
    DESC_U32_FIELD(McEmemAdrCfgChannelMaskPropagationCount),
    DESC_U32_FIELD(McEmemAdrCfgBankMask0),
    DESC_U32_FIELD(McEmemAdrCfgBankMask1),
    DESC_U32_FIELD(McEmemAdrCfgBankMask2),
    DESC_U32_FIELD(McEmemCfg),
    DESC_U32_FIELD(McEmemArbCfg),
    DESC_U32_FIELD(McEmemArbOutstandingReq),
    DESC_U32_FIELD(McEmemArbTimingRcd),
    DESC_U32_FIELD(McEmemArbTimingRp),
    DESC_U32_FIELD(McEmemArbTimingRc),
    DESC_U32_FIELD(McEmemArbTimingRas),
    DESC_U32_FIELD(McEmemArbTimingFaw),
    DESC_U32_FIELD(McEmemArbTimingRrd),
    DESC_U32_FIELD(McEmemArbTimingRap2Pre),
    DESC_U32_FIELD(McEmemArbTimingWap2Pre),
    DESC_U32_FIELD(McEmemArbTimingR2R),
    DESC_U32_FIELD(McEmemArbTimingW2W),
    DESC_U32_FIELD(McEmemArbTimingR2W),
    DESC_U32_FIELD(McEmemArbTimingW2R),
    DESC_U32_FIELD(McEmemArbDaTurns),
    DESC_U32_FIELD(McEmemArbDaCovers),
    DESC_U32_FIELD(McEmemArbMisc0),
    DESC_U32_FIELD(McEmemArbMisc1),
    DESC_U32_FIELD(McEmemArbRing1Throttle),
    DESC_U32_FIELD(McEmemArbOverride),
    DESC_U32_FIELD(McEmemArbRsv),
    DESC_U32_FIELD(McClkenOverride),
    DESC_U32_FIELD(McEmcRegMode),
    DESC_U32_FIELD(McVideoProtectBom),
    DESC_U32_FIELD(McVideoProtectSizeMb),
    DESC_U32_FIELD(McVideoProtectVprOverride),
    DESC_U32_FIELD(McSecCarveoutBom),
    DESC_U32_FIELD(McSecCarveoutSizeMb),
    DESC_U32_FIELD(McVideoProtectWriteAccess),
    DESC_U32_FIELD(McSecCarveoutProtectWriteAccess),
    DESC_U32_FIELD(EmcCaTrainingEnable),
    DESC_U32_FIELD(EmcCaTrainingTimingCntl1),
    DESC_U32_FIELD(EmcCaTrainingTimingCntl2),
    DESC_U32_FIELD(SwizzleRankByteEncode),
    DESC_U32_FIELD(BootRomPatchControl),
    DESC_U32_FIELD(BootRomPatchData),
    DESC_U32_FIELD(Ch1EmcDllXformDqs0),
    DESC_U32_FIELD(Ch1EmcDllXformDqs1),
    DESC_U32_FIELD(Ch1EmcDllXformDqs2),
    DESC_U32_FIELD(Ch1EmcDllXformDqs3),
    DESC_U32_FIELD(Ch1EmcDllXformDqs4),
    DESC_U32_FIELD(Ch1EmcDllXformDqs5),
    DESC_U32_FIELD(Ch1EmcDllXformDqs6),
    DESC_U32_FIELD(Ch1EmcDllXformDqs7),
    DESC_U32_FIELD(Ch1EmcDllXformQUse0),
    DESC_U32_FIELD(Ch1EmcDllXformQUse1),
    DESC_U32_FIELD(Ch1EmcDllXformQUse2),
    DESC_U32_FIELD(Ch1EmcDllXformQUse3),
    DESC_U32_FIELD(Ch1EmcDllXformQUse4),
    DESC_U32_FIELD(Ch1EmcDllXformQUse5),
    DESC_U32_FIELD(Ch1EmcDllXformQUse6),
    DESC_U32_FIELD(Ch1EmcDllXformQUse7),
    DESC_U32_FIELD(Ch1EmcDliTrimTxDqs0),
    DESC_U32_FIELD(Ch1EmcDliTrimTxDqs1),
    DESC_U32_FIELD(Ch1EmcDliTrimTxDqs2),
    DESC_U32_FIELD(Ch1EmcDliTrimTxDqs3),
    DESC_U32_FIELD(Ch1EmcDliTrimTxDqs4),
    DESC_U32_FIELD(Ch1EmcDliTrimTxDqs5),
    DESC_U32_FIELD(Ch1EmcDliTrimTxDqs6),
    DESC_U32_FIELD(Ch1EmcDliTrimTxDqs7),
    DESC_U32_FIELD(Ch1EmcDllXformDq0),
    DESC_U32_FIELD(Ch1EmcDllXformDq1),
    DESC_U32_FIELD(Ch1EmcDllXformDq2),
    DESC_U32_FIELD(Ch1EmcDllXformDq3),
    DESC_U32_FIELD(Ch1EmcSwizzleRank0ByteCfg),
    DESC_U32_FIELD(Ch1EmcSwizzleRank0Byte0),
    DESC_U32_FIELD(Ch1EmcSwizzleRank0Byte1),
    DESC_U32_FIELD(Ch1EmcSwizzleRank0Byte2),
    DESC_U32_FIELD(Ch1EmcSwizzleRank0Byte3),
    DESC_U32_FIELD(Ch1EmcSwizzleRank1ByteCfg),
    DESC_U32_FIELD(Ch1EmcSwizzleRank1Byte0),
    DESC_U32_FIELD(Ch1EmcSwizzleRank1Byte1),
    DESC_U32_FIELD(Ch1EmcSwizzleRank1Byte2),
    DESC_U32_FIELD(Ch1EmcSwizzleRank1Byte3),
    DESC_U32_FIELD(Ch1EmcAddrSwizzleStack1a),
    DESC_U32_FIELD(Ch1EmcAddrSwizzleStack1b),
    DESC_U32_FIELD(Ch1EmcAddrSwizzleStack2a),
    DESC_U32_FIELD(Ch1EmcAddrSwizzleStack2b),
    DESC_U32_FIELD(Ch1EmcAddrSwizzleStack3),
    DESC_U32_FIELD(Ch1EmcAutoCalConfig),
    DESC_U32_FIELD(Ch1EmcAutoCalConfig2),
    DESC_U32_FIELD(Ch1EmcAutoCalConfig3),
    DESC_U32_FIELD(Ch1EmcCdbCntl1),
    DESC_U32_FIELD(Ch1EmcDllXformAddr0),
    DESC_U32_FIELD(Ch1EmcDllXformAddr1),
    DESC_U32_FIELD(Ch1EmcDllXformAddr2),
    DESC_U32_FIELD(Ch1EmcFbioSpare),
    DESC_U32_FIELD(Ch1EmcXm2ClkPadCtrl),
    DESC_U32_FIELD(Ch1EmcXm2ClkPadCtrl2),
    DESC_U32_FIELD(Ch1EmcXm2CmdPadCtrl2),
    DESC_U32_FIELD(Ch1EmcXm2CmdPadCtrl3),
    DESC_U32_FIELD(Ch1EmcXm2CmdPadCtrl4),
    DESC_U32_FIELD(Ch1EmcXm2DqPadCtrl),
    DESC_U32_FIELD(Ch1EmcXm2DqPadCtrl2),
    DESC_U32_FIELD(Ch1EmcXm2DqsPadCtrl),
    DESC_U32_FIELD(Ch1EmcXm2DqsPadCtrl3),
    DESC_U32_FIELD(Ch1EmcXm2DqsPadCtrl4),
    { NULL, 0, 0, NULL , NV_TRUE , NV_TRUE , NV_TRUE , NV_TRUE }
};

static FieldItem s_SdmmcTable[] =
{
    DESC_U32_FIELD(ClockDivider),
    DESC_ENUM_FIELD(DataWidth, s_SdmmcDataWidthTable),
    DESC_U32_FIELD(MaxPowerClassSupported),
    DESC_U32_FIELD(MultiPageSupport),
    { NULL, 0, 0, NULL , NV_TRUE , NV_TRUE , NV_TRUE , NV_TRUE }
};

static FieldItem s_NandTable[] =
{
    /*
     * Note: NandTiming2 must appear before NandTiming, because NandTiming
     * is a prefix of NandTiming2 and would otherwise match first.
     */
    DESC_U32_FIELD(NandAsyncTiming3),
    DESC_U32_FIELD(NandAsyncTiming2),
    DESC_U32_FIELD(NandAsyncTiming1),
    DESC_U32_FIELD(NandAsyncTiming0),
    DESC_U32_FIELD(DisableSyncDDR),
    DESC_U32_FIELD(NandSDDRTiming1),
    DESC_U32_FIELD(NandSDDRTiming0),
    DESC_U32_FIELD(NandTDDRTiming1),
    DESC_U32_FIELD(NandTDDRTiming0),
    DESC_U32_FIELD(ClockDivider),
    DESC_U32_FIELD(NandFbioDqsibDlyByte),
    DESC_U32_FIELD(NandFbioQuseDlyByte),
    DESC_U32_FIELD(NandFbioCfgQuseLate),
    DESC_U32_FIELD(BlockSizeLog2),
    DESC_U32_FIELD(PageSizeLog2),
    { NULL, 0, 0, NULL , NV_TRUE , NV_TRUE , NV_TRUE , NV_TRUE }
};

static FieldItem s_SnorTable[] =
{
    DESC_ENUM_FIELD(ClockSource, s_SnorClockSourceTable),
    DESC_U32_FIELD(ClockDivider),
    DESC_U32_FIELD(TimingParamCfgSnorTimingCfg0),
    DESC_U32_FIELD(TimingParamCfgSnorTimingCfg1),
    DESC_U32_FIELD(TimingParamCfgSnorTimingCfg2),
    DESC_ENUM_FIELD(DataXferMode, s_SnorDataXferMode),
    DESC_U32_FIELD(DmaDestMemAddress),
    DESC_U32_FIELD(CntrllerBsyTimeout),
    DESC_U32_FIELD(DmaTransferTimeout),
    { NULL, 0, 0, NULL , NV_TRUE , NV_TRUE , NV_TRUE , NV_TRUE }
};

static FieldItem s_SpiFlashTable[] =
{
    DESC_ENUM_FIELD(ClockSource, s_SpiClockSourceTable),
    DESC_BOOL_FIELD(ReadCommandTypeFast),
    { "ClockDivider", Token_ClockDivider, FieldType_U8, NULL,
       NV_FALSE, NV_FALSE, NV_FALSE, NV_FALSE },
    DESC_U32_FIELD(PageSize2kor16k),
    { NULL, 0, 0, NULL , NV_TRUE , NV_TRUE , NV_TRUE , NV_TRUE }
};

static FieldItem s_Usb3Table[] =
{
    DESC_U32_FIELD(ClockDivider),
    DESC_U32_FIELD(RootPortNumber),
    DESC_U32_FIELD(PageSize2kor16k),
    DESC_U32_FIELD(OCPin),
    DESC_U32_FIELD(VBusEnable),
    { NULL, 0, 0, NULL , NV_TRUE , NV_TRUE , NV_TRUE , NV_TRUE }
};

static ParseSubfieldItem s_DeviceTypeTable[] =
{
    { "SdmmcParams.",
      Token_SdmmcParams,
      s_SdmmcTable,
      t11xSetSdmmcParam },

    { "NandParams.", Token_NandParams, s_NandTable, t11xSetNandParam },
    { "SnorParams.", Token_SnorParams, s_SnorTable, t11xSetSnorParam },

    { "SpiFlashParams.",
      Token_SpiFlashParams,
      s_SpiFlashTable,
      t11xSetSpiFlashParam },

    { "Usb3Params.", Token_Usb3Params, s_Usb3Table, t11xSetUsb3Param },

    { NULL, 0, NULL, NULL }
};

static EnumItem s_DevTypeTable[] =
{
    { "Irom",       NvBootDevType_Irom },
    { "Nand_x16",   NvBootDevType_Nand_x16 },
    { "Nand",       NvBootDevType_Nand },
    { "None",       NvBootDevType_None },
    { "Sdmmc",      NvBootDevType_Sdmmc },
    { "Spi",        NvBootDevType_Spi },
    { "Snor",       NvBootDevType_Snor },
    { "Uart",       NvBootDevType_Uart },
    { "Usb",        NvBootDevType_Usb },
    { "Usb3",       NvBootDevType_Usb3 },
    { NULL, 0 }
};

static ParseItem s_TopLevelItems[] =
{
    { "Attribute=",          Token_Attribute,       ParseValueNvU32, NV_FALSE},
    { "BlockSize=",          Token_BlockSize,       ParseValueNvU32,   NV_FALSE},
    { "Attribute[",          Token_Attribute,       ParseArray, NV_FALSE},
    { "DevType[",            Token_DevType,         ParseArray, NV_FALSE},
    { "DeviceParam[",        Token_DeviceParam,     ParseDevParam,  NV_FALSE},
    { "PageSize=",           Token_PageSize,        ParseValueNvU32,  NV_FALSE},
    { "PartitionSize=",      Token_PartitionSize,   ParseValueNvU32, NV_FALSE},
    { "SDRAM[",              Token_Sdram,           ParseSdramParam,  NV_FALSE},
    { "Version=",            Token_Version,         ParseValueNvU32, NV_FALSE},
    { "SecureJtagControl=",  Token_EnableJtag,      ParseValueNvU32, NV_FALSE},
    { "FusePrivateTZBit=",   Token_FusePrivateTZ,   ParseValueNvU32, NV_FALSE},
    { "ECID_0=",             Token_ECID_0,          ParseValueNvU32, NV_FALSE},
    { "ECID_1=",             Token_ECID_1,          ParseValueNvU32, NV_FALSE},
    { "ECID_2=",             Token_ECID_2,          ParseValueNvU32, NV_FALSE},
    { "ECID_3=",             Token_ECID_3,          ParseValueNvU32, NV_FALSE},
    { "End",                 Token_End,             EndConfig, NV_FALSE},
    { NULL, 0, NULL, NV_TRUE} /* Must be last */
};

/* Macro to simplify parser code a bit. */
#define PARSE_COMMA(x) if (*Rest != ',') return (x); Rest++

/* This parsing code was initially borrowed from nvcamera_config_parse.c. */
/* Returns the address of the character after the parsed data. */
static char *
ParseNvU32(char *statement, NvU32 *val)
{
    NvU32 value = 0;

    while (*statement == '0')
    {
        statement++;
    }

    if (*statement == 'x' || *statement == 'X')
    {
        statement++;
        while (((*statement >= '0') && (*statement <= '9')) ||
               ((*statement >= 'a') && (*statement <= 'f')) ||
               ((*statement >= 'A') && (*statement <= 'F')))
        {
            value *= 16;
            if ((*statement >= '0') && (*statement <= '9'))
            {
                value += (*statement - '0');
            }
            else if ((*statement >= 'A') && (*statement <= 'F'))
            {
                value += ((*statement - 'A')+10);
            }
            else
            {
                value += ((*statement - 'a')+10);
            }
            statement++;
        }
    }
    else
    {
        while (*statement >= '0' &&
               *statement <= '9')
        {
            value = value*10 + (*statement - '0');
            statement++;
        }
    }
    *val = value;
    return statement;
}

/*
 * ParseArray(): Processes commands to set an array value.
 */
static int
ParseArray(BuildBctContext *Context, ParseToken Token, char *Rest, char *Prefix)
{
    NvU32       Index;
    NvU32       Value;

    NV_ASSERT(Context != NULL);
    NV_ASSERT(Rest    != NULL);

    /* Parse the index. */
    Rest = ParseNvU32(Rest, &Index);
    if (Rest == NULL) return 1;

    /* Parse the closing bracket. */
    if (*Rest != ']') return 1;
    Rest++;

    /* Parse the equals sign.*/
    if (*Rest != '=') return 1;
    Rest++;

    /* Parse the value based on the field table. */
    switch(Token)
    {
    case Token_Attribute:
        Rest = ParseNvU32(Rest, &Value);
        break;

    case Token_DevType:
        Rest = ParseEnum(Rest, s_DevTypeTable, &Value);
        break;

    case Token_End:
        break;

    default:
        /* Unknown token */
        return 1;
    }

    if (Rest == NULL) return 1;

    /* Store the result. */
    return SetArray(Context, Index, Token, Value);
}

/*
 * SetArray(): Sets an array value.
 */
static int
SetArray(BuildBctContext *Context,
         NvU32              Index,
         ParseToken         Token,
         NvU32              Value)
{
    NV_ASSERT(Context != NULL);
    NV_ASSERT(Context->NvBCT != NULL);

    switch (Token)
    {
    case Token_Attribute:
        ((NvBootConfigTable *)(Context->NvBCT))->BootLoader[Index].Attribute = Value;
        break;

    case Token_DevType:
        ((NvBootConfigTable *)(Context->NvBCT))->DevType[Index] = Value;
        break;

    default:
        break;
    }
    return 0;
}

static char *
ParseNvU8(char *statement, NvU32 *val)
{
    char *retval;

    retval = ParseNvU32(statement, val);

    if (*val > 0xff)
    {
        NvAuPrintf("Warning: Parsed 8-bit value that exceeded 8-bits.\n");
        NvAuPrintf("         Parsed value = %d. Remaining text = %s\n",
                   *val, retval);
    }

    return retval;
}

static char *ParseEnum(char *Statement, EnumItem *Table, NvU32 *val)
{
    int   i;
    char *Rest;

    for (i = 0; Table[i].Name != NULL; i++)
    {
        if (!NvOsStrncmp(Table[i].Name, Statement,
                         NvOsStrlen(Table[i].Name)))
        {
            *val = Table[i].Value;
            Rest = Statement + NvOsStrlen(Table[i].Name);

            return Rest;
        }
    }

    /*
     * If this point was reached, a symbolic name wasn't found, so parse
     * the value as an NvU32.
     * there was a processing error.
     */
    return ParseNvU32(Statement, val);
}

static char *ParseBool(char *statement, NvBool *val)
{
    NvU32  temp;
    char  *result;

    result = ParseEnum(statement, s_BoolTable, &temp);
    *val = (NvBool) temp;

    return result;
}

static char *
ParseFieldValue(char *Rest, FieldItem *Field, NvU32 *Value)
{
    NV_ASSERT(Rest  != NULL);
    NV_ASSERT(Value != NULL);
    NV_ASSERT(Field != NULL);
    NV_ASSERT((Field->Type != FieldType_Enum) || (Field->EnumTable != NULL));

    switch (Field->Type)
    {
    case FieldType_Enum:
        Rest = ParseEnum(Rest, Field->EnumTable, Value);
        break;

    case FieldType_U32:
        Rest = ParseNvU32(Rest, Value);
        break;

    case FieldType_U8:
        Rest = ParseNvU8(Rest, Value);
        break;

    case FieldType_Bool:
        Rest = ParseBool(Rest, (NvBool*)Value);
        break;

    default:
        NvAuPrintf("Unexpected field type %d at line %d\n",
                   Field->Type, __LINE__);
        Rest = NULL;
    }

    return Rest;
}

static char *
ParseFieldName(char *Rest, FieldItem *FieldTable, FieldItem **Field)
{
    NvU32 i;
    NvU32 FieldNameLength;

    NV_ASSERT(Rest       != NULL);
    NV_ASSERT(FieldTable != NULL);
    NV_ASSERT(Field      != NULL);

    FieldNameLength = 0;
    while(*(Rest+FieldNameLength) != '=')
        FieldNameLength++;

    /* Parse the field name. */
    for (i = 0; FieldTable[i].Name != NULL; i++)
    {
        if ((NvOsStrlen(FieldTable[i].Name) == FieldNameLength) &&
            !NvOsStrncmp(FieldTable[i].Name, Rest, FieldNameLength))
        {
            *Field = &(FieldTable[i]);
            Rest = Rest + NvOsStrlen(FieldTable[i].Name);
            return Rest;
        }
    }
    /* Field wasn't found or a parse error occurred. */
    NvAuPrintf("\n Error: Wrong config file is being used."
                 " Config file have unexpected parameters. \n");
    NvAuPrintf("\n BCT file was not created. \n");
    return NULL;
}

/*
 * ParseSdram(): Processes commands to set Sdram parameters.
 */
static int
ParseSdramParam(BuildBctContext *Context, ParseToken Token, char *Rest, char *Prefix)
{
    FieldItem  *Field;
    NvU32       Index;
    NvU32       Value;
    int i;
    NV_ASSERT(Context != NULL);
    NV_ASSERT(Rest    != NULL);

    /* Parse the index. */
    Rest = ParseNvU32(Rest, &Index);
    if (Rest == NULL) return 1;
    if ((Index != 0) && (Index != 1) && (Index != 2) && (Index != 3))
    {
        NvAuPrintf("\nError: Value entered for Index of SDRAM[%d]."
                    " is not valid. Valid values are 0, 1, 2 & 3. \n ",Index);
        NvAuPrintf("\nBCT file was not created. \n");
        exit(1);
    }
    /* Parse the closing bracket. */
    if (*Rest != ']')
    {
        NvAuPrintf("\nError: Unexpected Input '%s' in cfg file \n ",Rest);
        NvAuPrintf("\nBCT file was not created. \n");
        return 1;
    }
    Rest++;

    /* Parse the '.' */
    if (*Rest != '.')
    {
        NvAuPrintf("\nError: Unexpected Input '%s' in cfg file \n ",Rest);
        NvAuPrintf("\nBCT file was not created. \n");
        return 1;
    }
    Rest++;

    /* Parse the field name. */
    Rest = ParseFieldName(Rest, s_SdramFieldTable, &Field);
    if (Rest == NULL) return 1;

    for (i = 0; s_SdramFieldTable[i].Token != 0; i++)
    {
        if ((NvOsStrlen(Field->Name) == NvOsStrlen(s_SdramFieldTable[i].Name)) &&
            !NvOsStrncmp(s_SdramFieldTable[i].Name,  Field->Name,
             NvOsStrlen(s_SdramFieldTable[i].Name)))
        {
            if (Index == 0 && s_SdramFieldTable[i].StatusZero == NV_TRUE)
            {
                NvAuPrintf("\n Error: Config file has duplicate parameters for"
                             " SDRAM[%d].%s. \n", Index, s_SdramFieldTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            else if (Index == 1 && s_SdramFieldTable[i].StatusOne == NV_TRUE)
            {
                NvAuPrintf("\n Error: Config file has duplicate parameters for"
                             " SDRAM[%d].%s. \n", Index, s_SdramFieldTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            else if (Index == 2 && s_SdramFieldTable[i].StatusTwo == NV_TRUE)
            {
                NvAuPrintf("\n Error: Config file has duplicate parameters for"
                             " SDRAM[%d].%s. \n", Index, s_SdramFieldTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            else if (Index == 3 && s_SdramFieldTable[i].StatusThree == NV_TRUE)
            {
                NvAuPrintf("\n Error: Config file has duplicate parameters for"
                             " SDRAM[%d].%s. \n", Index, s_SdramFieldTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
        }
    }

    for (i = 0; s_SdramFieldTable[i].Token != 0; i++)
    {
        if ((NvOsStrlen(Field->Name) == NvOsStrlen(s_SdramFieldTable[i].Name)) &&
            !NvOsStrncmp(s_SdramFieldTable[i].Name,  Field->Name,
             NvOsStrlen(s_SdramFieldTable[i].Name)))
        {
            if (Index == 0)
            {
                SdramIndexZero = NV_TRUE;
                s_SdramFieldTable[i].StatusZero = NV_TRUE;
            }
            else if (Index == 1)
            {
                SdramIndexOne = NV_TRUE;
                s_SdramFieldTable[i].StatusOne = NV_TRUE;
            }
            else if (Index == 2)
            {
                SdramIndexTwo = NV_TRUE;
                s_SdramFieldTable[i].StatusTwo = NV_TRUE;
            }
            else if (Index == 3)
            {
                SdramIndexThree= NV_TRUE;
                s_SdramFieldTable[i].StatusThree = NV_TRUE;
            }
        }
    }

    /* Parse the equals sign.*/
    if (*Rest != '=')
        {
             NvAuPrintf("\nError: Unexpected Input '%s' in cfg file \n ",Rest);
             NvAuPrintf("\nBCT file was not created. \n");
             return 1;
        }
    Rest++;

    /* Parse the value based on the field table. */
    Rest = ParseFieldValue(Rest, Field, &Value);
    if (Rest == NULL) return 1;

    /* Store the result. */
    return t11xSetSdramParam(Context, Index, Field->Token, Value);
}

/*
 * ParseDevParam(): Processes commands to set device parameters.
 */
static int
ParseDevParam(BuildBctContext *Context,
              ParseToken         Token,
              char              *Rest,
              char *Prefix)
{
    NvU32              i;
    NvU32              Value;
    FieldItem         *Field;
    NvU32              Index;
    ParseSubfieldItem *DeviceItem = NULL;

    NV_ASSERT(Context != NULL);
    NV_ASSERT(Rest    != NULL);

    /* Parse the index. */
    Rest = ParseNvU32(Rest, &Index);
    if (Rest == NULL) return 1;
    if ((Index != 0) && (Index != 1) && (Index != 2) && (Index != 3))
    {
        NvAuPrintf("\nError: Value entered for Index of DeviceParam[%d]."
                    " is not valid. Valid values are 0, 1, 2 & 3. \n ",Index);
        NvAuPrintf("\nBCT file was not created. \n");
        exit(1);
    }
    /* Parse the closing bracket. */
    if (*Rest != ']')
    {
        NvAuPrintf("\nError: Unexpected Input '%s' in cfg file \n ",Rest);
        NvAuPrintf("\nBCT file was not created. \n");
        return 1;
    }
    Rest++;

    /* Parse the following '.' */
    if (*Rest != '.')
    {
        NvAuPrintf("\nError: Unexpected Input '%s' in cfg file \n ",Rest);
        NvAuPrintf("\nBCT file was not created. \n");
        return 1;
    }
    Rest++;

    /* Parse the device name. */
    for (i = 0; s_DeviceTypeTable[i].Prefix != NULL; i++)
    {
        if (!NvOsStrncmp(s_DeviceTypeTable[i].Prefix,
                         Rest,
                         NvOsStrlen(s_DeviceTypeTable[i].Prefix)))
        {
            DeviceItem = &(s_DeviceTypeTable[i]);
            Rest       = Rest + NvOsStrlen(s_DeviceTypeTable[i].Prefix);

            /* Parse the field name. */
            Rest = ParseFieldName(Rest,
                                  s_DeviceTypeTable[i].FieldTable,
                                  &Field);

            if (Rest == NULL) return 1;

            if (!NvOsStrncmp(s_DeviceTypeTable[i].Prefix,
                        "NandParams.",
                        NvOsStrlen(s_DeviceTypeTable[i].Prefix)))
            {
                if ((((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Sdmmc) ||
                (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Spi) ||
                (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Snor) ||
                (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Usb3))
                {
                    NvAuPrintf("\n Error: Config file has multiple Secondary"
                                 " Boot Device parameters \n");
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
                }
                ((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] = NvBootDevType_Nand;

                for (i = 0; s_NandTable[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_NandTable[i].Name,  Field->Name,
                        NvOsStrlen(s_NandTable[i].Name)))
                    {
                        if (Index == 0 && s_NandTable[i].StatusZero == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Nand"
                                          "Params.%s. \n", Index, s_NandTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 1 && s_NandTable[i].StatusOne == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Nand"
                                          "Params.%s. \n", Index, s_NandTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 2 && s_NandTable[i].StatusTwo == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Nand"
                                          "Params.%s. \n", Index, s_NandTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 3 && s_NandTable[i].StatusThree == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Nand"
                                          "Params.%s. \n", Index, s_NandTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                    }
                }

                for (i = 0; s_NandTable[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_NandTable[i].Name,  Field->Name,
                        NvOsStrlen(Field->Name)))
                    {
                        if (Index == 0)
                        {
                            DeviceIndexZero = NV_TRUE;
                            s_NandTable[i].StatusZero = NV_TRUE;
                        }
                        else if (Index == 1)
                        {
                            DeviceIndexOne = NV_TRUE;
                            s_NandTable[i].StatusOne = NV_TRUE;
                        }
                        else if (Index == 2)
                        {
                            DeviceIndexTwo = NV_TRUE;
                            s_NandTable[i].StatusTwo = NV_TRUE;
                        }
                        else if (Index == 3)
                        {
                            DeviceIndexThree = NV_TRUE;
                            s_NandTable[i].StatusThree = NV_TRUE;
                        }
                    }
                }
            }
            else if (!NvOsStrncmp(s_DeviceTypeTable[i].Prefix,
                        "SdmmcParams.",
                        NvOsStrlen(s_DeviceTypeTable[i].Prefix)))
            {
                if ((((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Nand) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Snor) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Spi) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Usb3))
                {
                    NvAuPrintf("\n Error: Config file has multiple Secondary"
                                 " Boot Device parameters \n");
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
                }
                ((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] = NvBootDevType_Sdmmc;

                for (i = 0; s_SdmmcTable[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_SdmmcTable[i].Name,  Field->Name,
                         NvOsStrlen(s_SdmmcTable[i].Name)))
                    {
                        if (Index == 0 && s_SdmmcTable[i].StatusZero == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Sdmmc"
                                          "Params.%s. \n", Index, s_SdmmcTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 1 && s_SdmmcTable[i].StatusOne == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Sdmmc"
                                          "Params.%s. \n", Index, s_SdmmcTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 2 && s_SdmmcTable[i].StatusTwo == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Sdmmc"
                                          "Params.%s. \n", Index, s_SdmmcTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 3 && s_SdmmcTable[i].StatusThree == NV_TRUE)
                       {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Sdmmc"
                                          "Params.%s. \n", Index, s_SdmmcTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                    }
                }

                for (i = 0; s_SdmmcTable[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_SdmmcTable[i].Name,  Field->Name,
                         NvOsStrlen(Field->Name)))
                    {
                        if (Index == 0)
                        {
                            DeviceIndexZero = NV_TRUE;
                            s_SdmmcTable[i].StatusZero = NV_TRUE;
                        }
                        else if (Index == 1)
                        {
                            DeviceIndexOne = NV_TRUE;
                            s_SdmmcTable[i].StatusOne = NV_TRUE;
                        }
                        else if (Index == 2)
                        {
                            DeviceIndexTwo = NV_TRUE;
                            s_SdmmcTable[i].StatusTwo = NV_TRUE;
                        }
                        else if (Index == 3)
                        {
                            DeviceIndexThree = NV_TRUE;
                            s_SdmmcTable[i].StatusThree = NV_TRUE;
                        }
                    }
                }
            }
            else if (!NvOsStrncmp(s_DeviceTypeTable[i].Prefix,
                        "SnorParams.",
                        NvOsStrlen(s_DeviceTypeTable[i].Prefix)))
            {
                if ((((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Nand) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Sdmmc) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Spi) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Usb3))
                {
                    NvAuPrintf("\n Error: Config file has multiple Secondary"
                                 " Boot Device parameters \n");
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
                }
                ((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] = NvBootDevType_Snor;

                for (i = 0; s_SnorTable[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_SnorTable[i].Name,  Field->Name,
                         NvOsStrlen(s_SnorTable[i].Name)))
                    {
                        if (Index == 0 && s_SnorTable[i].StatusZero == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Snor"
                                          "Params.%s. \n", Index, s_SnorTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 1 && s_SnorTable[i].StatusOne == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Snor"
                                          "Params.%s. \n", Index, s_SnorTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 2 && s_SnorTable[i].StatusTwo == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Snor"
                                          "Params.%s. \n", Index, s_SnorTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 3 && s_SnorTable[i].StatusThree == NV_TRUE)
                       {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Snor"
                                          "Params.%s. \n", Index, s_SnorTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                    }
                }

                for (i = 0; s_SnorTable[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_SnorTable[i].Name,  Field->Name,
                         NvOsStrlen(Field->Name)))
                    {
                        if (Index == 0)
                        {
                            DeviceIndexZero = NV_TRUE;
                            s_SnorTable[i].StatusZero = NV_TRUE;
                        }
                        else if (Index == 1)
                        {
                            DeviceIndexOne = NV_TRUE;
                            s_SnorTable[i].StatusOne = NV_TRUE;
                        }
                        else if (Index == 2)
                        {
                            DeviceIndexTwo = NV_TRUE;
                            s_SnorTable[i].StatusTwo = NV_TRUE;
                        }
                        else if (Index == 3)
                        {
                            DeviceIndexThree = NV_TRUE;
                            s_SnorTable[i].StatusThree = NV_TRUE;
                        }
                    }
                }
            }
            else if (!NvOsStrncmp(s_DeviceTypeTable[i].Prefix,
                        "SpiFlashParams.",
                        NvOsStrlen(s_DeviceTypeTable[i].Prefix)))
            {
                if ((((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Nand) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Sdmmc) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Snor) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Usb3))
                {
                    NvAuPrintf("\n Error: Config file has multiple Secondary Boot"
                                 " Device parameters \n");
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
                }
                ((NvBootConfigTable *)(Context->NvBCT))
                                    ->DevType[Index] = NvBootDevType_Spi;

                for (i = 0; s_SpiFlashTable[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_SpiFlashTable[i].Name,  Field->Name,
                         NvOsStrlen(s_SpiFlashTable[i].Name)))
                    {
                        if (Index == 0 && s_SpiFlashTable[i].StatusZero == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].SpiFlash"
                                          "Params.%s. \n", Index, s_SpiFlashTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 1 && s_SpiFlashTable[i].StatusOne == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].SpiFlash"
                                          "Params.%s. \n", Index, s_SpiFlashTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 2 && s_SpiFlashTable[i].StatusTwo == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].SpiFlash"
                                          "Params.%s. \n", Index, s_SpiFlashTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 3 && s_SpiFlashTable[i].StatusThree == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].SpiFlash"
                                          "Params.%s. \n", Index, s_SpiFlashTable[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                    }
                }

                for (i = 0; s_SpiFlashTable[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_SpiFlashTable[i].Name,  Field->Name,
                         NvOsStrlen(Field->Name)))
                    {
                        if (Index == 0)
                        {
                            DeviceIndexZero = NV_TRUE;
                            s_SpiFlashTable[i].StatusZero = NV_TRUE;
                        }
                        else if (Index == 1)
                        {
                            DeviceIndexOne = NV_TRUE;
                            s_SpiFlashTable[i].StatusOne = NV_TRUE;
                        }
                        else if (Index == 2)
                        {
                            DeviceIndexTwo = NV_TRUE;
                            s_SpiFlashTable[i].StatusTwo = NV_TRUE;
                        }
                        else if (Index == 3)
                        {
                            DeviceIndexThree = NV_TRUE;
                            s_SpiFlashTable[i].StatusThree = NV_TRUE;
                        }
                    }
                }
            }
            else if (!NvOsStrncmp(s_DeviceTypeTable[i].Prefix,
                        "Usb3Params.",
                        NvOsStrlen(s_DeviceTypeTable[i].Prefix)))
            {
                if ((((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Nand) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Sdmmc) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Snor) ||
                   (((NvBootConfigTable *)(Context->NvBCT))
                                ->DevType[Index] == NvBootDevType_Spi))
                {
                    NvAuPrintf("\n Error: Config file has multiple Secondary Boot"
                                 " Device parameters \n");
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
                }
                ((NvBootConfigTable *)(Context->NvBCT))->DevType[Index] = NvBootDevType_Usb3;

                for (i = 0; s_Usb3Table[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_Usb3Table[i].Name,  Field->Name,
                         NvOsStrlen(s_Usb3Table[i].Name)))
                    {
                        if (Index == 0 && s_Usb3Table[i].StatusZero == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Usb3"
                                          "Params.%s. \n", Index, s_Usb3Table[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 1 && s_Usb3Table[i].StatusOne == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Usb3"
                                          "Params.%s. \n", Index, s_Usb3Table[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 2 && s_Usb3Table[i].StatusTwo == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Usb3"
                                          "Params.%s. \n", Index, s_Usb3Table[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                        else if (Index == 3 && s_Usb3Table[i].StatusThree == NV_TRUE)
                        {
                            NvAuPrintf("\n Error: Config file has duplicate"
                                         " parameters for DeviceParam[%d].Usb3"
                                          "Params.%s. \n", Index, s_Usb3Table[i].Name);
                            NvAuPrintf("\n BCT file was not created. \n");
                            exit(1);
                        }
                    }
                }

                for (i = 0; s_Usb3Table[i].Token != 0; i++)
                {
                    if (!NvOsStrncmp(s_Usb3Table[i].Name,  Field->Name,
                         NvOsStrlen(Field->Name)))
                    {
                        if (Index == 0)
                        {
                            DeviceIndexZero = NV_TRUE;
                            s_Usb3Table[i].StatusZero = NV_TRUE;
                        }
                        else if (Index == 1)
                        {
                            DeviceIndexOne = NV_TRUE;
                            s_Usb3Table[i].StatusOne = NV_TRUE;
                        }
                        else if (Index == 2)
                        {
                            DeviceIndexTwo = NV_TRUE;
                            s_Usb3Table[i].StatusTwo = NV_TRUE;
                        }
                        else if (Index == 3)
                        {
                            DeviceIndexThree = NV_TRUE;
                            s_Usb3Table[i].StatusThree = NV_TRUE;
                        }
                    }
                }
            }

            /* Parse the equals sign.*/
            if (*Rest != '=')
            {
                 NvAuPrintf("\nError: Unexpected Input '%s' in cfg file \n ",Rest);
                 NvAuPrintf("\nBCT file was not created. \n");
                 return 1;
            }
            Rest++;

            /* Parse the value based on the field table. */
            Rest = ParseFieldValue(Rest, Field, &Value);
            if (Rest == NULL) return 1;

            return DeviceItem->Process(Context, Index, Field->Token, Value);
        }
    }

    return 1;
}

/*
 * ParseValueNvU32(): General handler for setting NvU32 values in config files.
 */
static int ParseValueNvU32(BuildBctContext *Context, ParseToken Token,
                            char *Rest, char *Prefix)
{
    NvU32 Value;

    NV_ASSERT(Context != NULL);
    NV_ASSERT(Rest    != NULL);

    Rest = ParseNvU32(Rest, &Value);
    if (Rest == NULL) return 1;
    if (Value == 0 && Token != Token_EnableJtag)
    {
        NvAuPrintf("Error: Value entered for %s is not valid."
                   "Please enter Integer Value. \n ", Prefix);
    }

    return t11xSetValue(Context, Token, Value);
}

/*
 * Stop reading and processing of configuration file.
 */
static int EndConfig(BuildBctContext *Context, ParseToken Token,
                char *Rest, char *Prefix)
{
    return 2;
}

/* Return 0 on success, 1 on error */
static int ProcessStatement(BuildBctContext *Context, char *Statement)
{
    int i;
    char *Rest;

    for (i = 0; s_TopLevelItems[i].Prefix != NULL; i++)
    {
        if (!NvOsStrncmp(s_TopLevelItems[i].Prefix, Statement,
                         NvOsStrlen(s_TopLevelItems[i].Prefix)))
        {
            s_TopLevelItems[i].Status = NV_TRUE;
            Rest = Statement + NvOsStrlen(s_TopLevelItems[i].Prefix);

            return s_TopLevelItems[i].Process(Context,
                                              s_TopLevelItems[i].Token,
                                              Rest,
                                              s_TopLevelItems[i].Prefix);
        }
    }

    for (i = 0; s_TopLevelItems[i].Prefix != NULL; i++)
    {
        if (NvOsStrncmp(s_TopLevelItems[i].Prefix, Statement,
                         NvOsStrlen(s_TopLevelItems[i].Prefix)))
        {
            NvAuPrintf("\n Error: Wrong config file is being used. "
                      "Config file has unexpected parameter: %s\n", Statement);
            NvAuPrintf("\n BCT file was not created. \n");
            exit(1);
        }
    }
    /* If this point was reached, there was a processing error. */
    return 1;
}

/* Note: Basic parsing borrowed from nvcamera_config.c */
void t11xProcessConfigFile(BuildBctContext *Context)
{
    char buffer[MAX_BUFFER];
    int  space = 0;
    char current;
    NvBool comment = NV_FALSE;
    NvBool string = NV_FALSE;
    NvBool equalEncountered = NV_FALSE;
    int i;
    int Index = 0;
    int ret   = 0;
    size_t ReadSize = 0;
    int bootdevicetype;

    NV_ASSERT(Context != NULL);
    NV_ASSERT(Context->ConfigFile != NULL);


    while (NvOsFread(Context->ConfigFile, &current, sizeof(current),
                     &ReadSize) == NvSuccess && !ret)
    {
        if (space >= (MAX_BUFFER-1))
        {
            // if we exceeded the max buffer size, it is likely
            // due to a missing semi-colon at the end of a line
            NvAuPrintf("Config file parsing error!");
            exit(1);
        }

        switch (current)
        {
        case '\"': /* " indicates start or end of a string */
            if (!comment)
            {
                string ^= NV_TRUE;
                buffer[space++] = current;
            }
            break;
        case ';':
            if (!string && !comment)
            {
                buffer[space++] = '\0';

                /* Process a statement. */
                ret = ProcessStatement(Context, buffer);
                if (ret == 1)
                {
                    goto error;
                }
                space = 0;
                equalEncountered = NV_FALSE;
            }
            else if (string)
            {
                buffer[space++] = current;
            }
            break;

        //  ignore whitespace.  uses fallthrough
        case '\n':
        case '\r':  // carriage returns end comments
            string  = NV_FALSE;
            comment = NV_FALSE;
        case ' ':
        case '\t':
            if (string)
            {
                buffer[space++] = current;
            }
            break;

        case '#':
            if (!string)
            {
                comment = NV_TRUE;
            }
            else
            {
                buffer[space++] = current;
            }
            break;

        default:
            if (!comment)
            {
                buffer[space++] = current;
                if (current == '=')
                {
                    if (!equalEncountered)
                    {
                        equalEncountered = NV_TRUE;
                    }
                    else
                    {
                        goto error;
                    }
                }
            }
            break;
        }
    }

    for (i = 0; s_SdramFieldTable[i].Token != 0; i++)
    {
        if (SdramIndexZero == NV_TRUE && s_SdramFieldTable[i].StatusZero == NV_FALSE)
        {
            NvAuPrintf("\n Error: Config file doesn't have parameters"
                         " for SDRAM[0].%s. \n",  s_SdramFieldTable[i].Name);
            NvAuPrintf("\n BCT file was not created. \n");
            exit(1);
        }
        if (SdramIndexOne == NV_TRUE && s_SdramFieldTable[i].StatusOne == NV_FALSE)
        {
            NvAuPrintf("\n Error: Config file doesn't have parameters"
                         " for SDRAM[1].%s. \n",  s_SdramFieldTable[i].Name);
            NvAuPrintf("\n BCT file was not created. \n");
            exit(1);
        }
        if (SdramIndexTwo == NV_TRUE&& s_SdramFieldTable[i].StatusTwo == NV_FALSE)
        {
            NvAuPrintf("\n Error: Config file doesn't have parameters"
                         " for SDRAM[2].%s. \n",  s_SdramFieldTable[i].Name);
            NvAuPrintf("\n BCT file was not created. \n");
            exit(1);
        }
        if (SdramIndexThree == NV_TRUE && s_SdramFieldTable[i].StatusThree == NV_FALSE)
        {
            NvAuPrintf("\n Error: Config file doesn't have parameters"
                         " for SDRAM[3].%s. \n",  s_SdramFieldTable[i].Name);
            NvAuPrintf("\n BCT file was not created. \n");
            exit(1);
        }
    }

    if (DeviceIndexZero)
        Index = 0;
    if (DeviceIndexOne)
        Index = 1;
    if (DeviceIndexTwo)
        Index = 2;
    if (DeviceIndexThree)
        Index = 3;

    if ((DeviceIndexZero != NV_TRUE) || (DeviceIndexOne != NV_TRUE) ||
       (DeviceIndexTwo != NV_TRUE) || (DeviceIndexThree != NV_TRUE))
    {
        if ((DeviceIndexZero == NV_TRUE) || (DeviceIndexOne == NV_TRUE) ||
           (DeviceIndexTwo == NV_TRUE) || (DeviceIndexThree == NV_TRUE))
        {
            NvAuPrintf("\n Warning: Config file doesn't have all the 4 sets of device"
                         " parameters. Hence duplicating the unfilled sets with "
                          "previous set values. \n");
            ((NvBootConfigTable *)(Context->NvBCT))->NumParamSets = 4;
            bootdevicetype = ((NvBootConfigTable *)(Context->NvBCT))->DevType[Index];
            for (i = 0; i < NVBOOT_BCT_MAX_PARAM_SETS; i++)
                ((NvBootConfigTable *)(Context->NvBCT))->DevType[i] = bootdevicetype;
        }
    }

    if (((NvBootConfigTable *)(Context->NvBCT))
                    ->DevType[Index] == NvBootDevType_Nand)
    {
        for (i = 0; s_NandTable[i].Token != 0; i++)
        {
            if (DeviceIndexZero == NV_TRUE && s_NandTable[i].StatusZero == NV_FALSE)
            {
                    NvAuPrintf("\n Error: Config file doesn't have parameters for"
                                 " DeviceParam[0].NandParams.%s. \n", s_NandTable[i].Name);
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
            }
            if (DeviceIndexOne == NV_TRUE && s_NandTable[i].StatusOne == NV_FALSE)
            {
                    NvAuPrintf("\n Error: Config file doesn't have parameters for"
                                 " DeviceParam[1].NandParams.%s. \n", s_NandTable[i].Name);
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
            }
            if (DeviceIndexTwo == NV_TRUE && s_NandTable[i].StatusTwo == NV_FALSE)
            {
                    NvAuPrintf("\n Error: Config file doesn't have parameters for"
                                 " DeviceParam[2].NandParams.%s. \n", s_NandTable[i].Name);
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
            }
            if (DeviceIndexThree == NV_TRUE && s_NandTable[i].StatusThree == NV_FALSE)
            {
                    NvAuPrintf("\n Error: Config file doesn't have parameters for"
                                 " DeviceParam[3].NandParams.%s. \n", s_NandTable[i].Name);
                    NvAuPrintf("\n BCT file was not created. \n");
                    exit(1);
            }
        }
        while ((DeviceIndexZero != NV_TRUE) || (DeviceIndexOne != NV_TRUE) ||
               (DeviceIndexTwo != NV_TRUE) || (DeviceIndexThree != NV_TRUE))
        {
            if ((DeviceIndexZero != NV_TRUE) && (DeviceIndexThree == NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[0].NandParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[3].NandParams;
                DeviceIndexZero = NV_TRUE;
            }
            if ((DeviceIndexZero == NV_TRUE) && (DeviceIndexOne != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[1].NandParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[0].NandParams;
                DeviceIndexOne = NV_TRUE;
            }
            if ((DeviceIndexOne == NV_TRUE) && (DeviceIndexTwo != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[2].NandParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[1].NandParams;
                DeviceIndexTwo = NV_TRUE;
            }
            if ((DeviceIndexTwo == NV_TRUE) && (DeviceIndexThree != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[3].NandParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[2].NandParams;
                DeviceIndexThree = NV_TRUE;
            }
        }
    }
    if (((NvBootConfigTable *)(Context->NvBCT))
                    ->DevType[Index] == NvBootDevType_Sdmmc)
    {
        for (i = 0; s_SdmmcTable[i].Token != 0; i++)
        {
            if (DeviceIndexZero == NV_TRUE && s_SdmmcTable[i].StatusZero == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[0].SdmmcParams.%s. \n",
                               s_SdmmcTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexOne == NV_TRUE && s_SdmmcTable[i].StatusOne == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[1].SdmmcParams.%s. \n",
                               s_SdmmcTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexTwo == NV_TRUE && s_SdmmcTable[i].StatusTwo == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[2].SdmmcParams.%s. \n",
                               s_SdmmcTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexThree == NV_TRUE && s_SdmmcTable[i].StatusThree == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[3].SdmmcParams.%s. \n",
                               s_SdmmcTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
        }
        while ((DeviceIndexZero != NV_TRUE) || (DeviceIndexOne != NV_TRUE) ||
              (DeviceIndexTwo != NV_TRUE) || (DeviceIndexThree != NV_TRUE))
        {
            if ((DeviceIndexZero != NV_TRUE) && (DeviceIndexThree == NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[0].SdmmcParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[3].SdmmcParams;
                DeviceIndexZero = NV_TRUE;
            }
            if ((DeviceIndexZero == NV_TRUE) && (DeviceIndexOne != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[1].SdmmcParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[0].SdmmcParams;
                DeviceIndexOne = NV_TRUE;
            }
            if ((DeviceIndexOne == NV_TRUE) && (DeviceIndexTwo != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[2].SdmmcParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[1].SdmmcParams;
                DeviceIndexTwo = NV_TRUE;
            }
            if ((DeviceIndexTwo == NV_TRUE) && (DeviceIndexThree != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[3].SdmmcParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[2].SdmmcParams;
                DeviceIndexThree = NV_TRUE;
            }
        }
    }
    if (((NvBootConfigTable *)(Context->NvBCT))->DevType[Index] == NvBootDevType_Spi)
    {
        for (i = 0; s_SpiFlashTable[i].Token != 0; i++)
        {
            if (DeviceIndexZero == NV_TRUE && s_SpiFlashTable[i].StatusZero == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[0].SpiFlashParams.%s. \n",
                               s_SpiFlashTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexOne == NV_TRUE && s_SpiFlashTable[i].StatusOne== NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[1].SpiFlashParams.%s. \n",
                               s_SpiFlashTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexTwo == NV_TRUE && s_SpiFlashTable[i].StatusTwo== NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[2].SpiFlashParams.%s. \n",
                               s_SpiFlashTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexThree == NV_TRUE && s_SpiFlashTable[i].StatusThree== NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[3].SpiFlashParams.%s. \n",
                               s_SpiFlashTable[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
        }
        while ((DeviceIndexZero != NV_TRUE) || (DeviceIndexOne != NV_TRUE) ||
              (DeviceIndexTwo != NV_TRUE) || (DeviceIndexThree != NV_TRUE))
        {
            if ((DeviceIndexZero != NV_TRUE) && (DeviceIndexThree == NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[0].SpiFlashParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[3].SpiFlashParams;
                DeviceIndexZero = NV_TRUE;
            }
            if ((DeviceIndexZero == NV_TRUE) && (DeviceIndexOne != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[1].SpiFlashParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[0].SpiFlashParams;
                DeviceIndexOne = NV_TRUE;
            }
            if ((DeviceIndexOne == NV_TRUE) && (DeviceIndexTwo != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[2].SpiFlashParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[1].SpiFlashParams;
                DeviceIndexTwo = NV_TRUE;
            }
            if ((DeviceIndexTwo == NV_TRUE) && (DeviceIndexThree != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[3].SpiFlashParams =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[2].SpiFlashParams;
                DeviceIndexThree = NV_TRUE;
            }
        }
    }
    if (((NvBootConfigTable *)(Context->NvBCT))->DevType[Index] == NvBootDevType_Usb3)
    {
        for (i = 0; s_Usb3Table[i].Token != 0; i++)
        {
            if (DeviceIndexZero == NV_TRUE && s_Usb3Table[i].StatusZero == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[0].Usb3Params.%s. \n",
                               s_Usb3Table[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexOne == NV_TRUE && s_Usb3Table[i].StatusOne == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[1].Usb3Params.%s. \n",
                               s_Usb3Table[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexTwo == NV_TRUE && s_Usb3Table[i].StatusTwo == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[2].Usb3Params.%s. \n",
                               s_Usb3Table[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
            if (DeviceIndexThree == NV_TRUE && s_Usb3Table[i].StatusThree == NV_FALSE)
            {
                NvAuPrintf("\n Error: Config file doesn't have parameters"
                             " for DeviceParam[3].Usb3Params.%s. \n",
                               s_Usb3Table[i].Name);
                NvAuPrintf("\n BCT file was not created. \n");
                exit(1);
            }
        }
        while ((DeviceIndexZero != NV_TRUE) || (DeviceIndexOne != NV_TRUE) ||
              (DeviceIndexTwo != NV_TRUE) || (DeviceIndexThree != NV_TRUE))
        {
            if ((DeviceIndexZero != NV_TRUE) && (DeviceIndexThree == NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[0].Usb3Params =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[3].Usb3Params;
                DeviceIndexZero = NV_TRUE;
            }
            if ((DeviceIndexZero == NV_TRUE) && (DeviceIndexOne != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[1].Usb3Params =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[0].Usb3Params;
                DeviceIndexOne = NV_TRUE;
            }
            if ((DeviceIndexOne == NV_TRUE) && (DeviceIndexTwo != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[2].Usb3Params =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[1].Usb3Params;
                DeviceIndexTwo = NV_TRUE;
            }
            if ((DeviceIndexTwo == NV_TRUE) && (DeviceIndexThree != NV_TRUE))
            {
                ((NvBootConfigTable *)(Context->NvBCT))->DevParams[3].Usb3Params =
                    ((NvBootConfigTable *)(Context->NvBCT))->DevParams[2].Usb3Params;
                DeviceIndexThree = NV_TRUE;
            }
        }
    }

    return;

 error:
    NvOsDebugPrintf("Error parsing: %s\n", buffer);
    exit(1);

}
