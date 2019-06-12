/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVML_DEVICE_H
#define INCLUDED_NVML_DEVICE_H

#include "nvml_device_private.h"

typedef struct NvMlDevMgrCallbacksRec
{
    NvBootDeviceGetParams      GetParams;
    NvBootDeviceValidateParams ValidateParams;
    NvBootDeviceGetBlockSizes  GetBlockSizes;
    NvBootDeviceInit           Init;
    NvBootDeviceReadPage       ReadPage;
    NvBootDeviceQueryStatus    QueryStatus;
    NvBootDeviceShutdown       Shutdown;
} NvMlDevMgrCallbacks;

/*
 * NvMlDevMgr: State & data used by the device manager.
 */
typedef struct NvMlDevMgrRec
{
    /* Device Info */
    NvU32                   BlockSizeLog2;
    NvU32                   PageSizeLog2;
    NvMlDevMgrCallbacks     *Callbacks;    /* Callbacks to the chosen driver. */
} NvMlDevMgr;


/*
 * NvMlContext: This is the structure used to manage global data used
 * during the boot process.
 */
typedef struct NvMlContextRec
{
    NvU8              *BootLoader; /* Start address of the boot loader. */

    NvMlDevMgr         DevMgr;

    NvBool             DecryptBootloader;
    NvBool             CheckBootloaderHash;
    NvU32              ChunkSize; /* Size of bootloader operations in bytes */

    NvBool             DecryptIsFirstBlock; /* True for processing 1st block */
    NvBool             HashIsFirstBlock; /* True for processing 1st block */

    NvBool             BlFailBack; /* Set from AO bit.  If true, failback to
                                    * older generation(s) of BLs.  If false,
                                    * failure to read the primary BL leads to
                                    * RCM.
                                    */
} NvMlContext;

/**
 * NvMlDevMgrInit(): Initialize the device manager, select the boot
 * device, and initialize it.  Upon completion, the device is ready for
 * access.
 *
 * @param[in] DevMgr Pointer to the device manager
 * @param[in] DevType The type of device from the fuses.
 * @param[in] ConfigIndex The device configuration data from the fuses
 *
 * @retval NvBootErrorSuccess The device was successfully initialized.
 * @retval NvBootInvalidParameter The device type was not valid.
 * @retval TODO Error codes from InitDevice()
 */
NvBootError NvMlDevMgrInit(NvMlDevMgr   *DevMgr,
                             NvBootDevType  DevType,
                             NvU32             ConfigIndex);

/**
 * NvMlVerifySignature(): verify the PKC signature of the binary
 * using the public key in BCT
 *
 * @param[in] Data Pointer to the data
 * @param[in] DataType bct or bootloader.
 * @param[in] Length length of the data
 *
 * @retval NV_TRUE The signature was successfully verified.
 * @retval NV_FALSE signature verification failed.
 */
NvBool
NvMlVerifySignature(NvU8 *Data, NvBool IsBct, NvU32 Length, NvU8 *Sig);

void NvMlSetMemioVoltage(NvU32 PllmFeedBackDivider);

NvBool NvMlCheckIfSbkBurned(void);
Nv3pDkStatus NvMlCheckIfDkBurned(void);

#endif//INCLUDED_NVML_DEVICE_H
