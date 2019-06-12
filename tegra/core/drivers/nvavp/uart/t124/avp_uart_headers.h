/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#ifndef INCLUDED_AVP_UART_T12X_H
#define INCLUDED_AVP_UART_T12X_H
#include "nvboot_hardware_access_int.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NV_ADDRESS_MAP_PPSB_CLK_RST_BASE     0x60006000
#define NV_ADDRESS_MAP_APB_MISC_BASE         0x70000000
#define DISABLE_UARTE // uarte is not available for t124


/**
 * Gets the configuration and does the pinmux accordingly for the given
 * uart port
 *
 * @param port The index to represent whether UARTA/UARTB/UARTC etc.
 */
void NvAvpSetUartConfig(NvU32 port);

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_AVP_UART_T12X_H
