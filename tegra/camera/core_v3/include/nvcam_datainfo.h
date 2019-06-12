/*
* Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/


#ifndef INCLUDED_NVCAM_DATA_INFO_H
#define INCLUDED_NVCAM_DATA_INFO_H

    /*
     * NvCamDataInfo is a container object that holds the DataItem
     * Descriptors which, in turn, describes the available Dataitem and
     * manipulates instances thereof.
     */
    typedef struct NvCamDataInfoRec *NvCamDataInfoHandle;

#include "nvcommon.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvcam_dataitemdescriptor.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /*
     * Creates an empty NvCamDataInfo object.
     *
     * @param [out] hInfo : pointer to handle to NvCamDataInfo.
     * @retval NvSuccess on success and returns NvError_InsufficientMemory in
     *         Out Of Memory case.
     */
    NvError NvCamCreateDataInfo(NvCamDataInfoHandle *hInfo);

    /*
     * Destroys the NvCamDataInfo object.
     * Also destroys all the registered NvCamDataItemDescriptors.
     *
     * @param [in] hInfo : handle to NvCamDataInfo.
     * @retval void
     */
    void NvCamDestroyDataInfo(NvCamDataInfoHandle hInfo);

    /*
     * Registers the NvCamDataItemDescriptor to the NvCamDataInfo.
     * Internally an unique ID will be assigned to each NvCamDataItemDescriptor
     * which can by obtained by calling the function
     * NvCamGetDataItemDescriptorId if it knows the Descriptor handle otherwise
     * it can first call NvCamGetDataItemDescriptorUsingName to get the handle.
     *
     * The client should make sure that it does not register two
     * NvCamDataItemDescriptors having the same name otherwise the error
     * NvError_BadValue would be returned.
     *
     * @param [in]  hInfo : handle to NvCamDataInfo.
     * @param [in]  hDescriptor : Handle to NvCamDataItemDescriptorInfo object
     *              already created.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataItemDescriptorHandle or NvCamDataInfoHandle is NULL or
     *         NvError_InsufficientMemory in Out Of Memory case or
     *         NvError_InvalidState in case if Name is not set before calling
     *         this function or NvError_BadValue if the NvCamDataItemDescriptor
     *         has a name which is already registered with the NvCamDataInfo.
     */
    NvError NvCamRegisterDataItemDescriptor(NvCamDataInfoHandle hInfo,
        NvCamDataItemDescriptorHandle hDescriptor);

    /*
     * Unregisters the NvCamDataItemDescriptor from the NvCamDataInfo.
     * Detaches the identification of the Descriptor from the NvCamDataInfo.
     *
     * If the client calls this function then it is its responsibility to
     * destroy it by calling NvCamDestroyDataItemDescriptor as NvCamDataInfo
     * loses track of it and would cause memory leakage.
     *
     * @param [in] hInfo : handle to NvCamDataInfo.
     * @param [in] hDescriptor : Handle to NvCamDataItemDescriptorInfo object
     *                           already created.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataItemDescriptorHandle or NvCamDataInfoHandle is NULL.
     */
    NvError NvCamUnRegisterDataItemDescriptor(NvCamDataInfoHandle hInfo,
        NvCamDataItemDescriptorHandle hDescriptor);

    /* Gets the handle of the NvCamDataItemDescriptor based on the Unique
     * ID associated with the descriptor. Where Unique ID is a unique number
     * generated when we register a descriptor with the NvCamDataInfo.
     *
     * @param [in] hInfo : handle to NvCamDataInfo.
     * @param [in] Id : Unique ID associated with each descriptor of
     *                  NvCamDataInfo.
     * @param [out] hDescriptor : pointer to NvCamDataItemDescriptorHandle
     *                            associated to the Unique ID.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataInfoHandle is NULL or NvError_BadValue if the Id is not
     *         registered with the NvCamDataInfo.
     */
    NvError NvCamGetDataItemDescriptorUsingId(
            NvCamDataInfoHandle hInfo,
            NvU32 Id,
            NvCamDataItemDescriptorHandle *hDescriptor);

    /* Gets the handle of the NvCamDataItemDescriptor based on the name
     * associated with the descriptor.
     *
     * @param [in] hInfo : handle to NvCamDataInfo.
     * @param [in] Name : Unique Name or String associated with the Descriptor.
     * @param [out] hDescriptor : pointer to NvCamDataItemDescriptorHandle
     *                            associated to the Unique Name.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataInfoHandle is NULL or NvError_BadValue if the Name is
     *         not registered with the NvCamDataInfo.
     */
    NvError NvCamGetDataItemDescriptorUsingName(
        NvCamDataInfoHandle hInfo,
        NvU8 *Name,
        NvCamDataItemDescriptorHandle *hDescriptor);


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVCAM_DATA_INFO_H
