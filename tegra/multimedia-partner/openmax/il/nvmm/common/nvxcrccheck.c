/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <nvxcrccheck.h>
#include <md5.h>

NvU32 NvxCrcGetDigestLength(void)
{
    return 2*NV_MD5_OUTPUT_LENGTH + 1; 
}

void NvxCrcCalcBuffer( NvU8 *buffer, int len, char *out)
{
    int i;
    MD5Context md5Ctx;
    NvU8 digest[NV_MD5_OUTPUT_LENGTH];

    MD5Init(&md5Ctx);
    MD5Update(&md5Ctx, buffer, len);
    MD5Final(digest, &md5Ctx);

    // convert md5 to printable string
    #define INT2HEX(a) (a<=9)?((char)'0'+a):((char)'A'+a-10)
    for ( i=0 ; i<NV_MD5_OUTPUT_LENGTH ; i++ ) {
        out[2*i+0] = INT2HEX(digest[i]/16);
        out[2*i+1] = INT2HEX(digest[i]%16);
    }
    out[2*i] = 0; 
}

NvBool NvxCrcCalcNvMMSurface( NvxSurface *pSurface, char *out)
{
    // TO DO: Fill in
    if ( (pSurface->SurfaceCount == 3) && 
         (pSurface->Surfaces[0].ColorFormat == NvColorFormat_Y8) )
    {
        // YUV420 Planar
        return NvxCrcCalcYUV420Planar(pSurface, out);
    }
    else if (pSurface->SurfaceCount == 1)
    {
        return NvxCrcCalcRgbSurface(pSurface, out); 
    }

    out[0] = 0; 
    return NV_FALSE; 
}

NvBool NvxCrcCalcRgbSurface( NvxSurface *pSurface, char *out)
{
    int i;
    MD5Context md5Ctx;
    NvU8 digest[NV_MD5_OUTPUT_LENGTH];

    NvRmSurface *pSurf = &pSurface->Surfaces[0];
    NvU8 *pBuffer = NULL;
    NvU32 nBufSize = 0; 


    MD5Init(&md5Ctx);

    // Calc Y plane
    nBufSize = NvRmSurfaceComputeSize(pSurf);
    if (nBufSize == 0) 
        return NV_FALSE;
    pBuffer = NvOsAlloc(nBufSize);
    if(!pBuffer)
        return NV_FALSE; 
    NvRmSurfaceRead( pSurf, 0, 0, 
        pSurf->Width, pSurf->Height, pBuffer);
    MD5Update(&md5Ctx, pBuffer, nBufSize);
    NvOsFree(pBuffer);

    // Finalize MD5 hash and print out result
    MD5Final(digest, &md5Ctx);

    // convert md5 to printable string
    #define INT2HEX(a) (a<=9)?((char)'0'+a):((char)'A'+a-10)
    for ( i=0 ; i<NV_MD5_OUTPUT_LENGTH ; i++ ) {
        out[2*i+0] = INT2HEX(digest[i]/16);
        out[2*i+1] = INT2HEX(digest[i]%16);
    }
    out[2*i] = 0; 
    return NV_TRUE; 
}

NvBool NvxCrcCalcYUV420Planar( NvxSurface *pSurface, char *out)
{
    int i;
    MD5Context md5Ctx;
    NvU8 digest[NV_MD5_OUTPUT_LENGTH];

    NvRmSurface *pYSurf = &pSurface->Surfaces[0];
    NvRmSurface *pUSurf = &pSurface->Surfaces[1];
    NvRmSurface *pVSurf = &pSurface->Surfaces[2];
    NvU8 *pBuffer = NULL;
    NvU32 nBufSize = 0; 


    MD5Init(&md5Ctx);

    // Calc Y plane
    nBufSize = NvRmSurfaceComputeSize(pYSurf);
    if (nBufSize == 0) 
        return NV_FALSE;
    pBuffer = NvOsAlloc(nBufSize);
    if(!pBuffer)
        return NV_FALSE; 
    NvRmSurfaceRead( pYSurf, 0, 0, 
        pYSurf->Width, pYSurf->Height, pBuffer);
    MD5Update(&md5Ctx, pBuffer, nBufSize);
    NvOsFree(pBuffer);

    // Calc U plane
    nBufSize = NvRmSurfaceComputeSize(pUSurf);
    if (nBufSize == 0) 
        return NV_FALSE;
    pBuffer = NvOsAlloc(nBufSize);
    if(!pBuffer)
        return NV_FALSE; 
    NvRmSurfaceRead( pUSurf, 0, 0, 
        pUSurf->Width, pUSurf->Height, pBuffer);
    MD5Update(&md5Ctx, pBuffer, nBufSize);
    NvOsFree(pBuffer);

    // Calc V plane
    nBufSize = NvRmSurfaceComputeSize(pVSurf);
    if (nBufSize == 0) 
        return NV_FALSE;
    pBuffer = NvOsAlloc(nBufSize);
    if(!pBuffer)
        return NV_FALSE; 
    NvRmSurfaceRead( pVSurf, 0, 0, 
        pVSurf->Width, pVSurf->Height, pBuffer);
    MD5Update(&md5Ctx, pBuffer, nBufSize);
    NvOsFree(pBuffer);

    // Finalize MD5 hash and print out result
    MD5Final(digest, &md5Ctx);

    // convert md5 to printable string
    #define INT2HEX(a) (a<=9)?((char)'0'+a):((char)'A'+a-10)
    for ( i=0 ; i<NV_MD5_OUTPUT_LENGTH ; i++ ) {
        out[2*i+0] = INT2HEX(digest[i]/16);
        out[2*i+1] = INT2HEX(digest[i]%16);
    }
    out[2*i] = 0; 
    return NV_TRUE; 
}

