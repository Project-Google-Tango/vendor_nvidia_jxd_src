/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
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

#ifndef INCLUDED_NVDDK_I2S_AC97_HW_PRIVATE_H
#define INCLUDED_NVDDK_I2S_AC97_HW_PRIVATE_H

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

/// Define the sampling rate calculation accuracy in terms of %
#define SAMPLING_RATE_ACCURACY  2

#define I2S_REG_READ32(pI2sHwRegsVirtBaseAdd, reg) \
        NV_READ32((pI2sHwRegsVirtBaseAdd) + ((I2S_##reg##_0)/sizeof(NvU32)))

#define I2S_REG_WRITE32(pI2sHwRegsVirtBaseAdd, reg, val) \
    do { \
        NV_WRITE32((((pI2sHwRegsVirtBaseAdd) + ((I2S_##reg##_0)/sizeof(NvU32)))), (val)); \
    } while (0)

#if !NV_IS_AVP
// Clock source supported by the i2s clock source in KHz.
static const NvU32 s_I2sClockSourceFreq[] = {250, 11289, 11289, 12288, 48000};
static const NvU32 s_I2sSupportedFreq[]   = {44100, 48000, 22050, 11025, 8000, 12000, 16000,
                                            24000, 32000, 88200, 96000};
#endif //!NV_IS_AVP


//#define NVDDK_I2S_POWER_PRINT
//#define NVDDK_I2S_CONTROLLER_PRINT
//#define NVDDK_I2S_REG_PRINT

#if defined(NVDDK_I2S_POWER_PRINT)
    #define NVDDK_I2S_POWERLOG(x)       NvOsDebugPrintf x
#else
    #define NVDDK_I2S_POWERLOG(x)       do {} while(0)
#endif
#if defined(NVDDK_I2S_CONTROLLER_PRINT)
    #define NVDDK_I2S_CONTLOG(x)        NvOsDebugPrintf x
#else
    #define NVDDK_I2S_CONTLOG(x)        do {} while(0)
#endif
#if defined(NVDDK_I2S_REG_PRINT)
    #define NVDDK_I2S_REGLOG(x)         NvOsDebugPrintf x
#else
    #define NVDDK_I2S_REGLOG(x)         do {} while(0)
#endif

/**
 * Combines the definition of the i2s register and modem signals.
 */
typedef struct
{
    NvBool IsI2sChannel;

    // I2s channel Id.
    NvU32 ChannelId;

    // Virtual base address of the i2s hw register.
    NvU32 *pRegsVirtBaseAdd;

    // Physical base address of the i2s hw register.
    NvRmPhysAddr RegsPhyBaseAdd;

    // Register bank size.
    NvU32 BankSize;

    // Tx Fifo address of i2s/ Txfifo1 address of the AC97.
    NvRmPhysAddr TxFifoAddress;

    // Rx Fifo address of i2s/ Rxfifo1 address of the AC97.
    NvRmPhysAddr RxFifoAddress;

#if !NV_IS_AVP
    // Txfifo1 address of the AC97.
    NvRmPhysAddr TxFifo2Address;

    // Rxfifo1 address of the AC97.
    NvRmPhysAddr RxFifo2Address;
#endif

    // Apbif channel information
    NvDdkI2sChannelInfo  ApbifPlayChannelInfo;
    NvDdkI2sChannelInfo  ApbifRecChannelInfo;

} I2sAc97HwRegisters;

#if !NV_IS_AVP
/**
 * Combines the i2s interrupt sources.
 */
typedef enum
{
    // No I2s interrupt source.
    I2sHwInterruptSource_None = 0x0,

    // Receive I2s interrupt source.
    I2sHwInterruptSource_Receive = 0x1,

    // Transmit interrupt source.
    I2sHwInterruptSource_Transmit = 0x2,

    I2sHwInterruptSource_Force32 = 0x7FFFFFFF
} I2sHwInterruptSource;

#endif //!NV_IS_AVP

/**
 * The data flow of the i2s controller.
 */
typedef enum
{
    // Fifo type to None
    I2sAc97Direction_None = 0x0,

    // Receive fifo type.
    I2sAc97Direction_Receive = 0x1,

    // Transmit fifo type.
    I2sAc97Direction_Transmit = 0x2,

    // Both, receive and transmit fifo type.
    I2sAc97Direction_Both = 0x3,

    I2sAc97Direction_Force32 = 0x7FFFFFFF

} I2sAc97Direction;

/**
 * Dma bus width
 */
typedef enum
{
    // Dma width 8
    I2sDma_Width_8 = 1,

    // width 16
    I2sDma_Width_16 = 2,

    // width 32
    I2sDma_Width_32 = 4,

    // width 64
    I2sDma_Width_64 = 8,

    // width 32
    I2sDma_Width_128 = 16,

    I2sDma_Width_Force32 = 0x7FFFFFFF

} I2sDma_Width;

/**
 * I2s Trigger Level
 */
typedef enum
{
    // one slot
    I2sTriggerlvl_1 = 0x0,

    // four slot
    I2sTriggerlvl_4,

    // 8 slot
    I2sTriggerlvl_8,

    // 12 slot
    I2sTriggerlvl_12,

    I2sTriggerlvl_Force32 = 0x7FFFFFFF

} I2sTriggerlvl;

#if !NV_IS_AVP
/*
 * i2s/ac97 Registers saved before standby entry
 * and are restored after exit from stanbby mode.
 */
typedef struct I2sAc97StandbyRegistersRec
{
    NvU32 I2sAc97PcmCtrlReg;    // 0x0
    NvU32 I2sAc97TimingReg;     // 0x8
    NvU32 I2sAc97FifoConfigReg; // 0xc
    NvU32 I2sAc97CtrlReg;       // 0x10
    NvU32 I2sAc97NwCtrlReg;     // 0x14
} I2sAc97StandbyRegisters;

#endif // !NV_IS_AVP




#if !NV_IS_AVP

/**
 * Get the clock source frequency for desired sampling rate.
 */
NvU32 NvDdkPrivI2sGetClockSourceFreq(NvU32 SamplingRate, NvU32 DatabitsPerLRCLK);

/**
 * Calculate the timing register value based on the clock source frequency.
 */

NvU32
NvDdkPrivI2sCalculateSamplingFreq(
    I2sAc97HwRegisters *pI2sHwRegs,
    NvU32 SamplingRate,
    NvU32 DatabitsPerLRCLK,
    NvU32 ClockSourceFreq);

/**
 * Verify whether to enable the Auto Adjust flag for Clock function
 * for PLLA to program the correct frequency based on SamplingRate
 */
NvBool NvDdkPrivI2sGetAutoAdjustFlag(NvU32 SamplingRate);

#endif  // !NV_IS_AVP
/** @} */

#include "t30ddk_i2s_hw_private.h"

#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_NVDDK_I2S_AC97_HW_PRIVATE_H
