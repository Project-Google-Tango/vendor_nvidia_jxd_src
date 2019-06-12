/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVABOOT_USBF_H
#define NVABOOT_USBF_H

#include "nverror.h"
#ifdef LPM_BATTERY_CHARGING
/**
 * @brief Detects the USB charger.
 *
 * Detects the USB charger according to the "USB Battery Charging
 * Specification" version 1.0. The result can be read through
 * NvBlUsbfGetChargerType().
 *
 * @returns NV_TRUE if an USB charger is connected
 */
NvBool NvBlDetectUsbCharger(void);

/**
 * @brief Gets the USB enumerator from the USB host.
 *
 * @returns NV_TRUE if the configuration is accepted by the USB host.
 */
NvBool NvBlUsbEnumerate(void);

/**
 * @brief Reads out the result of NvBlDetectUsbCharger().
 * @param chargerType pointer in which the type of charger
 * will be returned
 * @returns appropriate error if any.
 */
NvError NvBlUsbfGetChargerType(NvOdmUsbChargerType *chargerType);

/**
 * @brief Close USB charger.
 *
 */
void NvBlUsbClose(void);
#endif
#endif
