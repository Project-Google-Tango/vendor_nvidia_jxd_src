/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef  INCLUDED_NV_ASN_PARSER_H
#define INCLUDED_NV_ASN_PARSER_H

/**
*   Universal Class Tags
*/
typedef enum{
    EOC                 = 0x0,
    BOOLEAN             = 0x1,
    INTEGER             = 0x2,
    BIT_STRING          = 0x3,
    OCTET_STRING        = 0x4,
    NULL_IDENTIFIER     = 0x5,
    OBJECT_IDENTIFIER   = 0x6,
    OBJECT_DESCRIPTOR   = 0x7,
    EXTERNAL            = 0x8,
    REAL                = 0x9,
    ENUMERATED          = 0xA,
    EMBEDDED_PDV        = 0xB,
    UTF8STRING          = 0xC,
    RELATIVE_OID        = 0xD,
    SEQUENCE            = 0x10,
    SET                 = 0x11,
    NUMERIC_STRING      = 0x12,
    PRINTABLE_STRING    = 0x13,
    T61STRING           = 0x14,
    VIDEOTEXSTRING      = 0x15,
    IA5STRING           = 0x16,
    UTCTIME             = 0x17,
    GENERALIZED_TIME    = 0x18,
    GRAPHICSTRING       = 0x19,
    VISIBLESTRING       = 0x1A,
    GENERALSTRING       = 0x1B,
    UNIVERSALSTRING     = 0x1C,
    CHARACTERSTRING     = 0x1D,
    BMPSTRING           = 0x1E,

    INVALID             = 0xFF
}NVUniversalClassTag;

/**
*   Class bits in an Identifier Octet
*/
typedef enum{
    UNI = 0x0,
    APP = 0x1,
    CTX = 0x2,
    PRI = 0x3
}NVIndentifierClass;

/**
*   Primitive/Constructed bits in an Identifier Octet
*/
typedef enum{
    PRIM = 0x0,
    CSTR = 0x1
}NVPrimitiveConstructed;

#endif /*  NV_ASN_PARSER_H  */
