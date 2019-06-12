/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "max8907b.h"
#include "max8907b_interrupt.h"
#include "max8907b_i2c.h"
#include "max8907b_reg.h"
#include "max8907b_batterycharger.h"

NvBool 
Max8907bSetupInterrupt(
    NvOdmPmuDeviceHandle hDevice,
    Max8907bStatus *pmuStatus)
{
    NvBool status = NV_FALSE;
    NvU8   data   = 0;    

    NV_ASSERT(hDevice);
    NV_ASSERT(pmuStatus);
    
    /* Init Pmu Status */
    pmuStatus->lowBatt       = NV_FALSE;
    pmuStatus->highTemp      = NV_FALSE;

    if (!Max8907bBatteryChargerMainBatt(hDevice, &status))
        return NV_FALSE;
    pmuStatus->mChgPresent = status;

    /* Set up Interrupt Mask */

    // CHG_IRQ1
    data = 0;
    if (!Max8907bI2cWrite8(hDevice, MAX8907B_CHG_IRQ1, data))
        return NV_FALSE;

    // CHG_IRQ2
    data = MAX8907B_CHG_IRQ2_CHG_DONE_MASK      |
           MAX8907B_CHG_IRQ2_MBATTLOW_F_SHIFT   |
           MAX8907B_CHG_IRQ2_THM_OK_F_MASK      |
           MAX8907B_CHG_IRQ2_THM_OK_R_MASK      ;
    if (!Max8907bI2cWrite8(hDevice, MAX8907B_CHG_IRQ2, data))
        return NV_FALSE;

    // ON_OFF_IRQ1
    data = 0;
    if (!Max8907bI2cWrite8(hDevice, MAX8907B_ON_OFF_IRQ1, data))
        return NV_FALSE;

    // ON_OFF_IRQ2
    data = 0;
    if (!Max8907bI2cWrite8(hDevice, MAX8907B_ON_OFF_IRQ2, data))
        return NV_FALSE;

    // RTC_IRQ
    data = 0;
    if (!Max8907bI2cWrite8(hDevice, MAX8907B_RTC_IRQ, data))
        return NV_FALSE;

    return NV_TRUE;
}

void 
Max8907bInterruptHandler_int(
    NvOdmPmuDeviceHandle hDevice,
    Max8907bStatus *pmuStatus)
{
    NvU8 data = 0;

    NV_ASSERT(hDevice);
    NV_ASSERT(pmuStatus);

    /* Check which interrupt... */

    // CHG_IRQ1
    if (!Max8907bI2cRead8(hDevice, MAX8907B_CHG_IRQ1, &data))
    {
        return;
    }

    // CHG_IRQ2
    if (!Max8907bI2cRead8(hDevice, MAX8907B_CHG_IRQ2, &data))
    {
        return;
    }
    if (data)
    {
        if (data & MAX8907B_CHG_IRQ2_CHG_DONE_MASK)
        {
            pmuStatus->mChgPresent = NV_TRUE;
            pmuStatus->batFull = NV_TRUE;
        }
        if (data & MAX8907B_CHG_IRQ2_MBATTLOW_F_SHIFT)
        {
            pmuStatus->batFull = NV_FALSE;
        }
        if (data & MAX8907B_CHG_IRQ2_THM_OK_F_MASK)
        {
            pmuStatus->highTemp = NV_TRUE;
        }
        if (data & MAX8907B_CHG_IRQ2_THM_OK_R_MASK)
        {
            pmuStatus->highTemp = NV_FALSE;
        }
    }

    // ON_OFF_IRQ1
    if (!Max8907bI2cRead8(hDevice, MAX8907B_ON_OFF_IRQ1, &data))
    {
        return;
    }

    // ON_OFF_IRQ2
    if (!Max8907bI2cRead8(hDevice, MAX8907B_ON_OFF_IRQ2, &data))
    {
        return;
    }

    // RTC_IRQ
    if (!Max8907bI2cRead8(hDevice, MAX8907B_RTC_IRQ, &data))
    {
        return;
    }

    return;
}

