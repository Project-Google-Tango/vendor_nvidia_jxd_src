/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NV3PSERVER_H
#define INCLUDED_NV3PSERVER_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvError Nv3pServer(void);
void T1xxUpdateCustDataBct(NvU8 *data);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NV3P_H
