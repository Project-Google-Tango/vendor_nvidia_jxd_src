/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#ifndef INCLUDED_NVAVP_SDMMC_H
#define INCLUDED_NVAVP_SDMMC_H

#include "nverror.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "arsdmmc.h"
#include "nvrm_drf.h"
#include "arclk_rst.h"
#include "nvbl_sdmmc_context.h"
#include "nvbl_sdmmc_int.h"
#include "nvrm_hardware_access.h"
#include "nvuart.h"


#if defined(__cplusplus)
extern "C"
{
#endif

#define DEBUG_SDMMC 0

#if DEBUG_SDMMC
#define PRINT_SDMMC_REG_ACCESS(...)  NvOsAvpDebugPrintf(__VA_ARGS__);
#define PRINT_SDMMC_MESSAGES(...)    NvOsAvpDebugPrintf(__VA_ARGS__);
#define PRINT_SDMMC_ERRORS(...)      NvOsAvpDebugPrintf(__VA_ARGS__);
#else
#define PRINT_SDMMC_REG_ACCESS(...)
#define PRINT_SDMMC_MESSAGES(...)
#define PRINT_SDMMC_ERRORS(...)
#endif


#define      NV_ADDRESS_MAP_TMRUS_BASE           0x60005010
#define      NV_ADDRESS_MAP_PPSB_CLK_RST_BASE    0x60006000
#define      NV_ADDRESS_MAP_PMC_BASE             0x7000E400
#define      NV_ADDRESS_MAP_SDMMC3_BASE          0x78000400

#ifdef T124_NVAVP_SDMMC
#define      NV_ADDRESS_MAP_SDMMC4_BASE          0x700B0600
#else
#define      NV_ADDRESS_MAP_SDMMC4_BASE          0x78000600
#endif

#define NV_SDMMC_READ(reg, value)                                        \
    do                                                                   \
    {                                                                    \
        value = NV_READ32((s_SdmmcContext->SdControllerBaseAddress +     \
                    SDMMC_##reg##_0));                                   \
        PRINT_SDMMC_REG_ACCESS("\r\nRead %s Offset 0x%x = 0x%8.8x", #reg,\
            SDMMC_##reg##_0, value);                                     \
    } while (0)

#define NV_SDMMC_WRITE(reg, value)                                        \
    do {                                                                  \
        NV_WRITE32((s_SdmmcContext->SdControllerBaseAddress +             \
                SDMMC_##reg##_0), value);                                 \
        PRINT_SDMMC_REG_ACCESS("\r\nWrite %s Offset 0x%x = 0x%8.8x", #reg,\
            SDMMC_##reg##_0, value);                                      \
    } while (0)

#define NV_SDMMC_WRITE_08(reg, offset, value)                             \
    do {                                                                  \
        NV_WRITE08((s_SdmmcContext->SdControllerBaseAddress +             \
              SDMMC_##reg##_0 + offset),                                  \
                   value);                                                \
        PRINT_SDMMC_REG_ACCESS("\r\nWrite %s Offset 0x%x = 0x%8.8x", #reg,\
            (SDMMC_##reg##_0 + offset), value);                           \
    } while (0)


#define NV_CLK_RST_READ(reg, value)                                       \
    do                                                                    \
    {                                                                     \
        value = NV_READ32(NV_ADDRESS_MAP_PPSB_CLK_RST_BASE                \
                          + CLK_RST_CONTROLLER_##reg##_0);                \
    } while (0)

#define NV_CLK_RST_WRITE(reg, value)                                        \
    do                                                                      \
    {                                                                       \
        NV_WRITE32((NV_ADDRESS_MAP_PPSB_CLK_RST_BASE                        \
                    + CLK_RST_CONTROLLER_##reg##_0), value);                \
    } while (0)


 #define RESET_ENABLE(instance, clock)                                       \
   do                                                                        \
   {                                                                         \
        NvU32 Reg= 0;                                                        \
        NV_CLK_RST_READ(RST_DEVICES_##clock, Reg);                           \
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_##clock,    \
                                  SWR_SDMMC##instance##_RST, ENABLE, Reg);   \
        NV_CLK_RST_WRITE(RST_DEVICES_##clock, Reg);                          \
    }while(0)

#define RESET_DISABLE(instance, clock)                                      \
    do                                                                      \
    {                                                                       \
        NvU32 Reg = 0;                                                      \
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,                        \
            RST_DEVICES_##clock, SWR_SDMMC##instance##_RST, DISABLE, Reg);\
        NV_CLK_RST_WRITE(RST_DEVICES_##clock, Reg);                         \
    }while(0)


#define CLOCK_SET_SOUCE_DIVIDER(instance, clock, divisor)                     \
    do                                                                        \
    {                                                                         \
        NvU32 Reg = 0;                                                        \
        Reg = NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SDMMC##instance,      \
                         SDMMC##instance##_CLK_SRC, PLLP_OUT0);               \
        Reg |= NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SDMMC##instance,     \
                         SDMMC##instance##_CLK_DIVISOR, divisor);             \
        NV_CLK_RST_WRITE(CLK_SOURCE_SDMMC##instance, Reg);                    \
        NV_CLK_RST_READ(CLK_OUT_ENB_##clock, Reg);                            \
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_##clock,     \
                                 CLK_ENB_SDMMC##instance, ENABLE, Reg);       \
        NV_CLK_RST_WRITE(CLK_OUT_ENB_##clock, Reg);                           \
    } while (0)

#define CLOCK_ENABLE(instance, clock)                                         \
    do                                                                        \
    {                                                                         \
        NvU32 Reg = 0;                                                        \
        NV_CLK_RST_READ(CLK_OUT_ENB_##clock, Reg);                            \
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_##clock,     \
                                 CLK_ENB_SDMMC##instance, ENABLE, Reg);       \
        NV_CLK_RST_WRITE(CLK_OUT_ENB_##clock, Reg);                           \
    } while (0)

#define CLOCK_DISABLE(instance, clock)                                        \
    do                                                                        \
    {                                                                         \
        NvU32 Reg = 0;                                                            \
        NV_CLK_RST_READ(CLK_OUT_ENB_##clock, Reg);                            \
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_##clock,     \
                                 CLK_ENB_SDMMC##instance, DISABLE, Reg);      \
        NV_CLK_RST_WRITE(CLK_OUT_ENB_##clock, Reg);                           \
    } while (0)


// Can't use do while for this, as it is also called from PRINT_SDMMC_XXX.
#define QUOTIENT_CEILING(dividend, divisor)                   \
                        ((dividend + divisor - 1) / divisor)
#define NVSDMMC_CLOCKS_7_1_DIVIDER_BY(INT_RATIO, PLUS_HALF)   \
                       (2 * INT_RATIO + PLUS_HALF - 2)


NvError HwSdmmcResetController(void);
NvError HwSdmmcWaitForClkStable(void);
NvBool HwSdmmcIsCardPresent(void);
void HwSdmmcReadResponse(SdmmcResponseType ResponseType, NvU32* pRespBuffer);
NvError HwSdmmcWaitForDataLineReady(void);
NvError HwSdmmcWaitForCmdInhibitData(void);
NvError HwSdmmcWaitForCmdInhibitCmd(void);
NvError HwSdmmcWaitForCommandComplete(void);
NvError HwSdmmcIssueAbortCommand(void);
NvError HwSdmmcRecoverControllerFromErrors(NvBool IsDataCmd);
void HwSdmmcAbortDataRead(void);
void HwSdmmcSetNumOfBlocks(NvU32 BlockLength, NvU32 NumOfBlocks);
void HwSdmmcSetupDma(NvU8 *pBuffer, NvU32 NumOfBytes);
void HwSdmmcEnableHighSpeed(NvBool Enable);
void HwSdmmcCalculateCardClockDivisor(void);
NvBool SdmmcIsCardInTransferState(void);
NvError EmmcGetOpConditions(void);
NvError SdmmcGetCsd(void);
NvError EmmcVerifyResponse(SdmmcCommand command, NvBool AfterCmdExecution);
NvError EmmcSendSwitchCommand(NvU32 CmdArg);
NvError EmmcSelectAccessRegion(SdmmcAccessRegion region);
NvError SdmmcSelectAccessRegion(NvU32* Block, NvU32* Page);
void EmmcEnableHighSpeed(void);
void NvNvBlSdmmcGetBlockSizes(NvU32 *BlockSizeLog2, NvU32 *PageSizeLog2);
void NvNvBlSdmmcGetPagesPerChunkLog2(NvU32 *PagesPerChunkLog2);
void NvBootResetSetEnable(NvU32 port, NvU32 EnableOrDisable);
void NvClocksConfigureClock(NvU32 port, NvU32 divisor);
void NvClocksSetEnable(NvU32 port, NvU32 EnableOrDisable);
NvU32 NvBlUtilGetTimeUS( void );
NvU32 NvBlUtilElapsedTimeUS(NvU32 StartTime);
void NvBlUtilWaitUS( NvU32 usec );

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVAVP_SDMMC_H */


