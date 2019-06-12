/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvos.h"
#include "nverror.h"

NvError UsbTestInit(NvU32 Instance)
{
    NvOsDebugPrintf("UsbInit not supported\n");
    return NvError_NotImplemented;
}

NvError NvUsbVerifyWriteRead(NvBDKTest_TestPrivData data)
{
    NvOsDebugPrintf("NvUsbVerifyWriteRead not supported\n");
    return NvError_NotImplemented;
}

NvError NvUsbVerifyWriteReadFull(NvBDKTest_TestPrivData data)
{
    NvOsDebugPrintf("NvUsbVerifyWriteReadFull not supported\n");
    return NvError_NotImplemented;
}

NvError NvUsbVerifyWriteReadSpeed(NvBDKTest_TestPrivData data)
{
    NvOsDebugPrintf("NvUsbVerifyWriteReadSpeed not supported\n");
    return NvError_NotImplemented;
}
void UsbTestClose(void)
{
    NvOsDebugPrintf("UsbTestClose not supported\n");
}
