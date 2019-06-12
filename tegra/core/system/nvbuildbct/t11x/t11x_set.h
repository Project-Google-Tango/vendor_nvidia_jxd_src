/**
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 *  For setting the different parameters
 */


#ifndef INCLUDED_T11X_SET_H
#define INCLUDED_T11X_SET_H

#include "../nvbuildbct_config.h"
#include "../nvbuildbct.h"
#include "../parse.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Processes commands to set a bootloader.
 * @param Context The nvbuildbct context
 * @param Filename Bootloader filename
 * @param LoadAddress load adress for the bootloader
 * @param EntryPoint the bootloader entry point for execution

 * @retval Returns the '0'
 */
int
t11xSetBootLoader(BuildBctContext *Context,
                  NvU8 *Filename,
                  NvU32 LoadAddress,
                  NvU32 EntryPoint);

/**
 * Computation of the random aes block
 * @param Context The nvbuildbct context
 */
void t11xComputeRandomAesBlock(BuildBctContext *Context);

/**
 * Sets the aes random block based on type
 * @param Context The nvbuildbct context
 * @param value Aes Block Type
 * @param AesBlock pointer

 * @retval Returns the '0' or '1'
 */
int
t11xSetRandomAesBlock(BuildBctContext *Context, NvU32 Value, NvU8* AesBlock);

/**
 * Sets sdmmc parameters
 * @param Context The nvbuildbct context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetSdmmcParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Nand parameters
 * @param Context The nvbuildbct context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetNandParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Snor parameters
 * @param Context The nvbuildbct context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetSnorParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets SpiFlash parameters
 * @param Context The nvbuildbct context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetSpiFlashParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Usb3 parameters
 * @param Context The nvbuildbct context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetUsb3Param(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);


/**
 * Sets  Sets an SDRAM parameter.
 * @param Context The nvbuildbct context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetSdramParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Media emmc parameters
 * @param Context The nvbuildbct context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetOsMediaEmmcParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * Sets Media Nand Parameters
 * @param Context The nvbuildbct context
 * @param Index parameter index
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetOsMediaNandParam(BuildBctContext *Context,
                  NvU32              Index,
                  ParseToken         Token,
                  NvU32              Value);

/**
 * General handler for setting values in config files.
 * @param Context The nvbuildbct context
 * @param Token TokenNumber for the parameter
 * @param Value Value of the parameter

 * @retval Returns the '0' or '1'
 */
int
t11xSetValue(BuildBctContext *Context,
                  ParseToken Token,
                  NvU32 Value);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_SET_H */