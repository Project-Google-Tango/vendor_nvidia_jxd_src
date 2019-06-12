/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *                  I2s Driver implementation</b>
 *
 * @b Description: Implementation of the NvDdk I2S API.
 *
 */

#include "nvddk_i2s.h"
#include "nvddk_i2s_ac97_hw_private.h"
#include "nvrm_dma.h"
#include "nvassert.h"
#include "nvrm_hardware_access.h"
#include "nvrm_pinmux.h"

#if !NV_IS_AVP
#include "nvodm_query.h"
#include "nvrm_power.h"
#include "nvrm_interrupt.h"
#endif //!NV_IS_AVP

// Constants used to size arrays.  Must be >= the max of all chips.
#define MAX_I2S_CONTROLLERS 5

#define MAX_I2S_AC97_CONTROLLERS (MAX_I2S_CONTROLLERS)
#define INSTANCE_MASK 0xFFFF

#if !defined(ADDRESS_WRAP)
#define ADDRESS_WRAP(bus_width_bits, wrap_size_byte) \
                     (((bus_width_bits) << 16) | (wrap_size_byte & 0xFFFF))
#endif

typedef struct
{

    // Instance Id.
    NvU32 InstanceId;

    // Holds whether packed mode is supported or not  i.e. half of the data is
    // for right channel and half of the data is for left channel. The I2S
    // data word size will be of I2SDataWordSizeInBits from capability
    // structure.
    NvBool IsValidBitsInPackedModeSupported;

    // Holds whether valid bits in the I2S data word start from Msb is
    // supported or not.
    NvBool IsLsbAlignmentSupported;

    // Holds whether valid bits in the I2S data word start from Lsb is
    // supported or not.
    NvBool IsMsbAlignmentSupported;

    // Holds whether Mono data format is supported or not  i.e. single I2S
    //data
    // word which can go to both right and left channel.
    NvBool IsMonoFormatSupported;

    // Holds whether loopback mode is supported or not.This can be used for
    // self test.
    NvBool IsLoopbackSupported;

    // Holds the information for the I2S data word size in bits.  The client
    // should passed the data buffer such that each I2S data word should match
    // with this value.
    // All passed physical address and bytes requested size should be aligned
    // to this value.
    NvU32 I2sDataWordSizeInBits;

    NvBool IsNetworkModeSupported;
    NvBool IsTDMModeSupported;

    // AudioCIF mode is supported for latest H/w Version.
    NvBool IsAudioCIFModeSupported;
} SocI2sCapability;

/**
 * Combines the i2s/ac97 channel information.
 */
typedef struct
{
    // Reference count of how many times the I2S has been opened.
    NvU32 OpenCount;

    // Nv Rm device handles.
    NvRmDeviceHandle hDevice;

    // Pointer to the head of the i2s channel handle.
    NvDdkI2sHandle hI2sChannelList[MAX_I2S_AC97_CONTROLLERS];

    // Mutex for i2s channel information.
    NvOsMutexHandle hOsMutex4I2sInfoAccess;

    NvU32 TotalI2sChannel;

    // The i2s capabiity which is general per chip
    SocI2sCapability SocI2sCaps;

} DdkI2sInfo, *DdkI2sInfoHandle;

/**
 * Functions for the hw interfaces.
 */
typedef struct
{
    /**
     * Initialize the i2s register.
     */
    void (*HwInitializeFxn)(NvU32 InstanceId, I2sAc97HwRegisters *pHwRegs);

    /**
     * Enable/Disable the data flow.
     */
    void
    (*SetDataFlowFxn)(
        I2sAc97HwRegisters *pHwRegs,
        I2sAc97Direction Direction,
        NvBool IsEnable);

    /**
    * Set the the trigger level for receive/transmit fifo.
    */
    void
    (*SetTriggerLevelFxn)(
        I2sAc97HwRegisters *pHwRegs,
        I2sAc97Direction FifoType,
        NvU32 TriggerLevel);

    // Following features are enabled only for cpu side.
#if !NV_IS_AVP

     /**
     * Enable/Disable the dsp data flow.
     */
    void
    (*SetDspDataFlowFxn)(
        I2sAc97HwRegisters *pI2sHwRegs,
        I2sAc97Direction Direction,
        NvBool IsEnable);

    /**
     * Reset the fifo.
     */
    void (*ResetFifoFxn)(I2sAc97HwRegisters *pHwRegs,
            I2sAc97Direction Direction);

    /**
     * Enable the loop back in the i2s channels.
     */
    void (*SetLoopbackTestFxn)(I2sAc97HwRegisters *pHwRegs, NvBool IsEnable);

    /**
     * Get the clock source frequency for desired sampling rate.
     */
    NvU32 (*GetClockSourceFreqFxn)(NvU32 SamplingRate, NvU32 DatabitsPerLRCLK);

    /**
     * Verify whether to enable the Auto Adjust flag for Clock function
     * for PLLA to program the correct frequency based on SamplingRate
     */
    NvBool (*GetAutoAdjustFxn)(NvU32 SamplingRate);

    /**
     * Set the timing ragister based on the clock source frequency.
     */
    void
    (*SetSamplingFreqFxn)(
        I2sAc97HwRegisters *pHwRegs,
        NvU32 SamplingRate,
        NvU32 DatabitsPerLRCLK,
        NvU32 ClockSourceFreq);

    /**
     * Set the fifo format.
     */
    void (*SetFifoFormatFxn)(
        I2sAc97HwRegisters *pHwRegs,
        NvDdkI2SDataWordValidBits DataFifoFormat,
        NvU32 DataSize);

    /**
     * Set the I2s left right control polarity.
     */
    void (*SetInterfacePropertyFxn)(
        I2sAc97HwRegisters *pHwRegs,
        void *pInterface);


    /**
     * Get the interrupt source occured from i2s channels.
     */
    I2sHwInterruptSource (*GetInterruptSourceFxn)(I2sAc97HwRegisters *pHwRegs);

    /**
     * Ack the interrupt source occured from i2s channels.
     */
    void
    (*AckInterruptSourceFxn)(
        I2sAc97HwRegisters *pHwRegs,
        I2sHwInterruptSource IntSource);

    /**
     * Sets the Slot selection mask for Tx or Rx in the Netowrk mode
     * based on IsRecieve flag
     */
    void
    (*NwModeSetSlotSelectionMaskFxn)(
        I2sAc97HwRegisters *pHwRegs,
        I2sAc97Direction Direction,
        NvU8 ActiveRxSlotSelectionMask,
        NvBool IsEnable);

    /**
     * Sets the number of active slots in TDM mode
     */
    void
    (*TdmModeSetNumberOfSlotsFxn)(
        I2sAc97HwRegisters *pHwRegs,
        NvU32 NumberOfSlotsPerFsync);

    /**
     * Sets the Tx or Rx Data word format (LSB first or MSB first)
     */
    void
    (*TdmModeSetDataWordFormatFxn)(
        I2sAc97HwRegisters *pHwRegs,
        NvBool IsRecieve,
        NvDdkI2SDataWordValidBits I2sDataWordFormat);

    /**
     * Sets the number of bits per slot in TDM mode
     */
    void
    (*TdmModeSetSlotSizeFxn)(
        I2sAc97HwRegisters *pHwRegs,
        NvU32 SlotSize);

    /**
    * Sets the apbif channel used with i2s
    */
    void
    (*SetApbifChannelFxn)(
        I2sAc97HwRegisters *pHwRegs,
        void *pChannelInfo);

    /**
    * Sets the i2s acif register
    */
    void
    (*SetAcifRegisterFxn)(
        I2sAc97HwRegisters *pHwRegs,
        NvBool IsReceive,
        NvBool IsRead,
        NvU32 *pCrtlValue);
#endif

} I2sAc97HwInterface;

/**
 * I2s handle which combines all the information related to the given i2s
 * channels
 */
typedef struct NvDdkI2sRec
{
    // Nv Rm device handles.
    NvRmDeviceHandle hDevice;

    // Instance Id of the combined i2s/AC97 channels passed by client.
    NvU32 InstanceId;

    // Module Id.
    NvRmModuleID RmModuleId;

    // Dma module Id
    NvRmDmaModuleID RmDmaModuleId;

    // Channel reference count.
    NvU32 OpenCount;

    // I2s/Ac97 hw register information.
    I2sAc97HwRegisters I2sAc97HwRegs;

    // Receive Synchronous sempahore Id which need to be signalled.
    NvOsSemaphoreHandle RxSynchSema;

    // Transmit Synchronous sempahore Id which need to be signalled.
    NvOsSemaphoreHandle TxSynchSema;

    // Read dma handle.
    NvRmDmaHandle hRxRmDma;

    // Write dma handle.
    NvRmDmaHandle hTxRmDma;

    NvError RxTransferStatus;

    NvError TxTransferStatus;

    // Mutex to access this channel.
    NvOsMutexHandle MutexForChannelAccess;

    // Hw interface apis
    I2sAc97HwInterface *pHwInterface;

    /// Holds the sampling rate. Based on sampling rate it generates the clock
    /// and sends the data if it is in master mode. This parameter will be
    /// ignored when slave mode is selected.
    NvU32 SamplingRate;

    // Save the DatabitsPerBCLK when i2s is in Master mode
    NvU32 DatabitsPerLRCLK;

    // Pointer to the i2s information.
    DdkI2sInfo *pI2sInfo;

    // Mode (PCM/Network/TDM/DSP)
    NvDdkI2SMode I2sMode;

    // I2s data property config based on Mode (PCM/Netowrk/TDM).
    void * I2sDataProps;

    // Dma width
    NvU32 DmaWidth;

#if !NV_IS_AVP

    // I2s configuration parameter.
    NvOdmQueryI2sInterfaceProperty I2sInterfaceProps;

    // Ddk i2s capability.
    NvDdkI2sDriverCapability DdkI2sCaps;

    NvOsInterruptHandle InterruptHandle;

    NvOsSemaphoreHandle hPowerMgmntSema;

    NvU32 RmPowerClientId;

    NvBool PowerStateChange;

    NvBool IsClockEnabled;

    NvBool IsClockInitDone;

    NvS32  I2sUseCount;

    // Saved I2s registers for power mode
    I2sAc97StandbyRegisters I2sStandbyRegs;

#endif //!NV_IS_AVP

} NvDdkI2s ;

static DdkI2sInfo *s_pI2sInfo = NULL;
static I2sAc97HwInterface s_I2sHwInterface;
static NvOsMutexHandle s_i2sMutex = 0;

/***************** Static function prototype Ends here *********************/

static void InitT30I2sInterface(void)
{

    s_I2sHwInterface.HwInitializeFxn    = NvDdkPrivT30I2sHwInitialize;
    s_I2sHwInterface.SetDataFlowFxn     = NvDdkPrivT30I2sSetDataFlow;
    s_I2sHwInterface.SetTriggerLevelFxn = NvDdkPrivT30I2sSetTriggerLevel;

#if !NV_IS_AVP
    s_I2sHwInterface.ResetFifoFxn = NvDdkPrivT30I2sResetFifo;
    s_I2sHwInterface.SetLoopbackTestFxn = NvDdkPrivT30I2sSetLoopbackTest;
    s_I2sHwInterface.GetClockSourceFreqFxn = NvDdkPrivI2sGetClockSourceFreq;
    s_I2sHwInterface.SetSamplingFreqFxn = NvDdkPrivT30I2sSetSamplingFreq;
    s_I2sHwInterface.SetFifoFormatFxn = NvDdkPrivT30I2sSetFifoFormat;
    s_I2sHwInterface.SetInterfacePropertyFxn =
                        NvDdkPrivT30I2sSetInterfaceProperty;
//  s_I2sHwInterface.GetInterruptSourceFxn = NvDdkPrivT30I2sGetInterruptSource;
//  s_I2sHwInterface.AckInterruptSourceFxn = NvDdkPrivT30I2sAckInterruptSource;
    s_I2sHwInterface.GetAutoAdjustFxn = NvDdkPrivI2sGetAutoAdjustFlag;
    s_I2sHwInterface.NwModeSetSlotSelectionMaskFxn =
                        NvDdkPrivT30I2sSetMaskBits;
    s_I2sHwInterface.TdmModeSetNumberOfSlotsFxn =
                        NvDdkPrivT30I2sSetNumberOfSlotsFxn;
    s_I2sHwInterface.TdmModeSetDataWordFormatFxn =
                        NvDdkPrivT30I2sSetDataWordFormatFxn;
    s_I2sHwInterface.TdmModeSetSlotSizeFxn = NvDdkPrivT30I2sSetSlotSizeFxn;
    s_I2sHwInterface.SetApbifChannelFxn    =
                        NvDdkPrivT30I2sSetApbifBaseAddress;
    s_I2sHwInterface.SetAcifRegisterFxn    = NvDdkPrivT30I2sSetAudioCif;
    s_I2sHwInterface.SetDspDataFlowFxn = NvDdkPrivT30I2sSetDataFlow;
#endif //!NV_IS_AVP

}

/**
 * Get the total number of i2s channels.
 */
static NvU32 GetTotalI2sChannel(NvRmDeviceHandle hDevice)
{
// As avp rm doesnot support these call - commenting for avp
// Enable this once rm support this call
#if !NV_IS_AVP
    return NvRmModuleGetNumInstances(hDevice, NvRmModuleID_I2s);
#else
    return MAX_I2S_CONTROLLERS;
#endif
}

/**
 * Get the i2s soc capability.
 *
 */
static NvError
I2sGetSocCapabilities(
    NvRmDeviceHandle hDevice,
    NvRmModuleID RmModuleId,
    SocI2sCapability *pI2sSocCaps)
{
    NvRmModuleID ModuleType;
    NvU32 i = 0;
    NvError  Error = NvSuccess;

    static SocI2sCapability s_SocI2sCapsList[3];
    /* FIXME for now using the same caps for both */
    NvRmModuleCapability I2sCapsList[] =
    {
        { 2, 1, 0, NvRmModulePlatform_Silicon,
            &s_SocI2sCapsList[2] },  // version 2.1 I2S in T30
    };
    SocI2sCapability *pI2sCaps = NULL;


    ModuleType = (NvRmModuleID)NVRM_MODULE_ID_MODULE(RmModuleId);
    if (ModuleType == NvRmModuleID_I2s)
    {
        for (i = 0; i < NV_ARRAY_SIZE(s_SocI2sCapsList); i++)
        {
            // First entry for the capability structure.
            s_SocI2sCapsList[i].IsValidBitsInPackedModeSupported = NV_TRUE;
            s_SocI2sCapsList[i].IsLsbAlignmentSupported = NV_FALSE;
            s_SocI2sCapsList[i].IsMsbAlignmentSupported = NV_TRUE;
            s_SocI2sCapsList[i].IsMonoFormatSupported = NV_FALSE;
            s_SocI2sCapsList[i].IsLoopbackSupported = NV_TRUE;
            s_SocI2sCapsList[i].I2sDataWordSizeInBits = 32;
            s_SocI2sCapsList[i].IsTDMModeSupported = (i>=1);
            s_SocI2sCapsList[i].IsNetworkModeSupported = (i>=1);
            s_SocI2sCapsList[i].IsAudioCIFModeSupported = (i>=2);
        }
    }
    else
    {
        for (i = 0; i < 2; i++)
        {
            // First entry for the capability structure.
            s_SocI2sCapsList[i].IsValidBitsInPackedModeSupported = NV_TRUE;
            s_SocI2sCapsList[i].IsLsbAlignmentSupported = NV_FALSE;
            s_SocI2sCapsList[i].IsMsbAlignmentSupported = NV_TRUE;
            s_SocI2sCapsList[i].IsMonoFormatSupported = NV_FALSE;
            s_SocI2sCapsList[i].IsLoopbackSupported = NV_TRUE;
            s_SocI2sCapsList[i].I2sDataWordSizeInBits = 32;
            //  FIXME:  What about TDMMode and NetworkMode?
        }
    }

    // Get the capabity from modules files.
    Error = NvRmModuleGetCapabilities(hDevice, RmModuleId, I2sCapsList,
                              NV_ARRAY_SIZE(I2sCapsList), (void **)&pI2sCaps);

    if ((Error != NvSuccess) || (pI2sCaps == NULL))
    {
        NV_ASSERT_SUCCESS(Error);
        return Error;
    }

    NvOsMemcpy(pI2sSocCaps, pI2sCaps, sizeof(*pI2sSocCaps));
    return Error;
}

static void SetI2sMode(
                NvDdkI2sHandle hI2s,
                I2sAc97Direction Direction,
                NvBool IsEnable)
{
#if !NV_IS_AVP
    switch (hI2s->I2sMode)
    {
        case NvDdkI2SMode_DSP:
            hI2s->pHwInterface->SetDspDataFlowFxn(&hI2s->I2sAc97HwRegs,
                                            Direction, IsEnable);
            break;

        case NvDdkI2SMode_Network:
        {
            NvDdkI2sNetworkDataProperty* pTempNwI2sDataProp =
                             (NvDdkI2sNetworkDataProperty*) hI2s->I2sDataProps;

            hI2s->pHwInterface->NwModeSetSlotSelectionMaskFxn(
                                &hI2s->I2sAc97HwRegs,
                                Direction,
                                (Direction & I2sAc97Direction_Receive)?
                                pTempNwI2sDataProp->ActiveRxSlotSelectionMask:
                                pTempNwI2sDataProp->ActiveTxSlotSelectionMask,
                                IsEnable);
        }
            break;
        default:
            break;
    }
#endif
}

#if !NV_IS_AVP

static void
I2sSetDmaBusWidth(
    NvDdkI2sHandle hI2s,
    NvDdkI2SDataWordValidBits DataWordFormat,
    NvU32 ValidBitsInDataWord)
{

    // 16bit packed mode for recording with Tight bitsPerLRCLK and
    // dsp mono has HW issue.

    // SW WAR - Set the 16bit stereo as 16 bit fifo format
    // dma width in 16 bit and Attn level double the
    // dma burst size.

    // For mono dsp mode - Attn level must be same as
    // dma burst size and dma width as 16bit.

    // Default to Attn 4 as Dma burst size is 4.
    // Bus width in 32bit
    NvU32 I2sAttnLvl = I2sTriggerlvl_4;
    hI2s->DmaWidth =  I2sDma_Width_32;

    // Check DatabitPersLRCLK and DataSize if needed
    if ((DataWordFormat == NvDdkI2SDataWordValidBits_StartFromLsb)
         && (ValidBitsInDataWord == 16))
    {
        hI2s->DmaWidth = I2sDma_Width_16;
        if ((hI2s->I2sMode == NvDdkI2SMode_PCM) ||
             (hI2s->I2sMode == NvDdkI2SMode_DSP)||
             (hI2s->I2sMode == NvDdkI2SMode_Network))
        {
            // Only Stereo format in I2s mode.
            // Attn must be double if dma width is 16bit
            I2sAttnLvl = I2sTriggerlvl_8;
        }
    }
    // Set the trigger Lvl for both play and record here
    hI2s->pHwInterface->SetTriggerLevelFxn(&hI2s->I2sAc97HwRegs,
                            I2sAc97Direction_Transmit, I2sAttnLvl);

    hI2s->pHwInterface->SetTriggerLevelFxn(&hI2s->I2sAc97HwRegs,
                             I2sAc97Direction_Receive, I2sAttnLvl);

    NVDDK_I2S_POWERLOG(("I2sSetDmaBusWidth 0x%x AttnLvl 0x%x \n", hI2s->DmaWidth, I2sAttnLvl));
}

static void SetInterfaceProperty(NvDdkI2sHandle hDdkI2s)
{
    hDdkI2s->pHwInterface->SetInterfacePropertyFxn(&hDdkI2s->I2sAc97HwRegs,
                            (void *)&hDdkI2s->I2sInterfaceProps);
}

static NvError EnableI2sPower(NvDdkI2sHandle hI2s)
{
    NvError Error = NvSuccess;

    NVDDK_I2S_POWERLOG(("EnableI2sPower IsClockEnabled 0x%x \n", hI2s->IsClockEnabled));
    if (hI2s->IsClockEnabled)
        return Error;

    // Enable power
    Error = NvRmPowerVoltageControl(hI2s->hDevice, hI2s->RmModuleId,
                        hI2s->RmPowerClientId,
                        NvRmVoltsUnspecified, NvRmVoltsUnspecified,
                        NULL, 0, NULL);

    // Enable the clock.
    if (!Error)
    {
        Error = NvRmPowerModuleClockControl(hI2s->hDevice, hI2s->RmModuleId,
                    hI2s->RmPowerClientId,  NV_TRUE);
    }

    if (!Error)
        hI2s->IsClockEnabled   = NV_TRUE;
    NVDDK_I2S_POWERLOG(("EnableI2sPower Status 0x%x \n", Error));
    return Error;
}

static NvError DisableI2sPower(NvDdkI2sHandle hI2s)
{
    NvError Error = NvSuccess;

    NVDDK_I2S_POWERLOG(("DisableI2sPower IsClockEnabled 0x%x \n", hI2s->IsClockEnabled));
    if (!hI2s->IsClockEnabled)
        return Error;

    // Disable the clock
    Error = NvRmPowerModuleClockControl(hI2s->hDevice, hI2s->RmModuleId,
                                    hI2s->RmPowerClientId, NV_FALSE);

    // Disable power
    if (!Error)
        Error = NvRmPowerVoltageControl(hI2s->hDevice, hI2s->RmModuleId,
                    hI2s->RmPowerClientId, NvRmVoltsOff, NvRmVoltsOff,
                    NULL, 0, NULL);

    if (!Error)
        hI2s->IsClockEnabled   = NV_FALSE;
    NVDDK_I2S_POWERLOG(("DisableI2sPower Status 0x%x \n", Error));
    return Error;
}

#if (0)

static void HandleI2sRxInterrupt(NvDdkI2sHandle hI2s)
{
    hI2s->pHwInterface->AckInterruptSourceFxn(&hI2s->I2sAc97HwRegs,
                            I2sHwInterruptSource_Receive);
}

static void HandleI2sTxInterrupt(NvDdkI2sHandle hI2s)
{
    hI2s->pHwInterface->AckInterruptSourceFxn(&hI2s->I2sAc97HwRegs,
                            I2sHwInterruptSource_Transmit);
}

/**
 * Handle the i2s interrupt.
 */
static void HandleI2sInterrupt(void *args)
{
    NvDdkI2sHandle hI2s = NULL;
    I2sHwInterruptSource InterruptReason;

    hI2s = (NvDdkI2sHandle)args;
    if (hI2s == NULL)
    {
        return;
    }

    // The interrupt status is provided from high priority till no pending
    // interrupt so do in the loop to handle all interrupts.
    while (1)
    {
        // Get the interrupt reason for this i2s channel.
         InterruptReason =
            hI2s->pHwInterface->GetInterruptSourceFxn(&hI2s->I2sAc97HwRegs);

        // There is a valid interrupt reason so call appropriate function.
        switch(InterruptReason)
        {
            case I2sHwInterruptSource_Receive:
                // Receive interrupt. Call receive int handler.
                HandleI2sRxInterrupt(hI2s);
                break;

            case I2sHwInterruptSource_Transmit:
                // Transmit interrupt. Call transmit interrupt handler.
                HandleI2sTxInterrupt(hI2s);
                break;

            default:
                break;
        }
        if (InterruptReason == I2sHwInterruptSource_None)
        {
            break;
        }
    }

    NvRmInterruptDone(hI2s->InterruptHandle);
    return;
}

static NvError RegisterI2sAc97Interrupt(
        NvRmDeviceHandle hDevice,
        NvDdkI2sHandle hDdkI2s)
{
    NvError Error;
    NvU32 IrqList;
    NvOsInterruptHandler IntHandlers;

    if (hDdkI2s->InterruptHandle)
    {
        return NvSuccess;
    }
    IrqList =
        NvRmGetIrqForLogicalInterrupt(hDevice, hDdkI2s->RmModuleId, 0);
    IntHandlers = HandleI2sInterrupt;

    Error = NvRmInterruptRegister(hDevice, 1, &IrqList, &IntHandlers, hDdkI2s,
            &hDdkI2s->InterruptHandle, NV_TRUE);
    return Error;
}

#endif

// Set the dsp mode registers
static NvError I2sConfigureDspMode(
                        NvDdkI2sHandle hI2s,
                        NvDdkI2SMode I2sMode,
                        NvDdkI2sDspDataProperty* pDspProp)
{
    return NvSuccess;
}

// Configure the I2s Clock accordingly
static
NvError ConfigureI2sClock(
        NvDdkI2sHandle hI2s,
        NvU32 NewSampleRate,
        NvU32 DatabitsPerLRCLK)
{
    NvError Error = NvSuccess;
    NvU32 PrefClockSource;
    NvU32 ClockFreqReqd = 0;
    NvU32 ConfiguredClockFreq = 0;
    NvBool ClockFlag = NvRmClockConfig_InternalClockForCore;
    NvDdkI2sPcmDataProperty *pTempPcmI2sDataProp;
    NvDdkI2sDspDataProperty *pTempDspI2sDataProp;
    NvDdkI2sNetworkDataProperty *pTempNwI2sDataProp;
    NvDdkI2sTdmDataProperty *pTempTdmI2sDataProp;

    {
        // Set the sampling rate only when it is master mode
        if (hI2s->I2sInterfaceProps.Mode != NvOdmQueryI2sMode_Master)
        {
            // As POR value for i2s is Master. Calling the RmModuleClockConfig
            // with External Clock and Preferred Clock as OScillator clock to
            // set the i2s as Slave.
            ClockFreqReqd    = NvRmPowerGetPrimaryFrequency(hI2s->hDevice);
            PrefClockSource  = ClockFreqReqd;
            Error = NvRmPowerModuleClockConfig(hI2s->hDevice,
                                      hI2s->RmModuleId, 0, ClockFreqReqd,
                                      NvRmFreqUnspecified, &PrefClockSource,
                                      1, &ConfiguredClockFreq,
                                      NvRmClockConfig_ExternalClockForCore);
            if (!Error)
            {
                switch (hI2s->I2sMode)
                {
                    case NvDdkI2SMode_PCM:
                        pTempPcmI2sDataProp =
                            (NvDdkI2sPcmDataProperty*) hI2s->I2sDataProps;
                        pTempPcmI2sDataProp->SamplingRate = NewSampleRate;
                        break;
                    case NvDdkI2SMode_DSP:
                        pTempDspI2sDataProp =
                            (NvDdkI2sDspDataProperty*) hI2s->I2sDataProps;
                        pTempDspI2sDataProp->SamplingRate = NewSampleRate;
                        break;
                    case NvDdkI2SMode_Network:
                        if (hI2s->pI2sInfo->SocI2sCaps.IsNetworkModeSupported)
                        {
                            pTempNwI2sDataProp =
                             (NvDdkI2sNetworkDataProperty*) hI2s->I2sDataProps;
                            pTempNwI2sDataProp->SamplingRate = NewSampleRate;
                        }
                        else
                        {
                            return NvError_NotSupported;
                        }
                        break;
                    case NvDdkI2SMode_TDM:
                        if (hI2s->pI2sInfo->SocI2sCaps.IsTDMModeSupported)
                        {
                            pTempTdmI2sDataProp =
                             (NvDdkI2sTdmDataProperty*) hI2s->I2sDataProps;
                            pTempTdmI2sDataProp->SamplingRate = NewSampleRate;
                        }
                        else
                        {
                            return NvError_NotSupported;
                        }
                        break;
                    default:
                        NV_ASSERT(!"I2S Mode Unsupported\n");
                }
            }
            return NvSuccess;
        }

        ClockFlag     = NvRmClockConfig_InternalClockForCore;

        // Check whether the codec need fixed MCLK and on what Samplerate
        if (hI2s->I2sInterfaceProps.IsFixedMCLK && !hI2s->IsClockInitDone)
        {
            ClockFreqReqd = hI2s->I2sInterfaceProps.FixedMCLKFrequency;
            ClockFlag |= NvRmClockConfig_AudioAdjust;
            // Calling the ClockConfig with Fixed Frequency value.
            // Got the clock frequency for i2s source so configured it.
            PrefClockSource = ClockFreqReqd;
            Error = NvRmPowerModuleClockConfig(hI2s->hDevice,
                                      hI2s->RmModuleId, 0, ClockFreqReqd,
                                      NvRmFreqUnspecified, &PrefClockSource,
                                      1, &ConfiguredClockFreq,
                                      ClockFlag);
            if (!Error)
                hI2s->IsClockInitDone = NV_TRUE;
        }

        ClockFlag     = NvRmClockConfig_InternalClockForCore;
        ClockFreqReqd =
            hI2s->pHwInterface->GetClockSourceFreqFxn(NewSampleRate,
                                         DatabitsPerLRCLK);
        if (hI2s->pHwInterface->GetAutoAdjustFxn(NewSampleRate)
                        && !hI2s->I2sInterfaceProps.IsFixedMCLK)
            ClockFlag |= NvRmClockConfig_AudioAdjust;

        // Got the clock frequency for i2s source so configured it.
        PrefClockSource = ClockFreqReqd;
        Error = NvRmPowerModuleClockConfig(hI2s->hDevice,
                                      hI2s->RmModuleId, 0, ClockFreqReqd,
                                      NvRmFreqUnspecified, &PrefClockSource,
                                      1, &ConfiguredClockFreq,
                                      ClockFlag);

    }

    // If no error then check for configured clock frequency and verify.
    if (!Error)
    {
        // Now set the timing register based on the new clock source
        // and sampling rate.
        // check the i2s Interface mode - as DSP or other mode to calculate
        // the Databits accordingly
        if (hI2s->I2sInterfaceProps.I2sDataCommunicationFormat ==
                                            NvOdmQueryI2sDataCommFormat_Dsp)
        {
            // Databits = RIGHT + LEFT
            DatabitsPerLRCLK = DatabitsPerLRCLK * 2;
        }
        hI2s->pHwInterface->SetSamplingFreqFxn(&hI2s->I2sAc97HwRegs,
                                                NewSampleRate,
                                                DatabitsPerLRCLK,
                                                ConfiguredClockFreq);
        hI2s->SamplingRate = NewSampleRate;
        hI2s->DatabitsPerLRCLK = DatabitsPerLRCLK;
        NVDDK_I2S_REGLOG((" Setting I2s Samplerate as %d \n", NewSampleRate));
    }

    return Error;
}

/**
 * Get the i2s capabilies at ddk level.
 */
NvError
NvDdkI2sGetCapabilities(
    NvRmDeviceHandle hDevice,
    NvU32 InstanceId,
    NvDdkI2sDriverCapability * const pI2sDriverCaps)
{
    NvError Error = NvSuccess;
    SocI2sCapability SocI2sCaps;
    NvU32 TotalChannel = 0;
    NvRmModuleID RmModuleId;
    NvU32 ModInstId = (InstanceId & INSTANCE_MASK);

    // If null pointer for the i2s driver Caps then return error.
    NV_ASSERT(hDevice);
    NV_ASSERT(pI2sDriverCaps);

    // Get the soc i2s capabilities.
    TotalChannel = GetTotalI2sChannel(hDevice);

    if (ModInstId >= TotalChannel)
        return NvError_NotSupported;


    RmModuleId = NVRM_MODULE_ID(NvRmModuleID_I2s, ModInstId);

    Error = I2sGetSocCapabilities(hDevice, RmModuleId, &SocI2sCaps);
    if (!Error)
    {
        // Set the required parameter first.
        pI2sDriverCaps->I2sDataWordSizeInBits =
                                   SocI2sCaps.I2sDataWordSizeInBits;
        pI2sDriverCaps->IsValidBitsInPackedModeSupported =
                                   SocI2sCaps.IsValidBitsInPackedModeSupported;

        pI2sDriverCaps->IsValidBitsStartsFromLsbSupported =
                                    SocI2sCaps.IsLsbAlignmentSupported;

        pI2sDriverCaps->IsValidBitsStartsFromMsbSupported =
                                    SocI2sCaps.IsMsbAlignmentSupported;

        pI2sDriverCaps->IsMonoDataFormatSupported =
                                            SocI2sCaps.IsMonoFormatSupported;

        pI2sDriverCaps->IsLoopbackSupported = SocI2sCaps.IsLoopbackSupported;

    }

    return Error;
}

#endif //!NV_IS_AVP


/**
 * Destroy all the i2s information. It free all the allocated resource.
 * Thread safety: Caller responsibity.
 */
static void DeInitI2sInformation(void)
{
    DdkI2sInfo* local = s_pI2sInfo;
    if (!local->OpenCount)
    {
        // Free all allocations.
        local->hOsMutex4I2sInfoAccess = 0;
        NvOsMutexUnlock(s_i2sMutex);
        NvOsMutexDestroy(s_i2sMutex);
        NvOsFree(local);
        s_i2sMutex = 0;
        s_pI2sInfo = NULL;
    }
    else
    {
        NvOsMutexUnlock(local->hOsMutex4I2sInfoAccess);
    }
}

/**
 * Initialize the i2s information.
 * Thread safety: Caller responsibity.
 */
static NvError InitI2sInformation(
                   NvRmDeviceHandle hDevice,
                   NvU32 InstanceId)
{
    DdkI2sInfo *pI2sInfo;
    NvRmModuleID RmModuleId;
    NvError Error = NvSuccess;

    NV_ASSERT(GetTotalI2sChannel(hDevice) <= MAX_I2S_CONTROLLERS);

    // Allocate the memory for the i2s information.
    pI2sInfo = NvOsAlloc(sizeof(*pI2sInfo));
    if (!pI2sInfo)
    {
        return NvError_InsufficientMemory;
    }

    NvOsMemset(pI2sInfo, 0, sizeof(*pI2sInfo));

    // Initialize all the parameters.
    pI2sInfo->hDevice = hDevice;
    pI2sInfo->TotalI2sChannel = GetTotalI2sChannel(hDevice);

    // Check the SOC capability first and set the Interface accordingly.
    RmModuleId =
            NVRM_MODULE_ID(NvRmModuleID_I2s, (InstanceId & INSTANCE_MASK));

    Error = I2sGetSocCapabilities(hDevice, RmModuleId, &pI2sInfo->SocI2sCaps);
    if (Error)
    {
        NvOsFree(pI2sInfo);
        return Error;
    }

    if (pI2sInfo->SocI2sCaps.IsAudioCIFModeSupported)
    {
        InitT30I2sInterface();
    }

    s_pI2sInfo = pI2sInfo;
    return NvSuccess;
}

/**
 * Destroy the handle of i2s channel and free all the allocation done for it.
 * Thread safety: Caller responsibity.
 */
static void DestroyChannelHandle(NvDdkI2sHandle hI2s)
{
    // If null pointer for i2s handle then return error.
    if (hI2s == NULL)
        return;

#if !NV_IS_AVP
    //if (hI2s->InterruptHandle)
    //{
    //    NvRmInterruptUnregister(hI2s->hDevice, hI2s->InterruptHandle);
    //    hI2s->InterruptHandle = NULL;
    //}
#endif //!NV_IS_AVP

    // Unmap the virtual mapping of the i2s hw register.
    if (hI2s->I2sAc97HwRegs.pRegsVirtBaseAdd)
    {
        NvOsPhysicalMemUnmap(hI2s->I2sAc97HwRegs.pRegsVirtBaseAdd,
                             hI2s->I2sAc97HwRegs.BankSize);
    }

#if !NV_IS_AVP
    // Disable the clocks.
    NV_ASSERT_SUCCESS(DisableI2sPower(hI2s));

    // Unregister for the power manager.
    if (hI2s->RmPowerClientId)
        NvRmPowerUnRegister(hI2s->hDevice, hI2s->RmPowerClientId);

    if (hI2s->hPowerMgmntSema)
        NvOsSemaphoreDestroy(hI2s->hPowerMgmntSema);

    if (hI2s->I2sDataProps)
        NvOsFree(hI2s->I2sDataProps);

#endif //!NV_IS_AVP

    // Release the read dma.
    if (hI2s->hRxRmDma)
    {
        NvRmDmaAbort(hI2s->hRxRmDma);
        NvRmDmaFree(hI2s->hRxRmDma);
    }

    // Release the write dma.
    if (hI2s->hTxRmDma)
    {
        NvRmDmaAbort(hI2s->hTxRmDma);
        NvRmDmaFree(hI2s->hTxRmDma);
    }

    // Destroy the mutex allocated for the channel accss.
    if (hI2s->MutexForChannelAccess)
        NvOsMutexDestroy(hI2s->MutexForChannelAccess);

    // Destroy the sync sempahores.
    if (hI2s->RxSynchSema)
        NvOsSemaphoreDestroy(hI2s->RxSynchSema);
    if (hI2s->TxSynchSema)
        NvOsSemaphoreDestroy(hI2s->TxSynchSema);

    // Free the memory of the i2s handles.
    NvOsFree(hI2s);
}
/**
 * Create the handle for the i2s channel.
 * Thread safety: Caller responsibity.
 */
static NvError CreateChannelHandle(
    NvRmDeviceHandle hDevice,
    NvU32 InstanceId,
    DdkI2sInfo *pI2sInfo,
    NvDdkI2sHandle *phI2s)
{
    NvError Error = NvSuccess;
    NvDdkI2s *pI2sChannel = NULL;
#if !NV_IS_AVP
    NvOdmQueryI2sInterfaceProperty *pI2sInterfaceProps;
#endif

    *phI2s = NULL;


    // Allcoate the memory for the i2s handle.
    pI2sChannel = NvOsAlloc(sizeof(*pI2sChannel));

    // If allocation fails then return error.
    if (!pI2sChannel)
        return NvError_InsufficientMemory;

    // Reset the memory allocated for the i2s handle.
    NvOsMemset(pI2sChannel,0,sizeof(*pI2sChannel));

    // Allocate the Data property as the max size
    // set for the max size of PropertySet
    pI2sChannel->I2sDataProps = NvOsAlloc(sizeof(NvDdkI2sDspDataProperty));
    if (!pI2sChannel->I2sDataProps)
    {
        NvOsFree(pI2sChannel);
        return NvError_InsufficientMemory;
    }

    NvOsMemset(pI2sChannel->I2sDataProps, 0, sizeof(NvDdkI2sDspDataProperty));

    // Set the i2s handle parameters.
    pI2sChannel->hDevice = hDevice;
    pI2sChannel->InstanceId = (InstanceId & INSTANCE_MASK);
    pI2sChannel->OpenCount= 0;
    pI2sChannel->DmaWidth = 0;

    pI2sChannel->RxSynchSema = NULL;
    pI2sChannel->TxSynchSema = NULL;

    pI2sChannel->hRxRmDma = NULL;
    pI2sChannel->hTxRmDma = NULL;
    pI2sChannel->MutexForChannelAccess = NULL;
    pI2sChannel->pHwInterface = NULL;

#if !NV_IS_AVP
    pI2sChannel->pI2sInfo = pI2sInfo;
    pI2sChannel->hPowerMgmntSema = NULL;
    pI2sChannel->RmPowerClientId = 0;
#endif //!NV_IS_AVP

    pI2sChannel->RmModuleId =
            NVRM_MODULE_ID(NvRmModuleID_I2s, pI2sChannel->InstanceId);

    pI2sChannel->RmDmaModuleId = NvRmDmaModuleID_I2s;

    pI2sChannel->pHwInterface = &s_I2sHwInterface;

    pI2sChannel->pHwInterface->HwInitializeFxn(pI2sChannel->InstanceId,
            &pI2sChannel->I2sAc97HwRegs);

#if !NV_IS_AVP

    Error =
        NvDdkI2sGetCapabilities(hDevice, InstanceId, &pI2sChannel->DdkI2sCaps);

    // If error the return now.
    if (Error)
    {
        NvOsFree(pI2sChannel);
        NvOsFree(pI2sChannel->I2sDataProps);
        return Error;
    }

    pI2sInterfaceProps =
        (NvOdmQueryI2sInterfaceProperty *)
        NvOdmQueryI2sGetInterfaceProperty(pI2sChannel->InstanceId);

    if(pI2sInterfaceProps)
    {
        pI2sChannel->I2sMode = NvDdkI2SMode_PCM; // set default mode to PCM
        NvDdkI2sGetDataProperty(*phI2s, pI2sChannel->I2sMode,
                                    pI2sChannel->I2sDataProps);
        NvOsMemcpy(&pI2sChannel->I2sInterfaceProps, pI2sInterfaceProps,
                                     sizeof(NvOdmQueryI2sInterfaceProperty));
    }
    else
        Error = NvError_NotSupported;

    if (Error)
        goto ChannelExit;

#endif //!NV_IS_AVP

    // Create the mutex for channel access.
    Error = NvOsMutexCreate(&pI2sChannel->MutexForChannelAccess);
    if (Error)
        goto ChannelExit;

    // Create the synchrnous semaphores.
    Error = NvOsSemaphoreCreate(&pI2sChannel->RxSynchSema, 0);
    if (Error)
        goto ChannelExit;

    Error = NvOsSemaphoreCreate(&pI2sChannel->TxSynchSema, 0);
    if (Error)
        goto ChannelExit;

    // Get the i2s hw physical base address and map in virtual memory space.
    NvRmModuleGetBaseAddress(hDevice, pI2sChannel->RmModuleId,
        &pI2sChannel->I2sAc97HwRegs.RegsPhyBaseAdd,
        &pI2sChannel->I2sAc97HwRegs.BankSize);

    pI2sChannel->I2sAc97HwRegs.RxFifoAddress +=
                            pI2sChannel->I2sAc97HwRegs.RegsPhyBaseAdd;
    pI2sChannel->I2sAc97HwRegs.TxFifoAddress +=
                            pI2sChannel->I2sAc97HwRegs.RegsPhyBaseAdd;
#if !NV_IS_AVP
    pI2sChannel->I2sAc97HwRegs.RxFifo2Address +=
                            pI2sChannel->I2sAc97HwRegs.RegsPhyBaseAdd;
    pI2sChannel->I2sAc97HwRegs.TxFifo2Address +=
                            pI2sChannel->I2sAc97HwRegs.RegsPhyBaseAdd;
#endif //!NV_IS_AVP

//  NvOsDebugPrintf(" Avp base for Tx fifo 0x%x \n",
//                        pI2sChannel->I2sAc97HwRegs.TxFifoAddress);

    Error = NvRmPhysicalMemMap(
                pI2sChannel->I2sAc97HwRegs.RegsPhyBaseAdd,
                pI2sChannel->I2sAc97HwRegs.BankSize, NVOS_MEM_READ_WRITE,
                NvOsMemAttribute_Uncached,
                (void **)&pI2sChannel->I2sAc97HwRegs.pRegsVirtBaseAdd);

    if (Error)
        goto ChannelExit;

#if !NV_IS_AVP
    // Register as the Rm power client
    Error = NvOsSemaphoreCreate(&pI2sChannel->hPowerMgmntSema, 0);
    if (Error)
        goto ChannelExit;

    pI2sChannel->RmPowerClientId = NVRM_POWER_CLIENT_TAG('I','2','S','*');
    Error = NvRmPowerRegister(pI2sChannel->hDevice,
                                pI2sChannel->hPowerMgmntSema,
                                &pI2sChannel->RmPowerClientId);
    if (Error)
        goto ChannelExit;

    // Enable power
    EnableI2sPower(pI2sChannel);

    // Reset the module.
    NvRmModuleReset(hDevice, pI2sChannel->RmModuleId);


    SetInterfaceProperty(pI2sChannel);

    pI2sChannel->I2sUseCount++;

#endif //!NV_IS_AVP

    *phI2s = pI2sChannel;

    return Error;

ChannelExit:
    // If error then destroy all the allocation done here.
    DestroyChannelHandle(pI2sChannel);
    return Error;
}


/**
 * Open the i2s handle.
 */
NvError
NvDdkI2sOpen(
    NvRmDeviceHandle hDevice,
    NvU32 InstanceId,
    NvDdkI2sHandle *phI2s)
{
    NvError Error = NvSuccess;
    DdkI2sInfo *pI2sInfo = NULL;
    NvDdkI2s *pI2sChannel = NULL;
    NvU32 TotalChannel =0 ;
    NvU32 ModInstId = (InstanceId & INSTANCE_MASK);

    NV_ASSERT(phI2s);
    NV_ASSERT(hDevice);

    *phI2s = NULL;

    NVDDK_I2S_POWERLOG(("NvDdkI2sOpen++ 0x%x \n", s_pI2sInfo));
    if (!s_pI2sInfo)
    {
        Error = NvOsMutexCreate(&s_i2sMutex);
        if (Error != NvSuccess)
        {
            return Error;
        }
    }


    NvOsMutexLock(s_i2sMutex);
    pI2sInfo = s_pI2sInfo;
    if (!pI2sInfo)
    {
        Error = InitI2sInformation(hDevice, InstanceId);
        if (Error)
        {
            NvOsMutexUnlock(s_i2sMutex);
            goto fail;
        }

        pI2sInfo = s_pI2sInfo;
        pI2sInfo->hOsMutex4I2sInfoAccess = s_i2sMutex;
    }

    TotalChannel =  pI2sInfo->TotalI2sChannel;

    // Validate the channel Id parameter.
    if (InstanceId >= TotalChannel)
    {
        DeInitI2sInformation();
        Error = NvError_NotSupported;
        return Error;
    }

    // Check for the open i2s handle to find out whether same instance port
    // name already exists or not.
    // Get the head pointer of i2s channel
    pI2sChannel = pI2sInfo->hI2sChannelList[ModInstId];
    NVDDK_I2S_POWERLOG(("NvDdkI2sOpen channel 0x%x TotalChannel %d Module %d\n", pI2sChannel, TotalChannel, ModInstId));
    // If the i2s handle does not exist then cretae it.
    if (!pI2sChannel)
    {
        Error =
            CreateChannelHandle(hDevice, InstanceId, pI2sInfo, &pI2sChannel);
        if(Error)
        {
            DeInitI2sInformation();
            return Error;
        }

        pI2sInfo->hI2sChannelList[ModInstId] = pI2sChannel;
        pI2sChannel->pI2sInfo = pI2sInfo;
    }

    // Increase the open count.
    pI2sChannel->OpenCount++;
    *phI2s = pI2sChannel;

    pI2sInfo->OpenCount++;
    NvOsMutexUnlock(s_i2sMutex);

    return NvSuccess;
fail:
    if (s_i2sMutex)
    {
        NvOsMutexDestroy(s_i2sMutex);
        s_i2sMutex = 0;
    }
    return Error;
}

/**
 * Close the i2s handle.
 */
void NvDdkI2sClose(NvDdkI2sHandle hI2s)
{
    DdkI2sInfo *pI2sInfo = NULL;

    // if null parametre then do nothing.
    if (!hI2s)
        return;

    if (!hI2s->pI2sInfo)
        return;

    pI2sInfo = hI2s->pI2sInfo;

    // Lock the i2s information access.
    NvOsMutexLock(pI2sInfo->hOsMutex4I2sInfoAccess);

    // decremenr the open count and it becomes 0 then release all the
    // allocation done for this handle.
    hI2s->OpenCount--;
    NVDDK_I2S_POWERLOG(("NvDdkI2sClose 0x%x InstId 0x%x \n", hI2s->OpenCount, hI2s->InstanceId));
    // If the open count become zero then remove from the list of handles and
    // free..
    if (!hI2s->OpenCount)
    {
        hI2s->pI2sInfo->hI2sChannelList[hI2s->InstanceId] = NULL;

        // Now destroy the handles.
        DestroyChannelHandle(hI2s);
    }

    pI2sInfo->OpenCount--;
    DeInitI2sInformation();
}


/**
 * Transmit the data from i2s channel.
 * Thread safety: Provided in the function.
 */
NvError
NvDdkI2sWrite(
    NvDdkI2sHandle hI2s,
    NvRmPhysAddr TransmitBufferPhyAddress,
    NvU32 *pBytesWritten,
    NvU32 WaitTimeoutInMilliSeconds,
    NvOsSemaphoreHandle AsynchSemaphoreId)
{
    NvError Error = NvSuccess;
    NvRmDmaClientBuffer DmaClientBuff;

    NV_ASSERT(hI2s);
    NV_ASSERT(pBytesWritten);

    // Allocate the dma if it is not done yet.
    if (!hI2s->hTxRmDma)
    {
        NvU32 DmaInstanceId = hI2s->InstanceId;
        Error = NvRmDmaAllocate(hI2s->hDevice, &hI2s->hTxRmDma,
                        NV_FALSE, NvRmDmaPriority_High, hI2s->RmDmaModuleId,
                        DmaInstanceId);
        if (Error)
        {
            return Error;
        }

#if NV_IS_AVP
        hI2s->pHwInterface->SetTriggerLevelFxn(&hI2s->I2sAc97HwRegs,
                            I2sAc97Direction_Transmit, I2sTriggerlvl_4);
        hI2s->DmaWidth = I2sDma_Width_32;
#endif
    }

    // check the i2s mode and set the mode bit properly
    SetI2sMode(hI2s, I2sAc97Direction_Transmit, NV_TRUE);
    hI2s->pHwInterface->SetDataFlowFxn(&hI2s->I2sAc97HwRegs,
                                I2sAc97Direction_Transmit, NV_TRUE);

    // !!!!!! FIXME !!!!!!!!
    // Immediate hack added to make the i2s working
    // Will be removed once a clean solution is implemented
    // to figure the channel being used across cpu and avp code
    // In addition having static i2s library is also causing issue.
    hI2s->I2sAc97HwRegs.RxFifoAddress = 0x70080010;
    hI2s->I2sAc97HwRegs.TxFifoAddress = 0x7008000c;

    // Now do the dma transfer
    DmaClientBuff.SourceBufferPhyAddress = TransmitBufferPhyAddress;
    DmaClientBuff.SourceAddressWrapSize = 0;
    DmaClientBuff.DestinationBufferPhyAddress =
                                    hI2s->I2sAc97HwRegs.TxFifoAddress;
    DmaClientBuff.DestinationAddressWrapSize = ADDRESS_WRAP(hI2s->DmaWidth, 4);
    DmaClientBuff.TransferSize = *pBytesWritten;

    if (!WaitTimeoutInMilliSeconds)
    {
        Error = NvRmDmaStartDmaTransfer(hI2s->hTxRmDma, &DmaClientBuff,
                    NvRmDmaDirection_Forward, 0, AsynchSemaphoreId);
        return Error;
    }

    Error = NvRmDmaStartDmaTransfer(hI2s->hTxRmDma, &DmaClientBuff,
                NvRmDmaDirection_Forward, 0, hI2s->TxSynchSema);

    if (!Error)
    {
        Error = NvOsSemaphoreWaitTimeout(hI2s->TxSynchSema,
                                                WaitTimeoutInMilliSeconds);
        if (Error == NvError_Timeout)
        {
            NvRmDmaAbort(hI2s->hTxRmDma);
        }
    }

    hI2s->pHwInterface->SetDataFlowFxn(&hI2s->I2sAc97HwRegs,
                                I2sAc97Direction_Transmit, NV_FALSE);
    return Error;
}

/**
 * Stop the write opeartion.
 */
void NvDdkI2sWriteAbort(NvDdkI2sHandle hI2s)
{
    // Required parameter validation
    if (hI2s && hI2s->hTxRmDma)
    {
        NvRmDmaAbort(hI2s->hTxRmDma);
        hI2s->pHwInterface->SetDataFlowFxn(&hI2s->I2sAc97HwRegs,
                                        I2sAc97Direction_Transmit, NV_FALSE);
    }

}


/**
 * Read the data from com channel.
 * Thread safety: Provided in the function.
 */
NvError
NvDdkI2sRead(
    NvDdkI2sHandle hI2s,
    NvRmPhysAddr ReceiveBufferPhyAddress,
    NvU32 *pBytesRead,
    NvU32 WaitTimeoutInMilliSeconds,
    NvOsSemaphoreHandle AsynchSemaphoreId)
{
    NvError Error = NvSuccess;
    NvRmDmaClientBuffer DmaClientBuff;

    NV_ASSERT(hI2s);
    NV_ASSERT(pBytesRead);

    // Allocate the dma if it is not done yet.
    if (hI2s->hRxRmDma == NULL)
    {
        NvU32 DmaInstanceId = hI2s->InstanceId;
        // check for h.w version
        if (hI2s->pI2sInfo->SocI2sCaps.IsAudioCIFModeSupported)
        {
            DmaInstanceId =
                hI2s->I2sAc97HwRegs.ApbifRecChannelInfo.ChannelIndex;
        }

        Error = NvRmDmaAllocate(hI2s->hDevice, &hI2s->hRxRmDma,
                        NV_FALSE, NvRmDmaPriority_High, hI2s->RmDmaModuleId,
                        DmaInstanceId);
        if (Error)
        {
            return Error;
        }

#if NV_IS_AVP
        hI2s->pHwInterface->SetTriggerLevelFxn(&hI2s->I2sAc97HwRegs,
                               I2sAc97Direction_Receive, I2sTriggerlvl_4);
        hI2s->DmaWidth = I2sDma_Width_32;
#endif

    }

    // !!!!!! FIXME !!!!!!!!
    // Immediate hack added to make the i2s working
    // Will be removed once a clean solution is implemented
    // to figure the channel being used across cpu and avp code
    // In addition having static i2s library is also causing issue.
    hI2s->I2sAc97HwRegs.RxFifoAddress = 0x70080010;
    hI2s->I2sAc97HwRegs.TxFifoAddress = 0x7008000c;

    SetI2sMode(hI2s, I2sAc97Direction_Receive, NV_TRUE);
    hI2s->pHwInterface->SetDataFlowFxn(&hI2s->I2sAc97HwRegs,
                               I2sAc97Direction_Receive, NV_TRUE);

    // Now do the dma transfer
    DmaClientBuff.SourceBufferPhyAddress = ReceiveBufferPhyAddress;
    DmaClientBuff.SourceAddressWrapSize = 0;
    DmaClientBuff.DestinationBufferPhyAddress =
                                 hI2s->I2sAc97HwRegs.RxFifoAddress;
    DmaClientBuff.DestinationAddressWrapSize = ADDRESS_WRAP(hI2s->DmaWidth, 4);
    DmaClientBuff.TransferSize = *pBytesRead;

    if (!WaitTimeoutInMilliSeconds)
    {
        Error = NvRmDmaStartDmaTransfer(hI2s->hRxRmDma, &DmaClientBuff,
                    NvRmDmaDirection_Reverse, 0, AsynchSemaphoreId);
        return Error;
    }

    Error = NvRmDmaStartDmaTransfer(hI2s->hRxRmDma, &DmaClientBuff,
                NvRmDmaDirection_Reverse, 0, hI2s->RxSynchSema);

    if (!Error)
    {
        Error = NvOsSemaphoreWaitTimeout(hI2s->RxSynchSema,
                                                WaitTimeoutInMilliSeconds);
        if (Error == NvError_Timeout)
        {
            NvRmDmaAbort(hI2s->hRxRmDma);
        }
    }

    hI2s->pHwInterface->SetDataFlowFxn(&hI2s->I2sAc97HwRegs,
                                       I2sAc97Direction_Receive, NV_FALSE);
    return Error;
}

/**
 * Stop the Read opeartion.
 */
void NvDdkI2sReadAbort(NvDdkI2sHandle hI2s)
{
    // Required parameter validation
    if (hI2s && hI2s->hRxRmDma)
    {
        NvRmDmaAbort(hI2s->hRxRmDma);
        hI2s->pHwInterface->SetDataFlowFxn(&hI2s->I2sAc97HwRegs,
                                    I2sAc97Direction_Receive, NV_FALSE);
    }
}

/**
* Obtain the current DMA transfer Count
*/
NvError NvDdkI2sGetTransferCount(NvDdkI2sHandle hI2s, NvBool IsWrite, NvU32 *pTransferCount)
{
    NvError status = NvError_NotSupported;
    if (IsWrite && hI2s->hTxRmDma)
    {
        status = NvRmDmaGetTransferredCount(hI2s->hTxRmDma, pTransferCount, NV_FALSE);
    }
    else if (!IsWrite && hI2s->hRxRmDma)
    {
        status = NvRmDmaGetTransferredCount(hI2s->hRxRmDma, pTransferCount, NV_FALSE);
    }
    return status;
}

#if !NV_IS_AVP

/**
* Obtain the DdkI2sHandle based on InstanceID
*/
NvError NvDdkI2sGetHandle(NvDdkI2sHandle *phI2s, NvU32 InstanceId)
{
    NvU32 ModInstId     = (InstanceId & INSTANCE_MASK);
    NvU32 TotalChannel  = s_pI2sInfo->TotalI2sChannel;

    NV_ASSERT(phI2s);

    *phI2s = NULL;

    // Validate the channel Id parameter.
    if (InstanceId >= TotalChannel)
    {
        return NvError_NotSupported;
    }

    // Get the head pointer of i2s channel
    *phI2s = s_pI2sInfo->hI2sChannelList[ModInstId];

    // If the i2s handle does not exist then cretae it.
    if (!*phI2s)
    {
        return NvError_NotSupported;
    }
    return NvSuccess;
}

/**
 * Get the asynchrnous read transfer information for last transaction.
 */
NvError
NvDdkI2sGetAsynchReadTransferInfo(
    NvDdkI2sHandle hI2s,
    NvDdkI2sClientBuffer *pI2SReceivedBufferInfo)
{
    NV_ASSERT(hI2s);
    NV_ASSERT(pI2SReceivedBufferInfo);

    pI2SReceivedBufferInfo->TransferStatus = hI2s->RxTransferStatus;
    return NvSuccess;
}

/**
 * Get the asynchrnous write transfer information.
 */
NvError
NvDdkI2sGetAsynchWriteTransferInfo(
    NvDdkI2sHandle hI2s,
    NvDdkI2sClientBuffer *pI2SSentBufferInfo)

{
    NV_ASSERT(hI2s);
    NV_ASSERT(pI2SSentBufferInfo);

    pI2SSentBufferInfo->TransferStatus = hI2s->TxTransferStatus;
    return NvSuccess;
}

NvError NvDdkI2sWritePause(NvDdkI2sHandle hI2s)
{
    NV_ASSERT(hI2s);
    hI2s->pHwInterface->SetDataFlowFxn(&hI2s->I2sAc97HwRegs,
                        I2sAc97Direction_Transmit, NV_FALSE);
    return NvSuccess;
}

NvError NvDdkI2sWriteResume(NvDdkI2sHandle hI2s)
{
    NV_ASSERT(hI2s);
    hI2s->pHwInterface->SetDataFlowFxn(&hI2s->I2sAc97HwRegs,
                        I2sAc97Direction_Transmit, NV_TRUE);
    return NvSuccess;
}

NvError NvDdkI2sSetLoopbackMode(NvDdkI2sHandle hI2s, NvBool IsEnable)
{
    NV_ASSERT(hI2s);
    if (hI2s->pI2sInfo->SocI2sCaps.IsLoopbackSupported == NV_TRUE)
    {
        hI2s->pHwInterface->SetLoopbackTestFxn(&hI2s->I2sAc97HwRegs, IsEnable);
    }
    else
    {
        return NvError_NotSupported;
    }
    return NvSuccess;
}

NvError
NvDdkI2sSetContinuousDoubleBufferingMode(
    NvDdkI2sHandle hI2s,
    NvBool IsEnable,
    NvRmPhysAddr TransmitBufferPhyAddress,
    NvU32 TransferSize,
    NvOsSemaphoreHandle AsynchSemaphoreId1,
    NvOsSemaphoreHandle AsynchSemaphoreId2)

{
    return NvError_NotSupported;
}

NvError
NvDdkI2sGetInterfaceProperty(
    NvDdkI2sHandle hI2s,
    void *pI2SInterfaceProperty)
{
    NvOsMemcpy(pI2SInterfaceProperty, &hI2s->I2sInterfaceProps,
                     sizeof(NvOdmQueryI2sInterfaceProperty));
    return NvSuccess;
}

NvError
NvDdkI2sSetInterfaceProperty(
    NvDdkI2sHandle hI2s,
    void *pI2SInterfaceProperty)
{
    NvError Error   = NvSuccess;
    NvBool IsUpdate = NV_FALSE;
    NvDdkI2sPcmDataProperty *pTempPcmI2sDataProp;
    NvDdkI2sNetworkDataProperty *pTempNwI2sDataProp;
    NvDdkI2sTdmDataProperty *pTempTdmI2sDataProp;
    NvDdkI2sDspDataProperty *pTempDspI2sDataProp;

    // Get the Interface property first
    NvOdmQueryI2sInterfaceProperty CurIntProperty, *pNewIntProperty;
    NvDdkI2sGetInterfaceProperty(hI2s, &CurIntProperty);
    pNewIntProperty = (NvOdmQueryI2sInterfaceProperty*)pI2SInterfaceProperty;

    // check for mode and comm format
    if ((CurIntProperty.Mode != pNewIntProperty->Mode) ||
       (CurIntProperty.I2sDataCommunicationFormat !=
                            pNewIntProperty->I2sDataCommunicationFormat) ||
       (CurIntProperty.I2sLRLineControl != pNewIntProperty->I2sLRLineControl))
    {
        IsUpdate = NV_TRUE;
        NvOsMemcpy(&hI2s->I2sInterfaceProps, pI2SInterfaceProperty,
                                 sizeof(NvOdmQueryI2sInterfaceProperty));
    }

    // Set only if the property are different from Current one
    if (IsUpdate)
    {
        hI2s->pHwInterface->SetInterfacePropertyFxn(&hI2s->I2sAc97HwRegs,
                       pI2SInterfaceProperty);
        switch (hI2s->I2sMode)
        {
            case NvDdkI2SMode_PCM:
                pTempPcmI2sDataProp =
                    (NvDdkI2sPcmDataProperty*) hI2s->I2sDataProps;
                Error = ConfigureI2sClock(hI2s,
                             pTempPcmI2sDataProp->SamplingRate,
                             pTempPcmI2sDataProp->DatabitsPerLRCLK);
                break;
            case NvDdkI2SMode_DSP:
                pTempDspI2sDataProp =
                    (NvDdkI2sDspDataProperty*) hI2s->I2sDataProps;
                Error = ConfigureI2sClock(hI2s,
                                pTempDspI2sDataProp->SamplingRate,
                                pTempDspI2sDataProp->DatabitsPerLRCLK);
                break;
            case NvDdkI2SMode_Network:
                if (hI2s->pI2sInfo->SocI2sCaps.IsNetworkModeSupported)
                {
                    pTempNwI2sDataProp =
                        (NvDdkI2sNetworkDataProperty*) hI2s->I2sDataProps;
                    Error = ConfigureI2sClock(hI2s,
                                pTempNwI2sDataProp->SamplingRate,
                                pTempNwI2sDataProp->DatabitsPerLRCLK);
                }
                else
                {
                    return NvError_NotSupported;
                }
                break;
            case NvDdkI2SMode_TDM:
                if (hI2s->pI2sInfo->SocI2sCaps.IsTDMModeSupported)
                {
                    pTempTdmI2sDataProp =
                        (NvDdkI2sTdmDataProperty*) hI2s->I2sDataProps;
                    Error = ConfigureI2sClock(hI2s,
                                    pTempTdmI2sDataProp->SamplingRate,
                                    pTempTdmI2sDataProp->DatabitsPerLRCLK);
                }
                else
                {
                    return NvError_NotSupported;
                }
                break;
            default:
                    NV_ASSERT(!"I2S Operating mode Unsupported");
                    return NvError_NotSupported;
        }
    }
    return Error;
}


NvError
NvDdkI2sGetDataProperty(
        NvDdkI2sHandle hI2s,
        NvDdkI2SMode I2sMode,
        void * const pI2SDataProperty)
{
    NvDdkI2sPcmDataProperty TempPcmI2sDataProp;
    NvDdkI2sNetworkDataProperty TempNwI2sDataProp;
    NvDdkI2sTdmDataProperty TempTdmI2sDataProp;
    NvDdkI2sDspDataProperty TempDspI2sDataProp;

    NV_ASSERT(pI2SDataProperty);

    switch (I2sMode)
    {
        case NvDdkI2SMode_PCM:
            TempPcmI2sDataProp.I2sDataWordFormat =
                            NvDdkI2SDataWordValidBits_StartFromLsb;
            TempPcmI2sDataProp.IsMonoDataFormat = NV_FALSE;
            TempPcmI2sDataProp.SamplingRate = 0;
            TempPcmI2sDataProp.ValidBitsInI2sDataWord = 16;
            // Copy the current configured parameter for the i2s communication.
            NvOsMemcpy(pI2SDataProperty, &TempPcmI2sDataProp,
                                sizeof(NvDdkI2sPcmDataProperty));
            break;
        case NvDdkI2SMode_Network:
            TempNwI2sDataProp.ActiveRxSlotSelectionMask = 0x0;
            TempNwI2sDataProp.ActiveTxSlotSelectionMask = 0x0;
            TempNwI2sDataProp.SamplingRate = 0x0;
            // Copy the current configured parameter for the i2s communication.
            NvOsMemcpy(pI2SDataProperty, &TempNwI2sDataProp,
                            sizeof(NvDdkI2sNetworkDataProperty));
            break;
        case NvDdkI2SMode_TDM:
            TempTdmI2sDataProp.NumberOfSlotsPerFSync = 1;
            TempTdmI2sDataProp.RxDataWordFormat =
                    NvDdkI2SDataWordValidBits_StartFromLsb;
            TempTdmI2sDataProp.TxDataWordFormat =
                    NvDdkI2SDataWordValidBits_StartFromLsb;
            TempTdmI2sDataProp.SamplingRate = 0;
            TempTdmI2sDataProp.SlotSize = 32;
            // Copy the current configured parameter for the i2s communication.
            NvOsMemcpy(pI2SDataProperty, &TempTdmI2sDataProp,
                         sizeof(NvDdkI2sTdmDataProperty));
            break;
        case NvDdkI2SMode_DSP:
            TempDspI2sDataProp.I2sDataWordFormat =
                    NvDdkI2SDataWordValidBits_StartFromLsb;
            TempDspI2sDataProp.SamplingRate = 0;
            TempDspI2sDataProp.ValidBitsInI2sDataWord = 16;
            TempDspI2sDataProp.TxMaskBits = 0;
            TempDspI2sDataProp.RxMaskBits = 0;
            TempDspI2sDataProp.FsyncWidth = 0;
            TempDspI2sDataProp.HighzCtrl  = NvDdkI2SHighzMode_No_PosEdge;
            // Copy the current configured parameter for the i2s communication.
            NvOsMemcpy(pI2SDataProperty, &TempDspI2sDataProp,
                        sizeof(NvDdkI2sDspDataProperty));
            break;
        default:
            return NvError_NotSupported;

    }

    return NvSuccess;
}

/**
 * Set the i2s configuration which is configured currently.
 */
NvError
NvDdkI2sSetDataProperty(
        NvDdkI2sHandle hI2s,
        NvDdkI2SMode I2sMode,
        const void *pI2SNewDataProp)
{
    NvError Error = NvSuccess;
    NvBool IsConfigureClk = NV_FALSE;
    NvU32 TempSamplingRate = 0;
    NvU32 TempDatabitsPerLRCLK = 32;
    NvU32 TempI2sDataWord = 0;
    NvU32 TempI2sValidBitsInDataWord = 0;
    NvU32 TempSize = 0;
    NvU32 TempDataOffset = 1;
    NvDdkI2sPcmDataProperty *pTempPcmI2sDataProp, *pCurPcmI2sDataProp;
    NvDdkI2sNetworkDataProperty *pTempNwI2sDataProp, *pCurNwI2sDataProp;
    NvDdkI2sTdmDataProperty *pTempTdmI2sDataProp, *pCurTdmI2sDataProp;
    NvDdkI2sDspDataProperty *pTempDspI2sDataProp, *pCurDspI2sDataProp;

    NV_ASSERT(hI2s);
    NV_ASSERT(pI2SNewDataProp);

    switch (I2sMode)
    {
        case NvDdkI2SMode_PCM:
            pTempPcmI2sDataProp  = (NvDdkI2sPcmDataProperty*) pI2SNewDataProp;
            pCurPcmI2sDataProp   =
                        (NvDdkI2sPcmDataProperty*) hI2s->I2sDataProps;
            TempSamplingRate     = pTempPcmI2sDataProp->SamplingRate;
            TempDatabitsPerLRCLK = pTempPcmI2sDataProp->DatabitsPerLRCLK;
            TempSize             = sizeof(NvDdkI2sPcmDataProperty);
            // Check if sampling rate need to change, If not dont configure the
            // clock for sampling rate
            if ((hI2s->PowerStateChange) ||
                (TempSamplingRate != pCurPcmI2sDataProp->SamplingRate) ||
                (TempSamplingRate != pCurPcmI2sDataProp->DatabitsPerLRCLK))
                IsConfigureClk = NV_TRUE;

            TempI2sDataWord = pTempPcmI2sDataProp->I2sDataWordFormat;
            TempI2sValidBitsInDataWord =
                     pTempPcmI2sDataProp->ValidBitsInI2sDataWord;
            // Change to a proper place.
            hI2s->I2sInterfaceProps.I2sDataCommunicationFormat
                                    = NvOdmQueryI2sDataCommFormat_I2S;
            // Make sure pcm ctrl register is not set
            hI2s->pHwInterface->SetDspDataFlowFxn(&hI2s->I2sAc97HwRegs, I2sAc97Direction_Transmit, NV_FALSE);
            hI2s->pHwInterface->SetDspDataFlowFxn(&hI2s->I2sAc97HwRegs, I2sAc97Direction_Receive, NV_FALSE);

            // Make sure NW Ctrl register is not set as well
            hI2s->pHwInterface->NwModeSetSlotSelectionMaskFxn(&hI2s->I2sAc97HwRegs,
                                            I2sAc97Direction_Transmit, 0, NV_FALSE);
            hI2s->pHwInterface->NwModeSetSlotSelectionMaskFxn(&hI2s->I2sAc97HwRegs,
                                            I2sAc97Direction_Receive, 0, NV_FALSE);
            break;

        case NvDdkI2SMode_Network:
            pTempNwI2sDataProp =
                (NvDdkI2sNetworkDataProperty*) pI2SNewDataProp;
            pCurNwI2sDataProp =
                (NvDdkI2sNetworkDataProperty*) hI2s->I2sDataProps;
            TempSamplingRate = pTempNwI2sDataProp->SamplingRate;
            TempDatabitsPerLRCLK = pTempNwI2sDataProp->DatabitsPerLRCLK;

            // Check if sampling rate need to change, If not dont configure the
            // clock for sampling rate
            if ((hI2s->PowerStateChange) ||
                (pTempNwI2sDataProp->SamplingRate !=
                                pCurNwI2sDataProp->SamplingRate) ||
                (pTempNwI2sDataProp->DatabitsPerLRCLK !=
                                pCurNwI2sDataProp->DatabitsPerLRCLK))
                IsConfigureClk = NV_TRUE;

            TempSize        =   sizeof(NvDdkI2sNetworkDataProperty);
            TempI2sDataWord =   pTempNwI2sDataProp->I2sDataWordFormat,
            TempI2sValidBitsInDataWord =
                    pTempNwI2sDataProp->ValidBitsInI2sDataWord;
            break;
        case NvDdkI2SMode_TDM:
            pTempTdmI2sDataProp = (NvDdkI2sTdmDataProperty*) pI2SNewDataProp;
            pCurTdmI2sDataProp = (NvDdkI2sTdmDataProperty*) hI2s->I2sDataProps;
            TempSamplingRate = pTempTdmI2sDataProp->SamplingRate;
            TempDatabitsPerLRCLK = pTempTdmI2sDataProp->DatabitsPerLRCLK;

            // Check if sampling rate need to change, If not dont configure the
            // clock for sampling rate
            if ((hI2s->PowerStateChange) ||
                (TempSamplingRate != pCurTdmI2sDataProp->SamplingRate) ||
                (TempDatabitsPerLRCLK != pCurTdmI2sDataProp->DatabitsPerLRCLK))
                IsConfigureClk = NV_TRUE;

            TempSize        =   sizeof(NvDdkI2sTdmDataProperty);
            TempI2sDataWord =   pTempTdmI2sDataProp->I2sDataWordFormat,
            TempI2sValidBitsInDataWord =
                    pTempTdmI2sDataProp->ValidBitsInI2sDataWord;
            break;
        case NvDdkI2SMode_DSP:
            pTempDspI2sDataProp = (NvDdkI2sDspDataProperty*) pI2SNewDataProp;
            pCurDspI2sDataProp = (NvDdkI2sDspDataProperty*) hI2s->I2sDataProps;

            TempSamplingRate = pTempDspI2sDataProp->SamplingRate;
            TempDatabitsPerLRCLK = pTempDspI2sDataProp->DatabitsPerLRCLK;

            // Check if sampling rate need to change, If not dont configure the
            // clock for sampling rate
            if ((hI2s->PowerStateChange) ||
                (TempSamplingRate != pCurDspI2sDataProp->SamplingRate) ||
                (TempDatabitsPerLRCLK != pCurDspI2sDataProp->DatabitsPerLRCLK))
                IsConfigureClk = NV_TRUE;

            TempSize        = sizeof(NvDdkI2sDspDataProperty);
            TempI2sDataWord = pTempDspI2sDataProp->I2sDataWordFormat,
            TempI2sValidBitsInDataWord =
                    pTempDspI2sDataProp->ValidBitsInI2sDataWord;
            I2sConfigureDspMode(hI2s, I2sMode, pTempDspI2sDataProp);
            break;
        default:
            return NvError_NotSupported;
    }
    if (TempSize)
    {
        NvOsMemcpy((void *)hI2s->I2sDataProps,
                            pI2SNewDataProp,
                            TempSize);

        hI2s->pHwInterface->SetFifoFormatFxn(&hI2s->I2sAc97HwRegs,
                      TempI2sDataWord,
                      TempI2sValidBitsInDataWord);

        hI2s->I2sMode  = I2sMode;
        hI2s->DmaWidth = 0;
        // Set DmaWidth
        I2sSetDmaBusWidth(hI2s, TempI2sDataWord, TempI2sValidBitsInDataWord);

        // Make the fn common  to avoid the checking
        if (hI2s->pI2sInfo->SocI2sCaps.IsAudioCIFModeSupported)
        {
            NvDdkPrivT30I2sSetDataOffset(&hI2s->I2sAc97HwRegs,
                                            NV_TRUE, TempDataOffset);
            NvDdkPrivT30I2sSetDataOffset(&hI2s->I2sAc97HwRegs,
                                            NV_FALSE, TempDataOffset);
        }

    }

    if (IsConfigureClk)
    {
        Error =
            ConfigureI2sClock(hI2s, TempSamplingRate, TempDatabitsPerLRCLK);
    }
    return Error;
}

/*
    Set the Abif channel information needed for i2s
*/
NvError
NvDdkI2sSetApbifChannelInterface(
    NvDdkI2sHandle hI2s,
    void *pApbifChannelInfo)
{

    // check the h/w type
    if (hI2s->pI2sInfo->SocI2sCaps.IsAudioCIFModeSupported)
    {
        hI2s->pHwInterface->SetApbifChannelFxn(&(hI2s->I2sAc97HwRegs),
                             pApbifChannelInfo);
    }
    return NvSuccess;
}

/*
    Set the acif channel information
*/
NvError
NvDdkI2sSetAcifRegister(
    NvDdkI2sHandle hI2s,
    NvBool IsReceive,
    NvBool IsRead,
    NvU32  *pData)
{

    // check the h/w type
    if (hI2s->pI2sInfo->SocI2sCaps.IsAudioCIFModeSupported)
    {
        hI2s->pHwInterface->SetAcifRegisterFxn(&(hI2s->I2sAc97HwRegs),
                        IsReceive, IsRead, pData);
    }
    return NvSuccess;
}


NvError NvDdkI2sSuspend(NvDdkI2sHandle hI2s)
{
    NvError Error;
    NV_ASSERT(hI2s);

    NVDDK_I2S_POWERLOG(("NvDdkI2sSuspend i2srefccnt %d \n",hI2s->I2sUseCount));

    if (hI2s->I2sUseCount > 0)
        hI2s->I2sUseCount--;

    NVDDK_I2S_POWERLOG((" PR- %d \n",hI2s->I2sUseCount));

    if (hI2s->I2sUseCount > 0)
        return NvSuccess;

    // Disable the clock
    Error = DisableI2sPower(hI2s);

    hI2s->PowerStateChange = NV_TRUE;
    return Error;
}

NvError NvDdkI2sResume(NvDdkI2sHandle hI2s)
{
    NvError Error;
    NvRmPowerState PowerState;

    NV_ASSERT(hI2s);

    NVDDK_I2S_POWERLOG(("NvDdkI2sResume I2srefcnt %d \n",hI2s->I2sUseCount));

    hI2s->I2sUseCount++;

    NVDDK_I2S_POWERLOG((" PR+ %d \n",hI2s->I2sUseCount));

    if (hI2s->I2sUseCount > 1)
        return NvSuccess;

    // Enable power for i2s module
    Error = EnableI2sPower(hI2s);

    // Reset the module.
    if (!Error)
    {
        NvRmModuleReset(hI2s->hDevice, hI2s->RmModuleId);

        Error = ConfigureI2sClock(hI2s, hI2s->SamplingRate,
                                     hI2s->DatabitsPerLRCLK);
        NVDDK_I2S_POWERLOG(("NvDdkI2sResume sr=%d bpclk %d ",hI2s->SamplingRate, hI2s->DatabitsPerLRCLK));

        // Check whether the power state is active
        if (!Error)
        {
            Error = NvRmPowerGetState(hI2s->hDevice, &PowerState);
            if (!Error)
                NV_ASSERT(PowerState == NvRmPowerState_Active);

            hI2s->PowerStateChange = NV_FALSE;

        }
        else
        {
            // Disable the module
            Error = NvDdkI2sSuspend(hI2s);
            hI2s->I2sUseCount--;
        }
    }

    return Error;
}
#endif //!NV_IS_AVP

