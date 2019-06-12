/*
 * Copyright (c) 2006 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Keyboard Controller Interface</b>
 *
 * @b Description: Defines the ODM query interface for NVIDIA keyboard 
 *                 controller (KBC) adaptation.
 * 
 */

#ifndef INCLUDED_NVODM_QUERY_KBC_H
#define INCLUDED_NVODM_QUERY_KBC_H

#include "nvcommon.h"

/**
 * @defgroup nvodm_query_kbc Keyboard Controller Query Interface
 * This is the keyboard controller (KBC) ODM Query interface.
 * See also the \link nvodm_kbc KBC ODM Adaptation Interface\endlink.
 * @ingroup nvodm_query
 * @{
 */

/**
 * Defines the parameters associated with this device.
 */
typedef enum
{
    NvOdmKbcParameter_NumOfRows=1,
    NvOdmKbcParameter_NumOfColumns,
    NvOdmKbcParameter_DebounceTime,
    NvOdmKbcParameter_RepeatCycleTime,
    NvOdmKbcParameter_Force32 = 0x7FFFFFFF
} NvOdmKbcParameter;

/**
 * Queries the peripheral device for its current settings.
 * 
 * @see NvOdmKbcParameter
 *
 * @param param  Specifies which parameter value to get.
 * @param sizeOfValue  The length of the parameter data (in bytes).
 * @param value  A pointer to the location where the requested parameter shall
 *     be stored.
 * 
 */
void 
NvOdmKbcGetParameter(
        NvOdmKbcParameter param,
        NvU32 sizeOfValue,
        void *value);

/**
 * Gets the key code depending upon the row and column values.
 * 
 * @param Row  The value of the row.
 * @param Column The value of the column.
 * @param RowCount The number of the rows present in the keypad matrix.
 * @param ColumnCount The number of the columns present in the keypad matrix.
 * 
 * @return The appropriate key code.
 */
NvU32 
NvOdmKbcGetKeyCode(
    NvU32 Row, 
    NvU32 Column,
    NvU32 RowCount,
    NvU32 ColumnCount);

/**
 * Queries if wake-up only on selected keys is enabled for WPC-like
 * configurations. If it is enabled, returns the pointers to the static array
 * containing the row and columns numbers. If this is enabled and \a NumOfKeys
 * selected is zero, all the keys are disabled for wake-up when system is
 * suspended.
 * 
 * @note The selected keys must not be a configuration of type 1x1, 1x2, etc. 
 * In other words, a minimum of two rows must be enabled due to hardware
 * limitations.
 *
 * @param pRowNumber A pointer to the static array containing the row 
 *                   numbers of the keys.
 * @param pColNumber A pointer to the static array containing the column 
 *                   numbers of the keys.
 * @param NumOfKeys A pointer to the number of keys that must be enabled.
 *                  This indicates the number of elements in the arrays pointer by
 *                   \a pRowNumber and \a pColNumber.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmKbcIsSelectKeysWkUpEnabled(
    NvU32 **pRowNumber,
    NvU32 **pColNumber,
    NvU32 *NumOfKeys);

/** @} */
#endif // INCLUDED_NVODM_QUERY_KBC_H

