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
#include "nvrm_hardware_access.h"
#include "nvrm_module_private.h"
#include "nvrm_rmctrace.h"
#include "nvrm_hwintf.h"

// FIXME:  This file needs to be split up, when we build user/kernel 
//         The NvRegr/NvRegw should thunk to the kernel since the rm
//         handle is not usable in user space.
//
//         NvRmPhysicalMemMap/NvOsPhysicalMemUnmap need to be in user space.
//

static NvRmModuleInstance *
get_instance( NvRmDeviceHandle rm, NvRmModuleID aperture )
{
    NvRmModuleTable *tbl;
    NvRmModuleInstance *inst;
    NvU32 Module   = NVRM_MODULE_ID_MODULE( aperture );
    NvU32 Instance = NVRM_MODULE_ID_INSTANCE( aperture );
    NvU32 Bar      = NVRM_MODULE_ID_BAR( aperture );
    NvU32 DeviceId;
    NvU32 idx = 0;

    tbl = NvRmPrivGetModuleTable( rm );

    inst = tbl->ModInst + tbl->Modules[Module].Index;
    NV_ASSERT( inst );
    NV_ASSERT( inst < tbl->LastModInst );

    DeviceId = inst->DeviceId;

    // find the right instance and bar
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

    NV_ASSERT( inst->DeviceId == DeviceId );
    NV_ASSERT( inst->VirtAddr );

    return inst;
}

NvU32 NvRegr( NvRmDeviceHandle rm, NvRmModuleID aperture, NvU32 offset )
{
    NvRmModuleInstance *inst;
    void *addr;

    inst = get_instance( rm, aperture );
    addr = (void *)((NvUPtr)inst->VirtAddr + offset);
    return NV_READ32( addr );
}

void NvRegw( NvRmDeviceHandle rm, NvRmModuleID aperture, NvU32 offset,
    NvU32 data )
{
    NvRmModuleInstance *inst;
    void *addr;

    inst = get_instance( rm, aperture );
    addr = (void *)((NvUPtr)inst->VirtAddr + offset);
    NV_WRITE32( addr, data );
}

void NvRegrm( NvRmDeviceHandle rm, NvRmModuleID aperture, NvU32 num,
    const NvU32 *offsets, NvU32 *values )
{
    void *addr;
    NvRmModuleInstance *inst;
    NvU32 i;

    inst = get_instance( rm, aperture );
    for( i = 0; i < num; i++ )
    {
        addr = (void *)((NvUPtr)inst->VirtAddr + offsets[i]);
        values[i] = NV_READ32( addr );
    }
}

void NvRegwm( NvRmDeviceHandle rm, NvRmModuleID aperture, NvU32 num,
    const NvU32 *offsets, const NvU32 *values )
{
    void *addr;
    NvRmModuleInstance *inst;
    NvU32 i;

    inst = get_instance( rm, aperture );
    for( i = 0; i < num; i++ )
    {
        addr = (void *)((NvUPtr)inst->VirtAddr + offsets[i]);
        NV_WRITE32( addr, values[i] );
    }
}

void NvRegwb( NvRmDeviceHandle rm, NvRmModuleID aperture, NvU32 num,
    NvU32 offset, const NvU32 *values )
{
    void *addr;
    NvRmModuleInstance *inst;

    inst = get_instance( rm, aperture );
    addr = (void *)((NvUPtr)inst->VirtAddr + offset);
    NV_WRITE( addr, values, (num << 2) );
}

void NvRegrb( NvRmDeviceHandle rm, NvRmModuleID aperture, NvU32 num,
    NvU32 offset, NvU32 *values )
{
    void *addr;
    NvRmModuleInstance *inst;

    inst = get_instance( rm, aperture );
    addr = (void *)((NvUPtr)inst->VirtAddr + offset);
    NV_READ( values, addr, (num << 2 ) );
}
