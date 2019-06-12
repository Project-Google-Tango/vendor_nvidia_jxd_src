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
 * @brief <b>nVIDIA Driver Development Kit:
 *           Private functions for the i2s Ddk driver</b>
 *
 * @b Description:  Defines the private interfacing functions for the i2s
 * hw interface.
 *
 */

#ifndef INCLUDED_T30I2S_HW_PRIVATE_H
#define INCLUDED_T30I2S_HW_PRIVATE_H

/**
 * @defgroup nvddk_i2s Integrated Inter Sound (I2S) Controller hw interface API
 *
 * This is the I2S hw interface controller api.
 *
 * @ingroup nvddk_modules
 * @{
 *
 */

#include "nvcommon.h"
#include "nvddk_i2s.h"
#include "nvodm_query.h"

#if defined(__cplusplus)
extern "C" {
#endif

 /**
 * Bit coding for transmission
 */
typedef enum
{
    // Linear
    I2sBitCode_Linear = 0x0,

    // ulaw
    I2sBitCode_uLaw = 0x1,

    // alaw
    I2sBitCode_ALaw = 0x2,

    // reserved
    I2sBitCode_RSVD = 0x3,

    I2sBitCode_Force32 = 0x7FFFFFFF

} I2sBitCode;

/**
 * Clock Edge control
 */
typedef enum
{
    NvDdkI2sDataClockEdge_Left = 0,
    NvDdkI2sDataClockEdge_Right
}NvDdkI2sDataClockEdge;

/**
 * NvDdkPrivT30I2sHwInitialize
 * @brief Initialize the i2s register.
 * @param InstanceId InstanceId for i2s
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 */
void
NvDdkPrivT30I2sHwInitialize(
    NvU32 InstanceId,
    I2sAc97HwRegisters *pI2sHwRegs);

/**
 * NvDdkPrivT30I2sSetDataFlow
 * @brief Enable/Disable the data flow.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param I2sAc97Direction Direction mode whether Tx or Rx
 * @param IsEnable Variable to enable or disable the data flow
 */
void
NvDdkPrivT30I2sSetDataFlow(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97Direction Direction,
    NvBool IsEnable);


/**
* NvDdkPrivT30I2sSetFlowControl
* @brief Enable/Disable Flow control for Transmit channel
* @param I2sAc97HwRegisters The pointer to I2s Hw structure
* @param IsEnable Variable to enable or disable the flow
*/
void
NvDdkPrivT30I2sSetFlowControl(
               I2sAc97HwRegisters *pI2sHwRegs,
               NvBool IsEnable);

/**
* NvDdkPrivT30I2sSetReplicate
* @brief  Enable/Disable Replicate last sample for Transmit channel
*         used mainly when there is an underflow happens in Tx side
* @param I2sAc97HwRegisters The pointer to I2s Hw structure
* @param IsEnable Variable to enable or disable the last sample
*/
void
NvDdkPrivT30I2sSetReplicate(
               I2sAc97HwRegisters *pI2sHwRegs,
               NvBool IsEnable);

/**
* NvDdkPrivT30I2sSetBitCode
* @brief  Set Bit code for Transmit channel
* @param I2sAc97HwRegisters The pointer to I2s Hw structure
* @param I2sBitCode specify the needed bit mode
*/
void
NvDdkPrivT30I2sSetBitCode(
               I2sAc97HwRegisters *pI2sHwRegs,
               I2sBitCode bitMode);

/**
 * NvDdkPrivT30I2sSetTriggerLevel
 * @brief Set the the trigger level for receive/transmit fifo.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param I2sAc97Direction Direction mode whether Tx or Rx
 * @param TriggerLevel Variable to enable or disable the trigger level
 */
void
NvDdkPrivT30I2sSetTriggerLevel(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97Direction FifoType,
    NvU32 TriggerLevel);

#if !NV_IS_AVP

/**
 * NvDdkPrivT30I2sGetFifoCount
 * @brief Get the fifo count.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param I2sAc97Direction Direction mode whether Tx or Rx
 * @param pFifoCount pointer to return free fifo count
 */
NvBool
NvDdkPrivT30I2sGetFifoCount(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97Direction Direction,
    NvU32* pFifoCount);

/**
 * NvDdkPrivT30I2sSetFifoFormat
 * @brief Set the fifo format
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param NvDdkI2SDataWordValidBits specify the Data format.
 * @param DataSize specify the bit size being used
 */
void
NvDdkPrivT30I2sSetFifoFormat(
    I2sAc97HwRegisters *pI2sHwRegs,
    NvDdkI2SDataWordValidBits DataFifoFormat,
    NvU32 DataSize);

/**
 * NvDdkPrivT30I2sSetLoopbackTest
 * @brief Enable the loop back in the i2s channels.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param IsEnable Variable to enable or disable the loopback feature
 */
void NvDdkPrivT30I2sSetLoopbackTest(I2sAc97HwRegisters *pI2sHwRegs, NvBool IsEnable);

/**
 * NvDdkPrivT30I2sResetFifo
 * @brief Reset the fifo.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param I2sAc97Direction Direction mode whether Tx or Rx
 */
void
NvDdkPrivT30I2sResetFifo(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97Direction Direction);


/**
 * NvDdkPrivT30I2sSetApbifBaseAddress
 * @brief Set the apbif channel base address to  I2s use.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param pChannelInfo struct that explains the channel information
 */
void
NvDdkPrivT30I2sSetApbifBaseAddress(
    I2sAc97HwRegisters *pI2sHwRegs,
    void *pChannelInfo);

/**
 * NvDdkPrivT30I2sSetSamplingFreq
 * @brief Set the timing register based on the clock source frequency.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param SamplingRate Specify the sampling rate
 * @param DatabitsPerLRCLK Specify the Databits per LR CLK
 * @param ClockSourceFreq Specify the frequency of the clock source
 */
void
NvDdkPrivT30I2sSetSamplingFreq(
    I2sAc97HwRegisters *pI2sHwRegs,
    NvU32 SamplingRate,
    NvU32 DatabitsPerLRCLK,
    NvU32 ClockSourceFreq);


/**
 * NvDdkPrivT30I2sSetInterfaceProperty
 * @brief Set the register based on the interface variable being set
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param pInterface pointer to the interface property
 */
void
NvDdkPrivT30I2sSetInterfaceProperty(
    I2sAc97HwRegisters *pI2sHwRegs,
    void *pInterface);

/*
 * NvDdkPrivT30I2sGetStandbyRegisters
 * @brief Return the I2s registers to be saved before standby entry.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 */
void
NvDdkPrivT30I2sGetStandbyRegisters(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97StandbyRegisters *pStandbyRegs);

/*
 * NvDdkPrivT30I2sSetStandbyRegisters
 * @brief Set the I2s registers to be saved before PowerOn entry.
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 */
void
NvDdkPrivT30I2sSetStandbyRegisters(
    I2sAc97HwRegisters *pI2sHwRegs,
    I2sAc97StandbyRegisters *pStandbyRegs);


/**
 * NvDdkPrivT30I2sSetAudioCif
 * @brief Sets the Acif property of Tx or Rx ctrl
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param IsRecieve Specify whether this is for Tx or Rx
 * @param IsRead Specify whether this is read or write operation
 * @param CrtlValue Specify the control value
 */
void
NvDdkPrivT30I2sSetAudioCif(
    I2sAc97HwRegisters * pHwRegs,
    NvBool IsRecieve,
    NvBool IsRead,
    NvU32 *pCrtlValue);

/**
 * NvDdkPrivT30I2sSetDataOffset
 * @brief Sets the data offset to fsync
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param IsRecieve Specify whether this is for Tx or Rx
 * @param DataOffset Specify the data offset
 */
void
NvDdkPrivT30I2sSetDataOffset(
         I2sAc97HwRegisters *pHwRegs,
         NvBool IsReceive,
         NvU32  DataOffset);

/**
 * NvDdkPrivT30I2sSetClockEdge 
 * @brief Set the edge clock on positive or negative
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param NvDdkI2sDataClockEdge Specify edge type
 */
void
NvDdkPrivT30I2sSetClockEdge(
        I2sAc97HwRegisters *pHwRegs,
        NvDdkI2sDataClockEdge DataClkEdge);

/**
 * NvDdkPrivT30I2sSetMaskBits
 * @brief Sets the Slot selection mask for Tx or Rx 
 *        based on IsRecieve flag
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param Direction Specify whether this is for Tx or Rx
 * @param SlotSelectionMask Specify the slot selection mask
 * @param IsEnable Enable/Disable
 */
void
NvDdkPrivT30I2sSetMaskBits(
            I2sAc97HwRegisters *pHwRegs,
            I2sAc97Direction Direction,
            NvU8 SlotSelectionMask,
            NvBool IsEnable);


/**
 * NvDdkPrivT30I2sSetNumberOfSlotsFxn
 * @brief Sets the number of active slots
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param NumberOfSlotsPerFsync Specify number of slots per fsync
 */
void
NvDdkPrivT30I2sSetNumberOfSlotsFxn(
        I2sAc97HwRegisters *pHwRegs,
        NvU32 NumberOfSlotsPerFsync);

/**
 * NvDdkPrivT30I2sSetDataWordFormatFxn
 * @brief Sets the Tx or Rx Data word format (LSB first or MSB first)
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param NvDdkI2SDataWordValidBits specify the Data format.
 * @param IsRecieve Specify whether this is for Tx or Rx
 */
void
NvDdkPrivT30I2sSetDataWordFormatFxn(
        I2sAc97HwRegisters *pHwRegs,
        NvBool IsRecieve,
        NvDdkI2SDataWordValidBits I2sDataWordFormat);

/**
 * NvDdkPrivT30I2sSetSlotSizeFxn
 * @brief Sets the number of bits per slot in TDM mode
 * @param I2sAc97HwRegisters The pointer to I2s Hw structure
 * @param SlotSize Set the slotSize
 */
void
NvDdkPrivT30I2sSetSlotSizeFxn(
        I2sAc97HwRegisters *pHwRegs,
        NvU32 SlotSize);

#endif  // !NV_IS_AVP
/** @} */

#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_DDKI2S_HW_PRIVATE_H
