/*
 * Copyright (c) 2006-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCOLOR_H
#define INCLUDED_NVCOLOR_H

#include <nvcommon.h>

/**
 * @file
 * <b> NVIDIA Color Definitions</b>
 *
 * @b Description: This file defines the interface for specifying color formats.
 */

/**
 * @defgroup nvcolor_group Color Specification Declarations
 *
 * We provide a very generic, orthogonal way to specify color formats.  There
 * are four steps in specifying a color format:
 * -# What is the data type of the color components?
 * -# How are the color components packed into words of memory?
 * -# How are those color components swizzled into an (x,y,z,w) color vector?
 * -# How is that vector interpreted as a color?
 *
 * These correspond to ::NvColorDataType, ::NvColorComponentPacking,
 * \c NV_COLOR_SWIZZLE_*, and ::NvColorSpace, respectively.
 *
 * First, you must understand the NVIDIA standard way of describing color
 * units (used in several business units within NVIDIA). Within a word, color
 * components are ordered from most-significant bit to least-significant bit.
 * Words are separated by underscores. For example:
 *
 * A8R8B8G8 = a single 32-bit word containing 8 bits alpha, 8 bits red, 8 bits
 * green, 8 bits blue.
 *
 * In little endian:
 * <pre>
  *                   Byte      3  ||   2  ||   1  ||   0
 *                    Bits 31                              0
 *                          AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB
 * </pre>
 *
 * In big endian:
  * <pre>
 *                    Byte      0  ||   1  ||   2  ||   3
 *                    Bits 31                              0
 *                          AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB
 * </pre>
 *
 * R8_G8_B8_A8 = four consecutive 8-bit words, consisting the red, green, blue,
 * and alpha components (in that order).
 *
 * In little endian:
 * <pre>
 *                    Byte      0  ||   1  ||   2  ||   3
 *                    Bits  76543210765432107654321076543210
 *                          RRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA
 * </pre>
 *
 * In big endian:
 * <pre>
 *                    Byte      0  ||   1  ||   2  ||   3
 *                    Bits  76543210765432107654321076543210
 *                          RRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA
 * </pre>
 *
 * R5G6B5 = a single 16-bit word containing 5 bits red, 6 bits green, 5 bits
 * blue.
 *
 * In little endian:
 * <pre>
 *                    Byte      1  ||   0
 *                    Bits 15              0
 *                          RRRRRGGGGGGBBBBB
 * </pre>
 *
 * In big endian:
 * <pre>
 *                    Byte      0  ||   1
 *                    Bits 15              0
 *                          RRRRRGGGGGGBBBBB
 * </pre>
 *
 * In cases where a word is less than 8 bits (e.g., an A1 1-bit alpha mask
 * bitmap), pixels are ordered from LSB to MSB within a word. That is, the LSB
 * of the byte is the pixel at x%8 == 0, while the MSB of the byte is the pixel
 * at x%8 == 7.
 *
 * @note Also note equivalences such as the following.
 *
 * In little endian: R8_G8_B8_A8 = A8B8G8R8.
 * In big endian:    R8_G8_B8_A8 = R8G8B8A8.
 *
 * Some YUV "422" formats have different formats for pixels whose X is even vs.
 * those whose X is odd. Every pixel contains a Y component, while (for
 * example) only even pixels might contain a U component and only odd pixels
 * might contain a V component. Such formats use a double-underscore to
 * separate the even pixels from the odd pixels. For example, the format just
 * described might be referred to as Y8_U8__Y8_V8.
 *
 * Here is how we would we go about mapping a color format (say, R5G6B5) to the
 * NvColorFormat enums.
 *
 * -# Remove the color information and rename the component R,G,B to generic
 *    names X,Y,Z. Our ::NvColorComponentPacking is therefore X5Y6Z5.
 * -# Pick the appropriate color space. This is plain old RGBA, so we pick
 *    ::NvColorSpace_LinearRGBA.
 * -# Determine what swizzle to use.  We need R=X, G=Y, B=Z, and A=1, so we
 *    pick the "XYZ1" swizzle.
 * -# Pick the data type of the color components. This is just plain integers,
 *    so ::NvColorDataType_Integer is our choice.
 *
 * @ingroup graphics_modules
 * @{
 */

/**
 * We provide a flexible way to map the input vector (x,y,z,w) to an output
 * vector (x',y',z',w'). Each output component can select any of the input
 * components or the constants zero or one.  For example, the swizzle "XXX1"
 * can be used to create a luminance pixel from the input x component, while
 * the swizzle "ZYXW" swaps the X and Z components (converts between RGBA and
 * BGRA).
 */
#define NV_COLOR_SWIZZLE_X 0
#define NV_COLOR_SWIZZLE_Y 1
#define NV_COLOR_SWIZZLE_Z 2
#define NV_COLOR_SWIZZLE_W 3
#define NV_COLOR_SWIZZLE_0 4
#define NV_COLOR_SWIZZLE_1 5

#define NV_COLOR_MAKE_SWIZZLE(x,y,z,w) \
    ((NV_COLOR_SWIZZLE_##x) | ((NV_COLOR_SWIZZLE_##y) << 3) | \
     ((NV_COLOR_SWIZZLE_##z) << 6) | ((NV_COLOR_SWIZZLE_##w) << 9))

#define NV_COLOR_SWIZZLE_GET_X(swz) (((swz)     ) & 7)
#define NV_COLOR_SWIZZLE_GET_Y(swz) (((swz) >> 3) & 7)
#define NV_COLOR_SWIZZLE_GET_Z(swz) (((swz) >> 6) & 7)
#define NV_COLOR_SWIZZLE_GET_W(swz) (((swz) >> 9) & 7)

#define NV_COLOR_SWIZZLE_XYZW NV_COLOR_MAKE_SWIZZLE(X,Y,Z,W)
#define NV_COLOR_SWIZZLE_ZYXW NV_COLOR_MAKE_SWIZZLE(Z,Y,X,W)
#define NV_COLOR_SWIZZLE_WZYX NV_COLOR_MAKE_SWIZZLE(W,Z,Y,X)
#define NV_COLOR_SWIZZLE_YZWX NV_COLOR_MAKE_SWIZZLE(Y,Z,W,X)
#define NV_COLOR_SWIZZLE_XYZ1 NV_COLOR_MAKE_SWIZZLE(X,Y,Z,1)
#define NV_COLOR_SWIZZLE_YZW1 NV_COLOR_MAKE_SWIZZLE(Y,Z,W,1)
#define NV_COLOR_SWIZZLE_XXX1 NV_COLOR_MAKE_SWIZZLE(X,X,X,1)
#define NV_COLOR_SWIZZLE_XZY1 NV_COLOR_MAKE_SWIZZLE(X,Z,Y,1)
#define NV_COLOR_SWIZZLE_ZYX1 NV_COLOR_MAKE_SWIZZLE(Z,Y,X,1)
#define NV_COLOR_SWIZZLE_WZY1 NV_COLOR_MAKE_SWIZZLE(W,Z,Y,1)
#define NV_COLOR_SWIZZLE_X000 NV_COLOR_MAKE_SWIZZLE(X,0,0,0)
#define NV_COLOR_SWIZZLE_0X00 NV_COLOR_MAKE_SWIZZLE(0,X,0,0)
#define NV_COLOR_SWIZZLE_00X0 NV_COLOR_MAKE_SWIZZLE(0,0,X,0)
#define NV_COLOR_SWIZZLE_000X NV_COLOR_MAKE_SWIZZLE(0,0,0,X)
#define NV_COLOR_SWIZZLE_0XY0 NV_COLOR_MAKE_SWIZZLE(0,X,Y,0)
#define NV_COLOR_SWIZZLE_XXXY NV_COLOR_MAKE_SWIZZLE(X,X,X,Y)
#define NV_COLOR_SWIZZLE_YYYX NV_COLOR_MAKE_SWIZZLE(Y,Y,Y,X)
#define NV_COLOR_SWIZZLE_0YX0 NV_COLOR_MAKE_SWIZZLE(0,Y,X,0)
#define NV_COLOR_SWIZZLE_X00Y NV_COLOR_MAKE_SWIZZLE(X,0,0,Y)
#define NV_COLOR_SWIZZLE_Y00X NV_COLOR_MAKE_SWIZZLE(Y,0,0,X)
#define NV_COLOR_SWIZZLE_X001 NV_COLOR_MAKE_SWIZZLE(X,0,0,1)
#define NV_COLOR_SWIZZLE_XY01 NV_COLOR_MAKE_SWIZZLE(X,Y,0,1)

/**
 * This macro extracts the number of bits per pixel out of an NvColorFormat or
 * NvColorComponentPacking.
 */
#define NV_COLOR_GET_BPP(fmt) (((NvU32)(fmt)) >> 24)

/**
 * This macro encodes the number of bits per pixel into an
 * NvColorComponentPacking enum.
 */
#define NV_COLOR_SET_BPP(bpp) ((bpp) << 24)

/**
 * NvColorComponentPacking enumerates the possible ways to pack color
 * components into words in memory.
 */
typedef enum
{
    NvColorComponentPacking_X1              = 0x01 | NV_COLOR_SET_BPP(1),
    NvColorComponentPacking_X2              = 0x02 | NV_COLOR_SET_BPP(2),
    NvColorComponentPacking_X4              = 0x03 | NV_COLOR_SET_BPP(4),
    NvColorComponentPacking_X8              = 0x04 | NV_COLOR_SET_BPP(8),
    NvColorComponentPacking_X3Y3Z2          = 0x05 | NV_COLOR_SET_BPP(8),
    NvColorComponentPacking_Y4X4            = 0x06 | NV_COLOR_SET_BPP(8),
    NvColorComponentPacking_X16             = 0x07 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X5Y5Z5W1        = 0x08 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X1Y5Z5W5        = 0x09 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X5Y6Z5          = 0x0A | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X8_Y8           = 0x0B | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X8_Y8__X8_Z8    = 0x0C | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_Y8_X8__Z8_X8    = 0x0D | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_Y6X10           = 0x0E | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_Y4X12           = 0x0F | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_Y2X14           = 0x10 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X4Y4Z4W4        = 0x11 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X8Y8            = 0x12 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X5Y5Z1W5        = 0x13 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X5Y1Z5W5        = 0x14 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X6Y5Z5          = 0x15 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X5Y5Z6          = 0x16 | NV_COLOR_SET_BPP(16),
    NvColorComponentPacking_X24             = 0x17 | NV_COLOR_SET_BPP(24),
    NvColorComponentPacking_X8_Y8_Z8        = 0x18 | NV_COLOR_SET_BPP(24),
    NvColorComponentPacking_X32             = 0x19 | NV_COLOR_SET_BPP(32),
    NvColorComponentPacking_X8Y8Z8W8        = 0x1A | NV_COLOR_SET_BPP(32),
    NvColorComponentPacking_X11Y11Z10       = 0x1B | NV_COLOR_SET_BPP(32),
    NvColorComponentPacking_X10Y11Z11       = 0x1C | NV_COLOR_SET_BPP(32),
    NvColorComponentPacking_X16Y16          = 0x1D | NV_COLOR_SET_BPP(32),
    NvColorComponentPacking_X16_Y16         = 0x1E | NV_COLOR_SET_BPP(32),
    NvColorComponentPacking_X10Y10Z10W2     = 0x1F | NV_COLOR_SET_BPP(32),
    NvColorComponentPacking_X2Y10Z10W10     = 0x20 | NV_COLOR_SET_BPP(32),
    NvColorComponentPacking_X16_Y16_Z16     = 0x21 | NV_COLOR_SET_BPP(48),
    NvColorComponentPacking_X16_Y16_Z16_W16 = 0x22 | NV_COLOR_SET_BPP(64),
    NvColorComponentPacking_X16Y16Z16W16    = 0x23 | NV_COLOR_SET_BPP(64),
    NvColorComponentPacking_X32_Y32         = 0x24 | NV_COLOR_SET_BPP(64),
    NvColorComponentPacking_X32_Y32_Z32     = 0x25 | NV_COLOR_SET_BPP(96),
    NvColorComponentPacking_X32_Y32_Z32_W32 = 0x26 | NV_COLOR_SET_BPP(128),
    NvColorComponentPacking_X32Y32Z32W32    = 0x27 | NV_COLOR_SET_BPP(128),

    NvColorComponentPacking_Force32 = 0x7FFFFFFF
} NvColorComponentPacking;

/**
 * NvColorDataType defines the data type of color components.
 *
 * The default datatype of color components is 'Integer' which should be used
 * when the color value is to be intepreted as an integer value ranging from 0
 * to the maximum value representable by the width of the components (as
 * specified by the packing of the component). Use 'Integer' also when the
 * interpretation of color value bits is not known, does not matter or is
 * context dependent.
 *
 * A data type of 'Signed' indicates that the color value is to be interpreted
 * as a signed two's complement integer value. The width of the component
 * includes the sign bit (e.g. S8 ranges from -128 to 127).
 *
 * A data type of 'Float' indicates that float values are stored in the
 * components of the color. The combination of data type 'Float' and the
 * bit width of the component packing defines the final data format of the
 * individual component.
 *
 * The list below defines the accepted combinations, when adding new
 * float formats please add an entry into this list.
 *
 * - DataType = Float, Component bit width = 32:
 *      A IEEE 754 single precision float (binary32) with 1 sign bit,
 *      8 exponent bits and 23 mantissa bits.
 *
 * - DataType = Float, Component bit width = 16:
 *      A IEEE 754 half precision float (binary16) with 1 sign bit,
 *      5 exponent bits and 10 mantissa bits.
 *
 * - DataType = Float, Component bit width = 10:
 *      An unsigned nvfloat with 5 exponent bits and 5 mantissa bits.
 *
 * - DataType = Float, Component bit width = 11:
 *      An unsigned nvfloat with 5 exponent bits and 6 mantissa bits.
 *
 */
typedef enum
{
    NvColorDataType_Integer     = 0x0,
    NvColorDataType_Float       = 0x1,
    NvColorDataType_Signed      = 0x2,

    NvColorDataType_Force32 = 0x7FFFFFFF
} NvColorDataType;

/**
 * NvColorSpace defines a number of ways of interpreting an (x,y,z,w) tuple as
 * a color.  The most common and basic is linear RGBA, which simply maps X->R,
 * Y->G, Z->B, and W->A, but various other color spaces also exist.
 *
 * Some future candidates for expansion are premultiplied alpha formats and
 * Z/stencil formats.  They have been omitted for now until there is a need.
 */
typedef enum
{
    /** Linear RGBA color space. */
    NvColorSpace_LinearRGBA = 1,

    /** Limited range linear RGBA color space. */
    NvColorSpace_LinearRGBALimited,

    /** sRGB color space with linear alpha. */
    NvColorSpace_sRGB,

    /** Paletted/color index color space. (data is meaningless w/o the palette)
     */
    NvColorSpace_ColorIndex,

    /** YCbCr ITU-R BT.601 color space for standard range luma (16-235) */
    NvColorSpace_YCbCr601,

    /** YCbCr ITU-R BT.601 color space with range reduced YCbCr for VC1 decoded
     *  surfaces. If picture layer of VC1 bit stream has RANGEREDFRM bit set,
     *  decoded YUV data has to be scaled up (range expanded).
     * For this type of surface, clients should range expand Y,Cb,Cr as follows:
     * - Y  = clip( (( Y-128)*2) + 128 );
     * - Cb = clip( ((Cb-128)*2) + 128 );
     * - Cr = clip( ((Cr-128)*2) + 128 );
     */
    NvColorSpace_YCbCr601_RR,

    /** YCbCr ITU-R BT.601 color space for extended range luma (0-255) */
    NvColorSpace_YCbCr601_ER,

    /** YCbCr ITU-R BT.709 color space. */
    NvColorSpace_YCbCr709,

    /**
     * Bayer format with the X component mapped to samples as follows.
     *  - span 1: R G R G R G R G
     *  - span 2: G B G B G B G B
     *
     * (Y,Z,W are discarded.)
     */
    NvColorSpace_BayerRGGB,

    /**
     * Bayer format with the X component mapped to samples as follows.
     *  - span 1: B G B G B G B G
     *  - span 2: G R G R G R G R
     *
     * (Y,Z,W are discarded.)
     */
    NvColorSpace_BayerBGGR,

    /**
     * Bayer format with the X component mapped to samples as follows.
     *  - span 1: G R G R G R G R
     *  - span 2: B G B G B G B G
     *
     * (Y,Z,W are discarded.)
     */
    NvColorSpace_BayerGRBG,

    /**
     * Bayer format with the X component mapped to samples as follows.
     *  - span 1: G B G B G B G B
     *  - span 2: R G R G R G R G
     *
     * (Y,Z,W are discarded.)
     */
    NvColorSpace_BayerGBRG,

    /** YCbCr ITU-R BT.709 color space for extended range luma (0-255). */
    NvColorSpace_YCbCr709_ER,

    /**
     * Noncolor data (for example depth, stencil, coverage).
     */
    NvColorSpace_NonColor,

    NvColorSpace_Force32 = 0x7FFFFFFF
} NvColorSpace;

/**
 * NV_COLOR_MAKE_FORMAT_XXX macros build NvColor values out of the
 * constituent parts.
 *
 * \c NV_COLOR_MAKE_FORMAT_GENERIC is the generic form that accepts
 * the ::NvColorDataType of the format as the fourth parameter.
 *
 * \c NV_COLOR_MAKE_FORMAT is used to build DataType = Integer formats.
 * This special case macro exists because integer formats are the
 * overwhelming majority and for retaining backwards compatibility with
 * code written before addition of \c NvColorDataType.
 */

#define NV_COLOR_MAKE_FORMAT_GENERIC(ColorSpace, Swizzle, ComponentPacking, DataType) \
    (((NvColorSpace_##ColorSpace) << 20) | \
     ((NV_COLOR_SWIZZLE_##Swizzle) << 8) | \
     ((NvColorDataType_##DataType) << 6) |  \
     (NvColorComponentPacking_##ComponentPacking))

#define NV_COLOR_MAKE_FORMAT(ColorSpace, Swizzle, ComponentPacking) \
    NV_COLOR_MAKE_FORMAT_GENERIC(ColorSpace, Swizzle, ComponentPacking, Integer)

#define NV_COLOR_GET_COLOR_SPACE(fmt) ((NvU32)(((fmt) >> 20) & 0xF))
#define NV_COLOR_GET_SWIZZLE(fmt) ((NvU32)(((fmt) >> 8) & 0xFFF))
#define NV_COLOR_GET_COMPONENT_PACKING(fmt) ((NvU32)((fmt) & 0xFF00003F))
#define NV_COLOR_GET_DATA_TYPE(fmt) ((NvU32)(((fmt) >> 6) & 0x3))

/**
 * Each value of \c NvColorFormat represents a way of laying out pixels in memory.
 * Some of the most common color formats are listed here, but other formats can
 * be constructed freely using \c NV_COLOR_MAKE_FORMAT, so you should generally
 * use \c NV_COLOR_GET_* to extract out the constituent parts of the color format
 * if if you want to provide fully general color format support. (There is no
 * requirement, of course, that any particular API must support all conceivable
 * color formats.)
 */
typedef enum
{
    /**
     * In some cases we don't know or don't care about the color format of a
     * block of data. This value can be used as a placeholder. It is
     * guaranteed that this value (zero) will never collide with any real color
     * format, based on the way that we construct color format enums.
     */
    NvColorFormat_Unspecified = 0,

    /// RGBA color formats.
    NvColorFormat_R3G3B2   = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZ1, X3Y3Z2),

    NvColorFormat_A4R4G4B4 = NV_COLOR_MAKE_FORMAT(LinearRGBA, YZWX, X4Y4Z4W4),
    NvColorFormat_R4G4B4A4 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZW, X4Y4Z4W4),
    NvColorFormat_A4B4G4R4 = NV_COLOR_MAKE_FORMAT(LinearRGBA, WZYX, X4Y4Z4W4),
    NvColorFormat_B4G4R4A4 = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYXW, X4Y4Z4W4),

    NvColorFormat_A1R5G5B5 = NV_COLOR_MAKE_FORMAT(LinearRGBA, YZWX, X1Y5Z5W5),
    NvColorFormat_R5G5B5A1 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZW, X5Y5Z5W1),
    NvColorFormat_A1B5G5R5 = NV_COLOR_MAKE_FORMAT(LinearRGBA, WZYX, X1Y5Z5W5),
    NvColorFormat_B5G5R5A1 = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYXW, X5Y5Z5W1),

    NvColorFormat_X1R5G5B5 = NV_COLOR_MAKE_FORMAT(LinearRGBA, YZW1, X1Y5Z5W5),
    NvColorFormat_R5G5B5X1 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZ1, X5Y5Z5W1),
    NvColorFormat_X1B5G5R5 = NV_COLOR_MAKE_FORMAT(LinearRGBA, WZY1, X1Y5Z5W5),
    NvColorFormat_B5G5R5X1 = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYX1, X5Y5Z5W1),

    NvColorFormat_A5R1G5B5 = NV_COLOR_MAKE_FORMAT(LinearRGBA, YZWX, X5Y1Z5W5),
    NvColorFormat_R1G5B5A5 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZW, X1Y5Z5W5),
    NvColorFormat_A5B5G5R1 = NV_COLOR_MAKE_FORMAT(LinearRGBA, WZYX, X5Y5Z5W1),
    NvColorFormat_B5G5R1A5 = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYXW, X5Y5Z1W5),

    NvColorFormat_R5G6B5   = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZ1, X5Y6Z5),
    NvColorFormat_B5G6R5   = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYX1, X5Y6Z5),

    NvColorFormat_R5G5B6   = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZ1, X5Y5Z6),
    NvColorFormat_B6G5R5   = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYX1, X6Y5Z5),

    NvColorFormat_R8_G8_B8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZ1, X8_Y8_Z8),
    NvColorFormat_B8_G8_R8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYX1, X8_Y8_Z8),
    NvColorFormat_A8R8G8B8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, YZWX, X8Y8Z8W8),
    NvColorFormat_A8B8G8R8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, WZYX, X8Y8Z8W8),
    NvColorFormat_R8G8B8A8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZW, X8Y8Z8W8),
    NvColorFormat_B8G8R8A8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYXW, X8Y8Z8W8),
    NvColorFormat_X8R8G8B8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, YZW1, X8Y8Z8W8),
    NvColorFormat_R8G8B8X8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZ1, X8Y8Z8W8),
    NvColorFormat_X8B8G8R8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, WZY1, X8Y8Z8W8),
    NvColorFormat_B8G8R8X8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYX1, X8Y8Z8W8),
    NvColorFormat_A8R8G8B8Limited = NV_COLOR_MAKE_FORMAT(LinearRGBALimited, YZWX, X8Y8Z8W8),
    NvColorFormat_A8B8G8R8Limited = NV_COLOR_MAKE_FORMAT(LinearRGBALimited, WZYX, X8Y8Z8W8),

    NvColorFormat_R10G10B10A2 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XYZW, X10Y10Z10W2),
    NvColorFormat_B10G10R10A2 = NV_COLOR_MAKE_FORMAT(LinearRGBA, ZYXW, X10Y10Z10W2),
    NvColorFormat_A2R10G10B10 = NV_COLOR_MAKE_FORMAT(LinearRGBA, YZWX, X2Y10Z10W10),
    NvColorFormat_A2B10G10R10 = NV_COLOR_MAKE_FORMAT(LinearRGBA, WZYX, X2Y10Z10W10),

    NvColorFormat_X16B16G16R16   = NV_COLOR_MAKE_FORMAT(LinearRGBA, WZY1, X16Y16Z16W16),

    NvColorFormat_Float_B10G11R11     = NV_COLOR_MAKE_FORMAT_GENERIC(LinearRGBA, ZYX1, X10Y11Z11, Float),
    NvColorFormat_Float_A16B16G16R16  = NV_COLOR_MAKE_FORMAT_GENERIC(LinearRGBA, WZYX, X16Y16Z16W16, Float),
    NvColorFormat_Float_X16B16G16R16  = NV_COLOR_MAKE_FORMAT_GENERIC(LinearRGBA, WZY1, X16Y16Z16W16, Float),

    // Single and dual channel color formats
    NvColorFormat_R8  = NV_COLOR_MAKE_FORMAT(LinearRGBA, X001, X8),
    NvColorFormat_RG8 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XY01, X8Y8),

    NvColorFormat_Float_R16    = NV_COLOR_MAKE_FORMAT_GENERIC(LinearRGBA, X001, X16, Float),
    NvColorFormat_Float_R16G16 = NV_COLOR_MAKE_FORMAT_GENERIC(LinearRGBA, XY01, X16Y16, Float),

    /// Luminance color formats.
    NvColorFormat_L1  = NV_COLOR_MAKE_FORMAT(LinearRGBA, XXX1, X1),
    NvColorFormat_L2  = NV_COLOR_MAKE_FORMAT(LinearRGBA, XXX1, X2),
    NvColorFormat_L4  = NV_COLOR_MAKE_FORMAT(LinearRGBA, XXX1, X4),
    NvColorFormat_L8  = NV_COLOR_MAKE_FORMAT(LinearRGBA, XXX1, X8),
    NvColorFormat_L16 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XXX1, X16),
    NvColorFormat_L32 = NV_COLOR_MAKE_FORMAT(LinearRGBA, XXX1, X32),

    NvColorFormat_A4L4          = NV_COLOR_MAKE_FORMAT(LinearRGBA, XXXY, Y4X4),
    NvColorFormat_L4A4          = NV_COLOR_MAKE_FORMAT(LinearRGBA, YYYX, Y4X4),
    NvColorFormat_A8L8          = NV_COLOR_MAKE_FORMAT(LinearRGBA, YYYX, X8Y8),
    NvColorFormat_L8A8          = NV_COLOR_MAKE_FORMAT(LinearRGBA, XXXY, X8Y8),
    NvColorFormat_Float_L16     = NV_COLOR_MAKE_FORMAT_GENERIC(LinearRGBA, XXX1, X16, Float),
    NvColorFormat_Float_A16L16  = NV_COLOR_MAKE_FORMAT_GENERIC(LinearRGBA, YYYX, X16Y16, Float),

    /// Alpha color formats.
    NvColorFormat_A1  = NV_COLOR_MAKE_FORMAT(LinearRGBA, 000X, X1),
    NvColorFormat_A2  = NV_COLOR_MAKE_FORMAT(LinearRGBA, 000X, X2),
    NvColorFormat_A4  = NV_COLOR_MAKE_FORMAT(LinearRGBA, 000X, X4),
    NvColorFormat_A8  = NV_COLOR_MAKE_FORMAT(LinearRGBA, 000X, X8),
    NvColorFormat_A16 = NV_COLOR_MAKE_FORMAT(LinearRGBA, 000X, X16),
    NvColorFormat_A32 = NV_COLOR_MAKE_FORMAT(LinearRGBA, 000X, X32),

    NvColorFormat_Float_A16 = NV_COLOR_MAKE_FORMAT_GENERIC(LinearRGBA, 000X, X16, Float),

    /// Color index formats.
    NvColorFormat_I1 = NV_COLOR_MAKE_FORMAT(ColorIndex, X000, X1),
    NvColorFormat_I2 = NV_COLOR_MAKE_FORMAT(ColorIndex, X000, X2),
    NvColorFormat_I4 = NV_COLOR_MAKE_FORMAT(ColorIndex, X000, X4),
    NvColorFormat_I8 = NV_COLOR_MAKE_FORMAT(ColorIndex, X000, X8),

    NvColorFormat_I4A4 = NV_COLOR_MAKE_FORMAT(ColorIndex, Y00X, Y4X4),
    NvColorFormat_A4I4 = NV_COLOR_MAKE_FORMAT(ColorIndex, X00Y, Y4X4),
    NvColorFormat_I8A8 = NV_COLOR_MAKE_FORMAT(ColorIndex, X00Y, X8Y8),
    NvColorFormat_A8I8 = NV_COLOR_MAKE_FORMAT(ColorIndex, Y00X, X8Y8),

    /// YUV interleaved color formats.
    NvColorFormat_Y8_U8_V8 = NV_COLOR_MAKE_FORMAT(YCbCr601, XYZ1, X8_Y8_Z8),
    NvColorFormat_A8Y8U8V8 = NV_COLOR_MAKE_FORMAT(YCbCr601, YZWX, X8Y8Z8W8),
    NvColorFormat_Y8U8V8A8 = NV_COLOR_MAKE_FORMAT(YCbCr601, XYZW, X8Y8Z8W8),
    NvColorFormat_UYVY     = NV_COLOR_MAKE_FORMAT(YCbCr601, XYZ1, Y8_X8__Z8_X8),
    NvColorFormat_VYUY     = NV_COLOR_MAKE_FORMAT(YCbCr601, XZY1, Y8_X8__Z8_X8),
    NvColorFormat_YUYV     = NV_COLOR_MAKE_FORMAT(YCbCr601, XYZ1, X8_Y8__X8_Z8),
    NvColorFormat_YVYU     = NV_COLOR_MAKE_FORMAT(YCbCr601, XZY1, X8_Y8__X8_Z8),

    /// YUV planar color formats.
    NvColorFormat_Y8       = NV_COLOR_MAKE_FORMAT(YCbCr601, X000, X8),
    NvColorFormat_U8       = NV_COLOR_MAKE_FORMAT(YCbCr601, 0X00, X8),
    NvColorFormat_V8       = NV_COLOR_MAKE_FORMAT(YCbCr601, 00X0, X8),
    NvColorFormat_U8_V8    = NV_COLOR_MAKE_FORMAT(YCbCr601, 0XY0, X8_Y8),
    NvColorFormat_V8_U8    = NV_COLOR_MAKE_FORMAT(YCbCr601, 0YX0, X8_Y8),

    /// Range Reduced YUV planar color formats.
    NvColorFormat_Y8_RR    = NV_COLOR_MAKE_FORMAT(YCbCr601_RR, X000, X8),
    NvColorFormat_U8_RR    = NV_COLOR_MAKE_FORMAT(YCbCr601_RR, 0X00, X8),
    NvColorFormat_V8_RR    = NV_COLOR_MAKE_FORMAT(YCbCr601_RR, 00X0, X8),
    NvColorFormat_U8_V8_RR = NV_COLOR_MAKE_FORMAT(YCbCr601_RR, 0XY0, X8_Y8),
    NvColorFormat_V8_U8_RR = NV_COLOR_MAKE_FORMAT(YCbCr601_RR, 0YX0, X8_Y8),

    /// BT.601 Extended Range YUV planar color formats.
    NvColorFormat_Y8_ER    = NV_COLOR_MAKE_FORMAT(YCbCr601_ER, X000, X8),
    NvColorFormat_U8_ER    = NV_COLOR_MAKE_FORMAT(YCbCr601_ER, 0X00, X8),
    NvColorFormat_V8_ER    = NV_COLOR_MAKE_FORMAT(YCbCr601_ER, 00X0, X8),
    NvColorFormat_U8_V8_ER = NV_COLOR_MAKE_FORMAT(YCbCr601_ER, 0XY0, X8_Y8),
    NvColorFormat_V8_U8_ER = NV_COLOR_MAKE_FORMAT(YCbCr601_ER, 0YX0, X8_Y8),

    /// BT.709 YUV planar color formats.
    NvColorFormat_Y8_709        = NV_COLOR_MAKE_FORMAT(YCbCr709, X000, X8),
    NvColorFormat_U8_709        = NV_COLOR_MAKE_FORMAT(YCbCr709, 0X00, X8),
    NvColorFormat_V8_709        = NV_COLOR_MAKE_FORMAT(YCbCr709, 00X0, X8),
    NvColorFormat_U8_V8_709     = NV_COLOR_MAKE_FORMAT(YCbCr709, 0XY0, X8_Y8),
    NvColorFormat_V8_U8_709     = NV_COLOR_MAKE_FORMAT(YCbCr709, 0YX0, X8_Y8),

    /// BT.709 Extended Range YUV planar color formats.
    NvColorFormat_Y8_709_ER     = NV_COLOR_MAKE_FORMAT(YCbCr709_ER, X000, X8),
    NvColorFormat_U8_709_ER     = NV_COLOR_MAKE_FORMAT(YCbCr709_ER, 0X00, X8),
    NvColorFormat_V8_709_ER     = NV_COLOR_MAKE_FORMAT(YCbCr709_ER, 00X0, X8),
    NvColorFormat_U8_V8_709_ER  = NV_COLOR_MAKE_FORMAT(YCbCr709_ER, 0XY0, X8_Y8),
    NvColorFormat_V8_U8_709_ER  = NV_COLOR_MAKE_FORMAT(YCbCr709_ER, 0YX0, X8_Y8),

    /// Bayer color formats.
    NvColorFormat_Bayer8RGGB    = NV_COLOR_MAKE_FORMAT(BayerRGGB, X000, X8),
    NvColorFormat_Bayer8BGGR    = NV_COLOR_MAKE_FORMAT(BayerBGGR, X000, X8),
    NvColorFormat_Bayer8GRBG    = NV_COLOR_MAKE_FORMAT(BayerGRBG, X000, X8),
    NvColorFormat_Bayer8GBRG    = NV_COLOR_MAKE_FORMAT(BayerGBRG, X000, X8),
    NvColorFormat_X6Bayer10RGGB = NV_COLOR_MAKE_FORMAT(BayerRGGB, X000, Y6X10),
    NvColorFormat_X6Bayer10BGGR = NV_COLOR_MAKE_FORMAT(BayerBGGR, X000, Y6X10),
    NvColorFormat_X6Bayer10GRBG = NV_COLOR_MAKE_FORMAT(BayerGRBG, X000, Y6X10),
    NvColorFormat_X6Bayer10GBRG = NV_COLOR_MAKE_FORMAT(BayerGBRG, X000, Y6X10),
    NvColorFormat_X4Bayer12RGGB = NV_COLOR_MAKE_FORMAT(BayerRGGB, X000, Y4X12),
    NvColorFormat_X4Bayer12BGGR = NV_COLOR_MAKE_FORMAT(BayerBGGR, X000, Y4X12),
    NvColorFormat_X4Bayer12GRBG = NV_COLOR_MAKE_FORMAT(BayerGRBG, X000, Y4X12),
    NvColorFormat_X4Bayer12GBRG = NV_COLOR_MAKE_FORMAT(BayerGBRG, X000, Y4X12),
    NvColorFormat_X2Bayer14RGGB = NV_COLOR_MAKE_FORMAT(BayerRGGB, X000, Y2X14),
    NvColorFormat_X2Bayer14BGGR = NV_COLOR_MAKE_FORMAT(BayerBGGR, X000, Y2X14),
    NvColorFormat_X2Bayer14GRBG = NV_COLOR_MAKE_FORMAT(BayerGRBG, X000, Y2X14),
    NvColorFormat_X2Bayer14GBRG = NV_COLOR_MAKE_FORMAT(BayerGBRG, X000, Y2X14),
    NvColorFormat_Bayer16RGGB   = NV_COLOR_MAKE_FORMAT(BayerRGGB, X000, X16),
    NvColorFormat_Bayer16BGGR   = NV_COLOR_MAKE_FORMAT(BayerBGGR, X000, X16),
    NvColorFormat_Bayer16GRBG   = NV_COLOR_MAKE_FORMAT(BayerGRBG, X000, X16),
    NvColorFormat_Bayer16GBRG   = NV_COLOR_MAKE_FORMAT(BayerGBRG, X000, X16),

    // 16-bit signed Bayer color formats
    // For these formats the black point is usually placed at 0 and the white
    // point at 2^14 in order to provide headroom/footroom
    NvColorFormat_BayerS16RGGB  = NV_COLOR_MAKE_FORMAT_GENERIC(BayerRGGB, X000, X16, Signed),
    NvColorFormat_BayerS16BGGR  = NV_COLOR_MAKE_FORMAT_GENERIC(BayerBGGR, X000, X16, Signed),
    NvColorFormat_BayerS16GRBG  = NV_COLOR_MAKE_FORMAT_GENERIC(BayerGRBG, X000, X16, Signed),
    NvColorFormat_BayerS16GBRG  = NV_COLOR_MAKE_FORMAT_GENERIC(BayerGBRG, X000, X16, Signed),

    /// Non-color formats. VCAA generic non-color formats used by depth and stencil.
    NvColorFormat_X4C4          = NV_COLOR_MAKE_FORMAT(NonColor, X000, Y4X4),
    NvColorFormat_NonColor8     = NV_COLOR_MAKE_FORMAT(NonColor, X000, X8),
    NvColorFormat_NonColor16    = NV_COLOR_MAKE_FORMAT(NonColor, X000, X16),
    NvColorFormat_NonColor24    = NV_COLOR_MAKE_FORMAT(NonColor, X000, X24),
    NvColorFormat_NonColor32    = NV_COLOR_MAKE_FORMAT(NonColor, X000, X32),

    NvColorFormat_Force32 = 0x7FFFFFFF
} NvColorFormat;

/**
 * Returns a string representation of the specified color format.
 */
static NV_INLINE const char *
NvColorFormatToString(NvColorFormat format)
{
    switch (format)
    {
        #define CASE(f) case NvColorFormat_##f: return #f
        CASE(Unspecified);
        CASE(R3G3B2);
        CASE(A4R4G4B4);
        CASE(R4G4B4A4);
        CASE(A4B4G4R4);
        CASE(B4G4R4A4);
        CASE(A1R5G5B5);
        CASE(R5G5B5A1);
        CASE(R5G6B5);
        CASE(B5G6R5);
        CASE(A1B5G5R5);
        CASE(B5G5R5A1);
        CASE(X1R5G5B5);
        CASE(R5G5B5X1);
        CASE(X1B5G5R5);
        CASE(B5G5R5X1);
        CASE(A5B5G5R1);
        CASE(A5R1G5B5);
        CASE(B5G5R1A5);
        CASE(R1G5B5A5);
        CASE(R5G5B6);
        CASE(B6G5R5);
        CASE(R10G10B10A2);
        CASE(B10G10R10A2);
        CASE(A2R10G10B10);
        CASE(A2B10G10R10);
        CASE(R8_G8_B8);
        CASE(B8_G8_R8);
        CASE(A8R8G8B8);
        CASE(A8R8G8B8Limited);
        CASE(A8B8G8R8);
        CASE(A8B8G8R8Limited);
        CASE(R8G8B8A8);
        CASE(B8G8R8A8);
        CASE(X8R8G8B8);
        CASE(R8G8B8X8);
        CASE(X8B8G8R8);
        CASE(B8G8R8X8);
        CASE(X16B16G16R16);
        CASE(Float_B10G11R11);
        CASE(Float_A16B16G16R16);
        CASE(Float_X16B16G16R16);
        CASE(R8);
        CASE(RG8);
        CASE(Float_R16);
        CASE(Float_R16G16);
        CASE(L1);
        CASE(L2);
        CASE(L4);
        CASE(L8);
        CASE(L16);
        CASE(L32);
        CASE(A4L4);
        CASE(L4A4);
        CASE(A8L8);
        CASE(L8A8);
        CASE(Float_L16);
        CASE(Float_A16L16);
        CASE(A1);
        CASE(A2);
        CASE(A4);
        CASE(A8);
        CASE(A16);
        CASE(A32);
        CASE(Float_A16);
        CASE(I1);
        CASE(I2);
        CASE(I4);
        CASE(I8);
        CASE(I4A4);
        CASE(A4I4);
        CASE(I8A8);
        CASE(A8I8);
        CASE(Y8_U8_V8);
        CASE(UYVY);
        CASE(VYUY);
        CASE(YUYV);
        CASE(YVYU);
        CASE(A8Y8U8V8);
        CASE(Y8U8V8A8);
        CASE(Y8);
        CASE(U8);
        CASE(V8);
        CASE(U8_V8);
        CASE(V8_U8);
        CASE(Y8_RR);
        CASE(U8_RR);
        CASE(V8_RR);
        CASE(U8_V8_RR);
        CASE(V8_U8_RR);
        CASE(Y8_ER);
        CASE(U8_ER);
        CASE(V8_ER);
        CASE(U8_V8_ER);
        CASE(V8_U8_ER);
        CASE(Y8_709);
        CASE(U8_709);
        CASE(V8_709);
        CASE(U8_V8_709);
        CASE(V8_U8_709);
        CASE(Y8_709_ER);
        CASE(U8_709_ER);
        CASE(V8_709_ER);
        CASE(U8_V8_709_ER);
        CASE(V8_U8_709_ER);
        CASE(Bayer8RGGB);
        CASE(Bayer8BGGR);
        CASE(Bayer8GRBG);
        CASE(Bayer8GBRG);
        CASE(X6Bayer10RGGB);
        CASE(X6Bayer10BGGR);
        CASE(X6Bayer10GRBG);
        CASE(X6Bayer10GBRG);
        CASE(X4Bayer12RGGB);
        CASE(X4Bayer12BGGR);
        CASE(X4Bayer12GRBG);
        CASE(X4Bayer12GBRG);
        CASE(X2Bayer14RGGB);
        CASE(X2Bayer14BGGR);
        CASE(X2Bayer14GRBG);
        CASE(X2Bayer14GBRG);
        CASE(Bayer16RGGB);
        CASE(Bayer16BGGR);
        CASE(Bayer16GRBG);
        CASE(Bayer16GBRG);
        CASE(BayerS16RGGB);
        CASE(BayerS16BGGR);
        CASE(BayerS16GRBG);
        CASE(BayerS16GBRG);
        CASE(X4C4);
        #undef CASE

        default:
            break;
    }

    return "custom";
}

/** @} */
#endif // INCLUDED_NVCOLOR_H
