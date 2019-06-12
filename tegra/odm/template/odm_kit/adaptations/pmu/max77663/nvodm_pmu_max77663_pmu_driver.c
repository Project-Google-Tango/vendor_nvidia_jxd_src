/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: Max77663 pmu driver</b>
  *
  * @b Description: Implements the Max77663 pmu drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_pmu_max77663_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

#define MAX77663PMUGUID    NV_ODM_GUID('m','a','x','7','7','6','6','3')
#define INVALID_VOLTAGE    0xFFFFFFFF

#define MAX77663_I2C_SPEED 100
#define MAX77663_REG_ONOFF_CFG1         0x41
#define ONOFF_SFT_RST_MASK              (1 << 7)

#define RAIL_TYPE_SD       0
#define RAIL_TYPE_LDO      1
#define RAIL_TYPE_GPIO     2
#define RAIL_TYPE_INVALID  3

#define POWER_MODE_DISABLE 0
#define POWER_MODE_GLPM    1
#define POWER_MODE_LPM     2
#define POWER_MODE_NORMAL  3
#define POWER_MODE_INVALID 4

#define FPS_SRC_0          0
#define FPS_SRC_1          1
#define FPS_SRC_2          2
#define FPS_SRC_NONE       3

typedef struct Max77663PmuInfoRec *Max77663PmuInfoHandle;

typedef struct Max77663PmuOpsRec {
    NvBool (*IsRailEnabled)(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id);
    NvBool (*EnableRail)(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                         NvU32 *DelayUS);
    NvBool (*DisableRail)(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                          NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                             NvU32 new_uV, NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                             NvU32 *uV);
} Max77663PmuOps;

typedef struct Max77663PmuPropsRec {
    NvU32 Id;
    NvU8 Type;

    NvU32 MinUV;
    NvU32 MaxUV;
    NvU32 StepUV;

    NvU8 VoltReg;
    NvU8 CfgReg;
    NvU8 FpsReg;

    NvU8 FpsSrc;
    NvU8 VoltMask;

    NvU8 PowerMode;
    NvU8 PowerModeMask;
    NvU8 PowerModeShift;

    /* Power rail specific turn-on delay */
    NvU32 Delay;
    Max77663PmuOps *pOps;
} Max77663PmuProps;

typedef struct Max77663RailInfoRec {
    Max77663PmuSupply Id;
    NvU32 SystemRailId;
    int UserCount;
    NvBool IsValid;
    Max77663PmuProps *pPmuProps;
    NvOdmPmuRailProps OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Max77663RailInfo;

typedef struct Max77663PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    Max77663RailInfo RailInfo[Max77663PmuSupply_Num];
} Max77663PmuInfo;

static NvBool Max77663IsRailEnabled(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id);
static NvBool Max77663RailEnable(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                 NvU32 *DelayUS);
static NvBool Max77663RailDisable(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                  NvU32 *DelayUS);
static NvBool Max77663RailSetVoltage(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 new_uV, NvU32 *DelayUS);
static NvBool Max77663RailGetVoltage(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 *uV);
static NvBool Max77663GpioRailIsEnabled(Max77663PmuInfoHandle hMaxPmuInfo,
                                        NvU32 Id);
static NvBool Max77663GpioRailEnable(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 *DelayUS);
static NvBool Max77663GpioRailDisable(Max77663PmuInfoHandle hMaxPmuInfo,
                                      NvU32 Id, NvU32 *DelayUS);
static NvBool Max77663GpioGetVoltage(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 *uV);
static NvBool Max77663GpioSetVoltage(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 new_uV, NvU32 *DelayUS);

static Max77663PmuOps s_PmuOps = {
    Max77663IsRailEnabled,
    Max77663RailEnable,
    Max77663RailDisable,
    Max77663RailSetVoltage,
    Max77663RailGetVoltage,
};

static Max77663PmuOps s_GpioPmuOps = {
    Max77663GpioRailIsEnabled,
    Max77663GpioRailEnable,
    Max77663GpioRailDisable,
    Max77663GpioSetVoltage,
    Max77663GpioGetVoltage,
};

#define REG_DATA(_Id, _Type, _MinUV, _MaxUV, _StepUV, _VoltReg, _CfgReg, \
                 _FpsReg, _VoltMask, _PowerModeMask, _PowerModeShift, _pOps) \
    { \
            Max77663PmuSupply_##_Id, RAIL_TYPE_##_Type, \
            _MinUV, _MaxUV, _StepUV, _VoltReg, _CfgReg, _FpsReg, 0, _VoltMask, \
            POWER_MODE_NORMAL, _PowerModeMask, _PowerModeShift, 500, _pOps, \
    }

static Max77663PmuProps s_Max77663PmuProps[] = {
    REG_DATA(Invalid, INVALID, 0, 0, 0, 0, 0, 0, 0, 0, 0, &s_PmuOps),
    REG_DATA(SD0, SD, 600000, 3387500, 12500, 0x16, 0x1D, 0x4F, 0xFF, 0x30, 4, &s_PmuOps),
    REG_DATA(SD1, SD, 800000, 1587500, 12500, 0x17, 0x1E, 0x50, 0x3F, 0x30, 4, &s_PmuOps),
    REG_DATA(SD2, SD, 600000, 3387500, 12500, 0x18, 0x1F, 0x51, 0xFF, 0x30, 4, &s_PmuOps),
    REG_DATA(SD3, SD, 600000, 3387500, 12500, 0x19, 0x20, 0x52, 0xFF, 0x30, 4, &s_PmuOps),
    REG_DATA(LDO0, LDO, 800000, 2350000, 25000, 0x23, 0x24, 0x46, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(LDO1, LDO, 800000, 2350000, 25000, 0x25, 0x26, 0x47, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(LDO2, LDO, 800000, 3950000, 50000, 0x27, 0x28, 0x48, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(LDO3, LDO, 800000, 3950000, 50000, 0x29, 0x2A, 0x49, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(LDO4, LDO, 800000, 1587500, 12500, 0x2B, 0x2C, 0x4A, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(LDO5, LDO, 800000, 3950000, 50000, 0x2D, 0x2E, 0x4B, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(LDO6, LDO, 800000, 3950000, 50000, 0x2F, 0x30, 0x4C, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(LDO7, LDO, 800000, 3950000, 50000, 0x31, 0x32, 0x4D, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(LDO8, LDO, 800000, 3950000, 50000, 0x33, 0x34, 0x4E, 0x3F, 0xC0, 6, &s_PmuOps),
    REG_DATA(GPIO0, GPIO, 0, 0, 0, 0, 0x36, FPS_SRC_NONE, 0, 0, 0, &s_GpioPmuOps),
    REG_DATA(GPIO1, GPIO, 0, 0, 0, 0, 0x37, FPS_SRC_NONE, 0, 0, 0, &s_GpioPmuOps),
    REG_DATA(GPIO2, GPIO, 0, 0, 0, 0, 0x38, FPS_SRC_NONE, 0, 0, 0, &s_GpioPmuOps),
    REG_DATA(GPIO3, GPIO, 0, 0, 0, 0, 0x39, FPS_SRC_NONE, 0, 0, 0, &s_GpioPmuOps),
    REG_DATA(GPIO4, GPIO, 0, 0, 0, 0, 0x3A, FPS_SRC_NONE, 0, 0, 0, &s_GpioPmuOps),
    REG_DATA(GPIO5, GPIO, 0, 0, 0, 0, 0x3B, FPS_SRC_NONE, 0, 0, 0, &s_GpioPmuOps),
    REG_DATA(GPIO6, GPIO, 0, 0, 0, 0, 0x3C, FPS_SRC_NONE, 0, 0, 0, &s_GpioPmuOps),
    REG_DATA(GPIO7, GPIO, 0, 0, 0, 0, 0x3D, FPS_SRC_NONE, 0, 0, 0, &s_GpioPmuOps),
};

static int Max77663SetPowerMode(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                NvU8 PowerMode)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
	NvU8 Addr;
	NvU8 Mask = pPmuProps->PowerModeMask;
	NvU8 Shift = pPmuProps->PowerModeShift;
	int Ret;

	if (pPmuProps->Type == RAIL_TYPE_SD)
		Addr = pPmuProps->CfgReg;
	else
		Addr = pPmuProps->VoltReg;

	Ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             MAX77663_I2C_SPEED, Addr, PowerMode << Shift, Mask);
	if (Ret < 0)
		return Ret;

	pPmuProps->PowerMode = PowerMode;
	return Ret;
}

static NvU8 Max77663GetPowerMode(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id)
{
	Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
	NvU8 Addr, Val;
	NvU8 Mask = pPmuProps->PowerModeMask;
	NvU8 Shift = pPmuProps->PowerModeShift;
	int Ret;

	if (pPmuProps->Type == RAIL_TYPE_SD)
		Addr = pPmuProps->CfgReg;
	else
		Addr = pPmuProps->VoltReg;

	Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                           MAX77663_I2C_SPEED, Addr, &Val);
	if (Ret < 0)
		return POWER_MODE_INVALID;

	pPmuProps->PowerMode = (Val & Mask) >> Shift;
	return pPmuProps->PowerMode;
}

static NvBool Max77663IsRailEnabled(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvBool Ret = NV_TRUE;

    if (pPmuProps->FpsSrc != FPS_SRC_NONE) {
        NvOdmOsDebugPrintf("%s() Rail%u is using FPS%u\n",
                           __func__, pPmuProps->Id, pPmuProps->FpsSrc);
		return Ret;
	}

    if (Max77663GetPowerMode(hMaxPmuInfo, Id) == POWER_MODE_DISABLE)
		Ret = NV_FALSE;

	return Ret;
}

static NvBool Max77663RailEnable(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                 NvU32 *DelayUS)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvBool Ret = NV_TRUE;

    if (pPmuProps->FpsSrc != FPS_SRC_NONE) {
        NvOdmOsDebugPrintf("%s() Rail%u is using FPS%u\n",
                           __func__, pPmuProps->Id, pPmuProps->FpsSrc);
		return Ret;
	}

    Ret = Max77663SetPowerMode(hMaxPmuInfo, Id, POWER_MODE_NORMAL);
    if (DelayUS)
        *DelayUS = pPmuProps->Delay;

    return Ret;
}

static NvBool Max77663RailDisable(Max77663PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                  NvU32 *DelayUS)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvBool Ret = NV_TRUE;

    if (pPmuProps->FpsSrc != FPS_SRC_NONE) {
        NvOdmOsDebugPrintf("%s() Rail%u is using FPS%u\n",
                           __func__, pPmuProps->Id, pPmuProps->FpsSrc);
		return Ret;
	}

    Ret = Max77663SetPowerMode(hMaxPmuInfo, Id, POWER_MODE_DISABLE);
    if (DelayUS)
        *DelayUS = pPmuProps->Delay;

    return Ret;
}

static NvBool Max77663RailSetVoltage(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 new_uV, NvU32 *DelayUS)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU32 Val;
    NvBool Ret;

    if ((new_uV < pPmuProps->MinUV) || (new_uV > pPmuProps->MaxUV))
        return NV_FALSE;

    Val = (new_uV - pPmuProps->MinUV) / pPmuProps->StepUV;
    Ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             MAX77663_I2C_SPEED, pPmuProps->VoltReg, Val,
                             pPmuProps->VoltMask);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in updating voltage register 0x%02x\n",
                      __func__, pPmuProps->VoltReg);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->Delay;
    return Ret;
}

static NvBool Max77663RailGetVoltage(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 *uV)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU8 Val;
    NvBool Ret;

    Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                           MAX77663_I2C_SPEED, pPmuProps->VoltReg, &Val);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading voltage register 0x%02x\n",
                    __func__, pPmuProps->VoltReg);
        return Ret;
    }
    Val &= Val & pPmuProps->VoltMask;
    *uV = Val * pPmuProps->StepUV + pPmuProps->MinUV;
    return NV_TRUE;
}

static NvBool Max77663GpioRailIsEnabled(Max77663PmuInfoHandle hMaxPmuInfo,
                                        NvU32 Id)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU8 Val;
    NvBool Ret = NV_FALSE;

    Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                           MAX77663_I2C_SPEED, pPmuProps->CfgReg, &Val);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in readin the supply register 0x%02x\n",
                      __func__, pPmuProps->CfgReg);
        return Ret;
    }

    if (Val & 0x08)
        return NV_TRUE;

    return Ret;
}
static NvBool Max77663GpioRailEnable(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 *DelayUS)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvBool Ret;

    /* Disable Pull-Up */
    Ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             MAX77663_I2C_SPEED, 0x3E, 0x0,
                             1 << (Id - Max77663PmuSupply_GPIO0));
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register 0x%02x\n",
                      __func__, 0x3E);
    }

    /* Disable Pull-Down */
    Ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             MAX77663_I2C_SPEED, 0x3F, 0x0,
                             1 << (Id - Max77663PmuSupply_GPIO0));
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register 0x%02x\n",
                      __func__, 0x3F);
    }

    Ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             MAX77663_I2C_SPEED, pPmuProps->CfgReg, 0x09, 0x0D);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register 0x%02x\n",
                      __func__, pPmuProps->CfgReg);
    }

    if (DelayUS)
        *DelayUS = pPmuProps->Delay;
    return Ret;
}

static NvBool Max77663GpioRailDisable(Max77663PmuInfoHandle hMaxPmuInfo,
                                      NvU32 Id, NvU32 *DelayUS)
{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvBool Ret;

    Ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             MAX77663_I2C_SPEED, pPmuProps->CfgReg, 0x0, 0x0C);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register 0x%02x\n",
                      __func__, pPmuProps->CfgReg);
    }

    if (DelayUS)
        *DelayUS = pPmuProps->Delay;
    return Ret;
}

static NvBool Max77663GpioGetVoltage(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 *uV)
{
    *uV = hMaxPmuInfo->RailInfo[Id].Caps.requestMilliVolts * 1000;
    return NV_TRUE;
}

static NvBool Max77663GpioSetVoltage(Max77663PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 new_uV, NvU32 *DelayUS)

{
    Max77663PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;

    if (DelayUS)
        *DelayUS = pPmuProps->Delay;
    return NV_TRUE;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule, NvU32 *pI2cInstance,
                                 NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(MAX77663PMUGUID);
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

NvOdmPmuDriverInfoHandle Max77663PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                                               NvOdmPmuRailProps *pOdmRailProps,
                                               int NrOfOdmRailProps)
{
    Max77663PmuInfoHandle hMaxPmuInfo;
    int i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    NvU32 Id;
    NvU8 Val;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    /* Information for all rail is required */
    if (NrOfOdmRailProps != Max77663PmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }
    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hMaxPmuInfo = NvOdmOsAlloc(sizeof(Max77663PmuInfo));
    if (!hMaxPmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hMaxPmuInfo, 0, sizeof(Max77663PmuInfo));

    hMaxPmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hMaxPmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hMaxPmuInfo->DeviceAddr = I2cAddress;

    hMaxPmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hMaxPmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hMaxPmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hMaxPmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 0; i < Max77663PmuSupply_Num; ++i)
    {
        Id = pOdmRailProps[i].VddId;
        if (Id == Max77663PmuSupply_Invalid)
                continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                        "Ignoring this entry\n", __func__, Id);
            continue;
        }

        if (Id != s_Max77663PmuProps[Id].Id)
        {
            NvOdmOsPrintf("%s(): The entries for vdd %d are not in proper sequence, "
                        "got %d, Ignoring this entry\n", __func__, Id, s_Max77663PmuProps[Id].Id);
            continue;
        }

        if (hMaxPmuInfo->RailInfo[Id].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, Id);
            continue;
        }

        hMaxPmuInfo->RailInfo[Id].pPmuProps = &s_Max77663PmuProps[Id];

        NvOdmOsMemcpy(&hMaxPmuInfo->RailInfo[Id].OdmProps,
                &pOdmRailProps[Id], sizeof(NvOdmPmuRailProps));

        Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                               MAX77663_I2C_SPEED,
                               hMaxPmuInfo->RailInfo[Id].pPmuProps->FpsReg,
                               &Val);
		if (!Ret)
			NvOdmOsPrintf("%s() Error in reading the register 0x%02x\n",
                    __func__, hMaxPmuInfo->RailInfo[Id].pPmuProps->FpsReg);
		hMaxPmuInfo->RailInfo[Id].pPmuProps->FpsSrc = (Val & 0xC0) >> 6;

        hMaxPmuInfo->RailInfo[Id].Caps.OdmProtected =
                        hMaxPmuInfo->RailInfo[Id].OdmProps.IsOdmProtected;

        if (hMaxPmuInfo->RailInfo[Id].OdmProps.Min_mV)
             hMaxPmuInfo->RailInfo[Id].Caps.MinMilliVolts =
                        NV_MAX(hMaxPmuInfo->RailInfo[Id].OdmProps.Min_mV,
                           hMaxPmuInfo->RailInfo[Id].pPmuProps->MinUV/1000);
        else
             hMaxPmuInfo->RailInfo[Id].Caps.MinMilliVolts =
                        hMaxPmuInfo->RailInfo[Id].pPmuProps->MinUV/1000;

        if (hMaxPmuInfo->RailInfo[Id].OdmProps.Max_mV)
             hMaxPmuInfo->RailInfo[Id].Caps.MaxMilliVolts =
                        NV_MIN(hMaxPmuInfo->RailInfo[Id].OdmProps.Max_mV,
                           hMaxPmuInfo->RailInfo[Id].pPmuProps->MaxUV/1000);
        else
             hMaxPmuInfo->RailInfo[Id].Caps.MaxMilliVolts =
                           hMaxPmuInfo->RailInfo[Id].pPmuProps->MaxUV/1000;

        hMaxPmuInfo->RailInfo[Id].Caps.StepMilliVolts =
                           hMaxPmuInfo->RailInfo[Id].pPmuProps->StepUV/1000;

        if (hMaxPmuInfo->RailInfo[Id].OdmProps.Setup_mV)
             hMaxPmuInfo->RailInfo[Id].Caps.requestMilliVolts =
                        hMaxPmuInfo->RailInfo[Id].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                        "Ignoring this entry\n", __func__,
                         hMaxPmuInfo->RailInfo[Id].OdmProps.Setup_mV, Id);
            continue;
        }
        hMaxPmuInfo->RailInfo[Id].UserCount  = 0;
        hMaxPmuInfo->RailInfo[Id].IsValid = NV_TRUE;
        hMaxPmuInfo->RailInfo[Id].SystemRailId = pOdmRailProps[Id].SystemRailId;
    }

    return (NvOdmPmuDriverInfoHandle)hMaxPmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hMaxPmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hMaxPmuInfo->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hMaxPmuInfo);
    return NULL;
}

void Max77663PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77663PmuInfoHandle hMaxPmuInfo = (Max77663PmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hMaxPmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hMaxPmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hMaxPmuInfo);
}

void Max77663PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                        NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Max77663PmuInfoHandle hMaxPmuInfo = (Max77663PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(Id < Max77663PmuSupply_Num);
    NvOdmOsMemcpy(pCapabilities, &hMaxPmuInfo->RailInfo[Id].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Max77663PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                             NvU32* pMilliVolts)
{
    Max77663PmuInfoHandle hMaxPmuInfo = (Max77663PmuInfoHandle)hPmuDriver;
    Max77663PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(Id < Max77663PmuSupply_Num);

    pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hMaxPmuInfo, Id)) {
        Status = pPmuProps->pOps->GetRailVoltage(hMaxPmuInfo, Id, &uV);
        if (Status) {
            *pMilliVolts = uV /1000;
        }
    }
    return Status;
}

NvBool Max77663PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                             NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    Max77663PmuInfoHandle hMaxPmuInfo = (Max77663PmuInfoHandle)hPmuDriver;
    Max77663PmuProps *pPmuProps;
    Max77663RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(Id < Max77663PmuSupply_Num);

    pRailInfo = &hMaxPmuInfo->RailInfo[Id];
    pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    if (pSettleMicroSeconds)
        *pSettleMicroSeconds = 0;

    if (pRailInfo->OdmProps.IsOdmProtected == NV_TRUE)
    {
        NvOdmOsPrintf("%s:The voltage rail %d is protected and can not be set.\n",
                    __func__, Id);
        return NV_TRUE;
    }

    if ((MilliVolts != ODM_VOLTAGE_OFF) &&
        ((MilliVolts > pRailInfo->Caps.MaxMilliVolts) ||
         (MilliVolts < pRailInfo->Caps.MinMilliVolts)))
    {
        NvOdmOsPrintf("%s:The required voltage is not supported..vddRail: %d,"
                         " MilliVolts: %d\n", __func__, Id, MilliVolts);
        return NV_TRUE;
    }

    NvOdmOsMutexLock(hMaxPmuInfo->hPmuMutex);
    if (MilliVolts == ODM_VOLTAGE_OFF)
    {
        Status = NV_TRUE;
        if (!pRailInfo->UserCount)
        {
            NvOdmOsPrintf("%s:The imbalance in voltage off No user for rail %d\n",
                        __func__, Id);
            goto End;
        }

        if (pRailInfo->UserCount == 1)
        {
            NvOdmServicesPmuSetSocRailPowerState(hMaxPmuInfo->hOdmPmuService,
                        hMaxPmuInfo->RailInfo[Id].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hMaxPmuInfo, Id,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hMaxPmuInfo->hOdmPmuService,
                        hMaxPmuInfo->RailInfo[Id].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hMaxPmuInfo, Id,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hMaxPmuInfo, Id,
                                                        pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, Id);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hMaxPmuInfo->hPmuMutex);
    return Status;
}

void Max77663PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77663PmuInfoHandle hMaxPmuInfo;

    NV_ASSERT(hPmuDriver);
    hMaxPmuInfo = (Max77663PmuInfoHandle) hPmuDriver;

    NvOdmPmuI2cSetBits(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
            MAX77663_I2C_SPEED, MAX77663_REG_ONOFF_CFG1, ONOFF_SFT_RST_MASK);

    NV_ASSERT(0);
}
