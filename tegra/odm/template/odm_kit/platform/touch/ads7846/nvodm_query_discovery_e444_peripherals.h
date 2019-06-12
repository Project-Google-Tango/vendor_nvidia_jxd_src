/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database Peripheral entries
 *                 for the E906 LCD Module.
 */

//  Touch Panel
{
    NV_ODM_GUID('a','d','s','t','o','u','c','h'),
    s_ffaAds7846TouchPanelAddresses,
    NV_ARRAY_SIZE(s_ffaAds7846TouchPanelAddresses),
    NvOdmPeripheralClass_HCI
},
// NOTE: This list *must* end with a trailing comma.
