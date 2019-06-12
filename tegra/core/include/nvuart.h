/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * @brief <b>UART Driver Support on AVP</b>
 *
 * @b Description: This file defines macros and functions that support the
 * UART driver on the Audio/Visual Processor (AVP).
 */

/** @defgroup uart_avp_group UART Support on AVP
 *
 * Defines macros and functions for the AVP capability on the UART driver.
 *
 * @{
*/

#ifndef INCLUDED_NVUART_H
#define INCLUDED_NVUART_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvodm_query.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define PLLP_FIXED_FREQ_KHZ_13000            13000
#define PLLP_FIXED_FREQ_KHZ_216000          216000 /**< T30 bootrom PLLP frequency. */
#define PLLP_FIXED_FREQ_KHZ_408000          408000 /**< T30 bootloader PLLP frequency. */
#define PLLP_FIXED_FREQ_KHZ_432000          432000

/**
 * Intializes the UART controller.
 *
 * @param[in] pll The PLLP frequency, such as ::PLLP_FIXED_FREQ_KHZ_216000.
 *
 */
void NvAvpUartInit(NvU32 pll);

/**
 * Polls the UART receive port and returns the received character.
 *
 * @return The received character if there is one; otherwise, -1.
 *
 */
NvS32 NvAvpUartPoll(void);

/**
 * Writes the buffer to the UART port.
 *
 * @param[in] ptr A pointer to the buffer.
 *
 * @retval NvSuccess Write the the UART port was successful.
 */
void NvAvpUartWrite(const void *ptr);

/**
 * Outputs a message to the debugging console, if present.
 *
 * @param format A pointer to the format string.
 */
void NvOsAvpDebugPrintf(const char *format, ...);
NvOdmDebugConsole NvAvpGetDebugPort(void);


/** @} */
#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVUART_H
