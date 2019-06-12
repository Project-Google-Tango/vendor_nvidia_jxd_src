/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef ODM_PMU_TPS80036_ADC_DRIVER_H
#define ODM_PMU_TPS80036_ADC_DRIVER_H


#include "nvodm_pmu.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "pmu/nvodm_adc_driver.h"

#define CAL_X1 2064
#define CAL_X2 3112
#define CAL_V2 3800
#define CAL_V1 2520

enum {
    slave_0,
    slave_1,
    slave_2
};

typedef struct Tps80036CoreDriverRec *Tps80036CoreDriverHandle;
NvOdmAdcDriverInfoHandle Tps80036AdcDriverOpen(NvOdmPmuDeviceHandle hDevice);

void Tps80036AdcDriverClose(NvOdmAdcDriverInfoHandle hAdcDriverInfo);

NvBool
Tps80036AdcDriverReadData(
    NvOdmAdcDriverInfoHandle hAdcDriverInfo,
    NvU32 AdcChannelId,
    NvU32 *pData);

#if defined(__cplusplus)
}
#endif
#endif