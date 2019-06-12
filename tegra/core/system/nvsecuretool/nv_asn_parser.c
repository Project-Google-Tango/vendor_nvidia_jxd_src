/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include "nvos.h"
#include "nv_asn_parser.h"
#include "nv_rsa.h"
#include "nv_cryptolib.h"


NvS32 NvGetPQFromPrivateKey(NvU8 *pBuf, NvU32 BufSize, NvU8 *P,
                            NvU32 PSize, NvU8 *Q, NvU32 QSize)
{
    NvS32 RetVal = -1;
    NvS32 i = 0;
    NvS32  LocalSize = 0;
    NvU32 Index = 0;
    NvU32 SequenceNum = 0;
    NvU32 IntegerNum = 0;
    NvU32 expo = 0;

    if ((pBuf == NULL) || (P == NULL) || Q == NULL)
    {
        goto fail;
    }

    while(Index <= BufSize)
    {
        switch(pBuf[Index++] & 0x1F)
        {
            case SEQUENCE:                      /*  SEQUENCE */
                LocalSize = 0;
                if(SequenceNum == 0)
                {   /*  Main sequence */
                    if( (pBuf[Index] & 0x80) == 0x80)
                    {
                        for(i = (pBuf[Index++] & 0x7F) - 1 ; i >= 0 ; i--)
                        {
                            LocalSize |= pBuf[Index++] << (8 * i);
                        }
                    }
                    else
                    {
                        LocalSize = (NvU32)pBuf[Index++];
                    }
                    if(LocalSize + Index != BufSize)
                    {
                        /* pk8 buffer size didn't match with the sequence size */
                        goto fail;
                    }

                }
                else
                {      /*  Just run through, we don't really need this */
                    if( (pBuf[Index] & 0x80) == 0x80)
                    {
                        for(i = (pBuf[Index++] & 0x7F) - 1 ; i >= 0 ; i--)
                        {
                            LocalSize |= pBuf[Index++] << (8 * i);
                        }
                    }
                    else
                    {
                        LocalSize = (NvU32)pBuf[Index++];
                    }
                    if(SequenceNum == 1)
                    {
                        Index += LocalSize;
                    }
                }

                SequenceNum++;
                break;

            case INTEGER:                       /*  INTEGER */
                LocalSize = 0;
                switch(IntegerNum)
                {
                    case 0:     /*  PrivateKeyInfo version  */
                    case 1:     /*  PrivateKey version  */
                    case 3:     /*  Modulus */
                        if( (pBuf[Index] & 0x80) == 0x80)
                        {
                            for(i = (pBuf[Index++]& 0x7F) - 1 ; i >= 0 ; i--)
                            {
                                LocalSize |= pBuf[Index++] << (8 * i);
                            }
                        }
                        else
                        {
                            LocalSize = (NvU32)pBuf[Index++];
                        }
                        Index += LocalSize;
                        IntegerNum++;
                        break;

                    case 2:     /*  Public Exponent */
                        if( (pBuf[Index] & 0x80) == 0x80)
                        {
                            for(i = (pBuf[Index++] & 0x7F) - 1 ; i >= 0 ; i--)
                            {
                                LocalSize |= pBuf[Index++] << (8 * i);
                            }
                        }
                        else
                        {
                            LocalSize = (NvU32)pBuf[Index++];
                        }
                        for(i = LocalSize-1 ; i >= 0 ; i--)
                        {
                            expo |= pBuf[Index++] << (8 * i);
                        }
                        if(expo != RSA_PUBLIC_EXPONENT_VAL)
                        {
                            goto fail;
                        }
                        IntegerNum++;
                        break;

                    case 4:     /*  P */
                        if( (pBuf[Index] & 0x80) == 0x80){
                            for(i = (pBuf[Index++] & 0x7F) - 1 ; i >= 0 ; i--)
                            {
                                LocalSize |= pBuf[Index++] << (8 * i);
                            }
                        }
                        else
                        {
                            LocalSize = (NvU32)pBuf[Index++];
                        }

                        if(LocalSize != (NvS32)PSize)
                        {
                            if(LocalSize-1 != (NvS32)PSize)
                            {
                                goto fail;
                            }
                            else
                            {
                                LocalSize -= 1;
                            }
                        }
                        for(i = 0,Index++;i < LocalSize; i++)
                        {
                            *(P++) = pBuf[Index++];
                        }
                        IntegerNum++;
                        break;

                    case 5:     /*  Q */
                        if( (pBuf[Index] & 0x80) == 0x80)
                        {
                            for(i = (pBuf[Index++] & 0x7F) - 1 ; i >= 0 ; i--)
                            {
                                LocalSize |= pBuf[Index++] << (8 * i);
                            }
                        }
                        else
                        {
                            LocalSize = (NvU32)pBuf[Index++];
                        }

                        if(LocalSize != (NvS32)QSize)
                        {
                            if(LocalSize-1 != (NvS32)QSize)
                            {
                                goto fail;
                            }
                            else
                            {
                                LocalSize -= 1;
                            }
                        }
                        for(i = 0,Index++;i < LocalSize; i++)
                        {
                            *(Q++) = pBuf[Index++];
                        }
                        IntegerNum++;

                        RetVal = 0;
                        goto fail;
                }
                break;

            case OCTET_STRING:                       /*  OCTET_STRING */
                if( (pBuf[Index] & 0x80) == 0x80)
                {
                    for(i = (pBuf[Index++] & 0x7F) - 1 ; i >= 0 ; i--)
                    {
                        LocalSize |= pBuf[Index++] << (8 * i);
                    }
                }
                else
                {
                    LocalSize = (NvU32)pBuf[Index++];
                }
                break;

            default:
                /*  Ideally control should not come here for a .pk8 file */
                goto fail;
        }
    }

fail:
    return RetVal;
}

