/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef INCLUDED_NVSCHED_PRIVATE_H
#define INCLUDED_NVSCHED_PRIVATE_H

#include "nvrm_sched.h"
NvError NvSchedClientPrivInit(NvRmDeviceHandle hDevice, NvRmChannelHandle hChannel,
                          NvRmModuleID ModuleID,
                          NvSchedClient *sc);
void NvSchedClientPrivClose(NvSchedClient *sc);

#endif //INCLUDED_NVSCHED_PRIVATE_H
