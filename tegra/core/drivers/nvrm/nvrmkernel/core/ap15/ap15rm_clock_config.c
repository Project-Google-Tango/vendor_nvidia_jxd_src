/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvrm_clocks.h"
#include "nvrm_module.h"
#include "nvrm_drf.h"
#include "ap20/arclk_rst.h"
#include "ap15rm_clocks.h"
#include "ap15rm_private.h"
#include "nvrm_pmu_private.h"
#include "nvodm_query_discovery.h"

// Minimum PLLX VCO frequency for reliable operation of DCC circuit
#define NVRM_PLLX_DCC_VCO_MIN (600000)

// Default PLLC output frequency
#define NVRM_PLLC_DEFAULT_FREQ_KHZ (600000)

// TODO: CAR and EMC access macros for time critical access

static void
Ap15PllPConfigure(NvRmDeviceHandle hRmDevice);

static void
Ap15AudioSyncInit(NvRmDeviceHandle hRmDevice, NvRmFreqKHz AudioSyncKHz);


static void
OscPllRefFreqInit(
    NvRmDeviceHandle hRmDevice,
    NvRmFreqKHz* pOscKHz,
    NvRmFreqKHz* pPllRefKHz)
{
    NvRmPrivOscPllRefFreqInit(hRmDevice, pOscKHz, pPllRefKHz);
    return;
}

static NvError
OscDoublerConfigure(NvRmDeviceHandle hRmDevice, NvRmFreqKHz OscKHz)
{
    return NvRmPrivOscDoublerConfigure(hRmDevice, OscKHz);
}

static NvBool
IsPllcInitConfigure(NvRmDeviceHandle hRmDevice)
{
    switch (hRmDevice->ChipId.Id)
    {
        case 0x30:
            return NV_TRUE;
        default:
            return NV_FALSE;
    }
}

void
NvRmPrivClockSourceFreqInit(
    NvRmDeviceHandle hRmDevice,
    NvU32* pClockSourceFreq)
{
    NvU32 reg;
    const NvRmCoreClockInfo* pCore = NULL;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pClockSourceFreq);

    /*
     * Fixed clock sources: 32kHz, main oscillator and doubler
     * (OSC control should be already configured by the boot code)
     */
    pClockSourceFreq[NvRmClockSource_ClkS] = 32;

    OscPllRefFreqInit(hRmDevice, &pClockSourceFreq[NvRmClockSource_ClkM],
                      &pClockSourceFreq[NvRmClockSource_PllRef]);

    if (NvSuccess == OscDoublerConfigure(
        hRmDevice, pClockSourceFreq[NvRmClockSource_ClkM]))
    {
        pClockSourceFreq[NvRmClockSource_ClkD] =
            pClockSourceFreq[NvRmClockSource_ClkM] << 1;
    }
    else
        pClockSourceFreq[NvRmClockSource_ClkD] = 0;

    /*
     * PLLs and secondary PLL dividers
     */
    #define INIT_PLL_FREQ(PllId) \
    do\
    {\
        pClockSourceFreq[NvRmClockSource_##PllId] = NvRmPrivAp15PllFreqGet( \
         hRmDevice, NvRmPrivGetClockSourceHandle(NvRmClockSource_##PllId)->pInfo.pPll); \
    } while(0)

    // PLLX (check if present, keep boot settings
    // and just init frequency table)
    if (NvRmPrivGetClockSourceHandle(NvRmClockSource_PllX0))
    {
        INIT_PLL_FREQ(PllX0);
    }
    // PLLC with output divider (if enabled keep boot settings and just init
    // frequency table, if disbled or bypassed - configure)
    INIT_PLL_FREQ(PllC0);
    if ((pClockSourceFreq[NvRmClockSource_PllC0] <=
        pClockSourceFreq[NvRmClockSource_ClkM]) &&
        IsPllcInitConfigure(hRmDevice))
    {
        NvRmFreqKHz f = NVRM_PLLC_DEFAULT_FREQ_KHZ;
        NvRmPrivAp15PllConfigureSimple(hRmDevice, NvRmClockSource_PllC0, f, &f);
    }
    pClockSourceFreq[NvRmClockSource_PllC1] = NvRmPrivDividerFreqGet(hRmDevice,
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllC1)->pInfo.pDivider);

    // PLLM with output divider (keep boot settings
    // and just init frequency)
    INIT_PLL_FREQ(PllM0);
    pClockSourceFreq[NvRmClockSource_PllM1] = NvRmPrivDividerFreqGet(hRmDevice,
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllM1)->pInfo.pDivider);
    // PLLD and PLLU with no output dividers (keep boot settings
    // and just init frequency table)
    INIT_PLL_FREQ(PllD0);
    INIT_PLL_FREQ(PllU0);

    // PLLP and output dividers: set PLLP fixed frequency and enable dividers
    // with fixed settings in override mode, so they can be changed later, as
    // necessary. Switch system clock to oscillator during PLLP reconfiguration
    INIT_PLL_FREQ(PllP0);
    if (pClockSourceFreq[NvRmClockSource_PllP0] != NVRM_PLLP_FIXED_FREQ_KHZ)
    {
        pCore = NvRmPrivGetClockSourceHandle(
        NvRmClockSource_SystemBus)->pInfo.pCore;
        reg = NV_REGR(hRmDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
                pCore->SelectorOffset);
        NvRmPrivCoreClockSet(hRmDevice, pCore, NvRmClockSource_ClkM, 0, 0);
        Ap15PllPConfigure(hRmDevice);
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
                pCore->SelectorOffset, reg);
    }

    NV_ASSERT(pClockSourceFreq[NvRmClockSource_PllP0] ==
        NVRM_PLLP_FIXED_FREQ_KHZ);

    NvRmPrivDividerSet(
        hRmDevice,
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllP1)->pInfo.pDivider,
        NVRM_FIXED_PLLP1_SETTING);
    NvRmPrivDividerSet(
        hRmDevice,
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllP2)->pInfo.pDivider,
        NVRM_FIXED_PLLP2_SETTING);
    NvRmPrivDividerSet(
        hRmDevice,
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllP3)->pInfo.pDivider,
        NVRM_FIXED_PLLP3_SETTING);
    NvRmPrivDividerSet(
        hRmDevice,
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllP4)->pInfo.pDivider,
        NVRM_FIXED_PLLP4_SETTING);

    // PLLA and output divider must be init after PLLP1, used as a
    // reference (keep boot settings and just init frequency table)
    INIT_PLL_FREQ(PllA1);
    pClockSourceFreq[NvRmClockSource_PllA0] = NvRmPrivDividerFreqGet(hRmDevice,
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllA0)->pInfo.pDivider);

    #undef INIT_PLL_FREQ

    /*
     * Core and bus clock sources
     * - Leave CPU bus as set by boot-loader
     * - Leave System bus as set by boot-loader, make sure all bus dividers are 1:1
     */
    pCore = NvRmPrivGetClockSourceHandle(NvRmClockSource_CpuBus)->pInfo.pCore;
    pClockSourceFreq[NvRmClockSource_CpuBus] =
        NvRmPrivCoreClockFreqGet(hRmDevice, pCore);
    if (NvRmPrivGetClockSourceHandle(NvRmClockSource_CpuBridge))
    {
        pClockSourceFreq[NvRmClockSource_CpuBridge] = NvRmPrivDividerFreqGet(hRmDevice,
            NvRmPrivGetClockSourceHandle(NvRmClockSource_CpuBridge)->pInfo.pDivider);
    }
    pCore = NvRmPrivGetClockSourceHandle(NvRmClockSource_SystemBus)->pInfo.pCore;
    pClockSourceFreq[NvRmClockSource_SystemBus] =
        NvRmPrivCoreClockFreqGet(hRmDevice, pCore);
    NvRmPrivBusClockInit(
        hRmDevice, pClockSourceFreq[NvRmClockSource_SystemBus]);

    /*
     * Initialize AudioSync clocks (PLLA will be re-configured if necessary)
     */
    Ap15AudioSyncInit(hRmDevice, NVRM_AUDIO_SYNC_KHZ);
}

void
NvRmPrivBusClockInit(NvRmDeviceHandle hRmDevice, NvRmFreqKHz SystemFreq)
{
    /*
     * Set all bus clock frequencies equal to the system clock frequency,
     * and clear AVP clock skipper i.e., set all bus clock dividers 1:1.
     * If APB clock is limited below system clock for a particular SoC,
     * set the APB divider to satisfy this limitation.
     */
    NvRmFreqKHz AhbFreq, ApbFreq;
    NvRmFreqKHz ApbMaxFreq = SystemFreq;
    if (hRmDevice->ChipId.Id != 0x30)
    {
        ApbMaxFreq = SystemFreq/2; // Ahb:Apb = 2:1 starting from T35
    }
    AhbFreq = SystemFreq;
    ApbFreq = NV_MIN(SystemFreq, ApbMaxFreq);

    NvRmPrivBusClockFreqSet(
        hRmDevice, SystemFreq, &SystemFreq, &AhbFreq, &ApbFreq, ApbMaxFreq);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        CLK_RST_CONTROLLER_COP_CLK_SKIP_POLICY_0, 0x0);
}
static const NvRmFreqKHz s_PllLpCpconSelectionTable[] =
{
    NVRM_PLL_LP_CPCON_SELECT_STEPS_KHZ
};

static const NvU32 s_PllLpCpconSelectionTableSize =
NV_ARRAY_SIZE(s_PllLpCpconSelectionTable);

static const NvU32 s_PllMipiCpconSelectionTable[] =
{
    NVRM_PLL_MIPI_CPCON_SELECT_STEPS_N_DIVIDER
};
static const NvU32 s_PllMipiCpconSelectionTableSize =
NV_ARRAY_SIZE(s_PllMipiCpconSelectionTable);

static void
PllLpGetTypicalControls(
    NvRmFreqKHz InputKHz,
    NvU32 M,
    NvU32 N,
    NvU32* pCpcon)
{
    NvU32 i;
    if (N >= NVRM_PLL_LP_MIN_N_FOR_CPCON_SELECTION)
    {
        // CPCON depends on comparison frequency
        for (i = 0; i < s_PllLpCpconSelectionTableSize; i++)
        {
            if (InputKHz >= s_PllLpCpconSelectionTable[i] * M)
                break;
        }
        *pCpcon = i + 1;
    }
    else    // CPCON is 1, regardless of frequency
    {
        *pCpcon = 1;
    }
}

static void
PllMipiGetTypicalControls(
    NvU32 N,
    NvU32* pCpcon,
    NvU32* pLfCon)
{
    NvU32 i;

    // CPCON depends on feedback divider
    for (i = 0; i < s_PllMipiCpconSelectionTableSize; i++)
    {
        if (N <= s_PllMipiCpconSelectionTable[i])
            break;
    }
    *pCpcon = i + 1;
    *pLfCon = (N >= NVRM_PLL_MIPI_LFCON_SELECT_N_DIVIDER) ? 1 : 0;
}

void
NvRmPrivAp15PllSet(
    NvRmDeviceHandle hRmDevice,
    const NvRmPllClockInfo* pCinfo,
    NvU32 M,
    NvU32 N,
    NvU32 P,
    NvU32 StableDelayUs,
    NvU32 cpcon,
    NvU32 lfcon,
    NvBool TypicalControls,
    NvU32 flags)
{
    NvU32 base, misc;
    NvU32 old_base, old_misc;
    NvU32 delay = 0;
    NvU32 override = 0;
    NvBool diff_clock = NV_FALSE;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pCinfo);
    NV_ASSERT(pCinfo->PllBaseOffset);
    NV_ASSERT(pCinfo->PllMiscOffset);

    /*
     * PLL control fields used below have the same layout for all PLLs with
     * the following exceptions:
     *
     * a) PLLP base register OVERRIDE field has to be set in order to enable
     *  PLLP re-configuration in diagnostic mode. For other PLLs this field is
     *  "Don't care".
     * b) PLLU HS P divider field is one bit, inverse logic field. Other control
     *  bits, that are mapped to P divider in common layout should be set to 0.
     *
     * PLLP h/w field definitions will be used in DRF macros to construct base
     * values for all PLLs, with special care of a) and b). All base fields not
     * explicitly used below are set to 0 for all PLLs.
     *
     * c) PLLD/PLLU miscellaneous register has a unique fields determined based
     *  on the input flags. For other PLLs these fields have different meaning,
     *  and will be preserved.
     *
     *  PLLP h/w field definitions will be used in DRF macros to construct
     *  miscellaneous values with common layout. For unique fields PLLD h/w
     *  definitions will be used. All miscellaneous fields not explicitly used
     *  below are preserved for all PLLs.
     */
    base = NV_REGR(hRmDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        pCinfo->PllBaseOffset);
    old_base = base;

    // Disable PLL if either input or feedback divider setting is zero
    if ((M == 0) || (N == 0))
    {
        base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, DISABLE, base);
        base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE, base);
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            pCinfo->PllBaseOffset, base);
        NvRmPrivPllFreqUpdate(hRmDevice, pCinfo);
        return;
    }

    // Determine type-specific controls, construct new misc settings
    misc = NV_REGR(hRmDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        pCinfo->PllMiscOffset);
    old_misc = misc;
    if (pCinfo->PllType == NvRmPllType_MIPI)
    {
        if (flags & NvRmPllConfigFlags_SlowMode)
        {
            misc = NV_FLD_SET_DRF_NUM(  // "1" = slow (/8) MIPI clock output
                    CLK_RST_CONTROLLER, PLLD_MISC, PLLD_FO_MODE, 1, misc);
        }
        else if (flags & NvRmPllConfigFlags_FastMode)
        {
            misc = NV_FLD_SET_DRF_NUM(  // "0" = fast MIPI clock output
                    CLK_RST_CONTROLLER, PLLD_MISC, PLLD_FO_MODE, 0, misc);
        }
        if (flags & NvRmPllConfigFlags_DiffClkEnable)
        {
            misc = NV_FLD_SET_DRF_NUM(  // Enable differential clocks
                    CLK_RST_CONTROLLER, PLLD_MISC, PLLD_CLKENABLE, 1, misc);
        }
        else if (flags & NvRmPllConfigFlags_DiffClkDisable)
        {
            misc = NV_FLD_SET_DRF_NUM(  // Disable differential clocks
                    CLK_RST_CONTROLLER, PLLD_MISC, PLLD_CLKENABLE, 0, misc);
        }
        if (TypicalControls)
        {
            PllMipiGetTypicalControls(N, &cpcon, &lfcon);
        }
        delay = NVRM_PLL_MIPI_STABLE_DELAY_US;
    }
    else if (pCinfo->PllType == NvRmPllType_LP)
    {
        if (flags & NvRmPllConfigFlags_DccEnable)
        {
            misc = NV_FLD_SET_DRF_NUM(  // "1" = enable DCC
                    CLK_RST_CONTROLLER, PLLP_MISC, PLLP_DCCON, 1, misc);
        }
        else if (flags & NvRmPllConfigFlags_DccDisable)
        {
            misc = NV_FLD_SET_DRF_NUM(  // "0" = disable DCC
                    CLK_RST_CONTROLLER, PLLP_MISC, PLLP_DCCON, 0, misc);
        }
        if (TypicalControls)
        {
            NvRmFreqKHz InputKHz = NvRmPrivGetClockSourceFreq(pCinfo->InputId);
            PllLpGetTypicalControls(InputKHz, M, N, &cpcon);
        }
        lfcon = 0; // always for LP PLL
        delay = NVRM_PLL_LP_STABLE_DELAY_US;
    }
    else if (pCinfo->PllType == NvRmPllType_UHS)
    {
        if (TypicalControls)    // Same as MIPI typical controls
        {
            PllMipiGetTypicalControls(N, &cpcon, &lfcon);
        }
        delay = NVRM_PLL_MIPI_STABLE_DELAY_US;
        P = (P == 0) ? 1 : 0;   // P-divider is 1 bit, inverse logic
    }
    else
    {
        NV_ASSERT(!"Invalid PLL type");
    }
    misc = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_CPCON, cpcon, misc);
    misc = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLP_MISC, PLLP_LFCON, lfcon, misc);

    // Construct new base setting
    // Override is PLLP specific, and it is just ignored by other PLLs;
    override = ((flags & NvRmPllConfigFlags_Override) != 0) ?
                CLK_RST_CONTROLLER_PLLP_BASE_0_PLLP_BASE_OVRRIDE_ENABLE :
                CLK_RST_CONTROLLER_PLLP_BASE_0_PLLP_BASE_OVRRIDE_DISABLE;
    {   // Compiler failed to generate correct code for the base fields
        // concatenation without the split below
        volatile NvU32 prebase =
            NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, DISABLE) |
            NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, ENABLE) |
            NV_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, REF_ENABLE);
        base = prebase |
            NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BASE_OVRRIDE, override) |
            NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, P) |
            NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN, N) |
            NV_DRF_NUM(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, M);
    }

    // If PLL is not bypassed, and new configurations is the same as the old
    // one - exit without overwriting h/w. Otherwise, bypass and disable PLL
    // outputs before changing configuration.
    if ((base == old_base) && (misc == old_misc))
    {
        NvRmPrivPllFreqUpdate(hRmDevice, pCinfo);
        return;
    }
    if (pCinfo->SourceId == NvRmClockSource_PllD0)
    {
        old_misc = NV_FLD_SET_DRF_NUM(
            CLK_RST_CONTROLLER, PLLD_MISC, PLLD_CLKENABLE, 0, old_misc);
        NV_REGW(hRmDevice,  NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            pCinfo->PllMiscOffset, old_misc);
        if (NV_DRF_VAL(CLK_RST_CONTROLLER, PLLD_MISC, PLLD_CLKENABLE, misc))
        {
            diff_clock = NV_TRUE;
            misc = NV_FLD_SET_DRF_NUM(
                CLK_RST_CONTROLLER, PLLD_MISC, PLLD_CLKENABLE, 0, misc);
        }
        misc = NV_FLD_SET_DRF_NUM(
            CLK_RST_CONTROLLER, PLLD_MISC, PLLD_LOCK_ENABLE, 0, misc);
    }
    old_base = NV_FLD_SET_DRF_DEF(
        CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE, old_base);
    NV_REGW(hRmDevice, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            pCinfo->PllBaseOffset, old_base);
    old_base = NV_FLD_SET_DRF_DEF(
        CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE, old_base);
    NV_REGW(hRmDevice, NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            pCinfo->PllBaseOffset, old_base);

    // Configure and enable PLL, keep it bypassed
    base = NV_FLD_SET_DRF_DEF(
        CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, ENABLE, base);
    base = NV_FLD_SET_DRF_DEF(
        CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, DISABLE, base);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        pCinfo->PllBaseOffset, base);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        pCinfo->PllMiscOffset, misc);
    base = NV_FLD_SET_DRF_DEF(
        CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, ENABLE, base);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
        pCinfo->PllBaseOffset, base);

    if(hRmDevice->ChipId.Id != 0x20 && pCinfo->SourceId == NvRmClockSource_PllD0)
    {
        misc = NV_FLD_SET_DRF_NUM(
            CLK_RST_CONTROLLER, PLLD_MISC, PLLD_LOCK_ENABLE, 1, misc);
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            pCinfo->PllMiscOffset, misc);
        // For PLLD, wait until locking
        do
        {
            base = NV_REGR(hRmDevice,
                    NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
                    pCinfo->PllBaseOffset);
        }
        while(!(NV_DRF_NUM(CLK_RST_CONTROLLER,PLLD_BASE,PLLD_LOCK,1)& base));
        goto end;
    }

    // Wait for PLL to stabilize and switch to PLL output

    NV_ASSERT(StableDelayUs);
    if (StableDelayUs > delay)
        StableDelayUs = delay;
    NvOsWaitUS(StableDelayUs);

end:
    if (diff_clock)
    {
        misc = NV_FLD_SET_DRF_NUM(
            CLK_RST_CONTROLLER, PLLD_MISC, PLLD_CLKENABLE, 1, misc);
        NV_REGW(hRmDevice,  NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            pCinfo->PllMiscOffset, misc);
    }
    base = NV_FLD_SET_DRF_DEF(
        CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, DISABLE, base);
    NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        pCinfo->PllBaseOffset, base);
    NvRmPrivPllFreqUpdate(hRmDevice, pCinfo);
}

NvRmFreqKHz
NvRmPrivAp15PllFreqGet(
    NvRmDeviceHandle hRmDevice,
    const NvRmPllClockInfo* pCinfo)
{
    NvU32 M, N, P;
    NvU32 base, misc;
    NvRmFreqKHz PllKHz;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pCinfo);
    NV_ASSERT(pCinfo->PllBaseOffset);
    NV_ASSERT(pCinfo->PllMiscOffset);

    /*
     * PLL control fields used below have the same layout for all PLLs with
     * the following exceptions:
     *
     * a) PLLP base register OVERRIDE field ("Don't care" for other PLLs).
     *  Respectively, PLLP h/w field definitions will be used in DRF macros
     *  to construct base values for all PLLs.
     *
     * b) PLLD/PLLU miscellaneous register fast/slow mode control (does not
     *  affect output frequency for other PLLs). Respectively, PLLD h/w field
     *  definitions will be used in DRF macros to construct miscellaneous values.
     */
    base = NV_REGR( hRmDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
        pCinfo->PllBaseOffset);
    PllKHz = NvRmPrivGetClockSourceFreq(pCinfo->InputId);
    NV_ASSERT(PllKHz);
    NV_ASSERT(NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_REF_DIS, base) ==
              CLK_RST_CONTROLLER_PLLP_BASE_0_PLLP_REF_DIS_REF_ENABLE);

    if (NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS, base) ==
        CLK_RST_CONTROLLER_PLLP_BASE_0_PLLP_BYPASS_DISABLE)
    {
        // Special cases: PLL is disabled, or in fixed mode (PLLP only)
        if (NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, base) ==
            CLK_RST_CONTROLLER_PLLP_BASE_0_PLLP_ENABLE_DISABLE)
            return 0;
        if ((pCinfo->SourceId == NvRmClockSource_PllP0) &&
            (NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BASE_OVRRIDE, base) ==
             CLK_RST_CONTROLLER_PLLP_BASE_0_PLLP_BASE_OVRRIDE_DISABLE))
        {
            if (hRmDevice->ChipId.Id < 0x30 && hRmDevice->ChipId.Id != 0x14)
                return NV_BOOT_PLLP_FIXED_FREQ_KHZ;
            else
                return NVRM_PLLP_FIXED_FREQ_KHZ;
        }

        // PLL formula - Output F = (Reference F * N) / (M * 2^P)
        M = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVM, base);
        N = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVN, base);
        P = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_DIVP, base);
        NV_ASSERT((M != 0) && (N != 0));

        if (pCinfo->PllType == NvRmPllType_UHS)
        {
            // Adjust P divider field size and inverse logic for USB HS PLL
            P = (P & 0x1) ? 0 : 1;
        }
        PllKHz = ((PllKHz * N) / M) >> P;

        // Check slow/fast mode selection for MIPI PLLs
        if (pCinfo->PllType == NvRmPllType_MIPI)
        {
            misc = NV_REGR(hRmDevice,
                NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
                pCinfo->PllMiscOffset);
            if (NV_DRF_VAL(CLK_RST_CONTROLLER, PLLD_MISC, PLLD_FO_MODE, misc) == 1)
            {
                PllKHz = PllKHz >> 3;   // In slow mode output is divided by 8
            }
        }
    }
    if (pCinfo->SourceId == NvRmClockSource_PllD0)
    {
        PllKHz = PllKHz >> 1;   // PLLD output always divided by 2
    }
    return PllKHz;
}

static void
Ap15PllControl(
    NvRmDeviceHandle hRmDevice,
    NvRmClockSource PllId,
    NvBool Enable)
{
    NvU32 base;
    NvU32 delay = 0;
    const NvRmPllClockInfo* pCinfo =
        NvRmPrivGetClockSourceHandle(PllId)->pInfo.pPll;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pCinfo->PllBaseOffset);

    /*
     * PLL control fields used below have the same layout for all PLLs.
     * PLLP h/w field definitions will be used in DRF macros to construct base
     * values for all PLLs.
     */
    base = NV_REGR(hRmDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
        pCinfo->PllBaseOffset);

    if (Enable)
    {
        // No need to enable already enabled PLL - do nothing
        if (NV_DRF_VAL(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE, base) ==
            CLK_RST_CONTROLLER_PLLP_BASE_0_PLLP_ENABLE_ENABLE)
            return;

        // Get ready stabilization delay
        if ((pCinfo->PllType == NvRmPllType_MIPI) ||
            (pCinfo->PllType == NvRmPllType_UHS))
            delay = NVRM_PLL_MIPI_STABLE_DELAY_US;
        else if (pCinfo->PllType == NvRmPllType_LP)
            delay = NVRM_PLL_LP_STABLE_DELAY_US;
        else
            NV_ASSERT(!"Invalid PLL type");

        // Bypass PLL => Enable PLL => wait for PLL to stabilize
        // => switch to PLL output. All other settings preserved.
        base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS,
            ENABLE, base);
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
            pCinfo->PllBaseOffset, base);
        base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE,
            ENABLE, base);
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
            pCinfo->PllBaseOffset, base);

        NvOsWaitUS(delay);

        base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS,
            DISABLE, base);
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
            pCinfo->PllBaseOffset, base);
    }
    else
    {
        // Disable PLL, no bypass. All other settings preserved.
        base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_BYPASS,
            DISABLE, base);
        base = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLP_BASE, PLLP_ENABLE,
            DISABLE, base);
        NV_REGW(hRmDevice, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0),
            pCinfo->PllBaseOffset, base);
    }
    NvRmPrivPllFreqUpdate(hRmDevice, pCinfo);
}

void
NvRmPrivAp15PllConfigureSimple(
    NvRmDeviceHandle hRmDevice,
    NvRmClockSource PllId,
    NvRmFreqKHz MaxOutKHz,
    NvRmFreqKHz* pPllOutKHz)
{
#define NVRM_PLL_FCMP_1 (1000)
#define NVRM_PLL_VCO_RANGE_1 (1000000)
#define NVRM_PLL_FCMP_2 (2000)
#define NVRM_PLL_VCO_RANGE_2 (2000000)

    /*
     * Simple PLL configuration (assuming that target output frequency is
     * always in VCO range, and does not exceed 2GHz).
     * - output divider is set 1:1
     * - input divider is set to get comparison frequency equal or slightly
     *   above 1MHz if VCO is below 1GHz . Otherwise, input divider is set
     *   to get comparison frequency equal or slightly below 2MHz.
     * - feedback divider is calculated based on target output frequency
     * With simple configuration the absolute output frequency error does not
     * exceed half of comparison frequency. It has been verified that simple
     * configuration provides necessary accuracy for all display pixel clocks
     * use cases.
     */
    NvU32 M, N, P;
    NvRmFreqKHz RefKHz, VcoKHz;
    const NvRmPllClockInfo* pCinfo =
        NvRmPrivGetClockSourceHandle(PllId)->pInfo.pPll;
    NvU32 flags = 0;

    NV_ASSERT(hRmDevice);
    VcoKHz = *pPllOutKHz;
    P = 0;

    if (pCinfo->SourceId == NvRmClockSource_PllD0)
    {   // PLLD output is always divided by 2 (after P-divider)
        VcoKHz = VcoKHz << 1;
        MaxOutKHz = MaxOutKHz << 1;
        while (VcoKHz < pCinfo->PllVcoMin)
        {
            VcoKHz = VcoKHz << 1;
            MaxOutKHz = MaxOutKHz << 1;
            P++;
        }
        NV_ASSERT(P <= CLK_RST_CONTROLLER_PLLD_BASE_0_PLLD_DIVP_DEFAULT_MASK);
        flags = NvRmPllConfigFlags_DiffClkEnable;
    }
    if (pCinfo->SourceId == NvRmClockSource_PllX0)
    {
        flags = VcoKHz < NVRM_PLLX_DCC_VCO_MIN ?
            NvRmPllConfigFlags_DccDisable : NvRmPllConfigFlags_DccEnable;
    }
    NV_ASSERT((pCinfo->PllVcoMin <= VcoKHz) && (VcoKHz <= pCinfo->PllVcoMax));
    NV_ASSERT(VcoKHz <= NVRM_PLL_VCO_RANGE_2);
    NV_ASSERT(VcoKHz <= MaxOutKHz);

    RefKHz = NvRmPrivGetClockSourceFreq(pCinfo->InputId);
    NV_ASSERT(RefKHz);
    if (VcoKHz <= NVRM_PLL_VCO_RANGE_1)
        M = RefKHz / NVRM_PLL_FCMP_1;
    else
        M = (RefKHz + NVRM_PLL_FCMP_2 - 1) / NVRM_PLL_FCMP_2;
    N = (RefKHz + ((VcoKHz * M) << 1) ) / (RefKHz << 1);
    if ((RefKHz * N) > (MaxOutKHz * M))
        N--;    // use floor if rounding violates client's max limit

    NvRmPrivAp15PllSet(
        hRmDevice, pCinfo, M, N, P, (NvU32)-1, 0, 0, NV_TRUE, flags);
    *pPllOutKHz = NvRmPrivGetClockSourceFreq(pCinfo->SourceId);
}

static const NvRmPllFixedConfig*
FindPllFixedConfig(
    NvRmFreqKHz InputKHz,
    const NvRmPllFixedConfig* ConfigTable,
    NvU32 TableSize)
{
    NvU32 i;

    for (i = 0; i < TableSize; i++)
    {
        if (ConfigTable[i].InputKHz == InputKHz)
            return &ConfigTable[i];
    }
    return NULL;
}

// Fixed list of PLL HDMI configurations for different reference frequencies
// arranged according to CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_FIELD enum
static const NvRmPllFixedConfig s_Ap15HdmiPllD_Configurations[] =
{
    NVRM_PLLHD_AT_13MHZ,
    NVRM_PLLHD_AT_19MHZ,
    NVRM_PLLHD_AT_12MHZ,
    NVRM_PLLHD_AT_26MHZ,
    NVRM_PLLHD_AT_16MHZ
};

static const NvRmPllFixedConfig s_Ap15HdmiPllC_Configurations[] =
{
    NVRM_PLLHC_AT_13MHZ,
    NVRM_PLLHC_AT_19MHZ,
    NVRM_PLLHC_AT_12MHZ,
    NVRM_PLLHC_AT_26MHZ,
    NVRM_PLLHC_AT_16MHZ
};

#define NVRM_HDMI_CPCON (8)

void
NvRmPrivAp15PllConfigureHdmi(
    NvRmDeviceHandle hRmDevice,
    NvRmClockSource PllId,
    NvRmFreqKHz* pPllOutKHz)
{
    const NvRmPllFixedConfig* pHdmiConfig = NULL;
    const NvRmPllClockInfo* pCinfo =
        NvRmPrivGetClockSourceHandle(PllId)->pInfo.pPll;
    NvRmFreqKHz RefKHz = NvRmPrivGetClockSourceFreq(pCinfo->InputId);

    if (PllId == NvRmClockSource_PllD0)
        pHdmiConfig = FindPllFixedConfig(
            RefKHz, s_Ap15HdmiPllD_Configurations,
            NV_ARRAY_SIZE(s_Ap15HdmiPllD_Configurations));
    else if (PllId == NvRmClockSource_PllC0)
        pHdmiConfig = FindPllFixedConfig(
            RefKHz, s_Ap15HdmiPllC_Configurations,
            NV_ARRAY_SIZE(s_Ap15HdmiPllC_Configurations));
    else
    {
        NV_ASSERT(!"Only PLLD or PLLC should be configured here");
        return;
    }
    NV_ASSERT(pHdmiConfig);
    NvRmPrivAp15PllSet(hRmDevice, pCinfo, pHdmiConfig->M, pHdmiConfig->N,
        pHdmiConfig->P, (NvU32)-1, NVRM_HDMI_CPCON, 0, NV_FALSE, 0);
    *pPllOutKHz = NvRmPrivGetClockSourceFreq(pCinfo->SourceId);
}

/*****************************************************************************/

// Fixed list of PLLP configurations for different reference frequencies
// arranged according to CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_FIELD enum
static const NvRmPllFixedConfig s_Ap15PllPConfigurations[] =
{
    NVRM_PLLP_AT_13MHZ,
    NVRM_PLLP_AT_19MHZ,
    NVRM_PLLP_AT_12MHZ,
    NVRM_PLLP_AT_26MHZ,
    NVRM_PLLP_AT_16MHZ
};

static void
Ap15PllPConfigure(NvRmDeviceHandle hRmDevice)
{
    NvRmFreqKHz PllKHz;
    const NvRmPllFixedConfig* pPllPConfig = NULL;

    const NvRmPllClockInfo* pCinfo =
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllP0)->pInfo.pPll;

    // Configure and enable PllP at RM fixed frequency,
    // if it is not already enabled
    PllKHz = NvRmPrivGetClockSourceFreq(pCinfo->SourceId);
    if (PllKHz == NVRM_PLLP_FIXED_FREQ_KHZ)
        return;

    // Get fixed PLLP configuration for current oscillator  frequency.
    PllKHz = NvRmPrivGetClockSourceFreq(pCinfo->InputId);
    pPllPConfig = FindPllFixedConfig(PllKHz, s_Ap15PllPConfigurations,
                                     NV_ARRAY_SIZE(s_Ap15PllPConfigurations));
    NV_ASSERT(pPllPConfig);

    // Configure and enable PLLP
    NvRmPrivAp15PllSet(hRmDevice, pCinfo, pPllPConfig->M, pPllPConfig->N,
                       pPllPConfig->P, (NvU32)-1, 0, 0, NV_TRUE,
                       NvRmPllConfigFlags_Override);
}

/*****************************************************************************/

// Fixed list of PLLU configurations for different reference frequencies
// arranged according to CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_FIELD enum
static const NvRmPllFixedConfig s_Ap15UsbPllConfigurations[] =
{
    NVRM_PLLU_AT_13MHZ,
    NVRM_PLLU_AT_19MHZ,
    NVRM_PLLU_AT_12MHZ,
    NVRM_PLLU_AT_26MHZ
};

static const NvRmPllFixedConfig s_Ap15UlpiPllConfigurations[] =
{
    NVRM_PLLU_ULPI_AT_13MHZ,
    NVRM_PLLU_ULPI_AT_19MHZ,
    NVRM_PLLU_ULPI_AT_12MHZ,
    NVRM_PLLU_ULPI_AT_26MHZ
};

static const NvRmPllFixedConfig s_Ap15UhsPllConfigurations[] =
{
    NVRM_PLLU_HS_AT_13MHZ,
    NVRM_PLLU_HS_AT_19MHZ,
    NVRM_PLLU_HS_AT_12MHZ,
    NVRM_PLLU_HS_AT_26MHZ,
    NVRM_PLLU_HS_AT_16MHZ
};

static void
PllUmipiConfigure(NvRmDeviceHandle hRmDevice, NvRmFreqKHz TargetFreq)
{
    const NvRmPllFixedConfig* pConfig = NULL;
    const NvRmPllClockInfo* pCinfo =
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllU0)->pInfo.pPll;
    NvRmFreqKHz CurrentFreq = NvRmPrivGetClockSourceFreq(pCinfo->SourceId);
    NvRmFreqKHz RefKHz = NvRmPrivGetClockSourceFreq(pCinfo->InputId);
    const NvRmPllFixedConfig* pUsbConfig = FindPllFixedConfig(
        RefKHz, s_Ap15UsbPllConfigurations,
        NV_ARRAY_SIZE(s_Ap15UsbPllConfigurations));
    const NvRmPllFixedConfig* pUlpiConfig = FindPllFixedConfig(
        RefKHz, s_Ap15UlpiPllConfigurations,
        NV_ARRAY_SIZE(s_Ap15UlpiPllConfigurations));
    NV_ASSERT(pUsbConfig && pUlpiConfig);


    if (CurrentFreq == TargetFreq)
        return;     // PLLU is already configured at target frequency - exit

    if (TargetFreq == NvRmFreqUnspecified)
    {
        // By default set standard USB frequency, if PLLU is not configured
        if ((CurrentFreq == pUsbConfig->OutputKHz) ||
            (CurrentFreq == pUlpiConfig->OutputKHz))
        {
            return; // PLLU is already configured at supported frequency - exit
        }
        pConfig = pUsbConfig;
    }
    else if (TargetFreq == pUsbConfig->OutputKHz)
    {
        pConfig = pUsbConfig;
    }
    else if (TargetFreq == pUlpiConfig->OutputKHz)
    {
        pConfig = pUlpiConfig;
    }
    else
    {
        NV_ASSERT(!"Invalid target frequency");
        return;
    }
    // Configure and enable PLLU
    NvRmPrivAp15PllSet(hRmDevice, pCinfo, pConfig->M, pConfig->N,
                       pConfig->P, (NvU32)-1, 0, 0, NV_TRUE, 0);
}

static void
PllUhsConfigure(NvRmDeviceHandle hRmDevice, NvRmFreqKHz TargetFreq)
{
    const NvRmPllClockInfo* pCinfo =
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllU0)->pInfo.pPll;
    NvRmFreqKHz CurrentFreq = NvRmPrivGetClockSourceFreq(pCinfo->SourceId);
    NvRmFreqKHz RefKHz = NvRmPrivGetClockSourceFreq(pCinfo->InputId);
    const NvRmPllFixedConfig* pUhsConfig = FindPllFixedConfig(
        RefKHz, s_Ap15UhsPllConfigurations,
        NV_ARRAY_SIZE(s_Ap15UhsPllConfigurations));
    NV_ASSERT(pUhsConfig);

    // If PLLU is already configured - exit
    if (CurrentFreq == pUhsConfig->OutputKHz)
        return;

    /*
     * Target may be unspecified, or any of the standard USB, ULPI, or UHS
     * frequencies. In any case, main PLLU HS output is configured at UHS
     * frequency, with ULPI and USB frequencies are generated on secondary
     * outputs by fixed post dividers
     */
    if (!( (TargetFreq == NvRmFreqUnspecified) ||
           (TargetFreq == s_Ap15UsbPllConfigurations[0].OutputKHz) ||
           (TargetFreq == s_Ap15UlpiPllConfigurations[0].OutputKHz) ||
           (TargetFreq == s_Ap15UhsPllConfigurations[0].OutputKHz) )
        )
    {
        NV_ASSERT(!"Invalid target frequency");
        return;
    }
    // Configure and enable PLLU
    NvRmPrivAp15PllSet(hRmDevice, pCinfo, pUhsConfig->M, pUhsConfig->N,
                       pUhsConfig->P, (NvU32)-1, 0, 0, NV_TRUE, 0);
}

static void
Ap15PllUConfigure(NvRmDeviceHandle hRmDevice, NvRmFreqKHz TargetFreq)
{
    const NvRmPllClockInfo* pCinfo =
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllU0)->pInfo.pPll;

    if (pCinfo->PllType == NvRmPllType_MIPI)
        PllUmipiConfigure(hRmDevice, TargetFreq);
    else if (pCinfo->PllType == NvRmPllType_UHS)
        PllUhsConfigure(hRmDevice, TargetFreq);
}

/*****************************************************************************/

// Fixed list of PLLA configurations for supported audio clocks
static const NvRmPllFixedConfig s_Ap15AudioPllConfigurations[] =
{
    NVRM_PLLA_CONFIGURATIONS
};

static void
Ap15PllAConfigure(
    NvRmDeviceHandle hRmDevice,
    NvRmFreqKHz* pAudioTargetKHz)
{
// The reminder bits used to check divisibility
#define REMINDER_BITS (6)

    NvU32 i, rem;
    NvRmFreqKHz OutputKHz;
    NvU32 BestRem = (0x1 << REMINDER_BITS);
    NvU32 BestIndex = NV_ARRAY_SIZE(s_Ap15AudioPllConfigurations) - 1;

    NvRmPllFixedConfig AudioConfig = {0};
    const NvRmPllClockInfo* pPllCinfo =
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllA1)->pInfo.pPll;
    const NvRmDividerClockInfo* pDividerCinfo =
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllA0)->pInfo.pDivider;
    NV_ASSERT(hRmDevice);
    NV_ASSERT(*pAudioTargetKHz);

    // Fixed PLLA FPGA configuration
    if (NvRmPrivGetExecPlatform(hRmDevice) == ExecPlatform_Fpga)
    {
        *pAudioTargetKHz = NvRmPrivGetClockSourceFreq(pDividerCinfo->SourceId);
        return;
    }
    // Find PLLA configuration with smallest output frequency that can be
    // divided by fractional divider into the closest one to the target.
    for (i = 0; i < NV_ARRAY_SIZE(s_Ap15AudioPllConfigurations); i++)
    {
        OutputKHz = s_Ap15AudioPllConfigurations[i].OutputKHz;
        if (*pAudioTargetKHz > OutputKHz)
            continue;
        rem = ((OutputKHz << (REMINDER_BITS + 1)) / (*pAudioTargetKHz)) &
              ((0x1 << REMINDER_BITS) - 1);
        if (rem < BestRem)
        {
            BestRem = rem;
            BestIndex = i;
            if (rem == 0)
                break;
        }
    }

    // Configure PLLA and output divider
    AudioConfig = s_Ap15AudioPllConfigurations[BestIndex];
    NvRmPrivAp15PllSet(hRmDevice, pPllCinfo, AudioConfig.M, AudioConfig.N,
                       AudioConfig.P, (NvU32)-1, 0, 0, NV_TRUE, 0);
    NvRmPrivDividerSet(
        hRmDevice, pDividerCinfo, AudioConfig.D);
    *pAudioTargetKHz = NvRmPrivGetClockSourceFreq(pDividerCinfo->SourceId);
}

static void
Ap15PllAControl(
    NvRmDeviceHandle hRmDevice,
    NvBool Enable)
{
    const NvRmDividerClockInfo* pCinfo =
        NvRmPrivGetClockSourceHandle(NvRmClockSource_PllA0)->pInfo.pDivider;
    if (NvRmPrivGetExecPlatform(hRmDevice) == ExecPlatform_Fpga)
        return; // No PLLA control on FPGA

    if (Enable)
    {
        Ap15PllControl(hRmDevice, NvRmClockSource_PllA1, NV_TRUE);
    }
    else
    {
        // Disable provided PLLA is not used as a source for any clock
        if (NvRmPrivGetDfsFlags(hRmDevice) & NvRmDfsStatusFlags_StopPllA0)
            Ap15PllControl(hRmDevice, NvRmClockSource_PllA1, NV_FALSE);
    }
    NvRmPrivDividerFreqUpdate(hRmDevice, pCinfo);
}

static void
Ap15AudioSyncInit(NvRmDeviceHandle hRmDevice, NvRmFreqKHz AudioSyncKHz)
{
    NvRmFreqKHz AudioTargetKHz;
    NvRmClockSource AudioSyncSource;
    const NvRmSelectorClockInfo* pCinfo;
    NV_ASSERT(hRmDevice);

    // Configure PLLA. Requested frequency must always exactly match one of the
    // fixed audio frequencies.
    AudioTargetKHz = AudioSyncKHz;
    Ap15PllAConfigure(hRmDevice, &AudioTargetKHz);
    NV_ASSERT(AudioTargetKHz == AudioSyncKHz);

    // Use PLLA as audio sync source, and disable doublers.
    // (verify if SoC supports audio sync selectors)
    AudioSyncSource = NvRmClockSource_PllA0;
    if (NvRmPrivGetClockSourceHandle(NvRmClockSource_AudioSync))
    {
        pCinfo = NvRmPrivGetClockSourceHandle(
            NvRmClockSource_AudioSync)->pInfo.pSelector;
        NvRmPrivSelectorClockSet(hRmDevice, pCinfo, AudioSyncSource, NV_FALSE);
    }
    if (NvRmPrivGetClockSourceHandle(NvRmClockSource_MpeAudio))
    {
        pCinfo = NvRmPrivGetClockSourceHandle(
            NvRmClockSource_MpeAudio)->pInfo.pSelector;
        NvRmPrivSelectorClockSet(hRmDevice, pCinfo, AudioSyncSource, NV_FALSE);
    }
}

/*****************************************************************************/

static void
Ap15PllDControl(
    NvRmDeviceHandle hRmDevice,
    NvBool Enable)
{
    NvU32 reg;
    NvRmModuleClockInfo* pCinfo = NULL;
    NvRmModuleClockState* pCstate = NULL;
    NV_ASSERT_SUCCESS(NvRmPrivGetClockState(
        hRmDevice, NvRmModuleID_Dsi, &pCinfo, &pCstate));

    if (Enable)
    {
        Ap15PllControl(hRmDevice, NvRmClockSource_PllD0, NV_TRUE);
        pCstate->actual_freq =
            NvRmPrivGetClockSourceFreq(NvRmClockSource_PllD0);
        return;
    }

    // Disable PLLD if it is not used by either display head or DSI
    reg = NV_REGR(hRmDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        pCinfo->ClkEnableOffset);
    if (NvRmPrivIsSourceSelectedByModule(hRmDevice, NvRmClockSource_PllD0,
                                   NVRM_MODULE_ID(NvRmModuleID_Display, 0)) ||
        NvRmPrivIsSourceSelectedByModule(hRmDevice, NvRmClockSource_PllD0,
                                   NVRM_MODULE_ID(NvRmModuleID_Display, 1)) ||
        ((reg & pCinfo->ClkEnableField) == pCinfo->ClkEnableField))
        return;

    Ap15PllControl(hRmDevice, NvRmClockSource_PllD0, NV_FALSE);
    pCstate->actual_freq =
        NvRmPrivGetClockSourceFreq(NvRmClockSource_PllD0);
}

static void
Ap15PllDConfigure(
    NvRmDeviceHandle hRmDevice,
    NvRmFreqKHz TargetFreq,
    NvU32 DsiInstance)
{
    NvRmFreqKHz MaxFreq = NvRmPrivGetSocClockLimits(NvRmModuleID_Dsi)->MaxKHz;
    NvRmModuleClockInfo* pCinfo = NULL;
    NvRmModuleClockState* pCstate = NULL;

    if(DsiInstance == 1)
    {
        // Set DSIB_CLK_SRC to PLLD in PLLD2_BASE
        SetDsibClkToPllD(hRmDevice);
        NV_ASSERT_SUCCESS(NvRmPrivGetClockState(
                hRmDevice, NVRM_MODULE_ID(NvRmModuleID_Dsi,1), &pCinfo, &pCstate));
    }
    else
    {
        NV_ASSERT_SUCCESS(NvRmPrivGetClockState(
            hRmDevice, NvRmModuleID_Dsi, &pCinfo, &pCstate));
    }
    /*
     * PLLD is adjusted when DDK/ODM is initializing DSI or reconfiguring
     * display clock (for HDMI, DSI, or in some cases CRT).
     */
    if (NvRmIsFixedHdmiKHz(TargetFreq))
    {
        // 480p or 720p or 1080i/1080p HDMI - use fixed PLLD configuration
        NvRmPrivAp15PllConfigureHdmi(
            hRmDevice, NvRmClockSource_PllD0, &TargetFreq);
    }
    else
    {
        // for other targets use simple variable configuration
        NV_ASSERT(TargetFreq <= MaxFreq);
        NvRmPrivAp15PllConfigureSimple(
            hRmDevice, NvRmClockSource_PllD0, MaxFreq, &TargetFreq);
    }

    // Update DSI clock state (PLLD is a single source, no divider)
    pCstate->SourceClock = 0;
    pCstate->Divider = 1;
    pCstate->actual_freq =
        NvRmPrivGetClockSourceFreq(NvRmClockSource_PllD0);
    NvRmPrivModuleVscaleReAttach(hRmDevice,
        pCinfo, pCstate, pCstate->actual_freq, pCstate->actual_freq);
}

/*****************************************************************************/
/*****************************************************************************/

static void
Ap15DisplayClockConfigure(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleClockInfo *pCinfo,
    NvRmFreqKHz MinFreq,
    NvRmFreqKHz MaxFreq,
    NvRmFreqKHz TargetFreq,
    NvRmModuleClockState* pCstate,
    NvU32 flags)
{
    NvU32 i;
    NvRmClockSource SourceId;
    NvRmFreqKHz PixelFreq = TargetFreq;
    NvRmFreqKHz SourceClockFreq = NvRmPrivGetClockSourceFreq(NvRmClockSource_ClkM);
    NvU32 DsiInstance = 0;

    // Clip target to maximum - we still may be able to configure frequency
    // within tolearnce range
    PixelFreq = TargetFreq = NV_MIN(TargetFreq, MaxFreq);

    /*
     * Display clock source selection policy:
     * - if MIPI flag is specified - use PLLD, and reconfigure it as necessary
     * - else if Oscillator output provides required accuracy - use Oscillator
     * - else if PLLP fixed output provides required accuracy - use fixed PLLP
     * - else if PPLC is used by other head - use PLLD, and reconfigure it as
     *   necessary
     * - else - use use PLLC, and reconfigure it as necessary
     */
    if (flags & NvRmClockConfig_MipiSync)
    {
        // PLLD requested - use it as a source, and reconfigure,
        //  unless it is also routed to the pads
        SourceId = NvRmClockSource_PllD0;
        if (!(flags & NvRmClockConfig_InternalClockForPads))
            Ap15PllDConfigure(hRmDevice, TargetFreq, DsiInstance);
    }
    else if (NvRmIsFreqRangeReachable(
        SourceClockFreq, MinFreq, MaxFreq, NVRM_DISPLAY_DIVIDER_MAX))
    {
        // Target frequency is reachable from Oscillator - nothing to do
        SourceId = NvRmClockSource_ClkM;
    }
    else if (NvRmIsFreqRangeReachable(NVRM_PLLP_FIXED_FREQ_KHZ,
                MinFreq, MaxFreq, NVRM_DISPLAY_DIVIDER_MAX))
    {
        // Target frequency is reachable from PLLP0 - make sure it is enabled
        SourceId = NvRmClockSource_PllP0;
        Ap15PllPConfigure(hRmDevice);
    }
    else
    {
        // PLLC is used by the other head - only PLLD left
        SourceId = NvRmClockSource_PllD0;
        Ap15PllDConfigure(hRmDevice, TargetFreq, DsiInstance);
    }

    // Fill in clock state
    for (i = 0; i < NvRmClockSource_Num; i++)
    {
        if (pCinfo->Sources[i] == SourceId)
            break;
    }
    NV_ASSERT(i < NvRmClockSource_Num);
    pCstate->SourceClock = i;       // source index
    pCstate->Divider = 1;           // no divider (display driver has its own)
    pCstate->actual_freq = NvRmPrivGetClockSourceFreq(SourceId); // source KHz
    NV_ASSERT(NvRmIsFreqRangeReachable(
        pCstate->actual_freq, MinFreq, MaxFreq, NVRM_DISPLAY_DIVIDER_MAX));

    if (flags & NvRmClockConfig_SubConfig)
    {
        NvRmModuleClockInfo* pTvDacInfo = NULL;
        NvRmModuleClockState* pTvDacState = NULL;
        NV_ASSERT_SUCCESS(NvRmPrivGetClockState(
            hRmDevice, NvRmModuleID_Tvo, &pTvDacInfo, &pTvDacState));

        // TVDAC is the 2nd TVO subclock (CVE is the 1st one)
        pTvDacInfo += 2;
        pTvDacState += 2;
        NV_ASSERT(pTvDacInfo->Module == NvRmModuleID_Tvo);
        NV_ASSERT(pTvDacInfo->SubClockId == 2);

        // enable the tvdac clock
        EnableTvDacClock(hRmDevice, ModuleClockState_Enable);

        // Set TVDAC = pixel clock (same source index and calculate divider
        // exactly as dc_hal.c does). On T30 matching TVDAC source index is
        // equal to display source index divided by 2, and only even display
        // index would be selected by display policy above
        pTvDacState->SourceClock =
            (hRmDevice->ChipId.Id >= 0x30 || hRmDevice->ChipId.Id == 0x14) ? (i >> 1) : i;
        pTvDacState->Divider =
            (((pCstate->actual_freq * 2 ) + PixelFreq / 2) / PixelFreq) - 2;
        pTvDacState->actual_freq =
            (pCstate->actual_freq * 2 ) / (pTvDacState->Divider + 2);
        NvRmPrivModuleClockSet(hRmDevice, pTvDacInfo, pTvDacState);
    }
    if (flags & NvRmClockConfig_DisableTvDAC)
    {
        // disable the tvdac clock
        EnableTvDacClock(hRmDevice, ModuleClockState_Disable);
    }
}

NvBool
NvRmPrivAp15IsModuleClockException(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleClockInfo *pCinfo,
    NvU32 ClockSourceCount,
    NvRmFreqKHz MinFreq,
    NvRmFreqKHz MaxFreq,
    const NvRmFreqKHz* PrefFreqList,
    NvU32 PrefCount,
    NvRmModuleClockState* pCstate,
    NvU32 flags)
{
    NvU32 i;
    NvRmFreqKHz FreqKHz;
    NvRmClockSource SourceId;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pCinfo && PrefFreqList && pCstate);

    switch (pCinfo->Module)
    {
        case NvRmModuleID_Display:
            /*
             * Special handling for display clocks. Must satisfy requirements
             * for the 1st requested frequency and complete configuration.
             * Note that AP15 display divider is within module itself, so the
             * input request is for pisxel clock, but output *pCstate specifies
             * source frequency. Display driver will configure divider.
             */
            Ap15DisplayClockConfigure(hRmDevice, pCinfo,
                MinFreq, MaxFreq, PrefFreqList[0], pCstate, flags);
            return NV_TRUE;

        case NvRmModuleID_Dsi:
            /*
             * Reconfigure PLLD to match requested frequency, and update DSI
             * clock state.
             */
            Ap15PllDConfigure(hRmDevice, PrefFreqList[0], pCinfo->Instance);
            NV_ASSERT((MinFreq <= pCstate->actual_freq) &&
                      (pCstate->actual_freq <= MaxFreq));
            return NV_TRUE;

        case NvRmModuleID_Hdmi:
            /*
             * Complete HDMI configuration; choose among possible sources:
             * PLLP, PLLD, PLLC in the same order as for display (PLLD or
             * PLLC should be already configured properly for display)
             */
            if (flags & NvRmClockConfig_MipiSync)
                SourceId = NvRmClockSource_PllD0;
            else if (NvRmIsFreqRangeReachable(NVRM_PLLP_FIXED_FREQ_KHZ,
                         MinFreq, MaxFreq, NVRM_DISPLAY_DIVIDER_MAX))
                SourceId = NvRmClockSource_PllP0;
            else
                SourceId = NvRmClockSource_PllC0;

            // HDMI clock state with selected source
            for (i = 0; i < NvRmClockSource_Num; i++)
            {
                if (pCinfo->Sources[i] == SourceId)
                    break;
            }
            NV_ASSERT(i < NvRmClockSource_Num);
            pCstate->SourceClock = i;       // source index
            FreqKHz = NvRmPrivGetClockSourceFreq(SourceId);
            pCstate->Divider = ((FreqKHz << 2) + PrefFreqList[0]) /
                               (PrefFreqList[0] << 1) - 2;
            pCstate->actual_freq = (FreqKHz << 1) / (pCstate->Divider + 2);
            NV_ASSERT(pCstate->Divider <= pCinfo->DivisorFieldMask);
            NV_ASSERT((MinFreq <= pCstate->actual_freq) &&
                      (pCstate->actual_freq <= MaxFreq));
            return NV_TRUE;

        case NvRmModuleID_Spdif:
            if (flags & NvRmClockConfig_SubConfig)
                return NV_FALSE; // Nothing special for SPDIFIN
            // fall through for SPDIFOUT
        case NvRmModuleID_I2s:
        case NvRmModuleID_ExtPeriphClk:
            /*
             * If requested, reconfigure PLLA to match target frequency, and
             * complete clock configuration with PLLA as a source. Otherwise,
             * make sure PLLA is enabled (at current configuration), and
             * continue regular configuration for SPDIFOUT and I2S.
             * For T30 ExtPeriph clock is audio required feq, use PLLAoutput.
             */
            if (flags & NvRmClockConfig_AudioAdjust)
            {
                FreqKHz = PrefFreqList[0];
                Ap15PllAConfigure(hRmDevice, &FreqKHz);

                pCstate->SourceClock = 0;   // PLLA source index
                pCstate->Divider = ((FreqKHz << 2) + PrefFreqList[0]) /
                                    (PrefFreqList[0] << 1) - 2;
                pCstate->actual_freq = (FreqKHz << 1) / (pCstate->Divider + 2);
                if (NvRmPrivGetExecPlatform(hRmDevice) == ExecPlatform_Fpga)
                {   // Fake return on FPGA (PLLA is not configurable, anyway)
                    pCstate->actual_freq = PrefFreqList[0];
                }
                NV_ASSERT(pCinfo->Sources[pCstate->SourceClock] ==
                          NvRmClockSource_PllA0);
                NV_ASSERT(pCstate->Divider <= pCinfo->DivisorFieldMask);
                NV_ASSERT((MinFreq <= pCstate->actual_freq) &&
                          (pCstate->actual_freq <= MaxFreq));
                return NV_TRUE;
            }
            Ap15PllAControl(hRmDevice, NV_TRUE);
            return NV_FALSE;

        case NvRmModuleID_Usb2Otg:
            /*
             * Reconfigure PLLU to match requested frequency, and complete USB
             * clock configuration (PLLU is a single source, no divider)
             */
            Ap15PllUConfigure(hRmDevice, PrefFreqList[0]);
            pCstate->SourceClock = 0;
            pCstate->Divider = 1;
            pCstate->actual_freq =
                NvRmPrivGetClockSourceFreq(pCinfo->Sources[0]);
            return NV_TRUE;

        default:
            // No exception for other modules - continue regular configuration
            return NV_FALSE;
    }
}

static void PllEControl(NvRmDeviceHandle hRmDevice, NvBool Enable)
{
    NvRmPrivPllEControl(hRmDevice, Enable);
    return;
}

void
NvRmPrivConfigureClockSource(
        NvRmDeviceHandle hRmDevice,
        NvRmModuleID ModuleId,
        NvBool enable)
{
    // Extract module and instance from composite module id.
    NvU32 Module   = NVRM_MODULE_ID_MODULE( ModuleId );

    switch (Module)
    {
        case NvRmModuleID_Usb2Otg:
            // Do not disable the PLLU clock once it is enabled
            // Set PLLU default configuration if it is not already configured
            if (enable)
                Ap15PllUConfigure(hRmDevice, NvRmFreqUnspecified);
            break;
        case NvRmModuleID_Spdif:
        case NvRmModuleID_I2s:
            if (enable)
            {
                // Do not enable if PLLA is not used as a source for any clock
                if (NvRmPrivGetDfsFlags(hRmDevice) & NvRmDfsStatusFlags_StopPllA0)
                    break;
            }
            // fall through
        case NvRmModuleID_Mpe:
        case NvRmModuleID_MSENC:
            Ap15PllAControl(hRmDevice, enable);
            break;

        case NvRmModuleID_Dsi:
            Ap15PllDControl(hRmDevice, enable);
            break;

        case NvRmPrivModuleID_Pcie:
            PllEControl(hRmDevice, enable);
            break;
        default:
            break;
    }
    return;
}

