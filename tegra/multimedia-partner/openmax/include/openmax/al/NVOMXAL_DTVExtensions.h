/* Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * This is the NVIDIA OpenMAX AL DTV extensions interface.
 *
 */

#ifndef NVOMXAL_DTVEXTENSIONS_H_
#define NVOMXAL_DTVEXTENSIONS_H_

#include <OMXAL/OpenMAXAL.h>

#ifdef __cplusplus
extern "C" {
#endif

XA_API extern const XAInterfaceID XA_IID_NVDTVSECTIONFILTERING;

struct NVXADTVSectionFilteringItf_;
typedef const struct NVXADTVSectionFilteringItf_ * const * NVXADTVSectionFilteringItf;

typedef XAresult (XAAPIENTRY * nvxaDTVSectionFilteringCallback) (
    NVXADTVSectionFilteringItf caller,
    const void * pContext,
    void *pTSData,
    XAuint32 dataSize
);

struct NVXADTVSectionFilteringItf_ {
    XAresult (*GetAvailablePrograms) (
        NVXADTVSectionFilteringItf self,
        XAuint32 * pNumberOfPrograms,
        XAuint32 * pProgramIDs
    );
    XAresult (*GetCurrentProgram) (
        NVXADTVSectionFilteringItf self,
        XAuint32 * pProgramID
    );
    XAresult (*SetCurrentProgram) (
        NVXADTVSectionFilteringItf self,
        XAuint32 ProgramID
    );
    XAresult (*GetDataPIDList) (
        NVXADTVSectionFilteringItf self,
        XAuint32 * pNumberOfPID,
        XAuint32 * pPIDs
    );
    XAresult (*SetDataPIDList) (
        NVXADTVSectionFilteringItf self,
        XAuint32 NumberOfPID,
        XAuint32 * pPIDs
    );
    XAresult (*RegisterDTVSectionFilteringCallback) (
        NVXADTVSectionFilteringItf self,
        nvxaDTVSectionFilteringCallback Callback,
        void * pContext
    );
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NVOMXAL_DTVEXTENSIONS_H_ */
