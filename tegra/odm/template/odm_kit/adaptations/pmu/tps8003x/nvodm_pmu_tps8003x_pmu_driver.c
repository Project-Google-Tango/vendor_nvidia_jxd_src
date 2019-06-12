/*
 * Copyright (c) 2011-2012 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS8003X-driver Interface</b>
  *
  * @b Description: Implements the TPS8003x pmu drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_pmu_tps8003x_pmu_driver.h"
#include "tps8003x_core_driver.h"

#define INVALID_VOLTAGE 0xFFFFFFFF

/* TPS80031x registers */

/* SMPS_OFFSET register */
#define TPS80031x_RE0_SMPS_OFFSET   0xE0

/* SMPS_MULT register */
#define TPS80031x_RE3_SMPS_MULT     0xE3

/* Power supply register */
#define TPS80031x_R41_SMPS4         0x41
#define TPS80031x_R42_SMPS4         0x42
#define TPS80031x_R44_SMPS4         0x44

#define TPS80031x_R47_VIO           0x47
#define TPS80031x_R48_VIO           0x48
#define TPS80031x_R49_VIO           0x49

#define TPS80031x_R53_SMPS1         0x53
#define TPS80031x_R54_SMPS1         0x54
#define TPS80031x_R55_SMPS1         0x55

#define TPS80031x_R59_SMPS2         0x59
#define TPS80031x_R5A_SMPS2         0x5A
#define TPS80031x_R5B_SMPS2         0x5B


#define TPS80031x_R65_SMPS3         0x65
#define TPS80031x_R66_SMPS3         0x66
#define TPS80031x_R68_SMPS3         0x68

#define TPS80031x_R81_VANA          0x81
#define TPS80031x_R82_VANA          0x82
#define TPS80031x_R83_VANA          0x83

#define TPS80031x_R85_LDO2          0x85
#define TPS80031x_R86_LDO2          0x86
#define TPS80031x_R87_LDO2          0x87

#define TPS80031x_R89_LDO4          0x89
#define TPS80031x_R8A_LDO4          0x8A
#define TPS80031x_R8B_LDO4          0x8B

#define TPS80031x_R8D_LDO3          0x8D
#define TPS80031x_R8E_LDO3          0x8E
#define TPS80031x_R8F_LDO3          0x8F

#define TPS80031x_R91_LDO6          0x91
#define TPS80031x_R92_LDO6          0x92
#define TPS80031x_R93_LDO6          0x93

#define TPS80031x_R95_LDOLN         0x95
#define TPS80031x_R96_LDOLN         0x96
#define TPS80031x_R97_LDOLN         0x97

#define TPS80031x_R99_LDO5          0x99
#define TPS80031x_R9A_LDO5          0x9A
#define TPS80031x_R9B_LDO5          0x9B

#define TPS80031x_R9D_LDO1          0x9D
#define TPS80031x_R9E_LDO1          0x9E
#define TPS80031x_R9F_LDO1          0x9F

#define TPS80031x_RA1_LDOUSB        0xA1
#define TPS80031x_RA2_LDOUSB        0xA2
#define TPS80031x_RA3_LDOUSB        0xA3

#define TPS80031x_RA5_LDO7          0xA5
#define TPS80031x_RA6_LDO7          0xA6
#define TPS80031x_RA7_LDO7          0xA7

#define TPS80031x_RC3_VRTC          0xC3
#define TPS80031x_RC4_VRTC          0xC4

/* GPIO Control */
#define TPS80031x_RAE_REGEN1        0xAE
#define TPS80031x_RAF_REGEN1        0xAF

#define TPS80031x_RB1_REGEN2        0xB1
#define TPS80031x_RB2_REGEN2        0xB2

#define TPS80031x_RB4_SYSEN         0xB4
#define TPS80031x_RB5_SYSEN         0xB5

/* Flags for DCDC Voltage reading */
#define DCDC_OFFSET_EN              (1 << 0)
#define DCDC_EXTENDED_EN            (1 << 1)

#define SMPS_MULTOFFSET_VIO         (1 << 1)
#define SMPS_MULTOFFSET_SMPS1       (1 << 3)
#define SMPS_MULTOFFSET_SMPS2       (1 << 4)
#define SMPS_MULTOFFSET_SMPS3       (1 << 6)
#define SMPS_MULTOFFSET_SMPS4       (1 << 0)

#define PMC_SMPS_OFFSET_ADD         0xE0
#define PMC_SMPS_MULT_ADD           0xE3

#define STATE_OFF                   0x00
#define STATE_ON                    0x01
#define STATE_MASK                  0x03
#define SMPS_CMD_MASK               0xC0
#define SMPS_VSEL_MASK              0x3F
#define LDO_VSEL_MASK               0x1F

#define TPS80031x_PHOENIX_DEV_ON    0x25

#define EXTENDED_MIN_MV             1852
#define EXTENDED_MAX_MV             3241

typedef struct Tps8003xPmuInfoRec *Tps8003xPmuInfoHandle;

typedef struct Tps8003xPmuOpsRec {
    NvBool (*IsRailEnabled)(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
            NvU32 VddId);
    NvBool (*EnableRail)(Tps8003xPmuInfoHandle hTps8003xPmuInfo, NvU32 VddId,
            NvU32 *DelayUS);
    NvBool (*DisableRail)(Tps8003xPmuInfoHandle hTps8003xPmuInfo, NvU32 VddId,
            NvU32 *DelayUS);
     NvBool (*SetRailVoltage)(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
            NvU32 VddId, NvU32 *uV);
} Tps8003xPmuOps;

typedef struct Tps8003xPmuPropsRec {
    NvU32           Id;
    /* Regulator supply register information.*/
    NvU8            trans_reg;
    NvU8            state_reg;
    NvU8            force_reg;
    NvU8            volt_reg;
    NvU8            volt_id;

    /* Pmu voltage props */
    NvU32            min_uV;
    NvU32            max_uV;
    NvU32            step_uV;
    NvU32            nsteps;

    NvU8            DcDcFlags;

    /* Power rail specific turn-on delay */
    long            delay;
    Tps8003xPmuOps *pOps;

} Tps8003xPmuProps;

typedef struct Tps8003xRailInfoRec {
    Tps8003xPmuSupply   VddId;
    NvU32               SystemRailId;
    int                 UserCount;
    NvBool              IsValid;
    Tps8003xPmuProps    *pPmuProps;
    NvOdmPmuRailProps     OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Tps8003xRailInfo;

typedef struct Tps8003xPmuInfoRec {
    Tps8003xCoreDriverHandle hTps8003xCore;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr[4];
    Tps8003xRailInfo RailInfo[Tps8003xPmuSupply_Num];
} Tps8003xPmuInfo;

static NvBool Tps8003xIsRailEnabled(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId);

static NvBool Tps8003xRailEnable(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Tps8003xRailDisable(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Tps8003xDcDcSetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
               NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps8003xDcDcGetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
               NvU32 RailId, NvU32 *uV);

static NvBool Tps8003xLdoSetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
               NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps8003xLdoGetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
               NvU32 RailId, NvU32 *uV);

static NvBool Tps8003xGpioRailIsEnabled(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId);

static NvBool Tps8003xGpioRailEnable(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Tps8003xGpioRailDisable(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Tps8003xGpioGetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps8003xGpioSetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static Tps8003xPmuOps s_PmuDcDcOps = {
    Tps8003xIsRailEnabled,
    Tps8003xRailEnable,
    Tps8003xRailDisable,
    Tps8003xDcDcSetVoltage,
    Tps8003xDcDcGetVoltage,
};

static Tps8003xPmuOps s_PmuLdoOps = {
    Tps8003xIsRailEnabled,
    Tps8003xRailEnable,
    Tps8003xRailDisable,
    Tps8003xLdoSetVoltage,
    Tps8003xLdoGetVoltage,
};

static Tps8003xPmuOps s_PmuGpioOps = {
    Tps8003xGpioRailIsEnabled,
    Tps8003xGpioRailEnable,
    Tps8003xGpioRailDisable,
    Tps8003xGpioSetVoltage,
    Tps8003xGpioGetVoltage,
};

#define REG_DATA(_id, _trans_reg, _state_reg, _force_reg, _volt_reg, \
            _volt_id, _min_mV, _max_mV, _step_uV, _nsteps, _pOps, _delay)  \
    {       \
            Tps8003xPmuSupply_##_id, _trans_reg, _state_reg, _force_reg, \
            _volt_reg,  _volt_id, \
            _min_mV*1000, _max_mV*1000, _step_uV, _nsteps, 0, _delay, _pOps, \
    }

/* Should be in same sequence as the enums are */
static Tps8003xPmuProps s_Tps8003xPmuProps[] = {
    REG_DATA(Invalid, 0x0, 0, 0, 0, 0, 0, 0, 0, 0,
                &s_PmuDcDcOps, 500),
    REG_DATA(VIO,   0x47, 0x48, 0x49, 0x4A, Tps8003xSlaveId_0, 600, 2100, 10000,
                63, &s_PmuDcDcOps,  500),
    REG_DATA(SMPS1, 0x53, 0x54, 0x55, 0x56, Tps8003xSlaveId_0, 600, 2100, 10000,
                63, &s_PmuDcDcOps,  500),
    REG_DATA(SMPS2, 0x59, 0x5A, 0x5B, 0x5C, Tps8003xSlaveId_0, 600, 2100, 10000,
                63, &s_PmuDcDcOps,  500),
    REG_DATA(SMPS3, 0x65, 0x66, 0x00, 0x68, Tps8003xSlaveId_1, 600, 2100, 10000,
                63, &s_PmuDcDcOps,  500),
    REG_DATA(SMPS4, 0x41, 0x42, 0x00, 0x44, Tps8003xSlaveId_1, 600, 2100, 10000,
                63, &s_PmuDcDcOps,  500),

    REG_DATA(VANA,   0x81, 0x82, 0x00, 0x83, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDO1,   0x9D, 0x9E, 0x00, 0x9F, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDO2,   0x85, 0x86, 0x00, 0x87, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDO3,   0x8D, 0x8E, 0x00, 0x8F, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDO4,   0x89, 0x8A, 0x00, 0x8B, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDO5,   0x99, 0x9A, 0x00, 0x9B, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDO6,   0x91, 0x92, 0x00, 0x93, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDO7,   0xA5, 0xA6, 0x00, 0xA7, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDOLN,  0x95, 0x96, 0x00, 0x97, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),
    REG_DATA(LDOUSB, 0xA1, 0xA2, 0x00, 0xA3, Tps8003xSlaveId_1, 1000, 3300, 100000,
                25, &s_PmuLdoOps, 500),

    REG_DATA(VRTC, 0x0, 0, 0, 0, 0, 0, 0, 0, 0, &s_PmuLdoOps, 500),

    REG_DATA(REGEN1, 0xAE, 0xAF, 0x00, 0x00, Tps8003xSlaveId_1, 0, 3300, 3300000,
                1, &s_PmuGpioOps, 500),
    REG_DATA(REGEN2, 0xB1, 0xB2, 0x00, 0x00, Tps8003xSlaveId_1, 0, 3300, 3300000,
                1, &s_PmuGpioOps, 500),
    REG_DATA(SYSEN, 0xB4, 0xB5, 0x00, 0xA3, Tps8003xSlaveId_1, 0, 3300, 3300000,
                1, &s_PmuGpioOps, 500),
};

#define mask_val(nbits) ((1U << (nbits)) - 1)
#define get_val(val, sbits, nbits) (((val) >> (sbits)) & mask_val(nbits))
#define update_val(val, num, sbits, nbits) \
       ((val & ~(mask_val(nbits) << sbits)) | ((num & mask_val(nbits)) << sbits))

static NvU8 Tps8003xGetSmpsOffset(Tps8003xPmuInfoHandle hTps8003xPmuInfo)
{
    NvBool ret;
    static NvBool is_first_read = NV_TRUE;
    static NvU8 Value = 0;

    if (!is_first_read)
         return Value;

    ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
            Tps8003xSlaveId_1, PMC_SMPS_OFFSET_ADD, &Value);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the register 0x%02x\n",
                __func__, PMC_SMPS_OFFSET_ADD);
        return 0;
    }
    is_first_read =  NV_FALSE;
    return Value;
}

static NvU8 Tps8003xGetSmpsMult(Tps8003xPmuInfoHandle hTps8003xPmuInfo)
{
    NvBool ret;
    static NvBool is_first_read = NV_TRUE;
    static NvU8 Value;

    if (!is_first_read)
         return Value;

    ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
            Tps8003xSlaveId_1, PMC_SMPS_MULT_ADD, &Value);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the register 0x%02x\n",
                __func__, PMC_SMPS_MULT_ADD);
        return 0;
    }
    is_first_read =  NV_FALSE;
    return Value;
}

static void CheckSmpsModeMult(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
    NvU32 RailId)
{
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU32 mult_offset;
    NvU8 Offset;
    NvU8 Mult;

    switch (RailId) {
        case Tps8003xPmuSupply_VIO:
            mult_offset = SMPS_MULTOFFSET_VIO;
            break;

        case Tps8003xPmuSupply_SMPS1:
            mult_offset = SMPS_MULTOFFSET_SMPS1;
            break;

        case Tps8003xPmuSupply_SMPS2:
            mult_offset = SMPS_MULTOFFSET_SMPS2;
            break;

        case Tps8003xPmuSupply_SMPS3:
            mult_offset = SMPS_MULTOFFSET_SMPS3;
            break;

        case Tps8003xPmuSupply_SMPS4:
            mult_offset = SMPS_MULTOFFSET_SMPS4;
            break;

        default:
            pPmuProps->DcDcFlags = 0;
            return;
    }

    Offset = Tps8003xGetSmpsOffset(hTps8003xPmuInfo);
    Mult = Tps8003xGetSmpsMult(hTps8003xPmuInfo);

    pPmuProps->DcDcFlags = (Offset & mult_offset) ? DCDC_OFFSET_EN : 0;
    pPmuProps->DcDcFlags |= (Mult & mult_offset) ? DCDC_EXTENDED_EN : 0;
    return;
}

static NvBool Tps8003xIsRailEnabled(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId)
{
    NvU8 State;
    NvBool ret;
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
                Tps8003xSlaveId_1, pPmuProps->state_reg, &State);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in reading the state register 0x%02x\n",
                        __func__, pPmuProps->state_reg);
        return ret;
    }
    return ((State & STATE_MASK) == STATE_ON);
}

static NvBool Tps8003xRailEnable(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = Tps8003xRegisterUpdate8(hTps8003xPmuInfo->hTps8003xCore,
            Tps8003xSlaveId_1, pPmuProps->state_reg, STATE_ON, STATE_MASK);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in update the state register 0x%02x\n",
                        __func__, pPmuProps->state_reg);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps8003xRailDisable(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = Tps8003xRegisterUpdate8(hTps8003xPmuInfo->hTps8003xCore,
            Tps8003xSlaveId_1, pPmuProps->state_reg, STATE_OFF, STATE_MASK);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in update the state register 0x%02x\n",
                        __func__, pPmuProps->state_reg);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps8003xDcDcSetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
               NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvBool ret;
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU32 vsel = 0;
    NvU8 force = 0;

    switch (pPmuProps->DcDcFlags)
    {
        case 0:
            if (new_uV == 0)
                vsel = 0;
            else if ((new_uV >= 600000) && (new_uV <= 1300000))
                vsel = ((new_uV - 600000 + 12500 - 1) / 12500) + 1;
            else if ((new_uV > 1300000) && (new_uV <= 1350000))
                vsel = 58;
            else if ((new_uV > 1350000) && (new_uV <= 1500000))
                vsel = 59;
            else if ((new_uV > 1500000) && (new_uV <= 1800000))
                vsel = 60;
            else if ((new_uV > 1800000) && (new_uV <= 1900000))
                vsel = 61;
            else if ((new_uV > 1900000) && (new_uV <= 2100000))
                vsel = 62;
            else
            {
                NvOdmOsPrintf("%s() The rail %d voltage is out of range\n",
                                __func__, RailId);
                return NV_FALSE;
            }
            break;

        case DCDC_OFFSET_EN:
            if (new_uV == 0)
                vsel = 0;
            else if ((new_uV >= 700000) && (new_uV <= 1420000))
                vsel = ((new_uV - 700000 + 12500 - 1) / 12500) + 1;
            else if ((new_uV > 1300000) && (new_uV <= 1350000))
                vsel = 58;
            else if ((new_uV > 1350000) && (new_uV <= 1500000))
                vsel = 59;
            else if ((new_uV > 1350000) && (new_uV <= 1800000))
                vsel = 60;
            else if ((new_uV > 1800000) && (new_uV <= 1900000))
                vsel = 61;
            else if ((new_uV > 1900000) && (new_uV <= 2100000))
                vsel = 62;
            else
            {
                NvOdmOsPrintf("%s() The rail %d voltage is out of range\n",
                                __func__, RailId);
                return NV_FALSE;
            }
            break;

        case DCDC_EXTENDED_EN:
            if (new_uV == 0)
                vsel = 0;
            else if ((new_uV >= 1852000) && (new_uV <= 4013600))
                vsel = ((new_uV - 1852000 + 38600 -1) / 38600) + 1;
            else
            {
                NvOdmOsPrintf("%s() The rail %d voltage is out of range\n",
                                __func__, RailId);
                return NV_FALSE;
            }

            break;

        case DCDC_OFFSET_EN|DCDC_EXTENDED_EN:
            if (new_uV == 0)
                vsel = 0;
            else if ((new_uV >= 2161000) && (new_uV <= 4321000))
                vsel = ((new_uV - 2161000 + 38600 -1) / 38600) + 1;
            else
            {
                NvOdmOsPrintf("%s() The rail %d voltage is out of range\n",
                                __func__, RailId);
                return NV_FALSE;
            }
            break;
    }

    if (pPmuProps->force_reg) {
        ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
                pPmuProps->volt_id, pPmuProps->force_reg, &force);
        if (!ret) {
            NvOdmOsPrintf("%s(): Error in reading the force register 0x%02x\n",
                            __func__, pPmuProps->force_reg);
            return ret;
        }

        if (((force >> 6) & 0x3) == 0) {
            ret = Tps8003xRegisterWrite8(hTps8003xPmuInfo->hTps8003xCore,
                            pPmuProps->volt_id, pPmuProps->force_reg, vsel);
            if (!ret)
                NvOdmOsPrintf("%s(): Error in writing the force register 0x%02x\n",
                                __func__, pPmuProps->force_reg);
            goto out;
        }
    }
    ret = Tps8003xRegisterWrite8(hTps8003xPmuInfo->hTps8003xCore,
                    pPmuProps->volt_id, pPmuProps->volt_reg, vsel);
    if (!ret)
        NvOdmOsPrintf("%s(): Error in writing the volt register 0x%02x\n",
                        __func__, pPmuProps->volt_reg);

out:
    return ret;
}

static NvBool Tps8003xDcDcGetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
               NvU32 RailId, NvU32 *uV)
{
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 vsel = 0;
    NvBool ret;
    NvU32 voltage = 0;


    if (pPmuProps->force_reg) {
        ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
                pPmuProps->volt_id, pPmuProps->force_reg, &vsel);
        if (!ret) {
            NvOdmOsPrintf("%s(): Error in reading the force register 0x%02x\n",
                            __func__, pPmuProps->force_reg);
            return ret;
        }
        if ((vsel & SMPS_CMD_MASK) == 0)
            goto decode;
    }
    ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
            pPmuProps->volt_id, pPmuProps->volt_reg, &vsel);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in reading the volt register 0x%02x\n",
                        __func__, pPmuProps->volt_reg);
        return ret;
    }

decode:
    vsel &= SMPS_VSEL_MASK;

    switch (pPmuProps->DcDcFlags) {
    case 0:
        if (vsel == 0)
            voltage = 0;
        else if (vsel < 58)
            voltage = (600000 + (12500 * (vsel - 1)));
        else if (vsel == 58)
            voltage = 1350 * 1000;
        else if (vsel == 59)
            voltage = 1500 * 1000;
        else if (vsel == 60)
            voltage = 1800 * 1000;
        else if (vsel == 61)
            voltage = 1900 * 1000;
        else if (vsel == 62)
            voltage = 2100 * 1000;
        break;

    case DCDC_OFFSET_EN:
        if (vsel == 0)
            voltage = 0;
        else if (vsel < 58)
            voltage = (700000 + (12500 * (vsel - 1)));
        else if (vsel == 58)
            voltage = 1350 * 1000;
        else if (vsel == 59)
            voltage = 1500 * 1000;
        else if (vsel == 60)
            voltage = 1800 * 1000;
        else if (vsel == 61)
            voltage = 1900 * 1000;
        else if (vsel == 62)
            voltage = 2100 * 1000;
        break;

    case DCDC_EXTENDED_EN:
        if (vsel == 0)
            voltage = 0;
        else if (vsel < 58)
            voltage = (1852000 + (38600 * (vsel - 1)));
        else if (vsel == 58)
            voltage = 2084 * 1000;
        else if (vsel == 59)
            voltage = 2315 * 1000;
        else if (vsel == 60)
            voltage = 2778 * 1000;
        else if (vsel == 61)
            voltage = 2932 * 1000;
        else if (vsel == 62)
            voltage = 3241 * 1000;
        break;

    case DCDC_EXTENDED_EN|DCDC_OFFSET_EN:
        if (vsel == 0)
            voltage = 0;
        else if (vsel < 58)
            voltage = (2161000 + (38600 * (vsel - 1)));
        else if (vsel == 58)
            voltage = 4167 * 1000;
        else if (vsel == 59)
            voltage = 2315 * 1000;
        else if (vsel == 60)
            voltage = 2778 * 1000;
        else if (vsel == 61)
            voltage = 2932 * 1000;
        else if (vsel == 62)
            voltage = 3241 * 1000;
        break;
    }

    *uV = voltage;
    return NV_TRUE;
}

static NvBool Tps8003xLdoSetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
               NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 vsel = 0;
    NvBool ret;

    if ((new_uV  < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    /*
     * Use the below formula to calculate vsel
     * mV = 1000mv + 100mv * (vsel - 1)
     */
    vsel = (new_uV - 1000000)/pPmuProps->step_uV + 1;

    ret = Tps8003xRegisterWrite8(hTps8003xPmuInfo->hTps8003xCore,
                    pPmuProps->volt_id, pPmuProps->volt_reg, vsel);
    if (!ret)
        NvOdmOsPrintf("%s(): Error in writing the volt register 0x%02x\n",
                        __func__, pPmuProps->volt_reg);
    return ret;
}

static NvBool Tps8003xLdoGetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
               NvU32 RailId, NvU32 *uV)
{
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 vsel;
    int ret;

    ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
                    pPmuProps->volt_id, pPmuProps->volt_reg, &vsel);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in reading the volt register 0x%02x\n",
                        __func__, pPmuProps->volt_reg);
        return ret;
    }
    vsel &= LDO_VSEL_MASK;

    /*
     * Use the below formula to calculate vsel
     * mV = 1000mv + 100mv * (vsel - 1)
     */
    *uV = (1000 + (100 * (vsel - 1))) * 1000;
    return NV_TRUE;
}

static NvBool Tps8003xGpioRailIsEnabled(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId)
{
    NvBool ret;
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 Val;

    ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
            Tps8003xSlaveId_1, pPmuProps->trans_reg, &Val);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in reading the trans register 0x%02x\n",
                        __func__, pPmuProps->trans_reg);
        return ret;
    }
    if ((Val & 0x3) == 0x1)
        return NV_TRUE;
    return NV_FALSE;
}

static NvBool Tps8003xGpioRailEnable(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = Tps8003xRegisterUpdate8(hTps8003xPmuInfo->hTps8003xCore,
            Tps8003xSlaveId_1, pPmuProps->trans_reg, 0x1, 0x3);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in updating the trans register 0x%02x\n",
                        __func__, pPmuProps->trans_reg);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps8003xGpioRailDisable(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = Tps8003xRegisterUpdate8(hTps8003xPmuInfo->hTps8003xCore,
            Tps8003xSlaveId_1, pPmuProps->trans_reg, 0x0, 0x3);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in updating the trans register 0x%02x\n",
                        __func__, pPmuProps->trans_reg);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps8003xGpioGetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
                NvU32 RailId, NvU32 *uV)
{
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;
    NvBool ret;
    NvU8 Val;

    ret = Tps8003xRegisterRead8(hTps8003xPmuInfo->hTps8003xCore,
            Tps8003xSlaveId_1, pPmuProps->trans_reg, &Val);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in reading the trans register 0x%02x\n",
                        __func__, pPmuProps->trans_reg);
        return ret;
    }

    if ((Val & 0x1) == 0x1)
        *uV = hTps8003xPmuInfo->RailInfo[RailId].Caps.requestMilliVolts * 1000;
    else
        *uV = NVODM_VOLTAGE_OFF;
    return NV_TRUE;
}

static NvBool Tps8003xGpioSetVoltage(Tps8003xPmuInfoHandle hTps8003xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)

{
    Tps8003xPmuProps *pPmuProps = hTps8003xPmuInfo->RailInfo[RailId].pPmuProps;
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return NV_TRUE;
}

NvOdmPmuDriverInfoHandle Tps8003xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, int NrOfOdmRailProps)
{
    Tps8003xPmuInfoHandle hTps8003xPmuInfo;
    int i;
    NvU32 VddId;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    /* Information for all rail is required */
    if (NrOfOdmRailProps != Tps8003xPmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }

    hTps8003xPmuInfo = NvOdmOsAlloc(sizeof(Tps8003xPmuInfo));
    if (!hTps8003xPmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hTps8003xPmuInfo, 0, sizeof(Tps8003xPmuInfo));

    hTps8003xPmuInfo->hTps8003xCore = Tps8003xCoreDriverOpen(hDevice);
    if (!hTps8003xPmuInfo->hTps8003xCore)
    {
        NvOdmOsPrintf("%s: Error Open core driver.\n", __func__);
        goto CoreDriverOpenFailed;
    }

    hTps8003xPmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hTps8003xPmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hTps8003xPmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hTps8003xPmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 1; i < Tps8003xPmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        if (VddId == Tps8003xPmuSupply_Invalid)
                continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (VddId != s_Tps8003xPmuProps[VddId].Id)
        {
            NvOdmOsPrintf("%s(): The entries for rail %d are not in proper sequence, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }
        if (hTps8003xPmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries %d are duplicated, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }
        hTps8003xPmuInfo->RailInfo[VddId].pPmuProps = &s_Tps8003xPmuProps[VddId];

        NvOdmOsMemcpy(&hTps8003xPmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));
        hTps8003xPmuInfo->RailInfo[VddId].Caps.OdmProtected =
                        hTps8003xPmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        CheckSmpsModeMult(hTps8003xPmuInfo, VddId);

        if (hTps8003xPmuInfo->RailInfo[VddId].pPmuProps->DcDcFlags == DCDC_EXTENDED_EN)
        {
            hTps8003xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts = EXTENDED_MIN_MV;
            hTps8003xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts = EXTENDED_MAX_MV;
        }
        else
        {
            if (hTps8003xPmuInfo->RailInfo[VddId].OdmProps.Min_mV)
                 hTps8003xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                       NV_MAX(hTps8003xPmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                        hTps8003xPmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
            else
                hTps8003xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                   hTps8003xPmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

            if (hTps8003xPmuInfo->RailInfo[VddId].OdmProps.Max_mV)
                 hTps8003xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                       NV_MIN(hTps8003xPmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                        hTps8003xPmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
            else
                hTps8003xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                    hTps8003xPmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;
        }
        hTps8003xPmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
                           hTps8003xPmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hTps8003xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
             hTps8003xPmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                        hTps8003xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                        "Ignoring this entry\n", __func__,
                         hTps8003xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                         VddId);
            continue;
        }
        hTps8003xPmuInfo->RailInfo[VddId].UserCount  = 0;
        hTps8003xPmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hTps8003xPmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;

        hTps8003xPmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                        hTps8003xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV;

    }

    return (NvOdmPmuDriverInfoHandle)hTps8003xPmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hTps8003xPmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    Tps8003xCoreDriverClose(hTps8003xPmuInfo->hTps8003xCore);
CoreDriverOpenFailed:
    NvOdmOsFree(hTps8003xPmuInfo);
    return NULL;
}

void Tps8003xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps8003xPmuInfoHandle hTps8003xPmuInfo = (Tps8003xPmuInfoHandle)hPmuDriver;

    NvOdmOsMutexDestroy(hTps8003xPmuInfo->hPmuMutex);
    NvOdmServicesPmuClose(hTps8003xPmuInfo->hOdmPmuService);
    Tps8003xCoreDriverClose(hTps8003xPmuInfo->hTps8003xCore);
    NvOdmOsFree(hTps8003xPmuInfo);
}

void Tps8003xPmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Tps8003xPmuInfoHandle hTps8003xPmuInfo =  (Tps8003xPmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps8003xPmuSupply_Num);
    if (!hTps8003xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }
    if (VddId == Tps8003xPmuSupply_VRTC)
    {
        NvOdmOsPrintf("%s(): Voltage control is not possible in this rail %d",
                    __func__, VddId);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    NvOdmOsMemcpy(pCapabilities, &hTps8003xPmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Tps8003xPmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Tps8003xPmuInfoHandle hTps8003xPmuInfo =  (Tps8003xPmuInfoHandle)hPmuDriver;
    Tps8003xPmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps8003xPmuSupply_Num);

    if (!hTps8003xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    if (VddId == Tps8003xPmuSupply_VRTC)
    {
        NvOdmOsPrintf("%s(): Voltage control is not possible in this rail %d",
                    __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hTps8003xPmuInfo->RailInfo[VddId].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hTps8003xPmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hTps8003xPmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV /1000;
        }
    }
    return Status;
}

NvBool Tps8003xPmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Tps8003xPmuInfoHandle hTps8003xPmuInfo =  (Tps8003xPmuInfoHandle)hPmuDriver;
    Tps8003xPmuProps *pPmuProps;
    Tps8003xRailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    PmuDebugPrintf("%s(): The rail %d and the voltage %u\n", __func__, VddId, MilliVolts);
    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps8003xPmuSupply_Num);
    if (!hTps8003xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    if (VddId == Tps8003xPmuSupply_VRTC)
    {
        NvOdmOsPrintf("%s(): Voltage control is not possible in this rail %d",
                    __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hTps8003xPmuInfo->RailInfo[VddId];
    pPmuProps = hTps8003xPmuInfo->RailInfo[VddId].pPmuProps;
    if (pSettleMicroSeconds)
        *pSettleMicroSeconds = 0;

    if (pRailInfo->OdmProps.IsOdmProtected == NV_TRUE)
    {
        PmuDebugPrintf("%s:The rail %d voltage is protected and can not be set.\n",
                    __func__, VddId);
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

    NvOdmOsMutexLock(hTps8003xPmuInfo->hPmuMutex);
    if (MilliVolts == ODM_VOLTAGE_OFF)
    {
        Status = NV_TRUE;
        if (!pRailInfo->UserCount)
        {
            NvOdmOsPrintf("%s:The imbalance in voltage off No user for rail %d\n",
                        __func__, VddId);
            goto End;
        }

        if (pRailInfo->UserCount == 1)
        {
            NvOdmServicesPmuSetSocRailPowerState(hTps8003xPmuInfo->hOdmPmuService,
                        hTps8003xPmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hTps8003xPmuInfo, VddId,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }

    if (!pRailInfo->UserCount)
    {
        NvOdmServicesPmuSetSocRailPowerState(hTps8003xPmuInfo->hOdmPmuService,
                        hTps8003xPmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hTps8003xPmuInfo, VddId,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hTps8003xPmuInfo, VddId,
                                                        pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, VddId);
            goto End;
        }
    }
    pRailInfo->UserCount++;

End:
    NvOdmOsMutexUnlock(hTps8003xPmuInfo->hPmuMutex);
    return Status;
}

NvBool Tps8003xPmuPowerOff(
    NvOdmPmuDriverInfoHandle hPmuDriver)
{
    NvBool ret = NV_FALSE;

    Tps8003xCoreDriverHandle hTps8003xCore = (Tps8003xCoreDriverHandle)hPmuDriver;

    if (hTps8003xCore)
        ret = Tps8003xRegisterWrite8(hTps8003xCore, Tps8003xSlaveId_1, TPS80031x_PHOENIX_DEV_ON, 0x01);

    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error in shutting down\n", __func__);
    }
    return ret;
}
