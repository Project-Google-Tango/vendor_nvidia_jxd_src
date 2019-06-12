/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvassert.h"
#include "nverror.h"
#include "nvos.h"

#include "nvddk_aes_blockdev.h"
#include "nvddk_aes_priv.h"
#include "nvddk_aes_core.h"
#include "nvddk_aes_common.h"
#include "nvddk_aes.h"

// Defines client structure.
typedef struct NvDdkAesBlockDevClientRec
{
    NvDdkBlockDev BlockDev;
    NvDdkAes State;
} NvDdkAesBlockDevClient;

/**
 * Free up client's resources.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 *
 */
static void NvDdkAesBlockDevClose(NvDdkBlockDevHandle hBlockDev)
{
    NvDdkAesBlockDevClient *pClient = (NvDdkAesBlockDevClient *)hBlockDev;

    if (!pClient)
        return;

    AesCoreClose(&pClient->State);

    // Zero Out the Client memory before freeing
    NvOsMemset(pClient, 0, sizeof(NvDdkAesBlockDevClient));
    NvOsFree(pClient);
}

/**
 * It gets the device's information.
 * If device is hot pluggable, It must be called every time hot plug sema
 * is signalled.
 * If the device is present and initialized successfully, it will return
 * valid number of TotalBlocks.
 * If the device is neither present nor initialization failed, then it will
 * return TotalBlocks as zero.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 * @param pBlockDevInfo Returns device's information.
 *
 */
static void
NvDdkAesBlockDevGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo* pBlockDevInfo)
{
    /// Not Supported
}

/**
 * It should be called to register a semaphore, If the device is
 * hot pluggable. This sema will be signaled on both removal and insertion
 * of device.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 * @param hHotPlugSema Hanlde to semaphore.
 */
static void
NvDdkAesBlockDevRegisterHotplugSemaphore(
    NvDdkBlockDevHandle hBlockDev,
    NvOsSemaphoreHandle hHotPlugSema)
{
    /// Not Supported
}

/**
 * Reads data synchronously from the device.
 *
 * @warning The device needs to be initialized using NvDdkAesBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 * @param SectorNum Sector number to read the data from.
 * @param pBuffer Pointer to Buffer, which data will be read into.
 * @param NumberOfSectors Number of sectors to read.
 *
 * @retval NvSuccess Read is successful.
 * @retval NvError_ReadFailed Read operation failed.
 * @retval NvError_InvalidBlock Sector(s) address doesn't exist in the device.
 */
static NvError
NvDdkAesBlockDevReadSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    /// Not Supported
    return NvError_NotSupported;
}

/**
 * Writes data synchronously to the device.
 *
 * @warning The device needs to be initialized using NvDdkAesBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 * @param SectorNum Sector number to write the data to.
 * @param pBuffer Pointer to Buffer, which data will be written from.
 * @param NumberOfSectors Number of sectors to write.
 *
 * @retval NvSuccess Write is successful.
 * @retval NvError_WriteFailed Write operation failed.
 * @retval NvError_InvalidBlock Sector(s) address doesn't exist in the device.
 */
static NvError
NvDdkAesBlockDevWriteSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    /// Not Supported
    return NvError_NotSupported;
}

/**
 * Powers up the device. It could be taking out of low power mode or
 * reinitializing.such as --
 *
 * @warning The device needs to be initialized using NvDdkAesBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 */
static void NvDdkAesBlockDevPowerUp(NvDdkBlockDevHandle hBlockDev)
{
   NvDdkAesBlockDevClient *pClient = (NvDdkAesBlockDevClient *)hBlockDev;

    if (pClient)
        AesCorePowerUp(pClient->State.pAesCoreEngine);
}

/**
 * Powers down the device. It could be low power mode or shutdown.
 *
 * @warning The device needs to be initialized using NvDdkAesBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 */
static void NvDdkAesBlockDevPowerDown(NvDdkBlockDevHandle hBlockDev)
{
   NvDdkAesBlockDevClient *pClient = (NvDdkAesBlockDevClient *)hBlockDev;

    if (pClient)
        AesCorePowerDown(pClient->State.pAesCoreEngine);
}

/**
 * Flush buffered data out to the device.
 *
 * @warning The device needs to be initialized using NvDdkAesBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 */
static void NvDdkAesBlockDevFlushCache(NvDdkBlockDevHandle hBlockDev)
{
    /// Not Supported
}

/**
 * NvDevIoctl()
 *
 * Perform an I/O Control operation on the device.
 *
 * @param hBlockDev Handle acquired during the NvDdkAesBlockDevOpen call.
 * @param Opcode Type of control operation to perform
 * @param InputSize Size of input arguments buffer, in bytes
 * @param OutputSize Size of output arguments buffer, in bytes
 * @param InputArgs Pointer to input arguments buffer
 * @param OutputArgs Pointer to output arguments buffer
 *
 * @retval NvError_Success Ioctl is successful
 * @retval NvError_NotSupported Opcode is not recognized
 * @retval NvError_InvalidSize InputSize or OutputSize is incorrect.
 * @retval NvError_InvalidParameter InputArgs or OutputArgs is incorrect.
 * @retval NvError_IoctlFailed Ioctl failed for other reason.
 */
static NvError
NvDdkAesBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    NvError e = NvSuccess;
    NvDdkAesBlockDevClient *pClient = (NvDdkAesBlockDevClient *)hBlockDev;

    NV_ASSERT(pClient);

    if (pClient->State.pAesCoreEngine->IsEngineDisabled)
    {
        NvOsDebugPrintf("Crypto Engine Disabled, Returning IOCTL\n");
        return NvError_InvalidState;
    }

    // enable clocks before accessing AES
    NvDdkAesBlockDevPowerUp(hBlockDev);

    switch (Opcode)
    {
        case NvDdkAesBlockDevIoctlType_SelectKey:
        {
            if (InputSize != sizeof(NvDdkAesKeyInfo))
               return NvError_InvalidSize;
            e = NvDdkAesSelectKey(&pClient->State, (NvDdkAesKeyInfo*)InputArgs);
            break;
        }
        case NvDdkAesBlockDevIoctlType_SelectAndSetSbk:
        {
            if (InputSize != sizeof(NvDdkAesKeyInfo))
               return NvError_InvalidSize;
            e = NvDdkAesSelectAndSetSbk(&pClient->State, (NvDdkAesKeyInfo*)InputArgs);
            break;
        }
        case NvDdkAesBlockDevIoctlType_GetCapabilities:
        {
            if (OutputSize != sizeof(NvDdkAesBlockDevIoctl_GetCapabilitiesOutputArgs))
                return NvError_InvalidSize;
            e = NvDdkAesGetCapabilities(&pClient->State, (NvDdkAesCapabilities *)OutputArgs);
            break;
        }
        case NvDdkAesBlockDevIoctlType_SetInitialVector:
        {
            if (InputSize != sizeof(NvDdkAesBlockDevIoctl_SetInitialVectorInputArgs))
               return NvError_InvalidSize;
            e = NvDdkAesSetInitialVector(&pClient->State, (NvU8*)InputArgs, InputSize);
            break;
        }
        case NvDdkAesBlockDevIoctlType_GetInitialVector:
        {
            if (OutputSize != sizeof(NvDdkAesBlockDevIoctl_GetInitialVectorOutputArgs))
               return NvError_InvalidSize;
            e = NvDdkAesGetInitialVector(&pClient->State, OutputSize, (NvU8*)OutputArgs);
            break;
        }
        case NvDdkAesBlockDevIoctlType_SelectOperation:
        {
            if (InputSize != sizeof(NvDdkAesOperation))
               return NvError_InvalidSize;
            e = NvDdkAesSelectOperation(&pClient->State, (NvDdkAesOperation*)InputArgs);
            break;
        }
        case NvDdkAesBlockDevIoctlType_ProcessBuffer:
        {
            NvDdkAesBlockDevIoctl_ProcessBufferInputArgs *pBuffer = (NvDdkAesBlockDevIoctl_ProcessBufferInputArgs*) InputArgs;

            NV_ASSERT(pBuffer);

            e = NvDdkAesProcessBuffer(
                &pClient->State,
                pBuffer->BufferSize,
                pBuffer->BufferSize,
                pBuffer->SrcBuffer,
                pBuffer->DestBuffer);
            break;
        }
        case NvDdkAesBlockDevIoctlType_ClearSecureBootKey:
        {
            e = NvDdkAesClearSecureBootKey(&pClient->State);
            break;
        }
        case NvDdkAesBlockDevIoctlType_LockSecureStorageKey:
        {
            e = NvDdkAesLockSecureStorageKey(&pClient->State);
            break;
        }
        case NvDdkAesBlockDevIoctlType_SetAndLockSecureStorageKey:
        {
            NvDdkAesBlockDevIoctl_SetAndLockSecureStorageKeyInputArgs *pKey =
                (NvDdkAesBlockDevIoctl_SetAndLockSecureStorageKeyInputArgs*)InputArgs;
            if (!pKey)
               return NvError_BadParameter;

            if (InputSize != sizeof(NvDdkAesBlockDevIoctl_SetAndLockSecureStorageKeyInputArgs))
                return NvError_InvalidSize;
            e = NvDdkAesSetAndLockSecureStorageKey(&pClient->State, pKey->KeyLength, pKey->Key);
            break;
        }
        case NvDdkAesBlockDevIoctlType_DisableCrypto:
        {
            e = NvDdkAesDisableCrypto(&pClient->State);
            break;
        }
        default:
            NvOsDebugPrintf("AES DDK Unsupported IOCTL COMMAND\n");
            e = NvError_NotSupported;
            break;
    }

    // disable clocks after accessing AES
    NvDdkAesBlockDevPowerDown(hBlockDev);
    return e;
}

NvError
NvDdkAesBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDevInstance)
{
    NvDdkAesBlockDevClient *pNewClient = NULL;

    NV_ASSERT(phBlockDevInstance);

    if (AesCoreInitIsDone())
    {
        // Create client record
        pNewClient = NvOsAlloc(sizeof(NvDdkAesBlockDevClient));
        if (pNewClient == NULL)
            return NvError_InsufficientMemory;

        // Zero Out the Client memory initially
        NvOsMemset(pNewClient, 0, sizeof(NvDdkAesBlockDevClient));

        // Assign the function pointers of the block dev
        pNewClient->BlockDev.NvDdkBlockDevClose = NvDdkAesBlockDevClose;
        pNewClient->BlockDev.NvDdkBlockDevGetDeviceInfo = NvDdkAesBlockDevGetDeviceInfo;
        pNewClient->BlockDev.NvDdkBlockDevRegisterHotplugSemaphore = NvDdkAesBlockDevRegisterHotplugSemaphore;
        pNewClient->BlockDev.NvDdkBlockDevReadSector = NvDdkAesBlockDevReadSector;
        pNewClient->BlockDev.NvDdkBlockDevWriteSector = NvDdkAesBlockDevWriteSector;
        pNewClient->BlockDev.NvDdkBlockDevPowerUp = NvDdkAesBlockDevPowerUp;
        pNewClient->BlockDev.NvDdkBlockDevPowerDown = NvDdkAesBlockDevPowerDown;
        pNewClient->BlockDev.NvDdkBlockDevFlushCache = NvDdkAesBlockDevFlushCache;
        pNewClient->BlockDev.NvDdkBlockDevIoctl = NvDdkAesBlockDevIoctl;

        AesCoreOpen(&pNewClient->State.pAesCoreEngine);
        *phBlockDevInstance = &pNewClient->BlockDev;
        return NvSuccess;
    }
    else
        return NvError_NotInitialized;
}

NvError NvDdkAesBlockDevInit(NvRmDeviceHandle hRmDevice)
{
    return AesCoreInit(hRmDevice);
}

void NvDdkAesBlockDevDeinit(void)
{
    AesCoreDeinit();
}

