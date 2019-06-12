/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_backlight.h"
#include "nvodm_disp.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvassert.h"
#include "bl_pcf50626.h"

#if NV_DEBUG
#define ASSERT_SUCCESS( expr ) \
    do { \
        NvBool b = (expr); \
        NV_ASSERT( b == NV_TRUE ); \
    } while( 0 )
#else
#define ASSERT_SUCCESS( expr ) \
    do { \
        (void)(expr); \
    } while( 0 )
#endif

static NvOdmServicesI2cHandle s_hOdmI2c = NULL;
static NvBool s_Configured = NV_FALSE;

/**
 * GPIO Structure -- On Concorde, the display utilizes an 
 * external GPIO for backlight enable/disable which comes from 
 * the PMU (PCF50626).  However, an on-board GPIO from the APxx 
 * could be used just as easily without changing this 
 * implementation--only the entry in the Peripheral Discovery DB
 * would need to be updated.
 *  
 * @see nvodm_gpio_ext.h 
 */
typedef struct DeviceGpioRec
{
    NvOdmPeripheralConnectivity const * pPeripheralConnectivity;
    NvOdmServicesGpioHandle hGpio;
    NvOdmGpioPinHandle hPin;
} DeviceGpio;

static DeviceGpio s_DeviceGpio = {
    NULL,
    NULL,
    NULL,
};

#define PCF50626_I2C_SPEED_KHZ  400
#define PCF50626_DEVICE_ADDR    0xE0
#define PCF50626_PWM1S_ADDR     0x2D
#define PCF50626_PWM1D_ADDR     0x2E

static NvBool NvOdmPrivBacklightEnable( NvBool Enable );
static NvBool BL_PCF50626_Config( NvOdmDispDeviceHandle hDevice, NvBool bReopen );
static void BL_PCF50626_Release( NvOdmDispDeviceHandle hDevice );
static void BL_PCF50626_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );
static NvBool BL_PCF50626_I2cWrite8(
    NvOdmServicesI2cHandle hOdmI2c,
    NvU8 Addr,
    NvU8 Data);

static NvBool BL_PCF50626_Config( NvOdmDispDeviceHandle hDevice, NvBool bReopen )
{
    s_hOdmI2c = NvOdmI2cOpen( NvOdmIoModule_I2c_Pmu, 0 );
    if (!s_hOdmI2c)
        return NV_FALSE;

    if( bReopen )
    {
        return NV_TRUE;
    }

    /* backlight should be disabled by default */
    if (!BL_PCF50626_I2cWrite8( s_hOdmI2c, PCF50626_PWM1S_ADDR, 0x40 ))
        return NV_FALSE; 
    if( !BL_PCF50626_I2cWrite8( s_hOdmI2c, PCF50626_PWM1D_ADDR, 0x0 ))
        return NV_FALSE;
    if (!NvOdmPrivBacklightEnable( NV_TRUE ))
        return NV_FALSE; 

    return NV_TRUE;
}

static void BL_PCF50626_Release( NvOdmDispDeviceHandle hDevice )
{
    ASSERT_SUCCESS(
        BL_PCF50626_I2cWrite8( s_hOdmI2c, PCF50626_PWM1S_ADDR, 0x0 )
    );

    ASSERT_SUCCESS(
        BL_PCF50626_I2cWrite8( s_hOdmI2c, PCF50626_PWM1D_ADDR, 0x0 )
    );

    ASSERT_SUCCESS(
        NvOdmPrivBacklightEnable( NV_FALSE )
    );

    NvOdmI2cClose(s_hOdmI2c);
    s_hOdmI2c = 0;
    s_Configured = NV_FALSE;
}

static void BL_PCF50626_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_Configured )
    {
        // Enable PWM1 and set its freq to 128Hz
        BL_PCF50626_I2cWrite8( s_hOdmI2c, PCF50626_PWM1S_ADDR, 0x41 );
        s_Configured = NV_TRUE;
    }

    BL_PCF50626_I2cWrite8( s_hOdmI2c, PCF50626_PWM1D_ADDR, intensity ); 
}

static NvBool
NvOdmPrivBacklightEnable( NvBool Enable )
{
    NvBool RetVal = NV_FALSE;
    NvU32 GpioPort = 0;
    NvU32 GpioPin  = 0;

    NvU32 Index;

    // 0. Get the peripheral connectivity information
    NvOdmPeripheralConnectivity const *pConnectivity;

    pConnectivity =
        NvOdmPeripheralGetGuid( NV_ODM_GUID('p','c','f','_','p','m','u','0') );

    if ( !pConnectivity )
        return NV_FALSE;

    s_DeviceGpio.pPeripheralConnectivity = pConnectivity;

    // 1. Retrieve the GPIO Port & Pin from peripheral connectivity entry --

    // Use Peripheral Connectivity DB to locate the Port & Pin configuration
    //    NOTE: Revise & refine this search if there is more than one GPIO defined.
    for ( Index = 0; Index < s_DeviceGpio.pPeripheralConnectivity->NumAddress; ++Index )
    {
        if ( s_DeviceGpio.pPeripheralConnectivity->AddressList[Index].Interface == NvOdmIoModule_Gpio )
        {
            RetVal = NV_TRUE;
            GpioPort = s_DeviceGpio.pPeripheralConnectivity->AddressList[Index].Instance;
            GpioPin  = s_DeviceGpio.pPeripheralConnectivity->AddressList[Index].Address;
        }
    }

    if ( !RetVal )
        return NV_FALSE;

    // 2. Open the GPIO Port (acquire a handle)
    s_DeviceGpio.hGpio = NvOdmGpioOpen();

    if ( !s_DeviceGpio.hGpio )
    {
        RetVal = NV_FALSE;
        goto NvOdmPrivBacklightEnable_Fail;
    }

    // 3. Acquire a pin handle from the opened GPIO port
    s_DeviceGpio.hPin = NvOdmGpioAcquirePinHandle( s_DeviceGpio.hGpio, GpioPort, GpioPin );
    if ( !s_DeviceGpio.hPin )
    {
        RetVal = NV_FALSE;
        goto NvOdmPrivBacklightEnable_Fail;
    }

    // 4. Set the GPIO as requested
    NvOdmGpioSetState( s_DeviceGpio.hGpio, s_DeviceGpio.hPin, (NvU32)Enable );

    // 5. Release pin handle and close GPIO
    NvOdmGpioReleasePinHandle( s_DeviceGpio.hGpio, s_DeviceGpio.hPin );
    s_DeviceGpio.hPin = NULL;

NvOdmPrivBacklightEnable_Fail:
    if( s_DeviceGpio.hGpio )
        NvOdmGpioClose( s_DeviceGpio.hGpio );
    s_DeviceGpio.hGpio = NULL;

    return RetVal;
}

static NvBool BL_PCF50626_I2cWrite8(
    NvOdmServicesI2cHandle hOdmI2c,
    NvU8 Addr,
    NvU8 Data)
{  
    NvU8 WriteBuffer[2];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;    
    NvOdmI2cTransactionInfo TransactionInfo;
    NvU32 DeviceAddr = (NvU32)PCF50626_DEVICE_ADDR;

    WriteBuffer[0] = Addr & 0xFF;   // PMU offset
    WriteBuffer[1] = Data & 0xFF;   // written data

    TransactionInfo.Address = DeviceAddr;
    TransactionInfo.Buf = WriteBuffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    status = NvOdmI2cTransaction(hOdmI2c, &TransactionInfo, 1, 
                        PCF50626_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status == NvOdmI2cStatus_Success)
        return NV_TRUE;
    else
        return NV_FALSE;
}

void BL_PCF50626_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);

    // Backlight functions
    hDevice->pfnBacklightSetup = BL_PCF50626_Config;
    hDevice->pfnBacklightRelease = BL_PCF50626_Release;
    hDevice->pfnBacklightIntensity = BL_PCF50626_SetBacklight;
}
