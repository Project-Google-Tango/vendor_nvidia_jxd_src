/*
 * Copyright (c) 2012 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: SMB349-driver Interface</b>
  *
  * @b Description: Implements the SMB349 charger drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_smb349_battery_charger_driver.h"

#define SMB349_PMUGUID NV_ODM_GUID('s','m','b','3','4','9',' ',' ')
#define SMB349_I2C_SPEED 400

#define DUMB_CHARGER_LIMIT_MA      250
#define DEDICATED_CHARGER_LIMIT_MA 1000   // 1000mA for dedicated charger

#define SMB349_CHARGE		0x00
#define SMB349_CHRG_CRNTS	0x01
#define SMB349_VRS_FUNC		0x02
#define SMB349_FLOAT_VLTG	0x03
#define SMB349_CHRG_CTRL	0x04
#define SMB349_STAT_TIME_CTRL	0x05
#define SMB349_PIN_CTRL		0x06
#define SMB349_THERM_CTRL	0x07
#define SMB349_CTRL_REG		0x09

#define SMB349_OTG_TLIM_REG	0x0A
#define SMB349_HRD_SFT_TEMP	0x0B
#define SMB349_FAULT_INTR	0x0C
#define SMB349_STS_INTR_1	0x0D
#define SMB349_SYSOK_USB3	0x0E
#define SMB349_IN_CLTG_DET	0x10
#define SMB349_STS_INTR_2	0x11

#define SMB349_CMD_REG		0x30
#define SMB349_CMD_REG_B	0x31
#define SMB349_CMD_REG_c	0x33

#define SMB349_INTR_STS_A	0x35
#define SMB349_INTR_STS_B	0x36
#define SMB349_INTR_STS_C	0x37
#define SMB349_INTR_STS_D	0x38
#define SMB349_INTR_STS_E	0x39
#define SMB349_INTR_STS_F	0x3A

#define SMB349_STS_REG_A	0x3B
#define SMB349_STS_REG_B	0x3C
#define SMB349_STS_REG_C	0x3D
#define SMB349_STS_REG_D	0x3E
#define SMB349_STS_REG_E	0x3F

#define SMB349_ENABLE_WRITE	1
#define SMB349_DISABLE_WRITE	0
#define ENABLE_WRT_ACCESS	0x80
#define THERM_CTRL		0x10
#define BATTERY_MISSING		0x10
#define CHARGING		0x06
#define DEDICATED_CHARGER	0x04
#define CHRG_DOWNSTRM_PORT	0x08
#define ENABLE_CHARGE		0x02

#define BIT(nr)			(1UL << (nr))
#define APSD			BIT(2)
#define AICL			BIT(4)
#define USBHC_MODE		BIT(0)
#define USBCS_CTRL		BIT(4)
#define CHRG_LIMIT_1600MA	0x30
#define INPT_LIMIT_2000MA	0x0A

typedef struct Smb349DriverRec {
    NvU32 OpenCount;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
} Smb349CoreDriver;

static Smb349CoreDriver s_Smb349CoreDriver = {0, NULL, 0};


static NvBool
GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(SMB349_PMUGUID);
    if (pConnectivity)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_I2c)
            {
                *pI2cModule   = NvOdmIoModule_I2c;
                *pI2cInstance = pConnectivity->AddressList[i].Instance;
                *pI2cAddress  = pConnectivity->AddressList[i].Address;
                return NV_TRUE;
            }
        }
    }
    NvOdmOsPrintf("%s: The system did not discover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}


static NvBool
Smb349ConfigWrites(NvU8 Value)
{
    if (Value == SMB349_ENABLE_WRITE)
    {
        if (!NvOdmPmuI2cSetBits(s_Smb349CoreDriver.hOdmPmuI2c,
                    s_Smb349CoreDriver.DeviceAddr,
                    SMB349_I2C_SPEED, SMB349_CMD_REG, ENABLE_WRT_ACCESS))
        {
            NvOdmOsPrintf("%s: Error SMB349 register update failed.\n", __func__);
            return NV_FALSE;
        }
    }
    else
    {
        if (!NvOdmPmuI2cClrBits(s_Smb349CoreDriver.hOdmPmuI2c,
                    s_Smb349CoreDriver.DeviceAddr,
                    SMB349_I2C_SPEED, SMB349_CMD_REG, ENABLE_WRT_ACCESS))
        {
            NvOdmOsPrintf("%s: Error SMB349 register update failed.\n", __func__);
            return NV_FALSE;
        }
    }
    return NV_TRUE;
}


NvOdmBatChargingDriverInfoHandle
Smb349BatChargingDriverOpen(
    NvOdmPmuDeviceHandle hDevice,
    OdmBatteryChargingProps *pBChargingProps)
{
    NvBool Status;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;

    if (!s_Smb349CoreDriver.OpenCount)
    {
        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
            return NULL;

        NvOdmOsMemset(&s_Smb349CoreDriver, 0, sizeof(Smb349CoreDriver));

        s_Smb349CoreDriver.hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
        if (!s_Smb349CoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error SMB349 Open I2C device.\n", __func__);
            return NULL;
        }
        s_Smb349CoreDriver.DeviceAddr = I2cAddress;

    }

    s_Smb349CoreDriver.OpenCount++;
    return &s_Smb349CoreDriver;
}

void Smb349BatChargingDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver)
{
    if (!s_Smb349CoreDriver.OpenCount)
        return;
    s_Smb349CoreDriver.OpenCount--;
    if (!s_Smb349CoreDriver.OpenCount)
    {
        NvOdmPmuI2cClose(s_Smb349CoreDriver.hOdmPmuI2c);
        NvOdmOsMemset(&s_Smb349CoreDriver, 0, sizeof(Smb349CoreDriver));
    }
}

NvBool
Smb349BatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMax,
    NvOdmUsbChargerType ChargerType)
{
    NvS8 Val = 0;
    NvU8 Data = 0;

    Smb349BatChargingDriverOpen(NULL, NULL);
    if (!NvOdmPmuI2cRead8(s_Smb349CoreDriver.hOdmPmuI2c,
                s_Smb349CoreDriver.DeviceAddr,
                SMB349_I2C_SPEED, SMB349_INTR_STS_B, &Val))
    {
        NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
        return NV_FALSE;
    }
    if (Val < 0)
    {
        NvOdmOsPrintf("%s: Error SMB349 failed to read battery status.\n", __func__);
        return NV_FALSE;
    }
    if (Val & BATTERY_MISSING)
        NvOdmOsPrintf("%s: Battery is missing.\n", __func__);
    else
        NvOdmOsPrintf("%s: Battery is present.\n", __func__);

    //Configure charger
    // Enable volatile writes to registers
    if (!Smb349ConfigWrites(SMB349_ENABLE_WRITE))
    {
        NvOdmOsPrintf("%s: Error SMB349 write enable failed.\n", __func__);
        return NV_FALSE;
    }

    // Enable charging
    if (chargingCurrentLimitMax)
    {
        if (!NvOdmPmuI2cSetBits(s_Smb349CoreDriver.hOdmPmuI2c,
                s_Smb349CoreDriver.DeviceAddr,
                SMB349_I2C_SPEED, SMB349_CMD_REG, ENABLE_CHARGE))
        {
            NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
            return NV_FALSE;
        }

        Val = (ChargerType == NvOdmUsbChargerType_UsbHost) ?
                CHRG_LIMIT_1600MA : (CHRG_LIMIT_1600MA | INPT_LIMIT_2000MA);

        if (!NvOdmPmuI2cSetBits(s_Smb349CoreDriver.hOdmPmuI2c,
                                s_Smb349CoreDriver.DeviceAddr,
                                SMB349_I2C_SPEED, SMB349_CHARGE,
                                Val))
        {
            NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
            return NV_FALSE;
        }
        if (!NvOdmPmuI2cClrBits(s_Smb349CoreDriver.hOdmPmuI2c,
                                s_Smb349CoreDriver.DeviceAddr,
                                SMB349_I2C_SPEED, SMB349_VRS_FUNC, APSD | AICL))
        {
            NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
            return NV_FALSE;
        }
        if (!NvOdmPmuI2cSetBits(s_Smb349CoreDriver.hOdmPmuI2c,
                                s_Smb349CoreDriver.DeviceAddr,
                                SMB349_I2C_SPEED, SMB349_VRS_FUNC, AICL))
        {
            NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
            return NV_FALSE;
        }
        if (!NvOdmPmuI2cSetBits(s_Smb349CoreDriver.hOdmPmuI2c,
                                s_Smb349CoreDriver.DeviceAddr,
                                SMB349_I2C_SPEED, SMB349_CMD_REG_B, USBHC_MODE))
        {
            NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
            return NV_FALSE;
        }
        if (!NvOdmPmuI2cClrBits(s_Smb349CoreDriver.hOdmPmuI2c,
                                s_Smb349CoreDriver.DeviceAddr,
                                SMB349_I2C_SPEED, SMB349_PIN_CTRL, USBCS_CTRL))
        {
            NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
            return NV_FALSE;
        }
        if (!NvOdmPmuI2cSetBits(s_Smb349CoreDriver.hOdmPmuI2c,
                                s_Smb349CoreDriver.DeviceAddr,
                                SMB349_I2C_SPEED, SMB349_THERM_CTRL, THERM_CTRL))
        {
            NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
            return NV_FALSE;
        }
    }
    else
    {
        if (!NvOdmPmuI2cClrBits(s_Smb349CoreDriver.hOdmPmuI2c,
                s_Smb349CoreDriver.DeviceAddr,
                SMB349_I2C_SPEED, SMB349_CMD_REG, ENABLE_CHARGE))
        {
            NvOdmOsPrintf("%s: Error SMB349 register access failed.\n", __func__);
            return NV_FALSE;
        }
    }

    // Disable volatile writes to registers
    if (!Smb349ConfigWrites(SMB349_DISABLE_WRITE))
    {
        NvOdmOsPrintf("%s: Error SMB349 write disable failed.\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}
