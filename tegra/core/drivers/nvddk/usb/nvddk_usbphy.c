/*
 * Copyright (c) 2009 - 2013, NVIDIA CORPORATION.  All rights reserved.
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
 *           NvDDK USB PHY functions</b>
 *
 * @b Description: Defines USB PHY private functions
 *
 */

#include "nvrm_pinmux.h"
#include "nvrm_power.h"
#include "nvrm_pmu.h"
#include "nvrm_hardware_access.h"
#include "nvddk_usbphy_priv.h"

#define MAX_USB_INSTANCES 5

// On platforms that never disable USB controller clock, use 1KHz as an
// indicator that USB controller is idle, and core voltage can be scaled down
#define USBC_IDLE_KHZ (1)

static NvDdkUsbPhy *s_pUsbPhy = NULL;
static NvDdkUsbPhyUtmiPadConfig *s_pUtmiPadConfig = NULL;
static NvS32 s_UsbPhyPowerRailRefCount = 0;

static NvBool
UsbPhyDiscover(
    NvDdkUsbPhy *pUsbPhy)
{
    NvU64 guid = NV_VDD_USB2_VBUS_ODM_ID;
    NvOdmPeripheralConnectivity const *pConnectivity;

    if(pUsbPhy->pConnectivity)
    {
        return NV_TRUE;
    }

    /* get the connectivity info */
    pConnectivity = NvOdmPeripheralGetGuid(guid);
    if(!pConnectivity)
    {
        return NV_FALSE;
    }

    pUsbPhy->Guid = guid;
    pUsbPhy->pConnectivity = pConnectivity;

    return NV_TRUE;
}

static void UsbPrivEnableVbus(NvDdkUsbPhy *pUsbPhy, NvBool Enable)
{
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvRmPmuVddRailCapabilities RailCaps;
    NvU32 i, SettlingTimeUSec = 0;

    switch (pUsbPhy->Instance)
    {
        case 0:
            pConnectivity = NvOdmPeripheralGetGuid(NV_VDD_VBUS_ODM_ID);
            break;
        case 1:
            pConnectivity = NvOdmPeripheralGetGuid(NV_VDD_USB2_VBUS_ODM_ID);
            break;
        case 2:
            pConnectivity = NvOdmPeripheralGetGuid(NV_VDD_USB3_VBUS_ODM_ID);
            break;
        default:
            break;
    }

    if (pConnectivity != NULL)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            // Search for the vdd rail entry
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
            {
                NvRmPmuGetCapabilities(pUsbPhy->hRmDevice,
                            pConnectivity->AddressList[i].Address, &RailCaps);


                if (Enable)
                {
                    NvRmPmuSetVoltage(pUsbPhy->hRmDevice,
                            pConnectivity->AddressList[i].Address,
                            RailCaps.requestMilliVolts,
                            &SettlingTimeUSec);
                }
                else
                {
                    NvRmPmuSetVoltage(pUsbPhy->hRmDevice,
                            pConnectivity->AddressList[i].Address,
                            ODM_VOLTAGE_OFF,
                            &SettlingTimeUSec);
                }
            }
        }
    }
}

static void
UsbPhyPowerRailEnable(
    NvDdkUsbPhy *pUsbPhy,
    NvBool Enable)
{
    NvU32 i;
    NvOdmPeripheralConnectivity const *pConnectivity;
    NvU32 settle_time_us;

    /* get the peripheral config */
    if(!UsbPhyDiscover(pUsbPhy))
    {
        // Do nothing if no power rail info is discovered
    return;
    }

    /* enable the power rail */
    pConnectivity = pUsbPhy->pConnectivity;

    if (Enable)
    {
        if(NvOsAtomicExchangeAdd32(&s_UsbPhyPowerRailRefCount, 1) == 0)
        {
            for(i = 0; i < pConnectivity->NumAddress; i++)
            {
                if(pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
                {
                    NvRmPmuVddRailCapabilities cap;

                    /* address is the vdd rail id */
                    NvRmPmuGetCapabilities(
                        pUsbPhy->hRmDevice,
                        pConnectivity->AddressList[i].Address, &cap);

                    /* set the rail volatage to the recommended */
                    NvRmPmuSetVoltage(
                        pUsbPhy->hRmDevice, pConnectivity->AddressList[i].Address,
                        cap.requestMilliVolts, &settle_time_us);

                    /* wait for the rail to settle */
                    NvOsWaitUS(settle_time_us);
                }
            }
        }
    }
    else
    {
        if(NvOsAtomicExchangeAdd32(&s_UsbPhyPowerRailRefCount, -1) == 1)
        {
            for(i = 0; i < pConnectivity->NumAddress; i++)
            {
                if(pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
                {
                    /* set the rail volatage to the recommended */
                    NvRmPmuSetVoltage(
                        pUsbPhy->hRmDevice, pConnectivity->AddressList[i].Address,
                        ODM_VOLTAGE_OFF, 0);
                }
            }
        }
    }

}


static void
UsbPhyOpenHwInterface(
    NvDdkUsbPhyHandle hUsbPhy)
{
    static NvDdkUsbPhyCapabilities s_UsbPhyCap[] =
    {
        // T124
        { NV_TRUE, NV_FALSE, NV_FALSE },

    };
    NvDdkUsbPhyCapabilities *pUsbfCap = NULL;
    NvRmModuleCapability s_UsbPhyCaps[] =
    {
        {2, 0, 0, NvRmModulePlatform_Silicon, &s_UsbPhyCap[0]}, // T124, USB1
        {2, 1, 0, NvRmModulePlatform_Silicon, &s_UsbPhyCap[0]}, // T124, USB2
        {2, 2, 0, NvRmModulePlatform_Silicon, &s_UsbPhyCap[0]}, // T124, USB3
    };

    NV_ASSERT_SUCCESS(
        NvRmModuleGetCapabilities(hUsbPhy->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
            s_UsbPhyCaps, NV_ARRAY_SIZE(s_UsbPhyCaps),
            (void**)&pUsbfCap));

    // Fill the client capabilities structure.
    NvOsMemcpy(&hUsbPhy->Caps, pUsbfCap, sizeof(NvDdkUsbPhyCapabilities));

    if (!hUsbPhy->Caps.PllRegInController)
    {
        T124UsbPhyOpenHwInterface(hUsbPhy);
    }
    else
    {
        NV_ASSERT(!"Unknown interface requested");
    }
}


static NvError
UsbPhyDfsBusyHint(
    NvDdkUsbPhyHandle hUsbPhy,
    NvBool DfsOn,
    NvU32 BoostDurationMs)
{
    NvRmDfsBusyHint pUsbHintOn[] =
    {
        { NvRmDfsClockId_Emc, NV_WAIT_INFINITE, USB_HW_MIN_SYSTEM_FREQ_KH, NV_TRUE },
        { NvRmDfsClockId_Ahb, NV_WAIT_INFINITE, USB_HW_MIN_SYSTEM_FREQ_KH, NV_TRUE }
    };
    NvRmDfsBusyHint pUsbHintOff[] =
    {
        { NvRmDfsClockId_Emc, 0, 0, NV_TRUE },
        { NvRmDfsClockId_Ahb, 0, 0, NV_TRUE }
    };
    NvError e = NvSuccess;

    pUsbHintOn[0].BoostDurationMs = BoostDurationMs;
    pUsbHintOn[1].BoostDurationMs = BoostDurationMs;

    if (DfsOn)
    {
        if (hUsbPhy->Caps.PhyRegInController)
        {
            // Indicate USB controller is active
            NvRmFreqKHz PrefFreq = NvRmPowerModuleGetMaxFrequency(
                hUsbPhy->hRmDevice,
                NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance));

            NV_CHECK_ERROR_CLEANUP(
                NvRmPowerModuleClockConfig(hUsbPhy->hRmDevice,
                    NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
                    hUsbPhy->RmPowerClientId, PrefFreq, PrefFreq, &PrefFreq,
                    1, NULL, 0));
        }
        return NvRmPowerBusyHintMulti(hUsbPhy->hRmDevice,
                                      hUsbPhy->RmPowerClientId,
                                      pUsbHintOn,
                                      NV_ARRAY_SIZE(pUsbHintOn),
                                      NvRmDfsBusyHintSyncMode_Async);
    }
    else
    {
        if (hUsbPhy->Caps.PhyRegInController)
        {
            // Indicate USB controller is idle
            NvRmFreqKHz PrefFreq = USBC_IDLE_KHZ;

            NV_CHECK_ERROR_CLEANUP(
                NvRmPowerModuleClockConfig(hUsbPhy->hRmDevice,
                    NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
                    hUsbPhy->RmPowerClientId, PrefFreq, PrefFreq, &PrefFreq,
                    1, NULL, 0));
        }
        return NvRmPowerBusyHintMulti(hUsbPhy->hRmDevice,
                                      hUsbPhy->RmPowerClientId,
                                      pUsbHintOff,
                                      NV_ARRAY_SIZE(pUsbHintOff),
                                      NvRmDfsBusyHintSyncMode_Async);
    }

fail:
    return e;
}


static NvError
UsbPhyInitialize(
    NvDdkUsbPhyHandle hUsbPhy)
{
    NvError e = NvSuccess;
    NvRmFreqKHz CurrentFreq = 0;
    NvRmFreqKHz PrefFreqList[3] = {12000, 60000, NvRmFreqUnspecified};
    // request power
    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerVoltageControl(hUsbPhy->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
            hUsbPhy->RmPowerClientId, NvRmVoltsUnspecified,
            NvRmVoltsUnspecified, NULL, 0, NULL));

    // Enable clock to the USB controller and Phy
    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerModuleClockControl(hUsbPhy->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
                hUsbPhy->RmPowerClientId, NV_TRUE));

    if (!hUsbPhy->Caps.PhyRegInController)
    {
        if (hUsbPhy->pProperty->UsbInterfaceType == NvOdmUsbInterfaceType_UlpiNullPhy)
        {
            /* Request for 60MHz clk */
            NV_CHECK_ERROR_CLEANUP(
                NvRmPowerModuleClockConfig(hUsbPhy->hRmDevice,
                    NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
                    hUsbPhy->RmPowerClientId, PrefFreqList[1],
                    PrefFreqList[1], &PrefFreqList[1], 1, &CurrentFreq, 0));
        }
        else
        {
            /* Request for 12 MHz clk */
            NV_CHECK_ERROR_CLEANUP(
                NvRmPowerModuleClockConfig(hUsbPhy->hRmDevice,
                    NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
                    hUsbPhy->RmPowerClientId, PrefFreqList[0],
                    PrefFreqList[0], &PrefFreqList[0], 1, &CurrentFreq, 0));
        }
    }
    else
    {
        /* No need for actual clock configuration - all USB PLL frequencies
         are available concurrently. Just indicate USB controller is idle */

        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerModuleClockConfig(hUsbPhy->hRmDevice,
                NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
                hUsbPhy->RmPowerClientId, PrefFreqList[0],
                PrefFreqList[0], &PrefFreqList[0], 1, &CurrentFreq, 0));
    }
    // Reset controller
    NvRmModuleReset(hUsbPhy->hRmDevice,
        NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance));

    // Power Up the USB Phy
    NV_CHECK_ERROR_CLEANUP(hUsbPhy->PowerUp(hUsbPhy));

    hUsbPhy->IsPhyPoweredUp = NV_TRUE;

fail:
    return e;
}

static NvError
UsbPhyIoctlUsbBisuHintsOnOff(
    NvDdkUsbPhy *pUsbPhy,
    const void *pInputArgs)
{
   NvDdkUsbPhyIoctl_UsbBusyHintsOnOffInputArgs *pOnOff = NULL;

    if (!pInputArgs)
        return NvError_BadParameter;

    pOnOff = (NvDdkUsbPhyIoctl_UsbBusyHintsOnOffInputArgs *)pInputArgs;

    return UsbPhyDfsBusyHint(pUsbPhy, pOnOff->OnOff, pOnOff->BoostDurationMs);
}

NvError
NvDdkUsbPhyOpen(
    NvRmDeviceHandle hRm,
    NvU32 Instance,
    NvDdkUsbPhyHandle *hUsbPhy)
{
    NvError e;
    NvU32 MaxInstances = 0;
    NvDdkUsbPhy *pUsbPhy = NULL;
    NvRmModuleInfo info[MAX_USB_INSTANCES];
    NvU32 j;

    NV_ASSERT(hRm);
    NV_ASSERT(hUsbPhy);
    NV_ASSERT(Instance < MAX_USB_INSTANCES);
    NV_CHECK_ERROR(NvRmModuleGetModuleInfo(hRm, NvRmModuleID_Usb2Otg, &MaxInstances, NULL));
    if (MaxInstances > MAX_USB_INSTANCES)
    {
       // Ceil "instances" to MAX_USB_INSTANCES
       MaxInstances = MAX_USB_INSTANCES;
    }
    NV_CHECK_ERROR(NvRmModuleGetModuleInfo(hRm, NvRmModuleID_Usb2Otg, &MaxInstances, info));
    for (j = 0; j < MaxInstances; j++)
    {
    // Check whether the requested instance is present
        if(info[j].Instance == Instance)
            break;
    }
    // No match found return
    if (j == MaxInstances)
    {
        return NvError_ModuleNotPresent;
    }

    if (!s_pUsbPhy)
    {
        s_pUsbPhy = NvOsAlloc(MaxInstances * sizeof(NvDdkUsbPhy));
        if (s_pUsbPhy)
            NvOsMemset(s_pUsbPhy, 0, MaxInstances * sizeof(NvDdkUsbPhy));
        else
            return NvError_InsufficientMemory;
    }

    if (!s_pUtmiPadConfig)
    {
        s_pUtmiPadConfig = NvOsAlloc(sizeof(NvDdkUsbPhyUtmiPadConfig));
        if (s_pUtmiPadConfig)
        {
            NvRmPhysAddr PhyAddr;

            NvOsMemset(s_pUtmiPadConfig, 0, sizeof(NvDdkUsbPhyUtmiPadConfig));
            NvRmModuleGetBaseAddress(
                hRm,
                NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, 0),
                &PhyAddr, &s_pUtmiPadConfig->BankSize);

            NV_CHECK_ERROR_CLEANUP(
                NvRmPhysicalMemMap(
                    PhyAddr, s_pUtmiPadConfig->BankSize, NVOS_MEM_READ_WRITE,
                    NvOsMemAttribute_Uncached, (void **)&s_pUtmiPadConfig->pVirAdr));
        }
        else
        {
            return NvError_InsufficientMemory;
        }
    }

    pUsbPhy = &s_pUsbPhy[Instance];

    if (!pUsbPhy->RefCount)
    {
        NvRmPhysAddr PhysAddr;
        NvOsMemset(pUsbPhy, 0, sizeof(NvDdkUsbPhy));
        pUsbPhy->Instance = Instance;
        pUsbPhy->hRmDevice = hRm;
        pUsbPhy->RefCount = 1;
        pUsbPhy->IsPhyPoweredUp = NV_FALSE;
        pUsbPhy->pUtmiPadConfig = s_pUtmiPadConfig;
        pUsbPhy->pProperty = NvOdmQueryGetUsbProperty(
                                    NvOdmIoModule_Usb, pUsbPhy->Instance);

        NvRmModuleGetBaseAddress(
            pUsbPhy->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, pUsbPhy->Instance),
            &PhysAddr, &pUsbPhy->UsbBankSize);

        NV_CHECK_ERROR_CLEANUP(
            NvRmPhysicalMemMap(
                PhysAddr, pUsbPhy->UsbBankSize, NVOS_MEM_READ_WRITE,
                NvOsMemAttribute_Uncached, (void **)&pUsbPhy->UsbVirAdr));

        NvRmModuleGetBaseAddress(
            pUsbPhy->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_Misc, 0),
            &PhysAddr, &pUsbPhy->MiscBankSize);

        NV_CHECK_ERROR_CLEANUP(
            NvRmPhysicalMemMap(
                PhysAddr, pUsbPhy->MiscBankSize, NVOS_MEM_READ_WRITE,
                NvOsMemAttribute_Uncached, (void **)&pUsbPhy->MiscVirAdr));
        // Register with Power Manager
        NV_CHECK_ERROR_CLEANUP(
            NvOsSemaphoreCreate(&pUsbPhy->hPwrEventSem, 0));

        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerRegister(pUsbPhy->hRmDevice,
            pUsbPhy->hPwrEventSem, &pUsbPhy->RmPowerClientId));

        if (pUsbPhy->pProperty->UsbMode == NvOdmUsbModeType_Host)
        {
            UsbPrivEnableVbus(pUsbPhy, NV_TRUE);
        }

        UsbPhyPowerRailEnable(pUsbPhy, NV_TRUE);

        // Open the H/W interface
        UsbPhyOpenHwInterface(pUsbPhy);

        // Initialize the USB Phy
        NV_CHECK_ERROR_CLEANUP(UsbPhyInitialize(pUsbPhy));
    }
    else
    {
        pUsbPhy->RefCount++;
    }

    *hUsbPhy = pUsbPhy;

    return NvSuccess;

fail:
    NvDdkUsbPhyClose(pUsbPhy);
    return e;
}


void
NvDdkUsbPhyClose(
    NvDdkUsbPhyHandle hUsbPhy)
{
    if (!hUsbPhy)
        return;

    if (!hUsbPhy->RefCount)
        return;

    --hUsbPhy->RefCount;

    if (hUsbPhy->RefCount)
    {
        return;
    }

    if (hUsbPhy->CloseHwInterface)
    {
        hUsbPhy->CloseHwInterface(hUsbPhy);
    }

    if (hUsbPhy->pProperty->UsbMode == NvOdmUsbModeType_Host)
    {
        UsbPrivEnableVbus(hUsbPhy, NV_FALSE);
    }

    if (hUsbPhy->RmPowerClientId)
    {
        if (hUsbPhy->IsPhyPoweredUp)
        {
            NV_ASSERT_SUCCESS(
                NvRmPowerModuleClockControl(hUsbPhy->hRmDevice,
                  NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
                  hUsbPhy->RmPowerClientId,
                  NV_FALSE));

            NV_ASSERT_SUCCESS(
                NvRmPowerVoltageControl(hUsbPhy->hRmDevice,
                  NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
                  hUsbPhy->RmPowerClientId,
                  NvRmVoltsOff, NvRmVoltsOff,
                  NULL, 0, NULL));
            hUsbPhy->IsPhyPoweredUp = NV_FALSE;
        }
        // Unregister driver from Power Manager
        NvRmPowerUnRegister(hUsbPhy->hRmDevice, hUsbPhy->RmPowerClientId);
        NvOsSemaphoreDestroy(hUsbPhy->hPwrEventSem);
    }

    UsbPhyPowerRailEnable(hUsbPhy, NV_FALSE);

    NvOsPhysicalMemUnmap(
        (void*)hUsbPhy->UsbVirAdr, hUsbPhy->UsbBankSize);

    NvOsPhysicalMemUnmap(
        (void*)hUsbPhy->MiscVirAdr, hUsbPhy->MiscBankSize);

    NvOsMemset(hUsbPhy, 0, sizeof(NvDdkUsbPhy));
}


NvError
NvDdkUsbPhyPowerUp(
    NvDdkUsbPhyHandle hUsbPhy,
    NvBool IsDpd)
{
    NvError e = NvSuccess;

    NV_ASSERT(hUsbPhy);

    if (hUsbPhy->IsPhyPoweredUp)
        return e;

    // On wake up from Deep power down mode turn on the rails based on odm query
    if (IsDpd)
    {
        if (hUsbPhy->pProperty->UsbRailPoweOffInDeepSleep)
            UsbPhyPowerRailEnable(hUsbPhy, NV_TRUE);
    }

    // Enable power for USB module
    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerVoltageControl(hUsbPhy->hRmDevice,
          NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
          hUsbPhy->RmPowerClientId, NvRmVoltsUnspecified,
          NvRmVoltsUnspecified, NULL, 0, NULL));

    // On Ap20 We will not turn off the H-Clk so not required to turn on
    if (!hUsbPhy->Caps.PhyRegInController)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerModuleClockControl(hUsbPhy->hRmDevice,
              NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
              hUsbPhy->RmPowerClientId, NV_TRUE));
    }

    NV_CHECK_ERROR_CLEANUP(hUsbPhy->PowerUp(hUsbPhy));
    hUsbPhy->IsPhyPoweredUp = NV_TRUE;

fail:
    return e;
}


NvError
NvDdkUsbPhyPowerDown(
    NvDdkUsbPhyHandle hUsbPhy,
    NvBool IsDpd)
{
    NvError e = NvSuccess;

    NV_ASSERT(hUsbPhy);

    if (!hUsbPhy->IsPhyPoweredUp)
        return e;

    // Power down the USB Phy
    NV_CHECK_ERROR_CLEANUP(hUsbPhy->PowerDown(hUsbPhy));

    // On AP20 H-CLK should not be turned off
    // This is required to detect the sensor interrupts.
    // However, phy can be programmed to put in the low power mode
    if (!hUsbPhy->Caps.PhyRegInController)
    {
        // Disable the clock
        NV_CHECK_ERROR_CLEANUP(
            NvRmPowerModuleClockControl(hUsbPhy->hRmDevice,
              NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
              hUsbPhy->RmPowerClientId, NV_FALSE));
    }

    // Disable power
    NV_CHECK_ERROR_CLEANUP(
        NvRmPowerVoltageControl(hUsbPhy->hRmDevice,
          NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbPhy->Instance),
          hUsbPhy->RmPowerClientId, NvRmVoltsOff, NvRmVoltsOff,
          NULL, 0, NULL));

    // In Deep power down mode turn off the rails based on odm query
    if (IsDpd)
    {
        if (hUsbPhy->pProperty->UsbRailPoweOffInDeepSleep)
            UsbPhyPowerRailEnable(hUsbPhy, NV_FALSE);
    }

    hUsbPhy->IsPhyPoweredUp = NV_FALSE;

fail:

    return e;
}


NvError
NvDdkUsbPhyWaitForStableClock(
    NvDdkUsbPhyHandle hUsbPhy)
{
    NV_ASSERT(hUsbPhy);

    return hUsbPhy->WaitForStableClock(hUsbPhy);
}


NvError
NvDdkUsbPhyIoctl(
    NvDdkUsbPhyHandle hUsbPhy,
    NvDdkUsbPhyIoctlType IoctlType,
    const void *InputArgs,
    void *OutputArgs)
{
    NvError ErrStatus = NvSuccess;

    NV_ASSERT(hUsbPhy);

    switch(IoctlType)
    {
        case NvDdkUsbPhyIoctlType_UsbBusyHintsOnOff:
            ErrStatus= UsbPhyIoctlUsbBisuHintsOnOff(hUsbPhy, InputArgs);
            break;

        default:
            ErrStatus =  hUsbPhy->Ioctl(hUsbPhy, IoctlType, InputArgs, OutputArgs);
            break;
    }
    return ErrStatus;
}
