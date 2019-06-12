/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef ANDROID_NV_CPU_SERVICE_H
#define ANDROID_NV_CPU_SERVICE_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/threads.h>
#include <utils/Errors.h>
#include <utils/Looper.h>

#include <binder/IMemory.h>
#include <binder/BinderService.h>

#include <nvcpud/INvCpuService.h>

#include "TimeoutPoker.h"

namespace android {

#define TIMEOUT_DECL(func) \
    virtual status_t func(int arg, nsecs_t timeoutNs);

#define HANDLE_DECL(func) \
    virtual int func##Handle(int arg);

#define SET_DECL(func) \
    virtual void set##func(int arg);

#define GET_DECL(func) \
    virtual int get##func(void);

class NvCpuService :
    public BinderService<NvCpuService>,
    public BnNvCpuService
{
public:
    static char const* getServiceName() { return "NvCpuService"; }

    NVCPU_FUNC_LIST(TIMEOUT_DECL);
    NVCPU_FUNC_LIST(HANDLE_DECL);

    NVCPU_AP_FUNC_LIST(SET_DECL);
    NVCPU_AP_FUNC_LIST(GET_DECL);

    NvCpuService();
    virtual ~NvCpuService();
    void init();

    //BnNvCpuService
    virtual status_t onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags);

    virtual status_t dump(int fd, const Vector<String16>& args);

    //Called first time it's used
    virtual void onFirstRef();
private:
    nvcpu::TimeoutPoker* mTimeoutPoker;
};

#undef TIMEOUT_DECL
#undef HANDLE_DECL
#undef SET_DECL
#undef GET_DECL

}; // namespace android

#endif
