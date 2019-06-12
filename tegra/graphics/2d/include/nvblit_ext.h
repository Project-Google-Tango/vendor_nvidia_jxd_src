/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVBLIT_EXT_H
#define INCLUDED_NVBLIT_EXT_H

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Defines extension flags for controlling behaviour of the ::NvBlit
 * function.
 * @see NvBlitSetFlag
 */
typedef enum
{
    /**
     * Extension flag to expose a path to the NvDdk2dBlitFlags_ReturnDstCrop
     * functionality of ddk2d. When enabled the destination crop is returned in
     * the NvBlitResult object.
     */
    NvBlitFlagExt_ReturnDstCrop = 1 << 31,

    NVBLIT_FORCE32(NvBlitFlagExt)
} NvBlitFlagExt;

/**
 * @brief Force a blit flag for all subsequent 2D operations.
 *
 * The specified flag blit is stored in the NvBlit context and applied to all
 * subsequent 2D operations. This is useful for enabling the flags for debug
 * features on a particular NvBlit context, such as tracing and surface dumping.
 *
 * An alternative method of doing this is setting a system property to enable
 * these debugging features, but this will affect all other modules using the
 * NvBlit API.
 *
 * @see NvBlitFlag
 */
void
NvBlitForceFlag(NvBlitContext *ctx,
                NvBlitFlag flag,
                NvBool enable);

/**
 * @brief Set the location for surface dumping.
 *
 * This sets the file system directory into which surface dumps will be written.
 *
 * See ::NVBLIT_CONFIG_DUMP for more information on surface dumping.
 */
void
NvBlitSetDumpPath(NvBlitContext *ctx,
                  const char *path);

/**
 * @brief Gets the destination offset
 *
 * Gets the destination offset of the blit, which must be applied to the
 * destination surface to compute where the blit output was written. This offset
 * will always be set to (0, 0) unless the client used the
 * ::NvBlitFlagExt_ReturnDstCrop flag.
 *
 * @see NvBlitResult, NvBlitFlagExt
 *
 * @param result  A pointer to the result object to query.
 * @param dstOffset  A pointer to where the destination offset will be returned.
 */
static NV_INLINE void
NvBlitGetDstOffset(NvBlitResult *result,
                   NvPoint *dstOffset)
{
    *dstOffset = result->DstOffset;
}

/**
 * @brief Controls backend engine used by NvBlit.
 * @ingroup config
 *
 * This sets the backend engine used for hardware operations. This has to be set
 * before the call to ::NvBlitOpen and will override the default engine
 * selection. Possible values are:
 *  - ddk2d
 *  - ddkvic
 */
#define NVBLIT_CONFIG_ENGINE  "nvblit.engine"

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVBLIT_EXT_H */
