
/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* MetadataTraversal implementation */

#include "linux_common.h"

static XAresult IMetadataTraversal_SetMode(XAMetadataTraversalItf self, XAuint32 mode)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataTraversal_GetChildCount(XAMetadataTraversalItf self,
                                                 XAuint32 * pCount)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataTraversal_GetChildMIMETypeSize(XAMetadataTraversalItf self,
                                                        XAuint32 index,
                                                        XAuint32 * pSize)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataTraversal_GetChildInfo(XAMetadataTraversalItf self,
                                                XAuint32 index,
                                                XAint32 * pNodeID,
                                                XAuint32 * pType,
                                                XAuint32 size,
                                                XAchar * pMimeType)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataTraversal_SetActiveNode(XAMetadataTraversalItf self,
                                                 XAuint32 index)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static const struct XAMetadataTraversalItf_ IMetadataTraversal_Itf = {
    IMetadataTraversal_SetMode,
    IMetadataTraversal_GetChildCount,
    IMetadataTraversal_GetChildMIMETypeSize,
    IMetadataTraversal_GetChildInfo,
    IMetadataTraversal_SetActiveNode
};

void IMetadataTraversal_init(void* self)
{
    IMetadataTraversal* pMetaTraversal = (IMetadataTraversal*)self;
    pMetaTraversal->mItf = &IMetadataTraversal_Itf;
}

