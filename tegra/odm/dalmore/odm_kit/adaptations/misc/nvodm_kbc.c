/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file          Nvodm_Kbc.c
 * @brief         <b>KBC odm implementation</b>
 *
 * @Description : Implementation of the odm KBC API
 */
#include "nvodm_kbc.h"


#define KEYPAD_HAS_DIODES 1

NvU32
NvOdmKbcFilterKeys(
    NvU32 *pRows,
    NvU32 *pCols,
    NvU32 NumOfKeysPressed)
{

#if KEYPAD_HAS_DIODES
    return NumOfKeysPressed;
#else
    NvU32 i=0;
    NvU32 j=0;
    NvU32 k=0;
    NvU32 FilterKeys[2] = {0};
    NvBool IsFiltered = NV_FALSE;
    NvU32 NewKeyPressCount = NumOfKeysPressed;

    if (NumOfKeysPressed <= 3)
    {
        for (i=0; i<NumOfKeysPressed; i++)
        {
            for (j=(i+1); j<NumOfKeysPressed; j++)
            {
                if ((pRows[i]+1==pRows[j])||(pRows[j]+1==pRows[i]))
                {
                    for (k=j; i<(NumOfKeysPressed - 1); i++)
                    {
                        pRows[k] = pRows[k+1];
                        pCols[k] = pCols[k+1];
                    }
                    NumOfKeysPressed--;
                }
                if ((pCols[i]+1==pCols[j])||(pCols[j]+1==pCols[i]))
                {
                    for (k=j; i<(NumOfKeysPressed - 1); i++)
                    {
                        pRows[k] = pRows[k+1];
                        pCols[k] = pCols[k+1];
                    }
                    NumOfKeysPressed--;
                }
            }
        }
        return NumOfKeysPressed;
    }

    for (i=0; i<NumOfKeysPressed; i++)
    {
        for (j=(i+1); j<NumOfKeysPressed; j++)
        {
            if (pRows[i] == pRows[j])
            {
                for (k=0; k<NumOfKeysPressed; k++)
                {
                    if (k == i)
                        continue;

                    if(pCols[i] == pCols[k])
                    {
                        FilterKeys[0] = k;
                        IsFiltered = NV_TRUE;
                    }
                }
                for (k=0; k<NumOfKeysPressed; k++)
                {
                    if (k == j)
                        continue;

                    if (pCols[j] == pCols[k])
                    {
                        FilterKeys[1] = k;
                        IsFiltered = NV_TRUE;
                    }
                }
                goto end;
            }
        }
    }

    end:
        if (IsFiltered)
        {
            for (i=FilterKeys[0]; i<(NumOfKeysPressed - 1); i++)
            {
                pRows[i] = pRows[i+1];
                pCols[i] = pCols[i+1];
            }
            NewKeyPressCount--;
            for (i=FilterKeys[1]; i<(NumOfKeysPressed - 1); i++)
            {
                pRows[i] = pRows[i+1];
                pCols[i] = pCols[i+1];
            }
            NewKeyPressCount--;
        }
        NumOfKeysPressed = NewKeyPressCount;
    return NewKeyPressCount;
#endif

}


