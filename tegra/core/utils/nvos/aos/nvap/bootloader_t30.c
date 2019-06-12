/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "bootloader_t30.h"
#include "nvassert.h"
#include "aos.h"
#include "aos_common.h"
#include "avp_gpio.h"
#include "nvuart.h"
#include "aos_avp_pmu.h"
#include "aos_avp_board_info.h"
#include "nvbl_query.h"

#if !__GNUC__
#pragma arm
#endif

#define PWRI2C_CLOCK_CYCLES 9
#define EEPROM_I2C_SLAVE_ADDRESS  0xAC
#define EEPROM_BOARDID_LSB_ADDRESS 0x4
#define EEPROM_BOARDID_MSB_ADDRESS 0x5
#define EEPROM_FAB_ADDRESS 0x8
#define BOARD_FAB_A00 0x00
#define BOARD_FAB_A01 0x01
#define BOARD_FAB_A02 0x02
#define BOARD_FAB_A03 0x03

/* FIXME: remove - bug 784619 */
#define PMU_IGNORE_PWRREQ 1

extern NvU32 *g_ptimerus;
extern NvU32 g_BlAvpTimeStamp;
extern NvU32 s_enablePowerRail;
extern NvU32 e_enablePowerRail;
extern NvU32 proc_tag;
extern NvU32 cpu_boot_stack;
extern NvU32 entry_point;
extern NvU32 avp_boot_stack;
extern NvU32 deadbeef;
extern NvU32 avp_cache_settings;
extern NvU32 avp_cache_config;

extern NvBoardInfo g_BoardInfo;

static NvBlCpuClusterId NvBlAvpQueryBootCpuClusterId(void)
{
    NvU32   Reg;            // Scratch register
    NvU32   Fuse;           // Fuse register

    // Read the fuse register containing the CPU capabilities.
    Fuse = NV_FUSE_REGR(FUSE_PA_BASE, SKU_DIRECT_CONFIG);

    // Read the bond-out register containing the CPU capabilities.
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, BOND_OUT_V);

    // Are there any CPUs present in the G complex?
    if ((!NV_DRF_VAL(CLK_RST_CONTROLLER, BOND_OUT_V, BOND_OUT_CPUG, Reg))
     && (!NV_DRF_VAL(FUSE, SKU_DIRECT_CONFIG, DISABLE_ALL, Fuse)))
    {
        return NvBlCpuClusterId_G;
    }

    // Is there a CPU present in the LP complex?
    if (!NV_DRF_VAL(CLK_RST_CONTROLLER, BOND_OUT_V, BOND_OUT_CPULP, Reg))
    {
        return NvBlCpuClusterId_LP;
    }

    return NvBlCpuClusterId_Unknown;
}

static NvBool NvBlIsValidBootRomSignature(const NvBootInfoTable*  pBootInfo)
{
    // Look at the beginning of IRAM to see if the boot ROM placed a Boot
    // Information Table (BIT) there.
    if (((pBootInfo->BootRomVersion == NVBOOT_VERSION(3, 1))
      || (pBootInfo->BootRomVersion == NVBOOT_VERSION(3, 2)))
    &&   (pBootInfo->DataVersion    == NVBOOT_VERSION(3, 1))
    &&   (pBootInfo->RcmVersion     == NVBOOT_VERSION(3, 1))
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
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T30_BASE_PA_BOOT_INFO);

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
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T30_BASE_PA_BOOT_INFO);

    // Look at the beginning of IRAM to see if the boot ROM placed a Boot
    // Information Table (BIT) there.
    if (((pBootInfo->BootRomVersion == NVBOOT_VERSION(3, 1))
      || (pBootInfo->BootRomVersion == NVBOOT_VERSION(3, 2)))
    &&   (pBootInfo->DataVersion    == NVBOOT_VERSION(3, 1))
    &&   (pBootInfo->RcmVersion     == NVBOOT_VERSION(3, 1))
    &&   (pBootInfo->BootType       == NvBootType_Recovery)
    &&   (pBootInfo->PrimaryDevice  == NvBootDevType_Irom))
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

static void NvBlAvpHaltCpu(NvBool halt)
{
    NvU32   Reg;

    if (halt)
    {
        Reg = NV_DRF_DEF(FLOW_CTLR, HALT_CPU_EVENTS, MODE, FLOW_MODE_STOP);
    }
    else
    {
        Reg = NV_DRF_DEF(FLOW_CTLR, HALT_CPU_EVENTS, MODE, FLOW_MODE_NONE);
    }

    NV_FLOW_REGW(FLOW_PA_BASE, HALT_CPU_EVENTS, Reg);
}

static NvU32 NvBlGetOscillatorDriveStrength( void )
{
    return 0xf;
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
            if (NvBlAvpQueryFlowControllerClusterId() == NvBlCpuClusterId_G)
            {
                frequency = 700;
            }
            else
            {
                frequency = 400;
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

static void NvBlAvpEnableCpuClock(NvBool enable)
{
    NvU32   Reg;        // Scratch reg
    NvU32   Clk;        // Scratch reg

    //-------------------------------------------------------------------------
    // NOTE:  Regardless of whether the request is to enable or disable the CPU
    //        clock, every processor in the CPU complex except the master (CPU
    //        0) will have it's clock stopped because the AVP only talks to the
    //        master. The AVP, it does not know, nor does it need to know that
    //        there are multiple processors in the CPU complex.
    //-------------------------------------------------------------------------

    // Always halt CPU 1-3 at the flow controller so that in uni-processor
    // configurations the low-power trigger conditions will work properly.
    Reg = NV_DRF_DEF(FLOW_CTLR, HALT_CPU1_EVENTS, MODE, FLOW_MODE_STOP);
    NV_FLOW_REGW(FLOW_PA_BASE, HALT_CPU1_EVENTS, Reg);
    NV_FLOW_REGW(FLOW_PA_BASE, HALT_CPU2_EVENTS, Reg);
    NV_FLOW_REGW(FLOW_PA_BASE, HALT_CPU3_EVENTS, Reg);

    // Enabling clock?
    if (enable)
    {
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
            | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CWAKEUP_FIQ_SOURCE, PLLX_OUT0)
            | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CWAKEUP_IRQ_SOURCE, PLLX_OUT0)
            | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CWAKEUP_RUN_SOURCE, PLLX_OUT0)
            | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLK_BURST_POLICY, CWAKEUP_IDLE_SOURCE, PLLX_OUT0);
        NV_CAR_REGW(CLK_RST_PA_BASE, CCLK_BURST_POLICY, Reg);
    }

    //*((volatile long *)0x60006024) = 0x80000000 ;// SUPER_CCLK_DIVIDER
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_ENB, 0x1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIS_FROM_COP_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIS_FROM_CPU_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIS_FROM_COP_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIS_FROM_CPU_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIVIDEND, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLK_DIVIDER, SUPER_CDIV_DIVISOR, 0x0);
    NV_CAR_REGW(CLK_RST_PA_BASE, SUPER_CCLK_DIVIDER, Reg);

    // Stop the clock to all CPUs.
    Clk = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_CPU_CMPLX_SET, SET_CPU0_CLK_STP, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_CPU_CMPLX_SET, SET_CPU1_CLK_STP, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_CPU_CMPLX_SET, SET_CPU2_CLK_STP, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_CPU_CMPLX_SET, SET_CPU3_CLK_STP, 1);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_CPU_CMPLX_SET, Clk);

    // Enable the CPU0 clock if requested.
    if (enable)
    {
        Clk = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_CPU_CMPLX_CLR, CLR_CPU0_CLK_STP, 1);
        NV_CAR_REGW(CLK_RST_PA_BASE, CLK_CPU_CMPLX_CLR, Clk);
    }

    // Always enable the main CPU complex clock.
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_ENB_L_SET, SET_CLK_ENB_CPU, 1);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_ENB_L_SET, Reg);
}

static void NvBlAvpResetCpu(NvBool reset)
{
    NvU32   Reg;    // Scratch reg
    NvU32   Cpu;    // Scratch reg
    NvU32 BoardId;

    /* Read the board Id */
    BoardId = g_BoardInfo.BoardId;

    //-------------------------------------------------------------------------
    // NOTE:  Regardless of whether the request is to hold the CPU in reset or
    //        take it out of reset, every processor in the CPU complex except
    //        the master (CPU 0) will be held in reset because the AVP only
    //        talks to the master. The AVP does not know, nor does it need to
    //        know, that there are multiple processors in the CPU complex.
    //-------------------------------------------------------------------------

    // Hold CPU 1-3 in reset.
    Cpu = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_CPURESET1, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_DBGRESET1, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_DERESET1,  1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_CPURESET2, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_DBGRESET2, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_DERESET2,  1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_CPURESET3, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_DBGRESET3, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_DERESET3,  1);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_CPU_CMPLX_SET, Cpu);

    if (reset)
    {
        // Place CPU0 into reset.
        Cpu = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_CPURESET0, 1)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_DBGRESET0, 1)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_SET, SET_DERESET0,  1);
        NV_CAR_REGW(CLK_RST_PA_BASE, RST_CPU_CMPLX_SET, Cpu);

        // Enable master CPU reset.
        Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_ENB_L_SET, SET_CLK_ENB_CPU, 1);
        NV_CAR_REGW(CLK_RST_PA_BASE, CLK_ENB_L_SET, Reg);
    }
    else
    {
        // Take CPU0 out of reset.
        Cpu = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_CLR, CLR_CPURESET0, 1)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_CLR, CLR_DBGRESET0, 1)
            | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPU_CMPLX_CLR, CLR_DERESET0,  1);
        NV_CAR_REGW(CLK_RST_PA_BASE, RST_CPU_CMPLX_CLR, Cpu);

        // Disable master CPU reset.
        Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_L_CLR, CLR_CPU_RST, 1);
        NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEV_L_CLR, Reg);
    }
}

static void NvBlAvpClockEnableCorsight(NvBool enable)
{
    NvU32   Reg;        // Scratch register

    if (enable)
    {
        // Put CoreSight on PLLP_OUT0 (216 MHz) and divide it down by 1.5
        // giving an effective frequency of 144MHz.

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
            /* Clock divider request for 136MHz would setup CSITE clock as
             * 144MHz for PLLP base 216MHz and 136MHz for PLLP base 408MHz.
             */
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
    else
    {
        // Disable CoreSight clock and hold it in reset.
        Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_ENB_U_CLR, CLR_CLK_ENB_CSITE, 1);
        NV_CAR_REGW(CLK_RST_PA_BASE, CLK_ENB_U_CLR, Reg);

        Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_U_SET, SET_CSITE_RST, 1);
        NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEV_U_SET, Reg);
    }
}

#if (NVBL_PLLC_KHZ > 0)
static void InitPllC(NvBootClocksOscFreq OscFreq)
{
    NvU32   Base;
    NvU32   Misc;
    NvU32   Divn;   // Multiplier
    NvU32   Divp;   // Divider == Divp ^ 2

    Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_MISC, PLLC_CPCON, 8);

    if (s_FpgaEmulation)
    {
        Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_BYPASS, ENABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_ENABLE, DISABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_REF_DIS, REF_ENABLE)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_LOCK, 0x0)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVP, 0x0)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVN, 0x258)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVM, 0xD);
    }
    else
    {
        Divp = 0;
        Divn = NVBL_PLLC_KHZ / 1000;

        if (Divn <= 500)
        {
            Divp = 1;
            Divn <<= Divp;
        }

        // Program PLL-C.
        switch (OscFreq)
        {
            case NvBootClocksOscFreq_13:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000D0D;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVM, 0xD);
                #endif
                break;

            case NvBootClocksOscFreq_38_4:
                // Fall through -- 38.4 MHz is identical to 19.2 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 2 for 38.4 MHz.

            case NvBootClocksOscFreq_19_2:
                // NOTE: With a 19.2 or 38.4 MHz oscillator, the PLL will run
                //       1.05% faster than the target frequency because we cheat
                //       on the math to make this simple.
                #if NVBL_PLL_BYPASS
                    Base = 0x80001313;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVM, 0x13);
                #endif
                break;

            case NvBootClocksOscFreq_48:
                // Fall through -- 48 MHz is identical to 12 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 4 for 48 MHz.

            case NvBootClocksOscFreq_12:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000C0C;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVM, 0xC);
                #endif
                break;

            case NvBootClocksOscFreq_26:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001A1A;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVM, 0x1A);
                #endif
                break;

            case NvBootClocksOscFreq_16_8:
                // NOTE: With a 16.8 MHz oscillator, the PLL will run 1.18%
                //       slower than the target frequency because we cheat
                //       on the math to make this simple.
                #if NVBL_PLL_BYPASS
                    Base = 0x80001111;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_DIVM, 0x11);
                #endif
                break;

            default:
                NV_ASSERT(!"Bad frequency");
        }
    }

    NV_CAR_REGW(CLK_RST_PA_BASE, PLLC_MISC, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLC_BASE, Base);

    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_ENABLE, ENABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLC_BASE, Base);

#if !NVBL_PLL_BYPASS
    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLC_BASE, PLLC_BYPASS, DISABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLC_BASE, Base);

#if NVBL_USE_PLL_LOCK_BITS
    Misc = NV_CAR_REGR(CLK_RST_PA_BASE, PLLC_MISC);
    Misc = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLC_MISC, PLLC_LOCK_ENABLE, ENABLE, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLC_MISC, Misc);
#endif
#endif
}
#endif // (NVBL_PLLC_KHZ > 0)

static void InitPllM(NvBootClocksOscFreq OscFreq)
{
    NvU32   Base;
    NvU32   Misc;
    NvU32   Divn;   // Multiplier
    NvU32   Divp;   // Divider == Divp ^ 2

    // Set charge pump to 4 uA.
    Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC, PLLM_CPCON, 8);

    if (s_FpgaEmulation)
    {
        Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_BYPASS, ENABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_ENABLE, DISABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_REF_DIS, REF_ENABLE)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_LOCK, 0x0)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVP, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, DEFAULT);
    }
    else
    {
        Divp = 1;
        Divn = (NVBL_PLLM_KHZ / 1000) << Divp;

        switch (OscFreq)
        {
            case NvBootClocksOscFreq_13:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000D0D;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, 0xD);
                #endif
                break;

            case NvBootClocksOscFreq_38_4:
                // Fall through -- 38.4 MHz is identical to 19.2 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 2 for 38.4 MHz.

            case NvBootClocksOscFreq_19_2:
                // NOTE: With a 19.2 MHz oscillator, the PLL will run 1.05% faster
                //       than the target frequency.
                #if NVBL_PLL_BYPASS
                    Base = 0x80001313;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, 0x13);
                #endif
                break;

            case NvBootClocksOscFreq_48:
                // Fall through -- 48 MHz is identical to 12 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 4 for 48 MHz.

            case NvBootClocksOscFreq_12:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000C0C;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, 0xC);
                #endif
                break;

            case NvBootClocksOscFreq_26:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001A1A;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, 0x1A);
                #endif
                break;

            case NvBootClocksOscFreq_16_8:
                // NOTE: With a 16.8 MHz oscillator, the PLL will run 1.18%
                //       slower than the target frequency because we cheat
                //       on the math to make this simple.
                #if NVBL_PLL_BYPASS
                    Base = 0x80001111;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, 0x11);
                #endif
                break;

            default:
                Base = 0; /* Suppress warning */
                NV_ASSERT(!"Bad frequency");
        }
    }

    NV_CAR_REGW(CLK_RST_PA_BASE, PLLM_MISC, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLM_BASE, Base);

    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_ENABLE, ENABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLM_BASE, Base);

#if !NVBL_PLL_BYPASS
    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_BYPASS, DISABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLM_BASE, Base);

#if NVBL_USE_PLL_LOCK_BITS
    Misc = NV_CAR_REGR(CLK_RST_PA_BASE, PLLM_MISC);
    Misc = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLM_MISC, PLLM_LOCK_ENABLE, ENABLE, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLM_MISC, Misc);
#endif
#endif
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

    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, ENABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_BASE, Base);
    NvBlAvpStallUs(301);

#if !NVBL_PLL_BYPASS
    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, DISABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_BASE, Base);

    // Set pllp_out1,2,3,4 to frequencies 48MHz, 48MHz, 102MHz, 102MHz.
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_RATIO, 83) | // 9.6MHz
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_OVRRIDE, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_CLKEN, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT2_RSTN,
              RESET_DISABLE) |
          NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_RATIO, 15) |  // 48MHz
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_OVRRIDE, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_CLKEN, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTA, PLLP_OUT1_RSTN,
              RESET_DISABLE);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_OUTA, Reg);

    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_RATIO, 6) |  // 102MHz
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_OVRRIDE, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_CLKEN, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT4_RSTN,
              RESET_DISABLE) |
          NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_RATIO, 6) | // 102MHz
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_OVRRIDE, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_CLKEN, ENABLE) |
          NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_OUTB, PLLP_OUT3_RSTN,
              RESET_DISABLE);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_OUTB, Reg);

#if NVBL_USE_PLL_LOCK_BITS
    Misc = NV_CAR_REGR(CLK_RST_PA_BASE, PLLP_MISC);
    Misc = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LOCK_ENABLE, ENABLE, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLP_MISC, Misc);
#endif
#endif
}


//----------------------------------------------------------------------------------------------
static void InitPllU(NvBootClocksOscFreq OscFreq)
{
    NvU32   Base;
    NvU32   Misc;
    NvU32   isEnabled;
    NvU32   isBypassed;

    Base = NV_CAR_REGR(CLK_RST_PA_BASE, PLLU_BASE);
    isEnabled  = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE, Base);
    isBypassed = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS, Base);

    if (isEnabled && !isBypassed )
    {
#if NVBL_USE_PLL_LOCK_BITS
        // Make sure lock enable is set.
        Misc = NV_CAR_REGR(CLK_RST_PA_BASE, PLLU_MISC);
        Misc = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLU_MISC, PLLU_LOCK_ENABLE, ENABLE, Misc);
        NV_CAR_REGW(CLK_RST_PA_BASE, PLLU_MISC, Misc);
#endif
        return;
    }

    if (s_FpgaEmulation)
    {
        Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS, ENABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE, DISABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_REF_DIS, REF_ENABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_LOCK, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_OVERRIDE, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_ICUSB, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_HSIC, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_USB, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_VCO_FREQ, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVN, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVM, DEFAULT);
        Misc = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, DEFAULT)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LFCON, DEFAULT);
    }
    else
    {
        // DIVP/DIVM/DIVN values taken from arclk_rst.h table
        switch (OscFreq)
        {
            case NvBootClocksOscFreq_13:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000D0D;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_LOCK, 0x0)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_OVERRIDE, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_ICUSB, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_HSIC, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_USB, DEFAULT)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_VCO_FREQ, 0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVN, 0x3C0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVM, 0xD);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 0xC)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LFCON, 0x1);
                break;

            case NvBootClocksOscFreq_38_4:
                // Fall through -- 38.4 MHz is identical to 19.2 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 2 for 38.4 MHz.

            case NvBootClocksOscFreq_19_2:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001313;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_LOCK, 0x0)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_OVERRIDE, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_ICUSB, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_HSIC, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_USB, DEFAULT)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_VCO_FREQ, 0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVN, 0xC8)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVM, 0x4);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 0x3)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LFCON, 0x0);
                break;

            case NvBootClocksOscFreq_48:
                // Fall through -- 48 MHz is identical to 12 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 4 for 48 MHz.

            case NvBootClocksOscFreq_12:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000C0C;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_LOCK, 0x0)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_OVERRIDE, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_ICUSB, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_HSIC, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_USB, DEFAULT)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_VCO_FREQ, 0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVN, 0x3C0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVM, 0xC);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 0xC)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LFCON, 0x1);
                break;

            case NvBootClocksOscFreq_26:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001A1A;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_LOCK, 0x0)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_OVERRIDE, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_ICUSB, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_HSIC, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_USB, DEFAULT)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_VCO_FREQ, 0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVN, 0x3C0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVM, 0x1A);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 0xC)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LFCON, 0x1);
                break;

            case NvBootClocksOscFreq_16_8:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001111;
                #else
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_LOCK, 0x0)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_OVERRIDE, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_ICUSB, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_HSIC, DEFAULT)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_CLKENABLE_USB, DEFAULT)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_VCO_FREQ, 0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVN, 0x190)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_DIVM, 0x7);
                #endif
                Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, 0x5)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LFCON, 0x0);
                break;

            default:
                Misc = 0; /* Suppress warning */
                NV_ASSERT(!"Bad frequency");
        }
    }

    NV_CAR_REGW(CLK_RST_PA_BASE, PLLU_MISC, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLU_BASE, Base);

    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_ENABLE, ENABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLU_BASE, Base);

#if !NVBL_PLL_BYPASS
    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLU_BASE, PLLU_BYPASS, DISABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLU_BASE, Base);

#if NVBL_USE_PLL_LOCK_BITS
    Misc = NV_CAR_REGR(CLK_RST_PA_BASE, PLLU_MISC);
    Misc = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLU_MISC, PLLU_LOCK_ENABLE, ENABLE, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLU_MISC, Misc);
#endif
#endif
}

//----------------------------------------------------------------------------------------------
static void InitPllX(NvBootClocksOscFreq OscFreq)
{
    NvU32               Base;       // PLL Base
    NvU32               Misc;       // PLL Misc
    NvU32               Divm;       // Reference
    NvU32               Divn;       // Multiplier
    NvU32               Divp;       // Divider == Divp ^ 2

    // Is PLL-X already running?
    Base = NV_CAR_REGR(CLK_RST_PA_BASE, PLLX_BASE);
    Base = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, Base);
    if (Base == CLK_RST_CONTROLLER_PLLX_BASE_0_PLLX_ENABLE_ENABLE)
    {
        return;
    }

    Misc = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_MISC, PLLX_CPCON, 1);

    if (s_FpgaEmulation)
    {
        Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, ENABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, DISABLE)
             | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_REF_DIS, REF_ENABLE)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_LOCK, 0x0)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP, 0x0)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, 0x258)
             | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM, 0xD);
    }
    else
    {
        Divm = 1;
        Divp = 0;
        Divn = NvBlAvpQueryBootCpuFrequency();

        // Above 600 MHz, set DCCON in the PLLX_MISC register.
        if (Divn > 600)
        {
            Misc |= NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_MISC, PLLX_DCCON, 1);
        }

        // Operating below the 50% point of the divider's range?
        if (Divn <= (NV_DRF_VAL(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, ~0)/2))
        {
            // Yes, double the post divider and the feedback divider.
            Divp = 1;
            Divn <<= Divp;
        }
        // Operating above the range of the feedback divider?
        else if (Divn > NV_DRF_VAL(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, ~0))
        {
            // Yes, double the input divider and halve the feedback divider.
            Divn >>= 1;
            Divm = 2;
        }

        // Program PLL-X.
        switch (OscFreq)
        {
            case NvBootClocksOscFreq_13:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000D0D;
                #else
                Divm = (Divm == 1) ? 13 : (13 / Divm);
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM, Divm);
                #endif
                break;

            case NvBootClocksOscFreq_38_4:
                // Fall through -- 38.4 MHz is identical to 19.2 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 2 for 38.4 MHz.

            case NvBootClocksOscFreq_19_2:
                // NOTE: With a 19.2 or 38.4 MHz oscillator, the PLL will run
                //       1.05% faster than the target frequency because we cheat
                //       on the math to make this simple.
                #if NVBL_PLL_BYPASS
                    Base = 0x80001313;
                #else
                Divm = (Divm == 1) ? 19 : (19 / Divm);
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM, Divm);
                #endif
                break;

            case NvBootClocksOscFreq_48:
                // Fall through -- 48 MHz is identical to 12 MHz except
                // that the PLL reference divider has been previously set
                // to divide by 4 for 48 MHz.

            case NvBootClocksOscFreq_12:
                #if NVBL_PLL_BYPASS
                    Base = 0x80000C0C;
                #else
                Divm = (Divm == 1) ? 12 : (12 / Divm);
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM, Divm);
                #endif
                break;

            case NvBootClocksOscFreq_26:
                #if NVBL_PLL_BYPASS
                    Base = 0x80001A1A;
                #else
                Divm = (Divm == 1) ? 26 : (26 / Divm);
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM, Divm);
                #endif
                break;

            case NvBootClocksOscFreq_16_8:
                // NOTE: With a 16.8 MHz oscillator, the PLL will run 1.18%
                //       slower than the target frequency because we cheat
                //       on the math to make this simple.
                #if NVBL_PLL_BYPASS
                    Base = 0x80001111;
                #else
                Divm = (Divm == 1) ? 17 : (17 / Divm);
                Base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, ENABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, DISABLE)
                     | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_REF_DIS, REF_ENABLE)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_LOCK, 0x0)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP, Divp)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN, Divn)
                     | NV_DRF_NUM(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM, Divm);
                #endif
                break;

            default:
                NV_ASSERT(!"Bad frequency");
        }
    }

    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_MISC, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_BASE, Base);

    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_ENABLE, ENABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_BASE, Base);

#if !NVBL_PLL_BYPASS
    Base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLX_BASE, PLLX_BYPASS, DISABLE, Base);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_BASE, Base);

#if NVBL_USE_PLL_LOCK_BITS
    Misc = NV_CAR_REGR(CLK_RST_PA_BASE, PLLX_MISC);
    Misc = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLX_MISC, PLLX_LOCK_ENABLE, ENABLE, Misc);
    NV_CAR_REGW(CLK_RST_PA_BASE, PLLX_MISC, Misc);
#endif
#endif
}

void NvBlInitPllX_t30(void)
{
    const NvBootInfoTable*  pBootInfo;      // Boot Information Table pointer
    NvBootClocksOscFreq     OscFreq;        // Oscillator frequency

    // Get a pointer to the Boot Information Table.
    pBootInfo = (const NvBootInfoTable*)(T30_BASE_PA_BOOT_INFO);

    // Get the oscillator frequency.
    OscFreq = pBootInfo->OscFrequency;

    InitPllX(OscFreq);
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
    pBootInfo = (const NvBootInfoTable*)(T30_BASE_PA_BOOT_INFO);

    // Get the oscillator frequency.
    OscFreq = pBootInfo->OscFrequency;

    // For most oscillator frequencies, the PLL reference divider is 1.
    // Frequencies that require a different reference divider will set
    // it below.
    PllRefDiv = CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV1;

    // Get Oscillator Drive Strength Setting
    OscStrength = NvBlGetOscillatorDriveStrength();

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
#ifndef NV_EMBEDDED_BUILD
            OscStrength = 0x8;
#else
            OscStrength = 0x7;
#endif
            break;

        case NvBootClocksOscFreq_26:
            UsecCfg = NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVIDEND, (1-1))
                    | NV_DRF_NUM(TIMERUS_USEC, CFG, USEC_DIVISOR, (26-1));
            OscStrength = 0xF;
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
    // Are we booting to LP CPU complex?
    if (CpuClusterId == NvBlCpuClusterId_LP)
    {
        // Set active CPU to LP.
        Reg = NV_FLD_SET_DRF_DEF(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, LP, Reg);
    }
    else
    {
        // Set active CPU to G.
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
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CPU_SOFTRST_CTRL,
                CPU_SOFTRST_WIDTH, DEFAULT_MASK, Reg);
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

    //-------------------------------------------------------------------------
    // If the boot ROM hasn't performed the PLLM initialization, we have to
    // do it here and always reconfigure PLLP.
    //-------------------------------------------------------------------------


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
        // Update same value in PMC_OSC_EDPD_OVER OXFS field for warmboot
        Reg = NV_PMC_REGR(PMC_PA_BASE, OSC_EDPD_OVER);
        Reg = NV_FLD_SET_DRF_NUM(APBDEV_PMC, OSC_EDPD_OVER, XOFS, OscStrength, Reg);
        NV_PMC_REGW(PMC_PA_BASE, OSC_EDPD_OVER, Reg);
    }

    // Initialize PLL-P.
    InitPllP(OscFreq);

    if (!IsChipInitialized)
    {
        // Initialize PLL-M.
        InitPllM(OscFreq);
    }

#if NVBL_USE_PLL_LOCK_BITS

    // Wait for PLL-P to lock.
    do
    {
        Reg = NV_CAR_REGR(CLK_RST_PA_BASE, PLLP_BASE);
    } while (!NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_LOCK, Reg));

    if (!IsChipInitialized)
    {
        // Wait for PLL-M to lock.
        do
        {
            Reg = NV_CAR_REGR(CLK_RST_PA_BASE, PLLM_BASE);
        } while (!NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_LOCK, Reg));
    }

#else

    // Give PLLs time to stabilize.
    NvBlAvpStallUs(NVBOOT_CLOCKS_PLL_STABILIZATION_DELAY);

#endif

    SetAvpClockToPllP();

    if (!IsChipInitialized)
    {
        Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, PLLM_OUT, PLLM_OUT1_RATIO, 0x2)
            | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_OUT, PLLM_OUT1_CLKEN, ENABLE)
            | NV_DRF_DEF(CLK_RST_CONTROLLER, PLLM_OUT, PLLM_OUT1_RSTN, RESET_DISABLE);
        NV_CAR_REGW(CLK_RST_PA_BASE, PLLM_OUT, Reg);
    }

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
    InitPllX(OscFreq);

#if (NVBL_PLLC_KHZ > 0)
    // Initialize PLL-C.
    InitPllC(OscFreq);
#endif

    // Initialize PLL-U.
    InitPllU(OscFreq);

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
 #if PMU_IGNORE_PWRREQ
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DVC_I2C, ENABLE, Reg);
 #else
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DVC_I2C, ENABLE, Reg);
 #endif
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC3, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_XIO, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_NOR, ENABLE, Reg);
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
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_M_DOUBLER_ENB, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, SYNC_CLK_DOUBLER_ENB, ENABLE, Reg);
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
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SPEEDO, ENABLE, Reg);
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

    // Switch MSELECT clock to PLLP
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_SOURCE_MSELECT);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_MSELECT,
            MSELECT_CLK_SRC, PLLP_OUT0, Reg);
    /* Clock divider request for 102MHz would setup MSELECT clock as 108MHz for PLLP base 216MHz
     * and 102MHz for PLLP base 408MHz.
     */
    Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_MSELECT,
            MSELECT_CLK_DIVISOR, CLK_DIVIDER(NVBL_PLLP_KHZ, 102000), Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_SOURCE_MSELECT, Reg);

#if PMU_IGNORE_PWRREQ
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_SOURCE_DVC_I2C);
    Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_DVC_I2C,
            DVC_I2C_CLK_DIVISOR, 0x10, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_SOURCE_DVC_I2C, Reg);
#endif

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
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_TRIG_SYS_RST, DISABLE, Reg);
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
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_TVDAC_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_CSI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_HDMI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_MIPI_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_TVO_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_DSI_RST, DISABLE, Reg);
#if PMU_IGNORE_PWRREQ
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_DVC_I2C_RST, DISABLE, Reg);
#else
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_DVC_I2C_RST, DISABLE, Reg);
#endif
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC3_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_XIO_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC2_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_NOR_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_SBC1_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_FUSE_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_STAT_MON_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_APBDMA_RST, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_H, SWR_MEM_RST, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_H, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_U);
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
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_U, SWR_SPEEDO_RST, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_U, Reg);

    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, RST_DEVICES_V);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SE_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_TZRAM_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_HDA_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SATA_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SATA_OOB_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SATA_FPCI_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_VDE_CBC_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM2_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM1_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_DAM0_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_APBIF_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_AUDIO_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SBC6_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_SBC5_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_I2S4_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_I2S3_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_TSENSOR_RST, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_MSELECT_RST, DISABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_3D2_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_CPULP_RST, ENABLE, Reg);
    // Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_V, SWR_CPUG_RST, ENABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, RST_DEVICES_V, Reg);
}

static void NvBlAvpClockDeInit(void)
{
    NvU32 Reg;
    // Disabling audio related clocks
    Reg = NV_CAR_REGR(CLK_RST_PA_BASE, CLK_OUT_ENB_V);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_SPDIF_DOUBLER, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S4_DOUBLER, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S3_DOUBLER, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S2_DOUBLER, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S1_DOUBLER, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_I2S0_DOUBLER, DISABLE, Reg);
    NV_CAR_REGW(CLK_RST_PA_BASE, CLK_OUT_ENB_V, Reg);

    NV_CAR_REGW(CLK_RST_PA_BASE, AUDIO_SYNC_CLK_I2S0, 0x10);
    NV_CAR_REGW(CLK_RST_PA_BASE, AUDIO_SYNC_CLK_I2S1, 0x10);
    NV_CAR_REGW(CLK_RST_PA_BASE, AUDIO_SYNC_CLK_I2S2, 0x10);
    NV_CAR_REGW(CLK_RST_PA_BASE, AUDIO_SYNC_CLK_I2S3, 0x10);
    NV_CAR_REGW(CLK_RST_PA_BASE, AUDIO_SYNC_CLK_I2S4, 0x10);
    NV_CAR_REGW(CLK_RST_PA_BASE, AUDIO_SYNC_CLK_SPDIF, 0x10);

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
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T30_BASE_PA_BOOT_INFO);

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
    // Memory Controller Tuning
    //-------------------------------------------------------------------------

    // Is this an FPGA?
    if (s_FpgaEmulation)
    {
        // Do nothing for now.
    }
    else
    {
        // USE DEFAULTS FOR NOW NV_ASSERT(0);   // NOT SUPPORTED
    }

    // Set up the AHB Mem Gizmo
    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, AHB_MEM);
#ifndef DISABLE_AHB_SPLIT_TRANSACTIONS
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_MEM, ENABLE_SPLIT, 1, Reg);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_MEM, DONT_SPLIT_AHB_WR, 0, Reg);
#else
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_MEM, ENABLE_SPLIT, 0, Reg);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_MEM, DONT_SPLIT_AHB_WR, 0, Reg);

    // Disable Split Transactions on all AHB masters
    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, USB);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, USB, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, USB, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, AHB_XBAR_BRIDGE);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_XBAR_BRIDGE, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, AHB_XBAR_BRIDGE, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, SDMMC4);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, SDMMC4, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, SDMMC4, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, TZRAM);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, TZRAM, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, TZRAM, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, NOR);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, NOR, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, NOR, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, USB2);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, USB2, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, USB2, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, USB3);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, USB3, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, USB3, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, SDMMC1);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, SDMMC1, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, SDMMC1, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, SDMMC2);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, SDMMC2, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, SDMMC2, Reg);

    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, SDMMC3);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, SDMMC3, ENABLE_SPLIT, 0, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, SDMMC3, Reg);

#endif

    /// Added for USB Controller
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, AHB_MEM, ENB_FAST_REARBITRATE, 1, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, AHB_MEM, Reg);

    // Enable GIZMO settings for USB controller.
    Reg = NV_AHB_GIZMO_REGR(AHB_PA_BASE, USB);
    Reg = NV_FLD_SET_DRF_NUM(AHB_GIZMO, USB, IMMEDIATE, 1, Reg);
    NV_AHB_GIZMO_REGW(AHB_PA_BASE, USB, Reg);

    // Make sure the debug registers are clear.
    Reg = NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_IDDQ_EN, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_PULLUP_EN, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_PULLDOWN_EN, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_DEBUG_ENABLE, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_PM_ENABLE, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_CLKBYP_FUNC, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_SW_BP_T1CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_SW_BP_T2CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_SW_BP_T3CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_SW_BP_T4CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_SW_BP_T5CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_SW_BP_T6CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_SW_BP_T7CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_SW_BP_CLK_DIV, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_EMAA, DISABLE)
        | NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_DP, 0)
        | NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_PDP, 0)
        | NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_REG, 0)
        | NV_DRF_NUM(APB_MISC, GP_ASDBGREG, CFG2TMC_RAM_SVOP_SP, 0);
    NV_MISC_REGW(MISC_PA_BASE, GP_ASDBGREG, Reg);

    Reg = NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T8CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T9CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T10CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T11CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T12CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T13CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T14CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T15CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T16CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T17CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_T18CLK, DISABLE)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG2, CFG2TMC_SW_BP_SYNC, DISABLE);
    NV_MISC_REGW(MISC_PA_BASE, GP_ASDBGREG2, Reg);

    Reg = NV_DRF_DEF(APB_MISC, GP_ASDBGREG3, CFG2TMC_RAM_SVOP_L1DATA, DEFAULT)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG3, CFG2TMC_RAM_SVOP_L1TAG, DEFAULT)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG3, CFG2TMC_RAM_SVOP_L2DATA, DEFAULT)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG3, CFG2TMC_RAM_SVOP_L2TAG, DEFAULT)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG3, CFG2TMC_RAM_SVOP_IRAM, DEFAULT)
        | NV_DRF_DEF(APB_MISC, GP_ASDBGREG3, CFG2TMC_RAM_SVOP_IRAM, DEFAULT);
    NV_MISC_REGW(MISC_PA_BASE, GP_ASDBGREG3, Reg);
}

NvBool NvBlAvpInit_T30(NvBool IsRunningFromSdram)
{
    NvBool              IsChipInitialized;
    NvBool              IsLoadedByScript = NV_FALSE;
    NvBootInfoTable*    pBootInfo = (NvBootInfoTable*)(T30_BASE_PA_BOOT_INFO);

    // Upon entry, IsRunningFromSdram reflects whether or not we're running out
    // of NOR (== NV_FALSE) or SDRAM (== NV_TRUE).
    //
    // (1)  If running out of SDRAM, whoever put us there may or may not have
    //      initialized everying. It is possible that this code was put here by a
    //      direct download over JTAG into SDRAM.
    // (2)  If running out of NOR, the chip may or may not have been initialized.
    //      In the case of secondary boot from NOR, the boot ROM will have already
    //      set up everything properly but we'll be executing from NOR. In that situation,
    //      the bootloader must not attempt to reinitialize the modules the boot ROM
    //      has already initialized.
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
        if ((pBootInfo->BootRomVersion == ~0)
        &&  (pBootInfo->DataVersion    == ~0)
        &&  (pBootInfo->RcmVersion     == ~0)
        &&  (pBootInfo->BootType       == ~0)
        &&  (pBootInfo->PrimaryDevice  == ~0))
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
            NvOsMemset(pBootInfo, 0, sizeof(NvBootInfoTable));

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

static NvBlCpuClusterId NvBlAvpQueryFlowControllerClusterId(void)
{
    NvBlCpuClusterId    ClusterId = NvBlCpuClusterId_G; // Active CPU id
    NvU32               Reg;                            // Scratch register

    Reg = NV_FLOW_REGR(FLOW_PA_BASE, CLUSTER_CONTROL);
    Reg = NV_DRF_VAL(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, Reg);

    if (Reg == NV_DRF_DEF(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, LP))
    {
        ClusterId = NvBlCpuClusterId_LP;
    }

    return ClusterId;
}

static NvBool NvBlAvpIsCpuPowered(NvBlCpuClusterId ClusterId)
{
    NvU32   Reg;        // Scratch reg
    NvU32   Mask;       // Mask

    // Get power gate status.
    Reg = NV_PMC_REGR(PMC_PA_BASE, PWRGATE_STATUS);

    // WARNING: Do not change the way this is coded (BUG 626323)
    //          Otherwise, the compiler can generate invalid ARMv4i instructions
    Mask = (ClusterId == NvBlCpuClusterId_LP) ?
            NV_DRF_DEF(APBDEV_PMC, PWRGATE_STATUS, A9LP, ON) :
            NV_DRF_DEF(APBDEV_PMC, PWRGATE_STATUS, CPU, ON);

    if (Reg & Mask)
    {
        return NV_TRUE;
    }

    return NV_FALSE;
}

static void NvBlAvpRemoveCpuIoClamps(void)
{
    NvU32               Reg;        // Scratch reg

    // Always remove the clamps on the LP CPU I/O signals.
    Reg = NV_DRF_DEF(APBDEV_PMC, REMOVE_CLAMPING_CMD, A9LP, ENABLE);
    NV_PMC_REGW(PMC_PA_BASE, REMOVE_CLAMPING_CMD, Reg);

    // If booting the G CPU ...
    if (NvBlAvpQueryFlowControllerClusterId() == NvBlCpuClusterId_G)
    {
        // ... remove the clamps on the G CPU0 I/O signals as well.
        Reg = NV_DRF_DEF(APBDEV_PMC, REMOVE_CLAMPING_CMD, CPU, ENABLE);
        NV_PMC_REGW(PMC_PA_BASE, REMOVE_CLAMPING_CMD, Reg);
    }

    // Give I/O signals time to stabilize.
    NvBlAvpStallUs(20);  // !!!FIXME!!! THIS TIME HAS NOT BEEN CHARACTERIZED
}

static void  NvBlAvpPowerUpCpu(void)
{
    NvU32   Reg;        // Scratch reg

    // Always power up the LP CPU
    if (!NvBlAvpIsCpuPowered(NvBlCpuClusterId_LP))
    {
        // Toggle the LP CPU power state (OFF -> ON).
        Reg = NV_DRF_DEF(APBDEV_PMC, PWRGATE_TOGGLE, PARTID, A9LP)
            | NV_DRF_DEF(APBDEV_PMC, PWRGATE_TOGGLE, START, ENABLE);
        NV_PMC_REGW(PMC_PA_BASE, PWRGATE_TOGGLE, Reg);

        // Wait for the power to come up.
        while (!NvBlAvpIsCpuPowered(NvBlCpuClusterId_LP))
        {
            // Do nothing
        }
    }

    // Are we booting to the G CPU?
    if (NvBlAvpQueryFlowControllerClusterId() == NvBlCpuClusterId_G)
    {
        if (!NvBlAvpIsCpuPowered(NvBlCpuClusterId_G))
        {
            // 0x26E8F0 == 25*102000000/1000 -- 25ms, APB clock 102MHz
            NV_PMC_REGW(PMC_PA_BASE, CPUPWRGOOD_TIMER, 0x26E8F0);

            // Toggle the G CPU0 power state (OFF -> ON).
            Reg = NV_DRF_DEF(APBDEV_PMC, PWRGATE_TOGGLE, PARTID, CP)
                | NV_DRF_DEF(APBDEV_PMC, PWRGATE_TOGGLE, START, ENABLE);
            NV_PMC_REGW(PMC_PA_BASE, PWRGATE_TOGGLE, Reg);
        }

        // Wait for the power to come up.
        while (!NvBlAvpIsCpuPowered(NvBlCpuClusterId_G))
        {
            // Do nothing
        }
    }

    // Remove the I/O clamps from CPU power partition.
    NvBlAvpRemoveCpuIoClamps();
}

static void NvBlAvpEnableCpuPowerRail(void)
{
    NvU32   Reg;        // Scratch reg
    NvU32 BoardId;
    NvU32 BoardFab;
    NvU32 val = 0;
    NvU32 retry = PWRI2C_CLOCK_CYCLES;
    NvAvpGpioHandle i2cGpio;
    NvAvpGpioPinHandle i2cScl;
    NvAvpGpioPinHandle i2cSda;

    Reg = NV_PMC_REGR(PMC_PA_BASE, CNTRL);
    Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC, CNTRL, CPUPWRREQ_OE, ENABLE, Reg);
    NV_PMC_REGW(PMC_PA_BASE, CNTRL, Reg);

    NvAvpGpioOpen(&i2cGpio);

    NvAvpGpioAcquirePinHandle(&i2cGpio,'z' - 'a', 6, &i2cScl);
    NvAvpGpioAcquirePinHandle(&i2cGpio,'z' - 'a', 7, &i2cSda);

    NvAvpGpioConfigPin(&i2cGpio, &i2cScl, NvAvpGpioPinMode_Output);
    NvAvpGpioConfigPin(&i2cGpio, &i2cSda, NvAvpGpioPinMode_InputData);

    while (retry)
    {
        val = 0;
        NvAvpGpioWritePin(&i2cGpio, &i2cScl, &val);
        NvBlAvpStallUs(5);

        val = 1;
        NvAvpGpioWritePin(&i2cGpio, &i2cScl, &val);
        NvBlAvpStallUs(5);

        NvAvpGpioReadPin(&i2cGpio, &i2cSda, &val);
        if (val)
            break;
        retry = retry - 1;
    }

    NvAvpGpioReleasePinHandle(&i2cGpio, &i2cScl);
    NvAvpGpioReleasePinHandle(&i2cGpio, &i2cSda);

#ifdef NV_EMBEDDED_BUILD
    return;
#endif

    if (s_FpgaEmulation)
    {
        return;
    }

    /* Read the board Id */
    BoardId = g_BoardInfo.BoardId;
    BoardFab = g_BoardInfo.Fab;

#if __GNUC__
    NvOsAvpDebugPrintf("Board Id= 0x%04x\n", BoardId);
#endif

#if PMU_IGNORE_PWRREQ
    if (NvBlAvpQueryBootCpuClusterId() == NvBlCpuClusterId_Fast)
    {
#define CARDHU_PMU_I2C_SLAVE_ADDRESS    0x5A
        NvBlPmuWrite(CARDHU_PMU_I2C_SLAVE_ADDRESS, 0X28, 0x23);
        NvBlPmuWrite(CARDHU_PMU_I2C_SLAVE_ADDRESS, 0X27, 0x01);
#undef  CARDHU_PMU_I2C_SLAVE_ADDRESS
    }
#endif

    NvBlDeinitPmuSlaves();
    // 5ms delay is enough and can wait upto 25V cpu voltage to be enabled.
    NvBlAvpStallMs(5);
}

//------------------------------------------------------------------------------
void NvBlStartCpu_T30(NvU32 ResetVector)
{
    // Enable VDD_CPU
    s_enablePowerRail = *g_ptimerus;
    NvBlAvpEnableCpuPowerRail();
    e_enablePowerRail = *g_ptimerus;

    // Halt the CPU (it should not be running).
    NvBlAvpHaltCpu(NV_TRUE);

    // Hold the CPUs in reset.
    NvBlAvpResetCpu(NV_TRUE);

    // Disable the CPU clock.
    NvBlAvpEnableCpuClock(NV_FALSE);

    // Enable CoreSight.
    NvBlAvpClockEnableCorsight(NV_TRUE);

    // Set the entry point for CPU execution from reset, if it's a non-zero value.
    if (ResetVector)
    {
        NvBlAvpSetCpuResetVector(ResetVector);
    }

    // Enable the CPU clock.
    NvBlAvpEnableCpuClock(NV_TRUE);

    // Power up the CPU.
    NvBlAvpPowerUpCpu();

    // Take the CPU out of reset.
    NvBlAvpResetCpu(NV_FALSE);

    // Unhalt the CPU.
    NvBlAvpHaltCpu(NV_FALSE);
}

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
    {   // Is the address valid?
        if (pBootInfo->SafeStartAddr)
        {
            // Get the address where the signature is stored. (Safe start - 4)
            SafeStartAddress = (NvU32 *)(pBootInfo->SafeStartAddr - sizeof(NvU32));
            // compare signature...if this is coming from miniloader

            isNv3pSignatureSet = 0;
            if (*(SafeStartAddress) == Nv3pSignature)
            {
                //if yes then modify the safe start address with correct start address (Safe address -4)
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
    //NOT IMPLEMENTED
    return;
}

void NvBlConfigJtagAccess(void)
{
    NvU32 reg = 0;

    if (NV_FUSE_REGR(FUSE_PA_BASE, SECURITY_MODE))
    {
        // set debug capabilities
        reg  = AOS_REGR(SECURE_BOOT_PA_BASE + NVBL_SECURE_BOOT_PFCFG_OFFSET);
        reg &= NVBL_SECURE_BOOT_JTAG_CLEAR;
        reg |= NVBL_SECURE_BOOT_JTAG_VAL;
        AOS_REGW(SECURE_BOOT_PA_BASE + NVBL_SECURE_BOOT_PFCFG_OFFSET, reg);
    }
#ifndef BUILD_FOR_COSIM
    // enable JTAG
    reg = NV_DRF_DEF(APB_MISC, PP_CONFIG_CTL, TBE, ENABLE)
        | NV_DRF_DEF(APB_MISC, PP_CONFIG_CTL, JTAG, ENABLE);
    AOS_REGW(MISC_PA_BASE + APB_MISC_PP_CONFIG_CTL_0, reg);
#endif
    return;
}

#if !__GNUC__
__asm void NvBlStartUpAvp_T30( void )
{
    CODE32
    PRESERVE8

    IMPORT NvBlAvpInit_T30
    IMPORT NvBlStartCpu_T30
    IMPORT NvBlAvpHalt

    //;------------------------------------------------------------------
    //; Initialize the AVP, clocks, and memory controller.
    //;------------------------------------------------------------------

    //The SDRAM is guaranteed to be on at this point
    //in the nvml environment. Set r0 = 1.
    MOV     r0, #1
    BL      NvBlAvpInit_T30

    //;------------------------------------------------------------------
    //; Start the CPU.
    //;------------------------------------------------------------------

    LDR     r0, =ColdBoot               //; R0 = reset vector for CPU
    BL      NvBlStartCpu_T30

    //;------------------------------------------------------------------
    //; Transfer control to the AVP code.
    //;------------------------------------------------------------------

    BL      NvBlAvpHalt

    //;------------------------------------------------------------------
    //; Should never get here.
    //;------------------------------------------------------------------
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

    //;------------------------------------------------------------------
    //; Check current processor: CPU or AVP?
    //; If AVP, go to AVP boot code, else continue on.
    //;------------------------------------------------------------------

    LDR     r0, =PG_UP_PA_BASE
    LDRB    r2, [r0, #PG_UP_TAG_0]
    CMP     r2, #PG_UP_TAG_0_PID_CPU _AND_ 0xFF //; are we the CPU?
    LDR     sp, =NVAP_LIMIT_PA_IRAM_CPU_EARLY_BOOT_STACK
    // leave in some symbols for release debugging
    LDR     r3, =0xDEADBEEF
    STR     r3, [sp, #-4]!
    STR     r3, [sp, #-4]!
    BEQ     NV_AOS_ENTRY_POINT                   //; yep, we are the CPU

    //;==================================================================
    //; AVP Initialization follows this path
    //;==================================================================

    LDR     sp, =NVAP_LIMIT_PA_IRAM_AVP_EARLY_BOOT_STACK
    // leave in some symbols for release debugging
    LDR     r3, =0xDEADBEEF
    STR     r3, [sp, #-4]!
    STR     r3, [sp, #-4]!
    B       NvBlStartUpAvp_T30
}
#else
NV_NAKED void NvBlStartUpAvp_T30( void )
{
    //;------------------------------------------------------------------
    //; Initialize the AVP, clocks, and memory controller.
    //;------------------------------------------------------------------

    asm volatile(
    //The SDRAM is guaranteed to be on at this point
    //in the nvml environment. Set r0 = 1.
    "MOV     r0, #1                          \n"
    "BL      NvBlAvpInit_T30                 \n"

    //;------------------------------------------------------------------
    //; Start the CPU.
    //;------------------------------------------------------------------
    "LDR     r0, =ColdBoot                   \n"//; R0 = reset vector for CPU
    "BL      NvBlStartCpu_T30                \n"

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
    "BNE     is_T30_avp                                             \n"
    //; yep, we are the CPU
    "BL      ARM_ERRATA_743622                                      \n"
    "BL      ARM_ERRATA_751472                                      \n"
    "BXEQ     %4                                                    \n"
    //;==================================================================
    //; AVP Initialization follows this path
    //;==================================================================
"is_T30_avp:                                                        \n"
    "MOV     r2, %5                                                 \n"
    "MOV     r3, %6                                                 \n"
    "STR     r2, [r3]                                               \n"
    "MOV     sp, %7                                                 \n"
    // Leave in some symbols for release debugging
    "MOV     r3, %8                                                 \n"
    "STR     r3, [sp, #-4]!                                         \n"
    "STR     r3, [sp, #-4]!                                         \n"
    "B       NvBlStartUpAvp_T30                                     \n"
    :
    :"I"(PG_UP_PA_BASE),
     "I"(PG_UP_TAG_0),
     "r"(proc_tag),
     "r"(cpu_boot_stack),
     "r"(entry_point),
     "r"(avp_cache_settings),
     "r"(avp_cache_config),
     "r"(avp_boot_stack),
     "r"(deadbeef)
    : "r0", "r2", "r3", "cc", "lr"
    );
}
#endif

