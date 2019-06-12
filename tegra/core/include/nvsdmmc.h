/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVSDMMC_H
#define INCLUDED_NVSDMMC_H

#include "nvboot_error.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * Initlializes the sd controller and keeps the card in transfer state
 * Reads the EXT_CSD register and initializes the  sdmmc context
 * global structure.
 *
 * @retval NvError_Success Read operation is launched successfully.
 * @retval NvError_TimeOut Device is not responding.
 * @retval NvError_NotInitialized
 */
NvError NvNvBlSdmmcInit(void);


/**
 * It assumes that sd controller is initialized and the card is in transfer
 * state.  Reads the EXT_CSD register andinitializes the  sdmmc context
 * global structure.
 *
 * @retval NvError_Success Read operation is launched successfully.
 * @retval NvError_TimeOut Device is not responding.
 * @retval NvError_NotInitialized

 */
NvError NvNvBlSdmmcInit_Mini(void);


/**
 * Initiate the reading of a page of data into Dest.buffer.
 *
 * @param Block Block number to read from.
 * @param Page Page number in the block to read from.
 *          valid range is 0 <= Page < PagesPerBlock.
 * @param Dest Buffer to rad the data into.
 *
 * @retval NvError_Success Read operation is launched successfully.
 * @retval NvError_TimeOut Device is not responding.
 * @retval NvError_NotInitialized
 */
NvError NvNvBlSdmmcReadPage(        const NvU32 Block,
                                    const NvU32 Page,
                                    NvU8 *Dest);


/**
 * Initiate the reading of multiple pages of data into Dest.buffer
 *
 * @param Block Block number to read from.
 * @param Page Page number in the block to read from.
 *          valid range is 0 <= Page < PagesPerBlock.
 * @param PagesToRead Number of Pages to Read
            valid range is 0 <= Page < PagesPerBlock.
 * @param Dest Buffer to rad the data into.
 *
 * @retval NvError_Success Read operation is launched successfully.
 * @retval NvError_TimeOut Device is not responding.
 * @retval NvError_NotInitialized
 */
NvError
NvNvBlSdmmcReadMultiPage( const NvU32 Block,
                                  const NvU32 Page,
                                  const NvU32 PagesToRead,
                                  NvU8 *pBuffer);

/**
 * Waits for completion of previous Read[Multi]Page commands.
 */
NvError NvNvBlSdmmcWaitForIdle(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVSDMMC_H */
