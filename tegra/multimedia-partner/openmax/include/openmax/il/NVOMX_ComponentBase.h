/* Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
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
 * <b>NVIDIA Tegra: OpenMAX Component Base Interface</b>
 *
 */

/**
 * @defgroup nv_omx_il_comp_base Component Base Interface
 *   
 * This is the NVIDIA OpenMAX component base interface.
 *
 * @ingroup nvomx_custom 
 * @{
 */

#ifndef _NVOMX_ComponentBase_h_
#define _NVOMX_ComponentBase_h_

#include <OMX_Core.h>
#include <OMX_Component.h>

#define NVOMX_COMPONENT_MAX_PORTS 8 /** Max number of ports NVOMX_Component may have. */

/**
   Holds definition of a simple port abstraction used in NVOMX_Component.
 */
typedef struct NVOMX_Port 
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef; /**< Holds a pointer to the OMX port definition
                                                for this port. */
    OMX_AUDIO_PARAM_PCMMODETYPE  *pPCMMode; /**< Holds a pointer to the PCM description of this component, if pertinent. */
    OMX_BUFFERHEADERTYPE         *pCurrentBufferHdr; /**< Holds a pointer to the current
                                                         buffer on this port, if any. */
} NVOMX_Port;

/**
   Defines a simple OMX component template that abstracts most of the 
   complexity away from the user.

   To use:
   -# Call NVOMX_CreateComponent() from the OMX component's init functions to create this 
   structure.
   -# Call NVOMX_AddRole() to set what OMX roles this component will fill.
   -# Call NVOMX_InitPort() once for each port on this component to setup the ports.
   -# Fill in the function pointers in the NVOMX_Component structure as 
   appropriate -- at a minimum _NVOMX_Component::WorkerFunction and
   _NVOMX_Component::DeInit must point to valid functions. 
 */
typedef struct _NVOMX_Component NVOMX_Component;
/**
   Holds a simple OMX component.
 */
struct _NVOMX_Component
{
    OMX_PTR     pBase; /**< Internal pointer, do not touch. */

    OMX_U32     nPorts; /**< Holds the number of valid ports for this component. */
    NVOMX_Port  pPorts[NVOMX_COMPONENT_MAX_PORTS]; /**< Holds an array of port structures
                                                       for this component. */

    OMX_PTR     pComponentData; /**< An opaque pointer to any data this component desires 
                                    to keep track of. */

    /**
       Frees any remaining memory/resources
       allocated by the component.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @retval OMX_ERRORTYPE
           This generally should not have an error, but should return as appropriate.
     */
    OMX_ERRORTYPE (*DeInit)(NVOMX_Component *pComp);

    /**
       Called whenever an OMX_GetParameter has been done on the
       component.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @param [in] nIndex
           Specifies what type of configuration this is.
       @param [inout] pComponentParameterStructure
           Any data associated with this parameter call.
       @param [out] bHandled
           OMX_TRUE if this call was processed, otherwise OMX_FALSE.
       @retval OMX_ERRORTYPE
           Returns an appropriate error.
     */
    OMX_ERRORTYPE (*GetParameter)(NVOMX_Component *pComp, 
                                  OMX_INDEXTYPE nParamIndex, 
                                  OMX_PTR pComponentParameterStructure, 
                                  OMX_BOOL *bHandled);

    /**
       Called whenever an OMX_SetParameter has been done on the 
       component.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @param [in] nIndex
           Specifies what type of configuration this is.
       @param [in] pComponentParameterStructure
           Any data associated with this parameter call.
       @param [out] bHandled
           OMX_TRUE if this call was processed, otherwise OMX_FALSE.
       @retval OMX_ERRORTYPE
           Returns an appropriate error.
     */
    OMX_ERRORTYPE (*SetParameter)(NVOMX_Component *pComp, 
                                  OMX_INDEXTYPE nIndex,
                                  OMX_PTR pComponentParameterStructure,
                                  OMX_BOOL *bHandled);

    /**
       Called whenever an OMX_GetConfig has been done on the component.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @param [in] nIndex
           Specifies what type of configuration this is.
       @param [inout] pComponentConfigStructure
           Any data associated with this config call.
       @param [out] bHandled
           OMX_TRUE if this call was processed, otherwise OMX_FALSE.
       @retval OMX_ERRORTYPE
           Returns an appropriate error.
     */
    OMX_ERRORTYPE (*GetConfig)(NVOMX_Component *pComp,
                               OMX_INDEXTYPE nIndex,
                               OMX_PTR pComponentConfigStructure,
                               OMX_BOOL *bHandled);

    /**
       Called whenever an OMX_SetConfig has been done on the component.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @param [in] nIndex
           Specifies what type of configuration this is.
       @param [in] pComponentConfigStructure
           Any data associated with this config call.
       @param [out] bHandled
           OMX_TRUE if this call was processed, otherwise OMX_FALSE.
       @retval OMX_ERRORTYPE
           Returns an appropriate error.
     */
    OMX_ERRORTYPE (*SetConfig)(NVOMX_Component *pComp,
                               OMX_INDEXTYPE nIndex,
                               OMX_PTR pComponentConfigStructure,
                               OMX_BOOL *bHandled);

    /**
       Called whenever all ports of this component have a
       valid buffer and there needs to be work done to process them.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @param [out] pbMoreWork
           OMX_TRUE if there is still more work to be done on a given
           input buffer.
       @retval OMX_ERRORTYPE
           This generally should not fail, but returns an appropriate error type.
     */
    OMX_ERRORTYPE (*WorkerFunction)(NVOMX_Component *pComp,
                                    OMX_BOOL *pbMoreWork);

    /**
       Called on the transition to OMX_StateIdle (from Loaded).
       Allocates/acquires any resources in this function. 

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @retval OMX_ERRORTYPE
           This generally should not fail, but returns an appropriate error type.
       @retval OMX_ErrorNotReady
           If OMX_ErrorNotReady is returned, the component core will retry the 
           transtion periodically until it succeeds. If any other error than 
           OMX_ErrorNone is returned, the error will be sent back to the component.
     */
    OMX_ERRORTYPE (*AcquireResources)(NVOMX_Component *pComp);

    /**
       Called when this component needs to release its hold
       on any resources acquired by _NVOMX_Component::AcquireResources.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @retval OMX_ERRORTYPE
           This generally should not fail, but returns an appropriate error type.
     */
    OMX_ERRORTYPE (*ReleaseResources)(NVOMX_Component *pComp);

    /**
       Called when this component needs to flush.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @param [in] nPort
           Port to flush, or OMX_ALL for all.
       @retval OMX_ERRORTYPE
           This should not fail, but returns an appropriate error type.
      */
    OMX_ERRORTYPE (*Flush)(NVOMX_Component *pComp, OMX_U32 nPort);

    /**
       Called before a component is changing state for informative purposes.

       @param [in] pComp
           A pointer to the NVOMX_Component structure.
       @param [in] oCurState
           The current state we're in.
       @param [in] oNewState
           The new state we're transitioning to.
       @retval OMX_ERRORTYPE
           This should not fail, but returns an appropriate error type.
      */
    OMX_ERRORTYPE (*ChangeState)(NVOMX_Component *pComp, 
                                 OMX_STATETYPE oCurState,
                                 OMX_STATETYPE oNewState);
};

/** 
   Creates an NVOMX_Component.

   @param [in] hComponent
       A pointer to the OMX handle passed into the component's init function.
   @param [in] nPorts
       Specifies how many ports this component will have.
   @param [in] name
       The name of this component, must be unique
   @param [out] ppComp
       A pointer to the created NVOMX_Component; use this for calling any other
       NVOMX_Component functions.
   @retval OMX_ERRORTYPE
       Returns an appropriate error.
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_CreateComponent(
    OMX_HANDLETYPE hComponent,
    OMX_U32 nPorts,
    OMX_STRING name,
    NVOMX_Component **ppComp);

/**
   Adds a role name to this component.

   @param [in] pComp
       A pointer to the NVOMX_Component structure.
   @param [in] roleName
       A pointer to a static char string with the role name.
   @retval OMX_ERRORTYPE
       Returns an appropriate error.
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_AddRole(
    NVOMX_Component *pComp,
    OMX_STRING roleName);

/**
   Registers an index extension with this component.

   @param [in] pComp
       A pointer to the NVOMX_Component structure.
   @param [in] indexName
       A pointer to a static char string with the role name.
   @param [in] indexType
       The index to register
   @retval OMX_ERRORTYPE
       Returns an appropriate error.
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_AddExtensionIndex(
    NVOMX_Component *pComp,
    OMX_STRING indexName,
    OMX_INDEXTYPE indexType);

/**
   Creates a port structure.

   @param [in] pComp
       A pointer to the NVOMX_Component structure.
   @param [in] nPort
       Specifies the port number to create.
   @param [in] eDir
       Specifies the direction (input/output) of the port.
   @param [in] nBufferCount
       Specifies the minimum number of buffers to create on this port.
   @param [in] nBufferSize
       Specifies the minimum buffer size to create.
   @param [in] eDomain
       Specifies what port type (other, image, video, audio) this port will be.
   @param [in] pFormat
       A pointer to the format type of the port. May be one of type
       OMX_OTHER_FORMATTYPE, OMX_AUDIO_CODINGTYPE, OMX_VIDEO_CODINGTYPE, or
       OMX_IMAGE_CODINGTYPE.
   @retval OMX_ERRORTYPE
       Returns an appropriate error.
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_InitPort(
    NVOMX_Component *pComp,
    OMX_U32 nPort, 
    OMX_DIRTYPE eDir,
    OMX_U32 nBufferCount,
    OMX_U32 nBufferSize,
    OMX_PORTDOMAINTYPE eDomain,
    OMX_PTR pFormat);

/**
   Copies any buffer metadata (flags, marks, etc.) from one port to another.

   @param [in] pComp
       A pointer to the NVOMX_Component structure.
   @param [in] nSourcePort
       Specifies the source port from which to copy metadata.
   @param [in] nDestPort
       Specifies the port to which to copy metadata.
   @retval OMX_ERRORTYPE
       Returns an appropriate error.
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_CopyBufferMetadata(
    NVOMX_Component *pComp,
    OMX_U32 nSourcePort,
    OMX_U32 nDestPort);

/**
   Gets an empty buffer to an input port.
   The buffer will be queued for delivery either to the tunneled component
   associated with the given port, or back to the IL client via the
   EmptyBufferDone callback.

   @param [in] pComp
       A pointer to the NVOMX_Component structure.
   @param [in] nPort
       Specifies the input port in which to return the buffer.
   @param [in] pBuffer
       Specifies the buffer to return to the input port.
   @retval OMX_ERRORTYPE
       Returns an appropriate error.
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_ReleaseEmptyBuffer(
    NVOMX_Component *pComp,
    OMX_U32 nPort,
    OMX_BUFFERHEADERTYPE *pBuffer);

/**
   Delivers a full buffer to an output port.
   The buffer will be queued for delivery either to the tunneled component
   associated with the given port, or back to the IL client via the
   FillBufferDone callback.
   
   @param [in] pComp
       A pointer to the NVOMX_Component structure.
   @param [in] nPort
       Specifies the output port in which to deliver the buffer.
   @param [in] pBuffer
       Specifies the buffer to deliver to the output port.
   @retval OMX_ERRORTYPE
       Returns an appropriate error.
 */
OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_DeliverFullBuffer(
    NVOMX_Component *pComp,
    OMX_U32 nPort,
    OMX_BUFFERHEADERTYPE *pBuffer);

/** 
   Sends an audio master-time update to the clock.

   @param [in] pComp
       A pointer to the NVOMX_Component structure.
   @param [in] nPortClock
       The port number of the clock component.
   @param [in] nAudioTime
       The current audio time.
   @retval OMX_ERRORTYPE
       Returns an appropriate error.
 */

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_SendClockUpdate(
    NVOMX_Component *pComp,
    OMX_U32 nPortClock,
    OMX_TICKS nAudioTime);

#endif
/** @} */

