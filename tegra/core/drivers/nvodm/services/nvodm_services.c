/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"
#include "nvrm_gpio.h"
#include "nvrm_spi.h"
#include "nvrm_i2c.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_query_gpio.h"
#include "nvodm_query_discovery.h"
#include "nvrm_pmu.h"
#include "nvrm_keylist.h"
#include "nvrm_pwm.h"
#include "nvrm_power.h"
#include "nvrm_analog.h"
#include "nvrm_pinmux.h"
#include "nvrm_owr.h"
#include "nvddk_fuse.h"

typedef struct NvOdmServicesGpioRec
{
    NvRmDeviceHandle hRmDev;
    NvRmGpioHandle hGpio;
} NvOdmServicesGpio;

typedef struct NvOdmServicesSpiRec
{
    NvRmDeviceHandle hRmDev;
    NvRmSpiHandle hSpi;
    NvOdmSpiPinMap SpiPinMap;
} NvOdmServicesSpi;

typedef struct NvOdmServicesI2cRec
{
    NvRmDeviceHandle hRmDev;
    NvRmI2cHandle hI2c;
    NvOdmI2cPinMap I2cPinMap;
} NvOdmServicesI2c;

typedef struct NvOdmServicesPwmRec
{
    NvRmDeviceHandle hRmDev;
    NvRmPwmHandle hPwm;
} NvOdmServicesPwm;

typedef struct NvOdmServicesOwrRec
{
    NvRmDeviceHandle hRmDev;
    NvRmOwrHandle hOwr;
    NvOdmOwrPinMap OwrPinMap;
} NvOdmServicesOwr;

// ----------------------- GPIO IMPLEMENTATION ------------

NvOdmServicesGpioHandle NvOdmGpioOpen(void)
{
    NvError e;
    NvOdmServicesGpio *pOdmServices = NULL;

    // Allocate memory for the handle.
    pOdmServices = NvOsAlloc(sizeof(NvOdmServicesGpio));
    if (!pOdmServices)
        return NULL;
    NvOsMemset(pOdmServices, 0, sizeof(NvOdmServicesGpio));

    // Open RM device and RM GPIO handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pOdmServices->hRmDev, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmGpioOpen(
        pOdmServices->hRmDev, &pOdmServices->hGpio));

    return pOdmServices;

fail:
    NvOdmGpioClose(pOdmServices);
    return NULL;
}

void NvOdmGpioClose(NvOdmServicesGpioHandle hOdmGpio)
{
    if (!hOdmGpio)
        return;
    NvRmGpioClose(hOdmGpio->hGpio);
    NvRmClose(hOdmGpio->hRmDev);
    NvOsFree(hOdmGpio);
}

NvOdmGpioPinHandle 
NvOdmGpioAcquirePinHandle(NvOdmServicesGpioHandle hOdmGpio, 
        NvU32 Port, NvU32 Pin)
{
    NvError e;
    NvRmGpioPinHandle hGpioPin;

    if (!hOdmGpio || Port == NVODM_GPIO_INVALID_PORT || Pin == NVODM_GPIO_INVALID_PIN)
        return NULL;

    NV_CHECK_ERROR_CLEANUP(NvRmGpioAcquirePinHandle(
            hOdmGpio->hGpio, Port, Pin, &hGpioPin)); 

    return (NvOdmGpioPinHandle)hGpioPin;

fail:
    return NULL;
}

void
NvOdmGpioReleasePinHandle(NvOdmServicesGpioHandle hOdmGpio, 
        NvOdmGpioPinHandle hPin)
{
    NvRmGpioPinHandle hRmPin = (NvRmGpioPinHandle)hPin;
    if (hOdmGpio && hPin)
        NvRmGpioReleasePinHandles(hOdmGpio->hGpio, &hRmPin, 1);
}

void
NvOdmGpioSetState(
    NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvU32 PinValue)
{
    NvRmGpioPinState val = (NvRmGpioPinState)PinValue;
    NvRmGpioPinHandle hRmPin = (NvRmGpioPinHandle)hGpioPin;
    if (hOdmGpio == NULL || hGpioPin == NULL)
        return;

    NvRmGpioWritePins( hOdmGpio->hGpio, &hRmPin, &val, 1);
}

void
NvOdmGpioGetState(
    NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvU32 *PinValue)
{
    NvRmGpioPinState val;
    NvRmGpioPinHandle hRmPin = (NvRmGpioPinHandle)hGpioPin;

    if (hOdmGpio == NULL || hGpioPin == NULL)
        return;

    NvRmGpioReadPins(hOdmGpio->hGpio, &hRmPin, &val, 1);
    *PinValue = val;
}

void
NvOdmGpioConfig( 
    NvOdmServicesGpioHandle hOdmGpio, 
    NvOdmGpioPinHandle hGpioPin,
    NvOdmGpioPinMode mode)
{
    NvRmGpioPinHandle hRmPin = (NvRmGpioPinHandle)hGpioPin;
    if (hOdmGpio == NULL || hGpioPin == NULL)
        return;

    NV_ASSERT_SUCCESS(
        NvRmGpioConfigPins(hOdmGpio->hGpio, &hRmPin, 1, (NvRmGpioPinMode)mode)
    );
}

NvBool
NvOdmGpioInterruptRegister(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmServicesGpioIntrHandle *hGpioIntr,
    NvOdmGpioPinHandle hGpioPin,
    NvOdmGpioPinMode Mode, 
    NvOdmInterruptHandler Callback,
    void *arg,
    NvU32 DebounceTime)
{
    NvRmGpioInterruptHandle handle;
    NvError err;

    err = NvRmGpioInterruptRegister(
        hOdmGpio->hGpio,
        hOdmGpio->hRmDev,
        (NvRmGpioPinHandle)hGpioPin,
        (NvOsInterruptHandler)Callback,
        (NvRmGpioPinMode)Mode,
        arg,
        &handle,
        DebounceTime);

    if (err == NvSuccess)
    {
        *hGpioIntr = (NvOdmServicesGpioIntrHandle)handle;
        err = NvRmGpioInterruptEnable(handle);
        if (err != NvSuccess)
        {
            NvRmGpioInterruptUnregister(hOdmGpio->hGpio,
                hOdmGpio->hRmDev,
                (NvRmGpioInterruptHandle)handle);
            *hGpioIntr = NULL;
            return NV_FALSE;
        }
        return NV_TRUE;
    }
    else
    {
        *hGpioIntr = NULL;
        return NV_FALSE;
    }
}

void
NvOdmGpioInterruptMask(NvOdmServicesGpioIntrHandle handle, NvBool mask)
{
    NvRmGpioInterruptMask( (NvRmGpioInterruptHandle)handle, mask);
}

void
NvOdmGpioInterruptUnregister(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvOdmServicesGpioIntrHandle handle)
{
    NvRmGpioInterruptUnregister(hOdmGpio->hGpio,
            hOdmGpio->hRmDev,
            (NvRmGpioInterruptHandle)handle);
}

void NvOdmGpioInterruptDone( NvOdmServicesGpioIntrHandle handle )
{
    NvRmGpioInterruptDone((NvRmGpioInterruptHandle)handle);
}

NvBool
NvOdmExternalClockConfig(
    NvU64 Guid,
    NvBool EnableTristate,
    NvU32 *pInstances,
    NvU32 *pFrequencies,
    NvU32 *pNum)
{
    const NvOdmPeripheralConnectivity *pConn = NULL;
    const NvOdmIoAddress *pIo = NULL;
    NvRmDeviceHandle hRmDev = NULL;
    NvBool result = NV_TRUE;
    NvU32 i;
    NvU32 ClockListEntries = 0;

    NV_ASSERT(pInstances);
    NV_ASSERT(pFrequencies);
    NV_ASSERT(pNum);

    pConn = NvOdmPeripheralGetGuid(Guid);
    if (NvRmOpen(&hRmDev,0)!=NvSuccess)
        return NV_FALSE;

    if (pConn && pConn->AddressList && pConn->NumAddress)
    {
        NvBool found = NV_FALSE;
        pIo = pConn->AddressList;
        for (i=0; i<pConn->NumAddress; pIo++, i++)
        {
            if (pIo->Interface == NvOdmIoModule_ExternalClock)
            {
                found = NV_TRUE;
                pInstances[ClockListEntries] = pIo->Instance;
                ClockListEntries++;
            }
        }
        result = result && found;
    }
    else
        result = NV_FALSE;

    *pNum = ClockListEntries;
    NvRmClose(hRmDev);
    return result;
}

 NvBool NvOdmExternalPheriphPinConfig(
    NvU64 Guid,
    NvU32 Instance,
    NvBool EnableTristate,
    NvU32 MinFreq,
    NvU32 MaxFreq,
    NvU32 TargetFreq,
    NvU32 * CurrentFreq,
    NvU32 flags)
{
    const NvOdmPeripheralConnectivity *pConn = NULL;
    const NvOdmIoAddress *pIo = NULL;
    NvRmDeviceHandle hRmDev = NULL;
    NvU32 i = 0;
    NvBool result = NV_TRUE;
    NvError e;
    NvBool found = NV_FALSE;

    pConn = NvOdmPeripheralGetGuid(Guid);
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&hRmDev, 0));

    if (flags == NVODM_STANDARD_AUDIO_FREQ)
        flags = NvRmClockConfig_AudioAdjust;

    if (pConn && pConn->AddressList && pConn->NumAddress)
    {
        pIo = pConn->AddressList;
        for (i = 0; i<pConn->NumAddress; pIo++, i++)
        {
            if ((pIo->Interface == NvOdmIoModule_ExternalClock) && (pIo->Instance == Instance))
            {
                found = NV_TRUE;
                break;
            }
        }

        // If it is not found return false
        if (found)
        {
            NV_CHECK_ERROR_CLEANUP(NvRmPowerModuleClockConfig(hRmDev,
                    NVRM_MODULE_ID(NvRmModuleID_ExtPeriphClk, Instance),
                    0, MinFreq, MaxFreq, &TargetFreq, 1, CurrentFreq, flags));

            result = NV_TRUE;
        }
        else
            result = NV_FALSE;
    }
    else
    {
        result = NV_FALSE;
    }
fail:
    if (hRmDev)
        NvRmClose(hRmDev);
    return result;
}

// ----------------------- I2C IMPLEMENTATION ------------

// Maximum number of bytes that can be sent between the i2c start and stop conditions
#define NVODM_I2C_PACKETSIZE   8

// Maximum number of bytes that can be sent between the i2c start and repeat start condition.
#define NVODM_I2C_REPEAT_START_PACKETSIZE   4

NvOdmServicesI2cHandle
NvOdmI2cOpen(
    NvOdmIoModule OdmIoModuleId,
    NvU32 instance)
{
    NvError e;
    NvOdmServicesI2c *pOdmServices = NULL;

    if (OdmIoModuleId != NvOdmIoModule_I2c &&
        OdmIoModuleId != NvOdmIoModule_I2c_Pmu)
        return NULL;
    
    // Allocate memory for the handle.
    pOdmServices = NvOsAlloc(sizeof(NvOdmServicesI2c));
    if (!pOdmServices)
        return NULL;
    NvOsMemset(pOdmServices, 0, sizeof(NvOdmServicesI2c));

    // Open RM device and RM I2C handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pOdmServices->hRmDev, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmI2cOpen(
        pOdmServices->hRmDev, OdmIoModuleId, instance, &pOdmServices->hI2c));

    pOdmServices->I2cPinMap = 0;
    return pOdmServices;

fail:
    NvOdmI2cClose(pOdmServices);
    return NULL;
}

NvOdmServicesI2cHandle
NvOdmI2cPinMuxOpen(
    NvOdmIoModule OdmIoModuleId,
    NvU32 instance,
    NvOdmI2cPinMap PinMap)
{
    NvError e;
    NvOdmServicesI2c *pOdmServices = NULL;

    if (OdmIoModuleId != NvOdmIoModule_I2c &&
        OdmIoModuleId != NvOdmIoModule_I2c_Pmu)
        return NULL;

    // Allocate memory for the handle.
    pOdmServices = NvOsAlloc(sizeof(NvOdmServicesI2c));
    if (!pOdmServices)
        return NULL;
    NvOsMemset(pOdmServices, 0, sizeof(NvOdmServicesI2c));

    // Open RM device and RM I2C handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pOdmServices->hRmDev, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmI2cOpen(
        pOdmServices->hRmDev, OdmIoModuleId, instance, &pOdmServices->hI2c));

    pOdmServices->I2cPinMap = PinMap;
    return pOdmServices;

fail:
    NvOdmI2cClose(pOdmServices);
    return NULL;
}

void NvOdmI2cClose(NvOdmServicesI2cHandle hOdmI2c)
{
    if (!hOdmI2c)
        return;
    NvRmI2cClose(hOdmI2c->hI2c);
    NvRmClose(hOdmI2c->hRmDev);
    NvOsFree(hOdmI2c);
}


NvOdmI2cStatus
NvOdmI2cTransaction(
    NvOdmServicesI2cHandle hOdmI2c,
    NvOdmI2cTransactionInfo *TransactionInfo,
    NvU32 NumberOfTransactions,
    NvU32 ClockSpeedKHz,
    NvU32 WaitTimeoutInMilliSeconds)
{
    NvU32 len = 0;
    NvU8 *buffer = NULL;
    NvU8 stack_buffer[64];
    NvRmI2cTransactionInfo stack_t[8];
    NvRmI2cTransactionInfo *t = NULL;
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvError err;
    NvU32 i;
    NvU8 *tempBuffer;

    NV_ASSERT(hOdmI2c);
    NV_ASSERT(TransactionInfo);
    NV_ASSERT(NumberOfTransactions);
    NV_ASSERT(WaitTimeoutInMilliSeconds);

    for (i=0; i < NumberOfTransactions; i++)
    {
        len += TransactionInfo[i].NumBytes;
    }

    if (len > 64)
    {
        buffer = NvOsAlloc(len);
        if (!buffer)
        {
            status = NvOdmI2cStatus_InternalError;
            goto fail;
        }
    } else
    {
        buffer = stack_buffer;
    }

    if (NumberOfTransactions > 8)
    {
        t = NvOsAlloc(sizeof(NvRmI2cTransactionInfo) * NumberOfTransactions);
        if (!t)
        {
            status = NvOdmI2cStatus_InternalError;
            goto fail;
        }
    } else
    {
        t = stack_t;
    }

    NvOsMemset(buffer, 0, len);
    NvOsMemset(t, 0, sizeof(NvRmI2cTransactionInfo) * NumberOfTransactions);

    tempBuffer = buffer;
    for (i=0; i < NumberOfTransactions; i++)
    {
        if ( TransactionInfo[i].Flags & NVODM_I2C_SOFTWARE_CONTROLLER )
        {
            t[i].Flags |= NVRM_I2C_SOFTWARE_CONTROLLER;
        } 

        if ( TransactionInfo[i].Flags & NVODM_I2C_USE_REPEATED_START )
        {
            t[i].Flags |= NVRM_I2C_NOSTOP;
        } 

        if ( TransactionInfo[i].Flags & NVODM_I2C_NO_ACK )
        {
            t[i].Flags |= NVRM_I2C_NOACK;
        } 

        if ( TransactionInfo[i].Flags & NVODM_I2C_IS_WRITE )
        {
            t[i].Flags |= NVRM_I2C_WRITE;
            /* Copy the data */
            NvOsMemcpy(tempBuffer, TransactionInfo[i].Buf, TransactionInfo[i].NumBytes);
        } else
        {
            t[i].Flags |= NVRM_I2C_READ;
        }

        tempBuffer += TransactionInfo[i].NumBytes;
        t[i].NumBytes = TransactionInfo[i].NumBytes;
        t[i].Is10BitAddress = (NvBool)(TransactionInfo[i].Flags & NVODM_I2C_IS_10_BIT_ADDRESS);
        t[i].Address = (TransactionInfo[i].Address) & ~0x1;
    }

    err = NvRmI2cTransaction(hOdmI2c->hI2c, 
            hOdmI2c->I2cPinMap,
            WaitTimeoutInMilliSeconds,
            ClockSpeedKHz,
            buffer,
            len,
            t,
            NumberOfTransactions);
    if (err != NvSuccess)
    {    
        switch ( err )
        {
            case NvError_I2cDeviceNotFound:
                status =  NvOdmI2cStatus_SlaveNotFound;
                break;
            case NvError_I2cReadFailed:
                status = NvOdmI2cStatus_ReadFailed;
                break;
            case NvError_I2cWriteFailed:
                status = NvOdmI2cStatus_WriteFailed;
                break;
            case NvError_I2cArbitrationFailed:
                status = NvOdmI2cStatus_ArbitrationFailed;
                break;
            case NvError_I2cInternalError:
                status = NvOdmI2cStatus_InternalError;
                break;
            case NvError_Timeout:
            default:
                status = NvOdmI2cStatus_Timeout;
                break;
        }
        goto fail;
    }

    tempBuffer = buffer;
    for (i=0; i < NumberOfTransactions; i++)
    {
        if (t[i].Flags & NVRM_I2C_READ)
        {
            NvOsMemcpy(TransactionInfo[i].Buf, tempBuffer, TransactionInfo[i].NumBytes);
        }
        tempBuffer += TransactionInfo[i].NumBytes;
    }

fail:

    if (t != NULL && t != stack_t)
    {
        NvOsFree(t);
    }

    if (buffer != NULL && buffer != stack_buffer)
    {
        NvOsFree(buffer);
    }

    return status;
}

NvOdmServicesSpiHandle NvOdmSpiOpen(NvOdmIoModule OdmIoModule, NvU32 ControllerId)
{
    NvError e;
    NvOdmServicesSpi *pOdmServices;

    // Supporting the odm_sflash and odm_spi only.
    if (OdmIoModule != NvOdmIoModule_Sflash &&
        OdmIoModule != NvOdmIoModule_Spi)
        return NULL;

    // Allocate memory for the handle.
    pOdmServices = NvOsAlloc(sizeof(NvOdmServicesSpi));
    if (!pOdmServices)
        return NULL;
    NvOsMemset(pOdmServices, 0, sizeof(NvOdmServicesSpi));

    // Open RM device and RM SPI handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pOdmServices->hRmDev, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmSpiOpen(
        pOdmServices->hRmDev, OdmIoModule, ControllerId, NV_TRUE, &pOdmServices->hSpi));

    pOdmServices->SpiPinMap = 0;
    return pOdmServices;

fail:
    NvOdmSpiClose(pOdmServices);
    return NULL;
}

NvOdmServicesSpiHandle
NvOdmSpiPinMuxOpen(NvOdmIoModule OdmIoModule, 
                   NvU32 ControllerId, 
                   NvOdmSpiPinMap PinMap)
{
    NvError e;
    NvOdmServicesSpi *pOdmServices;

    if (OdmIoModule != NvOdmIoModule_Sflash &&
        OdmIoModule != NvOdmIoModule_Spi)
        return NULL;

    // Allocate memory for the handle.
    pOdmServices = NvOsAlloc(sizeof(NvOdmServicesSpi));
    if (!pOdmServices)
        return NULL;
    NvOsMemset(pOdmServices, 0, sizeof(NvOdmServicesSpi));

    // Open RM device and RM SPI handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pOdmServices->hRmDev, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmSpiOpen(
        pOdmServices->hRmDev, OdmIoModule, ControllerId, NV_TRUE, &pOdmServices->hSpi));

    pOdmServices->SpiPinMap = PinMap;

    return pOdmServices;

fail:
    NvOdmSpiClose(pOdmServices);
    return NULL;
}

void NvOdmSpiClose(NvOdmServicesSpiHandle hOdmSpi)
{
    if (!hOdmSpi)
        return;

    // clean up
    NvRmSpiClose(hOdmSpi->hSpi);
    NvRmClose(hOdmSpi->hRmDev);
    NvOsFree(hOdmSpi);
}

void
NvOdmSpiTransaction(
    NvOdmServicesSpiHandle hOdmSpi,
    NvU32 ChipSelect,
    NvU32 ClockSpeedInKHz,
    NvU8 *ReadBuf,
    const NvU8 *WriteBuf,
    NvU32 Size,
    NvU32 PacketSize)
{
    NvRmSpiTransaction(hOdmSpi->hSpi, hOdmSpi->SpiPinMap, ChipSelect,
        ClockSpeedInKHz, ReadBuf, (NvU8 *)WriteBuf, Size, PacketSize);
}

void
NvOdmSpiSetSignalMode(
    NvOdmServicesSpiHandle hOdmSpi,
    NvU32 ChipSelectId,
    NvOdmQuerySpiSignalMode SpiSignalMode)
{
    NvRmSpiSetSignalMode(hOdmSpi->hSpi, ChipSelectId, SpiSignalMode);
}



NvOdmServicesPmuHandle NvOdmServicesPmuOpen(void)
{
    NvRmDeviceHandle hRmDev;

    if (NvRmOpen(&hRmDev, 0) != NvError_Success)
    {
        return (NvOdmServicesPmuHandle)0;
    }

    return (NvOdmServicesPmuHandle)hRmDev;
}

void NvOdmServicesPmuClose(NvOdmServicesPmuHandle handle)
{
    NvRmClose((NvRmDeviceHandle)handle);
    return;
}

void NvOdmServicesPmuGetCapabilities( 
        NvOdmServicesPmuHandle handle,
        NvU32 vddId, 
        NvOdmServicesPmuVddRailCapabilities *pCapabilities )
{
    NvRmDeviceHandle hRmDev = (NvRmDeviceHandle)handle;

    NV_ASSERT(sizeof(NvRmPmuVddRailCapabilities) == 
            sizeof(NvOdmServicesPmuVddRailCapabilities));

    NvRmPmuGetCapabilities(hRmDev, vddId, 
        (NvRmPmuVddRailCapabilities *)pCapabilities);
    return;
}

void NvOdmServicesPmuGetVoltage( 
        NvOdmServicesPmuHandle handle,
        NvU32 vddId, 
        NvU32 * pMilliVolts )
{
    NvRmDeviceHandle hRmDev = (NvRmDeviceHandle)handle;
    NvRmPmuGetVoltage(hRmDev, vddId, pMilliVolts);
}

void NvOdmServicesPmuSetVoltage( 
        NvOdmServicesPmuHandle handle,
        NvU32 vddId, 
        NvU32 MilliVolts, 
        NvU32 * pSettleMicroSeconds )
{
    NvRmDeviceHandle hRmDev = (NvRmDeviceHandle)handle;
    NvRmPmuSetVoltage(hRmDev, vddId, MilliVolts, pSettleMicroSeconds);
}

void NvOdmServicesPmuSetSocRailPowerState(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId, 
        NvBool Enable )
{
    NvRmDeviceHandle hRmDev = (NvRmDeviceHandle)handle;
    NvRmPmuSetSocRailPowerState(hRmDev, vddId, Enable);
}

NvBool 
NvOdmServicesPmuGetBatteryStatus(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvU8 * pStatus)
{
    return NvRmPmuGetBatteryStatus(
        (NvRmDeviceHandle)handle,
        (NvRmPmuBatteryInstance)batteryInst,
        pStatus);
}

NvBool
NvOdmServicesPmuGetBatteryData(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvOdmServicesPmuBatteryData * pData)
{
    return NvRmPmuGetBatteryData(
        (NvRmDeviceHandle)handle,
        (NvRmPmuBatteryInstance)batteryInst,
        (NvRmPmuBatteryData *)pData);
}

void
NvOdmServicesPmuGetBatteryFullLifeTime(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvU32 * pLifeTime)
{
    NvRmPmuGetBatteryFullLifeTime(
        (NvRmDeviceHandle)handle,
        (NvRmPmuBatteryInstance)batteryInst,
        pLifeTime);
}

void
NvOdmServicesPmuGetBatteryChemistry(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvOdmServicesPmuBatteryChemistry * pChemistry)
{
    NvRmPmuGetBatteryChemistry(
        (NvRmDeviceHandle)handle,
        (NvRmPmuBatteryInstance)batteryInst,
        (NvRmPmuBatteryChemistry *)pChemistry);
}

void 
NvOdmServicesPmuSetChargingCurrentLimit( 
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuChargingPath ChargingPath,
    NvU32 ChargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    NvRmPmuSetChargingCurrentLimit(
        (NvRmDeviceHandle)handle,
        (NvRmPmuChargingPath)ChargingPath,
        ChargingCurrentLimitMa,
        ChargerType);
}

// ----------------------- PWM IMPLEMENTATION ------------

NvOdmServicesPwmHandle NvOdmPwmOpen(void)
{
    NvError e;
    NvOdmServicesPwm *pOdmServices = NULL;

    // Allocate memory for the handle.
    pOdmServices = NvOsAlloc(sizeof(NvOdmServicesPwm));
    if (!pOdmServices)
        return NULL;
    NvOsMemset(pOdmServices, 0, sizeof(NvOdmServicesPwm));

    // Open RM device and RM PWM handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pOdmServices->hRmDev, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmPwmOpen(
        pOdmServices->hRmDev, &pOdmServices->hPwm));

    return pOdmServices;

fail:
    NvOdmPwmClose(pOdmServices);
    return NULL;
}

void NvOdmPwmClose(NvOdmServicesPwmHandle hOdmPwm)
{
    if (!hOdmPwm)
        return;
    NvRmPwmClose(hOdmPwm->hPwm);
    NvRmClose(hOdmPwm->hRmDev);
    NvOsFree(hOdmPwm);
}

void
NvOdmPwmConfig(NvOdmServicesPwmHandle hOdmPwm,
    NvOdmPwmOutputId OutputId,
    NvOdmPwmMode Mode,            
    NvU32 DutyCycle,
    NvU32 *pRequestedFreqHzOrPeriod,
    NvU32 *pCurrentFreqHzOrPeriod)
{
    NvU32 RequestedFreqHzOrPeriod = 0;

    if (pRequestedFreqHzOrPeriod == NULL)
        RequestedFreqHzOrPeriod = NvRmFreqMaximum;
    else
        RequestedFreqHzOrPeriod = *pRequestedFreqHzOrPeriod;

    NvRmPwmConfig(hOdmPwm->hPwm,
        (NvRmPwmOutputId)OutputId, 
        (NvRmPwmMode)Mode,
        DutyCycle,
        RequestedFreqHzOrPeriod,
        pCurrentFreqHzOrPeriod);
}

// ----------------------- OWR IMPLEMENTATION ------------
NvOdmServicesOwrHandle NvOdmOwrOpen(NvU32 instance)
{
    NvError e;
    NvOdmServicesOwr *pOdmServices = NULL;

    // Allocate memory for the handle.
    pOdmServices = NvOsAlloc(sizeof(NvOdmServicesOwr));
    if (!pOdmServices)
        return NULL;
    NvOsMemset(pOdmServices, 0, sizeof(NvOdmServicesOwr));

    // Open RM device and RM OWR handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pOdmServices->hRmDev, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmOwrOpen(
        pOdmServices->hRmDev, instance, &pOdmServices->hOwr));

    pOdmServices->OwrPinMap = 0;
    return pOdmServices;

fail:
    NvOdmOwrClose(pOdmServices);
    return NULL;
}

NvOdmServicesOwrHandle 
NvOdmOwrPinMuxOpen(
    NvU32 instance,
    NvOdmOwrPinMap PinMap)
{
    NvError e;
    NvOdmServicesOwr *pOdmServices = NULL;

    // Allocate memory for the handle.
    pOdmServices = NvOsAlloc(sizeof(NvOdmServicesOwr));
    if (!pOdmServices)
        return NULL;
    NvOsMemset(pOdmServices, 0, sizeof(NvOdmServicesOwr));

    // Open RM device and RM OWR handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pOdmServices->hRmDev, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmOwrOpen(
        pOdmServices->hRmDev, instance, &pOdmServices->hOwr));

    pOdmServices->OwrPinMap = PinMap;
    return pOdmServices;

fail:
    NvOdmOwrClose(pOdmServices);
    return NULL;
}
void NvOdmOwrClose(NvOdmServicesOwrHandle hOdmOwr)
{
    if (!hOdmOwr)
        return;
    NvRmOwrClose(hOdmOwr->hOwr);
    NvRmClose(hOdmOwr->hRmDev);
    NvOsFree(hOdmOwr);
}

NvOdmOwrStatus NvOdmOwrTransaction(
    NvOdmServicesOwrHandle hOdmOwr,
    NvOdmOwrTransactionInfo *TransactionInfo,
    NvU32 NumberOfTransactions)
{
    NvU32 len = 0;
    NvU8 *buffer = NULL;
    NvRmOwrTransactionInfo *t = NULL;
    NvOdmOwrStatus status = NvOdmOwrStatus_Success;
    NvError err;
    NvU32 i;
    NvU8 *tempBuffer;

    NV_ASSERT(hOdmOwr);
    NV_ASSERT(TransactionInfo);
    NV_ASSERT(NumberOfTransactions);

    for (i=0; i < NumberOfTransactions; i++)
    {
        len += TransactionInfo[i].NumBytes;
    }
    
    buffer = NvOsAlloc(len);
    if (!buffer)
    {
        status = NvOdmOwrStatus_InternalError;
        goto fail;
    }

    t = NvOsAlloc(sizeof(NvRmOwrTransactionInfo) * NumberOfTransactions);
    if (!t)
    {
        status = NvOdmOwrStatus_InternalError;
        goto fail;
    }
    NvOsMemset(buffer, 0, len);
    NvOsMemset(t, 0, sizeof(NvRmOwrTransactionInfo) * NumberOfTransactions);

    tempBuffer = buffer;
    for (i = 0; i < NumberOfTransactions; i++)
    {
        if (TransactionInfo[i].Flags & NVODM_OWR_MEMORY_WRITE)
        {
            t[i].Flags = NvRmOwr_MemWrite;
            /* Copy the data */
            NvOsMemcpy(tempBuffer, TransactionInfo[i].Buf, TransactionInfo[i].NumBytes);
        }

        if (TransactionInfo[i].Flags & NVODM_OWR_MEMORY_READ)
        {
            t[i].Flags = NvRmOwr_MemRead;
        }
        
        if (TransactionInfo[i].Flags & NVODM_OWR_ADDRESS_READ)
        {
            t[i].Flags = NvRmOwr_ReadAddress;
        }
        tempBuffer += TransactionInfo[i].NumBytes;
        t[i].NumBytes = TransactionInfo[i].NumBytes;
        t[i].Offset = TransactionInfo[i].Offset;
        t[i].Address = TransactionInfo[i].Address;
    }
    
    err = NvRmOwrTransaction(hOdmOwr->hOwr,
            hOdmOwr->OwrPinMap,
            buffer,
            len,
            t,
            NumberOfTransactions);
    if (err != NvSuccess)
    {
        switch ( err )
        {
            case NvError_NotSupported:
                status = NvOdmOwrStatus_NotSupported;
                break;
            case NvError_InvalidState:
                status = NvOdmOwrStatus_InvalidState;
                break;
            default:
                status = NvOdmOwrStatus_InternalError;
                break;
        }
        goto fail;
    }

    tempBuffer = buffer;
    for (i=0; i < NumberOfTransactions; i++)
    {
        if ((t[i].Flags == NvRmOwr_MemRead) ||
            (t[i].Flags == NvRmOwr_ReadAddress))
        {
            NvOsMemcpy(TransactionInfo[i].Buf, tempBuffer, TransactionInfo[i].NumBytes);
            tempBuffer += TransactionInfo[i].NumBytes;
        }
    }

fail:
   
    if (t != NULL)
    {
        NvOsFree(t);
    }

    if (buffer != NULL)
    {
        NvOsFree(buffer);
    }

    return status;
}

NvBool 
NvOdmPlayWave(
    NvU32 *pWaveDataBuffer, 
    NvU32 BufferSizeInBytes, 
    NvOdmWaveHalfBufferCompleteFxn HalfBuffCompleteCallback,
    NvOdmWaveDataProp *pWaveDataProp,
    void *pContext)
{
    NV_ASSERT(!"Not Supported ");
    return NV_FALSE;
}

NvOdmAudioConnectionHandle 
NvOdmAudioConnectionOpen(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmDapConnectionIndex ConnectionType,
    NvOdmAudioSignalType SignalType,
    NvU32 PortIndex,
    NvBool IsEnable)
{
    NV_ASSERT(!"Not Supported ");
    return NULL;
}

void 
NvOdmAudioConnectionClose(
    NvOdmAudioConnectionHandle hAudioConnection)
{
}

void NvOdmDispOpen(
    NvOdmServicesDispHandle *hDisplay,
    const NvOdmDispBufferAttributes *bufAttributes,
    const NvU8 *pBuffer)
{
    NV_ASSERT(!"Not Supported ");
}

void NvOdmDispClose(NvOdmServicesDispHandle hDisplay)
{
    NV_ASSERT(!"Not Supported ");
}

void NvOdmDispSendBuffer(
    const NvOdmServicesDispHandle hDisplay,
    const NvOdmDispBufferAttributes *bufAttributes,
    const NvU8 *pBuffer)
{
    NV_ASSERT(!"Not Supported ");
}

void NvOdmEnableOtgCircuitry(NvBool Enable)
{
    // Rm analog interface calls related to usb are deleted. This API does nothing.
    // This API should not be called for usb phy related operations
    return;
}

NvBool NvOdmUsbIsConnected(void)
{
    NV_ASSERT(!"Not Supported ");
    return NV_FALSE;
}

NvOdmUsbChargerType NvOdmUsbChargingType(NvU32 Instance)
{
    NV_ASSERT(!"Not Supported ");
    return NvOdmUsbChargerType_Dummy;
}

