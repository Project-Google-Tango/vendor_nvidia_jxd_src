/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Cross Proc Communication driver </b>
 *
 * @b Description: Implements the interface to the NvDdk XPC.
 *
 */

#include "nvrm_xpc.h"
#include "nvrm_memmgr.h"
#include "ap15rm_xpc_hw_private.h"
#include "nvrm_hardware_access.h"
#include "nvassert.h"
#include "ap20/ararb_sema.h"
#include "ap20/arictlr_arbgnt.h"
#include "nvrm_avp_shrd_interrupt.h"

// Minimum sdram offset required so that avp can access the address which is
// passed.
// AVP can not access the 0x0000:0000 to 0x0000:0040
enum { MIN_SDRAM_OFFSET = 0x100};


//There are only 32 arb semaphores
#define MAX_ARB_NUM 32

#define ARBSEMA_REG_READ(pArbSemaVirtAdd, reg) \
        NV_READ32(pArbSemaVirtAdd + (ARB_SEMA_##reg##_0))

#define ARBSEMA_REG_WRITE(pArbSemaVirtAdd, reg, data) \
        NV_WRITE32(pArbSemaVirtAdd + (ARB_SEMA_##reg##_0), (data));

#define ARBGNT_REG_READ(pArbGntVirtAdd, reg) \
        NV_READ32(pArbGntVirtAdd + (ARBGNT_##reg##_0))

#define ARBGNT_REG_WRITE(pArbGntVirtAdd, reg, data) \
        NV_WRITE32(pArbGntVirtAdd + (ARBGNT_##reg##_0), (data));

static NvOsInterruptHandle s_arbInterruptHandle = NULL;

// Combines the Processor Xpc system details. This contains the details of the
// receive/send message queue and messaging system.
typedef struct NvRmPrivXpcMessageRec
{
    NvRmDeviceHandle hDevice;

    // Hw mail box register.
    CpuAvpHwMailBoxReg HwMailBoxReg;

} NvRmPrivXpcMessage;

typedef struct NvRmPrivXpcArbSemaRec
{
    NvRmDeviceHandle hDevice;
    NvU8 *pArbSemaVirtAddr;
    NvU8 *pArbGntVirtAddr;
    NvOsSemaphoreHandle semaphore[MAX_ARB_NUM];
    NvOsMutexHandle mutex[MAX_ARB_NUM];
    NvOsIntrMutexHandle hIntrMutex;

} NvRmPrivXpcArbSema;

static NvRmPrivXpcArbSema s_ArbSema;

//Forward declarations
static NvError InitArbSemaSystem(NvRmDeviceHandle hDevice);
static void ArbSemaIsr(void *args);
NvU32 GetArbIdFromRmModuleId(NvRmModuleID modId);
/**
 * Initialize the cpu avp hw mail box address and map the hw register address
 * to virtual address.
 * Thread Safety: Caller responsibility
 */
static NvError
InitializeCpuAvpHwMailBoxRegister(NvRmPrivXpcMessageHandle hXpcMessage)
{
    NvError e;
    NvRmPhysAddr ResourceSemaPhysAddr;

    // Get base address of the hw mail box register. This register is in the set
    // of resource semaphore module Id.
    NvRmModuleGetBaseAddress(hXpcMessage->hDevice,
        NVRM_MODULE_ID(NvRmModuleID_ResourceSema, 0),
        &ResourceSemaPhysAddr, &hXpcMessage->HwMailBoxReg.BankSize);

    // Map the base address to the virtual address.
    hXpcMessage->HwMailBoxReg.pHwMailBoxRegBaseVirtAddr = NULL;
    NV_CHECK_ERROR(NvRmPhysicalMemMap(
        ResourceSemaPhysAddr, hXpcMessage->HwMailBoxReg.BankSize,
        NVOS_MEM_READ_WRITE, NvOsMemAttribute_Uncached,
        (void **)&hXpcMessage->HwMailBoxReg.pHwMailBoxRegBaseVirtAddr));

    NvRmPrivXpcHwResetOutbox(&hXpcMessage->HwMailBoxReg);

    return NvSuccess;
}

/**
 * DeInitialize the cpu avp hw mail box address and unmap the hw register address
 * virtual address.
 * Thread Safety: Caller responsibility
 */
static void DeInitializeCpuAvpHwMailBoxRegister(NvRmPrivXpcMessageHandle hXpcMessage)
{
    // Unmap the hw register base virtual address
    NvOsPhysicalMemUnmap(hXpcMessage->HwMailBoxReg.pHwMailBoxRegBaseVirtAddr,
                        hXpcMessage->HwMailBoxReg.BankSize);
    hXpcMessage->HwMailBoxReg.pHwMailBoxRegBaseVirtAddr = NULL;
}

/**
 * Create the cpu-avp messaging system.
 * This function will call other helper function to create the messaging technique
 * used for cpu-avp communication.
 * Thread Safety: Caller responsibility
 */
static NvError
CreateCpuAvpMessagingSystem(NvRmPrivXpcMessageHandle hXpcMessage)
{
    NvError Error = NvSuccess;

    Error = InitializeCpuAvpHwMailBoxRegister(hXpcMessage);

#if NV_IS_AVP
    hXpcMessage->HwMailBoxReg.IsCpu = NV_FALSE;
#else
    hXpcMessage->HwMailBoxReg.IsCpu = NV_TRUE;
#endif

    // If error found then destroy all the allocation and initialization,
    if (Error)
        DeInitializeCpuAvpHwMailBoxRegister(hXpcMessage);

    return Error;
}


/**
 * Destroy the cpu-avp messaging system.
 * This function destroy all the allocation/initialization done for creating
 * the cpu-avp messaging system.
 * Thread Safety: Caller responsibility
 */
static void DestroyCpuAvpMessagingSystem(NvRmPrivXpcMessageHandle hXpcMessage)
{
    // Destroy the cpu-avp hw mail box registers.
    DeInitializeCpuAvpHwMailBoxRegister(hXpcMessage);
    hXpcMessage->HwMailBoxReg.pHwMailBoxRegBaseVirtAddr = NULL;
    hXpcMessage->HwMailBoxReg.BankSize = 0;
}


NvError
NvRmPrivXpcCreate(
    NvRmDeviceHandle hDevice,
    NvRmPrivXpcMessageHandle *phXpcMessage)
{
    NvError Error = NvSuccess;
    NvRmPrivXpcMessageHandle hNewXpcMsgHandle = NULL;

    *phXpcMessage = NULL;

    // Allocates the memory for the xpc message handle.
    hNewXpcMsgHandle = NvOsAlloc(sizeof(*hNewXpcMsgHandle));
    if (!hNewXpcMsgHandle)
    {
        return NvError_InsufficientMemory;
    }

    // Initialize all the members of the xpc message handle.
    hNewXpcMsgHandle->hDevice = hDevice;
    hNewXpcMsgHandle->HwMailBoxReg.pHwMailBoxRegBaseVirtAddr = NULL;
    hNewXpcMsgHandle->HwMailBoxReg.BankSize = 0;

    // Create the messaging system between the processors.
    Error = CreateCpuAvpMessagingSystem(hNewXpcMsgHandle);

    // if error the destroy all allocations done here.
    if (Error)
    {
        NvOsFree(hNewXpcMsgHandle);
        hNewXpcMsgHandle = NULL;
    }

#if NV_IS_AVP
    Error = InitArbSemaSystem(hDevice);
    if (Error)
    {
        NvOsFree(hNewXpcMsgHandle);
        hNewXpcMsgHandle = NULL;
    }
#endif

    // Copy the new xpc message handle into the passed parameter.
    *phXpcMessage = hNewXpcMsgHandle;
    return Error;
}


/**
 * Destroy the Rm Xpc message handle.
 * Thread Safety: It is provided inside the function.
 */
void NvRmPrivXpcDestroy(NvRmPrivXpcMessageHandle hXpcMessage)
{
    // If not a null pointer then destroy.
    if (hXpcMessage)
    {
        // Destroy the messaging system between processor.
        DestroyCpuAvpMessagingSystem(hXpcMessage);

        // Free the allocated memory for the xpc message handle.
        NvOsFree(hXpcMessage);
    }
}


// Set the outbound mailbox with the given data.  We might have to spin until
// it's safe to send the message.
NvError
NvRmPrivXpcSendMessage(NvRmPrivXpcMessageHandle hXpcMessage, NvU32 data)
{
    NvRmPrivXpcHwSendMessageToTarget(&hXpcMessage->HwMailBoxReg, data);
    return NvSuccess;
}


// Get the value currently in the inbox register.  This read clears the incoming
// interrupt.
NvU32
NvRmPrivXpcGetMessage(NvRmPrivXpcMessageHandle hXpcMessage)
{
    NvU32 data;
    NvRmPrivXpcHwReceiveMessageFromTarget(&hXpcMessage->HwMailBoxReg, &data);
    return data;
}

NvError NvRmXpcInitArbSemaSystem(NvRmDeviceHandle hDevice)
{
#if NV_IS_AVP
    return NvSuccess;
#else
    return InitArbSemaSystem(hDevice);
#endif
}

static NvError InitArbSemaSystem(NvRmDeviceHandle hDevice)
{
    NvOsInterruptHandler ArbSemaHandler;
    NvRmPhysAddr ArbSemaBase, ArbGntBase;
    NvU32        ArbSemaSize, ArbGntSize;
    NvU32 irq;
    NvError e;
    NvU32 i = 0;

    irq = NvRmGetIrqForLogicalInterrupt(
             hDevice, NvRmModuleID_ArbitrationSema, 0);

    ArbSemaHandler = ArbSemaIsr;

    NV_CHECK_ERROR_CLEANUP(
        NvRmInterruptRegister(hDevice, 1, &irq, &ArbSemaHandler,
            hDevice, &s_arbInterruptHandle, NV_TRUE)
    );

    NvRmModuleGetBaseAddress(hDevice,
            NVRM_MODULE_ID(NvRmModuleID_ArbitrationSema, 0),
            &ArbSemaBase, &ArbSemaSize);

    NvRmModuleGetBaseAddress(hDevice,
            NVRM_MODULE_ID(NvRmPrivModuleID_InterruptArbGnt, 0),
            &ArbGntBase, &ArbGntSize);

    NV_CHECK_ERROR_CLEANUP(
        NvRmPhysicalMemMap(ArbSemaBase, ArbSemaSize, NVOS_MEM_READ_WRITE,
            NvOsMemAttribute_Uncached, (void**)&s_ArbSema.pArbSemaVirtAddr)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvRmPhysicalMemMap(ArbGntBase, ArbGntSize, NVOS_MEM_READ_WRITE,
            NvOsMemAttribute_Uncached, (void**)&s_ArbSema.pArbGntVirtAddr)
    );

    //Initialize all the semaphores and mutexes
    for (i=0;i<MAX_ARB_NUM;i++)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvOsSemaphoreCreate(&s_ArbSema.semaphore[i], 0)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvOsMutexCreate(&s_ArbSema.mutex[i])
        );
    }

    NV_CHECK_ERROR_CLEANUP(
        NvOsIntrMutexCreate(&s_ArbSema.hIntrMutex)
    );

fail:

    return e;
}

void NvRmXpcDeinitArbSemaSystem(NvRmDeviceHandle hDevice)
{
#if NV_IS_AVP
    return;
#else
    NvU32 i = 0;
    NvOsIntrMutexDestroy(s_ArbSema.hIntrMutex);
    //De-initialize all the semaphores and mutexes
    for (i=0;i<MAX_ARB_NUM;i++)
    {
        NvOsSemaphoreDestroy(s_ArbSema.semaphore[i]);
        NvOsMutexDestroy(s_ArbSema.mutex[i]);
    }
    NvRmInterruptUnregister(hDevice, s_arbInterruptHandle);
#endif
}

static void ArbSemaIsr(void *args)
{
    NvU32 int_mask, proc_int_enable, arb_gnt, i = 0;

    NvOsIntrMutexLock(s_ArbSema.hIntrMutex);
    //Check which arb semaphores have been granted to this processor
    arb_gnt = ARBSEMA_REG_READ(s_ArbSema.pArbSemaVirtAddr, SMP_GNT_ST);

    //Figure out which arb semaphores were signalled and then disable them.
#if NV_IS_AVP
    proc_int_enable = ARBGNT_REG_READ(s_ArbSema.pArbGntVirtAddr, COP_ENABLE);
    int_mask = arb_gnt & proc_int_enable;
    ARBGNT_REG_WRITE(s_ArbSema.pArbGntVirtAddr,
        COP_ENABLE, (proc_int_enable & ~int_mask));
#else
    proc_int_enable = ARBGNT_REG_READ(s_ArbSema.pArbGntVirtAddr, CPU_ENABLE);
    int_mask = arb_gnt & proc_int_enable;
    ARBGNT_REG_WRITE(s_ArbSema.pArbGntVirtAddr,
        CPU_ENABLE, (proc_int_enable & ~int_mask));
#endif

    //Signal all the required semaphores
    do
    {
        if (int_mask & 0x1)
        {
            NvOsSemaphoreSignal(s_ArbSema.semaphore[i]);
        }
        int_mask >>= 1;
        i++;

    } while (int_mask);

    NvOsIntrMutexUnlock(s_ArbSema.hIntrMutex);
    NvRmInterruptDone(s_arbInterruptHandle);
}

NvU32 GetArbIdFromRmModuleId(NvRmModuleID modId)
{
    NvU32 arbId;

    switch(modId)
    {
        case NvRmModuleID_BseA:
            arbId = NvRmArbSema_Bsea;
            break;
        case NvRmModuleID_Vde:
        default:
            arbId = NvRmArbSema_Vde;
            break;
    }

    return arbId;
}

void NvRmXpcModuleAcquire(NvRmModuleID modId)
{
    NvU32 RequestedSemaNum;
    NvU32 reg;

    RequestedSemaNum = GetArbIdFromRmModuleId(modId);

    NvOsMutexLock(s_ArbSema.mutex[RequestedSemaNum]);
    NvOsIntrMutexLock(s_ArbSema.hIntrMutex);

    //Try to grab the lock
    ARBSEMA_REG_WRITE(s_ArbSema.pArbSemaVirtAddr, SMP_GET, 1 << RequestedSemaNum);

    //Enable arb sema interrupt
#if NV_IS_AVP
    reg = ARBGNT_REG_READ(s_ArbSema.pArbGntVirtAddr, COP_ENABLE);
    reg |= (1 << RequestedSemaNum);
    ARBGNT_REG_WRITE(s_ArbSema.pArbGntVirtAddr, COP_ENABLE, reg);
#else
    reg = ARBGNT_REG_READ(s_ArbSema.pArbGntVirtAddr, CPU_ENABLE);
    reg |= (1 << RequestedSemaNum);
    ARBGNT_REG_WRITE(s_ArbSema.pArbGntVirtAddr, CPU_ENABLE, reg);
#endif

    NvOsIntrMutexUnlock(s_ArbSema.hIntrMutex);
    NvOsSemaphoreWait(s_ArbSema.semaphore[RequestedSemaNum]);
}

void NvRmXpcModuleRelease(NvRmModuleID modId)
{
    NvU32 RequestedSemaNum;

    RequestedSemaNum = GetArbIdFromRmModuleId(modId);

    //Release the lock
    ARBSEMA_REG_WRITE(s_ArbSema.pArbSemaVirtAddr, SMP_PUT, 1 << RequestedSemaNum);

    NvOsMutexUnlock(s_ArbSema.mutex[RequestedSemaNum]);
}
