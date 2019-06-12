/*
* Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/


#ifndef INCLUDED_NVCAM_FRAME_INFO_H
#define INCLUDED_NVCAM_FRAME_INFO_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvcam_datainfo.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /*
     * NvCamFrameInfo describes the container DataItem object that holds
     * number of NvCamDataItem for a frame.
     */
    typedef struct NvCamFrameInfoRec *NvCamFrameInfoHandle;

    /* Enum to decide what type of FrameClone is requested from the client*/
    typedef enum
    {
       /*
        * Attribute when the client wants to clone the entire FrameInfo using
        * the Copy Constructor Function. In this cloning the client allocates
        * memory and assign the values of the NvCamDataItem depending upon the
        * data type of the object represented by the NvCamDataItem using the
        * Copy Constructor function.
        */
        NvCamFrameInfoCloneType_Deep = 0x0, // index starts from 0

       /*
        * Attribute when the client wants to clone the entire FrameInfo by just
        * assigning the data pointer of the NvCamDataItems of the current frame
        * to the data pointer of the NvCamDataItems of the cloned frame.
        */
        NvCamFrameInfoCloneType_Shallow,

        NvCamFrameInfoCloneType_Force32 = 0x7FFFFFFF

    } NvCamFrameInfoCloneType;

    /*
     * Creates the handle to access the NvCamFrameInfo information for a frame.
     *
     * @param [in] hInfo : pointer to handle of NvCamDataInfo
     * @param [out] hFrame : pointer to handle of NvCamFrameInfo.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamDataInfoHandle is NULL or NvError_InsufficientMemory in
     *         Out Of Memory case.
     */
    NvError NvCamCreateFrameInfo(NvCamDataInfoHandle hInfo,
                                NvCamFrameInfoHandle *hFrame);

    /*
     * Destroys the NvCamFrameInfo for a particular Frame.
     * Destroys all the NvCamDataItems associated with the frame.
     * If some of the DataItems are still used by the client and are not in the
     * released state then these DataItems are not destroyed and also the
     * FrameInfo is not destroyed. When the release of these DataItems are
     * called then the framework again internally calls NvCamDestroyFrameInfo.
     * FrameInfo gets destroyed completely only when all the DataItems are in
     * the released state.
     *
     * @param [in] hFrame : handle to NvCamFrameInfo.
     * @retval void.
     */
    void NvCamDestroyFrameInfo(NvCamFrameInfoHandle hFrame);

    /*
     * Gets NvCamDataInfo handle to access the NvCamDataItemDescriptors and its
     * attributes like Descriptor Text, Descriptor Id etc and also to register/
     * unregister/destroy NvCamDataItemDescriptors.
     *
     *@param [in] hFrame : handle to NvCamFrameInfo
     *@param [out] hInfo : pointer to handle of NvCamDataInfo
     *@retval Returns NvSuccess on success and returns NvError_BadParameter
     *        if the NvCamFrameInfoHandle is NULL.
     */
    NvError NvCamGetDataInfo(NvCamFrameInfoHandle hFrame,
                            NvCamDataInfoHandle *hInfo);

    /*
     * Reset the values of all the NvCamDataItems for a frame to their
     * undefined state.
     *
     * @param [in] hFrame : handle to NvCamFrameInfo.
     * @retval Returns NvSuccess on success and returns NvError_BadParameter
     *         if the NvCamFrameInfoHandle is NULL.
     */
    NvError NvCamResetFrameInfo(NvCamFrameInfoHandle hFrame);

    /*
     * Creates a clone of the current NvCamFrameInfo along with the
     * NvCamDataItems associated with the current NvCamFrameInfo. The type of
     * the clone is specified by the NvCamFrameInfoCloneType. Currently only
     * DeepCopy is supported.
     *
     * @param [in] hFrame : handle to NvCamFrameInfo.
     * @param [in] CloneType : Type of FrameInfo Clone requested by the client.
     * @param [out] hFrameClone : pointer to handle of NvCamFrameInfo which is
     *                            cloned.
     * @retval Returns NvSuccess on success and returns NvError_BadParameter
     *         if the NvCamFrameInfoHandle is NULL or NvError_InsufficientMemory
     *         in Out Of Memory case or NvError_InvalidState when the client has
     *         requested for a deep clone and has not set any one of the Copy
     *         Constructor function of the NvCamDataItemDescriptor or
     *         NvError_NotImplemented if it has requested for a shallow copy of
     *         the frame and NvError_BadValue if the Clonetype is invalid.
     */
    NvError NvCamCloneFrameInfo(
        NvCamFrameInfoHandle hFrame,
        NvCamFrameInfoCloneType CloneType,
        NvCamFrameInfoHandle *hFrameClone);

    /*
     * Adds a NvCamDataItem of a particular Id to the NvCamFrameInfo and sets
     * its data reference. The client should call the function
     * NvCamReleaseDataItem after it has finished its usage with the
     * NvCamDataItem as the client has the pointer which means it might be using
     * it.
     *
     * @param [in] hFrame : Handle to the NvCamFrameInfo
     * @param [in] Id : The Unique Id of the descriptor
     * @param [in] DataReference : Pointer to the data associated with the
     *                             NvCamDataItem.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamFrameInfoHandle is NULL or NvError_InsufficientMemory in
     *         Out Of Memory case or NvError_InvalidState if a NvCamDataItem
     *         with the same id is already inserted in the NvCamFrameInfo.
     */
    NvError NvCamInsertDataItem(NvCamFrameInfoHandle hFrame, NvU32 Id,
                                void *DataReference);

    /*
     * Client can get the data pointer associated witha a particular
     * NvCamDataItem for a particualar frame. After the client has finished its
     * usage of the NvCamDataItem it must call the function NvCamReleaseDataItem
     *
     * @param [in] hFrame : handle to NvCamFrameInfo.
     * @param [in] Id : Unique Id for the item's descriptor.
     * @param [out] DataReference : Pointer to the handle of the data item to be
     *                              fetched.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamFrameInfoHandle is NULL or NvError_BadValue if the Id is not
     *         valid.
     */
    NvError NvCamGetDataItem(NvCamFrameInfoHandle hFrame,
        NvU32 Id,
        void **DataReference);

    /*
     * Client can call this function when it wants to retain the DataItem for a
     * longer time. This would ensure that even if NvCamDestroyFrameInfo is
     * called this particular NvCamDataItem would not be destroyed as the client
     * has requested for the NvCamDataItem to be retained. This NvCamDataItem
     * wont be destroyed until the client calls NvCamReleaseDataItem for this
     * particular NvCamDataItem.
     *
     * @param [in] hFrame : handle to NvCamFrameInfo.
     * @param [in] Id : Unique Id for the item's descriptor.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamFrameInfoHandle is NULL or NvError_BadValue if the Id is not
     *         valid.
     */
    NvError NvCamRetainDataItem(NvCamFrameInfoHandle hFrame, NvU32 Id);

    /*
     * The client needs to call this function when it has completed its usage
     * of NvCamDataItem. This is required so that the framework knows that this
     * particular client does not need the NvCamDataItem anymore.
     * If NvCamDestroyFrameInfo has already been called for this frame whose
     * NvCamDataItem is to be released then the function internally calls
     * NvCamDestroyFrameInfo and destroys the DataItem and if all the DataItems
     * are in the released state then it also destroys the entire frame.
     * If the DataItem is already in released state then NvError_InvalidState
     * would be returned.
     *
     * @param [in] hFrame : handle to NvCamFrameInfo.
     * @param [in] Id : Unique Id for the item's descriptor.
     * @retval NvSuccess on success and returns NvError_BadParameter if the
     *         NvCamFrameInfoHandle is NULL or NvError_BadValue if the Id is not
     *         valid or NvError_InvalidState if the DataItem is already in
     *         released state.
     */
    NvError NvCamReleaseDataItem(NvCamFrameInfoHandle hFrame,NvU32 Id);

    /*
     * Returns the validity status of the value for this NvCamDataItem.
     *
     * @param [in] hFrame : handle to NvCamFrameInfo.
     * @param [in] Id : Unique Id for the item's descriptor.
     * @retval NvSuccess on valid Id and returns NvError_BadParameter if the
     *         NvCamFrameInfoHandle is NULL or NvError_BadValue if the Id is not
     *         valid.
     */
    NvError NvCamIsDataItemValid(NvCamFrameInfoHandle hFrame, NvU32 Id);


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVCAM_FRAME_INFO_H
