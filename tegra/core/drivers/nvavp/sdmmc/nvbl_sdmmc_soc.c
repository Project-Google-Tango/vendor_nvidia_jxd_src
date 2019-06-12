/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include "nvbl_sdmmc_headers.h"
#include "nvbl_sdmmc_t30_int.h"
#include "nvbl_sdmmc.h"
#include "nvsdmmc.h"

extern NvBootSdmmcContext *s_SdmmcContext;
NvBootSdmmcContext s_DeviceContext;


NvError HwSdmmcSetCardClock(NvBootSdmmcCardClock ClockRate)
{
    NvU32 taac;
    NvU32 nsac;
    NvU32 StcReg;
    NvError e;
    NvU32 CardClockInMHz;
    NvU32 CardClockDivisor;
    NvU32 TimeOutCounter = 0;
    NvU32 ClockCyclesRequired;
    NvU32 ControllerClockInMHz;
    NvU32 CardCycleTimeInNanoSec;
    NvU32 ControllerClockDivisor;
    NvU32 ContollerCycleTimeInNanoSec;
    // These array values are as per Emmc/Esd Spec's.
    const NvU32 TaacTimeUnitArray[] = {1, 10, 100, 1000, 10000, 100000, 1000000,
                                       10000000};
    const NvU32 TaacMultiplierArray[] = {10, 10, 12, 13, 15, 20, 25, 30, 35, 40,
                                         45, 50, 55, 60, 70, 80};

    s_SdmmcContext->CurrentClockRate = ClockRate;
    // Disable Card clock before changing it's Frequency.
    NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    StcReg = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                SD_CLOCK_EN, DISABLE, StcReg);
    NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);

    if (ClockRate == NvBootSdmmcCardClock_Identification)
    {
        /*
         * Set the clock divider as 18 for card identification. With clock divider
         * as 18, controller gets a frequency of 216/9 = 24MHz and it will be
         * furthur divided by 64. After dividing it by 64, card gets a
         * frequency of 375KHz. It is the frequency at which card should
         * be identified.
         */
#if TEST_ON_FPGA
        ControllerClockDivisor = 2;
        CardClockDivisor = 18;
#else
        ControllerClockDivisor = 9;
        CardClockDivisor = 64;
#endif
    }
    else if (ClockRate == NvBootSdmmcCardClock_DataTransfer)
    {
#if TEST_ON_FPGA
        ControllerClockDivisor = 8;
        CardClockDivisor = 2;
#else
        ControllerClockDivisor = s_SdmmcContext->ClockDivisor;
        CardClockDivisor = s_SdmmcContext->CardClockDivisor;
#endif
    }
    else //if (ClockRate == NvBootSdmmcCardClock_20MHz)
    {
#if TEST_ON_FPGA
        ControllerClockDivisor = 8;
        CardClockDivisor = 1;
#else
        ControllerClockDivisor = 11;
        CardClockDivisor = 1;
#endif
    }

    ControllerClockInMHz = QUOTIENT_CEILING(SDMMC_PLL_FREQ_IN_MHZ,
                                ControllerClockDivisor);
    ContollerCycleTimeInNanoSec = QUOTIENT_CEILING(1000, ControllerClockInMHz);
    CardClockInMHz = QUOTIENT_CEILING(SDMMC_PLL_FREQ_IN_MHZ,
                        (ControllerClockDivisor * CardClockDivisor));
    CardCycleTimeInNanoSec = QUOTIENT_CEILING(1000, CardClockInMHz);
    // Find read time out.
    if (s_SdmmcContext->taac != 0)
    {
        // For Emmc, Read time is 10 times of (TAAC + NSAC).
        // for Esd, Read time is 100 times of (TAAC + NSAC) or 100ms, which ever
        // is lower.
        taac = TaacTimeUnitArray[s_SdmmcContext->taac &
               EMMC_CSD_TAAC_TIME_UNIT_MASK] *
               TaacMultiplierArray[(s_SdmmcContext->taac >>
               EMMC_CSD_TAAC_TIME_VALUE_OFFSET) & \
               EMMC_CSD_TAAC_TIME_VALUE_MASK];
        nsac = CardCycleTimeInNanoSec * s_SdmmcContext->nsac * 1000;
        // taac and nsac are already multiplied by 10.
        s_SdmmcContext->ReadTimeOutInUs = QUOTIENT_CEILING((taac + nsac), 1000);
        PRINT_SDMMC_MESSAGES("\r\nCard ReadTimeOutInUs=%d",
            s_SdmmcContext->ReadTimeOutInUs);
        // Use 200ms time out instead of 100ms. This could be helpful in case
        // old version of cards.
        if (s_SdmmcContext->ReadTimeOutInUs < 200000)
            s_SdmmcContext->ReadTimeOutInUs = 200000;
        else if (s_SdmmcContext->ReadTimeOutInUs > 800000)
        {
            //NV_ASSERT(NV_FALSE);
            // Calculation seem to have gone wrong or TAAc is not valid.
            // Set it to 800msec, which is max timeout.
            s_SdmmcContext->ReadTimeOutInUs = 800000;
        }
    }
    PRINT_SDMMC_MESSAGES("\r\nBase Clock=%dMHz",
        QUOTIENT_CEILING(SDMMC_PLL_FREQ_IN_MHZ, ControllerClockDivisor));
    PRINT_SDMMC_MESSAGES("\r\nHwSdmmcSetCardClock Div=%d", CardClockDivisor);

    NvClocksConfigureClock(4, NVSDMMC_CLOCKS_7_1_DIVIDER_BY(ControllerClockDivisor, 0));

    // If the card clock divisor is 64, the register should be written with 32.
    StcReg = NV_FLD_SET_DRF_NUM(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                SDCLK_FREQUENCYSELECT, (CardClockDivisor >> 1), StcReg);
    NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    // Wait till clock is stable.
    NV_CHECK_ERROR(HwSdmmcWaitForClkStable());
    // Reload reg value after clock is stable.
    NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    // Enable card's clock after clock frequency is changed.
    StcReg = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                SD_CLOCK_EN, ENABLE, StcReg);
    /*
     * Set Data timeout.
     * If the time out bit field is set to 0xd here, which means the time out at
     * base clock 63MHz is 1065 msec. i.e 2^26 / 63MHz = 1065msec.
     * ControllerClockInMHz = SDMMC_PLL_FREQ_IN_MHZ/ ControllerClockDivisor;
     * ControllerCycleTimeInNanoSec = 1000 / ControllerClockInMHz;
     * ClockCyclesRequired = (s_SdmmcContext->ReadTimeOutInUs * 1000) /
     *                       ControllerClockTimeInNanoSec;
     *                     = (s_SdmmcContext->ReadTimeOutInUs * 1000)/
     *                       (1000 / ControllerClockInMHz);
     *                     = (s_SdmmcContext->ReadTimeOutInUs * ControllerClockInMHz);
     *                     = (s_SdmmcContext->ReadTimeOutInUs *
     *                       (SDMMC_PLL_FREQ_IN_MHZ/ ControllerClockDivisor));
     *                     = (s_SdmmcContext->ReadTimeOutInUs * SDMMC_PLL_FREQ_IN_MHZ) /
     *                       ControllerClockDivisor;
     */
    ClockCyclesRequired = QUOTIENT_CEILING( (s_SdmmcContext->ReadTimeOutInUs *
                            SDMMC_PLL_FREQ_IN_MHZ), ControllerClockDivisor );
    // TimeOutCounter value zero means that the time out is (1 << 13).
    while ( ClockCyclesRequired > (1u << (13 + TimeOutCounter)) )
    {
        TimeOutCounter++;
        // This is max value. so break out from here.
        if (TimeOutCounter == 0xE)
            break;
    }
    // Recalculate the ReadTimeOutInUs based value that is set to register.
    // We shouldn't timout in the code before the controller times out.
    s_SdmmcContext->ReadTimeOutInUs = (1 << (13 + TimeOutCounter));
    s_SdmmcContext->ReadTimeOutInUs = QUOTIENT_CEILING(
                                        s_SdmmcContext->ReadTimeOutInUs, 1000);
    s_SdmmcContext->ReadTimeOutInUs = s_SdmmcContext->ReadTimeOutInUs *
                                      ContollerCycleTimeInNanoSec;
    // The code should never time out before controller. Give some extra time for
    // read time out. Add 50msecs.
    s_SdmmcContext->ReadTimeOutInUs += 50000;
    if (s_SdmmcContext->ReadTimeOutInUs < 200000)
        s_SdmmcContext->ReadTimeOutInUs = 200000;
    else if (s_SdmmcContext->ReadTimeOutInUs > 800000)
    {
        //NV_ASSERT(NV_FALSE);
        // Calculation seem to have gone wrong. Set it to 800msec, which
        // is max timeout.
        s_SdmmcContext->ReadTimeOutInUs = 800000;
    }

    StcReg = NV_FLD_SET_DRF_NUM(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                DATA_TIMEOUT_COUNTER_VALUE, TimeOutCounter, StcReg);
    NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    PRINT_SDMMC_MESSAGES("\r\nTimeOutCounter=%d, ClockCyclesRequired=%d, "
        "CardCycleTimeInNanoSec=%d, ContollerCycleTimeInNanoSec=%d",
        TimeOutCounter, ClockCyclesRequired, CardCycleTimeInNanoSec,
        ContollerCycleTimeInNanoSec);
    PRINT_SDMMC_MESSAGES("\r\nRecalc ReadTimeOutInUs=%d, clk cycles time in ns=%d",
        s_SdmmcContext->ReadTimeOutInUs, ((1 << (13 + TimeOutCounter)) *
        ContollerCycleTimeInNanoSec));
    return NvError_Success;
}

void HwSdmmcSetDataWidth(NvBootSdmmcDataWidth DataWidth)
{
    NvU32 PowerControlHostReg = 0;

    NV_SDMMC_READ(POWER_CONTROL_HOST, PowerControlHostReg);
    PowerControlHostReg = NV_FLD_SET_DRF_NUM(SDMMC, POWER_CONTROL_HOST,
                            DATA_XFER_WIDTH, DataWidth, PowerControlHostReg);
    // When 8-bit data width is enabled, the bit field DATA_XFER_WIDTH
    // value is not valid.
    PowerControlHostReg = NV_FLD_SET_DRF_NUM(SDMMC, POWER_CONTROL_HOST,
                            EXTENDED_DATA_TRANSFER_WIDTH,
                            ((DataWidth == NvBootSdmmcDataWidth_8Bit) ||
                            (DataWidth == NvBootSdmmcDataWidth_Ddr_8Bit)? 1 : 0),
                            PowerControlHostReg);
    NV_SDMMC_WRITE(POWER_CONTROL_HOST, PowerControlHostReg);
}

NvError HwSdmmcInitController(void)
{
    NvU32 StcReg;
    NvError e;
    NvU32 CapabilityReg;
    NvU32 IntStatusEnableReg;
    NvU32 PowerControlHostReg;

    NvBootResetSetEnable(4, NV_TRUE);

    // Configure the clock source with divider 9, which gives 24MHz.
    PRINT_SDMMC_MESSAGES("\r\nBase Clock=%dMHz",
        QUOTIENT_CEILING(SDMMC_PLL_FREQ_IN_MHZ, 9));

    NvClocksConfigureClock(4, NVSDMMC_CLOCKS_7_1_DIVIDER_BY(9, 0));


    // Enable the clock.
    NvClocksSetEnable(4, NV_TRUE);

    // Remove the controller from Reset.
    NvBootResetSetEnable(4, NV_FALSE);

    // Reset Controller's All registers.
    NV_CHECK_ERROR(HwSdmmcResetController());

    // Set Internal Clock Enable and SDCLK Frequency Select in the
    // Clock Control register.
    StcReg = NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                INTERNAL_CLOCK_EN, OSCILLATE) |
             NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                SDCLK_FREQUENCYSELECT, DIV64);
    NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    // Wait till clock is stable.
    NV_CHECK_ERROR(HwSdmmcWaitForClkStable());
    // Reload reg value after clock is stable.
    NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);

    // Find out what volatage is supported.
    NV_SDMMC_READ(CAPABILITIES, CapabilityReg);
    PowerControlHostReg = 0;

    // voltage setting logic changed 'TO' Required voltage "FROM" supported voltage !!!

        if (NV_DRF_VAL(SDMMC, CAPABILITIES,\
            VOLTAGE_SUPPORT_3_3_V, CapabilityReg))
        {
            PowerControlHostReg |= NV_DRF_DEF(SDMMC, POWER_CONTROL_HOST,
                                    SD_BUS_VOLTAGE_SELECT, V3_3);
        }
        else if (NV_DRF_VAL(SDMMC, CAPABILITIES,\
            VOLTAGE_SUPPORT_3_0_V, CapabilityReg))
        {
            PowerControlHostReg |= NV_DRF_DEF(SDMMC, POWER_CONTROL_HOST,
                                    SD_BUS_VOLTAGE_SELECT, V3_0);
        }
        else
        {
            PowerControlHostReg |= NV_DRF_DEF(SDMMC, POWER_CONTROL_HOST,
                                    SD_BUS_VOLTAGE_SELECT, V1_8);
        }
    // Enable bus power.
    PowerControlHostReg |= NV_DRF_DEF(SDMMC, POWER_CONTROL_HOST,
                            SD_BUS_POWER, POWER_ON);
    NV_SDMMC_WRITE(POWER_CONTROL_HOST, PowerControlHostReg);

    s_SdmmcContext->HostSupportsHighSpeedMode = NV_FALSE;
    if (NV_DRF_VAL(SDMMC, CAPABILITIES, HIGH_SPEED_SUPPORT, CapabilityReg))
        s_SdmmcContext->HostSupportsHighSpeedMode = NV_TRUE;
    PRINT_SDMMC_MESSAGES("\r\nHostSupportsHighSpeedMode=%d",
        s_SdmmcContext->HostSupportsHighSpeedMode);
    // Enable Command complete, Transfer complete and various error events.
    IntStatusEnableReg =
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, DATA_END_BIT_ERR, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, DATA_CRC_ERR, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, DATA_TIMEOUT_ERR, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_INDEX_ERR, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_END_BIT_ERR, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_CRC_ERR, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_TIMEOUT_ERR, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, CARD_REMOVAL, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, CARD_INSERTION, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, DMA_INTERRUPT, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, TRANSFER_COMPLETE, ENABLE) |
    NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_COMPLETE, ENABLE);
    NV_SDMMC_WRITE(INTERRUPT_STATUS_ENABLE, IntStatusEnableReg);
    // This method resets card clock divisor. So, set it again.
    HwSdmmcSetCardClock(s_SdmmcContext->CurrentClockRate);
    return NvError_Success;
}

NvError
HwSdmmcSendCommand(
    SdmmcCommand CommandIndex,
    NvU32 CommandArg,
    SdmmcResponseType ResponseType,
    NvBool IsDataCmd)
{
    NvError e;
    NvU32 retries = 3;
    NvU32 CommandXferMode;
    NvU32 InterruptStatus;
    NvU32* pSdmmcResponse = &s_SdmmcContext->SdmmcResponse[0];
    NV_ASSERT(ResponseType < SdmmcResponseType_Num);
    PRINT_SDMMC_MESSAGES("\r\nCmd Index=0x%x, Arg=0x%x, RespType=%d, data=%d",
        CommandIndex, CommandArg, ResponseType, IsDataCmd);
    // Wait till Controller is ready.
    NV_CHECK_ERROR(HwSdmmcWaitForCmdInhibitCmd());

    CommandXferMode =
        NV_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_INDEX, CommandIndex) |
        NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_PRESENT_SELECT, (IsDataCmd ? 1 : 0)) |
        NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_XFER_DIR_SEL, (IsDataCmd ? 1 : 0)) |
        NV_DRF_NUM(SDMMC, CMD_XFER_MODE, BLOCK_COUNT_EN, (IsDataCmd ? 1 : 0)) |
        NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DMA_EN, (IsDataCmd ? 1 : 0));

    if (CommandIndex == SdmmcCommand_ReadMulti)
        CommandXferMode |=
            NV_DRF_NUM(SDMMC, CMD_XFER_MODE,MULTI_BLOCK_SELECT , 1);

    //cmd index check
    if((ResponseType != SdmmcResponseType_NoResponse) &&
        (ResponseType != SdmmcResponseType_R2) &&
        (ResponseType != SdmmcResponseType_R3) &&
        (ResponseType != SdmmcResponseType_R4) )
        {
          CommandXferMode |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE,
                                                CMD_INDEX_CHECK_EN, ENABLE);

        }

    //crc index check
    if((ResponseType != SdmmcResponseType_NoResponse) &&
        (ResponseType != SdmmcResponseType_R3) &&
        (ResponseType != SdmmcResponseType_R4) )
        {
              CommandXferMode |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE,
                                                CMD_CRC_CHECK_EN, ENABLE);
        }

    //response type check
    if(ResponseType == SdmmcResponseType_NoResponse)
        {
              CommandXferMode |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE,
                                                RESP_TYPE_SELECT,
                                                NO_RESPONSE);
        }
    else if(ResponseType == SdmmcResponseType_R2 )
        {
                  CommandXferMode |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE,
                                                RESP_TYPE_SELECT,
                                                RESP_LENGTH_136);
        }
    else if(ResponseType == SdmmcResponseType_R1B)
        {
          CommandXferMode |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE,
                                                RESP_TYPE_SELECT,
                                                RESP_LENGTH_48BUSY);
        }
    else
        {
          CommandXferMode |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE,
                                                RESP_TYPE_SELECT,
                                                RESP_LENGTH_48);
        }
    while (retries)
    {
        // Clear Status bits what ever is set.
        NV_SDMMC_READ(INTERRUPT_STATUS, InterruptStatus);
        NV_SDMMC_WRITE(INTERRUPT_STATUS, InterruptStatus);
        // This redundant read is for debug purpose.
        NV_SDMMC_READ(INTERRUPT_STATUS, InterruptStatus);
        NV_SDMMC_WRITE(ARGUMENT, CommandArg);
        NV_SDMMC_WRITE(CMD_XFER_MODE, CommandXferMode);
        // Wait for the command to be sent out. If it fails, retry.
        e = HwSdmmcWaitForCommandComplete();
        if (e == NvError_Success)
            break;
        // Recover Controller from Errors.
        HwSdmmcRecoverControllerFromErrors(IsDataCmd);
        retries--;
    }
    // FIXME -need to see why it is required, otherwise
    // reading incorrect data
    NvBlUtilWaitUS(400);
    if (retries)
    {
        // Wait till response is received from card.
        NV_CHECK_ERROR(HwSdmmcWaitForCmdInhibitCmd());
        if (ResponseType == SdmmcResponseType_R1B)
            // Wait till busy line is deasserted by card.
            NV_CHECK_ERROR(HwSdmmcWaitForCmdInhibitData());
        HwSdmmcReadResponse(ResponseType, pSdmmcResponse);
    }
    return e;
}


NvError EmmcGetExtCsd(void)
{
    NvError e;
    NvBootDeviceStatus DevStatus;
    NvU8* pBuffer = (NvU8*)&s_SdmmcContext->SdmmcInternalBuffer[0];

    // Set num of blocks to read to 1.
    HwSdmmcSetNumOfBlocks((1 << s_SdmmcContext->PageSizeLog2), 1);
    // Setup Dma.
    HwSdmmcSetupDma((NvU8*)s_SdmmcContext->SdmmcInternalBuffer,
        (1 << s_SdmmcContext->PageSizeLog2));
    // Send SEND_EXT_CSD(CMD8) command to get boot partition size.
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_EmmcSendExtendedCsd,
        0, SdmmcResponseType_R1, NV_TRUE));
    // If response fails, return error. Nothing to clean up.
    NV_CHECK_ERROR(EmmcVerifyResponse(SdmmcCommand_EmmcSendExtendedCsd,
        NV_FALSE));
    s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_ReadInProgress;
    s_SdmmcContext->ReadStartTime = NvBlUtilGetTimeUS();
    do
    {
        DevStatus = NvNvBlSdmmcQueryStatus();
    } while ( (DevStatus != NvBootDeviceStatus_Idle) &&
              (DevStatus == NvBootDeviceStatus_ReadInProgress) );
    if (DevStatus != NvBootDeviceStatus_Idle)
        return NvError_SdioCardNotPresent;
    s_SdmmcContext->EmmcBootPartitionSize =
        // The partition size comes in 128KB units.
        // Left shift it by 17 to get it multiplied by 128KB.
        (pBuffer[EMMC_ECSD_BOOT_PARTITION_SIZE_OFFSET] << 17);
    s_SdmmcContext->PowerClass26MHz360V = pBuffer[EMMC_ECSD_POWER_CL_26_360_OFFSET];
    s_SdmmcContext->PowerClass52MHz360V = pBuffer[EMMC_ECSD_POWER_CL_52_360_OFFSET];
    s_SdmmcContext->PowerClass26MHz195V = pBuffer[EMMC_ECSD_POWER_CL_26_195_OFFSET];
    s_SdmmcContext->PowerClass52MHz195V = pBuffer[EMMC_ECSD_POWER_CL_52_195_OFFSET];
    s_SdmmcContext->PowerClass52MHzDdr360V = pBuffer[EMMC_ECSD_POWER_CL_DDR_52_360_OFFSET];
    s_SdmmcContext->PowerClass52MHzDdr195V = pBuffer[EMMC_ECSD_POWER_CL_DDR_52_195_OFFSET];
    s_SdmmcContext->BootConfig = pBuffer[EMMC_ECSD_BOOT_CONFIG_OFFSET];
    if (s_SdmmcContext->IsHighCapacityCard)
    {
        s_SdmmcContext->NumOfBlocks = (pBuffer[EMMC_ECSD_SECTOR_COUNT_0_OFFSET] |
                                      (pBuffer[EMMC_ECSD_SECTOR_COUNT_1_OFFSET] << 8) |
                                      (pBuffer[EMMC_ECSD_SECTOR_COUNT_2_OFFSET] << 16) |
                                      (pBuffer[EMMC_ECSD_SECTOR_COUNT_3_OFFSET] << 24));
        PRINT_SDMMC_MESSAGES("\r\nEcsd NumOfBlocks=%d", s_SdmmcContext->NumOfBlocks);
    }

    PRINT_SDMMC_MESSAGES("\r\nBootPartition Size=%d",
        s_SdmmcContext->EmmcBootPartitionSize);
    PRINT_SDMMC_MESSAGES("\r\nPowerClass26MHz360V=%d, PowerClass52MHz360V=%d, "
        "PowerClass26MHz195V=%d, PowerClass52MHz195V=%d",
        s_SdmmcContext->PowerClass26MHz360V, s_SdmmcContext->PowerClass52MHz360V,
        s_SdmmcContext->PowerClass26MHz195V, s_SdmmcContext->PowerClass52MHz195V);
    PRINT_SDMMC_MESSAGES("\r\nCurrentPowerClass=%d, CardType=%d",
        pBuffer[EMMC_ECSD_POWER_CLASS_OFFSET], pBuffer[EMMC_ECSD_CARD_TYPE_OFFSET]);

    s_SdmmcContext->CardSupportSpeed = pBuffer[EMMC_ECSD_CARD_TYPE_OFFSET];
    s_SdmmcContext->CardBusWidth = pBuffer[EMMC_ECSD_BUS_WIDTH];
    return e;
}

static NvU32 EmmcGetPowerClass(void)
{
    NvU32 PowerClass;

    // set Ddr mode from getextcsd data & width supported
    if(s_SdmmcContext->IsDdrMode)
        PowerClass = s_SdmmcContext->IsHighVoltageRange ?
                     s_SdmmcContext->PowerClass52MHzDdr360V :
                     s_SdmmcContext->PowerClass52MHzDdr195V;
    else if (s_SdmmcContext->IsHighVoltageRange)
        PowerClass = s_SdmmcContext->HighSpeedMode ?
                     s_SdmmcContext->PowerClass52MHz360V :
                     s_SdmmcContext->PowerClass26MHz360V;
    else
        PowerClass = s_SdmmcContext->HighSpeedMode ?
                     s_SdmmcContext->PowerClass52MHz195V :
                     s_SdmmcContext->PowerClass26MHz195V;
    /*
     * In the above power class, lower 4 bits give power class requirement for
     * for 4-bit data width and upper 4 bits give power class requirement for
     * for 8-bit data width.
     */
    if ((s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_4Bit) ||
        (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_Ddr_4Bit))
        PowerClass = (PowerClass >> EMMC_ECSD_POWER_CLASS_4_BIT_OFFSET) &
                     EMMC_ECSD_POWER_CLASS_MASK;
    else if ((s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_8Bit) ||
        (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_Ddr_8Bit))
        PowerClass = (PowerClass >> EMMC_ECSD_POWER_CLASS_8_BIT_OFFSET) &
                     EMMC_ECSD_POWER_CLASS_MASK;
    else //if (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_1Bit)
        PowerClass = 0;
    return PowerClass;
}

static NvError EmmcSetPowerClass(void)
{
    NvU32 CmdArg;
    NvU32 PowerClassToSet;
    NvError e = NvError_Success;

    PowerClassToSet = EmmcGetPowerClass();
    // Select best possible configuration here.
    while (PowerClassToSet > s_SdmmcContext->MaxPowerClassSupported)
    {
        if (s_SdmmcContext->HighSpeedMode)
        {
            // Disable high speed and see, if it can be supported.
            s_SdmmcContext->CardSupportsHighSpeedMode = NV_FALSE;
            // Find out clock divider for card clock again for normal speed.
            HwSdmmcCalculateCardClockDivisor();
        }
        else if ((s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_8Bit) ||
        (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_Ddr_8Bit))
            s_SdmmcContext->DataWidth = NvBootSdmmcDataWidth_4Bit;
        else if ((s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_4Bit) ||
        (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_Ddr_4Bit))
            s_SdmmcContext->DataWidth = NvBootSdmmcDataWidth_1Bit;
        PowerClassToSet = EmmcGetPowerClass();
    }
    if (PowerClassToSet)
    {
        PRINT_SDMMC_MESSAGES("\r\nSet Power Class to %d", PowerClassToSet);
        CmdArg = EMMC_SWITCH_SELECT_POWER_CLASS_ARG |
                 (PowerClassToSet << EMMC_SWITCH_SELECT_POWER_CLASS_OFFSET);
        NV_CHECK_ERROR(EmmcSendSwitchCommand(CmdArg));
    }
    return e;
}

static NvError EmmcSetBusWidth(void)
{
    NvU32 CmdArg;
    NvError e = NvError_Success;

    // Send SWITCH(CMD6) Command to select bus width.
    PRINT_SDMMC_MESSAGES("\r\n\r\nChange Data width to %d(0->1bit, 1->4bit,"
        " 2->8-bit, 5->4-bit DDR, 6->8-bit DDR)", s_SdmmcContext->DataWidth);
    CmdArg = EMMC_SWITCH_BUS_WIDTH_ARG |
             (s_SdmmcContext->DataWidth << EMMC_SWITCH_BUS_WIDTH_OFFSET);
    NV_CHECK_ERROR(EmmcSendSwitchCommand(CmdArg));
    HwSdmmcSetDataWidth(s_SdmmcContext->DataWidth);
    return e;
}

static NvError EmmcEnableDDRSupport(void)
{
    NvU32 HostControl2Reg=0;
    NvU32 StcReg;
    NvU32 CapabilityReg;
    NvU32 CapabilityHighReg;
    NvError e = NvError_Success;

    if((s_SdmmcContext->CardSupportSpeed & EMMC_ECSD_CT_HS_DDR_52_180_300_MASK )
            == EMMC_ECSD_CT_HS_DDR_52_180_300)
    {
        // card operates @ eiher 1.8v or 3.0v I/O
        // Power class selection
        // eMMC ddr works @ 1.8v and 3v
        // change the bus speed mode.
        // switch command to select card type -- ddr hs 1.8/3v

        // set HS_TIMING to 1 before setting the ddr mode data width..
        #if TEST_ON_FPGA
            if(!s_SdmmcContext->HighSpeedMode)
            {
                s_SdmmcContext->HighSpeedMode = NV_TRUE;
                EmmcEnableHighSpeed();
                s_SdmmcContext->HighSpeedMode = NV_FALSE;
            }
        #endif

        NV_CHECK_ERROR(EmmcSetBusWidth());
        //set the ddr mode in Host controller and other misc things..
        // DDR support is available by fuse bit
        // check capabilities register for Ddr support
        // read capabilities and capabilities higher reg
        NV_SDMMC_READ(CAPABILITIES, CapabilityReg);
        NV_SDMMC_READ(CAPABILITIES_HIGHER, CapabilityHighReg);

        if (NV_DRF_VAL(SDMMC, CAPABILITIES_HIGHER, DDR50, CapabilityHighReg) &&
            NV_DRF_VAL(SDMMC, CAPABILITIES, VOLTAGE_SUPPORT_1_8_V, CapabilityReg) &&
            NV_DRF_VAL(SDMMC, CAPABILITIES, HIGH_SPEED_SUPPORT, CapabilityReg))
        {
            // reset SD clock enable
            NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
            StcReg = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                        SD_CLOCK_EN, DISABLE, StcReg);
            NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);

            // set DDR50 UHS Mode
            NV_SDMMC_READ(AUTO_CMD12_ERR_STATUS, HostControl2Reg);
            HostControl2Reg |= NV_DRF_DEF(SDMMC, AUTO_CMD12_ERR_STATUS,
                                    UHS_MODE_SEL, DDR50);
            NV_SDMMC_WRITE(AUTO_CMD12_ERR_STATUS, HostControl2Reg);

            // set enable SD clock
            StcReg = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                        SD_CLOCK_EN, ENABLE, StcReg);
            NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
        }

    }
    else if((s_SdmmcContext->CardSupportSpeed & EMMC_ECSD_CT_HS_DDR_52_120_MASK)
            == EMMC_ECSD_CT_HS_DDR_52_120)
    {
        // NOT supported
        e = NvError_NotImplemented;
    }
    else
    {
        // should not be here!!
        // ASSERT
        e = NvError_BadParameter;
    }
    return e;
}


static NvError EmmcIdentifyCard(void)
{
    NvError e = NvError_Success;
    SdmmcAccessRegion NvBootPartitionEn = SdmmcAccessRegion_UserArea;
    NvU8 CardSpeed = 0;

    // Set Clock rate to 375KHz for identification of card.
    HwSdmmcSetCardClock(NvBootSdmmcCardClock_Identification);

    // Send GO_IDLE_STATE(CMD0) Command.
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_GoIdleState,
        0, SdmmcResponseType_NoResponse, 0));

    // This sends SEND_OP_COND(CMD1) Command and finds out address mode and
    // capacity status.
    NV_CHECK_ERROR(EmmcGetOpConditions());

    // Send ALL_SEND_CID(CMD2) Command.
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_AllSendCid,
        0, SdmmcResponseType_R2, NV_FALSE));


    // Set RCA to Card Here. It should be greater than 1 as JEDSD spec.
    s_SdmmcContext->CardRca = (2 << SDMMC_RCA_OFFSET);

    // Send SET_RELATIVE_ADDR(CMD3) Command.
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_EmmcSetRelativeAddress,
        s_SdmmcContext->CardRca, SdmmcResponseType_R1, NV_FALSE));

    // Get Card specific data. We can get it at this stage of identification.
    NV_CHECK_ERROR(SdmmcGetCsd());

    // Send SELECT/DESELECT_CARD(CMD7) Command to place the card in tran state.
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_SelectDeselectCard,
        s_SdmmcContext->CardRca, SdmmcResponseType_R1, NV_FALSE));

    // Card should be in transfer state now. Confirm it.
    if (SdmmcIsCardInTransferState() == NV_FALSE)
        return NvError_SdioCardNotPresent;

    // Find out clock divider for card clock and high speed mode requirement.
    HwSdmmcCalculateCardClockDivisor();

    if (s_SdmmcContext->SpecVersion >= 4) // v4.xx
    {
        // Bus width can only be changed to 4-bit/8-bit after required power class
        // is set. To get Power class, we need to read Ext CSD. As we don't know
        // the power class required for 4-bit/8-bit, we need to read Ext CSD
        // with 1-bit data width.
        PRINT_SDMMC_MESSAGES("\r\n\r\nSet Data width to 1-bit for ECSD");
        HwSdmmcSetDataWidth(NvBootSdmmcDataWidth_1Bit);
        // Set data clock rate to 20MHz.
        HwSdmmcSetCardClock(NvBootSdmmcCardClock_20MHz);
        // It is valid for v4.xx and above cards only.
        // EmmcGetExtCsd() Finds out boot partition size also.
        NV_CHECK_ERROR(EmmcGetExtCsd());

        // Select the power class now.
        NV_CHECK_ERROR(EmmcSetPowerClass());

        // Enable the High Speed Mode, if required.
        EmmcEnableHighSpeed();
    }

    // Set the clock for data transfer.
    HwSdmmcSetCardClock(NvBootSdmmcCardClock_DataTransfer);

    // v4.4 and ddr supported enable ddr
    if ((s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_Ddr_4Bit) ||
        (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_Ddr_8Bit))
    {
        // v4.4 and ddr supported enable ddr
        CardSpeed = (s_SdmmcContext->CardSupportSpeed &
                                EMMC_ECSD_CT_HS_DDR_MASK);
        if((CardSpeed & EMMC_ECSD_CT_HS_DDR_52_120) ||
            (CardSpeed & EMMC_ECSD_CT_HS_DDR_52_180_300))
        {
            NV_CHECK_ERROR(EmmcEnableDDRSupport());
        }
        else
        {
            // set the bus width since the card does not support the requested mode data width..
            if (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_Ddr_4Bit)
                s_SdmmcContext->DataWidth = NvBootSdmmcDataWidth_4Bit;
            else if(s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_Ddr_8Bit)
                s_SdmmcContext->DataWidth = NvBootSdmmcDataWidth_8Bit;

            NV_CHECK_ERROR(EmmcSetBusWidth());
        }
    }
    else
    {
        NV_CHECK_ERROR(EmmcSetBusWidth());
    }

    // Select boot partition1 here.
    // also the boot partition should be checked (enabled) for access the partition.
    NvBootPartitionEn = (SdmmcAccessRegion)((s_SdmmcContext->BootConfig >>
                                        EMMC_ECSD_BC_BPE_OFFSET)
                                    &&  EMMC_ECSD_BC_BPE_MASK);

    if (s_SdmmcContext->EmmcBootPartitionSize != 0 &&
        ((NvBootPartitionEn == EMMC_ECSD_BC_BPE_BAP1) ||
        (NvBootPartitionEn == EMMC_ECSD_BC_BPE_BAP2)))
        NV_CHECK_ERROR(EmmcSelectAccessRegion(NvBootPartitionEn));

   return e;
}


NvError
NvNvBlSdmmcInit_Mini(void)
{
    NvError e = NvError_Success;

    PRINT_SDMMC_MESSAGES("NvNvBlSdmmcInit_Mini: entry\n");
    s_SdmmcContext = &s_DeviceContext;
    s_SdmmcContext->BootConfig = 0;
    s_SdmmcContext->CardRca = 0x20000;
    s_SdmmcContext->BlockSizeLog2 =12;
    s_SdmmcContext->CurrentAccessRegion =4;
    s_SdmmcContext->PageSizeLog2 = 9;
    s_SdmmcContext->PagesPerBlockLog2 = 3;
    s_SdmmcContext->IsHighCapacityCard =1;
    s_SdmmcContext->SdControllerId = 0;
    s_SdmmcContext->SdControllerBaseAddress = NV_ADDRESS_MAP_SDMMC4_BASE;
    NV_CHECK_ERROR(EmmcGetExtCsd());
    return e;
}


NvError
NvNvBlSdmmcInit(void)
{
    NvError e = NvError_Success;
    NvU32 CapabilityReg;
    NvU32 PageSize = 0;

    PRINT_SDMMC_MESSAGES("NvNvBlSdmmcInit: entry\n");
    s_SdmmcContext = &s_DeviceContext;
    s_SdmmcContext->taac = 0;
    s_SdmmcContext->nsac = 0;
    // BCT clock divisor values are based on 432MHZ, but microboot clock is
    // running at 216MHZ. To adjust this make clock divisor by half( 432/216).
    s_SdmmcContext->ClockDivisor = ((11 + 1) >> 1);
    s_SdmmcContext->DataWidth = 2;
    s_SdmmcContext->MaxPowerClassSupported =0;
    s_SdmmcContext->CardSupportsHighSpeedMode = NV_FALSE;
    s_SdmmcContext->ReadTimeOutInUs = SDMMC_READ_TIMEOUT_IN_US;
    s_SdmmcContext->EmmcBootPartitionSize = 0;
    s_SdmmcContext->CurrentClockRate = 0;
    s_SdmmcContext->CurrentAccessRegion = SdmmcAccessRegion_Unknown;
    s_SdmmcContext->BootModeReadInProgress = NV_FALSE;
    s_SdmmcContext->SdControllerId = 0;
    s_SdmmcContext->SdControllerBaseAddress = NV_ADDRESS_MAP_SDMMC4_BASE;

    // Initialize the Hsmmc Hw controller.
    NV_SDMMC_READ(CAPABILITIES, CapabilityReg);
    if (NV_DRF_VAL(SDMMC, CAPABILITIES, HIGH_SPEED_SUPPORT, CapabilityReg))
        s_SdmmcContext->HostSupportsHighSpeedMode = NV_TRUE;
    else
        s_SdmmcContext->HostSupportsHighSpeedMode = NV_FALSE;

    // Check whether card is present. If not, return from here itself.
    if (HwSdmmcIsCardPresent() == NV_FALSE)
    {
        NvNvBlSdmmcShutdown();
        return NvError_SdioCardNotPresent;
    }

    e = EmmcIdentifyCard();

    s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_Idle;

    // enable block length setting, if CYA is cleared.
    if((s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_4Bit) ||
            (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_8Bit))
    {
        PageSize = (1 << s_SdmmcContext->PageSizeLog2);
        // Send SET_BLOCKLEN(CMD16) Command.
        NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_SetBlockLength,
            PageSize, SdmmcResponseType_R1, NV_FALSE));

        NV_CHECK_ERROR(EmmcVerifyResponse(SdmmcCommand_SetBlockLength,
            NV_FALSE));
    }

    return e;
}

NvError
NvNvBlSdmmcReadPage(
    const NvU32 Block,
    const NvU32 Page,
    NvU8 *pBuffer)
{
    NvError e;
    NvU32 CommandArg;
    NvU32 ActualBlockToRead;
    NvU32 Page2Access = Page;
    NvU32 Block2Access = Block;
    NvU32 PageSize = (1 << s_SdmmcContext->PageSizeLog2);

    NV_ASSERT(Page < (1u << s_SdmmcContext->PagesPerBlockLog2));
    NV_ASSERT(pBuffer != NULL);
    PRINT_SDMMC_MESSAGES("\r\nRead Block=%d, Page=%d", Block, Page);

    // If data line ready times out, try to recover from errors.
    if (HwSdmmcWaitForDataLineReady() != NvError_Success)
        NV_CHECK_ERROR(HwSdmmcRecoverControllerFromErrors(NV_TRUE));
    // Select access region.This will intern changes block and page addresses
    // based on the region the request falls in.
    NV_CHECK_ERROR(SdmmcSelectAccessRegion(&Block2Access, &Page2Access));
    PRINT_SDMMC_MESSAGES("\r\nRegion=%d(1->BP1, 2->BP2, 0->UP)Block2Access=%d, "
        "Page2Access=%d", s_SdmmcContext->CurrentAccessRegion, Block2Access,
        Page2Access);

    // enable block length setting, if CYA set.
    if((s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_4Bit) ||
            (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_8Bit))
    {
        // Send SET_BLOCKLEN(CMD16) Command.
        NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_SetBlockLength,
            PageSize, SdmmcResponseType_R1, NV_FALSE));
        NV_CHECK_ERROR(EmmcVerifyResponse(SdmmcCommand_SetBlockLength,
            NV_FALSE));
    }

    // Find out the Block to read from MoviNand.
    ActualBlockToRead = (Block2Access << s_SdmmcContext->PagesPerBlockLog2) +
                        Page2Access;
    /*
     * If block to read is beyond card's capacity, then some Emmc cards are
     * responding with error back and continue to work. Some are not responding
     * for this and for subsequent valid operations also.
     */
    //if (ActualBlockToRead >= s_SdmmcContext->NumOfBlocks)
    //    return NvError_IllegalParameter;
    // Set number of blocks to read to 1.
    HwSdmmcSetNumOfBlocks(PageSize, 1);
    // Set up command arg.
    if (s_SdmmcContext->IsHighCapacityCard)
        CommandArg = ActualBlockToRead;
    else
        CommandArg = (ActualBlockToRead << s_SdmmcContext->PageSizeLog2);
    PRINT_SDMMC_MESSAGES("\r\nActualBlockToRead=%d, CommandArg=%d",
        ActualBlockToRead, CommandArg);
    // Store address of pBuffer in sdmmc context
    s_SdmmcContext->CurrentReadBufferAddress = pBuffer;
    // Setup Dma.
    HwSdmmcSetupDma(pBuffer, PageSize);
    // Send command to card.
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_ReadSingle,
                            CommandArg, SdmmcResponseType_R1, NV_TRUE));
    // If response fails, return error. Nothing to clean up.
    NV_CHECK_ERROR_CLEANUP(EmmcVerifyResponse(SdmmcCommand_ReadSingle,
        NV_FALSE));
    s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_ReadInProgress;
    s_SdmmcContext->ReadStartTime = NvBlUtilGetTimeUS();
    //flush the data since the data is read into uncached memory
//    NvOsDataCacheWriteback();
    return e;
fail:
    HwSdmmcAbortDataRead();
    return e;
}

NvError
NvNvBlSdmmcReadMultiPage(
    const NvU32 Block,
    const NvU32 Page,
    const NvU32 PagesToRead,
    NvU8 *pBuffer)
{
    NvError e;
    NvU32 CommandArg;
    NvU32 ActualBlockToRead;
    NvU32 Page2Access = Page;
    NvU32 Block2Access = Block;
    NvU32 PageSize = (1 << s_SdmmcContext->PageSizeLog2);

    NV_ASSERT(Page < (1u << s_SdmmcContext->PagesPerBlockLog2));
    NV_ASSERT(pBuffer != NULL);
    PRINT_SDMMC_MESSAGES("\r\nRead Block=%d, Page=%d", Block, Page);

    // If data line ready times out, try to recover from errors.
    if (HwSdmmcWaitForDataLineReady() != NvError_Success)
        NV_CHECK_ERROR(HwSdmmcRecoverControllerFromErrors(NV_TRUE));
    // Select access region.This will intern changes block and page addresses
    // based on the region the request falls in.
    NV_CHECK_ERROR(SdmmcSelectAccessRegion(&Block2Access, &Page2Access));
    PRINT_SDMMC_MESSAGES("\r\nRegion=%d(1->BP1, 2->BP2, 0->UP)Block2Access=%d, "
        "Page2Access=%d", s_SdmmcContext->CurrentAccessRegion, Block2Access,
        Page2Access);

    // enable block length setting, if CYA set.
    if ((s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_4Bit) ||
            (s_SdmmcContext->DataWidth == NvBootSdmmcDataWidth_8Bit))
    {
        // Send SET_BLOCKLEN(CMD16) Command.
        NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_SetBlockLength,
            PageSize, SdmmcResponseType_R1, NV_FALSE));
        NV_CHECK_ERROR(EmmcVerifyResponse(SdmmcCommand_SetBlockLength,
            NV_FALSE));
    }

    // Find out the Block to read from MoviNand.
    ActualBlockToRead = (Block2Access << s_SdmmcContext->PagesPerBlockLog2) +
                        Page2Access;
    /*
     * If block to read is beyond card's capacity, then some Emmc cards are
     * responding with error back and continue to work. Some are not responding
     * for this and for subsequent valid operations also, Hence the condition
     * actual block to read is with in cards capacity is not checked
     */

    // Set number of blocks to read to 1.
    HwSdmmcSetNumOfBlocks(PageSize, PagesToRead);
    PRINT_SDMMC_MESSAGES("\r\nActualBlockToRead=%d, CommandArg=%d",
        ActualBlockToRead, CommandArg);
    // Set up command arg.
    if (s_SdmmcContext->IsHighCapacityCard)
        CommandArg = ActualBlockToRead;
    else
        CommandArg = (ActualBlockToRead << s_SdmmcContext->PageSizeLog2);
    PRINT_SDMMC_MESSAGES("\r\nActualBlockToRead=%d, CommandArg=%d",
        ActualBlockToRead, CommandArg);
    // Store address of pBuffer in sdmmc context
    s_SdmmcContext->CurrentReadBufferAddress = pBuffer;
    // Setup Dma.
    HwSdmmcSetupDma(pBuffer, PageSize * PagesToRead);

    // settingthe block count command
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_SetBlockCount,
        PagesToRead, SdmmcResponseType_R1, NV_FALSE));
    // Send command to card.
    NV_CHECK_ERROR(HwSdmmcSendCommand(SdmmcCommand_ReadMulti,
                            CommandArg, SdmmcResponseType_R1, NV_TRUE));

    // If response fails, return error. Nothing to clean up.
    NV_CHECK_ERROR_CLEANUP(EmmcVerifyResponse(SdmmcCommand_ReadMulti,
        NV_FALSE));

    s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_ReadInProgress;
    s_SdmmcContext->ReadStartTime = NvBlUtilGetTimeUS();
    // FIXME - is data cache writeback required?
    return e;
fail:
    HwSdmmcAbortDataRead();
    return e;
}

NvBootDeviceStatus NvNvBlSdmmcQueryStatus(void)
{
    NvError e;
    NvU32 SdmaAddress;
    NvU32 TransferDone = 0;
    NvU32 InterruptStatusReg;
    NvU32 ErrorMask =
              NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_END_BIT_ERR, ERR) |
              NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_CRC_ERR, ERR) |
              NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_TIMEOUT_ERR,
                TIMEOUT) |
              NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_INDEX_ERR,
                ERR) |
              NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
                END_BIT_ERR_GENERATED) |
              NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_CRC_ERR,
                CRC_ERR_GENERATED) |
              NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR,
                TIMEOUT);
    NvU32 DmaBoundaryInterrupt = NV_DRF_DEF(SDMMC, INTERRUPT_STATUS,
                                    DMA_INTERRUPT,GEN_INT);
    NvU32 DataTimeOutError =
                NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_TIMEOUT_ERR, TIMEOUT);

    if (s_SdmmcContext->DeviceStatus == NvBootDeviceStatus_ReadInProgress)
    {
        // Check whether Transfer is done.
        NV_SDMMC_READ(INTERRUPT_STATUS, InterruptStatusReg);
        TransferDone = NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE,
                            InterruptStatusReg);
        // Check whether there are any errors.
        if (InterruptStatusReg & ErrorMask)
        {
            if ( (InterruptStatusReg & ErrorMask) == DataTimeOutError)
                s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_DataTimeout;
            else
            {
                s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_CrcFailure;
            }
            // Recover from errors here.
            (void)HwSdmmcRecoverControllerFromErrors(NV_TRUE);
        }
        else if (InterruptStatusReg & DmaBoundaryInterrupt)
        {
            // Need to clear this DMA boundary interrupt and write SDMA address
            // again. Otherwise controller doesn't go ahead.
            NV_SDMMC_WRITE(INTERRUPT_STATUS, DmaBoundaryInterrupt);
            NV_SDMMC_READ(SYSTEM_ADDRESS, SdmaAddress);
            NV_SDMMC_WRITE(SYSTEM_ADDRESS, SdmaAddress);
        }
        else if (TransferDone)
        {
            s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_Idle;
            NV_SDMMC_WRITE(INTERRUPT_STATUS, InterruptStatusReg);
            if (s_SdmmcContext->BootModeReadInProgress == NV_FALSE)
            {
                // Check Whether there is any read ecc error.
                e = HwSdmmcSendCommand(SdmmcCommand_SendStatus,
                        s_SdmmcContext->CardRca, SdmmcResponseType_R1, NV_FALSE);
                if (e == NvError_Success)
                {
                    e = EmmcVerifyResponse(SdmmcCommand_ReadSingle, NV_TRUE);
                  // FIXME_SDMMC - need to verify coherency with ahb
                }
                if (e != NvError_Success)
                    s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_ReadFailure;
            }
        }
        else if (NvBlUtilElapsedTimeUS(s_SdmmcContext->ReadStartTime) >
                 s_SdmmcContext->ReadTimeOutInUs)
        {
            s_SdmmcContext->DeviceStatus = NvBootDeviceStatus_ReadFailure;
        }
    }
    return s_SdmmcContext->DeviceStatus;
}

NvError NvNvBlSdmmcWaitForIdle(void)
{
    while (NvNvBlSdmmcQueryStatus() == NvBootDeviceStatus_ReadInProgress)
        ;
    if (NvNvBlSdmmcQueryStatus() != NvBootDeviceStatus_Idle)
        return NvError_EmmcReadFailed;
    return NvSuccess;
}

void NvNvBlSdmmcShutdown(void)
{
    NvU32 StcReg;
    NvU32 PowerControlHostReg;

    // Stop the clock to SDMMC card.
    NV_SDMMC_READ(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    StcReg = NV_FLD_SET_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
                SD_CLOCK_EN, DISABLE, StcReg);
    NV_SDMMC_WRITE(SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, StcReg);
    // Disable the bus power.
    NV_SDMMC_READ(POWER_CONTROL_HOST, PowerControlHostReg);
    PowerControlHostReg = NV_FLD_SET_DRF_DEF(SDMMC, POWER_CONTROL_HOST,
                            SD_BUS_POWER, POWER_OFF, PowerControlHostReg);
    NV_SDMMC_WRITE(POWER_CONTROL_HOST, PowerControlHostReg);

    // Keep the controller in reset and disable the clock.
    NvBootResetSetEnable(4, NV_TRUE);
    NvClocksSetEnable(4, NV_FALSE);

    s_SdmmcContext = NULL;
}

void GetTransSpeed(void)
{
          // For Emmc, it is 26MHz.
        s_SdmmcContext->TranSpeedInMHz = 26;
}

