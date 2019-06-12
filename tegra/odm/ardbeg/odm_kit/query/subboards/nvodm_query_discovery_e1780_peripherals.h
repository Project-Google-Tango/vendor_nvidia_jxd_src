/*
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database Peripheral
 *                 entries for the peripherals on E1780 module.
 */

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

// USB VDD (NV reserved)
{
    NV_VDD_USB_ODM_ID,
    s_AVddUsbAddresses,
    NV_ARRAY_SIZE(s_AVddUsbAddresses),
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

// VPP FUSE
{
    NV_VPP_FUSE_ODM_ID,
    s_VppFuseAddresses,
    NV_ARRAY_SIZE(s_VppFuseAddresses),
    NvOdmPeripheralClass_Other
},

//  Tps80036
{
    NV_ODM_GUID('t','p','s','8','0','0','3','6'),
    s_Tps80036Addresses,
    NV_ARRAY_SIZE(s_Tps80036Addresses),
    NvOdmPeripheralClass_Other
},

//  Tps51632
{
    NV_ODM_GUID('t','p','s','5','1','6','3','2'),
    s_Tps51632Addresses,
    NV_ARRAY_SIZE(s_Tps51632Addresses),
    NvOdmPeripheralClass_Other
},

//  as3722
{
    NV_ODM_GUID('a','s','3','7','2','2',' ',' '),
    s_As3722Addresses,
    NV_ARRAY_SIZE(s_As3722Addresses),
    NvOdmPeripheralClass_Other,
},

// TPS80036 rails test
{
    NV_ODM_GUID('8','0','0','3','6','T','S','T'),
    s_ArdbegTps80036Rails,
    NV_ARRAY_SIZE(s_ArdbegTps80036Rails),
    NvOdmPeripheralClass_Other,
},

// TPS51632 rails test
{
    NV_ODM_GUID('5','1','6','3','2','T','S','T'),
    s_ArdbegTps51632Rails,
    NV_ARRAY_SIZE(s_ArdbegTps51632Rails),
    NvOdmPeripheralClass_Other,
},

// AS3722 rails test
{
    NV_ODM_GUID('3','7','2','2','T','S','T',' '),
    s_ArdbegAs3722Rails,
    NV_ARRAY_SIZE(s_ArdbegAs3722Rails),
    NvOdmPeripheralClass_Other,
},

//  Tps65914
{
    NV_ODM_GUID('t','p','s','6','5','9','1','4'),
    s_Tps65914Addresses,
    NV_ARRAY_SIZE(s_Tps65914Addresses),
    NvOdmPeripheralClass_Other
},

//  BQ2419x
{
    NV_ODM_GUID('f','f','d','2','4','1','9','x'),
    s_Bq2419xFFDAddresses,
    NV_ARRAY_SIZE(s_Bq2419xFFDAddresses),
    NvOdmPeripheralClass_Other
},

//  BQ2419x
{
    NV_ODM_GUID('b','q','2','4','1','9','x','*'),
    s_Bq2419xAddresses,
    NV_ARRAY_SIZE(s_Bq2419xAddresses),
    NvOdmPeripheralClass_Other
},

//  BQ2477x
{
    NV_ODM_GUID('b','q','2','4','7','7','x','*'),
    s_Bq2477xAddresses,
    NV_ARRAY_SIZE(s_Bq2477xAddresses),
    NvOdmPeripheralClass_Other
},

//  Max17048
{
    NV_ODM_GUID('f','f','d','1','7','0','4','8'),
    s_Max17048FFDAddresses,
    NV_ARRAY_SIZE(s_Max17048FFDAddresses),
    NvOdmPeripheralClass_Other
},

//  Max17048
{
    NV_ODM_GUID('m','a','x','1','7','0','4','8'),
    s_Max17048Addresses,
    NV_ARRAY_SIZE(s_Max17048Addresses),
    NvOdmPeripheralClass_Other
},

//  Bq24z45
{
    NV_ODM_GUID('b','q','2','4','z','4','5','*'),
    s_Bq24z45Addresses,
    NV_ARRAY_SIZE(s_Bq24z45Addresses),
    NvOdmPeripheralClass_Other
},

//  CW2015
{
    NV_ODM_GUID('c','w','2','0','1','5','*','*'),
    s_CW2015Addresses,
    NV_ARRAY_SIZE(s_CW2015Addresses),
    NvOdmPeripheralClass_Other
},

// TPS65914 rails test
{
   NV_ODM_GUID('6','5','9','1','4','T','S','T'),
   s_ArdbegTps65914Rails,
   NV_ARRAY_SIZE(s_ArdbegTps65914Rails),
   NvOdmPeripheralClass_Other,
},
// NOTE: This list *must* end with a trailing comma.
