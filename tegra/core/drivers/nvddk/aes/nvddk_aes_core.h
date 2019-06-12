/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_AES_CORE_H
#define INCLUDED_NVDDK_AES_CORE_H

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Open the AES core engine.
 *
 * @param ppAesCoreEngine Pointer to a pointer that will hold new AesCoreEngine object.
 */
void AesCoreOpen(AesCoreEngine **ppAesCoreEngine);

/**
 * Close the AES core engine.
 *
 * @param pAes Pointer to the client argument.
 *
 * @retval None.
 */
void AesCoreClose(NvDdkAes *pAes);

/**
 * Initialize the AES core engine.
 *
 * @param hRmDevice RmDevice handle.
 *
 * @retval NvSuccess if successfully completed.
 */
NvError AesCoreInit(NvRmDeviceHandle hRmDevice);

/**
 * De-Initialize AES core engine.
 */
void AesCoreDeinit(void);

/**
 * Get AES engine initialization state.
 *
 * @retval NV_TRUE if engine has already been initialized, NV_FALSE otherwise.
 */
NvBool AesCoreInitIsDone(void);

/**
 * Power up the AES core engine.
 *
 * @param pAesCoreEngine Pointer to the AesCoreEngine argument.
 */
void AesCorePowerUp(AesCoreEngine *pAesCoreEngine);

/**
 * Power down the AES core engine.
 *
 * @param pAesCoreEngine Pointer to the AesCoreEngine argument.
 */
void AesCorePowerDown(AesCoreEngine *pAesCoreEngine);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVDDK_AES_CORE_H
