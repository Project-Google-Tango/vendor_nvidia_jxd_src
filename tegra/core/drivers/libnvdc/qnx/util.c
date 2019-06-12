/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define _ISOC99_SOURCE
#include <math.h>
#include <nvdc.h>

ufixed_20_12_t pack_ufixed_20_12_f(double x)
{
    double integral, fractional = modf(x, &integral);
    return pack_ufixed_20_12((unsigned int)integral,
                             (unsigned int)(fractional * ((1 << 12) - 1)));
}
