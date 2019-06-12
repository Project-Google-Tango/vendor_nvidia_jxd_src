/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS65914-driver Interface</b>
 *
 * @b Description: Defines the interface for pmu TPS65914 driver.
 *
 */

#ifndef ODM_PMU_TPS65914_PMU_DRIVER_H
#define ODM_PMU_TPS65914_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    TPS65914PmuSupply_Invalid = 0x0,

    /* SMPS12 */
    TPS65914PmuSupply_SMPS12,

    /* SMPS123 */
    TPS65914PmuSupply_SMPS123,

    /* SMPS3 */
    TPS65914PmuSupply_SMPS3,

    /* SMPS45 */
    TPS65914PmuSupply_SMPS45,

    /* SMPS457 */
    TPS65914PmuSupply_SMPS457, //05

     /* SMPS6 */
    TPS65914PmuSupply_SMPS6,

    /* SMPS7 */
    TPS65914PmuSupply_SMPS7,

    /* SMPS8 */
    TPS65914PmuSupply_SMPS8,

    /* SMPS9 */
    TPS65914PmuSupply_SMPS9,

    /* SMPS10 */
    TPS65914PmuSupply_SMPS10, //10

    /* LDO1 */
    TPS65914PmuSupply_LDO1,

    /* LDO2 */
    TPS65914PmuSupply_LDO2,

    /* LDO3 */
    TPS65914PmuSupply_LDO3,

    /* LDO4 */
    TPS65914PmuSupply_LDO4,

    /* LDO5 */
    TPS65914PmuSupply_LDO5, //15

    /* LDO6 */
    TPS65914PmuSupply_LDO6,

    /* LDO7 */
    TPS65914PmuSupply_LDO7,

    /* LDO8 */
    TPS65914PmuSupply_LDO8,

    /* LDO9 */
    TPS65914PmuSupply_LDO9,

    /* LDOLN */
    TPS65914PmuSupply_LDOLN, //20

    /* LDOUSB */
    TPS65914PmuSupply_LDOUSB,

    /* GPIO0 */
    TPS65914PmuSupply_GPIO0,

    /* GPIO1 */
    TPS65914PmuSupply_GPIO1,

    /* GPIO2 */
    TPS65914PmuSupply_GPIO2,

    /* GPIO3 */
    TPS65914PmuSupply_GPIO3, //25

    /* GPIO4 */
    TPS65914PmuSupply_GPIO4,

    /* GPIO5 */
    TPS65914PmuSupply_GPIO5,

    /* GPIO6 */
    TPS65914PmuSupply_GPIO6,

    /* GPIO7 */
    TPS65914PmuSupply_GPIO7, //29

    /* Last entry to MAX */
    TPS65914PmuSupply_Num,

    TPS65914PmuSupply_Force32 = 0x7FFFFFFF
} TPS65914PmuSupply;

typedef struct Tps65914BoardDataRec {
    NvBool Range;
}Tps65914BoardData;

typedef enum {
    KeyDuration_SixSecs = 0x00,
    KeyDuration_EightSecs,
    KeyDuration_TenSecs,
    KeyDuration_TwelveSecs,
    KeyDuration_Force8 = 0x7F
} KeyDurationType;

NvOdmPmuDriverInfoHandle Tps65914PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps);

void Tps65914PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps65914PmuDriverSuspend(void);

void Tps65914PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Tps65914PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Tps65914PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

NvBool Tps65914PmuReadRstReason(NvOdmPmuDriverInfoHandle hPmuDriver,
    NvOdmPmuResetReason *rst_reason);

NvBool Tps65914PmuClearAlarm(NvOdmPmuDriverInfoHandle hDevice);
NvU8 Tps65914PmuPowerOnSource(NvOdmPmuDriverInfoHandle hDevice);
void Tps65914PmuGetDate(NvOdmPmuDriverInfoHandle hPmuDriver, DateTime* time);
void Tps65914PmuSetAlarm (NvOdmPmuDriverInfoHandle hPmuDriver, DateTime time);
void Tps65914SetInterruptMasks(NvOdmPmuDriverInfoHandle hPmuDriver);
void Tps65914DisableInterrupts(NvOdmPmuDriverInfoHandle hPmuDriver);
NvBool Tps65914PmuInterruptHandler(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps65914PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver);

NvBool Tps65914PmuSetLongPressKeyDuration(NvOdmPmuDriverInfoHandle hPmuDriver,
        KeyDurationType duration);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS65914_PMU_DRIVER_H */
