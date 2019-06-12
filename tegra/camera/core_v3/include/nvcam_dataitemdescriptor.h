/*
* Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/


#ifndef INCLUDED_NVCAM_DATAITEM_DESCRIPTOR_H
#define INCLUDED_NVCAM_DATAITEM_DESCRIPTOR_H

    /*
     * NvCamDataItemDescriptors describe the available NvCamDataItems and
     * manipulates them.
     */
    typedef struct NvCamDataItemDescriptorRec *NvCamDataItemDescriptorHandle;

#include "nvcommon.h"
#include "nvos.h"
#include "nvcam_datainfo.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /*
     * NvCamCopyConstructorFunction is the function pointer to the function
     * implemented by the client when it wants to clone the NvCamDataItem.
     * It should set the value of the function pointer with the framework by
     * calling the function NvCamSetCopyConstructorFunction. This function is
     * used by the framework when the client wants to deep copy the entire
     * NvCamFrameInfo.
     */
    typedef NvError (*NvCamCopyConstructorFunction)(void **ValueDest,
                                                    void *ValueSrc);

    /*
     * NvCamDestructorFunction is the function pointer to the function
     * implemented by the client when it wants to destroy the data pointer of
     * the NvCamDataItem.
     * It should set the value of the function pointer with the framework by
     * calling the function NvCamSetDestructorFunction. The client should
     * implement the function and destroy all the memory allocated by it
     * depending upon the data type of the object represented by the
     * NvCamDataItem.
     * This function is called everytime when the NvCamDataItem is to be
     * destroyed. If the client does not want to use it, it should not call
     * NvCamSetDestructorFunction and the value of the function pointer remains
     * NULL with the framework. When the NvCamDataItem is to be destroyed in
     * this case the framework would do nothing with the data pointer of
     * NvCamDataItem. It would be responsibility of the client to destroy it.
     */
    typedef void (*NvCamDestructorFunction)(void *Value);

    /*
     * Creates an empty NvCamDataItemDescriptor.
     * The descriptor contents should be filled in before registration with
     * the NvCamDataInfo object.
     *
     * @param [out] hDescriptor : pointer to handle to NvCamDataItemDescriptor.
     * @retval NvSuccess on success and returns NvError_InsufficientMemory in
     *         Out Of Memory case.
     */
    NvError NvCamCreateDataItemDescriptor(
            NvCamDataItemDescriptorHandle *hDescriptor);

    /* Destroys the NvCamDataItemDescriptor.
     * It must not be registered with the NvCamDataInfo object when it is
     * destroyed.
     *
     * If the NvCamDataItemDescriptor is not unregistered before getting
     * destroyed then it gets tracked as a part of NvCamDataInfo and can cause
     * crash if it is referred by any NvCamDataItem or NvCamDataInfo. To avoid
     * this if the client does not unregister the descriptor and calls this
     * function the descriptor wont be destroyed and an error print would be
     * displayed.
     *
     * @param [in] hInfo : handle to NvCamDataInfo.
     * @param [in] hDescriptor : handle to NvCamDataItemDescriptor.
     * @retval void
     */
    void NvCamDestroyDataItemDescriptor( NvCamDataInfoHandle hInfo,
                   NvCamDataItemDescriptorHandle hDescriptor);

    /* Set methods */
    /*
     * The client will call this function to set the copyconstructor function
     * for a particular Descriptor with the framework. This function will be
     * used by the framework when the client wants to create a deep clone of
     * NvCamDataItem. If the CopyConstructor is not set and if the client calls
     * NvCamCloneFrameInfo for a deep clone NvError_InvalidState is returned.
     *
     * param [in] hDescriptor : handle to NvCamDataItemDescriptor.
     * param [in] NvCamCopyConstructorFunction : Copy Function used when client
     *                                 wants to create copy of an NvCamDataItem.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataItemDescriptorHandle is NULL or NvError_BadValue if the
     *         NvCamCopyConstructorFunction is NULL.
     */
    NvError NvCamSetCopyConstructorFunction(
            NvCamDataItemDescriptorHandle hDescriptor,
            NvCamCopyConstructorFunction CopyConstructor);

    /*
     * The client will call this function to set the destructor function
     * for a particular Descriptor with the framework. This function will be
     * used by the framework when the client wants to destroy a NvCamDataItem.
     *
     * If the client does not set this destructor function then it is client's
     * responsibility to destroy the data pointer associated with the
     * NvCamDataItem and the framework while destroying NvCamFrameInfo wont
     * destroy the data pointer of the NvCamDataItem.
     *
     * param [in] hDescriptor : handle to NvCamDataItemDescriptor.
     * param [in] NvCamDestructorFunction : Destructor Function used when client
     *                                wants to destroy copy of an NvCamDataItem.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataItemDescriptorHandle is NULL or NvError_BadValue if the
     *         NvCamDestructorFunction is NULL.
     */
    NvError NvCamSetDestructorFunction(NvCamDataItemDescriptorHandle hDescriptor,
                                           NvCamDestructorFunction Destructor);

    /*
     * Sets the name for a particular Descriptor.The framework will allocate its
     * own memory for Name and copy the user given Name into its memory
     * allocated.
     *
     * param [in] hDescriptor : handle to NvCamDataItemDescriptor.
     * param [in] Name : pointer to the string containing the Name of the
     *                  descriptor.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataItemDescriptorHandle is NULL or
     *         NvError_InsufficientMemory in Out Of Memory case or
     *         NvError_BadValue if the Name is NULL.
     */
    NvError NvCamSetDataItemDescriptorName(
            NvCamDataItemDescriptorHandle hDescriptor,
            const NvU8 *Name);

    /*
     * Sets the text describing a particular Descriptor. The framework will
     * allocate its own memory for Text and copy the user given DescText into
     * its memory allocated.
     *
     * param [in] hDescriptor : handle to NvCamDataItemDescriptor.
     * param [in] DescText : pointer to the string describing the descriptor.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataItemDescriptorHandle is NULL or
     *         NvError_InsufficientMemory in Out Of Memory case or
     *         NvError_BadValue if the DescText is NULL.
     */
    NvError NvCamSetDataItemDescriptorDescText(
            NvCamDataItemDescriptorHandle hDescriptor,
            const NvU8 *DescText);

    /* Get methods */

    /*
     * Gets the Id for the particular Descriptor.
     * The client/user should pass a valid NvCamDataItemDescriptorHandle
     * otherwise failure will occur.
     *
     * @param [in] hDescriptor : handle to NvCamDataItemDescriptor.
     * @retval Id of the descriptor. Returns zero in case of invalid
     *         NvCamDataItemDescriptorHandle.
     */
    NvU32 NvCamGetDataItemDescriptorId(
          NvCamDataItemDescriptorHandle hDescriptor);

    /*
     * Gets the pointer to the string with the name of particular Descriptor.
     * The client/user should pass a valid NvCamDataItemDescriptorHandle
     * otherwise failure will occur.
     *
     * The client/user should copy this name string with itself and it should
     * not try to free/corrupt the pointer returned by this function as the
     * memory for it is allocated by this framework and the framework would take
     * care of freeing it. If the client tries to free/corrupt it then the
     * framework would lose its value and won't be able to provide it when
     * queried for the next time.
     *
     * @param [in] hDescriptor : handle to NvCamDataItemDescriptor.
     * @retval pointer to the string describing name of the descriptor.
     *         Returns NULL in case of invalid NvCamDataItemDescriptorHandle.
     */
    NvU8 *NvCamGetDataItemDescriptorName(
          NvCamDataItemDescriptorHandle hDescriptor);

    /*
     * Gets the pointer to the string describing particular Descriptor.
     * The client/user should pass a valid NvCamDataItemDescriptorHandle
     * otherwise failure will occur.
     *
     * The client/user should copy this name string with itself and it should
     * not try to free/corrupt the pointer returned by this function as the
     * memory for it is allocated by this framework and the framework would take
     * care of freeing it.If the client tries to free/corrupt it then the
     * framework would lose its value and won't be able to provide it when
     * queried for the next time.
     *
     * @param [in] hDescriptor : handle to NvCamDataItemDescriptor.
     * @retval pointer to the string describing NvCamDataItemDescriptor.
     *         Returns NULL in case of invalid NvCamDataItemDescriptorHandle.
     */
    NvU8 *NvCamGetDataItemDescriptorDescText(
          NvCamDataItemDescriptorHandle hDescriptor);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVCAM_DATAITEM_DESCRIPTOR_H
