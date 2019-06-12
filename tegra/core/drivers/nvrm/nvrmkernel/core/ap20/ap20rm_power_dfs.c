/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "ap20rm_power_dfs.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_hwintf.h"
#include "nvrm_pmu.h"
#include "ap20/aremc.h"
#include "ap20/arclk_rst.h"
#include "ap20/arapb_misc.h"

// Regsiter access macros for EMC module
#define NV_EMC_REGR(pEmcRegs, reg) \
    NV_READ32((((NvU32)(pEmcRegs)) + EMC_##reg##_0))
#define NV_EMC_REGW(pEmcRegs, reg, val) \
    NV_WRITE32((((NvU32)(pEmcRegs)) + EMC_##reg##_0), (val))

// Regsiter access macros for APB MISC module
#define NV_APB_REGR(pApbRegs, reg) \
    NV_READ32((((NvU32)(pApbRegs)) + APB_MISC_##reg##_0))
#define NV_APB_REGW(pApbRegs, reg, val) \
    NV_WRITE32((((NvU32)(pApbRegs)) + APB_MISC_##reg##_0), (val))

// TODO: Always Disable before check-in
#define NVRM_TEST_PMREQUEST_UP_MODE (0)

NvError NvRmPrivAp20EmcMonitorsInit(NvRmDfs* pDfs)
{
    NvU32 RegValue;
    void* pEmcRegs = pDfs->Modules[NvRmDfsModuleId_Emc].pBaseReg;
    NV_ASSERT(pEmcRegs);

    /*
     * EMC power management monitor belongs to EMC module - just reset it,
     * and do not touch anything else in EMC.
     */ 
    RegValue = NV_EMC_REGR(pEmcRegs, STAT_CONTROL);
    RegValue = NV_FLD_SET_DRF_DEF(EMC, STAT_CONTROL, PWR_GATHER, RST, RegValue);
    NV_EMC_REGW(pEmcRegs, STAT_CONTROL, RegValue);

    /*
    * EMC active clock cycles = EMC monitor reading * 2^M, where M depends
    * on DRAM type and bus width. Power M is stored as EMC readouts scale
    */
    #define COUNT_SHIFT_DDR1_X32 (1)
    RegValue = NV_EMC_REGR(pEmcRegs, FBIO_CFG5);
    switch (NV_DRF_VAL(EMC, FBIO_CFG5, DRAM_TYPE, RegValue))
    {
        case EMC_FBIO_CFG5_0_DRAM_TYPE_DDR1:
        case EMC_FBIO_CFG5_0_DRAM_TYPE_LPDDR2:
        case EMC_FBIO_CFG5_0_DRAM_TYPE_DDR2:
            pDfs->Modules[NvRmDfsModuleId_Emc].Scale = COUNT_SHIFT_DDR1_X32;
            break;
        default:
            NV_ASSERT(!"Not supported DRAM type");
    }
    if (NV_DRF_VAL(EMC, FBIO_CFG5, DRAM_WIDTH, RegValue) ==
        EMC_FBIO_CFG5_0_DRAM_WIDTH_X16)
    {
        pDfs->Modules[NvRmDfsModuleId_Emc].Scale++;
    }
    return NvSuccess;
}

void NvRmPrivAp20EmcMonitorsDeinit(NvRmDfs* pDfs)
{
     // Stop monitor using initialization procedure
    (void)NvRmPrivAp20EmcMonitorsInit(pDfs);
}

void
NvRmPrivAp20EmcMonitorsStart(
    const NvRmDfs* pDfs,
    const NvRmDfsFrequencies* pDfsKHz,
    const NvU32 IntervalMs)
{
    NvU32 RegValue, SavedRegValue;
    void* pEmcRegs = pDfs->Modules[NvRmDfsModuleId_Emc].pBaseReg;

    // EMC sample period is specified in EMC clock cycles, accuracy 0-16 cycles.
    #define MEAN_EMC_LIMIT_ERROR (8)
    NvU32 cycles = IntervalMs * pDfsKHz->Domains[NvRmDfsClockId_Emc] +
                    MEAN_EMC_LIMIT_ERROR;
    /*
     * Start EMC power monitor for the next sample period: clear EMC counters,
     * set sample interval limit in EMC cycles, enable monitoring. Monitor is
     * counting EMC 1x clock cycles while any memory access is detected. 
     */
    SavedRegValue = NV_EMC_REGR(pEmcRegs, STAT_CONTROL);
    RegValue = NV_FLD_SET_DRF_DEF(EMC, STAT_CONTROL, PWR_GATHER, CLEAR, SavedRegValue);
    NV_EMC_REGW(pEmcRegs, STAT_CONTROL, RegValue);

    RegValue = NV_DRF_NUM(EMC, STAT_PWR_CLOCK_LIMIT, PWR_CLOCK_LIMIT, cycles);
    NV_EMC_REGW(pEmcRegs, STAT_PWR_CLOCK_LIMIT, RegValue);

    RegValue = NV_FLD_SET_DRF_DEF(EMC, STAT_CONTROL, PWR_GATHER, ENABLE, SavedRegValue);
    NV_EMC_REGW(pEmcRegs, STAT_CONTROL, RegValue);
}

void
NvRmPrivAp20EmcMonitorsRead(
    const NvRmDfs* pDfs,
    const NvRmDfsFrequencies* pDfsKHz,
    NvRmDfsIdleData* pIdleData)
{
    NvU32 RegValue, TotalClocks;
    NvU32 CountShift = pDfs->Modules[NvRmDfsModuleId_Emc].Scale;
    void* pEmcRegs = pDfs->Modules[NvRmDfsModuleId_Emc].pBaseReg;

    /*
     * Read EMC monitor: disable it (=stop, the readings are preserved), and
     * determine idle count based on total and active clock counts. Monitor
     * readings are multiplied by 2^M factor to determine active count, where
     * power M depends on DRAM type and bus width. Store result in the idle
     * data packet.
     */
    RegValue = NV_EMC_REGR(pEmcRegs, STAT_CONTROL);
    RegValue = NV_FLD_SET_DRF_DEF(EMC, STAT_CONTROL, PWR_GATHER, DISABLE, RegValue);
    NV_EMC_REGW(pEmcRegs, STAT_CONTROL, RegValue);

    RegValue = NV_EMC_REGR(pEmcRegs, STAT_PWR_CLOCKS);
    TotalClocks = NV_DRF_VAL(EMC, STAT_PWR_CLOCKS, PWR_CLOCKS, RegValue);
    RegValue = NV_EMC_REGR(pEmcRegs, STAT_PWR_COUNT);
    RegValue = NV_DRF_VAL(EMC, STAT_PWR_COUNT, PWR_COUNT, RegValue) << CountShift;

    pIdleData->Readings[NvRmDfsClockId_Emc] = 
        (TotalClocks > RegValue) ? (TotalClocks - RegValue) : 0;
}

// AP20 Thermal policy definitions

#define NVRM_DTT_DEGREES_HIGH           (85L)
#define NVRM_DTT_DEGREES_LOW            (60L)
#define NVRM_DTT_DEGREES_HYSTERESIS     (5L)

#define NVRM_DTT_VOLTAGE_THROTTLE_MV    (900UL)
#define NVRM_DTT_CPU_DELTA_KHZ          (100000UL)

#define NVRM_DTT_POLL_MS_SLOW           (2000UL)
#define NVRM_DTT_POLL_MS_FAST           (1000UL)
#define NVRM_DTT_POLL_MS_CRITICAL       (500UL)

typedef enum
{
    NvRmDttAp20PolicyRange_Unknown = 0,

    // No throttling
    NvRmDttAp20PolicyRange_FreeRunning,

    // Keep CPU frequency below low voltage threshold
    NvRmDttAp20PolicyRange_LimitVoltage,

    // Decrease CPU frequency in steps over time
    NvRmDttAp20PolicyRange_ThrottleDown,

    NvRmDttAp20PolicyRange_Num,
    NvRmDttAp20PolicyRange_Force32 = 0x7FFFFFFFUL
} NvRmDttAp20PolicyRange;

static NvRmFreqKHz s_CpuThrottleMaxKHz = 0;
static NvRmFreqKHz s_CpuThrottleMinKHz = 0;
static NvRmFreqKHz s_CpuThrottleKHz = 0;

NvBool
NvRmPrivAp20DttClockUpdate(
    NvRmDeviceHandle hRmDevice,
    NvS32 TemperatureC,
    const NvRmTzonePolicy* pDttPolicy,
    const NvRmDfsFrequencies* pCurrentKHz,
    NvRmDfsFrequencies* pDfsKHz)
{
    switch ((NvRmDttAp20PolicyRange)pDttPolicy->PolicyRange)
    {
        case NvRmDttAp20PolicyRange_LimitVoltage:
            s_CpuThrottleKHz = s_CpuThrottleMaxKHz;
            break;

        case NvRmDttAp20PolicyRange_ThrottleDown:
            if (pDttPolicy->UpdateFlag)
                s_CpuThrottleKHz -= NVRM_DTT_CPU_DELTA_KHZ;
            s_CpuThrottleKHz = NV_MAX(s_CpuThrottleKHz, s_CpuThrottleMinKHz); 
            break;

        // No throttling by default (just reset throttling limit to max)
        default:
            s_CpuThrottleKHz = s_CpuThrottleMaxKHz;
            return NV_FALSE;
    }
    pDfsKHz->Domains[NvRmDfsClockId_Cpu] =
        NV_MIN(pDfsKHz->Domains[NvRmDfsClockId_Cpu], s_CpuThrottleKHz);

    // Throttling step is completed - no need to force extra DVFS update
    return NV_FALSE;
}

NvRmPmRequest 
NvRmPrivAp20GetPmRequest(
    NvRmDeviceHandle hRmDevice,
    const NvRmDfsSampler* pCpuSampler,
    NvRmFreqKHz* pCpuKHz)
{
    // Assume initial slave CPU1 On request
    static NvRmPmRequest s_LastPmRequest = (NvRmPmRequest_CpuOnFlag | 0x1);
    static NvRmFreqKHz s_Cpu1OnMinKHz = 0, s_Cpu1OffMaxKHz = 0;
    static NvU32 s_Cpu1OnPendingCnt = 0, s_Cpu1OffPendingCnt = 0;

    NvRmModuleID id = NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 );
    NvRmPmRequest PmRequest = NvRmPmRequest_None;
    NvBool Cpu1Off =
        (0 != NV_DRF_VAL(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_CPURESET1,
                NV_REGR(hRmDevice, id,
                    CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET_0)));
    NvRmFreqKHz CpuLoadGaugeKHz = *pCpuKHz;

    // Slave CPU1 power management policy thresholds:
    // - use fixed values if they are defined explicitly, otherwise
    // - set CPU1 OffMax threshold at 2/3 of cpu frequency range,
    //   and half of that frequency as CPU1 OnMin threshold
    if ((s_Cpu1OffMaxKHz == 0) && (s_Cpu1OnMinKHz == 0))
    {
        NvRmFreqKHz MaxKHz =
            NvRmPrivGetSocClockLimits(NvRmModuleID_Cpu)->MaxKHz;

        s_Cpu1OnMinKHz = NVRM_CPU1_ON_MIN_KHZ ?
                         NVRM_CPU1_ON_MIN_KHZ : (MaxKHz / 3);
        s_Cpu1OffMaxKHz = NVRM_CPU1_OFF_MAX_KHZ ?
                          NVRM_CPU1_OFF_MAX_KHZ : (2 * MaxKHz / 3);
        NV_ASSERT(s_Cpu1OnMinKHz < s_Cpu1OffMaxKHz);
    }

    /*
     * Request OS kernel to turn CPU1 Off if all of the following is true:
     * (a) CPU frequency is below OnMin threshold, 
     * (b) Last request was CPU1 On request
     * (c) CPU1 is actually On
     *
     * Request OS kernel to turn CPU1 On if all of the following is true:
     * (a) CPU frequency is above OffMax threshold 
     * (b) Last request was CPU1 Off request
     * (c) CPU1 is actually Off
     */
    if (CpuLoadGaugeKHz < s_Cpu1OnMinKHz)
    {
        s_Cpu1OnPendingCnt = 0;
        if (s_Cpu1OffPendingCnt < NVRM_CPU1_OFF_PENDING_CNT)
        {
            s_Cpu1OffPendingCnt++;
            return PmRequest;
        }
        if ((s_LastPmRequest & NvRmPmRequest_CpuOnFlag) && (!Cpu1Off))
        {
            s_LastPmRequest = PmRequest = (NvRmPmRequest_CpuOffFlag | 0x1);
#if NVRM_TEST_PMREQUEST_UP_MODE
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET_0,
            CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET_0_SET_CPURESET1_FIELD);
#endif
        }
        else if((s_LastPmRequest & NvRmPmRequest_CpuOffFlag) && (!Cpu1Off))
        {
            NV_DEBUG_PRINTF(("Cpu1 is still On when flag shows it is off\n"));
            s_LastPmRequest = PmRequest = (NvRmPmRequest_CpuOffFlag | 0x1);

        }
    }
    else if (CpuLoadGaugeKHz > s_Cpu1OffMaxKHz)
    {
        s_Cpu1OffPendingCnt = 0;
        if (s_Cpu1OnPendingCnt < NVRM_CPU1_ON_PENDING_CNT)
        {
            s_Cpu1OnPendingCnt++;
            return PmRequest;
        }
        if ((s_LastPmRequest & NvRmPmRequest_CpuOffFlag) && Cpu1Off)
        {
            s_LastPmRequest = PmRequest = (NvRmPmRequest_CpuOnFlag | 0x1);
            *pCpuKHz = NvRmPrivGetSocClockLimits(NvRmModuleID_Cpu)->MaxKHz;
        }
#if NVRM_TEST_PMREQUEST_UP_MODE
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR_0,
            CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR_0_CLR_CPURESET1_FIELD);
#endif
    }
    return PmRequest;
}
