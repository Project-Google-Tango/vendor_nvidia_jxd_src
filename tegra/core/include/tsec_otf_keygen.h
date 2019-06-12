/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_TSEC_OTF_KEYGEN_H
#define INCLUDED_TSEC_OTF_KEYGEN_H

#if defined(__cplusplus)
extern "C"
{
#endif

#define UNENCRYPTED_OTF_KEY_SIZE_BITS (128)
#define ENCRYPTED_OTF_KEY_SIZE_BITS   (128)

#define UNENCRYPTED_OTF_KEY_SIZE_BYTES (16)
#define ENCRYPTED_OTF_KEY_SIZE_BYTES   (16)

#define OTF_COMBO_KEY_SIZE_BYTES (ENCRYPTED_OTF_KEY_SIZE_BYTES + \
                                  UNENCRYPTED_OTF_KEY_SIZE_BYTES)


typedef struct
{
    NvU8 EncryptedOtfKey[ENCRYPTED_OTF_KEY_SIZE_BYTES];

} NvOtfEncryptedKey;

/**
 * Initializes TSEC for the generation of OTF key
 *
 * @retval NvSuccess Indicates the above mentioned operation is done properly
 *                   and key can be collected from the supplied pointer
 * @retval NvError_BadParameter Indicates the supplied pointer is NULL.
 * @retval NvError_Timeout Indicates the operations are taking longer than usual.
 */
NvError
NvInitOtfKeyGeneration(void);

/**
 * Finalizes OTF key generation process.
 *
 * After TSEC generates two versions of the OTF key, one being encrypted
 * and the other being un-encrypted, this API loads the un-encrypted version
 * to the VDE-OTF key slot and also to PMC-OTF slots and lock them from being
 * red later. The Encrypted version is copied to the memory point to by the
 * input argument.
 *
 * @param pEncryptedOtfKey pointer to the Encrypted key
 *
 * @retval NvSuccess Indicates the above mentioned operation is done properly
 *                   and key can be collected from the supplied pointer
 * @retval NvError_BadParameter Indicates the supplied pointer is NULL.
 * @retval NvError_Timeout Indicates the operations are taking longer than usual.
 */
NvError
NvFinalizeOtfKeyGeneration(NvOtfEncryptedKey *pEncryptedOtfKey);

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_TSEC_OTF_KEYGEN_H

