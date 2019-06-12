/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_T30RM_CLOCKS_LIMITS_PRIVATE_H
#define INCLUDED_T30RM_CLOCKS_LIMITS_PRIVATE_H

#include "nvrm_clocks_limits_private.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

// TODO: T30 process corners and mapping to fuses

// Number of characterized T30 process corners
#define NV_T30_PROCESS_CORNERS (4)

// T30 RAM timing controls low voltage threshold and setting
#define NV_T30_SVOP_LOW_VOLTAGE (1010)
#define NV_T30_SVOP_LOW_SETTING \
        (NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_DP,  0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_PDP, 0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_REG, 0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_SP,  0))

#define NV_T30_SVOP_HIGH_SETTING \
        (NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_DP,  0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_PDP, 0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_REG, 0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_SP,  0))

/**
 * The tables below provide clock limits for T30 modules for different core
 * voltages for each process corner. Note that EMC and MC are not included,
 * as memory scaling has board dependencies, and EMC scaling configurations
 * are retrieved by ODM query NvOdmQuerySdramControllerConfigGet(). The ODM
 * data is adjuasted based on process corner using EMC DQSIB offsets.
 */

#define NV_T30SS_SHMOO_VOLTAGES           950,   1000,    1100,    1200,  1250
#define NV_T30SS_SCALED_CLK_LIMITS \
    { NV_DEVID_I2S      , 0,       1,  {    1,      1,      1,   26000,   26000 } }, \
    { NV_DEVID_SPDIF    , 0,       1,  {    1,      1,      1,  100000,  100000 } }, \
    { NV_DEVID_I2C      , 0,       1,  {    1,      1,      1,   26000,   26000 } }, \
    { NV_DEVID_DVC      , 0,       1,  {    1,      1,      1,   26000,   26000 } }, \
    { NV_DEVID_OWR      , 0,       1,  {    1,      1,      1,   26000,   26000 } }, \
    { NV_DEVID_PWFM     , 0,       1,  {    1,      1,      1,  216000,  216000 } }, \
    { NV_DEVID_XIO      , 0,       1,  {    1,      1,      1,  150000,  150000 } }, \
    { NV_DEVID_TWC      , 0,       1,  {    1,      1,      1,  150000,  150000 } }, \
    { NV_DEVID_HSMMC    , 0,       1,  {    1,      1,      1,  208000,  208000 } }, \
    { NV_DEVID_VFIR     , 0,       1,  {    1,      1,      1,   72000,   72000 } }, \
    { NV_DEVID_UART     , 0,       1,  {    1,      1,      1,   72000,   72000 } }, \
    { NV_DEVID_NANDCTRL , 0,       1,  {    1,      1,      1,  240000,  240000 } }, \
    { NV_DEVID_SNOR     , 0,       1,  {    1,      1,      1,  127000,  102000 } }, \
    { NV_DEVID_EIDE     , 0,       1,  {    1,      1,      1,  100000,  100000 } }, \
    { NV_DEVID_MIPI_HS  , 0,       1,  {    1,      1,      1,   40000,   40000 } }, \
    { NV_DEVID_SDIO     , 0,       1,  {    1,      1,      1,  208000,  208000 } }, \
    { NV_DEVID_SPI      , 0,       1,  {    1,      1,      1,   40000,   25000 } }, \
    { NV_DEVID_SLINK    , 0,       1,  {    1,      1,      1,  160000,  160000 } }, \
    { NV_DEVID_USB      , 0,       1,  {    1,      1,      1,  480000,  480000 } }, \
    { NV_DEVID_PCIE     , 0,       1,  {    1,      1,      1,  250000,  250000 } }, \
    { NV_DEVID_HOST1X   , 0,       1,  {    1,      1,      1,  166000,  242000 } }, \
    { NV_DEVID_EPP      , 0,       1,  {    1,      1,      1,  300000,  484000 } }, \
    { NV_DEVID_GR2D     , 0,       1,  {    1,      1,      1,  356000,  484000 } }, \
    { NV_DEVID_GR3D     , 0,       1,  {    1,      1,      1,  356000,  484000 } }, \
    { NV_DEVID_MPE      , 0,       1,  {    1,      1,      1,  333000,  484000 } }, \
    { NV_DEVID_VI       , 0,       1,  {    1,      1,      1,  150000,  150000 } }, \
    { NV_DEVID_DISPLAY  , 0,       1,  {    1,      1,      1,  270000,  270000 } }, \
    { NV_DEVID_DSI      , 0,       1,  {    1,      1,      1,  500000,  500000 } }, \
    { NV_DEVID_TVO      , 0,       1,  {    1,      1,      1,  300000,  300000 } }, \
    { NV_DEVID_HDMI     , 0,       1,  {    1,      1,      1,  148500,  148500 } }, \
    { NV_DEVID_HDA      , 0,       1,  {    1,      1,      1,  108000,  108000 } }, \
    { NV_DEVID_ACTMON   , 0,       1,  {    1,      1,      1,  216000,  216000 } }, \
    { NV_DEVID_MSELECT  , 0,       1,  {    1,      1,      1,  216000,  216000 } }, \
    { NV_DEVID_TSENSOR  , 0,       1,  {    1,      1,      1,  216000,  216000 } }, \
    { NV_DEVID_SE       , 0,       1,  {    1,      1,      1,  216000,  650000 } }, \
    { NV_DEVID_SATA     , 0,       1,  {    1,      1,      1,  216000,  216000 } }, \
    { NV_DEVID_AVP,0, NVRM_BUS_MIN_KHZ,{    1,      1,      1,  275000,  378000 } }, \
    { NV_DEVID_VDE      , 0,       1,  {    1,      1,      1,  300000,  484000 } }, \
    { NVRM_DEVID_CLK_SRC, 0,       1,  {    1,      1,      1, 1067000, 1067000 } }

#define NV_T30ST_SHMOO_VOLTAGES     NV_T30SS_SHMOO_VOLTAGES
#define NV_T30ST_SCALED_CLK_LIMITS  NV_T30SS_SCALED_CLK_LIMITS

#define NV_T30FT_SHMOO_VOLTAGES     NV_T30SS_SHMOO_VOLTAGES
#define NV_T30FT_SCALED_CLK_LIMITS  NV_T30SS_SCALED_CLK_LIMITS

#define NV_T30FF_SHMOO_VOLTAGES     NV_T30SS_SHMOO_VOLTAGES
#define NV_T30FF_SCALED_CLK_LIMITS  NV_T30SS_SCALED_CLK_LIMITS

/**
 * The tables below provides clock limits for T30 CPU for different CPU
 * voltages, for all process corners
 */

#define NV_T30SS_CPU_VOLTAGES                750,    800,    850,     900,     950,    1025
#define NV_T30SS_SCALED_CPU_CLK_LIMITS \
    { NV_DEVID_CPU, 0, NVRM_BUS_MIN_KHZ, {     1,      1,      1,       1,       1, 1000000} }

#define NV_T30ST_CPU_VOLTAGES               NV_T30SS_CPU_VOLTAGES
#define NV_T30ST_SCALED_CPU_CLK_LIMITS      NV_T30SS_SCALED_CPU_CLK_LIMITS

#define NV_T30FT_CPU_VOLTAGES               NV_T30SS_CPU_VOLTAGES
#define NV_T30FT_SCALED_CPU_CLK_LIMITS      NV_T30SS_SCALED_CPU_CLK_LIMITS

#define NV_T30FF_CPU_VOLTAGES               NV_T30SS_CPU_VOLTAGES
#define NV_T30FF_SCALED_CPU_CLK_LIMITS      NV_T30SS_SCALED_CPU_CLK_LIMITS

/**
 * For Automotive
**/

#define NV_T30SS_AUTOMOTIVE_CPU_VOLTAGES     750,    800,    850,     912,     1025
#define NV_T30SS_AUTOMOTIVE_SCALED_CPU_CLK_LIMITS \
    { NV_DEVID_CPU, 0, NVRM_BUS_MIN_KHZ, {     1,      1, 600000,  900000,     1000000} }

#define NV_T30ST_AUTOMOTIVE_CPU_VOLTAGES               NV_T30SS_AUTOMOTIVE_CPU_VOLTAGES
#define NV_T30ST_AUTOMOTIVE_SCALED_CPU_CLK_LIMITS      NV_T30SS_AUTOMOTIVE_SCALED_CPU_CLK_LIMITS

#define NV_T30FT_AUTOMOTIVE_CPU_VOLTAGES               NV_T30SS_AUTOMOTIVE_CPU_VOLTAGES
#define NV_T30FT_AUTOMOTIVE_SCALED_CPU_CLK_LIMITS      NV_T30SS_AUTOMOTIVE_SCALED_CPU_CLK_LIMITS

#define NV_T30FF_AUTOMOTIVE_CPU_VOLTAGES               NV_T30SS_AUTOMOTIVE_CPU_VOLTAGES
#define NV_T30FF_AUTOMOTIVE_SCALED_CPU_CLK_LIMITS      NV_T30SS_AUTOMOTIVE_SCALED_CPU_CLK_LIMITS

/**
 * T30 EMC DQSIB offsets in order of process calibration settings
 */

#define NV_T30_DQSIB_OFFSETS    0, 0, 0, 0

/**
 * T30 OSC Doubler tap delays for each supported frequency in order
 * of process calibration settings
 */
#define NV_T30_OSC_DOUBLER_CONFIGURATIONS \
/*     Osc kHz  SS  ST  FT  FF   */ \
    {  12000, {  7,  7,  7,  7 } }, \
    {  13000, {  7,  7,  7,  7 } },

#define NV_T30_AUTOMOTIVE_SKUS {0x11, 0x12, 0x90, 0x91, 0x92, 0x93, 0xb0, 0xb1}

/**
 * This table provides maximum limits for selected T30 modules depended on
 * T30 SKUs. Table entries are ordered by SKU number.
 */
//    CPU      Avp     Vde     MC      EMC2x    3D      DispA   DispB    mV   Cpu mV   SKU
#define NV_T30_SKUED_LIMITS \
    {{1000000, 240000, 240000, 400000,  800000, 300000, 190000, 190000, 1200, 1025 },  0}, /* Dev (T30 Unrestricted) */ \
    {{900000, 378000, 484000, 400000,  800000, 484000, 190000, 190000, 1250,  912 },  0x11}, \
    {{900000, 378000, 484000, 400000,  800000, 484000, 190000, 190000, 1250,  912 },  0x12}, /* T30XC-ES1-A-A3 */ \
    {{900000, 378000, 484000, 400000,  800000, 484000, 190000, 190000, 1250,  912 },  0x90}, /* T30AQS-Ax */ \
    {{900000, 378000, 484000, 400000,  800000, 484000, 190000, 190000, 1250,  912 },  0x91}, /* T30AGS-Ax */ \
    {{900000, 378000, 484000, 400000,  800000, 484000, 190000, 190000, 1250,  912 },  0x92}, \
    {{600000, 378000, 484000, 400000,  800000, 484000, 190000, 190000, 1250,  850 },  0x93}, /* T30AG-Ax */ \
    {{900000, 378000, 484000, 400000,  800000, 484000, 190000, 190000, 1250,  912 },  0xb0}, /* T30IQS-Ax */ \
    {{900000, 378000, 484000, 400000,  800000, 484000, 190000, 190000, 1250,  912 },  0xb1}, /* T30MQS-Ax */ \

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  // INCLUDED_T30RM_CLOCKS_LIMITS_PRIVATE_H
