/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_hwintf.h"
#include "nvrm_module.h"
#include "nvrm_module_private.h"
#include "nvrm_moduleids.h"
#include "nvrm_chipid.h"
#include "nvrm_chip_private.h"
#include "nvrm_drf.h"
#include "nvrm_power.h"
#include "nvrm_structure.h"
#include "ap15/ap15rm_private.h"
#include "arapb_misc.h"
#include "t11x/arapbpm.h"
#include "t11x/arfuse.h"
#include "arclk_rst.h"
#include "common/common_misc_private.h"

void ModuleReset(NvRmDeviceHandle hDevice, NvRmModuleID ModuleId,
    NvBool hold);

void
NvRmModuleResetWithHold(NvRmDeviceHandle hDevice, NvRmModuleID ModuleId,
    NvBool hold)
{
#if !NV_IS_AVP
    ModuleReset(hDevice, ModuleId, hold);
#endif
}

void NvRmModuleReset(NvRmDeviceHandle hDevice, NvRmModuleID ModuleId)
{
    NvRmModuleResetWithHold(hDevice, ModuleId, NV_FALSE);
}

NvError
NvRmModuleGetCapabilities(
    NvRmDeviceHandle hDevice,
    NvRmModuleID ModuleId,
    NvRmModuleCapability *pCaps,
    NvU32 NumCaps,
    void **Capability )
{
    NvError err;
    NvRmChipId *id;
    NvRmModuleCapability *cap;
    NvRmModuleInstance *inst;
    void *ret = 0;
    NvU32 i;
    NvU8 eco = 0;
    NvBool bMatch = NV_FALSE;
    NvRmModulePlatform curPlatform = NvRmModulePlatform_Silicon;

    if( NVRM_MODULE_ID_MODULE( ModuleId ) != NvRmModuleID_Vp8 )
    {
        err = NvRmPrivGetModuleInstance( hDevice, ModuleId, &inst );
        if( err != NvSuccess )
        {
            return err;
        }
    }

    id = NvRmPrivGetChipId( hDevice );

    // Check for emu and QT. Handle simulation separately below.
    if( id->Major == 0 && id->Netlist != 0 )
    {
        if( id->Minor != 0 )
        {
           curPlatform = NvRmModulePlatform_Emulation;
        }
        else
        {
            curPlatform = NvRmModulePlatform_Quickturn;
        }
    }

    // Loop through the caps and return the config that maches
    for( i = 0; i < NumCaps; i++ )
    {
        cap = &pCaps[i];

        if( NVRM_MODULE_ID_MODULE( ModuleId ) == NvRmModuleID_Vp8 )
        {
            if (id->Id != 0x30)
            {
                volatile NvU32 reg_val = NV_REGR( hDevice,
                                NVRM_MODULE_ID( NvRmModuleID_Fuse,
                                NVRM_MODULE_ID_INSTANCE( NvRmModuleID_Fuse ) ),
                                FUSE_VP8_ENABLE_0 );
                if( NV_DRF_VAL(FUSE, VP8_ENABLE, VP8_ENABLE, reg_val) )
                    ret = cap->Capability;
            }
            break;
        }

        // WAR for same versions in all chips' relocation table
        if( NVRM_MODULE_ID_MODULE( ModuleId ) == NvRmModuleID_MSENC )
        {
            if( id->Id == 0x35 &&
                cap->MajorVersion == 2 &&
                cap->MinorVersion == 0 )
            {
                ret = cap->Capability;
                break;
            }
            if( id->Id == 0x14 &&
                cap->MajorVersion == 3 &&
                cap->MinorVersion == 0 )
            {
                ret = cap->Capability;
                break;
            }
            if( id->Id == 0x40 &&
                cap->MajorVersion == 3 &&
                cap->MinorVersion == 1 )
            {
                ret = cap->Capability;
                break;
            }
        }

        // WAR for same versions in all chips' relocation table
        if( NVRM_MODULE_ID_MODULE( ModuleId ) == NvRmModuleID_Vde )
        {
            if( id->Id == 0x35 &&
                cap->MajorVersion == 3 &&
                cap->MinorVersion == 0 )
            {
                ret = cap->Capability;
                break;
            }
            if( id->Id == 0x14 &&
                cap->MajorVersion == 4 &&
                cap->MinorVersion == 0 )
            {
                ret = cap->Capability;
                break;
            }
            if( id->Id == 0x40 &&
                cap->MajorVersion == 5 &&
                cap->MinorVersion == 0 )
            {
                ret = cap->Capability;
                break;
            }
        }

        if( NVRM_MODULE_ID_MODULE( ModuleId ) == NvRmModuleID_Se )
        {
            if( id->Id == 0x35 &&
                cap->MajorVersion == 2 &&
                cap->MinorVersion == 0 )
            {
                ret = cap->Capability;
                break;
            }
            if( id->Id == 0x14 &&
                cap->MajorVersion == 2 &&
                cap->MinorVersion == 0 )
            {
                ret = cap->Capability;
                break;
            }
            if( id->Id == 0x40 &&
                cap->MajorVersion == 2 &&
                cap->MinorVersion == 0 )
            {
                ret = cap->Capability;
                break;
            }
        }

        if( cap->MajorVersion == inst->MajorVersion &&
            cap->MinorVersion == inst->MinorVersion )
        {
            // return closest eco cap if match
            if(curPlatform == cap->Platform)
            {
                if( !eco ||
                    (cap->EcoLevel > eco && cap->EcoLevel <= id->Minor) )
                {
                    ret = cap->Capability;
                    eco = cap->EcoLevel;
                    bMatch = NV_TRUE;
                    continue;
                }
            }

            // return possible cap if not match
            if( !bMatch )
                ret = cap->Capability;
        }
    }

    if( !ret )
    {
        NV_ASSERT(!"Could not find matching version of module in table");
        *Capability = 0;
        return NvError_NotSupported;
    }

    *Capability = ret;
    return NvSuccess;
}

NvError
NvRmFindModule( NvRmDeviceHandle hDevice, NvU32 Address,
    NvRmModuleIdHandle *phModuleId )
{
    NvU32 i;
    NvU32 devid;
    NvRmModuleTable *tbl;
    NvRmModuleInstance *inst;
    NvRmModule *mod;
    NvU16 num;

    if ((hDevice == NULL) || (phModuleId == NULL))
    {
        return NvError_BadParameter;
    }

    tbl = NvRmPrivGetModuleTable( hDevice );

    mod = tbl->Modules;
    for (i = 0; i < NvRmPrivModuleID_Num; i++)
    {
        if (mod[i].Index == NVRM_MODULE_INVALID)
        {
            continue;
        }

        // For each instance of the module id ...
        inst = tbl->ModInst + mod[i].Index;
        devid = inst->DeviceId;
        num = 0;

        // We never search for these device ids by address. They either
        // don't have an address or they are aliaes of some other device.
        if ((devid == NvRmPrivModuleID_InterruptSw) ||
            (devid == NvRmPrivModuleID_ExternalMemory_MMIO))
        {
            continue;
        }

        while (devid == inst->DeviceId)
        {
            // If the device address matches
            if (inst->PhysAddr == Address)
            {
                // Return the module id and instance information.
                ((NvRmModuleId *)phModuleId)->ModuleId =
                    (NvRmPrivModuleID)NVRM_MODULE_ID(i, num);
                return NvSuccess;
            }

            inst++;
            num++;
        }
    }

    // we are here implies no matching module was found.
    return NvError_ModuleNotPresent;
}

NvBool
NvRmModuleIsMemory(NvRmDeviceHandle hDevHandle, NvRmModuleID ModId)
{
    NvRmModuleID    ModuleId = (NvRmModuleID)NVRM_MODULE_ID_MODULE(ModId);
    NvU32           Instance = NVRM_MODULE_ID_INSTANCE(ModId);
    NvU32           NumInstances;

    NV_ASSERT(hDevHandle);

    switch (ModuleId)
    {
        case NvRmPrivModuleID_ExternalMemory:
        case NvRmPrivModuleID_InternalMemory:
        case NvRmPrivModuleID_Iram:
        case NvRmPrivModuleID_Tcram:
            return NV_TRUE;

        case NvRmPrivModuleID_ExternalMemory_MMIO:
            NumInstances = NvRmModuleGetNumInstances(hDevHandle, ModId);

            if (Instance < NumInstances)
            {
                /* Read extended address map aperture configuration. */
                NvU32 Reg = NV_REGR(hDevHandle,
                                    NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
                                    APBDEV_PMC_GLB_AMAP_CFG_0);

                /* Aperture configured as DRAM? */
                if (Reg & (1 << Instance))
                {
                    return NV_TRUE;
                }
            }
            break;

        default:
            break;
    }

    return NV_FALSE;
}

NvError NvRmPrivGpuGetInfo(
    NvRmDeviceHandle hRm,
    NvRmPrivGpuInfo *pGpuInfo)
{
     if (!hRm || !pGpuInfo)
        return NvError_BadParameter;

    if (hRm->ChipId.Id == NVRM_T114_ID)
    {
        pGpuInfo->NumPixelPipes = 4;
        pGpuInfo->NumALUsPerPixelPipe = 3;
    }
    else if (hRm->ChipId.Id == NVRM_T148_ID)
    {
        volatile NvU32 reg_val = NV_REGR( hRm,
                                NVRM_MODULE_ID( NvRmModuleID_Fuse,
                                NVRM_MODULE_ID_INSTANCE( NvRmModuleID_Fuse ) ),
                                FUSE_SKU_DIRECT_CONFIG_0 );
        pGpuInfo->NumPixelPipes = 2;
        pGpuInfo->NumALUsPerPixelPipe = 6;
        if (reg_val & 0x200)
            pGpuInfo->NumPixelPipes = 1;

        if (reg_val & 0x400)
            pGpuInfo->NumALUsPerPixelPipe = 1;
    }
    else
    {
        pGpuInfo->NumPixelPipes = 1;
        pGpuInfo->NumALUsPerPixelPipe = 1;
    }
    return NvSuccess;
}

NvError NvRmGetRandomBytes(
    NvRmDeviceHandle hRm,
    NvU32 NumBytes,
    void *pBytes)
{
    NvU8 *Array = (NvU8 *)pBytes;
    NvU16 Val;

    if (!hRm || !pBytes)
        return NvError_BadParameter;

    while (NumBytes)
    {
        Val = (NvU16)NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_PLL_LFSR_0);
        *Array++ = (Val & 0xff);
        Val>>=8;
        NumBytes--;
        if (NumBytes)
        {
            *Array++ = (Val & 0xff);
            NumBytes--;
        }
    }

    return NvSuccess;
}

NvError
NvRmPrivModuleInit( NvRmModuleTable *mod_table, NvU32 *reloc_table )
{
    NvError err;
    NvU32 i;

    /* invalidate the module table */
    for( i = 0; i < NvRmPrivModuleID_Num; i++ )
    {
        mod_table->Modules[i].Index = NVRM_MODULE_INVALID;
    }

    /* clear the irq map */
    NvOsMemset( &mod_table->IrqMap, 0, sizeof(mod_table->IrqMap) );

    err = NvRmPrivRelocationTableParse( reloc_table,
        &mod_table->ModInst, &mod_table->LastModInst,
        mod_table->Modules, &mod_table->IrqMap );
    if( err != NvSuccess )
    {
        NV_ASSERT( !"NvRmPrivModuleInit failed" );
        return err;
    }

    NV_ASSERT( mod_table->LastModInst);
    NV_ASSERT( mod_table->ModInst );

    mod_table->NumModuleInstances = mod_table->LastModInst -
        mod_table->ModInst;

    return NvSuccess;
}

void
NvRmPrivModuleDeinit( NvRmModuleTable *mod_table )
{
}

void
NvRmPrivListModuleInstances(NvRmDeviceHandle hDevice)
{
    NvRmModuleTable *tbl;
    NvRmModuleInstance *inst;
    NvU32 devid;
    NvU32 instances;

    NV_ASSERT( hDevice );
    tbl       = NvRmPrivGetModuleTable( hDevice );
    inst      = tbl->ModInst;
    devid     = inst->DeviceId;
    instances = 1;
    while (inst != tbl->LastModInst) {
        if (devid == inst->DeviceId) {
            instances++;
        }
        else {
            NvOsDebugPrintf("DevId=%d, moduleID=%d, instances=%d\n",
                            devid, NvRmPrivDevToModuleID(devid), instances);
            devid = inst->DeviceId;
            instances = 1;
        }
        inst++;
    }
}

NvError
NvRmPrivGetModuleInstance( NvRmDeviceHandle hDevice, NvRmModuleID ModuleId,
    NvRmModuleInstance **out )
{
    NvRmModuleTable *tbl;
    NvRmModule *module;             // Pointer to module table
    NvRmModuleInstance *inst;       // Pointer to device instance
    NvU32 DeviceId;                 // Hardware device id
    NvU32 Module;
    NvU32 Instance;
    NvU32 Bar;
    NvU32 idx;

    *out = NULL;

    NV_ASSERT( hDevice );

    tbl = NvRmPrivGetModuleTable( hDevice );

    Module   = NVRM_MODULE_ID_MODULE( ModuleId );
    Instance = NVRM_MODULE_ID_INSTANCE( ModuleId );
    Bar      = NVRM_MODULE_ID_BAR( ModuleId );
    NV_ASSERT( (NvU32)Module < (NvU32)NvRmPrivModuleID_Num );

    // Get a pointer to the first instance of this module id type.
    module = tbl->Modules;

    // Check whether the index is valid or not.
    if (module[Module].Index == NVRM_MODULE_INVALID)
    {
        return NvError_NotSupported;
    }

    inst = tbl->ModInst + module[Module].Index;

    // Get its device id.
    DeviceId = inst->DeviceId;

    // find the right instance and bar
    idx = 0;
    while( inst->DeviceId == DeviceId )
    {
        if( idx == Instance && inst->Bar == Bar )
        {
            break;
        }
        if( inst->Bar == 0 )
        {
            idx++;
        }

        inst++;
    }

    // Is this a valid instance and is it of the same hardware type?
    if( (inst >= tbl->LastModInst) || (DeviceId != inst->DeviceId) )
    {
        // Invalid instance.
        return NvError_BadValue;
    }

    *out = inst;

    // Check if instance is still valid and not bonded out.
    // Still returning inst structure.
    if ( (NvU16)-1 == inst->DevIdx )
        return NvError_NotSupported;

    return NvSuccess;
}

void
NvRmModuleGetBaseAddress( NvRmDeviceHandle hDevice,
    NvRmModuleID ModuleId, NvRmPhysAddr* pBaseAddress,
    NvU32* pSize )
{
    NvRmModuleInstance *inst;
    NvRmChipId *id;

    // Ignore return value. In case of error, inst gets 0.
    (void)NvRmPrivGetModuleInstance(hDevice, ModuleId, &inst);

    if (pBaseAddress)
        *pBaseAddress = inst ? inst->PhysAddr : 0;
    if (pSize)
        *pSize = inst ? inst->Length : 0;

    id = NvRmPrivGetChipId(hDevice);
    /*
     * Present relocation table tells only lower SDRAM size.
     * T12x does not have any High vector
     * address region at the end of lower SDRAM but present relocation
     * table gives the size less than 1MB to 2GB size so updated
     * the size accordingly for not wasting any space at the end of
     * Lower SDRAM */
    if ((id->Id == 0x40) && (ModuleId == NvRmPrivModuleID_ExternalMemory))
        *pSize = 0x80000000;
}

NvU32
NvRmModuleGetNumInstances(
    NvRmDeviceHandle hDevice,
    NvRmModuleID Module)
{
    NvError e;
    NvRmModuleInstance *inst;
    NvU32 n;
    NvU32 id;

    e = NvRmPrivGetModuleInstance( hDevice, NVRM_MODULE_ID(Module, 0), &inst);
    if( e != NvSuccess )
    {
        return 0;
    }

    if (NVRM_MODULE_ID_MODULE( Module ) == NvRmModuleID_3D)
    {
        if ((inst->MajorVersion == 1) && (inst->MinorVersion == 3))
            return 2;
    }

    n = 0;
    id = inst->DeviceId;
    while( inst->DeviceId == id )
    {
        if( inst->Bar == 0 )
        {
            n++;
        }

        inst++;
    }

    return n;
}

NvError
NvRmModuleGetModuleInfo(
    NvRmDeviceHandle    hDevice,
    NvRmModuleID        module,
    NvU32 *             pNum,
    NvRmModuleInfo      *pModuleInfo )
{
    NvU32   instance = 0;
    NvU32   i = 0;

    if ( NULL == pNum )
        return NvError_BadParameter;

    // if !pModuleInfo, returns total number of entries
    while ( (NULL == pModuleInfo) || (i < *pNum) )
    {
        NvRmModuleInstance *inst;
        NvError e = NvRmPrivGetModuleInstance(
            hDevice, NVRM_MODULE_ID(module, instance), &inst);
        if (e != NvSuccess)
        {
            if ( !(inst && ((NvU8)-1 == inst->DevIdx)) )
                break;

             /* else if a module instance not avail (bonded out), continue
              *  looking for next instance
              */
        }
        else
        {
            if ( pModuleInfo )
            {
                pModuleInfo->Instance = instance;
                pModuleInfo->Bar = inst->Bar;
                pModuleInfo->BaseAddress = inst->PhysAddr;
                pModuleInfo->Length = inst->Length;
                pModuleInfo++;
            }

            i++;
        }

        instance++;
    }

    *pNum = i;

    return NvSuccess;
}

NvError NvRmCheckValidSecureState( NvRmDeviceHandle hDevice )
{
    return NvSuccess;
}

NvError NvRmCheckValidNVSIState( NvRmDeviceHandle hDevice )
{
    return NvSuccess;
}

NvRmChipIdLimited NvRmPrivGetChipIdLimited( void )
{
    static NvRmChipIdLimited gs_ChipId = { 0, 0 };
    volatile NvU32 RegValue;
    NvU8 *VirtAddr;
    NvError e;

    if(gs_ChipId.Id)
        return gs_ChipId;

    e = NvRmPhysicalMemMap(0x70000000, 0x1000, NVOS_MEM_READ_WRITE,
        NvOsMemAttribute_Uncached, (void **)&VirtAddr);
    if (e != NvSuccess)
    {
        NV_DEBUG_PRINTF(("NvRmPrivGetChipIdLimited: APB misc aperture map failure\n"));
        return gs_ChipId;
    }

    /* chip id is in the misc aperture */
    RegValue = NV_READ32( VirtAddr + APB_MISC_GP_HIDREV_0 );
    gs_ChipId.Id = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, RegValue);
    gs_ChipId.Revision = NV_DRF_VAL(APB_MISC, GP_HIDREV, MAJORREV, RegValue);
    NvOsPhysicalMemUnmap(VirtAddr, 0x1000);
    return gs_ChipId;
}
