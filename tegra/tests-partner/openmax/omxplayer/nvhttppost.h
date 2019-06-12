/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDE_HTTPPOST_H
#define INCLUDE_HTTPPOST_H

#include "nvos.h"
#include "nvmm.h"
#include "nvdrm.h"


/** NvHttpClient - This function posts a request to a license server
 *  of licenses that will ultimately be stored in the device license store.
 *
 *  @param pwszUrl  IN contains the license URL.
 *  @param cchUrl IN sizeof the URL.
 *  @param pszChallenge IN Challange query.
 *  @param cchChallenge IN Challange query size.
 *  @returns NvError_Success on success and status on failure
 */
NvError NvHttpClient(NvU16  *pwszUrl,NvU32 cchUrl, NvU8 *pszChallenge,NvU32 cchChallenge);

NvError NvPOSTCallBack(char * postResponse,NvS64 responseSize);


#endif // INCLUDE_HTTPOST_H
