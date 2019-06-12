/* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file
 * <b>NVIDIA Tegra: OpenMAX Container Extension Interface</b>
 *
 */

#ifndef NVOMX_DrmExtensions_h_
#define NVOMX_DrmExtensions_h_

/** @} */

/**
 * @defgroup nv_omx_il_parser_drm Types
 *
 * This is the NVIDIA OpenMAX DRM class extension interface.
 *
 * These extensions include types of DRM, get and set license info
 * and cleaning teh license store and more.
 *
 * @ingroup nvomx_container_extension
 * @{
 */

typedef enum
{
    NvxDRMType_WMDRM10= 1,
    NvxDRMType_PlayReady,
    NvxDRMType_Force32 = 0x7FFFFFFF
} EDrmType;

#define DRM_INDEX_CONFIG_SET_SURFACE "DRM.index.config.setsurface"
typedef struct DRM_CONFIG_SET_SURFACE{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 displayWidth;               // display width of display area
    OMX_U32 displayHeight;              // display height of display area
    OMX_U32 displayX;                   // display X coordinate of top left corner of display area
    OMX_U32 displayY;                   // display Y coordinate of top left corner of display area
    OMX_U32 componentNameSize;          // (actual data size send from IL component to IL Client)
    OMX_U8  componentName[1];           // Variable length array of component name(send from IL client to IL component)
}DRM_CONFIG_SET_SURFACE;


/** @} */

/** Get license URL and challange config
 * See ::DRM_INDEX_CONFIG_LICENSE_CHALLENGE
 */
#define DRM_INDEX_CONFIG_LICENSE_CHALLENGE "DRM.index.config.licensechallenge"
/** Holds url data and challenge.query */
typedef struct DRM_CONFIG_LICENSE_CHALLENGE
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    EDrmType DrmType;    /**< Type of DRM */
    union
    {
        OMX_U32 nPsiObjectSize;    /**< actual data size send from IL client to Il component*/
        OMX_U32 nLicenseQuerySize;    /**< actual data size send from IL component to IL Client */
    }size;
    union
    {
       OMX_U8 pPsiObject[1];    /**< PSI object data*/
       OMX_U8 pLicenseQuery[1];    /**< challenge query to be sent to the license server */
    }data;
}DRM_CONFIG_LICENSE_CHALLENGE;

/** @} */

/** Process license response config
 * See ::DRM_INDEX_CONFIG_LICENSE_RESPONSE
 */
#define DRM_INDEX_CONFIG_LICENSE_RESPONSE "DRM.index.config.licenseresponse"
/** Holds the license response recieved from license server */
typedef struct DRM_CONFIG_LICENSE_RESPONSE
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    EDrmType DrmType;    /**< Type of DRM */
    OMX_U32 pLicenseHandle;    /**< Handle to the license */
    OMX_U32 nResponseSize;     /**< Size of Response */
    OMX_U8 pLicenseResponse[1];       /**< Response recieved from license server */
}DRM_CONFIG_LICENSE_RESPONSE;


/** Delete a license config
 * See ::DRM_INDEX_CONFIG_DELETE_LICENSE
 */
#define DRM_INDEX_CONFIG_DELETE_LICENSE "DRM.index.config.deletelicense"
/** Holds the license info to be deleted */
typedef struct DRM_CONFIG_DELETE_LICENSE
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    EDrmType DrmType;    /**< Type of DRM */
    OMX_U32 pLicenseHandle;    /**< Handle to the license */
}DRM_CONFIG_DELETE_LICENSE;

/** Setting InitializationVector config
 * See ::DRM_BUFFER_HEADER_EXTRADATA_INITIALIZATION_VECTOR
 */
#define DRM_BUFFER_HEADER_EXTRADATA_INITIALIZATION_VECTOR "DRM.buffer.header.extradata.initializationvector"
typedef struct DRM_IVDATA
{
    OMX_U64 qwInitializationVector;
    OMX_U64 qwBlockOffset;
    OMX_U8 bByteOffset;
}DRM_IVDATA;

/** Setting Decryptr offset config
 * See ::DRM_BUFFER_HEADER_EXTRADATA_ENCRYPTION
 */
#define DRM_BUFFER_HEADER_EXTRADATA_ENCRYPTION_OFFSET "DRM.buffer.header.extradata.encryptionoffset"
typedef struct DRM_ENCRYPTION_OFFSET
{
    OMX_U32 encryptionOffset;
}DRM_ENCRYPTION_OFFSET;

/** Setting Decryptr offset config
 * See ::DRM_BUFFER_HEADER_EXTRADATA_ENCRYPTION_METADATA
 */
#define DRM_BUFFER_HEADER_EXTRADATA_ENCRYPTION_METADATA "DRM.buffer.header.extradata.encryptionmetadata"
typedef struct DRM_EXTRADATA_ENCRYPTION_METADATA
{
    OMX_U32 encryptionOffset;
    OMX_U32 encryptionSize;
    DRM_IVDATA ivData;
}DRM_EXTRADATA_ENCRYPTION_METADATA;

/** @} */

#endif
/* File EOF */
