/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
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
 *         Reserved Key ID Definition</b>
 *
 * @b Description: Defines the reserved key IDs for the default keys provided
 *                 by the ODM key/value list service.
 */

#ifndef INCLUDED_NVODM_KEYLIST_RESERVED_H
#define INCLUDED_NVODM_KEYLIST_RESERVED_H

/**
 * @addtogroup nvodm_services
 * @{
 */

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines the list of reserved key IDs for the ODM key/value list service.
 * These keys may be read by calling NvOdmServicesGetKeyValue(), but they
 * may not be modified.
 */
enum 
{
    /// Specifies the starting range of key IDs reserved for use by NVIDIA.
    NvOdmKeyListId_ReservedAreaStart = 0x6fff0000UL,

    /** Returns the value stored in the CustomerOption field of the BCT,
     *  which was specified when the device was flashed. If no value was
     *  specified when flashing, a default value of 0 will be returned. */
    NvOdmKeyListId_ReservedBctCustomerOption = NvOdmKeyListId_ReservedAreaStart,

    /// Specifes the last ID of the reserved key area.
    NvOdmKeyListId_ReservedAreaEnd = 0x6ffffffeUL

};

#if defined(__cplusplus)
}
#endif

/** @} */
#endif
