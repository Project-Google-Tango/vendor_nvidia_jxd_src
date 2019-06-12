/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifdef __cplusplus
extern "C" {
#endif

NvError nvcap_aac_enc_init(void *pGVar );

NvError
nvcap_aac_enc_run(
    void *pGVar,          /* pointer to the AAC ENC object */
    short *pBufInp,      /* input buffer samples */
    short *bufInpSize,   /* input buffer size */
    short *pBufOut,      /* output buffer - digit */
    short *bufOutSize    /* output buffer size */
    );

#ifdef __cplusplus
    }
#endif
