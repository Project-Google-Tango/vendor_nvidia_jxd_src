/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nverror.h"
#include "nvcommon.h"
#include "nvuart.h"
#include "nvbl_sdmmc.h"
#include "nvbl_sdmmc_headers.h"


NvBootSdmmcContext *s_SdmmcContext = NULL;

// Table that maps fuse values to CMD1 OCR argument values.
NvU32 s_OcrVoltageRange[] =
{
    EmmcOcrVoltageRange_QueryVoltage, // NvBootSdmmcVoltageRange_QueryVoltage
    EmmcOcrVoltageRange_HighVoltage,  // NvBootSdmmcVoltageRange_HighVoltage
    EmmcOcrVoltageRange_DualVoltage,  // NvBootSdmmcVoltageRange_DualVoltage
    EmmcOcrVoltageRange_LowVoltage    // NvBootSdmmcVoltageRange_LowVoltage
};


/*
 * Functions HwSdmmcxxx --> does Sdmmc register programming.
 * Functions Sdmmcxxx --> Common to Emmc and Esd cards.
 * Functions Emmcxxx --> Specific to Emmc card.
 * Functions Esdxxx --> Specific to Esd card.
 * Functions NvBlSdmmcxxx --> Public API's
 */

NvError HwSdmmcResetController(void)
{
    NvU32 StcReg;
    NvU32 ResetInProgress;
    NvU32 TimeOut = SDMMC_TIME_OUT_IN_US;

    // Reset Controller's All reg's.
    StcReg = NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                SW_RESET_FOR_ALL, RESETED);
    NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    // Wait till Reset is completed.
    while (TimeOut)
    {
        NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
        ResetInProgress = NV_DRF_VAL(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                            SW_RESET_FOR_ALL, StcReg);
        if (!ResetInProgress)
            break;
        NvBlUtilWaitUS(1);
        TimeOut--;
        if (!TimeOut)
        {
            PRINT_SDMMC_ERRORS("\r\nReset all timed out.");
            return NvError_Timeout;
        }
    }
    return NvSuccess;
}

NvError HwSdmmcWaitForClkStable(void)
{
    NvU32 StcReg;
    NvU32 ClockReady;
    NvU32 TimeOut = SDMMC_TIME_OUT_IN_US;

    while (TimeOut)
    {
        NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
        ClockReady = NV_DRF_VAL(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                        INTERNAL_CLOCK_STABLE, StcReg);
        if (ClockReady)
            break;
        NvBlUtilWaitUS(1);
        TimeOut--;
        if (!TimeOut)
        {
            PRINT_SDMMC_ERRORS("\r\nHwSdmmcInitController()-Clk stable timed out.");
            return NvError_Timeout;
        }
    }
    return NvSuccess;
}

NvBool HwSdmmcIsCardPresent(void)
{
    NvU32 CardStable;
    NvU32 PresentState;
    NvU32 CardInserted = 0;
    NvU32 TimeOut = SDMMC_TIME_OUT_IN_US;

    while (TimeOut)
    {
        NV_SDMMC_READ(PRESENT_STATE, PresentState);
        CardStable = NV_DRF_VAL(SDMMC, PRESENT_STATE, CARD_STATE_STABLE,
                        PresentState);
        if (CardStable)
        {
            CardInserted = NV_DRF_VAL(SDMMC, PRESENT_STATE, CARD_INSERTED,
                            PresentState);
            break;
        }
        NvBlUtilWaitUS(1);
        TimeOut--;
    }
    PRINT_SDMMC_ERRORS("\r\nCard is %s stable", CardStable ? "":"not");
    PRINT_SDMMC_ERRORS("\r\nCard is %s present", CardInserted ? "":"not");

    return (CardInserted ? NV_TRUE : NV_FALSE);
}

void
HwSdmmcReadResponse(
    SdmmcResponseType ResponseType,
    NvU32* pRespBuffer)
{
    NvU32* pTemp = pRespBuffer;

    switch (ResponseType)
    {
        case SdmmcResponseType_R1:
        case SdmmcResponseType_R1B:
        case SdmmcResponseType_R3:
        case SdmmcResponseType_R4:
        case SdmmcResponseType_R5:
        case SdmmcResponseType_R6:
        case SdmmcResponseType_R7:
            // bits 39:8 of response are mapped to 31:0.
            NV_SDMMC_READ(RESPONSE_R0_R1, *pTemp);
            break;
        case SdmmcResponseType_R2:
            // bits 127:8 of response are mapped to 119:0.
            NV_SDMMC_READ(RESPONSE_R0_R1, *pTemp);
            pTemp++;
            NV_SDMMC_READ(RESPONSE_R2_R3, *pTemp);
            pTemp++;
            NV_SDMMC_READ(RESPONSE_R4_R5, *pTemp);
            pTemp++;
            NV_SDMMC_READ(RESPONSE_R6_R7, *pTemp);
            break;
        case SdmmcResponseType_NoResponse:
        default:
            *pTemp = 0;
    }
}

NvError HwSdmmcWaitForDataLineReady(void)
{
    NvU32 PresentState;
    NvU32 DataLineActive;
    NvU32 TimeOut = s_SdmmcContext->ReadTimeOutInUs;

    while (TimeOut)
    {
        NV_SDMMC_READ(PRESENT_STATE, PresentState);
        DataLineActive = NV_DRF_VAL(SDMMC, PRESENT_STATE, DAT_LINE_ACTIVE,
                            PresentState);
        if (!DataLineActive)
            break;
        NvBlUtilWaitUS(1);
        TimeOut--;
        if (!TimeOut)
        {
            PRINT_SDMMC_ERRORS("\r\nDataLineActive is not set to 0 and timed out");
            return NvError_Timeout;
        }
    }
    return NvSuccess;
}

NvError HwSdmmcWaitForCmdInhibitData(void)
{
    NvU32 PresentState;
    NvU32 CmdInhibitData;
    NvU32 TimeOut = s_SdmmcContext->ReadTimeOutInUs;

    while (TimeOut)
    {
        NV_SDMMC_READ(PRESENT_STATE, PresentState);
        // This bit is set to zero after busy line is deasserted.
        // For response R1b, need to wait for this.
        CmdInhibitData = NV_DRF_VAL(SDMMC, PRESENT_STATE, CMD_INHIBIT_DAT,
                            PresentState);
        if (!CmdInhibitData)
            break;
        NvBlUtilWaitUS(1);
        TimeOut--;
        if (!TimeOut)
        {
            PRINT_SDMMC_ERRORS("\r\nCmdInhibitData is not set to 0 and timed out");
            return NvError_Timeout;
        }
    }
    return NvSuccess;
}

NvError HwSdmmcWaitForCmdInhibitCmd(void)
{
    NvU32 PresentState;
    NvU32 CmdInhibitCmd;
    NvU32 TimeOut = SDMMC_COMMAND_TIMEOUT_IN_US;

    while (TimeOut)
    {
        NV_SDMMC_READ(PRESENT_STATE, PresentState);
        // This bit is set to zero after response is received. So, response
        // registers should be read only after this bit is cleared.
        CmdInhibitCmd = NV_DRF_VAL(SDMMC, PRESENT_STATE, CMD_INHIBIT_CMD,
                            PresentState);
        if (!CmdInhibitCmd)
            break;
        NvBlUtilWaitUS(1);
        TimeOut--;
        if (!TimeOut)
        {
            PRINT_SDMMC_ERRORS("\r\nCmdInhibitCmd is not set to 0 and timed out");
            return NvError_Timeout;
        }
    }
    return NvSuccess;
}

NvError HwSdmmcWaitForCommandComplete(void)
{
    NvU32 CommandDone;
    NvU32 InterruptStatus;
    NvU32 TimeOutCounter = SDMMC_COMMAND_TIMEOUT_IN_US;
    NvU32 ErrorMask = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_INDEX_ERR,
                        ERR) |
                      NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
                        END_BIT_ERR_GENERATED) |
                      NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_CRC_ERR,
                        CRC_ERR_GENERATED) |
                      NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR,
                        TIMEOUT);
    while (TimeOutCounter)
    {
        NV_SDMMC_READ(INTERRUPT_STATUS, InterruptStatus);
        PRINT_SDMMC_MESSAGES("\nInterrupt Status %x",InterruptStatus);
        CommandDone = NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, CMD_COMPLETE,
                        InterruptStatus);
        if (InterruptStatus & ErrorMask)
        {
            PRINT_SDMMC_ERRORS("\r\n Error in HwSdmmcWaitForCommandComplete");
            return NvError_NotInitialized;
        }
        if (CommandDone)
            break;
        NvBlUtilWaitUS(1);
        TimeOutCounter--;
        if (!TimeOutCounter)
        {
            PRINT_SDMMC_ERRORS("\r\nTimed out in HwSdmmcWaitForCommandComplete");
            return NvError_Timeout;
        }
    }
    return NvSuccess;
}

NvError HwSdmmcIssueAbortCommand(void)
{
    NvError e;
    NvU32 retries = 2;
    NvU32 CommandXferMode;
    NvU32 InterruptStatus;
    NvU32* pSdmmcResponse = &s_SdmmcContext->SdmmcResponse[0];

    PRINT_SDMMC_MESSAGES("\r\n\r\n     Sending Abort CMD%d",
        SdmmcCommand_StopTransmission);

    CommandXferMode =
        NV_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_INDEX,
            SdmmcCommand_StopTransmission) |
        NV_DRF_DEF(SDMMC, CMD_XFER_MODE, COMMAND_TYPE, ABORT) |
        NV_DRF_DEF(SDMMC, CMD_XFER_MODE, DATA_PRESENT_SELECT, NO_DATA_TRANSFER) |
        NV_DRF_DEF(SDMMC, CMD_XFER_MODE, CMD_INDEX_CHECK_EN, ENABLE) |
        NV_DRF_DEF(SDMMC, CMD_XFER_MODE, CMD_CRC_CHECK_EN, ENABLE) |
        NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT, RESP_LENGTH_48BUSY) |
        NV_DRF_DEF(SDMMC, CMD_XFER_MODE, DATA_XFER_DIR_SEL, WRITE) |
        NV_DRF_DEF(SDMMC, CMD_XFER_MODE, BLOCK_COUNT_EN, DISABLE) |
        NV_DRF_DEF(SDMMC, CMD_XFER_MODE, DMA_EN, DISABLE);

    while (retries)
    {
        // Clear Status bits what ever is set.
        NV_SDMMC_READ(INTERRUPT_STATUS, InterruptStatus);
        NV_SDMMC_WRITE(INTERRUPT_STATUS, InterruptStatus);
        // This redundant read is for debug purpose.
        NV_SDMMC_READ(INTERRUPT_STATUS, InterruptStatus);
        NV_SDMMC_WRITE(ARGUMENT, 0);
        NV_SDMMC_WRITE(CMD_XFER_MODE, CommandXferMode);
        // Wait for the command to be sent out.if it fails, retry.
        e = HwSdmmcWaitForCommandComplete();
        if (e == NvSuccess)
            break;
        HwSdmmcInitController();
        retries--;
    }
    if (retries)
    {
        // Wait till response is received from card.
        NV_CHECK_ERROR(HwSdmmcWaitForCmdInhibitCmd());
        // Wait till busy line is deasserted by card. It is for R1b response.
        NV_CHECK_ERROR(HwSdmmcWaitForCmdInhibitData());
        HwSdmmcReadResponse(SdmmcResponseType_R1B, pSdmmcResponse);
    }
    return e;
}

NvError HwSdmmcRecoverControllerFromErrors(NvBool IsDataCmd)
{
    NvU32 StcReg;
    NvU32 PresentState;
    NvU32 ResetInProgress;
    NvU32 InterruptStatus;
    NvU32 TimeOut = SDMMC_TIME_OUT_IN_US;
    NvU32 CommandError = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_INDEX_ERR,
                            ERR) |
                         NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
                            END_BIT_ERR_GENERATED) |
                         NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_CRC_ERR,
                            CRC_ERR_GENERATED) |
                         NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR,
                            TIMEOUT);
    NvU32 DataError = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_END_BIT_ERR,
                        ERR) |
                      NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_CRC_ERR,
                        ERR) |
                      NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_TIMEOUT_ERR,
                        TIMEOUT);
    NvU32 DataStateMask = NV_DRF_DEF(SDMMC, PRESENT_STATE, DAT_3_0_LINE_LEVEL,
                            DEFAULT_MASK);

    NV_SDMMC_READ(INTERRUPT_STATUS, InterruptStatus);
    NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    if (InterruptStatus & CommandError)
    {
        // Reset Command line.
        StcReg |= NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                    SW_RESET_FOR_CMD_LINE, RESETED);
        NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
        // Wait till Reset is completed.
        while (TimeOut)
        {
            NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
            ResetInProgress = NV_DRF_VAL(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                                SW_RESET_FOR_CMD_LINE, StcReg);
            if (!ResetInProgress)
                break;
            NvBlUtilWaitUS(1);
            TimeOut--;
        }
        if (!TimeOut)
        {
            PRINT_SDMMC_ERRORS("\r\nReset Command line timed out.");
            return NvError_Timeout;
        }
    }
    if (InterruptStatus & DataError)
    {
        // Reset Data line.
        StcReg |= NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                    SW_RESET_FOR_DAT_LINE, RESETED);
        NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
        // Wait till Reset is completed.
        while (TimeOut)
        {
            NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
            ResetInProgress = NV_DRF_VAL(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                                SW_RESET_FOR_DAT_LINE, StcReg);
            if (!ResetInProgress)
                break;
            NvBlUtilWaitUS(1);
            TimeOut--;
        }
        if (!TimeOut)
        {
            PRINT_SDMMC_ERRORS("\r\nReset Data line timed out.");
            return NvError_Timeout;
        }
    }
    // Clear Interrupt Status
    NV_SDMMC_WRITE(INTERRUPT_STATUS, InterruptStatus);
    // Issue abort command.
    if (IsDataCmd)
        (void)HwSdmmcIssueAbortCommand();
    // Wait for 40us as per spec.
    NvBlUtilWaitUS(40);
    // Read Present State register.
    NV_SDMMC_READ(PRESENT_STATE, PresentState);
    if ( (PresentState & DataStateMask) != DataStateMask )
    {
        // Before give up, try full reset once.
        HwSdmmcInitController();
        NV_SDMMC_READ(PRESENT_STATE, PresentState);
        if ( (PresentState & DataStateMask) != DataStateMask)
        {
            PRINT_SDMMC_ERRORS("\r\nError Recovery Failed.");
            return NvError_NotInitialized;
        }
    }
    return NvSuccess;
}

void HwSdmmcAbortDataRead(void)
{
    NvU32 StcReg;
    NvU32 PresentState;
    NvU32 ResetInProgress;
    NvU32 TimeOut = SDMMC_TIME_OUT_IN_US;
    NvU32 DataStateMask = NV_DRF_DEF(SDMMC, PRESENT_STATE, DAT_3_0_LINE_LEVEL,
                            DEFAULT_MASK);

    NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    // Reset Data line.
    StcReg |= NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                SW_RESET_FOR_DAT_LINE, RESETED);
    NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    // Wait till Reset is completed.
    while (TimeOut)
    {
        NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
        ResetInProgress = NV_DRF_VAL(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                            SW_RESET_FOR_DAT_LINE, StcReg);
        if (!ResetInProgress)
            break;
        NvBlUtilWaitUS(1);
        TimeOut--;
    }
    if (!TimeOut)
    {
        PRINT_SDMMC_ERRORS("\r\nAbortDataRead-Reset Data line timed out.");
    }
    // Read Present State register.
    NV_SDMMC_READ(PRESENT_STATE, PresentState);
    if ( (PresentState & DataStateMask) != DataStateMask )
    {
        // Before give up, try full reset once.
        HwSdmmcInitController();
        NV_SDMMC_READ(PRESENT_STATE, PresentState);
        if ( (PresentState & DataStateMask) != DataStateMask)
        {
            PRINT_SDMMC_ERRORS("\r\nError Recovery Failed.");
        }
    }
}

void HwSdmmcSetNumOfBlocks(NvU32 BlockLength, NvU32 NumOfBlocks)
{
    NvU32 BlockReg;

    BlockReg = NV_DRF_NUM(SDMMC, BLOCK_SIZE_BLOCK_COUNT, BLOCKS_COUNT,
                NumOfBlocks) |
               NV_DRF_DEF(SDMMC, BLOCK_SIZE_BLOCK_COUNT,
               /*
                * This makes controller halt when ever it detects 512KB boundary.
                * When controller halts on this boundary, need to clear the
                * dma block boundary event and write SDMA base address again.
                * Writing address again triggers controller to continue.
                * We can't disable this. We have to live with it.
                */
                HOST_DMA_BUFFER_SIZE, DMA512K) |
               NV_DRF_NUM(SDMMC, BLOCK_SIZE_BLOCK_COUNT,
                XFER_BLOCK_SIZE_11_0, BlockLength);
    NV_SDMMC_WRITE(BLOCK_SIZE_BLOCK_COUNT, BlockReg);
}

void HwSdmmcSetupDma(NvU8 *pBuffer, NvU32 NumOfBytes)
{
    // Program Single DMA base address.
    NV_SDMMC_WRITE(SYSTEM_ADDRESS, (NvU32)(pBuffer));
}

void HwSdmmcEnableHighSpeed(NvBool Enable)
{
    NvU32 PowerControlHostReg = 0;

    NV_SDMMC_READ(POWER_CONTROL_HOST, PowerControlHostReg);
    PowerControlHostReg = NV_FLD_SET_DRF_NUM(SDMMC, POWER_CONTROL_HOST,
                            HIGH_SPEED_EN, ((Enable == NV_TRUE) ? 1 : 0),
                            PowerControlHostReg);
    NV_SDMMC_WRITE(POWER_CONTROL_HOST, PowerControlHostReg);
}

void HwSdmmcCalculateCardClockDivisor(void)
{
    NvU32 TotalClockDivisor = s_SdmmcContext->ClockDivisor;

    s_SdmmcContext->CardClockDivisor = 1;
    s_SdmmcContext->HighSpeedMode = NV_FALSE;
    if ( (s_SdmmcContext->HostSupportsHighSpeedMode == NV_FALSE) ||
         (s_SdmmcContext->CardSupportsHighSpeedMode == NV_FALSE) ||
         (s_SdmmcContext->SpecVersion < 4) )
    {
        // Either card or host doesn't support high speed. So reduce the clock
        // frequency if required.
        if (QUOTIENT_CEILING(SDMMC_PLL_FREQ_IN_MHZ,
            s_SdmmcContext->ClockDivisor) >
            s_SdmmcContext->TranSpeedInMHz)
            s_SdmmcContext->ClockDivisor =
            QUOTIENT_CEILING(SDMMC_PLL_FREQ_IN_MHZ,
            s_SdmmcContext->TranSpeedInMHz);
    }
    else
    {
        while (QUOTIENT_CEILING(SDMMC_PLL_FREQ_IN_MHZ, TotalClockDivisor) >
                SDMMC_MAX_CLOCK_FREQUENCY_IN_MHZ)
        {
            s_SdmmcContext->CardClockDivisor <<= 1;
            TotalClockDivisor <<= 1;
        }
        if (QUOTIENT_CEILING(SDMMC_PLL_FREQ_IN_MHZ, TotalClockDivisor) >
            s_SdmmcContext->TranSpeedInMHz)
            s_SdmmcContext->HighSpeedMode = NV_TRUE;
    }
    PRINT_SDMMC_MESSAGES("\r\nClockDivisor=%d, CardClockDivisor=%d, "
        "HighSpeedMode=%d", s_SdmmcContext->ClockDivisor,
        s_SdmmcContext->CardClockDivisor, s_SdmmcContext->HighSpeedMode);
}

NvBool SdmmcIsCardInTransferState(void)
{
    NvError e;
    NvU32 CardState;
    NvU32* pResp = &s_SdmmcContext->SdmmcResponse[0];

    // Send SEND_STATUS(CMD13) Command.
    NV_CHECK_ERROR_CLEANUP(HwSdmmcSendCommand(SdmmcCommand_SendStatus,
        s_SdmmcContext->CardRca, SdmmcResponseType_R1, NV_FALSE));
    // Extract the Card State from the Response.
    CardState = NV_DRF_VAL(SDMMC, CS, CURRENT_STATE, pResp[0]);
    if (CardState == SdmmcState_Tran)
        return NV_TRUE;
fail:
    return NV_FALSE;
}

NvError EmmcGetOpConditions(void)
{
    NvError e;
    NvU32 StartTime;
    NvU32 OCRRegister = 0;
    NvU32 ElapsedTime = 0;
    NvU32* pSdmmcResponse = &s_SdmmcContext->SdmmcResponse[0];
    NvU32 Cmd1Arg = EmmcOcrVoltageRange_QueryVoltage;

    StartTime = NvBlUtilGetTimeUS();
    // Send SEND_OP_COND(CMD1) Command.
    while (ElapsedTime <= SDMMC_OP_COND_TIMEOUT_IN_US)
    {
        NV_CHECK_ERROR(HwSdmmcSendCommand(
            SdmmcCommand_EmmcSendOperatingConditions, Cmd1Arg,
            SdmmcResponseType_R3, NV_FALSE));
        // Extract OCR from Response.
        OCRRegister = pSdmmcResponse[SDMMC_OCR_RESPONSE_WORD];
        // Check for Card Ready.
        if (OCRRegister & SDMMC_OCR_READY_MASK)
            break;
        if (Cmd1Arg == EmmcOcrVoltageRange_QueryVoltage)
        {
            if (OCRRegister & EmmcOcrVoltageRange_HighVoltage)
            {
                Cmd1Arg = EmmcOcrVoltageRange_HighVoltage;
                s_SdmmcContext->IsHighVoltageRange = NV_TRUE;
            }
            else if (OCRRegister & EmmcOcrVoltageRange_LowVoltage)
            {
                Cmd1Arg = EmmcOcrVoltageRange_LowVoltage;
                s_SdmmcContext->IsHighVoltageRange = NV_FALSE;
            }
            else
            {
                ElapsedTime = NvBlUtilElapsedTimeUS(StartTime);
                continue;
            }
            Cmd1Arg |= SDMMC_CARD_CAPACITY_MASK;
            StartTime = NvBlUtilGetTimeUS();
            continue;
        }
        #if DEBUG_SDMMC
        ElapsedTime += 10000;
        #else
        ElapsedTime = NvBlUtilElapsedTimeUS(StartTime);
        #endif
        // Wait for ten milliseconds between commands. This to avoid
        // sending cmd1 too many times.
        NvBlUtilWaitUS(10000);
    }
    if (ElapsedTime > SDMMC_OP_COND_TIMEOUT_IN_US)
    {
        PRINT_SDMMC_ERRORS("\r\nTimeout during CMD1");
        return NvError_Timeout;
    }
    PRINT_SDMMC_MESSAGES("\r\nTimeTaken for CMD1=%dus", ElapsedTime);
    s_SdmmcContext->IsHighCapacityCard = ( (OCRRegister &
                                           SDMMC_CARD_CAPACITY_MASK) ?
                                           NV_TRUE : NV_FALSE );
    return e;
}

NvError SdmmcGetCsd(void)
{
    NvU32 Mult;
    NvU32 CSize;
    NvError e;
    NvU32 CSizeMulti;
    NvU32* pResp = &s_SdmmcContext->SdmmcResponse[0];

    // Send SEND_CSD(CMD9) Command.
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_SendCsd,
        s_SdmmcContext->CardRca, SdmmcResponseType_R2, NV_FALSE));
    // Extract the page size log2 from Response data.
    s_SdmmcContext->PageSizeLog2 = NV_DRF_VAL(EMMC, CSD, READ_BL_LEN, pResp[2]);
    s_SdmmcContext->PageSizeLog2ForCapacity = s_SdmmcContext->PageSizeLog2;
    s_SdmmcContext->BlockSizeLog2 = NVBOOT_SDMMC_BLOCK_SIZE_LOG2;
    /*
     * The page size can be 512, 1024, 2048 or 4096. this size can only be used
     * for Card capacity calculation. For Read/Write operations, We must use
     * 512 byte page size only.
     */
    // Restrict the reads to (1 << EMMC_MAX_PAGE_SIZE_LOG_2) byte reads.
    if (s_SdmmcContext->PageSizeLog2 > SDMMC_MAX_PAGE_SIZE_LOG_2)
        s_SdmmcContext->PageSizeLog2 = SDMMC_MAX_PAGE_SIZE_LOG_2;
    if (s_SdmmcContext->PageSizeLog2 == 0)
        return NvError_NotInitialized;
    s_SdmmcContext->PagesPerBlockLog2 = (s_SdmmcContext->BlockSizeLog2 -
                                           s_SdmmcContext->PageSizeLog2);
    // Extract the Spec Version from Response data.
    s_SdmmcContext->SpecVersion = NV_DRF_VAL(EMMC, CSD, SPEC_VERS, pResp[3]);
    s_SdmmcContext->taac = NV_DRF_VAL(EMMC, CSD, TAAC, pResp[3]);
    s_SdmmcContext->nsac = NV_DRF_VAL(EMMC, CSD, NSAC, pResp[3]);
    s_SdmmcContext->TranSpeed = NV_DRF_VAL(EMMC, CSD, TRAN_SPEED, pResp[2]);

    // For <= Emmc v4.0, v4.1, v4.2 and < Esd v1.10.
    s_SdmmcContext->TranSpeedInMHz = 20;
    // For Emmc v4.3, v4.4 and Esd 1.10 onwards.
    if (s_SdmmcContext->TranSpeed == EMMC_CSD_V4_3_TRAN_SPEED)
    {
        // For Esd, it is 25MHz.
        s_SdmmcContext->TranSpeedInMHz = 25;
        GetTransSpeed();
    }

    if (s_SdmmcContext->SpecVersion >= 4)
        s_SdmmcContext->CardSupportsHighSpeedMode = NV_TRUE;
    // Fund out number of blocks in card.
    CSize = NV_DRF_VAL(EMMC, CSD, C_SIZE_0, pResp[1]);
    CSize |= (NV_DRF_VAL(EMMC, CSD, C_SIZE_1, pResp[2]) <<
             EMMC_CSD_C_SIZE_1_LEFT_SHIFT_OFFSET);
    CSizeMulti = NV_DRF_VAL(EMMC, CSD, C_SIZE_MULTI, pResp[1]);
    if ( (CSize == EMMC_CSD_MAX_C_SIZE) &&
         (CSizeMulti == EMMC_CSD_MAX_C_SIZE_MULTI) )
    {
        // Capacity is > 2GB and should be calculated from ECSD fields,
        // which is done in EmmcGetExtCSD() method.
        PRINT_SDMMC_MESSAGES("\r\nSdmmcGetCsd:Capacity is > 2GB")
    }
    else
    {
        Mult = 1 << (CSizeMulti + 2);
        s_SdmmcContext->NumOfBlocks = (CSize + 1) * Mult *
                               (1 << (s_SdmmcContext->PageSizeLog2ForCapacity -
                                s_SdmmcContext->PageSizeLog2));
        PRINT_SDMMC_MESSAGES("\r\nCsd NumOfBlocks=%d",
            s_SdmmcContext->NumOfBlocks);
    }

    PRINT_SDMMC_MESSAGES("\r\nPage size from Card=0x%x, 0x%x",
        s_SdmmcContext->PageSizeLog2, (1 << s_SdmmcContext->PageSizeLog2));
    PRINT_SDMMC_MESSAGES("\r\nEmmc SpecVersion=0x%x", s_SdmmcContext->SpecVersion);
    PRINT_SDMMC_MESSAGES("\r\ntaac=0x%x", s_SdmmcContext->taac);
    PRINT_SDMMC_MESSAGES("\r\nnsac=0x%x", s_SdmmcContext->nsac);
    PRINT_SDMMC_MESSAGES("\r\nTranSpeed=0x%x", s_SdmmcContext->TranSpeed);
    PRINT_SDMMC_MESSAGES("\r\nTranSpeedInMHz=%d", s_SdmmcContext->TranSpeedInMHz);
    PRINT_SDMMC_MESSAGES("\r\nCardCommandClasses=0x%x", NV_DRF_VAL(EMMC, CSD,
        CCC, pResp[2]));
    PRINT_SDMMC_MESSAGES("\r\nCSize=0x%x, CSizeMulti=0x%x", CSize, CSizeMulti);
    return e;
}

NvError
EmmcVerifyResponse(
    SdmmcCommand command,
    NvBool AfterCmdExecution)
{
    NvU32* pResp = &s_SdmmcContext->SdmmcResponse[0];
    NvU32 AddressOutOfRange = NV_DRF_VAL(SDMMC, CS, ADDRESS_OUT_OF_RANGE, pResp[0]);
    NvU32 AddressMisalign = NV_DRF_VAL(SDMMC, CS, ADDRESS_MISALIGN, pResp[0]);
    NvU32 BlockLengthError = NV_DRF_VAL(SDMMC, CS, BLOCK_LEN_ERROR, pResp[0]);
    NvU32 CommandCrcError = NV_DRF_VAL(SDMMC, CS, COM_CRC_ERROR, pResp[0]);
    // For illegal commands, card does not respond. It can
    // be known only through CMD13.
    NvU32 IllegalCommand = NV_DRF_VAL(SDMMC, CS, ILLEGAL_CMD, pResp[0]);
    NvU32 CardInternalError = NV_DRF_VAL(SDMMC, CS, CC_ERROR, pResp[0]);
    NvU32 CardEccError = NV_DRF_VAL(SDMMC, CS, CARD_ECC_FAILED, pResp[0]);
    NvU32 SwitchError = NV_DRF_VAL(SDMMC, CS, SWITCH_ERROR, pResp[0]);
    NvBool BeforeCommandExecution = (AfterCmdExecution ? NV_FALSE : NV_TRUE);

    if ((command == SdmmcCommand_ReadSingle) || (command == SdmmcCommand_ReadMulti))
    {
        if (BeforeCommandExecution)
        {
            // This is during response time.
            if ( AddressOutOfRange || AddressMisalign || BlockLengthError ||
                 CardInternalError )
            {
                PRINT_SDMMC_ERRORS("\r\nReadSingle/Multi Operation failed.");
                return NvError_NotInitialized;
            }
        }
        else if (CommandCrcError || IllegalCommand || CardEccError)
        {
            return NvError_NotInitialized;
        }
    }
    else if (command == SdmmcCommand_SetBlockLength)
    {
        if ( BeforeCommandExecution && (BlockLengthError || CardInternalError) )
        {
            // Either the argument of a SET_BLOCKLEN command exceeds the
            // maximum value allowed for the card, or the previously defined
            // block length is illegal for the current command
            PRINT_SDMMC_ERRORS("\r\nSetBlockLength Operation failed.");
            return NvError_NotInitialized;
        }
    }
    else if (command == SdmmcCommand_Switch)
    {
        if ( AfterCmdExecution && (SwitchError || CommandCrcError) )
        {
            // If set, the card did not switch to the expected mode as
            // requested by the SWITCH command.
            PRINT_SDMMC_ERRORS("\r\nSwitch Operation failed.");
            return NvError_NotInitialized;
        }
    }
    else if (command == SdmmcCommand_EmmcSendExtendedCsd)
    {
        if (BeforeCommandExecution && CardInternalError)
        {
            PRINT_SDMMC_ERRORS("\r\nSend Extneded CSD Operation failed.");
            return NvError_NotInitialized;
        }
    }
    else if (command == SdmmcCommand_EsdSelectPartition)
    {
        if (AfterCmdExecution && AddressOutOfRange)
        {
            PRINT_SDMMC_ERRORS("\r\nEsdSelectPartition Out of range error.");
            return NvError_NotInitialized;
        }
    }
    return NvSuccess;
}

NvError EmmcSendSwitchCommand(NvU32 CmdArg)
{
    NvError e;
    SdmmcResponseType Response = SdmmcResponseType_R1B;

    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_Switch,
        CmdArg, Response, NV_FALSE));

    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_SendStatus,
        s_SdmmcContext->CardRca, SdmmcResponseType_R1, NV_FALSE));
    NV_CHECK_ERROR(EmmcVerifyResponse(SdmmcCommand_Switch, NV_TRUE));
    return e;
}

NvError EmmcSelectAccessRegion(SdmmcAccessRegion region)
{
    NvU32 CmdArg;
    NvError e;
    NV_ASSERT(region < SdmmcAccessRegion_Num);

    CmdArg = s_SdmmcContext->BootConfig & (~EMMC_SWITCH_SELECT_PARTITION_MASK);
    CmdArg |= region;
    CmdArg <<= EMMC_SWITCH_SELECT_PARTITION_OFFSET;
    CmdArg |= EMMC_SWITCH_SELECT_PARTITION_ARG;
    PRINT_SDMMC_MESSAGES("\nCmdArg = %x", CmdArg);
    NV_CHECK_ERROR(EmmcSendSwitchCommand(CmdArg));
    s_SdmmcContext->CurrentAccessRegion = region;
    PRINT_SDMMC_MESSAGES("\r\nSelected Region=%d(1->BP1, 2->BP2, 0->User)",
        region);
    return e;
}

NvError SdmmcSelectAccessRegion(NvU32* Block, NvU32* Page)
{
    NvError e = NvSuccess;
    SdmmcAccessRegion region;
    NvU32 BlocksPerPartition = s_SdmmcContext->EmmcBootPartitionSize >>
                               s_SdmmcContext->BlockSizeLog2;
    PRINT_SDMMC_MESSAGES("\nBlocksPerPartition = %d", BlocksPerPartition);
    PRINT_SDMMC_MESSAGES("\nBlock = %d", *Block);

    // If boot partition size is zero, then the card is either eSD or
    // eMMC version is < 4.3.
    if (s_SdmmcContext->EmmcBootPartitionSize == 0)
    {
        s_SdmmcContext->CurrentAccessRegion = SdmmcAccessRegion_UserArea;
        return e;
    }
    // This will not work always, if the request is a multipage one.
    // But this driver never gets multipage requests.
    if ( (*Block) < BlocksPerPartition )
    {
        region = SdmmcAccessRegion_BootPartition1;
    }
    else if ( (*Block) < (BlocksPerPartition << 1) )
    {
        region = SdmmcAccessRegion_BootPartition2;
        *Block = (*Block) - BlocksPerPartition;
    }
    else
    {
        region = SdmmcAccessRegion_UserArea;
        *Block = (*Block) - (BlocksPerPartition << 1);
    }

    if (region != s_SdmmcContext->CurrentAccessRegion)
        NV_CHECK_ERROR(EmmcSelectAccessRegion(region));
    return e;
}

void EmmcEnableHighSpeed(void)
{
    NvError e;
    NvU8* pBuffer = (NvU8*)&s_SdmmcContext->SdmmcInternalBuffer[0];

    // Clear controller's high speed bit.
    HwSdmmcEnableHighSpeed(NV_FALSE);
    // Enable the High Speed Mode, if required.
    if (s_SdmmcContext->HighSpeedMode)
    {
        PRINT_SDMMC_MESSAGES("\r\n\r\nSet High speed to %d",
            s_SdmmcContext->HighSpeedMode);
        NV_CHECK_ERROR_CLEANUP(EmmcSendSwitchCommand(
            EMMC_SWITCH_HIGH_SPEED_ENABLE_ARG));
        // Set the clock for data transfer.
        HwSdmmcSetCardClock(NvBootSdmmcCardClock_DataTransfer);
        // Validate high speed mode bit from card here.
        NV_CHECK_ERROR_CLEANUP(EmmcGetExtCsd());
        if (pBuffer[EMMC_ECSD_HS_TIMING_OFFSET])
        {
            // As per Hw team, it should not be enabled. See Hw bug number
            // AP15#353684/AP20#478599.
            //HwSdmmcEnableHighSpeed(NV_TRUE);
            return;
        }
    fail:
        // If enable high speed fails, run in normal speed.
        PRINT_SDMMC_ERRORS("\r\nEmmcEnableHighSpeed Failed");
        s_SdmmcContext->CardSupportsHighSpeedMode = NV_FALSE;
        // Find out clock divider for card clock again.
        HwSdmmcCalculateCardClockDivisor();
    }
}

/* Public Function Definitions*/
void
NvNvBlSdmmcGetBlockSizes(
    NvU32 *BlockSizeLog2,
    NvU32 *PageSizeLog2)
{
    NV_ASSERT(BlockSizeLog2 != NULL);
    NV_ASSERT(PageSizeLog2 != NULL);
    NV_ASSERT(s_SdmmcContext != NULL);

    *BlockSizeLog2 = s_SdmmcContext->BlockSizeLog2;
    *PageSizeLog2 = s_SdmmcContext->PageSizeLog2;
    PRINT_SDMMC_MESSAGES("\r\nBlockSize=%d, PageSize=%d, PagesPerBlock=%d",
         (1 << s_SdmmcContext->BlockSizeLog2),
         (1 << s_SdmmcContext->PageSizeLog2),
         (1 << s_SdmmcContext->PagesPerBlockLog2));
}

void
NvNvBlSdmmcGetPagesPerChunkLog2(NvU32 *PagesPerChunkLog2)
{
    *PagesPerChunkLog2 = s_SdmmcContext->BlockSizeLog2 -     \
                                 s_SdmmcContext->PageSizeLog2;
}


void NvBootResetSetEnable(NvU32 port, NvU32 EnableOrDisable)
{
    if(EnableOrDisable)
    {
        switch(port)
        {
            case 0://sdmmc 0
                RESET_ENABLE(1, L);
                break;
#ifdef CLK_RST_CONTROLLER_RST_DEVICES_L_SWR_SDMMC2_RST_ENABLE
            case 1://sdmmc1
                RESET_ENABLE(2,L);
                break;
#endif
            case 2://sdmmc2
                RESET_ENABLE(3,U);
                break;
            case 3://sdmmc3
                RESET_ENABLE(4,L);
                break;
            default:
               break;
        }
    }
    else
    {
        switch(port)
        {
            case 0://sdmmc 0
                RESET_DISABLE(1, L);
                break;
#ifdef CLK_RST_CONTROLLER_RST_DEVICES_L_SWR_SDMMC2_RST_ENABLE
            case 1://sdmmc1
                RESET_DISABLE(2,L);
                break;
#endif
            case 2://sdmmc2
                RESET_DISABLE(3,U);
                break;
            case 3://sdmmc3
                RESET_DISABLE(4,L);
                break;
            default:
             break;
        }
    }
    return;
}


void NvClocksConfigureClock(NvU32 port, NvU32 divisor)
{
    switch(port)
    {
        case 0://sdmmc 0
            CLOCK_SET_SOUCE_DIVIDER(1, L, divisor);
            break;
#ifdef CLK_RST_CONTROLLER_RST_DEVICES_L_SWR_SDMMC2_RST_ENABLE
        case 1://sdmmc1
            CLOCK_SET_SOUCE_DIVIDER(2,L, divisor);
            break;
#endif
        case 2://sdmmc2
            CLOCK_SET_SOUCE_DIVIDER(3,U, divisor);
            break;
        case 3://sdmmc3
            CLOCK_SET_SOUCE_DIVIDER(4,L, divisor);
            break;
        default:
           break;
    }
    return;
}

void NvClocksSetEnable(NvU32 port, NvU32 EnableOrDisable)
{
    if(EnableOrDisable)
    {
        switch(port)
        {
            case 0://sdmmc 0
                CLOCK_ENABLE(1, L);
                break;
#ifdef CLK_RST_CONTROLLER_RST_DEVICES_L_SWR_SDMMC2_RST_ENABLE
            case 1://sdmmc1
                CLOCK_ENABLE(2,L);
                break;
#endif
            case 2://sdmmc2
                CLOCK_ENABLE(3,U);
                break;
            case 3://sdmmc3
                CLOCK_ENABLE(4,L);
                break;
            default:
               break;
        }
    }
    else
    {
        switch(port)
        {
            case 0://sdmmc 0
                CLOCK_DISABLE(1, L);
                break;
#ifdef CLK_RST_CONTROLLER_RST_DEVICES_L_SWR_SDMMC2_RST_ENABLE
            case 1://sdmmc1
                CLOCK_DISABLE(2,L);
                break;
#endif
            case 2://sdmmc2
                CLOCK_DISABLE(3,U);
                break;
            case 3://sdmmc3
                CLOCK_DISABLE(4,L);
                break;
            default:
             break;
        }
   }
    return;
}

NvU32 NvBlUtilGetTimeUS( void )
{
    // Should we take care of roll over of us counter? roll over happens after 71.58 minutes.
    NvU32 usec;
    usec = *(volatile NvU32 *)(NV_ADDRESS_MAP_TMRUS_BASE);
    return usec;
}

NvU32 NvBlUtilElapsedTimeUS(NvU32 StartTime)
{
    // doing a diff and ignoring the overflow gets you the correct value
    // even at wrararound, e.g.
    // StartTime   = 0xFFFFFFFF
    // CurrentTime = 0x00000000
    // Current - Start = 1 + overflow flag (ignored in C)
    // this would *NOT* work if the counter was not 32 bits
    return  NvBlUtilGetTimeUS() - StartTime ;
}

void NvBlUtilWaitUS( NvU32 usec )
{
    NvU32 t0;
    NvU32 t1;

    t0 = NvBlUtilGetTimeUS();
    t1 = t0;

    // Use the difference for the comparison to be wraparound safe
    while( (t1 - t0) < usec )
    {
        t1 = NvBlUtilGetTimeUS();
    }
}

