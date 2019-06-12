/*
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_bq2419x_batterycharger_driver.h"
#include "nvodm_services.h"
#include "nvos.h"

#define PMU_E1761          0x6e1
#define BQ2419xA_I2C_SPEED 100
#define DUMB_CHARGER_LIMIT_MA      500
#define CDP_DCP_LIMIT_MA 1500
#define DEDICATED_CHARGER_LIMIT_MA 3000   // 1800mA for dedicated charger

#define BIT(x) (1 << x)
#define BQ2419xA_FFD_PMUGUID NV_ODM_GUID('f','f','d','2','4','1','9','x')
#define BQ2419xA_PMUGUID NV_ODM_GUID('b','q','2','4','1','9','x','*')

#define CHECK_CHARGE_COMPLETE(x)(((x)& 0x30) >> 4)
#define CHECK_USB_UNPLUG(x) (((x)& 0x04) >> 2) //No DC source  1:usb plug,  0: usb unplug
#define CHECK_WD_EXPIRY(x) (faultreg >> 7)
#define CHECK_SAFETY_EXP(x)  (((x)& 0x30) >> 4)

typedef struct BQ2419xCoreDriverRec {
    NvOdmOsSemaphoreHandle hEventSema;
    NvOdmOsThreadHandle         hBattEventThread;
    NvOdmGpioPinHandle hOdmPins;
    NvOdmServicesGpioIntrHandle IntrHandle;
    NvOdmServicesGpioHandle hOdmGpio;
    NvBool BqInitDone;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr[3];
} BQ2419xCoreDriver;

typedef enum {
    slave_charger,
    slave_pmu,
    slave_rom,
}slave_id;

static NvOdmPmuDeviceHandle s_hPmu = NULL;

static BQ2419xCoreDriver s_BQ2419xCoreDriver = {NULL, NULL, NULL,
                           NULL, NULL, NV_FALSE, NULL, {0, 0, 0}};

static
NvBool BQ2419xRegisterWrite8(
    Bq2419xCoreDriverHandle hBQ2419xCore,
    NvU8 RegAddr,
    slave_id slave,
    NvU8 Data)
{
    NV_ASSERT(hBQ2419xCore);
    if (!hBQ2419xCore)
    {
        NvOdmOsPrintf(" hBQ2419xCore is NULL \n");
        return NV_FALSE ;
    }
    return NvOdmPmuI2cWrite8(hBQ2419xCore->hOdmPmuI2c,
                hBQ2419xCore->DeviceAddr[slave],
                BQ2419xA_I2C_SPEED, RegAddr, Data);
}

static
NvBool BQ2419xRegisterRead8(
    Bq2419xCoreDriverHandle hBQ2419xCore,
    NvU8 RegAddr,
    slave_id slave,
    NvU8* Data)
{
    return NvOdmPmuI2cRead8(hBQ2419xCore->hOdmPmuI2c,
                hBQ2419xCore->DeviceAddr[slave],
                BQ2419xA_I2C_SPEED, RegAddr, Data);
}

static
NvBool BQ2419xRegisterUpdate8(
    Bq2419xCoreDriverHandle hBQ2419xCore,
    NvU8 RegAddr,
    slave_id slave,
    NvU8 Mask,
    NvU8 Val)
{
    NvU8 regread;
    NvU8 temp;
    NvBool status = NV_TRUE;

    NV_ASSERT(hBQ2419xCore);
    if (!hBQ2419xCore)
    {
        NvOdmOsPrintf(" hBQ2419xCore is NULL \n");
        return NV_FALSE ;
    }

    status = NvOdmPmuI2cRead8(hBQ2419xCore->hOdmPmuI2c,
                hBQ2419xCore->DeviceAddr[slave],
                BQ2419xA_I2C_SPEED, RegAddr, &regread);

    if (!status)
    {
        NvOdmOsPrintf("%s:  Failed to read register \n", __func__);
        return NV_FALSE;
    }

    temp = regread & ~Mask;
    temp |= Val & Mask;

    if (temp != regread)
        return NvOdmPmuI2cWrite8(hBQ2419xCore->hOdmPmuI2c,
                hBQ2419xCore->DeviceAddr[slave],
                BQ2419xA_I2C_SPEED, RegAddr, temp);
    return NV_TRUE;
}

#define PMU_E1769           0x06E9 //TI 913/4

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity;
    NvOdmPmuBoardInfo PmuBoardInfo;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PmuBoard,
                                 &PmuBoardInfo, sizeof(PmuBoardInfo));
    if (PmuBoardInfo.BoardInfo.BoardID == PMU_E1761||PmuBoardInfo.BoardInfo.BoardID == PMU_E1769)
        pConnectivity = NvOdmPeripheralGetGuid(BQ2419xA_FFD_PMUGUID);
    else
        pConnectivity = NvOdmPeripheralGetGuid(BQ2419xA_PMUGUID);

    NV_ASSERT(pI2cModule);
    NV_ASSERT(pI2cInstance);
    NV_ASSERT(pI2cAddress);
    if (!pI2cModule || !pI2cInstance || !pI2cAddress)
        return NV_FALSE;

    if (pConnectivity)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_I2c)
            {
                *pI2cModule   = NvOdmIoModule_I2c;
                *pI2cInstance = pConnectivity->AddressList[i].Instance;
                *pI2cAddress  = pConnectivity->AddressList[i].Address;
            }
            pI2cAddress++;
            pI2cInstance++;
        }
        return NV_TRUE;
    }
    NvOdmOsPrintf("%s: The system did not discover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

static void
BQ2419xClearHiZ(Bq2419xCoreDriverHandle hDevice)
{

    NvBool Status = NV_TRUE;

    NV_ASSERT(hDevice);

    if (!hDevice)
    {
        NvOdmOsPrintf("hDevice is NULL \n");
        return;
    }

    Status = BQ2419xRegisterUpdate8(hDevice, BQ2419X_INPUT_SRC_REG,
                     slave_charger, DISABLE_HIZ, 0);
    if (!Status)
        NvOdmOsPrintf("%s: Register update failed \n", __func__);

    NvOdmOsPrintf("%s: HIZ bit cleared \n", __func__);
}

#if defined(NV_TARGET_LOKI) || defined(NV_TARGET_ARDBEG)
//Interrupt should lead to here
static void GpioIrq(void *arg)
{

    Bq2419xCoreDriverHandle hBQ2419xCore = &s_BQ2419xCoreDriver;
    if(hBQ2419xCore->hEventSema)
        NvOdmOsSemaphoreSignal(hBQ2419xCore->hEventSema);
    NvOdmGpioInterruptDone(hBQ2419xCore->IntrHandle);
}

 Bq2419xCoreDriverHandle global_bq;
int get_bqvbus()
{
	NvU8 Data = 0;

	NvBool Status = NV_TRUE;

	if (global_bq){

		Status = BQ2419xRegisterRead8(global_bq, BQ2419X_SYS_STAT_REG,
						   slave_charger, &Data);
	 	if (!Status)
            NvOdmOsPrintf("%s: Register Read failed \n", __func__);
		
		if (CHECK_USB_UNPLUG(Data)){
			return 1;
		}

	}
	return 0;

}

static void GpioInterruptHandler(void *arg)
{

     Bq2419xCoreDriverHandle hBQ2419xCore = (Bq2419xCoreDriverHandle)arg;
     NvU8 Data = 0;
     NvU8 faultreg = 0;
     DateTime date;
     NvBool Status = NV_TRUE;

    if (!hBQ2419xCore) return;

    while (1)
    {
        NvOdmOsSemaphoreWait(hBQ2419xCore->hEventSema);

        Status = BQ2419xRegisterRead8(hBQ2419xCore, BQ2419X_SYS_STAT_REG,
                         slave_charger, &Data);
        if (!Status)
            NvOdmOsPrintf("%s: %d Register read failed \n", __func__, __LINE__);

        Status = BQ2419xRegisterRead8(hBQ2419xCore, BQ2419X_FAULT_REG,
                         slave_charger, &faultreg);
        if (!Status)
            NvOdmOsPrintf("%s: Register Read failed \n", __func__);

        NvOsDebugPrintf("BQ charger interrupt is recieved \n");

        if (CHECK_CHARGE_COMPLETE(Data) == BQ2419x_CHARGE_COMPLETE)
        {
            NvOsDebugPrintf("Charge complete \n");
            BQ2419xClearHiZ (hBQ2419xCore);
            BQ2419xBatChargingDriverSetCurrent(hBQ2419xCore, NvOdmPmuChargingPath_UsbBus,
                3000, NvOdmUsbChargerType_UsbHost);

            //Disable Charging
            Status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_PWR_ON_REG, \
                         slave_charger, ENABLE_CHARGE_MASK, DISABLE_CHARGE);
            if (!Status)
                NvOdmOsPrintf("%s: Register Update failed \n", __func__);

            NvOdmPmuGetDate(s_hPmu, &date);
            NvOdmPmuSetAlarm(s_hPmu, NvAppAddAlarmSecs(date, 3600));
            NvOdmPmuPowerOff(s_hPmu);
        }
        else if (!CHECK_USB_UNPLUG(Data))
        {
            NvOsDebugPrintf("USB unplug event happened\n");
            BQ2419xClearHiZ (hBQ2419xCore);
            BQ2419xBatChargingDriverSetCurrent(hBQ2419xCore, NvOdmPmuChargingPath_UsbBus,
                3000, NvOdmUsbChargerType_UsbHost);
            NvOdmPmuGetDate(s_hPmu, &date);
            NvOdmPmuSetAlarm(s_hPmu, NvAppAddAlarmSecs(date, 3600));
            //Enable charging
            Status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_PWR_ON_REG, \
                         slave_charger, ENABLE_CHARGE_MASK, ENABLE_CHARGE);
            if (!Status)
                NvOdmOsPrintf("%s: Register update failed \n", __func__);
            NvOdmPmuPowerOff(s_hPmu);
        }
        else if(CHECK_WD_EXPIRY(faultreg))
        {
            Status = BQ2419xRegisterRead8(hBQ2419xCore, BQ2419X_FAULT_REG,
                         slave_charger, &faultreg);
            if (!Status)
                NvOdmOsPrintf("%s: Register read failed \n", __func__);
            NvOsDebugPrintf("Watch dog fault occurred\n");
            //Enable charging
             BQ2419xBatChargingDriverSetCurrent(hBQ2419xCore, NvOdmPmuChargingPath_UsbBus,
                3000, NvOdmUsbChargerType_UsbHost);
            Status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_PWR_ON_REG, \
                         slave_charger, ENABLE_CHARGE_MASK, ENABLE_CHARGE);
            if (!Status)
                NvOdmOsPrintf("%s: Register update failed \n", __func__);
        }
        else
        {
            NvOsDebugPrintf("Some Fault occurred in charger\n");
        }
    }
}
#endif

NvOdmBatChargingDriverInfoHandle
BQ2419xDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    Bq2419xCoreDriverHandle hBQ2419xCore = NULL;
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance[3] = {0, 0, 0};
    NvU32 I2cAddress[3] = {0, 0, 0};
    NvU8 Data = 0;

    hBQ2419xCore = &s_BQ2419xCoreDriver;
	global_bq = hBQ2419xCore;

    if (!hBQ2419xCore->BqInitDone)
    {
        s_hPmu = hDevice;

        Status = GetInterfaceDetail(&I2cModule, I2cInstance, I2cAddress);
        if (!Status)
        {
            NvOdmOsPrintf("%s: Error in getting Interface details.\n", __func__);
            return NULL;
        }
        NvOdmOsMemset(hBQ2419xCore, 0, sizeof(BQ2419xCoreDriver));

        hBQ2419xCore->hOdmPmuI2c = NvOdmPmuI2cOpen(NvOdmIoModule_I2c, I2cInstance[slave_charger]);
        if (!hBQ2419xCore->hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
            return NULL;
        }
        hBQ2419xCore->DeviceAddr[slave_charger]= I2cAddress[slave_charger];
        hBQ2419xCore->DeviceAddr[slave_pmu]= I2cAddress[slave_pmu];
        hBQ2419xCore->DeviceAddr[slave_rom]= I2cAddress[slave_rom];

        //Set FastCharge Limit, Based on battery currently 2.6 Amps is max
        Status = BQ2419xRegisterUpdate8 (hBQ2419xCore, BQ2419X_CHRG_CTRL_REG, slave_charger,
                                                            INPUT_CURRENT_CTRL_MASK,  MAX_CURRENT);
        if (!Status)
            NvOdmOsPrintf("%s: Register update failed \n", __func__);

        //Update VINDPM to default
        Status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_INPUT_SRC_REG, slave_charger, \
                INPUT_VOLTAGE_LIMIT_MASK, DEFAULT_VOLTAGE_LIMIT);
        if (!Status)
            NvOdmOsPrintf("%s: Register update failed \n", __func__);

        Status = BQ2419xRegisterRead8(hBQ2419xCore, BQ2419X_FAULT_REG, slave_charger, &Data);
        if (!Status)
            NvOdmOsPrintf("%s: Register read failed \n", __func__);

        //Safety Timer Recovery
        if (CHECK_SAFETY_EXP(Data) == SAFETY_TIMER_SET)
        {
            //Toggle Safety timer enable
            Status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_TIME_CTRL_REG, slave_charger, \
                                   (1<<ENABLE_TIMER_BIT), 0);
            if (!Status)
                 NvOdmOsPrintf("%s: Register update failed \n", __func__);

            Status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_TIME_CTRL_REG, slave_charger, \
                                   (1<<ENABLE_TIMER_BIT),  (1<<ENABLE_TIMER_BIT));
            if (!Status)
                NvOdmOsPrintf("%s: Register update failed \n", __func__);
        }

        //Enable charging
        Status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_PWR_ON_REG, slave_charger,\
                           ENABLE_CHARGE_MASK, ENABLE_CHARGE);
        if (!Status)
            NvOdmOsPrintf("%s: Register update failed \n", __func__);

        //Disable any accumulated Watch Dog Faults
        Status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_PWR_ON_REG, slave_charger,\
                                            (1 << WD_RESET_BIT),  (1 << WD_RESET_BIT));
        if (!Status)
            NvOdmOsPrintf("%s: Register update failed \n", __func__);

        Status = BQ2419xRegisterRead8(hBQ2419xCore, BQ2419X_FAULT_REG, slave_charger, &Data);
        if (!Status)
            NvOdmOsPrintf("%s: Register read failed \n", __func__);

        Status = BQ2419xRegisterRead8(hBQ2419xCore, BQ2419X_FAULT_REG, slave_charger, &Data);
        if (!Status)
            NvOdmOsPrintf("%s: Register read failed \n", __func__);

        //Clear HiZ bit
        BQ2419xClearHiZ(hBQ2419xCore);

        /* Enable termination, Enable WD, EN_TIMER(safety 8hrs)*/
        Status = BQ2419xRegisterWrite8(hBQ2419xCore, \
                                       BQ2419X_TIME_CTRL_REG, slave_charger, 0x9a);
        if (!Status)
            NvOdmOsPrintf("%s: Register update failed 0x5\n", __func__);

#if !defined(NV_TARGET_LOKI) && !defined(NV_TARGET_ARDBEG)
        //Disble Wireless charging
        Status = BQ2419xRegisterUpdate8(hBQ2419xCore, CHGR_CNTRL, slave_pmu,
                                                 WIRELESS_DIS_MASK, WIRELESS_DIS);
        if (!Status)
        {
            NvOdmOsPrintf("%s(): Error in updating charge control register\n", __func__);
            return NV_FALSE;
        }

        // Clear USB SUSPEND bit/Set Default USB current 500mA
        Status = BQ2419xRegisterUpdate8(hBQ2419xCore, USB_CHGCTL1, slave_rom,
                                                 USB_SUSPEND_MASK, USB_SUSPEND);
        if (!Status)
        {
            NvOdmOsPrintf("%s(): Error in updating USBcontrol register\n", __func__);
            return NV_FALSE;
        }
#endif
    }

    hBQ2419xCore->BqInitDone = NV_TRUE;

    return hBQ2419xCore;
}

NvBool BQ2419xCheckChargeComplete (NvOdmBatChargingDriverInfoHandle hBChargingDriver)
{
    Bq2419xCoreDriverHandle hBQ2419xCore = NULL;
    NvBool Status = NV_FALSE;
    NvU8 Data = 0;

    NV_ASSERT(hBChargingDriver);
    if (!hBChargingDriver)
    {
        NvOdmOsPrintf("hBChargingDriver is NULL \n");
        return  NV_FALSE;
    }

    hBQ2419xCore = (Bq2419xCoreDriverHandle)hBChargingDriver;

    Status = BQ2419xRegisterRead8(hBQ2419xCore, BQ2419X_SYS_STAT_REG,
                    slave_charger, &Data);
    if (!Status)
        NvOdmOsPrintf("%s: %d Register read failed \n", __func__, __LINE__);

    if (CHECK_CHARGE_COMPLETE(Data) == BQ2419x_CHARGE_COMPLETE)
    {
        NvOsDebugPrintf ("Battery is fully charged \n");
        return NV_TRUE;
    }
    return NV_FALSE;
}


void BQ2419xDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver)
{
    Bq2419xCoreDriverHandle hBQ2419xCore = NULL;
    NvBool status;

    NV_ASSERT(hBChargingDriver);
    if (!hBChargingDriver)
    {
        NvOdmOsPrintf("hBChargingDriver is NULL \n");
        return ;
    }

    hBQ2419xCore = (Bq2419xCoreDriverHandle)hBChargingDriver;

    if (hBQ2419xCore->BqInitDone)
    {

        //Setting charging to 500ma
        status = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_INPUT_SRC_REG, slave_charger,\
                               INPUT_CURRENT_LIMIT_MASK, 0x01);
        if (!status)
            NvOdmOsPrintf("%s: Register update failed \n", __func__);

        //Clear HIZ
        BQ2419xClearHiZ(hBQ2419xCore);
        if (!status)
            NvOdmOsPrintf("%s: Register update failed \n", __func__);

#if defined(NV_TARGET_LOKI) || defined(NV_TARGET_ARDBEG)
        //Unregister the BQ interrrupt if enabled
        if (hBQ2419xCore->hOdmGpio)
            NvOdmGpioInterruptUnregister(hBQ2419xCore->hOdmGpio, hBQ2419xCore->hOdmPins,
                hBQ2419xCore->IntrHandle);
#endif

        NvOdmPmuI2cClose(hBQ2419xCore->hOdmPmuI2c);
        hBQ2419xCore->BqInitDone = NV_FALSE;
        NvOdmOsMemset(hBQ2419xCore, 0, sizeof(BQ2419xCoreDriver));
        return;
    }
}

#if defined(NV_TARGET_LOKI) || defined(NV_TARGET_ARDBEG)
static NvU32 CurrentFromUsb(NvOdmUsbChargerType ChargerType)
{
    switch (ChargerType){
        case NvOdmUsbChargerType_NVCharger:
        case NvOdmUsbChargerType_CDP:
        case NvOdmUsbChargerType_DCP:
            return 6;        // 2 Amps
        case NvOdmUsbChargerType_UsbHost:
        default:
            return 6;      // 2: 500mA
    }
}
#endif

NvBool
BQ2419xBatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    Bq2419xCoreDriverHandle hBQ2419xCore = NULL;
    NvBool ret;
    NvU8 val = 0;
#if defined(NV_TARGET_LOKI) || defined(NV_TARGET_ARDBEG)
    NvU8 Data = 0;
    NvU8 floor = 5;
#else
    DateTime date;
#endif

    hBQ2419xCore =  (Bq2419xCoreDriverHandle)hBChargingDriverInfo;
    NV_ASSERT(hBQ2419xCore);

    BQ2419xClearHiZ (hBQ2419xCore);

    if (!hBQ2419xCore)
    {
        NvOdmOsPrintf("hBQ2419xCore is NULL \n");
        return NV_FALSE;
    }

#if defined(NV_TARGET_LOKI) || defined(NV_TARGET_ARDBEG)
    if (!hBQ2419xCore->hEventSema)
    {
        NvOsDebugPrintf("Enabling interrupt in BQCharger \n");
        hBQ2419xCore->hEventSema = NvOdmOsSemaphoreCreate(0);
        if (!hBQ2419xCore->hEventSema)
        {
            NvOdmOsPrintf("%s: Error In attatining Semaphore \n", __func__);
            return NV_FALSE;
        }

        hBQ2419xCore->hOdmGpio  = NvOdmGpioOpen();

        /* Thread to handle Battery events */
        hBQ2419xCore->hBattEventThread  = NvOdmOsThreadCreate(
                                (NvOdmOsThreadFunction)GpioInterruptHandler,
                                hBQ2419xCore);

        hBQ2419xCore->hOdmPins = NvOdmGpioAcquirePinHandle(hBQ2419xCore->hOdmGpio, 'j' - 'a', 0);

        NvOdmGpioConfig(hBQ2419xCore->hOdmGpio, hBQ2419xCore->hOdmPins, NvOdmGpioPinMode_InputData);

        if (!NvOdmGpioInterruptRegister(hBQ2419xCore->hOdmGpio,
                      &hBQ2419xCore->IntrHandle, hBQ2419xCore->hOdmPins,
                      NvOdmGpioPinMode_InputInterruptRisingEdge,
                      GpioIrq, (void *)(0), 0x00))
        {
            NvOsDebugPrintf("Interrupt register failed ..... \n");
        }

        ret = BQ2419xRegisterRead8(hBQ2419xCore, BQ2419X_SYS_STAT_REG,
                slave_charger, &Data);
        if (!ret)
            NvOdmOsPrintf("%s: Register update failed 0x5\n", __func__);

        if (!CHECK_USB_UNPLUG(Data))
            NvOdmOsSemaphoreSignal(hBQ2419xCore->hEventSema);  //USB unplug exists
    }
#endif

    if (chargingCurrentLimitMa == 0)
    {
        NvOdmOsPrintf(" Disabling further charging ");

        ret = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_INPUT_SRC_REG, slave_charger,\
                               INPUT_CURRENT_LIMIT_MASK, 0x00);

        if (!ret)
        {
            NvOdmOsPrintf("%s(): Error in setting  the charging to 100mA\n", __func__);
            return NV_FALSE;
        }
        return NV_TRUE;
    }
#if 0
    if (chargingCurrentLimitMa > DEDICATED_CHARGER_LIMIT_MA)
        chargingCurrentLimitMa = DEDICATED_CHARGER_LIMIT_MA;

    if (chargingPath == NvOdmPmuChargingPath_UsbBus)
    {
#if defined(NV_TARGET_LOKI) || defined(NV_TARGET_ARDBEG)
        val =  CurrentFromUsb(ChargerType);

        for (; floor <= val; floor++)
        {
            ret =  BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_INPUT_SRC_REG, \
                            slave_charger, INPUT_CURRENT_LIMIT_MASK, floor);
            NvOsWaitUS(1000);
            NvOdmOsDebugPrintf (" Updating reg: %d \n", floor);
        }
#else
        if (ChargerType == NvOdmUsbChargerType_UsbHost)
        {

            val = 2;           //500mA
        }
        else
        {
            //Set alarm for 30 sec
            NvOdmPmuGetDate(s_hPmu, &date);
            NvOdmPmuSetAlarm(s_hPmu, NvAppAddAlarmSecs(date, 30));

            if ((ChargerType == NvOdmUsbChargerType_CDP) ||
                           (ChargerType == NvOdmUsbChargerType_DCP))
            {
                val = 5;          //1500mA
            }
            else if (ChargerType == NvOdmUsbChargerType_Proprietary)
            {
                val = 6;         //2000mA
            }
            else
            {
                val = 2;
            }
        }
#endif
        ret =  BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_INPUT_SRC_REG, slave_charger,\
                            INPUT_CURRENT_LIMIT_MASK, val);
    }
#endif
	BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_INPUT_SRC_REG, slave_charger, INPUT_CURRENT_LIMIT_MASK, 7);
#if defined(NV_TARGET_LOKI) || defined(NV_TARGET_ARDBEG)
    if ((ChargerType == NvOdmUsbChargerType_NVCharger) ||
            (ChargerType == NvOdmUsbChargerType_CDP)  ||
            (ChargerType == NvOdmUsbChargerType_DCP))
    {
        //Update VINDPM to remove noise
        ret = BQ2419xRegisterUpdate8(hBQ2419xCore, BQ2419X_INPUT_SRC_REG, \
                            slave_charger, INPUT_VOLTAGE_LIMIT_MASK, NV_VOLTAGE_LIMIT);
        if (!ret)
        {
            NvOdmOsPrintf("%s: Register update failed \n", __func__);
            return NV_FALSE;
        }
    }
#endif
    return NV_TRUE;
}
