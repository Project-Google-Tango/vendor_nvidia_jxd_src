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

/* MetadataInsertion implementation */


#include "linux_common.h"

static XAresult IMetadataInsertion_CreateChildNode(XAMetadataInsertionItf self,XAint32 parentNodeID,XAuint32 type,XAchar * pMimeType,XAint32 * pChildNodeID)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataInsertion_GetSupportedKeysCount(XAMetadataInsertionItf self,XAint32 nodeID,XAboolean * pFreeKeys,XAuint32 * pKeyCount,XAuint32 * pEncodingCount)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataInsertion_GetKeySize(XAMetadataInsertionItf self,XAint32 nodeID,XAuint32 keyIndex,XAuint32 * pKeySize)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataInsertion_GetKey(XAMetadataInsertionItf self,XAint32 nodeID,XAuint32 keyIndex,XAuint32 keySize,XAMetadataInfo * pKey)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataInsertion_GetFreeKeysEncoding(XAMetadataInsertionItf self,XAint32 nodeID,XAuint32 encodingIndex,XAuint32 * pEncoding)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataInsertion_InsertMetadataItem(XAMetadataInsertionItf self,XAint32 nodeID,XAMetadataInfo * pKey,XAMetadataInfo * pValue,XAboolean overwrite)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataInsertion_RegisterCallback(XAMetadataInsertionItf self,xaMetadataInsertionCallback callback,void * pContext)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static const struct XAMetadataInsertionItf_ IMetadataInsertion_Itf = {
    IMetadataInsertion_CreateChildNode,
    IMetadataInsertion_GetSupportedKeysCount,
    IMetadataInsertion_GetKeySize,
    IMetadataInsertion_GetKey,
    IMetadataInsertion_GetFreeKeysEncoding,
    IMetadataInsertion_InsertMetadataItem,
    IMetadataInsertion_RegisterCallback
};

void IMetadataInsertion_init(void* self)
{
    IMetadataInsertion* pIMetadataInsertion = (IMetadataInsertion*)self;
    pIMetadataInsertion->mItf = &IMetadataInsertion_Itf;
}
