/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA IRAM Addresses API</b>
 *
 * @b Description: Contains notations of all addresses in IRAM used by the
 *                 boot loader.
 */

#ifndef INCLUDED_IRAM_ADDRESS_ALLOCATION_H
#define INCLUDED_IRAM_ADDRESS_ALLOCATION_H

/**
 * @defgroup nviram_boot IRAM Addresses Used by Boot Loader
 *
 * This contains notations of all addresses in IRAM used by the boot
 * loader.
 *
 * @pre
 * IRAM usage in recovery mode
 *                T30                  T11x
 * BIT            0x0~0xf0             0x0~0xf0
 * BCT[FR]        0xf0~0x18f0          0xf0~0x20f0
 * BCT[RC]        0x18f0~0x30f0        0x20f0~0x40f0
 * FREE AREA      0x30f0~0x3100[16]    0x40f0~0x4100[16]
 * AES KEY TABLE  0x3100~0x3140        0x4100~0x4140
 * FREE AREA      0x3140~0x4000[3776]  0x4140~0x5000[3776]
 * USB            0x4000~0x9000        0x5000~0xA000
 * FREE AREA      0x9000~0xA000[4096]  0xA000-0xE000[16384]
 * ML             0xA000               0xE000
 * []->free space in bytes
 * @pre
 *
 * @ingroup nv3p_modules
 * @{
 */

/// Size of BCT for T30 platforms is 6128 bytes (~0x1800).
/// BCT uses 0xF0 to 0x18F0 offset in IRAM in FR mode and
/// address 0x18F0 to 0x30F0 when device goes into recovery mode
/// due to BCT validation failure.
///
/// Iram Address offsets aligned to 0x1000 allocated to USB transfer buffer for T30.
#define NV3P_AES_KEYTABLE_OFFSET_T30            (0x3100)
#define NV3P_TRANSFER_EP0_BUFFER_OFFSET_T30     (0x4000)
#define NV3P_TRANSFER_EP_BULK_BUFFER_OFFSET_T30 (0x6000)
#define NV3P_USB_BUFFER_OFFSET_T30              (0x7000)

/// Size of BCT for T11x platforms is 8096 bytes (~0x2000).
/// BCT uses 0xF0 to 0x20F0 offset in IRAM in FR mode and
/// address 0x20F0 to 0x40F0 when device goes into recovery mode
/// due to BCT validation failure.
/// Size of BCT for T14x platforms is different though.
///
/// Iram Address offsets aligned to 0x1000 allocated to USB transfer buffer for T11x.
#define NV3P_AES_KEYTABLE_OFFSET_T1XX            (0x4100)
#define NV3P_TRANSFER_EP0_BUFFER_OFFSET_T1XX     (0x5000)
#define NV3P_TRANSFER_EP_BULK_BUFFER_OFFSET_T1XX (0x7000)
#define NV3P_USB_BUFFER_OFFSET_T1XX              (0x8000)

/** @} */
#endif // INCLUDED_IRAM_ADDRESS_ALLOCATION_H
