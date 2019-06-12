/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "bootloader_t11x.h"
#include "nvassert.h"
#include "nverror.h"
#include "aos.h"
#include "aos_common.h"
#include "nvuart.h"
#include "nvodm_services.h"
#include "avp_gpio.h"
#include "nvbct.h"
#include "nvbct_customer_platform_data.h"
#include "nvddk_i2c.h"
#include "aos_avp_pmu.h"
#include "aos_avp_board_info.h"
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

    // Is there a CPU present in the slow CPU complex?
    if (!NV_DRF_VAL(CLK_RST_CONTROLLER, BOND_OUT_V, BOND_OUT_CPULP, Reg))
    {
        return NvBlCpuClusterId_Slow;
    }

    // Are there any CPUs present in the fast CPU complex?
    if ((!NV_DRF_VAL(CLK_RST_CONTROLLER, BOND_OUT_V, BOND_OUT_CPUG, Reg))
     && (!NV_DRF_VAL(FUSE, SKU_DIRECT_CONFIG, DISABLE_ALL, Fuse)))
    {
        return NvBlCpuClusterId_Fast;
    }

    return NvBlCpuClusterId_Unknown;
}

static NvBool NvBlIsValidBootRomSignature(const NvBootInfoTable*  pBootInfo)
{
    // Look at the beginning of IRAM to see if the boot ROM placed a Boot
    // Information Table (BIT) there.
    // !!!FIXME!!! CHECK FOR CORRECT BOOT ROM VERSION IDS
    if (((pBootInfo->BootRomVersion == NVBOOT_VERSION(0x35,0x01))
      || (pBootInfo->BootRomVersion == NVBOOT_VERSION(0x35,0x02)))
    &&   (pBootInfo->DataVersion == NVBOOT_VERSION(0x35,0x01))
    &&   (pBootInfo->RcmVersion == NVBOOT_VERSION(0x35,0x01))
    &&   (pBootInfo->BootType == NvBootType_Cold)
    &&   (pBootInfo->PrimaryDevice == NvBootDevType_Irom))
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
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T11X_BASE_PA_BOOT_INFO);

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
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T11X_BASE_PA_BOOT_INFO);

    // Look at the beginning of IRAM to see if the boot ROM placed a Boot
    // Information Table (BIT) there.
    // !!!FIXME!!! CHECK FOR CORRECT BOOT ROM VERSION IDS
    if (((pBootInfo->BootRomVersion == NVBOOT_VERSION(0x35,0x01))
      || (pBootInfo->BootRomVersion == NVBOOT_VERSION(0x35,0x02)))
    &&   (pBootInfo->DataVersion == NVBOOT_VERSION(0x35,0x01))
    &&   (pBootInfo->RcmVersion == NVBOOT_VERSION(0x35,0x01))
    &&   (pBootInfo->BootType == NvBootType_Recovery)
    &&   (pBootInfo->PrimaryDevice == NvBootDevType_Irom))
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

static NvU32 NvBlGetOscillatorDriveStrength( void )
{
    //This function is pretty hacky. Ideally,
    //you want to get this from the ODM, but
    //I didn't want to compile the ODM into nvml.
    //Keeping the default value of POR
    //Refer Bug - 1044151 for more details
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
                frequency = 600;
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

    #if NVBL_USE_PLL_LOCK_BITS

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
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPULP_CMPLX_CLR, CLR_CXRESET0,  1);
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
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CXRESET3,  1);

    NV_CAR_REGW(CLK_RST_PA_BASE, RST_CPUG_CMPLX_CLR, Reg);
}

static void NvBlAvpClockEnableCoresight(void)
{
    NvU32   Reg;        // Scratch register
    const NvBootInfoTable *pBit;
    pBit = (const NvBootInfoTable *)(NV_BIT_ADDRESS);

    if (pBit->BctPtr->SecureJtagControl == 1)
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
    NvU32 RegData;

    RegData = NV_CAR_REGR(CLK_RST_PA_BASE, SCLK_BURST_POLICY);
    RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY,
                    SYS_STATE, RUN, RegData);
    RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY,
                    SWAKEUP_RUN_SOURCE, CLKM, RegData);
    NV_CAR_REGW(CLK_RST_PA_BASE, SCLK_BURST_POLICY, RegData);
    NvBlAvpStallUs(3);
}

static void SetAvpClockToPllP(void)
{
    NvU32 RegData;

    RegData = NV_CAR_REGR(CLK_RST_PA_BASE, SCLK_BURST_POLICY);
    RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY,
                    SYS_STATE, RUN, RegData);
    RegData = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY,
                  SWAKEUP_RUN_SOURCE, PLLP_OUT4, RegData);
    NV_CAR_REGW(CLK_RST_PA_BASE, SCLK_BURST_POLICY, RegData);
}

static void InitPllP(NvBootClocksOscFreq OscFreq)
{
    NvU32 Base, Misc, Reg;
    Base = NV_CAR_REGR(CLK_RST_PA_BASE, PLLP_BASE);

    if (s_FpgaEmulation)
    {
        Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, REF_ENABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BASE_OVRRIDE, DISABLE)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, 0x0)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, DEFAULT);
        Misc = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, DEFAULT);
    }
    else
    {
        SetAvpClockToClkM();
        // DIVP/DIVM/DIVN values taken from arclk_rst.h table for fixed 216 MHz operation.
        switch (OscFreq)
        {
            case NvBootClocksOscFreq_13:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000D0D;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, REF_ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BASE_OVRRIDE, ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN, 0x198)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, 0xD);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 8);
                break;

            case NvBootClocksOscFreq_38_4:
                // Fall through -- 38.4 MHz is identical to 19.2 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 2 for 38.4 MHz.

            case NvBootClocksOscFreq_19_2:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001313;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, REF_ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BASE_OVRRIDE, ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, 0x1)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN, 0xB4)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, 0x16);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 4);
                break;

            case NvBootClocksOscFreq_12:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000C0C;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, REF_ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BASE_OVRRIDE, ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN, 0x198)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, 0x0C);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 8);
                break;

            case NvBootClocksOscFreq_26:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001A1A;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, REF_ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BASE_OVRRIDE, ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN, 0x198)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, 0x1A);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 8);
                break;

            case NvBootClocksOscFreq_16_8:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001111;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, 0x1)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN, 0xB4)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, 0x14);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 4);
                break;

            default:
                Misc = 0; /* Suppress warning */
                NV_ASSERT(!"Bad frequency");
        }
    }

    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_MISC, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_BASE, Base);

    // Assert OUTx_RSTN for pllp_out1,2,3,4 before PLLP enable
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE,PLLP_OUTA);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_RSTN,
              RESET_ENABLE,Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_RSTN,
              RESET_ENABLE,Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_OUTA, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE,PLLP_OUTB);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_RSTN,
              RESET_ENABLE,Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_RSTN,
              RESET_ENABLE,Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_OUTB, Reg);

    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, ENABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_BASE, Base);

#if !NVBL_PLL_BYPASS
    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, DISABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_BASE, Base);

    // Set pllp_out1,2,3,4 to frequencies 48MHz, 48MHz, 102MHz, 102MHz.
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_RATIO, 83) | // 9.6MHz
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_OVRRIDE, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_CLKEN, ENABLE) |
          NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_RATIO, 15) |  // 48MHz
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_OVRRIDE, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_CLKEN, ENABLE);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_OUTA, Reg);

    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_RATIO, 6) |  // 102MHz
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_OVRRIDE, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_CLKEN, ENABLE) |
          NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_RATIO, 6) | // 102MHz
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_OVRRIDE, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_CLKEN, ENABLE);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_OUTB, Reg);

#if NVBL_USE_PLL_LOCK_BITS
    Misc = NV_CAR_REGR(CLK_RST_PA_BASE, PLLP_MISC);
    Misc = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LOCK_ENABLE, ENABLE, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_MISC, Misc);
#endif
#endif
}

//----------------------------------------------------------------------------------------------
static void InitPllX(void)
{
    NvU32               Base;       // PLL Base
    NvU32               Divm;       // Reference
    NvU32               Divn;       // Multiplier
    NvU32               Divp;       // Divider == Divp ^ 2
    NvU32               p;
    NvU32               Val;
    NvU32               OscFreqKhz, RateInKhz, VcoMin = 700000;

    // Is PLL-X already running?
    Base = NV_CAR_REGR(CLK_RST_PA_BASE, PLLX_BASE);
    Base = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, Base);
    if (Base == CLK_RST_CONTROLLER_PLLX_BASE_0_PLLX_ENABLE_ENABLE)
    {
        return;
    }

    OscFreqKhz = NvBlAvpQueryOscillatorFrequency();
    NV_ASSERT(OscFreqKhz); // Invalid frequency

    RateInKhz = NvBlAvpQueryBootCpuFrequency() * 1000;

    if (s_FpgaEmulation)
    {
        OscFreqKhz = 13000; // 13 Mhz for FPGA
        RateInKhz = 600000; // 600 Mhz
    }

    /* Clip vco_min to exact multiple of OscFreq to avoid
       crossover by rounding */
    VcoMin = ((VcoMin + OscFreqKhz- 1) / OscFreqKhz)*OscFreqKhz;

    p = (VcoMin + RateInKhz - 1)/RateInKhz; // Rounding p
    Divp = p - 1;
    Divm = (OscFreqKhz> 19200)? 2 : 1; // cf_max = 19200 Khz
    Divn = (RateInKhz * p * Divm)/OscFreqKhz;

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
    // Disable IDDQ
    Val = NV_CAR_REGR(CLK_RST_PA_BASE, PLLX_MISC_3);
    Val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLX_MISC_3, PLLX_IDDQ, 0x0, Val);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_MISC_3, Val);

    // Wait 2 us
    NvBlAvpStallUs(2);

    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, ENABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_BASE, Base);
}

void NvBlInitPllX_t11x(void)
{
    InitPllX();
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
    NvU32                   AbsFreq;        // Absolute frequency of oscillator
    NvU32                   PllRefDiv;      // PLL reference divider
    NvBlCpuClusterId        CpuClusterId;   // Boot CPU id

    // Get a pointer to the Boot Information Table.
    pBootInfo = (const NvBootInfoTable*)(T11X_BASE_PA_BOOT_INFO);

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
            AbsFreq = 13000000;
            break;

        case NvBootClocksOscFreq_19_2:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (5-1))
                    | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (96-1));
            AbsFreq = 19200000;
            break;

        case NvBootClocksOscFreq_12:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (1-1))
                    | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (12-1));
            AbsFreq = 12000000;
            break;

        case NvBootClocksOscFreq_26:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (1-1))
                    | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (26-1));
            AbsFreq = 26000000;
            break;

        case NvBootClocksOscFreq_16_8:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (5-1))
                    | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (84-1));
            AbsFreq = 16800000;
            break;

        case NvBootClocksOscFreq_38_4:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (5-1))
                    | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (192-1));
            PllRefDiv = CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV2;
            AbsFreq = 38400000;
            break;

        case NvBootClocksOscFreq_48:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (1-1))
                    | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (48-1));
            PllRefDiv = CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV4;
            AbsFreq = 48000000;
            break;
    }

    NV_WRITE32(SYSCTR0_PA_BASE+SYSCTR0_CNTFID0_0, AbsFreq);

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

    // Enable the PPSB_STOPCLK feature to allow SCLK to be run at higher
    // frequencies. See bug 811773.
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, MISC_CLK_ENB);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, MISC_CLK_ENB,
            EN_PPSB_STOPCLK, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, MISC_CLK_ENB, Reg);

    Reg = NV_AHB_ARBC_REGR(AHB_PA_BASE, XBAR_CTRL);
    Reg = NV_FLD_SET_DRF_DEF(AHB_ARBITRATION, XBAR_CTRL,
            PPSB_STOPCLK_ENABLE, ENABLE, Reg);
    NV_AHB_ARBC_REGW(AHB_PA_BASE, XBAR_CTRL, Reg);

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
                | NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_CTRL, XOBP, 0)
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
    }

    // Initialize PLL-P.
    InitPllP(OscFreq);

#if NVBL_USE_PLL_LOCK_BITS

    // Wait for PLL-P to lock.
    do
    {
        Reg = NV_CAR_REGR(CLK_RST_PA_BASE, PLLP_BASE);
    } while (!NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, Reg));

#else

    // Give PLLs time to stabilize.
    NvBlAvpStallUs(NVBOOT_CLOCKS_PLL_STABILIZATION_DELAY);

#endif

    /* Deassert OUTx_RSTN for pllp_out1,2,3,4 after lock bit
     * or waiting for Stabilization Delay (S/W Bug 954659)
     */
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE,PLLP_OUTA);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_RSTN,
              RESET_DISABLE,Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_RSTN,
              RESET_DISABLE,Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_OUTA, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE,PLLP_OUTB);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_RSTN,
              RESET_DISABLE,Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_RSTN,
              RESET_DISABLE,Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_OUTB, Reg);

    SetAvpClockToPllP();

    //-------------------------------------------------------------------------
    // Switch system clock to PLLP_out 4 (108 MHz) MHz, AVP will now run at 108 MHz.
    // This is glitch free as only the source is changed, no special precaution needed.
    //-------------------------------------------------------------------------

    //*((volatile long *)0x60006028) = 0x20002222 ;// SCLK_BURST_POLICY
    Reg = NV_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, SYS_STATE, RUN)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, COP_AUTO_SWAKEUP_FROM_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, CPU_AUTO_SWAKEUP_FROM_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, COP_AUTO_SWAKEUP_FROM_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, CPU_AUTO_SWAKEUP_FROM_IRQ, 0x0)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, SWAKEUP_FIQ_SOURCE, PLLP_OUT4)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, SWAKEUP_IRQ_SOURCE, PLLP_OUT4)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, SWAKEUP_RUN_SOURCE, PLLP_OUT4)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, SCLK_BURST_POLICY, SWAKEUP_IDLE_SOURCE, PLLP_OUT4);
    NV_CAR_REGW(CLK_RST_PA_BASE, SCLK_BURST_POLICY, Reg);

    //*((volatile long *)0x6000602C) = 0x80000000 ;// SUPER_SCLK_DIVIDER
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_SCLK_DIVIDER, SUPER_SDIV_ENB, 0x1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_SCLK_DIVIDER, SUPER_SDIV_DIS_FROM_COP_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_SCLK_DIVIDER, SUPER_SDIV_DIS_FROM_CPU_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_SCLK_DIVIDER, SUPER_SDIV_DIS_FROM_COP_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_SCLK_DIVIDER, SUPER_SDIV_DIS_FROM_CPU_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_SCLK_DIVIDER, SUPER_SDIV_DIVIDEND, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_SCLK_DIVIDER, SUPER_SDIV_DIVISOR, 0x0);
    NV_CAR_REGW(CLK_RST_PA_BASE, SUPER_SCLK_DIVIDER, Reg);

    // Initialize PLL-X.
    InitPllX();

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
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_CPU, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_L, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_H);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_BSEV, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_BSEA, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_VDE, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MPE, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_USB3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_USB2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_EMC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MIPI_CAL, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_UARTC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_I2C2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_CSI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_HDMI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_HSI, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DSI, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_I2C5, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_XIO, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_NOR, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SNOR, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SLC1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_KFUSE, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_FUSE, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_PMC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_STAT_MON, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_KBC, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_APBDMA, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_AHBDMA, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MEM, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_H, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_U);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_XUSB_DEV, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DEV1_OUT, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DEV2_OUT, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SUS_OUT, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_MSENC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_M_DOUBLER_ENB, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_CRAM2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMD, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMC, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMB, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMA, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_TSEC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DSIB, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_I2C_SLOW, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_NAND_SPEED, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DTV, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SOC_THERM, ENABLE, Reg);
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
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_EXTPERIPH3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_EXTPERIPH2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_EXTPERIPH1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_ACTMON, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SPDIF_DOUBLER, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S4_DOUBLER, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S3_DOUBLER, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S2_DOUBLER, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S1_DOUBLER, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_HDA2CODEC_2X, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_DAM2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_DAM1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_DAM0, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_APBIF, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_AUDIO, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SBC6, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SBC5, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2C4, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S4, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_TSENSOR, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_MSELECT, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_3D2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_CPULP, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_CPUG, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_V, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_W);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_EMC1, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_MC1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_DP2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_DDS, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_ENTROPY, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_DISB_LP, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_DISA_LP, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_CILE, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_CILCD, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_CILAB, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_XUSB, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_MIPI_IOBIST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_SATA_IOBIST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_HDMI_IOBIST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_EMC_IOBIST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_PCIE2_IOBIST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_CEC, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_PCIERX5, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_PCIERX4, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_PCIERX3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_PCIERX2, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_PCIERX1, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_PCIERX0, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W, CLK_ENB_HDA2HDMICODEC, ENABLE, Reg);
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
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_USB3_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_USB2_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_EMC_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_MIPI_CAL_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_UARTC_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_I2C2_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_CSI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_HDMI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_HSI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_DSI_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_I2C5_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC3_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_XIO_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC2_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_NOR_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SNOR_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC1_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_KFUSE_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_FUSE_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_STAT_MON_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_APBDMA_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_AHBDMA_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_MEM_RST, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_H, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_U);
    if (s_FpgaEmulation)
    {
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_EMUCIF_RST, DISABLE, Reg);
    }
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_MSENC_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_TSEC_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_DSIB_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_I2C_SLOW_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_NAND_SPEED_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_DTV_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_SOC_THERM_RST, DISABLE, Reg);
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
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_EXTPERIPH3_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_EXTPERIPH2_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_EXTPERIPH1_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_ACTMON_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_ATOMICS_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_HDA2CODEC_2X_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM2_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM1_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM0_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_APBIF_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_AUDIO_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SBC6_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SBC5_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_I2C4_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_I2S4_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_I2S3_RST, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_MSELECT_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_3D2_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_CPULP_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_CPUG_RST, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_V, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_W);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_EMC1_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_MC1_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_DP2_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_DDS_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_ENTROPY_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_XUSB_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_CEC_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_SATACOLD_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W, SWR_HDA2HDMICODEC_RST, ENABLE, Reg);
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
    NvU32   NorSize = 0;        // Size of NOR
    NvBool  IsSdramInitialized; // Nonzero if SDRAM already initialized

    // Get a pointer to the Boot Information Table.
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T11X_BASE_PA_BOOT_INFO);

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

    // Get the default aperture configuration.
    Reg = NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, IROM_LOVEC, MMIO)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, PCIE_A1, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, PCIE_A2, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, PCIE_A3, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, IRAM_RSVD, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, NOR_A1, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, NOR_A2, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, NOR_A3, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, VERIF_RSVD, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, GFX_HOST_RSVD, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, GART_GPU, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, PPSB_RSVD, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, EXTIO_RSVD, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, APB_RSVD, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, AHB_A1, MMIO)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, AHB_A1_RSVD, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, AHB_A2_RSVD, DRAM)
        | NV_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, IROM_HIVEC, MMIO);

#if !NV_TEST_LOADER // Test loader does not support ODM query
    {
        // Get the amount of NOR memory present.
        NvOdmOsOsInfo OsInfo;       // OS information
        NvOsAvpMemset(&OsInfo, 0, sizeof(OsInfo));
        OsInfo.OsType = NvOdmOsOs_Linux;
        NorSize = NvOdmQueryOsMemSize(NvOdmMemoryType_Nor, &OsInfo);
    }
#endif

    if (NorSize != 0)
    {
        Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, NOR_A1, MMIO, Reg);
        if (NorSize > 0x01000000)
        {
            Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, NOR_A2, MMIO, Reg);
            if (NorSize > 0x03000000)
            {
                Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, GLB_AMAP_CFG, NOR_A3, MMIO, Reg);
            }
        }
    }
    NV_PMC_REGW(PMC_PA_BASE, GLB_AMAP_CFG, Reg);

    // Disable any further writes to the address map configuration register.
    Reg = NV_PMC_REGR(PMC_PA_BASE, SEC_DISABLE);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, SEC_DISABLE, AMAP_WRITE, ON, Reg);
    NV_PMC_REGW(PMC_PA_BASE, SEC_DISABLE, Reg);

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

    // Make sure the debug registers are clear.
    Reg = 0;
    NV_MISC_REGW(MISC_PA_BASE, GP_ASDBGREG, Reg);
    NV_MISC_REGW(MISC_PA_BASE, GP_ASDBGREG2, Reg);

    //Set Weak Bias
    NV_PMC_REGW(PMC_PA_BASE, WEAK_BIAS, 0x3ff);

    // Enable errata for 1157520
    Reg = NV_MC_REGR(MC_PA_BASE, EMEM_ARB_OVERRIDE);
    Reg &= ~0x2;
    NV_MC_REGW(MC_PA_BASE, EMEM_ARB_OVERRIDE, Reg);
}

NvBool NvBlAvpInit_T11x(NvBool IsRunningFromSdram)
{
    NvU32               Reg;
    NvBool              IsChipInitialized;
    NvBool              IsLoadedByScript = NV_FALSE;
    NvBootInfoTable*    pBootInfo = (NvBootInfoTable*)(T11X_BASE_PA_BOOT_INFO);

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
    NvOsAvpDebugPrintf("Bootloader-AVP Init at (time stamp): %d us\n", g_BlAvpTimeStamp);
#endif
    // Do i2c reset and bus clear for instance 5
    NvDdkI2cClearBus(NvDdkI2c5);

    // Disable all module clocks except which are required to boot.
    NvBlAvpClockDeInit();

    // Initialize memory controller.
    NvBlMemoryControllerInit(IsChipInitialized);

    // Set power gating timer multiplier -- see bug 966323
    Reg = NV_PMC_REGR(PMC_PA_BASE, PWRGATE_TIMER_MULT);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, PWRGATE_TIMER_MULT, MULT, EIGHT, Reg);
    NV_PMC_REGW(PMC_PA_BASE, PWRGATE_TIMER_MULT, Reg);

    return IsChipInitialized;
}

#ifdef USED_BY_RTAPI
// This finction is a simplified version of NvBlAvpInit_T11x, and it is only used
// by RTAPI flow for skipping all BIT setting. (there is no BIT used in RTAPI flow)
//
// NOTE:
//  (1) This function is called by:
//               tegra/core-private/drivers/nvtestloader/testloader.c
//  (2) This function will be removed when RTAPI flow is merged with nvflash flow.
//  (3) macro "USED_BY_RTAPI" is defined in RTAPI flow, in the file:
//               tegra/core-private/drivers/nvtestloader/Makefile
NvBool NvBlAvpInitFromScript_T11x()
{
    NvBool              IsChipInitialized = NV_TRUE;
    NvBool              IsLoadedByScript = NV_TRUE;
    NvBootInfoTable*    pBootInfo = (NvBootInfoTable*)(T11X_BASE_PA_BOOT_INFO);

    NvBlInitI2cPinmux();
    NvBlReadBoardInfo(NvBoardType_ProcessorBoard, &g_BoardInfo);
#ifdef PINMUX_INIT
    // initialize pinmuxes
    NvOdmPinmuxInit(g_BoardInfo.BoardId);
#endif

    pBootInfo->OscFrequency = NvBlAvpMeasureOscFreq();

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
#endif

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

static NvBool NvBlAvpIsClampRemoved(NvU32 Mask)
{
    // Get power gate status.
    NvU32   Reg = NV_PMC_REGR(PMC_PA_BASE, CLAMP_STATUS);

    if ((Reg & Mask) == Mask)
    {
        return NV_FALSE;
    }

    return NV_TRUE;
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

        // Wait for the clamp status to be cleared.
        while (!NvBlAvpIsClampRemoved(status))
        {
            // Do nothing
        }
    }
}

// Function "NvBlAvpToggleJtagClk" is a workaround for RAM repair
// It shall not affect boards without RAM repair rework.
static void NvBlAvpToggleJtagClk(void)
{
    volatile NvU32 reg;
    NvU32 i;

    reg = NV_READ32(PINMUX_BASE + PINMUX_AUX_GMI_CS7_N_0);
    reg = NV_FLD_SET_DRF_DEF(PINMUX_AUX, GMI_CS7_N, TRISTATE, NORMAL, reg);
    NV_WRITE32(PINMUX_BASE + PINMUX_AUX_GMI_CS7_N_0, reg);

    NV_GPIO_REGW(GPIO3_I_PA_BASE, MSK_CNF, 0x4040);
    NV_GPIO_REGW(GPIO3_I_PA_BASE, MSK_OE, 0x4040);

    for (i=0; i<16; i++) {
        NV_GPIO_REGW(GPIO3_I_PA_BASE, MSK_OUT, 0x4000);
        NvBlAvpStallUs(1);
        NV_GPIO_REGW(GPIO3_I_PA_BASE, MSK_OUT, 0x4040);
        NvBlAvpStallUs(1);
    }

    NV_GPIO_REGW(GPIO3_I_PA_BASE, MSK_OE, 0x4000);
}

#define POWER_PARTITION(x)  \
    NvBlAvpPowerPartition(PMC_PWRGATE_STATUS(x), PMC_PWRGATE_TOGGLE(x))

static void  NvBlAvpRamRepair(void)
{
    NvU32   Reg;        // Scratch reg

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
}

static void  NvBlAvpPowerUpCpu(void)
{
    // Run the WAR irrespective of fast or slow cluster.
    // Need not do it again while switching to fast cluster.
    if (NvBlRamRepairRequired())
        NvBlAvpToggleJtagClk();

    // Are we booting to the fast cluster?
    if (NvBlAvpQueryFlowControllerClusterId() == NvBlCpuClusterId_Fast)
    {
        // TODO: Set CPU Power Good Timer

        // Power up the fast cluster rail partition.
        POWER_PARTITION(CRAIL);

        if (NvBlRamRepairRequired())
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
static void NvBlAvpEnableCpuPowerRail(void)
{
    NvU32 Reg;        // Scratch reg
    NvU32 BoardId = 0;

#define DALMORE_E1613_BOARDID   0x64D
#define DALMORE_E1611_BOARDID   0x64B
#define ROTH_BOARDID            0xA00
#define PLUTO_E1580_BOARDID     0x62C
#define PLUTO_E1577_BOARDID     0x629
#define VARUNA_E1575_BOARDID    0x627
#define CERES_E1680_BOARDID     0x690

    if (!s_FpgaEmulation) {
#if USE_ROTH_BOARD_ID
        BoardId = ROTH_BOARDID;
#else
        BoardId = g_BoardInfo.BoardId;
#endif
    }
#if __GNUC__
    NvOsAvpDebugPrintf("Board Id = 0x%04x\n", BoardId);
#endif
    /*
     * Program PMIC TPS51632
     *
     * slave address 7b 1000011 (3 LSB selected by SLEWA which is 0.8v on
     * Dalmore E1613.
     *
     * The cold boot voltage is selected by BRAMP  which is 0v on Dalmore,
     * so we need to program VSR to 1v
     */

#define DALMORE_TPS51632_I2C_ADDR   0x86
#define ROTH_TPS51632_I2C_ADDR      0x86
#define PLUTO_TPS65913_I2C_ADDR     0xB0
#define MACALLAN_TPS65913_I2C_ADDR  0xB0
#define PLUTO_CPUPWRREQ_EN          0x01
#define CERES_MAX77660_I2C_ADDR     0x46

#if __GNUC__
    NvOsAvpDebugPrintf("Dummy read for TPS65913\n");
#endif
    NvDdkI2cDummyRead(NvDdkI2c5, MACALLAN_TPS65913_I2C_ADDR);

    if ((s_FpgaEmulation == NV_FALSE)
        && (NvBlAvpQueryBootCpuClusterId() == NvBlCpuClusterId_Fast))
    {
        if (BoardId == DALMORE_E1611_BOARDID ||
            BoardId == DALMORE_E1613_BOARDID)
        {
            NvBlPmuWrite(DALMORE_TPS51632_I2C_ADDR, 0x00, 0x55);
        }
        else if (BoardId == PLUTO_E1580_BOARDID ||
                 BoardId == VARUNA_E1575_BOARDID ||
                 BoardId == PLUTO_E1577_BOARDID)
        {
#ifdef PLUTO_CPUPWRREQ_EN
            NvBlPmuWrite(PLUTO_TPS65913_I2C_ADDR, 0x20, 0x01);
            NvBlPmuWrite(PLUTO_TPS65913_I2C_ADDR, 0x23, 0x40);
            NvBlPmuWrite(PLUTO_TPS65913_I2C_ADDR, 0x24, 0x01);
#else
            NvBlPmuWrite(PLUTO_TPS65913_I2C_ADDR, 0x20, 0x05);
            NvBlPmuWrite(PLUTO_TPS65913_I2C_ADDR, 0x23, 0x40);
            NvBlPmuWrite(PLUTO_TPS65913_I2C_ADDR, 0x24, 0x05);
#endif

        }
        else if (BoardId == ROTH_BOARDID)
        {
            NvBlPmuWrite(ROTH_TPS51632_I2C_ADDR, 0x00, 0x55);
            /* Enable SMPS10 for CPU_PWR_RAIL */
            NvBlPmuWrite(0xB0, 0x3C, 0x0D);
        }
    }
    //Hack for enabling necessary rails for display, Need to be removed
    //once PWR I2C driver is fixed for CPU side
    if (BoardId == CERES_E1680_BOARDID)
    {
        /* SW1 - VDD_1V8_LCD_S and SW5 - AVDD_DSI_CSI*/
        NvBlPmuWrite(CERES_MAX77660_I2C_ADDR, 0x43, 0x09);
        /* BUCK4,3,2,1 Normal Mode */
        NvBlPmuWrite(CERES_MAX77660_I2C_ADDR, 0x37, 0xFF);
         /* BUCK2 is not a part of FPS */
        NvBlPmuWrite(CERES_MAX77660_I2C_ADDR, 0x28, 0x7F);
        /* RAMP_BUCK=25mV/us, ADE=1, ROVS_EN=1, FSRADE=1*/
        NvBlPmuWrite(CERES_MAX77660_I2C_ADDR, 0x4F, 0x4B);
        /* BUCK2 = 11v -> VDD_CPU */
        NvBlPmuWrite(CERES_MAX77660_I2C_ADDR, 0x47, 0x50);
        /* Reset WDT Counter*/
        NvBlPmuWrite(CERES_MAX77660_I2C_ADDR, 0x20, 0x01);
    }
    else if (BoardId == DALMORE_E1613_BOARDID)
    {
        NvBlPmuWrite(0x78, 0x2D, 0x08);
        NvBlPmuWrite(0x78, 0x2E, 0x00);
        //MIPI_RAIL
        NvBlPmuWrite(0x78, 0x4B, 0x00);
        //VOUT1 (FET1) : VDD_LCD_BL_0
        NvBlPmuWrite(0x90, 0x0F, 0x03);
        //VOUT4 (FET4) : AVDD_LCD
        NvBlPmuWrite(0x90, 0x12, 0x03);
    }
    else if (BoardId == PLUTO_E1580_BOARDID ||
             BoardId == VARUNA_E1575_BOARDID ||
             BoardId == PLUTO_E1577_BOARDID)
    {
        NvBlPmuWrite(0xB0, 0x3B, 0xEA);
        NvBlPmuWrite(0xB0, 0x38, 0x01);

        /* LDO2 for AVDD_LCD */
        NvBlPmuWrite(0xB0, 0x53, 0x27);
        NvBlPmuWrite(0xB0, 0x52, 0x01);

        NvBlPmuWrite(0xB0, 0xFB, 0x00);
        /* Init steps for VDD_1V8_LCD_S */
        /* Driver PMU GPIO4 High */
        NvBlPmuWrite(0xB2, 0X81, 0x10);
        NvBlPmuWrite(0xB2, 0X82, 0x10);

        /* Power up  MIPI Rail  */
        /* Power source AVDD_1V2_LDO3 */
        /* LDO3_CTRL */
        NvBlPmuWrite(0xB0, 0X54, 0x01);
        /* LDO3_VOLTAGE */
        NvBlPmuWrite(0xB0, 0X55, 0x07);
    }

    if (BoardId == DALMORE_E1613_BOARDID ||
        BoardId == PLUTO_E1580_BOARDID ||
        BoardId == VARUNA_E1575_BOARDID ||
        BoardId == PLUTO_E1577_BOARDID)
    {
        /* Use GMI_CS1_N as CPUPWRGOOD -- is this really the SOC_THERM_OC2 ??? */
        Reg = NV_PMC_REGR(PMC_PA_BASE, CNTRL);
        Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRGOOD_SEL, SOC_THERM_OC2, Reg);
        NV_PMC_REGW(PMC_PA_BASE, CNTRL, Reg);

        /* set pinmux for GMI_CS1_N */
        Reg = NV_READ32(MISC_PA_BASE + PINMUX_AUX_GMI_CS1_N_0);
        Reg = NV_FLD_SET_DRF_DEF(PINMUX, AUX_GMI_CS1_N, PM, SOC, Reg);
        Reg = NV_FLD_SET_DRF_DEF(PINMUX, AUX_GMI_CS1_N, TRISTATE, NORMAL, Reg);
        Reg = NV_FLD_SET_DRF_DEF(PINMUX, AUX_GMI_CS1_N, E_INPUT, ENABLE, Reg);
        NV_WRITE32(MISC_PA_BASE + PINMUX_AUX_GMI_CS1_N_0, Reg);
    }

    /* set CPUPWRGOOD_TIMER -- APB clock is 1/2 of SCLK, 102M, 0x26E8F0 is 25ms */
    Reg = 0x26E8F0;
    NV_PMC_REGW(PMC_PA_BASE, CPUPWRGOOD_TIMER, Reg);

    /* enable CPUPWRGOOD feedback */
    Reg = NV_PMC_REGR(PMC_PA_BASE, CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRGOOD_EN, ENABLE, Reg);
    // test test test !!! NV_PMC_REGW(PMC_PA_BASE, CNTRL, Reg);

    /* polarity set to 0 (normal) */
    Reg = NV_PMC_REGR(PMC_PA_BASE, CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRREQ_POLARITY, NORMAL, Reg);
    NV_PMC_REGW(PMC_PA_BASE, CNTRL, Reg);

    /* CPUPWRREQ_OE set to 1 (enable) */
    Reg = NV_PMC_REGR(PMC_PA_BASE, CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRREQ_OE, ENABLE, Reg);
    NV_PMC_REGW(PMC_PA_BASE, CNTRL, Reg);

   /*
    * Set CLK_RST_CONTROLLER_CPU_SOFTRST_CTRL2_0_CAR2PMC_CPU_ACK_WIDTH to 408
    * to satisfy the requirement of having at least 16 CPU clock cycles before
    * clamp removal.
    */
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE,CPU_SOFTRST_CTRL2);
    Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CPU_SOFTRST_CTRL2, CAR2PMC_CPU_ACK_WIDTH, 408, Reg);
    Reg = NV_CAR_REGW(CLK_RST_PA_BASE,CPU_SOFTRST_CTRL2, Reg);

    NvBlDeinitPmuSlaves();
}

static void NvBlAvpSetCpuResetVectorT114(NvU32 ResetVector)
{
    NvU32  imme, inst;

    inst = imme = ResetVector & 0xFFFF;
    inst &= 0xFFF;
    inst |= ((imme >> 12) << 16);
    inst |= 0xE3000000;
    NV_WRITE32(0x4003FFF0, inst); // MOV R0, #LSB16(ResetVector)

    imme = (ResetVector >> 16) & 0xFFFF;
    inst = imme & 0xFFF;
    inst |= ((imme >> 12) << 16);
    inst |= 0xE3400000;
    NV_WRITE32(0x4003FFF4, inst); // MOVT R0, #MSB16(ResetVector)

    NV_WRITE32(0x4003FFF8, 0xE12FFF10); // BX R0

    inst = (NvU32)-20;
    inst >>= 2;
    inst &= 0x00FFFFFF;
    inst |= 0xEA000000;
    NV_WRITE32(0x4003FFFC, inst); // B -12

    NvBlAvpSetCpuResetVector(ResetVector);
}

//------------------------------------------------------------------------------
void NvBlStartCpu_T11x(NvU32 ResetVector)
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
    NvBlAvpSetCpuResetVectorT114(ResetVector);

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
    static NvS8 isNv3pSignatureSet = -1;

    if (isNv3pSignatureSet == 0 || isNv3pSignatureSet == 1)
        return isNv3pSignatureSet;

    // Yes, is there a valid BCT?
    if (pBootInfo->BctValid)
    {
        // Is the address valid?
        if (pBootInfo->SafeStartAddr)
        {
            // Get the address where the signature is stored. (Safe start - 4)
            SafeStartAddress =
                (NvU32 *)(pBootInfo->SafeStartAddr - sizeof(NvU32));
            // compare signature...if this is coming from miniloader

            isNv3pSignatureSet = 0;
            if (*(SafeStartAddress) == Nv3pSignature)
            {
                //if yes then modify the safe start address
                //with correct start address (Safe address -4)
                isNv3pSignatureSet = 1;
                (pBootInfo->SafeStartAddr) -= sizeof(NvU32);
                return NV_TRUE;
            }

        }
    }

    // return false on any failure.
    return NV_FALSE;
}

void NvBlConfigFusePrivateTZ(void)
{
    NvU32 reg=0;
    NvU8 *ptr = NULL;
    NvBctAuxInfo *auxInfo;

    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T11X_BASE_PA_BOOT_INFO);

    ptr = pBootInfo->BctPtr->CustomerData + sizeof(NvBctCustomerPlatformData);
    ptr = (NvU8*)((((NvU32)ptr + sizeof(NvU32) - 1) >> 2) << 2);
    auxInfo = (NvBctAuxInfo *)ptr;

    if (auxInfo->StickyBit & 0x1)
    {
        reg = AOS_REGR(FUSE_PA_BASE + FUSE_PRIVATEKEYDISABLE_0);
        reg |= NV_DRF_NUM(FUSE, PRIVATEKEYDISABLE, TZ_STICKY_BIT_VAL, 1);
        AOS_REGW(FUSE_PA_BASE + FUSE_PRIVATEKEYDISABLE_0, reg);
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
__asm void NvBlStartUpAvp_T11x( void )
{
    CODE32
    PRESERVE8

    IMPORT NvBlAvpInit_T11x
    IMPORT NvBlStartCpu_T11x
    IMPORT NvBlAvpHalt

    //------------------------------------------------------------------
    // Initialize the AVP, clocks, and memory controller.
    //------------------------------------------------------------------

    // The SDRAM is guaranteed to be on at this point
    // in the nvml environment. Set r0 = 1.
    MOV     r0, #1
    BL      NvBlAvpInit_T11x

    //------------------------------------------------------------------
    // Start the CPU.
    //------------------------------------------------------------------

    LDR     r0, =ColdBoot               //; R0 = reset vector for CPU
    BL      NvBlStartCpu_T11x

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

// this function inits the MPCORE and then jumps to the MPCORE
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
#ifdef BUILD_FOR_COSIM
    //always jump to etry point. AVP is not modeled in COSIM
    B       NV_AOS_ENTRY_POINT                   //; yep, we are the CPU
#else
    BEQ     NV_AOS_ENTRY_POINT                   //; yep, we are the CPU
#endif

    //==================================================================
    // AVP Initialization follows this path
    //==================================================================

    LDR     sp, =NVAP_LIMIT_PA_IRAM_AVP_EARLY_BOOT_STACK
    // leave in some symbols for release debugging
    LDR     r3, =0xDEADBEEF
    STR     r3, [sp, #-4]!
    STR     r3, [sp, #-4]!
    B       NvBlStartUpAvp_T11x
}
#else
NV_NAKED void NvBlStartUpAvp_T11x( void )
{
    //;------------------------------------------------------------------
    //; Initialize the AVP, clocks, and memory controller.
    //;------------------------------------------------------------------

    asm volatile(
    //The SDRAM is guaranteed to be on at this point
    //in the nvml environment. Set r0 = 1.
    "MOV     r0, #1                          \n"
    "BL      NvBlAvpInit_T11x                \n"
    //;------------------------------------------------------------------
    //; Start the CPU.
    //;------------------------------------------------------------------
    "LDR     r0, =ColdBoot                   \n"//; R0 = reset vector for CPU
    "BL      NvBlStartCpu_T11x               \n"
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
    "B       NvBlStartUpAvp_T11x                                    \n"
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

