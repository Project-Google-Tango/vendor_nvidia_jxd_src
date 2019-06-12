/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVINTRHANDLER_H
#define INCLUDED_NVINTRHANDLER_H
#include "nverror.h"
#include "nvos.h"
#include "nvintr.h"

/** Interrupt handler function.
 */
typedef struct NvOsInterruptThreadArgsRec
{
    void *context;
    NvOsInterruptHandler handler;
    NvOsSemaphoreHandle sem;
    NvOsInterruptHandle hInt;
    volatile NvBool bShutdown;
} NvOsInterruptThreadArgs;

typedef struct NvOsIntListRec
{
    NvOsThreadHandle        thread;
    NvOsInterruptThreadArgs arg;
} NvOsIntList;

typedef struct NvOsInterruptRec
{
    NvU32  irq;
    NvU32  nIrqs;
    NvOsIntList Irqs[1];
} NvOsInterrupt;

typedef struct NvIrqListRec
{
    NvU32 irq;
    NvOsSemaphoreHandle semaHandle;
    NvOsInterruptThreadArgs arg;
    struct NvIrqListRec *Next;
}NvIrqList;

// structure to hold the sub interrupt details.
typedef struct NvIntrSubIrqRec
{
    // Lower irq index
    NvU16 LowerIrqIndex;

    // Upper irq index
    NvU16 UpperIrqIndex;

    //Irq instance record
    NvU16 Instances;
} NvIntrSubIrq;

typedef struct NvRmDeviceRec *NvRmIntrHandle;

typedef struct  NvIntrRec *pNvIntr;

// Interupt functions types
typedef void  (*PFNvIntrSetInterrupt)( NvRmIntrHandle hRmDeviceIntr,\
                                NvU32 irq, NvBool val );

// Get the main irq which is enbled by reding the interrupt controller register.
typedef NvU32 (*PFNvIntrDecodeInterrupt)(NvRmIntrHandle hRmDeviceIntr);
typedef NvU32 (*PFNvIntrTimerIrq)(void);

/* The function pointers to handle the sub interrupt from single controller
 * based on the pin (in case of Gpio) or channels (in case of dma) etc.
 */

// Function pointer to get the lower and upper irq index supported by the
// controller.
typedef void (*PFNvIntrSubIntrIrqIndex)(NvRmIntrHandle hRmDeviceIntr,
                                NvU32 *pLowerIndex, NvU32 *pUpperIndex);

typedef NvBool (*PFNvIntrSubIntIrq)(NvRmIntrHandle hRmDeviceIntr,\
                                            pNvIntr,NvU32 *irq );

// Enable/disable the sub interrupt bit of the controller and return the
// main Irq bit number if it also need to be enable/disable.
typedef void (*PFNvIntrSubIntSetLogicalIrq)(NvRmIntrHandle hRmDeviceIntr,\
                                            pNvIntr, NvU32 irq,\
                                            NvU32 *pIrq, NvBool val);

// Tells whether the given irq is supported or not.
typedef NvBool (*PFNvIntrIsIrqSupported)(NvRmIntrHandle hRmDeviceIntr, NvU32 Irq);

// Function to tell whether the given Irq is belong to the supported
// interrupt or not.

// Interrupt function table
typedef struct  NvIntrRec
{
    NvRmIntrHandle                          hRmIntrDevice;
    PFNvIntrSetInterrupt                    NvSetInterrupt;
    PFNvIntrDecodeInterrupt                 NvDecodeInterrupt;
    PFNvIntrTimerIrq                        NvTimerIrq;
    PFNvIntrIsIrqSupported                  NvIsMainIntSupported;
    NvU32                                   NvMaxMainIntIrq;

    PFNvIntrSubIntrIrqIndex                 NvGpioGetLowHighIrqIndex;
    PFNvIntrSubIntIrq                       NvGpioIrq;
    PFNvIntrSubIntSetLogicalIrq             NvGpioSetLogicalIrq;
    PFNvIntrIsIrqSupported                  NvIsGpioSubIntSupported;

    PFNvIntrSubIntrIrqIndex                 NvApbDmaGetLowHighIrqIndex;
} NvIntr;

// Number of IRQs per interrupt controller
#define NVRM_IRQS_PER_INTRCTLR     32

#define NVRM_IRQ_INVALID        0xFFFF

void NvIntrGetInterfaceGic(NvIntr *pNvIntr, NvU32 ChipId);
void NvIntrGetGpioInterface(NvIntr *pNvIntr, NvU32 chipId);
void NvIntrGetMainIntInterface(NvIntr *pNvIntr, NvU32 ChipId);

#endif//INCLUDED_NVINTRHANDLER_H
