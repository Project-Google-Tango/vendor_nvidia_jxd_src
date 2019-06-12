/* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
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
 * <b>NVIDIA Tegra: OpenMAX Component Register Interface</b>
 *
 */

/**
 * @defgroup nv_omx_il_comp_reg Component Register Interface
 *   
 * This is the NVIDIA OpenMAX component register interface.
 *
 * @ingroup nvomx_custom
 * @{
 */

#ifndef NVOMX_ComponentRegister_h
#define NVOMX_ComponentRegister_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <OMX_Core.h>

/** 
 * Dynamically registers a component with the core.
 * The core should return from this call in 20 msec.
 *
 * @pre OMX_Init()
 *
 * @param [in] pComponentReg A pointer to a component register
 *  structure. Both \c pName and \c pInitialize values of this structure
 *  shall be non-null. The \c pName value must be unique, otherwise an
 *  error is returned.
 * @retval OMX_ErrorNone If successful, or the appropriate OMX error code.
 * @ingroup core
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_RegisterComponent(
    OMX_IN  OMX_COMPONENTREGISTERTYPE *pComponentReg);

/** 
 * Deregisters a component with the core. 
 * A dynamically registered component is not required to be deregistered
 * with a call to this function. OMX_DeInit() will deregister
 * all components.
 * The core should return from this call in 20 msec.
 * 
 * @pre OMX_Init()
 *
 * @param [in] pComponentName The name of the component to deregister.
 * @retval OMX_ErrorNone If successful, or the appropriate OMX error code.
 * @ingroup core
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_DeRegisterComponent(
    OMX_IN OMX_STRING pComponentName);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
/** @} */
/* File EOF */

