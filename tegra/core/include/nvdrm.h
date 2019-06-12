/*
* Copyright (c) 2008-2010 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** 
 * @file
 * <b>NVIDIA Tegra: Custom DRM Plug-in Interface</b>
 *
 */

/**
 * @defgroup nv_drm Custom DRM Plug-in Interface
 *  
 * A platform-independent NVIDIA Digital Rights Management interface (NvDrm)
 * that must be used as an interface between parser and any DRM. DRM
 * implementations must adapt to NvDrm interface to enable parsers to use
 * DRM capabilities.
 *
 * @ingroup nvomx_plugins
 * @{
 */

#ifndef INCLUDED_NVDRM_H
#define INCLUDED_NVDRM_H

#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C" {
#endif


/**
 * Handle to the DRM context.
 */
typedef struct NvDrmContextRec* NvDrmContextHandle;

/**
 * Defines DRM store types.
 */
typedef enum
{
    /// Callbacks with this reason will have \a pvData as a ::DRM_CONST_STRING.
    DRM_STORE_LICENSE_NOTIFY_KID_TYPE       = 1,
    /// Callbacks with this reason will have \a pvData as a
    /// ::DRM_ANSI_CONST_STRING.
    DRM_STORE_LICENSE_STORE_V1_LICENSE_TYPE = 2,
    /// Callbacks with this reason will have \a pvData as a 
    /// ::DRM_INCLUSION_LIST_CALLBACK_STRUCT. @note \a dwChain will always
    /// be 0 (zero) as when licenses are stored they are evaluated individually,
    /// not in a chain.
    DRM_STORE_LICENSE_NOTIFY_INCLUSION_LIST_TYPE = 4
} DRM_STORE_LICENSE_CALLBACK_ENUM_TYPE;

/** 
 * DRM callback prototype.
 */
typedef NvError (* DRMCALLBACK) (
    void *drmData,
    long callBackType,
    const void *context);

typedef NvError ( *pfnLicenseCallback)(
    void *pvData,
    DRM_STORE_LICENSE_CALLBACK_ENUM_TYPE  eReason,
    void *pvContext );

/** Callback function to report after updating license store. */
typedef NvError ( *pfnLicenseStore )( 
    const void    *pvData, 
    NvU32   noOfLicensesUpdated,
    NvU32   TotalNoOfLicenses
    );

/**
 * Data that is needed for DRM binding will 
 * be populated in these variables by the client.
 */
typedef  struct NvDrmBindInfo
{
    /// Holds Guid Value of ::ASFContentEncryptionObjectEx.
    NvU8  pObjectId[16];
    /// Specifies the size, in bytes, of the \c ASFContentEncryptionObjectEx.
    NvU64 ObjectSize;
    /// Points to string that has encrypted data or secret data.
    NvU8* pEncryptedData;
    /// Length of \a pEncryptedData bytes.
    NvU32 EncDataSize;
    /// Points to string that has Key ID.
    NvU8* pKeyData;
    /// Length of \a pKeyData bytes.
    NvU32 KeyDataSize;
    /// Stores the DRM version number.
    NvU32 version;
    /// To inform the client about output protection levels.
    DRMCALLBACK f_pfnDrmCallback;
    /// Holds the file name.
    char *FileName;
    /// specifies vertical size of pixel aspect ratio.
    NvU32 Width;
    /// specifies horizontal size of pixel aspect ratio.
    NvU32 Height;
    const void *privData;
 } NvDrmBindInfo;


typedef struct NvDrmConstString
{
    const NvU16 *pString;
    NvU32 cString;
} NvDrmConstString;

typedef struct NvDrmFileTime
{
    NvU32 LowDateTime;
    NvU32 HighDateTime;
}NvDrmFileTime;

/**
* Holds license properties queries.
*/
typedef struct NvDrmLicenseState
{
    /// ID of the stream.
    NvU32 StreamId;
    /// Category of the license.
    NvU32 group;
    /// Number of items supplied in \a Count.
    NvU32 NumCounts;
    /// Up to 4 counts.
    NvU32 Count[4];
    /// Number of items supplied \a datetime.
    NvU32 NumDates;
    /// Up to 4 dates.
    NvDrmFileTime datetime[4];
    void * userDefined;
} NvDrmLicenseState;

/** 
 * Creates and initializes NvDrm context. NvDrm context
 * is specific to that DRM type.
 *
 * @param [out] phNvDrmContext A pointer to the handle to the NvDrm Context.
 *
 * @retval NvError_Success
 * @retval NvError_DrmOutOfMemory
 * @retval NvError_DrmFailure
 */
NvError 
NvDrmCreateContext( NvDrmContextHandle *phNvDrmContext );


/** Binds file-specific content.
 *
 * @param [in] hNvDrmContext The NvDrm context.
 * @param [in] BindInfo A pointer to file-specific data.
 * @param [in] size Size of buffer in bytes.
 *
 * @retval NvError_Success 
 * @retval NvError_DrmFailure 
 */
NvError 
NvDrmBindContent( NvDrmContextHandle hNvDrmContext, 
                  NvDrmBindInfo *BindInfo, 
                  NvU32 size );


/** Binds a license in the DRM subsystem.
 *
 * @param [in] hNvDrmContext The NvDrm context.
 *
 * @retval NvError_Success
 * @retval NvError_DrmInvalidLicense
 */
NvError 
NvDrmBindLicense( NvDrmContextHandle hNvDrmContext );


/** This function decrypts data contained in \a pBuffer of 
 * \a size (in bytes). Typically the size will be size of one data packet.
 * This function will be called once for every packet until the complete file
 * is exhausted. The function does in place computation where the encrypted 
 * input in \a pBuffer will be the decrypted by this API and the output will be 
 * placed in the same buffer.
 *
 * @param [in] hNvDrmContext The NvDrm context.
 * @param [in,out] pBuffer A pointer to the encrypted buffer.
 * @param [in] size Size of \a pBuffer in bytes.
 *
 * @retval NvError_Success
 * @retval NvError_DrmDecryptionFailed 
 */
NvError 
NvDrmDecrypt( NvDrmContextHandle hNvDrmContext, 
              NvU8* pBuffer, 
              NvU32 size );


/** Calls to this function will indicate that content 
 * metering info can be updated.
 *
 * @param [in] hNvDrmContext The NvDrm context.
 *
 * @retval NvError_Success
 * @retval NvError_DrmMeteringNotSupported
 * @retval NvError_DrmRightsNotAvailable
 * @retval NvError_DrmLicenseExpired
 */
NvError 
NvDrmUpdateMeteringInfo( NvDrmContextHandle hNvDrmContext );


/** Using the current DRM header set by SetV2Header DRM will
 * generate a liecense challenge that can be sent to a license server.
 *
 * @param hNvDrmContext [in] The NvDrm context.
 * @param url [out] A pointer to the URL of the license server.
 * @param urlSize [in]  A pointer to size of the \a url.
 * @param Challenge [out]  A pointer to challenge query to be sent to license
 *     server.
 * @param challengeSize [in] A pointer to size of the \a Challenge query.
 * 
 * @retval NvError_Success
 * @retval NvError_DrmFailure
 */
NvError
NvDrmGenerateLicenseChallenge( NvDrmContextHandle hNvDrmContext,
                               NvU16 *url,
                               unsigned long *urlSize,
                               NvU8  *Challenge,
                               unsigned long *challengeSize);

/** 
 * Processes a response from a license server. Usually this is a series
 * of licenses that will ultimately be stored in the device license store.
 *
 * @param hNvDrmContext [in] NvDrm Context.
 * @param fnCallback [in]  A pointer to a callback function which is called for
 *     each license in response. Can be NULL.
 * @param callBackContext [in]  A pointer to the user context returned in
 *     callback function. Can be NULL.
 * @param response [in,out] A pointer to a response blob from a license server.
 * @param responseSize [in] Count of bytes in \a response.
 *
 * @retval NvError_Success
 * @retval NvError_DrmFailure
 */
 NvError
 NvDrmProcessLicenseResponse(
    NvDrmContextHandle hNvDrmContext,
    pfnLicenseCallback fnCallback,
    void* callBackContext,
    NvU8 *response,
    unsigned long responseSize);

/** 
 * Creates a secure clock challenge.
 *
 * @param hNvDrmContext [in] The NvDrm context.
 * @param url [out] A pointer to the URL of license server.
 * @param urlSize [in] A pointer to user allocated buffer to get URL in WCHAR.
 *      Null terminated.
 * @param Challenge [out] A pointer to the query for URL. Can be NULL if URL
 *     is not required.
 * @param challengeSize [in,out] A pointer to the size of \a Challenge.
 *
 * @retval NvError_Success
 * @retval NvError_DrmFailure
 */
NvError
NvDrmClkGenerateChallenge(
    NvDrmContextHandle hNvDrmContext,
    NvU16 *url,
    NvU32 *urlSize,
    NvU8  *Challenge,
    NvU32 *challengeSize);

/** 
 * Processes secure clock request response received from server.
 *
 * @param hNvDrmContext [in] The NvDrm context.
 * @param pbResponse [in] A pointer to the response string received from server.
 * @param cbResponse [in] The size of \a pbResponse in bytes.
 * @param pResult [out] A pointer to DRM_RESULT to get error from server
 *     included in response.
 *
 * @retval NvError_Success
 * @retval NvError_DrmFailure
 */
NvError
NvDrmClkProcessResponse(
    NvDrmContextHandle hNvDrmContext,
    NvU8 *pbResponse,
    NvU32 cbResponse,
    NvS32 *pResult);

/** 
 * Destroys the NvDRM context.
 *
 * @param [in] hNvDrmContext The NvDrm context.
 *
 * @retval NvError_Success
 * @retval NvError_DrmFailure
 */
NvError 
NvDrmDestroyContext( NvDrmContextHandle hNvDrmContext );


/** 
 * Retrieves data about the usability of content.
 *
 * @param [in] hNvDrmContext The NvDrm context.
 * @param [in] strRights A pointer to an array of strings representing rights
 *     to be queried.
 * @param [out] licenseData  The results of the queires.
 * @param [in] noOfRights Number of elements in the \a strRights array, and the
 *   \a licenseData array.
 *
 * @retval NvError_Success
 * @retval NvError_DrmFailure
 */

NvError 
NvDrmGetLicenseInfo(
    NvDrmContextHandle hNvDrmContext,
    const NvDrmConstString *strRights[],
    NvDrmLicenseState  licenseData[],
    NvU32 noOfRights );


/** 
 * Updates the license store.
 *
 * @param [in] hNvDrmContext The NvDrm context.
 * @param [in] pvData A pointer to private data; could be NULL.
 * @param [in] pfnCallback Callback to be called after updating license store;
 *    could be NULL.
 *
 * @retval NvError_Success
 * @retval NvError_DrmFailure
 */
NvError
NvDrmUpdateLicenseStore(
    NvDrmContextHandle hNvDrmContext,
    const void *pvData,
    pfnLicenseStore  pfnCallback);

/** 
 * Structure used by clients for accessing the above APIs via function
 * pointers.
 */
typedef struct
{
    NvError (*pNvDrmCreateContext)(NvDrmContextHandle *phNvDRMContext);
    NvError (*pNvDrmDestroyContext)(NvDrmContextHandle hNvDRMContext);
    NvError (*pNvDrmBindContent)( NvDrmContextHandle hNvDRMContext,
        NvDrmBindInfo*BindInfo ,
        NvU32 size );

    NvError (*pNvDrmDecrypt)( NvDrmContextHandle hNvDRMContext,
                 NvU8 *pBuffer ,
                 NvU32 size );
    NvError (*pNvDrmUpdateMeteringInfo)(NvDrmContextHandle hNvDRMContext);
    NvError (*pNvDrmBindLicense)(NvDrmContextHandle hNvDRMContext);
    NvError (*pNvDrmGenerateLicenseChallenge)(NvDrmContextHandle hNvDRMContext, NvU16 *url,
                NvU32 *urlSize, NvU8  *Challenge,
                NvU32 *challengeSize
                );
    NvError (*pNvDrmProcessLicenseResponse)(NvDrmContextHandle hNvDRMContext, pfnLicenseCallback fnCallback,
                void* callBackContext, NvU8 *response,
                NvU32 responseSize
                );
    NvError (*pNvDrmClkGenerateChallenge)(NvDrmContextHandle hNvDrmContext,NvU16 *url,NvU32 *urlSize,NvU8  *Challenge,NvU32 *challengeSize);
    NvError (*pNvDrmClkProcessResponse)(NvDrmContextHandle hNvDrmContext,NvU8 *pbResponse,NvU32 cbResponse,NvS32 *pResult);
NvError (*pNvDrmGetPetetionURL)(NvU8 *url,NvU32 urlLength);
}NvDrmInterface;



#if defined(__cplusplus)
}
#endif

#endif   // INCLUDED_NVDRM_H

/** @} */
/* File EOF */
