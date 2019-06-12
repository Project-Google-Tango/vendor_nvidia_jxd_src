/*
 * Copyright (c) 2007-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: I2C API</b>
 *
 * @b Description: Contains the NvRM I2C implementation.
 */

#include "nvrm_i2c.h"
#include "nvrm_i2c_private.h"
#include "nvrm_drf.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvrm_interrupt.h"
#include "nvassert.h"
#include "nvodm_query_pinmux.h"
#include "nvrm_pinmux.h"
#include "nvrm_hwintf.h"
#include "nvodm_modules.h"
#include "nvrm_structure.h"
#include "nvrm_pinmux_utils.h"

/* Array of controllers */
static NvRmI2cController gs_I2cControllers[MAX_I2C_INSTANCES];
static NvRmI2cController *gs_Cont = NULL;

// Maximum I2C instances present in this SOC
static NvU32 MaxI2cControllers;
static NvU32 MaxDvcControllers;
static NvU32 MaxI2cInstances;

/**
 * SOC I2C  capability structure.
 */
typedef struct SocI2cCapabilityRec
{
    // Tells whether i2c slave supports packet mode or not
    NvBool IsPacketModeI2cSlaveSupport;
} SocI2cCapability;

#define I2C_SOURCE_FREQUENCY_FIXED    48000

/**
 * Get the I2C  SOC capability.
 *
 */
static void
I2cGetSocCapabilities(
    NvRmDeviceHandle hDevice,
    NvRmModuleID ModuleId,
    NvU32 Instance,
    SocI2cCapability *pI2cSocCaps)
{
    static SocI2cCapability s_SocI2cCapsList[3];
    NvRmModuleCapability I2cCapsList[] =
        {
            {1, 2, 0, NvRmModulePlatform_Silicon, &s_SocI2cCapsList[1]},
            {1, 3, 0, NvRmModulePlatform_Silicon, &s_SocI2cCapsList[2]},
        };
    SocI2cCapability *pI2cCaps = NULL;
    NvU32 numCaps = NV_ARRAY_SIZE(I2cCapsList);

    // AP15 A01P and A02 does not support I2C packet or DVC master interface
    // AP20 and T30 support packet-based interface with new master enable
    // AP15 A01P and A02, AP20 does not support new slave.
    // T30 supports packet based slave.

    s_SocI2cCapsList[1].IsPacketModeI2cSlaveSupport = NV_FALSE;

    s_SocI2cCapsList[2].IsPacketModeI2cSlaveSupport = NV_TRUE;

    // Check for a valid module!
    NV_ASSERT((ModuleId == NvRmModuleID_I2c) || (ModuleId == NvRmModuleID_Dvc));

    // Small modifications for DVC.
    if (ModuleId == NvRmModuleID_Dvc)
    {
        // AP20 and T30 = 1.1.0 here, note their support.
        I2cCapsList[1].Capability = &s_SocI2cCapsList[1];
        // There are only two module versions here so reduce caps size.
        numCaps = 2;
    }

    // Get the compatibility.
    NV_ASSERT_SUCCESS(NvRmModuleGetCapabilities(hDevice,
                NVRM_MODULE_ID(ModuleId, Instance), I2cCapsList,
                numCaps, (void **)&pI2cCaps));

    if (pI2cCaps)
    {
        pI2cSocCaps->IsPacketModeI2cSlaveSupport= pI2cCaps->IsPacketModeI2cSlaveSupport;
    }
    else
    {
        NV_ASSERT(!"Failed to get module compatibility for I2c/Dvc!");
        pI2cSocCaps->IsPacketModeI2cSlaveSupport = NV_FALSE;
    }
}

static NvError PrivI2cSetSpeed(NvRmI2cController *c)
{
    NvRmModuleID ModuleId = NVRM_MODULE_ID(c->ModuleId, c->Instance);

    // The clock source frequency of i2c should be 8 times on interface clock
    // i.e. SCLK frequency.
    NvU32 I2cBusClockFreq = c->clockfreq * 8;
    NvU32 PrefClockFreqToI2cController;

#if defined (CHIP_T30)
    PrefClockFreqToI2cController = I2cBusClockFreq;

    NvRmPowerModuleClockConfig(c->hRmDevice, ModuleId,
                        c->I2cPowerClientId, 1,
                        PrefClockFreqToI2cController,
                        &PrefClockFreqToI2cController, 1, NULL, 0);
#else
    PrefClockFreqToI2cController = I2C_SOURCE_FREQUENCY_FIXED;
    NvRmPowerModuleClockConfig(c->hRmDevice, ModuleId,
                        c->I2cPowerClientId, 1,
                        PrefClockFreqToI2cController,
                        &PrefClockFreqToI2cController, 1, NULL, 0);
    c->setDivisor(c, PrefClockFreqToI2cController, I2cBusClockFreq);
#endif

    return NvSuccess;
}

static NvError PrivI2cConfigurePower(NvRmI2cController *c, NvBool IsEnablePower)
{
    NvError status = NvSuccess;
    NvRmModuleID ModuleId = NVRM_MODULE_ID(c->ModuleId, c->Instance);

    if (IsEnablePower == NV_TRUE)
    {
        status = NvRmPowerVoltageControl(c->hRmDevice, ModuleId,
                    c->I2cPowerClientId, NvRmVoltsUnspecified,
                    NvRmVoltsUnspecified, NULL, 0, NULL);
        if(status == NvSuccess)
        {
            // Enable the clock to the i2c controller
            NV_ASSERT_SUCCESS(NvRmPowerModuleClockControl(c->hRmDevice,
                        ModuleId, c->I2cPowerClientId, NV_TRUE));
        }
    }
    else
    {
       // Disable the clock to the i2c controller
        NV_ASSERT_SUCCESS(NvRmPowerModuleClockControl(c->hRmDevice,
                    ModuleId, c->I2cPowerClientId, NV_FALSE));

        //disable power
        status = NvRmPowerVoltageControl(c->hRmDevice, ModuleId,
                        c->I2cPowerClientId, NvRmVoltsOff, NvRmVoltsOff,
                        NULL, 0, NULL);
    }
    return status;
}

static NvError
I2cOpen(
    NvRmDeviceHandle hRmDevice,
    NvBool IsMasterMode,
    NvU32 IoModule,
    NvU32 instance,
    NvRmI2cHandle *phI2c)
{
    NvError status = NvSuccess;
    NvU32 PrefClockFreq = MAX_I2C_CLOCK_SPEED_KHZ;
    NvU32 Index = instance;
    NvRmModuleID ModuleID = NvRmModuleID_I2c;
    NvRmI2cController *c;
    NvOsMutexHandle hThreadSaftyMutex = NULL;
    NvU32 scl, sda;
    SocI2cCapability SocI2cCaps;
    NV_ASSERT(hRmDevice);
    NV_ASSERT(phI2c);
    NV_ASSERT((IoModule == NvOdmIoModule_I2c) || (IoModule == NvOdmIoModule_I2c_Pmu));

    *phI2c = 0;
    /* If none of the controller is opened, allocate memory for all controllers
    * in the system */
    if (gs_Cont == NULL)
    {
        gs_Cont = gs_I2cControllers;
        MaxI2cControllers = NvRmModuleGetNumInstances(hRmDevice, NvRmModuleID_I2c);
        MaxDvcControllers = NvRmModuleGetNumInstances(hRmDevice, NvRmModuleID_Dvc);
        MaxI2cInstances = MaxI2cControllers + MaxDvcControllers;
        NV_ASSERT(MaxI2cInstances <= MAX_I2C_INSTANCES);
    }
    /* Validate the instance number passed and return the Index of the
     * controller to the caller.
     *
     */
    if (IoModule == NvOdmIoModule_I2c)
    {
        NV_ASSERT(instance < MaxI2cControllers);
        if (instance >= MaxI2cControllers)
            return NvError_NotSupported;

        ModuleID = NvRmModuleID_I2c;
        Index = instance;
    }
    else  if (IoModule == NvOdmIoModule_I2c_Pmu)
    {
        NV_ASSERT(instance < MaxDvcControllers);
        if (instance >= MaxDvcControllers)
            return NvError_NotSupported;

        ModuleID = NvRmModuleID_Dvc;
        Index = MaxI2cControllers + instance;
    }
    else
    {
        NV_ASSERT(!"Invalid IO module");
        return NvError_NotSupported;
    }

    c = &(gs_Cont[Index]);

    // Create the mutex for providing the thread safety for i2c API
    if ((c->NumberOfClientsOpened == 0) && (c->I2cThreadSafetyMutex == NULL))
    {
        status = NvOsMutexCreate(&hThreadSaftyMutex);
        if (status)
            return status;

        if (NvOsAtomicCompareExchange32((NvS32*)&c->I2cThreadSafetyMutex, 0,
                                                    (NvS32)hThreadSaftyMutex)!=0)
        {
            NvOsMutexDestroy(hThreadSaftyMutex);
            hThreadSaftyMutex = NULL;
        }
    }

    NvOsMutexLock(c->I2cThreadSafetyMutex);
    // If no clients are opened yet, initialize the i2c controller
    if (c->NumberOfClientsOpened == 0)
    {
        /* Polulate the controller structure */
        c->hRmDevice = hRmDevice;
        c->OdmIoModule = IoModule;
        c->ModuleId = ModuleID;
        c->Instance = instance;

        c->I2cPowerClientId = 0;
        c->receive  = NULL;
        c->send = NULL;
        c->close = NULL;
        c->GetGpioPins = NULL;
        c->hGpio = NULL;
        c->hSclPin = 0;
        c->hSdaPin = 0;


        I2cGetSocCapabilities(hRmDevice, ModuleID, instance, &SocI2cCaps);
        c->IsPacketModeI2cSlaveSupport = SocI2cCaps.IsPacketModeI2cSlaveSupport;

        /* Call appropriate open function according to the controller
         * supports packet mode or not. If packet mode is supported
         * call AP20RmI2cOpen for packet mode funcitons. Other wise
         * use normal mode */
        if (IsMasterMode)
        {
            status = AP20RmMasterI2cOpen(c);
        }
        else
        {
            if (c->IsPacketModeI2cSlaveSupport)
                status = T30RmSlaveI2cOpen(c);
            else
                NV_ASSERT(0);
        }
        if (status)
            goto open_fail;

        /* Make sure that all the functions are polulated by the HAL driver */
        if (IsMasterMode)
              NV_ASSERT(c->receive && c->send && c->close);
        else
              NV_ASSERT(c->close);

        /* Initalize the GPIO handles */
        if (c->GetGpioPins)
        {
            status = NvRmGpioOpen(c->hRmDevice, &c->hGpio);
            if(status)
                goto gpio_open_fail;
            if ((c->GetGpioPins)(c, c->PinMapConfig, &scl, &sda))
            {
                status = NvRmGpioAcquirePinHandle(c->hGpio, (scl >> 16),
                    (scl & 0xFFFF), &c->hSclPin);
                if (status)
                    goto gpio_acquire_fail;

                status = NvRmGpioAcquirePinHandle(c->hGpio, (sda >> 16),
                    (sda & 0xFFFF), &c->hSdaPin);
                if(status)
                {
                    NvRmGpioReleasePinHandles(c->hGpio, &c->hSclPin, 1);
                    c->hSclPin = 0;
                    goto gpio_acquire_fail;
                }
            }
        }
        status = NvRmPowerRegister(hRmDevice, NULL, &c->I2cPowerClientId);
        if (status != NvSuccess)
            goto power_reg_fail;

        if (instance == 4 && hRmDevice->ChipId.Id != 0x30)
        {
            NvRmPowerModuleClockControl(hRmDevice,
                NvRmModuleID_ClDvfs,
                c->I2cPowerClientId, NV_TRUE);
        }

        /* Enable power rail, enable clock, configure clock to right freq,
         * reset, disable clock, notify to disable power rail.
         *
         *  All of this is done to just reset the controller.
         * */
        PrivI2cConfigurePower(c, NV_TRUE);
        status = NvRmPowerModuleClockConfig(hRmDevice,
                NVRM_MODULE_ID(ModuleID, instance), c->I2cPowerClientId,
                PrefClockFreq, NvRmFreqUnspecified, &PrefClockFreq, 1, NULL, 0);
        if (status != NvSuccess)
            goto clock_config_fail;

        NvRmModuleReset(hRmDevice, NVRM_MODULE_ID(ModuleID, instance));

        PrivI2cConfigurePower(c, NV_FALSE);
    }
    else
    {
        if (c->IsMasterMode != IsMasterMode)
            return NvError_AlreadyAllocated;

        if (!c->IsMasterMode)
            return NvError_AlreadyAllocated;
    }
    c->NumberOfClientsOpened++;
    NvOsMutexUnlock(c->I2cThreadSafetyMutex);

    /*
     * We cannot return handle with a value of 0, as some clients check the
     * handle to ne non-zero. So, to get around that we set MSB bit to 1.
     */
    *phI2c = (NvRmI2cHandle)(Index | 0x80000000);
    return NvSuccess;

clock_config_fail:
    PrivI2cConfigurePower(c, NV_FALSE);
    NvRmPowerUnRegister(hRmDevice, c->I2cPowerClientId);
    c->I2cPowerClientId = 0;

power_reg_fail:
    if (c->hSclPin)
        NvRmGpioReleasePinHandles(c->hGpio, &c->hSclPin, 1);

    if (c->hSdaPin)
        NvRmGpioReleasePinHandles(c->hGpio, &c->hSdaPin, 1);

gpio_acquire_fail:
    NvRmGpioClose(c->hGpio);
gpio_open_fail:
open_fail:
    return status;
}

NvError
NvRmI2cOpen(
    NvRmDeviceHandle hRmDevice,
    NvU32 IoModule,
    NvU32 instance,
    NvRmI2cHandle *phI2c)
{
    return I2cOpen(hRmDevice, NV_TRUE, IoModule, instance, phI2c);
}

NvError
NvRmI2cSlaveOpen(
    NvRmDeviceHandle hRmDevice,
    NvU32 IoModule,
    NvU32 instance,
    NvRmI2cHandle *phI2c)
{
    return I2cOpen(hRmDevice, NV_FALSE, IoModule, instance, phI2c);
}

void NvRmI2cClose(NvRmI2cHandle hI2c)
{
    NvU32 Index;
    NvRmI2cController *c;

    if ((NvU32)hI2c == 0)
        return;

    Index = ((NvU32) hI2c) & 0xFF;
    if (Index < MaxI2cInstances)
    {
        c = &(gs_Cont[Index]);

        NvOsMutexLock(c->I2cThreadSafetyMutex);
        c->NumberOfClientsOpened--;
        if (c->NumberOfClientsOpened == 0)
        {

            if(c->GetGpioPins)
            {
                if (c->hSclPin)
                    NvRmGpioReleasePinHandles(c->hGpio, &c->hSclPin, 1);
                if (c->hSdaPin)
                    NvRmGpioReleasePinHandles(c->hGpio, &c->hSdaPin, 1);
                c->hSdaPin = 0;
                c->hSclPin = 0;
            }
            NvRmGpioClose(c->hGpio);

            /* Unregister the power client ID */
            NvRmPowerUnRegister(c->hRmDevice, c->I2cPowerClientId);
            c->I2cPowerClientId = 0;

            NV_ASSERT(c->close);
            (c->close)(c);

            /* FIXME: There is a race here. After the Mutex is unlocked someone can
            * call NvRmI2cOpen and create the mutex, which will then destropyed
            * here?
            * */
            NvOsMutexUnlock(c->I2cThreadSafetyMutex);
            NvOsMutexDestroy(c->I2cThreadSafetyMutex);
            c->I2cThreadSafetyMutex = NULL;
        }
        else
        {
            NvOsMutexUnlock(c->I2cThreadSafetyMutex);
        }
    }
}

NvError NvRmI2cTransaction(
    NvRmI2cHandle hI2c,
    NvU32 I2cPinMap,
    NvU32 WaitTimeoutInMilliSeconds,
    NvU32 ClockSpeedKHz,
    NvU8 *Data,
    NvU32 DataLength,
    NvRmI2cTransactionInfo * Transaction,
    NvU32 NumOfTransactions)
{
    NvU32 len = 0;
    NvError status;
    NvU32 i;
    NvU32 BytesTransferred = 0;
    NvBool useGpioI2c = NV_FALSE;
    NvRmI2cController* hRmI2cCont;
    NvU32 Index;
    NvU32 RSCount  = 0; // repeat start count
    NvS32 StartTransIndex = -1;

    Index = ((NvU32)hI2c) & 0x7FFFFFFF;

    NV_ASSERT(((NvU32)hI2c) & 0x80000000);
    NV_ASSERT(Index < MaxI2cInstances);
    NV_ASSERT(Transaction);
    NV_ASSERT(Data);
    NV_ASSERT(ClockSpeedKHz <= MAX_I2C_CLOCK_SPEED_KHZ);

    hRmI2cCont = &(gs_Cont[Index]);
    hRmI2cCont->timeout = 100000;

    hRmI2cCont->clockfreq = ClockSpeedKHz;

    NvOsMutexLock(hRmI2cCont->I2cThreadSafetyMutex);

    if ((Transaction[0].Flags & NVRM_I2C_SOFTWARE_CONTROLLER) ||
                            (useGpioI2c == NV_TRUE))
    {
        status = NvRmGpioI2cTransaction(hRmI2cCont, I2cPinMap, Data, DataLength,
                                        Transaction, NumOfTransactions);
        NvOsMutexUnlock(hRmI2cCont->I2cThreadSafetyMutex);
        return status;
    }

    status = PrivI2cConfigurePower(hRmI2cCont, NV_TRUE);
    if (status != NvSuccess)
        goto power_config_fail;
    status = PrivI2cSetSpeed(hRmI2cCont);
    if (status != NvSuccess)
        goto clock_config_fail;
    len = 0;
    StartTransIndex = -1;
    for (i = 0; i < NumOfTransactions; i++)
    {
        hRmI2cCont->Is10BitAddress = Transaction[i].Is10BitAddress;
        hRmI2cCont->NoACK = NV_FALSE;

        if (Transaction[i].Flags & NVRM_I2C_NOACK)
            hRmI2cCont->NoACK = NV_TRUE;

        // Check  whether this transation is repeat start or not
        if (!(Transaction[i].Flags & NVRM_I2C_NOSTOP) && (!RSCount))
        {
            if (Transaction[i].Flags & NVRM_I2C_WRITE)
                status = (hRmI2cCont->send)(hRmI2cCont, Data,
                                        &Transaction[i], &BytesTransferred);
            else
                status = (hRmI2cCont->receive)(hRmI2cCont, Data,
                                        &Transaction[i], &BytesTransferred);
            Data += Transaction[i].NumBytes;
        }
        else
        {
            RSCount++;
            // If transation is repeat start,
            if (Transaction[i].Flags & NVRM_I2C_NOSTOP)
            {
                len += Transaction[i].NumBytes;
                if (StartTransIndex == -1)
                    StartTransIndex = i;
            }
            else
            {
                // i2c transaction with repeat-start
                status = (hRmI2cCont->repeatStart)(hRmI2cCont, Data,
                                    &(Transaction[StartTransIndex]), RSCount);
                Data += len + Transaction[i].NumBytes;
                RSCount = 0;
                len = 0;
                StartTransIndex = -1;
            }
        }
        if (status != NvSuccess)
            break;
    }

clock_config_fail:
    PrivI2cConfigurePower(hRmI2cCont, NV_FALSE);

power_config_fail:
    NvOsMutexUnlock(hRmI2cCont->I2cThreadSafetyMutex);
    return status;
}

NvError
NvRmI2cSlaveStart(
    NvRmI2cHandle hI2c,
    NvU32 I2cPinMap,
    NvU32 ClockSpeedKHz,
    NvU32 Address,
    NvBool IsTenBitAdd,
    NvU32 maxRecvPacketSize,
    NvU8 DummyCharTx)
{
    NvU32 Index;
    NvRmI2cController* hRmI2cCont;
    NvError Err;

    Index = ((NvU32)hI2c) & 0x7FFFFFFF;

    NV_ASSERT(((NvU32)hI2c) & 0x80000000);
    NV_ASSERT(Index < MaxI2cInstances);
    hRmI2cCont = &(gs_Cont[Index]);
    NV_ASSERT(hRmI2cCont->I2cSlaveStart);

    NvOsMutexLock(hRmI2cCont->I2cThreadSafetyMutex);
    hRmI2cCont->clockfreq = ClockSpeedKHz;

    Err = PrivI2cConfigurePower(hRmI2cCont, NV_TRUE);
    if (Err != NvSuccess)
        goto config_power_fail;

    Err = PrivI2cSetSpeed(hRmI2cCont);
    if (Err != NvSuccess)
        goto config_clock_fail;

    Err = hRmI2cCont->I2cSlaveStart(hRmI2cCont, Address, IsTenBitAdd,
                        maxRecvPacketSize, DummyCharTx);
    if (!Err)
    {
        NvOsMutexUnlock(hRmI2cCont->I2cThreadSafetyMutex);
        return Err;
    }
config_clock_fail:
    PrivI2cConfigurePower(hRmI2cCont, NV_FALSE);

config_power_fail:
    NvOsMutexUnlock(hRmI2cCont->I2cThreadSafetyMutex);
    return Err;
}

void NvRmI2cSlaveStop(NvRmI2cHandle hI2c)
{
    NvU32 Index;
    NvRmI2cController* hRmI2cCont;

    Index = ((NvU32)hI2c) & 0x7FFFFFFF;

    NV_ASSERT(((NvU32)hI2c) & 0x80000000);
    NV_ASSERT(Index < MaxI2cInstances);
    hRmI2cCont = &(gs_Cont[Index]);
    NV_ASSERT(hRmI2cCont->I2cSlaveStop);

    NvOsMutexLock(hRmI2cCont->I2cThreadSafetyMutex);
    hRmI2cCont->I2cSlaveStop(hRmI2cCont);

    PrivI2cConfigurePower(hRmI2cCont, NV_FALSE);
    NvOsMutexUnlock(hRmI2cCont->I2cThreadSafetyMutex);
}

NvError
NvRmI2cSlaveRead(
    NvRmI2cHandle hI2c,
    NvU8 *pRxBuffer,
    NvU32 BytesRequested,
    NvU32 *pBytesRead,
    NvU8 MinBytesRead,
    NvU32 Timeoutms)
{
    NvU32 Index;
    NvRmI2cController* hRmI2cCont;

    Index = ((NvU32)hI2c) & 0x7FFFFFFF;

    NV_ASSERT(((NvU32)hI2c) & 0x80000000);
    NV_ASSERT(Index < MaxI2cInstances);
    hRmI2cCont = &(gs_Cont[Index]);
    NV_ASSERT(hRmI2cCont->I2cSlaveRead);

    return hRmI2cCont->I2cSlaveRead(hRmI2cCont, pRxBuffer, BytesRequested, pBytesRead,
                    MinBytesRead, Timeoutms);
}

NvError
NvRmI2cSlaveWriteAsynch(
    NvRmI2cHandle hI2c,
    NvU8 *pTxBuffer,
    NvU32 BytesRequested,
    NvU32 *pBytesWritten)
{
    NvU32 Index;
    NvRmI2cController* hRmI2cCont;

    Index = ((NvU32)hI2c) & 0x7FFFFFFF;

    NV_ASSERT(((NvU32)hI2c) & 0x80000000);
    NV_ASSERT(Index < MaxI2cInstances);
    hRmI2cCont = &(gs_Cont[Index]);
    NV_ASSERT(hRmI2cCont->I2cSlaveWriteAsynch);

    return hRmI2cCont->I2cSlaveWriteAsynch(hRmI2cCont, pTxBuffer, BytesRequested,
                        pBytesWritten);
}

NvError NvRmI2cSlaveGetWriteStatus(NvRmI2cHandle hI2c, NvU32 TimeoutMs, NvU32 *pBytesRemaining)
{
    NvU32 Index;
    NvRmI2cController* hRmI2cCont;

    Index = ((NvU32)hI2c) & 0x7FFFFFFF;

    NV_ASSERT(((NvU32)hI2c) & 0x80000000);
    NV_ASSERT(Index < MaxI2cInstances);
    hRmI2cCont = &(gs_Cont[Index]);
    NV_ASSERT(hRmI2cCont->I2cSlaveGetWriteStatus);
    return hRmI2cCont->I2cSlaveGetWriteStatus(hRmI2cCont, TimeoutMs, pBytesRemaining);
}

void NvRmI2cSlaveFlushWriteBuffer(NvRmI2cHandle hI2c)
{
    NvU32 Index;
    NvRmI2cController* hRmI2cCont;

    Index = ((NvU32)hI2c) & 0x7FFFFFFF;

    NV_ASSERT(((NvU32)hI2c) & 0x80000000);
    NV_ASSERT(Index < MaxI2cInstances);
    hRmI2cCont = &(gs_Cont[Index]);
    NV_ASSERT(hRmI2cCont->I2cSlaveFlushWriteBuffer);
    hRmI2cCont->I2cSlaveFlushWriteBuffer(hRmI2cCont);
}

NvU32 NvRmI2cSlaveGetNACKCount(NvRmI2cHandle hI2c, NvBool IsResetCount)
{
    NvU32 Index;
    NvRmI2cController* hRmI2cCont;

    Index = ((NvU32)hI2c) & 0x7FFFFFFF;

    NV_ASSERT(((NvU32)hI2c) & 0x80000000);
    NV_ASSERT(Index < MaxI2cInstances);
    hRmI2cCont = &(gs_Cont[Index]);
    NV_ASSERT(hRmI2cCont->I2cSlaveGetNACKCount);
    return hRmI2cCont->I2cSlaveGetNACKCount(hRmI2cCont, IsResetCount);
}
