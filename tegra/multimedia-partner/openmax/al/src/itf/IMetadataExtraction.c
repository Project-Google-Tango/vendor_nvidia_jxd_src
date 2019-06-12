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

/* MetadataExtraction implementation */

#include "linux_common.h"

static XAresult IMetadataExtraction_GetItemCount(XAMetadataExtractionItf self,
                                                 XAuint32 * pItemCount)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataExtraction_GetKeySize(XAMetadataExtractionItf self,
                                               XAuint32 index,
                                               XAuint32 * pKeySize)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}


static XAresult IMetadataExtraction_GetKey(XAMetadataExtractionItf self,
                                           XAuint32 index,
                                           XAuint32 keySize,
                                           XAMetadataInfo * pKey)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataExtraction_GetValueSize(XAMetadataExtractionItf self,
                                                 XAuint32 index,
                                                 XAuint32 * pValueSize)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataExtraction_GetValue(XAMetadataExtractionItf self,
                                             XAuint32 index,
                                             XAuint32 valueSize,
                                             XAMetadataInfo * pValue)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataExtraction_AddKeyFilter(XAMetadataExtractionItf self,
                                                 XAuint32 keySize,
                                                 const void * pKey,
                                                 XAuint32 keyEncoding,
                                                 const XAchar * pValueLangCountry,
                                                 XAuint32 valueEncoding,
                                                 XAuint8 filterMask)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult IMetadataExtraction_ClearKeyFilter(XAMetadataExtractionItf self)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static const struct XAMetadataExtractionItf_ IMetadataExtraction_Itf = {
    IMetadataExtraction_GetItemCount,
    IMetadataExtraction_GetKeySize,
    IMetadataExtraction_GetKey,
    IMetadataExtraction_GetValueSize,
    IMetadataExtraction_GetValue,
    IMetadataExtraction_AddKeyFilter,
    IMetadataExtraction_ClearKeyFilter
};

void IMetadataExtraction_init(void* self)
{
    IMetadataExtraction* pMetadataExt = (IMetadataExtraction*)self;
    pMetadataExt->mItf = &IMetadataExtraction_Itf;
}

