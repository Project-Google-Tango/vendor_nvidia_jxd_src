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
 *                 entries for the peripherals on E1612 module.
 */

// Imager - Front Sensor
{
    SENSOR_YUV_MT9M114_GUID,
    s_ffaImagerMT9M114Addresses,
    NV_ARRAY_SIZE(s_ffaImagerMT9M114Addresses),
    NvOdmPeripheralClass_Imager
},

// Imager - Rear Sensor
{
    SENSOR_BAYER_OV5693_GUID,
    NULL, // deprecated, set to NULL
    0, // deprecated, set to 0
    NvOdmPeripheralClass_Imager
},

