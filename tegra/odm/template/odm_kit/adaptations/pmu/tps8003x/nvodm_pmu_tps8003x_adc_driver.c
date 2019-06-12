/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS8003X-driver Interface</b>
  *
  * @b Description: Implements the TPS8003x adc drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_pmu_tps8003x_adc_driver.h"
#include "tps8003x_core_driver.h"


#define Tps80031x_TOGGLE1 0x90
#define Tps80031x_GPSELECT_ISB 0x35
#define Tps80031x_CTRL_P1 0x36
#define Tps80031x_GPADC_CTRL 0x2E
#define Tps80031x_MISC1 0xE4
#define Tps80031x_GPCH0_LSB 0x3B
#define Tps80031x_GPCHO_MSB 0x3C

NvOdmAdcDriverInfoHandle
Tps8003xAdcDriverOpen(
    NvOdmPmuDeviceHandle hDevice)
{
    NvBool ret;
    Tps8003xCoreDriverHandle hTps8003xCore = NULL;

    hTps8003xCore = Tps8003xCoreDriverOpen(hDevice);
    if (!hTps8003xCore)
    {
        NvOdmOsPrintf("%s: Error Open core driver.\n", __func__);
        return NULL;
    }
    //Enable the GPADC
    ret = Tps8003xRegisterWrite8( hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_TOGGLE1, 0x22);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in enabling the GPADC\n", __func__);
        return NULL;
    }
    return hTps8003xCore;
}

void Tps8003xAdcDriverClose(NvOdmAdcDriverInfoHandle hAdcDriverInfo)
{
    Tps8003xCoreDriverClose(hAdcDriverInfo);
}

NvBool Tps8003xAdcDriverSetProperty(NvOdmAdcDriverInfoHandle hAdcDriverInfo,
    NvU32 AdcChannelId, OdmAdcProps *pAdcProps)
{
    return NV_FALSE;
}

static NvU32 Tps8003xCalibrate(NvU32 LSB, NvU32 MSB)
{
    NvU32 voltage = 0;
    voltage = ((MSB << 8) | LSB) & (0x0fff);
    voltage = ((voltage * 1000) / 4096) * 5;
    return voltage;
}

NvBool
Tps8003xAdcDriverReadData(
    NvOdmAdcDriverInfoHandle hAdcDriverInfo,
    NvU32 AdcChannelId,
    NvU32 *pData)
{
    NvU8 RegMSB = 0;
    NvU8 RegLSB = 0;
    NvBool ret;
    Tps8003xCoreDriverHandle hTps8003xCore =  (struct Tps8003xCoreDriverRec*) hAdcDriverInfo;

    //Select  Channel   from ADC for System voltage
    ret = Tps8003xRegisterWrite8( hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_GPSELECT_ISB, AdcChannelId);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in writing the GPSELECT_ISB register\n", __func__);
        return NV_FALSE;
    }

    if (AdcChannelId == Tps8003x_SysSupply)
    {
        //Enable the Resistor divider for Channel
        ret = Tps8003xRegisterWrite8( hTps8003xCore, Tps8003xSlaveId_1, Tps80031x_MISC1, 0x02);
        if (!ret)
        {
            NvOdmOsPrintf("%s(): Error in writing the MISC1 register\n", __func__);
            return NV_FALSE;
        }
        //Select the Divider
        ret = Tps8003xRegisterWrite8( hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_GPADC_CTRL, 0xef);
        if (!ret)
        {
            NvOdmOsPrintf("%s(): Error in writing the GPADC_CTRL register\n", __func__);
            return NV_FALSE;
        }
    }
    else
    {
        // NOTE: Not implemented completely as it is only for bootloader.
        return NV_FALSE;
    }

    //Start the conversion
    ret = Tps8003xRegisterWrite8( hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_CTRL_P1, 0x08);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in writing the CTRL_P1 register\n", __func__);
        return NV_FALSE;
    }

    //Read the voltage values
    ret = Tps8003xRegisterRead8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_GPCH0_LSB, &RegLSB);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in reading the GPCH0_LSB register\n", __func__);
        return NV_FALSE;
    }

    ret = Tps8003xRegisterRead8(hTps8003xCore, Tps8003xSlaveId_2, Tps80031x_GPCHO_MSB, &RegMSB);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in reading the GPCHO_MSB register\n", __func__);
        return NV_FALSE;
    }

    *pData = Tps8003xCalibrate(RegLSB, RegMSB);
    return NV_TRUE;
}
