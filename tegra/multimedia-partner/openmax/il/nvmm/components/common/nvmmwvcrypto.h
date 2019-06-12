/* Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifdef __cplusplus
extern "C" {
#endif
void NvMMCryptoInit(void);
void NvMMCryptoDeinit(void);
OMX_BOOL NvMMCryptoIsWidevine(OMX_EXTRADATATYPE eType);
void NvMMCryptoProcessVideoBuffer(void * inbuf, int inlen, void * outbuf, int outlen, void * cryptobuf);
void NvMMCryptoProcessAudioBuffer(void * inbuf, int inlen, void * outbuf, int outlen, void * cryptobuf);
#ifdef __cplusplus
}
#endif
