/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVINTR_H
#define INCLUDED_NVINTR_H

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvcommon.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

/**
 * Registers the IRQ number associated with the handler.
 *
 * @note This function is intended to @b only be called
 *       from NvOsInterruptRegister().
 *
 * @param pIrqHandlerList A pointer to an array of interrupt routines to be
 *      called when an interrupt occurs.
 * @param context A pointer to the register's context handle.
 * @param IrqListSize Size of the \a IrqList passed in for registering the IRQ
 *      handlers for each IRQ number.
 * @param Irq Array of IRQ numbers for which interupt handlers are to be
 *     registerd.
 * @param handle A pointer to the interrupt handle.
 * @param InterruptEnable If true, immediately enable interrupt.
 *
 * @retval NvError_BadParameter If the handle is not valid.
 * @retval NvError_InsufficientMemory If interrupt enable failed.
 * @retval NvSuccess If interrupt enable is successful.
 */
NvError NvIntrRegister( const NvOsInterruptHandler* pIrqHandlerList ,
    void* context,
    NvU32 IrqListSize,
    const NvU32* Irq,
    NvOsInterruptHandle* handle,
    NvBool IntEnable);

/**
 * Unregisters the IRQ number associated with the handler.
 *
 * @note This function is intended to @b only be called
 *       from NvOsInterruptUnRegister().
 *
 * @param handle A pointer to the interrupt handle.
 *
 * @retval NvError_BadParameter If the Irq associated is not valid.
 * @retval NvSuccess If Unregister is successful.
 */
NvError
NvIntrUnRegister(NvOsInterruptHandle hndl);

/**
 * Sets the interrupt for the IRQ number.
 *
 * @note This function is intended to @b only be called
 *       from NvOsInterruptRegister(), NvOsInterruptEnable() and 
 *      NvOsInterruptDone().
 *
 * @param handle Interrupt handle returned when a successfull call is made to
 *     \c NvOsInterruptRegister.
 *
 * @retval NvError_BadParameter If the handle is not valid.
 * @retval NvError_InsufficientMemory If interrupt enable failed.
 * @retval NvSuccess If interrupt enable is successful.
 */
NvError
NvIntrSet(NvOsInterruptHandle hndl, NvBool val);

/**
 *  Interrupt handler 
 *
 * @note This function is intended to be called
 *       from any client for service of interrupts.
 *
 * @param device handle
 *
 * @retval NvError_BadParameter If the handle is not valid.
 * @retval NvError_NotImplemented If not initialised.
 * @retval NvSuccess If interrupt enable is successful.
 */
NvError
NvIntrHandler( void* arg );

/**
 *  Initializes the interrupt handler and sets up based on the chip Id
 *
 * @note This function is intended to be called during system init
 *      
 *
 * @param chip Id 
 * @param Timer handler 
 *
 * @param main Interrupt handler for registering with interrupt decoder
 *
 * @retval NvError_BadParameter If the chip Id/handle is not valid.
 * @retval NvError_InsufficientMemory If allocation for list failed.
 * @retval NvSuccess If interrupt function and setup is successful.
 */
NvError
NvIntrInit(
    NvU32 chip,
    NvOsInterruptHandler* mHndlr,
    NvOsInterruptHandler pHndlr);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVINTR_H

