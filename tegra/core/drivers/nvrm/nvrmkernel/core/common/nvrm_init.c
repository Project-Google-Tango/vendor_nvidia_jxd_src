/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvutil.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_init.h"
#include "nvrm_rmctrace.h"
#include "nvrm_configuration.h"
#include "nvrm_heap.h"
#include "nvrm_pmu_private.h"
#include "nvrm_processor.h"
#include "nvrm_structure.h"
#include "nvrm_xpc.h"
#include "nvodm_query.h"
#include "common/nvrm_hwintf.h"
#include "nvrm_pinmux_utils.h"
#include "nvrm_minikernel.h"
#include "arapb_misc.h"
#include "arapbpm.h"
#include "arfuse.h"

extern NvRmCfgMap g_CfgMap[];

void NvRmPrivMemoryInfo( NvRmDeviceHandle hDevice );
void NvRmPrivReadChipId( NvRmDeviceHandle rm );
void NvRmPrivGetSku( NvRmDeviceHandle rm );
/** Returns the pointer to the relocation table */
NvU32 *NvRmPrivGetRelocationTable( NvRmDeviceHandle hDevice );
NvError NvRmPrivMapApertures( NvRmDeviceHandle rm );
void NvRmPrivUnmapApertures( NvRmDeviceHandle rm );
NvU32 NvRmPrivGetBctCustomerOption(NvRmDeviceHandle hRm);

NvRmCfgMap g_CfgMap[] =
{
    { "NV_CFG_RMC_FILE", NvRmCfgType_String, (void *)"",
        STRUCT_OFFSET(RmConfigurationVariables, RMCTraceFileName) },

    { 0 }
};

NvRmModuleTable *
NvRmPrivGetModuleTable(
    NvRmDeviceHandle hDevice )
{
    return &hDevice->ModuleTable;
}

void
NvRmPrivReadChipId( NvRmDeviceHandle rm )
{
    NvU32 reg;
    NvRmChipId *id;
    NvU8 *VirtAddr;
    NvError e;

    NV_ASSERT( rm );
    id = &rm->ChipId;

    /* Hard coding the address of the chip ID address space, as we haven't yet
     * parsed the relocation table.
     */
    e = NvRmPhysicalMemMap(0x70000000, 0x1000, NVOS_MEM_READ_WRITE,
        NvOsMemAttribute_Uncached, (void **)&VirtAddr);
    if (e != NvSuccess)
    {
        NV_DEBUG_PRINTF(("APB misc aperture map failure\n"));
        return;
    }

    /* chip id is in the misc aperture */
    reg = NV_READ32( VirtAddr + APB_MISC_GP_HIDREV_0 );
    id->Id = (NvU16)NV_DRF_VAL( APB_MISC_GP, HIDREV, CHIPID, reg );
    id->Major = (NvU8)NV_DRF_VAL( APB_MISC_GP, HIDREV, MAJORREV, reg );
    id->Minor = (NvU8)NV_DRF_VAL( APB_MISC_GP, HIDREV, MINORREV, reg );

    NV_ASSERT(NV_DRF_VAL(APB_MISC_GP, HIDREV, HIDFAM, reg) ==
	      APB_MISC_GP_HIDREV_0_HIDFAM_HANDHELD_SOC);

    reg = NV_READ32( VirtAddr + APB_MISC_GP_EMU_REVID_0 );
    id->Netlist = (NvU16)NV_DRF_VAL( APB_MISC_GP, EMU_REVID, NETLIST, reg );
    id->Patch = (NvU16)NV_DRF_VAL( APB_MISC_GP, EMU_REVID, PATCH, reg );

    if( id->Major == 0 )
    {
        char *emu;
        if( id->Netlist == 0 )
        {
            NvOsDebugPrintf( "Simulation Chip: 0x%x\n", id->Id );
        }
        else
        {
            if( id->Minor == 0 )
            {
                emu = "QuickTurn";
            }
            else
            {
                emu = "FPGA";
            }

            NvOsDebugPrintf( "Emulation (%s) Chip: 0x%x Netlist: 0x%x "
                "Patch: 0x%x\n", emu, id->Id, id->Netlist, id->Patch );
        }
    }
    else
    {
        // on real silicon

        NvRmPrivGetSku( rm );

        NvOsDebugPrintf( "Chip Id: 0x%x (Handheld SOC) Major: 0x%x Minor: 0x%x "
            "SKU: 0x%x\n", id->Id, id->Major, id->Minor, id->SKU );
    }
    NvOsPhysicalMemUnmap(VirtAddr, 0x1000);

    id->Private = NULL;
}

void
NvRmPrivGetSku( NvRmDeviceHandle rm )
{
    NvError e;
    NvRmChipId *id;
    NvU8 *FuseVirt;
    NvU32 reg;

    NV_ASSERT( rm );
    id = &rm->ChipId;

    /* Read the fuse only on real silicon, as it is not guaranteed to be
     * present on emulation/simulation platforms.
     */
    e = NvRmPhysicalMemMap(0x7000f800, 0x400, NVOS_MEM_READ_WRITE,
        NvOsMemAttribute_Uncached, (void **)&FuseVirt);
    if (e == NvSuccess)
    {
        // Read the SKU from the fuse module.
        reg = NV_READ32( FuseVirt + FUSE_SKU_INFO_0 );
        id->SKU = (NvU16)reg;
        NvOsPhysicalMemUnmap(FuseVirt, 0x400);
    }
    else
    {
        NV_ASSERT(!"Cannot map the FUSE aperture to get the SKU");
        id->SKU = 0;
    }
}

NvError
NvRmPrivMapApertures( NvRmDeviceHandle rm )
{
    NvRmModuleTable *tbl;
    NvRmModuleInstance *inst;
    NvRmModule *mod;
    NvU32 devid;
    NvU32 i;
    NvError e;

    NV_ASSERT( rm );

    /* loop over the instance list and map everything */
    tbl = &rm->ModuleTable;
    mod = tbl->Modules;
    for( i = 0; i < NvRmPrivModuleID_Num; i++ )
    {
        if( mod[i].Index == NVRM_MODULE_INVALID )
        {
            continue;
        }

        if ((i != NvRmPrivModuleID_Ahb_Arb_Ctrl ) &&
            (i != NvRmPrivModuleID_ApbDma ) &&
            (i != NvRmPrivModuleID_ApbDmaChannel ) &&
            (i != NvRmPrivModuleID_ClockAndReset ) &&
            (i != NvRmPrivModuleID_ExternalMemoryController ) &&
            (i != NvRmPrivModuleID_Gpio ) &&
            (i != NvRmPrivModuleID_Interrupt ) &&
            (i != NvRmPrivModuleID_InterruptArbGnt ) &&
            (i != NvRmPrivModuleID_InterruptDrq ) &&
            (i != NvRmPrivModuleID_MemoryController ) &&
            (i != NvRmModuleID_Misc) &&
            (i != NvRmPrivModuleID_ArmPerif) &&
            (i != NvRmModuleID_3D) &&
            (i != NvRmModuleID_CacheMemCtrl ) &&
            (i != NvRmModuleID_Display) &&
            (i != NvRmModuleID_Dvc) &&
            (i != NvRmModuleID_FlowCtrl ) &&
            (i != NvRmModuleID_Fuse ) &&
            (i != NvRmModuleID_GraphicsHost ) &&
            (i != NvRmModuleID_I2c) &&
            (i != NvRmModuleID_Isp) &&
            (i != NvRmModuleID_Mpe) &&
            (i != NvRmModuleID_MSENC) &&
            (i != NvRmModuleID_Pmif ) &&
            (i != NvRmModuleID_Mipi ) &&
            (i != NvRmModuleID_ResourceSema ) &&
            (i != NvRmModuleID_SysStatMonitor ) &&
            (i != NvRmModuleID_TimerUs ) &&
            (i != NvRmModuleID_Vde ) &&
            (i != NvRmModuleID_ExceptionVector ) &&
            (i != NvRmModuleID_Usb2Otg ) &&
            (i != NvRmModuleID_Vi) &&
            (i != NvRmModuleID_Kbc) &&
            (i != NvRmModuleID_Rtc)
            )
        {
            continue;
        }

        /* FIXME If the multiple instances of the same module is adjacent to
         * each other then we can do one allocation for all those modules.
         */

        /* map all of the device instances */
        inst = tbl->ModInst + mod[i].Index;
        devid = inst->DeviceId;
        while( devid == inst->DeviceId )
        {
            /* If this is a device that actually has an aperture... */
            if (inst->PhysAddr)
            {
                e = NvRmPhysicalMemMap(
                        inst->PhysAddr, inst->Length, NVOS_MEM_READ_WRITE,
                        NvOsMemAttribute_Uncached, &inst->VirtAddr);
                if (e != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("Device %d at physical addr 0x%X has no "
                        "virtual mapping\n", devid, inst->PhysAddr));
                    return e;
                }
            }

            inst++;
        }
    }

    return NvSuccess;
}

void
NvRmPrivUnmapApertures( NvRmDeviceHandle rm )
{
    NvRmModuleTable *tbl;
    NvRmModuleInstance *inst;
    NvRmModule *mod;
    NvU32 devid;
    NvU32 i;

    NV_ASSERT( rm );

    /* loop over the instance list and unmap everything */
    tbl = &rm->ModuleTable;
    mod = tbl->Modules;
    for( i = 0; i < NvRmPrivModuleID_Num; i++ )
    {
        if( mod[i].Index == NVRM_MODULE_INVALID )
        {
            continue;
        }

        /* map all of the device instances */
        inst = tbl->ModInst + mod[i].Index;
        devid = inst->DeviceId;
        while( devid == inst->DeviceId )
        {
            NvOsPhysicalMemUnmap( inst->VirtAddr, inst->Length );
            inst++;
        }
    }
}

NvU32
NvRmPrivGetBctCustomerOption(NvRmDeviceHandle hRm)
{
    return NV_REGR(hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),
        APBDEV_PMC_SCRATCH20_0);
}

NvRmChipId *
NvRmPrivGetChipId(
    NvRmDeviceHandle hDevice )
{
    return &hDevice->ChipId;
}
extern NvRmDeviceHandle NvRmPrivGetRmDeviceHandle(void);
extern NvError NvRmPrivInitKeyList(NvRmDeviceHandle hRm,
                            const NvU32 *InitialValues,
                            NvU32 InitialCount);
extern void NvRmPrivInterruptTableInit( NvRmDeviceHandle hRmDevice );

void NvRmBasicInit(NvRmDeviceHandle * pHandle)
{
    NvRmDevice *rm = 0;
    NvError err;
    NvU32 *table = 0;
    NvU32 BctCustomerOption = 0;

    *pHandle = 0;
    rm = NvRmPrivGetRmDeviceHandle();

    if( rm->bBasicInit )
    {
        *pHandle = rm;
        return;
    }

    /* get the default configuration */
    err = NvRmPrivGetDefaultCfg( g_CfgMap, &rm->cfg );
    if( err != NvSuccess )
    {
        goto fail;
    }

    /* get the requested configuration */
    err = NvRmPrivReadCfgVars( g_CfgMap, &rm->cfg );
    if( err != NvSuccess )
    {
        goto fail;
    }

    /* Read the chip Id and store in the Rm structure. */
    NvRmPrivReadChipId( rm );

    // init the module control (relocation table, resets, etc.)
    table = NvRmPrivGetRelocationTable( rm );
    if( !table )
    {
        goto fail;
    }

    err = NvRmPrivModuleInit( &rm->ModuleTable, table );
    if( err != NvSuccess )
    {
        goto fail;
    }

    NvRmPrivMemoryInfo( rm );

    // setup the hw apertures
    err = NvRmPrivMapApertures( rm );
    if( err != NvSuccess )
    {
        goto fail;
    }

    BctCustomerOption = NvRmPrivGetBctCustomerOption(rm);
    err = NvRmPrivInitKeyList(rm, &BctCustomerOption, 1);
    if (err != NvSuccess)
    {
        goto fail;
    }

    // Now populate the logical interrupt table.
    NvRmPrivInterruptTableInit( rm );

    rm->bBasicInit = NV_TRUE;
    // basic init is a super-set of preinit
    rm->bPreInit = NV_TRUE;
    *pHandle = rm;

fail:
    return;
}
extern void NvRmPrivDeInitKeyList(NvRmDeviceHandle hRm);
void
NvRmBasicClose(NvRmDeviceHandle handle)
{
    NvRmPrivDeInitKeyList(handle);
    /* unmap the apertures */
    NvRmPrivUnmapApertures( handle );
    /* deallocate the instance table */
    NvRmPrivModuleDeinit( &handle->ModuleTable );
}
