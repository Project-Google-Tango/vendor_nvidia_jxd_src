/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#ifndef INCLUDED_AVP_UART_H
#define INCLUDED_AVP_UART_H

#include "nvcommon.h"
#include "nvassert.h"
#include "aruart.h"
#include "nverror.h"
#include "nvrm_drf.h"
#include "arapb_misc.h"
#include "arclk_rst.h"
#include "nvboot_bit.h"
#include "nvuart.h"
#include "aos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// 8 bit access to UART.
#define NV_UART_READ(pBase, Reg, value)                                       \
    do                                                                        \
    {                                                                         \
        value = NV_READ8(((NvUPtr)pBase + UART_##Reg##_0));                   \
    } while (0)

// 8 bit access to UART.
#define NV_UART_WRITE(pBase, Reg, value)                                      \
    do                                                                        \
    {                                                                         \
        NV_WRITE08(((NvUPtr)pBase + UART_##Reg##_0), value);                  \
    } while (0)

#define NV_MISC_READ(Reg, value)                                              \
    do                                                                        \
    {                                                                         \
        value = NV_READ32(NV_ADDRESS_MAP_APB_MISC_BASE + APB_MISC_##Reg##_0); \
    } while (0)

#define NV_MISC_WRITE(Reg, value)                                             \
    do                                                                        \
    {                                                                         \
        NV_WRITE32((NV_ADDRESS_MAP_APB_MISC_BASE + APB_MISC_##Reg##_0),       \
                   value);                                                    \
    } while (0)

#define NV_CLK_RST_READ(reg, value)                                           \
    do                                                                        \
    {                                                                         \
        value = NV_READ32(NV_ADDRESS_MAP_PPSB_CLK_RST_BASE                    \
                          + CLK_RST_CONTROLLER_##reg##_0);                    \
    } while (0)

#define NV_CLK_RST_WRITE(reg, value)                                          \
    do                                                                        \
    {                                                                         \
        NV_WRITE32((NV_ADDRESS_MAP_PPSB_CLK_RST_BASE                          \
                    + CLK_RST_CONTROLLER_##reg##_0), value);                  \
    } while (0)

#define CLOCK(port, clock)                                                    \
    do                                                                        \
    {                                                                         \
        NvU32 Reg;                                                            \
        Reg = NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_UART##port,           \
                         UART##port##_CLK_SRC, PLLP_OUT0);                    \
        NV_CLK_RST_WRITE(CLK_SOURCE_UART##port, Reg);                         \
        NV_CLK_RST_READ(CLK_OUT_ENB_##clock, Reg);                            \
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_##clock,     \
                                 CLK_ENB_UART##port, ENABLE, Reg);            \
        NV_CLK_RST_WRITE(CLK_OUT_ENB_##clock, Reg);                           \
        NV_CLK_RST_READ(RST_DEVICES_##clock, Reg);                            \
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_##clock,     \
                                 SWR_UART##port##_RST, ENABLE, Reg);          \
        NV_CLK_RST_WRITE(RST_DEVICES_##clock, Reg);                           \
        Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_##clock,     \
                                 SWR_UART##port##_RST, DISABLE, Reg);         \
        NV_CLK_RST_WRITE(RST_DEVICES_##clock, Reg);                           \
    } while (0)

int NvSimpleVsnprintf(char* b, size_t size, const char* f, va_list ap);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_AVP_UART_H
