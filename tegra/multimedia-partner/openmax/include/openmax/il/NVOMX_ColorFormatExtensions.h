/* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
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
 * <b>NVIDIA Tegra: OpenMAX Color Format Extension Interface</b>
 */

/**
 * @defgroup nv_omx_il_index General Index
 *
 * This is the NVIDIA OpenMAX color format extensions interface.
 *
 * These extend custom formats.
 *
 * @ingroup nvomx_general_extension
 * @{
 */

#ifndef _NVOMX_ColorFormatExtensions_h_
#define _NVOMX_ColorFormatExtensions_h_

/** android opaque colorformat. Tells the encoder that
     * the actual colorformat will be  relayed by the
     * Gralloc Buffers
     */
#define OMX_COLOR_FormatAndroidOpaque  0x7F000789

/** Defines custom error extensions. */
typedef enum NVX_IMAGE_COLOR_FORMATTYPE {
    NVX_IMAGE_COLOR_FormatYV12 = OMX_COLOR_FormatVendorStartUnused + 1,
    NVX_IMAGE_COLOR_FormatNV21,
    NVX_IMAGE_COLOR_FormatInvalid = 0x7FFFFFFF
} NVX_IMAGE_COLOR_FORMATTYPE;


#endif

/** @} */

/* File EOF */
