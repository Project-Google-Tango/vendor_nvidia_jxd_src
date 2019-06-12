/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
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
 *         NV3P Server Query Interface</b>
 *
 * @b Description: Defines ODM query interface used by
 *                 the 3P server. Implementation is optional.
 */

#ifndef INCLUDED_NVODM_QUERY_NV3P_H
#define INCLUDED_NVODM_QUERY_NV3P_H

#include "nvcommon.h"

/**
 * @defgroup nvodm_query_nv3p NV3P Server Query Interface
 * This is the ODM query interface for NV3P Server.
 * @ingroup nvodm_query
 * @{
 */
#ifdef __cplusplus
extern "C"
#endif

/**
 * Gets values for the interface parameters of the USB
 * configuration descriptor that is returned during initial
 * device setup.
 *
 * @param pIfcClass A pointer to the interface class.
 * @param pIfcSubclass A pointer to the interface subclass.
 * @param pIfcProtocol A pointer to the interface protocol.
 * @return NV_TRUE to use the returned values in the descriptor,
 *  or NV_FALSE to use the default Nv3p parameters (required
 *  when running the recovery-mode server).
 */
NV_WEAK NvBool NvOdmQueryUsbConfigDescriptorInterface(
    NvU32* pIfcClass,
    NvU32* pIfcSubclass,
    NvU32* pIfcProtocol);

/**
 * Gets the vendor, product, and BCD device IDs that
 * are provided in the device descriptor packet during USB setup.
 *
 * @param pOdmVendorId A pointer to the vendor ID.
 * @param pOdmProductId A pointer to the product ID.
 * @param pOdmBcdDeviceId A pointer to the BCD device ID.
 *
 * @return NV_TRUE If the returned values should be used in the
 *  descriptor, or NV_FALSE if the Nv3p server's default values
 *  should be used (required when entering recovery mode).
 */
NV_WEAK NvBool NvOdmQueryUsbDeviceDescriptorIds(
    NvU32 *pOdmVendorId,
    NvU32 *pOdmProductId,
    NvU32 *pOdmBcdDeviceId);


/**
 * Gets the product ID string that is provided to the host during
 * USB setup.
 *
 * @return NULL-terminated string that the 3P server should transmit
 *  during device setup, or NULL to use the default string (required
 *  when running the recovery-mode server).
 */
NV_WEAK NvU8 *NvOdmQueryUsbProductIdString(void);

NV_WEAK NvU8 *NvOdmQueryUsbSerialNumString(void);

#ifdef __cplusplus
}
#endif

/** @} */
#endif
