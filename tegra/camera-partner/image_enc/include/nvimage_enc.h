/*
* Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           Image encoder library APIs</b>
 *
 * @b Description: Declares Interface for the nvidia image encoder library.
 *
 */

#ifndef INCLUDED_NV_IMAGE_ENC_LIBRARY_H
#define INCLUDED_NV_IMAGE_ENC_LIBRARY_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvmm_buffertype.h"
#include "nvmm_exif.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define PRIMARY_ENC     (1 << 0)
#define THUMBNAIL_ENC   (1 << 1)

    /**
    * NvImageEncHandle is an opaque handle to the Nvidia image
    * encoding interface.
    */

    typedef struct NvImageEncRec *NvImageEncHandle;

    /**
     * NvJpegImageRotate identifies the rotation behaviour of the image
     *
     */
    typedef enum
    {
        NvJpegImageRotate_Physically = 1,
        NvJpegImageRotate_ByExifTag,

        /* Max is used to pad the enum to 32bits and should always be last */
        NvJpegImageRotate_Force32 = 0x7FFFFFFF
    } NvJpegImageRotate;

    /**
     * NvImageEncoder stream index
     */
    typedef enum
    {
        NvImageEncoderStream_INPUT = 1,
        NvImageEncoderStream_OUTPUT,
        NvImageEncoderStream_THUMBNAIL_INPUT,

        NvImageEncoderStream_Force32 = 0x7FFFFFFF
    } NvImageEncoderStreamId;

    /**
    * NvJpegImageOrientation defines the structure for holding
    * image orientation information. Indicates if image is
    * really rotated or rotated with Exif Orientation tag.
    *
    */
    typedef struct
    {
        NvMMExif_Orientation    orientation;
        NvJpegImageRotate       eRotation;
    } NvJpegImageOrientation;

    typedef enum
    {
        NV_IMG_JPEG_COLOR_FORMAT_420 = 1,
        NV_IMG_JPEG_COLOR_FORMAT_422,
        NV_IMG_JPEG_COLOR_FORMAT_422T,
        NV_IMG_JPEG_COLOR_FORMAT_444
    } NV_IMG_JPEG_COLOR_FORMAT;

    typedef enum
    {
        NV_IMG_JPEG_SURFACE_TYPE_YV12 = 1,
        NV_IMG_JPEG_SURFACE_TYPE_NV12,
    } NvJpegSurfaceType;

    typedef enum
    {
        NV_IMG_JPEG_ENC_QuantizationTable_Luma = 1,
        NV_IMG_JPEG_ENC_QuantizationTable_Chroma,
        /*
        *  Max should always be the last value, used to pad the enum to 32bits
        */
        NV_IMG_JPEG_ENC_QuantizationTable_Force32 = 0x7FFFFFFF
    } NV_IMG_JPEG_ENC_QUANTIZATIONTABLETYPE;

    typedef struct
    {
        NvBool   Enable;
        NV_IMG_JPEG_ENC_QUANTIZATIONTABLETYPE QTableType;
        NvU8    LumaQuantTable[64];
        NvU8    ChromaQuantTable[64];
    } NvImageEncCustomQuantTables;

    typedef struct
    {
        NvBool Enable;
        NvMM0thIFDTags ExifInfo;
    } NvImageExifParams;

    typedef struct
    {
        NvBool Enable;
        NvMMGPSInfo GpsInfo;
    } NvImageGpsParams;

    typedef struct
    {
        NvBool Enable;
        NvMMInterOperabilityInfo IFDInfo;
    } NvImageIFDParams;

    typedef struct
    {
        NvBool Enable;
        NvMMGPSProcMethod GpsProcMethodInfo;
    } NvImageGpsProcParams;

    /**
    * NvJpegEncParameters defines the various parameters that the nvidia
    * hardware image encoder can support. The client is expected to
    * collect all the information before calling NvImageEnc_SetParam.
    *
    */
    typedef struct {
        /* Set only when the params are for Thumbnail.
         * This is also a notification for the library if the client wants
         * thumbnail support or not.
         */
        NvBool      EnableThumbnail;

        /* Notifies the encoder about the encoding quality.
         * Range is 0 -100.
         */
        NvU32       Quality;

        /* Should be a type of NV_IMG_JPEG_COLOR_FORMAT */
        NvU32       InputFormat;

        /* Based on colorFormat and resolution, encoder suggests
         * the output buffer size. The client should allocate the
         * output buffer no less than this size in bytes */
        NvU32       EncodedSize;

        /*  */
        NvU32       ErrorResiliency;

        /* Size of input buffer */
        NvSize      Resolution;

        /* */
        NvImageExifParams EncParamExif;

        /* GPS info to be updated by the user / client */
        NvImageGpsParams EncParamGps;

        /* This Structure holds InterOperability IFD tags */
        NvImageIFDParams EncParamIFD;

        /* JPEG orientation setting. It can be either physically rotated
         * image or can be rotated by EXIF orientation value. */
        NvJpegImageOrientation Orientation;

        /* GPS Processing Method tag info */
        NvImageGpsProcParams EncParamGpsProc;

        /* */
        NvImageEncCustomQuantTables CustomQuantTables;

        /* Surface Type */
        NvJpegSurfaceType EncSurfaceType;
    } NvJpegEncParameters;

    /**
     * NvImageEncoderType defines various supported encoder types.
     *
     */
    typedef enum
    {
        NvImageEncoderType_Unknown,
        NvImageEncoderType_HwEncoder,
        NvImageEncoderType_SwEncoder,

        /*
         * This list can grow as we start adding
         * support for other encoders.
         */

        NvImageEncoderType_Force32 = 0x7FFFFFFF
    } NvImageEncoderType;

    /**
    * Function type for the client to receive buffer from the
    * image encoding library.
    *
    * The client has to set a buffer callback in the open parameters
    * for the imaging library to send buffers asynchronously. If the
    * client does not set the callback funtion, this value would be
    * treated as NULL and the imaging library will work in a serial
    * fashion.
    *
    * @param [in]  pContext         client's private context pointer.
    * @param [out] StreamIndex      index of the image encoder's stream. A type
    *                               of NvImageEncoderStreamId.
    * @param [out] BufferSize       The total size of the buffer being transferred.
    * @param [out] pBuffer          pointer to buffer.
    * @param [out] EncStatus        This returns the state of the encoder.
    *
    * @retval NvSuccess Transfer was successfully.
    *
    */
    typedef
        void
        (*NvImageEncBufferCB)(
            void *pContext,
            NvU32 StreamIndex,
            NvU32 BufferSize,
            void *pBuffer,
            NvError EncStatus);

    /**
     * NvImageEncSupportLevel defines what encoder type should be used for
     * encoding of a particular stream.
     *
     * The client can do a primary image encoding with the hw encoder, and
     * still be able to encode thumbnail with another encoder from the list
     * of available encoder type in NvImageEncoderType.
     *
     * However, the final output will be a complete jpeg image which will
     * include or exclude thumbnail as per the client's selection.
     */
    typedef struct
    {
        NvImageEncoderType PriImage;
        NvImageEncoderType ThumbImage;
    } NvImageEncLevelOfSupport;

    /**
     * Defines the parameters required to open the image encoder.
     *
     */
    typedef struct
    {
        void                        *pContext;
        //NvImageEncLevelOfSupport    *EncSupportLevel;  Rethink !!!!!
        NvU32                       LevelOfSupport;
        NvImageEncoderType          Type;
        NvImageEncBufferCB          ClientIOBufferCB;
    } NvImageEncOpenParameters;

    /**
    * @brief To open the interface to the nvidia image encoder API.
    * This api must be called before use of any other NvImageEnc api.
    * It initializes all client specific data for this instance of
    * the API.
    *
    * @param [out] hImageEnc    pointer to the handle to the Nvidia
    *                           image encoder.
    * @param [in]  pOpenParams  pointer to the NvImageEncOpenParameters.
    *
    * @retval NvError_Success on success and error on failure.
    *
    */
    NvError
    NvImageEnc_Open(
        NvImageEncHandle *hImageEnc,
        NvImageEncOpenParameters *pOpenParams);

    /**
    * @brief To set the various parameters for the image encoder.
    * The image encoder supports various attributes that the client
    * can set. This API will allow the client to set all the required
    * attributes. The client should be able to set all the parameters
    * of NvJpegEncParameters via this call.
    *
    * This API call is mandatory and should be called before calling
    * NvImageEnc_Start.
    *
    * @param [in]  hImageEnc    handle to the Nvidia image encoder.
    * @param [in]  params       pointer to NvJpegEncParameters.
    *
    * @retval NvError_Success on success and error on failure.
    *
    */
    NvError
    NvImageEnc_SetParam(
        NvImageEncHandle hImageEnc,
        NvJpegEncParameters *params);

    /**
    * @brief To get the current values for various parameters of the
    * image encoder.
    * The image encoder supports various attributes that the client
    * can set. This API will allow the client to get the current
    * values of all the attributes in NvJpegEncParameters.
    *
    * This API call is optional.
    *
    * @param [in]  hImageEnc    handle to the Nvidia image encoder.
    * @param [in]  params       pointer to NvJpegEncParameters.
    *
    * @retval NvError_Success on success and error on failure.
    *
    */
    NvError
    NvImageEnc_GetParam(
        NvImageEncHandle hImageEnc,
        NvJpegEncParameters *params);

    /**
    * @brief To start the encoding of a YUV frame.
    * This API will require an input buffer, output buffer and also a
    * notification if the data in the buffer is a thumbnail data or
    * is primary data.
    * The encoder will use the input buffers and fill the output buffer
    * with the encoded data. The encoder also supports thumbnail encoding,
    * and would treat the buffer as thumbnail buffer if the argument
    * 'IsThumbnail' is set to NV_TRUE. Otherwise the encoder will treat
    * the input data to be primary data.
    * If the encoder is also programmed for thumbnail support, then a
    * complete-input-data would be a set of 1 primary data and a thumbnail
    * data. The client is expected to comply to the defination of
    * complete-input-data.
    *
    * This API can operate in 2 modes. Synchronous and Asynchronous mode.
    * If the client does not set a callback function in the NvImageEnc_Open
    * call (via NvImageEncBufferCB), this function will operate in serial
    * mode and will block itself unless it has a complete output. If the
    * client enables thumbnail (via NvJpegEncParameters), the encoder will
    * not block itself untill it receives both the input buffers. Once the
    * encoder has both the data, it will return only when it has the complete
    * output data.
    * Alternatively, if the client sets the callback in NvImageEnc_Open call,
    * the encoder becomes non-blocking and can accept more input buffers. The
    * encoded output and the input buffer would be delivered to the client
    * via the client registered callback.
    *
    * The image encode library should have at least one output buffer for
    * an input buffer. However, if client expects both primary and thumbnail
    * encoding in 1 output file, AKA - output.jpg, only 1 outputBuffer is
    * sufficient.
    * The client is expected to send the outputBuffer with the first input data,
    * regardless of it being for thumbnail or primary data.
    *
    * @param [in]  hImageEnc    handle to the Nvidia image encoder.
    * @param [in]  InputBuffer  pointer to an NvMMBuffer which will contain
    *                           input data in YUV color format.
    * @param [out] OutPutBuffer pointer to an NvMMBuffer which will contain
    *                           the encoded data.
    *
    * @retval NvError_JPEGEncWaitForMoreData if encoder expects more data.
    * @retval NvError_Success on success and error on failure.
    *
    */
    NvError
    NvImageEnc_Start(
        NvImageEncHandle hImageEnc,
        NvMMBuffer  *InputBuffer,
        NvMMBuffer  *OutPutBuffer,
        NvBool      IsThumbnail);

    /**
    * @brief To close the interface to the nvidia image encoder API
    *
    * @param [in]  hImageEnc    handle to the Nvidia image encoder.
    *
    * @retval None. However, if we have pending work inside the encoder
    *         library, Close will block itself.
    */
    void
    NvImageEnc_Close(
        NvImageEncHandle hImageEnc);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NV_IMAGE_ENC_LIBRARY_H
