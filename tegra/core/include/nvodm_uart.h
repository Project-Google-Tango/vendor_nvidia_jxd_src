/*
 * Copyright (c) 2006-2008 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         UART Adaptation Interface</b>
 *
 * @b Description: Defines the ODM adaptation interface for UART devices.
 * 
 */

#ifndef INCLUDED_NVODM_UART_H
#define INCLUDED_NVODM_UART_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvodm_services.h"
#include "nverror.h"

/**
 * @defgroup nvodm_uart UART Adaptation Interface
 * 
 * This is the UART ODM adaptation interface. 
 * 
 * @ingroup nvodm_adaptation
 * @{
 */

/**
 * Defines an opaque handle that exists for each UART device in the
 * system, each of which is defined by the customer implementation.
 */
typedef struct NvOdmUartRec *NvOdmUartHandle;

/**
 * Gets a handle to the UART device.
 *
 * @param Instance         [IN] UART instance number
 *
 * @return A handle to the UART device.
 */
NvOdmUartHandle NvOdmUartOpen(NvU32 Instance);

/**
 * Closes the UART handle. 
 *
 * @param hOdmUart The UART handle to be closed.
 */
void NvOdmUartClose(NvOdmUartHandle hOdmUart);

/**
 * Call this API whenever the UART device goes into suspend mode.
 *
 * @param hOdmUart The UART handle.
  */
NvBool NvOdmUartSuspend(NvOdmUartHandle hOdmUart);

/**
 * Call this API whenever the UART device resumes from the suspend mode.
 *
 * @param hOdmUart The UART handle.
  */
NvBool NvOdmUartResume(NvOdmUartHandle hOdmUart);


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_uart_H
