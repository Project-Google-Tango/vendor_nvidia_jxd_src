/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvrm_clocks.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_hwintf.h"
#include "nvrm_memmgr.h"
#include "ap15/ap15rm_private.h"
#include "t11x/project_relocation_table.h"
#include "nvodm_query_discovery.h"

#define NvRmPrivGetStepMV(hRmDevice, step) \
         (s_ChipFlavor.pSocShmoo->ShmooVoltages[(step)])

// Extended clock limits IDs
typedef enum
{
    // Last Module ID
    NvRmClkLimitsExtID_LastModuleID = NvRmPrivModuleID_Num,

    // Extended ID for display A pixel clock limits
    NvRmClkLimitsExtID_DisplayA,

    // Extended ID for display B pixel clock limits
    NvRmClkLimitsExtID_DisplayB,

    // Extended ID for CAR clock sources limits
    NvRmClkLimitsExtID_ClkSrc,

    NvRmClkLimitsExtID_Num,
    NvRmClkLimitsExtID_Force32 = 0x7FFFFFFF,
} NvRmClkLimitsExtID;

/*
 * Module clocks frequency limits table ordered by s/w module ids.
 * Display is a special case and has 3 entries associated:
 * - one entry that corresponds to display ID specifies pixel clock limit used
 * for CAR clock sources configuration; it is retrieved by RM clock manager
 * via private interface (same limit for both CAR display clock selectors);
 * - two entries appended at the end of the table specify pixel clock limits
 * for two display heads used for DDK clock configuration, these limits will
 * be retrieved by DDK via public interface
 * Also appended at the end of the table limits for clock sources (PLLs) forced
 * by CAR clock dividers
 */
static NvRmModuleClockLimits s_ClockRangeLimits[NvRmClkLimitsExtID_Num];

// Translation table for module clock limits scaled with voltage
static const NvRmFreqKHz* s_pClockScales[NvRmClkLimitsExtID_Num];

// Reference counts of clocks that require the respective core voltage to run
static NvU32 s_VoltageStepRefCounts[NVRM_VOLTAGE_STEPS];

// Chip shmoo data records
static NvRmChipFlavor s_ChipFlavor;

static void NvRmPrivChipFlavorInit(NvRmDeviceHandle hRmDevice);
static void NvRmPrivChipFlavorInit(NvRmDeviceHandle hRmDevice)
{
    NvOsMemset((void*)&s_ChipFlavor, 0, sizeof(s_ChipFlavor));

    if (NvRmPrivChipShmooDataInit(hRmDevice, &s_ChipFlavor) == NvSuccess)
    {
        NvOsDebugPrintf("NVRM Initialized shmoo database\n");
        return;
    }
    NV_ASSERT(!"Failed to set clock limits");
}

const NvRmModuleClockLimits*
NvRmPrivClockLimitsInit(NvRmDeviceHandle hRmDevice)
{
    NvU32 i;
    NvRmFreqKHz CpuMaxKHz, AvpMaxKHz, VdeMaxKHz, TDMaxKHz, DispMaxKHz;
    const NvRmSKUedLimits* pSKUedLimits;
    const NvRmScaledClkLimits* pHwLimits;
    const NvRmSocShmoo* pShmoo;

    NV_ASSERT(hRmDevice);
    NvRmPrivChipFlavorInit(hRmDevice);
    pShmoo = s_ChipFlavor.pSocShmoo;
    pHwLimits = &pShmoo->ScaledLimitsList[0];
    pSKUedLimits = pShmoo->pSKUedLimits;

    NvOsMemset((void*)s_pClockScales, 0, sizeof(s_pClockScales));
    NvOsMemset(s_ClockRangeLimits, 0, sizeof(s_ClockRangeLimits));
    NvOsMemset(s_VoltageStepRefCounts, 0, sizeof(s_VoltageStepRefCounts));
    s_VoltageStepRefCounts[0] = NvRmPrivModuleID_Num; // all at minimum step

    // Combine AVP/System clock absolute limit with scaling V/F ladder upper
    // boundary, and set default clock range for all present modules the same
    // as for AVP/System clock
    AvpMaxKHz = pSKUedLimits->AvpMaxKHz;
    for (i = 0; i < pShmoo->ScaledLimitsListSize; i++)
    {
        if (pHwLimits[i].HwDeviceId == NV_DEVID_AVP)
        {
            AvpMaxKHz = NV_MIN(
                AvpMaxKHz, pHwLimits[i].MaxKHzList[pShmoo->ShmooVmaxIndex]);
            break;
        }
    }

    for (i = 0; i < NvRmPrivModuleID_Num; i++)
    {
        NvRmModuleInstance *inst;
        if (NvRmPrivGetModuleInstance(hRmDevice, i, &inst) == NvSuccess)
        {
            s_ClockRangeLimits[i].MaxKHz = AvpMaxKHz;
            s_ClockRangeLimits[i].MinKHz = NVRM_BUS_MIN_KHZ;

        }
    }

    // Fill in limits for modules with slectable clock sources and/or dividers
    // as specified by the h/w table according to the h/w device ID
    // (CPU and AVP are not in relocation table - need translate id explicitly)
    // TODO: need separate subclock limits? (current implementation applies
    // main clock limits to all subclocks)
    for (i = 0; i < pShmoo->ScaledLimitsListSize; i++)
    {
        NvRmModuleID id;
        if (pHwLimits[i].HwDeviceId == NV_DEVID_CPU)
            id = NvRmModuleID_Cpu;
        else if (pHwLimits[i].HwDeviceId == NV_DEVID_AVP)
            id = NvRmModuleID_Avp;
        else if (pHwLimits[i].HwDeviceId == NVRM_DEVID_CLK_SRC)
            id = NvRmClkLimitsExtID_ClkSrc;
        else
            id = NvRmPrivDevToModuleID(pHwLimits[i].HwDeviceId);
        if ((id != NVRM_DEVICE_UNKNOWN) &&
            (pHwLimits[i].SubClockId == 0))
        {
            s_ClockRangeLimits[id].MinKHz = pHwLimits[i].MinKHz;
            s_ClockRangeLimits[id].MaxKHz =
                pHwLimits[i].MaxKHzList[pShmoo->ShmooVmaxIndex];
            s_pClockScales[id] = pHwLimits[i].MaxKHzList;
        }
    }
    // Fill in CPU scaling data if SoC has dedicated CPU rail, and CPU clock
    // characterization data is separated from other modules on common core rail
    if (s_ChipFlavor.pCpuShmoo)
    {
        const NvRmScaledClkLimits* pCpuLimits =
            s_ChipFlavor.pCpuShmoo->pScaledCpuLimits;
        NV_ASSERT(pCpuLimits && (pCpuLimits->HwDeviceId == NV_DEVID_CPU));

        s_ClockRangeLimits[NvRmModuleID_Cpu].MinKHz = pCpuLimits->MinKHz;
        s_ClockRangeLimits[NvRmModuleID_Cpu].MaxKHz =
            pCpuLimits->MaxKHzList[s_ChipFlavor.pCpuShmoo->ShmooVmaxIndex];
        s_pClockScales[NvRmModuleID_Cpu] = pCpuLimits->MaxKHzList;
    }

    // Set AVP upper clock boundary with combined Absolute/Scaled limit;
    // Sync System clock with AVP (System is not in relocation table)
    s_ClockRangeLimits[NvRmModuleID_Avp].MaxKHz = AvpMaxKHz;
    s_ClockRangeLimits[NvRmPrivModuleID_System].MaxKHz =
        s_ClockRangeLimits[NvRmModuleID_Avp].MaxKHz;
    s_ClockRangeLimits[NvRmPrivModuleID_System].MinKHz =
        s_ClockRangeLimits[NvRmModuleID_Avp].MinKHz;
    s_pClockScales[NvRmPrivModuleID_System] = s_pClockScales[NvRmModuleID_Avp];

    // Set VDE upper clock boundary with combined Absolute/Scaled limit (on
    // AP15/Ap16 VDE clock derived from the system bus, and VDE maximum limit
    // must be the same as AVP/System).
    VdeMaxKHz = pSKUedLimits->VdeMaxKHz;
    VdeMaxKHz = NV_MIN(
        VdeMaxKHz, s_ClockRangeLimits[NvRmModuleID_Vde].MaxKHz);

    s_ClockRangeLimits[NvRmModuleID_Vde].MaxKHz = VdeMaxKHz;

    // Set upper clock boundaries for devices on CPU bus (CPU, Mselect,
    // CMC) with combined Absolute/Scaled limits
    CpuMaxKHz = pSKUedLimits->CpuMaxKHz;
    CpuMaxKHz = NV_MIN(
        CpuMaxKHz, s_ClockRangeLimits[NvRmModuleID_Cpu].MaxKHz);
    s_ClockRangeLimits[NvRmModuleID_Cpu].MaxKHz = CpuMaxKHz;
    if (hRmDevice->ChipId.Id == 0x30)
    {
        // FIXME: use ap20 for now
        s_ClockRangeLimits[NvRmPrivModuleID_Mselect].MaxKHz = CpuMaxKHz >> 2;
    }
    else if (hRmDevice->ChipId.Id == 0x35)
    {
        // FIXME: use ap20 for now
        s_ClockRangeLimits[NvRmPrivModuleID_Mselect].MaxKHz = CpuMaxKHz >> 2;
    }
    else if (hRmDevice->ChipId.Id == 0x40)
    {
        // FIXME: use ap20 for now
        s_ClockRangeLimits[NvRmPrivModuleID_Mselect].MaxKHz = CpuMaxKHz >> 2;
    }
    else if (hRmDevice->ChipId.Id == 0x14)
    {
        // FIXME: use ap20 for now
        s_ClockRangeLimits[NvRmPrivModuleID_Mselect].MaxKHz = CpuMaxKHz >> 2;
    }
    else
    {
        NV_ASSERT(!"Unsupported chip ID");
    }

    // Fill in memory controllers absolute range (scaled data is on ODM level)
    s_ClockRangeLimits[NvRmPrivModuleID_MemoryController].MaxKHz =
        pSKUedLimits->McMaxKHz;
    s_ClockRangeLimits[NvRmPrivModuleID_ExternalMemoryController].MaxKHz =
        pSKUedLimits->Emc2xMaxKHz;
    s_ClockRangeLimits[NvRmPrivModuleID_ExternalMemoryController].MinKHz =
        NVRM_SDRAM_MIN_KHZ * 2;
    s_ClockRangeLimits[NvRmPrivModuleID_ExternalMemory].MaxKHz =
        pSKUedLimits->Emc2xMaxKHz / 2;
    s_ClockRangeLimits[NvRmPrivModuleID_ExternalMemory].MinKHz =
        NVRM_SDRAM_MIN_KHZ;

    // Set 3D upper clock boundary with combined Absolute/Scaled limit.
    TDMaxKHz = pSKUedLimits->TDMaxKHz;
    TDMaxKHz = NV_MIN(
        TDMaxKHz, s_ClockRangeLimits[NvRmModuleID_3D].MaxKHz);
    s_ClockRangeLimits[NvRmModuleID_3D].MaxKHz = TDMaxKHz;

    // Set Display upper clock boundary with combined Absolute/Scaled limit.
    // (fill in clock limits for both display heads)
    DispMaxKHz = NV_MAX(pSKUedLimits->DisplayAPixelMaxKHz,
                        pSKUedLimits->DisplayBPixelMaxKHz);
    DispMaxKHz = NV_MIN(
        DispMaxKHz, s_ClockRangeLimits[NvRmModuleID_Display].MaxKHz);
    s_ClockRangeLimits[NvRmClkLimitsExtID_DisplayA].MaxKHz =
        NV_MIN(DispMaxKHz, pSKUedLimits->DisplayAPixelMaxKHz);
    s_ClockRangeLimits[NvRmClkLimitsExtID_DisplayA].MinKHz =
            s_ClockRangeLimits[NvRmModuleID_Display].MinKHz;
    s_ClockRangeLimits[NvRmClkLimitsExtID_DisplayB].MaxKHz =
        NV_MIN(DispMaxKHz, pSKUedLimits->DisplayBPixelMaxKHz);
    s_ClockRangeLimits[NvRmClkLimitsExtID_DisplayB].MinKHz =
            s_ClockRangeLimits[NvRmModuleID_Display].MinKHz;

    return s_ClockRangeLimits;
}

NvRmFreqKHz
NvRmPowerModuleGetMaxFrequency(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleID ModuleId)
{
    NvU32 Instance = NVRM_MODULE_ID_INSTANCE(ModuleId);
    NvRmModuleID Module = NVRM_MODULE_ID_MODULE(ModuleId);
    NV_ASSERT(Module < NvRmPrivModuleID_Num);
    NV_ASSERT(hRmDevice);

    // For all modules, except display, ignore instance, and return
    // max frequency for the clock generated from CAR dividers
    if (Module != NvRmModuleID_Display)
        return s_ClockRangeLimits[Module].MaxKHz;

    // For display return pixel clock for the respective head
    if (Instance == 0)
        return s_ClockRangeLimits[NvRmClkLimitsExtID_DisplayA].MaxKHz;
    else if (Instance == 1)
        return s_ClockRangeLimits[NvRmClkLimitsExtID_DisplayB].MaxKHz;
    else
    {
        NV_ASSERT(!"Inavlid display instance");
        return 0;
    }
}

NvRmMilliVolts
NvRmPrivGetNominalMV(NvRmDeviceHandle hRmDevice)
{
    const NvRmSocShmoo* p = s_ChipFlavor.pSocShmoo;
    return p->ShmooVoltages[p->ShmooVmaxIndex];
}

void
NvRmPrivGetSvopParameters(
    NvRmDeviceHandle hRmDevice,
    NvRmMilliVolts* pSvopLowMv,
    NvU32* pSvopLvSetting,
    NvU32* pSvopHvSetting)
{
    const NvRmSocShmoo* p = s_ChipFlavor.pSocShmoo;

    NV_ASSERT(pSvopLowMv && pSvopLvSetting && pSvopHvSetting);
    *pSvopLowMv = p->SvopLowVoltage;
    *pSvopLvSetting = p->SvopLowSetting;
    *pSvopHvSetting = p->SvopHighSetting;
}

NvRmMilliVolts
NvRmPrivSourceVscaleGetMV(NvRmDeviceHandle hRmDevice, NvRmFreqKHz FreqKHz)
{
    NvU32 i;
    const NvU32* pScaleSrc = s_pClockScales[NvRmClkLimitsExtID_ClkSrc];

    for (i = 0; i < s_ChipFlavor.pSocShmoo->ShmooVmaxIndex; i++)
    {
        if (FreqKHz <= pScaleSrc[i])
            break;
    }
    return NvRmPrivGetStepMV(hRmDevice, i);
}

NvRmMilliVolts
NvRmPrivModulesGetOperationalMV(NvRmDeviceHandle hRmDevice)
{
    NvU32 i;
    NV_ASSERT(hRmDevice);

    for (i = s_ChipFlavor.pSocShmoo->ShmooVmaxIndex; i != 0; i--)
    {
        if (s_VoltageStepRefCounts[i])
            break;
    }
    return NvRmPrivGetStepMV(hRmDevice, i);
}

NvRmMilliVolts
NvRmPrivModuleVscaleGetMV(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleID Module,
    NvRmFreqKHz FreqKHz)
{
    NvU32 i;
    const NvRmFreqKHz* pScale;
    NV_ASSERT(hRmDevice);
    NV_ASSERT(Module < NvRmPrivModuleID_Num);

    // If no scaling for this module - exit
    pScale = s_pClockScales[Module];
    if(!pScale)
        return NvRmPrivGetStepMV(hRmDevice, 0);

    // Find voltage step for the requested frequency, and convert it to MV
    // Use CPU specific voltage ladder if SoC has dedicated CPU rail
    if (s_ChipFlavor.pCpuShmoo && (Module == NvRmModuleID_Cpu))
    {
        for (i = 0; i < s_ChipFlavor.pCpuShmoo->ShmooVmaxIndex; i++)
        {
            if (FreqKHz <= pScale[i])
                break;
        }
        return s_ChipFlavor.pCpuShmoo->ShmooVoltages[i];
    }
    // Use common ladder for all other modules or CPU on core rail
    for (i = 0; i < s_ChipFlavor.pSocShmoo->ShmooVmaxIndex; i++)
    {
        if (FreqKHz <= pScale[i])
            break;
    }
    return NvRmPrivGetStepMV(hRmDevice, i);
}

const NvRmFreqKHz*
NvRmPrivModuleVscaleGetMaxKHzList(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleID Module,
    NvU32* pListSize)
{
    NV_ASSERT(hRmDevice);
    NV_ASSERT(pListSize && (Module < NvRmPrivModuleID_Num));

    // Use CPU specific voltage ladder if SoC has dedicated CPU rail
    if (s_ChipFlavor.pCpuShmoo && (Module == NvRmModuleID_Cpu))
        *pListSize = s_ChipFlavor.pCpuShmoo->ShmooVmaxIndex + 1;
    else
        *pListSize = s_ChipFlavor.pSocShmoo->ShmooVmaxIndex + 1;

    return s_pClockScales[Module];
}

NvRmMilliVolts
NvRmPrivModuleVscaleAttach(
    NvRmDeviceHandle hRmDevice,
    const NvRmModuleClockInfo* pCinfo,
    NvRmModuleClockState* pCstate,
    NvBool Enable)
{
    NvBool Enabled;
    NvU32 reg, vstep1, vstep2;
    NvRmMilliVolts VoltageRequirement = NvRmVoltsUnspecified;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pCinfo && pCstate);

    // If no scaling for this module - exit
    if (!pCstate->Vscale)
        return VoltageRequirement;

    //Check changes in clock status - exit if none (if clock is already
    // enabled || if clock still enabled => if enabled)
    NV_ASSERT(pCinfo->ClkEnableOffset);
    reg = NV_REGR(hRmDevice,
        NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
        pCinfo->ClkEnableOffset);
    Enabled = ((reg & pCinfo->ClkEnableField) == pCinfo->ClkEnableField);
    if (Enabled)
        return VoltageRequirement;

    // Update ref counts for module clock and subclock if any
    // (subclock state are located immediately after main one)
    vstep1 = pCstate->Vstep;
    if (Enable)
    {
        s_VoltageStepRefCounts[vstep1]++;
        if ((pCinfo->Module == NvRmModuleID_Usb2Otg) &&
            (hRmDevice->ChipId.Id == 0x16))
        {
            // Two AP16 USB modules share clock enable control
            s_VoltageStepRefCounts[vstep1]++;
        }
    }
    else
    {
        NV_ASSERT(s_VoltageStepRefCounts[vstep1]);
        s_VoltageStepRefCounts[vstep1]--;
        if ((pCinfo->Module == NvRmModuleID_Usb2Otg) &&
            (hRmDevice->ChipId.Id == 0x16))
        {
            // Two AP16 USB modules share clock enable control
            NV_ASSERT(s_VoltageStepRefCounts[vstep1]);
            s_VoltageStepRefCounts[vstep1]--;
        }
    }
    if ((pCinfo->Module == NvRmModuleID_Spdif) ||
        (pCinfo->Module == NvRmModuleID_Vi) ||
        (pCinfo->Module == NvRmModuleID_Tvo))
    {
        vstep2 = pCstate[1].Vstep;
        if (Enable)
        {
            s_VoltageStepRefCounts[vstep2]++;
            vstep1 = NV_MAX(vstep1, vstep2);
        }
        else
        {
            NV_ASSERT(s_VoltageStepRefCounts[vstep2]);
            s_VoltageStepRefCounts[vstep2]--;
        }
    }

    // Set new voltage requirements if module is to be enabled;
    // voltage can be turned Off if module was disabled.
    if (Enable)
        VoltageRequirement = NvRmPrivGetStepMV(hRmDevice, vstep1);
    else
        VoltageRequirement = NvRmVoltsOff;
    return VoltageRequirement;
}


NvRmMilliVolts
NvRmPrivModuleVscaleReAttach(
    NvRmDeviceHandle hRmDevice,
    const NvRmModuleClockInfo* pCinfo,
    NvRmModuleClockState* pCstate,
    NvRmFreqKHz TargetModuleKHz,
    NvRmFreqKHz TargetSrcKHz)
{
    NvU32 i, j, reg;
    const NvRmFreqKHz* pScale;
    NvRmFreqKHz FreqKHz;
    NvRmMilliVolts VoltageRequirement = NvRmVoltsUnspecified;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pCinfo && pCstate);

    // No scaling for this module - exit
    if (!pCstate->Vscale)
        return VoltageRequirement;

    // Clip target frequency to module clock limits and find voltage step for
    // running at target frequency
    FreqKHz = s_ClockRangeLimits[pCinfo->Module].MinKHz;
    FreqKHz = NV_MAX(FreqKHz, TargetModuleKHz);
    if (FreqKHz > s_ClockRangeLimits[pCinfo->Module].MaxKHz)
        FreqKHz = s_ClockRangeLimits[pCinfo->Module].MaxKHz;

    pScale = s_pClockScales[pCinfo->Module];
    NV_ASSERT(pScale);
    for (i = 0; i < s_ChipFlavor.pSocShmoo->ShmooVmaxIndex; i++)
    {
        if (FreqKHz <= pScale[i])
            break;
    }

    // Find voltage step for using the target source, and select maximum
    // step required for both module and its source to operate
    pScale = s_pClockScales[NvRmClkLimitsExtID_ClkSrc];
    NV_ASSERT(pScale);
    for (j = 0; j < s_ChipFlavor.pSocShmoo->ShmooVmaxIndex; j++)
    {
        if (TargetSrcKHz <= pScale[j])
            break;
    }
    i = NV_MAX(i, j);

    // If voltage step has changed, always update module state, and update
    // ref count provided module clock is enabled
    if (pCstate->Vstep != i)
    {
        NV_ASSERT(pCinfo->ClkEnableOffset);
        reg = NV_REGR(hRmDevice,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            pCinfo->ClkEnableOffset);
        if ((reg & pCinfo->ClkEnableField) == pCinfo->ClkEnableField)
        {
            NV_ASSERT(s_VoltageStepRefCounts[pCstate->Vstep]);
            s_VoltageStepRefCounts[pCstate->Vstep]--;
            s_VoltageStepRefCounts[i]++;
            VoltageRequirement = NvRmPrivGetStepMV(hRmDevice, i);
        }
        pCstate->Vstep = i;
    }
    return VoltageRequirement;
}

void
NvRmPrivModuleSetScalingAttribute(
    NvRmDeviceHandle hRmDevice,
    const NvRmModuleClockInfo* pCinfo,
    NvRmModuleClockState* pCstate)
{
    const NvRmFreqKHz* pScale;

    if ((NvRmPrivGetExecPlatform(hRmDevice) != ExecPlatform_Soc) ||
        ((hRmDevice->ChipId.Id >= 0x30 || hRmDevice->ChipId.Id == 0x14)))
        /* FIXME: no scaling in BL? */
    {
        pCstate->Vscale = NV_FALSE;
        return;
    }

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pCinfo && pCstate);

    // Voltage scaling for free running core clocks is done by DFS
    // independently from module clock control. Therefore modules
    // that have core clock as a source do not have their own v-scale
    // attribute set
    switch (pCinfo->Sources[0])
    {
        case NvRmClockSource_CpuBus:
        case NvRmClockSource_SystemBus:
        case NvRmClockSource_Ahb:
        case NvRmClockSource_Apb:
        case NvRmClockSource_Vbus:
            pCstate->Vscale = NV_FALSE;
            return;
        default:
            break;
    }

    // Memory controller scale is specified separately on ODM layer, as
    // it is board dependent; TVDAC scaling always follow CVE (TV) or
    // display (CRT); PMU transport must work at any volatge - no
    // v-scale attribute for these modules
    switch (pCinfo->Module)
    {
        case NvRmModuleID_Dvc:  // TOD0: check PMU transport with ODM DB
        case NvRmPrivModuleID_MemoryController:
        case NvRmPrivModuleID_ExternalMemoryController:
            pCstate->Vscale = NV_FALSE;
            return;
        case NvRmModuleID_Tvo:
            if (pCinfo->SubClockId == 2)
            {   // TVDAC is TVO subclock 2
                pCstate->Vscale = NV_FALSE;
                return;
            }
            break;
        default:
            break;
    }

    // Check if this module can run at maximum frequency at all
    // voltages - no v-scale for this module as well
    pScale = s_pClockScales[pCinfo->Module];
    if(!pScale)
    {
        NV_ASSERT(!"Need scaling information");
        pCstate->Vscale = NV_FALSE;
        return;
    }
    if (pScale[0] == pScale[s_ChipFlavor.pSocShmoo->ShmooVmaxIndex])
    {
        NvRmMilliVolts SrcMaxMv = NvRmPrivSourceVscaleGetMV(
            hRmDevice, NvRmPrivModuleGetMaxSrcKHz(hRmDevice, pCinfo));
        if (SrcMaxMv == NvRmPrivGetStepMV(hRmDevice, 0))
        {
            pCstate->Vscale = NV_FALSE;
            return;
        }
    }
    // Other modules have v-scale
    pCstate->Vscale = NV_TRUE;
}

NvU32
NvRmPrivGetEmcDqsibOffset(NvRmDeviceHandle hRmDevice)
{
    const NvRmSocShmoo* p = s_ChipFlavor.pSocShmoo;
    return p->DqsibOffset;
}

NvError
NvRmPrivGetOscDoublerTaps(
    NvRmDeviceHandle hRmDevice,
    NvRmFreqKHz OscKHz,
    NvU32* pTaps)
{
    NvU32 i;
    NvU32 size = s_ChipFlavor.pSocShmoo->OscDoublerCfgListSize;
    const NvRmOscDoublerConfig* p = s_ChipFlavor.pSocShmoo->OscDoublerCfgList;

    // Find doubler settings for the specified oscillator frequency, and
    // return the number of taps for the SoC corner
    for (i = 0; i < size; i++)
    {
        if (p[i].OscKHz == OscKHz)
        {
            *pTaps = p[i].Taps[s_ChipFlavor.corner];
            return NvSuccess;
        }
    }
    return NvError_NotSupported;     // Not supported oscillator frequency
}

NvBool NvRmPrivIsCpuRailDedicated(NvRmDeviceHandle hRmDevice)
{
    const NvRmCpuShmoo* p = s_ChipFlavor.pCpuShmoo;
    const NvOdmPeripheralConnectivity* pCpuPmuRail;
    pCpuPmuRail = NvOdmPeripheralGetGuid(NV_VDD_CPU_ODM_ID);
    return ((p != NULL) && (pCpuPmuRail !=NULL));
}

/*****************************************************************************/

