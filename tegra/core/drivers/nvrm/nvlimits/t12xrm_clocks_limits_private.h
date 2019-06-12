/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_T12XRM_CLOCKS_LIMITS_PRIVATE_H
#define INCLUDED_T12XRM_CLOCKS_LIMITS_PRIVATE_H

#include "nvrm_clocks_limits_private.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

// TODO: T12x process corners and mapping to fuses

// Number of characterized T12x process corners
#define NV_T12X_PROCESS_CORNERS (4)

// T12x RAM timing controls low voltage threshold and setting
#define NV_T12X_SVOP_LOW_VOLTAGE (1010)
#define NV_T12X_SVOP_LOW_SETTING \
        (NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_DP,  0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_PDP, 0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_REG, 0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_SP,  0))

#define NV_T12X_SVOP_HIGH_SETTING \
        (NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_DP,  0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_PDP, 0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_REG, 0) | \
         NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_SP,  0))

/**
 * The tables below provide clock limits for T12x modules for different core
 * voltages for each process corner. Note that EMC and MC are not included,
 * as memory scaling has board dependencies, and EMC scaling configurations
 * are retrieved by ODM query NvOdmQuerySdramControllerConfigGet(). The ODM
 * data is adjuasted based on process corner using EMC DQSIB offsets.
 */

#define NV_T12XSS_SHMOO_VOLTAGES           950,   1000,    1100,    1200
#define NV_T12XSS_SCALED_CLK_LIMITS \
    { NV_DEVID_I2S      , 0,       1,  {    1,      1,      1,   26000 } }, \
    { NV_DEVID_SPDIF    , 0,       1,  {    1,      1,      1,  100000 } }, \
    { NV_DEVID_I2C      , 0,       1,  {    1,      1,      1,   26000 } }, \
    { NV_DEVID_DVC      , 0,       1,  {    1,      1,      1,   26000 } }, \
    { NV_DEVID_OWR      , 0,       1,  {    1,      1,      1,   26000 } }, \
    { NV_DEVID_XIO      , 0,       1,  {    1,      1,      1,  150000 } }, \
    { NV_DEVID_TWC      , 0,       1,  {    1,      1,      1,  150000 } }, \
    { NV_DEVID_HSMMC    , 0,       1,  {    1,      1,      1,  208000 } }, \
    { NV_DEVID_VFIR     , 0,       1,  {    1,      1,      1,   72000 } }, \
    { NV_DEVID_UART     , 0,       1,  {    1,      1,      1,   72000 } }, \
    { NV_DEVID_NANDCTRL , 0,       1,  {    1,      1,      1,  240000 } }, \
    { NV_DEVID_SNOR     , 0,       1,  {    1,      1,      1,  127000 } }, \
    { NV_DEVID_EIDE     , 0,       1,  {    1,      1,      1,  100000 } }, \
    { NV_DEVID_MIPI_HS  , 0,       1,  {    1,      1,      1,   40000 } }, \
    { NV_DEVID_SDIO     , 0,       1,  {    1,      1,      1,  208000 } }, \
    { NV_DEVID_SPI      , 0,       1,  {    1,      1,      1,   40000 } }, \
    { NV_DEVID_SLINK    , 0,       1,  {    1,      1,      1,  160000 } }, \
    { NV_DEVID_USB      , 0,       1,  {    1,      1,      1,  480000 } }, \
    { NV_DEVID_PCIE     , 0,       1,  {    1,      1,      1,  250000 } }, \
    { NV_DEVID_HOST1X   , 0,       1,  {    1,      1,      1,  166000 } }, \
    { NV_DEVID_EPP      , 0,       1,  {    1,      1,      1,  300000 } }, \
    { NV_DEVID_GR2D     , 0,       1,  {    1,      1,      1,  356000 } }, \
    { NV_DEVID_GR3D     , 0,       1,  {    1,      1,      1,  356000 } }, \
    { NV_DEVID_MPE      , 0,       1,  {    1,      1,      1,  333000 } }, \
    { NV_DEVID_VI       , 0,       1,  {    1,      1,      1,  150000 } }, \
    { NV_DEVID_DISPLAY  , 0,       1,  {    1,      1,      1,  270000 } }, \
    { NV_DEVID_DSI      , 0,       1,  {    1,      1,      1,  500000 } }, \
    { NV_DEVID_TVO      , 0,       1,  {    1,      1,      1,  300000 } }, \
    { NV_DEVID_HDMI     , 0,       1,  {    1,      1,      1,  148500 } }, \
    { NV_DEVID_HDA      , 0,       1,  {    1,      1,      1,  108000 } }, \
    { NV_DEVID_ACTMON   , 0,       1,  {    1,      1,      1,  216000 } }, \
    { NV_DEVID_MSELECT  , 0,       1,  {    1,      1,      1,  216000 } }, \
    { NV_DEVID_TSENSOR  , 0,       1,  {    1,      1,      1,  216000 } }, \
    { NV_DEVID_SE       , 0,       1,  {    1,      1,      1,  216000 } }, \
    { NV_DEVID_SATA     , 0,       1,  {    1,      1,      1,  216000 } }, \
    { NV_DEVID_AVP,0, NVRM_BUS_MIN_KHZ,{    1,      1,      1,  275000 } }, \
    { NV_DEVID_VDE      , 0,       1,  {    1,      1,      1,  300000 } }, \
    { NVRM_DEVID_CLK_SRC, 0,       1,  {    1,      1,      1, 1067000 } }

#define NV_T12XST_SHMOO_VOLTAGES     NV_T12XSS_SHMOO_VOLTAGES
#define NV_T12XST_SCALED_CLK_LIMITS  NV_T12XSS_SCALED_CLK_LIMITS

#define NV_T12XFT_SHMOO_VOLTAGES     NV_T12XSS_SHMOO_VOLTAGES
#define NV_T12XFT_SCALED_CLK_LIMITS  NV_T12XSS_SCALED_CLK_LIMITS

#define NV_T12XFF_SHMOO_VOLTAGES     NV_T12XSS_SHMOO_VOLTAGES
#define NV_T12XFF_SCALED_CLK_LIMITS  NV_T12XSS_SCALED_CLK_LIMITS

/**
 * The tables below provides clock limits for T12x CPU for different CPU
 * voltages, for all process corners
 */

#define NV_T12XSS_CPU_VOLTAGES                750,    800,    850,     900,     950,    1025
#define NV_T12XSS_SCALED_CPU_CLK_LIMITS \
    { NV_DEVID_CPU, 0, NVRM_BUS_MIN_KHZ, {     1,      1,      1,       1,       1, 1000000} }

#define NV_T12XST_CPU_VOLTAGES               NV_T12XSS_CPU_VOLTAGES
#define NV_T12XST_SCALED_CPU_CLK_LIMITS      NV_T12XSS_SCALED_CPU_CLK_LIMITS

#define NV_T12XFT_CPU_VOLTAGES               NV_T12XSS_CPU_VOLTAGES
#define NV_T12XFT_SCALED_CPU_CLK_LIMITS      NV_T12XSS_SCALED_CPU_CLK_LIMITS

#define NV_T12XFF_CPU_VOLTAGES               NV_T12XSS_CPU_VOLTAGES
#define NV_T12XFF_SCALED_CPU_CLK_LIMITS      NV_T12XSS_SCALED_CPU_CLK_LIMITS


/**
 * T12x EMC DQSIB offsets in order of process calibration settings
 */

#define NV_T12X_DQSIB_OFFSETS    0, 0, 0, 0

/**
 * T12x OSC Doubler tap delays for each supported frequency in order
 * of process calibration settings
 */
#define NV_T12X_OSC_DOUBLER_CONFIGURATIONS \
/*     Osc kHz  SS  ST  FT  FF   */ \
    {  12000, {  7,  7,  7,  7 } }, \
    {  13000, {  7,  7,  7,  7 } },


/**
 * This table provides maximum limits for selected T12x modules depended on
 * T12x SKUs. Table entries are ordered by SKU number.
 */
//    CPU      Avp     Vde     MC      EMC2x    3D      DispA   DispB    mV   Cpu mV   SKU
#define NV_T12X_SKUED_LIMITS \
    {{1000000, 240000, 240000, 400000,  800000, 300000, 297000, 297000, 1200, 1025 },  0}, /* Dev (T12x Unrestricted) */ \


/**
 * Initializes SoC characterization data base
 *
 * @param hRmDevice The RM device handle.
 * @param pChipFlavor a pointer to the chip "flavor" structure
 *  that this function fills in
 */
void NvRmPrivT12xFlavorInit(
    NvRmDeviceHandle hRmDevice,
    NvRmChipFlavor* pChipFlavor);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  // INCLUDED_T12XRM_CLOCKS_LIMITS_PRIVATE_H
