/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCMS_MATRIX_H
#define INCLUDED_NVCMS_MATRIX_H

#include "nvcms.h"

void NvCmsMatrixLoadIdentity(NvCmsMatrix* mx);
void NvCmsMatrixMult(NvCmsMatrix* res, const NvCmsMatrix* a, const NvCmsMatrix* b);
void NvCmsMatrixMultVector(NvCmsVector* res, const NvCmsMatrix* a, const NvCmsVector* b);
void NvCmsMatrixInvert(NvCmsMatrix* dst, const NvCmsMatrix* src);

#endif // INCLUDED_NVCMS_MATRIX_H
