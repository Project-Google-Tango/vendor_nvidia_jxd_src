/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_avp_shrd_interrupt_H
#define INCLUDED_nvrm_avp_shrd_interrupt_H

#include "nvos.h"
#include "nvrm_init.h"
#include "nvrm_module.h"
#include "nvrm_interrupt.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* Max number of clients with shared interrupt handler */
enum {MAX_SHRDINT_CLIENTS = 32};

/* Now AP15 support only VDE interrupts 6 */
enum {MAX_SHRDINT_INTERRUPTS = 6};
    /* VDE Sync Token Interrupt */
enum {AP15_SYNC_TOKEN_INTERRUPT_INDEX = 0};
    /* VDE BSE-V Interrupt */
enum {AP15_BSE_V_INTERRUPT_INDEX = 1};
    /* VDE BSE-A Interrupt */
enum {AP15_BSE_A_INTERRUPT_INDEX = 2};
    /* VDE SXE Interrupt */
enum {AP15_SXE_INTERRUPT_INDEX = 3};
    /* VDE UCQ Error Interrupt */
enum {AP15_UCQ_INTERRUPT_INDEX = 4};
    /* VDE Interrupt */
enum {AP15_VDE_INTERRUPT_INDEX = 5};

/* Now AP20 support only VDE interrupts 5 */
enum {AP20_MAX_SHRDINT_INTERRUPTS = 5};
    /* VDE Sync Token Interrupt */
enum {AP20_SYNC_TOKEN_INTERRUPT_INDEX = 0};
    /* VDE BSE-V Interrupt */
enum {AP20_BSE_V_INTERRUPT_INDEX = 1};
    /* VDE SXE Interrupt */
enum {AP20_SXE_INTERRUPT_INDEX = 2};
    /* VDE UCQ Error Interrupt */
enum {AP20_UCQ_INTERRUPT_INDEX = 3};
    /* VDE Interrupt */
enum {AP20_VDE_INTERRUPT_INDEX = 4};

enum
{
    NvRmArbSema_Vde = 0,
    NvRmArbSema_Bsea,
    //This should be last
    NvRmArbSema_Num,
};

/* Shared interrupt private init , init done during RM init on AVP */
NvError NvRmAvpShrdInterruptPrvInit(NvRmDeviceHandle hRmDevice);

/* Shared interrupt private de-init , de-init done during RM close on AVP */
void NvRmAvpShrdInterruptPrvDeinit(NvRmDeviceHandle hRmDevice);

/* Get logical interrupt for a module*/
NvU32 NvRmAvpShrdInterruptGetIrqForLogicalInterrupt(NvRmDeviceHandle hRmDevice,
                                                    NvRmModuleID ModuleID,
                                                    NvU32 Index);
/* Register for shared interrpt */
NvError NvRmAvpShrdInterruptRegister(NvRmDeviceHandle hRmDevice,
                                     NvU32 IrqListSize,
                                     const NvU32 *pIrqList,
                                     const NvOsInterruptHandler *pIrqHandlerList,
                                     void *pContext,
                                     NvOsInterruptHandle *handle,
                                     NvU32 *ClientIndex);
/* Un-register a shared interrpt */
void NvRmAvpShrdInterruptUnregister(NvRmDeviceHandle hRmDevice,
                                    NvOsInterruptHandle handle,
                                    NvU32 ClientIndex);
/* Get exclisive access to a hardware(VDE) block */
NvError NvRmAvpShrdInterruptAquireHwBlock(NvRmModuleID ModuleID, NvU32 ClientId);

/* Release exclisive access to a hardware(VDE) block */
NvError NvRmAvpShrdInterruptReleaseHwBlock(NvRmModuleID ModuleID, NvU32 ClientId);

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_nvrm_avp_shrd_interrupt_H
