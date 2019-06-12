/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */



#include "nvodm_pmu_tps6586x_interrupt.h"
#include "nvodm_pmu_tps6586x_i2c.h"
#include "nvodm_pmu_tps6586x_supply_info_table.h"
#include "nvodm_services.h"
#include "nvodm_pmu_tps6586x_batterycharger.h"

#define TPS6586X_INT_BATT_INST 0x01
#define TPS6586X_INT_PACK_COLD_DET 0x02
#define TPS6586X_INT_PACK_HOT_DET 0x04

#define TPS6586X_INT_USB_DETECTION 0x04
#define TPS6586X_INT_AC_DETECTION  0x08
#define TPS6586X_INT_LOWSYS_DETECTION 0x40

NvBool Tps6586xSetupInterrupt(NvOdmPmuDeviceHandle  hDevice,
                              TPS6586xStatus *pmuStatus)
{
    NvBool status = NV_FALSE;
    NvU32   data = 0;

    NV_ASSERT(hDevice);
    NV_ASSERT(pmuStatus);

    /* Init Pmu Status */
    pmuStatus->lowBatt    = NV_FALSE;
    pmuStatus->highTemp   = NV_FALSE;

    if (!Tps6586xBatteryChargerMainChgPresent(hDevice,&status))
        return NV_FALSE;
    pmuStatus->mChgPresent = status;

    /* Set up Interrupt Mask */
    /* Mask1 */
    data = 0xFF;
    if(! Tps6586xI2cWrite8(hDevice, TPS6586x_RB0_INT_MASK1, data ))
        return NV_FALSE;

    /* Mask2 */
    data = 0xFF;
    if(! Tps6586xI2cWrite8(hDevice, TPS6586x_RB1_INT_MASK2, data ))
        return NV_FALSE;

    /* Mask3: Battery detction, etc */
    data = 0;
    data = (NvU32)~(TPS6586X_INT_BATT_INST|TPS6586X_INT_PACK_COLD_DET|TPS6586X_INT_PACK_HOT_DET);
    if(! Tps6586xI2cWrite8(hDevice, TPS6586x_RB2_INT_MASK3, data ))
        return NV_FALSE;

    /* Mask4 */
    data = 0xFF;
    if(! Tps6586xI2cWrite8(hDevice, TPS6586x_RB3_INT_MASK4, data ))
        return NV_FALSE;

    /* Mask5: USB Detection; AC Detection; Low System detection;  */
    data = 0;
    data = (NvU32) ~(TPS6586X_INT_USB_DETECTION|TPS6586X_INT_AC_DETECTION|TPS6586X_INT_LOWSYS_DETECTION);
    if(! Tps6586xI2cWrite8(hDevice, TPS6586x_RB4_INT_MASK5, data ))
        return NV_FALSE;

    return NV_TRUE;
}

void Tps6586xInterruptHandler_int(NvOdmPmuDeviceHandle  hDevice,
                                  TPS6586xStatus *pmuStatus)
{
    NvU32   data = 0;

    /* INT_ACK1 */
    if (!Tps6586xI2cRead8(hDevice, TPS6586x_RB5_INT_ACK1, &data))
    {
        return;
    }
    pmuStatus->powerGood = (data & 0xFF);


    /* INT_ACK2 */
    if (!Tps6586xI2cRead8(hDevice, TPS6586x_RB6_INT_ACK2, &data))
    {
        return;
    }
    pmuStatus->powerGood |= ((data & 0xFF)<<8);

    /* INT_ACK3 */
    /* LOW SYS */
    if (!Tps6586xI2cRead8(hDevice, TPS6586x_RB7_INT_ACK3, &data))
    {
        return;
    }
    if (data != 0)
    {
        if (data&0x40)
        {
            pmuStatus->highTemp = NV_TRUE;
        }
        if (data&0xc0)
        {
            pmuStatus->mChgPresent = NV_TRUE;
        }
    }

    /* INT_ACK4 */
    /* CHG TEMP */
    if (!Tps6586xI2cRead8(hDevice, TPS6586x_RB8_INT_ACK4, &data))
    {
        return;
    }
    if (data != 0)
    {
        if (data&0x02)
        {
            pmuStatus->lowBatt = NV_TRUE;
        }
    }
}
