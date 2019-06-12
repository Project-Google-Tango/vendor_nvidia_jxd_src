/*
 * Copyright (c) 2006-2008 NVIDIA Corporation.  All Rights Reserved.
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
 *         KBC Adaptation Interface</b>
 *
 * @b Description: Defines the ODM adaptation interface for KBC keypad.
 * 
 */

#ifndef INCLUDED_NVODM_KBC_H
#define INCLUDED_NVODM_KBC_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvodm_services.h"

/**
 * @defgroup nvodm_kbc Keyboard Controller Adaptation Interface
 * This is the keyboard controller (KBC) ODM adaptation interface.
 * See also the \link nvodm_query_kbc ODM Query KBC Interface\endlink.
 * @ingroup nvodm_adaptation
 * @{
 */
/**
 * This API takes the keys that have been pressed as input and filters out the
 * the keys that may have been caused due to ghosting effect and key roll-over.
 *
 * @note The row and column numbers of the keys that have been left after the 
 * filtering are stored in the \a pRows and \a pCols arrays. The extra keys must be 
 * deleted from the array.
 *
 * @param pRows A pointer to the array of the row numbers of the keys that have 
 * been detected. This array contains \a NumOfKeysPressed elements.
 *
 * @param pCols A pointer to the array of the column numbers of the keys that have 
 * been detected. This array contains \a NumOfKeysPressed elements. 
 * 
 * @param NumOfKeysPressed The number of key presses that have been detected by 
 * the driver.
 *
 * @return The number of keys pressed after the filter has been applied.
 *
*/
NvU32
NvOdmKbcFilterKeys(
    NvU32 *pRows,
    NvU32 *pCols,
    NvU32 NumOfKeysPressed);


#if defined(__cplusplus)
    }
#endif
    
/** @} */
    
#endif // INCLUDED_NVODM_KBC_H

