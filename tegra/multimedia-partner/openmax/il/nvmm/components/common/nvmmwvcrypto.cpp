/* Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "OMX_Core.h"
#include "nvmmwvcrypto.h"
#include <pthread.h>

#include "media/openmax_1.2/OMX_CoreExt.h"
#include "media/CryptoInfo.h"
#include "OEMCrypto.h"
#include "tee_client_api.h"

pthread_mutex_t NvMMCryptoMutex;

void NvMMCryptoInit(void)
{
    NvOsDebugPrintf("NvMMCryptoInit+");
    OEMCrypto_Initialize();
    pthread_mutex_init(&NvMMCryptoMutex, NULL);
    NvOsDebugPrintf("NvMMCryptoInit-");
}

void NvMMCryptoDeinit(void)
{
    NvOsDebugPrintf("NvMMCryptoDeinit+");
    pthread_mutex_destroy(&NvMMCryptoMutex);
    OEMCrypto_Terminate();
    NvOsDebugPrintf("NvMMCryptoDeinit-");
}

OMX_BOOL NvMMCryptoIsWidevine(OMX_EXTRADATATYPE eType)
{
    NvOsDebugPrintf("NvMMCryptoIsWidevine+");
    if (eType == OMX_ExtraDataCryptoInfo)
    {
        // Google/Widevine AES
        return OMX_TRUE;
    }
    NvOsDebugPrintf("NvMMCryptoIsWidevine-");
    return OMX_FALSE;
}

void NvMMCryptoProcessVideoBuffer(void * inbuf, int inlen, void * outbuf, int outlen, void * cryptobuf)
{
    OEMCrypto_UINT32 outsize;
    OEMCrypto_UINT8 buf1[16];
    OEMCrypto_UINT8 buf2[16];
    OEMCrypto_UINT8 buf3[16];
    char * inbuffer = (char *)inbuf;
    char * outbuffer = (char *)outbuf;

    NvOsDebugPrintf("NvMMCryptoProcessVideoBuffer+");
    pthread_mutex_lock(&NvMMCryptoMutex);
    NvOsDebugPrintf("NvMMCryptoProcessVideoBuffer After Lock+");

    outsize = outlen;
    android::CryptoInfo *cryptoInfo = (android::CryptoInfo *)cryptobuf;
    if(cryptoInfo != NULL)
    {
        NvOsDebugPrintf("V cryptoinfo valid");
        NvOsDebugPrintf("V cryptoinfo mode %d", cryptoInfo->mMode);
        NvOsDebugPrintf("V cryptoinfo key %x %x %x %x", cryptoInfo->mKey[0],cryptoInfo->mKey[1],cryptoInfo->mKey[2],cryptoInfo->mKey[3]);
        NvOsDebugPrintf("V cryptoinfo id %x %x %x %x", cryptoInfo->mDrmId[0],cryptoInfo->mDrmId[1],cryptoInfo->mDrmId[2],cryptoInfo->mDrmId[3]);
        NvOsDebugPrintf("V cryptoinfo iv %x %x %x %x", cryptoInfo->mIv[0],cryptoInfo->mIv[1],cryptoInfo->mIv[2],cryptoInfo->mIv[3]);
        NvOsDebugPrintf("V cryptoinfo subsamples %d", cryptoInfo->mNumSubSamples);
        for(int i = 0; i < cryptoInfo->mNumSubSamples; i++)
            NvOsDebugPrintf("V cryptoinfo subsample clear %d encrypted %d", cryptoInfo->mSubSamples[i].mNumBytesOfClearData, cryptoInfo->mSubSamples[i].mNumBytesOfEncryptedData);

        NvOsDebugPrintf("V bufferinfo in %x, insize %d, out %x, outsize %d", inbuf, inlen, outbuf, outsize);
        NvOsDebugPrintf("V buf %x %x %x", buf1, buf2, buf3);
        NvOsDebugPrintf("V cryptoinfo inbuf bytes %x %x %x %x", inbuffer[0], inbuffer[1], inbuffer[2], inbuffer[3]);

        OEMCrypto_DecryptVideo(
                (const OEMCrypto_UINT8*)cryptoInfo->mIv,
                (const OEMCrypto_UINT8*)inbuf,
                (const OEMCrypto_UINT32)inlen,
                (OEMCrypto_UINT32)outbuf,
                0,
                (OEMCrypto_UINT32*)&outsize);

        NvOsDebugPrintf("V cryptoinfo insize %d, outsize %d", inlen, outsize);
        NvOsDebugPrintf("V cryptoinfo outbuf bytes %x %x %x %x", outbuffer[0], outbuffer[1], outbuffer[2], outbuffer[3]);
        NvOsDebugPrintf("V cryptoinfo outbuf bytes %x %x %x %x", outbuffer[16], outbuffer[17], outbuffer[18], outbuffer[19]);
        NvOsDebugPrintf("V cryptoinfo outbuf bytes %x %x %x %x", outbuffer[20], outbuffer[21], outbuffer[22], outbuffer[23]);
        NvOsDebugPrintf("V cryptoinfo outbuf bytes %x %x %x %x", outbuffer[24], outbuffer[25], outbuffer[26], outbuffer[27]);
        NvOsDebugPrintf("V cryptoinfo outbuf bytes %x %x %x %x", outbuffer[28], outbuffer[29], outbuffer[30], outbuffer[31]);
        NvOsDebugPrintf("V cryptoinfo outbuf bytes %x %x %x %x", outbuffer[32], outbuffer[33], outbuffer[34], outbuffer[35]);
        NvOsDebugPrintf("V cryptoinfo outbuf bytes %x %x %x %x", outbuffer[36], outbuffer[37], outbuffer[38], outbuffer[39]);
        NvOsDebugPrintf("V cryptoinfo outbuf bytes %x %x %x %x", outbuffer[40], outbuffer[41], outbuffer[42], outbuffer[43]);
    }

    NvOsDebugPrintf("NvMMCryptoProcessVideoBuffer Before Unlock-");
    pthread_mutex_unlock(&NvMMCryptoMutex);
    NvOsDebugPrintf("NvMMCryptoProcessVideoBuffer-");
}

void NvMMCryptoProcessAudioBuffer(void * inbuf, int inlen, void * outbuf, int outlen, void * cryptobuf)
{
    OEMCrypto_UINT32 outsize;
    OEMCrypto_UINT8 buf1[16];
    OEMCrypto_UINT8 buf2[16];
    OEMCrypto_UINT8 buf3[16];
    char * inbuffer = (char *)inbuf;
    char * outbuffer = (char *)outbuf;
    OEMCryptoResult res;
    void * tempin;
    void * tempout;

    NvOsDebugPrintf("NvMMCryptoProcessAudioBuffer+");
    pthread_mutex_lock(&NvMMCryptoMutex);
    NvOsDebugPrintf("NvMMCryptoProcessAudioBuffer After Lock+");

    outsize = outlen;
    android::CryptoInfo *cryptoInfo = (android::CryptoInfo *)cryptobuf;
    if(cryptoInfo != NULL)
    {

        NvOsDebugPrintf("A cryptoinfo valid");
        NvOsDebugPrintf("A cryptoinfo mode %d", cryptoInfo->mMode);
        NvOsDebugPrintf("A cryptoinfo key %x %x %x %x", cryptoInfo->mKey[0],cryptoInfo->mKey[1],cryptoInfo->mKey[2],cryptoInfo->mKey[3]);
        NvOsDebugPrintf("A cryptoinfo id %x %x %x %x", cryptoInfo->mDrmId[0],cryptoInfo->mDrmId[1],cryptoInfo->mDrmId[2],cryptoInfo->mDrmId[3]);
        NvOsDebugPrintf("A cryptoinfo iv %x %x %x %x", cryptoInfo->mIv[0],cryptoInfo->mIv[1],cryptoInfo->mIv[2],cryptoInfo->mIv[3]);
        NvOsDebugPrintf("A cryptoinfo subsamples %d", cryptoInfo->mNumSubSamples);
        for(int i = 0; i < cryptoInfo->mNumSubSamples; i++)
            NvOsDebugPrintf("A cryptoinfo subsample clear %d encrypted %d", cryptoInfo->mSubSamples[i].mNumBytesOfClearData, cryptoInfo->mSubSamples[i].mNumBytesOfEncryptedData);

        NvOsDebugPrintf("A bufferinfo in %x, insize %d, out %x, outsize %d", inbuf, inlen, outbuf, outsize);
//        NvOsDebugPrintf("A buf %x %x %x", buf1, buf2, buf3);
        NvOsDebugPrintf("A cryptoinfo inbuf bytes %x %x %x %x", inbuffer[0], inbuffer[1], inbuffer[2], inbuffer[3]);

        NvOsDebugPrintf("Encrypted size %d", inlen);

//        OEMCrypto_GetRandom(buf1, 16);
/*
        OEMCrypto_DecryptAudio(
                (const OEMCrypto_UINT8*)buf1,
                (const OEMCrypto_UINT8*)buf2,
                (const OEMCrypto_UINT32)16,
                (OEMCrypto_UINT8*)buf3,
                (OEMCrypto_UINT32*)&outsize);

static int startup = 1;
static void * tempbuf;

        if(startup)
            tempbuf = NvOsAlloc(1024);

        NvOsMemset(tempbuf, 0, 1024);
        outsize = 400;
        res = OEMCrypto_DecryptAudio(
                (const OEMCrypto_UINT8*)tempbuf,
                (const OEMCrypto_UINT8*)tempbuf,
                (const OEMCrypto_UINT32)outsize,
                (OEMCrypto_UINT8*)tempbuf,
                (OEMCrypto_UINT32*)&outsize);

//        tempin = NvOsAlloc(inlen);
//        tempout = NvOsAlloc(outsize);
//        NvOsMemcpy(tempin, inbuf, inlen);
//        NvOsMemcpy(tempout, outbuf, outsize);

//        res = OEMCrypto_DecryptAudio(
//                (const OEMCrypto_UINT8*)cryptoInfo->mIv,
//                (const OEMCrypto_UINT8*)tempin,
//                (const OEMCrypto_UINT32)inlen,
//                (OEMCrypto_UINT8*)tempout,
//                (OEMCrypto_UINT32*)&outsize);

//        NvOsMemcpy(outbuf, tempout, outsize);
//        NvOsFree(tempin);
//        NvOsFree(tempout);
*/
        res = OEMCrypto_DecryptAudio(
                (const OEMCrypto_UINT8*)cryptoInfo->mIv,
                (const OEMCrypto_UINT8*)inbuf,
                (const OEMCrypto_UINT32)inlen,
                (OEMCrypto_UINT8*)outbuf,
                (OEMCrypto_UINT32*)&outsize);

        if(res != TEEC_SUCCESS)
            NvOsDebugPrintf("Decrypt failure %x", res);

        NvOsDebugPrintf("A cryptoinfo insize %d, outsize %d", inlen, outsize);
        NvOsDebugPrintf("A cryptoinfo outbuf bytes %x %x %x %x", outbuffer[0], outbuffer[1], outbuffer[2], outbuffer[3]);

    }

/*
    void passDataToDecoder(uint8_t *data, size_t size, CryptoInfo *cryptoInfo)
    {
        CHECK(NULL != data);
        if (NULL != cryptoInfo && CryptoInfo::kMode_Unencrypted != cryptoInfo->mMode)
        {
            size_t offset = 0;
            for (size_t i = 0; i < cryptoInfo->mNumSubSamples; ++i)
            {
                size_t numBytesOfClearData = cryptoInfo->mSubSamples[i].mNumBytesOfClearData;
                size_t numBytesOfEncryptedData = cryptoInfo->mSubSamples[i].mNumBytesOfEncryptedData;
                if (0u < numBytesOfClearData) {
                    // passClearData() is a platform implementation that actually
                    // passes data to the platform and mark them as clear data.
                    passClearData(data + offset, numBytesOfClearData);
                    offset += numBytesOfClearData;
                }
                if (0u < numBytesOfEncryptedData) {
                    // passEncryptedData() is a platform implementation that
                    // passes data to the platform and mark them as encrypted
                    // data with necessary parameters.
                    // passEncryptedData() may use key and iv inside a CryptoInfo
                    // object to decode the data correctly.
                    passEncryptedData(data + offset, numBytesOfEncryptedData, cryptoInfo);
                    offset += numBytesOfEncryptedData;
                }
            }

            CHECK_EQ(offset, size);
        }
        else {
            passClearData(data, size);
        }
    }


    if(0)
        OEMCrypto_DecryptAudio(NULL, NULL, 0, NULL, 0);
*/
    NvOsDebugPrintf("NvMMCryptoProcessAudioBuffer Before Unlock-");
    pthread_mutex_unlock(&NvMMCryptoMutex);
    NvOsDebugPrintf("NvMMCryptoProcessAudioBuffer-");
}
