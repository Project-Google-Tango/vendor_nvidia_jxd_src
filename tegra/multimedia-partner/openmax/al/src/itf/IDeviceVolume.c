/*
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "linux_audioiodeviceinfo.h"

static XAresult
IDeviceVolume_GetVolumeScale(
    XADeviceVolumeItf self,
    XAuint32 DeviceID,
    XAint32 *pMinValue,
    XAint32 *pMaxValue,
    XAboolean *pIsMillibelScale)
{
    IDeviceVolume *pDeviceVolume = (IDeviceVolume *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pDeviceVolume && pMinValue && pMaxValue && pIsMillibelScale);
    interface_lock_poke(pDeviceVolume);
    result = ALBackendGetDeviceVolumeScale(
        pDeviceVolume,
        DeviceID,
        pMinValue,
        pMaxValue);
    interface_unlock_poke(pDeviceVolume);
    *pIsMillibelScale = XA_BOOLEAN_FALSE;
    XA_LOGD("\nMIn Vol = %x - Max Vol = %x\n", *pMinValue , *pMaxValue);

    XA_LEAVE_INTERFACE
}

static XAresult
IDeviceVolume_SetVolume(
    XADeviceVolumeItf self,
    XAuint32 DeviceID,
    XAint32 Volume)
{
    IDeviceVolume *pDeviceVolume = (IDeviceVolume *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pDeviceVolume);
    interface_lock_poke(pDeviceVolume);
    result = ALBackendSetDeviceVolume(
        pDeviceVolume,
        DeviceID,
        Volume);
    interface_unlock_poke(pDeviceVolume);

    XA_LEAVE_INTERFACE
}

static XAresult
IDeviceVolume_GetVolume(
    XADeviceVolumeItf self,
    XAuint32 DeviceID,
    XAint32 *pVolume)
{
    IDeviceVolume *pDeviceVolume = (IDeviceVolume *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pDeviceVolume && pVolume);
    interface_lock_poke(pDeviceVolume);
    result = ALBackendGetDeviceVolume(
        pDeviceVolume,
        DeviceID,
        pVolume);
    interface_unlock_poke(pDeviceVolume);

    XA_LEAVE_INTERFACE
}

static const struct XADeviceVolumeItf_ IDeviceVolume_Itf =
{
    IDeviceVolume_GetVolumeScale,
    IDeviceVolume_SetVolume,
    IDeviceVolume_GetVolume
};

void IDeviceVolume_init(void *self)
{
    IDeviceVolume *pDeviceVolume = (IDeviceVolume *)self;
    if (pDeviceVolume)
    {
        pDeviceVolume->mItf = &IDeviceVolume_Itf;
    }
}
