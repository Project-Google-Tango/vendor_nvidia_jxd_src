/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
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

//  Tps65914
{
   NV_ODM_GUID('t','p','s','6','5','9','1','4'),
   s_Tps65914Addresses,
   NV_ARRAY_SIZE(s_Tps65914Addresses),
   NvOdmPeripheralClass_Other
},

// TPS65914 rails test
{
   NV_ODM_GUID('6','5','9','1','4','T','S','T'),
   s_LokiTps65914Rails,
   NV_ARRAY_SIZE(s_LokiTps65914Rails),
   NvOdmPeripheralClass_Other,
},

// OV7695 SOC YUV sensor
{
   SENSOR_YUV_OV7695_GUID,
   s_ffaImagerOV7695Addresses,
   NV_ARRAY_SIZE(s_ffaImagerOV7695Addresses),
   NvOdmPeripheralClass_Imager,
},

// MT9M114 SOC YUV sensor
{
   SENSOR_YUV_MT9M114_GUID,
   s_ffaImagerMT9M114Addresses,
   NV_ARRAY_SIZE(s_ffaImagerMT9M114Addresses),
   NvOdmPeripheralClass_Imager,
},

// NOTE: This list *must* end with a trailing comma.
