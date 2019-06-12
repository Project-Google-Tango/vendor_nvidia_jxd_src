/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahal3utils.h"
//#include <fstream>
#include "nv_log.h"

using namespace std;

namespace android {

void fillPatternInBuf(buffer_handle_t* pBuffer)
{
    buffer_handle_t handle = *pBuffer;
    const NvRmSurface *pSurf;
    size_t SurfCount;
    NvU32 width, height, pitch;
    NvU8 *pData = NULL;
    NvU32 patternW, patternH, sSize;
    NvU32 i;
    static int colorY = 255, colorUV = 128;

    colorY -= 13;
    colorY = (colorY < 0) ? 255 : colorY;
    colorUV += 7;
    colorUV = (colorUV > 255) ? 0 : colorUV;

    nvgr_get_surfaces(handle, &pSurf, &SurfCount);
    patternW = pSurf[0].Width;
    patternH = pSurf[0].Height;
    sSize = patternW * patternH;

    // Create a pattern
    pData = (NvU8 *)malloc(sSize);
    memset(pData, colorY, sSize);

    NV_LOGE("%s: Fill pattern to the preview buffer, \
           width = %u, height = %u, pitch = %u\n",
           __FUNCTION__, pSurf[0].Width, pSurf[0].Height,
           pSurf[0].Pitch);

    for (i = 0; i < SurfCount; i++)
    {
        if (i == 0)
        {
            NvRmSurfaceWrite((NvRmSurface *)&pSurf[i],
                            0, // x
                            0, // y
                            patternW, // width
                            patternH, // height
                            pData);
        }
        else
        {
            memset(pData, colorUV, sSize);
            NvRmSurfaceWrite((NvRmSurface *)&pSurf[i],
                            0, // x
                            0, // y
                            patternW / 2, // width
                            patternH / 2, // height
                            pData);
        }
    }

    free(pData);
#if 0
    // Test code, dump the modified data to a file
    pData = (NvU8 *)malloc(1.5 * pSurf[0].Width * pSurf[0].Height);
    NvU8 *ptr = pData;
    for (i = 0; i < SurfCount; i++)
    {
        NvRmSurfaceRead(&pSurf[i],
                        0, // x
                        0, // y
                        pSurf[i].Width, // width
                        pSurf[i].Height, // height
                        ptr);
        ptr += pSurf[i].Width * pSurf[i].Height;
    }

    size_t pDataSize = 1.5 * pSurf[0].Width * pSurf[0].Height;
    const char *dumpname = "/data/tmp/test.yuv";
    NV_LOGE("Dumping pattern to %s", dumpname);

    FILE* f = fopen(dumpname, "wt");
    fwrite(pData, sizeof(NvU8), pDataSize/sizeof(NvU8), f);
    fclose(f);


    free(pData);
#endif
}

#if 0
/*
 * 1. Needs to have a file /data/temp.jpg in the device
 *    for this function to work
 * 2. Gralloc needs to support BLOB format.
 */
void fillEncodedPattern(buffer_handle_t* pBuffer) {
    NV_LOGE("Filling Encoded Pattern++\n");
    ifstream::pos_type size;
    char* memblock;
    ifstream file ("/data/temp.jpg", ios::in | ios::binary | ios::ate);
    if(file.is_open()) {
        size = file.tellg();
        memblock = new char[size];
        file.seekg(0, ios::beg);
        file.read(memblock, size);
        file.close();
        /*
        for(int i=0; i<40; i++) {
            NV_LOGE("%x",memblock[i]);
        }*/

        const NvRmSurface *pSurf;

        nvgr_get_surfaces(*pBuffer, &pSurf, NULL);

        NvRmSurfaceWrite(&pSurf[0],
                            0, // x
                            0, // y
                            size, // width
                            1, // height
                            memblock);

        delete[] memblock;
    } else
        NV_LOGE("Unable to open file");

    NV_LOGE("Filling Encoded Pattern--\n");
}
#endif
} //namespace android
