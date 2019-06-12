/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: TPS80036 pmu driver</b>
 *
 * @b Description: Implements the TPS80036 pmu drivers.
 *
 */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "tps80036/nvodm_pmu_tps80036_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "tps80036_reg.h"
#include "nvos.h"

#define TPS80036_PMUGUID NV_ODM_GUID('t','p','s','8','0','0','3','6')

#define TPS80036_I2C_SPEED 100

#define PROC_BOARD_E1670    0x0686
#define PROC_BOARD_E1740    0x06CC

#define PALMAS_SMPS12_VOLTAGE_RANGE   0x80
#define PALMAS_CTRL_MODE_ACTIVE_MASK  0x03
#define PALMAS_LDO_VOLTAGE_RANGE      0xC0
#define LDO_CTRL_MODE_ON              0x01
#define TPS80036_SW_REVISION          0xB7
#define TPS80036_WDT_ADDRESS          0xA5
#define TPS80036_FUNC_INTERRUPT       0xB2
#define TPS80036_INT2_STATUS          0x15
#define TPS80036_INT2_MASK            0x16
#define TPS80036_INT_CTRL             0x24
#define WDT_ENABLE                    0x10

typedef struct Tps80036PmuInfoRec *Tps80036PmuInfoHandle;

typedef struct Tps80036PmuOpsRec {
    NvBool (*IsRailEnabled)(Tps80036PmuInfoHandle hTps80036PmuInfo, NvU32 VddId);
    NvBool (*EnableRail)(Tps80036PmuInfoHandle hTps80036PmuInfo, NvU32 VddId, NvU32 *DelayUS);
    NvBool (*DisableRail)(Tps80036PmuInfoHandle hTps80036PmuInfo, NvU32 VddId, NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(Tps80036PmuInfoHandle hTps80036PmuInfo, NvU32 VddId, NvU32 new_uV,
            NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Tps80036PmuInfoHandle hTps80036PmuInfo, NvU32 VddId, NvU32 *uV);
} Tps80036PmuOps;

typedef struct Tps80036PmuPropsRec {
    NvU32           Id;

    /* Regulator register information.*/
    NvU8            ctrl_reg;
    NvU8            volt_reg;

    /* Pmu voltage props */
    NvU32           min_uV;
    NvU32           max_uV;
    NvU32           step_uV;
    NvU32           nsteps;

   /* multiplier set in vsel */
    NvU32           mult;

    /* Power rail specific turn-on delay */
    NvU32           delay;
    Tps80036PmuOps *pOps;
} Tps80036PmuProps;

typedef struct Tps80036RailInfoRec {
    TPS80036PmuSupply   VddId;
    NvU32               SystemRailId;
    NvU32               UserCount;
    NvBool              IsValid;
    Tps80036PmuProps    *pPmuProps;
    NvOdmPmuRailProps   OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Tps80036RailInfo;

typedef struct Tps80036PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    NvU8  SwOTPVersion;
    Tps80036RailInfo RailInfo[TPS80036PmuSupply_Num];
} Tps80036PmuInfo;

static NvBool Tps80036IsRailEnabled(Tps80036PmuInfoHandle hTps80036PmuInfo,
               NvU32 VddId);

static NvBool Tps80036RailEnable(Tps80036PmuInfoHandle hTps80036PmuInfo,
               NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps80036RailDisable(Tps80036PmuInfoHandle hTps80036PmuInfo,
               NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps80036LDOGetVoltage(Tps80036PmuInfoHandle hTps80036PmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps80036LDOSetVoltage(Tps80036PmuInfoHandle hTps80036PmuInfo,
                NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps80036SMPSGetVoltage(Tps80036PmuInfoHandle hTps80036PmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps80036SMPSSetVoltage(Tps80036PmuInfoHandle hTps80036PmuInfo,
                NvU32 RailId, NvU32 uV, NvU32 *DelayUS);

extern void NvMlUsbfSetVbusOverrideT1xx(NvBool vbusStatus);

static Tps80036PmuOps s_PmuLDOOps = {
    Tps80036IsRailEnabled,
    Tps80036RailEnable,
    Tps80036RailDisable,
    Tps80036LDOSetVoltage,
    Tps80036LDOGetVoltage,
};

static Tps80036PmuOps s_PmuSMPSOps = {
    Tps80036IsRailEnabled,
    Tps80036RailEnable,
    Tps80036RailDisable,
    Tps80036SMPSSetVoltage,
    Tps80036SMPSGetVoltage,
};

#define REG_DATA(_id, ctrl_reg, volt_reg,  \
                 _min_mV, _max_mV, _pOps, _delay)  \
{       \
    TPS80036PmuSupply_##_id, \
    TPS80036_##ctrl_reg,     \
    TPS80036_##volt_reg,     \
    _min_mV*1000,            \
    _max_mV*1000,            \
    0,                       \
    0,                       \
    0,                       \
    _delay,                  \
    _pOps                   \
}

/* Should be in same sequence as the enums are */
static Tps80036PmuProps s_Tps80036PmuProps[] = {
    REG_DATA(Invalid, RFF_INVALID, RFF_INVALID,
                    0, 0, NULL, 0),
    REG_DATA(SMPS1_2,  SMPS1_2_CTRL, SMPS1_2_VOLTAGE,
                 500, 3300, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS3,  SMPS3_CTRL, SMPS3_VOLTAGE,
                 500, 3300, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS6,  SMPS6_CTRL, SMPS6_VOLTAGE,
                 500, 3300, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS7,  SMPS7_CTRL, SMPS7_VOLTAGE,
                 500, 3300, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS8,  SMPS8_CTRL, SMPS8_VOLTAGE,
                 500, 3300, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS9,  SMPS9_CTRL, SMPS9_VOLTAGE,
                 500, 3300, &s_PmuSMPSOps, 0),
    REG_DATA(LDO1,  LDO1_CTRL, LDO1_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO2,  LDO2_CTRL, LDO2_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO3,  LDO3_CTRL, LDO3_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO4,  LDO4_CTRL, LDO4_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO5,  LDO5_CTRL, LDO5_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO6,  LDO6_CTRL, LDO6_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO7,  LDO7_CTRL, LDO7_VOLTAGE,
                 900, 5000, &s_PmuLDOOps, 0),
    REG_DATA(LDO8,  LDO8_CTRL, LDO8_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO9,  LDO9_CTRL, LDO9_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO10,  LDO10_CTRL, LDO10_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO11,  LDO11_CTRL, LDO11_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO12,  LDO12_CTRL, LDO12_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO13,  LDO13_CTRL, LDO13_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDO14,  LDO14_CTRL, LDO14_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDOLN, LDOLN_CTRL, LDOLN_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0),
    REG_DATA(LDOUSB,LDOUSB_CTRL, LDOUSB_VOLTAGE,
                 900, 3300, &s_PmuLDOOps, 0)
};

static NvBool Tps80036IsRailEnabled(Tps80036PmuInfoHandle hTps80036PmuInfo,
                NvU32 RailId)
{
    NvU8 Control;
    NvBool ret;
    Tps80036PmuProps *pPmuProps;

    if (!hTps80036PmuInfo  || !hTps80036PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hTps80036PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->ctrl_reg, &Control);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the control register\n", __func__);
        return ret;
    }

    return (((Control & 0x01) == 1) || (Control & 0x03));
}

static NvBool Tps80036RailEnable(Tps80036PmuInfoHandle hTps80036PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps80036PmuProps *pPmuProps;

    if (!hTps80036PmuInfo  || !hTps80036PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hTps80036PmuInfo->RailInfo[RailId].pPmuProps;

    if (RailId == TPS80036PmuSupply_LDO12)
         ret = NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->ctrl_reg, 0x41);
    else
         ret = NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->ctrl_reg, 0x01);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the control register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps80036RailDisable(Tps80036PmuInfoHandle hTps80036PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps80036PmuProps *pPmuProps;

    if (!hTps80036PmuInfo  || !hTps80036PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hTps80036PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->ctrl_reg, 0x00);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the control register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool Tps80036SMPSGetVoltage(Tps80036PmuInfoHandle hTps80036PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    Tps80036PmuProps *pPmuProps;
    NvU8 vsel;
    NvBool ret;
    NvU8 range = 0;

    if (!hTps80036PmuInfo  || !hTps80036PmuInfo->RailInfo[RailId].pPmuProps || !uV)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hTps80036PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->volt_reg, &vsel);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the volt register\n", __func__);
        return ret;
    }
    vsel &= ~PALMAS_SMPS12_VOLTAGE_RANGE;

    ret = NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->volt_reg, &range);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the volt register\n", __func__);
        return ret;
    }
    range &= PALMAS_SMPS12_VOLTAGE_RANGE;

    if(range)
    {
       if(vsel < 7)
          *uV = 1000000;
       else if( vsel < 121)
          *uV = 1000000 + (vsel - 6) * 20000;
       else
          *uV = 3300000;
    }
    else
    {
         if(vsel < 7)
            *uV = 500000;
         else if( vsel < 121)
            *uV = 500000 + (vsel - 6) * 10000;
         else
            *uV = 1650000;
    }
    return ret;
}

static NvBool Tps80036SMPSSetVoltage(Tps80036PmuInfoHandle hTps80036PmuInfo,
                NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel = 0;
    NvBool ret = NV_FALSE;
    Tps80036PmuProps *pPmuProps;
    NvU8 range = 0;

    if (!hTps80036PmuInfo  || !hTps80036PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hTps80036PmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    ret = NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->volt_reg, &range);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the volt register\n", __func__);
        return ret;
    }
    range &= PALMAS_SMPS12_VOLTAGE_RANGE;

    if (range)
    {
        if (new_uV < 1000000)
        {
            NvOdmOsPrintf("%s() This value %d cannot be set as volt_reg range field is 1\n",
                           __func__, new_uV);
            return NV_FALSE;
         }

        if (new_uV == 1000000)
            vsel = 1;
        else if (new_uV < 3300000)
            vsel = (new_uV - 1000000) / 20000 + 6;
        else
            vsel = 121;
    }
    else
    {
        if (new_uV > 1650000)
        {
            NvOdmOsPrintf("%s() This value %d cannot be set as volt_reg range field is 0\n",
                           __func__, new_uV);
            return NV_FALSE;
        }

        if (new_uV <= 500000)
            vsel = 1;
        else if (new_uV < 1650000)
            vsel = (new_uV - 500000) / 10000 + 6;
        else
            vsel = 121;
    }

    ret = NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->ctrl_reg, 0x01);

    if (!ret)
    {
        NvOdmOsPrintf("%s() Error in writing the ctrl register\n", __func__);
        return ret;
    }

    ret = NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->volt_reg, vsel);

    if (!ret)
    {
        NvOdmOsPrintf("%s() Error in writing the volt register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool Tps80036LDOGetVoltage(Tps80036PmuInfoHandle hTps80036PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    Tps80036PmuProps *pPmuProps;
    NvU8 vsel;
    NvBool ret;

    if (!hTps80036PmuInfo  || !hTps80036PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hTps80036PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->volt_reg, &vsel);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the volt register\n", __func__);
        return ret;
    }

    vsel &= ~PALMAS_LDO_VOLTAGE_RANGE;

    if (vsel <= 0x30)
        *uV = 850000 + vsel * 50000;
    else
        *uV = pPmuProps->max_uV;

    return ret;
}

static NvBool Tps80036LDOSetVoltage(Tps80036PmuInfoHandle hTps80036PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel;
    NvBool ret;
    Tps80036PmuProps *pPmuProps;

    if (!hTps80036PmuInfo  || !hTps80036PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hTps80036PmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    if (new_uV < 3300000)
        vsel = (new_uV - 850000) / 50000;
    else
        vsel = 0x31;

    ret = NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->volt_reg, vsel);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the volt register\n", __func__);
        return ret;
    }

    // For LDO12, bypass should be enabled
    if(RailId == TPS80036PmuSupply_LDO12)
        NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->ctrl_reg, 0x40);
    // Set mode to active in ctrl register
    ret = NvOdmPmuI2cSetBits(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
            TPS80036_I2C_SPEED, pPmuProps->ctrl_reg, 0x01);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the ctrl register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;

    if (!pI2cModule  || !pI2cInstance || !pI2cAddress)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pConnectivity = NvOdmPeripheralGetGuid(TPS80036_PMUGUID);
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

static int Bcd2Binary(int x)
{
    int binary = 0;
    binary = ((x & 0xF0 )>>4 ) * 10 + (x & 0x0F);
    return (binary);
}

static int Binary2Bcd (int x)
{
    int bcd = 0;

    bcd = x/10 << 4;
    bcd += x%10;

    return bcd;
}

#define NUM_TIME_REGS 6

void Tps80036PmuGetDate(NvOdmPmuDriverInfoHandle hPmuDriver, DateTime* time)
{
    NvU32 count = 0;
    NvU8 rtc_data[NUM_TIME_REGS] = {0};
    Tps80036PmuInfoHandle hTps80036PmuInfo;

    hTps80036PmuInfo = (Tps80036PmuInfoHandle)hPmuDriver;
    NV_ASSERT(hPmuDriver);
    NV_ASSERT(time);

    //RTC and Enable GET_TIME for shadow registers
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0, TPS80036_I2C_SPEED, 0x10, 0x41);

    for ( ; count < NUM_TIME_REGS; count++)
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0, TPS80036_I2C_SPEED, count, &rtc_data[count]);

    //Stop RTC and Disable GET_TIME for shadow registers
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,TPS80036_I2C_SPEED, 0x10, 0x01);

    //Convert data from BCD to Binary
    time->Second = Bcd2Binary(rtc_data[0]);
    time->Minute = Bcd2Binary(rtc_data[1]);
    time->Hour   = Bcd2Binary(rtc_data[2] );
    time->Day = Bcd2Binary(rtc_data[3]);
    time->Month = Bcd2Binary(rtc_data[4]);
    time->Year = Bcd2Binary(rtc_data[5]);

}

void Tps80036PmuSetAlarm (NvOdmPmuDriverInfoHandle hPmuDriver, DateTime time)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;
    NvOdmPmuI2cHandle hBqI2c = NULL;
    NvU8 RtcStatus = 0;

    NV_ASSERT(hPmuDriver);
    if (!hPmuDriver)
        return;

    hTps80036PmuInfo = (Tps80036PmuInfoHandle)hPmuDriver;

    //SWOFF_COLDRST.SW_RST = 0
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,
                       TPS80036_I2C_SPEED, 0xb0, 0xfb);
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,
                         TPS80036_I2C_SPEED, 0x08, Binary2Bcd(time.Second));
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS80036_I2C_SPEED, 0x09, Binary2Bcd(time.Minute));
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS80036_I2C_SPEED, 0x0a, Binary2Bcd (time.Hour));
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS80036_I2C_SPEED, 0x0b, Binary2Bcd (time.Day));
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS80036_I2C_SPEED, 0x0c, Binary2Bcd (time.Month));
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS80036_I2C_SPEED, 0x0d, Binary2Bcd(time.Year));

    /* Clear ALARM If already set */
    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xb0,
                       TPS80036_I2C_SPEED, TPS80036_RTC_STATUS_REG, &RtcStatus);

    if (RtcStatus & (1 << TPS80036_SHIFT_RTC_STATUS_REG_IT_ALARM))
    {
        //Clear status
        NvOdmPmuI2cSetBits(hTps80036PmuInfo->hOdmPmuI2c, 0xb0,TPS80036_I2C_SPEED,
            TPS80036_RTC_STATUS_REG, (1 << TPS80036_SHIFT_RTC_STATUS_REG_IT_ALARM));
        NvOdmOsPrintf("[%u] TPS80036::RTC ALARM DETECTED AND CLEARED\n",NvOdmOsGetTimeMS());
    }

    /* Enable IT_ALARM INTERRUPT */
    NvOdmPmuI2cSetBits(hTps80036PmuInfo->hOdmPmuI2c, 0xb0, TPS80036_I2C_SPEED,
          TPS80036_RTC_INTERRUPTS_REG, (1 << TPS80036_SHIFT_RTC_INTERRUPTS_REG_IT_ALARM));
    NvOdmOsPrintf("[%u] TPS80036::RTC ALARM ENABLED\n",NvOdmOsGetTimeMS());

    /* Enable RTC Alarm interrupt to AP by unmasking RTC_ALARM in INT2 */
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xb2,
                       TPS80036_I2C_SPEED, TPS80036_INT2_MASK, 0xFE);
    NvOdmOsPrintf("[%u] TPS80036::RTC INTERRUPT ENABLED\n",NvOdmOsGetTimeMS());

    // Clear BQ WDT and HiZ bits
    hBqI2c = NvOdmPmuI2cOpen(NvOdmIoModule_I2c, 0x04);
    NvOdmPmuI2cWrite8(hBqI2c, 0xD4, TPS80036_I2C_SPEED, 0x01, 0x5B);
    NvOdmPmuI2cWrite8(hBqI2c, 0xD4, TPS80036_I2C_SPEED, 0x01, 0x5B);

    NvOdmPmuI2cUpdate8(hBqI2c,0xD4, TPS80036_I2C_SPEED, 0x00, 0, (1 << 7));
    NvOdmOsPrintf("[%u] TPS80036::BQ WDT and HiZ CLEARED\n\n",NvOdmOsGetTimeMS());
}

static NvU32 GetMaximumCurrentFromOtp(Tps80036PmuInfoHandle hTps80036PmuInfo,
                   NvU32 VddId)
{
   switch(hTps80036PmuInfo->SwOTPVersion) {
       case 0xA2:
           if(VddId == TPS80036PmuSupply_SMPS1_2)
               return 8000;
       case 0xA1:
       default:
           if(VddId == TPS80036PmuSupply_SMPS1_2)
               return 8000;
           if(VddId == TPS80036PmuSupply_SMPS6)
               return 4000;
    }
    return 4000;
}

NvOdmPmuDriverInfoHandle Tps80036PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;
    NvU32 i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    NvU32 VddId;
    NvU8 VbusLineState = 0;
    NvOdmBoardInfo ProcBoard;
    NvBool status;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));

    /* Get processor Board */
    status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
    if(!status)
    {
        NvOdmOsPrintf("%s: Error Get BoardInfo Failed.\n", __func__);
        return NULL;
    }
    if (!hDevice || !pOdmRailProps)
        return NULL;

    /* Information for all rail is required */
    if (NrOfOdmRailProps != TPS80036PmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }

    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hTps80036PmuInfo = NvOdmOsAlloc(sizeof(Tps80036PmuInfo));
    if (!hTps80036PmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hTps80036PmuInfo, 0, sizeof(Tps80036PmuInfo));

    hTps80036PmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hTps80036PmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: NvOdmPmuI2cOpen failed.\n", __func__);
        goto I2cOpenFailed;
    }
    hTps80036PmuInfo->DeviceAddr = I2cAddress;

    // Read OTP version
    Ret = NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, I2cAddress,
                TPS80036_I2C_SPEED, TPS80036_SW_REVISION,
                &hTps80036PmuInfo->SwOTPVersion);
    if (!Ret)
    {
        NV_ASSERT(0);
        NvOdmOsPrintf("%s() Error in reading the REG_CID4 register\n"
            "Defaulting to 0x02", __func__);
    }

    hTps80036PmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hTps80036PmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hTps80036PmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hTps80036PmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 1; i < TPS80036PmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;

        if (VddId == TPS80036PmuSupply_Invalid)
            continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                    "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (VddId != s_Tps80036PmuProps[VddId].Id)
        {
            NvOdmOsPrintf("%s(): The entries are not in proper sequence, "
                    "Ignoring this entry\n", __func__);
            continue;
        }

        if (hTps80036PmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                    "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hTps80036PmuInfo->RailInfo[VddId].pPmuProps = &s_Tps80036PmuProps[VddId];

        NvOdmOsMemcpy(&hTps80036PmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hTps80036PmuInfo->RailInfo[VddId].Caps.OdmProtected =
            hTps80036PmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        /* Maximum current */
        if ((VddId == TPS80036PmuSupply_SMPS1_2) || (VddId == TPS80036PmuSupply_SMPS6))
            hTps80036PmuInfo->RailInfo[VddId].Caps.MaxCurrentMilliAmp =
                                      GetMaximumCurrentFromOtp(hTps80036PmuInfo, VddId);

        /* min voltage */
        if (hTps80036PmuInfo->RailInfo[VddId].OdmProps.Min_mV)
            hTps80036PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                NV_MAX(hTps80036PmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                        hTps80036PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
            hTps80036PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                hTps80036PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        /* max voltage */
        if (hTps80036PmuInfo->RailInfo[VddId].OdmProps.Max_mV)
            hTps80036PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                NV_MIN(hTps80036PmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                        hTps80036PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
            hTps80036PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                hTps80036PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        /* step voltage */
        hTps80036PmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
            hTps80036PmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hTps80036PmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
            hTps80036PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                hTps80036PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                    "Ignoring this entry\n", __func__,
                    hTps80036PmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                    VddId);
            continue;
        }
        hTps80036PmuInfo->RailInfo[VddId].UserCount  = 0;
        hTps80036PmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hTps80036PmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;

        hTps80036PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
            hTps80036PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;

    }

    Tps80036PmuSetInterruptMasks(hTps80036PmuInfo);
    if(ProcBoard.BoardID == PROC_BOARD_E1670 ||
            ProcBoard.BoardID == PROC_BOARD_E1740)
    {
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT3_LINE_STATE, &VbusLineState);
        NvMlUsbfSetVbusOverrideT1xx(VbusLineState & 0x80);
    }
    return (NvOdmPmuDriverInfoHandle)hTps80036PmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hTps80036PmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hTps80036PmuInfo->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hTps80036PmuInfo);
    return NULL;
}

void Tps80036PmuSetInterruptMasks(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;
    NvU8 reg = 0;

    NV_ASSERT(hPmuDriver);

    if (!hPmuDriver)
        return;

    hTps80036PmuInfo = (Tps80036PmuInfoHandle)hPmuDriver;

    //Enable Clear on Read
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT_CTRL, 0x05);

    // Clear All Interrupts
    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT1_STATUS, &reg);

    while(reg)
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
                TPS80036_I2C_SPEED, TPS80036_INT1_STATUS, &reg);


    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT2_STATUS, &reg);

    while(reg)
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
                TPS80036_I2C_SPEED, TPS80036_INT2_STATUS, &reg);

    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT3_STATUS, &reg);

    while(reg)
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
                TPS80036_I2C_SPEED, TPS80036_INT3_STATUS, &reg);

    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT4_STATUS, &reg);

    while(reg)
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
                TPS80036_I2C_SPEED, TPS80036_INT4_STATUS, &reg);

    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT5_STATUS, &reg);

    while(reg)
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
                TPS80036_I2C_SPEED, TPS80036_INT5_STATUS, &reg);

    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT6_STATUS, &reg);

    while(reg)
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
                TPS80036_I2C_SPEED, TPS80036_INT6_STATUS, &reg);

    //Enable CHARGER WDT interrupt
    NvOdmPmuI2cClrBits(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT6_MASK, (1<<5));

    //Enable USB interrupts
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT3_MASK, 0x7f);

    //Enable RTC interrupts and WDT interrupt
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
                       TPS80036_I2C_SPEED, TPS80036_INT2_MASK, 0xFA);

    //Enable RTC ALARM
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, 0xB0,
                       TPS80036_I2C_SPEED, TPS80036_RTC_INTERRUPTS_REG, 0x08);

}

void Tps80036PmuInterruptHandler(NvOdmPmuDeviceHandle hPmuDriver)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;
    NvU8 reg = 0;
    NvU8 rtcstatus;
    NvBool status;
    NvOdmBoardInfo ProcBoard;
    NvOdmOsMemset(&ProcBoard, 0, sizeof(NvOdmBoardInfo));


    hTps80036PmuInfo = (Tps80036PmuInfoHandle) hPmuDriver;

    if(!hTps80036PmuInfo)
        return;

    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
        TPS80036_I2C_SPEED, TPS80036_INT1_STATUS, &reg);
    NvOsWaitUS(10);

    // Check for charger interrupt
    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT6_STATUS, &reg);
    if (reg & 0x20) {
        NvOdmOsPrintf("\nTPS80036:Charger WDT interrupt detected\n");
        NvOdmOsPrintf("Continue Charging at 500Ma\n");
    }

    // Check for USB INTERRUPT
    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT3_STATUS, &reg);

    if (reg & 0x80)
    {
        /* Get processor Board */
        status = NvOdmPeripheralGetBoardInfo(0, &ProcBoard);
        if(!status)
        {
            NvOdmOsPrintf("Tps80036PmuInterruptHandler: Unable to read BoardInfo \n");
            return;
        }
        if (ProcBoard.BoardID != PROC_BOARD_E1670 ||
                ProcBoard.BoardID != PROC_BOARD_E1740)
            return;
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
        TPS80036_I2C_SPEED, TPS80036_INT3_LINE_STATE, &reg);
        if (reg & 0x80)
        {
            NvOdmOsPrintf("TPS80036:Change in VBUS status:: CONNECTED \n");
            NvMlUsbfSetVbusOverrideT1xx(NV_TRUE);
        }
        else
        {
            NvOdmOsPrintf("TPS80036:Change in VBUS status:: DISCONNECTED \n");
            NvMlUsbfSetVbusOverrideT1xx(NV_FALSE);
        }
    }

    // Check for RTC Alarm interrupt or WDT Interrupt
    NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xB2,
            TPS80036_I2C_SPEED, TPS80036_INT2_STATUS, &reg);

    if(reg & 0x04)
    {
         NvOdmOsPrintf("[%u] RESET WDT\n", NvOdmOsGetTimeMS());
    }

    if (reg & (1<<TPS80036_SHIFT_INT2_STATUS_RTC_ALARM))
    {
        NvOdmPmuI2cRead8(hTps80036PmuInfo->hOdmPmuI2c, 0xb0,
                       TPS80036_I2C_SPEED, TPS80036_RTC_STATUS_REG, &rtcstatus);
        if(rtcstatus & (1 << TPS80036_SHIFT_RTC_STATUS_REG_IT_ALARM))
        {
            DateTime time;
            // Clear Status by writing 1 to the corresponding bit
            NvOdmPmuI2cSetBits(hTps80036PmuInfo->hOdmPmuI2c, 0xb0, TPS80036_I2C_SPEED,
                 TPS80036_RTC_STATUS_REG, 1 << TPS80036_SHIFT_RTC_STATUS_REG_IT_ALARM);

            NvOdmOsPrintf("TPS80036:RTC ALARM DETECTED\n");
            Tps80036PmuGetDate(hTps80036PmuInfo, &time);
            Tps80036PmuSetAlarm (hTps80036PmuInfo, NvAppAddAlarmSecs(time, 30));
        }
    }

}

void Tps80036PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;

    NV_ASSERT(hPmuDriver);

    if (!hPmuDriver)
        return;

    hTps80036PmuInfo = (Tps80036PmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hTps80036PmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hTps80036PmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hTps80036PmuInfo);
    hTps80036PmuInfo = NULL;
}

void Tps80036PmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS80036PmuSupply_Num);
    NV_ASSERT(pCapabilities);

    if (!hPmuDriver || !(VddId < TPS80036PmuSupply_Num) || !pCapabilities)
        return;

    hTps80036PmuInfo = (Tps80036PmuInfoHandle)hPmuDriver;

    if (!hTps80036PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d\n", __func__, VddId);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    NvOdmOsMemcpy(pCapabilities, &hTps80036PmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Tps80036PmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;
    Tps80036PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;
    hTps80036PmuInfo =  (Tps80036PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS80036PmuSupply_Num);
    NV_ASSERT(pMilliVolts);

    if (!hPmuDriver || !(VddId < TPS80036PmuSupply_Num) || !pMilliVolts)
        return NV_FALSE;

    hTps80036PmuInfo = (Tps80036PmuInfoHandle)hPmuDriver;

    if (!hTps80036PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provided the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hTps80036PmuInfo->RailInfo[VddId].pPmuProps;

    *pMilliVolts = 0;

    if (pPmuProps->pOps->IsRailEnabled(hTps80036PmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hTps80036PmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV/1000;
        }
    }
    return Status;
}

NvBool Tps80036PmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;
    Tps80036PmuProps *pPmuProps;
    Tps80036RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;
    hTps80036PmuInfo =  (Tps80036PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS80036PmuSupply_Num);
    NV_ASSERT(pSettleMicroSeconds);

    if (!hPmuDriver || !(VddId < TPS80036PmuSupply_Num) || !pSettleMicroSeconds)
        return NV_FALSE;

    if (!hTps80036PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hTps80036PmuInfo->RailInfo[VddId];
    pPmuProps = hTps80036PmuInfo->RailInfo[VddId].pPmuProps;
    if (pSettleMicroSeconds)
        *pSettleMicroSeconds = 0;

    if (pRailInfo->OdmProps.IsOdmProtected == NV_TRUE)
    {
        NvOdmOsPrintf("%s:The voltage is protected and can not be set.\n", __func__);
        return NV_TRUE;
    }
    if ((MilliVolts != ODM_VOLTAGE_OFF) &&
        ((MilliVolts > pRailInfo->Caps.MaxMilliVolts) ||
         (MilliVolts < pRailInfo->Caps.MinMilliVolts)))
    {
        NvOdmOsPrintf("%s:The required voltage is not supported..vddRail: %d,"
                         " MilliVolts: %d\n", __func__, VddId, MilliVolts);
        return NV_TRUE;
    }

    NvOdmOsMutexLock(hTps80036PmuInfo->hPmuMutex);
    if (MilliVolts == ODM_VOLTAGE_OFF)
    {
        if (!pRailInfo->UserCount)
        {
            NvOdmOsPrintf("%s:The imbalance in voltage off No user for rail %d\n",
                        __func__, VddId);
            Status = NV_TRUE;
            goto End;
        }

        if (pRailInfo->UserCount == 1)
        {
            NvOdmServicesPmuSetSocRailPowerState(hTps80036PmuInfo->hOdmPmuService,
                        hTps80036PmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hTps80036PmuInfo, VddId,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hTps80036PmuInfo->hOdmPmuService,
                        hTps80036PmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hTps80036PmuInfo, VddId,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hTps80036PmuInfo, VddId,
                                                        pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hTps80036PmuInfo->hPmuMutex);
    return Status;
}

void Tps80036PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps80036PmuInfoHandle hTps80036PmuInfo;

    NV_ASSERT(hPmuDriver);
    if (!hPmuDriver)
        return;

    hTps80036PmuInfo = (Tps80036PmuInfoHandle)hPmuDriver;
    NvOdmPmuI2cWrite8(hTps80036PmuInfo->hOdmPmuI2c, hTps80036PmuInfo->DeviceAddr,
                       TPS80036_I2C_SPEED, TPS80036_DEVCNTRL, TPS80036_POWER_OFF);
    //shouldn't reach here
    NV_ASSERT(0);
}

void Tps80036PmuEnableWdt(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps80036PmuInfoHandle hTpsPmuInfo;

    NV_ASSERT(hPmuDriver);
    hTpsPmuInfo = (Tps80036PmuInfoHandle) hPmuDriver;

    NvOdmPmuI2cSetBits(hTpsPmuInfo->hOdmPmuI2c, hTpsPmuInfo->DeviceAddr,
                       TPS80036_I2C_SPEED, TPS80036_WDT_ADDRESS, WDT_ENABLE);
}

void Tps80036PmuDisableWdt(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps80036PmuInfoHandle hTpsPmuInfo;

    NV_ASSERT(hPmuDriver);
    hTpsPmuInfo = (Tps80036PmuInfoHandle) hPmuDriver;

    NvOdmPmuI2cClrBits(hTpsPmuInfo->hOdmPmuI2c, hTpsPmuInfo->DeviceAddr,
                       TPS80036_I2C_SPEED, TPS80036_WDT_ADDRESS, WDT_ENABLE);
}
