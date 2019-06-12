/*
 * Copyright (c) 2008-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nv_3gp_writer.h"
#include "nvos.h"
#include "nvmm_common.h"

// if OPTIMAL_WRITE_SIZE is less than MINIMAL_OPTIMAL_WRITE_SIZE then
// we don't consider optimal write size we consider as per old way of writing
#define MINIMAL_OPTIMAL_WRITE_SIZE (64 * 1024)
//input buffer max size to be used before
#define TEMP_INPUT_BUFF_SIZE 250
#define STTS_OFFSET (8)
#define STSZ_OFFSET (TOTAL_ENTRIESPERATOM*8)
#define STSC_OFFSET (TOTAL_ENTRIESPERATOM*12)
#define CO64_OFFSET (TOTAL_ENTRIESPERATOM*24)
#define TOTAL_RESV_SIZE (6 * 1024 * 1024)
// worst case pending audio (128K) + worst case pending vidoe(128K)
#define RESV_BUFFER_SIZE ( TOTAL_ENTRIESPERATOM * SIZEOFATOMS * 2 )
#define MIN_INITIAL_RESV_SIZE (256 * 1024 ) // Actual 6 MB, loopscntr = 24, 24 * 256 KB
#define TOTALATOMSSIZE (SIZEOFATOMS * TOTAL_ENTRIESPERATOM)
#define ACC_FRAC_TRIGGER 1000
    //Give thresholds in seconds.
#define MOOV_THRESHOLD 20
#define MOOF_THRESHOLD 10
#define MAX_VIDEO_FRAMES_MOOV (MOOV_THRESHOLD * 30)  //5 min video@ 30fps
#define TIMESTAMP_RESOLUTION  10000000
#define NEXT_TRACK_ID 3
#define STTS_INIT_DONE 1
#define UNUSED_ESDS_DESCRIPTOR_LENGTH 71
/* size - 4 bytes,boxtype - 4 bytes,
 * versionflag = 4 bytes,language-2 bytes,
 */
#define ALBUM_USER_SIZE (14)
static NvU8 AlbumUserData[ALBUM_USER_SIZE]=
{
    0x0F,0x00,0x00,0x00,'a','l','b','m',0x00,0x00,0x00,0x00,0x00,0x00
};

/**  size - 4 bytes, boxtype - 4 bytes,
  *  versionflag = 4 bytes,language-2 bytes
  */
#define AUTHOR_USER_SIZE (14)
static NvU8 AuthorUserData[AUTHOR_USER_SIZE]=
{
    0x0E,0x00,0x00,0x00,'a','u','t','h',0x00,0x00,0x00,0x00,0x00,0x00
};

/* size - 4 bytes,boxtype - 4 bytes,
  * versionflag = 4 bytes,classificationEntity-4 bytes
  * classificationTable = 2 bytes, language-2 bytes
  */
#define CLASSIFICATION_USER_SIZE (20)
static NvU8 ClassificationUserData[CLASSIFICATION_USER_SIZE]=
{
    0x14,0x00,0x00,0x00,'c','l','s','f',0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

/* size - 4 bytes, boxtype - 4 bytes,
  * versionflag = 4 bytes, language-2 bytes
  */
#define COPYRIGHT_USER_SIZE (14)
static NvU8 CopyrightUserData[COPYRIGHT_USER_SIZE]=
{
    0x0E,0x00,0x00,0x00,'c','p','r','t',0x00,0x00,0x00,0x00,0x00,0x00
};

/* size - 4 bytes, boxtype - 4 bytes,versionflag = 4 bytes,
  * language-2 bytes
  */
#define DESCRIPTION_USER_SIZE (14)
static NvU8 DescriptionUserData[DESCRIPTION_USER_SIZE]=
{
    0x0E,0x00,0x00,0x00,'d','s','c','p',0x00,0x00,0x00,0x00,0x00,0x00
};

/* size - 4 bytes, boxtype - 4 bytes, versionflag = 4 bytes,
  * language-2 bytes
  */
#define GENRE_USER_SIZE  (14)
static NvU8 GenreUserData[GENRE_USER_SIZE]=
{
    0x0E,0x00,0x00,0x00,'g','n','r','e',0x00,0x00,0x00,0x00,0x00,0x00
};

/* size - 4 bytes, boxtype - 4 bytes, versionflag = 4 bytes,
  * language-2 bytes
  */
#define KEYWORD_USER_SIZE (14)
static NvU8 KeywordUserData[KEYWORD_USER_SIZE]=
{
    0x0E,0x00,0x00,0x00,'k','y','w','d',0x00,0x00,0x00,0x00,0x00,0x00
};


/* size - 4 bytes, boxtype - 4 bytes, versionflag = 4 bytes,
  * language-2 bytes
  */
#define LOCATION_USER_SIZE (14)
static NvU8 LocationUserData[LOCATION_USER_SIZE]=
{
    0x0E,0x00,0x00,0x00,'l','o','c','i',0x00,0x00,0x00,0x00,0x00,0x00
};

/* size - 4 bytes, boxtype - 4 bytes, versionflag = 4 bytes,
  * language-2 bytes
  */
#define PERFORMER_USER_SIZE (14)
static NvU8 PerformerUserData[PERFORMER_USER_SIZE]=
{
    0x0E,0x00,0x00,0x00,'p','e','r','f',0x00,0x00,0x00,0x00,0x00,0x00
};

/* size - 4 bytes, boxtype - 4 bytes, versionflag = 4 bytes,
  * rating entity-4 bytes, rating criteria - 4bytes, language-2 bytes
  */
#define RATING_USER_SIZE (22)
static NvU8 RatingUserData[RATING_USER_SIZE]=
{
    0x16,0x00,0x00,0x00,'r','t','n','g',0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00
};

/* size - 4 bytes, boxtype - 4 bytes, versionflag = 4 bytes,
  * language-2 bytes
  */
#define TITLE_USER_SIZE (14)
static NvU8 TitleUserData[TITLE_USER_SIZE] =
{
    0x0E,0x00,0x00,0x00,'t','i','t','l',0x00,0x00,0x00,0x00,0x00,0x00
};

static void
NvMM3GpMuxResvSpaceFileWriteThread(
    void *Arg)
{
    NvError Status = NvSuccess;
    Nv3gpWriter *Nv3GPMux = (Nv3gpWriter *)Arg;
    NvMMFileAVSWriteMsg Message;
    NvBool ShutDown = NV_FALSE;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "++NvMM3GpMuxResvSpaceFileWriteThread\n"));
    while (NV_FALSE == ShutDown)
    {
        if(0 == (NvMMQueueGetNumEntries(Nv3GPMux->MsgQueueResv)))
            NvOsSemaphoreWait(Nv3GPMux->ResvWriteSema);

        if(NvSuccess == NvMMQueueDeQ(Nv3GPMux->MsgQueueResv, &Message) )
        {
            while(Message.LoopCntr)
            {
                if(Message.ThreadShutDown)
                {
                    ShutDown = NV_TRUE;
                    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "--NvMM3GpMuxResvSpaceFileWriteThread\n"));
                    break;
                }
                else if(Message.pData != NULL)
                {
                    Status = NvOsFwrite(Nv3GPMux->FileResvHandle, (CPbyte *)(Message.pData), Message.DataSize);
                    if(Status != NvSuccess)
                    {
                        ShutDown = NV_TRUE;
                        Nv3GPMux->ResvFileWriteFailed = NV_TRUE;
                        Nv3GPMux->ResvThreadStatus = Status;
                        break;
                    }
                    Message.LoopCntr--;
                    if(Message.LoopCntr)
                    {
                        // sleep for 50 ms only when using fwrite contrinuously to dump huge data
                        NvOsSleepMS(50);
                    }
                }
            }
            NvOsSemaphoreSignal(Nv3GPMux->ResvWriteDoneSema);
       }
    }
}

/**
  * Layout in temporary files: audiM.dat , videM.dat for audio and video
  *                                      ___________________________________
  *                                     |                                                             |
  * STTS Table 1000 Entries |                 (0 to 7999)                            |
  * (sample count (32-bit),  |                                                             |
  *  sample data (32-bit))    |-----------------------------------|
  *                                     |                                                             |
  * STSZ Box 1000 Entries   |                 (8000 to 11999)                     |
  * (puSizeEntry(32-bit))     |                                                             |
  *                                     |-----------------------------------|
  *                                     |                                                             |
  * STSC Table 1000 enties  |                 (12000 to 23999)                   |
  * (FirstChunk (32-bit),      |                                                             |
  *  Samplesperchunk(32),  |-----------------------------------|
  *  SamplDescrIdx(32))      |                                                             |
  *                                     |                                                             |
  * CO64 1000 Entries         |                  (24000 to 31999)                   |
  * (puChunkOffset(64-bit)) |                                                             |
  *                                      ___________________________________
  *                                     |                                                             |
  * STTS Table 1000 Entries |                 (32000 to 39999)                   |
  *                                     |                                                             |
  *                                     |-----------------------------------|
  *                                     |                                                             |
  * STSZ Box 1000 Entries   |                 (40000 to 43999)                   |
  *                                     |                                                             |
  *                                     |-----------------------------------|
  *                                     |                                                             |
  * STSC Table 1000 enties  |                 (44000 to 55999)                   |
  *                                     |                                                             |
  *                                     |-----------------------------------|
  *                                     |                                                             |
  * CO64 1000 Entries         |                  (56000 to 63999)                   |
  *                                     |                                                             |
  *                                      ___________________________________
  *
  * this sequence repeats till the end of file and at last values from this files will
  * dumped into main file after all mdata is dumped.
  *
  * For STTS: a special case
  *     The valid entry will not start from 0th byte but from 8th byte till 7999th byte.
  * So valid entries in first chunk will be 8000 - 8 = 7992 bytes.
  * Applicable only for first chunk
  * Next sets will be normal i.e. 8000 bytes 1000 entries
  * For eg: Second chunk valid entry starts from 32000th byte and end at 39999the byte.
  *
  */
NvError
NvMM3GpMuxMuxWriteDataFromBuffer(
    NvU8 *pBuffer,
    NvU32 BytesToBeWritten,
    CP_PIPETYPE *pPipe,
    CPhandle cphandle)
{
    NvError Status = NvSuccess;
    NvU32 WriteSize=0;

    while(BytesToBeWritten > 0)
    {
        if( (BytesToBeWritten > OPTIMAL_WRITE_SIZE ) &&
            (OPTIMAL_WRITE_SIZE >= MINIMAL_OPTIMAL_WRITE_SIZE))
        {
            WriteSize = OPTIMAL_WRITE_SIZE;
        }else
        {
            WriteSize = BytesToBeWritten;
        }
        NVMM_CHK_ERR((NvError) pPipe->Write(cphandle, (CPbyte *)(pBuffer), WriteSize));
        pBuffer += WriteSize;
        BytesToBeWritten -= WriteSize;
    }

cleanup:
    return Status;
}

static void
NvMM3GpMuxReadStaggerDumpLinear(
    NvU32 TotalValidEntries,
    NvU32 AtomOffsetInTemp,
    NvU32 AtomSize,
    NvU8 *pSrc,
    NvU8 **pDest)
{
    NvU32 AtomEntriesCnt = 0;
    NvU32 ChunkCntr = 0;
    NvU32 EntriesToBeRead = 0;
    NvU32 AtomDataToRead = 0;
    NvU32 TotalNoOfEntriesPerAtom = 0;
    NvU32 ResetStartSecIterationSTSS = 0;
    NvU32 OffsetStscEntries = 0;
    NvU32 TotalEntriesRead = 0;
    NvU32 SttsEntries = 0;
    NvU8 * DestOrg = *pDest;
    NvU8 *tmp = pSrc;
    NvBool bMoveToNextChunk = NV_FALSE;

    TotalNoOfEntriesPerAtom = TOTAL_ENTRIESPERATOM;
    pSrc = pSrc + AtomOffsetInTemp + ACTUAL_ATOM_ENTRIES;

    if (STTS_OFFSET == AtomOffsetInTemp)
        SttsEntries = *(NvU32 *)(tmp + 4);
    if (STSC_OFFSET == AtomOffsetInTemp || CO64_OFFSET == AtomOffsetInTemp)
        OffsetStscEntries = *(NvU32 *)(tmp);

    /*
      * In temp file STTS atom data is present such a way that. For the first set of 8K values, the atom start at 8th postion.
      * From second set 8K values the atom start at 0th position. This is becoz of STTS_INIT_DONE.
      * See processspeech and processvideo in STTS atom populate.
    */
    if(AtomOffsetInTemp == STTS_OFFSET)
    {
        // Iniitally there will be one entry less than 1000 i.e. 7992 bytes special case
        TotalNoOfEntriesPerAtom -= 1;
        // resetting start of second set of STSS sample to start of boundard
        ResetStartSecIterationSTSS = STTS_OFFSET;
    }
    AtomEntriesCnt = 0;
    ChunkCntr = 0;
    EntriesToBeRead = TotalValidEntries;

    while(AtomEntriesCnt < TotalValidEntries)
    {
        if(EntriesToBeRead <= TOTAL_ENTRIESPERATOM)
            AtomDataToRead = EntriesToBeRead;
        else // greater than first chunk, then read one full chunk of stts i.e. 1000 entries (TOTAL_ENTRIESPERATOM)
            AtomDataToRead = TotalNoOfEntriesPerAtom;

        TotalNoOfEntriesPerAtom = TOTAL_ENTRIESPERATOM;
        if (STTS_OFFSET == AtomOffsetInTemp)
        {
            AtomDataToRead = SttsEntries;
            TotalEntriesRead += AtomDataToRead;
            if (TotalEntriesRead < TotalValidEntries)
                bMoveToNextChunk = NV_TRUE;
        }
        if (STSC_OFFSET == AtomOffsetInTemp || CO64_OFFSET == AtomOffsetInTemp)
        {
            AtomDataToRead = OffsetStscEntries;
            TotalEntriesRead += AtomDataToRead;
            if (TotalEntriesRead < TotalValidEntries)
                bMoveToNextChunk = NV_TRUE;
        }
        /**
         * Read from the temp file and extract them into AV Buffer
         */
        NvOsMemcpy(*pDest, pSrc, AtomSize* AtomDataToRead);
        *pDest = *pDest + (AtomSize* AtomDataToRead);
        /**
          * update atomEntriesCnt for next iteration
        */
        AtomEntriesCnt += AtomDataToRead;
        EntriesToBeRead -= AtomDataToRead;
        ChunkCntr++;

        if (AtomEntriesCnt == TotalValidEntries)
            break;

        /**
          * If atom entries are more than 1000 jump to second location which is after multiple of 32000 bytes (TOTALATOMSSIZE)
          */
        if(TotalValidEntries > TOTAL_ENTRIESPERATOM ||
            (NV_TRUE == bMoveToNextChunk))
        {
            pSrc = pSrc + TOTALATOMSSIZE + ACTUAL_ATOM_ENTRIES;
            if (STSC_OFFSET == AtomOffsetInTemp ||
                CO64_OFFSET == AtomOffsetInTemp ||
                STTS_OFFSET == AtomOffsetInTemp)
            {
                tmp += TOTALATOMSSIZE + ACTUAL_ATOM_ENTRIES;
                OffsetStscEntries = *(NvU32 *)tmp;
                SttsEntries = *(NvU32 *)(tmp + 4);
                bMoveToNextChunk = NV_FALSE;
            }

            if(ResetStartSecIterationSTSS == STTS_OFFSET)
            {
                // do not offset by 8 bytes from 2nd iteration onwards i.e. from 2nd set of entries starting
                pSrc -= STTS_OFFSET;
                ResetStartSecIterationSTSS = 0;
            }
            if(pSrc >= DestOrg)
                break;
        }
    }
}

static void
NvMM3GpMuxEvaluateFinalParametrs(
    NvMM3GpMuxInputParam *pInpParam,
    Nv3gpWriter *Nv3GPMux)
{
    NvU32 Count = 0;
    NvU32 Tempval = 0;
    NvU8 Temp[4];
    NvU32 i=0;
    NvU8 Max = 0;
    NvU32 Duration = 0;
    NvU32 BufferSizeDB = 0;

    //update sample count of ctts box
    Nv3GPMux->MuxCTTSBox[SOUND_TRACK].SampleCount =
    //update length (of box), sample count and size for amr/qcelp stsz box
    Nv3GPMux->MuxSTSZBox[SOUND_TRACK].SampleCount =
    Nv3GPMux->SpeechFrameCnt;

    Nv3GPMux->MuxCTTSBox[VIDEO_TRACK].SampleCount = Nv3GPMux->VideoFrameCnt;

    if( (pInpParam->SpeechAudioFlag == 3) ||
        (pInpParam->SpeechAudioFlag == 4) ||
        (pInpParam->SpeechAudioFlag == 2) )
    {
        Nv3GPMux->MuxSTSZBox[SOUND_TRACK].SampleSize = 0;
    }

    if(pInpParam->Mp4H263Flag == 0)
    {
        //update size field of esds box
        Nv3GPMux->MuxESDSBox[VIDEO_TRACK].FullBox.Box.BoxSize +=
            Nv3GPMux->MuxESDSBox[VIDEO_TRACK].DataLen3;

        //update size field of mp4v box
        Nv3GPMux->MuxMP4VBox.BoxWidthHeight.Box.BoxSize +=
            Nv3GPMux->MuxESDSBox[VIDEO_TRACK].FullBox.Box.BoxSize;

        //update size field of stsd box
        Nv3GPMux->MuxSTSDBox[VIDEO_TRACK].FullBox.Box.BoxSize +=
            Nv3GPMux->MuxMP4VBox.BoxWidthHeight.Box.BoxSize;
    }
    else if(pInpParam->Mp4H263Flag == 2)
    {
        //update size field of avcC box
        for(i=0; i < Nv3GPMux->MuxAVCCBox.NumOfSequenceParameterSets ; i++)
        {
            Nv3GPMux->MuxAVCCBox.Box.BoxSize += 2;
            Nv3GPMux->MuxAVCCBox.Box.BoxSize += Nv3GPMux->MuxAVCCBox.pPspsTable->SequenceParameterSetLength;
        }
        for(i=0; i < Nv3GPMux->MuxAVCCBox.NumOfPictureParameterSets; i++)
        {
            Nv3GPMux->MuxAVCCBox.Box.BoxSize += 2;
            Nv3GPMux->MuxAVCCBox.Box.BoxSize+= Nv3GPMux->MuxAVCCBox.pPpsTable->PictureParameterSetLength;
        }
        //update size field of avc1 box
        Nv3GPMux->MuxAVC1Box.BoxWidthHeight.Box.BoxSize += Nv3GPMux->MuxAVCCBox.Box.BoxSize;

        //update size field of stsd box
        Nv3GPMux->MuxSTSDBox[VIDEO_TRACK].FullBox.Box.BoxSize += Nv3GPMux->MuxAVC1Box.BoxWidthHeight.Box.BoxSize;
    }
    //update size field of stbl box i is SOUND_TRACK or VIDEO_TRACK
    for(i = 0; i< MAX_TRACK ; i++)
    {
        Nv3GPMux->MuxSTBLBox[i].Box.BoxSize +=
            Nv3GPMux->MuxSTSDBox[i].FullBox.Box.BoxSize +
            Nv3GPMux->MuxSTSCBox[i].FullBox.Box.BoxSize +
            Nv3GPMux->MuxSTTSBox[i].FullBox.Box.BoxSize +
            Nv3GPMux->MuxCTTSBox[i].FullBox.Box.BoxSize +
            Nv3GPMux->MuxSTSZBox[i].FullBox.Box.BoxSize;

        if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
            Nv3GPMux->MuxSTBLBox[i].Box.BoxSize += Nv3GPMux->MuxSTCOBox[i].FullBox.Box.BoxSize;
        else
            Nv3GPMux->MuxSTBLBox[i].Box.BoxSize += Nv3GPMux->MuxCO64Box[i].FullBox.Box.BoxSize;
        if(VIDEO_TRACK == i)
            Nv3GPMux->MuxSTBLBox[i].Box.BoxSize += Nv3GPMux->MuxSTSSBox[i].FullBox.Box.BoxSize;

        //update size field of minf box
        Nv3GPMux->MuxMINFBox[i].Box.BoxSize +=
        Nv3GPMux->MuxDINFBox[i].Box.BoxSize +
        Nv3GPMux->MuxSTBLBox[i].Box.BoxSize;
        if(SOUND_TRACK == i)
            Nv3GPMux->MuxMINFBox[i].Box.BoxSize += Nv3GPMux->MuxSMHDBox.FullBox.Box.BoxSize;
        else if(VIDEO_TRACK == i)
            Nv3GPMux->MuxMINFBox[i].Box.BoxSize += Nv3GPMux->MuxVMHDBox.FullBox.Box.BoxSize;
    }

    //update duration field of mdhd box
    if (Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration > Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration)
        Duration = Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration;
    else
        Duration = Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration;

    //update size field of mdia box
    for(i = 0; i< MAX_TRACK ; i++)
    {
        Nv3GPMux->MuxMDIABox[i].Box.BoxSize +=
            Nv3GPMux->MuxMDHDBox[i].FullBox.Box.BoxSize+
            Nv3GPMux->MuxHDLRBox[i].FullBox.Box.BoxSize+
            Nv3GPMux->MuxMINFBox[i].Box.BoxSize;

        //update duration field of tkhd box
        Nv3GPMux->MuxTKHDBox[i].Duration = Nv3GPMux->MuxMDHDBox[i].Duration;
    }

    //////Fill elst box fields and update edts box size/////
    if(pInpParam->AudioVideoFlag  == NvMM_AV_AUDIO_VIDEO_BOTH)
    {
        for(i = 0; i< MAX_TRACK ; i++)
        {
            Tempval = 0;
            //Entry_count = 1 : 'empty' edit list - to offset the beginning of the track
            Count = Nv3GPMux->MuxELSTBox[i].EntryCount++;

            Tempval = Duration - Nv3GPMux->MuxTKHDBox[i].Duration;

            LITTLE_TO_BIG_ENDIAN(Temp,Tempval);
            Nv3GPMux->MuxELSTBox[i].pElstTable[Count-1].SegmentDuration = NV_LE_TO_INT_32(&Temp[0]);
            // -1 in hex is a palindrome
            Nv3GPMux->MuxELSTBox[i].pElstTable[Count-1].MediaTime = -1;
            Nv3GPMux->MuxELSTBox[i].pElstTable[Count-1].MediaRateInteger = 0x0100;
            Nv3GPMux->MuxELSTBox[i].pElstTable[Count-1].MediaRateFraction = 0x0100;
            Nv3GPMux->MuxELSTBox[i].FullBox.Box.BoxSize+= 12;

            //Entry_count = 2 : Actual track
            Tempval = Nv3GPMux->MuxMDHDBox[i].Duration;
            LITTLE_TO_BIG_ENDIAN(Temp,Tempval);
            Nv3GPMux->MuxELSTBox[i].pElstTable[Count].SegmentDuration = NV_LE_TO_INT_32(&Temp[0]);
            Nv3GPMux->MuxELSTBox[i].pElstTable[Count].MediaTime = 0;
            Nv3GPMux->MuxELSTBox[i].pElstTable[Count].MediaRateInteger = 0x0100;
            Nv3GPMux->MuxELSTBox[i].pElstTable[Count].MediaRateFraction = 0x0100;
            Nv3GPMux->MuxELSTBox[i].FullBox.Box.BoxSize += 12;

            /////update size of edts box////
            Nv3GPMux->MuxEDTSBox[i].Box.BoxSize += Nv3GPMux->MuxELSTBox[i].FullBox.Box.BoxSize;

            //update size field of trak box
            Nv3GPMux->MuxTRAKBox[i].Box.BoxSize += Nv3GPMux->MuxEDTSBox[i].Box.BoxSize;
        }
    }

    for(i = 0; i< MAX_TRACK ; i++)
    {
        Nv3GPMux->MuxTRAKBox[i].Box.BoxSize +=
            Nv3GPMux->MuxTKHDBox[i].FullBox.Box.BoxSize+
            Nv3GPMux->MuxMDIABox[i].Box.BoxSize;
    }
    //update duration field of mvhd box
    Nv3GPMux->MuxMVHDBox.Duration = Duration;

    //update size field of moov box
    Nv3GPMux->MuxMOOVBox.Box.BoxSize +=  Nv3GPMux->MuxMVHDBox.FullBox.Box.BoxSize;

    if ( (pInpParam->SpeechAudioFlag == 3) || (pInpParam->SpeechAudioFlag == 4) )
    {
        //update parameters of esds box
        //Extract the parameters from the ratemaptable
        for (i = 10; i > 5; i--)
        {
            if (Nv3GPMux->RateMapTable[2*(i-6) + 1] != 0)
            {
                Nv3GPMux->NumRates++;
                Max = Nv3GPMux->RateMapTable[2*(i-6) + 1];
            }
        }
        /*  Theoritically assigning max bit rate to avg bit rate is incorrect!
        This has been done because quicktime introduces distortion
        in the (otherwise correct - the parser works fine on it) output .3g2 file.
        */
        if (Nv3GPMux->VARrate)
            BufferSizeDB = (NvU32)(Max+1);
        else
        {
            BufferSizeDB = Nv3GPMux->SpeechSampleSize;
            Nv3GPMux->NumRates = 0;
        }
        Nv3GPMux->MuxESDSBox[SOUND_TRACK].BufferSizeDB = BufferSizeDB;
        Nv3GPMux->MuxESDSBox[SOUND_TRACK].AvgBitrate = Nv3GPMux->MuxESDSBox[SOUND_TRACK].MaxBitrate = (NvU32)((Max+1)*8*50);
    }

    if( (NvMM_AV_AUDIO_PRESENT == pInpParam->AudioVideoFlag || NvMM_AV_AUDIO_VIDEO_BOTH == pInpParam->AudioVideoFlag ) &&
        (1 == Nv3GPMux->FAudioDataToBeDumped) )
    {
        Nv3GPMux->MuxMOOVBox.Box.BoxSize += Nv3GPMux->MuxTRAKBox[SOUND_TRACK].Box.BoxSize;
    }
    if( (NvMM_AV_VIDEO_PRESENT == pInpParam->AudioVideoFlag || NvMM_AV_AUDIO_VIDEO_BOTH == pInpParam->AudioVideoFlag ) &&
        (1 == Nv3GPMux->FVideoDataToBeDumped) )
    {
        Nv3GPMux->MuxMOOVBox.Box.BoxSize+= Nv3GPMux->MuxTRAKBox[VIDEO_TRACK].Box.BoxSize;
    }

    //put udta in moov box.
    Nv3GPMux->MuxMOOVBox.Box.BoxSize += Nv3GPMux->MuxUDTABox.Box.BoxSize;

    //increment file offset
    if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
        Nv3GPMux->FileOffset32 += Nv3GPMux->MuxMOOVBox.Box.BoxSize;
    else
        Nv3GPMux->FileOffset += Nv3GPMux->MuxMOOVBox.Box.BoxSize;
}

static void
NvMM3GpMuxCopyAtom(
    NvU32 BoxSize,
    NvU8 *Start,
    NvU32 *OffSet,
    NvU8 Ch1,
    NvU8 Ch2,
    NvU8 Ch3,
    NvU8 Ch4)
{
    LITTLE_TO_BIG_ENDIAN((Start + (*OffSet)),BoxSize);
    *OffSet += sizeof(NvU32);
    COPY_FOUR_BYTES((Start + (*OffSet)), Ch1,Ch2,Ch3,Ch4);
    *OffSet += sizeof(NvU32);
}

static void
NvMM3GpMuxConvertToBigEndian(
    NvU8 *pTemp,
    NvU32 *OffSet,
    NvU32 TempData)
{
    LITTLE_TO_BIG_ENDIAN((pTemp + (*OffSet)),TempData);
    *OffSet += sizeof(NvU32);
}

static void
NvMM3GpMuxGetBoxName(
    NvU8 *pTemp,
    NvU32 *OffSet,
    AtomBasicBox BoxType)
{
    GET_BOX_NAME( (pTemp + (*OffSet)),BoxType);
    *OffSet += sizeof(NvU32);
}

static NvError
NvMM3GpMuxCopyMp4AVAtom(
    Nv3gpWriter *Nv3GPMux,
    NvU32 *CurrentPointer,
    NvU8 *pTemp,
    CP_PIPETYPE *pPipe,
        CPhandle Cphandle,
    NvU32 TrackType)
{
    NvError Status=NvSuccess;
    NvU32 TempData =0;
    NvU8 i =0;
    NvU32 SamplingFreq=8000;
    NvU16 SpeechFramesPerSec = 50;

     //mp4a box for qcelp
    if(TrackType == SOUND_TRACK)
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxMP4ABox.BoxIndexTime.Box.BoxSize,pTemp, CurrentPointer, 'm', 'p', '4', 'a');
    else
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxMP4VBox.BoxWidthHeight.Box.BoxSize,pTemp, CurrentPointer, 'm', 'p', '4', 'v');
    //reserved 6 bytes
    *CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0,0,0,0x1);
    *CurrentPointer += sizeof(NvU32);
    //reserved
    *CurrentPointer += sizeof(NvU32);
    *CurrentPointer += sizeof(NvU32);
    if(TrackType == SOUND_TRACK)
    {
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0,0x2,0,0x10);
    }
    *CurrentPointer += sizeof(NvU32);
    //reserved
    *CurrentPointer += sizeof(NvU32);
    if(TrackType == SOUND_TRACK)
    {
        TempData = Nv3GPMux->MuxMP4ABox.BoxIndexTime.TimeScale;//assigned as SamplingFreq'cy
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF),0,0);
        *CurrentPointer += sizeof(NvU32);
     }
    else
    {
        TempData = Nv3GPMux->MuxMP4VBox.BoxWidthHeight.Width;
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF));
        *CurrentPointer += sizeof(NvU8) * 2;
        TempData = Nv3GPMux->MuxMP4VBox.BoxWidthHeight.Height;
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF));
        *CurrentPointer += sizeof(NvU8) * 2;
    }
    if(TrackType == VIDEO_TRACK)
    {
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0, 0x48,0,0);
        *CurrentPointer += sizeof(NvU32);
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0, 0x48,0,0);
        *CurrentPointer += sizeof(NvU32);
        *CurrentPointer += sizeof(NvU32);
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),0, 1);
        *CurrentPointer += sizeof(NvU8) * 2;
        //reserved
        *CurrentPointer += sizeof(NvU32) * 8;
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0, 0x24,0xFF,0xFF);
        *CurrentPointer += sizeof(NvU32);
        NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)pTemp, *CurrentPointer));
        pTemp = Nv3GPMux->pTempBuff;
        NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
        *CurrentPointer = 0;
    }
    //esds box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxESDSBox[TrackType].FullBox.Box.BoxSize,pTemp, CurrentPointer, 'e', 's', 'd', 's');
    //just skip
    *CurrentPointer += sizeof(NvU32);

        //ES_Descr Tag
    pTemp[(*CurrentPointer)] = (NvU8)0x03;
    (*CurrentPointer)++;
    if(TrackType == SOUND_TRACK)
    {
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),(NvU8)0x81,(NvU8)0x3B);
        *CurrentPointer += sizeof(NvU8)*2;
        TempData = Nv3GPMux->MuxESDSBox[TrackType].ES_ID;
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),
            (NvU8)((TempData>>8) & 0xFF),
            (NvU8)((TempData) & 0xFF),
             0,
            (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DecoderConfigDescrTag);
        *CurrentPointer += sizeof(NvU32);
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),
            0x81,
            0x32,
            (NvU8)Nv3GPMux->MuxESDSBox[TrackType].ObjectTypeIndication,
            (NvU8)Nv3GPMux->MuxESDSBox[TrackType].StreamType);
        *CurrentPointer += sizeof(NvU32);
    }
    else
    {
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0x80, 0x80,0x80,Nv3GPMux->MuxESDSBox[TrackType].DataLen1);
        *CurrentPointer += sizeof(NvU32);
        //skip
        *CurrentPointer += sizeof(NvU8);
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0xC9, 0x04,0x04,0x80);
        *CurrentPointer += sizeof(NvU32);
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),0x80,0x80);
        *CurrentPointer += sizeof(NvU8)*2;
        pTemp[(*CurrentPointer)++] = (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DataLen2;
        pTemp[(*CurrentPointer)++] = 0x20;
        pTemp[(*CurrentPointer)++] = 0x11;
    }

    TempData = Nv3GPMux->MuxESDSBox[TrackType].BufferSizeDB;
    pTemp[(*CurrentPointer)++] = (NvU8)((TempData>>16) & 0xFF);
    pTemp[(*CurrentPointer)++] = (NvU8)((TempData>>8) & 0xFF);
    pTemp[(*CurrentPointer)++] = (NvU8)((TempData) & 0xFF);
    NvMM3GpMuxConvertToBigEndian( pTemp, CurrentPointer, Nv3GPMux->MuxESDSBox[TrackType].MaxBitrate);
    NvMM3GpMuxConvertToBigEndian( pTemp, CurrentPointer, Nv3GPMux->MuxESDSBox[TrackType].AvgBitrate);

    if(TrackType == SOUND_TRACK)
    {
        pTemp[(*CurrentPointer)++] = (NvU8)Nv3GPMux->MuxESDSBox[SOUND_TRACK].DecSpecificInfoTag;
        pTemp[(*CurrentPointer)++] = (NvU8)0x81;
        pTemp[(*CurrentPointer)++] = (NvU8)0x22;
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),'Q','L','C','M');
        *CurrentPointer += sizeof(NvU32);
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),'f','m','t',0x20);
        *CurrentPointer += sizeof(NvU32);
        //chunk size (in HBO)
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0x96,0,0,0);
        *CurrentPointer += sizeof(NvU32);
        //major ,minor,codec-guid,codec version
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),1,0,0x41,0x6D);
        *CurrentPointer += sizeof(NvU32);
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0x7F,0x5E,0x15,0xB1);
        *CurrentPointer += sizeof(NvU32);
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0xD0,0x11,0xBA,0x91);
        *CurrentPointer += sizeof(NvU32);
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0x00,0x80,0x5F,0xB4);
        *CurrentPointer += sizeof(NvU32);
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0xB9,0x7E,0x02,0x00);
        *CurrentPointer += sizeof(NvU32);
        NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)pTemp, (*CurrentPointer)));
        NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
        *CurrentPointer = 0;
        //codec-name
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),'Q','c','e','l');
        *CurrentPointer += sizeof(NvU32);
        pTemp[(*CurrentPointer)++] = (NvU8)'p';
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0x20,0x31,0x33,0x4B);
        *CurrentPointer += sizeof(NvU32);
        //Skipping all other Descriptor's
        *CurrentPointer += UNUSED_ESDS_DESCRIPTOR_LENGTH;
        TempData = Nv3GPMux->MuxESDSBox[SOUND_TRACK].AvgBitrate;
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),(NvU8)((TempData) & 0xFF),(NvU8)((TempData>>8) & 0xFF));
        *CurrentPointer += sizeof(NvU8) * 2;
        //packet-size
        if(Nv3GPMux->VARrate)
        {
            TempData = Nv3GPMux->MuxESDSBox[SOUND_TRACK].BufferSizeDB;
        }
        //decoder buffer reqmnt that shd meet need of all samples.
        else
        {
            TempData = Nv3GPMux->SpeechSampleSize;
        }
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),(NvU8)((TempData) & 0xFF),(NvU8)((TempData>>8) & 0xFF));
        *CurrentPointer += sizeof(NvU8) * 2;
        //block-size
        TempData = SamplingFreq/SpeechFramesPerSec;
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),(NvU8)((TempData) & 0xFF),(NvU8)((TempData>>8) & 0xFF));
        *CurrentPointer += sizeof(NvU8) * 2;
        //sampling freq
        TempData = SamplingFreq;
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),(NvU8)((TempData) & 0xFF),(NvU8)((TempData>>8) & 0xFF));
        *CurrentPointer += sizeof(NvU8) * 2;
        //sample-size
        COPY_TWO_BYTES((pTemp + (*CurrentPointer)),0x10,0);
        *CurrentPointer += sizeof(NvU8) * 2;
        //numrate
        TempData = Nv3GPMux->NumRates;
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),
            TempData,
            ((TempData>>8) & 0xFF),
            ((TempData>>16) & 0xFF),
            ((TempData>>24)));
        *CurrentPointer += sizeof(NvU32);
        //ratemaptable - 16 octets
        for (i = 0;i < 5; i++)
        {
            TempData = Nv3GPMux->RateMapTable[2*i];
            pTemp[((*CurrentPointer) + 1) +2*i] = (NvU8)((TempData) & 0xFF);
            TempData = Nv3GPMux->RateMapTable[2*i + 1];
            pTemp[(*CurrentPointer) + 2*i] = (NvU8)((TempData) & 0xFF);
        }
        //5 * NvU32
        *CurrentPointer += 35;
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0,6,1,0x02);
        *CurrentPointer += sizeof(NvU32);
        NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)pTemp, *CurrentPointer));
        NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
        *CurrentPointer = 0;
    }
    else
    {
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0x5,0x80,0x80,0x80);
        *CurrentPointer += sizeof(NvU32);
        TempData = (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DataLen3;
        pTemp[(*CurrentPointer)] = TempData;
        (*CurrentPointer)++;
        NvOsMemcpy(&pTemp[(*CurrentPointer)],Nv3GPMux->MuxESDSBox[TrackType].DecoderSpecificInfo, TempData);

        *CurrentPointer += TempData;
        COPY_FOUR_BYTES((pTemp + (*CurrentPointer)),0x6,0x80,0x80,0x80);
        *CurrentPointer += sizeof(NvU32);
        pTemp[(*CurrentPointer)] = (NvU8)0x10;
        (*CurrentPointer)++;

        NvOsMemcpy(&pTemp[(*CurrentPointer)],Nv3GPMux->MuxESDSBox[TrackType].SLConfigDescrInfo, 16);
        *CurrentPointer += 16;

        NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)pTemp, *CurrentPointer));
     }
cleanup:
    return Status;

}

static NvError
NvMM3GpMuxExtractMoovEntries(
    NvMM3GpMuxInputParam *pInpParam,
    Nv3gpWriter *Nv3GPMux,
    NvOsFileHandle *FileHandle,
    NvU8 Trak)
{
    NvError Status = NvSuccess;
    NvU8 *pBuffer = NULL;
    NvU64 Bytesread=0;
    NvU32 TotalValidEntries = 0;
    NvU32 AtomOffsetInTemp = 0;
    NvU32 AtomSize = 0;
    NvU8 *pDest = NULL;
    NvU8 *pSrc = NULL;

    // Read all audio/video entries till end of file into buffer
    pBuffer = Nv3GPMux->pResvRunBuffer ;
    while(Status == NvSuccess) // while not reaching NvError_EndOfFile or hitting IO errors
    {
        Status = NvOsFread( *FileHandle, pBuffer, OPTIMAL_READ_SIZE, (size_t *)&Bytesread);
        pBuffer += (NvU32)Bytesread;
    }
    //Extract staggered STSS atom info of video and put them linearly within buffer
    TotalValidEntries = Nv3GPMux->MuxSTTSBox[Trak].EntryCount;
    AtomOffsetInTemp = STTS_OFFSET;
    AtomSize = sizeof(NvMM3GpMuxSTTSTable);
    pSrc = Nv3GPMux->pResvRunBuffer;
    pDest = &Nv3GPMux->pResvRunBuffer[Nv3GPMux->GraceSize];
    NvMM3GpMuxReadStaggerDumpLinear( TotalValidEntries , AtomOffsetInTemp, AtomSize, pSrc, &pDest);

    // Extract staggered STSZ atom info of video and put them linearly within buffer
    TotalValidEntries = Nv3GPMux->MuxSTSZBox[Trak].SampleCount;;
    AtomOffsetInTemp = STSZ_OFFSET;
    AtomSize = sizeof(NvU32);
    NvMM3GpMuxReadStaggerDumpLinear( TotalValidEntries , AtomOffsetInTemp, AtomSize, pSrc, &pDest);

    //Extract staggered STSC atom info of video and put them linearly within buffer
    TotalValidEntries = Nv3GPMux->MuxSTSCBox[Trak].EntryCount;;
    AtomOffsetInTemp = STSC_OFFSET;
    AtomSize = sizeof(NvMM3GpMuxSTSCTable);
    NvMM3GpMuxReadStaggerDumpLinear( TotalValidEntries , AtomOffsetInTemp, AtomSize, pSrc, &pDest);

    if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
    {
        AtomSize = sizeof(NvU32);
        //Extract staggered STCO atom info of video and put them linearly within buffer
        TotalValidEntries = Nv3GPMux->MuxSTCOBox[Trak].ChunkCount;
     }
    else
    {
        AtomSize = sizeof(NvU64);
        //Extract staggered CO64 atom info of video and put them linearly within buffer
        TotalValidEntries = Nv3GPMux->MuxCO64Box[Trak].ChunkCount;
    }
    AtomOffsetInTemp = CO64_OFFSET;
    NvMM3GpMuxReadStaggerDumpLinear( TotalValidEntries , AtomOffsetInTemp, AtomSize, pSrc, &pDest);

    NvOsFclose(*FileHandle);
    *FileHandle = NULL;

    return (NvError_EndOfFile == Status) ? NvSuccess : Status;
}

static NvError
NvMM3GpMuxWriteTRAKAtomForAV(
    NvMM3GpMuxInputParam *pInpParam,
    Nv3gpWriter *Nv3GPMux,
    CPhandle cphandle,
    CP_PIPETYPE *pPipe,
    NvU32 TrackType,
    NvU32 Flag)
{
    NvError Status=NvSuccess;
    NvU32 TempData =0;
    NvU8 *pTemp = NULL;
    NvU8 i =0;
    NvU32 Bytesread = 0;
    NvU32 TotalEntries;
    NvU32 AccumEntries=0;
    NvU8 *pBuffer = NULL;
    NvU32 CurrentPointer =0;
    NvMM3GpMuxTSPSTable *pCurrspsTable = NULL;
    NvMM3GpMuxPPSTable *pCurrppsTable = NULL;
    NvOsFileHandle *FileHandle = NULL;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "++NvMM3GpMuxWriteTRAKAtomForAV \n"));

    NVMM_CHK_ARG(Nv3GPMux && pInpParam && pPipe);

    if(VIDEO_TRACK == TrackType)
    {
        NVMM_CHK_ERR(NvOsFopen((const char *)Nv3GPMux->URIConcatSync[0], NVOS_OPEN_READ, &Nv3GPMux->FileSyncHandle));
        FileHandle = &Nv3GPMux->FileVideoHandle;
    }
    else
        FileHandle = &Nv3GPMux->FileAudioHandle;

    NVMM_CHK_ERR(NvMM3GpMuxExtractMoovEntries(pInpParam, Nv3GPMux,FileHandle,TrackType));

    if (VIDEO_TRACK == TrackType)
        NvOsFremove(Nv3GPMux->URIConcatVideo[0]);
    else
        NvOsFremove(Nv3GPMux->URIConcatAudio[0]);

    pTemp = Nv3GPMux->pTempBuff;

    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);

    //trak box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxTRAKBox[TrackType].Box.BoxSize,pTemp,&CurrentPointer, 't', 'r', 'a', 'k');
    //tkhd box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxTKHDBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 't', 'k', 'h', 'd');

    //Version,flags
    COPY_FOUR_BYTES((pTemp + CurrentPointer), 0,0,0,1);
    CurrentPointer += sizeof(NvU32);

    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxTKHDBox[TrackType].CreationTime);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxTKHDBox[TrackType].ModTime);

    //track_ID can't be 0
    COPY_FOUR_BYTES((pTemp + CurrentPointer), 0, 0, 0, (TrackType+1));
    CurrentPointer += sizeof(NvU32);
    //reserved
    CurrentPointer += sizeof(NvU32);

    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxTKHDBox[TrackType].Duration);
    //reserved
    CurrentPointer += sizeof(NvU32) * 2;
    //layer, alternate group
    CurrentPointer += sizeof(NvU32);
    //Volume,reserved
    if(TrackType == SOUND_TRACK)
        COPY_FOUR_BYTES((pTemp + CurrentPointer), 1, 0, 0, 0);
    CurrentPointer += sizeof(NvU32);

    //unity matrix { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000}
    COPY_FOUR_BYTES((pTemp + CurrentPointer), 0, 1, 0, 0);
    CurrentPointer += sizeof(NvU32);
    //skip as they are 0 anyway
    CurrentPointer += sizeof(NvU32) * 3;
    COPY_FOUR_BYTES((pTemp + CurrentPointer), 0, 1, 0, 0);
    CurrentPointer += sizeof(NvU32);
    //skip as they are 0 anyway
    CurrentPointer += sizeof(NvU32) * 3;
    COPY_FOUR_BYTES((pTemp + CurrentPointer), 0x40, 0, 0, 0);
    CurrentPointer += sizeof(NvU32);
    //width height
    if(TrackType == VIDEO_TRACK)
    {
        if(1 == Flag)
        {
            COPY_TWO_BYTES((pTemp + CurrentPointer),1,0x40);
        }
        else
        {
            COPY_TWO_BYTES((pTemp + CurrentPointer),(Nv3GPMux->VOPWidth >> 8),(Nv3GPMux->VOPWidth & 0xff));
        }
        CurrentPointer += sizeof(NvU8) * 2;
        if(1 == Flag)
        {
            COPY_FOUR_BYTES((pTemp + CurrentPointer), 0, 0, 0, 0xF0);
        }
        else
        {
            COPY_FOUR_BYTES((pTemp + CurrentPointer), 0, 0,
                (Nv3GPMux->VOPHeight >> 8),
                (Nv3GPMux->VOPHeight & 0xff));
        }
    }
    CurrentPointer += sizeof(NvU32);
    if(TrackType == VIDEO_TRACK)
        CurrentPointer += sizeof(NvU8) * 2;
    else
        CurrentPointer += sizeof(NvU32);


    if(NvMM_AV_AUDIO_VIDEO_BOTH == pInpParam->AudioVideoFlag)
    {
        //edts box
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxEDTSBox[TrackType].Box.BoxSize,pTemp, &CurrentPointer, 'e', 'd', 't', 's');
        //elst box
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxELSTBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 'e', 'l', 's', 't');
        //version
        CurrentPointer += sizeof(NvU32);
        NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxELSTBox[TrackType].EntryCount);
        NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));
        NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
        CurrentPointer = 0;

        pTemp = (NvU8 *)Nv3GPMux->MuxELSTBox[TrackType].pElstTable;
        CurrentPointer = sizeof(NvMM3GpMuxELSTTable)*Nv3GPMux->MuxELSTBox[TrackType].EntryCount;
    }
    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));

    pTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
    CurrentPointer = 0;

    //mdia box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxMDIABox[TrackType].Box.BoxSize,pTemp, &CurrentPointer, 'm', 'd', 'i', 'a');
    //mdhd box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxMDHDBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 'm', 'd', 'h', 'd');
    //version,flags
    COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,0);
    CurrentPointer += sizeof(NvU32);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxMDHDBox[TrackType].CreationTime);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxMDHDBox[TrackType].ModTime);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxMDHDBox[TrackType].TimeScale);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxMDHDBox[TrackType].Duration);
    //pad,lang,predefined
    CurrentPointer += sizeof(NvU32);

    //hdlr box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxHDLRBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 'h', 'd', 'l', 'r');
    //Version flag
    CurrentPointer += sizeof(NvU32);
    //pre-defined
    CurrentPointer += sizeof(NvU32);
    //Audio Track
    if(TrackType == SOUND_TRACK)
    {
        COPY_FOUR_BYTES((pTemp + CurrentPointer),'s','o','u','n');
    }
    else
    {
        COPY_FOUR_BYTES((pTemp + CurrentPointer),'v','i','d','e');
    }
    CurrentPointer += sizeof(NvU32);
    //reserved
     CurrentPointer += sizeof(NvU32) * 3;
    NvOsMemcpy(pTemp + CurrentPointer,Nv3GPMux->MuxHDLRBox[TrackType].StringName, 13);
    CurrentPointer += 13;
    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));
    pTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
    CurrentPointer = 0;

    //minf box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxMINFBox[TrackType].Box.BoxSize,pTemp, &CurrentPointer, 'm', 'i', 'n', 'f');
    //smhd box
    if(TrackType == SOUND_TRACK)
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxSMHDBox.FullBox.Box.BoxSize,pTemp, &CurrentPointer, 's', 'm', 'h', 'd');
    else
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxVMHDBox.FullBox.Box.BoxSize,pTemp, &CurrentPointer, 'v', 'm', 'h', 'd');

    //reserved,balance
    if(TrackType == VIDEO_TRACK)
    {
        //Version flag
        COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,1);
        CurrentPointer += sizeof(NvU32);
    }
    //Version flag in case of audio skip in case of video
    CurrentPointer += sizeof(NvU32);
    //reserved
    CurrentPointer += sizeof(NvU32);
    //dinf box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxDINFBox[TrackType].Box.BoxSize,pTemp, &CurrentPointer, 'd', 'i', 'n', 'f');
    //dref box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxDREFBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 'd', 'r', 'e', 'f');

    //Version flag
    CurrentPointer += sizeof(NvU32);
    //entry count
    COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,1);
    CurrentPointer += sizeof(NvU32);
    //sizeof url box, not sure why it is has to be hardcoded??
    COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,0xC);
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((pTemp + CurrentPointer),'u','r','l',' ');
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,0x1);
    CurrentPointer += sizeof(NvU32);

    //stbl box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxSTBLBox[TrackType].Box.BoxSize,pTemp, &CurrentPointer, 's', 't', 'b', 'l');
    //stts box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxSTTSBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 's', 't', 't', 's');
    //Version flag
    CurrentPointer += sizeof(NvU32);

    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxSTTSBox[TrackType].EntryCount);

    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));
    pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize;
    TotalEntries = (Nv3GPMux->MuxSTTSBox[TrackType].EntryCount) * sizeof(NvMM3GpMuxSTTSTable);
    if( ((TrackType == VIDEO_TRACK && 0 == Flag)) || ((TrackType == VIDEO_TRACK && 1 == Flag)))
        Nv3GPMux->AccumEntries = TotalEntries;
    else
        AccumEntries = TotalEntries;
    CurrentPointer = 0;
    NVMM_CHK_ERR(NvMM3GpMuxMuxWriteDataFromBuffer(pBuffer, TotalEntries, pPipe, cphandle));

    pTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
    CurrentPointer = 0;

    //stsd box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxSTSDBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 's', 't', 's', 'd');
    //Version flag
    CurrentPointer += sizeof(NvU32);
    //entry count
    COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,1);
    CurrentPointer += sizeof(NvU32);

    if(TrackType == SOUND_TRACK)
    {
        if(pInpParam->SpeechAudioFlag == 4)
        {
            NVMM_CHK_ERR(NvMM3GpMuxCopyMp4AVAtom(Nv3GPMux, &CurrentPointer,pTemp,pPipe,cphandle,TrackType));
        }//qcp data using mp4a box
        else if (pInpParam->SpeechAudioFlag == 2)
        {
            //mp4a box for AAC
            NvMM3GpMuxCopyAtom(Nv3GPMux->MuxMP4ABox.BoxIndexTime.Box.BoxSize,pTemp, &CurrentPointer, 'm', 'p', '4', 'a');
            //reserved 6 bytes
            CurrentPointer += sizeof(NvU32);
            COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,0x1);
            CurrentPointer += sizeof(NvU32);
            //reserved
            CurrentPointer += sizeof(NvU32) * 2;
            COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0x2,0,0x10);
            CurrentPointer += sizeof(NvU32);
            //reserved
            CurrentPointer += sizeof(NvU32);
            TempData = Nv3GPMux->MuxMP4ABox.BoxIndexTime.TimeScale;//assigned as SamplingFreq'cy
           //reserved and timescale
            COPY_FOUR_BYTES((pTemp + CurrentPointer),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF),0,0);
            CurrentPointer += sizeof(NvU32);
            //esds box
            NvMM3GpMuxCopyAtom(Nv3GPMux->MuxESDSBox[SOUND_TRACK].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 'e', 's', 'd', 's');
            //just skip
            CurrentPointer += sizeof(NvU32);
            //ES_Descr Tag
            pTemp[CurrentPointer++] = (NvU8)0x03;
            pTemp[CurrentPointer++] = (NvU8)Nv3GPMux->MuxESDSBox[SOUND_TRACK].DataLen1;
            //just skip
            CurrentPointer += sizeof(NvU8) * 3;
            COPY_FOUR_BYTES((pTemp + CurrentPointer),
                (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DecoderConfigDescrTag,
                (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DataLen2,
                (NvU8)Nv3GPMux->MuxESDSBox[TrackType].ObjectTypeIndication,
                (NvU8)Nv3GPMux->MuxESDSBox[TrackType].StreamType);
            CurrentPointer += sizeof(NvU32);
            TempData = Nv3GPMux->MuxESDSBox[TrackType].BufferSizeDB;
            pTemp[CurrentPointer++] = (NvU8)((TempData>>16) & 0xFF);
            pTemp[CurrentPointer++] = (NvU8)((TempData>>8) & 0xFF);
            pTemp[CurrentPointer++] = (NvU8)((TempData) & 0xFF);
            NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxESDSBox[TrackType].MaxBitrate);
            NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxESDSBox[TrackType].AvgBitrate);
            COPY_TWO_BYTES((pTemp + CurrentPointer),
                (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DecSpecificInfoTag,
                (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DataLen3);
            CurrentPointer += sizeof(NvU8) * 2;
            for (i = 0; i < Nv3GPMux->MuxESDSBox[TrackType].DataLen3; i++)
            {
                pTemp[CurrentPointer++] = (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DecoderSpecificInfo[i];
            }
            pTemp[CurrentPointer++] = (NvU8)Nv3GPMux->MuxESDSBox[TrackType].SLConfigDescrTag;
            pTemp[CurrentPointer++] = (NvU8)Nv3GPMux->MuxESDSBox[TrackType].DataLen4;
            pTemp[CurrentPointer++] = (NvU8)Nv3GPMux->MuxESDSBox[TrackType].SLConfigDescrInfo[0];
            NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));
            NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
            CurrentPointer = 0;
        }//AAC data using mp4a box
        else
        {
            // samr/qcelp box
            if(pInpParam->SpeechAudioFlag == 3)
                TempData = Nv3GPMux->MuxSQCPBox.BoxIndexTime.Box.BoxSize;
            else
                TempData = Nv3GPMux->MuxSAMRBox.BoxIndexTime.Box.BoxSize;
            NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, TempData);
            if(pInpParam->SpeechAudioFlag == 0)
            {
                COPY_FOUR_BYTES((pTemp + CurrentPointer),'s','a','m','r');
            }
            else if(pInpParam->SpeechAudioFlag == 1)
            {
                COPY_FOUR_BYTES((pTemp + CurrentPointer),'s','a','w','b');
            }
            else if(pInpParam->SpeechAudioFlag == 3)
            {
                COPY_FOUR_BYTES((pTemp + CurrentPointer),'s','q','c','p');
            }
            CurrentPointer += sizeof(NvU32);
             //reserved 6 bytes
            CurrentPointer += sizeof(NvU32);
            COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,0x1);
            CurrentPointer += sizeof(NvU32);
            //reserved
            CurrentPointer += sizeof(NvU32) * 2;
            COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0x2,0,0x10);
            CurrentPointer += sizeof(NvU32);
            if(pInpParam->SpeechAudioFlag == 3)
                TempData = Nv3GPMux->MuxSQCPBox.BoxIndexTime.TimeScale;
            else
                TempData = Nv3GPMux->MuxSAMRBox.BoxIndexTime.TimeScale;
           //reserved and timescale
            CurrentPointer += sizeof(NvU32);
            COPY_TWO_BYTES((pTemp + CurrentPointer),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF));
            CurrentPointer += sizeof(NvU8) * 2;
            // damr/dqcp box
            if(pInpParam->SpeechAudioFlag == 3)
                TempData = Nv3GPMux->MuxDQCPBox.Box.BoxSize;
            else
                TempData = Nv3GPMux->MuxDAMRBox.Box.BoxSize;
            CurrentPointer += sizeof(NvU8) * 2;
            NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, TempData);
            //i=147
            if(pInpParam->SpeechAudioFlag == 3)
            {
                COPY_FOUR_BYTES((pTemp + CurrentPointer),'d','q','c','p');
                CurrentPointer += sizeof(NvU32);
                //vendor for damr/dqcp
                COPY_FOUR_BYTES((pTemp + CurrentPointer),'N','V','D','A');
                CurrentPointer += sizeof(NvU32);
               //frame per sample
                COPY_TWO_BYTES((pTemp + CurrentPointer),0,0x1);
                CurrentPointer += sizeof(NvU8) * 2;
                // support variable frame size for QCELP, magic number??
                NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, 158));
                pTemp = Nv3GPMux->pTempBuff;
                NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
                CurrentPointer = 0;
            }
            else
            {
                COPY_FOUR_BYTES((pTemp + CurrentPointer),'d','a','m','r');
                CurrentPointer += sizeof(NvU32);
                COPY_FOUR_BYTES((pTemp + CurrentPointer),'N','V','D','A');
                CurrentPointer += sizeof(NvU32);
                TempData = Nv3GPMux->MuxDAMRBox.ModeSet;
                COPY_FOUR_BYTES((pTemp + CurrentPointer),
                    0,
                    (NvU8)((TempData>>8) & 0xFF),
                    (NvU8)((TempData) & 0xFF),
                    0);
                CurrentPointer += sizeof(NvU32);
                pTemp[CurrentPointer++] = (NvU8)Nv3GPMux->Max_Samples_Per_Frame;
                NVMM_CHK_ERR((NvError) pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));
                NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
                CurrentPointer = 0;
            }
        }
    }
    else
    {
       if(1 == Flag)
       {
           //s263 box
           NvMM3GpMuxCopyAtom(Nv3GPMux->MuxS263Box.BoxWidthHeight.Box.BoxSize,pTemp, &CurrentPointer, 's', '2', '6', '3');
           //version flags
           CurrentPointer += sizeof(NvU32);
           COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,1);
           CurrentPointer += sizeof(NvU32);
           //skip
           CurrentPointer += sizeof(NvU32) * 4;

           TempData = Nv3GPMux->MuxS263Box.BoxWidthHeight.Width;
           COPY_TWO_BYTES((pTemp + CurrentPointer),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF));
           CurrentPointer += sizeof(NvU8) * 2;

           TempData = Nv3GPMux->MuxS263Box.BoxWidthHeight.Height;
           COPY_TWO_BYTES((pTemp + CurrentPointer),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF));
           CurrentPointer += sizeof(NvU8) * 2;
           COPY_FOUR_BYTES((pTemp + CurrentPointer),0, 0x48,0,0);
           CurrentPointer += sizeof(NvU32);
           COPY_FOUR_BYTES((pTemp + CurrentPointer),0, 0x48,0,0);
           CurrentPointer += sizeof(NvU32);
           //skip
           CurrentPointer += sizeof(NvU32);
           COPY_TWO_BYTES((pTemp + CurrentPointer),0, 1);
           CurrentPointer += sizeof(NvU8) * 2;

           //reserved
           CurrentPointer += sizeof(NvU32) * 8;
           COPY_FOUR_BYTES((pTemp + CurrentPointer),0, 0x24,0xFF,0xFF);
           CurrentPointer += sizeof(NvU32);
           NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));

           pTemp = Nv3GPMux->pTempBuff;
           NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
           CurrentPointer = 0;

           //d263 box
           NvMM3GpMuxCopyAtom(Nv3GPMux->MuxD263Box.Box.BoxSize,pTemp, &CurrentPointer, 'd', '2', '6', '3');

           COPY_FOUR_BYTES((pTemp + CurrentPointer),'N', 'V','D','A');
           CurrentPointer += sizeof(NvU32);
           //skip a byte
           CurrentPointer += sizeof(NvU8);

           COPY_TWO_BYTES((pTemp + CurrentPointer),(NvU8)Nv3GPMux->MuxD263Box.H263_Level,(NvU8)Nv3GPMux->MuxD263Box.H263_Profile);
           CurrentPointer += sizeof(NvU8) * 2;
           NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));

           pTemp = Nv3GPMux->pTempBuff;
           NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
           CurrentPointer = 0;
       }
       else if(0 == Flag)
       {
           NVMM_CHK_ERR(NvMM3GpMuxCopyMp4AVAtom(Nv3GPMux, &CurrentPointer,pTemp,pPipe,cphandle,TrackType));
       }
       else
       {
           if(0 == pInpParam->Mp4H263Flag)
           {
               NVMM_CHK_ERR(NvMM3GpMuxCopyMp4AVAtom(Nv3GPMux, &CurrentPointer,pTemp,pPipe,cphandle,TrackType));
           }
           else if(2 == pInpParam->Mp4H263Flag)
           {
               //avc1 box
               NvMM3GpMuxCopyAtom(Nv3GPMux->MuxAVC1Box.BoxWidthHeight.Box.BoxSize,pTemp, &CurrentPointer, 'a', 'v', 'c', '1');
               //version
               CurrentPointer += sizeof(NvU32);
               COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0,0,1);
               CurrentPointer += sizeof(NvU32);
               //skip
               CurrentPointer += sizeof(NvU32) * 4;
               TempData = Nv3GPMux->MuxAVC1Box.BoxWidthHeight.Width;
               COPY_TWO_BYTES((pTemp + CurrentPointer),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF));
               CurrentPointer += sizeof(NvU8) * 2;
               TempData = Nv3GPMux->MuxAVC1Box.BoxWidthHeight.Height;
               COPY_TWO_BYTES((pTemp + CurrentPointer),(NvU8)((TempData>>8) & 0xFF),(NvU8)((TempData) & 0xFF));
               CurrentPointer += sizeof(NvU8) * 2;
               COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0x48,0,0);
               CurrentPointer += sizeof(NvU32);
               COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0x48,0,0);
               CurrentPointer += sizeof(NvU32);
               CurrentPointer += sizeof(NvU32);
               COPY_TWO_BYTES((pTemp + CurrentPointer),0,1);
               CurrentPointer += sizeof(NvU8) * 2;
               //skip
               CurrentPointer += sizeof(NvU32) * 8;
               COPY_FOUR_BYTES((pTemp + CurrentPointer),0,0x24,0xFF,0xFF);
               CurrentPointer += sizeof(NvU32);
               NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));
               NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
               CurrentPointer = 0;
               //avcC box
               NvMM3GpMuxCopyAtom(Nv3GPMux->MuxAVCCBox.Box.BoxSize,pTemp, &CurrentPointer, 'a', 'v', 'c', 'C');

               COPY_FOUR_BYTES((pTemp + CurrentPointer),
                   (NvU8)Nv3GPMux->MuxAVCCBox.ConfigurationVersion,
                   (NvU8)Nv3GPMux->MuxAVCCBox.AVCProfileIndication,
                   (NvU8)Nv3GPMux->MuxAVCCBox.Profile_compatibility,
                   (NvU8)Nv3GPMux->MuxAVCCBox.AVCLevelIndication);
               CurrentPointer += sizeof(NvU32);

               COPY_TWO_BYTES((pTemp + CurrentPointer),
                   (NvU8) ( 0xFC | Nv3GPMux->MuxAVCCBox.LengthSizeMinusOne),
                   (NvU8) ( 0xE0 | Nv3GPMux->MuxAVCCBox.NumOfSequenceParameterSets));
               CurrentPointer += sizeof(NvU8) * 2;
               NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));
               CurrentPointer = 0;

               for(i = 0; i < Nv3GPMux->MuxAVCCBox.NumOfSequenceParameterSets; i++)
               {
                   pCurrspsTable = (NvMM3GpMuxTSPSTable *)((NvU32)Nv3GPMux->MuxAVCCBox.pPspsTable + ( i * sizeof(NvMM3GpMuxTSPSTable)));
                   TempData = (pCurrspsTable->SequenceParameterSetLength & 0xFF) << 8;
                   CurrentPointer = 2;
                   NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)&TempData, CurrentPointer));
                   CurrentPointer = pCurrspsTable->SequenceParameterSetLength;
                   NVMM_CHK_ERR((NvError)pPipe->Write( cphandle, (CPbyte *)&pCurrspsTable->SequenceParameterSetNALUnit, CurrentPointer));
               }
               CurrentPointer = 1;
               NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)&Nv3GPMux->MuxAVCCBox.NumOfPictureParameterSets, CurrentPointer));
               for(i=0; i < Nv3GPMux->MuxAVCCBox.NumOfPictureParameterSets; i++)
               {
                   pCurrppsTable = (NvMM3GpMuxPPSTable *)((NvU32)Nv3GPMux->MuxAVCCBox.pPpsTable + (i * sizeof(NvMM3GpMuxPPSTable)));
                   TempData = (pCurrppsTable->PictureParameterSetLength & 0xFF) << 8;
                   CurrentPointer = 2;
                    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)&TempData, CurrentPointer));
                    CurrentPointer = pCurrppsTable->PictureParameterSetLength;
                    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)&pCurrppsTable->PictureParameterSetNALUnit, CurrentPointer));
               }
            }
        }
    }
    pTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
    CurrentPointer = 0;
    //stsz box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxSTSZBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 's', 't', 's', 'z');
    //Version
    CurrentPointer += sizeof(NvU32);

    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxSTSZBox[TrackType].SampleSize);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxSTSZBox[TrackType].SampleCount);

    // support variable frame size for AMR
    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));

    if((SOUND_TRACK == TrackType) && (2 == Flag) )
    {
        NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "H263Flag->%x \n",Flag));
        if( (pInpParam->SpeechAudioFlag == 0) ||
           (pInpParam->SpeechAudioFlag == 1) ||
           (pInpParam->SpeechAudioFlag == 2) ||
           (pInpParam->SpeechAudioFlag == 4))
       {
            pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize + AccumEntries;
            TotalEntries = Nv3GPMux->MuxSTSZBox[TrackType].SampleCount * (sizeof(NvU32));
            AccumEntries += TotalEntries;
            NVMM_CHK_ERR(NvMM3GpMuxMuxWriteDataFromBuffer(pBuffer, TotalEntries, pPipe, cphandle));
       }
       else
       {
          if(Nv3GPMux->VARrate)
          {
              pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize + AccumEntries;
              TotalEntries = Nv3GPMux->MuxSTSZBox[TrackType].SampleCount * (sizeof(NvU32));
              AccumEntries += TotalEntries;
              NVMM_CHK_ERR(NvMM3GpMuxMuxWriteDataFromBuffer(pBuffer, TotalEntries, pPipe, cphandle));
           }
       }
     }
    else //if(0 == Flag )
    {
        if( (VIDEO_TRACK == TrackType && 0 == Flag) || (VIDEO_TRACK == TrackType && 1 == Flag))
            pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize + Nv3GPMux->AccumEntries;
        else
            pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize + AccumEntries;
        TotalEntries = Nv3GPMux->MuxSTSZBox[TrackType].SampleCount * (sizeof(NvU32));
        if( (TrackType == VIDEO_TRACK && 0 == Flag) || (TrackType == VIDEO_TRACK && 1 == Flag))
           Nv3GPMux->AccumEntries += TotalEntries;
       else
           AccumEntries += TotalEntries;
        NVMM_CHK_ERR(NvMM3GpMuxMuxWriteDataFromBuffer(pBuffer, TotalEntries, pPipe, cphandle));
    }

    pTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
    CurrentPointer = 0;

    //stsc box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxSTSCBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 's', 't', 's', 'c');
    //Version
    CurrentPointer += sizeof(NvU32);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxSTSCBox[TrackType].EntryCount);

    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));

    // pstscTable
    if( (VIDEO_TRACK ==TrackType && 0 == Flag) || (VIDEO_TRACK == TrackType && 1 == Flag))
        pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize + Nv3GPMux->AccumEntries;
    else
        pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize + AccumEntries;
    TotalEntries = Nv3GPMux->MuxSTSCBox[TrackType].EntryCount * (sizeof(NvMM3GpMuxSTSCTable));
    if( (VIDEO_TRACK == TrackType && 0 == Flag) || (VIDEO_TRACK == TrackType && 1 == Flag))
        Nv3GPMux->AccumEntries += TotalEntries;
    else
        AccumEntries += TotalEntries;


    NVMM_CHK_ERR(NvMM3GpMuxMuxWriteDataFromBuffer(pBuffer, TotalEntries, pPipe, cphandle));
    pTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
    CurrentPointer = 0;

    if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
    {
        //stco box
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxSTCOBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 's', 't', 'c', 'o');
        TempData = Nv3GPMux->MuxSTCOBox[TrackType].ChunkCount;
    }
    else
    {
        //co64 box
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxCO64Box[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 'c', 'o', '6', '4');
        TempData = Nv3GPMux->MuxCO64Box[TrackType].ChunkCount;
    }
    //Version
    CurrentPointer += sizeof(NvU32);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, TempData);

    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));

    if( (VIDEO_TRACK == TrackType && 0 == Flag) || (VIDEO_TRACK == TrackType && 1 == Flag))
        pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize + Nv3GPMux->AccumEntries;
    else
        pBuffer = Nv3GPMux->pResvRunBuffer + Nv3GPMux->GraceSize + AccumEntries;

    if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
        TotalEntries = TempData * (sizeof(NvU32));
    else
        TotalEntries = TempData * (sizeof(NvU64));

    NVMM_CHK_ERR(NvMM3GpMuxMuxWriteDataFromBuffer(pBuffer, TotalEntries, pPipe, cphandle));
    pTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
    CurrentPointer = 0;

    if(VIDEO_TRACK == TrackType)
    {
        //stss box
        NvMM3GpMuxCopyAtom(Nv3GPMux->MuxSTSSBox[VIDEO_TRACK].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 's', 't', 's', 's');
        //Version
        CurrentPointer += sizeof(NvU32);

        TempData = Nv3GPMux->MuxSTSSBox[VIDEO_TRACK].EntryCount;
        NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, TempData);
        NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));

        pBuffer = Nv3GPMux->pResvRunBuffer;
        while( (Status != NvError_EndOfFile) )
        {
                Status = NvOsFread( Nv3GPMux->FileSyncHandle, pBuffer, OPTIMAL_READ_SIZE, (size_t *)&Bytesread);
                pBuffer += Bytesread;
        }
        pBuffer = Nv3GPMux->pResvRunBuffer ;
        TotalEntries = TempData * (sizeof(NvU32));
        NVMM_CHK_ERR(NvMM3GpMuxMuxWriteDataFromBuffer(pBuffer, TotalEntries, pPipe, cphandle));
        pTemp = Nv3GPMux->pTempBuff;
        NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
        CurrentPointer = 0;
     }
    //ctts box
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxCTTSBox[TrackType].FullBox.Box.BoxSize,pTemp, &CurrentPointer, 'c', 't', 't', 's');
    //Version
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((pTemp + CurrentPointer), 0, 0, 0, 0x1);
    CurrentPointer += sizeof(NvU32);

    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxCTTSBox[TrackType].SampleCount);
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxCTTSBox[TrackType].SampleOffset);

    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));
    if(VIDEO_TRACK == TrackType)
    {
        NvOsFclose(Nv3GPMux->FileSyncHandle);
        Nv3GPMux->FileSyncHandle = NULL;
        NvOsFremove(Nv3GPMux->URIConcatSync[0]);
    }

cleanup:
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "--NvMM3GpMuxWriteTRAKAtomForAV \n"));
    return Status;
}

static NvError
NvMM3GpMuxExtractTempFilePath(
    char *TempFileName,
    char **opFilePath)
{
    NvError Status=NvSuccess;
    char *Temp= NULL;
    NvU32 Length = 0;
    NvU32 TempLen = 0;
    NvBool FlagF = NV_FALSE;
    NvBool FlagB = NV_FALSE;
    NvBool Flag = NV_FALSE;

    Length = NvOsStrlen((char *)TempFileName);
    TempLen = Length;
    while(TempLen > 0)
    {
        if( (TempFileName[TempLen] == '\\') )
        {
            FlagB = NV_TRUE;
            break;
        }
        else if( (TempFileName[TempLen] == '/'))
        {
            FlagF = NV_TRUE;
            break;
        }
        TempLen--;
    }

    if(!((TempFileName[Length-1] == '\\') || (TempFileName[Length-1] == '/') ) )
    {
        Length += 1;
        Flag = NV_TRUE;
    }
    /**
      * length + 1 ; 1 for null terminator
      */
    Temp = (char *)NvOsAlloc((Length+1) * sizeof(char));
    if(Temp == NULL)
    {
        NVMM_CHK_MEM(0);
    }

    NvOsMemset(Temp,0,Length);

    NvOsMemcpy(Temp, TempFileName, Length);

    if(( Flag == NV_TRUE) && (FlagB == NV_TRUE))
    {
        Temp[Length-1] = '\\';
        Temp[Length] = '\0';
    }
    else if(( Flag == NV_TRUE) && (FlagF == NV_TRUE))
    {
        Temp[Length-1] = '/';
        Temp[Length] = '\0';
    }
    else
    {
        Temp[Length] = '\0';
    }

    *opFilePath = Temp;

cleanup:
    return Status;
}

static NvError
NvMM3GpMuxExtractOutputFilePath(
    char *FileName,
    char **opFilePath)
{
    NvError Status=NvSuccess;
    char *Temp= NULL;
    NvU32 Length = 0;
    NvU32 j = 0;

    Length = NvOsStrlen((char *)FileName);
    Length-= 1; // 1 for null terminator
    while(Length > 0)
    {
        if( (FileName[Length] == '\\') || (FileName[Length] == '/') )
        {
            Length++;
            break;
        }
        Length--;
    }

    /**
      * length + 1 ; 1 for null terminator
      */
    Temp = (char *)NvOsAlloc((Length+1) * sizeof(char));
    NVMM_CHK_MEM(Temp)
    NvOsMemset(Temp,0,Length);

    /**
      * Copy complete path except the last file name
      */
    for(j = 0; j < Length; j++)
    {
        Temp[j] = FileName[j];
    }
    Temp[j] = '\0';
    *opFilePath = Temp;

cleanup:
    return Status;
}

void
NvMM3GpMuxWriterStrcat(
    char **c3,
    char *c1,
    NvU32 Length1,
    const char *c2,
    NvU32 Length2)
{
    NvU8 i = 0;

    for(i=0;i< Length1;i++)
    {
        (*c3)[i] = c1[i];
    }

    for(i=0;i< Length2;i++)
    {
        (*c3)[Length1 + i] = c2[i];
    }

    (*c3)[Length1 + Length2] = '\0';
}

static NvError
NvMM3GpMuxResvInit(
    Nv3gpWriter *Nv3GPMux,
    NvU32 FileNameLength,
    NvU32 TmpFileNameLength,
    NvMM3GpMuxInputParam *pInpParam)
{
    NvError Status=NvSuccess;
    NvU32 Flags;
    NvMMFileAVSWriteMsg Message;
    char *ResvFileName = NULL;
    NvU32 TotalGraceSize=0;

    Nv3GPMux->URIConcatResv[0] = (char *)NvOsAlloc(sizeof(char) * (FileNameLength + TmpFileNameLength + 1));
    NVMM_CHK_MEM(Nv3GPMux->URIConcatResv[0]);

    NVMM_CHK_ERR(NvOsSemaphoreCreate(&Nv3GPMux->ResvWriteSema, 0));
    NVMM_CHK_ERR(NvOsSemaphoreCreate(&Nv3GPMux->ResvWriteDoneSema, 0));
    NVMM_CHK_ERR(NvMMQueueCreate( &Nv3GPMux->MsgQueueResv, MAX_QUEUE_LEN, sizeof(NvMMFileAVSWriteMsg), NV_TRUE));
    NVMM_CHK_ERR(NvOsThreadCreate( NvMM3GpMuxResvSpaceFileWriteThread, (void *)(Nv3GPMux), &(Nv3GPMux->FileWriterResvThread)));

    /**
      * General case: To handle case when mdat exceeds storage card or nandflash sizes
      * Creating a temporary file so that this can be deleted whenever there is overflow so that
      * room for appending moov atom info to mdat info becomes possible and generated file
      * will be valid
      */
    Flags =    NVOS_OPEN_CREATE| NVOS_OPEN_WRITE;
    ResvFileName = "resvX.dat";

    NvMM3GpMuxWriterStrcat( Nv3GPMux->URIConcatResv, pInpParam->pFilePath, FileNameLength, ResvFileName, TmpFileNameLength);

    NVMM_CHK_ERR(NvOsFopen((const char *)Nv3GPMux->URIConcatResv[0], (NvU32)Flags, &Nv3GPMux->FileResvHandle ));

    /**
    * TOTALATOMSSIZE * 4 corresponds to minimum sum of sizes of audiM.dat (32000),
    * videM.dat (32000),syncM.dat (32000) + 32000 extra - MOOV related
    * (max buffer size of video + audio) * 2 for ping and pong buffers
    * (1280 * 720 * 3 /2 + 9.8 kb) * 2 and after doing alignment check the total comes
    * to 2775040 bytes = 2710 kb
    * In total: 2710 kb + 125 kb = 2835 kb = 2.7 MB (MINTEMPMEMSIZE)
    * 2.7 MB * 2 = 5.4 MB for ping and pong buffers
    */

    TotalGraceSize= RESV_BUFFER_SIZE;

    Nv3GPMux->pResvRunBuffer = NvOsAlloc( (TotalGraceSize) * sizeof(NvU8));
    NVMM_CHK_MEM(Nv3GPMux->pResvRunBuffer);
    Message.ThreadShutDown = NV_FALSE;

    /**
      * Check if OPTIMAL_WRITE_SIZE is less than TOTAL_RESV_SIZE
      * and check if OPTIMAL_WRITE_SIZE is too small i.e. greater than
      * MINIMAL_OPTIMAL_WRITE_SIZE
      *
      * if false, then use our own way of writing data
      * if true, then split into chunks so that we use File System
      * OPTIMAL_WRITE_SIZE efficiently
    */
    if( (TOTAL_RESV_SIZE > OPTIMAL_WRITE_SIZE ) && (OPTIMAL_WRITE_SIZE >= MINIMAL_OPTIMAL_WRITE_SIZE) )
    {
        Message.LoopCntr = TOTAL_RESV_SIZE/OPTIMAL_WRITE_SIZE;
        TotalGraceSize = OPTIMAL_WRITE_SIZE;
    }
    else
    {
        Message.LoopCntr = TOTAL_RESV_SIZE/MIN_INITIAL_RESV_SIZE; //24,  24 * 256 * 1024 = 6 MB
        TotalGraceSize = MIN_INITIAL_RESV_SIZE;
    }

    Nv3GPMux->pPResvBuffer = NvOsAlloc(TotalGraceSize * sizeof(NvU8));
    NVMM_CHK_MEM(Nv3GPMux->pPResvBuffer);
    Message.pData = Nv3GPMux->pPResvBuffer;
    Message.DataSize = TotalGraceSize;
    NVMM_CHK_ERR(NvMMQueueEnQ(Nv3GPMux->MsgQueueResv, &Message, 0));

    /**
      * Trigger the thread to start writing MIN INITIAL RESV SIZE i.e. 6 MB
    */
    NvOsSemaphoreSignal(Nv3GPMux->ResvWriteSema);

cleanup:
    return Status;
}

static void
NvMM3GpMuxInitUserData(
    Nv3gpWriter *Nv3GPMux,
    NvMM3GpMuxInputParam *pInpParam)
{
    NvMM3GpMuxInpUserData *pInputUserData=NULL;
    NvMM3GpMuxUDTABox *MuxudtaBox= NULL;
    NvU32 i =0;
    NvU8 *DataBoxType = NULL;
    NvU8 *DataToCopy = NULL;
    NvU32 DataSize = 0;
    NvU32 *pBoxSize = 0;
    NvU8 *pUserData = NULL;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "++NvMM3GpMuxInitUserData \n"));
    MuxudtaBox = &Nv3GPMux->MuxUDTABox;
    // udta - 4 bytes, size 4 bytes
    MuxudtaBox->Box.BoxSize= sizeof(AtomBasicBox);
    COPY_FOUR_BYTES((MuxudtaBox->Box.BoxType), 'u','d','t','a');

    // TODO: Language needs to populated as explained in 26244-900 standard for all the boxes that have language

    /* Album Box Type */
    for(i = 0; i < TOTAL_USER_DATA_BOXES; i++)
    {
        pInputUserData = &pInpParam->InpUserData[NvMMMP4WriterDataInfo_Album + i];
        if(NV_TRUE == pInputUserData->boxPresent)
        {
            pUserData = pInputUserData->pUserData;
            switch(i)
            {
                case NvMMMP4WriterDataInfo_Album:
                {
                    DataToCopy = &AlbumUserData[0];
                    DataSize = ALBUM_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxALBMBox;
                    pBoxSize = &Nv3GPMux->MuxALBMBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxALBMBox.pAlbumtitle = pUserData;
                    Nv3GPMux->MuxALBMBox.TrackNumber = 0;
                }
                break;
                case NvMMMP4WriterDataInfo_Author:
                {
                    DataToCopy = &AuthorUserData[0];
                    DataSize = AUTHOR_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxAUTHBox;
                    pBoxSize = &Nv3GPMux->MuxAUTHBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxAUTHBox.pAuthor = pUserData;
                }
                break;
                case NvMMMP4WriterDataInfo_Classification:
                {
                    DataToCopy = &ClassificationUserData[0];
                    DataSize = CLASSIFICATION_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxCLSFBox;
                    pBoxSize = &Nv3GPMux->MuxCLSFBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxCLSFBox.pClassificationInfo = pUserData;
                }
                break;
                case NvMMMP4WriterDataInfo_Copyright:
                {
                    DataToCopy = &CopyrightUserData[0];
                    DataSize = COPYRIGHT_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxCPRTBox;
                    pBoxSize = &Nv3GPMux->MuxCPRTBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxCPRTBox.pCopyright = pUserData;
                }
                break;
                case NvMMMP4WriterDataInfo_Description:
                {
                    DataToCopy = &DescriptionUserData[0];
                    DataSize = DESCRIPTION_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxDSCPBox;
                    pBoxSize = &Nv3GPMux->MuxDSCPBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxDSCPBox.pDescription = pUserData;
                }
                break;
                case NvMMMP4WriterDataInfo_Genre:
                {
                    DataToCopy = &GenreUserData[0];
                    DataSize = GENRE_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxGNREBox;
                    pBoxSize = &Nv3GPMux->MuxGNREBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxGNREBox.pGenre = pUserData;
                }
                break;
                case NvMMMP4WriterDataInfo_Keywords:
                {
                    DataToCopy = &KeywordUserData[0];
                    DataSize = KEYWORD_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxKYWDBox;
                    pBoxSize = &Nv3GPMux->MuxKYWDBox.FullBox.Box.BoxSize;
                }
                break;
                case NvMMMP4WriterDataInfo_LocationInfo:
                {
                    DataToCopy = &LocationUserData[0];
                    DataSize = LOCATION_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxLOCIBox;
                    pBoxSize = &Nv3GPMux->MuxLOCIBox.FullBox.Box.BoxSize;
                }
                break;
                case NvMMMP4WriterDataInfo_PerfName:
                {
                    DataToCopy = &PerformerUserData[0];
                    DataSize = PERFORMER_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxPERFBox;
                    pBoxSize = &Nv3GPMux->MuxPERFBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxPERFBox.pPerformer = pUserData;
                }
                break;
                case NvMMMP4WriterDataInfo_Rating:
                {
                    DataToCopy = &RatingUserData[0];
                    DataSize = RATING_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxRTNGBox;
                    pBoxSize = &Nv3GPMux->MuxRTNGBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxRTNGBox.pRatingInfo = pUserData;
                }
                break;
                case NvMMMP4WriterDataInfo_Title:
                {
                    DataToCopy = &TitleUserData[0];
                    DataSize = TITLE_USER_SIZE;
                    DataBoxType = (NvU8 *)&Nv3GPMux->MuxTITLEBox;
                    pBoxSize = &Nv3GPMux->MuxTITLEBox.FullBox.Box.BoxSize;
                    Nv3GPMux->MuxTITLEBox.pTitle = pUserData;
                }
                break;
                case NvMMMP4WriterDataInfo_Year:
                {
                    COPY_FOUR_BYTES(Nv3GPMux->MuxYRRCBox.FullBox.Box.BoxType,'y','r','r','c');
                    Nv3GPMux->MuxYRRCBox.FullBox.VersionFlag= 0;
                    Nv3GPMux->MuxYRRCBox.RecordingYear = 1977;
                    // boxtype - 4 bytes,size - 4 bytes, versionflag = 4 bytes,* year-2 bytes
                    Nv3GPMux->MuxYRRCBox.FullBox.Box.BoxSize= 14;
                    pBoxSize = &Nv3GPMux->MuxYRRCBox.FullBox.Box.BoxSize;
                }
                break;
                case NvMMMP4WriterDataInfo_UserSpecific:
                {
                    //If box type is user specific, then directly use the name specfied by user
                    if(0 != pInputUserData->usrBoxType[0])
                    {
                        NvOsMemcpy(Nv3GPMux->MuxUSERBox.Box.BoxType,
                        pInputUserData->usrBoxType, NVMM_MAX_ATOM_SIZE-1);
                    }
                    else
                    {
                        COPY_FOUR_BYTES(Nv3GPMux->MuxUSERBox.Box.BoxType,'n','v','d','a');
                    }
                    // size - 4 bytes, userboxtype - 4 bytes
                    Nv3GPMux->MuxUSERBox.Box.BoxSize= sizeof(AtomBasicBox);
                    Nv3GPMux->MuxUSERBox.pUserData = pUserData;
                    // Add user data length(pInputUserData->sizeOfUserData) to udta size
                    pBoxSize = &Nv3GPMux->MuxUSERBox.Box.BoxSize;
                }
                break;

            }
            if(i != NvMMMP4WriterDataInfo_Year)
            {
                if(i != NvMMMP4WriterDataInfo_UserSpecific)
                    NvOsMemcpy(DataBoxType, DataToCopy, DataSize);
                if( (i != NvMMMP4WriterDataInfo_Keywords) && (i != NvMMMP4WriterDataInfo_LocationInfo) )
                {
                    (*pBoxSize) += pInputUserData->sizeOfUserData;
                }
            }
            MuxudtaBox->Box.BoxSize += (*pBoxSize);
        }
     }
    Nv3GPMux->FileSize += MuxudtaBox->Box.BoxSize;
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "--NvMM3GpMuxInitUserData\n"));
}

static void
NvMM3GpMuxWriterFreeResources(
    Nv3gpWriter *Nv3GPMux
)
{
    NvMMFileAVSWriteMsg Message;
    Message.pData = NULL;
    Message.DataSize = 0;
    Message.ThreadShutDown = NV_TRUE;
    Message.LoopCntr = 1;

    if(Nv3GPMux->MsgQueueResv)
        NvMMQueueEnQ(Nv3GPMux->MsgQueueResv, &Message, 0);
    //Stop the thread
    if(Nv3GPMux->ResvWriteSema)
        NvOsSemaphoreSignal(Nv3GPMux->ResvWriteSema);
    if(Nv3GPMux->ResvWriteDoneSema)
        NvOsSemaphoreWait(Nv3GPMux->ResvWriteDoneSema);
    if(Nv3GPMux->FileWriterResvThread)
    {
        NvOsThreadJoin (Nv3GPMux->FileWriterResvThread);
        Nv3GPMux->FileWriterResvThread = NULL;
    }
    if(Nv3GPMux->ResvWriteSema)
    {
        NvOsSemaphoreDestroy(Nv3GPMux->ResvWriteSema);
        Nv3GPMux->ResvWriteSema = NULL;
    }
    if(Nv3GPMux->ResvWriteDoneSema)
    {
        NvOsSemaphoreDestroy(Nv3GPMux->ResvWriteDoneSema);
        Nv3GPMux->ResvWriteDoneSema = NULL;
    }
    if(Nv3GPMux->MsgQueueResv)
    {
        NvMMQueueDestroy(&Nv3GPMux->MsgQueueResv);
        Nv3GPMux->MsgQueueResv = NULL;
    }
    if(Nv3GPMux->pPResvBuffer)
    {
        NvOsFree(Nv3GPMux->pPResvBuffer);
        Nv3GPMux->pPResvBuffer= NULL;
    }
    if(Nv3GPMux->pResvRunBuffer)
    {
        NvOsFree(Nv3GPMux->pResvRunBuffer);
        Nv3GPMux->pResvRunBuffer= NULL;
    }
    if(Nv3GPMux->FileResvHandle)
    {
        NvOsFclose(Nv3GPMux->FileResvHandle);
        Nv3GPMux->FileResvHandle = NULL;
    }
    if(Nv3GPMux->URIConcatResv[0])
    {
        NvOsFremove((const char *)Nv3GPMux->URIConcatResv[0]);
     }
    if(Nv3GPMux->URIConcatResv[0])
    {
        NvOsFree(Nv3GPMux->URIConcatResv[0]);
        Nv3GPMux->URIConcatResv[0] = NULL;
    }
}

static NvError
NvMM3GpMuxWriterDeallocate(
    Nv3gpWriter *Nv3GPMux,
    NvMM3GpMuxInputParam *pInpParam,
    NvMM3GpMuxContext *Context3GpMux)
{
    NvError Status = NvSuccess;
    NVMM_CHK_ARG(Nv3GPMux);

    WriterDeallocAudio(Nv3GPMux, pInpParam);
    WriterDeallocVideo(Nv3GPMux, pInpParam);

    NvMM3GpMuxWriterFreeResources(Nv3GPMux);

    if(pInpParam->pFilePath)
    {
        NvOsFree(pInpParam->pFilePath);
        pInpParam->pFilePath = NULL;
    }

    if(Context3GpMux->pIntMem)
    {
        NvOsFree(Context3GpMux->pIntMem);
        Context3GpMux->pIntMem = NULL;
    }
cleanup:
    return Status;
}

static void
NvMM3GpMuxInitGenericAtoms(
    Nv3gpWriter *Nv3GPMux,
    NvMM3GpMuxInputParam *pInpParam,
    NvU8 *pBufferOut,
    NvU32 *Offset)
{
    NvU8 *puctemp = NULL;
    NvMM3GpMuxMDATBox *tMuxmdatBox= NULL;
    NvMM3GpMuxMOOVBox *tMuxmoovBox= NULL;
    NvMM3GpMuxMVHDBox *tMuxmvhdBox= NULL;
    NvU8 *BoxType = NULL;
    NvU32 CurrentPointer =0;
    NvBool IsMp4 = NV_FALSE;

    puctemp = Nv3GPMux->pTempBuff;

    //write data for ftyp box in temp buffer
    COPY_FOUR_BYTES((puctemp + CurrentPointer), 0,0,0,0x14);
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((puctemp + CurrentPointer), 'f','t','y','p');
    CurrentPointer += sizeof(NvU32);

    if (!NvOsStrcmp(&pInpParam->FileExt[0], "mp4") )
        IsMp4 = NV_TRUE;

    if (NV_TRUE == IsMp4)
    {
        COPY_TWO_BYTES((puctemp + CurrentPointer), 'i','s');
    }
    else
    {
        COPY_TWO_BYTES((puctemp + CurrentPointer), '3','g');
    }
    CurrentPointer += sizeof(NvU8) * 2;

    if (NV_TRUE == IsMp4)
    {
        COPY_TWO_BYTES((puctemp + CurrentPointer), 'o','m');
    }
    else
    {
        if ((pInpParam->SpeechAudioFlag == 3) || (pInpParam->SpeechAudioFlag == 4) )
        {
            COPY_TWO_BYTES((puctemp + CurrentPointer), '2','a');
        }
        else
        {
            COPY_TWO_BYTES((puctemp + CurrentPointer), 'p','4');
        }
    }
    CurrentPointer += sizeof(NvU8) * 2;
    COPY_FOUR_BYTES((puctemp + CurrentPointer), 0, 0, 0x2,0);
    CurrentPointer += sizeof(NvU32);
    if (NV_TRUE == IsMp4)
    {
        COPY_TWO_BYTES((puctemp + CurrentPointer), 'i','s');
    }
    else
    {
        COPY_TWO_BYTES((puctemp + CurrentPointer), '3','g');
    }
    CurrentPointer += sizeof(NvU8) * 2;

    if (NV_TRUE == IsMp4)
    {
        COPY_TWO_BYTES((puctemp + CurrentPointer), 'o','2');
    }
    else
    {
        if( (pInpParam->SpeechAudioFlag == 3) || (pInpParam->SpeechAudioFlag == 4) )
        {
            COPY_TWO_BYTES((puctemp + CurrentPointer), '2','a');
        }
        else
        {
            COPY_TWO_BYTES((puctemp + CurrentPointer), 'p','4');
        }
    }
    CurrentPointer += sizeof(NvU8) * 2;
    //write data for skip box
    COPY_FOUR_BYTES((puctemp + CurrentPointer), 0, 0, 0,0x10);
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((puctemp + CurrentPointer), 's', 'k', 'i','p');
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((puctemp + CurrentPointer), 0x90, 1, 0x0F,1);
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((puctemp + CurrentPointer), 0x08, 0x40, 0xC6,1);
    CurrentPointer += sizeof(NvU32);

    //write data for media data i.e mdat box
    tMuxmdatBox = &Nv3GPMux->MuxMDATBox;
    tMuxmdatBox->Box.BoxSize= 8;
    BoxType = &tMuxmdatBox->Box.BoxType[0];
    COPY_FOUR_BYTES((BoxType), 'm', 'd', 'a', 't');
    CurrentPointer += sizeof(NvU32);
    //write to buffer for mdat box for writing to file
    COPY_FOUR_BYTES((puctemp + CurrentPointer), 'm', 'd', 'a', 't');
    CurrentPointer += sizeof(NvU32);
    //write data to file and update the file offset count
    NvOsMemcpy( pBufferOut + *Offset,Nv3GPMux->pTempBuff,CurrentPointer );
    *Offset += CurrentPointer;

    if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
    {
        Nv3GPMux->FileOffset32 += CurrentPointer;
        Nv3GPMux->FileSize = Nv3GPMux->FileOffset32;
    }
    else
    {
        Nv3GPMux->FileOffset += CurrentPointer;
        Nv3GPMux->FileSize = Nv3GPMux->FileOffset;
    }
    //write meta data for movie i.e moov
    tMuxmoovBox = &Nv3GPMux->MuxMOOVBox;
    tMuxmoovBox->Box.BoxSize = 8;
    BoxType = &tMuxmoovBox->Box.BoxType[0];
    COPY_FOUR_BYTES((BoxType), 'm', 'o', 'o', 'v');
    //write data for movie header i.e mvhd box
    tMuxmvhdBox = &Nv3GPMux->MuxMVHDBox;

    tMuxmvhdBox->FullBox.Box.BoxSize = 108;
    BoxType = &tMuxmvhdBox->FullBox.Box.BoxType[0];
    COPY_FOUR_BYTES((BoxType), 'm', 'v', 'h', 'd');
    tMuxmvhdBox->FullBox.VersionFlag = 0;
    tMuxmvhdBox->CreationTime = CREATION_TIME ;
    tMuxmvhdBox->ModTime = MODIFICATION_TIME;
    tMuxmvhdBox->TimeScale = TIME_SCALE;
    tMuxmvhdBox->NextTrackID = NEXT_TRACK_ID;

    Nv3GPMux->FileSize += tMuxmoovBox->Box.BoxSize + tMuxmvhdBox->FullBox.Box.BoxSize;

}

NvError
NvMM3GpMuxRec_3gp2WriteInit(
    NvMM3GpMuxInputParam *pInpParam,
    NvU8 *pBufferOut,
    NvU32 *Offset,
    void *StructMem,
    char *Uripath,
    char *Uritemppath,
    NvBool Mode)
{
    NvError Status = NvSuccess;
    NvMM3GpMuxContext *Context3GpMux = NULL;
    Nv3gpWriter *Nv3GPMux = NULL;
    NvU32 FileNameLength = 0;
    NvU32 TmpFileNameLength = 0;
    char *URI = NULL;

   NVMM_CHK_ARG(StructMem && pInpParam && Offset && Uripath);
   Context3GpMux = (NvMM3GpMuxContext *)StructMem;

    Context3GpMux->IntMemSize = 0;
    pInpParam->AudioFrameCount = 0;
    //internal memory size
    Context3GpMux->IntMemSize = sizeof(Nv3gpWriter) + TEMP_INPUT_BUFF_SIZE;

    Context3GpMux->pIntMem = NvOsAlloc(Context3GpMux->IntMemSize);
    NVMM_CHK_MEM(Context3GpMux->pIntMem);
    NvOsMemset( Context3GpMux->pIntMem, 0, (sizeof(Nv3gpWriter) + TEMP_INPUT_BUFF_SIZE));

    //assign memory for global library data structure
    Nv3GPMux = (Nv3gpWriter *)Context3GpMux->pIntMem;
    Nv3GPMux->Mode = Mode;

    if(Uritemppath == NULL)
    {
        // temp file path not set (szuritemppath = NULL)
        // So Keep temporary files in the same path as pointed by output file path
        // "opFilePath" points to path of output  file after removing file name
        NVMM_CHK_ERR( NvMM3GpMuxExtractOutputFilePath(Uripath, &pInpParam->pFilePath));
    }
    else
    {
        // temp file path set (szuritemppath !=NULL)
        // use temp path that is set by client
        NVMM_CHK_ERR(NvMM3GpMuxExtractTempFilePath(Uritemppath, &pInpParam->pFilePath));
    }

    URI = "tempx.dat";
    FileNameLength = NvOsStrlen(pInpParam->pFilePath);
    TmpFileNameLength = NvOsStrlen(URI);

    // Reserved File
    NVMM_CHK_ERR(NvMM3GpMuxResvInit(Nv3GPMux, FileNameLength, TmpFileNameLength, pInpParam));

    // Audio
    NVMM_CHK_ERR(AudioInit(Nv3GPMux, FileNameLength, TmpFileNameLength, pInpParam));

    // Video
    NVMM_CHK_ERR(VideoInit(Nv3GPMux, FileNameLength, TmpFileNameLength, pInpParam));

    //assign memory for tempory library buffer
    Nv3GPMux->pTempBuff = (NvU8 *)((NvU32)Context3GpMux->pIntMem + sizeof(Nv3gpWriter));

    // General Initializations
    Nv3GPMux->AccumFracTS = 0;
    Nv3GPMux->AccumFracAudioTS = 0;
    Nv3GPMux->AccumEntries = 0;
    Nv3GPMux->FileOffset = 0;
    Nv3GPMux->FileOffset32 = 0;
    Nv3GPMux->FileSize = 0;
    Nv3GPMux->InsertTitle = 0;
    Nv3GPMux->InsertAuthor = 0;
    Nv3GPMux->InsertDescription = 0;
    Nv3GPMux->Cnt = 0;
    Nv3GPMux->MoovCount = 0;
    Nv3GPMux->MoofCount = 0;
    Nv3GPMux->MoovChkComplete = 0;
    Nv3GPMux->MoofChkComplete = 0;
    Nv3GPMux->PrevRateFlag = -1;
    Nv3GPMux->SCloseMuxFlag = 1;
    Nv3GPMux->ConsumedDataSize=0;
    Nv3GPMux->HeadSize =0;
    Nv3GPMux->Context3GpMux = Context3GpMux;
    Nv3GPMux->VideoWriteStatus = NvSuccess;
    Nv3GPMux->AudioWriteStatus = NvSuccess;
    pInpParam->OverflowFlag = NV_FALSE;
    NvOsMemset(Nv3GPMux->RateMapTable,0,16);
    Nv3GPMux->DTXon = pInpParam->DTXon;
    Nv3GPMux->VARrate = 0;

    // FTYP, SKIP, MDAT, MOOv, MVHD
    NvMM3GpMuxInitGenericAtoms(Nv3GPMux, pInpParam, pBufferOut, Offset);

    //Track related stuff, so since it's not known apriori what all tracks are present, so
    //the filesize can't be updated here. Instead use SpeechBoxesSize and VideoBoxesSize variables
    //of the global struct
    InitAudioTrakAtom(Nv3GPMux, pInpParam);
    InitVideoTrakAtom(Nv3GPMux, pInpParam);
    NvMM3GpMuxInitUserData(Nv3GPMux,pInpParam);

    Nv3GPMux->HeadSize = Nv3GPMux->FileSize;

    if(NV_TRUE == Nv3GPMux->ResvFileWriteFailed)
    {
        Status = Nv3GPMux->ResvThreadStatus;
    }

cleanup:

if(NvSuccess != Status)
    (void)NvMM3GpMuxWriterDeallocate(Nv3GPMux, pInpParam, Context3GpMux);

    return Status;
}

static NvError
NvMM3GpMuxWriteUserDataInfo(
    Nv3gpWriter *Nv3GPMux,
    CP_PIPETYPE  *pPipe,
    CPhandle Cphandle,
    NvMM3GPWriterUserDataTypes UserDataType)
{
    NvError Status = NvSuccess;
    NvU32 ulTempData = 0;
    NvU8 *pucTemp = 0;
    NvMM3GpMuxALBMBox *pMuxalbmBox = NULL;
    NvMM3GpMuxAUTHBox *pMuxauthBox = NULL;
    NvMM3GpMuxCLSFBox *pMuxclsfBox = NULL;
    NvMM3GpMuxCPRTBox *pMuxcprtBox = NULL;
    NvMM3GpMuxDSCPBox *pMuxdscpBox = NULL;
    NvMM3GpMuxGNREBox *pMuxgnreBox = NULL;
    NvMM3GpMuxKYWDBox *pMuxkywdBox = NULL;
    NvMM3GpMuxLOCIBox *pMuxlociBox = NULL;
    NvMM3GpMuxPERFBox *pMuxperfBox = NULL;
    NvMM3GpMuxRTNGBox *pMuxrtngBox = NULL;
    NvMM3GpMuxTITLEBox *pMuxtitlBox = NULL;
    NvMM3GpMuxYRRCBox *pMuxyrrcBox = NULL;
    NvMM3GpMuxUSERBox *pMuxuserBox = NULL;
    NvU16 clsfTempTable = 0;
    NvU32 CurrentPointer =0;
    AtomBasicBox Box;
    NvU8 *DataToBeWritten = NULL;

    pucTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pucTemp, 0, TEMP_INPUT_BUFF_SIZE);
    NvOsMemset(&Box, 0, sizeof(AtomBasicBox));
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "++NvMM3GpMuxWriteUserDataInfo \n"));
    /* Write box size */
    switch(UserDataType)
    {
        case NvMMMP4WriterDataInfo_Album:
        {
            pMuxalbmBox = &Nv3GPMux->MuxALBMBox;
            ulTempData = pMuxalbmBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxalbmBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Author:
        {
            pMuxauthBox = &Nv3GPMux->MuxAUTHBox;
            ulTempData = pMuxauthBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxauthBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Classification:
        {
            pMuxclsfBox = &Nv3GPMux->MuxCLSFBox;
            ulTempData = pMuxclsfBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxclsfBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Copyright:
        {
            pMuxcprtBox = &Nv3GPMux->MuxCPRTBox;
            ulTempData = pMuxcprtBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxcprtBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Description:
        {
            pMuxdscpBox = &Nv3GPMux->MuxDSCPBox;
            ulTempData = pMuxdscpBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxdscpBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Genre:
        {
            pMuxgnreBox = &Nv3GPMux->MuxGNREBox;
            ulTempData = pMuxgnreBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxgnreBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Keywords:
        {
            pMuxkywdBox = &Nv3GPMux->MuxKYWDBox;
            ulTempData = pMuxkywdBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxkywdBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_LocationInfo:
        {
            pMuxlociBox = &Nv3GPMux->MuxLOCIBox;
            ulTempData = pMuxlociBox->FullBox.Box.BoxSize;
        }
        break;
        case NvMMMP4WriterDataInfo_PerfName:
        {
            pMuxperfBox = &Nv3GPMux->MuxPERFBox;
            ulTempData = pMuxperfBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxperfBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Rating:
        {
            pMuxrtngBox = &Nv3GPMux->MuxRTNGBox;
            ulTempData = pMuxrtngBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxrtngBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Title:
        {
            pMuxtitlBox = &Nv3GPMux->MuxTITLEBox;
            ulTempData = pMuxtitlBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxtitlBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_Year:
        {
            pMuxyrrcBox = &Nv3GPMux->MuxYRRCBox;
            ulTempData = pMuxyrrcBox->FullBox.Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxyrrcBox->FullBox.Box, sizeof(AtomBasicBox));
        }
        break;
        case NvMMMP4WriterDataInfo_UserSpecific:
        {
            pMuxuserBox = &Nv3GPMux->MuxUSERBox;
            ulTempData = pMuxuserBox->Box.BoxSize;
            NvOsMemcpy(&Box, &pMuxuserBox->Box, sizeof(AtomBasicBox));
        }
        break;
        default:
        {
            NVMM_CHK_ERR(NvError_WriterUnsupportedUserData);
        }
        break;
    }
    NvMM3GpMuxConvertToBigEndian(pucTemp, &CurrentPointer, ulTempData);
    NvMM3GpMuxGetBoxName(pucTemp, &CurrentPointer, Box);
    if(UserDataType != NvMMMP4WriterDataInfo_UserSpecific)
    {
        // Version Flag
        COPY_FOUR_BYTES((pucTemp + CurrentPointer), 0,0,0,0);
        CurrentPointer += sizeof(NvU32);
    }
    if(NvMMMP4WriterDataInfo_Classification == UserDataType)
    {
        //classificationEntity
        ulTempData = pMuxclsfBox->ClassificationEntity;
        NvMM3GpMuxConvertToBigEndian(pucTemp, &CurrentPointer, ulTempData);
        // classificationTable */
        clsfTempTable = pMuxclsfBox->ClassificationTable;
        COPY_TWO_BYTES((pucTemp + CurrentPointer),(NvU8)(clsfTempTable>>8),(NvU8)((clsfTempTable) & 0xFF));
        CurrentPointer += sizeof(NvU8) * 2;
    }
    if(NvMMMP4WriterDataInfo_Rating == UserDataType)
    {
        // ratingEntity
        ulTempData = pMuxrtngBox->RatingEntity;
        NvMM3GpMuxConvertToBigEndian(pucTemp, &CurrentPointer, ulTempData);
        // ratingCriteria
        ulTempData = pMuxrtngBox->RatingCriteria;
        NvMM3GpMuxConvertToBigEndian(pucTemp, &CurrentPointer, ulTempData);
    }
    if( (UserDataType != NvMMMP4WriterDataInfo_UserSpecific) && (UserDataType != NvMMMP4WriterDataInfo_Year))
   {
       // Language
       COPY_TWO_BYTES((pucTemp + CurrentPointer),0,0);
       CurrentPointer += sizeof(NvU8) * 2;
   }

    if(NvMMMP4WriterDataInfo_Keywords == UserDataType)
        CurrentPointer= pMuxkywdBox->FullBox.Box.BoxSize;
    if(NvMMMP4WriterDataInfo_LocationInfo == UserDataType)
        CurrentPointer= pMuxlociBox->FullBox.Box.BoxSize;
    if(NvMMMP4WriterDataInfo_Year == UserDataType)
    {
        ulTempData= pMuxyrrcBox->RecordingYear;
       COPY_TWO_BYTES((pucTemp + CurrentPointer),(NvU8)(ulTempData>>8),(NvU8)((ulTempData) & 0xFF));
       CurrentPointer += sizeof(NvU8) * 2;
    }

    NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)pucTemp, CurrentPointer));

    switch(UserDataType)
    {
        case NvMMMP4WriterDataInfo_Album:
        {
            // pAlbumtitle info is written tracknumber - 1 byte*/
            CurrentPointer = (pMuxalbmBox->FullBox.Box.BoxSize - CurrentPointer - 1);
            NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)(pMuxalbmBox->pAlbumtitle), CurrentPointer));
            CurrentPointer = 1;
            NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)(&pMuxalbmBox->TrackNumber), CurrentPointer));
        }
        break;
        case NvMMMP4WriterDataInfo_Author:
        {
            CurrentPointer = (pMuxauthBox->FullBox.Box.BoxSize - CurrentPointer);
            DataToBeWritten = pMuxauthBox->pAuthor;
        }
        break;
        case NvMMMP4WriterDataInfo_Classification:
        {
            CurrentPointer = (pMuxclsfBox->FullBox.Box.BoxSize - CurrentPointer);
            DataToBeWritten = pMuxclsfBox->pClassificationInfo;
        }
        break;
        case NvMMMP4WriterDataInfo_Copyright:
        {
            CurrentPointer = (pMuxcprtBox->FullBox.Box.BoxSize - CurrentPointer);
            DataToBeWritten = pMuxcprtBox->pCopyright;
        }
        break;
        case NvMMMP4WriterDataInfo_Description:
        {
            CurrentPointer = (pMuxdscpBox->FullBox.Box.BoxSize - CurrentPointer);
            DataToBeWritten = pMuxdscpBox->pDescription;
        }
        break;
        case NvMMMP4WriterDataInfo_Genre:
        {
            CurrentPointer = (pMuxgnreBox->FullBox.Box.BoxSize - CurrentPointer);
            DataToBeWritten = pMuxgnreBox->pGenre;
        }
        break;
        case NvMMMP4WriterDataInfo_PerfName:
        {
            CurrentPointer = (pMuxperfBox->FullBox.Box.BoxSize - CurrentPointer);
            DataToBeWritten = pMuxperfBox->pPerformer;
        }
        break;
        case NvMMMP4WriterDataInfo_Rating:
        {
            CurrentPointer = (pMuxrtngBox->FullBox.Box.BoxSize - CurrentPointer);
            DataToBeWritten = pMuxrtngBox->pRatingInfo;
        }
        break;
        case NvMMMP4WriterDataInfo_Title:
        {
           CurrentPointer = (pMuxtitlBox->FullBox.Box.BoxSize - CurrentPointer);
           DataToBeWritten = pMuxtitlBox->pTitle;
        }
        break;
        case NvMMMP4WriterDataInfo_UserSpecific:
        {
            CurrentPointer = (pMuxuserBox->Box.BoxSize - CurrentPointer);
            DataToBeWritten = pMuxuserBox->pUserData;
        }
        break;
        case NvMMMP4WriterDataInfo_Keywords:
        case NvMMMP4WriterDataInfo_LocationInfo:
        case NvMMMP4WriterDataInfo_Year:
        default:
        break;
     }

    if( (NvMMMP4WriterDataInfo_Author == UserDataType) ||
        (NvMMMP4WriterDataInfo_Classification == UserDataType) ||
        (NvMMMP4WriterDataInfo_Copyright == UserDataType) ||
        (NvMMMP4WriterDataInfo_Description == UserDataType) ||
        (NvMMMP4WriterDataInfo_PerfName == UserDataType) ||
        (NvMMMP4WriterDataInfo_Rating == UserDataType) ||
        (NvMMMP4WriterDataInfo_Title == UserDataType) ||
        (NvMMMP4WriterDataInfo_UserSpecific == UserDataType) )
    {
        NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)(DataToBeWritten), CurrentPointer));
    }

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "--NvMM3GpMuxWriteUserDataInfo \n"));
cleanup:
    return Status;
}

static NvError
NvMM3GpMuxwriteUserData(
    Nv3gpWriter *Nv3GPMux,
    CP_PIPETYPE  *pPipe,
    CPhandle Cphandle,
    NvMM3GpMuxInputParam *pInpParam)
{
    NvError Status = NvSuccess;
    NvMM3GpMuxInpUserData *pInputUserData=NULL;
    NvMM3GpMuxUDTABox *pMuxudtaBox= NULL;
    NvU8 *pTemp = 0;
    NvU32 CurrentPointer =0;
    NvU32 i = 0;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "++NvMM3GpMuxwriteUserData \n"));

    pTemp = Nv3GPMux->pTempBuff;
    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);

    pMuxudtaBox = &Nv3GPMux->MuxUDTABox;
    NvMM3GpMuxConvertToBigEndian(pTemp, &CurrentPointer, pMuxudtaBox->Box.BoxSize);
    NvMM3GpMuxGetBoxName(pTemp, &CurrentPointer, pMuxudtaBox->Box);
    NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)pTemp, CurrentPointer));

    for(i = 0; i < TOTAL_USER_DATA_BOXES; i++)
    {
        pInputUserData = &pInpParam->InpUserData[NvMMMP4WriterDataInfo_Album + i];
        if(NV_TRUE == pInputUserData->boxPresent)
        {
            NVMM_CHK_ERR(NvMM3GpMuxWriteUserDataInfo(Nv3GPMux, pPipe, Cphandle,(NvMM3GPWriterUserDataTypes)(NvMMMP4WriterDataInfo_Album + i)) );
        }
    }
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "--NvMM3GpMuxwriteUserData \n"));
cleanup:
    return Status;
}

NvError
NvMM3GpMuxRecDelReservedFile(
    NvMM3GpMuxContext Context3GpMux)
{
    NvError Status = NvSuccess;
    Nv3gpWriter *Nv3GPMux=NULL;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "++NvMM3GpMuxRecDelReservedFile \n"));

    Nv3GPMux = (Nv3gpWriter *)Context3GpMux.pIntMem;
    NVMM_CHK_ARG(Nv3GPMux);
    NvMM3GpMuxWriterFreeResources(Nv3GPMux);

    cleanup:
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "--NvMM3GpMuxRecDelReservedFile \n"));
    return Status;
}

static NvError
NvMM3GpMuxFindTempFileSize(
    char *FileName,
    NvOsFileHandle *FileHandle,
    NvU64 *Size)
{
    NvError Status = NvSuccess;

    // Open Audio MOOV temp file
    NVMM_CHK_ERR(NvOsFopen((const char *)FileName, NVOS_OPEN_READ, FileHandle));
    //Seek to end on Temp file
    NVMM_CHK_ERR(NvOsFseek(*FileHandle, 0, NvOsSeek_End ));
    // Get the end position in audio temp file
    NVMM_CHK_ERR(NvOsFtell(*FileHandle, (NvU64*)Size));
    NVMM_CHK_ERR(NvOsFseek(*FileHandle, 0, NvOsSeek_Set ));
cleanup:
    return Status;
}
static NvError
calcMaxMoovEntriesInBytes(
    NvMM3GpMuxInputParam *pInpParam,
    Nv3gpWriter *Nv3GPMux)
{
    NvError Status = NvSuccess;
    NvU64 TotalAudioSize=0;
    NvU64 TotalVideoSize = 0;
    NvU64 TotalGraceSize=0;

    if(pInpParam->AudioVideoFlag == NvMM_AV_AUDIO_PRESENT || pInpParam->AudioVideoFlag == NvMM_AV_AUDIO_VIDEO_BOTH)
        NVMM_CHK_ERR( NvMM3GpMuxFindTempFileSize( Nv3GPMux->URIConcatAudio[0], &Nv3GPMux->FileAudioHandle, &TotalAudioSize));

    if(pInpParam->AudioVideoFlag == NvMM_AV_VIDEO_PRESENT || pInpParam->AudioVideoFlag == NvMM_AV_AUDIO_VIDEO_BOTH)
        NVMM_CHK_ERR(NvMM3GpMuxFindTempFileSize( Nv3GPMux->URIConcatVideo[0], &Nv3GPMux->FileVideoHandle, &TotalVideoSize));

    // Allocate memory for extracting moov entries from
    if(pInpParam->AudioVideoFlag  == NvMM_AV_AUDIO_VIDEO_BOTH)
    {
        if(TotalAudioSize > TotalVideoSize)
            TotalGraceSize = TotalAudioSize;
        else
            TotalGraceSize = TotalVideoSize ;
    }
    else if(pInpParam->AudioVideoFlag  == NvMM_AV_AUDIO_PRESENT)
        TotalGraceSize = TotalAudioSize;
    else if(pInpParam->AudioVideoFlag  == NvMM_AV_VIDEO_PRESENT )
        TotalGraceSize = TotalVideoSize ;

    // Allocate memory twice of actual size to keep converted linear data
    Nv3GPMux->GraceSize = TotalGraceSize;

    Nv3GPMux->pResvRunBuffer = (NvU8*)NvOsAlloc((NvU32) (TotalGraceSize << 1) * sizeof(NvU8));
    NVMM_CHK_MEM(Nv3GPMux->pResvRunBuffer);

cleanup:
    return Status;
}

static NvError
NvMM3GpMuxWritePendingAVSMoovEntry(
    Nv3gpWriter *Nv3GPMux)
{
    NvError Status = NvSuccess;

    // Audio
    if(Nv3GPMux->AudioCount)
    {
        NVMM_CHK_ERR(NvOsFwrite( Nv3GPMux->FileAudioHandle,
        (CPbyte *)(Nv3GPMux->pPAudioBuffer),
        (TOTAL_ENTRIESPERATOM* SIZEOFATOMS) +
        ACTUAL_ATOM_ENTRIES));
    }
    NvOsFclose(Nv3GPMux->FileAudioHandle);
    Nv3GPMux->FileAudioHandle = NULL;

    //Video
    if(Nv3GPMux->VideoCount)
    {
        NVMM_CHK_ERR(NvOsFwrite( Nv3GPMux->FileVideoHandle,
        (CPbyte *)(Nv3GPMux->pPVideoBuffer),
        (TOTAL_ENTRIESPERATOM* SIZEOFATOMS) +
        ACTUAL_ATOM_ENTRIES));
    }
    NvOsFclose(Nv3GPMux->FileVideoHandle);
    Nv3GPMux->FileVideoHandle = NULL;

    // Sync
    if(Nv3GPMux->SyncCount)
        NVMM_CHK_ERR(NvOsFwrite( Nv3GPMux->FileSyncHandle, (CPbyte *)(Nv3GPMux->pPSyncBuffer), Nv3GPMux->SyncCount << 2 ));
    NvOsFclose(Nv3GPMux->FileSyncHandle);
    Nv3GPMux->FileSyncHandle = NULL;

cleanup:
    return Status;
}

static NvError
NvMM3GpMuxVProcessNALUnits(
    NvMM3GpMuxSplitMode *pSpMode,
    Nv3gpWriter *Nv3GPMux)
{
    NvError Status = NvSuccess;
    NvU32 Count = 0;
    NvU32 SPSLength = 0;
    NvU32 PPSLength = 0;
    NvU32 HeaderLength = 0;
    NvU32 SPSStartPos = 0;
    NvU32 PPSStartPos = 0;
    NvU32 FlagSPSReached = 0;
    NvU32 FlagPPSReached = 0;
    NvU32 NALUnitLength;
    NvU8 *Temp = NULL;
    NvMM3GpMuxTSPSTable *pCurrspsTable = NULL;
    NvMM3GpMuxPPSTable *pCurrppsTable = NULL;
    //currently we assume that in 1 video buffer at the max we will have 1 Sequence parameter set and
    // 1 Picture parameter set
    //Also sequence parameter set preceeds picture parameter set

    if(pSpMode->SplitFlag == NV_TRUE)
        Temp = pSpMode->ReInitBuffer;
    else
        Temp = Nv3GPMux->pVideo;

    SPSLength = PPSLength = 0;
    SPSLength = PPSLength = 0;
    while(Count < Nv3GPMux->VideoBuffDataSize)
    {
        NALUnitLength = (NvU32)(Temp[0] << 24);
        NALUnitLength |= (NvU32)(Temp[1] << 16);
        NALUnitLength |= (NvU32)(Temp[2] << 8);
        NALUnitLength |= (NvU32)(Temp[3]);

        if((Temp[4] & 0x1f) == 7)
            FlagSPSReached = 1;
        if((Temp[4] & 0x1f) == 8)
            FlagPPSReached = 1;

         if(FlagSPSReached)
         {
             SPSStartPos = Count;
             SPSLength = NALUnitLength;
             HeaderLength += (SPSLength + 4);

             pCurrspsTable = (NvMM3GpMuxTSPSTable *)((NvU32)Nv3GPMux->MuxAVCCBox.pPspsTable +
                 (Nv3GPMux->MuxAVCCBox.NumOfSequenceParameterSets*sizeof(NvMM3GpMuxTSPSTable)));
             pCurrspsTable->SequenceParameterSetLength = (NvU16)SPSLength;
             if(NV_FALSE == pSpMode->SplitFlag)
                 NvOsMemcpy(pCurrspsTable->SequenceParameterSetNALUnit, Nv3GPMux->pVideo + SPSStartPos + 4 , SPSLength);
             else
                 NvOsMemcpy(pCurrspsTable->SequenceParameterSetNALUnit, pSpMode->ReInitBuffer + SPSStartPos + 4 , SPSLength);

              Nv3GPMux->MuxAVCCBox.NumOfSequenceParameterSets++;
              //copy the Decoder specific info to avcC box structre variable
              Nv3GPMux->MuxAVCCBox.AVCProfileIndication = pCurrspsTable->SequenceParameterSetNALUnit[1];
              Nv3GPMux->MuxAVCCBox.Profile_compatibility = pCurrspsTable->SequenceParameterSetNALUnit[2];
              Nv3GPMux->MuxAVCCBox.AVCLevelIndication = pCurrspsTable->SequenceParameterSetNALUnit[3];
              FlagSPSReached = 0;
         }
         if(FlagPPSReached)
         {
             PPSStartPos = Count;
             PPSLength = NALUnitLength;
             HeaderLength += (PPSLength + 4);

             pCurrppsTable = (NvMM3GpMuxPPSTable *)((NvU32)Nv3GPMux->MuxAVCCBox.pPpsTable +
                 (Nv3GPMux->MuxAVCCBox.NumOfPictureParameterSets*sizeof(NvMM3GpMuxPPSTable)));
             pCurrppsTable->PictureParameterSetLength = (NvU16)PPSLength;
             if(NV_FALSE == pSpMode->SplitFlag)
                 NvOsMemcpy(pCurrppsTable->PictureParameterSetNALUnit, Nv3GPMux->pVideo + PPSStartPos + 4 , PPSLength);
             else
                  NvOsMemcpy(pCurrppsTable->PictureParameterSetNALUnit, pSpMode->ReInitBuffer + PPSStartPos + 4 , PPSLength);
              Nv3GPMux->MuxAVCCBox.NumOfPictureParameterSets++;
              FlagPPSReached = 0;
         }
         Count += NALUnitLength + 4;
         Temp += NALUnitLength + 4;
    }

    if(NV_FALSE == pSpMode->SplitFlag)
    {
        pSpMode->ReInitBuffer = NvOsAlloc(HeaderLength * sizeof(NvU8));
        NVMM_CHK_MEM(pSpMode->ReInitBuffer);
        NvOsMemcpy(pSpMode->ReInitBuffer, Nv3GPMux->pVideo, HeaderLength * sizeof(NvU8));
        Nv3GPMux->pVideo += HeaderLength;
        Nv3GPMux->VideoBuffDataSize -= HeaderLength;
    }
    pSpMode->SplitFlag = NV_FALSE;

cleanup:
    return Status;
}

static void
NvMM3GpMuxProcessFirstVideoFrame(
    Nv3gpWriter *Nv3GPMux)
{
    NvU32 Header = 0;
    NvU32 Count = 0;
    NvU32 BuffSz = Nv3GPMux->VideoBuffDataSize;
    NvU8 *pTemp = Nv3GPMux->pVideo;

    //parse for GOV Header
    while(Count < (BuffSz-4))
    {
        Header = (NvU32)(pTemp[0] << 24);
        Header |= (NvU32)(pTemp[1] << 16);
        Header |= (NvU32)(pTemp[2] << 8);
        Header |= (NvU32)(pTemp[3]);

        if((Header == 0x000001B3) || (Header == 0x000001B6) || (Header>>10 == 0x20))
            break;
        pTemp ++;
        Count++;
    }

    //copy the Decoder specific info to esds box structre variable
    NvOsMemcpy(Nv3GPMux->MuxESDSBox[VIDEO_TRACK].DecoderSpecificInfo, Nv3GPMux->pVideo, Count);

    //updata the esds box variable
    Nv3GPMux->MuxESDSBox[VIDEO_TRACK].DataLen1 += (NvU8)Count;
    Nv3GPMux->MuxESDSBox[VIDEO_TRACK].DataLen2 += (NvU8)Count;
    Nv3GPMux->MuxESDSBox[VIDEO_TRACK].DataLen3 = (NvU8)Count;
    Nv3GPMux->VideoBoxesSize += Count;
    //update the video buffer remaining data size
    Nv3GPMux->VideoBuffDataSize -= Count;

    //copy the remaining data to start of video buffer
    NvOsMemcpy(Nv3GPMux->pVideo, Nv3GPMux->pVideo + Count, Nv3GPMux->VideoBuffDataSize);
}

static void
NvMM3GpMuxPrepareEntry(
    NvU8 *pBuffer,
    NvU32 EntryOffSet,
    NvU32 Count,
    NvU32 Val,
    NvU32 Lsh)
{
    NvU32 CurrentPointer = 0;
    NvU8 TempBuff[4];
    NvU32 i =0;

    NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Val);

    // STTS table: Sample count
    for (i = 0; i < CurrentPointer; i++)
        pBuffer[i + EntryOffSet + (Count << Lsh)] = TempBuff[i];

}
static NvError
NvMM3GpMuxCountNoSpeechSamples(
    NvMM3GpMuxInputParam *pInpParam,
    Nv3gpWriter *Nv3GPMux)
{
    NvError Status=NvSuccess;
    NvU32 i =0;
    NvU32 BytesRead = 0;
    NvU32 BytesToRead = 0;
    NvU8 PackedSizeAmrNbMMS[] = {13, 14, 16, 18, 20, 21, 27, 32, 6, 1, 1, 1, 1, 1, 1, 1};
    NvU8 PackedSizeAmrNbIF1[] = {15, 16, 18, 20, 22, 23, 29, 34, 9, 6, 6, 6, 3, 3, 3, 1};
    NvU8 PackedSizeAmrNbIF2[] = {13, 14, 16, 18, 19, 21, 26, 31, 6, 6, 6, 6, 1, 1, 1, 1};
    NvU8 packed_size_wbamr[16] = {17, 23, 32, 36, 40, 46, 50, 58, 60, 5, 0, 0, 0, 0, 0, 0};
    NvU8 SizeQcelp[5] = {1, 4, 8, 17, 35};
    NvU8 *pTemp = NULL;
    NvU32 CurrAudioDelta = 0;
    NvU32 Temp = 0;
    NvU8 TempBuff[4];
    NvU64 Temp64 = 0;
    NvU64 TSDiff = 0;
    NvU64 TSTruncated = 0;
    NvU32 CurrentPointer = 0;


    pTemp = Nv3GPMux->pSpeech;
    while (BytesRead < Nv3GPMux->SpeechBuffDataSize && !pInpParam->bAudioDurationLimit)
    {
        if(Nv3GPMux->AudioCount == TOTAL_ENTRIESPERATOM)
        {
            Nv3GPMux->SpeechOffset = BytesRead;
            break;
        }
        pInpParam->AudioFrameCount++;
        if(pInpParam->SpeechAudioFlag == 0)
        {
            if(pInpParam->IsAMRIF1)
            {
                BytesToRead = (PackedSizeAmrNbIF1[((pTemp[0] >> 4) & 0x0F)]);
                if (pTemp[0] == 0x44)
                    BytesToRead = PackedSizeAmrNbIF1[8];
                else if (pTemp[0] == 0x7C)
                    BytesToRead = PackedSizeAmrNbIF1[15];
            }
            else if (pInpParam->IsAMRIF2)
            {
                BytesToRead= (PackedSizeAmrNbIF2[((pTemp[0]) & 0x0F)]);
            }
            else //default MMS
            {
                BytesToRead= (PackedSizeAmrNbMMS[((pTemp[0] >> 3) & 0x0F)]);
            }
        }
        else if (pInpParam->SpeechAudioFlag == 1)
        {
            BytesToRead= (packed_size_wbamr[((pTemp[0] >> 3) & 0x0F)] + 1);
        }
        else if ((pInpParam->SpeechAudioFlag == 3) || (pInpParam->SpeechAudioFlag == 4))
        {
            if (pTemp[0] > 4)
            {
                NVMM_CHK_ERR(NvError_WriterUnsupportedStream);
            }
            else
            {
                Nv3GPMux->SpeechSampleSize = SizeQcelp[pTemp[0]];  //CHECK!!
            }

            BytesToRead = SizeQcelp[pTemp[0]];
        }
        else if (pInpParam->SpeechAudioFlag == 2)
        {
            BytesToRead = Nv3GPMux->SpeechBuffDataSize;
        }

        BytesRead += BytesToRead;
        Nv3GPMux->ConsumedDataSize = BytesRead;
        pTemp = Nv3GPMux->pSpeech + BytesRead;

        if ((Nv3GPMux->Mode == NvMM_Mode32BitAtom &&
            (pInpParam->PrevAudioFileOffSet != (NvU64)Nv3GPMux->FileOffset32))
            ||(Nv3GPMux->Mode == NvMM_Mode32BitAtom &&
            0 == pInpParam->AudioStcoIndex))
        {
            //enter in sample tables
            //update table entry count for chunk offset
            CurrentPointer = 0;
            Temp = Nv3GPMux->FileOffset32;;
            NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);

            // stco
            for(i = 0; i < CurrentPointer; i++)
            {
                Nv3GPMux->pPAudioBuffer[i + CO64_OFFSET +
                    ACTUAL_ATOM_ENTRIES +
                    (pInpParam->AudioStcoIndex << 2)] = TempBuff[i];
            }
            Nv3GPMux->MuxSTCOBox[SOUND_TRACK].ChunkCount ++;
            Nv3GPMux->MuxSTCOBox[SOUND_TRACK].FullBox.Box.BoxSize+= sizeof(NvU32);
            Nv3GPMux->SpeechBoxesSize += sizeof(NvU32);
            pInpParam->PrevAudioFileOffSet = (NvU64)(Nv3GPMux->FileOffset32 +
                BytesToRead);
            pInpParam->AudStscEntries = 1;
        }
        else if ((Nv3GPMux->Mode == NvMM_Mode64BitAtom &&
            (pInpParam->PrevAudioFileOffSet != Nv3GPMux->FileOffset))
            || (Nv3GPMux->Mode == NvMM_Mode64BitAtom &&
            0 == pInpParam->AudioStcoIndex))
        {
            //enter in sample tables
            //update table entry count for chunk offset
            CurrentPointer = 0;
            Temp64 = Nv3GPMux->FileOffset;
            Temp = (NvU32)(Temp64 & 0x00000000FFFFFFFFULL);
            NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);

            // co64
            for(i = 0; i < CurrentPointer; i++)
            {
                Nv3GPMux->pPAudioBuffer[i + CO64_OFFSET +
                    ACTUAL_ATOM_ENTRIES + sizeof(NvU32) +
                    (pInpParam->AudioStcoIndex << 3)] = TempBuff[i];
            }
            CurrentPointer = 0;
            Temp = (NvU32)((Temp64 & 0xFFFFFFFF00000000ULL)>>32);
            NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);
            // co64
            for(i = 0; i < CurrentPointer; i++)
            {
                Nv3GPMux->pPAudioBuffer[i + CO64_OFFSET +
                    ACTUAL_ATOM_ENTRIES +
                    (pInpParam->AudioStcoIndex << 3)] = TempBuff[i];
            }
            Nv3GPMux->MuxCO64Box[SOUND_TRACK].ChunkCount ++;
            Nv3GPMux->MuxCO64Box[SOUND_TRACK].FullBox.Box.BoxSize+= sizeof(NvU64);
            Nv3GPMux->SpeechBoxesSize += 8;
            pInpParam->PrevAudioFileOffSet = (Nv3GPMux->FileOffset +
                BytesToRead);
            pInpParam->AudStscEntries =1;
        }
        else
        {
            pInpParam->PrevAudioFileOffSet += (NvU64)BytesToRead;
            pInpParam->AudStscEntries++;
        }

        //stts box
        if (Nv3GPMux->AudInitFlag == STTS_INIT_DONE)
        {
            // Update ulCurrAudioDelta depending on time stamps received.
            if (pInpParam->FrameDuration > 0)
                TSDiff = pInpParam->FrameDuration * (TIME_SCALE);
            else
                TSDiff = Nv3GPMux->CurrAudioTS - Nv3GPMux->PrevAudioTS;
            /* Find STTS sample delta in units media time scale */
            CurrAudioDelta = (NvU32)((TSDiff)*TIME_SCALE/TIMESTAMP_RESOLUTION);
            TSTruncated = (CurrAudioDelta) * (TIMESTAMP_RESOLUTION/TIME_SCALE);
            /* Calculate the error or fractional part of difference and accumulate the value */
            Nv3GPMux->AccumFracAudioTS += (NvS32)(TSDiff - TSTruncated);
            /* accumulated Fractional value is greate then 100 i.e ACC_FRAC_TRIGGER add 1 to audio delta */
            /* Update the fractional TS variable with pending fractional value that is above 100 */
            /* ACC_FRAC_TRIGGER = TIMESTAMP_RESOLUTION/TIME_SCALE */
            if(Nv3GPMux->AccumFracAudioTS >= ACC_FRAC_TRIGGER)
            {
                CurrAudioDelta += 1;
                Nv3GPMux->AccumFracAudioTS -= ACC_FRAC_TRIGGER;
            }
            Nv3GPMux->PrevAudioTS = Nv3GPMux->CurrAudioTS;
            //Double it , this is to take care of first entry
            if (!pInpParam->bFirstAudioEntry)
                Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration +=
                (CurrAudioDelta * 2);
            else
                Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration += CurrAudioDelta;

            if (pInpParam->MaxTrakDurationLimit >0)
            {
                if (NvMM_AV_AUDIO_VIDEO_BOTH ==
                    pInpParam->AudioVideoFlag &&
                    pInpParam->bVideoDurationLimit)
                {
                    if (Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration >=
                        (Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration))
                        pInpParam->bAudioDurationLimit = NV_TRUE;
                }
                else if (NvMM_AV_AUDIO_VIDEO_BOTH ==
                    pInpParam->AudioVideoFlag &&
                    (!pInpParam->bVideoDurationLimit))
                {
                    if ((Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration/10) >=
                        (pInpParam->MaxTrakDurationLimit))
                        pInpParam->bAudioDurationLimit = NV_TRUE;
                }
                else if (NvMM_AV_AUDIO_PRESENT == pInpParam->AudioVideoFlag &&
                    (NvU64)(Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration/10) >=
                    (pInpParam->MaxTrakDurationLimit))
                    pInpParam->bAudioDurationLimit = NV_TRUE;
            }

            if (pInpParam->PrevAudioDelta != CurrAudioDelta
                || 0 == pInpParam->AudioCount)
            {
                if (!pInpParam->bFirstAudioEntry)
                {
                    pInpParam->AudioSampleCount++;
                    pInpParam->bFirstAudioEntry = NV_TRUE;
                }
                else
                    pInpParam->AudioSampleCount =1;

                NvMM3GpMuxPrepareEntry(Nv3GPMux->pPAudioBuffer,
                    0 + ACTUAL_ATOM_ENTRIES,
                    pInpParam->AudioCount,
                    pInpParam->AudioSampleCount,
                    3);


                NvMM3GpMuxPrepareEntry(Nv3GPMux->pPAudioBuffer,
                    4+ ACTUAL_ATOM_ENTRIES,
                    pInpParam->AudioCount,
                    CurrAudioDelta,
                    3);

                Nv3GPMux->MuxSTTSBox[SOUND_TRACK].EntryCount ++;
                Nv3GPMux->MuxSTTSBox[SOUND_TRACK].FullBox.Box.BoxSize+=
                    sizeof(NvMM3GpMuxSTTSTable);
                Nv3GPMux->SpeechBoxesSize += sizeof(NvMM3GpMuxSTTSTable);
                pInpParam->AudioCount++;
                pInpParam->PrevAudioDelta = CurrAudioDelta;
                pInpParam->ASttsEntries++;
                *(NvU32 *)(Nv3GPMux->pPAudioBuffer + 4) = pInpParam->ASttsEntries;
            }
            else
            {
                pInpParam->AudioSampleCount++;
                NvMM3GpMuxPrepareEntry(Nv3GPMux->pPAudioBuffer,
                    0+ ACTUAL_ATOM_ENTRIES,
                    pInpParam->AudioCount -1,
                    pInpParam->AudioSampleCount,
                    3);
            }
        }
        else
        {
            Nv3GPMux->AudInitFlag = STTS_INIT_DONE;
            Nv3GPMux->PrevAudioTS = Nv3GPMux->CurrAudioTS;
            Nv3GPMux->FirstAudioTS = Nv3GPMux->CurrAudioTS;
            for(i = 0; i < 8; i++)
            {
               Nv3GPMux->pPAudioBuffer[i ] = 0;
            }
            pInpParam->AudioSampleCount++;
            pInpParam->AudioCount++;
        }
        //stsz box

        Temp = BytesToRead;
        CurrentPointer = 0;
        NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);

       // gp3GPPMuxStatus->audioCount << 2, the next value has to be written after 4 bytes
       // STSZ: pulSizeEntry
       for( i= 0; i < CurrentPointer; i++)
       {
           Nv3GPMux->pPAudioBuffer[i + STSZ_OFFSET +
               ACTUAL_ATOM_ENTRIES +
               (Nv3GPMux->AudioCount << 2)] = TempBuff[i];
       }
        Nv3GPMux->MuxSTSZBox[SOUND_TRACK].SampleCount ++;
        //Assuming a fixed rate stream - Will be changed during vEvaluateFinalParametrs()
        Nv3GPMux->MuxSTSZBox[SOUND_TRACK].SampleSize = 0;
        Nv3GPMux->MuxSTSZBox[SOUND_TRACK].FullBox.Box.BoxSize+= sizeof(NvU32);
        Nv3GPMux->SpeechFrameCnt++;
        Nv3GPMux->SpeechBoxesSize += 4;

        //update sample count and sample to chunk box
        if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
        {
            Temp = Nv3GPMux->MuxSTCOBox[SOUND_TRACK].ChunkCount;
        }
        else
        {
            Temp = Nv3GPMux->MuxCO64Box[SOUND_TRACK].ChunkCount;
        }

        if (1 == pInpParam->AudStscEntries)
        {
            //Chunk number
            NvMM3GpMuxPrepareEntry(Nv3GPMux->pPAudioBuffer,
                STSC_OFFSET + ACTUAL_ATOM_ENTRIES ,
                (pInpParam->AudioStcoIndex * 12),
                Temp,
                0);

            //Samples per Chunk
            NvMM3GpMuxPrepareEntry(Nv3GPMux->pPAudioBuffer,
                STSC_OFFSET + 4 + ACTUAL_ATOM_ENTRIES ,
                (pInpParam->AudioStcoIndex * 12),
                pInpParam->AudStscEntries,
                0);

           //Sample Desc
           NvMM3GpMuxPrepareEntry(Nv3GPMux->pPAudioBuffer,
               STSC_OFFSET + 8 + ACTUAL_ATOM_ENTRIES ,
               (pInpParam->AudioStcoIndex * 12),
               1,
               0);

            Nv3GPMux->MuxSTSCBox[SOUND_TRACK].EntryCount ++;
            Nv3GPMux->MuxSTSCBox[SOUND_TRACK].FullBox.Box.BoxSize+=
                sizeof(NvMM3GpMuxSTSCTable);
            Nv3GPMux->SpeechBoxesSize += 12;
            pInpParam->AudioStcoIndex++;
            pInpParam->AOffsetStscEntries++;
            *(NvU32 *)(Nv3GPMux->pPAudioBuffer) =
                pInpParam->AOffsetStscEntries;
        }
        else
        {
            //update Samples per Chunk in previous entry
            NvMM3GpMuxPrepareEntry(Nv3GPMux->pPAudioBuffer,
                STSC_OFFSET + 4 + ACTUAL_ATOM_ENTRIES ,
                ((pInpParam->AudioStcoIndex-1) * 12),
                pInpParam->AudStscEntries,
                0);
        }

        Nv3GPMux->AudioCount++;
        if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
            Nv3GPMux->FileOffset32 += BytesToRead;
        else
            Nv3GPMux->FileOffset += BytesToRead;
    }

cleanup:
    return Status;
}

static NvError
NvMM3GpMuxProcessSpeech(
    NvMM3GpMuxInputParam *pInpParam,
    NvU8 *pBufferOut,
    NvU32 *Offset,
    Nv3gpWriter *Nv3GPMux)
{
    NvError Status=NvSuccess;

    //calculate no. of speech frames in current chunk
    NVMM_CHK_ERR(NvMM3GpMuxCountNoSpeechSamples(pInpParam, Nv3GPMux));

    Nv3GPMux->Max_Samples_Per_Frame = 1;
    Nv3GPMux->PrevAudioTS = Nv3GPMux->CurrAudioTS;
    *Offset += Nv3GPMux->ConsumedDataSize;
    Nv3GPMux->FileSize += Nv3GPMux->ConsumedDataSize;
    //increment the length of mdat box
    Nv3GPMux->MuxMDATBox.Box.BoxSize+= Nv3GPMux->ConsumedDataSize;
cleanup:
    return Status;
}

static NvError
NvMM3GpMuxProcessSpeechLeftOver(
    NvMM3GpMuxInputParam *pInpParam,
    NvU8 *pBufferOut,
    NvU32 *Offset,
    Nv3gpWriter *Nv3GPMux)
{
    NvError Status=NvSuccess;
    NvU32 SpeechBuffDataSize = 0;

    if(Nv3GPMux->ConsumedDataSize < Nv3GPMux->SpeechBuffDataSize)
    {
        // Remember the value of Total size
        SpeechBuffDataSize = Nv3GPMux->SpeechBuffDataSize;

        //  Update ulSpeechBuffDataSize with left over size. So that in countnoofspeechsample this value
        // will be used for comparison
        Nv3GPMux->SpeechBuffDataSize -= Nv3GPMux->ConsumedDataSize;

        // reset conumed size to 0 as it get initialized in countnoofspeechsample
        Nv3GPMux->ConsumedDataSize = 0;

        // move the pointer to left speech frames so that conumption will start from the new location
        Nv3GPMux->pSpeech += Nv3GPMux->SpeechOffset;

        NVMM_CHK_ERR(NvMM3GpMuxProcessSpeech(pInpParam, pBufferOut, Offset, Nv3GPMux));

        // reset speech offset to 0.
        Nv3GPMux->SpeechOffset = 0;

        // Put back the actual ulspeechBuffDataSize
        Nv3GPMux->SpeechBuffDataSize = SpeechBuffDataSize;
    }
cleanup:
    return Status;
}

static void
NvMM3GpMuxProcessVideo(
    NvMM3GpMuxInputParam *pInpParam,
    NvU8 *pBufferOut,
    NvU32 *Offset,
    Nv3gpWriter *Nv3GPMux)
{
    NvU32 CurrVideoDelta = 0;
    NvU32 Temp = 0;
    NvU32 Val = 0;
    NvU64 Temp64 = 0;
    NvU64 Val64 = 0;
    NvU8 TempBuff[4];
    NvU8 i = 0;
    NvU64 TSDiff = 0;
    NvU64 TSTruncated = 0;
    NvU32 CurrentPointer = 0;

    if((Nv3GPMux->Mode == NvMM_Mode32BitAtom &&
        (pInpParam->PrevVideoFileOffSet != (NvU64)Nv3GPMux->FileOffset32))
        ||(Nv3GPMux->Mode == NvMM_Mode32BitAtom &&
        0 == pInpParam->VideoStcoIndex))
    {
        //update table entry count for chunk offset
        CurrentPointer = 0;
        Temp = Nv3GPMux->FileOffset32;
        NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);
        // to be written to a temp file 1
        for( i= 0; i < CurrentPointer; i++)
        {
            Nv3GPMux->pPVideoBuffer[i + CO64_OFFSET +
                ACTUAL_ATOM_ENTRIES +
                (pInpParam->VideoStcoIndex << 2)] = TempBuff[i];
        }
        Nv3GPMux->MuxSTCOBox[VIDEO_TRACK].ChunkCount ++;
        Nv3GPMux->MuxSTCOBox[VIDEO_TRACK].FullBox.Box.BoxSize+= sizeof(NvU32);
        Nv3GPMux->VideoBoxesSize += 4;
        pInpParam->PrevVideoFileOffSet = (NvU64)(Nv3GPMux->FileOffset32 +
            Nv3GPMux->VideoBuffDataSize);
        pInpParam->VidStscEntries =1;
    }
    else if ((Nv3GPMux->Mode == NvMM_Mode64BitAtom &&
        (pInpParam->PrevVideoFileOffSet != Nv3GPMux->FileOffset))
        ||(Nv3GPMux->Mode == NvMM_Mode64BitAtom &&
        0 == pInpParam->VideoStcoIndex))
    {
        //update table entry count for chunk offset
        Temp64 = Nv3GPMux->FileOffset;
        Temp = (NvU32)(Temp64 & 0x00000000FFFFFFFFULL);
        CurrentPointer = 0;
        NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);
        Val = NV_LE_TO_INT_32(&TempBuff[0]);

        Val64 = (NvU64)Val << 32;

        for(i = 0;i < CurrentPointer; i++)
        {
            Nv3GPMux->pPVideoBuffer[i + CO64_OFFSET + 4 +
                ACTUAL_ATOM_ENTRIES +
                (pInpParam->VideoStcoIndex << 3)] = TempBuff[i];
        }

        Temp = (NvU32)((Temp64 & 0xFFFFFFFF00000000ULL)>> 32);
        CurrentPointer = 0;
        NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);
        Val = NV_LE_TO_INT_32(&TempBuff[0]);

        Val64 |= (NvU64)Val;
        // to be written to a temp file 1
        for(i = 0; i < CurrentPointer; i++)
        {
            Nv3GPMux->pPVideoBuffer[i + CO64_OFFSET +
                ACTUAL_ATOM_ENTRIES +
                (pInpParam->VideoStcoIndex << 3)] = TempBuff[i];
        }

        Nv3GPMux->MuxCO64Box[VIDEO_TRACK].ChunkCount ++;
        Nv3GPMux->MuxCO64Box[VIDEO_TRACK].FullBox.Box.BoxSize+= sizeof(NvU64);
        Nv3GPMux->VideoBoxesSize += 8;
        pInpParam->PrevVideoFileOffSet = Nv3GPMux->FileOffset +
            Nv3GPMux->VideoBuffDataSize;
        pInpParam->VidStscEntries = 1;
    }
    else
    {
        pInpParam->PrevVideoFileOffSet += (NvU64)Nv3GPMux->VideoBuffDataSize;
        pInpParam->VidStscEntries++;
    }

    if (Nv3GPMux->VidInitFlag == STTS_INIT_DONE)
    {
        //Timestamp changes

        /* Calculate the difference between current TS and previous TS */
        TSDiff = (Nv3GPMux->CurrVideoTS - Nv3GPMux->PrevVideoTS);

        /* Find STTS sample delta in units media time scale */
        CurrVideoDelta = (NvU32)((TSDiff)*TIME_SCALE/TIMESTAMP_RESOLUTION);

        TSTruncated = (CurrVideoDelta) * (TIMESTAMP_RESOLUTION/TIME_SCALE);

        /* Calculate the error or fractional part of difference and accumulate the value */
        Nv3GPMux->AccumFracTS += (NvS32)(TSDiff - TSTruncated);

        /* accumulated Fractional value is greate then 1000 i.e ACC_FRAC_TRIGGER add 1 to video delta */
        /* Update the fractional TS variable with pending fractional value that is above 100 */
        /* ACC_FRAC_TRIGGER = TIMESTAMP_RESOLUTION/TIME_SCALE */
        if(Nv3GPMux->AccumFracTS >= ACC_FRAC_TRIGGER)
        {
            CurrVideoDelta += 1;
            Nv3GPMux->AccumFracTS -= ACC_FRAC_TRIGGER;
        }

        Nv3GPMux->PrevVideoTS = Nv3GPMux->CurrVideoTS;

        if (!pInpParam->bFirstVideoEntry)
            Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration += (CurrVideoDelta *2);
        else
            Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration += CurrVideoDelta;

        //NV3GPDebug(("Duration = %d\n ", gp3GPPMuxStatus->structmdhdBox[VIDEO_TRACK].ulDuration ));

        if (pInpParam->MaxTrakDurationLimit >0)
        {
            if (NvMM_AV_AUDIO_VIDEO_BOTH ==
                pInpParam->AudioVideoFlag &&
                pInpParam->bAudioDurationLimit)
            {
                if (Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration >=
                    (Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration))
                    pInpParam->bVideoDurationLimit = NV_TRUE;
            }
            else if (NvMM_AV_AUDIO_VIDEO_BOTH ==
                pInpParam->AudioVideoFlag &&
                (!pInpParam->bAudioDurationLimit))
            {
                if ((Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration/10) >=
                    (pInpParam->MaxTrakDurationLimit))
                    pInpParam->bVideoDurationLimit = NV_TRUE;
            }
            else if (NvMM_AV_VIDEO_PRESENT == pInpParam->AudioVideoFlag &&
                (Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration/10) >=
                (pInpParam->MaxTrakDurationLimit))
                pInpParam->bVideoDurationLimit = NV_TRUE;
        }

        if (pInpParam->MaxTrakDurationLimit >0)
        {
            if (NvMM_AV_AUDIO_VIDEO_BOTH ==
                pInpParam->AudioVideoFlag &&
                pInpParam->bAudioDurationLimit)
            {
                if (Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration >=
                    (Nv3GPMux->MuxMDHDBox[SOUND_TRACK].Duration))
                    pInpParam->bVideoDurationLimit = NV_TRUE;
            }
            else if (NvMM_AV_AUDIO_VIDEO_BOTH ==
                pInpParam->AudioVideoFlag &&
                (!pInpParam->bAudioDurationLimit))
            {
                if ((Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration/10) >=
                    (pInpParam->MaxTrakDurationLimit))
                    pInpParam->bVideoDurationLimit = NV_TRUE;
            }
            else if (NvMM_AV_VIDEO_PRESENT == pInpParam->AudioVideoFlag &&
                (Nv3GPMux->MuxMDHDBox[VIDEO_TRACK].Duration/10) >=
                (pInpParam->MaxTrakDurationLimit))
                pInpParam->bVideoDurationLimit = NV_TRUE;
        }

        // to be written to a temp file 1
        if (pInpParam->PrevVideoDelta != CurrVideoDelta
            || 0 == pInpParam->VideoCount)
        {
            if (!pInpParam->bFirstVideoEntry)
            {
                pInpParam->VideoSampleCount++;
                pInpParam->bFirstVideoEntry = NV_TRUE;
             }
            else
                pInpParam->VideoSampleCount = 1;

            NvMM3GpMuxPrepareEntry(Nv3GPMux->pPVideoBuffer,
                0+ ACTUAL_ATOM_ENTRIES ,
                pInpParam->VideoCount,
                pInpParam->VideoSampleCount,
                3);


            NvMM3GpMuxPrepareEntry(Nv3GPMux->pPVideoBuffer,
                4+ ACTUAL_ATOM_ENTRIES ,
                pInpParam->VideoCount,
                CurrVideoDelta,
                3);

            Nv3GPMux->MuxSTTSBox[VIDEO_TRACK].EntryCount ++;
            Nv3GPMux->MuxSTTSBox[VIDEO_TRACK].FullBox.Box.BoxSize+=
                sizeof(NvMM3GpMuxSTTSTable);
            Nv3GPMux->VideoBoxesSize += 8;
            pInpParam->VideoCount++;
            pInpParam->PrevVideoDelta = CurrVideoDelta;
            pInpParam->VSttsEntries++;
            *(NvU32 *)(Nv3GPMux->pPVideoBuffer + 4) = pInpParam->VSttsEntries;
        }
        else
        {
            pInpParam->VideoSampleCount++;
            NvMM3GpMuxPrepareEntry(Nv3GPMux->pPVideoBuffer,
                0+ ACTUAL_ATOM_ENTRIES ,
                pInpParam->VideoCount -1,
                pInpParam->VideoSampleCount,
                3);
        }
    }
    else
    {
        Nv3GPMux->VidInitFlag = STTS_INIT_DONE;
        Nv3GPMux->PrevVideoTS = Nv3GPMux->CurrVideoTS;
        Nv3GPMux->FirstVideoTS = Nv3GPMux->CurrVideoTS;

        for(i=0;i<8;i++)
        {
            Nv3GPMux->pPVideoBuffer[i ] = 0;
        }
        pInpParam->VideoSampleCount++;
        pInpParam->VideoCount++;
    }

    //update table entry count for sample size
    Temp = Nv3GPMux->VideoBuffDataSize;
    CurrentPointer = 0;
    NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);
    Val = NV_LE_TO_INT_32(&TempBuff[0]);

    // to be written to a temp file 1 - STSZ
    for(i = 0; i < CurrentPointer; i++)
    {
        Nv3GPMux->pPVideoBuffer[i + STSZ_OFFSET +
            ACTUAL_ATOM_ENTRIES +
            (Nv3GPMux->VideoCount << 2)] = TempBuff[i];
    }

    Nv3GPMux->MuxSTSZBox[VIDEO_TRACK].SampleCount ++;
    Nv3GPMux->MuxSTSZBox[VIDEO_TRACK].FullBox.Box.BoxSize+= sizeof(NvU32);
    Nv3GPMux->VideoBoxesSize += 4;

    //update sample count and sample to chunk box
    if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
    {
        Temp = Nv3GPMux->MuxSTCOBox[VIDEO_TRACK].ChunkCount;
    }
    else
    {
        Temp = Nv3GPMux->MuxCO64Box[VIDEO_TRACK].ChunkCount;
    }

    if (1 == pInpParam->VidStscEntries)
    {
        //Chunk number
        NvMM3GpMuxPrepareEntry(Nv3GPMux->pPVideoBuffer,
            STSC_OFFSET+ ACTUAL_ATOM_ENTRIES ,
            (pInpParam->VideoStcoIndex * 12),
            Temp,
            0);

        //Samples per Chunk
        NvMM3GpMuxPrepareEntry(Nv3GPMux->pPVideoBuffer,
            STSC_OFFSET + 4+ ACTUAL_ATOM_ENTRIES ,
            (pInpParam->VideoStcoIndex * 12),
            pInpParam->VidStscEntries,
            0);

        //Sample Desc
        NvMM3GpMuxPrepareEntry(Nv3GPMux->pPVideoBuffer,
            STSC_OFFSET + 8+ ACTUAL_ATOM_ENTRIES ,
            (pInpParam->VideoStcoIndex * 12),
            1,
            0);

        Nv3GPMux->MuxSTSCBox[VIDEO_TRACK].EntryCount ++;
        Nv3GPMux->MuxSTSCBox[VIDEO_TRACK].FullBox.Box.BoxSize+=
            sizeof(NvMM3GpMuxSTSCTable);
        Nv3GPMux->VideoBoxesSize += 12;
        pInpParam->VideoStcoIndex++;
        pInpParam->VOffsetStscEntries++;
        *(NvU32 *)(Nv3GPMux->pPVideoBuffer) =
            pInpParam->VOffsetStscEntries;
    }
    else
    {
        //update Samples per Chunk in previous entry
        NvMM3GpMuxPrepareEntry(Nv3GPMux->pPVideoBuffer,
            STSC_OFFSET + 4 + ACTUAL_ATOM_ENTRIES ,
            ((pInpParam->VideoStcoIndex-1) * 12),
            pInpParam->VidStscEntries,
            0);
    }

    if(Nv3GPMux->KeyFrame)
    {
        Temp = Nv3GPMux->VideoFrameCnt+1;
        CurrentPointer = 0;
        NvMM3GpMuxConvertToBigEndian( &TempBuff[0], &CurrentPointer, Temp);
        Val = NV_LE_TO_INT_32(&TempBuff[0]);

        // to be written to a temp file 3 - only for sample number STSS
        for( i= 0; i < CurrentPointer; i++)
        {
           Nv3GPMux->pPSyncBuffer[i + (Nv3GPMux->SyncCount <<2)] = TempBuff[i];
        }

        Nv3GPMux->MuxSTSSBox[VIDEO_TRACK].EntryCount ++;
        Nv3GPMux->MuxSTSSBox[VIDEO_TRACK].FullBox.Box.BoxSize+= sizeof(NvU32);
        Nv3GPMux->VideoBoxesSize += 4;
        Nv3GPMux->SyncCount++;
    }

    //this is not required, we have the data already, write it only for 1st video frame
    if(!Nv3GPMux->VideoFrameCnt)
    {
        NvOsMemcpy(pBufferOut + *Offset,Nv3GPMux->pVideo, Nv3GPMux->VideoBuffDataSize);
     }

    *Offset += Nv3GPMux->VideoBuffDataSize;
    Nv3GPMux->VideoCount++;

    //increment the file pointer offset
    if(Nv3GPMux->Mode == NvMM_Mode32BitAtom)
    {
        Nv3GPMux->FileOffset32 += Nv3GPMux->VideoBuffDataSize;
    }
    else
    {
        Nv3GPMux->FileOffset += Nv3GPMux->VideoBuffDataSize;
    }

    Nv3GPMux->FileSize += Nv3GPMux->VideoBuffDataSize;

    //increment the length of mdat box
    Nv3GPMux->MuxMDATBox.Box.BoxSize+= Nv3GPMux->VideoBuffDataSize;
    Nv3GPMux->VideoFrameCnt++;
}

NvError
NvMM3GpMuxRecWriteRun(
    NvMM3GpMuxSplitMode *pSpMode,
    NvMM3GpAVParam *pSVParam,
    NvMM3GpMuxInputParam *pInpParam,
    NvU8 *pBufferOut,
    NvU32 *Offset,
    NvMM3GpMuxContext Context3GpMux)
{
    Nv3gpWriter *Nv3GPMux = NULL;
    NvMMFileAVSWriteMsg Message;
    NvError Status = NvSuccess;

    NVMM_CHK_ARG(pSpMode && pSVParam && pInpParam && Offset);
    NvOsMemset(&Message, 0, sizeof(NvMMFileAVSWriteMsg));
    Nv3GPMux = (Nv3gpWriter *)Context3GpMux.pIntMem;
    Nv3GPMux->pSpeech = pSVParam->pSpeech;
    Nv3GPMux->pVideo = pSVParam->pVideo;
    Nv3GPMux->SpeechBuffDataSize = pSVParam->SpeechDataSize;
    Nv3GPMux->VideoBuffDataSize = pSVParam->VideoDataSize;
    Nv3GPMux->FVideoSyncStatus = pSVParam->FVideoSyncStatus;
    Nv3GPMux->CurrVideoTS = pSVParam->VideoTS;
    Nv3GPMux->CurrAudioTS = pSVParam->AudioTS;

    NVMM_CHK_ARG(!Nv3GPMux->ResvFileWriteFailed);
    NVMM_CHK_ARG(!Nv3GPMux->VideoWriteFailed);
    NVMM_CHK_ARG(!Nv3GPMux->AudioWriteFailed);

    if(pInpParam->ZeroSpeechDataFlag)
    {
        NVMM_CHK_ERR(NvMM3GpMuxProcessSpeech(pInpParam, pBufferOut, Offset, Nv3GPMux));
        Nv3GPMux->FAudioDataToBeDumped = 1;
        if(TOTAL_ENTRIESPERATOM == Nv3GPMux->AudioCount)
        {
            if((NvMM_AV_AUDIO_VIDEO_BOTH == pInpParam->AudioVideoFlag) || (NvMM_AV_AUDIO_PRESENT == pInpParam->AudioVideoFlag ) )
                Nv3GPMux->ResvFlag = NV_TRUE;
             Message.pData = Nv3GPMux->pPAudioBuffer ;
             Message.DataSize = (TOTAL_ENTRIESPERATOM * SIZEOFATOMS) +
                ACTUAL_ATOM_ENTRIES;
             Message.ThreadShutDown = NV_FALSE;
             NVMM_CHK_ERR(NvMMQueueEnQ(Nv3GPMux->MsgQueueAudio, &Message, 0));

             if(Nv3GPMux->AudioWriteSemaInitialized)
                 NvOsSemaphoreWait(Nv3GPMux->AudioWriteDoneSema);
             else
                Nv3GPMux->AudioWriteSemaInitialized = NV_TRUE;
             NvOsSemaphoreSignal(Nv3GPMux->AudioWriteSema);
             Nv3GPMux->AudioCount =0;
             pInpParam->AudioStcoIndex = 0;
             pInpParam->AOffsetStscEntries = 0;
             pInpParam->AudioCount = 0;
             pInpParam->ASttsEntries = 0;
             pInpParam->PrevAudioFileOffSet =0;
             if(Nv3GPMux->pPAudioBuffer == Nv3GPMux->PingAudioBuffer)
                 Nv3GPMux->pPAudioBuffer = Nv3GPMux->PongAudioBuffer;
             else
                 Nv3GPMux->pPAudioBuffer = Nv3GPMux->PingAudioBuffer;
             NvMM3GpMuxProcessSpeechLeftOver(pInpParam, pBufferOut, Offset, Nv3GPMux);
        }
    }

    if(pInpParam->ZeroVideoDataFlag)
    {
         if(!Nv3GPMux->VideoFrameCnt)
         {
             if(pInpParam->Mp4H263Flag == 0)
                 NvMM3GpMuxProcessFirstVideoFrame(Nv3GPMux);
             // It comes only for the first frame with current sw-implementaion
              if(pInpParam->Mp4H263Flag == 2)
              {
                  NVMM_CHK_ERR(NvMM3GpMuxVProcessNALUnits(pSpMode,Nv3GPMux));
             }
         }
         if(Nv3GPMux->VideoBuffDataSize>4)
         {
             Nv3GPMux->KeyFrame = pInpParam->IsSyncFrame;
             NvMM3GpMuxProcessVideo(pInpParam,pBufferOut, Offset, Nv3GPMux);
             if(Nv3GPMux->VideoCount == TOTAL_ENTRIESPERATOM)
             {
                 if(pInpParam->AudioVideoFlag == NvMM_AV_VIDEO_PRESENT)
                     Nv3GPMux->ResvFlag = NV_TRUE;
                 Message.pData = Nv3GPMux->pPVideoBuffer ;
                 Message.DataSize = (TOTAL_ENTRIESPERATOM * SIZEOFATOMS) +
                    ACTUAL_ATOM_ENTRIES;
                 Message.ThreadShutDown = NV_FALSE;
                 NVMM_CHK_ERR(NvMMQueueEnQ(Nv3GPMux->MsgQueueVideo, &Message, 0));

                 if(Nv3GPMux->VideoWriteSemaInitialized)
                     NvOsSemaphoreWait(Nv3GPMux->VideoWriteDoneSema);
                 else
                     Nv3GPMux->VideoWriteSemaInitialized = NV_TRUE;
                 NvOsSemaphoreSignal(Nv3GPMux->VideoWriteSema);
                 Nv3GPMux->VideoCount =0;
                 pInpParam->VideoStcoIndex = 0;
                 pInpParam->VOffsetStscEntries = 0;
                 pInpParam->VideoCount = 0;
                 pInpParam->VSttsEntries = 0;
                 pInpParam->PrevVideoFileOffSet =0;
                 if(Nv3GPMux->pPVideoBuffer == Nv3GPMux->PingVideoBuffer)
                     Nv3GPMux->pPVideoBuffer = Nv3GPMux->PongVideoBuffer;
                 else
                     Nv3GPMux->pPVideoBuffer = Nv3GPMux->PingVideoBuffer;
             }
             if(Nv3GPMux->SyncCount == ((TOTAL_ENTRIESPERATOM>>2) ))
             {
                 // 250 sync = 100 bytes
                 NVMM_CHK_ERR(NvOsFwrite( Nv3GPMux->FileSyncHandle, (CPbyte *)(Nv3GPMux->pPSyncBuffer), TOTAL_ENTRIESPERATOM ));
                 Nv3GPMux->SyncCount =0;
             }
         }
         Nv3GPMux->FVideoDataToBeDumped = 1;
    }

    if(Nv3GPMux->ResvFlag == NV_TRUE)
    {
        // Disable the flag, this flag will be set either audio only or video only or audio + video case
        Nv3GPMux->ResvFlag = NV_FALSE;
         // Wait for previous write operation to be completed
         NvOsSemaphoreWait(Nv3GPMux->ResvWriteDoneSema);
         // Free reserve buffer which of of 256KB (MIN_INITIAL_RESV_SIZE) size
         if(Nv3GPMux->pPResvBuffer)
         {
             NvOsFree(Nv3GPMux->pPResvBuffer);
             Nv3GPMux->pPResvBuffer= NULL;
         }
         Message.pData = Nv3GPMux->pResvRunBuffer;
         // *2 to cover both audio + video size
         Message.DataSize = TOTAL_ENTRIESPERATOM * SIZEOFATOMS * 2;
         Message.LoopCntr = 1;
         Message.ThreadShutDown = NV_FALSE;
         NVMM_CHK_ERR(NvMMQueueEnQ(Nv3GPMux->MsgQueueResv, &Message, 0));
         NvOsSemaphoreSignal(Nv3GPMux->ResvWriteSema);
    }

cleanup:
    if(NV_TRUE == Nv3GPMux->ResvFileWriteFailed)
        Status = Nv3GPMux->ResvThreadStatus;
    if(NV_TRUE == Nv3GPMux->VideoWriteFailed)
        Status = Nv3GPMux->VideoWriteStatus;
    if(NV_TRUE == Nv3GPMux->AudioWriteFailed)
        Status = Nv3GPMux->AudioWriteStatus;
    return Status;
}

static NvError
NvMM3GpMuxWriteMoovAtom(
    Nv3gpWriter * Nv3GPMux,
    CP_PIPETYPE  *pPipe,
    CPhandle Cphandle)
{
    NvError Status = NvSuccess;
    NvU8 *Temp = NULL;
    NvU32 CurrentPointer = 0;

    Temp = Nv3GPMux->pTempBuff;
    NvOsMemset(Temp, 0, TEMP_INPUT_BUFF_SIZE);
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxMOOVBox.Box.BoxSize,Temp,&CurrentPointer, 'm', 'o', 'o', 'v');
    NvMM3GpMuxCopyAtom(Nv3GPMux->MuxMVHDBox.FullBox.Box.BoxSize,Temp,&CurrentPointer, 'm', 'v', 'h', 'd');
    //reserved
    CurrentPointer += sizeof(NvU32);
    NvMM3GpMuxConvertToBigEndian( Temp, &CurrentPointer, Nv3GPMux->MuxMVHDBox.CreationTime);
    NvMM3GpMuxConvertToBigEndian( Temp, &CurrentPointer, Nv3GPMux->MuxMVHDBox.ModTime);
    NvMM3GpMuxConvertToBigEndian( Temp, &CurrentPointer, Nv3GPMux->MuxMVHDBox.TimeScale);
    NvMM3GpMuxConvertToBigEndian( Temp, &CurrentPointer, Nv3GPMux->MuxMVHDBox.Duration);
    COPY_FOUR_BYTES((Temp + CurrentPointer), 0, 1, 0, 0);
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((Temp + CurrentPointer), 1, 0, 0, 0);
    CurrentPointer += sizeof(NvU32);
    //skip
    CurrentPointer += sizeof(NvU32);
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((Temp + CurrentPointer), 0, 1, 0, 0);
    CurrentPointer += sizeof(NvU32);
    //skip
    CurrentPointer += sizeof(NvU32);
    CurrentPointer += sizeof(NvU32);
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((Temp + CurrentPointer), 0, 1, 0, 0);
    CurrentPointer += sizeof(NvU32);
    //skip
    CurrentPointer += sizeof(NvU32);
    CurrentPointer += sizeof(NvU32);
    COPY_FOUR_BYTES((Temp + CurrentPointer), 0, 0, 0, 0);
    CurrentPointer += sizeof(NvU32);
    Temp[CurrentPointer++] = (NvU8)0x40;
    //skip
    CurrentPointer += sizeof(NvU32) * 7;
    CurrentPointer += sizeof(NvU8) * 2;
    Temp[CurrentPointer++] = (NvU8)NEXT_TRACK_ID;

    NVMM_CHK_ERR((NvError)pPipe->Write(Cphandle, (CPbyte *)Temp, CurrentPointer));

cleanup:
    return Status;
}

NvError
NvMM3GpMuxRecWriteClose(
    NvMM3GpMuxInputParam *pInpParam,
    CPhandle cphandle,
    CP_PIPETYPE *pPipe,
    NvMM3GpMuxContext Context3GpMux)
{
    NvError Status = NvSuccess;
    Nv3gpWriter *Nv3GPMux = NULL;
    NvU32 TempData;
    NvU8 *pTemp;
    NvU32 CurrentPointer = 0;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "++Rec_3gp2WriteClose \n"));

    NVMM_CHK_ARG(pInpParam && pPipe);

    //allocate memory for global library data structure
    Nv3GPMux = (Nv3gpWriter *)Context3GpMux.pIntMem;
   NVMM_CHK_ARG(Nv3GPMux);
    NVMM_CHK_ERR( NvMM3GpMuxWritePendingAVSMoovEntry(Nv3GPMux));
    NVMM_CHK_ERR( calcMaxMoovEntriesInBytes(pInpParam, Nv3GPMux));

    NvMM3GpMuxEvaluateFinalParametrs(pInpParam, Nv3GPMux);
    NVMM_CHK_ERR(NvMM3GpMuxWriteMoovAtom(Nv3GPMux, pPipe, cphandle));

    if(pInpParam->Mp4H263Flag == 2)
    {
         //Write final speech trak data
         if(pInpParam->AudioVideoFlag == NvMM_AV_AUDIO_PRESENT || pInpParam->AudioVideoFlag == NvMM_AV_AUDIO_VIDEO_BOTH)
         {
             if(Nv3GPMux->FAudioDataToBeDumped)
             {
                 NVMM_CHK_ERR(NvMM3GpMuxWriteTRAKAtomForAV(pInpParam, Nv3GPMux, cphandle, pPipe,SOUND_TRACK,2));
             }
             Nv3GPMux->FAudioDataToBeDumped = 0;
         }
         if(pInpParam->AudioVideoFlag == NvMM_AV_VIDEO_PRESENT || pInpParam->AudioVideoFlag == NvMM_AV_AUDIO_VIDEO_BOTH)
         {
             //write final video trak data
             if(Nv3GPMux->FVideoDataToBeDumped)
             {
                 NVMM_CHK_ERR(NvMM3GpMuxWriteTRAKAtomForAV(pInpParam, Nv3GPMux, cphandle, pPipe,VIDEO_TRACK,2));
             }
             Nv3GPMux->FVideoDataToBeDumped = 0;
         }
          pTemp = Nv3GPMux->pTempBuff;
          NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
          Nv3GPMux->SMoov[Nv3GPMux->MoovCount].SeekPos = 36;
          TempData = Nv3GPMux->MuxMDATBox.Box.BoxSize;
          Nv3GPMux->SMoov[Nv3GPMux->MoovCount].Data = TempData;
          Nv3GPMux->MoovCount++;
          NVMM_CHK_ERR(NvMM3GpMuxwriteUserData( Nv3GPMux, pPipe, cphandle, pInpParam));
    }
    else
    {
        //write final speech trak data
        if(pInpParam->ZeroSpeechDataFlag)
        {
           NVMM_CHK_ERR(NvMM3GpMuxWriteTRAKAtomForAV(pInpParam, Nv3GPMux, cphandle, pPipe,SOUND_TRACK,0));
        }
       //write final video trak data
       if(pInpParam->ZeroVideoDataFlag)
       {
           if(pInpParam->Mp4H263Flag == 0)
           {
               NVMM_CHK_ERR(NvMM3GpMuxWriteTRAKAtomForAV(pInpParam, Nv3GPMux, cphandle, pPipe,VIDEO_TRACK,0));
            }
           else
           {
               NVMM_CHK_ERR(NvMM3GpMuxWriteTRAKAtomForAV(pInpParam, Nv3GPMux, cphandle, pPipe,VIDEO_TRACK,1));
           }
       }
       NVMM_CHK_ERR(NvMM3GpMuxwriteUserData( Nv3GPMux, pPipe, cphandle, pInpParam));
    }

    pTemp = Nv3GPMux->pTempBuff;
    CurrentPointer= 36;
    NVMM_CHK_ERR((NvError)pPipe->SetPosition( cphandle, (CPint)CurrentPointer, CP_OriginBegin));

    NvOsMemset(pTemp, 0, TEMP_INPUT_BUFF_SIZE);
    CurrentPointer = 0;
    NvMM3GpMuxConvertToBigEndian( pTemp, &CurrentPointer, Nv3GPMux->MuxMDATBox.Box.BoxSize);
    NVMM_CHK_ERR((NvError)pPipe->Write(cphandle, (CPbyte *)pTemp, CurrentPointer));

    (void)NvMM3GpMuxWriterDeallocate(Nv3GPMux,pInpParam, &Context3GpMux);

cleanup:
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_DEBUG, "--Rec_3gp2WriteClose \n"));
    return Status;
}
