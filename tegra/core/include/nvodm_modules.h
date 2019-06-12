/*
 * Copyright (c) 2007-2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         I/O Module Definitions</b>
 *
 * @b Description: Defines all of the I/O module types (buses, I/Os, etc.)
 *                 that may exist on an application processor.
 */

#ifndef INCLUDED_NVODM_MODULES_H
#define INCLUDED_NVODM_MODULES_H

/**
 * @addtogroup nvodm_services
 * @{
 */

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines I/O module types.
 * Application processors provide a multitude of interfaces for connecting
 * to external peripheral devices. These take the forms of individual pins
 * (such as GPIOs), buses (such as USB), and power rails. Each interface
 * may have zero, one, or multiple instantiations on the application processor;
 * see the technical notes to determine the availability of interconnects for
 * your platform.
 */
typedef enum
{
    NvOdmIoModule_Ata,
    NvOdmIoModule_Crt,
    NvOdmIoModule_Csi,
    NvOdmIoModule_Dap,
    NvOdmIoModule_Display,
    NvOdmIoModule_Dsi,
    NvOdmIoModule_Gpio,
    NvOdmIoModule_Hdcp,
    NvOdmIoModule_Hdmi,
    NvOdmIoModule_Hsi,
    NvOdmIoModule_Hsmmc,
    NvOdmIoModule_I2s,
    NvOdmIoModule_I2c,
    NvOdmIoModule_I2c_Pmu,
    NvOdmIoModule_Kbd,
    NvOdmIoModule_Mio,
    NvOdmIoModule_Nand,
    NvOdmIoModule_Pwm,
    NvOdmIoModule_Sdio,
    NvOdmIoModule_Sflash,
    NvOdmIoModule_Slink,
    NvOdmIoModule_Spdif,
    NvOdmIoModule_Spi,
    NvOdmIoModule_Twc,
    NvOdmIoModule_Tvo,
    NvOdmIoModule_Uart,
    NvOdmIoModule_Usb,
    NvOdmIoModule_Vdd,
    NvOdmIoModule_VideoInput,
    NvOdmIoModule_Xio,
    NvOdmIoModule_ExternalClock,
    NvOdmIoModule_Ulpi,
    NvOdmIoModule_OneWire,
    NvOdmIoModule_SyncNor,
    NvOdmIoModule_PciExpress,
    NvOdmIoModule_Trace,
    NvOdmIoModule_Tsense,
    NvOdmIoModule_BacklightPwm,

    NvOdmIoModule_Num,
    NvOdmIoModule_Force32 = 0x7fffffffUL
} NvOdmIoModule;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif  // INCLUDED_NVODM_MODULES_H
