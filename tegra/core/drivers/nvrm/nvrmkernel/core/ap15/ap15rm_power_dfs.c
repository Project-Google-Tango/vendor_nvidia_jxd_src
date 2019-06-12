/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Dynamic Frequency Scaling manager </b>
 *
 * @b Description: Implements NvRM Dynamic Frequency Scaling (DFS)
 *                  manager for SOC-wide clock domains.
 *
 */

#include "nvrm_power_dfs.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_hwintf.h"
#include "nvrm_pmu.h"
#include "ap15rm_power_dfs.h"
#include "ap20/arstat_mon.h"
#include "ap20/arvde_mon.h"
#include "ap20/aremc.h"
#include "ap20/arclk_rst.h"
#include "ap20/arapb_misc.h"
#include "ap20/artimerus.h"

/*****************************************************************************/

// Regsiter access macros for System Statistic module
#define NV_SYSTAT_REGR(pSystatRegs, reg) \
            NV_READ32((((NvU32)(pSystatRegs)) + STAT_MON_##reg##_0))
#define NV_SYSTAT_REGW(pSystatRegs, reg, val) \
            NV_WRITE32((((NvU32)(pSystatRegs)) + STAT_MON_##reg##_0), (val))

// Regsiter access macros for VDE module
#define NV_VDE_REGR(pVdeRegs, reg) \
            NV_READ32((((NvU32)(pVdeRegs)) + ARVDE_PPB_##reg##_0))
#define NV_VDE_REGW(pVdeRegs, reg, val) \
            NV_WRITE32((((NvU32)(pVdeRegs)) + ARVDE_PPB_##reg##_0), (val))

// Regsiter access macros for EMC module
#define NV_EMC_REGR(pEmcRegs, reg) \
            NV_READ32((((NvU32)(pEmcRegs)) + EMC_##reg##_0))
#define NV_EMC_REGW(pEmcRegs, reg, val) \
            NV_WRITE32((((NvU32)(pEmcRegs)) + EMC_##reg##_0), (val))

// Regsiter access macros for CAR module
#define NV_CAR_REGR(pCarRegs, reg) \
         NV_READ32((((NvU32)(pCarRegs)) + CLK_RST_CONTROLLER_##reg##_0))
#define NV_CAR_REGW(pCarRegs, reg, val) \
         NV_WRITE32((((NvU32)(pCarRegs)) + CLK_RST_CONTROLLER_##reg##_0), (val))

// Regsiter access macros for APB MISC module
#define NV_APB_REGR(pApbRegs, reg) \
         NV_READ32((((NvU32)(pApbRegs)) + APB_MISC_##reg##_0))
#define NV_APB_REGW(pApbRegs, reg, val) \
         NV_WRITE32((((NvU32)(pApbRegs)) + APB_MISC_##reg##_0), (val))

/*****************************************************************************/
// SYSTEM STATISTIC MODULE INTERFACES
/*****************************************************************************/

NvError NvRmPrivAp15SystatMonitorsInit(NvRmDfs* pDfs)
{
    NvError error;
    NvU32 RegValue;
    void* pSystatRegs = pDfs->Modules[NvRmDfsModuleId_Systat].pBaseReg;
    NV_ASSERT(pSystatRegs);

    /*
     * System Statistic Monitor module belongs to DFS, therefore it is full
     * initialization: Enable Clock => Reset => clear all control registers
     * including interrupt status flags (cleared by writing "1"). Note that
     * all monitors - used, or not used by DFS - are initialized. (The VPIPE
     * monitor in this module does not provide neccessary data for DFS; the
     * VDE idle monitor is employed for video-pipe domain control)
     */
    error = NvRmPowerModuleClockControl(
        pDfs->hRm, NvRmModuleID_SysStatMonitor, pDfs->PowerClientId, NV_TRUE);
    if (error != NvSuccess)
    {
        return error;
    }
    NvRmModuleReset(pDfs->hRm, NvRmModuleID_SysStatMonitor);

    RegValue = NV_DRF_NUM(STAT_MON, CPU_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, CPU_MON_CTRL, RegValue);

    RegValue = NV_DRF_NUM(STAT_MON, COP_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, COP_MON_CTRL, RegValue);

    RegValue = NV_DRF_NUM(STAT_MON, CACHE2_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, CACHE2_MON_CTRL, RegValue);

    RegValue = NV_DRF_NUM(STAT_MON, AHB_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, AHB_MON_CTRL, RegValue);

    RegValue = NV_DRF_NUM(STAT_MON, APB_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, APB_MON_CTRL, RegValue);

    RegValue = NV_DRF_NUM(STAT_MON, VPIPE_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, VPIPE_MON_CTRL, RegValue);

    RegValue = NV_DRF_NUM(STAT_MON, SMP_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, SMP_MON_CTRL, RegValue);

    return NvSuccess;
}

void NvRmPrivAp15SystatMonitorsDeinit(NvRmDfs* pDfs)
{
    // First initialize monitor, and then just turn off the clock (twice to
    // balance the clock control)
    (void)NvRmPrivAp15SystatMonitorsInit(pDfs);
    (void)NvRmPowerModuleClockControl(
        pDfs->hRm, NvRmModuleID_SysStatMonitor, pDfs->PowerClientId, NV_FALSE);
    (void)NvRmPowerModuleClockControl(
        pDfs->hRm, NvRmModuleID_SysStatMonitor, pDfs->PowerClientId, NV_FALSE);

}

void
NvRmPrivAp15SystatMonitorsStart(
    const NvRmDfs* pDfs,
    const NvRmDfsFrequencies* pDfsKHz,
    const NvU32 IntervalMs)
{
    NvU32 RegValue;
    NvU32 msec = IntervalMs - 1; // systat monitors use (n+1) ms counters
    void* pSystatRegs = pDfs->Modules[NvRmDfsModuleId_Systat].pBaseReg;

    /*
     * Start AVP (COP) monitor for the next sample period. Interrupt is
     * cleared (by writing "1") and left disabled. Monitor is counting
     * System clock cycles while AVP is halted by flow controller. Note
     * that AVP monitor is counting System (not AVP!) clock cycles
     */
    RegValue = NV_DRF_DEF(STAT_MON, COP_MON_CTRL, ENB, ENABLE) |
               NV_DRF_NUM(STAT_MON, COP_MON_CTRL, INT, 1) |
               NV_DRF_NUM(STAT_MON, COP_MON_CTRL, SAMPLE_PERIOD, msec);
    NV_SYSTAT_REGW(pSystatRegs, COP_MON_CTRL, RegValue);

    /*
     * Start AHB monitor for the next sample period. Interrupt is cleared
     * (by writing "1") and left disabled. Monitor is counting AHB clock
     * cycles while there is no data transfer on AHB initiated by any master
     */
    RegValue = NV_DRF_DEF(STAT_MON, AHB_MON_CTRL, ENB, ENABLE) |
               NV_DRF_NUM(STAT_MON, AHB_MON_CTRL, INT, 1) |
               NV_DRF_DEF(STAT_MON, AHB_MON_CTRL, MST_NUMBER, DEFAULT_MASK) |
               NV_DRF_NUM(STAT_MON, AHB_MON_CTRL, SAMPLE_PERIOD, msec);
    NV_SYSTAT_REGW(pSystatRegs, AHB_MON_CTRL, RegValue);

    /*
     * Start APB monitor for the next sample period. Interrupt is cleared
     * (by writing "1") and left disabled. Monitor is counting APB clock
     * cycles while there is no data transfer on APB targeted to any slave
     */
    RegValue = NV_DRF_DEF(STAT_MON, APB_MON_CTRL, ENB, ENABLE) |
               NV_DRF_NUM(STAT_MON, APB_MON_CTRL, INT, 1) |
               NV_DRF_DEF(STAT_MON, APB_MON_CTRL, SLV_NUMBER, DEFAULT_MASK) |
               NV_DRF_NUM(STAT_MON, APB_MON_CTRL, SAMPLE_PERIOD, msec);
    NV_SYSTAT_REGW(pSystatRegs, APB_MON_CTRL, RegValue);

    /*
     * Start CPU monitor for the next sample period. Interrupt is cleared
     * (by writing "1") and enabled, since CPU monitor is used to generate
     * DFS interrupt. Monitor is counting System clock cycles while CPU is
     * halted by flow controller. Note: CPU monitor is counting System (not
     * CPU!) clock cycles
     */
    RegValue = NV_DRF_DEF(STAT_MON, CPU_MON_CTRL, ENB, ENABLE) |
               NV_DRF_DEF(STAT_MON, CPU_MON_CTRL, INT_EN, ENABLE) |
               NV_DRF_NUM(STAT_MON, CPU_MON_CTRL, INT, 1) |
               NV_DRF_NUM(STAT_MON, CPU_MON_CTRL, SAMPLE_PERIOD, msec);
    NV_SYSTAT_REGW(pSystatRegs, CPU_MON_CTRL, RegValue);

    // Initialize LP2 time storage (WAR for bug 429585)
    NvRmPrivSetLp2TimeUS(pDfs->hRm, 0);
}

void
NvRmPrivAp15SystatMonitorsRead(
    const NvRmDfs* pDfs,
    const NvRmDfsFrequencies* pDfsKHz,
    NvRmDfsIdleData* pIdleData)
{
    NvU32 RegValue;
    NvU64 temp;
    void* pSystatRegs = pDfs->Modules[NvRmDfsModuleId_Systat].pBaseReg;
    NvBool NoLp2Offset = pDfs->Modules[NvRmDfsModuleId_Systat].Offset !=
        NVRM_CPU_IDLE_LP2_OFFSET;

    /*
     * Read AVP (COP) monitor: disable it (=stop, the readings are preserved)
     * and clear interrupt status bit (by writing "1"). Then, read AVP idle
     * count. Since AVP monitor is counting System (not AVP!) clock cycles,
     * the monitor reading is converted to AVP clocks before storing it in
     * idle data packet.
     */
    RegValue = NV_DRF_NUM(STAT_MON, COP_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, COP_MON_CTRL, RegValue);

    RegValue = NV_SYSTAT_REGR(pSystatRegs, COP_MON_STATUS);
    RegValue = NV_DRF_VAL(STAT_MON, COP_MON_STATUS, COUNT, RegValue);

    temp = ((NvU64)RegValue * pDfsKHz->Domains[NvRmDfsClockId_Avp]);
    temp = NvDiv64(temp, pDfsKHz->Domains[NvRmDfsClockId_System]);

    pIdleData->Readings[NvRmDfsClockId_Avp] = (NvU32)temp;

    /*
     * Read AHB monitor: disable it (=stop, the readings are preserved) and
     * clear interrupt status bit (by writing "1"). Then, read AHB idle count
     * (in AHB clock cycles) and store it in idle data packet.
     */
    RegValue = NV_DRF_NUM(STAT_MON, AHB_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, AHB_MON_CTRL, RegValue);

    RegValue = NV_SYSTAT_REGR(pSystatRegs, AHB_MON_STATUS);
    pIdleData->Readings[NvRmDfsClockId_Ahb] =
        NV_DRF_VAL(STAT_MON, AHB_MON_STATUS, COUNT, RegValue);

    /*
     * Read APB monitor: disable it (=stop, the readings are preserved) and
     * clear interrupt status bit (by writing "1"). Then, read APB idle count
     * (in APB clock cycles) and store it in idle data packet.
     */
    RegValue = NV_DRF_NUM(STAT_MON, APB_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, APB_MON_CTRL, RegValue);

    RegValue = NV_SYSTAT_REGR(pSystatRegs, APB_MON_STATUS);
    pIdleData->Readings[NvRmDfsClockId_Apb] =
        NV_DRF_VAL(STAT_MON, APB_MON_STATUS, COUNT, RegValue);

    /*
     * Read CPU monitor: read current sampling period and store it in idle
     * data packet. Disable monitor (=stop, the readings are preserved) and
     * clear interrupt status bit (by writing "1"). Read CPU idle count.
     * Since CPU monitor is counting System (not CPU!) cycles, the monitor
     * readings are converted to CPU clocks before storing in idle data packet
     */
    RegValue = NV_SYSTAT_REGR(pSystatRegs, CPU_MON_CTRL);
    pIdleData->CurrentIntervalMs = 1 +  // systat monitors use (n+1) ms counters
        NV_DRF_VAL(STAT_MON, CPU_MON_CTRL, SAMPLE_PERIOD, RegValue);

    RegValue = NV_DRF_NUM(STAT_MON, CPU_MON_CTRL, INT, 1);
    NV_SYSTAT_REGW(pSystatRegs, CPU_MON_CTRL, RegValue);

    // Add LP2 time to idle measurements (WAR for bug 429585)
    // For logging only - use 2^10 ~ 1000, and round up
    RegValue = NvRmPrivGetLp2TimeUS(pDfs->hRm);
    pIdleData->Lp2TimeMs = (RegValue + (0x1 << 10) - 1) >> 10;
    if ((RegValue == 0) || NoLp2Offset)
    {
        pIdleData->Readings[NvRmDfsClockId_Cpu] = 0;
    }
    else if (RegValue < NVRM_DFS_MAX_SAMPLE_MS * 1000)
    {   //  (US * KHz) / 1000 ~ (US * 131 * KHz) / (128 * 1024)
        pIdleData->Readings[NvRmDfsClockId_Cpu] =
            (NvU32)(((NvU64)(RegValue * 131) *
                     pDfsKHz->Domains[NvRmDfsClockId_Cpu]) >> 17);
    }
    else
    {
        pIdleData->Readings[NvRmDfsClockId_Cpu] = 0xFFFFFFFFUL;
        return; // the entire sample is idle, anyway
    }
    RegValue = NV_SYSTAT_REGR(pSystatRegs, CPU_MON_STATUS);
    RegValue = NV_DRF_VAL(STAT_MON, CPU_MON_STATUS, COUNT, RegValue);

    temp = ((NvU64)RegValue * pDfsKHz->Domains[NvRmDfsClockId_Cpu]);
    temp = NvDiv64(temp, pDfsKHz->Domains[NvRmDfsClockId_System]);

    pIdleData->Readings[NvRmDfsClockId_Cpu] += (NvU32)temp;
}

/*****************************************************************************/
// VDE MODULE INTERFACES
/*****************************************************************************/

NvError NvRmPrivAp15VdeMonitorsInit(NvRmDfs* pDfs)
{
    NvU32 RegValue;
    void* pVdeRegs = pDfs->Modules[NvRmDfsModuleId_Vde].pBaseReg;
    NV_ASSERT(pVdeRegs);

    /*
     * Video pipe monitor belongs to VDE module - just clear monitor control
     * register including interrupt status bit, and do not touch anything
     * else in VDE
     */
    RegValue = NV_DRF_NUM(ARVDE_PPB, IDLE_MON, INT_STATUS, 1);
    NV_VDE_REGW(pVdeRegs, IDLE_MON, RegValue);

    return NvSuccess;
}

void NvRmPrivAp15VdeMonitorsDeinit(NvRmDfs* pDfs)
{
    // Stop monitor using initialization procedure
    (void)NvRmPrivAp15VdeMonitorsInit(pDfs);
}

void
NvRmPrivAp15VdeMonitorsStart(
    const NvRmDfs* pDfs,
    const NvRmDfsFrequencies* pDfsKHz,
    const NvU32 IntervalMs)
{
    NvU32 RegValue;
    NvU32 cycles = IntervalMs * pDfsKHz->Domains[NvRmDfsClockId_Vpipe];
    void* pVdeRegs = pDfs->Modules[NvRmDfsModuleId_Vde].pBaseReg;

    /*
     * Start VDE vpipe monitor for the next sample period. Interrupt status bit
     * is cleared by writing "1" (it is not connected to interrupt controller,
     * just "count end" status bit). Monitor is counting v-clock cycles while
     * all VDE submodules are idle. The sample period is specified in v-clock
     * cycles rather than in time units.
     */
    RegValue = NV_DRF_NUM(ARVDE_PPB, IDLE_MON, ENB, 1) |
               NV_DRF_NUM(ARVDE_PPB, IDLE_MON, INT_STATUS, 1) |
               NV_DRF_NUM(ARVDE_PPB, IDLE_MON, SAMPLE_PERIOD, cycles);
    NV_VDE_REGW(pVdeRegs, IDLE_MON, RegValue);
}

void
NvRmPrivAp15VdeMonitorsRead(
    const NvRmDfs* pDfs,
    const NvRmDfsFrequencies* pDfsKHz,
    NvRmDfsIdleData* pIdleData)
{
    // CAR module virtual base address
    static void* s_pCarBaseReg = NULL;
    NvU32 RegValue;
    void* pVdeRegs = pDfs->Modules[NvRmDfsModuleId_Vde].pBaseReg;

    if (s_pCarBaseReg == NULL)
    {
        NvRmModuleTable *tbl = NvRmPrivGetModuleTable(pDfs->hRm);
        s_pCarBaseReg = (tbl->ModInst +
            tbl->Modules[NvRmPrivModuleID_ClockAndReset].Index)->VirtAddr;
    }
    RegValue = NV_CAR_REGR(s_pCarBaseReg, CLK_OUT_ENB_H);

    // If VDE clock is disabled set idle count to maximum
    if (!(RegValue & CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_VDE_FIELD))
    {
        pIdleData->Readings[NvRmDfsClockId_Vpipe] = (NvU32)-1;
        return;
    }

    /*
     * Read VDE vpipe monitor: disable it (=stop, the readings are preserved) and
     * clear count done status bit (by writing "1"). Then, read VDE idle count (in
     * v-clock cycles) and store it in idle data packet.
     */
    RegValue = NV_DRF_NUM(ARVDE_PPB, IDLE_MON, INT_STATUS, 1);
    NV_VDE_REGW(pVdeRegs, IDLE_MON, RegValue);

    RegValue = NV_VDE_REGR(pVdeRegs, IDLE_STATUS);
    pIdleData->Readings[NvRmDfsClockId_Vpipe] =
        NV_DRF_VAL(ARVDE_PPB, IDLE_STATUS, COUNT, RegValue);
}

/*****************************************************************************/

void
NvRmPrivAp15SetSvopControls(
    NvRmDeviceHandle hRm,
    NvU32 SvopSetting)
{
#define SVOP_MASK \
    (NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_DP, 0xFFFFFFFFUL) |  \
     NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_PDP, 0xFFFFFFFFUL) | \
     NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_REG, 0xFFFFFFFFUL) | \
     NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_SP, 0xFFFFFFFFUL))

    NvU32 reg;
    static void* s_pApbBaseReg = NULL;  // APB MISC module virtual base address

    if (s_pApbBaseReg == NULL)
    {
        NvRmModuleTable *tbl = NvRmPrivGetModuleTable(hRm);
        s_pApbBaseReg = (tbl->ModInst +
            tbl->Modules[NvRmModuleID_Misc].Index)->VirtAddr;
    }
    NV_ASSERT((SvopSetting & (~SVOP_MASK)) == 0);
    reg = NV_APB_REGR(s_pApbBaseReg, GP_ASDBGREG);  // RAM timing control
    reg = (reg & (~SVOP_MASK)) | SvopSetting;
    NV_APB_REGW(s_pApbBaseReg, GP_ASDBGREG, reg);
}

/*****************************************************************************/

void* NvRmPrivAp15GetTimerUsVirtAddr(NvRmDeviceHandle hRm)
{
    NvRmModuleTable *tbl = NvRmPrivGetModuleTable(hRm);
    void* va = (void*)((NvUPtr)((tbl->ModInst +
        tbl->Modules[NvRmModuleID_TimerUs].Index)->VirtAddr) +
        TIMERUS_CNTR_1US_0);
    return va;
}

/*****************************************************************************/
