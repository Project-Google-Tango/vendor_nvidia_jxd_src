/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvrm_init.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "armc.h"
#include "arahb_arbc.h"
#include "nvrm_drf.h"
#include "nvrm_hwintf.h"
#include "nvrm_structure.h"

#define ENABLE_ARB_EMEM_INT (0) // Set nonzero to enable EMEM ARB interrupt

NvError NvRmPrivMcErrorMonitorStart(NvRmDeviceHandle hRm);
void NvRmPrivMcErrorMonitorStop(NvRmDeviceHandle hRm);
void NvRmPrivSetupMc(NvRmDeviceHandle hRm);
static void McErrorIntHandler(void* args);

static NvOsInterruptHandle s_McInterruptHandle = NULL;

static char *s_McClients[] =
{
    "ptcr", "display0a", "display0ab", "display0b",
    "display0bb", "display0c", "display0cb", "display1b", "display1bb",
    "eppup", "g2pr", "g2sr", "mpeunifbr", "viruv", "afir", "avpcarm7r",
    "displayhc", "displayhcb", "fdcdrd", "fdcdrd2", "g2dr", "hdar",
    "host1xdmar", "host1xr", "idxsrd", "idxsrd2", "mpe_ipred", "mpeamemrd",
    "mpecsrd", "ppcsahbdmar", "ppcsahbslvr", "satar", "texsrd", "texsrd2",
    "vdebsevr", "vdember", "vdemcer", "vdetper", "mpcorelpr", "mpcorer",
    "eppu", "eppv", "eppy", "mpeunifbw", "viwsb", "viwu", "viwv", "viwy",
    "g2dw", "afiw", "avpcarm7w", "fdcdwr", "fdcdwr2", "hdaw", "host1xw",
    "ispw", "mpcorelpw", "mpcorew", "mpecswr", "ppcsahbdmaw", "ppcsahbslvw",
    "sataw", "vdebsevw", "vdedbgw", "vdembew", "vdetpmw"
};

static NvU32 s_McClientsArraySz = NV_ARRAY_SIZE(s_McClients);

static void McErrorIntHandler(void* args)
{
    NvU32 IntStatus;
    NvU32 IntClear = 0;
    NvU32 ErrAddr;
    NvU32 ErrStat;
    NvRmDeviceHandle hRm = (NvRmDeviceHandle)args;
    NvU32 ClientId;
    char *ClientName = "unknown";

    IntStatus = NV_REGR(hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), MC_INTSTATUS_0);
    ErrAddr = NV_REGR(hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), MC_ERR_ADR_0);
    ErrStat = NV_REGR(hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), MC_ERR_STATUS_0);
    ClientId = NV_DRF_VAL(MC, ERR_STATUS, ERR_ID, ErrStat);
    if (ClientId < s_McClientsArraySz)
        ClientName = s_McClients[ClientId];

    if ( NV_DRF_VAL(MC, INTSTATUS, SECURITY_VIOLATION_INT, IntStatus) )
    {
        IntClear |= NV_DRF_DEF(MC, INTSTATUS, SECURITY_VIOLATION_INT, SET);
        NvOsDebugPrintf("SECURITY_VIOLATION by %s @ 0x%08x (Status = 0x%08x)",
                ClientName, ErrAddr, ErrStat);
    }
    if ( NV_DRF_VAL(MC, INTSTATUS, DECERR_EMEM_INT, IntStatus) )
    {
        IntClear |= NV_DRF_DEF(MC, INTSTATUS, DECERR_EMEM_INT, SET);
        NvOsDebugPrintf("EMEM_DEC_ERR by %s @ 0x%08x (Status = 0x%08x)",
                ClientName, ErrAddr, ErrStat);
    }
    if ( NV_DRF_VAL(MC, INTSTATUS, ARBITRATION_EMEM_INT, IntStatus) )
    {
        IntClear |= NV_DRF_DEF(MC, INTSTATUS, ARBITRATION_EMEM_INT, SET);
        NvOsDebugPrintf("ARB EMEM Interrupt occurred");
    }
    if ( NV_DRF_VAL(MC, INTSTATUS, INVALID_SMMU_PAGE_INT, IntStatus) )
    {
        IntClear |= NV_DRF_DEF(MC, INTSTATUS, INVALID_SMMU_PAGE_INT, SET);
        NvOsDebugPrintf("SMMU_DEC_ERR by %s @ 0x%08x (Status = 0x%08x)",
                ClientName, ErrAddr, ErrStat);
    }

    // Something other than ARB EMEM?
    if (IntClear & ~NV_DRF_DEF(MC, INTSTATUS, ARBITRATION_EMEM_INT, SET))
    {
        NV_ASSERT(!"MC Decode Error ");
    }
    // Clear the interrupt.
    NV_REGW(hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), MC_INTSTATUS_0, IntClear);
    NvRmInterruptDone(s_McInterruptHandle);
}

NvError NvRmPrivMcErrorMonitorStart(NvRmDeviceHandle hRm)
{
    NvU32 val;
    NvU32 IrqList;
    NvError e = NvSuccess;
    NvOsInterruptHandler handler;

    if (s_McInterruptHandle == NULL)
    {
        // Install an interrupt handler.
        handler = McErrorIntHandler;
        IrqList = NvRmGetIrqForLogicalInterrupt(hRm,
                      NvRmPrivModuleID_MemoryController, 0);
        NV_CHECK_ERROR( NvRmInterruptRegister(hRm, 1, &IrqList,  &handler,
            hRm, &s_McInterruptHandle, NV_TRUE) );
        // Enable Dec Err interrupts in memory Controller.
        val = NV_DRF_DEF(MC, INTMASK, SECURITY_VIOLATION_INTMASK, UNMASKED) |
              NV_DRF_DEF(MC, INTMASK, DECERR_EMEM_INTMASK, UNMASKED) |
#if ENABLE_ARB_EMEM_INT
              NV_DRF_DEF(MC, INTMASK, ARBITRATION_EMEM_INTMASK, UNMASKED) |
#endif
              NV_DRF_DEF(MC, INTMASK, INVALID_SMMU_PAGE_INTMASK, UNMASKED);
        NV_REGW(hRm,
            NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), MC_INTMASK_0, val);
    }
    return e;
}

void NvRmPrivMcErrorMonitorStop(NvRmDeviceHandle hRm)
{
    NvRmInterruptUnregister(hRm, s_McInterruptHandle);
    s_McInterruptHandle = NULL;
}

/* This function sets some performance timings for MC & EMC.
 * Numbers are from the Arch team.
 */
void NvRmPrivSetupMc(NvRmDeviceHandle hRm)
{
    NvU32   reg;

    // Setup the AHB MEM configuration for USB performance.
    // Enabling the AHB prefetch bits for USB1 USB2 and USB3.
    // 64kiloByte boundaries
    // 4096 cycles before prefetched data is invalidated due to inactivity.
    reg = NV_DRF_NUM(AHB_AHB_MEM, PREFETCH_CFG1, ENABLE, 1) |
          NV_DRF_DEF(AHB_AHB_MEM, PREFETCH_CFG1, AHB_MST_ID, AHBDMA)|
          NV_DRF_NUM(AHB_AHB_MEM, PREFETCH_CFG1, ADDR_BNDRY, 0xC) |
          NV_DRF_NUM(AHB_AHB_MEM, PREFETCH_CFG1, INACTIVITY_TIMEOUT, 0x1000);
    NV_REGW( hRm, NVRM_MODULE_ID(NvRmPrivModuleID_Ahb_Arb_Ctrl, 0),
             AHB_AHB_MEM_PREFETCH_CFG1_0, reg );

    reg = NV_DRF_NUM(AHB_AHB_MEM, PREFETCH_CFG2, ENABLE, 1) |
          NV_DRF_DEF(AHB_AHB_MEM, PREFETCH_CFG2, AHB_MST_ID, USB)|
          NV_DRF_DEF(AHB_AHB_MEM, PREFETCH_CFG2, AHB_MST_ID, USB2)|
          NV_DRF_DEF(AHB_AHB_MEM, PREFETCH_CFG2, AHB_MST_ID, USB3)|
          NV_DRF_NUM(AHB_AHB_MEM, PREFETCH_CFG2, ADDR_BNDRY, 0xC) |
          NV_DRF_NUM(AHB_AHB_MEM, PREFETCH_CFG2, INACTIVITY_TIMEOUT, 0x1000);
    NV_REGW( hRm, NVRM_MODULE_ID(NvRmPrivModuleID_Ahb_Arb_Ctrl, 0),
             AHB_AHB_MEM_PREFETCH_CFG2_0, reg );
}

