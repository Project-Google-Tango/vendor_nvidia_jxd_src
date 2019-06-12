/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef WV_CRYPTO_PLUGIN_H_

#define WV_CRYPTO_PLUGIN_H_
#include <media/hardware/CryptoAPI.h>
#include <utils/threads.h>
#include <openssl/aes.h>

#ifdef SECUREOS
#ifdef __cplusplus
extern "C" {
#endif

#include "tee_client_api.h"
#include "rsa_secure_protocol.h"

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif

namespace android {

struct NvCryptoPlugin : public CryptoPlugin {

    NvCryptoPlugin(const void *pKey, size_t keySize);
    virtual ~NvCryptoPlugin();

    virtual bool requiresSecureDecoderComponent(const char *mime) const;

    virtual ssize_t decrypt(
            bool secure,
            const uint8_t key[16],
            const uint8_t iv[16],
            Mode mode,
            const void *srcPtr,
            const SubSample *subSamples,
            size_t numSubSamples,
            void *dstPtr,
            AString *errorDetailMsg);

private:

    typedef struct
    {
        uint8_t size[4];
        uint8_t version[4];
        uint8_t portIndex[4];
        uint8_t eType[4];
        uint8_t dataSize[4];
        size_t encryptionOffset;
        size_t encryptionSize;
        uint8_t IVdata[16];
        uint8_t byteOffset[4];
    } NvExtraDataFormat;

    NvExtraDataFormat *pExtraDataFormat;

#ifdef SECUREOS
    TEEC_Context mSecureContext;
    TEEC_Session *mpSession;
    TEEC_Operation mSecureOperation;
#endif

    NvCryptoPlugin(const NvCryptoPlugin &);
    NvCryptoPlugin &operator=(const NvCryptoPlugin &);
};

struct WVCryptoPlugin : public CryptoPlugin {
    WVCryptoPlugin(const void *data, size_t size);
    virtual ~WVCryptoPlugin();

    const static size_t kAES128BlockSize = 16;

    status_t initCheck() const;

    virtual bool requiresSecureDecoderComponent(const char *mime) const;

    virtual ssize_t decrypt(
            bool secure,
            const uint8_t key[kAES128BlockSize],
            const uint8_t iv[kAES128BlockSize],
            Mode mode,
            const void *srcPtr,
            const SubSample *subSamples, size_t numSubSamples,
            void *dstPtr,
            AString *errorDetailMsg);

private:
    status_t decryptSW(const uint8_t *key, uint8_t *out, const uint8_t *in, size_t length);

    Mutex mLock;

    status_t mInitCheck;
    AES_KEY mAesKey;
    uint8_t mEncKey[kAES128BlockSize];

    WVCryptoPlugin(const WVCryptoPlugin &);
    WVCryptoPlugin &operator=(const WVCryptoPlugin &);
};

struct NvCryptoFactory : public CryptoFactory {

    static const uint8_t mNvCryptoUUID[16];
    static const uint8_t kUUIDWidevine[16];

    virtual bool isCryptoSchemeSupported(const uint8_t uuid[16]) const {
        ALOGV("\n%s[%d]", __FUNCTION__, __LINE__);
        return !(memcmp(uuid, mNvCryptoUUID, 16) && memcmp(uuid, kUUIDWidevine, 16));
    }

    virtual status_t createPlugin(
        const uint8_t uuid[16],
        const void *data,
        size_t size,
        CryptoPlugin **plugin);
};

}  // namespace android

#endif

