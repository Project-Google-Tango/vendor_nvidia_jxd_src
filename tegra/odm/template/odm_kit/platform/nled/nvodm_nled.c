/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_nled.h"
#include "nvodm_nled_int.h"

/**
 *  @brief Allocates a handle to the device.
 *  @param hOdmNled  [IN] Opaque handle to the device.
 *  @return  NV_TRUE on success and NV_FALSE on error
 */
NvBool
NvOdmNledOpen(NvOdmNledDeviceHandle *hOdmNled)
{
    NV_ODM_TRACE(("NvOdmNledOpen: enter\n"));
    NV_ODM_TRACE(("NvOdmNledOpen: exit\n"));
    return NV_TRUE;
}

/**
 *  @brief Closes the ODM device and destroys all allocated resources.
 *  @param hOdmNled  [IN] Opaque handle to the device.
 *  @return None.
 */
void NvOdmNledClose(NvOdmNledDeviceHandle hOdmNled)
{
    NV_ODM_TRACE(("NvOdmNledClose enter\n"));
    NV_ODM_TRACE(("NvOdmNledClose exit\n"));
}

/**
 *  @brief .Sets the NLED to solid, blinking or off states. The blink settings should be given when
 *          the state is requested as blinking.
 *  @param hOdmNled  [IN] Opaque handle to the device.
 *  @param State     [IN] State of the LED
 *  @param Settings  [IN] LED settings when the state is blinking
 *  @return NV_TRUE on success
 */
NvBool NvOdmNledSetState(NvOdmNledDeviceHandle hOdmNled, NvOdmNledState State, NvOdmNledSettings *pSettings)
{
    NV_ODM_TRACE(("NvOdmNledSetState enter\n"));

    switch (State)
    {
        case NvOdmNledState_On:
            {
                NV_ODM_TRACE("**************** ON ***************");
                break;
            }

        case NvOdmNledState_Off:
            {
                NV_ODM_TRACE("**************** OFF ***************");
                break;
            }

        case NvOdmNledState_Blinking:
            {
                NV_ODM_TRACE("**************** Blink ***************");
                break;
            }

        default:
            {
                NV_ODM_TRACE("NvOdmNledSetState: invalid state specified\r\n");
                return NV_FALSE;
            }
    }

    NV_ODM_TRACE("NvOdmNledSetState exit\r\n");
    return NV_TRUE;
}

/**
 *  @brief .Power handler function
 *  @param pHostContext  [IN] Opaque handle to the device.
 *  @param PowerDown     [IN] Flag to indicate power up/down
 *  @return NV_TRUE on success
 */
NvBool NvOdmNledPowerHandler(NvOdmNledDeviceHandle hOdmNled, NvBool PowerDown)
{
    return NV_TRUE;
}

