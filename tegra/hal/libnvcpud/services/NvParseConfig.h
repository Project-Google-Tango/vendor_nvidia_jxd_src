/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef NV_PARSECONFIG_H
#define NV_PARSECONFIG_H

namespace android {
namespace nvcpu {

// maximum length of line in config file
#define MAX_LINE_LENGTH 256
#define NVCPUD_FILE_LOCATION "/etc/nvcpud.conf"

int NvCpud_parseCpuConf(int *cpuInfo, int *cameraLevels);
}
}

#endif // NV_PARSECONFIG_H
