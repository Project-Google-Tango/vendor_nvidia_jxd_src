/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_BDK_PCB_API_H
#define INCLUDED_BDK_PCB_API_H

#include "nverror.h"
#include "nvcommon.h"
#include "nvrm_i2c.h"
#include "nvodm_query_gpio.h"
#include "nvodm_services.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef NvError (*NvBDKTest_BoardFunc)(void *);

/** OSC API **/
typedef struct OscTestPrivateDataRec {
    NvU32 valid_range_us;
    NvU32 delay_loop_ms;
} OscTestPrivateData;
void NvApiOscCheck(NvBDKTest_TestPrivData param);

/** I2C API **/
#define I2C_MAX_READ_BUF_SIZE 512
typedef enum {
    _I2C_CMD_WRITE_ = 1,
    _I2C_CMD_READ_,
    _I2C_CMD_END_
} PcbTest_I2cCommand;

typedef struct PcbI2cBufRec {
    NvU32 len;
    NvU8 *buf;
} PcbI2cBuf;

typedef struct I2cTestPrivateDataRec {
    NvU32 instance;
    NvU32 i2cAddr;
    PcbTest_I2cCommand command;
    PcbI2cBuf rwData;
    PcbI2cBuf validationData;
} I2cTestPrivateData;

void NvApiI2cTransfer(NvBDKTest_TestPrivData param);

/** EMMC API **/
typedef struct EmmcTestPrivateDataRec {
    NvU32 instance;
    NvBDKTest_BoardFunc pValidationFunc;
} EmmcTestPrivateData;
void NvApiEmmcCheck(NvBDKTest_TestPrivData param);

/** WIFI API **/
#define PORT_SDMMC1 0
#define PORT_SDMMC3 2
#define PORT_SDMMC4 3

/* function numbers */
enum
{
    IO_FUNCTION0 = 0x0, // common function
    IO_FUNCTION1,
    IO_FUNCTION2,
    IO_FUNCTION3,
    IO_FUNCTION4,
    IO_FUNCTION5,
    IO_FUNCTION6,
    IO_FUNCTION_RESUME,
    IO_FUNCTION_MAX
};

typedef struct WifiTestPrivateDataRec {
    NvBDKTest_BoardFunc pInit;
    NvBDKTest_BoardFunc pDeinit;
    NvU32 instance;
    NvU32 testFuncId;
    NvU32 testAddr;
    NvU32 testCount;
    NvU32 vendorId;
    NvU32 deviceId;
    NvU32 verifyVendorId;
    NvU32 verifyDeviceId;
} WifiTestPrivateData;
void NvPcbWifiTest(NvBDKTest_TestPrivData param);

#define MAX_GPIO_KEYS   10
typedef struct NvBdkKbdContextRec
{
    const NvOdmGpioPinInfo *GpioPinInfo; // GPIO info struct
    NvOdmGpioPinHandle hOdmPins[MAX_GPIO_KEYS]; // array of gpio pin handles
    NvU32 PinCount;
    NvOdmServicesGpioHandle hOdmGpio;
} NvBdkKbdContext;
void NvPcbKeyTest(NvBDKTest_TestPrivData param);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_BDK_PCB_API_H
