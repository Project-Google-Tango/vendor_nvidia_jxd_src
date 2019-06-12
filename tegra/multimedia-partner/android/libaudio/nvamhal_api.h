/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NVAMHAL_API_H
#define NVAMHAL_API_H

/* Application Framework / Audio HW -> AMAHL commands */
#define HAL_OPEN "amhal_open"
#define HAL_CLOSE "amhal_close"
#define HAL_SENDTO "amhal_sendto"
#define HAL_GET_STATUS "amhal_getstatus"
#define HAL_LIB "libamhal.so"

typedef enum
{
    AMHAL_SPUV_START,
    AMHAL_SPUV_STOP,
    AMHAL_PCM_START,
    AMHAL_PCM_STOP,
    AMHAL_PCM_LOOPBACK_START,
    AMHAL_PCM_LOOPBACK_STOP,
    AMHAL_BBIF_LOOPBACK_START,
    AMHAL_BBIF_LOOPBACK_STOP,
    AMHAL_VOLUME,
//    AMHAL_MUTE,
    AMHAL_PARAMETERS,
    AMHAL_STOP,
} amhal_command_t;


typedef void* (*pFnamhal_open)(void*);
typedef void (*pFnamhal_close)(void*);
typedef int (*pFnamhal_sendto)(void*, int, void*);
typedef int (*pFnamhal_getstatus)(void*);

#endif // NVAMHAL_API_H
