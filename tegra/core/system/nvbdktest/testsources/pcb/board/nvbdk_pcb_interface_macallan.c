/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbdk_pcb_interface.h"
#include "nvbdk_pcb_api.h"
#include "nvodm_services.h"
#include "nvodm_query_gpio.h"

/* pcb:osc:xxxxx */
static OscTestPrivateData sOscData = {
    500, 100
};

/* pcb:emmc:xxxxx */
#define EMMC_CID_MID_CBX_OID 0x00450100
#define EMMC_CID_PRODUCT_NAME_1 0x53454d33
#define EMMC_CID_PRODUCT_NAME_2 0x32470000
static NvError EmmcValidationFunc(void *pInfo)
{
    NvError e = NvError_BadParameter;;
    NvU32 cid[4];
    if (pInfo)
    {
        NvOsMemcpy(&cid, pInfo, sizeof(cid));
        if (((cid[3] & 0x00FFFFFF) == EMMC_CID_MID_CBX_OID)
            && (cid[2] == EMMC_CID_PRODUCT_NAME_1)
            && ((cid[1] & 0xFFFF0000) == EMMC_CID_PRODUCT_NAME_2))
                    e  = NvSuccess;
    }
    return e;
}
static EmmcTestPrivateData sEmmcData = {
    3, EmmcValidationFunc
};

/* pcb:pmu:xxxxx */
static NvU8 sTPS65913_I2cWBuf[] = { 0x57 };
static I2cTestPrivateData sI2cTPS65913[] = {
    { 4, 0x5A, _I2C_CMD_WRITE_, {1, sTPS65913_I2cWBuf}, {0, NULL} },
    { 4, 0x5A, _I2C_CMD_READ_,  {1, NULL}, {0, NULL} },
    { 0, 0x00, _I2C_CMD_END_,   {0, NULL}, {0, NULL} },
};

/* pcb:audio:xxxxx */
static NvU8 sAlc5639_I2cWBuf[] = { 0xFE };
static NvU8 sAlc5639_I2cValidation[] = { 0x10, 0xEC };
static I2cTestPrivateData sI2cAlc5639[] = {
    { 0, 0x1C, _I2C_CMD_WRITE_, {1, sAlc5639_I2cWBuf}, {0, NULL} },
    { 0, 0x1C, _I2C_CMD_READ_,  {2, NULL}, {2, sAlc5639_I2cValidation} },
    { 0, 0x00, _I2C_CMD_END_,   {0, NULL}, {0, NULL} },
};

/* pcb:charger:xxxxx */
static NvU8 sBq24192_I2cWBuf[] = { 0x0A };
static NvU8 sBq24192_I2cValidation[] = { 0x2B };
static I2cTestPrivateData sI2cBq24192[] = {
    { 0, 0x6B, _I2C_CMD_WRITE_, {1, sBq24192_I2cWBuf}, {0, NULL} },
    { 0, 0x6B, _I2C_CMD_READ_,  {1, NULL}, {1, sBq24192_I2cValidation} },
    { 0, 0x00, _I2C_CMD_END_,   {0, NULL}, {0, NULL} },
};

/* pcb:accgyro:xxxxx */
static NvU8 sMpu9150_I2cWBuf[] = { 0x75 };
static NvU8 sMpu9150_I2cValidation[] = { 0x68 };
static I2cTestPrivateData sI2cMpu6050[] = {
    { 0, 0x69, _I2C_CMD_WRITE_, {1, sMpu9150_I2cWBuf}, {0, NULL} },
    { 0, 0x69, _I2C_CMD_READ_,  {1, NULL}, {1, sMpu9150_I2cValidation} },
    { 0, 0x00, _I2C_CMD_END_,   {0, NULL}, {0, NULL} },
};

/* pcb:compass:xxxxx */
static NvU8 sEnableINVPLL_I2cWBuf[] = { 0x6B, 0x01 };
static NvU8 sSetBypass_I2cWBuf[] = { 0x37, 0x02 };
static NvU8 sCompass_I2cWBuf[] = { 0x00 };
static NvU8 sCompass_I2cValidation[] = { 0x48 };
static I2cTestPrivateData sI2cCompass[] = {
    { 0, 0x69, _I2C_CMD_WRITE_, {2, sEnableINVPLL_I2cWBuf}, {0, NULL} },
    { 0, 0x69, _I2C_CMD_WRITE_, {2, sSetBypass_I2cWBuf}, {0, NULL} },
    { 0, 0x0D, _I2C_CMD_WRITE_, {1, sCompass_I2cWBuf}, {0, NULL} },
    { 0, 0x0D, _I2C_CMD_READ_,  {1, NULL}, {1, sCompass_I2cValidation} },
    { 0, 0x00, _I2C_CMD_END_,   {0, NULL}, {0, NULL} },
};

/* pcb:als:xxxxx */
static I2cTestPrivateData sI2cCM3217[] = {
    { 0, 0x10, _I2C_CMD_READ_, {1, NULL}, {0, NULL} },
    { 0, 0x11, _I2C_CMD_READ_, {1, NULL}, {0, NULL} },
    { 0, 0x00, _I2C_CMD_END_,  {0, NULL}, {0, NULL} },
};

/* pcb:fuel:xxxxx */
static NvU8 sMAX17048_I2cWBuf[] = { 0x08 };
static NvU8 sMAX17048_I2cValidation[] = { 0x00, 0x11 };
static I2cTestPrivateData sI2cMAX17048[] = {
    { 0, 0x36, _I2C_CMD_WRITE_, {1, sMAX17048_I2cWBuf}, {0, NULL} },
    { 0, 0x36, _I2C_CMD_READ_, {2, NULL}, {2, sMAX17048_I2cValidation} },
    { 0, 0x00, _I2C_CMD_END_,  {0, NULL}, {0, NULL} },
};

/* pcb:pwrmon:xxxxx */
static NvU8 sINA219_I2cWBuf[] = { 0x00 };
static I2cTestPrivateData sI2cINA219[] = {
    { 0, 0x44, _I2C_CMD_WRITE_, {1, sINA219_I2cWBuf}, {0, NULL} },
    { 0, 0x44, _I2C_CMD_READ_, {2, NULL}, {0, NULL} },
    { 0, 0x00, _I2C_CMD_END_,  {0, NULL}, {0, NULL} },
};

/* pcb:key_test:xxxxx */
/* NULL */

/* pcb:wifi:xxxxx */
static NvOdmServicesGpioHandle s_hGpio;
static NvOdmGpioPinHandle s_hWLANPwrPin;
static NvOdmGpioPinHandle s_hBTPwrPin;
static NvOdmGpioPinHandle s_hWLANRstPin;
static void NvPcbWifiPower(int on)
{
    if (on)
    {
        NvOdmGpioConfig(s_hGpio, s_hWLANPwrPin, NvOdmGpioPinMode_Output);
        NvOdmGpioSetState(s_hGpio, s_hWLANPwrPin, 0x1);

        NvOdmGpioConfig(s_hGpio, s_hBTPwrPin, NvOdmGpioPinMode_Output);
        NvOdmGpioSetState(s_hGpio, s_hBTPwrPin, 0x1);

        NvOsSleepMS(20);
    }
    else
    {
        NvOdmGpioConfig(s_hGpio, s_hWLANRstPin, NvOdmGpioPinMode_Output);
        NvOdmGpioSetState(s_hGpio, s_hWLANRstPin, 0x0);

        NvOdmGpioConfig(s_hGpio, s_hBTPwrPin, NvOdmGpioPinMode_Output);
        NvOdmGpioSetState(s_hGpio, s_hBTPwrPin, 0x0);
    }
}

static NvError NvPcbWifiGpioInit(void)
{
    const NvOdmGpioPinInfo *gpioPinTable;
    const NvOdmGpioPinInfo *gpioBTPinTable;
    NvU32 gpioPinCount;

    if (!s_hGpio)
        s_hGpio = NvOdmGpioOpen();
    if (!s_hGpio)
    {
        NvOsDebugPrintf("%s: Gpio open failed\n", __func__);
        return NvError_ResourceError;
    }

    gpioPinTable = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Wlan,
                                        0,
                                        &gpioPinCount);
    if (!gpioPinTable)
    {
        NvOsDebugPrintf("%s: can not get gpioPinTable\n", __func__);
        return NvError_ResourceError;
    }

    gpioBTPinTable = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Bluetooth,
                                          0,
                                          &gpioPinCount);
    if (!gpioBTPinTable)
    {
        NvOsDebugPrintf("%s: can not get gpioBTPinTable\n", __func__);
        return NvError_ResourceError;
    }

    s_hWLANPwrPin = NvOdmGpioAcquirePinHandle(s_hGpio,
                        gpioPinTable[NvOdmGpioPin_WlanPower - 1].Port,
                        gpioPinTable[NvOdmGpioPin_WlanPower - 1].Pin);
    if (!s_hWLANPwrPin )
    {
        NvOsDebugPrintf("%s: Pinhandle could not be acquired\n", __func__);
        return NvError_ResourceError;
    }

    s_hWLANRstPin = NvOdmGpioAcquirePinHandle(s_hGpio,
                          gpioPinTable[NvOdmGpioPin_WlanReset - 1].Port,
                          gpioPinTable[NvOdmGpioPin_WlanReset - 1].Pin);
    if (!s_hWLANRstPin)
    {
        NvOsDebugPrintf("%s: Pinhandle could not be acquired\n", __func__);
        return NvError_ResourceError;
    }

    s_hBTPwrPin = NvOdmGpioAcquirePinHandle(s_hGpio,
                          gpioBTPinTable[NvOdmGpioPin_BluetoothPower - 1].Port,
                          gpioBTPinTable[NvOdmGpioPin_BluetoothPower - 1].Pin);
    if (!s_hBTPwrPin) {
        NvOsDebugPrintf("%s: Pinhandle could not be acquired\n", __func__);
        return NvError_ResourceError;
    }

    NvOdmGpioConfig(s_hGpio, s_hWLANRstPin, NvOdmGpioPinMode_Output);
    NvOdmGpioSetState(s_hGpio, s_hWLANRstPin, 0x0);

    NvOdmGpioConfig(s_hGpio, s_hWLANPwrPin, NvOdmGpioPinMode_Output);
    NvOdmGpioSetState(s_hGpio, s_hWLANPwrPin, 0x0);

    NvOdmGpioConfig(s_hGpio, s_hBTPwrPin, NvOdmGpioPinMode_Output);
    NvOdmGpioSetState(s_hGpio, s_hBTPwrPin, 0x0);

    NvOsSleepMS(100);

    return  NvSuccess;
}

static void NvPcbWifiGpioRelease(void)
{
    if (s_hGpio)
    {
        if (s_hWLANPwrPin)
            NvOdmGpioReleasePinHandle(s_hGpio, s_hWLANPwrPin);
        if (s_hWLANRstPin)
            NvOdmGpioReleasePinHandle(s_hGpio, s_hWLANRstPin);
        if (s_hBTPwrPin)
            NvOdmGpioReleasePinHandle(s_hGpio, s_hBTPwrPin);
        NvOdmGpioClose(s_hGpio);
    }

    s_hWLANPwrPin = 0;
    s_hWLANRstPin = 0;
    s_hBTPwrPin = 0;
    s_hGpio = 0;

}
static NvError WifiInit(void *pInfo)
{
    // enable CLK_32K_OUT of AP -->
    NvError e = NvSuccess;
    NvOdmServicesPwmHandle hOdmPwm = NULL;
    NvU32 RequestedPeriod, ReturnedPeriod;
    NvU32 DutyCycle;

    hOdmPwm = NvOdmPwmOpen();
    if (hOdmPwm)
    {
        DutyCycle = 0;
        RequestedPeriod = 0;

        NvOdmPwmConfig(hOdmPwm, NvOdmPwmOutputId_Blink,
            NvOdmPwmMode_Blink_Disable,
            DutyCycle, &RequestedPeriod, &ReturnedPeriod);

        NvOsSleepMS(100);

        NvOdmPwmConfig(hOdmPwm, NvOdmPwmOutputId_Blink,
            NvOdmPwmMode_Blink_32KHzClockOutput,
            DutyCycle, &RequestedPeriod, &ReturnedPeriod);

        NvOdmPwmClose(hOdmPwm);

        e = NvPcbWifiGpioInit();
        if (NvSuccess != e)
            NvOsDebugPrintf("%s: NvPcbWifiGpioInit fail. error=0x%08X \n", __func__, e);
        else
            NvPcbWifiPower(1);
    }
    else
    {
        NvOsDebugPrintf("%s: can not open OdmPwm \n", __func__);
        e = NvError_InvalidState;
    }
    return e;

}
static NvError WifiDeinit(void *pInfo)
{
    NvPcbWifiPower(0);
    NvPcbWifiGpioRelease();
    return NvSuccess;
}

#define SDIO_VENDOR_ID_TI    0x0097
#define SDIO_DEVICE_ID_TI_WL1271    0x4076
static WifiTestPrivateData sWifi = {
    WifiInit,
    WifiDeinit,
    PORT_SDMMC1,
    IO_FUNCTION2, // test function
    0x18000, // test address
    1, // test count
    0, 0,
    SDIO_VENDOR_ID_TI, SDIO_DEVICE_ID_TI_WL1271
};



/* PCBTest data table for Macallan */
static PcbTestData sPcbTestDataList_Macallan[] = {
    { "osc", "osc_test",         NvApiOscCheck, (void *)&sOscData },
    { "emmc", "emmc_test",       NvApiEmmcCheck , (void *)&sEmmcData},
    { "pmu", "pmu_test",         NvApiI2cTransfer, (void *)sI2cTPS65913 },
    { "audio", "audio_test",     NvApiI2cTransfer, (void *)sI2cAlc5639 },
    { "charger", "charger_test", NvApiI2cTransfer, (void *)sI2cBq24192 },
    { "accgyro", "accgyro_test", NvApiI2cTransfer, (void *)sI2cMpu6050 },
    { "compass", "compass_test", NvApiI2cTransfer, (void *)sI2cCompass },
    { "als", "als_test",         NvApiI2cTransfer, (void *)sI2cCM3217 },
    { "fuel", "fuel_test",       NvApiI2cTransfer, (void *)sI2cMAX17048 },
    { "pwrmon", "pwrmon_test",   NvApiI2cTransfer, (void *)sI2cINA219 },
    { "wifi", "wifi_test",       NvPcbWifiTest, (void *)&sWifi },
    { "key", "key_test",         NvPcbKeyTest, NULL },
    { NULL, NULL, NULL, NULL },
};

PcbTestData *NvGetPcbTestDataList(void)
{
    return sPcbTestDataList_Macallan;
}

