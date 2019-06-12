/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * invaudioservice.h
 *
 * Binder based communications interface definition for NvAudioALSAService.
 *
 */

#ifndef NVCAPAUDIO_SERVICE_H
#define NVCAPAUDIO_SERVICE_H

#include "invaudioservice.h"

using namespace android;

class INvCapAudioService: public IInterface
{
public:
    DECLARE_META_INTERFACE(NvCapAudioService);

    virtual void registerClient(const sp<INvAudioALSAClient>& client) = 0;
};

/* --- Server side --- */

class BnNvCapAudioService: public BnInterface<INvCapAudioService>
{
public:
  virtual status_t    onTransact( uint32_t code,
                                  const Parcel& data,
                                  Parcel* reply,
                                  uint32_t flags = 0);
};
#endif
