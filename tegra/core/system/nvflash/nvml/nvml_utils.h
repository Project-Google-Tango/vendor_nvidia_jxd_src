/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVML_UTILS_H
#define INCLUDED_NVML_UTILS_H

#include "nvml.h"
#include "nvuart.h"

/**
 * NvMlUtilsSetupBootDevice(): Read boot device information from fuses, convert their
 *     values as needed, and initialize the device manager (which, in turn,
 *     will initialize the boot device).
 *
 * @param[in] Context Pointer to the boot context.
 *
 * @retval NvBootError_Success The boot device is ready for reading.
 *
 * Errors from NvBootFuseGetBootDevice:
 * @retval NvBootError_InvalidBootDeviceEncoding The device selection fuses
 * could not be mapped to a valid value.
 *
 * @retval TODO Errors from NvBootPadsConfigureForBootPeripheral
 * @retval TODO Errors from NvMlDevMgrInit
 */
NvBootError NvMlUtilsSetupBootDevice(void *Context);

/**
 * This API is supposed to perform all
 * hash related initialization
 *
 * @param engine AES engine (input)
  * @return none
 */
void
NvMlUtilsInitHash(NvBootAesEngine engine);

/**
 * This API is supposed to perform all
 * hash related de-initialization
 *
 * @param engine AES engine (input)
  * @return none
 */
void
NvMlUtilsDeInitHash(NvBootAesEngine engine);

/**
 * This API uses NvBootHash .interface for hashing blocks
 *
 * @param engine AES engine (input)
 * @param slot key slot (input)
 * @param pK1 pointer to K1 parameter, needed only for last block (input)
 * @param pData pointer to block of data to be hashed (input)
 * @param pHash pointer to hash result (output)
 *              Though parameter pHash is redundant right now, this may be utilized when hashing is not
 *              used in hardware NvBootAesStartEngine aes code asserts when either source or destination
 *              are NULL
 * @param numBlocks number of blocks over which the has is to be computed
 * @param startMsg Start of the message to be hashed.
 * @param endMsg End of the message to be hashed.
 * @return none
 *
 * Note: this implementation restricts the message length to be a multiple
 *       of 16 bytes; otherwise, the amount of data in the final "block"
 *       would be needed (to compute the padding), along with a 16-byte
 *       scratch buffer to hold the padded block, and the K2 parameter
 */
void
NvMlDoHashBlocks (
    NvBootAesEngine engine,
    NvBootAesIv *pK1,
    NvU8 *pData,
    NvBootHash *pHash,
    NvU32 numBlocks,
    NvBool startMsg,
    NvBool endMsg
);

/**
 * This API is reads the hash output
 * Required only when hash result is not directed to the hash buffer.
 *
 * @param engine AES engine (input)
  * @param pHash pointer to hash buffer (output)
  * @return none
 */
void
NvMlUtilsReadHashOutput(
    NvBootAesEngine engine,
    NvBootHash *pHash);

/**
 * Converts boot fuse device type to 3pdevice type
 * @param[in] NvBootFuse, NvBootFuseBootDevice type for which conversion is required,
 * @param[out] pNv3pDevice, pointer to Nv3pDevice type
 * converted vevice type value will be sent back using this pointer
 * @retval NvError_Success If conversion is successfull
 * @retval NvError_BadParameterIf conversion boot fuse device doesnt exist.
 */
NvError
NvMlUtilsConvertBootFuseDeviceTo3p(
    NvBootFuseBootDevice NvBootFuse,
    Nv3pDeviceType *pNv3pDevice);

NvError
NvMlUtilsConvert3pToBootFuseDevice(
    Nv3pDeviceType DevNv3p,
    NvBootFuseBootDevice *pDevNvBootFuse);

/**
 * Converts bootdev type to boot devtype to 3p sec device
 * @param[in] NvBootDevType
 * @param[out] pNv3pDevice pointer to Nv3pDevice type
 * @retval NvError_Success If conversion is successfull
 * @retval NvError_BadParameterIf conversion bootdev type is not possible
 */
NvError
NvMlUtilsConvertBootDevTypeTo3p(
    NvBootDevType BootDev,
    Nv3pDeviceType *pNv3pDevice);

/**
 * NvMlUtilsGetSecondaryBootDevice(): reads the fuses and gets the secondary boot device
 * @retval NvBootDevType boot device type
 */
NvBootDevType
NvMlUtilsGetSecondaryBootDevice(void);

/**
 * NvMlUtilsGetBootDeviceConfig(): reads the boot device config value from fuse and straps.
 * @pConfigVal pConfigVal pointer to the config value
 */
void NvMlUtilsGetBootDeviceConfig(NvU32 *pConfigVal);


/**
 * NvMlUtilsGetBootDeviceType(): reads the fuses and gets the boot device type.
 * @pDevType pDevType pointer to the boot device type
 */
void NvMlUtilsGetBootDeviceType(NvBootFuseBootDevice *pDevType);
/**
 * NvMlUtilsGetBctDeviceType(): reads the bct and gets the boot device type.
 * @retval NvError_Success If validation for boot fuse device is successfull
 * @retval NvError_BadParameter If validation for  boot fuse device fails.
 */
NvError NvMlUtilsValidateBctDevType(void);

/**
 * NvMlUtilsGetBctSize(): Gets the size of BCT structure
 */
NvU32 NvMlUtilsGetBctSize(void);

/**
 * Enables Pllm and and sets PLLM in recovery path which is normally done by BOOT ROM
 * except in recovery path.
 */
void SetPllmInRecovery(void);

/**
 * NvMlUtilsFuseGetUniqueId(): Gets the chip uniqueId from the fuses
 */
void NvMlUtilsFuseGetUniqueId(void *Id);

/**
 * NvMlUtilsSskGenerate(): Generates ssk from sbk and uid
 */
void NvMlUtilsSskGenerate(
    NvBootAesEngine SbkEngine,
    NvBootAesKeySlot SbkEncryptSlot,
    NvU8 *ssk);

/**
 * NvMlUtilsClocksStartPll(): Starts Pll clocks
 */
void NvMlUtilsClocksStartPll(NvU32 PllSet,
        NvBootSdramParams * pData,
        NvU32 StableTime);

#if NO_BOOTROM

/**
 * NvMlUtilsPreInitBit():Pre-initializes the bct in emulation platform
 * @pBitTable pointer to the BIT table
 */
void NvMlUtilsPreInitBit(NvBootInfoTable *pBitTable);

/**
 * NvMlUtilsStartUsbEnumeration():
 * does the USB enumeration
 */
NvBool NvMlUtilsStartUsbEnumeration(void);

/**
 * NvMlUtilsGetPLLPFreq():
 * return pllp frequency in bootrom
 */
NvU32 NvMlUtilsGetPLLPFreq(void);
#endif

#endif//INCLUDED_NVML_UTILS_H

