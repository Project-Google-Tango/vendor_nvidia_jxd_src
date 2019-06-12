/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvimage_enc_exifheader.h"
#include "nvmm_exif.h"
#include "nvmakernote_enc.h"

static NvError
CalcApp1HdrLengthNo1stIFd(NvImageEncHandle hImageEnc,
                    NvRmMemHandle hExifInfo,
                    NvU32 *pDataLen);
static NvU32 CalcLengthOfGPSIFD(NvEncoderPrivate * pEncCtx, NvMMGPSInfo *pGPSInfo);
static void RelativePrime (NvU32 *Nr, NvU32 *Dr);
static void Insert1stIFD(NvImageEncHandle hImageEnc,
                    NvMMEXIFInfo *pExifInfo);

static void
Insert1stIFD(NvImageEncHandle hImageEnc, NvMMEXIFInfo *pExifInfo)
{
    NvEncoderPrivate *pEncCtx = (NvEncoderPrivate *)hImageEnc->pEncoder;
    NvU32 Value, Offset;
    NvU8  *ptr = pEncCtx->HeaderParams.pbufHeader;
    NvU8  *lastptr = ptr;
    NvU32 OffsetTo_IFD1;
    NvU32 APP1_Length = 0;
    NvU32 Size = 0;

    /*
    * Change the APP1 length field and next IFD filed(for IFD1) written in the
    * function NvMMLiteJPEGEncWriteHeader.
    * 22 = SOI(2) + APP1(2) + APP1 Length (2) + EXIF tag(6) + TIFF Header(8 Bytes) + No.of 0th IFD fields(2 Bytes)
    */
    OffsetTo_IFD1 =  22 + (pEncCtx->HeaderParams.NoOf_0IFD_Fields * 12);

    /*
    * 20 = TIFF Header(8 Bytes) + No.of 0th IFD fields(2 Bytes) + 1st IFD offset(4 Bytes) +
    * No.of Exif IFD fields(2 Bytes) + 2nd IFD offset(4 Bytes)
    */
    Offset = 26 +
             ((pEncCtx->HeaderParams.NoOf_0IFD_Fields +
               pEncCtx->HeaderParams.NumofExifTags +
               TagCount_InterOperabilityIFD) * 12) +
             ExifVariable_XResolution +
             ExifVariable_YResolution +
             ExifVariable_DateandTime +
             ExifVariable_ExposureTime +
             ExifVariable_FNumber +
             (2 * ExifVariable_DateandTime) +
             ExifVariable_CompressedBitsPerPixel +
             ExifVariable_ExposureBias +
             ExifVariable_MaxAperture +
             ExifVariable_SubjectDistanceRange +
             ExifVariable_Focallength +
             ExifVariable_ZoomFactor +
             pEncCtx->HeaderParams.lenUserComment +
             pEncCtx->HeaderParams.lenSubjectArea +
             pEncCtx->HeaderParams.lenImageUniqueId +
             ExifVariable_ShutterSpeedValue +
             ExifVariable_ApertureValue +
             ExifVariable_BrightnessValue;

    if(pEncCtx->HeaderParams.lenImageDescription > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenImageDescription;

    if(pEncCtx->HeaderParams.lenMake > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenMake;

    if(pEncCtx->HeaderParams.lenModel > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenModel;

    if(pEncCtx->HeaderParams.lenSoftware > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenSoftware;

    if(pEncCtx->HeaderParams.lenArtist  > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenArtist ;

    if(pEncCtx->HeaderParams.lenCopyright > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenCopyright;

    if(pEncCtx->HeaderParams.lenMakerNote > LEN_VALUE_FIT_IN_TAG)
        Offset += MNoteEncodeSize(pEncCtx->HeaderParams.lenMakerNote);

    if(InterOperabilityTagLength_Index > LEN_VALUE_FIT_IN_TAG)
        Offset += InterOperabilityTagLength_Index;

    if(pEncCtx->PriParams.EncParamGps.Enable)
    {
        Offset += pEncCtx->HeaderParams.Length_Of_GPS_IFD;
    }

    ptr[OffsetTo_IFD1] = (NvU8)(Offset >> 24);
    ptr[OffsetTo_IFD1 + 1] = (NvU8)((Offset >> 16) & 0xFF);
    ptr[OffsetTo_IFD1 + 2] = (NvU8)((Offset >> 8) & 0xFF);
    ptr[OffsetTo_IFD1 + 3] = (NvU8)(Offset & 0xFF); // Offset to IFD 1

    ptr = ptr + pEncCtx->HeaderParams.headeroffset;
    lastptr = ptr;

    // writing Header of APP1 from IFD1
    *(ptr++) = 0;
    *(ptr++) = TagCount_1stIFD; //Number of 1st IFD fields

    // 6 = jpeg
    EMIT_TAG_NO_OFFSET(ptr,
                       Tag_Compression_High,
                       Tag_Compression_Low,
                       IfdTypes_Short,
                       1,
                       0,
                       6,
                       0,
                       0);

    //Orientation
    Value = (NvU32)pEncCtx->PriParams.Orientation.orientation;
    pExifInfo->Orientation_ThumbNail = Value;

    EMIT_TAG_NO_OFFSET(ptr,
                       Tag_Orientation_High,
                       Tag_Orientation_Low,
                       IfdTypes_Short,
                       1,
                       (NvU8)((Value >> 8) & 0xFF),
                       (NvU8)(Value & 0xFF),
                       0,
                       0);

    /*
    * 26 = TIFF Header(8 Bytes) + No.of 0th IFD fields(2 Bytes) + 1st IFD offset(4 Bytes) +
    * No.of Exif IFD fields(2 Bytes) + 2nd IFD offset(4 Bytes) +
    * No.of 1st IFD fields(2 Bytes) + 3rd IFD offset(4 Bytes)
    */
    Offset = 32 +
             ((pEncCtx->HeaderParams.NoOf_0IFD_Fields +
               pEncCtx->HeaderParams.NumofExifTags +
               TagCount_InterOperabilityIFD +
               TagCount_1stIFD) * 12) +
             ExifVariable_XResolution +
             ExifVariable_YResolution +
             ExifVariable_DateandTime +
             ExifVariable_ExposureTime +
             ExifVariable_FNumber +
             (2 * ExifVariable_DateandTime) +
             ExifVariable_ExposureBias +
             ExifVariable_MaxAperture +
             ExifVariable_CompressedBitsPerPixel +
             ExifVariable_SubjectDistanceRange +
             ExifVariable_Focallength +
             ExifVariable_ZoomFactor +
             pEncCtx->HeaderParams.lenUserComment +
             pEncCtx->HeaderParams.lenImageUniqueId +
             pEncCtx->HeaderParams.lenSubjectArea +
             ExifVariable_ShutterSpeedValue +
             ExifVariable_ApertureValue +
             ExifVariable_BrightnessValue;

    if(pEncCtx->HeaderParams.lenImageDescription > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenImageDescription;

    if(pEncCtx->HeaderParams.lenMake > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenMake;

    if(pEncCtx->HeaderParams.lenModel > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenModel;

    if(pEncCtx->HeaderParams.lenSoftware > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenSoftware;

    if(pEncCtx->HeaderParams.lenArtist  > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenArtist ;

    if(pEncCtx->HeaderParams.lenCopyright > LEN_VALUE_FIT_IN_TAG)
        Offset += pEncCtx->HeaderParams.lenCopyright;

    if(pEncCtx->HeaderParams.lenMakerNote > LEN_VALUE_FIT_IN_TAG)
        Offset += MNoteEncodeSize(pEncCtx->HeaderParams.lenMakerNote);

    if(InterOperabilityTagLength_Index > LEN_VALUE_FIT_IN_TAG)
        Offset += InterOperabilityTagLength_Index;

    if (pEncCtx->PriParams.EncParamGps.Enable)
    {
        Offset += pEncCtx->HeaderParams.Length_Of_GPS_IFD;
    }

    EMIT_TAG_WITH_OFFSET(ptr,
                         Tag_XResolution_High,
                         Tag_XResolution_Low,
                         IfdTypes_Rational,
                         1,
                         Offset);

    Offset += ExifVariable_XResolution;

    EMIT_TAG_WITH_OFFSET(ptr,
                         Tag_YResolution_High,
                         Tag_YResolution_Low,
                         IfdTypes_Rational,
                         1,
                         Offset);

    Offset += ExifVariable_YResolution;

    //defaults 2 = inches
    EMIT_TAG_NO_OFFSET(ptr,
                       Tag_ResolutionUnit_High,
                       Tag_ResolutionUnit_Low,
                       IfdTypes_Short,
                       1,
                       0,
                       2,
                       0,
                       0);

    EMIT_TAG_WITH_OFFSET(ptr,
                         Tag_JpegInterchangeFormat_High,
                         Tag_JpegInterchangeFormat_Low,
                         IfdTypes_Long,
                         1,
                         Offset);

    EMIT_TAG_NO_OFFSET(ptr,
                       Tag_JpegInterchangeFormatLength_High,
                       Tag_JpegInterchangeFormatLength_Low,
                       IfdTypes_Long,
                       1,
                       0,
                       0,
                       0,
                       0);

    // note down the pointer to offset. This value is not known at this time
    // so will be filled after thumbnail is coded
    pEncCtx->HeaderParams.JpegIFLengthLocation = pEncCtx->HeaderParams.headeroffset + (ptr - lastptr);
    pEncCtx->HeaderParams.JpegIFLengthLocation -= 4; // to compensate for length written in above macro

    switch (pEncCtx->ThumbParams.InputFormat)
    {
        case NV_IMG_JPEG_COLOR_FORMAT_422:
        case NV_IMG_JPEG_COLOR_FORMAT_422T:
            EMIT_TAG_NO_OFFSET(ptr, Tag_YCbCrPosition_High, Tag_YCbCrPosition_Low, IfdTypes_Short, 1, 0, 2, 0, 0);
            break;
        default:
            EMIT_TAG_NO_OFFSET(ptr, Tag_YCbCrPosition_High, Tag_YCbCrPosition_Low, IfdTypes_Short, 1, 0, 1, 0, 0);
            break;
    }

    *(ptr++) = 0x00;
    *(ptr++) = 0x00;
    *(ptr++) = 0x00;
    *(ptr++) = 0x00; // Offset of the next IFD

    // Hardcode to 72 dpi.
    TAG_SAVE_VAL_RESOLUTION(ptr);

    //Updating APP1 Header size
    APP1_Length = ((pEncCtx->HeaderParams.pbufHeader[4] << 8) |
                   (pEncCtx->HeaderParams.pbufHeader[5])) +
                  (ptr - lastptr); // length of APP1

    APP1_Length += pEncCtx->HeaderParams.ThumbNailSize;
    pEncCtx->HeaderParams.pbufHeader[4] = (NvU8)(APP1_Length >> 8); // length of APP1
    pEncCtx->HeaderParams.pbufHeader[5] = (NvU8)(APP1_Length & 0xFF);

    Size = pEncCtx->HeaderParams.ThumbNailSize; /* SOI Marker included in Size */
    /* Update JPEG Thumbnail data length */
    pEncCtx->HeaderParams.pbufHeader[pEncCtx->HeaderParams.JpegIFLengthLocation] = 0x0;
    pEncCtx->HeaderParams.pbufHeader[pEncCtx->HeaderParams.JpegIFLengthLocation + 1] = 0x0;
    pEncCtx->HeaderParams.pbufHeader[pEncCtx->HeaderParams.JpegIFLengthLocation + 2] = (NvU8)(Size >> 8);
    pEncCtx->HeaderParams.pbufHeader[pEncCtx->HeaderParams.JpegIFLengthLocation + 3] = (NvU8)(Size & 0xFF);

    pEncCtx->HeaderParams.headeroffset += (ptr - lastptr);

    return;
}

static void
RelativePrime (NvU32 *Nr, NvU32 *Dr)
{
    NvU32 i;
    if(((*Nr)!=0) && ((*Dr)!=0))
    {
        for (i = 10; i>1; i--)
        {
            while(!(((*Nr)%i) || ((*Dr)%i) ))
            {
                *Nr = (*Nr)/i;
                *Dr = (*Dr)/i;
            }
        }
    }
}

static NvU32 CalcLengthOfGPSIFD(NvEncoderPrivate *pEncCtx, NvMMGPSInfo *pGPSInfo)
{
    NvU32 Length_Of_GPSIFD = 0;

    /*
    * Setting the gpstags flags to NV_FALSE
    */
    pEncCtx->HeaderParams.bGPSLatitudeTag_Present =
    pEncCtx->HeaderParams.bGPSLongitudeTag_Present =
    pEncCtx->HeaderParams.bGPSAltitudeTag_Present =
    pEncCtx->HeaderParams.bGPSTimeStampTag_Present =
    pEncCtx->HeaderParams.bGPSSatellitesTag_Present =
    pEncCtx->HeaderParams.bGPSStatusTag_Present =
    pEncCtx->HeaderParams.bGPSMeasureModeTag_Present =
    pEncCtx->HeaderParams.bGPSDOPTag_Present =
    pEncCtx->HeaderParams.bGPSImgDirectionTag_Present =
    pEncCtx->HeaderParams.bGPSMapDatumTag_Present =
    pEncCtx->HeaderParams.bGPSDateStampTag_Present =
    pEncCtx->HeaderParams.bGPSProcessingMethodTag_Present = NV_FALSE;

    pEncCtx->HeaderParams.NoOf_GPS_Tags = MIN_NO_OF_GPS_TAGS;
    /*
    6 = No.of GPSInfo tags(2 Bytes)+ Next IFD Offset(4 Bytes)
    */
    Length_Of_GPSIFD = 6;

    if( (pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_LatitudeRef))
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 2;
        Length_Of_GPSIFD += GPSTagLength_Latitude;
        pEncCtx->HeaderParams.bGPSLatitudeTag_Present = NV_TRUE;
    }
    if( (pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_LongitudeRef))
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 2;
        Length_Of_GPSIFD += GPSTagLength_Longitude;
        pEncCtx->HeaderParams.bGPSLongitudeTag_Present = NV_TRUE;
    }
    if( (pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_AltitudeRef) )
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 2;
        Length_Of_GPSIFD += GPSTagLength_Altitude;
        pEncCtx->HeaderParams.bGPSAltitudeTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_TimeStamp)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 1;
        Length_Of_GPSIFD += GPSTagLength_TimeStamp;
        pEncCtx->HeaderParams.bGPSTimeStampTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_Satellites)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 1;
        pEncCtx->HeaderParams.GPSTagLength_Satellites = NvOsStrlen(pGPSInfo->GPSSatellites) + 1;
        if(pEncCtx->HeaderParams.GPSTagLength_Satellites > LEN_VALUE_FIT_IN_TAG)
            Length_Of_GPSIFD += pEncCtx->HeaderParams.GPSTagLength_Satellites;
        pEncCtx->HeaderParams.bGPSSatellitesTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_Status)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 1;
        pEncCtx->HeaderParams.bGPSStatusTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_MeasureMode)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 1;
        pEncCtx->HeaderParams.bGPSMeasureModeTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_DOP)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 1;
        Length_Of_GPSIFD += GPSTagLength_DOP;
        pEncCtx->HeaderParams.bGPSDOPTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_ImgDirectionRef)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 2;
        Length_Of_GPSIFD += GPSTagLength_ImgDirection;
        pEncCtx->HeaderParams.bGPSImgDirectionTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_MapDatum)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 1;
        pEncCtx->HeaderParams.GPSTagLength_MapDatum = NvOsStrlen(pGPSInfo->GPSMapDatum) + 1;
        if(pEncCtx->HeaderParams.GPSTagLength_MapDatum > LEN_VALUE_FIT_IN_TAG)
            Length_Of_GPSIFD += pEncCtx->HeaderParams.GPSTagLength_MapDatum;
        pEncCtx->HeaderParams.bGPSMapDatumTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_ProcessingMethod)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 1;
        pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod = MAX_GPS_PROCESSING_METHOD_IN_BYTES;
        Length_Of_GPSIFD += pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod;
        pEncCtx->HeaderParams.bGPSProcessingMethodTag_Present = NV_TRUE;
    }
    if(pGPSInfo->GPSBitMapInfo & NvMMGPSBitMap_DateStamp)
    {
        pEncCtx->HeaderParams.NoOf_GPS_Tags += 1;
        pEncCtx->HeaderParams.GPSTagLength_DateStamp = GPS_DATESTAMP_LENGTH;
        Length_Of_GPSIFD += pEncCtx->HeaderParams.GPSTagLength_DateStamp;
        pEncCtx->HeaderParams.bGPSDateStampTag_Present = NV_TRUE;
    }
    Length_Of_GPSIFD += (pEncCtx->HeaderParams.NoOf_GPS_Tags * 12);
    return Length_Of_GPSIFD;
}

static NvError
CalcApp1HdrLengthNo1stIFd(NvImageEncHandle hImageEnc,
    NvRmMemHandle hExifInfo,
    NvU32 *pDataLen)
{
    NvError         status = NvSuccess;
    NvEncoderPrivate *pEncCtx = (NvEncoderPrivate *)hImageEnc->pEncoder;
    NvU8            *pMakerNote = NULL;
    NvMMEXIFInfo    *pExifInfo = NULL;
    NvMMGPSInfo     *pGPSInfo = NULL;
    NvMM0thIFDTags  *pEXIFParamSet = &pEncCtx->PriParams.EncParamExif.ExifInfo;

    pEncCtx->HeaderParams.NumofExifTags = TagCount_EXIFIFD;

    if (hExifInfo != NULL)
    {
        pExifInfo = NULL;
        pEncCtx->HeaderParams.NoOf_0IFD_Fields = TagCount_0thIFD;

        status = NvRmMemMap(hExifInfo, 0, sizeof(NvMMEXIFInfo),
                    NVOS_MEM_READ_WRITE, (void **)&pExifInfo);
        if (status != NvSuccess)
            return status;

        pMakerNote = (NvU8 *)pExifInfo->MakerNote;
    }

    if (pEncCtx->PriParams.EncParamGps.Enable)
    {
        // adds GPS IFD Offset Tag
        pGPSInfo = &pEncCtx->PriParams.EncParamGps.GpsInfo;
        pEncCtx->HeaderParams.NoOf_0IFD_Fields = TagCount_0thIFD + 1;
    }

    // APP1
    if (hExifInfo != NULL)
    {
        pEncCtx->HeaderParams.defaultLen = NvOsStrlen(NA_STRING) + 1;
        pEncCtx->HeaderParams.lenImageDescription = NvOsStrlen(pEXIFParamSet->ImageDescription) + 1;
        if (pEncCtx->HeaderParams.lenImageDescription == 1)
            pEncCtx->HeaderParams.lenImageDescription = pEncCtx->HeaderParams.defaultLen;

        pEncCtx->HeaderParams.lenMake = NvOsStrlen(pEXIFParamSet->Make) + 1;
        if (pEncCtx->HeaderParams.lenMake == 1)
            pEncCtx->HeaderParams.lenMake = pEncCtx->HeaderParams.defaultLen;

        pEncCtx->HeaderParams.lenModel = NvOsStrlen(pEXIFParamSet->Model) + 1;
        if (pEncCtx->HeaderParams.lenModel == 1)
            pEncCtx->HeaderParams.lenModel = pEncCtx->HeaderParams.defaultLen;

        pEncCtx->HeaderParams.lenSoftware = NvOsStrlen(pEXIFParamSet->Software) + 1;
        if (pEncCtx->HeaderParams.lenSoftware == 1)
            pEncCtx->HeaderParams.lenSoftware = pEncCtx->HeaderParams.defaultLen;

        pEncCtx->HeaderParams.lenArtist = NvOsStrlen(pEXIFParamSet->Artist) + 1;
        if (pEncCtx->HeaderParams.lenArtist == 1)
            pEncCtx->HeaderParams.lenArtist = pEncCtx->HeaderParams.defaultLen;

        pEncCtx->HeaderParams.lenCopyright = NvOsStrlen(pEXIFParamSet->Copyright) + 1;
        if (pEncCtx->HeaderParams.lenCopyright == 1)
            pEncCtx->HeaderParams.lenCopyright = pEncCtx->HeaderParams.defaultLen;

        pEncCtx->HeaderParams.lenUserComment = NvOsStrlen((pEXIFParamSet->UserComment+8)) + 1 + 8;
        if (pEncCtx->HeaderParams.lenUserComment == 1)
            pEncCtx->HeaderParams.lenUserComment = pEncCtx->HeaderParams.defaultLen;

        pEncCtx->HeaderParams.lenImageUniqueId = MAX_EXIF_IMAGEUNIQUEID_IN_SIZE_IN_BYTES;

        pEncCtx->HeaderParams.lenMakerNote = pExifInfo->SizeofMakerNoteInBytes;
        if (pEncCtx->HeaderParams.lenMakerNote > MAX_EXIF_MAKER_NOTE_SIZE_IN_BYTES)
        {
            return NvError_BadParameter;
        }
        /* if the overhead of encoding throws us over the top,
           then hack off the last few bytes.  In practice, these
           aren't used anyway(s) */
        pEncCtx->HeaderParams.lenMakerNote = MNoteTrimSize(pEncCtx->HeaderParams.lenMakerNote);

        //Calculating length of APP1
        /*
        28 = APP1 Length(2 Bytes) + Exif00(6 Bytes) + TIFF Header(8 Bytes) + No.of 0th IFD fields(2 Bytes) + 1st IFD offset(4 Bytes) +
        No.of Exif IFD fields(2 Bytes) + 2nd IFD offset(4 Bytes)
        */
        *pDataLen = 34 +
                      ((pEncCtx->HeaderParams.NoOf_0IFD_Fields +
                        pEncCtx->HeaderParams.NumofExifTags +
                        TagCount_InterOperabilityIFD) * 12) +
                      ExifVariable_XResolution +
                      ExifVariable_YResolution +
                      ExifVariable_DateandTime +
                      ExifVariable_ExposureTime +
                      ExifVariable_CompressedBitsPerPixel +
                      ExifVariable_ExposureBias +
                      ExifVariable_FNumber +
                      (2 * ExifVariable_DateandTime) +
                      ExifVariable_Focallength +
                      ExifVariable_MaxAperture +
                      ExifVariable_SubjectDistanceRange +
                      ExifVariable_ZoomFactor +
                      pEncCtx->HeaderParams.lenUserComment +
                      pEncCtx->HeaderParams.lenSubjectArea +
                      pEncCtx->HeaderParams.lenImageUniqueId +
                      ExifVariable_ShutterSpeedValue +
                      ExifVariable_ApertureValue +
                      ExifVariable_BrightnessValue;

        if (pEncCtx->HeaderParams.lenImageDescription > LEN_VALUE_FIT_IN_TAG)
            *pDataLen += pEncCtx->HeaderParams.lenImageDescription;

        if (pEncCtx->HeaderParams.lenMake > LEN_VALUE_FIT_IN_TAG)
            *pDataLen += pEncCtx->HeaderParams.lenMake;

        if (pEncCtx->HeaderParams.lenModel > LEN_VALUE_FIT_IN_TAG)
            *pDataLen += pEncCtx->HeaderParams.lenModel;

        if (pEncCtx->HeaderParams.lenSoftware > LEN_VALUE_FIT_IN_TAG)
            *pDataLen += pEncCtx->HeaderParams.lenSoftware;

        if (pEncCtx->HeaderParams.lenArtist  > LEN_VALUE_FIT_IN_TAG)
            *pDataLen += pEncCtx->HeaderParams.lenArtist ;

        if (pEncCtx->HeaderParams.lenCopyright > LEN_VALUE_FIT_IN_TAG)
            *pDataLen += pEncCtx->HeaderParams.lenCopyright;

        if (pEncCtx->HeaderParams.lenMakerNote > LEN_VALUE_FIT_IN_TAG)
            *pDataLen += MNoteEncodeSize(pEncCtx->HeaderParams.lenMakerNote);

        if (InterOperabilityTagLength_Index > LEN_VALUE_FIT_IN_TAG)
            *pDataLen += InterOperabilityTagLength_Index;

        if (pEncCtx->PriParams.EncParamGps.Enable)
        {
            *pDataLen += CalcLengthOfGPSIFD(pEncCtx, pGPSInfo);
        }

        NvRmMemUnmap(hExifInfo,
            (void *)pExifInfo, sizeof(NvMMEXIFInfo));
    }
    else
    {
        *pDataLen = 0;
    }

    return status;
}

NvError
JPEGEncWriteHeader(
    NvImageEncHandle hImageEnc,
    NvU8 *Header)
{
    NvEncoderPrivate *pEncCtx = (NvEncoderPrivate *)hImageEnc->pEncoder;

    NvU8    *pExifBase = Header;
    NvU8    *pMakerNote = NULL;
    NvU8    bFlagMakerNoteAvailable = NV_FALSE;
    NvU32   APP1_Length = 0, Denominator, Numerator, Value, Offset,APP1_Length_NoGPS = 0;
    NvU32   stereoMetadataLength = 0;
    NvError status = NvSuccess;
    NvMM0thIFDTags  *pEXIFParamSet = &pEncCtx->PriParams.EncParamExif.ExifInfo;
    NvMMEXIFInfo    *pExifInfo = NULL;
    NvMMGPSInfo     *pGPSInfo = NULL;
    NvRmMemHandle   hExifInfo = pEncCtx->HeaderParams.hExifInfo;

    CalcApp1HdrLengthNo1stIFd(hImageEnc, hExifInfo, &APP1_Length);

    if (hExifInfo != NULL)
    {
        pExifInfo = NULL;
        status = NvRmMemMap(hExifInfo, 0, sizeof(NvMMEXIFInfo), NVOS_MEM_READ_WRITE, (void **)&pExifInfo );
        if (status != NvSuccess)
            goto JPEGEncWriteHeader_cleanup;

        pMakerNote = (NvU8 *)pExifInfo->MakerNote;
        bFlagMakerNoteAvailable = NV_TRUE;
    }

    if (pEncCtx->PriParams.EncParamGps.Enable)
    {
        pGPSInfo = &pEncCtx->PriParams.EncParamGps.GpsInfo;
        pEncCtx->HeaderParams.Length_Of_GPS_IFD = CalcLengthOfGPSIFD(pEncCtx, pGPSInfo);
        APP1_Length_NoGPS = APP1_Length - pEncCtx->HeaderParams.Length_Of_GPS_IFD;
    }

    // APP1
    if(hExifInfo != NULL)
    {
        // SOI
        *(Header++) = M_MARKER;
        *(Header++) = M_SOI;

        *(Header++) = M_MARKER;
        *(Header++) = M_APP1;

        *(Header++) = (NvU8)(APP1_Length >> 8); // length of APP1
        *(Header++) = (NvU8)(APP1_Length & 0xFF);

        *(Header++) = 'E';
        *(Header++) = 'x';
        *(Header++) = 'i';
        *(Header++) = 'f';
        *(Header++) = 0;
        *(Header++) = 0;

        // TIFF Header
        *(Header++) = 0x4D; // Big endian type storage
        *(Header++) = 0x4D;
        *(Header++) = 0;
        *(Header++) = 42;
        Offset   = 8; //8 -> Offset to the 0th IFD from the TIFF header
        *(Header++) = (NvU8)(Offset >> 24); // offset of the 0th IFD
        *(Header++) = (NvU8)((Offset >> 16) & 0xFF);
        *(Header++) = (NvU8)((Offset >> 8) & 0xFF);
        *(Header++) = (NvU8)(Offset & 0xFF);

        // 0th IFD
        *(Header++) = 0x00;
        *(Header++) = pEncCtx->HeaderParams.NoOf_0IFD_Fields; //Number of 0th IFD fields

        /* set offset for the first tag
        14 = TIFF Header(8 Bytes) + No.of 0th IFD fields(2 Bytes) + 1st IFD offset(4 Bytes)
        */
        Offset = 14 + (pEncCtx->HeaderParams.NoOf_0IFD_Fields * 12);

        if (pEncCtx->HeaderParams.lenImageDescription <= LEN_VALUE_FIT_IN_TAG)
        {
            EMIT_TAG_NO_OFFSET(Header, Tag_ImageTitle_High, Tag_ImageTitle_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenImageDescription,
                pEXIFParamSet->ImageDescription[0],
                pEXIFParamSet->ImageDescription[1],
                pEXIFParamSet->ImageDescription[2],
                pEXIFParamSet->ImageDescription[3]);
        }
        else
        {
            EMIT_TAG_WITH_OFFSET(Header, Tag_ImageTitle_High,
                Tag_ImageTitle_Low, IfdTypes_Ascii,
                pEncCtx->HeaderParams.lenImageDescription, Offset);
            Offset += pEncCtx->HeaderParams.lenImageDescription;
        }

        if (pEncCtx->HeaderParams.lenMake <= LEN_VALUE_FIT_IN_TAG)
        {
            EMIT_TAG_NO_OFFSET(Header, Tag_Make_High, Tag_Make_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenMake,
                pEXIFParamSet->Make[0], pEXIFParamSet->Make[1],
                pEXIFParamSet->Make[2], pEXIFParamSet->Make[3]);
        }
        else
        {
            EMIT_TAG_WITH_OFFSET(Header, Tag_Make_High, Tag_Make_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenMake, Offset);
            Offset += pEncCtx->HeaderParams.lenMake;
        }

        if (pEncCtx->HeaderParams.lenModel <= LEN_VALUE_FIT_IN_TAG)
        {
            EMIT_TAG_NO_OFFSET(Header, Tag_Model_High, Tag_Model_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenModel,
                pEXIFParamSet->Model[0], pEXIFParamSet->Model[1],
                pEXIFParamSet->Model[2], pEXIFParamSet->Model[3]);
        }
        else
        {
            EMIT_TAG_WITH_OFFSET(Header, Tag_Model_High, Tag_Model_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenModel, Offset);
            Offset += pEncCtx->HeaderParams.lenModel;
        }

        Value = (NvU32)pEncCtx->PriParams.Orientation.orientation;
        EMIT_TAG_NO_OFFSET(Header, Tag_Orientation_High, Tag_Orientation_Low,
            IfdTypes_Short, 1, (NvU8)((Value >> 8) & 0xFF),
            (NvU8)(Value & 0xFF), 0, 0 );

        EMIT_TAG_WITH_OFFSET(Header, Tag_XResolution_High, Tag_XResolution_Low,
            IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_XResolution;

        EMIT_TAG_WITH_OFFSET(Header, Tag_YResolution_High, Tag_YResolution_Low,
            IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_YResolution;

        // default 2 = Inches
        EMIT_TAG_NO_OFFSET(Header, Tag_ResolutionUnit_High,
            Tag_ResolutionUnit_Low, IfdTypes_Short, 1, 0x00, 0x02, 0, 0 );

        if (pEncCtx->HeaderParams.lenSoftware <= LEN_VALUE_FIT_IN_TAG)
        {
            EMIT_TAG_NO_OFFSET(Header, Tag_Software_High, Tag_Software_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenSoftware,
                pEXIFParamSet->Software[0], pEXIFParamSet->Software[1],
                pEXIFParamSet->Software[2], pEXIFParamSet->Software[3]);
        }
        else
        {
            EMIT_TAG_WITH_OFFSET(Header, Tag_Software_High, Tag_Software_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenSoftware, Offset);
            Offset += pEncCtx->HeaderParams.lenSoftware;
        }


        EMIT_TAG_WITH_OFFSET(Header, Tag_DateTime_High, Tag_DateTime_Low,
            IfdTypes_Ascii, ExifVariable_DateandTime, Offset);
        Offset += ExifVariable_DateandTime;


        if (pEncCtx->HeaderParams.lenArtist <= LEN_VALUE_FIT_IN_TAG)
        {
            EMIT_TAG_NO_OFFSET(Header, Tag_Artist_High, Tag_Artist_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenArtist,
                pEXIFParamSet->Artist[0], pEXIFParamSet->Artist[1],
                pEXIFParamSet->Artist[2], pEXIFParamSet->Artist[3]);
        }
        else
        {
            EMIT_TAG_WITH_OFFSET(Header, Tag_Artist_High, Tag_Artist_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenArtist, Offset);
            Offset += pEncCtx->HeaderParams.lenArtist;
        }

        {
            NvU8 tempVal;

            switch (pEncCtx->PriParams.InputFormat)
            {
            case NV_IMG_JPEG_COLOR_FORMAT_422:
            case NV_IMG_JPEG_COLOR_FORMAT_422T:
                tempVal = 0x02;
                break;
            default:
                tempVal = 0x01;
                break;
            }

            tempVal = 0x01;

            EMIT_TAG_NO_OFFSET(Header, Tag_YCbCrPosition_High,
                Tag_YCbCrPosition_Low, IfdTypes_Short, 1, 0x00, tempVal, 0, 0 );
        }

        if (pEncCtx->HeaderParams.lenCopyright <= LEN_VALUE_FIT_IN_TAG)
        {
            EMIT_TAG_NO_OFFSET(Header, Tag_Copyright_High, Tag_Copyright_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenCopyright,
                pEXIFParamSet->Copyright[0], pEXIFParamSet->Copyright[1],
                pEXIFParamSet->Copyright[2], pEXIFParamSet->Copyright[3]);
        }
        else
        {
            EMIT_TAG_WITH_OFFSET(Header, Tag_Copyright_High, Tag_Copyright_Low,
                IfdTypes_Ascii, pEncCtx->HeaderParams.lenCopyright, Offset);
            Offset += pEncCtx->HeaderParams.lenCopyright;
        }

        EMIT_TAG_WITH_OFFSET(Header, Tag_ExifIfdPointer_High,
            Tag_ExifIfdPointer_Low, IfdTypes_Long, 1, Offset);
        Offset += pEncCtx->HeaderParams.lenCopyright;

        if (pEncCtx->PriParams.EncParamGps.Enable)
        {
            Offset = APP1_Length_NoGPS-8; //temp-  APP1 Length(2 Bytes) - Exif00(6 Bytes)
            EMIT_TAG_WITH_OFFSET(Header, Tag_GPSIFDOffset_High,
                Tag_GPSIFDOffset_Low, IfdTypes_Long, 1, Offset);
        }

        *(Header++) = 0x00; // 1st IFD offset
        *(Header++) = 0x00;
        *(Header++) = 0x00;
        *(Header++) = 0x00;

        // Values of above fields (0th IFD) if the value is longer than the 4 bytes.
        if (pEncCtx->HeaderParams.lenImageDescription > LEN_VALUE_FIT_IN_TAG)
        {
            TAG_SAVE_VAL_STR(Header, pEXIFParamSet->ImageDescription, pEncCtx->HeaderParams.lenImageDescription);
        }

        if (pEncCtx->HeaderParams.lenMake > LEN_VALUE_FIT_IN_TAG)
        {
            TAG_SAVE_VAL_STR(Header, pEXIFParamSet->Make, pEncCtx->HeaderParams.lenMake);
        }

        if (pEncCtx->HeaderParams.lenModel > LEN_VALUE_FIT_IN_TAG)
        {
            TAG_SAVE_VAL_STR(Header, pEXIFParamSet->Model, pEncCtx->HeaderParams.lenModel);
        }

        // Hardcode to 72 dpi.
        TAG_SAVE_VAL_RESOLUTION(Header);

        if (pEncCtx->HeaderParams.lenSoftware > LEN_VALUE_FIT_IN_TAG)
        {
            TAG_SAVE_VAL_STR(Header, pEXIFParamSet->Software, pEncCtx->HeaderParams.lenSoftware);
        }

        TAG_SAVE_VAL_STR(Header, pEXIFParamSet->DateTime, ExifVariable_DateandTime);

        if (pEncCtx->HeaderParams.lenArtist > LEN_VALUE_FIT_IN_TAG)
        {
            TAG_SAVE_VAL_STR(Header, pEXIFParamSet->Artist, pEncCtx->HeaderParams.lenArtist);
        }

        if (pEncCtx->HeaderParams.lenCopyright > LEN_VALUE_FIT_IN_TAG)
        {
            TAG_SAVE_VAL_STR(Header, pEXIFParamSet->Copyright, pEncCtx->HeaderParams.lenCopyright);
        }

        // Exif IFD
        *(Header++) = 0x00; // Number of Exif Fields
        *(Header++) = pEncCtx->HeaderParams.NumofExifTags;

        /*
        20 = TIFF Header(8 Bytes) + No.of 0th IFD fields(2 Bytes) + 1st IFD offset(4 Bytes) +
        No.of 1st IFD fields(2 Bytes) + 2nd IFD offset(4 Bytes)
        */
        Offset = 20 +
                 (pEncCtx->HeaderParams.NoOf_0IFD_Fields * 12) +
                 ExifVariable_XResolution +
                 ExifVariable_YResolution +
                 ExifVariable_DateandTime +
                 (pEncCtx->HeaderParams.NumofExifTags * 12);

        if (pEncCtx->HeaderParams.lenImageDescription > LEN_VALUE_FIT_IN_TAG)
            Offset += pEncCtx->HeaderParams.lenImageDescription;

        if (pEncCtx->HeaderParams.lenMake > LEN_VALUE_FIT_IN_TAG)
            Offset += pEncCtx->HeaderParams.lenMake;

        if (pEncCtx->HeaderParams.lenModel > LEN_VALUE_FIT_IN_TAG)
            Offset += pEncCtx->HeaderParams.lenModel;

        if (pEncCtx->HeaderParams.lenSoftware > LEN_VALUE_FIT_IN_TAG)
            Offset += pEncCtx->HeaderParams.lenSoftware;

        if (pEncCtx->HeaderParams.lenArtist  > LEN_VALUE_FIT_IN_TAG)
            Offset += pEncCtx->HeaderParams.lenArtist ;

        if (pEncCtx->HeaderParams.lenCopyright > LEN_VALUE_FIT_IN_TAG)
            Offset += pEncCtx->HeaderParams.lenCopyright;

        EMIT_TAG_WITH_OFFSET(Header, Tag_ExposureTime_High, Tag_ExposureTime_Low, IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_ExposureTime;

        EMIT_TAG_WITH_OFFSET(Header, Tag_FNumber_High, Tag_FNumber_Low, IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_FNumber;

        Value = pExifInfo->ExposureProgram;
        EMIT_TAG_NO_OFFSET(Header, Tag_ExposureProgram_High, Tag_ExposureProgram_Low, IfdTypes_Short, 1, (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF), 0, 0 );

        Value = pExifInfo->GainISO;
        EMIT_TAG_NO_OFFSET(Header, Tag_GainISO_High, Tag_GainISO_Low, IfdTypes_Short, 1, (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF), 0, 0 );

        EMIT_TAG_NO_OFFSET(Header, Tag_ExifVersion_High, Tag_ExifVersion_Low, IfdTypes_Undefined, 4, '0', '2', '2', '0' );

        EMIT_TAG_WITH_OFFSET(Header, Tag_DateTimeOriginal_High, Tag_DateTimeOriginal_Low, IfdTypes_Ascii, ExifVariable_DateandTime, Offset);
        Offset += ExifVariable_DateandTime;

        EMIT_TAG_WITH_OFFSET(Header, Tag_DateTimeDigitized_High, Tag_DateTimeDigitized_Low, IfdTypes_Ascii, ExifVariable_DateandTime, Offset);
        Offset += ExifVariable_DateandTime;     // same as Date and time

        // all cases ecept uncompress sRGB supported.
        EMIT_TAG_NO_OFFSET(Header, Tag_ComponentConfig_High, Tag_ComponentConfig_Low, IfdTypes_Undefined, 4, 1, 2, 3, 0 );

        EMIT_TAG_WITH_OFFSET(Header, Tag_CompressedBitsPerPixel_High, Tag_CompressedBitsPerPixel_Low, IfdTypes_Rational, 0, Offset);
        Offset += ExifVariable_CompressedBitsPerPixel;

        EMIT_TAG_WITH_OFFSET(Header, Tag_ExposureBias_High, Tag_ExposureBias_Low, IfdTypes_Srational, 1, Offset);
        Offset += ExifVariable_ExposureBias;

        EMIT_TAG_WITH_OFFSET(Header, Tag_MaxAperture_High, Tag_MaxAperture_Low, IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_MaxAperture;

        EMIT_TAG_WITH_OFFSET(Header, Tag_SubjectDistanceRange_High, Tag_SubjectDistanceRange_Low, IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_SubjectDistanceRange;

        Value = pExifInfo->MeteringMode;
        EMIT_TAG_NO_OFFSET(Header, Tag_MeteringMode_High, Tag_MeteringMode_Low, IfdTypes_Short, 1, 0, (NvU8)(Value & 0xFF), 0, 0 );

        Value    = pExifInfo->LightSource;;
        EMIT_TAG_NO_OFFSET(Header, Tag_LightSource_High, Tag_LightSource_Low, IfdTypes_Short, 1, 0, (NvU8)(Value & 0xFF), 0, 0 );

        Value = pExifInfo->FlashUsed;
        EMIT_TAG_NO_OFFSET(Header, Tag_Flash_High, Tag_Flash_Low, IfdTypes_Short, 1, 0, (NvU8)(Value & 0xFF), 0, 0 );

        EMIT_TAG_WITH_OFFSET(Header, Tag_FocalLength_High, Tag_FocalLength_Low, IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_Focallength;

        EMIT_TAG_NO_OFFSET(Header, Tag_FalshpixVersion_High, Tag_FalshpixVersion_Low, IfdTypes_Undefined, 4, '0', '1', '0', '0' );

        EMIT_TAG_NO_OFFSET(Header, Tag_ColorSpace_High, Tag_ColorSpace_Low, IfdTypes_Short, 1, 0x00, 0x01, 0x00, 0x00 );

        pEncCtx->HeaderParams.WH_offset = Header - pEncCtx->HeaderParams.pbufHeader;

        Value = pEncCtx->PriParams.Resolution.width;
        EMIT_TAG_NO_OFFSET(Header, Tag_PixelXDim_High, Tag_PixelXDim_Low, IfdTypes_Long, 1, (NvU8)(Value >> 24), (NvU8)((Value >> 16) & 0xFF), (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF) );

        Value = pEncCtx->PriParams.Resolution.height;
        EMIT_TAG_NO_OFFSET(Header, Tag_PixelYDim_High, Tag_PixelYDim_Low, IfdTypes_Long, 1, (NvU8)(Value >> 24), (NvU8)((Value >> 16) & 0xFF), (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF) );

        if (pEncCtx->PriParams.EncParamGps.Enable)
        {
            Value = APP1_Length_NoGPS-8-6 - (TagCount_InterOperabilityIFD*12);
        }
        else
        {
            Value = APP1_Length - 8 - 6 - (TagCount_InterOperabilityIFD*12);
        }
        if (InterOperabilityTagLength_Index > LEN_VALUE_FIT_IN_TAG)
            Value -= InterOperabilityTagLength_Index;

        EMIT_TAG_NO_OFFSET(Header, Tag_InterOperabilityIFD_High, Tag_InterOperabilityIFD_Low, IfdTypes_Long, 1, (NvU8)(Value >> 24), (NvU8)((Value >> 16) & 0xFF), (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF) );

        Value = pEXIFParamSet->filesource;//Default = 3 -DSC recorded image
        EMIT_TAG_NO_OFFSET(Header, Tag_FileSource_High, Tag_FileSource_Low, IfdTypes_Undefined, 1, (NvU8)(Value & 0xFF), (NvU8)((Value >> 8) & 0xFF), (NvU8)((Value >> 16) & 0xFF), (NvU8)((Value >> 24) & 0xFF) );

        Value = 0; // default 0 = normal process
        EMIT_TAG_NO_OFFSET(Header, Tag_CustomRendered_High, Tag_CustomRendered_Low, IfdTypes_Short, 1, (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF), 0, 0 );

        Value = pExifInfo->ExposureMode; // default 0 = Auto exposure
        EMIT_TAG_NO_OFFSET(Header, Tag_ExposureMode_High, Tag_ExposureMode_Low, IfdTypes_Short, 1, (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF), 0, 0 );

        Value = pExifInfo->WhiteBalance; // default None, 0 = Auto White balance
        EMIT_TAG_NO_OFFSET(Header, Tag_WhiteBalance_High, Tag_WhiteBalance_Low, IfdTypes_Short, 1, (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF), 0, 0 );

        EMIT_TAG_WITH_OFFSET(Header, Tag_DigitalZoomRatio_High, Tag_DigitalZoomRatio_Low, IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_ZoomFactor;

        Value = pExifInfo->SceneCaptureType; //default 0 = Standard
        EMIT_TAG_NO_OFFSET(Header, Tag_SceneCaptureType_High, Tag_SceneCaptureType_Low, IfdTypes_Short, 1, (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF), 0, 0 );

        EMIT_TAG_WITH_OFFSET(Header, Tag_ImageUniqueId_High, Tag_ImageUniqueId_Low, IfdTypes_Ascii, pEncCtx->HeaderParams.lenImageUniqueId, Offset);
        Offset += pEncCtx->HeaderParams.lenImageUniqueId;

        EMIT_TAG_WITH_OFFSET(Header, Tag_ShutterSpeed_High, Tag_ShutterSpeed_Low, IfdTypes_Srational, 1, Offset);
        Offset += ExifVariable_ShutterSpeedValue;

        EMIT_TAG_WITH_OFFSET(Header, Tag_Aperture_High, Tag_Aperture_Low, IfdTypes_Rational, 1, Offset);
        Offset += ExifVariable_ApertureValue;

        EMIT_TAG_WITH_OFFSET(Header, Tag_Brightness_High, Tag_Brightness_Low, IfdTypes_Srational, 1, Offset);
        Offset += ExifVariable_BrightnessValue;

        EMIT_TAG_WITH_OFFSET(Header, Tag_UserComment_High, Tag_UserComment_Low, IfdTypes_Undefined, pEncCtx->HeaderParams.lenUserComment, Offset);
        Offset += pEncCtx->HeaderParams.lenUserComment;

        Value = pExifInfo->Sharpness;
        EMIT_TAG_NO_OFFSET(Header, Tag_Sharpness_High, Tag_Sharpness_Low, IfdTypes_Short, 1, (NvU8)((Value >> 8) & 0xFF), (NvU8)(Value & 0xFF), 0, 0 );
        EMIT_TAG_NO_OFFSET(Header, Tag_SensingMethod_High, Tag_SensingMethod_Low, IfdTypes_Short, 1, (NvU8)((2 >> 8) & 0xFF), (NvU8)(2 & 0xFF), 0, 0 );
        EMIT_TAG_WITH_OFFSET(Header, Tag_SubjectArea_High, Tag_SubjectArea_Low, IfdTypes_Short, 4, Offset);
        Offset += pEncCtx->HeaderParams.lenSubjectArea;

        if (bFlagMakerNoteAvailable == NV_TRUE)
        {
            //Maker note
            if (pEncCtx->HeaderParams.lenMakerNote <= LEN_VALUE_FIT_IN_TAG)
            {
                EMIT_TAG_NO_OFFSET(Header, Tag_MakerNote_High, Tag_MakerNote_Low, IfdTypes_Undefined, pEncCtx->HeaderParams.lenMakerNote,
                                   pMakerNote[0], pMakerNote[1], pMakerNote[2], pMakerNote[3]);
            }
            else
            {
                EMIT_TAG_WITH_OFFSET(Header, Tag_MakerNote_High, Tag_MakerNote_Low, IfdTypes_Undefined, MNoteEncodeSize(pEncCtx->HeaderParams.lenMakerNote), Offset);
                Offset += MNoteEncodeSize(pEncCtx->HeaderParams.lenMakerNote);
            }
        }

        *(Header++) = 0x00; // Next IFD offset
        *(Header++) = 0x00;
        *(Header++) = 0x00;
        *(Header++) = 0x00;

        // Here we need to write values of above fields (Exif IFD) if the value is longer than the 4 bytes.

        Numerator = pExifInfo->ExposureTimeNumerator;// Exposure Time
        Denominator = pExifInfo->ExposureTimeDenominator;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

        Numerator = pExifInfo->FNumberNumerator;// FNumber
        Denominator = pExifInfo->FNumberDenominator;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

        TAG_SAVE_VAL_STR(Header, pEXIFParamSet->DateTimeOriginal, ExifVariable_DateandTime);

        TAG_SAVE_VAL_STR(Header, pEXIFParamSet->DateTimeDigitized, ExifVariable_DateandTime);

        Numerator = 4;  //CompressedBitsPerPixel
        Denominator = 1;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);


        TAG_SAVE_VAL_NUM_DEN(Header, pExifInfo->ExposureBiasNumerator, pExifInfo->ExposureBiasDenominator);


        Numerator = pExifInfo->MaxApertureNumerator;// Max Aperture
        Denominator = pExifInfo->MaxApertureDenominator;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);


        Numerator = pExifInfo->SubjectDistanceNumerator;    //Subject Distance Range
        Denominator = pExifInfo->SubjectDistanceDenominator;
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

        Numerator = pExifInfo->FocalLengthNumerator;// FocalLength
        Denominator = pExifInfo->FocalLengthDenominator;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);


        Numerator = pExifInfo->ZoomFactorNumerator;// Zoom Factor
        Denominator = pExifInfo->ZoomFactorDenominator;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

        TAG_SAVE_VAL_STR(Header, pExifInfo->ImageUniqueID, pEncCtx->HeaderParams.lenImageUniqueId); //Unique image id from maker

        Numerator = pExifInfo->ShutterSpeedValueNumerator;// Shutter speed value in APEX
        Denominator = pExifInfo->ShutterSpeedValueDenominator;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

        Numerator = pExifInfo->ApertureValueNumerator;// aperture value in APEX
        Denominator =  pExifInfo->ApertureValueDenominator;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

        Numerator = pExifInfo->BrightnessValueNumerator;// brightness value in APEX
        Denominator = pExifInfo->BrightnessValueDenominator;
        RelativePrime( &Numerator, &Denominator);
        TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);


        // user comment
        NvOsMemcpy((NvU8 *)Header, pEXIFParamSet->UserComment, pEncCtx->HeaderParams.lenUserComment);
        Header += pEncCtx->HeaderParams.lenUserComment;

        // subject area
        NvOsMemcpy((NvU8 *)Header, pExifInfo->SubjectArea, pEncCtx->HeaderParams.lenSubjectArea);
        Header += pEncCtx->HeaderParams.lenSubjectArea;

        // Maker note
        if ( (bFlagMakerNoteAvailable == NV_TRUE) && (pEncCtx->HeaderParams.lenMakerNote > LEN_VALUE_FIT_IN_TAG))
        {
            status = encodeMakerNote(Header, pMakerNote, pEncCtx->HeaderParams.lenMakerNote);
            if (NvSuccess != status) goto JPEGEncWriteHeader_cleanup;

            Header += MNoteEncodeSize(pEncCtx->HeaderParams.lenMakerNote);
        }

        //Interoperability IFD Tags.

        *(Header++) = 0x00; // Number of INteroperability IFD Fields
        *(Header++) = TagCount_InterOperabilityIFD;

        if (pEncCtx->PriParams.EncParamGps.Enable)
            Offset =  APP1_Length_NoGPS-8 ;//2 for no.of GPS fields+4 for NextIFDOffset)
        else
            Offset =  APP1_Length-8 ;

        if (InterOperabilityTagLength_Index > LEN_VALUE_FIT_IN_TAG)
            Offset -= InterOperabilityTagLength_Index;

        if (InterOperabilityTagLength_Index <= LEN_VALUE_FIT_IN_TAG)
        {
            EMIT_TAG_NO_OFFSET(Header, Tag_InteroperabilityIndex_High,
                Tag_InteroperabilityIndex_Low, IfdTypes_Ascii,
                InterOperabilityTagLength_Index,
                pEncCtx->PriParams.EncParamIFD.IFDInfo.Index[0],
                pEncCtx->PriParams.EncParamIFD.IFDInfo.Index[1],
                pEncCtx->PriParams.EncParamIFD.IFDInfo.Index[2],
                pEncCtx->PriParams.EncParamIFD.IFDInfo.Index[3]);
        }
        else
        {
            EMIT_TAG_WITH_OFFSET(Header, Tag_InteroperabilityIndex_High, Tag_InteroperabilityIndex_Low, IfdTypes_Ascii, InterOperabilityTagLength_Index, Offset);
            Offset += InterOperabilityTagLength_Index;
        }

        EMIT_TAG_NO_OFFSET(Header, Tag_InteroperabilityVersion_High, Tag_InteroperabilityVersion_Low, IfdTypes_Undefined, 4, '0', '1', '1', '0' );

        *(Header++) = 0x00; //Next offset
        *(Header++) = 0x00;
        *(Header++) = 0x00;
        *(Header++) = 0x00;

        if (InterOperabilityTagLength_Index > LEN_VALUE_FIT_IN_TAG)
        {
            NvOsStrncpy((char*)Header, pEncCtx->PriParams.EncParamIFD.IFDInfo.Index, InterOperabilityTagLength_Index);//values greaterthan 4 bytes
            Header += InterOperabilityTagLength_Index;
        }

        if (pEncCtx->PriParams.EncParamGps.Enable)
        {
            // GPS IFD
            *(Header++) = 0x00; // Number of GPS Fields
            *(Header++) = pEncCtx->HeaderParams.NoOf_GPS_Tags;

            {
                /*
                ** Calculating Offset
                ** 2 for no.of GPS fields+4 for NextIFDOffset)
                */
                Offset =  APP1_Length_NoGPS - 8 + 2 + 4 +
                    pEncCtx->HeaderParams.NoOf_GPS_Tags * 12;
            }

            {
                NvU8 v1, v2, v3, v4;
                if ( pGPSInfo->GPSVersionID)
                {
                    Value    = pGPSInfo->GPSVersionID;
                    v1 = (NvU8)(Value >> 24);
                    v2 = (NvU8)((Value >> 16) & 0xFF);
                    v3 = (NvU8)((Value >> 8) & 0xFF);
                    v4 = (NvU8)(Value & 0xFF);
                }
                else
                {
                    v1 = 0x02;
                    v2 = 0x02;
                    v3 = 0x00;
                    v4 = 0x00;
                }
                EMIT_TAG_NO_OFFSET(Header, Tag_GPSVersionID_High, Tag_GPSVersionID_Low, IfdTypes_Byte, 4, v1, v2, v3, v4 );
            }


            if (pEncCtx->HeaderParams.bGPSLatitudeTag_Present == NV_TRUE)
            {
                NvU8 v1=0, v2=0, v3=0, v4=0;

                v1 = pGPSInfo->GPSLatitudeRef[0];
                v2 = pGPSInfo->GPSLatitudeRef[1];

                EMIT_TAG_NO_OFFSET(Header, Tag_GPSLatitudeRef_High, Tag_GPSLatitudeRef_Low,
                                   IfdTypes_Ascii, 2, v1, v2, v3, v4 );


                EMIT_TAG_WITH_OFFSET(Header, Tag_GPSLatitude_High, Tag_GPSLatitude_Low, IfdTypes_Rational, 3, Offset);
                Offset += GPSTagLength_Latitude;
            }

            if (pEncCtx->HeaderParams.bGPSLongitudeTag_Present == NV_TRUE)
            {
                NvU8 v1=0, v2=0, v3=0, v4=0;

                v1 = pGPSInfo->GPSLongitudeRef[0];
                v2 = pGPSInfo->GPSLongitudeRef[1];
                //v3 = pGPSInfo->GPSLongitudeRef[2];
                //v4 = pGPSInfo->GPSLongitudeRef[3];

                EMIT_TAG_NO_OFFSET(Header, Tag_GPSLongitudeRef_High, Tag_GPSLongitudeRef_Low, IfdTypes_Ascii, 2, v1, v2, v3, v4 );


                EMIT_TAG_WITH_OFFSET(Header, Tag_GPSLongitude_High, Tag_GPSLongitude_Low, IfdTypes_Rational, 3, Offset);
                Offset += GPSTagLength_Longitude;
            }

            if (pEncCtx->HeaderParams.bGPSAltitudeTag_Present == NV_TRUE)
            {
                NvU8 v1=0, v2=0, v3=0, v4=0;

                if ( pGPSInfo->GPSAltitudeRef)
                {
                    Value    = pGPSInfo->GPSAltitudeRef;
                    v1 = (NvU8)(Value >> 24);
                    v2 = (NvU8)((Value >> 16) & 0xFF);
                    v3 = (NvU8)((Value >> 8) & 0xFF);
                    v4 = (NvU8)(Value & 0xFF);
                }

                EMIT_TAG_NO_OFFSET(Header, Tag_GPSAltitudeRef_High, Tag_GPSAltitudeRef_Low, IfdTypes_Byte, 1, v1, v2, v3, v4 );

                EMIT_TAG_WITH_OFFSET(Header, Tag_GPSAltitude_High, Tag_GPSAltitude_Low, IfdTypes_Rational, 1, Offset);
                Offset += GPSTagLength_Altitude;
            }
            if (pEncCtx->HeaderParams.bGPSTimeStampTag_Present == NV_TRUE)
            {
                EMIT_TAG_WITH_OFFSET(Header, Tag_GPSTimeStamp_High, Tag_GPSTimeStamp_Low, IfdTypes_Rational, 3, Offset);
                Offset += GPSTagLength_TimeStamp;
            }

            if (pEncCtx->HeaderParams.bGPSSatellitesTag_Present == NV_TRUE)
            {
                if (pEncCtx->HeaderParams.GPSTagLength_Satellites <= LEN_VALUE_FIT_IN_TAG)
                {
                    EMIT_TAG_NO_OFFSET(Header, Tag_GPSSatellites_High, Tag_GPSSatellites_Low, IfdTypes_Ascii,
                                       pEncCtx->HeaderParams.GPSTagLength_Satellites,
                                       pGPSInfo->GPSSatellites[0], pGPSInfo->GPSSatellites[1],
                                       pGPSInfo->GPSSatellites[2], pGPSInfo->GPSSatellites[3]);
                }
                else
                {
                    EMIT_TAG_WITH_OFFSET(Header, Tag_GPSSatellites_High, Tag_GPSSatellites_Low, IfdTypes_Ascii,
                                       pEncCtx->HeaderParams.GPSTagLength_Satellites, Offset);
                    Offset += pEncCtx->HeaderParams.GPSTagLength_Satellites;
                }
            }

            if (pEncCtx->HeaderParams.bGPSStatusTag_Present == NV_TRUE)
            {
                NvU8 v1=0, v2=0, v3=0, v4=0;

                v1 = pGPSInfo->GPSStatus[0];
                v2 = pGPSInfo->GPSStatus[1];
                //v3 = pGPSInfo->GPSStatus[2];
                //v4 = pGPSInfo->GPSStatus[3];

                EMIT_TAG_NO_OFFSET(Header, Tag_GPSStatus_High, Tag_GPSStatus_Low, IfdTypes_Ascii, 2, v1, v2, v3, v4 );
            }

            if (pEncCtx->HeaderParams.bGPSMeasureModeTag_Present == NV_TRUE)
            {
                NvU8 v1=0, v2=0, v3=0, v4=0;

                v1 = pGPSInfo->GPSMeasureMode[0];
                v2 = pGPSInfo->GPSMeasureMode[1];
                //v3 = pGPSInfo->GPSMeasureMode[2];
                //v4 = pGPSInfo->GPSMeasureMode[3];

                EMIT_TAG_NO_OFFSET(Header, Tag_GPSMeasureMode_High, Tag_GPSMeasureMode_Low, IfdTypes_Ascii, 2, v1, v2, v3, v4 );
            }

            if (pEncCtx->HeaderParams.bGPSDOPTag_Present == NV_TRUE)
            {
                EMIT_TAG_WITH_OFFSET(Header, Tag_GPSDOP_High, Tag_GPSDOP_Low, IfdTypes_Rational, 1, Offset);
                Offset +=  GPSTagLength_DOP;
            }

            if (pEncCtx->HeaderParams.bGPSImgDirectionTag_Present == NV_TRUE)
            {
                EMIT_TAG_NO_OFFSET(Header, Tag_GPSImgDirectionRef_High, Tag_GPSImgDirectionRef_Low, IfdTypes_Ascii,
                                   GPSTagLength_ImgDirectionRef, pGPSInfo->GPSImgDirectionRef[0], pGPSInfo->GPSImgDirectionRef[1],0,0);

                EMIT_TAG_WITH_OFFSET(Header, Tag_GPSImgDirection_High, Tag_GPSImgDirection_Low, IfdTypes_Rational, 1, Offset);
                Offset +=  GPSTagLength_ImgDirection;
            }


            if (pEncCtx->HeaderParams.bGPSMapDatumTag_Present == NV_TRUE)
            {
                if (pEncCtx->HeaderParams.GPSTagLength_MapDatum <= LEN_VALUE_FIT_IN_TAG)
                {
                    EMIT_TAG_NO_OFFSET(Header, Tag_GPSMapDatum_High, Tag_GPSMapDatum_Low, IfdTypes_Ascii,
                                       pEncCtx->HeaderParams.GPSTagLength_MapDatum,
                                       pGPSInfo->GPSMapDatum[0], pGPSInfo->GPSMapDatum[1],
                                       pGPSInfo->GPSMapDatum[2], pGPSInfo->GPSMapDatum[3]);
                }
                else
                {
                    EMIT_TAG_WITH_OFFSET(Header, Tag_GPSMapDatum_High, Tag_GPSMapDatum_Low, IfdTypes_Ascii,
                                       pEncCtx->HeaderParams.GPSTagLength_MapDatum, Offset);
                    Offset +=  pEncCtx->HeaderParams.GPSTagLength_MapDatum;
                }
            }

            if (pEncCtx->HeaderParams.bGPSDateStampTag_Present == NV_TRUE)
            {
                EMIT_TAG_WITH_OFFSET(Header, Tag_GPSDateStamp_High, Tag_GPSDateStamp_Low, IfdTypes_Ascii,
                                       pEncCtx->HeaderParams.GPSTagLength_DateStamp, Offset);
                Offset += pEncCtx->HeaderParams.GPSTagLength_DateStamp;
            }

            if (pEncCtx->HeaderParams.bGPSProcessingMethodTag_Present == NV_TRUE)
            {
                if (pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod <= LEN_VALUE_FIT_IN_TAG)
                {
                    EMIT_TAG_NO_OFFSET(Header, Tag_GPSProcessingMethod_High, Tag_GPSProcessingMethod_Low, IfdTypes_Undefined,
                                       pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod,
                                       pGPSInfo->GPSProcessingMethod[0], pGPSInfo->GPSProcessingMethod[1],
                                       pGPSInfo->GPSProcessingMethod[2], pGPSInfo->GPSProcessingMethod[3]);
                }
                else
                {
                    EMIT_TAG_WITH_OFFSET(Header, Tag_GPSProcessingMethod_High, Tag_GPSProcessingMethod_Low, IfdTypes_Undefined,
                                       pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod, Offset);
                    Offset += pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod;
                }
            }

            //nextoffset
            *(Header++) = 0x00; // Next IFD offset
            *(Header++) = 0x00;
            *(Header++) = 0x00;
            *(Header++) = 0x00;

            //Value longer than 4 Bytes of GPS IFD

            //GPSLatitude Value
            if (pEncCtx->HeaderParams.bGPSLatitudeTag_Present == NV_TRUE)
            {
                Numerator = pGPSInfo->GPSLatitudeNumerator[0];// GPSLatitudeValue
                Denominator = pGPSInfo->GPSLatitudeDenominator[0];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

                Numerator = pGPSInfo->GPSLatitudeNumerator[1];
                Denominator = pGPSInfo->GPSLatitudeDenominator[1];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

                Numerator = pGPSInfo->GPSLatitudeNumerator[2];
                Denominator = pGPSInfo->GPSLatitudeDenominator[2];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);
            }

            if (pEncCtx->HeaderParams.bGPSLongitudeTag_Present == NV_TRUE)
            {
                Numerator = pGPSInfo->GPSLongitudeNumerator[0];// LongitudeValue
                Denominator = pGPSInfo->GPSLongitudeDenominator[0];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

                Numerator = pGPSInfo->GPSLongitudeNumerator[1];
                Denominator = pGPSInfo->GPSLongitudeDenominator[1];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

                Numerator = pGPSInfo->GPSLongitudeNumerator[2];
                Denominator = pGPSInfo->GPSLongitudeDenominator[2];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);
            }

            if (pEncCtx->HeaderParams.bGPSAltitudeTag_Present == NV_TRUE)
            {
                Numerator = pGPSInfo->GPSAltitudeNumerator; //GPSAltitudeValue
                Denominator = pGPSInfo->GPSAltitudeDenominator;
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);
            }

            if (pEncCtx->HeaderParams.bGPSTimeStampTag_Present == NV_TRUE)
            {
                Numerator = pGPSInfo->GPSTimeStampNumerator[0];//GPSTimeStampValue
                Denominator = pGPSInfo->GPSTimeStampDenominator[0];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

                Numerator = pGPSInfo->GPSTimeStampNumerator[1];
                Denominator = pGPSInfo->GPSTimeStampDenominator[1];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);

                Numerator = pGPSInfo->GPSTimeStampNumerator[2];
                Denominator = pGPSInfo->GPSTimeStampDenominator[2];
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);
            }

            if ( (pEncCtx->HeaderParams.bGPSSatellitesTag_Present == NV_TRUE) &&
                 (pEncCtx->HeaderParams.GPSTagLength_Satellites > LEN_VALUE_FIT_IN_TAG) )
            {
                NvOsStrncpy((char*)Header, pGPSInfo->GPSSatellites, pEncCtx->HeaderParams.GPSTagLength_Satellites);////GPSSatellitesValue
                Header += pEncCtx->HeaderParams.GPSTagLength_Satellites;
            }

            if (pEncCtx->HeaderParams.bGPSDOPTag_Present == NV_TRUE)
            {
                Numerator = pGPSInfo->GPSDOPNumerator;//GPSDOPValue
                Denominator = pGPSInfo->GPSDOPDenominator;
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);
            }

            if (pEncCtx->HeaderParams.bGPSImgDirectionTag_Present == NV_TRUE)
            {
                Numerator = pGPSInfo->GPSImgDirectionNumerator;//GPSImgDirection
                Denominator = pGPSInfo->GPSImgDirectionDenominator;
                if (Denominator==0) Denominator=1;
                TAG_SAVE_VAL_NUM_DEN(Header, Numerator, Denominator);
            }

            if ( (pEncCtx->HeaderParams.bGPSMapDatumTag_Present == NV_TRUE) &&
                 (pEncCtx->HeaderParams.GPSTagLength_MapDatum > LEN_VALUE_FIT_IN_TAG) )
            {
                NvOsStrncpy((char*)Header, pGPSInfo->GPSMapDatum, pEncCtx->HeaderParams.GPSTagLength_MapDatum);////GPSMapDatumValue
                Header += pEncCtx->HeaderParams.GPSTagLength_MapDatum; //GPSMapDatum
            }

            if (pEncCtx->HeaderParams.bGPSDateStampTag_Present == NV_TRUE)
            {
                NvOsStrncpy((char*)Header, pGPSInfo->GPSDateStamp, pEncCtx->HeaderParams.GPSTagLength_DateStamp);////GPSDateStampValue
                Header += pEncCtx->HeaderParams.GPSTagLength_DateStamp;
            }

            if ( (pEncCtx->HeaderParams.bGPSProcessingMethodTag_Present == NV_TRUE) &&
                 (pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod > LEN_VALUE_FIT_IN_TAG) )
            {
                NvOsMemcpy((NvU8 *)Header, pEncCtx->PriParams.EncParamGpsProc.GpsProcMethodInfo.GPSProcessingMethod,
                                   pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod);
                Header += pEncCtx->HeaderParams.GPSTagLength_GPSProcessingMethod;
            }
        }

        pEncCtx->HeaderParams.headeroffset = (Header - pExifBase);
        if (pEncCtx->ThumbParams.EnableThumbnail)
            Insert1stIFD(hImageEnc, pExifInfo);
    }
    else
    {
        // Inserting SOI in case Exif is not specified by the app
        *(Header++) = M_MARKER;
        *(Header++) = M_SOI;
        pEncCtx->HeaderParams.headeroffset = (Header - pExifBase);
    }
/*
    // After SOI and APP1 are done, write stereo markers before DQT (if needed)
    status = NvJPEGEncWriteStereoMetadata(pJpegEncCtx, Header, &stereoMetadataLength);
    if (NvSuccess != status)
    {
        return status;
    }

    Header += stereoMetadataLength;
    pJPEGParamSet->headeroffset += stereoMetadataLength;
*/
    return NvSuccess;

JPEGEncWriteHeader_cleanup:
    if (pExifInfo)
        NvRmMemUnmap(hExifInfo,(void *)pExifInfo, sizeof(NvMMEXIFInfo));

    return status;
}


NvError
GetHeaderLen(
    NvImageEncHandle hImageEnc,
    NvRmMemHandle hExifInfo,
    NvU32 *pDataLen)
{
    NvEncoderPrivate *pEncCtx = (NvEncoderPrivate *)hImageEnc->pEncoder;

    NvError status = CalcApp1HdrLengthNo1stIFd(hImageEnc, hExifInfo, pDataLen);
    if (NvSuccess != status)
        return status;

    if (hExifInfo != NULL)
    {
        *pDataLen += 4; // SOI for Primary + APP1 Marker
        if (pEncCtx->ThumbParams.EnableThumbnail)
           // Number Of Interoperability (2 byte) + Next IFD Offset (4 byte)
           *pDataLen += 6 + (TagCount_1stIFD * 12) +
                        ExifVariable_XResolution +
                        ExifVariable_YResolution;
    }
    else
        *pDataLen += 2; // SOI marker only for Primary


    return NvSuccess;
}
