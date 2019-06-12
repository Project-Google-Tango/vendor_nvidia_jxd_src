/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_touch_int.h"
#include "nvodm_services.h"
#include "nvodm_touch_ads7846.h"
#include "nvodm_query_discovery.h"
#include "ads7846_reg.h"

#define ADS7846_TOUCH_DEVICE_GUID NV_ODM_GUID('a','d','s','t','o','u','c','h')

#define TOUCH_SAMPLE_COUNT 5

#define DEFAULT_SPI_CLOCK_KHZ   1000//1Mhz

#define HIGH_SAMPLE_RATE_PEN_TIMER_MS   6//6ms -> 160 samples per second
#define LOW_SAMPLE_RATE_PEN_TIMER_MS    12//12ms -> 80 samples per second

#define ADS7846_DEBOUNCE_TIME_MS    0

#define READ_ALL_SAMPLE 0//read all samples in one SPI transaction

#define SPI(c) \
    CmdData[0] = (c); \
    CmdData[1] = 0; \
    CmdData[2] = 0; \
    NvOdmSpiTransaction( hTouch->hOdmSpi, hTouch->SpiCS, \
        hTouch->SpiClockKhz, 0, CmdData, 3, 24);


typedef enum
{
    PEN_DOWN = 0,
    PEN_UP_OR_TIMER
} ADS7846_EXPECTED_INT;

typedef struct ADS7846_TouchDeviceRec
{
    NvOdmTouchDevice OdmTouch;
    NvOdmTouchCapabilities Caps;
    NvOdmServicesSpiHandle hOdmSpi;
    NvU32 SpiChannel;
    NvU32 SpiCS;
    NvU32 SpiClockKhz;
    NvOdmServicesGpioHandle hGpio;
    NvOdmGpioPinHandle hPin;
    NvU32 GpioPort;
    NvU32 GpioPin;
    NvOdmServicesPmuHandle hPmu;
    NvU32 VddId;
    NvBool PowerOn;
    NvOdmServicesGpioIntrHandle hGpioIntr;
    NvOdmOsSemaphoreHandle hIntSema;
    NvOdmOsThreadHandle hTimerThread;
    NvOdmOsSemaphoreHandle hTimerSema;
    NvU32 Timer;
    NvBool TimerHalt;
    NvU32 ShutdownTimerThread;
    NvU32 SampleRate;
    ADS7846_EXPECTED_INT NextExpectedInterrupt;
} ADS7846_TouchDevice;

NvOdmTouchCapabilities ADS7846_Capabilities = 
{
    0, //IsMultiTouchSupported
    1, //MaxNumberOfFingerCoordReported;
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
    0 //Orientation
};


NvU32 TIMEOUT = NV_WAIT_INFINITE;

static void InitOdmTouch (NvOdmTouchDevice* Dev)
{
    Dev->Close              = ADS7846_Close;
    Dev->GetCapabilities    = ADS7846_GetCapabilities;
    Dev->ReadCoordinate     = ADS7846_ReadCoordinate;
    Dev->EnableInterrupt    = ADS7846_EnableInterrupt;
    Dev->HandleInterrupt    = ADS7846_HandleInterrupt;
    Dev->GetSampleRate      = ADS7846_GetSampleRate;
    Dev->SetSampleRate      = ADS7846_SetSampleRate;
    Dev->PowerControl       = ADS7846_PowerControl;
    Dev->PowerOnOff         = ADS7846_PowerOnOff;
    Dev->GetCalibrationData = ADS7846_GetCalibrationData;
    Dev->CurrentSampleRate  = 1;
    Dev->OutputDebugMessage = NV_FALSE;
}


/** 
 * Filter samples by means of majority vote. Pick up the closet 2 samples, and
 * average 2 samples.
 */
static NvBool
ADS7846_CalculateSamples(NvU32 *pSamples, NvU32 *pcalculatedsample)
{
    NvS32 delta0 = 0;
    NvS32 delta1 = 0;
    NvS32 delta2 = 0;
    NvU32 samples[3];
    
    NvOdmOsMemcpy(&samples[0], pSamples, sizeof(NvU32)*3);

    delta0 = samples[0] - samples[1];
    delta1 = samples[1] - samples[2];
    delta2 = samples[2] - samples[0];
    delta0 = delta0 > 0  ? delta0 : -delta0;
    delta1 = delta1 > 0  ? delta1 : -delta1;
    delta2 = delta2 > 0  ? delta2 : -delta2;
    
    if (delta0 < delta1)
        *pcalculatedsample = (NvU32)(samples[0] + ((delta2 < delta0) ? samples[2] : samples[1]));
    else
        *pcalculatedsample = (NvU32)(samples[2] + ((delta2 < delta1) ? samples[0] : samples[1]));
        
    *pcalculatedsample = *pcalculatedsample / 2;
            
    return NV_TRUE;   
}

/*
 * @brief      Read the X, Y cordinate from ADC via spi Controller. 
 *             And post-process the samples to provide accurate sample.
 * @param[out]  pXCor  X coordination info
 * @param[out]  pYCor  Y coordination info
 * @retval     NV_TRUE if successful
 */
static NvBool
ADS7846_GetSample(NvOdmTouchDeviceHandle hDevice, NvU32 *pXCor, NvU32 *pYCor)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;
    NvU32 XTemp[TOUCH_SAMPLE_COUNT];
    NvU32 YTemp[TOUCH_SAMPLE_COUNT];
    NvU32 Count;
#if READ_ALL_SAMPLE
    NvU8 DataWr[TOUCH_SAMPLE_COUNT*2*3];    //1 sample contains 5 sets of data
                                            //x, and y. And 24-clock-per-data
    NvU8 DataRd[TOUCH_SAMPLE_COUNT*2*3];
#else
    NvU8 DataWr[3], DataRd[3];
#endif
    
#if READ_ALL_SAMPLE
    NvOdmOsMemset(DataWr, 0, sizeof(DataWr));
    NvOdmOsMemset(DataRd, 0, sizeof(DataRd));

    //Read X, Y as X1Y1X2Y2X3Y3...XnYn
    for (Count = 0; Count < TOUCH_SAMPLE_COUNT*2*3, Count+6)
    {
        //Read X Sample
        DataWr[Count] = (ADS_START | ADS_A2A1A0_READ_X | ADS_MODE_12_BIT | ADS_DFR | ADS_PD10_PDOWN);
        //Read Y Sample
        DataWr[Count+3] = (ADS_START | ADS_A2A1A0_READ_Y | ADS_MODE_12_BIT | ADS_DFR | ADS_PD10_PDOWN);
    }

    NvOdmSpiTransaction(hTouch->hOdmSpi,
                        hTouch->SpiCS,
                        hTouch->SpiClockKhz,
                        DataRd,
                        DataWr,
                        TOUCH_SAMPLE_COUNT*2*3,
                        24);

    for (Count = 0; CCount < TOUCH_SAMPLE_COUNT*2*3, Count+6)
    {
        XTemp[Count] = ((DataRd[Count+1] << 5) | (DataRd[Count+2] >> 3)) & 0xFFF;
        YTemp[Count] = ((DataRd[Count+4] << 5) | (DataRd[Count+5] >> 3)) & 0xFFF;
    }
#else
    for (Count = 0; Count < TOUCH_SAMPLE_COUNT; Count++)
    {
        NvOdmOsMemset(DataWr, 0, sizeof(DataWr));
        NvOdmOsMemset(DataRd, 0, sizeof(DataRd));

        //Read X Sample
        DataWr[0] = (ADS_START | ADS_A2A1A0_READ_X | ADS_MODE_12_BIT | ADS_DFR | ADS_PD10_PDOWN);
        // 1 byte written and 2 bytes read hence 3 bytes sent. 24-bit single transfer achieves the result.
        NvOdmSpiTransaction(hTouch->hOdmSpi, 
                            hTouch->SpiCS, 
                            hTouch->SpiClockKhz,
                            DataRd, 
                            DataWr, 
                            3, 
                            24);
        XTemp[Count]  = ((DataRd[1] << 5) | (DataRd[2] >> 3)) & 0xFFF;

        NvOdmOsMemset(DataWr, 0, sizeof(DataWr));
        NvOdmOsMemset(DataRd, 0, sizeof(DataRd));

        //Read Y Sample
        DataWr[0] = (ADS_START | ADS_A2A1A0_READ_Y | ADS_MODE_12_BIT | ADS_DFR | ADS_PD10_PDOWN);
        // 1 byte written and 2 bytes read hence 3 bytes sent. 24-bit single transfer achieves the result.
        NvOdmSpiTransaction(hTouch->hOdmSpi, 
                            hTouch->SpiCS, 
                            hTouch->SpiClockKhz,
                            DataRd, 
                            DataWr, 
                            3, 
                            24);

        YTemp[Count] = ((DataRd[1] << 5) | (DataRd[2] >> 3)) & 0xFFF;
        NVODMTOUCH_PRINTF(("ADS7846_GetSample:%d, %d, %d\n", Count, XCorVal, YCorVal));
    }
#endif

    //Discard first 2 samples
    if (!ADS7846_CalculateSamples(&XTemp[2], pXCor))
        return NV_FALSE;

    //Discard first 2 samples            
    if (!ADS7846_CalculateSamples(&YTemp[2], pYCor))
        return NV_FALSE;
        
    NVODMTOUCH_PRINTF(("ADS7846_GetSample: X=%d, Y=%d\n", *pXCor, *pYCor));

    return NV_TRUE;
}

static void ADS7846_GpioIsr(void *arg)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)arg;

    //hTouch->NextExpectedInterrupt == PEN_UP_OR_TIMER;
    
    /* Signal the touch thread to read the sample. After it is done reading the
     * sample it should re-enable the interrupt. */
    NvOdmOsSemaphoreSignal(hTouch->hIntSema);
}

static void ADS7846_TimerThread(void* arg)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)arg;
    NvU32 waittimems = NV_WAIT_INFINITE;

    
    while (!hTouch->ShutdownTimerThread)
    {
        if (NV_FALSE == NvOdmOsSemaphoreWaitTimeout(hTouch->hTimerSema, waittimems))
        {
            if (hTouch->NextExpectedInterrupt == PEN_UP_OR_TIMER)
            {
                NVODMTOUCH_PRINTF(("ADS7846_TimerThread:TIME UP, Singal touch driver\n"));
                /* Signal the touch thread to read the sample. */
                waittimems = hTouch->Timer;
                NvOdmOsSemaphoreSignal(hTouch->hIntSema);
            }
        }
        else
        {
            if (hTouch->NextExpectedInterrupt == PEN_UP_OR_TIMER)
            {
                NVODMTOUCH_PRINTF(("ADS7846_TimerThread:Start\n"));
                waittimems = hTouch->Timer;
            }
            else
            {
                NVODMTOUCH_PRINTF(("ADS7846_TimerThread:Stop\n"));
                waittimems = NV_WAIT_INFINITE;
            }
        }            
    }
}

NvBool
ADS7846_HandleInterrupt(NvOdmTouchDeviceHandle hDevice)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;
    
    if (hTouch->NextExpectedInterrupt == PEN_DOWN)
    {
        NVODMTOUCH_PRINTF(("ADS7846_HandleInterrupt:Enable PENIRQ IST\n"));
        //Stop Timer Thread
        NvOdmOsSemaphoreSignal(hTouch->hTimerSema);
        hTouch->TimerHalt = NV_TRUE;

        //Enable PENIRQ Thread
        NvOdmGpioInterruptDone(hTouch->hGpioIntr);
    }
    else
    {
        if (hTouch->TimerHalt == NV_TRUE)
        {
            NVODMTOUCH_PRINTF(("ADS7846_HandleInterrupt:Enable TIMER IST\n"))
            hTouch->TimerHalt = NV_FALSE;    
            NvOdmOsSemaphoreSignal(hTouch->hTimerSema);
        }
    }

    return NV_TRUE;
}

NvBool
ADS7846_EnableInterrupt(NvOdmTouchDeviceHandle hDevice, NvOdmOsSemaphoreHandle hIntSema)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;
    NvU8    CmdData[3];

    NVODMTOUCH_PRINTF(("ADS7846_EnableInterrupt+\n"));
    NV_ASSERT(hIntSema);

    /* can only be initialized once */
    if (hTouch->hGpioIntr || hTouch->hIntSema)
        return NV_FALSE;

    // Create TIMER IST
    hTouch->Timer = HIGH_SAMPLE_RATE_PEN_TIMER_MS;
    hTouch->hTimerSema = NvOdmOsSemaphoreCreate(0);
    hTouch->TimerHalt = NV_TRUE;
    if (!hTouch->hTimerSema)
    {
        NVODMTOUCH_PRINTF(("ADS7846_EnableInterrupt:Create Timer Semaphore failed\r\n"));
        return NV_FALSE;
    }
        
    hTouch->hTimerThread = NvOdmOsThreadCreate(ADS7846_TimerThread, (void*)hTouch);
    if (!hTouch->hTimerThread)
    {
        NVODMTOUCH_PRINTF(("ADS7846_EnableInterrupt: Create Timer IST failed\r\n"));
        return NV_FALSE;
    }
        
    hTouch->hIntSema = hIntSema; 

    if (NvOdmGpioInterruptRegister(hTouch->hGpio, &hTouch->hGpioIntr,
        hTouch->hPin, NvOdmGpioPinMode_InputInterruptFallingEdge, ADS7846_GpioIsr,
        (void*)hTouch, ADS7846_DEBOUNCE_TIME_MS) == NV_FALSE)
    {
        NVODMTOUCH_PRINTF(("ADS7846_EnableInterrupt: Interrupt Register failed\n"));
        return NV_FALSE;
    }

    //if (!hTouch->hGpioIntr)
    //{
    //    NVODMTOUCH_PRINTF(("ADS7846_EnableInterrupt: Interrupt Register failed\n"));
    //    return NV_FALSE; 
    //}

    /* enable ADS7846 interrupt */
    SPI(ADS_PD10_PDOWN);
    
    NVODMTOUCH_PRINTF(("ADS7846_EnableInterrupt-\n"));
    return NV_TRUE;
}







NvBool
ADS7846_Open(NvOdmTouchDeviceHandle* hDevice)
{   
    ADS7846_TouchDevice* hTouch;
    NvU32 found = 0;
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvU32 i = 0;
    NvU8 CmdData[3];

    hTouch = NvOdmOsAlloc(sizeof(ADS7846_TouchDevice));
    if (!hTouch) return NV_FALSE;

    NvOdmOsMemset(hTouch, 0, sizeof(ADS7846_TouchDevice));

    /* set function pointers */
    InitOdmTouch(&hTouch->OdmTouch);

    pConnectivity = NvOdmPeripheralGetGuid(ADS7846_TOUCH_DEVICE_GUID);
    
    if (!pConnectivity)
    {
        NVODMTOUCH_PRINTF(("ADS7846_Open: Discovery failed\n"));
        goto fail;
    }
    
    if(pConnectivity->Class!= NvOdmPeripheralClass_HCI)
    {
        NVODMTOUCH_PRINTF(("ADS7846_Open : didn't find any periperal in discovery query for touch device Error \n"));
        goto fail;
    }
        
    for( i=0; i < pConnectivity->NumAddress; i++)
    {
        switch(pConnectivity->AddressList[i].Interface)
        {
            case NvOdmIoModule_Spi:
                hTouch->SpiChannel= pConnectivity->AddressList[i].Instance;
                hTouch->SpiCS = pConnectivity->AddressList[i].Address;
                NVODMTOUCH_PRINTF(("ADS7846_Open:SpiChannle(%d), SpiChipSelect(%d)\n",
                                    hTouch->SpiChannel, hTouch->SpiCS));
                found |= 1;
                break;
                
            case NvOdmIoModule_Gpio:
                hTouch->GpioPort = pConnectivity->AddressList[i].Instance;
                hTouch->GpioPin = pConnectivity->AddressList[i].Address;
                NVODMTOUCH_PRINTF(("ADS7846_Open:GpioPort(%d), GpioPin(%d)\n",
                                    hTouch->GpioPort, hTouch->GpioPin));
                found |= 2;
                break;
                
            case NvOdmIoModule_Vdd:
                hTouch->VddId = pConnectivity->AddressList[i].Address;
                found |= 4;
                break;
            default:
                break;
        }
    }

    if ((found & 3) != 3)
    {
        NVODMTOUCH_PRINTF(("ADS7846_Open : check discovery database\n"));
        goto fail;
    }

    if ((found & 4) != 0)
    {
        if (NV_FALSE == ADS7846_PowerOnOff(&hTouch->OdmTouch, 1))
            goto fail;            
    }
    else
    {
        hTouch->VddId = 0xFF; 
    }

    // Setup SPI bus
    hTouch->hOdmSpi = NvOdmSpiOpen(NvOdmIoModule_Spi, hTouch->SpiChannel);
    if (!hTouch->hOdmSpi)
    {
        NVODMTOUCH_PRINTF(("ADS7846_Open:NvOdmSpiOpen failed\r\n"));
        goto fail;
    }

    hTouch->hGpio = NvOdmGpioOpen();
    if (!hTouch->hGpio)
    {
        NVODMTOUCH_PRINTF(("ADS7846_Open:NvOdmGpioOpen failed\r\n"));
        goto fail;
    }

    hTouch->hPin = NvOdmGpioAcquirePinHandle(hTouch->hGpio, hTouch->GpioPort, hTouch->GpioPin);
    if (!hTouch->hPin)
    {
        NVODMTOUCH_PRINTF(("ADS7846_Open:NvOdmGpioAcquirePinHandle failed\r\n"));
        goto fail;
    }

    NvOdmGpioConfig(hTouch->hGpio,
                    hTouch->hPin,
                    NvOdmGpioPinMode_InputData);

    /* set default capabilities */
    NvOdmOsMemcpy(&hTouch->Caps, &ADS7846_Capabilities, sizeof(NvOdmTouchCapabilities));

    hTouch->NextExpectedInterrupt = PEN_DOWN;
    hTouch->SampleRate = 1;
    hTouch->SpiClockKhz = DEFAULT_SPI_CLOCK_KHZ;

    //disalbe ADS7846 PENIRQ
    SPI(ADS_PD10_ADC_ON);
    
    *hDevice = &hTouch->OdmTouch;

    NVODMTOUCH_PRINTF(("ADS7846_Open-\n"));
    return NV_TRUE;
    
fail:
    ADS7846_Close(&hTouch->OdmTouch);
    return NV_FALSE;
}

void ADS7846_Close(NvOdmTouchDeviceHandle hDevice)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;
    
    NVODMTOUCH_PRINTF(("ADS7846_Close+\n"));
    if (!hTouch) return;
        
    if (hTouch->hGpio)
    {
        if (hTouch->hPin)
        {
            if (hTouch->hGpioIntr)
                NvOdmGpioInterruptUnregister(hTouch->hGpio, hTouch->hPin, hTouch->hGpioIntr);

            NvOdmGpioReleasePinHandle(hTouch->hGpio, hTouch->hPin);
        }

        NvOdmGpioClose(hTouch->hGpio);
    }

    if (hTouch->hOdmSpi)
        NvOdmSpiClose(hTouch->hOdmSpi);

    if (hTouch->hTimerThread)
    {
        hTouch->ShutdownTimerThread = 1;
        NvOdmOsSemaphoreSignal(hTouch->hTimerSema);
        NvOdmOsThreadJoin(hTouch->hTimerThread);
    }
    
    if (hTouch->hTimerSema)
        NvOdmOsSemaphoreDestroy(hTouch->hTimerSema);

    NvOdmOsFree(hTouch);
    
    NVODMTOUCH_PRINTF(("ADS7846_Close-\n"));
}


void ADS7846_GetCapabilities(NvOdmTouchDeviceHandle hDevice, NvOdmTouchCapabilities* pCapabilities)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;
    *pCapabilities = hTouch->Caps;
}

NvBool
ADS7846_ReadCoordinate(NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo *coord)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;
    NvU32 UncalX, UncalY;
    NvU32 penirq;

    NVODMTOUCH_PRINTF(("ADS7846_ReadCoordinate+\n"))
    NvOdmGpioGetState(hTouch->hGpio, hTouch->hPin, &penirq);
    
    if (penirq)
    {   //Pen Up
        coord->fingerstate = (NvU32)NvOdmTouchSampleValidFlag;
        hTouch->NextExpectedInterrupt = PEN_DOWN;
        NVODMTOUCH_PRINTF(("ADS7846_ReadCoordinate:PEN UP\n"));
        return NV_TRUE;
    }
    
    if (NV_FALSE == ADS7846_GetSample(hDevice, &UncalX, &UncalY))
        return NV_FALSE;
    
    coord->xcoord = (NvU32)UncalX;
    coord->ycoord = (NvU32)UncalY;
    coord->fingerstate = (NvU32)(NvOdmTouchSampleValidFlag | NvOdmTouchSampleDownFlag);
    hTouch->NextExpectedInterrupt = PEN_UP_OR_TIMER;
    NVODMTOUCH_PRINTF(("ADS7846_ReadCoordinate:PEN DOWN - UncalX=%d, UncalY=%d\n", UncalX, UncalY));
    return NV_TRUE;    
}

NvBool
ADS7846_GetSampleRate(NvOdmTouchDeviceHandle hDevice, NvOdmTouchSampleRate* pTouchSampleRate)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;

    NVODMTOUCH_PRINTF(("ADS7846_GetSampleRate+\n"));
    //In theory, ADS7846 sample rate = SPI CLOCK Rate/(clock-per-conversion x N)
    //Refer to ads7846.h for options of clock-per-conversion
    //N is the number of data units in a set of samples.
    //e.g. 
    //SPI clock = 2000Khz, 
    //24 clock-per-conversion
    //and 5 data units per sample.
    //Sample rate = 2000000/(24* (5*2))= 8333 samples per second
    //
    //However when using a touch screen as user interface, one usually expectes 
    //around 100 to 500 valid sets of samples per second, in real world 
    //application. This could be managed by means of PEN TIMER routine. With
    //proper TIMER setting, touch driver could get samples periodically to reach
    //expected sample rate. With TIMER=6ms, around 160 samples per second are 
    //collected. With TIMER=12ms, around 80 samples per second are collectd.

    if (hTouch->Timer == HIGH_SAMPLE_RATE_PEN_TIMER_MS)
        hDevice->CurrentSampleRate = 1;
    else
        hDevice->CurrentSampleRate = 0;
    
    pTouchSampleRate->NvOdmTouchSampleRateHigh = 160 ; //poll every 6ms
    pTouchSampleRate->NvOdmTouchSampleRateLow = 80; //poll every 12ms
    pTouchSampleRate->NvOdmTouchCurrentSampleRate = hDevice->CurrentSampleRate;

    NVODMTOUCH_PRINTF(("ADS7846_GetSampleRate-\n"));
    return NV_TRUE;
}

NvBool 
ADS7846_SetSampleRate(NvOdmTouchDeviceHandle hDevice, NvU32 SampleRate)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;

    NVODMTOUCH_PRINTF(("ADS7846_SetSampleRate+\n"));
    
    if (hDevice->CurrentSampleRate == SampleRate)
        return NV_TRUE;
    else
    {
        if (1 == SampleRate)
        {
            hTouch->Timer = HIGH_SAMPLE_RATE_PEN_TIMER_MS;
        }
        else
            hTouch->Timer = LOW_SAMPLE_RATE_PEN_TIMER_MS;
    }
    
    hDevice->CurrentSampleRate = SampleRate;

    NVODMTOUCH_PRINTF(("ADS7846_SetSampleRate-\n"));
    return NV_TRUE;
}

NvBool 
ADS7846_PowerOnOff(NvOdmTouchDeviceHandle hDevice, NvBool OnOff)
{
    ADS7846_TouchDevice* hTouch = (ADS7846_TouchDevice*)hDevice;

    NVODMTOUCH_PRINTF(("ADS7846_PowerOnOff+\n"));

    hTouch->hPmu = NvOdmServicesPmuOpen();

    if (!hTouch->hPmu)
    {
        NVODMTOUCH_PRINTF(("ADS7846_PowerOnOff: NvOdmServicesPmuOpen Error \n"));
        return NV_FALSE;
    }
    
    if (OnOff != hTouch->PowerOn)
    {
        NvOdmServicesPmuVddRailCapabilities vddrailcap;
        NvU32 settletime;

        NvOdmServicesPmuGetCapabilities( hTouch->hPmu, hTouch->VddId, &vddrailcap);

        if(OnOff)
            NvOdmServicesPmuSetVoltage( hTouch->hPmu, hTouch->VddId, vddrailcap.requestMilliVolts, &settletime);
        else
            NvOdmServicesPmuSetVoltage( hTouch->hPmu, hTouch->VddId, NVODM_VOLTAGE_OFF, &settletime);

        if (settletime)
            NvOdmOsWaitUS(settletime); // wait to settle power

        hTouch->PowerOn = OnOff;

        if(OnOff)
            NvOdmOsSleepMS(100);
    }

    NvOdmServicesPmuClose(hTouch->hPmu);
    
    NVODMTOUCH_PRINTF(("ADS7846_PowerOnOff-\n"));
    return NV_TRUE;
}

NvBool
ADS7846_PowerControl(NvOdmTouchDeviceHandle hDevice, NvOdmTouchPowerModeType mode)
{   
    NVODMTOUCH_PRINTF(("ADS7846_PowerControl: Not Supported\n"));
    return NV_TRUE;
}

NvBool
ADS7846_GetCalibrationData(NvOdmTouchDeviceHandle hDevice, NvU32 NumOfCalibrationData, NvS32* pRawCoordBuffer)
{
    NVODMTOUCH_PRINTF(("ADS7846_GetCalibrationData: Not Supported\n"));
    return NV_FALSE;
}
