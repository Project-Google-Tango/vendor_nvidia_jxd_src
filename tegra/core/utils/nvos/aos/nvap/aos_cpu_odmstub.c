/*
 * Copyright (c) 2010-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "aos.h"

NvOdmDebugConsole
aosGetOdmDebugConsole(void)
{
#ifdef BUILD_FOR_COSIM
    return NvOdmDebugConsole_UartA;
#else
#if AOS_MON_MODE_ENABLE
    return NvOdmDebugConsole_UartA;
#else
    return NvOdmQueryDebugConsole();
#endif
#endif
}
