/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_touch_int.h"
#include "nvodm_services.h"
#include "nvodm_touch_tsi.h"
#include "nvodm_query_discovery.h"
#include "nvos.h"

#define TSI_I2C_SPEED_KHZ                          400
#define TSI_I2C_TIMEOUT                            NV_WAIT_INFINITE
#define TSI_DEFAULT_SAMPLE_RATE                    0      // 0 = low, 1 = high

#define TSI_C1                                     0x3C
#define TSI_C2                                     0x3B
#define TSI_DAT1                                   0x3D
#define TSI_DAT2                                   0x3E
#define TSI_DAT3                                   0x3F
#define TSI_DAT4                                   0xFF
#define TSI_NOI                                    0xFE
#define TSI_GPO4C1                                 0x5B
#define TSI_INT3M                                  0x0C //PCF50626_INT3M_ADDR

#define TSI_GPO4C1_PENDOWN                         0x06
#define TSI_GPO4C1_INVERSION                       0x40
#define TSI_START                                  0x01
#define TSI_FS_MASK                                0x38
#define TSI_FS_100                                 0x18
#define TSI_FS_200                                 0x20
#define TSI_PENDOWNM                               0x01
#define TSI_PENUPM                                 0x02
#define TSI_ADCRDYM                                0x04

#define TSI_TOUCH_DEVICE_GUID NV_ODM_GUID('t','s','i','t','o','u','c','h')

#define TSI_DEBOUNCE_TIME_MS 0

typedef struct TSI_TouchDeviceRec
{
    NvOdmTouchDevice OdmTouch;
    NvOdmTouchCapabilities Caps;
    NvOdmServicesI2cHandle hOdmI2c;
    NvOdmServicesGpioHandle hGpio;
    NvOdmServicesGpioIntrHandle hGpioInt;
    NvOdmServicesPmuHandle hPmu;
    NvOdmGpioPinHandle hPin;
    NvOdmServicesGpioIntrHandle hGpioIntr;
    NvOdmOsSemaphoreHandle hIntSema;
    NvBool PrevFingers;
    NvU32 DeviceAddr;
    NvU32 SampleRate;
    NvU32 SleepMode;
    NvBool PowerOn;
	NvBool PenDown;
	NvU32 X;
	NvU32 Y;
    NvOsThreadHandle  hTouchThreadID;
} TSI_TouchDevice;

TSI_TouchDevice* hTsiTouch;

NvBool TsiTouchI2cWrite8(
    TSI_TouchDevice *hTouch,
    NvU8 Addr,
    NvU8 Data)
{  
    NvU8 WriteBuffer[2];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo;

    WriteBuffer[0] = Addr & 0xFF;   // PMU offset
    WriteBuffer[1] = Data & 0xFF;   // written data

    TransactionInfo.Address = hTouch->DeviceAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    status = NvOdmI2cTransaction(hTouch->hOdmI2c, &TransactionInfo, 1, 
                        TSI_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status == NvOdmI2cStatus_Success)
    {
        return NV_TRUE;
    }
    else
    {
        return NV_FALSE;
    }    
}

NvBool TsiTouchI2cRead8(
    TSI_TouchDevice *hTouch,
    NvU8 Addr,
    NvU8 *Data)
{
    NvU8 ReadBuffer=0;
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;    
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // Write the PMU offset
    ReadBuffer = Addr & 0xFF;

    TransactionInfo[0].Address = hTouch->DeviceAddr;
    TransactionInfo[0].Buf = &ReadBuffer;
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo[0].NumBytes = 1;    
    TransactionInfo[1].Address = (hTouch->DeviceAddr | 0x1);
    TransactionInfo[1].Buf = &ReadBuffer;
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 1;

    // Read data from PMU at the specified offset
    status = NvOdmI2cTransaction(hTouch->hOdmI2c, &TransactionInfo[0], 2, 
                                    TSI_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status != NvOdmI2cStatus_Success)
    {
        return NV_FALSE;
    }  

    *Data = ReadBuffer;
    return NV_TRUE;
}

void NvTsiTouchThread(void *args)
{
	NvU8 cmdC1 =0x80;
	NvU8 dataNoi;
	NvU8 data[3];
    TsiTouchI2cWrite8(hTsiTouch, TSI_C2, 0x00);
    TsiTouchI2cWrite8(hTsiTouch, TSI_C1, cmdC1);
	while(1)
	{
		if(TsiTouchI2cRead8(hTsiTouch, TSI_NOI, &dataNoi) == NV_TRUE)
		{
			if(dataNoi > 0)
			{
				hTsiTouch->PenDown = NV_TRUE;
				TsiTouchI2cRead8(hTsiTouch, TSI_DAT1, &data[0]);
				TsiTouchI2cRead8(hTsiTouch, TSI_DAT2, &data[1]);
				TsiTouchI2cRead8(hTsiTouch, TSI_DAT3, &data[2]);
				hTsiTouch->X = data[0];
				hTsiTouch->X = (hTsiTouch->X<<2)+(data[2]&0x03);
				hTsiTouch->Y = data[1];
				hTsiTouch->Y = (hTsiTouch->Y<<2)+((data[2]&0x0C)>>2);
				TsiTouchI2cWrite8(hTsiTouch, TSI_C1, cmdC1);
				NvOdmOsSemaphoreSignal(hTsiTouch->hIntSema);
			}
			else {
				if(hTsiTouch->PenDown == NV_TRUE)
				{
					hTsiTouch->PenDown = NV_FALSE;
					NvOdmOsSemaphoreSignal(hTsiTouch->hIntSema);
				}
			}
		}
		NvOsSleepMS(50);
	}
}

static const NvOdmTouchCapabilities TSI_Capabilities =
{
    0, //IsMultiTouchSupported
    0, //MaxNumberOfFingerCoordReported;
    0, //IsRelativeDataSupported
    0, //MaxNumberOfRelativeCoordReported
    0, //MaxNumberOfWidthReported
    0, //MaxNumberOfPressureReported
    (NvU32)NvOdmTouchGesture_Not_Supported, //Gesture
    0, //IsWidthSupported
    0, //IsPressureSupported
    0, //IsFingersSupported
    0, //XMinPosition
    0, //YMinPosition
    0, //XMaxPosition
    0, //YMaxPosition
    0  //Orientation
};
static NvBool TSI_Configure (TSI_TouchDevice* hTouch)
{
    hTouch->SleepMode = 0x0;
    hTouch->SampleRate = 0; /* this forces register write */
    return TSI_SetSampleRate(&hTouch->OdmTouch, TSI_DEFAULT_SAMPLE_RATE);
}

static void InitOdmTouch (NvOdmTouchDevice* Dev)
{
    Dev->Close              = TSI_Close;
    Dev->GetCapabilities    = TSI_GetCapabilities;
    Dev->ReadCoordinate     = TSI_ReadCoordinate;
    Dev->EnableInterrupt    = TSI_EnableInterrupt;
    Dev->HandleInterrupt    = TSI_HandleInterrupt;
    Dev->GetSampleRate      = TSI_GetSampleRate;
    Dev->SetSampleRate      = TSI_SetSampleRate;
    Dev->PowerControl       = TSI_PowerControl;
    Dev->PowerOnOff         = TSI_PowerOnOff;
    Dev->GetCalibrationData = TSI_GetCalibrationData;
    Dev->OutputDebugMessage = NV_FALSE;
}

static void TSI_GpioIsr(void *arg)
{
    TSI_TouchDevice* hTouch = (TSI_TouchDevice*)arg;

    /* Signal the touch thread to read the sample. After it is done reading the
     * sample it should re-enable the interrupt. */
    NvOdmOsSemaphoreSignal(hTouch->hIntSema);            
}

NvBool TSI_ReadCoordinate (NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo* coord)
{
    if (hTsiTouch->PenDown == NV_TRUE)
    {
        coord->xcoord = hTsiTouch->X;
        coord->ycoord = hTsiTouch->Y;
        coord->fingerstate = (NvU32)(NvOdmTouchSampleValidFlag | NvOdmTouchSampleDownFlag);
        NVODMTOUCH_PRINTF(("Coordinate:%d,%d\n", coord->xcoord, coord->ycoord));
    }
    else
    {
        coord->fingerstate = NvOdmTouchSampleValidFlag;
        NVODMTOUCH_PRINTF(("Coordinate:Pen up\n"));
    }
    return NV_TRUE;
}

void TSI_GetCapabilities (NvOdmTouchDeviceHandle hDevice, NvOdmTouchCapabilities* pCapabilities)
{
    *pCapabilities = hTsiTouch->Caps;
}

NvBool TSI_PowerOnOff (NvOdmTouchDeviceHandle hDevice, NvBool OnOff)
{
    // Fix me: implement TSI power on/off
    return NV_TRUE;
}

NvBool TSI_Open (NvOdmTouchDeviceHandle* hDevice)
{
    NvU32 i;
    NvU32 GpioPort = 0;
    NvU32 GpioPin = 0;
    NvU32 I2cInstance = 0;

    const NvOdmPeripheralConnectivity *pConnectivity = NULL;

    hTsiTouch = NvOdmOsAlloc(sizeof(TSI_TouchDevice));
    if (!hTsiTouch) return NV_FALSE;

    NvOdmOsMemset(hTsiTouch, 0, sizeof(TSI_TouchDevice));

    /* set function pointers */
    InitOdmTouch(&hTsiTouch->OdmTouch);

    pConnectivity = NvOdmPeripheralGetGuid(TSI_TOUCH_DEVICE_GUID);

    if (pConnectivity->Class != NvOdmPeripheralClass_HCI)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : didn't find any periperal in discovery query for touch device Error \n"));
        goto fail;
    }

    for (i = 0; i < pConnectivity->NumAddress; i++)
    {
        switch (pConnectivity->AddressList[i].Interface)
        {
            case NvOdmIoModule_I2c_Pmu:
                hTsiTouch->DeviceAddr = pConnectivity->AddressList[i].Address;
                I2cInstance = pConnectivity->AddressList[i].Instance;
                break;
            case NvOdmIoModule_Gpio:
                GpioPort = pConnectivity->AddressList[i].Instance;
                GpioPin = pConnectivity->AddressList[i].Address;
                break;
            default:
                break;
        }
    }

    hTsiTouch->hPmu = NvOdmServicesPmuOpen();
    if (!hTsiTouch->hPmu)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : NvOdmServicesPmuOpen Error \n"));
        goto fail;
    }
    TSI_PowerOnOff(&hTsiTouch->OdmTouch, 1);
    
    hTsiTouch->hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c_Pmu, I2cInstance);
    if (!hTsiTouch->hOdmI2c)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : NvOdmI2cOpen Error \n"));
        goto fail;
    }

    hTsiTouch->hGpio = NvOdmGpioOpen();

    if (!hTsiTouch->hGpio)
    {        
        NVODMTOUCH_PRINTF(("NvOdm Touch : NvOdmGpioOpen Error \n"));
        goto fail;
    }

    hTsiTouch->hPin = NvOdmGpioAcquirePinHandle(hTsiTouch->hGpio, GpioPort, GpioPin);
    if (!hTsiTouch->hPin)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : Couldn't get GPIO pin \n"));
        goto fail;
    }

    NvOdmGpioConfig(hTsiTouch->hGpio,
                    hTsiTouch->hPin,
                    NvOdmGpioPinMode_InputData);

    /* set default capabilities */
    NvOdmOsMemcpy(&hTsiTouch->Caps, &TSI_Capabilities, sizeof(NvOdmTouchCapabilities));

    /* configure panel */
    if (!TSI_Configure(hTsiTouch)) goto fail;
       
    hTsiTouch->Caps.XMaxPosition = 1024;
    hTsiTouch->Caps.YMaxPosition = 1024;
    
    *hDevice = &hTsiTouch->OdmTouch;

    return NV_TRUE;

 fail:
    TSI_Close(&hTsiTouch->OdmTouch);
    return NV_FALSE;
}

NvBool TSI_EnableInterrupt (NvOdmTouchDeviceHandle hDevice, NvOdmOsSemaphoreHandle hIntSema)
{
    NvU8 int3MaskData;
    NvError ErrorStatus = NvSuccess;
    NV_ASSERT(hIntSema);
    
    /* can only be initialized once */
    if (hTsiTouch->hGpioIntr || hTsiTouch->hIntSema)
        return NV_FALSE;

    hTsiTouch->hIntSema = hIntSema;    

    if(NvOdmGpioInterruptRegister(hTsiTouch->hGpio, 
    														 &(hTsiTouch->hGpioIntr), 
    														 hTsiTouch->hPin,
                                 NvOdmGpioPinMode_InputInterruptLow,
                                 TSI_GpioIsr,
                                 (void*)hTsiTouch,
                                 TSI_DEBOUNCE_TIME_MS)== NV_FALSE)
                                 {
                                 		return NV_FALSE;
                                 }

    if (!hTsiTouch->hGpioIntr)
        return NV_FALSE;    

    /* Enable interrupts */
    TsiTouchI2cRead8(hTsiTouch, TSI_INT3M, &int3MaskData);
    int3MaskData &= ~(TSI_PENDOWNM);
    TsiTouchI2cWrite8(hTsiTouch, TSI_INT3M, int3MaskData);
    TsiTouchI2cWrite8(hTsiTouch, TSI_GPO4C1, (TSI_GPO4C1_PENDOWN | TSI_GPO4C1_INVERSION));

	/* Create touch event polling thread */
    hTsiTouch->hTouchThreadID = 0;
    ErrorStatus = NvOsThreadCreate(NvTsiTouchThread, NULL, &hTsiTouch->hTouchThreadID);

    return NV_TRUE;
}

NvBool TSI_HandleInterrupt(NvOdmTouchDeviceHandle hDevice)
{
    NvU32 pinValue;
    
    NvOdmGpioGetState(hTsiTouch->hGpio, hTsiTouch->hPin, &pinValue);
    if (!pinValue)
    {
        //interrupt pin is still LOW, read data until interrupt pin is released.
        return NV_FALSE;
    }
    else
        NvOdmGpioInterruptDone(hTsiTouch->hGpioIntr);
    
    return NV_TRUE;
}

NvBool TSI_GetSampleRate (NvOdmTouchDeviceHandle hDevice, NvOdmTouchSampleRate* pTouchSampleRate)
{
    pTouchSampleRate->NvOdmTouchSampleRateHigh = 80;
    pTouchSampleRate->NvOdmTouchSampleRateLow = 40;
    pTouchSampleRate->NvOdmTouchCurrentSampleRate = (hTsiTouch->SampleRate >> 1);
    return NV_TRUE;
}

NvBool TSI_SetSampleRate (NvOdmTouchDeviceHandle hDevice, NvU32 rate)
{
    if (rate != 0 && rate != 1)
        return NV_FALSE;
    
    rate = 1 << rate;
    
    if (hTsiTouch->SampleRate == rate)
        return NV_TRUE;

    // Fix me: send command to device
    
    hTsiTouch->SampleRate = rate;
    return NV_TRUE;
}


NvBool TSI_PowerControl (NvOdmTouchDeviceHandle hDevice, NvOdmTouchPowerModeType mode)
{
    NvU32 SleepMode;

    switch(mode)
    {
        case NvOdmTouch_PowerMode_0:
            SleepMode = 0x0;
            break;
        case NvOdmTouch_PowerMode_1:
        case NvOdmTouch_PowerMode_2:
        case NvOdmTouch_PowerMode_3:
            SleepMode = 0x03;
            break;
        default:
            return NV_FALSE;
    }

    if (hTsiTouch->SleepMode == SleepMode)
        return NV_TRUE;
    
    // Fix me: send command to device
    
    hTsiTouch->SleepMode = SleepMode;    
    return NV_TRUE;
}

NvBool TSI_GetCalibrationData(NvOdmTouchDeviceHandle hDevice, NvU32 NumOfCalibrationData, NvS32* pRawCoordBuffer)
{
    static const NvS32 RawCoordBuffer[] = {2054, 3624, 3937, 809, 3832, 6546, 453, 6528, 231, 890};

    if (NumOfCalibrationData*2 != (sizeof(RawCoordBuffer)/sizeof(NvS32)))
    {
        NVODMTOUCH_PRINTF(("WARNING: number of calibration data isn't matched\n"));
        return NV_FALSE;
    }
    
    NvOdmOsMemcpy(pRawCoordBuffer, RawCoordBuffer, sizeof(RawCoordBuffer));

    return NV_TRUE;
}


void TSI_Close (NvOdmTouchDeviceHandle hDevice)
{
    if (!hTsiTouch) return;

    if (hTsiTouch->hTouchThreadID) {
        NvOsThreadJoin(hTsiTouch->hTouchThreadID);
	}
    if (hTsiTouch->hGpio)
    {
        if (hTsiTouch->hPin)
        {
            if (hTsiTouch->hGpioIntr)
                NvOdmGpioInterruptUnregister(hTsiTouch->hGpio, hTsiTouch->hPin, hTsiTouch->hGpioIntr);

            NvOdmGpioReleasePinHandle(hTsiTouch->hGpio, hTsiTouch->hPin);
        }

        NvOdmGpioClose(hTsiTouch->hGpio);
    }

    if (hTsiTouch->hOdmI2c)
        NvOdmI2cClose(hTsiTouch->hOdmI2c);

    if (hTsiTouch->hPmu)
        NvOdmServicesPmuClose(hTsiTouch->hPmu);

    NvOdmOsFree(hTsiTouch);
}


