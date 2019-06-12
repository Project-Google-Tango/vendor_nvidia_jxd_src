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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS65090 driver Interface</b>
  *
  * @b Description: Defines the interface for pmu TPS65090 driver.
  *
  */

#ifndef ODM_PMU_TPS65090_PMU_DRIVER_H
#define ODM_PMU_TPS65090_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "tps65090_reg.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    TPS65090PmuSupply_Invalid = 0x0,

    /* VIO */
    TPS65090PmuSupply_DCDC1,

    /* VDD1 */
    TPS65090PmuSupply_DCDC2,

    /* VDD2 */
    TPS65090PmuSupply_DCDC3,

    /* FET1 */
    TPS65090PmuSupply_FET1,

    /* FET2 */
    TPS65090PmuSupply_FET2,

    /* FET3 */
    TPS65090PmuSupply_FET3,

    /* FET4 */
    TPS65090PmuSupply_FET4,

    /* FET5 */
    TPS65090PmuSupply_FET5,

    /* FET6 */
    TPS65090PmuSupply_FET6,

    /* FET7 */
    TPS65090PmuSupply_FET7,

    /* Last entry to MAX */
    TPS65090PmuSupply_Num,

    TPS65090PmuSupply_Force32 = 0x7FFFFFFF
} TPS65090PmuSupply;

typedef struct Tps65090PmuInfoRec *Tps65090PmuInfoHandle;

typedef struct Tps65090PmuOpsRec {
    NvBool (*IsRailEnabled)(Tps65090PmuInfoHandle hTps65090PmuInfo, NvU32 VddId);
    NvBool (*EnableRail)(Tps65090PmuInfoHandle hTps65090PmuInfo, NvU32 VddId, NvU32 *DelayUs);
    NvBool (*DisableRail)(Tps65090PmuInfoHandle hTps65090PmuInfo, NvU32 VddId, NvU32 *DelayUs);
} Tps65090PmuOps;

typedef struct Tps65090PmuPropsRec {
    NvU32           Id;
    NvU8            en_reg; /* Enable Register */
    NvU8            en_bit; /* Enable bit */
    NvU32           op_val; /* Operating Voltage */
    Tps65090PmuOps *pOps;
} Tps65090PmuProps;

typedef struct Tps65090RailInfoRec {
    TPS65090PmuSupply   VddId;
    NvU32               SystemRailId;
    NvBool              IsValid;
    Tps65090PmuProps    *pPmuProps;
    NvOdmPmuRailProps   OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Tps65090RailInfo;

typedef struct Tps65090PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    Tps65090RailInfo RailInfo[TPS65090PmuSupply_Num];
} Tps65090PmuInfo;

NvOdmPmuDriverInfoHandle Tps65090PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, NvS32 NrOfOdmRailProps);

void Tps65090PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps65090PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Tps65090PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Tps65090PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS65090_PMU_DRIVER_H */
