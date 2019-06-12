/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "aos.h"
#include "nvbl_lp0.h"
#include "nvassert.h"
#include "nverror.h"
#include "nvodm_services.h"
#include "nvappmain.h"
#include "nvddk_blockdev.h"
#include "nvddk_se_blockdev.h"
#include "nvddk_se_core.h"
#include "nvaboot.h"
#include "fastboot.h"
#include "nvappmain.h"
#include "nvodm_pmu.h"
#include "nvodm_query.h"
#include "nvddk_blockdevmgr.h"
#include "nvodm_query_discovery.h"
#include "nvodm_pinmux_init.h"
#include "nvuart.h"
#include "nvrm_init.h"
#include "nvddk_keyboard.h"
#include "avp_gpio.h"

#ifdef TEGRA_11x_SOC
#define PINMUX_REG_SPACE 0x408
#elif defined (TEGRA_14x_SOC)
#define PINMUX_REG_SPACE 0x514
#else
#define PINMUX_REG_SPACE 0x3E0
#endif
#define PINMUX_PA_BASE   0x70003000

#include "arflow_ctlr.h"

#ifdef TEGRA_12x_SOC
#include "bootloader_t12x.h"
#include "t12x/arapbpm.h"
#endif
#ifdef TEGRA_14x_SOC
#include "bootloader_t14x.h"
#include "t14x/arapbpm.h"
#endif
#ifdef TEGRA_11x_SOC
#include "bootloader_t11x.h"
#include "t11x/arapbpm.h"
#endif
#ifdef TEGRA_3x_SOC
#include "arrtc.h"
#include "bootloader_t30.h"
#include "t30/arapbpm.h"
#endif

#define GET_ACTIVE_CLUSTER(x) \
    do  \
    { \
        x = NV_DRF_VAL(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, \
                FLOW_CTLR_READ(CLUSTER_CONTROL));  \
    }while (0)

#define PMC_READ(reg) \
    NV_REGR(s_hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0), \
                                                    APBDEV_PMC_##reg##_0)

#define PMC_WRITE(reg, data) \
    do \
    { \
        NV_REGW(s_hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0), \
                                              APBDEV_PMC_##reg##_0, data); \
    }while (0)

#define FLOW_CTLR_READ(reg) \
    NV_REGR(s_hRm, NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), \
                                                    FLOW_CTLR_##reg##_0)

#define FLOW_CTLR_WRITE(reg, data) \
    do \
    { \
        NV_REGW(s_hRm, NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), \
                                              FLOW_CTLR_##reg##_0, data); \
    }while (0)

static NvU32 s_IramSaveSize;
static NvU8 *s_IramSave;
static void *s_IramCode = (void *)TEGRA_IRAM_CODE_AREA;
static NvU8 *s_Bct;
static NvU32 *s_PinmuxDump;
static NvRmDeviceHandle s_hRm;

extern void NvBlMemoryControllerInit(NvBool IsChipInitialized);

/*

  Test Suite for lp0: The following are the basic test cases
  which the developer should test if any changes are made to
  the code. Please add to this test suite if you see any test
  case not being covered

  * Basic Suspend/Resume functionality by enabling BASIC_LP0_TEST
  * Test Suspend/Resume on all the configured wake events
  * Wake up the cluster which is being configured

*/

static void NvBlLp0ConfigurePMC()
{
    NvU32 Reg = 0;

    Reg = PMC_READ(CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRREQ_OE, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, PWRREQ_OE, ENABLE, Reg);

#ifdef TEGRA_11x_SOC
    // disable CPUPWRGOOD_EN
    // this is needed only for T11x, not for other chips
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRGOOD_EN, DISABLE, Reg);
#endif

    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, SIDE_EFFECT_LP0, ENABLE, Reg);
    PMC_WRITE(CNTRL, Reg);

#ifdef TEGRA_3x_SOC
    /* configure dpd pads oride */
    Reg = PMC_READ(DPD_PADS_ORIDE);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, DPD_PADS_ORIDE, BLINK, ENABLE, Reg);
    PMC_WRITE(DPD_PADS_ORIDE, Reg);
#endif

    /* Enable DPD sample to trigger sampling pads data and direction
       in which pad will be driven during lp0 mode */
    PMC_WRITE(DPD_SAMPLE, 0x01);
}

//FIXME: can this be moved to keyboard driver open call
static void NvBlLp0ConfigureKBC()
{
    NvDdkLp0ConfigureKeyboard();
}

#ifdef TEGRA_3x_SOC
static void NvBlLp0ConfigRTC()
{
    NvU32 Reg = 1;

    while(NV_REGR(s_hRm, NVRM_MODULE_ID(NvRmModuleID_Rtc, 0),
                APBDEV_RTC_BUSY_0));

    //enable RTC alarm
    Reg = NV_FLD_SET_DRF_NUM (APBDEV_RTC, INTR_MASK, MSEC_CDN_ALARM, 0x1, Reg);

    NV_REGW(s_hRm, NVRM_MODULE_ID(NvRmModuleID_Rtc, 0), \
            APBDEV_RTC_INTR_MASK_0, Reg);

       while(NV_REGR(s_hRm, NVRM_MODULE_ID(NvRmModuleID_Rtc, 0),
                APBDEV_RTC_BUSY_0));

    //set up RTC alarm
    Reg = 0;
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_RTC, MILLI_SECONDS_COUNTDOWN_ALARM, ENABLE, ENABLED, Reg);
    Reg = NV_FLD_SET_DRF_NUM(APBDEV_RTC, MILLI_SECONDS_COUNTDOWN_ALARM, VALUE, 120000, Reg);

    NV_REGW(s_hRm, NVRM_MODULE_ID(NvRmModuleID_Rtc, 0), \
            APBDEV_RTC_MILLI_SECONDS_COUNTDOWN_ALARM_0, Reg);
}

static void NvBlLp0ClearRTCInterrupt()
{
     NvU32 regVal = 1;
   //check if RTC is busy before writing to it

     while(NV_REGR(s_hRm, NVRM_MODULE_ID(NvRmModuleID_Rtc, 0),
           APBDEV_RTC_BUSY_0));

     regVal = NV_FLD_SET_DRF_NUM (APBDEV_RTC, INTR_STATUS, MSEC_CDN_ALARM, 0x1, regVal);

     NV_REGW(s_hRm, NVRM_MODULE_ID(NvRmModuleID_Rtc, 0), \
             APBDEV_RTC_INTR_STATUS_0, regVal);
}
#endif

static void NvBlLp0ConfigureCLK()
{
#ifdef TEGRA_11x_SOC
    NV_REGW(s_hRm, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            CLK_RST_CONTROLLER_CLK_ENB_H_SET_0, 0XFFDDFFF7);
    NV_REGW(s_hRm, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0, 0XFFDDFFF7);
#endif
}

static void NvBlLp0ConfigureScratchRegs(NvU32 WarmBootAddr)
{
    NvU32 Reg = 0;
    NvU32 WarmBootFlag = 0;
    NvU32 ActiveCluster = 0;

    Reg = PMC_READ(SCRATCH4);
    GET_ACTIVE_CLUSTER(ActiveCluster);

    /* always wake up on the cluster which we are running */
    if (ActiveCluster == 1)
    {
        NvOsDebugPrintf("Bootloader Suspend: Configure wake up on LP cluster\n");
        Reg |= (1 << 31); /* set to LP-cluster */
    }
    else if (ActiveCluster == 0)
    {
        NvOsDebugPrintf("Bootloader Suspend: Configure wake up on G cluster\n");
        Reg &= ~(1 << 31); /* set to G-cluster */
    }
    else
        NV_ASSERT(0);
    PMC_WRITE(SCRATCH4, Reg);

    /* Scratch 39 */
    PMC_WRITE(SCRATCH39, 0x00);

    /* Scratch 0 - Set warmbootflag */
    WarmBootFlag = PMC_READ(SCRATCH0);
    WarmBootFlag = WarmBootFlag | 1 ;
    PMC_WRITE(SCRATCH0, WarmBootFlag);

    /* Scratch 1 - Set warmboot entry address */
    PMC_WRITE(SCRATCH1, WarmBootAddr);

#ifndef CONFIG_TRUSTED_FOUNDATIONS
    /* Scratch 41 - Set NvBlLp0CoreResume address */
    PMC_WRITE(SCRATCH41, (NvU32)NvBlLp0CoreResume);
#endif
}

static void NvBlLp0ConfigureFlowCtrl()
{
    NvU32 Reg = 0;
    NvU32 ActiveCluster = 0;

    //configure cpu_csr register
    Reg = FLOW_CTLR_READ(CPU_CSR);

    Reg |= FLOW_CTLR_CSR_INTR_FLAG;     /* clear intr flag */
    Reg |= FLOW_CTLR_CSR_EVENT_FLAG;    /* clear event flag */
    Reg |= FLOW_CTLR_CSR_ENABLE;        /* enable power gating */

#ifdef TEGRA_11x_SOC
    GET_ACTIVE_CLUSTER(ActiveCluster);
    /* when boot using LP cluster */
    if (ActiveCluster == 1)
        Reg |= FLOW_CTLR_CSR_ENABLE_EXT_NCPU;
    /* when boot using G cluster */
    else if (ActiveCluster == 0)
        Reg |= FLOW_CTLR_CSR_ENABLE_EXT_CRAIL;
    else
        NV_ASSERT(0);
#endif

    Reg &= ~FLOW_CTLR_CSR_WFI_BITMAP;   /* clear wfi bitmap */
    Reg |= FLOW_CTLR_CSR_WFI_CPU0 << 0;   /* enable power gating on wfi */

    FLOW_CTLR_WRITE(CPU_CSR, Reg);

#ifdef TEGRA_3x_SOC
    //FIXME: why not this configuration for T114
    Reg = FLOW_MODE_STOP_UNTIL_IRQ;
    Reg |= FLOW_CTLR_HALT_IRQ_0;
    Reg |= FLOW_CTLR_HALT_IRQ_1;
    FLOW_CTLR_WRITE(HALT_CPU_EVENTS, Reg);
#endif
}

static void NvBlLp0ConfigureWakeEvents()
{
    NvU32 wake_mask = 0;
    NvU32 wake_level = 0;
    NvU32 Reg = 0;

    NvOdmQueryLP0WakeConfig(&wake_mask, &wake_level);

    // Configure wake mask
    PMC_WRITE(WAKE_MASK, wake_mask);

    // Configure wake level
    PMC_WRITE(WAKE_LVL, wake_level);
}

NvError NvBlLp0StartSuspend(NvAbootHandle hAboot)
{
    NvError e = NvError_Success;
    NvU32 Reg;
    void *ptr = NULL;
    NvU64 WarmBootAddr = 0;
    NvU64 WarmBootSize = 0;
    NvU32 IramShutDownCodeStart = (NvU32)&g_NvBlLp0IramStart;
    NvU32 IramShutDownCodeEnd = (NvU32)&g_NvBlLp0IramEnd;
    NvU32 ActiveCluster = 0;

    NvOsDebugPrintf("Bootloader Suspend: start suspend\n");
    NvRmOpen(&s_hRm, 0);

    NvOsDebugPrintf("Bootloader Suspend: suspend se\n");
    NV_CHECK_ERROR_CLEANUP(NvDdkSeSuspend());

    if (!NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_lp0_vec,
                &WarmBootAddr, &WarmBootSize))
    {
        NvOsDebugPrintf("Bootloader Suspend: NvAbootMemLayoutBaseSize failed\n");
        e = NvError_InvalidState;
        goto fail;
    }

    NvBlLp0ConfigureCLK();

    NvBlLp0ConfigureKBC();

#ifdef TEGRA_3x_SOC
    NvBlLp0ConfigRTC();
#endif

    NvOsDebugPrintf("Bootloader Suspend: shutdown all modules\n");

    NvOdmPmuDeviceSuspend();

    NvDdkBlockDevMgrDeinit();

    NvOdmPmuI2cShutDown();

    /* Check active cluster */
    GET_ACTIVE_CLUSTER(ActiveCluster);
    NvOsDebugPrintf("Bootloader Suspend: Enter suspend on %s cluster\n",
            (ActiveCluster == 1) ? "LP" : "G");

    /* store the bct from iram */
    s_Bct = NvOsAlloc(TEGRA_IRAM_BCT_SPACE);
    ptr = s_Bct;
    NvOsMemcpy(ptr, (const void *)TEGRA_IRAM_BASE, TEGRA_IRAM_BCT_SPACE);

    /* store the pinmux */
    s_PinmuxDump = NvOsAlloc(PINMUX_REG_SPACE);
    ptr = s_PinmuxDump;
    NvOsMemcpy(ptr, (const void *)PINMUX_PA_BASE, PINMUX_REG_SPACE);

    /* store the gpio */
    NvAvpGpioSuspend();

    s_IramSaveSize = IramShutDownCodeEnd - IramShutDownCodeStart;
    s_IramSave = NvOsAlloc(s_IramSaveSize);

    NvBlLp0ConfigureWakeEvents();

    /* Save reset vector and SDRAM shutdown code to IRAM */
    /* Reset vector being saved to IRAM is 0x00. And where is it stored in IRAM?*/
    /* How to generate the shutdown code ? */
    NvOsMemcpy(s_IramSave, s_IramCode, s_IramSaveSize);
    NvOsMemcpy(s_IramCode, (void *)IramShutDownCodeStart, s_IramSaveSize);

    NvBlLp0ConfigurePMC();

    NvBlLp0ConfigureScratchRegs(WarmBootAddr);

#if !defined(TEGRA_3x_SOC)
    PMC_WRITE(IO_DPD_REQ, 0x800fdfff);
    PMC_WRITE(IO_DPD2_REQ, 0x80001fff);
#endif

#ifdef TEGRA_11x_SOC
    // core_timer
    PMC_WRITE(PWRGOOD_TIMER,0x157e);
    // core_off_timer
    PMC_WRITE(WAKE_DELAY, 2000);
#elif TEGRA_3x_SOC
    // core_timer
    PMC_WRITE(PWRGOOD_TIMER,0x7e7e);
    // core_off_timer
    PMC_WRITE(WAKE_DELAY, 0x80);
#endif

    NvBlLp0ConfigureFlowCtrl();

#if DEBUG_TEST_PWR_GATE
    PMC_WRITE(TEST_PWRGATE, 0x18);
#endif

    NvRmShutdown();

    NvBlLp0CoreSuspend();

    /* control shouldn't reach here */
    NV_ASSERT(0);

fail:
    return e;
}


void NvBlLp0StartResume()
{
    void *ptr = NULL;
    NvU32 Reg = 0;
    NvU32 ActiveCluster = 0;

    nvaos_ConfigureCpu();
    nvaos_ConfigureA15();

    nvaos_ConfigureInterruptController();
    nvaos_EnableTimer();

    nvaosEnableMMU((NvU32)nvaos_GetFirstLevelPageTableAddr());

    /* restore bct to iram */
    ptr = s_Bct;
    NvOsMemcpy((void *)TEGRA_IRAM_BASE, ptr, TEGRA_IRAM_BCT_SPACE);

    NvBlMemoryControllerInit(NV_TRUE);

    /* restore pinmux */
    ptr = s_PinmuxDump;
    NvOsMemcpy((void *)PINMUX_PA_BASE, ptr, PINMUX_REG_SPACE);

    /* restore gpio */
    NvAvpGpioResume();

    nvaosTimerInit();

    /* uart */
    NvAvpUartInit(PLLP_FIXED_FREQ_KHZ_408000);

    /* FIXME: pmc_read/write should be done only after rmopen
    but calling rmopen in the beginning is causing crash */
    /* clear DPD sample */
    PMC_WRITE(DPD_SAMPLE, 0x00);

    /* clear scratch registers shared by suspend and the reset pen */
#ifndef CONFIG_TRUSTED_FOUNDATIONS
    PMC_WRITE(SCRATCH41, 0x0);
#endif

#if !defined(TEGRA_3x_SOC)
    PMC_WRITE(IO_DPD_REQ, 0x400FFFFF);
    PMC_WRITE(IO_DPD2_REQ, 0x40001fff);
#endif

    //Verify for wake up on LP0
    Reg = PMC_READ(RST_STATUS);
    NV_ASSERT(Reg & PMC_RST_SOURCE_LP0);
    NvOsDebugPrintf("\nBootloader Resume: start resume\n");

#ifdef TEGRA_3x_SOC
    NvBlLp0ClearRTCInterrupt();
#endif

    nvaosEnableInterrupts();

    NvOsDebugPrintf("Bootloader Resume: init ddk drivers\n");

    Reg = PMC_READ(WAKE_STATUS);
    switch (Reg)
    {
        case (1 << NvOdmLP0WakeEvent_kbc_interrupt):
            NvOsDebugPrintf("Bootloader Resume: Woke up on Kbc event\n");
            break;
        case (1 << NvOdmLP0WakeEvent_rtc_irq):
            NvOsDebugPrintf("Bootloader Resume: Woke up on Rtc event\n");
            break;
        case (1 << NvOdmLP0WakeEvent_usb_vbus_wakeup_0):
            NvOsDebugPrintf("Bootloader Resume: Woke up on usb vbus event\n");
            break;
        default:
            NvOsDebugPrintf("Bootloader Resume: Woke up on unknown event 0x%x\n", Reg);
    }


    GET_ACTIVE_CLUSTER(ActiveCluster);

    if (ActiveCluster == 1)
        NvOsDebugPrintf("Bootloader Resume: Woke up on LP cluster\n");
    else if (ActiveCluster == 0)
        NvOsDebugPrintf("Bootloader Resume: Woke up on G cluster\n");
    else
        NV_ASSERT(0);

    NvDdkBlockDevMgrInit();

#ifndef TEGRA_14x_SOC
    SetAvpClockToClkM();
#endif

    // Resume kbc to enable power and clock
    NvDdkLp0KeyboardResume();
    NvDdkKeyboardDeinit();
    NvAppResume();
}


