/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_arm_cp.h"
#include "nvrm_module.h"
#include "nvrm_interrupt.h"
#include "nvintrhandler.h"
#include "nvrm_init.h"
#include "nvrm_minikernel.h"
#include "nvrm_hardware_access.h"
#include "nvrm_chipid.h"
#include <stdlib.h>
#include <string.h>

static NvIntr* g_pNvIntr = NULL;
NvRmIntrHandle hRmDeviceIntr = NULL;

static NvError NvIntrGetList(NvIrqList **pList)
{
    static NvIrqList *gList = NULL;

    if(!gList)
    {
        gList = NvOsAlloc(sizeof(NvIrqList));
        NvOsMemset(gList , 0 , sizeof(NvIrqList));
        if(!gList)
            return NvError_InsufficientMemory;
    }
    *pList = gList;
    return NvSuccess;
}

static NvError InitInterrupInterface(NvU32 ChipId)
{
    if (!hRmDeviceIntr)
        NvRmBasicInit(&hRmDeviceIntr);

    g_pNvIntr = NvOsAlloc(sizeof(NvIntr));
    if(!g_pNvIntr)
        return NvError_InsufficientMemory;
    NvOsMemset(g_pNvIntr, 0, sizeof(NvIntr));

    g_pNvIntr->hRmIntrDevice = hRmDeviceIntr;

    NvIntrGetMainIntInterface(g_pNvIntr, ChipId);
    NvIntrGetGpioInterface(g_pNvIntr, ChipId);
    return NvSuccess;
}

static void NvIntr_thread_function(void *v)
{
    NvOsInterruptThreadArgs *args = (NvOsInterruptThreadArgs *)v;
    NV_ASSERT(args);
    while(!args->bShutdown)
    {
        NvOsSemaphoreWait(args->sem);
        if(args->bShutdown)
        {
            break;
        }
        args->handler(args->context);
    }
}

static void NvIntrCleanup(NvOsInterruptHandle hInt)
{
    NvU32 i;

    if (!hInt)
        return;

    for (i = 0; i < hInt->nIrqs; i++)
    {
        if (hInt->Irqs[i].thread)
        {
            hInt->Irqs[i].arg.bShutdown = NV_TRUE;
            if (hInt->Irqs[i].arg.sem)
                NvOsSemaphoreSignal(hInt->Irqs[i].arg.sem);
            NvOsThreadJoin(hInt->Irqs[i].thread);
        }

        if (hInt->Irqs[i].arg.sem)
            NvOsSemaphoreDestroy(hInt->Irqs[i].arg.sem);
    }
}

static NvBool NvIntrHandleIsr(NvU32 irq)
{
    NvIrqList *t;
    NvU32 tirq = irq;

    if(!g_pNvIntr)
        return NV_FALSE;

    if ((g_pNvIntr->NvGpioIrq)(hRmDeviceIntr, g_pNvIntr, &irq))
        (g_pNvIntr->NvSetInterrupt)(hRmDeviceIntr, tirq, NV_TRUE);

    if(NvIntrGetList(&t) != NvSuccess)
        return NV_FALSE;

    if(!t)
    {
        return NV_FALSE;// still not initialized..
    }
    else
    {
        while((t->irq != irq) && (t->Next))
            t = t->Next;

        if(t->irq == irq)
        {
            // Sub interrupt handler then signal the semaphore else
            // call the handler in isr context.
            if(t->semaHandle)
                NvOsSemaphoreSignal(t->semaHandle);
            else
                t->arg.handler(t->arg.context);
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}

NvError NvIntrInit(
        NvU32 ChipId,
        NvOsInterruptHandler *fptr,
        NvOsInterruptHandler pHndlr)
{
    NvIrqList *tmr;
    NvError Err = NvSuccess;
    if(NvIntrGetList(&tmr) != NvSuccess)
        return NvError_InsufficientMemory;

    if (!g_pNvIntr)
    {
        Err = InitInterrupInterface(ChipId);
        if (Err)
            return Err;
    }

    tmr->irq = (g_pNvIntr->NvTimerIrq)();
    tmr->arg.handler = pHndlr;

    *fptr = (NvOsInterruptHandler)(NvIntrHandler);
    return NvSuccess;
}

NvError NvIntrSet(NvOsInterruptHandle hOsInterrupt, NvBool IsEnable)
{
    NvU32 mainIrq = 0;

    if (hOsInterrupt == NULL)
    {
        NV_ASSERT(!"Interrupt handle should not be null\n");
        return NvError_BadParameter;
    }

    mainIrq = hOsInterrupt->irq;

    if(!g_pNvIntr)
        return NvError_NotImplemented;

    if(!hRmDeviceIntr)
        return NvError_BadParameter;

    if (mainIrq > g_pNvIntr->NvMaxMainIntIrq)
    {
        // Get the main irq from logical irq if it belongs to gpio logical irq.
        (g_pNvIntr->NvGpioSetLogicalIrq)(hRmDeviceIntr, g_pNvIntr,
                                            hOsInterrupt->irq, &mainIrq, IsEnable);

    }
    (g_pNvIntr->NvSetInterrupt)(hRmDeviceIntr, mainIrq, IsEnable);
    return NvSuccess;
}

NvError NvIntrHandler(void *arg)
{
    if(g_pNvIntr)
    {
        if(NvIntrHandleIsr((g_pNvIntr->NvDecodeInterrupt)(hRmDeviceIntr)))
            return NvSuccess;
        else
            return NvError_BadParameter;
    }
    return NvError_NotImplemented;
}

NvError NvIntrRegister(const NvOsInterruptHandler* pIrqList, void* context,
    NvU32 IrqListSize, const NvU32 *Irq, NvOsInterruptHandle* handle,
    NvBool InterruptEnable)
{
    NvU32 i = 0;
    NvU32 size =0;
    NvIrqList *nxt, *tmp;
    NvOsSemaphoreHandle h = NULL;
    NvOsInterrupt       *hInt      = NULL;
    NvError  e = NvSuccess;
    NvRmChipId *id;
    NvU16 ChipId;
    NvBool IsSubInterrupt;
    NvIrqList *pExistIntList = NULL;
    NvBool IsIrqSupported;

    if(NvIntrGetList(&tmp) != NvSuccess)
        return NvError_InsufficientMemory;

    size = sizeof(NvOsInterrupt) + sizeof(NvOsIntList)*(IrqListSize-1);
    hInt = NvOsAlloc(size);

    if(!hInt)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NvOsMemset(hInt, 0, size);
    hInt->nIrqs = IrqListSize;

    /* Set the handle to make sure, if any interrupt gets triggered
     * before returning from this function, interrupt service routine
     * always has valid handle.
     */
    *handle = hInt;

    if(!g_pNvIntr)
    {
        if (!hRmDeviceIntr)
            NvRmBasicInit(&hRmDeviceIntr);

        id = NvRmPrivGetChipId(hRmDeviceIntr);
        ChipId = id->Id;
        e = InitInterrupInterface(ChipId);
        if (e)
            goto fail;
    }

    for(i = 0; i < IrqListSize; i++)
    {
        hInt->irq = Irq[i];
        hInt->Irqs[i].arg.context   = context;
        hInt->Irqs[i].arg.handler   = pIrqList[i];
        hInt->Irqs[i].arg.hInt      = hInt;
        hInt->Irqs[i].arg.bShutdown = NV_FALSE;
        hInt->Irqs[i].arg.sem = NULL;
        hInt->Irqs[i].thread = NULL;
        IsSubInterrupt = NV_FALSE;

        // Check if this interrupt is already registerd.
        NvIntrGetList(&pExistIntList);
        while (pExistIntList->Next)
        {
            if (pExistIntList->irq == hInt->irq)
            {
                e = NvError_AlreadyAllocated;
                goto fail;
            }
            pExistIntList = pExistIntList->Next;
        }

        // Check whether this irq is supported on main irq or not
        IsIrqSupported = g_pNvIntr->NvIsMainIntSupported(hRmDeviceIntr,
                                                                    hInt->irq);
        if (!IsIrqSupported)
        {
            // Check whether this irq belongs to the gpio sub irq or not.
            IsIrqSupported = g_pNvIntr->NvIsGpioSubIntSupported(hRmDeviceIntr,
                                                                    hInt->irq);
            IsSubInterrupt = NV_TRUE;
        }

        // If it is not supported then return the error
        if (!IsIrqSupported)
        {
            e = NvError_NotSupported;
            goto fail;
        }

        if(IsSubInterrupt)
        {
            NV_CHECK_ERROR_CLEANUP(NvOsSemaphoreCreate(&h, 0));
            hInt->Irqs[i].arg.sem       = h;

            NV_CHECK_ERROR_CLEANUP(
                NvOsInterruptPriorityThreadCreate(NvIntr_thread_function,
                    &hInt->Irqs[i].arg, &hInt->Irqs[i].thread)
           );
        }

        while(tmp->Next)
            tmp = tmp->Next;

        nxt = NvOsAlloc(sizeof(NvIrqList));

        if(!nxt)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemset(nxt, 0, sizeof(NvIrqList));

        nxt->irq = Irq[i];
        if(hInt->Irqs[i].arg.sem)
        {
            nxt->semaHandle = hInt->Irqs[i].arg.sem;
        }
        else
        {
            nxt->arg = hInt->Irqs[i].arg;
        }
        tmp->Next = nxt;

        if (InterruptEnable)
            e = NvIntrSet(hInt, InterruptEnable);

        if (e != NvSuccess)
            goto fail;
    }

fail:
    if (e != NvSuccess)
    {
         *handle = NULL;
        NvIntrCleanup(hInt);
        NvOsFree(hInt);
    }
    return e;
}

NvError NvIntrUnRegister(NvOsInterruptHandle hndl)
{
    NvIrqList *tmp,*next,*pre;

    if((NvIntrGetList(&tmp) != NvSuccess) || !hndl)
        return NvError_BadParameter;

    if(!tmp->Next)
    {
        return NvError_BadParameter;
    }
    else
    {
        pre = tmp;
        tmp = tmp->Next;
        while((tmp->irq != hndl->irq) && tmp->Next)
        {
            pre = tmp;
            tmp = tmp->Next;
        }
        if(tmp->irq == hndl->irq)
        {
            next = tmp->Next;
            NvOsFree(tmp);
            pre->Next = next;
        }
    }

    NvIntrCleanup(hndl);
    NvOsFree(hndl);
    return NvSuccess;
}
