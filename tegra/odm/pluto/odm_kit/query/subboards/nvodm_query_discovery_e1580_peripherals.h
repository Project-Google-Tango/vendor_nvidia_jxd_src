/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 *
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database Peripheral
 *                 entries for the peripherals on E1580 module.
 */

// DDR VDD (NV reserved)
{
    NV_VDD_DDR_ODM_ID,
    s_VddDdr3L1V35Addresses,
    NV_ARRAY_SIZE(s_VddDdr3L1V35Addresses),
    NvOdmPeripheralClass_Other
},

// RTC (NV reserved)
{
    NV_VDD_RTC_ODM_ID,
    s_VddRtcAddresses,
    NV_ARRAY_SIZE(s_VddRtcAddresses),
    NvOdmPeripheralClass_Other
},

// CORE (NV reserved)
{
    NV_VDD_CORE_ODM_ID,
    s_VddCoreAddresses,
    NV_ARRAY_SIZE(s_VddCoreAddresses),
    NvOdmPeripheralClass_Other
},

// CPU (NV reserved)
{
    NV_VDD_CPU_ODM_ID,
    s_VddCpuAddresses,
    NV_ARRAY_SIZE(s_VddCpuAddresses),
    NvOdmPeripheralClass_Other
},

// System IO VDD (NV reserved)
{
    NV_VDD_SYS_ODM_ID,
    s_VddIoSysAddresses,
    NV_ARRAY_SIZE(s_VddIoSysAddresses),
    NvOdmPeripheralClass_Other
},

// USB VDD (NV reserved)
{
    NV_VDD_USB_ODM_ID,
    s_AVddUsbAddresses,
    NV_ARRAY_SIZE(s_AVddUsbAddresses),
    NvOdmPeripheralClass_Other
},

// Power for HDMI Hotplug
{
    NV_VDD_HDMI_INT_ID,
    s_VddIoLcdAddresses,
    NV_ARRAY_SIZE(s_VddIoLcdAddresses),
    NvOdmPeripheralClass_Other,
},

// LCD VDD (NV reserved)
{
    NV_VDD_LCD_ODM_ID,
    s_VcoreLcdAddresses,
    NV_ARRAY_SIZE(s_VcoreLcdAddresses),
    NvOdmPeripheralClass_Other
},

// DDR_RX (NV reserved)
{
    NV_VDD_DDR_RX_ODM_ID,
    s_VddDdrRxAddresses,
    NV_ARRAY_SIZE(s_VddDdrRxAddresses),
    NvOdmPeripheralClass_Other
},

// VDD_EMMC_CORE (NV reserved)
{
    NV_VDD_NAND_ODM_ID,
    s_VddEmmcCoreAddresses,
    NV_ARRAY_SIZE(s_VddEmmcCoreAddresses),
    NvOdmPeripheralClass_Other
},

// UART VDD (NV reserved)
{
    NV_VDD_UART_ODM_ID,
    s_VddIoUartAddresses,
    NV_ARRAY_SIZE(s_VddIoUartAddresses),
    NvOdmPeripheralClass_Other
},

// SDIO VDD (NV reserved)
{
    NV_VDD_SDIO_ODM_ID,
    s_VddIoSdmmc3Addresses,
    NV_ARRAY_SIZE(s_VddIoSdmmc3Addresses),
    NvOdmPeripheralClass_Other
},

// VI VDD (NV reserved)
{
    NV_VDD_VI_ODM_ID,
    s_VddIoViAddresses,
    NV_ARRAY_SIZE(s_VddIoViAddresses),
    NvOdmPeripheralClass_Other
},

// BB VDD (NV reserved)
{
    NV_VDD_BB_ODM_ID,
    s_VddIoBBAddresses,
    NV_ARRAY_SIZE(s_VddIoBBAddresses),
    NvOdmPeripheralClass_Other
},

//  VBUS for USB1
{
    NV_VDD_VBUS_ODM_ID,
    s_AVddUsbAddresses,
    NV_ARRAY_SIZE(s_AVddUsbAddresses),
    NvOdmPeripheralClass_Other
},

// VPP FUSE
{
    NV_VPP_FUSE_ODM_ID,
    s_VppFuseAddresses,
    NV_ARRAY_SIZE(s_VppFuseAddresses),
    NvOdmPeripheralClass_Other
},

//  Tps65914
{
    NV_ODM_GUID('t','p','s','6','5','9','1','4'),
    s_Tps65914Addresses,
    NV_ARRAY_SIZE(s_Tps65914Addresses),
    NvOdmPeripheralClass_Other
},

// Max77665a
{
    NV_ODM_GUID('m','a','x','7','7','6','6','5'),
    s_Max77665aAddresses,
    NV_ARRAY_SIZE(s_Max77665aAddresses),
    NvOdmPeripheralClass_Other
},

// Max17047
{
    NV_ODM_GUID('m','a','x','1','7','0','4','7'),
    s_Max17047Addresses,
    NV_ARRAY_SIZE(s_Max17047Addresses),
    NvOdmPeripheralClass_Other
},

//TPS65914 rails test
{
    NV_ODM_GUID('6','5','9','1','4','T','S','T'),
    s_AllRailAddresses,
    NV_ARRAY_SIZE(s_AllRailAddresses),
    NvOdmPeripheralClass_Other,
},

// Key Pad
{
    NV_ODM_GUID('k','e','y','b','o','a','r','d'),
    s_KeyPadAddresses,
    NV_ARRAY_SIZE(s_KeyPadAddresses),
    NvOdmPeripheralClass_HCI
},

// NOTE: This list *must* end with a trailing comma.
