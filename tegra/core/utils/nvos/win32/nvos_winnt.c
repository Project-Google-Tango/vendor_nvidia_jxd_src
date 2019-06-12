/*
 * Copyright (c) 2006-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include <windows.h>
#include "nvos.h"
#include "nvos_win32.h"
#include "nvos_internal.h"
#include "nvassert.h"
#include "nvos_debug.h"

#if NVOS_TRACE || NV_DEBUG
#undef NvOsPhysicalMemMap
#undef NvOsPhysicalMemUnmap
#undef NvOsPhysicalMemMapIntoCaller
#undef NvOsSemaphoreUnmarshal
#undef NvOsInterruptRegister
#undef NvOsInterruptUnregister
#undef NvOsInterruptEnable
#undef NvOsInterruptDone
#undef NvOsPageAlloc
#undef NvOsPageFree
#undef NvOsPageLock
#undef NvOsPageMap
#undef NvOsPageMapIntoPtr
#undef NvOsPageUnmap
#undef NvOsPageAddress
#endif

BOOL WINAPI DllMain(HANDLE hInstDll, DWORD dwReason, LPVOID lpvReserved)
{
    /* NVBUG: 461815
    * forces microsoft compiler to include floating point support
    */
    float f = 0.0;
    (void)f;

    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        NvOsProcessAttachCommonInternal();
        NvOsInitResourceTracker();
        break;

    case DLL_PROCESS_DETACH:
        NvOsDeinitResourceTracker();
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        NvOsTlsRunTerminatorsWin32();
        break;
    }

    return TRUE;
}

NvError
NvOsPhysicalMemMap( NvOsPhysAddr phys, size_t size, NvOsMemAttribute attrib,
    NvU32 flags, void **ptr )
{
    void *addr;

    if (flags & NVOS_MEM_GLOBAL_ADDR)
        return NvError_NotSupported;

    if( flags == NVOS_MEM_NONE )
    {
        /* no permissions */
        addr = VirtualAlloc( 0, size, MEM_RESERVE, PAGE_NOACCESS );
        if (addr == NULL)
            return NvError_InsufficientMemory;
        NvOsResourceAllocated(NvOsResourceType_NvOsPhysicalMemMap, addr);
        *ptr = addr;
        return NvSuccess;
    }
    else
    {
        return NvError_NotImplemented;
    }
}

void
NvOsPhysicalMemUnmap( void *p, size_t size )
{
    if (!p)
        return;
    if (!VirtualFree(p, 0, MEM_RELEASE))
        NV_ASSERT(!"NvOsVirtualFree: VirtualFree() failed");
    NvOsResourceFreed(NvOsResourceType_NvOsPhysicalMemMap, p);
}

NvError
NvOsCopyIn( void *pDst, const void *pSrc, size_t Bytes )
{
    NvOsMemcpy(pDst, pSrc, Bytes);
    return NvError_Success;
}

NvError
NvOsCopyOut( void *pDst, const void *pSrc, size_t Bytes )
{
    NvOsMemcpy(pDst, pSrc, Bytes);
    return NvError_Success;
}

NvError
NvOsIoctl( NvOsFileHandle hFile, NvU32 IotclCode, void *pBuffer,
    NvU32 InBufferSize, NvU32 InOutBufferSize, NvU32 OutBufferSize )
{
    return NvError_NotImplemented;
}

NvError
NvOsSemaphoreUnmarshal(
    NvOsSemaphoreHandle hClientSema,
    NvOsSemaphoreHandle *phDriverSema)
{
    return NvError_NotImplemented;
}


NvError
NvOsInterruptRegister( NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable)
{
    return NvError_NotImplemented;
}

void NvOsInterruptMask(NvOsInterruptHandle handle, NvBool mask)
{
    return;
}


void
NvOsInterruptUnregister( NvOsInterruptHandle handle)
{

}

NvError
NvOsInterruptEnable(NvOsInterruptHandle handle)
{
    return NvError_NotImplemented;
}

void
NvOsInterruptDone( NvOsInterruptHandle handle)
{
    return;
}

NvError
NvOsPageAlloc( size_t size, NvOsMemAttribute attrib,
    NvOsPageFlags flags, NvU32 protect, NvOsPageAllocHandle *descriptor )
{

    return NvError_NotImplemented;
}

void
NvOsPageFree( NvOsPageAllocHandle descriptor )
{

}

NvError
NvOsPageLock(void *ptr, size_t size, NvU32 protect, NvOsPageAllocHandle *descriptor)
{
    return NvError_NotImplemented;
}

NvError
NvOsPageMap( NvOsPageAllocHandle descriptor, size_t offset, size_t size,
    void **ptr )
{

    return NvError_NotImplemented;
}

NvError
NvOsPageMapIntoPtr( NvOsPageAllocHandle descriptor,
                    void *pCallerPtr,
                    size_t offset,
                    size_t size)
{

    return NvError_NotImplemented;
}

void
NvOsPageUnmap( NvOsPageAllocHandle descriptor, void *ptr, size_t size )
{

}

NvOsPhysAddr
NvOsPageAddress( NvOsPageAllocHandle descriptor, size_t offset )
{
    return 0;
}

void
NvOsDataCacheWriteback(void)
{

}

void
NvOsDataCacheWritebackInvalidate(void)
{

}

void
NvOsDataCacheWritebackRange(void *start, NvU32 length)
{

}

void
NvOsDataCacheWritebackInvalidateRange(void *start, NvU32 length)
{

}

void
NvOsInstrCacheInvalidate(void)
{

}

void
NvOsInstrCacheInvalidateRange(void *start, NvU32 length)
{

}

void
NvOsFlushWriteCombineBuffer(void)
{

}

NvError
NvOsPhysicalMemMapIntoCaller( void *pCallerPtr,
                             NvOsPhysAddr phys,
                             size_t size,
                             NvOsMemAttribute attrib,
                             NvU32 flags)
{
    NV_ASSERT(!"Should never get here under winxp");
    return NvError_NotImplemented;
}

void NvOsThreadSetAffinity(NvU32 CpuHint)
{
    // Not yet supported
    return ;
}

NvU32 NvOsThreadGetAffinity(void)
{
    return NVOS_INVALID_CPU_AFFINITY;
}

void NvOsGetProcessInfo(char* buf, NvU32 len)
{
    NvOsStrncpy(buf, "<procinfo>", len);
}

