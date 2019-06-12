/*
 * Copyright (c) 2009-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Private functions for the i2s hw access</b>
 *
 * @b Description:  Implements  the private interfacing functions for the i2s
 * hw interface.
 *
 */

#include "nvrm_drf.h"
#include "nvddk_i2s_ac97_hw_private.h"
#include "t30/ari2s.h"
#include "nvrm_hardware_access.h"
#include "nvassert.h"
#include "t30/arapbif.h"

//*****************************************************************************
// NvDdkPrivT30I2sHwInitialize
// Initialize the i2s register.
//*****************************************************************************
void NvDdkPrivT30I2sHwInitialize(
        NvU32 I2sChannelId,
        I2sAc97HwRegisters *pI2sHwRegs)
{
    // Initialize the member of I2s hw register structure.
    pI2sHwRegs->IsI2sChannel = NV_TRUE;
    pI2sHwRegs->pRegsVirtBaseAdd = NULL;
    pI2sHwRegs->ChannelId = I2sChannelId;
    pI2sHwRegs->RegsPhyBaseAdd = 0;
    pI2sHwRegs->BankSize = 0;

    // Get the fifo address from the apbif interface
    pI2sHwRegs->TxFifoAddress = 0;
    pI2sHwRegs->RxFifoAddress = 0;
}

//*****************************************************************************
// NvDdkPrivT30I2sSetApbifBaseAddress
// Enable/Disable the data flow.
//*****************************************************************************
void
NvDdkPrivT30I2sSetApbifBaseAddress(
    I2sAc97HwRegisters *pI2sHwRegs,
    void *pChannelInfo)
{
    NvDdkI2sChannelInfo *pChInfo = (NvDdkI2sChannelInfo *)pChannelInfo;

    if ((pChInfo->ChannelType == NvDdkI2sChannelType_InOut) ||
        (pChInfo->ChannelType == NvDdkI2sChannelType_Out))
        NvOsMemcpy(&pI2sHwRegs->ApbifPlayChannelInfo,
                            pChannelInfo, sizeof(NvDdkI2sChannelInfo));

    if ((pChInfo->ChannelType == NvDdkI2sChannelType_InOut) ||
        (pChInfo->ChannelType == NvDdkI2sChannelType_In))
        NvOsMemcpy(&pI2sHwRegs->ApbifRecChannelInfo,
                           pChannelInfo, sizeof(NvDdkI2sChannelInfo));

    pI2sHwRegs->TxFifoAddress =
                    pI2sHwRegs->ApbifPlayChannelInfo.ChannelPhysAddress
                                + APBIF_CHANNEL0_TXFIFO_0;

    pI2sHwRegs->RxFifoAddress =
                    pI2sHwRegs->ApbifRecChannelInfo.ChannelPhysAddress
                                + APBIF_CHANNEL0_RXFIFO_0;
}
//*****************************************************************************
// NvDdkPrivT30I2sSetDataFlow
// Enable/Disable the data flow.
//*****************************************************************************
void
NvDdkPrivT30I2sSetDataFlow(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97Direction Direction,
    NvBool IsEnable)
{
    NvU32 I2sControl = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL);
    if (Direction & I2sAc97Direction_Transmit)
    {
        if (IsEnable)
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, XFER_EN_TX,
                            ENABLE, I2sControl);
        else
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, XFER_EN_TX,
                            DISABLE, I2sControl);
    }
    if (Direction & I2sAc97Direction_Receive)
    {
        if (IsEnable)
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, XFER_EN_RX,
                                ENABLE, I2sControl);
        else
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, XFER_EN_RX,
                                DISABLE, I2sControl);
    }
    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL, I2sControl);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetFlowControl
// Enable/Disable Flow control for Transmit channel
//*****************************************************************************
void
NvDdkPrivT30I2sSetFlowControl(
               I2sAc97HwRegisters *pI2sHwRegs,
               NvBool IsEnable)
{
    NvU32 ControlReg =0;
    NvU32 I2sControl = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL);

    if (IsEnable)
        I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, TX_FLOWCTL_EN,
                        ENABLE, I2sControl);
    else
        I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, TX_FLOWCTL_EN,
                        DISABLE, I2sControl);

    ControlReg = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, TX_STEP);
    ControlReg |= NV_DRF_VAL(I2S, TX_STEP, STEP_SIZE, 0x28F);
    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, TX_STEP, ControlReg);

    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL, I2sControl);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetReplicate
// Enable/Disable Replicate last sample for Transmit channel
//*****************************************************************************
void
NvDdkPrivT30I2sSetReplicate(
               I2sAc97HwRegisters *pI2sHwRegs,
               NvBool IsEnable)
{
#if defined(I2S_CTRL_0_REPLICATE_RANGE)   // !!!FIXME!!! NOT DEFINED IN NET08
    NvU32 I2sControl = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL);

    if (IsEnable)
        I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, REPLICATE,
                        ENABLE, I2sControl);
    else
        I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, REPLICATE,
                        DISABLE, I2sControl);

    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL, I2sControl);
#endif
}

//*****************************************************************************
// NvDdkPrivT30I2sSetBitCode
// Set Bit code for Transmit channel
//*****************************************************************************
void
NvDdkPrivT30I2sSetBitCode(
               I2sAc97HwRegisters *pI2sHwRegs,
               I2sBitCode bitMode)
{
    NvU32 I2sControl = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL);

    I2sControl = NV_FLD_SET_DRF_NUM(I2S, CTRL, BIT_CODE, bitMode, I2sControl);

    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL, I2sControl);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetTriggerLevel
// Set the the trigger level for receive/transmit fifo.
//*****************************************************************************
void
NvDdkPrivT30I2sSetTriggerLevel(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97Direction FifoType,
    NvU32 TriggerLevel)
{
}

//*****************************************************************************
// Currently following are enabled only for cpu side
//*****************************************************************************
#if !NV_IS_AVP

//*****************************************************************************
// NvDdkPrivT30I2sGetFifoCount
// Get the fifo count.
//*****************************************************************************
NvBool
NvDdkPrivT30I2sGetFifoCount(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97Direction Direction,
    NvU32* pFifoCount)
{
    return 0x1F;
}

//*****************************************************************************
// NvDdkPrivT30I2sResetFifo
// Reset the fifo.
//*****************************************************************************
void
NvDdkPrivT30I2sResetFifo(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97Direction Direction)
{
    /*
    NvU32 FifoScr = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, I2S_FIFO_SCR);

    if (Direction & I2sAc97Direction_Receive)
        FifoScr =
            NV_FLD_SET_DRF_DEF(I2S, I2S_FIFO_SCR, FIFO2_CLR, CLEAR, FifoScr);

    if (Direction & I2sAc97Direction_Transmit)
        FifoScr =
            NV_FLD_SET_DRF_DEF(I2S, I2S_FIFO_SCR, FIFO1_CLR, CLEAR, FifoScr);

    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, I2S_FIFO_SCR, FifoScr);
    */
}

//*****************************************************************************
// NvDdkPrivT30I2sSetLoopbackTest
// Enable the loop back in the i2s channels.
//*****************************************************************************
void NvDdkPrivT30I2sSetLoopbackTest(
        I2sAc97HwRegisters *pI2sHwRegs,
        NvBool IsEnable)
{
    NvU32 I2sControlVal = 0;

    I2sControlVal = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL);
    if (IsEnable)
    {
        I2sControlVal =
            NV_FLD_SET_DRF_DEF(I2S, CTRL, LPBK, ENABLE, I2sControlVal);
    }
    else
    {
        I2sControlVal =
            NV_FLD_SET_DRF_DEF(I2S, CTRL, LPBK, DISABLE, I2sControlVal);
    }

    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL, I2sControlVal);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetSamplingFreq
// Set the timing ragister based on the clock source frequency.
//*****************************************************************************
void
NvDdkPrivT30I2sSetSamplingFreq(
    I2sAc97HwRegisters *pI2sHwRegs,
    NvU32 SamplingRate,
    NvU32 DatabitsPerLRCLK,
    NvU32 ClockSourceFreq)
{
    NvU32 FinalSamplingRateConstant;
    NvU32 TimingRegisterVal;

    FinalSamplingRateConstant =
                        NvDdkPrivI2sCalculateSamplingFreq (pI2sHwRegs,
                            SamplingRate, DatabitsPerLRCLK, ClockSourceFreq);

    TimingRegisterVal = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, TIMING);
    TimingRegisterVal = NV_FLD_SET_DRF_NUM(I2S, TIMING, CHANNEL_BIT_CNT,
                        FinalSamplingRateConstant, TimingRegisterVal);
    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, TIMING, TimingRegisterVal);
}


//*****************************************************************************
// NvDdkPrivT30I2sSetInterfaceProperty
// Set the I2s Interface properties
//*****************************************************************************
void
NvDdkPrivT30I2sSetInterfaceProperty(
    I2sAc97HwRegisters *pI2sHwRegs,
    void *pInterface)
{
    NvOdmQueryI2sInterfaceProperty *pIntProps = NULL;
    NvU32 I2sControl;

    pIntProps  = (NvOdmQueryI2sInterfaceProperty *)pInterface;

    // Get the ctrl register value.
    I2sControl = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL);

    if (pIntProps->Mode == NvOdmQueryI2sMode_Master)
        I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, MASTER, ENABLE, I2sControl);
    else
        I2sControl =
                NV_FLD_SET_DRF_DEF(I2S, CTRL, MASTER, DISABLE, I2sControl);


    switch (pIntProps->I2sLRLineControl)
    {
        case NvOdmQueryI2sLRLineControl_LeftOnLow:
            I2sControl =
                NV_FLD_SET_DRF_DEF(I2S, CTRL, LRCK_POLARITY, LOW, I2sControl);
            break;

        case NvOdmQueryI2sLRLineControl_RightOnLow:
            I2sControl =
                NV_FLD_SET_DRF_DEF(I2S, CTRL, LRCK_POLARITY, HIGH, I2sControl);
            break;

        default:
            NV_ASSERT(!"Invalid I2sLrControl");
    }

    switch (pIntProps->I2sDataCommunicationFormat)
    {
        case NvOdmQueryI2sDataCommFormat_I2S:
        case NvOdmQueryI2sDataCommFormat_LeftJustified:
        case NvOdmQueryI2sDataCommFormat_RightJustified:
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, FRAME_FORMAT,
                                LRCK_MODE, I2sControl);
            break;


        case NvOdmQueryI2sDataCommFormat_Dsp:
        case NvOdmQueryI2sDataCommFormat_PCM:
        case NvOdmQueryI2sDataCommFormat_NW:
        case NvOdmQueryI2sDataCommFormat_TDM:

            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, FRAME_FORMAT,
                                FSYNC_MODE, I2sControl);
            break;

        default:
            NV_ASSERT(!"Invalid DataCommFormat");
    }
    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL, I2sControl);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetFifoFormat
// Set the fifo format.
//*****************************************************************************
void
NvDdkPrivT30I2sSetFifoFormat(
    I2sAc97HwRegisters *pI2sHwRegs,
    NvDdkI2SDataWordValidBits DataFifoFormat,
    NvU32 DataSize)
{
    NvU32 I2sControl;

    I2sControl = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL);
    switch (DataSize)
    {
        case 8:
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, BIT_SIZE,
                            BIT_SIZE_8, I2sControl);
            break;

        case 12:
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, BIT_SIZE,
                            BIT_SIZE_12, I2sControl);
            break;

        case 20:
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, BIT_SIZE,
                            BIT_SIZE_20, I2sControl);
            break;

        case 24:
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, BIT_SIZE,
                            BIT_SIZE_24, I2sControl);
            break;

        case 28:
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, BIT_SIZE,
                            BIT_SIZE_28, I2sControl);
            break;

        case 32:
            I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, BIT_SIZE,
                            BIT_SIZE_32, I2sControl);
            break;

        case 16:
        default:
             I2sControl = NV_FLD_SET_DRF_DEF(I2S, CTRL, BIT_SIZE,
                            BIT_SIZE_16, I2sControl);
            break;
    }

    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL, I2sControl);
}


//*****************************************************************************
// NvDdkPrivT30I2sGetInterruptSource
// Get the interrupt source occured from i2s channels.
//*****************************************************************************
/*I2sHwInterruptSource
NvDdkPrivT30I2sGetInterruptSource(
    I2sAc97HwRegisters *pI2sHwRegs)
{
    return I2sHwInterruptSource_None;
}*/

//*****************************************************************************
// NvDdkPrivT30I2sSetNumberOfSlotsFxn
// Sets the number of active slots
//*****************************************************************************
void
NvDdkPrivT30I2sSetNumberOfSlotsFxn(
        I2sAc97HwRegisters *pHwRegs,
        NvU32 NumberOfSlotsPerFsync)
{
    NvU32 ControlReg =0;
    ControlReg = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd, SLOT_CTRL);
    ControlReg =
        NV_FLD_SET_DRF_NUM(I2S, SLOT_CTRL, TOTAL_SLOTS,
                                NumberOfSlotsPerFsync, ControlReg);
    I2S_REG_WRITE32(pHwRegs->pRegsVirtBaseAdd, SLOT_CTRL, ControlReg);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetDataOffset
// Sets the Data offset to fsync
//*****************************************************************************
void
NvDdkPrivT30I2sSetDataOffset(
         I2sAc97HwRegisters *pHwRegs,
         NvBool IsReceive,
         NvU32  DataOffset)
{
    NvU32 ControlReg =0;
    ControlReg = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd, OFFSET);
    if (IsReceive)
        ControlReg =
            NV_FLD_SET_DRF_NUM(I2S, OFFSET, RX_DATA_OFFSET,
                            DataOffset, ControlReg);
    else
        ControlReg =
            NV_FLD_SET_DRF_NUM(I2S, OFFSET, TX_DATA_OFFSET,
                            DataOffset, ControlReg);

    I2S_REG_WRITE32(pHwRegs->pRegsVirtBaseAdd, OFFSET, ControlReg);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetMaskBits
// Sets the Slot selection mask for Tx or Rx .
//*****************************************************************************
void
NvDdkPrivT30I2sSetMaskBits(
            I2sAc97HwRegisters *pHwRegs,
            I2sAc97Direction Direction,
            NvU8 SlotSelectionMask,
            NvBool IsEnable)
{
    NvU32 ControlReg = 0;
    ControlReg = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd, CH_CTRL);
    if (Direction & I2sAc97Direction_Receive) // Rx mode
    {
        ControlReg =
            NV_FLD_SET_DRF_NUM(I2S, CH_CTRL, RX_MASK_BITS,
                        SlotSelectionMask, ControlReg);
    }
    else if (Direction & I2sAc97Direction_Transmit)// Tx mode
    {
        ControlReg =
            NV_FLD_SET_DRF_NUM(I2S, CH_CTRL, TX_MASK_BITS,
                        SlotSelectionMask, ControlReg);
    }

    I2S_REG_WRITE32(pHwRegs->pRegsVirtBaseAdd, CH_CTRL, ControlReg);
}


//*****************************************************************************
// NvDdkPrivT30I2sSetClockEdge
// set the edge clock on positive or negative
//*****************************************************************************
void
NvDdkPrivT30I2sSetClockEdge(
        I2sAc97HwRegisters *pHwRegs,
        NvDdkI2sDataClockEdge DataClkEdge)
{
    NvU32 ControlReg =0;
    ControlReg = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd, CH_CTRL);

    if (DataClkEdge == NvDdkI2sDataClockEdge_Left)
        ControlReg = NV_FLD_SET_DRF_DEF(I2S, CH_CTRL, EDGE_CTRL,
                     POS_EDGE, ControlReg);
    else
        ControlReg = NV_FLD_SET_DRF_DEF(I2S, CH_CTRL, EDGE_CTRL,
                     NEG_EDGE, ControlReg);

    I2S_REG_WRITE32(pHwRegs->pRegsVirtBaseAdd, CH_CTRL, ControlReg);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetDataWordFormatFxn
// Sets the Tx or Rx Data word format (LSB first or MSB first)
//*****************************************************************************
void
NvDdkPrivT30I2sSetDataWordFormatFxn(
        I2sAc97HwRegisters *pHwRegs,
        NvBool IsRecieve,
        NvDdkI2SDataWordValidBits I2sDataWordFormat)
{
    NvU32 ControlReg =0;
    ControlReg = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd, CH_CTRL);
    if (IsRecieve)  // Rx mode
    {
        if (I2sDataWordFormat == NvDdkI2SDataWordValidBits_StartFromLsb)
            ControlReg |= NV_DRF_DEF(I2S, CH_CTRL, RX_BIT_ORDER, LSB_FIRST);
        else if (I2sDataWordFormat == NvDdkI2SDataWordValidBits_StartFromMsb)
            ControlReg |= NV_DRF_DEF(I2S, CH_CTRL, RX_BIT_ORDER, MSB_FIRST);
        else
            NV_ASSERT(!"I2s Data format not supported\n");
    }
    else            // Tx mode
    {
        if (I2sDataWordFormat == NvDdkI2SDataWordValidBits_StartFromLsb)
            ControlReg |= NV_DRF_DEF(I2S, CH_CTRL, TX_BIT_ORDER, LSB_FIRST);
        else if (I2sDataWordFormat == NvDdkI2SDataWordValidBits_StartFromMsb)
            ControlReg |= NV_DRF_DEF(I2S, CH_CTRL, TX_BIT_ORDER, MSB_FIRST);
        else
            NV_ASSERT(!"I2s Data format not supported\n");
    }
    I2S_REG_WRITE32(pHwRegs->pRegsVirtBaseAdd, CH_CTRL, ControlReg);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetSlotSizeFxn
// Sets the number of bits per slot
//*****************************************************************************
void
NvDdkPrivT30I2sSetSlotSizeFxn(
    I2sAc97HwRegisters * pHwRegs,
    NvU32 SlotSize)
{
    //NvU32 ControlReg =0;
    //ControlReg = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd, TDM_CTRL);
    //ControlReg |= NV_DRF_VAL(I2S, TDM_CTRL, TDM_BIT_SIZE, SlotSize);
    //I2S_REG_WRITE32(pHwRegs->pRegsVirtBaseAdd, TDM_CTRL, TdmControlReg);
}


//*****************************************************************************
// NvDdkPrivT30I2sSetAudioCif
// Sets the acif property
//*****************************************************************************
void
NvDdkPrivT30I2sSetAudioCif(
    I2sAc97HwRegisters * pHwRegs,
    NvBool IsReceive,
    NvBool IsRead,
    NvU32 *pCrtlValue)
{
    NvU32 I2sAcifReg =0;
    if (IsRead)
    {
        if (IsReceive)
            *pCrtlValue = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd,
                            AUDIOCIF_I2STX_CTRL);
        else
            *pCrtlValue = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd,
                            AUDIOCIF_I2SRX_CTRL);
    }
    else
    {
        if (!IsReceive)
        {
            I2sAcifReg = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd,
                            AUDIOCIF_I2STX_CTRL);
            I2sAcifReg |= *pCrtlValue;
            I2S_REG_WRITE32(pHwRegs->pRegsVirtBaseAdd, AUDIOCIF_I2STX_CTRL,
                            I2sAcifReg);
        }
        else
        {
            I2sAcifReg = I2S_REG_READ32(pHwRegs->pRegsVirtBaseAdd,
                             AUDIOCIF_I2SRX_CTRL);
            I2sAcifReg |= *pCrtlValue;
            I2S_REG_WRITE32(pHwRegs->pRegsVirtBaseAdd, AUDIOCIF_I2SRX_CTRL,
                            I2sAcifReg);
        }
    }
}

//*****************************************************************************
// NvDdkPrivT30I2sGetStandbyRegisters
// Return the I2s registers to be saved before standby entry.
//*****************************************************************************
void
NvDdkPrivT30I2sGetStandbyRegisters(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97StandbyRegisters *pStandbyRegs)
{
//    NvU32 Reg32Rd;

     //I2s Control Register - entire register saved and restored
  //   Reg32Rd  = I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL);
     // Retain only the needed regbits
     //pStandbyRegs->I2sAc97CtrlReg = Reg32Rd & 0x03FFFFF0;

     // I2s Timing Registers
     pStandbyRegs->I2sAc97TimingReg =
                I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, TIMING);

     // Spdif Fifo Config and status register - Attn Lvl
     //pStandbyRegs->I2sAc97FifoConfigReg  =
     //            I2S_REG_READ32(pI2sHwRegs->pRegsVirtBaseAdd, I2S_FIFO_SCR);
}

//*****************************************************************************
// NvDdkPrivT30I2sSetStandbyRegisters
// Set the I2s registers to be saved before PowerOn entry.
//*****************************************************************************
void
NvDdkPrivT30I2sSetStandbyRegisters(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97StandbyRegisters *pStandbyRegs)
{
    //I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, CTRL, pStandbyRegs->I2sAc97CtrlReg);
    I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, TIMING,
                        pStandbyRegs->I2sAc97TimingReg);
    //I2S_REG_WRITE32(pI2sHwRegs->pRegsVirtBaseAdd, I2S_FIFO_SCR,
    //         pStandbyRegs->I2sAc97FifoConfigReg);
}

#endif


