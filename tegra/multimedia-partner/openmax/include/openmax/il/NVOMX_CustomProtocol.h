/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/** 
 * @file
 * <b>NVIDIA Tegra: OpenMAX Custom Protocol Registration Interface</b>
 *
 */

/**
 * @defgroup nv_omx_il_protocol Custom Protocol Plug-in Interface
 *   
 * This interface is meant to allow registering of a custom set of file I/O 
 * operations that are then associated with a URI protocol. This can be used 
 * to implement various network streaming protocols or simple forms of 
 * full-file DRM decryption. See \c NVOMX_RegisterNVCustomProtoReader()
 * for how to register a protocol.
 *
 * This is essentially a simplified version of an OpenMAX ContentPipe.
 *
 * @ingroup nvomx_plugins
 * @{
 */

#ifndef NVOMX_CUSTOMPROTOCOL_H_
#define NVOMX_CUSTOMPROTOCOL_H_

#include "OMX_Types.h"
#include "OMX_ContentPipe.h"
#include "nvcustomprotocol.h"

/** Registers an NV_CUSTOM_PROTOCOL interface to handle protocol \a szProto. 
 *
 * Associates a protocol (eg, "drm://") with an ::NVMM_CUSTOM_PROTOCOL 
 * structure. Any files then opened with the protocol prefix will use the
 * functions in \a pProtocol for all file access.
 *
 * @param szProto   The protocol name to register, must include '://'
 * @param pProtocol A pointer to an NVMM_CUSTOM_PROTOCOL implementation.
 */
CPresult NVOMX_RegisterNVCustomProtocol(OMX_STRING szProto, 
                                        NV_CUSTOM_PROTOCOL *pProtocol);

#endif

/** @} */
/* File EOF */
