/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCMM_CONTEXT_H
#define INCLUDED_NVCMM_CONTEXT_H

#include "nvcms.h"
#include "nvcms_profile.h"
#include "nverror.h"

/* TODO [TaCo]: The profile spec is too limited. */
typedef struct NvCmsDeviceLinkProfileCacheSpecRec
{
    NvCmsAbsColorSpace inputColorSpace;
    NvCmsAbsColorSpace outputColorSpace;
    NvCmsProfileType   type;
} NvCmsDeviceLinkProfileCacheSpec;

typedef struct NvCmsDeviceLinkProfileCacheNodeRec NvCmsDeviceLinkProfileCacheNode;

typedef struct NvCmsDeviceLinkProfileCacheRec
{
    NvCmsDeviceLinkProfileCacheNode* nodes;
    NvCmsDeviceLinkProfileCacheNode* last;
} NvCmsDeviceLinkProfileCache;

void NvCmsDeviceLinkProfileCacheInit(NvCmsDeviceLinkProfileCache* cache);
void NvCmsDeviceLinkProfileCacheDeinit(NvCmsDeviceLinkProfileCache* cache);

NvCmsDeviceLinkProfile* NvCmsDeviceLinkProfileCacheGetProfile(NvCmsDeviceLinkProfileCache* cache,
                                                              NvCmsDeviceLinkProfileCacheSpec* spec);
NvError NvCmsDeviceLinkProfileCacheStoreProfile(NvCmsDeviceLinkProfileCache* cache, NvCmsDeviceLinkProfile* prof);

void NvCmsDeviceLinkProfileCacheMaint(NvCmsDeviceLinkProfileCache* cache);

struct NvCmsContextRec
{
    NvCmsDisplayProfile*            dispProf;
    NvCmsDeviceLinkProfileCache     cache;
};

/*
 * Core funcs
 */
NvError NvCmsContextCreate(NvCmsContext** ctx);
void NvCmsContextDestroy(NvCmsContext* ctx);

void NvCmsContextInit(NvCmsContext* res);
void NvCmsContextDeinit(NvCmsContext* ctx);

#endif // INCLUDED_NVCMM_CONTEXT_H
