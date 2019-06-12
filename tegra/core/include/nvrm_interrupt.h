/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_interrupt_H
#define INCLUDED_nvrm_interrupt_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_module.h"
#include "nvrm_init.h"

#include "nvos.h"

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *     Resource Manager %Interrupt API</b>
 *
 * @b Description: Declares the interrupt API for use by NvDDK modules.
 */

/**
 * @defgroup nvrm_interrupt RM Interrupt Management Services
 *
 * This file declares the interrupt API for use by NvDDK modules.
 *
 * <h3>IRQ Numbers</h3>
 *
 * In most cases, we are using the CPU's legacy interrupt support, rather than
 * the new MPCore interrupt controller. This means that we only have one ISR
 * shared between all of the devices in our chip. To determine which device is
 * interrupting us, we have to read some registers. We assign each interrupt
 * source an "IRQ number". IRQ numbers are OS-independent and HW-dependent (a
 * given device may have a different IRQ number from chip to chip).
 *
 * It is arbitrary how far we decode interrupts as part of determining the IRQ
 * number. Normally we might assign one IRQ number to each interrupt line that
 * feeds into the main interrupt controller (typically one per device in the
 * chip), but we can decode further if we want. For example, there are several
 * GPIO controllers, each of which controls 32 GPIO lines. The GPIO controller
 * interrupt line is constructed by OR'ing together the interrupt lines for each
 * of the 32 GPIO pins. If we want, we can assign each GPIO controller 32
 * separate IRQ numbers, one per GPIO line; this simply means we have to sub-
 * decode the interrupts a little further inside the ISR.
 *
 * The main advantage of doing this sub-decoding is that only a single driver is
 * allowed to hook each interrupt source -- if multiple drivers both want to
 * register interrupt handlers for the same interrupt source, the drivers will
 * fight with one another trying to handle the same interrupt, so this is an
 * error. At the same time, it is entirely plausible that out of a group of 32
 * GPIO pins, multiple different drivers care about different groups of those
 * pins. In the absence of sub-decoding, we would have to implement a "GPIO
 * driver" whose sole purpose was to allow those other drivers to register for
 * GPIO notifications, and then use driver-to-driver signaling to indicate when
 * a pin has transitioned state. This is an extra level of overhead compared
 * to if drivers are allowed to directly hook the interrupts for the pins they
 * care about.
 *
 * Because IRQ numbers change from chip to chip, you must ask the RM for the IRQ
 * number of the device when you want to hook its interrupt. This can be
 * accomplished using the NvRmGetIrqForLogicalInterrupt() API. You pass it an
 * [NvRmModuleID, Index] pair telling it what device you are interested in, and
 * which sub-interrupt within that device. Often \a Index is just zero (many
 * devices only have one IRQ number). For GPIO it might by the pin number
 * within the GPIO controller. For UART, you might (entirely hypothetically --
 * there is no requirement that you do this) have Index=0 for the receive
 * interrupt and Index=1 for the send interrupt.
 *
 * <h3>Hooking an Interrupt</h3>
 *
 * Once you have the IRQ number(s), you can hook the interrupt(s) by calling
 * NvRmInterruptRegister(). At driver shutdown, you can unhook the interrupt(s)
 * by calling NvRmInterruptUnregister().
 *
 * \c NvRmInterruptRegister takes a list of IRQs and a list of callback
 * functionsto be called when the corresponding interrupt fires. The callback
 * functions will be passed an extra "void *context" parameter, typically a
 * pointer to your private driver structure that keeps track of the state of
 * your device. For example, the NAND driver might pass the \a NvDdkNandHandle
 * as the context param.
 *
 * Drivers that care about more than one IRQ should call \c
 * NvRmInterruptRegister only once. Calling it twice (each time with a
 * single IRQ number) may consume more system resources than calling it once
 * with a list of 2 IRQ numbers and 2 callbacks.
 *
 * <h3>Rules for Interrupt Handlers</h3>
 *
 * We assume that all interrupt handlers (i.e., the callbacks passed to
 * ::NvRmInterruptRegister) are "fast"; that is, any complex processing that
 * cannot complete in a tightly bounded amount of time -- such as polling
 * registers to wait for the HW to complete some processing -- is not done in
 * the ISR proper. Instead, the ISR would signal a semaphore, clear the
 * interrupt, and pass off the rest of the work to another thread.
 *
 * To be more precise about this, we expect all interrupt handlers to follow
 * these rules:
 * - They may only call a subset of NvOs functions. The exact subset is
 *   documented in nvos.h.
 * - No floating-point. We don't want to have to save and restore the
 *   floating point registers on an interrupt.
 * - They should use as little stack space as possible. For example, they
 *   certainly should not use any recursive algorithms. (If they need
 *   to look up a node in a red-black tree, they must use an iterative version
 *   of the tree search rather than recursion.) Straw man: 256 B maximum?
 * - Any control flow structure that involves looping (like a \c for or \c
 *   while statement) must be guaranteed to terminate within a clearly
 *   understood time limit. We don't have a strict upper bound, but if it takes
 *   milliseconds, it's out of the question.
 * - The callback function \em must clear the cause of the interrupt. Upon
 *   returning from the callback the interrupt will be automatically re-enabled.
 *   If the cause is not cleared the system will be stuck in an infinite loop
 *   taking interrupts.
 *
 * @ingroup nvddk_rm
 * @{
 */

/**
 * A logical interrupt is a tuple that includes the class of interrupts
 * (i.e., a module), an instance of that module, and the specific interrupt
 * within that instance (an index). This is an abstraction for the
 * actual interrupt bits implemented on the SOC.
 */

typedef struct NvRmLogicalIntrRec
{

    /**
     * Interrupt index within the current instance of specified module.
     * This identifies a specific interrupt. This is an enumerated index
     * and not a bit-mask.
     */
        NvU8 Index;

    /**
     * The SOC hardware controller class identifier.
     */
        NvRmModuleID ModuleID;
} NvRmLogicalIntr;

/**
 * Translates a given logical interrupt to its corresponding IRQ number.
 *
 * @param hRmDevice The RM device handle.
 * @param ModuleID The module of interest.
 * @param Index Zero-based interrupt index within the module.
 *
 * @return The IRQ number.
 */

 NvU32 NvRmGetIrqForLogicalInterrupt(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleID ModuleID,
    NvU32 Index );

/**
 * Retrieves the number of IRQs associated with a particular module instance.
 *
 * @param hRmDevice The RM device handle.
 * @param ModuleID The module of interest.
 *
 * @return The number of IRQs.
 */

 NvU32 NvRmGetIrqCountForLogicalInterrupt(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleID ModuleID );

/*
 * Registers the interrupt with the given interrupt handler.
 *
 * Assert encountered in debug mode if IRQ number is not valid.
 *
 * @see NvRmInterruptEnable()
 *
 * @param hRmDevice The RM device handle.
 * @param IrqListSize Size of the \a IrqList passed in for registering
 *    the IRQ handlers for each IRQ number.
 * @param pIrqList Array of IRQ numbers for which interupt handlers are
 *    to be registerd.
 * @param pIrqHandlerList Array intrupt routine to be called when interrupt
 * occurs.
 * @param context A pointer to the registrer's context handle.
 * @param handle Handle to the registered interrupts. This handle is used for
 * unregistering the interrupt.
 * @param InterruptEnable If NV_TRUE, immediately enable interrupt. Otherwise
 *  enable interrupt only after calling NvRmInterruptEnable().
 *
 * @retval NvError_IrqRegistrationFailed If interupt is already registred.
 * @retval NvSuccess If registration is successfull.
 */
NvError
NvRmInterruptRegister(
    NvRmDeviceHandle hRmDevice,
    NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable);

/**
 * Un-registers the interrupt handler from the associated interrupt handle,
 * which is returned by the NvRmInterruptRegister() API.
 *
 * @param hRmDevice The RM device handle.
 * @param handle Handle returned when the interrupt is registered.
 */
void
NvRmInterruptUnregister(
    NvRmDeviceHandle hRmDevice,
    NvOsInterruptHandle handle);

/**
 * Enables the interrupt handler from the associated interrupt handle, which
 * is returned by the NvRmInterruptRegister() API.
 *
 * @param hRmDevice The RM device handle.
 * @param handle Handle returned when the interrupt is registered.
 *
 * @retval NvError_BadParameter If handle is not valid.
 * @retval NvError_InsufficientMemory If interupt enable failed.
 * @retval NvSuccess If registration is successfull.
 */
NvError
NvRmInterruptEnable(
    NvRmDeviceHandle hRmDevice,
    NvOsInterruptHandle handle);

/**
 *  Called by the interrupt callaback to re-enable the interrupt.
 */
void
NvRmInterruptDone( NvOsInterruptHandle handle );


#if defined(__cplusplus)
}
#endif

#endif
