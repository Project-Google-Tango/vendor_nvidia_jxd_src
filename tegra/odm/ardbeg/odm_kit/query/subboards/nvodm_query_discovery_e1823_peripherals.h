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

// Imager - Rear Sensor
{
    IMAGER_NVC_GUID,
    NULL, // deprecated, set to NULL
    0, // deprecated, set to 0
    NvOdmPeripheralClass_Imager
},

// Imager - Front Sensor
{
    SENSOR_BAYER_AR0261_GUID,
    NULL, // deprecated, set to NULL
    0, // deprecated, set to 0
    NvOdmPeripheralClass_Imager
},

// Imager - Rear Sensor
{
    SENSOR_BAYER_IMX135_GUID,
    NULL, // deprecated, set to NULL
    0, // deprecated, set to 0
    NvOdmPeripheralClass_Imager
},


// Imager - Flash
{
    TORCH_NVC_GUID,
    NULL,
    0,
    NvOdmPeripheralClass_Other
},
// NOTE: This list *must* end with a trailing comma.
