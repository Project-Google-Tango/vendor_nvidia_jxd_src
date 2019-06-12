/*
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
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
#include "nvddk_blockdev.h"
#include "nvddk_se_blockdev.h"
#include "nvddk_se_core.h"

static NvU32 s_gOpenHandleCount;

// Defines client structure.
typedef struct NvDdkSeBlockDevClientRec
{
    NvDdkBlockDev BlockDev;
    NvDdkSe SeClient;
} NvDdkSeBlockDevClient;

/**
 * Free up client's resources.
 *
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
 *
 */
void NvDdkSeBlockDevClose(NvDdkBlockDevHandle hBlockDev)
{
    NvDdkSeBlockDevClient *pClient = (NvDdkSeBlockDevClient *)hBlockDev;

    if (!pClient)
        return;
    /* Free out Aes key slots Used */
    NvDdkSeAesReleaseKeySlot(&pClient->SeClient.Algo.SeAesHwCtx);
    /* Remove the client */
    if (s_gOpenHandleCount > 0)
        s_gOpenHandleCount--;
    /* Zero Out the Client memory before freeing */
    NvOsMemset(pClient, 0, sizeof(NvDdkSeBlockDevClient));
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
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
 * @param pBlockDevInfo Returns device's information.
 *
 */
static void
NvDdkSeBlockDevGetDeviceInfo(
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
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
 * @param hHotPlugSema Hanlde to semaphore.
 */
static void
NvDdkSeBlockDevRegisterHotplugSemaphore(
    NvDdkBlockDevHandle hBlockDev,
    NvOsSemaphoreHandle hHotPlugSema)
{
    /// Not Supported
}

/**
 * Reads data synchronously from the device.
 *
 * @warning The device needs to be initialized using NvDdkSeBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
 * @param SectorNum Sector number to read the data from.
 * @param pBuffer Pointer to Buffer, which data will be read into.
 * @param NumberOfSectors Number of sectors to read.
 * @retval Not supported.
 */
static NvError
NvDdkSeBlockDevReadSector(
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
 * @warning The device needs to be initialized using NvDdkSeBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
 * @param SectorNum Sector number to write the data to.
 * @param pBuffer Pointer to Buffer, which data will be written from.
 * @param NumberOfSectors Number of sectors to write.
 * @retval Not supported.
 */
static NvError
NvDdkSeBlockDevWriteSector(
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
 * @warning The device needs to be initialized using NvDdkSeBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
 */
static void NvDdkSeBlockDevPowerUp(NvDdkBlockDevHandle hBlockDev)
{
    /// Not Supported
}

/**
 * Powers down the device. It could be low power mode or shutdown.
 *
 * @warning The device needs to be initialized using NvDdkSeBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
 */
static void NvDdkSeBlockDevPowerDown(NvDdkBlockDevHandle hBlockDev)
{
    /// Not Supported
}

/**
 * Flush buffered data out to the device.
 *
 * @warning The device needs to be initialized using NvDdkSeBlockDevOpen before
 * using this method.
 *
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
 */
static void NvDdkSeBlockDevFlushCache(NvDdkBlockDevHandle hBlockDev)
{
    /// Not Supported
}

/**
 * NvDevIoctl()
 *
 * Perform an I/O Control operation on the device.
 *
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen call.
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
NvError
NvDdkSeBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    NvError e = NvSuccess;
    NvDdkSeBlockDevClient *pClient = (NvDdkSeBlockDevClient *)hBlockDev;

    NV_ASSERT(pClient);

    switch (Opcode)
    {
        case NvDdkSeBlockDevIoctlType_SHAInit:
        {
            if (InputSize != sizeof(NvDdkSeShaInitInfo))
               return NvError_InvalidSize;
            e = NvDdkSeShaInit(&pClient->SeClient.Algo.SeShaHwCtx, (NvDdkSeShaInitInfo*)InputArgs);
            break;
        }
        case NvDdkSeBlockDevIoctlType_SHAUpdate:
        {
            if (InputSize != sizeof(NvDdkSeShaUpdateArgs))
                return NvError_InvalidSize;
            e = NvDdkSeShaUpdate(&pClient->SeClient.Algo.SeShaHwCtx, (NvDdkSeShaUpdateArgs *)InputArgs);
            break;
        }
        case NvDdkSeBlockDevIoctlType_SHAFinal:
        {
            if (!OutputArgs)
               return NvError_InvalidSize;
            e = NvDdkSeShaFinal(&pClient->SeClient.Algo.SeShaHwCtx, (NvDdkSeShaFinalInfo*)OutputArgs);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_SelectKey:
        {
            if (InputSize != sizeof(NvDdkSeAesKeyInfo))
               return NvError_InvalidSize;
            e = NvDdkSeAesSelectKey(&pClient->SeClient.Algo.SeAesHwCtx,
                    (const NvDdkSeAesKeyInfo*)InputArgs);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_GetInitialVector:
        {
            if (OutputSize != sizeof(NvDdkSeAesGetIvArgs))
               return NvError_InvalidSize;
            e = NvDdkSeAesGetInitialVector(&pClient->SeClient.Algo.SeAesHwCtx,
                    (NvDdkSeAesGetIvArgs *)OutputArgs);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_SelectOperation:
        {
            if (InputSize != sizeof(NvDdkSeAesOperation))
               return NvError_InvalidSize;
            e = NvDdkSeAesSelectOperation(&pClient->SeClient.Algo.SeAesHwCtx,
                    (NvDdkSeAesOperation*)InputArgs);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_ProcessBuffer:
        {
            if (InputSize != sizeof(NvDdkSeAesProcessBufferInfo))
                return NvError_InvalidSize;
            e = NvDdkSeAesProcessBuffer(
                &pClient->SeClient.Algo.SeAesHwCtx,
                (NvDdkSeAesProcessBufferInfo *)InputArgs);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_ClearSecureBootKey:
        {
            e = NvDdkSeAesClearSecureBootKey(&pClient->SeClient.Algo.SeAesHwCtx);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_CalculateCMAC:
        {
           if (InputSize != sizeof(NvDdkSeAesComputeCmacInInfo))
                return NvError_InvalidSize;
            e = NvDdkSeAesComputeCMAC(
                    &pClient->SeClient.Algo.SeAesHwCtx,
                    (const NvDdkSeAesComputeCmacInInfo *)InputArgs,
                    (NvDdkSeAesComputeCmacOutInfo *)OutputArgs);

            break;
        }
        case NvDdkSeAesBlockDevIoctlType_LockSecureStorageKey:
        {
            e = NvDdkSeAesLockSecureStorageKey(&pClient->SeClient.Algo.SeAesHwCtx);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_SetAndLockSecureStorageKey:
        {
            if (sizeof(NvDdkSeAesSsk) != InputSize)
                return NvError_InvalidSize;
            e = NvDdkSeAesSetAndLockSecureStorageKey(&pClient->SeClient.Algo.SeAesHwCtx,
                    (const NvDdkSeAesSsk *)InputArgs);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_SetIV:
        {
            if (sizeof(NvDdkSeAesSetIvInfo) != InputSize)
                return NvError_InvalidSize;
            e = NvDdkSeAesSetIv(&pClient->SeClient.Algo.SeAesHwCtx,
                    (const NvDdkSeAesSetIvInfo *)InputArgs);
            break;
        }
        case NvDdkSeAesBlockDevIoctlType_GetIV:
        {
            e = NvDdkSeAesGetIv(&pClient->SeClient.Algo.SeAesHwCtx,
                    (NvDdkSeAesSetIvInfo *)OutputArgs);
            break;
        }
        case NvDdkSeBlockDevIoctlType_RSAModularExponentiation:
        {
            if (InputSize != sizeof(NvSeRsaKeyDataInfo))
                return NvError_InvalidSize;
            e = NvDdkSeRSAPerformModExpo(&pClient->SeClient.Algo.SeRsaHwCtx,
                                         (NvSeRsaKeyDataInfo*)InputArgs,
                                         (NvSeRsaOutDataInfo*)OutputArgs);
            break;
        }
         case NvDdkSeBlockDevIoctlType_RNGSetUpContext:
        {
            if (InputSize != sizeof(NvDdkSeRngInitInfo))
                return NvError_InvalidSize;
            e = NvDdkSeRngSetUpContext(&pClient->SeClient.Algo.SeRngHwCtx,
                    (NvDdkSeRngInitInfo *)InputArgs);
            break;
        }
        case NvDdkSeBlockDevIoctlType_RNGGetRandomNum:
        {
            if (InputSize != sizeof(NvDdkSeRngProcessInInfo) || !OutputArgs)
                return NvError_BadParameter;
            e = NvDdkSeRngGetRandomNumber(&pClient->SeClient.Algo.SeRngHwCtx,
                    (NvDdkSeRngProcessInInfo *)InputArgs,
                    (NvDdkSeRngProcessOutInfo *)OutputArgs);
            break;
        }
        default:
            NvOsDebugPrintf("Unsupported IOCTL COMMAND in SE DDK\n");
            e = NvError_NotSupported;
            break;
    }
    return e;
}

NvError
NvDdkSeBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDevInstance)
{
    NvError e = NvSuccess;
    NvDdkSeBlockDevClient *pNewClient = NULL;

    NV_ASSERT(phBlockDevInstance);

    if (NvDdkIsSeCoreInitDone())
    {
        // Create client record
        pNewClient = NvOsAlloc(sizeof(NvDdkSeBlockDevClient));
        if (pNewClient == NULL)
            return NvError_InsufficientMemory;

        // Zero Out the Client memory initially
        NvOsMemset(pNewClient, 0, sizeof(NvDdkSeBlockDevClient));

        // Assign the function pointers of the block dev
        pNewClient->BlockDev.NvDdkBlockDevClose = NvDdkSeBlockDevClose;
        pNewClient->BlockDev.NvDdkBlockDevGetDeviceInfo = NvDdkSeBlockDevGetDeviceInfo;
        pNewClient->BlockDev.NvDdkBlockDevRegisterHotplugSemaphore = NvDdkSeBlockDevRegisterHotplugSemaphore;
        pNewClient->BlockDev.NvDdkBlockDevReadSector = NvDdkSeBlockDevReadSector;
        pNewClient->BlockDev.NvDdkBlockDevWriteSector = NvDdkSeBlockDevWriteSector;
        pNewClient->BlockDev.NvDdkBlockDevPowerUp = NvDdkSeBlockDevPowerUp;
        pNewClient->BlockDev.NvDdkBlockDevPowerDown = NvDdkSeBlockDevPowerDown;
        pNewClient->BlockDev.NvDdkBlockDevFlushCache = NvDdkSeBlockDevFlushCache;
        pNewClient->BlockDev.NvDdkBlockDevIoctl = NvDdkSeBlockDevIoctl;

        *phBlockDevInstance = &pNewClient->BlockDev;
        /* Add New Client */
        s_gOpenHandleCount++;
        return NvSuccess;
    }
    else
        e = NvError_NotInitialized;

    return e;
}

NvError NvDdkSeBlockDevInit(NvRmDeviceHandle hRmDevice)
{
    if (s_gOpenHandleCount == 0)
        return NvDdkSeCoreInit(hRmDevice);
    return NvSuccess;
}

void NvDdkSeBlockDevDeinit(void)
{
    if (s_gOpenHandleCount == 0)
        NvDdkSeCoreDeInit();
}

