/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file
 * @brief <b>nVIDIA driver Development Kit:
 *           Private functions implementation for the slink Rm driver</b>
 *
 * @b Description:  Implements the private functions for the T30 specific slink
 *                  hw interface.
 *
 */

// hardware includes
#include "t30/arslink.h"
#include "../ap15/rm_spi_slink_hw_private.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvassert.h"

#define SLINK_REG_READ32(pSlinkHwRegsVirtBaseAdd, reg) \
        NV_READ32((pSlinkHwRegsVirtBaseAdd) + ((SLINK_##reg##_0)/4))
#define SLINK_REG_WRITE32(pSlinkHwRegsVirtBaseAdd, reg, val) \
    do { \
        NV_WRITE32((((pSlinkHwRegsVirtBaseAdd) + ((SLINK_##reg##_0)/4))), (val)); \
    } while(0)

#define MAX_SLINK_FIFO_DEPTH 32

/**
 * Initialize the slink register.
 */
static void
SlinkHwRegisterInitialize(
    NvU32 SlinkInstanceId,
    SerialHwRegisters *pSlinkHwRegs)
{
    NvU32 CommandReg1;
    NvU32 CommandReg2;
    pSlinkHwRegs->InstanceId = SlinkInstanceId;
    pSlinkHwRegs->pRegsBaseAdd = NULL;
    pSlinkHwRegs->RegBankSize = 0;
    pSlinkHwRegs->HwTxFifoAdd = SLINK_TX_FIFO_0;
    pSlinkHwRegs->HwRxFifoAdd = SLINK_RX_FIFO_0;
    pSlinkHwRegs->IsPackedMode = NV_FALSE;
    pSlinkHwRegs->PacketLength = 1;
    pSlinkHwRegs->CurrSignalMode = NvOdmQuerySpiSignalMode_Invalid;
    pSlinkHwRegs->MaxWordTransfer = MAX_SLINK_FIFO_DEPTH;
    pSlinkHwRegs->IsLsbFirst = NV_FALSE;
    pSlinkHwRegs->IsMasterMode = NV_TRUE;
    pSlinkHwRegs->IsNonWordAlignedPackModeSupported = NV_TRUE;
    pSlinkHwRegs->IsHwChipSelectSupported = NV_TRUE;
    pSlinkHwRegs->IsSlaveAllRxPacketReadable = NV_FALSE;

    CommandReg1 = NV_RESETVAL(SLINK, COMMAND);
    CommandReg2 = NV_RESETVAL(SLINK, COMMAND2);

    // Do not toggle the CS between each packet.
    CommandReg2 = NV_FLD_SET_DRF_DEF(SLINK, COMMAND2, CS_ACTIVE_BETWEEN, HIGH,
                    CommandReg2);

    CommandReg1 = NV_FLD_SET_DRF_DEF(SLINK, COMMAND, M_S, MASTER, CommandReg1);

    if (pSlinkHwRegs->IsIdleDataOutHigh)
        CommandReg1 = NV_FLD_SET_DRF_DEF(SLINK, COMMAND, IDLE_SDA, DRIVE_HIGH, CommandReg1);
    else
        CommandReg1 = NV_FLD_SET_DRF_DEF(SLINK, COMMAND, IDLE_SDA, DRIVE_LOW, CommandReg1);

    pSlinkHwRegs->HwRegs.SlinkRegs.Command1 = CommandReg1;
    pSlinkHwRegs->HwRegs.SlinkRegs.Command2 = CommandReg2;
    pSlinkHwRegs->HwRegs.SlinkRegs.Status = NV_RESETVAL(SLINK, STATUS);
    pSlinkHwRegs->HwRegs.SlinkRegs.DmaControl = NV_RESETVAL(SLINK, DMA_CTL);
}

static void
SlinkHwSetCsSetupHoldTime(
    SerialHwRegisters *pSlinkHwRegs,
    NvU32 CsSetupTimeInClocks,
    NvU32 CsHoldTimeInClocks)
{
    NvU32 CommandReg2 = pSlinkHwRegs->HwRegs.SlinkRegs.Command2;
    NvU32 SetupTime;

    SetupTime = (CsSetupTimeInClocks +1)/2;
    SetupTime = (SetupTime > 3)?3: SetupTime;
    CommandReg2 = NV_FLD_SET_DRF_NUM(SLINK, COMMAND2, SS_SETUP,
                                        SetupTime, CommandReg2);
    pSlinkHwRegs->HwRegs.SlinkRegs.Command2 = CommandReg2;
    SLINK_REG_WRITE32(pSlinkHwRegs->pRegsBaseAdd, COMMAND2,
                            pSlinkHwRegs->HwRegs.SlinkRegs.Command2);
}

/**
 * Initialize the slink intterface for the hw access.
 */
void
NvRmPrivSpiSlinkInitSlinkInterface_v1_2(
    HwInterface *pSlinkInterface,
    NvU32 MinorId)
{
    if (MinorId == 2)
    {
        pSlinkInterface->HwRegisterInitializeFxn = SlinkHwRegisterInitialize;
        pSlinkInterface->HwSetCsSetupHoldTime    = SlinkHwSetCsSetupHoldTime;
    }
}
