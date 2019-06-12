/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <stdlib.h>
#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_memmgr.h"
#include "nvrm_power.h"
#include "nvrm_rmctrace.h"
#include "nvrm_memmgr_private.h"
#include "nvrm_hwintf.h"
#include "nvrm_power_private.h"
#include "nvrm_interrupt.h"

NvU8 NvRmMemRd08(NvRmMemHandle hMem, NvU32 Offset)
{
    NvU8 buffer[4];
    NvRmMemRead(hMem, Offset, buffer, sizeof(buffer[0]));
    return buffer[0];
}

NvU16 NvRmMemRd16(NvRmMemHandle hMem, NvU32 Offset)
{
    NvU16 buffer[2];
    NV_ASSERT(!(Offset & 1));
    NvRmMemRead(hMem, Offset, buffer, sizeof(buffer[0]));
    return buffer[0];
}

NvU32 NvRmMemRd32(NvRmMemHandle hMem, NvU32 Offset)
{
    NvU32 buffer[1];
    NV_ASSERT(!(Offset & 3));
    NvRmMemRead(hMem, Offset, buffer, sizeof(buffer[0]));
    return buffer[0];
}


void NvRmMemWr08(NvRmMemHandle hMem, NvU32 Offset, NvU8 Data)
{
    NvU8 buffer[4];
    buffer[0] = Data;
    NvRmMemWrite(hMem, Offset, buffer, sizeof(buffer[0]));
}

void NvRmMemWr16(NvRmMemHandle hMem, NvU32 Offset, NvU16 Data)
{
    NvU16 buffer[2];
    buffer[0] = Data;
    NV_ASSERT(!(Offset & 1));
    NvRmMemWrite(hMem, Offset, buffer, sizeof(buffer[0]));
}

void NvRmMemWr32(NvRmMemHandle hMem, NvU32 Offset, NvU32 Data)
{
    NvU32 buffer[1];
    buffer[0] = Data;
    NV_ASSERT(!(Offset & 3));
    NvRmMemWrite(hMem, Offset, buffer, sizeof(buffer[0]));
}

NvError NvRmMemMapIntoCallerPtr(
    NvRmMemHandle hMem,
    void  *pCallerPtr,
    NvU32 Offset,
    NvU32 Size)
{
    return NvError_NotSupported;
}

NvError NvRmInterruptRegister(
    NvRmDeviceHandle hRmDevice,
    NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable)
{

    // The stub just always passes through to nvos
    return NvOsInterruptRegister(IrqListSize, pIrqList, 
            pIrqHandlerList, context, handle, InterruptEnable);
}

void NvRmInterruptUnregister(
    NvRmDeviceHandle hRmDevice,
    NvOsInterruptHandle handle)
{
    NvOsInterruptUnregister(handle);
}

NvError NvRmInterruptEnable(
    NvRmDeviceHandle hRmDevice,
    NvOsInterruptHandle handle)
{
    return NvOsInterruptEnable(handle);
}

void NvRmInterruptDone( NvOsInterruptHandle handle )
{
    NvOsInterruptDone( handle );
}

/* stubbing these out for user mode nvrm until we figure
 * out how to handle multi-process rmc files
 */
NvError NvRmRmcOpen( const char *name, NvRmRmcFile *rmc )
{
    return NvSuccess;
}

void NvRmRmcClose( NvRmRmcFile *rmc )
{
    return;
}

void NvRmRmcTrace( NvRmRmcFile *rmc, const char *format, ... )
{
    return;
}

static NvRmRmcFile s_FakeRmcFile;

NvError NvRmGetRmcFile( NvRmDeviceHandle hDevice, NvRmRmcFile **file )
{
    s_FakeRmcFile.file = NULL;
    s_FakeRmcFile.enable = NV_FALSE;

    *file = &s_FakeRmcFile;
    return NvSuccess;
}



void *NvRmHostAlloc(size_t size)
{
    return NvOsAlloc(size);
}

void NvRmHostFree(void *ptr)
{
    NvOsFree(ptr);
}

// FIXME: Phony function(s) that does nothing to make the link of libnvrm.dll
// happy. To fix, put this in the .idl file, which is even uglier
void NvRmPrivHeapCarveoutGetInfo(NvU32 *CarveoutPhysBase, 
                            void  **pCarveout,
                            NvU32 *CarveoutSize)
{
    return;
}

NvRmPmRequest NvRmPrivPmThread(void)
{
    return NvRmPmRequest_None;
}

