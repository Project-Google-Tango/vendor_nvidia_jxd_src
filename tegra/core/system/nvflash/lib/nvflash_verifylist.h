/*
 * Copyright (c) 2008 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVFLASH_VERIFYLIST_H
#define INCLUDED_NVFLASH_VERIFYLIST_H

#include "nvcommon.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Initializes the list of partitions to verify
 *
 * @retval None
 */
void NvFlashVerifyListInitLstPartitions(void);

/**
 * Releases memory occupied by the list of partitions to verify
 *
 * @retval None
 */
void NvFlashVerifyListDeInitLstPartitions(void);

/**
 * Adds a partition id to the list of partitions.
 *
 * @param Partition Id/Partition name of partition to be added to the list for
 *    verification of corresponding partition data.
 *
 * @retval NvSuccess Addition of partition was successful
 * @retval NvError_InsufficientMemory if a list node could not be created for
     the partition id.
 */
NvError
NvFlashVerifyListAddPartition(const char* Partition);


/**
 * Returns if a partition is to be verified for data.
 *
 * @param PartitionId Id of partition that needs to be checked for existence in
     the list.
 *.@param PartitionName Name of partition that needs to be checked for existence in
     the list. Note that either the name or id will be passed at a time.
 *
 * @retval NvSuccess Partition Id was found in the list
 * @retval NvError_BadParameter if partition id was not found in the list
 */
NvError
NvFlashVerifyListIsPartitionToVerify(NvU32 PartitionId, const char* PartitionName);


/**
 * Returns the next partition id in the list
 *
 * @param SeekStart NV_TRUE indicates list iterator to be reset to the beginning
     of the list. NV_FALSE has no effect.
 * @param Partition Partition name/Id of partition that needs to be checked for existence in
     the list.
 *
 * @retval NvSuccess there's a valid partition id in the list.
 * @retval NvError_BadValue no more valid partition id or list is empty
     or end of listreached.
 */
NvError
NvFlashVerifyListGetNextPartition(char *pPartition, NvBool SeekStart);



#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVFLASH_COMMANDS_H
