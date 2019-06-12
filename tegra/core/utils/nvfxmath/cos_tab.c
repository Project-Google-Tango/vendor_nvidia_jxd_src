/*
 * Copyright (c) 2005 - 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

#ifdef _WIN32
float cosf(float _X) {return ((float)cos((double)_X)); }
#endif

#define DEG_STEP_SIZE (1.0f)
#define EPSILON (0.0001f)

int main()
{
    float deg, cos_deg;
    float biggestDeltaDegree = 0;
    int fxCos;
    int lastCos = -1;
    int biggestDelta = 0;

    printf("// cos table in 1.15 fixed point format in units of %f degrees\n",
            DEG_STEP_SIZE);
    printf("static const int NvSFx_cos_fx_table[] = {\n");

    for ( deg=0.0f ; deg<=90.0f+EPSILON ; deg+=DEG_STEP_SIZE ) {

        cos_deg = cosf((float)(deg * M_PI/180.0f));
        fxCos = (int)(cos_deg * 65536.0f + 1.5f) >> 1;

        printf("  %8d, // 0x%04x, cos(%2.f)=%f\n", fxCos, fxCos, deg, cos_deg);

        if ((biggestDelta < abs(lastCos - fxCos)) && (lastCos != -1)) {
            biggestDelta = abs(lastCos - fxCos);
            biggestDeltaDegree = deg;
        }
        lastCos = fxCos;
    }
    printf("};\n");

    printf("// biggest delta is %d [0x%x] at degree %f\n",
            biggestDelta,
            biggestDelta,
            biggestDeltaDegree);

    return 0;
}
