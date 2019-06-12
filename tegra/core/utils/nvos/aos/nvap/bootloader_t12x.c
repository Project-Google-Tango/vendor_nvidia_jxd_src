/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "bootloader_t12x.h"
#include "bootloader.h"
#include "nvassert.h"
#include "aos.h"
#include "nvuart.h"
#include "nvddk_i2c.h"
#include "aos_avp_pmu.h"
#include "aos_avp_board_info.h"
#include "nvbct_customer_platform_data.h"
#include "nvbct.h"
#include "nvsdmmc.h"
#include "nvodm_pinmux_init.h"
#include "nvbl_query.h"

#if !__GNUC__
#pragma arm
#endif

#define PMC_CLAMPING_CMD(x)   \
    (NV_DRF_DEF(APBDEV_PMC, REMOVE_CLAMPING_CMD, x, ENABLE))
#define PMC_PWRGATE_STATUS(x) \
    (NV_DRF_DEF(APBDEV_PMC, PWRGATE_STATUS, x, ON))
#define PMC_PWRGATE_TOGGLE(x) \
    (NV_DRF_DEF(APBDEV_PMC, PWRGATE_TOGGLE, PARTID, x) | \
     NV_DRF_DEF(APBDEV_PMC, PWRGATE_TOGGLE, START, ENABLE))

extern NvU32 *g_ptimerus;
extern NvU32 g_BlAvpTimeStamp;
extern NvU32 s_enablePowerRail;
extern NvU32 e_enablePowerRail;
extern NvU32 proc_tag;
extern NvU32 cpu_boot_stack;
extern NvU32 entry_point;
extern NvU32 avp_boot_stack;
extern NvU32 deadbeef;
extern NvBoardInfo g_BoardInfo;

static NvBlCpuClusterId NvBlAvpQueryBootCpuClusterId(void)
{
    NvU32   Reg;            // Scratch register
    NvU32   Fuse;           // Fuse register

    // Read the fuse register containing the CPU capabilities.
    Fuse = NV_FUSE_REGR(FUSE_PA_BASE, SKU_DIRECT_CONFIG);

    // Read the bond-out register containing the CPU capabilities.
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, BOND_OUT_V);

    // Are there any CPUs present in the fast CPU complex?
    if ((!NV_DRF_VAL(CLK_RST_CONTROLLER, BOND_OUT_V, BOND_OUT_CPUG, Reg))
     && (!NV_DRF_VAL(FUSE, SKU_DIRECT_CONFIG, DISABLE_ALL, Fuse)))
    {
        return NvBlCpuClusterId_Fast;
    }

    // Is there a CPU present in the slow CPU complex?
    if (!NV_DRF_VAL(CLK_RST_CONTROLLER, BOND_OUT_V, BOND_OUT_CPULP, Reg))
    {
        return NvBlCpuClusterId_Slow;
    }

    return NvBlCpuClusterId_Unknown;
}

static NvBool NvBlIsValidBootRomSignature(const NvBootInfoTable*  pBootInfo)
{
    // Look at the beginning of IRAM to see if the boot ROM placed a Boot
    // Information Table (BIT) there.
    if (((pBootInfo->BootRomVersion == NVBOOT_VERSION(0x40, 1))
      || (pBootInfo->BootRomVersion == NVBOOT_VERSION(0x40, 2)))
    &&   (pBootInfo->DataVersion    == NVBOOT_VERSION(0x40, 1))
    &&   (pBootInfo->RcmVersion     == NVBOOT_VERSION(0x40, 1))
    &&   (pBootInfo->BootType       == NvBootType_Cold)
    &&   (pBootInfo->PrimaryDevice  == NvBootDevType_Irom))
    {
        // There is a valid Boot Information Table.
        return NV_TRUE;
    }

    return NV_FALSE;
}

static NvBool NvBlAvpIsChipInitialized( void )
{
    // Pointer to the start of IRAM which is where the Boot Information Table
    // will reside (if it exists at all).
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T12X_BASE_PA_BOOT_INFO);

    // Look at the beginning of IRAM to see if the boot ROM placed a Boot
    // Information Table (BIT) there.
    if (NvBlIsValidBootRomSignature(pBootInfo))
    {
        // There is a valid Boot Information Table.
        // Return status the boot ROM gave us.
        return pBootInfo->DevInitialized;
    }

    return NV_FALSE;
}

static NvBool NvBlAvpIsChipInRecovery( void )
{
    // Pointer to the start of IRAM which is where the Boot Information Table
    // will reside (if it exists at all).
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T12X_BASE_PA_BOOT_INFO);

    // Look at the beginning of IRAM to see if the boot ROM placed a Boot
    // Information Table (BIT) there.
    if (((pBootInfo->BootRomVersion == NVBOOT_VERSION(0x40, 1))
      || (pBootInfo->BootRomVersion == NVBOOT_VERSION(0x40, 2)))
    &&   (pBootInfo->DataVersion    == NVBOOT_VERSION(0x40, 1))
    &&   (pBootInfo->RcmVersion     == NVBOOT_VERSION(0x40, 1))
    &&   (pBootInfo->BootType       == NvBootType_Recovery)
    &&   (pBootInfo->PrimaryDevice  == NvBootDevType_Irom))
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

static NvU32 NvBlGetOscillatorDriveStrength( void )
{
    // Setting this value to default now.
    // This value can be changed based on characterization.
    return 0x01;
}

static NvU32  NvBlAvpQueryBootCpuFrequency( void )
{
    NvU32   frequency;

    if (s_FpgaEmulation)
    {
        frequency = NvBlAvpQueryOscillatorFrequency() / 1000;
    }
    else
    {
        #if     NVBL_PLL_BYPASS
            // In bypass mode we run at the oscillator frequency.
            frequency = NvBlAvpQueryOscillatorFrequency();
        #else
            // !!!FIXME!!! ADD SKU VARIANT FREQUENCIES
            if (NvBlAvpQueryFlowControllerClusterId() == NvBlCpuClusterId_Fast)
            {
                frequency = 700;
            }
            else
            {
                frequency = 350;
            }
        #endif
    }

    return frequency;
}

static NvU32  NvBlAvpQueryOscillatorFrequency( void )
{
    NvU32               Reg;
    NvU32               Oscillator;
    NvBootClocksOscFreq Freq;

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, OSC_CTRL);
    Freq = (NvBootClocksOscFreq)NV_DRF_VAL(CLK_RST_CONTROLLER, OSC_CTRL, OSC_FREQ, Reg);

    switch (Freq)
    {
        case NvBootClocksOscFreq_13:
            Oscillator = 13000;
            break;

        case NvBootClocksOscFreq_19_2:
            Oscillator = 19200;
            break;

        case NvBootClocksOscFreq_12:
            Oscillator = 12000;
            break;

        case NvBootClocksOscFreq_26:
            Oscillator = 26000;
            break;

        case NvBootClocksOscFreq_16_8:
            Oscillator = 16800;
            break;

        case NvBootClocksOscFreq_38_4:
            Oscillator = 38400;
            break;

        case NvBootClocksOscFreq_48:
            Oscillator = 48000;
            break;

        default:
            NV_ASSERT(0);
            Oscillator = 0;
            break;
    }

    return Oscillator;
}

static NvBootClocksOscFreq NvBlAvpOscillatorCountToFrequency(NvU32 Count)
{
    if ((Count >= NVBOOT_CLOCKS_MIN_CNT_12) && (Count <= NVBOOT_CLOCKS_MAX_CNT_12))
    {
        return NvBootClocksOscFreq_12;
    }

    if ((Count >= NVBOOT_CLOCKS_MIN_CNT_13) && (Count <= NVBOOT_CLOCKS_MAX_CNT_13))
    {
        return NvBootClocksOscFreq_13;
    }

    if ((Count >= NVBOOT_CLOCKS_MIN_CNT_19_2) && (Count <= NVBOOT_CLOCKS_MAX_CNT_19_2))
    {
        return NvBootClocksOscFreq_19_2;
    }

    if ((Count >= NVBOOT_CLOCKS_MIN_CNT_26) && (Count <= NVBOOT_CLOCKS_MAX_CNT_26))
    {
        return NvBootClocksOscFreq_26;
    }

    if ((Count >= NVBOOT_CLOCKS_MIN_CNT_16_8) && (Count <= NVBOOT_CLOCKS_MAX_CNT_16_8))
    {
        return NvBootClocksOscFreq_16_8;
    }

    if ((Count >= NVBOOT_CLOCKS_MIN_CNT_38_4) && (Count <= NVBOOT_CLOCKS_MAX_CNT_38_4))
    {
        return NvBootClocksOscFreq_38_4;
    }

    if ((Count >= NVBOOT_CLOCKS_MIN_CNT_48) && (Count <= NVBOOT_CLOCKS_MAX_CNT_48))
    {
        return NvBootClocksOscFreq_48;
    }

    return NvBootClocksOscFreq_Unknown;
}

static void NvBlAvpEnableCpuClock(void)
{
    NvU32   Reg;        // Scratch reg

    #if NVBL_USE_PLL_LOCK_BITS && !defined(QT_EMUL)
    // NvBlAvpEnableCpuClock hangs when
    // waiting for PLLX to lock on QT

    // Wait for PLL-X to lock.
    do
    {
        Reg = NV_CAR_REGR(CLK_RST_PA_BASE, PLLX_BASE);
    } while (!NV_DRF_VAL(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_LOCK, Reg));

    #else

    // Give PLLs time to stabilize.
    NvBlAvpStallUs(NVBOOT_CLOCKS_PLL_STABILIZATION_DELAY);

    #endif

    //*((volatile long *)0x60006020) = 0x20008888 ;// CCLK_BURST_POLICY
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CPU_STATE, 0x2)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, COP_AUTO_CWAKEUP_FROM_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CPU_AUTO_CWAKEUP_FROM_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, COP_AUTO_CWAKEUP_FROM_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CPU_AUTO_CWAKEUP_FROM_IRQ, 0x0)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CWAKEUP_FIQ_SOURCE, PLLX_OUT0_LJ)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CWAKEUP_IRQ_SOURCE, PLLX_OUT0_LJ)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CWAKEUP_RUN_SOURCE, PLLX_OUT0_LJ)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CWAKEUP_IDLE_SOURCE, PLLX_OUT0_LJ);
    NV_CAR_REGW(CLK_RST_PA_BASE, CCLK_BURST_POLICY, Reg);

    //*((volatile long *)0x60006024) = 0x80000000 ;// SUPER_CCLK_DIVIDER
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_ENB, 0x1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIS_FROM_COP_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIS_FROM_CPU_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIS_FROM_COP_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIS_FROM_CPU_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIVIDEND, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIVISOR, 0x0);
    NV_CAR_REGW(CLK_RST_PA_BASE, SUPER_CCLK_DIVIDER, Reg);

    // Always enable the main CPU complex clock.
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_ENB_L_SET, SET_CLK_ENB_CPU, 1);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_ENB_L_SET, Reg);
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_ENB_V_SET, SET_CLK_ENB_CPULP, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_ENB_V_SET, SET_CLK_ENB_CPUG, 1);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_ENB_V_SET, Reg);
}

static void NvBlAvpRemoveCpuReset(void)
{
     NvU32   Reg;    // Scratch reg

    // Take the slow non-CPU partition out of reset.
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPULP_CMPLX_CLR, CLR_NONCPURESET, 1);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_CPULP_CMPLX_CLR, Reg);

    // Take the fast non-CPU partition out of reset.
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_NONCPURESET, 1);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_CPUG_CMPLX_CLR, Reg);

    if (s_FpgaEmulation)
    {
         // make SOFTRST_CTRL2 equal to SOFTRST_CTRL1, this gurantees C0NC/C1NC is
         // always removed out of reset earlier than CE0/CELP
         Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CPU_SOFTRST_CTRL2);
         NV_CAR_REGW(CLK_RST_PA_BASE, CPU_SOFTRST_CTRL1, Reg);
    }

    // Clear software controlled reset of slow cluster
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPULP_CMPLX_CLR, CLR_CPURESET0, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPULP_CMPLX_CLR, CLR_DBGRESET0, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPULP_CMPLX_CLR, CLR_CORERESET0,1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPULP_CMPLX_CLR, CLR_CXRESET0,  1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPULP_CMPLX_CLR, CLR_L2RESET,   1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPULP_CMPLX_CLR, CLR_PRESETDBG, 1);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_CPULP_CMPLX_CLR, Reg);

    // Clear software controlled reset of fast cluster
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CPURESET0, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_DBGRESET0, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CORERESET0,1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CXRESET0,  1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CPURESET1, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_DBGRESET1, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CORERESET1,1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CXRESET1,  1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CPURESET2, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_DBGRESET2, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CORERESET2,1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CXRESET2,  1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CPURESET3, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_DBGRESET3, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CORERESET3,1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CXRESET3,  1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_L2RESET,   1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_PRESETDBG, 1);

    NV_CAR_REGW(CLK_RST_PA_BASE, RST_CPUG_CMPLX_CLR, Reg);
}

static void NvBlAvpClockEnableCoresight(void)
{
    NvU32   Reg;        // Scratch register
    const NvBootInfoTable *pBit;
    pBit = (const NvBootInfoTable *)(NV_BIT_ADDRESS);

    if (pBit->BctPtr->SecureJtagControl == 0)
    {
       // Is this an FPGA?
       if (s_FpgaEmulation)
       {
          // Leave CoreSight on the default clock source (oscillator).
       }
       else
       {
          // Program the CoreSight clock source and divider (but not at the same time).
          // Note that CoreSight has a fractional divider (LSB == .5).
          Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_SOURCE_CSITE);

          // Clock divider request for 136MHz would setup CSITE clock as
          // 144MHz for PLLP base 216MHz and 136MHz for PLLP base 408MHz.
          Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_CSITE,
                 CSITE_CLK_DIVISOR, CLK_DIVIDER(NVBL_PLLP_KHZ, 136000), Reg);
          NV_CAR_REGW(CLK_RST_PA_BASE, CLK_SOURCE_CSITE, Reg);

          NvBlAvpStallUs(3);

          Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_CSITE,
                 CSITE_CLK_SRC, PLLP_OUT0, Reg);
          NV_CAR_REGW(CLK_RST_PA_BASE, CLK_SOURCE_CSITE, Reg);
       }

       // Enable CoreSight clock and take the module out of reset in case
       // the clock is not enabled or the module is being held in reset.
       Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_ENB_U_SET, SET_CLK_ENB_CSITE, 1);
       NV_CAR_REGW(CLK_RST_PA_BASE, CLK_ENB_U_SET, Reg);

       Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_U_CLR, CLR_CSITE_RST, 1);
       NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEV_U_CLR, Reg);
    }
}

void SetAvpClockToClkM(void)
{
    /** This is a stub function.
          this is actually called by nvbl_lp0. BL Lp0 is
          not enabled yet. It will be implemented if needed. **/
}
//----------------------------------------------------------------------------------------------
static void InitPllX(NvU32 PllRefDiv)
{
    NvU32               Base;       // PLL Base
    NvU32               Divm;       // Reference
    NvU32               Divn;       // Multiplier
    NvU32               Divp;       // Divider == Divp ^ 2
    NvU32               p;
    NvU32               Val;
    NvU32               OscFreqKhz, RateInKhz, VcoMin = 700000, RefPllKhz;

    // Is PLL-X already running?
    Base = NV_CAR_REGR(CLK_RST_PA_BASE, PLLX_BASE);
    Base = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, Base);
    if (Base == NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, ENABLE))
    {
        return;
    }

    OscFreqKhz = NvBlAvpQueryOscillatorFrequency();
    NV_ASSERT(OscFreqKhz); // Invalid frequency
    RefPllKhz = OscFreqKhz >> PllRefDiv;

    RateInKhz = NvBlAvpQueryBootCpuFrequency() * 1000;

    if (s_FpgaEmulation)
    {
        OscFreqKhz = 13000; // 13 Mhz for FPGA
        RateInKhz = 600000; // 600 Mhz
        RefPllKhz = 13000;
    }

    // First, disable IDDQ
    Val = NV_CAR_REGR(CLK_RST_PA_BASE, PLLX_MISC_3);
    Val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLX_MISC_3, PLLX_IDDQ, 0x0, Val);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_MISC_3, Val);

    // Wait 2 us
    NvBlAvpStallUs(2);

    /* Clip vco_min to exact multiple of OscFreq to avoid
       crossover by rounding */
    VcoMin = ((VcoMin + RefPllKhz- 1) / RefPllKhz)*RefPllKhz;

    p = (VcoMin + RateInKhz - 1)/RateInKhz; // Rounding p
    Divp = p - 1;
    Divm = (RefPllKhz> 19200)? 2 : 1; // cf_max = 19200 Khz
    Divn = (RateInKhz * p * Divm)/RefPllKhz;

    Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, ENABLE)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, DISABLE)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_REF_DIS, REF_ENABLE)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_LOCK, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP, Divp)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, Divn)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM, Divm);

    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_BASE, Base);

#if !NVBL_PLL_BYPASS
        Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, DISABLE, Base);
        NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_BASE, Base);
#if NVBL_USE_PLL_LOCK_BITS
        {
            NvU32 Misc = NV_CAR_REGR(CLK_RST_PA_BASE, PLLX_MISC);
            Misc = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLX_MISC, PLLX_LOCK_ENABLE, ENABLE, Misc);
            NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_MISC, Misc);
        }
#endif
#endif
    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, ENABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_BASE, Base);
}

//----------------------------------------------------------------------------------------------
static void NvBlAvpClockInit(NvBool IsChipInitialized, NvBool IsLoadedByScript)
{
    const NvBootInfoTable*  pBootInfo;      // Boot Information Table pointer
    NvBootClocksOscFreq     OscFreq;        // Oscillator frequency
    NvU32                   Reg;            // Temporary register
    NvU32                   OscCtrl;        // Oscillator control register
    NvU32                   OscStrength;    // Oscillator Drive Strength
    NvU32                   UsecCfg;        // Usec timer configuration register
    NvU32                   PllRefDiv;      // PLL reference divider
    NvBlCpuClusterId        CpuClusterId;   // Boot CPU id

    // Get a pointer to the Boot Information Table.
    pBootInfo = (const NvBootInfoTable*)(T12X_BASE_PA_BOOT_INFO);

    // Get the oscillator frequency.
    OscFreq = pBootInfo->OscFrequency;

    // For most oscillator frequencies, the PLL reference divider is 1.
    // Frequencies that require a different reference divider will set
    // it below.
    PllRefDiv = CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV1;

    // Set up the oscillator dependent registers.
    // NOTE: Don't try to replace this switch statement with an array lookup.
    //       Can't use global arrays here because the data segment isn't valid yet.
    switch (OscFreq)
    {
        default:
            // Fall Through -- this is what the boot ROM does.
        case NvBootClocksOscFreq_13:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (1-1))
                | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (13-1));
            break;

        case NvBootClocksOscFreq_19_2:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (5-1))
                | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (96-1));
            break;

        case NvBootClocksOscFreq_12:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (1-1))
                | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (12-1));
            break;

        case NvBootClocksOscFreq_26:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (1-1))
                | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (26-1));
            break;

        case NvBootClocksOscFreq_16_8:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (5-1))
                | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (84-1));
            break;

        case NvBootClocksOscFreq_38_4:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (5-1))
                | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (192-1));
            PllRefDiv = CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV2;
            break;

        case NvBootClocksOscFreq_48:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (1-1))
                | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (48-1));
            PllRefDiv = CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV4;
            break;
    }

    // Find out which CPU we're booting to.
    CpuClusterId = NvBlAvpQueryBootCpuClusterId();

    Reg = NV_FLOW_REGR(FLOW_PA_BASE, CLUSTER_CONTROL);
    // Are we booting to slow CPU complex?
    if (CpuClusterId == NvBlCpuClusterId_Slow)
    {
        // Set active cluster to the slow cluster.
        Reg = NV_FLD_SET_DRF_DEF(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, LP, Reg);
    }
    else
    {
        // Set active cluster to the fast cluster.
        Reg = NV_FLD_SET_DRF_DEF(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, G, Reg);
    }
    NV_FLOW_REGW(FLOW_PA_BASE, CLUSTER_CONTROL, Reg);

    // Running on an FPGA?
    if (s_FpgaEmulation)
    {
        // Increase the reset delay time to maximum for FPGAs to avoid
        // race conditions between WFI and reset propagation due to
        // delays introduced by serializers.
        Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CPU_SOFTRST_CTRL);
        Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CPU_SOFTRST_CTRL,
                CPU_SOFTRST_LEGACY_WIDTH, 0xF0, Reg);
        NV_CAR_REGW(CLK_RST_PA_BASE, CPU_SOFTRST_CTRL, Reg);
    }

    // Get Oscillator Drive Strength Setting
    OscStrength = NvBlGetOscillatorDriveStrength();

    if (!IsChipInitialized)
    {
        // Program the microsecond scaler.
        NV_TIMERUS_REGW(TIMERUS_PA_BASE, USEC_CFG, UsecCfg);
        // Program the oscillator control register.
        OscCtrl = NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_CTRL, OSC_FREQ, (int)OscFreq)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_CTRL, PLL_REF_DIV, PllRefDiv)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_CTRL, OSCFI_SPARE, 0)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_CTRL, XODS, 0)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_CTRL, XOFS, OscStrength)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_CTRL, XOE, 1);
        NV_CAR_REGW(CLK_RST_PA_BASE, OSC_CTRL, OscCtrl);
    }
    else
    {
        // Change the oscillator drive strength
        Reg = NV_CAR_REGR(CLK_RST_PA_BASE, OSC_CTRL);
        Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, OSC_CTRL, XOFS, OscStrength, Reg);
        NV_CAR_REGW(CLK_RST_PA_BASE, OSC_CTRL, Reg);
        // Update same value in PMC_OSC_EDPD_OVER XOFS field for warmboot
        Reg = NV_PMC_REGR(PMC_PA_BASE, OSC_EDPD_OVER);
        Reg = NV_FLD_SET_DRF_NUM(APBDEV_PMC, OSC_EDPD_OVER, XOFS, OscStrength, Reg);
        NV_PMC_REGW(PMC_PA_BASE, OSC_EDPD_OVER, Reg);

        // Copy CTRL_SELECT to PMC for warmboot
        Reg = NV_PMC_REGR(PMC_PA_BASE, OSC_EDPD_OVER);
        Reg = NV_FLD_SET_DRF_NUM(APBDEV_PMC, OSC_EDPD_OVER, OSC_CTRL_SELECT, 1, Reg);
        NV_PMC_REGW(PMC_PA_BASE, OSC_EDPD_OVER, Reg);

        // Set HOLD_CKE_LOW_EN to 1
        Reg = NV_PMC_REGR(PMC_PA_BASE, CNTRL2);
        Reg = NV_FLD_SET_DRF_NUM(APBDEV_PMC, CNTRL2, HOLD_CKE_LOW_EN, 1, Reg);
        NV_PMC_REGW(PMC_PA_BASE, CNTRL2, Reg);
    }

    // Initialize PLL-X.
    InitPllX(PllRefDiv);

    //*((volatile long *)0x60006030) = 0x00000010 ;// CLK_SYSTEM_RATE
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SYSTEM_RATE, HCLK_DIS, 0x0)
#if NVBL_PLL_BYPASS
    | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SYSTEM_RATE, AHB_RATE, 0x0)
#else
    | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SYSTEM_RATE, AHB_RATE, 0x1)
#endif
    | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SYSTEM_RATE, PCLK_DIS, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SYSTEM_RATE, APB_RATE, 0x0);
        NV_CAR_REGW(CLK_RST_PA_BASE, CLK_SYSTEM_RATE, Reg);

    //-------------------------------------------------------------------------
    // If the boot ROM hasn't already enabled the clocks to the memory
    // controller we have to do it here.
    //-------------------------------------------------------------------------
    if (!IsChipInitialized)
    {
        //*((volatile long *)0x6000619C) = 0x03000000 ;//  CLK_SOURCE_EMC
        Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_EMC, EMC_2X_CLK_SRC, 0x0)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_EMC, EMC_2X_CLK_DIVISOR, 0x0);
        NV_CAR_REGW(CLK_RST_PA_BASE, CLK_SOURCE_EMC, Reg);
    }

    //-------------------------------------------------------------------------
    // Enable clocks to required peripherals.
    //-------------------------------------------------------------------------

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_L);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_CACHE2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S0, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_VCP, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_HOST1X, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_3D, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_ISP, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_USBD, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_2D, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_VI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_EPP, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_PWM, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_TWC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC4, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_NDFLASH, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2C1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SPDIF, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_GPIO, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_UARTB, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_UARTA, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_TMR, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_RTC, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_CPU, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_L, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_H);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_BSEV, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_BSEA, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_VDE, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MPE, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_EMC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_UARTC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_I2C2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_TVDAC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_CSI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_HDMI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MIPI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_TVO, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DSI, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_I2C5, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_XIO, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_NOR, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SNOR, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SLC1, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_FUSE, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_PMC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_STAT_MON, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_KBC, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_APBDMA, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MEM, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_H, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_U);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DEV1_OUT, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DEV2_OUT, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SUS_OUT, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_M_DOUBLER_ENB, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_CRAM2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMD, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMC, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMB, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMA, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DTV, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_LA, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_AVPUCQ, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_CSITE, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_AFI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_OWR, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_PCIE, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SDMMC3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SBC4, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_I2C3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_UARTE, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_UARTD, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_U, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_V);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SE, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_TZRAM, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_HDA, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SATA, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SATA_OOB, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SATA_FPCI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_VDE_CBC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_DAM2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_DAM1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_DAM0, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_APBIF, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_AUDIO, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SBC6, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SBC5, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S4, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_TSENSOR, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_MSELECT, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_3D2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_CPULP, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_CPUG, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_V, Reg);

    // Enable CL_DVFS without this clock, I2C5 will not work..
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_W);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_DVFS, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_W, Reg);

    // Switch MSELECT clock to PLLP
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_SOURCE_MSELECT);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_MSELECT,
            MSELECT_CLK_SRC, PLLP_OUT0, Reg);

    // Clock divider request for 102MHz would setup MSELECT clock as 108MHz for PLLP base 216MHz
    // and 102MHz for PLLP base 408MHz.
    Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_MSELECT,
            MSELECT_CLK_DIVISOR, CLK_DIVIDER(NVBL_PLLP_KHZ, 102000), Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_SOURCE_MSELECT, Reg);

    // Give clocks time to stabilize.
    NvBlAvpStallMs(1);

    // Take requried peripherals out of reset.
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_L);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_CACHE2_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_I2S0_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_VCP_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_HOST1X_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_DISP1_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_DISP2_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_3D_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_ISP_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_USBD_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_2D_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_VI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_EPP_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_I2S2_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_PWM_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_TWC_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_SDMMC4_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_SDMMC1_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_NDFLASH_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_I2C1_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_I2S1_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_SPDIF_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_SDMMC2_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_GPIO_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_UARTB_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_UARTA_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_TMR_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_COP_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_CPU_RST, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_L, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_H);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_BSEV_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_BSEA_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_VDE_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_MPE_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_EMC_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_UARTC_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_I2C2_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_CSI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_HDMI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_DSI_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_I2C5_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC3_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_XIO_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC2_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC1_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_FUSE_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_STAT_MON_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_APBDMA_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_MEM_RST, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_H, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_U);
    // FIXME:: Is this necessary for T12x
    if (s_FpgaEmulation)
    {
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_EMUCIF_RST, DISABLE, Reg);
    }

    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_DTV_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_LA_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_AVPUCQ_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_PCIEXCLK_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_CSITE_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_AFI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_OWR_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_PCIE_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_SDMMC3_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_SBC4_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_I2C3_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_UARTE_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_UARTD_RST, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_U, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_V);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SE_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_TZRAM_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_HDA_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SATA_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SATA_OOB_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM2_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM1_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM0_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_APBIF_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_AUDIO_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SBC6_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SBC5_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_I2S4_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_I2S3_RST, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_MSELECT_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_3D2_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_CPULP_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_CPUG_RST, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_V, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_W);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_DVFS_RST, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_W, Reg);
}

static void NvBlAvpClockDeInit(void)
{
    // !!!FIXME!!! Need to implement this.
}

static NvBootClocksOscFreq NvBlAvpMeasureOscFreq(void)
{
    NvU32 Reg;
    NvU32 Count;

    // Start measurement, window size uses n-1 coding
    Reg = NV_DRF_DEF(CLK_RST_CONTROLLER, OSC_FREQ_DET, OSC_FREQ_DET_TRIG, ENABLE)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_FREQ_DET, REF_CLK_WIN_CFG, (1-1));
    NV_CAR_REGW(CLK_RST_PA_BASE, OSC_FREQ_DET, Reg);

    // wait until the measurement is done
    do
    {
        Reg = NV_CAR_REGR(CLK_RST_PA_BASE, OSC_FREQ_DET_STATUS);
    }
    while (NV_DRF_VAL(CLK_RST_CONTROLLER, OSC_FREQ_DET_STATUS, OSC_FREQ_DET_BUSY, Reg));

    Count = NV_DRF_VAL(CLK_RST_CONTROLLER, OSC_FREQ_DET_STATUS, OSC_FREQ_DET_CNT, Reg);

    // Convert the measured count to a frequency.
    return NvBlAvpOscillatorCountToFrequency(Count);
}

void NvBlMemoryControllerInit(NvBool IsChipInitialized)
{
    NvU32   Reg;                // Temporary register
    NvBool  IsSdramInitialized; // Nonzero if SDRAM already initialized

    // Get a pointer to the Boot Information Table.
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T12X_BASE_PA_BOOT_INFO);

    // Get SDRAM initialization state.
    IsSdramInitialized = pBootInfo->SdramInitialized;

    //-------------------------------------------------------------------------
    // EMC/MC (Memory Controller) Initialization
    //-------------------------------------------------------------------------

    // Has the boot rom initialized the memory controllers?
    if (!IsSdramInitialized)
    {
        NV_ASSERT(0);   // NOT SUPPORTED
    }

    //-------------------------------------------------------------------------
    // Memory Aperture Configuration
    //-------------------------------------------------------------------------

// Skip the logic if we are building bootloader for RTAPI - RTAPI tests should be
// able to modify AMAP register
#ifndef USED_BY_RTAPI
    // Disable writes to the address map configuration register.
    Reg = NV_PMC_REGR(PMC_PA_BASE, SEC_DISABLE);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, SEC_DISABLE, AMAP_WRITE, ON, Reg);
    NV_PMC_REGW(PMC_PA_BASE, SEC_DISABLE, Reg);
#endif

    //-------------------------------------------------------------------------
    // Memory Controller Tuning
    //-------------------------------------------------------------------------

    // Set up the AHB Mem Gizmo
    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, AHB_MEM);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_MEM, ENABLE_SPLIT, 1, Reg);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_MEM, DONT_SPLIT_AHB_WR, 0, Reg);
    /// Added for USB Controller
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_MEM, ENB_FAST_REARBITRATE, 1, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, AHB_MEM, Reg);

    // Enable GIZMO settings for USB controller.
    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, USB);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, USB, IMMEDIATE, 1, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, USB, Reg);

    // AHB_ARBC settings to select USB2_OTG to use PPCS1 ASID.
    Reg = AOS_REGR(AHB_PA_BASE + AHB_MASTER_SWID_0);
    Reg |= NV_DRF_NUM(AHB_MASTER, SWID, USB2, 1);
    AOS_REGW(AHB_PA_BASE + AHB_MASTER_SWID_0, Reg);

    // Make sure the debug registers are clear.
    Reg = 0;
    NV_MISC_REGW(MISC_PA_BASE, GP_ASDBGREG, Reg);
}

NvBool NvBlAvpInit_T12x(NvBool IsRunningFromSdram)
{
    NvBool              IsChipInitialized;
    NvBool              IsLoadedByScript = NV_FALSE;
    NvBootInfoTable*    pBootInfo = (NvBootInfoTable*)(T12X_BASE_PA_BOOT_INFO);

    // Upon entry, IsRunningFromSdram reflects whether or not we're running out
    // of NOR (== NV_FALSE) or SDRAM (== NV_TRUE).
    //
    // (1)  If running out of SDRAM, whoever put us there may or may not have
    //      initialized everything. It is possible that this code was put here
    //      by a direct download over JTAG into SDRAM.
    // (2)  If running out of NOR, the chip may or may not have been initialized.
    //      In the case of secondary boot from NOR, the boot ROM will have already
    //      set up everything properly but we'll be executing from NOR. In that
    //      situation, the bootloader must not attempt to reinitialize the modules
    //      the boot ROM has already initialized.
    //
    // In either case, if the chip has not been initialized (as indicated by no
    // valid signature in the Boot Information Table) we must take steps to do
    // it ourself.

    // See if the boot ROM initialized us.
    IsChipInitialized = NvBlAvpIsChipInitialized();

    // Has the boot ROM done it's part of chip initialization?
    if (!IsChipInitialized)
    {
        // Is this the special case of the boot loader being directly deposited
        // in SDRAM by a debugger script?
        if ((pBootInfo->BootRomVersion == 0xFFFFFFFF)
        &&  (pBootInfo->DataVersion    == 0xFFFFFFFF)
        &&  (pBootInfo->RcmVersion     == 0xFFFFFFFF)
        &&  (pBootInfo->BootType       == 0xFFFFFFFF)
        &&  (pBootInfo->PrimaryDevice  == 0xFFFFFFFF))
        {
            // The script must have initialized everything necessary to get
            // the chip up so don't try to reinitialize.
            IsChipInitialized = NV_TRUE;
            IsLoadedByScript  = NV_TRUE;
        }

        // Was the bootloader placed in SDRAM by a USB Recovery Mode applet?
        if (IsRunningFromSdram && NvBlAvpIsChipInRecovery())
        {
            // Keep the Boot Information Table setup by that applet
            // but mark the SDRAM and device as initialized.
            pBootInfo->SdramInitialized = NV_TRUE;
            pBootInfo->DevInitialized   = NV_TRUE;
            IsChipInitialized           = NV_TRUE;
        }
        else
        {
            // We must be doing primary boot from NOR or a script boot.
            // Build a fake BIT structure in IRAM so that the rest of
            // the code can proceed as if the boot ROM were present.
            NvOsAvpMemset(pBootInfo, 0, sizeof(NvBootInfoTable));

            // Fill in the parts we need.
            pBootInfo->PrimaryDevice   = NvBootDevType_Nor;
            pBootInfo->SecondaryDevice = NvBootDevType_Nor;
            pBootInfo->OscFrequency    = NvBlAvpMeasureOscFreq();
            pBootInfo->BootType        = NvBootType_Cold;

            // Did a script load this code?
            if (IsLoadedByScript)
            {
                // Scripts must initialize the whole world.
                pBootInfo->DevInitialized   = NV_TRUE;
                pBootInfo->SdramInitialized = NV_TRUE;
            }
        }
    }

    // Enable the boot clocks.
    NvBlAvpClockInit(IsChipInitialized, IsLoadedByScript);
#if __GNUC__
    // Enable Uart after PLLP init
    NvAvpUartInit(PLLP_FIXED_FREQ_KHZ_408000);
    NvOsAvpDebugPrintf("Bootloader-AVP Init at: %d us\n", g_BlAvpTimeStamp);
#endif

    // Disable all module clocks except which are required to boot.
    NvBlAvpClockDeInit();

    // Initialize memory controller.
    NvBlMemoryControllerInit(IsChipInitialized);

    return IsChipInitialized;
}

#ifdef USED_BY_RTAPI
// This finction is a simplified version of NvBlAvpInit_T12x, and it is only used
// by RTAPI flow for skipping all BIT setting. (there is no BIT used in RTAPI flow)
//
// NOTE:
//  (1) This function is called by:
//               tegra/core-private/drivers/nvtestloader/testloader.c
//  (2) This function will be removed when RTAPI flow is merged with nvflash flow.
//  (3) macro "USED_BY_RTAPI" is defined in RTAPI flow, in the file:
//               tegra/core-private/drivers/nvtestloader/Makefile
NvBool NvBlAvpInitFromScript_T12x()
{
    NvBool              IsChipInitialized = NV_TRUE;
    NvBool              IsLoadedByScript = NV_TRUE;
    NvBootInfoTable*    pBootInfo = (NvBootInfoTable*)(T12X_BASE_PA_BOOT_INFO);

    NvBlInitI2cPinmux();
    NvBlReadBoardInfo(NvBoardType_ProcessorBoard, &g_BoardInfo);

#ifdef PINMUX_INIT
    // initialize pinmuxes
    NvOdmPinmuxInit(g_BoardInfo.BoardId);
#endif

#ifndef QT_EMUL
    pBootInfo->OscFrequency = NvBlAvpMeasureOscFreq();
#else
    // NvBlAvpMeasureOscFreq does not work for QT
    // Set clock to 12 MHz just to avoid NV_ASSERT
    // in NvBlAvpQueryOscillatorFrequency
    pBootInfo->OscFrequency = NvBootClocksOscFreq_12;
#endif

    // avoid NV_ASSERT in NvBlMemoryControllerInit
    pBootInfo->SdramInitialized = NV_TRUE;

    // Enable the boot clocks.
    NvBlAvpClockInit(IsChipInitialized, IsLoadedByScript);

    // Disable all module clocks except which are required to boot.
    NvBlAvpClockDeInit();

    // Initialize memory controller.
    NvBlMemoryControllerInit(IsChipInitialized);

    return IsChipInitialized;
}
//
// declarations
//
extern NvU32 g_EnableGpu; // defined in testloader.cpp
static void NvBlAvpEnableGpuPowerRail(void);
static void NvBlAvpEnableCpuPowerRail(void);
static void NvBlAvpEnableCpuClock(void);
static void NvBlAvpClockEnableCoresight(void);
static void NvBlAvpRemoveCpuReset(void);
static void NvBlAvpPowerUpCpu(void);

//------------------------------------------------------------------------------
void NvBlStartCpuFromScript_T12x(NvU32 ResetVector)
{
   // Enable VDD_CPU
    s_enablePowerRail = *g_ptimerus;
    NvBlAvpEnableCpuPowerRail();
    e_enablePowerRail = *g_ptimerus;

    if(g_EnableGpu)
        NvBlAvpEnableGpuPowerRail();

    // Enable the CPU clock.
    NvBlAvpEnableCpuClock();

    // Enable CoreSight.
    NvBlAvpClockEnableCoresight();

    // Remove all software overrides
    NvBlAvpRemoveCpuReset();

    // Set the entry point for CPU execution from reset, if it's a non-zero value.
    NvBlAvpSetCpuResetVector(ResetVector);

    // Power up the CPU.
    NvBlAvpPowerUpCpu();

}
#endif // USED_BY_RTAPI

static NvBlCpuClusterId NvBlAvpQueryFlowControllerClusterId(void)
{
    NvBlCpuClusterId    ClusterId = NvBlCpuClusterId_Fast; // Active CPU id
    NvU32               Reg;                            // Scratch register

    Reg = NV_FLOW_REGR(FLOW_PA_BASE, CLUSTER_CONTROL);
    Reg = NV_DRF_VAL(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, Reg);

    if (Reg == NV_DRF_DEF(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, LP))
    {
        ClusterId = NvBlCpuClusterId_Slow;
    }

    return ClusterId;
}

static NvBool NvBlAvpIsPartitionPowered(NvU32 Mask)
{
    // Get power gate status.
    NvU32   Reg = NV_PMC_REGR(PMC_PA_BASE, PWRGATE_STATUS);

    if ((Reg & Mask) == Mask)
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

static void NvBlAvpPowerPartition(NvU32 status, NvU32 toggle)
{
    // Is the partition already on?
    if (!NvBlAvpIsPartitionPowered(status))
    {
        // No, toggle the partition power state (OFF -> ON).
        NV_PMC_REGW(PMC_PA_BASE, PWRGATE_TOGGLE, toggle);

        // Wait for the power to come up.
        while (!NvBlAvpIsPartitionPowered(status))
        {
            // Do nothing
        }
    }
}

#define POWER_PARTITION(x)  \
    NvBlAvpPowerPartition(PMC_PWRGATE_STATUS(x), PMC_PWRGATE_TOGGLE(x))

static void NvBlAvpRamRepair(void)
{
#ifdef FLOW_CTLR_RAM_REPAIR_0  // !!!FIXME!!! remove when bug 842533 is fixed
    NvU32 Reg; // Scratch reg

    // Can only be performed on fast cluster
    NV_ASSERT(NvBlAvpQueryFlowControllerClusterId() == NvBlCpuClusterId_Fast);

    // Request RAM repair
    Reg = NV_DRF_DEF(FLOW_CTLR, RAM_REPAIR, REQ, ENABLE);
    NV_FLOW_REGW(FLOW_PA_BASE, RAM_REPAIR, Reg);

    // Wait for completion
    do
    {
        Reg = NV_FLOW_REGR(FLOW_PA_BASE, RAM_REPAIR);
    } while (!NV_DRF_VAL(FLOW_CTLR, RAM_REPAIR, STS, Reg));
#endif
}

static void  NvBlAvpPowerUpCpu(void)
{
   // Are we booting to the fast cluster?
    if (NvBlAvpQueryFlowControllerClusterId() == NvBlCpuClusterId_Fast)
    {
        // TODO: Set CPU Power Good Timer

        // Power up the fast cluster rail partition.
        POWER_PARTITION(CRAIL);

        //NvBlAvpToggleJtagClk();
        NvBlAvpRamRepair();

        // Power up the fast cluster non-CPU partition.
        POWER_PARTITION(C0NC);

        // Power up the fast cluster CPU0 partition.
        POWER_PARTITION(CE0);
    }
    else
    {
        // Power up the slow cluster non-CPU partition.
        POWER_PARTITION(C1NC);

        // Power up the slow cluster CPU partition.
        POWER_PARTITION(CELP);
    }
}

//------------------------------------------------------------------------------

// Defines for Proc board ids
#define SHIELD_ERS_E1780          0x6F4        // E1780
#define SHIELD_ERS_E1792          0x700        // E1792
#define SHIELD_ERS_E1782          0x6f6        // E1782
#define SHIELD_ERS_E1791          0x6ff        // E1791
#define SHIELD_ERS_S_E1783        0x6f7        // E1783
#define DSC_ERS_S_E1781           0x6F5        // E1781
#define LAGUNA_ERS_PM358          0x166        // PM358
#define LAGUNA_ERS_S_PM359        0x167        // PM359
#define LAGUNA_FFD_PM363          0x16B        // PM363
#define LAGUNA_FFD_PM374          0x176        // PM374
#define LAGUNA_FFD_PM370          0x172        // PM370
#define LOKI_NFF_E2548            0x9F4        // E2548
#define LOKI_NFF_E2549            0x9F5        // E2549
#define LOKI_FFD_P2530            0x9E2        // P2530
#define TN8_FFD_P1761             0x6E1        // P1761
#define TN8_E1784                 0x6F8        // E1784
#define TN8_E1922                 0x782        // E1922
#define TN8_E1923                 0x783        // E1923

// Defines for I2c slave addresses
#define PMU_EEPROM_I2C_SLAVE_ADDR   0xAA
#define TPS65913_I2C_SLAVE_ADDR     0xB0
#define AS3722_I2C_SLAVE_ADDR       0x80

// Defines for Pmu Board Ids
#define PMU_BOARD_E1733             0x6C5    // AMS3722 module
#define PMU_BOARD_E1734             0x6C6    // AMS3722 module
#define PMU_BOARD_E1735             0x6C7    // TI 913/4 + OpenVR
#define PMU_BOARD_E1736             0x6C8    // TI 913/4
#define PMU_BOARD_E1769             0x6E9    // TI 913/4
#define PMU_BOARD_E1936             0x790    // TI 913/4

// GPIO ADDRs for KB_ROW13
#define GPIO_S05_CNF                0x6000D408
#define GPIO_S05_OE                 0x6000D418
#define GPIO_S05_OUT                0x6000D428
#define GPIO_U06_CNF                0x6000D500
#define GPIO_U06_OE                0x6000D510
#define GPIO_U06_OUT                0x6000D520

static void NvBlEnableAmsCpuRail(void)
{
    NvU16 val;
    // AMS pmu power rail init sequence
    // Set SD0 to voltage to 0.6 + 0x28 * 0.01 = 1.0 V
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, 0x3C, 0x02);
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, 0x00, 0x28);

    // Update SDControl (RegAddr = 0x4D) to enable SD0

// Reads from AMS registers seem to be yielding garabage
// We're debugging this at the moment.
// Update to SDControl (0x4D) is not necessary since
// all SDs are enabled by default (0x7F is the default
// value of SDControl)
#if 0
    val = NvBlPmuRead(AS3722_I2C_SLAVE_ADDR, 0x4D, 1);
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, 0x4D, (val | 0x1));
#endif

    #define AMS3722_GPIO4control 0xC
    #define AMS3722_GPIOsignal_out 0x20
    #define AMS3722_SD5Voltage     0x5
    #define AMS3722_SDControl      0x4D

    //GPIO4 -> EN_AVDD_LCD
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, AMS3722_GPIO4control, 0x02);
    val = NvBlPmuRead(AS3722_I2C_SLAVE_ADDR, AMS3722_GPIOsignal_out, 1); //GPIO4_OUT : Drive High
    val = val | 0x10;
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, AMS3722_GPIOsignal_out, val);

    //SD5 -> VDD_TS_1V8B_LDO13 = 1.8V -> VDD_TS_1V8B_DIS
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, AMS3722_SD5Voltage, 0x50);//VDD_1V8 = 1.8V
    val = NvBlPmuRead(AS3722_I2C_SLAVE_ADDR, AMS3722_SDControl, 1);
    val = val | 0x20;
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, AMS3722_SDControl, val);//Global Stepdown5 Enable
}

static void NvBlEnableOpenVregCpuRail(void)
{
    NvU32 Reg = 0;
    // KB_ROW13 needs to be driven high to put VID in tristate.
    // This is necessary for putting OpenVreg in boot mode.

    // Set pinmux for KB_ROW13
    Reg = NV_READ32( PINMUX_BASE + PINMUX_AUX_KB_ROW13_0);
    Reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, KB_ROW13, PM, 2, Reg);
    Reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, KB_ROW13, E_INPUT, 0, Reg);
    Reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, KB_ROW13, PUPD, 0, Reg);
    Reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, KB_ROW13, TRISTATE, 0,Reg);
    NV_WRITE32( PINMUX_BASE + PINMUX_AUX_KB_ROW13_0, Reg);

    // Configure KB_ROW13 as GPIO
    NV_WRITE32(GPIO_S05_CNF, (NV_READ32(GPIO_S05_CNF) | 0x20));
    NV_WRITE32(GPIO_S05_OE, (NV_READ32(GPIO_S05_OE)  | 0x20));
    NV_WRITE32(GPIO_S05_OUT, (NV_READ32(GPIO_S05_OUT) | 0x20));

}

static void NvBlEnableE2545OpenVregCpuRail(void)
{
    NvU32 Reg = 0;
    // GPIO_PU6 needs to be driven high to put VID in tristate.
    // This is necessary for putting OpenVreg in boot mode.

    // Set pinmux for GPIO_PU6
    Reg = NV_READ32( PINMUX_BASE + PINMUX_AUX_KB_ROW13_0);
    Reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PU6, E_INPUT, 0, Reg);
    Reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PU6, PUPD, 0, Reg);
    Reg = NV_FLD_SET_DRF_NUM(PINMUX_AUX, GPIO_PU6, TRISTATE, 0,Reg);
    NV_WRITE32( PINMUX_BASE + PINMUX_AUX_GPIO_PU6_0, Reg);

    // Configure GPIO_PU6 as GPIO
    NV_WRITE32(GPIO_U06_CNF, (NV_READ32(GPIO_U06_CNF) | 0x40));
    NV_WRITE32(GPIO_U06_OE, (NV_READ32(GPIO_U06_OE)  | 0x40));
    NV_WRITE32(GPIO_U06_OUT, (NV_READ32(GPIO_U06_OUT) | 0x40));
}

static void NvBlEnableE1736CpuRail(void)
{
    NvBlPmuWrite(TPS65913_I2C_SLAVE_ADDR, 0x20, 0x01);
    NvBlPmuWrite(TPS65913_I2C_SLAVE_ADDR, 0x23, 0x38);
}

static void NvBlAvpEnableCpuPowerRail(void)
{
    NvU32   Reg;        // Scratch reg
    NvU32 ProcBoardId = 0;
    NvU32 PmuBoardId = 0;
    NvBoardInfo ProcBoardInfo = {0};
    NvBoardInfo PmuBoardInfo = {0};
    NvBctAuxInfo *auxInfo;
    const NvBootInfoTable *pBit;
    NvU8 *ptr, *alignedPtr;

#if __GNUC__
    NvOsAvpDebugPrintf("Dummy read for TPS65913\n");
#endif
    NvDdkI2cDummyRead(NvDdkI2c5, TPS65913_I2C_SLAVE_ADDR);

    // Read processor and pmu board Ids
    if (!s_FpgaEmulation)
    {
        NvBlReadBoardInfo(NvBoardType_ProcessorBoard, &ProcBoardInfo);
        if (ProcBoardInfo.BoardId > 0xA00 || ProcBoardInfo.BoardId == 0)
        {
            pBit = (const NvBootInfoTable *)(NV_BIT_ADDRESS);
            NV_ASSERT(pBit->BctPtr);
            ptr = pBit->BctPtr->CustomerData + sizeof(NvBctCustomerPlatformData);
            alignedPtr = (NvU8*)((((NvU32)ptr + sizeof(NvU32) - 1) >> 2) << 2);
            auxInfo = (NvBctAuxInfo *)alignedPtr;
            ProcBoardInfo.BoardId = auxInfo->NCTBoardInfo.proc_board_id;
            ProcBoardInfo.Fab = auxInfo->NCTBoardInfo.proc_fab;
            ProcBoardInfo.Sku = auxInfo->NCTBoardInfo.proc_sku;
            ProcBoardInfo.Revision = 0;
            ProcBoardInfo.MinorRevision = 0;
         }
         ProcBoardId = ProcBoardInfo.BoardId;
     }

#if __GNUC__
        NvOsAvpDebugPrintf("Board Id = 0x%x\n", ProcBoardId);
#endif

    switch(ProcBoardId)
    {
        case LAGUNA_ERS_PM358:
        case LAGUNA_ERS_S_PM359:
        case LAGUNA_FFD_PM363:
        case LAGUNA_FFD_PM374:
        case LAGUNA_FFD_PM370:
            NvBlEnableAmsCpuRail();
            break;
        case SHIELD_ERS_E1780:
        case SHIELD_ERS_E1792:
        case DSC_ERS_S_E1781:
        case SHIELD_ERS_E1782:
        case SHIELD_ERS_S_E1783:
        case SHIELD_ERS_E1791:
        case TN8_E1784:
        case TN8_E1922:
        case TN8_E1923:
            NvBlReadBoardInfo(NvBoardType_PmuBoard, &PmuBoardInfo);
            PmuBoardId = PmuBoardInfo.BoardId;
            switch(PmuBoardId)
            {
                case PMU_BOARD_E1733:
                case PMU_BOARD_E1734:
                    NvBlEnableAmsCpuRail();
                    break;
                case PMU_BOARD_E1735:
                    NvBlEnableOpenVregCpuRail();
                    break;
                case PMU_BOARD_E1736:
                case PMU_BOARD_E1769:
                case PMU_BOARD_E1936:
                    NvBlEnableE1736CpuRail();
                    break;
                default:
#if __GNUC__
                NvOsAvpDebugPrintf("Unsupported PmuBoard 0x%x\n", PmuBoardId);
#endif
                break;
            }
            break;
        case TN8_FFD_P1761:
            NvBlEnableE1736CpuRail();
            break;
        case LOKI_NFF_E2548:
        case LOKI_NFF_E2549:
        case LOKI_FFD_P2530:
            // TODO: need to modify this
            NvBlEnableE2545OpenVregCpuRail();
            break;
        default:
            if(!s_FpgaEmulation)
            {
#if __GNUC__
                NvOsAvpDebugPrintf("Unsupported Processor Board 0x%x\n", ProcBoardId);
#endif
            }
            break;
    }

    // Enable 5 ms delay
    Reg = 0x7c830;
    NV_PMC_REGW(PMC_PA_BASE, CPUPWRGOOD_TIMER, Reg);

    // Disable CPUPWRREQ_POLARITY
    Reg = NV_PMC_REGR(PMC_PA_BASE, CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRREQ_POLARITY, NORMAL, Reg);
    NV_PMC_REGW(PMC_PA_BASE, CNTRL, Reg);

    // Enable CPUPWRREQ_OE
    Reg = NV_PMC_REGR(PMC_PA_BASE, CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRREQ_OE, ENABLE, Reg);
    NV_PMC_REGW(PMC_PA_BASE, CNTRL, Reg);
}

#ifdef USED_BY_RTAPI
static void NvBlEnableAmsGpuRail(NvU32 Vdd_Gpu)
{
    #define AMS3722_SD6Voltage      0x6
    #define AMS3722_SDControl       0x4D

    NvU32 Data = 0;
    //SD6 Voltage
    //VDD_GPU = 1.0V
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, AMS3722_SD6Voltage, 0x28);
    Data = NvBlPmuRead(AS3722_I2C_SLAVE_ADDR, AMS3722_SDControl, 1);
    Data = Data | 0x40;
   //Global Stepdown6 Enable
    NvBlPmuWrite(AS3722_I2C_SLAVE_ADDR, AMS3722_SDControl, Data);
}

static void NvBlEnableTi913GpuRail(NvU32 Vdd_Gpu)
{
    #define TPS65913_SMPS12_CTRL        0x20
    #define TPS65913_SMPS12_VOLTAGE     0x23
    #define TPS65913_SMPS3_CTRL         0x24

    NvBlPmuWrite(TPS65913_I2C_SLAVE_ADDR, TPS65913_SMPS12_CTRL, 0x01);
    //VDD_GPU = 1.0v
    NvBlPmuWrite(TPS65913_I2C_SLAVE_ADDR, TPS65913_SMPS12_VOLTAGE, 0x38);
    NvBlPmuWrite(TPS65913_I2C_SLAVE_ADDR, TPS65913_SMPS3_CTRL, 0x01);
}

static void NvBlAvpEnableGpuPowerRail(void)
{
    NvU32 ProcBoardId, PmuBoardId;
    NvBoardInfo PmuBoardInfo;
    NvU32 Reg = 0;
    // For now, set Vdd_Gpu to 1000 mv
    NvU32   Vdd_Gpu = 1000; // mv

    ProcBoardId = g_BoardInfo.BoardId;
    switch(ProcBoardId)
    {
        case LAGUNA_ERS_PM358:
        case LAGUNA_ERS_S_PM359:
        case LAGUNA_FFD_PM363:
        case LAGUNA_FFD_PM374:
        case LAGUNA_FFD_PM370:
            NvBlEnableAmsGpuRail(Vdd_Gpu);
            break;
        case SHIELD_ERS_E1780:
        case SHIELD_ERS_E1792:
        case DSC_ERS_S_E1781:
        case SHIELD_ERS_E1782:
        case SHIELD_ERS_S_E1783:
        case SHIELD_ERS_E1791:
        case TN8_E1784:
        case TN8_E1922:
        case TN8_E1923:
            NvBlReadBoardInfo(NvBoardType_PmuBoard, &PmuBoardInfo);
            PmuBoardId = PmuBoardInfo.BoardId;
            switch(PmuBoardId)
            {
                case PMU_BOARD_E1733:
                case PMU_BOARD_E1734:
                    NvBlEnableAmsGpuRail(Vdd_Gpu);
                    break;
                case PMU_BOARD_E1735:
                case PMU_BOARD_E1736:
                case PMU_BOARD_E1769:
                case PMU_BOARD_E1936:
                    NvBlEnableTi913GpuRail(Vdd_Gpu);
                    break;
                default:
                    break;
            }
        case TN8_FFD_P1761:
            NvBlEnableTi913GpuRail(Vdd_Gpu);
            break;
     }

    //
    // disable GPU's clock
    //
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_X);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, \
                CLK_OUT_ENB_X, CLK_ENB_GPU, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_X, Reg);
    NvBlAvpStallUs(2000);

    //
    // enable GPU's clock
    //
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_X);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, \
                CLK_OUT_ENB_X, CLK_ENB_GPU, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_X, Reg);
    //
    // put GPU into reset
    //
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE,  RST_DEVICES_X);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, \
                RST_DEVICES_X, SWR_GPU_RST, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_X, Reg);

    //
    // disable GPU clamp
    //
    Reg = NV_PMC_REGR(PMC_PA_BASE, GPU_RG_CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, GPU_RG_CNTRL, \
                RAIL_CLAMP, DISABLE, Reg);
    NV_PMC_REGW(PMC_PA_BASE, GPU_RG_CNTRL, Reg);
    NvBlAvpStallUs(2000);

    //
    // take the GPU out of reset
    //
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE,  RST_DEVICES_X);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, \
                RST_DEVICES_X, SWR_GPU_RST, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_X, Reg);
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE,  RST_DEVICES_X);
    NvBlAvpStallUs(2000);

    //
    // configure MSELECT
    //
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_V);

    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, \
                CLK_ENB_MSELECT, ENABLE, Reg);

    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_V, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_SOURCE_MSELECT);

    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_MSELECT, \
                MSELECT_CLK_SRC, PLLP_OUT0, Reg);
    Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_MSELECT,
                 MSELECT_CLK_DIVISOR, 6, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_SOURCE_MSELECT, Reg);

    //
    // take MSELECT out of reset
    //
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_V);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, \
                CLK_ENB_MSELECT, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_V, Reg);
    NvBlAvpStallUs(2000);
}
#endif //USED_BY_RTAPI

//------------------------------------------------------------------------------
void NvBlStartCpu_T12x(NvU32 ResetVector)
{
   // Enable VDD_CPU
    s_enablePowerRail = *g_ptimerus;
    NvBlAvpEnableCpuPowerRail();
    e_enablePowerRail = *g_ptimerus;

    // Enable the CPU clock.
    NvBlAvpEnableCpuClock();

    // Enable CoreSight.
    NvBlAvpClockEnableCoresight();

    // Remove all software overrides
    NvBlAvpRemoveCpuReset();

    // Set the entry point for CPU execution from reset, if it's a non-zero value.
    NvBlAvpSetCpuResetVector(ResetVector);

    // Power up the CPU.
    NvBlAvpPowerUpCpu();
}

//------------------------------------------------------------------------------
NvBool NvBlQueryGetNv3pServerFlag(void)
{
    // Pointer to the start of IRAM which is where the Boot Information Table
    // will reside (if it exists at all).
    NvBootInfoTable*  pBootInfo = (NvBootInfoTable*)(NV_BIT_ADDRESS);
    NvU32 * SafeStartAddress = 0;
    NvU32 Nv3pSignature = NV3P_SIGNATURE;
    static NvS8 IsNv3pSignatureSet = -1;
    NvBool ret = NV_FALSE;

    if (IsNv3pSignatureSet == 0 || IsNv3pSignatureSet == 1)
    {
        ret = (NvBool)IsNv3pSignatureSet;
    }

    // Yes, is there a valid BCT?
    if (pBootInfo->BctValid && IsNv3pSignatureSet == -1)
    {   // Is the address valid?
        if (pBootInfo->SafeStartAddr)
        {
            // Get the address where the signature is stored. (Safe start - 4)
            SafeStartAddress = (NvU32 *)(pBootInfo->SafeStartAddr - sizeof(NvU32));
            // compare signature...if this is coming from miniloader

            IsNv3pSignatureSet = 0;
            if (*(SafeStartAddress) == Nv3pSignature)
            {
                // if yes then modify the safe start address
                // with correct start address (Safe address -4)
                IsNv3pSignatureSet = 1;
                (pBootInfo->SafeStartAddr) -= sizeof(NvU32);
                ret = NV_TRUE;
            }
        }
    }
    return ret;
}

void NvBlConfigFusePrivateTZ(void)
{
    NvU32 Reg=0;
    NvU8 *pBuffer = NULL;
    NvBctAuxInfo *pAuxInfo;

    const NvBootInfoTable* pBootInfo =
        (const NvBootInfoTable*)(T12X_BASE_PA_BOOT_INFO);

    pBuffer = pBootInfo->BctPtr->CustomerData +
                    sizeof(NvBctCustomerPlatformData);
    pBuffer = (NvU8*)((((NvU32)pBuffer + sizeof(NvU32) - 1) >> 2) << 2);
    pAuxInfo = (NvBctAuxInfo *)pBuffer;

    if (pAuxInfo->StickyBit & 0x1)
    {
        Reg = AOS_REGR(FUSE_PA_BASE + FUSE_PRIVATEKEYDISABLE_0);
        Reg |= NV_DRF_NUM(FUSE, PRIVATEKEYDISABLE, TZ_STICKY_BIT_VAL, 1);
        AOS_REGW(FUSE_PA_BASE + FUSE_PRIVATEKEYDISABLE_0, Reg);
    }
}

void NvBlConfigJtagAccess(void)
{
    NvU32 reg = 0;

    // Leave JTAG untouched in NvProd mode
    if (NV_FUSE_REGR(FUSE_PA_BASE, SECURITY_MODE))
    {
        if (NvBlAvpIsChipInitialized() || NvBlAvpIsChipInRecovery())
        {
            reg = AOS_REGR(SECURE_BOOT_PA_BASE + NVBL_SECURE_BOOT_CSR_OFFSET);
            if (!(reg & (1 << NVBL_SECURE_JTAG_OFFSET_IN_CSR)))// JTAG is enabled by BootROM
            {
                reg  = AOS_REGR(SECURE_BOOT_PA_BASE + NVBL_SECURE_BOOT_PFCFG_OFFSET);
                // Maximum level of debuggability is decided by what
                // NVBL_SECURE_BOOT_JTAG_VAL is going to get during build time.
                // It depends on the existence of SecureOs and requirement to
                // enable profiling/debugging.
                reg &= NVBL_SECURE_BOOT_JTAG_CLEAR;
                reg |= NVBL_SECURE_BOOT_DEBUG_CONFIG;
                AOS_REGW(SECURE_BOOT_PA_BASE + NVBL_SECURE_BOOT_PFCFG_OFFSET, reg);

                reg = AOS_REGR(MISC_PA_BASE + APB_MISC_PP_CONFIG_CTL_0);
                reg = NV_FLD_SET_DRF_DEF(APB_MISC,PP_CONFIG_CTL,JTAG,ENABLE,reg);
                AOS_REGW(MISC_PA_BASE + APB_MISC_PP_CONFIG_CTL_0, reg);
            }
            else // JTAG is disabled by BootROM
            {
                // Must write '0' to SPNIDEN, SPIDEN, DBGEN and untouch NIDEN
                reg  = AOS_REGR(SECURE_BOOT_PA_BASE + NVBL_SECURE_BOOT_PFCFG_OFFSET);
                reg &= NVBL_SECURE_BOOT_JTAG_CLEAR;
                reg |= NVBL_NON_SECURE_PROF;
                AOS_REGW(SECURE_BOOT_PA_BASE + NVBL_SECURE_BOOT_PFCFG_OFFSET, reg);

                // Set '1' to JTAG_STS of APBDEV_PMC_STICKY_BITS_0
                reg = AOS_REGR(PMC_PA_BASE + APBDEV_PMC_STICKY_BITS_0);
                reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC,STICKY_BITS,JTAG_STS,DISABLE,reg);
                AOS_REGW(PMC_PA_BASE + APBDEV_PMC_STICKY_BITS_0, reg);
            }
        }
        else
        {
            //Reset the chip (Chip is neither initialized nor in recovery)
            reg = NV_READ32(PMC_PA_BASE + APBDEV_PMC_CNTRL_0);
            reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, MAIN_RST, ENABLE, reg);
            NV_WRITE32(PMC_PA_BASE + APBDEV_PMC_CNTRL_0, reg);
            while(1);
        }
    }
    return;
}

//------------------------------------------------------------------------------
#if !__GNUC__
__asm void NvBlStartUpAvp_T12x( void )
{
    CODE32
    PRESERVE8

    IMPORT NvBlAvpInit_T12x
    IMPORT NvBlStartCpu_T12x
    IMPORT NvBlAvpHalt

    //------------------------------------------------------------------
    // Initialize the AVP, clocks, and memory controller.
    //------------------------------------------------------------------

    // The SDRAM is guaranteed to be on at this point
    // in the nvml environment. Set r0 = 1.
    MOV     r0, #1
    BL      NvBlAvpInit_T12x

    //------------------------------------------------------------------
    // Start the CPU.
    //------------------------------------------------------------------

    LDR     r0, =ColdBoot               //; R0 = reset vector for CPU
    BL      NvBlStartCpu_T12x

    //------------------------------------------------------------------
    // Transfer control to the AVP code.
    //------------------------------------------------------------------

    BL      NvBlAvpHalt

    //------------------------------------------------------------------
    // Should never get here.
    //------------------------------------------------------------------
    B       .
}

// we're hard coding the entry point for all AOS images

// this function inits the MPCORE and then jumps to the to the MPCORE
// executable at NV_AOS_ENTRY_POINT
__asm void ColdBoot( void )
{
    CODE32
    PRESERVE8

    MSR     CPSR_c, #PSR_MODE_SVC _OR_ PSR_I_BIT _OR_ PSR_F_BIT

    //------------------------------------------------------------------
    // Check current processor: CPU or AVP?
    // If AVP, go to AVP boot code, else continue on.
    //------------------------------------------------------------------

    LDR     r0, =PG_UP_PA_BASE
    LDRB    r2, [r0, #PG_UP_TAG_0]
    CMP     r2, #PG_UP_TAG_0_PID_CPU _AND_ 0xFF //; are we the CPU?
    LDR     sp, =NVAP_LIMIT_PA_IRAM_CPU_EARLY_BOOT_STACK
    // leave in some symbols for release debugging
    LDR     r3, =0xDEADBEEF
    STR     r3, [sp, #-4]!
    STR     r3, [sp, #-4]!
    BEQ     NV_AOS_ENTRY_POINT                   //; yep, we are the CPU

    //==================================================================
    // AVP Initialization follows this path
    //==================================================================

    LDR     sp, =NVAP_LIMIT_PA_IRAM_AVP_EARLY_BOOT_STACK
    // leave in some symbols for release debugging
    LDR     r3, =0xDEADBEEF
    STR     r3, [sp, #-4]!
    STR     r3, [sp, #-4]!
    B       NvBlStartUpAvp_T12x
}
#else
NV_NAKED void NvBlStartUpAvp_T12x( void )
{
    //;------------------------------------------------------------------
    //; Initialize the AVP, clocks, and memory controller.
    //;------------------------------------------------------------------

    asm volatile(
    //The SDRAM is guaranteed to be on at this point
    //in the nvml environment. Set r0 = 1.
    "MOV     r0, #1                          \n"
    "BL      NvBlAvpInit_T12x                \n"
    //;------------------------------------------------------------------
    //; Start the CPU.
    //;------------------------------------------------------------------
    "LDR     r0, =ColdBoot                   \n"//; R0 = reset vector for CPU
    "BL      NvBlStartCpu_T12x               \n"
    //;------------------------------------------------------------------
    //; Transfer control to the AVP code.
    //;------------------------------------------------------------------
    "BL      NvBlAvpHalt                     \n"
    //;------------------------------------------------------------------
    //; Should never get here.
    //;------------------------------------------------------------------
    "B       .                               \n"
    );
}

// this function inits the CPU and then jumps to the to the CPU
// executable at NV_AOS_ENTRY_POINT
NV_NAKED void ColdBoot( void )
{
    asm volatile(
    "MSR     CPSR_c, #0xd3                                          \n"
    //;------------------------------------------------------------------
    //; Check current processor: CPU or AVP?
    //; If AVP, go to AVP boot code, else continue on.
    //;------------------------------------------------------------------
    "MOV     r0, %0                                                 \n"
    "LDRB    r2, [r0, %1]                                           \n"
     //;are we the CPU?
    "CMP     r2, %2                                                 \n"
    "MOV     sp, %3           \n"
    // leave in some symbols for release debugging
    "MOV     r3, %6                                                 \n"
    "STR     r3, [sp, #-4]!                                         \n"
    "STR     r3, [sp, #-4]!                                         \n"
    //; yep, we are the CPU
    "BXEQ     %4                                                    \n"
    //;==================================================================
    //; AVP Initialization follows this path
    //;==================================================================
    "MOV     sp, %5                                                 \n"
    // leave in some symbols for release debugging
    "MOV     r3, %6                                                 \n"
    "STR     r3, [sp, #-4]!                                         \n"
    "STR     r3, [sp, #-4]!                                         \n"
    "B       NvBlStartUpAvp_T12x                                    \n"
    :
    :"I"(PG_UP_PA_BASE),
     "I"(PG_UP_TAG_0),
     "r"(proc_tag),
     "r"(cpu_boot_stack),
     "r"(entry_point),
     "r"(avp_boot_stack),
     "r"(deadbeef)
    : "r0", "r2", "r3", "cc", "lr"
    );
}

#endif
