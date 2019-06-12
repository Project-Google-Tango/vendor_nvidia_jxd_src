/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <media/stagefright/MediaErrors.h>
#include "NvCryptoPlugin.h"

//mergerd from wvcryptoplugin.cpp
#undef LOG_TAG
#define LOG_TAG "wv_crypto_plugin"

#include <cutils/properties.h>
#include <string.h>
#include <openssl/md5.h>

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/MediaErrors.h>
#ifdef REQUIRE_SECURE_BUFFERS
#include <OEMCrypto_L1.h>
#endif

android::CryptoFactory *createCryptoFactory() {
    ALOGV("\n%s[%d]", __FUNCTION__, __LINE__);
    return new android::NvCryptoFactory;
}

namespace android {

// static
const uint8_t NvCryptoFactory::mNvCryptoUUID[16] = {
    0x31, 0x70, 0x8f, 0x40, 0xe5, 0x20, 0x11, 0xe1,
    0x87, 0x91, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b
};

const uint8_t NvCryptoFactory::kUUIDWidevine[16] = {
    0xED,0xEF,0x8B,0xA9,0x79,0xD6,0x4A,0xCE,
    0xA3,0xC8,0x27,0xDC,0xD5,0x1D,0x21,0xED
};

status_t NvCryptoFactory::createPlugin(
        const uint8_t uuid[16],
        const void *data,
        size_t size,
        CryptoPlugin **plugin) {

    ALOGV("\n%s[%d]",__FUNCTION__, __LINE__);

    if(!memcmp(uuid, mNvCryptoUUID, 16)){
        *plugin = new NvCryptoPlugin(data, size);
        if (*plugin == NULL)
            return -ENOMEM;
    }else if(!memcmp(uuid, kUUIDWidevine, 16)){
        WVCryptoPlugin *wvPlugin = new WVCryptoPlugin(data, size);
        status_t err;
        if ((err = wvPlugin->initCheck()) != OK) {
            delete wvPlugin;
            wvPlugin = NULL;
            return err;
        }

        *plugin = wvPlugin;
    }else{
        return -ENOENT;
    }
    return OK;
}

NvCryptoPlugin::NvCryptoPlugin(const void *pKey, size_t keySize) {

    // Key should be of 128 byte
    if (pKey == NULL || keySize != 128)
        return;

    pExtraDataFormat  = (NvExtraDataFormat *)malloc(sizeof(NvExtraDataFormat));

    if (pExtraDataFormat) {
        memset(pExtraDataFormat, 0, sizeof(NvExtraDataFormat));
        pExtraDataFormat->size[0] = 0x30;
        pExtraDataFormat->version[0] = 0x01;
        pExtraDataFormat->eType[0] = 0x04;
        pExtraDataFormat->eType[1] = 0x00;
        pExtraDataFormat->eType[2] = 0xe0;
        pExtraDataFormat->eType[3] = 0x7f;
        pExtraDataFormat->dataSize[0] = 0x19;
    }

#ifdef SECUREOS
    ALOGV("\n%s[%d] Secure OS is defined", __FUNCTION__, __LINE__);

    TEEC_Result nTeeError;
    const TEEC_UUID *pServiceUUID;
    const TEEC_UUID SERVICE_UUID = SERVICE_RSA_UUID;

    nTeeError = TEEC_InitializeContext(NULL, &mSecureContext);
    ALOGV("TEEC_INIT = %x\n", nTeeError);
    if (nTeeError != TEEC_SUCCESS) {
        return;
    }

    pServiceUUID = &SERVICE_UUID;
    mpSession = (TEEC_Session *)malloc(sizeof(TEEC_Session));
    if (mpSession == NULL) {
        ALOGV("TEEC_Session Memory allocation failed");
        return;
    }

    mSecureOperation.paramTypes = TEEC_PARAM_TYPES(
        TEEC_NONE,
        TEEC_NONE,
        TEEC_NONE,
        TEEC_NONE);

    nTeeError = TEEC_OpenSession(
        &mSecureContext,
        mpSession,                 /* OUT session */
        pServiceUUID,              /* destination UUID */
        TEEC_LOGIN_PUBLIC,         /* connectionMethod */
        NULL,                      /* connectionData */
        &mSecureOperation,         /* IN OUT operation */
        NULL);                     /* OUT returnOrigin, optional */

    ALOGV("TEEC_OPEN = %x\n", nTeeError);
    if (nTeeError != TEEC_SUCCESS) {
        free(mpSession);
        mpSession = NULL;
        return;
    }

    mSecureOperation.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_NONE,
        TEEC_NONE,
        TEEC_NONE);
    mSecureOperation.params[0].tmpref.buffer = (char *)pKey;
    mSecureOperation.params[0].tmpref.size = (int)keySize;

    nTeeError = TEEC_InvokeCommand(
        mpSession,
        RSA_SET_AES_KEY,
        &mSecureOperation,       /* IN OUT operation */
        NULL);                   /* OUT returnOrigin, optional */

    ALOGV("TEEC_SETKEY = %x\n", nTeeError);
    if (nTeeError != TEEC_SUCCESS) {
        TEEC_CloseSession(mpSession);
        free(mpSession);
        mpSession = NULL;
        return;
    }
#else
    ALOGV("\n%s[%d]Secure OS is not defined", __FUNCTION__, __LINE__);
#endif
}

NvCryptoPlugin::~NvCryptoPlugin() {
#ifdef SECUREOS
    if (mpSession != NULL) {
        TEEC_CloseSession(mpSession);
        free(mpSession);
        mpSession = NULL;
        TEEC_FinalizeContext(&mSecureContext);
    }
#endif
    if (pExtraDataFormat)
        free(pExtraDataFormat);
}

bool NvCryptoPlugin::requiresSecureDecoderComponent(const char *mime) const {
    return false;
}

//Error Handling is pending like exception and null checks
ssize_t NvCryptoPlugin::decrypt(
        bool secure,
        const uint8_t key[16],
        const uint8_t iv[16],
        Mode mode,
        const void *srcPtr,
        const SubSample *subSamples,
        size_t numSubSamples,
        void *dstPtr,
        AString *errorDetailMsg) {

    ALOGV("\n%s[%d] numSubSamples:%d", __FUNCTION__, __LINE__, numSubSamples);

    size_t dataSize = 0;
    size_t i = 0;
    size_t prvSubSampleEnd = 0;
    bool containsEncrypted = false;

    if (numSubSamples > 1) {
        for (i = 0; i < numSubSamples - 1; i++) {
            dataSize += subSamples[i].mNumBytesOfClearData +
                subSamples[i].mNumBytesOfEncryptedData;
            // Check if any of subsample has encrypted data
            if (subSamples[i].mNumBytesOfEncryptedData)
                containsEncrypted = true;
        }
        // align encoded size to 4-byte boundary to put extradata
        dataSize = (dataSize + 3) & ~3;
    } else {
        dataSize += subSamples[0].mNumBytesOfClearData +
            subSamples[0].mNumBytesOfEncryptedData;
    }

    // srcPtr will always be greater than dataSize as allocated inside ICrypto
    memcpy(dstPtr, srcPtr, dataSize);

#ifdef SECUREOS
    if (containsEncrypted) {
        if (mpSession == NULL) {
            return -ENODEV;
        }

        uint8_t *pData = (uint8_t *)dstPtr + dataSize;

        for (i = 0; i < (numSubSamples - 1); i++) {
            if (subSamples[i].mNumBytesOfEncryptedData > 0) {
                pExtraDataFormat->encryptionSize =
                    subSamples[i].mNumBytesOfEncryptedData;
                pExtraDataFormat->encryptionOffset = prvSubSampleEnd +
                    subSamples[i].mNumBytesOfClearData;

                memcpy(pExtraDataFormat->IVdata, iv, 16);
                memcpy(pData, pExtraDataFormat, sizeof(NvExtraDataFormat));
                pData += sizeof(NvExtraDataFormat);
                prvSubSampleEnd += subSamples[i].mNumBytesOfClearData +
                    subSamples[i].mNumBytesOfEncryptedData;
            }
        }
    }
#endif

    return dataSize;
}

WVCryptoPlugin::WVCryptoPlugin(const void *data, size_t size)
    : mInitCheck(NO_INIT)
{
    // not using data at this time, require
    // size to be zero.
    if (size > 0) {
        mInitCheck = -EINVAL;
    } else {
        mInitCheck = OK;

#ifdef REQUIRE_SECURE_BUFFERS
        OEMCryptoResult res = OEMCrypto_Initialize();
        if (res != OEMCrypto_SUCCESS) {
            ALOGE("OEMCrypto_Initialize failed: %d", res);
            mInitCheck = -EINVAL;
        }
#endif
    }
}

WVCryptoPlugin::~WVCryptoPlugin() {

#ifdef REQUIRE_SECURE_BUFFERS
    if (mInitCheck == OK) {
        OEMCryptoResult res = OEMCrypto_Terminate();
        if (res != OEMCrypto_SUCCESS) {
            ALOGW("OEMCrypto_Terminate failed: %d", res);
        }
    }
#endif
}

status_t WVCryptoPlugin::initCheck() const {
    return mInitCheck;
}

bool WVCryptoPlugin::requiresSecureDecoderComponent(const char *mime) const {
#ifdef REQUIRE_SECURE_BUFFERS
    return !strncasecmp(mime, "video/", 6);
#else
    return false;
#endif
}

// Returns negative values for error code and
// positive values for the size of decrypted data, which can be larger
// than the input length.
ssize_t WVCryptoPlugin::decrypt(
        bool secure,
        const uint8_t key[16],
        const uint8_t iv[16],
        Mode mode,
        const void *srcPtr,
        const SubSample *subSamples, size_t numSubSamples,
        void *dstPtr,
        AString *errorDetailMsg) {
    Mutex::Autolock autoLock(mLock);


    CHECK(mode == kMode_Unencrypted || mode == kMode_AES_WV);

    size_t srcOffset = 0;
    size_t dstOffset = 0;
    for (size_t i = 0; i < numSubSamples; ++i) {
        const SubSample &ss = subSamples[i];

        size_t srcSize;

        if (mode == kMode_Unencrypted) {
            srcSize = ss.mNumBytesOfClearData;
            CHECK_EQ(ss.mNumBytesOfEncryptedData, 0u);
        } else {
            CHECK_EQ(ss.mNumBytesOfClearData, 0u);
            srcSize = ss.mNumBytesOfEncryptedData;
        }

        //ALOGD("size[%d]=%d", i, srcSize);
        if (srcSize == 0) {
            continue;   // segment size is zero, do not call decrypt
        }

#ifdef REQUIRE_SECURE_BUFFERS
        // decrypt using OEMCrypto API, used for L1 devices
        OEMCrypto_UINT32 dstSize = srcSize;

        OEMCryptoResult res;

        OEMCrypto_UINT8 _iv[16];
        const OEMCrypto_UINT8 *iv = NULL;

        if (mode != kMode_Unencrypted) {
            memset(_iv, 0, sizeof(_iv));
            iv = _iv;
        }

        if (secure) {
            //ALOGD("calling DecryptVideo, size=%d", srcSize);
            res = OEMCrypto_DecryptVideo(
                    iv,
                    (const OEMCrypto_UINT8 *)srcPtr + srcOffset,
                    srcSize,
                    (OEMCrypto_UINT32)dstPtr,
                    dstOffset,
                    &dstSize);
        } else {
            //ALOGD("calling DecryptAudio: size=%d", srcSize);
            res = OEMCrypto_DecryptAudio(
                    iv,
                    (const OEMCrypto_UINT8 *)srcPtr + srcOffset,
                    srcSize,
                    (OEMCrypto_UINT8 *)dstPtr + dstOffset,
                    &dstSize);
        }

        if (res != OEMCrypto_SUCCESS) {
            ALOGE("decrypt result: %d", res);
            return -EINVAL;
        }

        dstOffset += dstSize;
#else
        if (mode == kMode_Unencrypted) {
            memcpy((char *)dstPtr + dstOffset, (char *)srcPtr + srcOffset, srcSize);
        } else {
            status_t status = decryptSW(key, (uint8_t *)dstPtr + dstOffset,
                                        (const uint8_t *)srcPtr + srcOffset, srcSize);
            if (status != OK) {
                ALOGE("decryptSW returned %d", status);
                return status;
            }
        }

        dstOffset += srcSize;
#endif
        srcOffset += srcSize;
    } // for each subsample

    return static_cast<ssize_t>(dstOffset);
}

// SW AES CTS decrypt, used only for L3 devices
status_t WVCryptoPlugin::decryptSW(const uint8_t *key, uint8_t *out,
                                   const uint8_t *in, size_t length)
{
#ifndef REQUIRE_SECURE_BUFFERS
    unsigned char iv[kAES128BlockSize] = {0};

    if (memcmp(key, mEncKey, sizeof(mEncKey)) != 0) {
        // key has changed, recompute mAesKey from key
        uint8_t hash[MD5_DIGEST_LENGTH];
        char value[PROPERTY_VALUE_MAX] = {0};
        char seed[] = "34985woeirsdlkfjxc";

        property_get("ro.serialno", value, NULL);

        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, (uint8_t *)seed, sizeof(seed));
        MD5_Update(&ctx, (uint8_t *)value, strlen(value));
        MD5_Final(hash, &ctx);

        AES_KEY aesKey;
        if (AES_set_decrypt_key(hash, sizeof(hash) * 8, &aesKey) == 0) {
            uint8_t clearKey[kAES128BlockSize];
            AES_ecb_encrypt(key, clearKey, &aesKey, 0);

            if (AES_set_decrypt_key(clearKey, sizeof(hash) * 8, &mAesKey) == 0) {
                memcpy(mEncKey, key, sizeof(mEncKey));
            } else {
                return -EINVAL;
            }
        } else {
            return -EINVAL;
        }
    }

    size_t k, r = length % kAES128BlockSize;

    if (r) {
        k = length - r - kAES128BlockSize;
    } else {
        k = length;
    }

    AES_cbc_encrypt(in, out, k, &mAesKey, iv, 0);

    if (r) {
        // cipher text stealing - Schneier Figure 9.5 p 196
        unsigned char peniv[kAES128BlockSize] = {0};
        memcpy(peniv, in + k + kAES128BlockSize, r);

        AES_cbc_encrypt(in + k, out + k, kAES128BlockSize, &mAesKey, peniv, 0);

        // exchange the final plaintext and ciphertext
        for (size_t i = 0; i < r; i++) {
            *(out + k + kAES128BlockSize + i) = *(out + k + i);
            *(out + k + i) = *(in + k + kAES128BlockSize + i);
        }
        AES_cbc_encrypt(out + k, out + k, kAES128BlockSize, &mAesKey, iv, 0);
    }
#endif
    return OK;
}

}  // namespace android

