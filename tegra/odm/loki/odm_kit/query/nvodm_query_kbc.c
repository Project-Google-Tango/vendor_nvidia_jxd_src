/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *                  ODM Kbc interface</b>
 *
 */

#include "nvodm_query_kbc.h"

static NvU32 *RowNumbers = NULL;
static NvU32 *ColNumbers = NULL;
static NvU32 s_key_code[3][3] = {
    {0,103,0},//scan code 103 defined for home button
    {0,114,0},
    {0,115,0}
};

void
NvOdmKbcGetParameter(
        NvOdmKbcParameter Param,
        NvU32 SizeOfValue,
        void * pValue)
{
    NvU32 *pTempVar;
    switch (Param)
    {
        case NvOdmKbcParameter_DebounceTime:
            pTempVar = (NvU32 *)pValue;
            *pTempVar = 10;
            break;
        case NvOdmKbcParameter_RepeatCycleTime:
            pTempVar = (NvU32 *)pValue;
            *pTempVar = 32;
            break;
        default:
            break;
    }
}

NvU32
NvOdmKbcGetKeyCode(
    NvU32 Row,
    NvU32 Column,
    NvU32 RowCount,
    NvU32 ColumnCount)
{
    if ((Row < 3) && (Column < 3))
        return s_key_code[Row][Column];

    return ((Row * ColumnCount) + Column);
}

NvBool
NvOdmKbcIsSelectKeysWkUpEnabled(
    NvU32 **pRowNumber,
    NvU32 **pColNumber,
    NvU32 *NumOfKeys)
{
    *pRowNumber = RowNumbers;
    *pColNumber = ColNumbers;
    *NumOfKeys = 0;
    return NV_FALSE;
}

