/*
 * Copyright (c) 2011 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and/or associated documentation files (the
 * "Materials "), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 * NVOMXAL_EGLExtensions.h - OpenMAX AL EGLStream DataLocator Extension v1.0
 *
 */

/**************************************************************************************/
/* NOTE: This file is a standard OpenMAX AL extension header file and should not be   */
/* modified in any way.                                                               */
/**************************************************************************************/

#ifndef _NVOMXAL_EGLExtensions_H_
#define _NVOMXAL_EGLExtensions_H_

#include <OMXAL/OpenMAXAL.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XA_DATALOCATOR_EGLSTREAM	(XAuint32) 0x0000000A

typedef struct XADataLocator_EGLStream_ {
    XAuint32 locatorType;
    const void * pEGLStream;
    const void * pEGLDisplay;
} XADataLocator_EGLStream;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NVOMXAL_EGLExtensions_H_ */
