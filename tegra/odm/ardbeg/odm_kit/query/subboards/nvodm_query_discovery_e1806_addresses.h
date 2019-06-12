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

// mt9m114 sensor
static const NvOdmIoAddress s_ffaImagerMT9M114Addresses[] =
{
    { NvOdmIoModule_I2c, 0x02, 0x90, 0 },
    { NvOdmIoModule_VideoInput, 0x00, NVODM_CAMERA_SERIAL_CSI_D1A, 0 },
    { NvOdmIoModule_ExternalClock, 2, 0, 0 } // CSUS
};
