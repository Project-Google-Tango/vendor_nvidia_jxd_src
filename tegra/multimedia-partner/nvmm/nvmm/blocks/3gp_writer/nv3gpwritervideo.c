/*
* Copyright (c) 2007 NVIDIA Corporation. All rights reserved.
* 
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software and related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#include "nv_3gp_writer.h"
#include "nvos.h"
#include "nvmm_logger.h"

static void NvmmVideoFileWriteThread(void *arg)
{
    Nv3gpWriter *pgp3GPPMuxStatus = (Nv3gpWriter *)arg;
    NvMMFileAVSWriteMsg pMessage; 
    NvBool bShutDown = NV_FALSE;
    NvError status = NvSuccess;    
    NvU64 CurrentWritePosition = 0;

    while (bShutDown == NV_FALSE)
    {
        NvOsSemaphoreWait(pgp3GPPMuxStatus->VideoWriteSema);

        
        status = NvMMQueueDeQ(pgp3GPPMuxStatus->MsgQueueVideo, &pMessage);
        if(status != NvSuccess)
        {
            continue;
        }
        else
        {

            if( pMessage.ThreadShutDown)
            {
                bShutDown = NV_TRUE;

                NvOsSemaphoreSignal(pgp3GPPMuxStatus->VideoWriteDoneSema);
                break;
             }
            else
            {
                status = NvOsFwrite(pgp3GPPMuxStatus->FileVideoHandle, 
                    (CPbyte *)(pMessage.pData), pMessage.DataSize);
                if(status != NvSuccess)
                {
                    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_ERROR, "--NvmmVideoFileWriteThread::Write failed:bail out...\n"));
                    // Write failed
                    pgp3GPPMuxStatus->VideoWriteFailed = NV_TRUE;
                    pgp3GPPMuxStatus->VideoWriteStatus = status;
                    // Delete the temporary file to create room for writing pending mdat info
                    status = NvMM3GpMuxRecDelReservedFile(*pgp3GPPMuxStatus->Context3GpMux);
                    if(NvSuccess == status)
                    {
                        status = NvOsFseek(pgp3GPPMuxStatus->FileVideoHandle, CurrentWritePosition, NvOsSeek_Set);
                        if(NvSuccess == status)
                        {
                            //Once again try to write the data if it fails, it fails
                            status = NvOsFwrite(pgp3GPPMuxStatus->FileVideoHandle, (CPbyte *)(pMessage.pData), pMessage.DataSize);
                            if(status != NvSuccess)
                                NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmVideoFileWriteThread:: File write failed second time:recorded file is unusable\n"));
                        }
                        else
                            NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmVideoFileWriteThread::Seek failed recorded file is unusable\n"));
                    }
                    else
                        NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmVideoFileWriteThread::Deleting reserved partition failed\n"));
                }
                CurrentWritePosition += pMessage.DataSize;
                NvOsSemaphoreSignal(pgp3GPPMuxStatus->VideoWriteDoneSema);
             }
         }
    }
    


}

NvError VideoInit(Nv3gpWriter *p3gpmux, NvU32 length1, NvU32 length2, NvMM3GpMuxInputParam *pInpParam)
{
    NvError status=NvSuccess;
    char *szURI;
    NvU32 flags;    
    
    p3gpmux->URIConcatVideo[0] = 
        (char *)NvOsAlloc(sizeof(char) * (length1 + length2 + 1));
    
    if(p3gpmux->URIConcatVideo[0] == NULL)
    {
        status = NvError_InsufficientMemory;        
        return status;
    }

    p3gpmux->URIConcatSync[0] = 
        (char *)NvOsAlloc(sizeof(char) * (length1 + length2 + 1));
    
    if(p3gpmux->URIConcatSync[0] == NULL)
    {
        status = NvError_InsufficientMemory;        
        return status;
    }    
    
    status = NvOsSemaphoreCreate(&p3gpmux->VideoWriteSema, 0) ;
    if ( status != NvSuccess)
    {
        return status;
    }

    status = NvOsSemaphoreCreate(&p3gpmux->VideoWriteDoneSema, 0);
    if ( status != NvSuccess)
    {
        return status;
    }

    status = NvMMQueueCreate( &p3gpmux->MsgQueueVideo, MAX_QUEUE_LEN, 
                                              sizeof(NvMMFileAVSWriteMsg), NV_TRUE);
    if(status != NvSuccess)
    {
        return status;
    }    

    status = NvOsThreadCreate( NvmmVideoFileWriteThread, (void *)(p3gpmux), 
                                               &(p3gpmux->FileWriterVideoThread));
    if(status != NvSuccess)
    {
        return status;
    }    

    /*
    * Absolute memory allocation
    * (sizeof(telstTable) * pInpParam->maxVideoFrames);
    * In actual, only a table with 2 entries is required
    */
    if(pInpParam->ZeroVideoDataFlag == 1)
    {
        p3gpmux->MuxELSTBox[VIDEO_TRACK].pElstTable = NvOsAlloc((sizeof(NvMM3GpMuxELSTTable) * 2));
        if(p3gpmux->MuxELSTBox[VIDEO_TRACK].pElstTable  == NULL)
        {
            status = NvError_InsufficientMemory;                        
            return status;
        }        

        flags =    NVOS_OPEN_CREATE| NVOS_OPEN_WRITE;
        szURI = "videM.dat";
        
        NvMM3GpMuxWriterStrcat(p3gpmux->URIConcatVideo, pInpParam->pFilePath,length1, szURI,length2);            

            
        status = NvOsFopen((const char *)p3gpmux->URIConcatVideo[0], (NvU32)flags, &p3gpmux->FileVideoHandle);
        if (status != NvSuccess)
        {
            return status;
        }          

        //////////////// ping and pong buffer to be alloc
        // sizeof(STTS Table) + sizeof (STSZ) + sizeof(STSC) + sizeof(CO64) = 32 Bytes
        p3gpmux->PingVideoBuffer = NvOsAlloc((TOTAL_ENTRIESPERATOM *
            SIZEOFATOMS * sizeof(NvU8)) + ACTUAL_ATOM_ENTRIES);
        if(p3gpmux->PingVideoBuffer  == NULL)
        {
            status = NvError_InsufficientMemory;                        
            return status;
        }

        // sizeof(STTS Table) + sizeof (STSZ) + sizeof(STSC) + sizeof(CO64) = 32 Bytes
        p3gpmux->PongVideoBuffer = NvOsAlloc((TOTAL_ENTRIESPERATOM *
            SIZEOFATOMS * sizeof(NvU8)) + ACTUAL_ATOM_ENTRIES);
        if(p3gpmux->PongVideoBuffer  == NULL)
        {
            status = NvError_InsufficientMemory;                            
            return status;
        }                        

        p3gpmux->pPVideoBuffer = p3gpmux->PingVideoBuffer;


        // STSS  
        flags =    NVOS_OPEN_CREATE| NVOS_OPEN_WRITE;
        szURI = "syncM.dat";
        
        NvMM3GpMuxWriterStrcat(p3gpmux->URIConcatSync, pInpParam->pFilePath,length1, szURI,length2);            

        
        status = NvOsFopen((const char *)p3gpmux->URIConcatSync[0], (NvU32)flags, &p3gpmux->FileSyncHandle);
        if (status != NvSuccess)
        {
            return status;
        }       

        //////////////// ping and pong buffer to be alloc
        // sizeof(STSS Table) = 4 Bytes
        p3gpmux->PingSyncBuffer = NvOsAlloc(TOTAL_ENTRIESPERATOM * 4 * sizeof(NvU8));
        if(p3gpmux->PingSyncBuffer  == NULL)
        {
            status = NvError_InsufficientMemory;                        
            return status;
        }

        // sizeof(STSS Table) = 4 Bytes
        p3gpmux->PongSyncBuffer = NvOsAlloc(TOTAL_ENTRIESPERATOM * 4 * sizeof(NvU8));
        if(p3gpmux->PongSyncBuffer  == NULL)
        {
            status = NvError_InsufficientMemory;                        
            return status;
        }          

        p3gpmux->pPSyncBuffer = p3gpmux->PingSyncBuffer;    

        //allocate memory for sequence parameter set NAL Units for AVC
        p3gpmux->MuxAVCCBox.pPspsTable = NvOsAlloc(sizeof(NvMM3GpMuxTSPSTable) * pInpParam->MaxVideoFrames/10);
        if(p3gpmux->MuxAVCCBox.pPspsTable   == NULL)
        {
            status = NvError_InsufficientMemory;                        
            return status;
        }             

        //allocate memory for picture parameter set NAL Units for AVC
        p3gpmux->MuxAVCCBox.pPpsTable =  NvOsAlloc(sizeof(NvMM3GpMuxPPSTable) * pInpParam->MaxVideoFrames);
       
        if(p3gpmux->MuxAVCCBox.pPpsTable  == NULL)
        {
            status = NvError_InsufficientMemory;                        
            return status;
        }       
    }

    //assign user parameters to global library variables
    p3gpmux->VOPWidth = pInpParam->VOPWidth;
    p3gpmux->VOPHeight = pInpParam->VOPHeight;
    p3gpmux->FrameRate = pInpParam->VOPFrameRate;
    p3gpmux->VideoBoxesSize = 0;
    p3gpmux->VideoFrameCnt = 0;
    p3gpmux->VideoBitRate = pInpParam->VideoBitRate;
    p3gpmux->MaxVideoBitRate = pInpParam->MaxVideoBitRate;
    p3gpmux->FVideoDataToBeDumped = 0;
    p3gpmux->VideoCount = 0;
    p3gpmux->SyncCount= 0;      
    p3gpmux->VidInitFlag = 0;    
    
    return status;
}

void InitVideoTrakAtom(Nv3gpWriter *p3gpmux, NvMM3GpMuxInputParam *pInpParam)
{
    NvMM3GpMuxSTSCBox *tMuxstscBox= NULL;
    NvMM3GpMuxTRAKBox *tMuxtrakBox= NULL;
    NvMM3GpMuxTKHD *tMuxtkhdBox= NULL;
    NvMM3GpMuxMDIA *tMuxmdiaBox= NULL;
    NvMM3GpMuxMDHDBox *tMuxmdhdBox= NULL;
    NvMM3GpMuxHDLRBox *tMuxhdlrBox= NULL;
    NvMM3GpMuxMINFBox *tMuxminfBox= NULL;
    NvMM3GpMuxDINFBox *tMuxdinfBox= NULL;
    NvMM3GpMuxDREFBox *tMuxdrefBox= NULL;
    NvMM3GpMuxSTBLBox *tMuxstblBox= NULL;
    NvMM3GpMuxSTSDBox *tMuxstsdBox= NULL;
    NvMM3GpMuxESDSBox *tMuxesdsBox= NULL;
    NvMM3GpMuxSTSZBox *tMuxstszBox= NULL;
    NvMM3GpMuxCO64Box *tMuxco64Box= NULL;
    NvMM3GpMuxSTCOBox *tMuxstcoBox= NULL; 
    NvMM3GpMuxSTTSBox *tMuxsttsBox= NULL;
    NvMM3GpMuxCTTSBox *tMuxcttsBox= NULL;
    NvMM3GpMuxSTSSBox *tMuxstssBox= NULL;
    NvMM3GpMuxEDTSBox *tMuxedtsBox= NULL;
    NvMM3GpMuxELST *tMuxelstBox= NULL;  
    NvMM3GpMuxAVCCBox *tMuxavcCBox= NULL;
    NvMM3GpMuxS263Box *tMuxs263Box= NULL;
    NvMM3GpMuxD263Box *tMuxd263Box= NULL;
    NvMM3GpMuxAVC1Box *tMuxavc1Box= NULL;
    NvMM3GpMuxMP4VBox *tMuxmp4vBox= NULL;
    NvMM3GpMuxVMHDBox *tMuxvmhdBox= NULL;    
    
    if(pInpParam->ZeroVideoDataFlag == 1)
    {
        //write data for video track i.e trak box
        tMuxtrakBox = &p3gpmux->MuxTRAKBox[VIDEO_TRACK];

        tMuxtrakBox->Box.BoxSize= 8;

        COPY_FOUR_BYTES(tMuxtrakBox->Box.BoxType,'t','r','a','k');
        
        p3gpmux->VideoBoxesSize += tMuxtrakBox->Box.BoxSize;

        tMuxtkhdBox = &p3gpmux->MuxTKHDBox[VIDEO_TRACK];
#ifdef SUPPORT_64BIT_3GP
        tMuxtkhdBox->FullBox.Box.BoxSize= 104;

        COPY_FOUR_BYTES(tMuxtkhdBox->FullBox.Box.BoxType,'t','k','h','d');        
        
        tMuxtkhdBox->FullBox.VersionFlag= 1;
        tMuxtkhdBox->ulCreationTime = CREATION_TIME ;
        tMuxtkhdBox->ulModTime = MODIFICATION_TIME;
        tMuxtkhdBox->ulTrackID = VIDEO_TRACK+1;    
#else
        tMuxtkhdBox->FullBox.Box.BoxSize= 92;

        COPY_FOUR_BYTES(tMuxtkhdBox->FullBox.Box.BoxType,'t','k','h','d');                
        
        tMuxtkhdBox->FullBox.VersionFlag= 1;
        tMuxtkhdBox->CreationTime = CREATION_TIME ;
        tMuxtkhdBox->ModTime = MODIFICATION_TIME;
        tMuxtkhdBox->TrackID = VIDEO_TRACK+1;    
#endif

        p3gpmux->VideoBoxesSize += tMuxtkhdBox->FullBox.Box.BoxSize;
        
        //write data for video media i.e mdia box
        tMuxmdiaBox = &p3gpmux->MuxMDIABox[VIDEO_TRACK];

        tMuxmdiaBox->Box.BoxSize= 8;
        COPY_FOUR_BYTES(tMuxmdiaBox->Box.BoxType,'m','d','i','a');                
        p3gpmux->VideoBoxesSize += tMuxmdiaBox->Box.BoxSize;
    
        //write data for video media i.e mdhd box
        tMuxmdhdBox = &p3gpmux->MuxMDHDBox[VIDEO_TRACK];
#ifdef SUPPORT_64BIT_3GP
        tMuxmdhdBox->FullBox.Box.BoxSize= 44;
        COPY_FOUR_BYTES(tMuxmdhdBox->FullBox.Box.BoxType,'m','d','h','d');                

        tMuxmdhdBox->FullBox.VersionFlag= 1;
        tMuxmdhdBox->ulCreationTime = CREATION_TIME ;
        tMuxmdhdBox->ulModTime = MODIFICATION_TIME;
        tMuxmdhdBox->ulTimeScale = TIME_SCALE;
        tMuxmdhdBox->usLanguage = LANGUAGE_CODE;

#else
        tMuxmdhdBox->FullBox.Box.BoxSize= 32;
        COPY_FOUR_BYTES(tMuxmdhdBox->FullBox.Box.BoxType,'m','d','h','d');                        
        tMuxmdhdBox->FullBox.VersionFlag= 0;
        tMuxmdhdBox->CreationTime = CREATION_TIME ;
        tMuxmdhdBox->ModTime = MODIFICATION_TIME;
        tMuxmdhdBox->TimeScale = TIME_SCALE;
        tMuxmdhdBox->Language = LANGUAGE_CODE;

#endif
        p3gpmux->VideoBoxesSize += tMuxmdhdBox->FullBox.Box.BoxSize;

        tMuxhdlrBox = &p3gpmux->MuxHDLRBox[VIDEO_TRACK];

        tMuxhdlrBox->FullBox.Box.BoxSize= 45;
        COPY_FOUR_BYTES(tMuxhdlrBox->FullBox.Box.BoxType,'h','d','l','r');                
        
        tMuxhdlrBox->FullBox.VersionFlag= 0;
        tMuxhdlrBox->HandlerType[0] = 'v';
        tMuxhdlrBox->HandlerType[1] = 'i';
        tMuxhdlrBox->HandlerType[2] = 'd';
        tMuxhdlrBox->HandlerType[3] = 'e';
        NvOsStrncpy((char *)tMuxhdlrBox->StringName, "VideoHandler",13 * sizeof(NvU8));

        p3gpmux->VideoBoxesSize += tMuxhdlrBox->FullBox.Box.BoxSize;

        //write data for video media information i.e minf box
        tMuxminfBox = &p3gpmux->MuxMINFBox[VIDEO_TRACK];

        tMuxminfBox->Box.BoxSize= 8;
        COPY_FOUR_BYTES(tMuxminfBox->Box.BoxType,'m','i','n','f');                
        
        p3gpmux->VideoBoxesSize += tMuxminfBox->Box.BoxSize;

        //write data for video media information header i.e vmhd box
        tMuxvmhdBox = &p3gpmux->MuxVMHDBox;

        tMuxvmhdBox->FullBox.Box.BoxSize= 20;
        COPY_FOUR_BYTES(tMuxvmhdBox->FullBox.Box.BoxType,'v','m','h','d');                        
        tMuxvmhdBox->FullBox.VersionFlag= 1;
        p3gpmux->VideoBoxesSize += tMuxvmhdBox->FullBox.Box.BoxSize;

        //write data for data information for video i.e dinf box
        tMuxdinfBox = &p3gpmux->MuxDINFBox[VIDEO_TRACK];

        tMuxdinfBox->Box.BoxSize= 36;
        COPY_FOUR_BYTES(tMuxdinfBox->Box.BoxType,'d','i','n','f');                        
        
        p3gpmux->VideoBoxesSize += tMuxdinfBox->Box.BoxSize;

        //write data for data reference for video i.e dref box
        tMuxdrefBox = &p3gpmux->MuxDREFBox[VIDEO_TRACK];

        tMuxdrefBox->FullBox.Box.BoxSize= 28;
        COPY_FOUR_BYTES(tMuxdrefBox->FullBox.Box.BoxType,'d','r','e','f');                                
        tMuxdrefBox->FullBox.VersionFlag= 0;
        tMuxdrefBox->EntryCount = 1;
        tMuxdrefBox->EntryVersionFlag = 1;
        p3gpmux->VideoBoxesSize += tMuxdrefBox->FullBox.Box.BoxSize;

          //write data for video sample table box i.e stbl box
        tMuxstblBox = &p3gpmux->MuxSTBLBox[VIDEO_TRACK];

        tMuxstblBox->Box.BoxSize= 8;

        COPY_FOUR_BYTES(tMuxstblBox->Box.BoxType,'s','t','b','l');                                        
        p3gpmux->VideoBoxesSize += tMuxstblBox->Box.BoxSize;

        //write data for video sample table description box i.e stsd box
        tMuxstsdBox = &p3gpmux->MuxSTSDBox[VIDEO_TRACK];

        switch(pInpParam->Mp4H263Flag)
        {
            case 0:
                tMuxstsdBox->FullBox.Box.BoxSize= 16;
                break;
            case 1:
                tMuxstsdBox->FullBox.Box.BoxSize= 117;
                break;
            case 2:
                tMuxstsdBox->FullBox.Box.BoxSize= 16;
                break;
            default:
                //printf("Incorrect video flag: Possibly no video data\n");
                break;
        }

        COPY_FOUR_BYTES(tMuxstsdBox->FullBox.Box.BoxType,'s','t','s','d');                                        
        
        tMuxstsdBox->FullBox.VersionFlag= 0;
        tMuxstsdBox->EntryCount = 1;
        p3gpmux->VideoBoxesSize += tMuxstsdBox->FullBox.Box.BoxSize;

        if(pInpParam->Mp4H263Flag == 0)
        {
            //write data for mpeg4 sample entry box i.e mp4v box
            tMuxmp4vBox = &p3gpmux->MuxMP4VBox;

            tMuxmp4vBox->BoxWidthHeight.Box.BoxSize= 86;

            COPY_FOUR_BYTES(tMuxmp4vBox->BoxWidthHeight.Box.BoxType,'m','p','4','v');                                        
        
            tMuxmp4vBox->DataRefIndex = 1;
            tMuxmp4vBox->BoxWidthHeight.Width= p3gpmux->VOPWidth;
            tMuxmp4vBox->BoxWidthHeight.Height= p3gpmux->VOPHeight;
        }
        else if(pInpParam->Mp4H263Flag == 1)
        {
            //write data for H263 sample entry box i.e s263 box
            tMuxs263Box = &p3gpmux->MuxS263Box;

            tMuxs263Box->BoxWidthHeight.Box.BoxSize= 101;

            COPY_FOUR_BYTES(tMuxs263Box->BoxWidthHeight.Box.BoxType,'s','2','6','3');                                        
            
            tMuxs263Box->usDataRefIndex = 1;
            tMuxs263Box->BoxWidthHeight.Width= p3gpmux->VOPWidth;
            tMuxs263Box->BoxWidthHeight.Height= p3gpmux->VOPHeight;
        }
        else if(pInpParam->Mp4H263Flag == 2)
        {
            //write data for AVC1 sample entry box i.e avc1 box
            tMuxavc1Box = &p3gpmux->MuxAVC1Box;

            tMuxavc1Box->BoxWidthHeight.Box.BoxSize= 86;
            COPY_FOUR_BYTES(tMuxavc1Box->BoxWidthHeight.Box.BoxType,'a','v','c','1');                                        
            
            tMuxavc1Box->DataRefIndex = 1;
            tMuxavc1Box->BoxWidthHeight.Width= p3gpmux->VOPWidth;
            tMuxavc1Box->BoxWidthHeight.Height= p3gpmux->VOPHeight;
        }

        if(pInpParam->Mp4H263Flag == 0)
        {
            //write data for mpeg4 specific box i.e esds box
            tMuxesdsBox = &p3gpmux->MuxESDSBox[VIDEO_TRACK];

            tMuxesdsBox->FullBox.Box.BoxSize= 64;

            COPY_FOUR_BYTES(tMuxesdsBox->FullBox.Box.BoxType,'e','s','d','s');                                        
            
            tMuxesdsBox->FullBox.VersionFlag= 0;
            tMuxesdsBox->ES_DescrTag = 3;
            tMuxesdsBox->DataLen1 = 47;
            tMuxesdsBox->ES_ID = 0xC9;
            tMuxesdsBox->FlagPriority = 4;
            tMuxesdsBox->DecoderConfigDescrTag = 4;
            tMuxesdsBox->DataLen2 = 18;
            tMuxesdsBox->ObjectTypeIndication = 0x20;
            tMuxesdsBox->StreamType = 0x11;
            tMuxesdsBox->BufferSizeDB = (p3gpmux->VideoBitRate*1000)>>2;
            tMuxesdsBox->MaxBitrate = p3gpmux->MaxVideoBitRate*1000;
            tMuxesdsBox->AvgBitrate = p3gpmux->VideoBitRate*1000;
            tMuxesdsBox->DecSpecificInfoTag = 5;
            tMuxesdsBox->SLConfigDescrTag = 6;
            tMuxesdsBox->DataLen4 = 16;
            NvOsMemset(tMuxesdsBox->SLConfigDescrInfo,0,16);

            tMuxesdsBox->SLConfigDescrInfo[0] = 0;
            tMuxesdsBox->SLConfigDescrInfo[4] = (TIME_SCALE >> 8);
            tMuxesdsBox->SLConfigDescrInfo[5] = (TIME_SCALE & 0xFF);
            tMuxesdsBox->SLConfigDescrInfo[10] = 0x0;
            tMuxesdsBox->SLConfigDescrInfo[15] = 3;
        }
        else if(pInpParam->Mp4H263Flag == 1)
        {
            //write data for H263 specific box i.e d263 box
            tMuxd263Box = &p3gpmux->MuxD263Box;

            tMuxd263Box->Box.BoxSize= 15;

            COPY_FOUR_BYTES(tMuxd263Box->Box.BoxType,'d','2','6','3');                                        
            
            tMuxd263Box->Vendor[0] = 'N';
            tMuxd263Box->Vendor[1] = 'V';
            tMuxd263Box->Vendor[2] = 'D';
            tMuxd263Box->Vendor[3] = 'A';
            tMuxd263Box->DecoderVersion = 0;
            tMuxd263Box->H263_Level = (NvU8)pInpParam->Level;
            tMuxd263Box->H263_Profile = (NvU8)pInpParam->Profile;
        }
        else if(pInpParam->Mp4H263Flag == 2)
        {
            //write data for AVC specific box i.e avcC box
            tMuxavcCBox = &p3gpmux->MuxAVCCBox;

            tMuxavcCBox->Box.BoxSize= 15;

            COPY_FOUR_BYTES(tMuxavcCBox->Box.BoxType,'a','v','c','C');
            
            tMuxavcCBox->ConfigurationVersion = 1;
            tMuxavcCBox->NumOfSequenceParameterSets = 0;
            tMuxavcCBox->NumOfPictureParameterSets = 0;
            tMuxavcCBox->LengthSizeMinusOne = 3; //4-1
        }

        //write data for video sample size box i.e stsz box
        tMuxstszBox = &p3gpmux->MuxSTSZBox[VIDEO_TRACK];

        tMuxstszBox->FullBox.Box.BoxSize= 20;

        COPY_FOUR_BYTES(tMuxstszBox->FullBox.Box.BoxType,'s','t','s','z');
            
        tMuxstszBox->FullBox.VersionFlag= 0;
        tMuxstszBox->SampleSize = 0;
        tMuxstszBox->SampleCount = 0;
        p3gpmux->VideoBoxesSize += tMuxstszBox->FullBox.Box.BoxSize;

        if(p3gpmux->Mode == NvMM_Mode32BitAtom)
        {
            //write data for video chunk offset box i.e stco box
            tMuxstcoBox = &p3gpmux->MuxSTCOBox[VIDEO_TRACK];

            tMuxstcoBox->FullBox.Box.BoxSize= 16;

            COPY_FOUR_BYTES(tMuxstcoBox->FullBox.Box.BoxType,'s','t','c','o');
        
            tMuxstcoBox->FullBox.VersionFlag= 0;
            tMuxstcoBox->ChunkCount = 0;
            p3gpmux->VideoBoxesSize += tMuxstcoBox->FullBox.Box.BoxSize;
        }
        else
        {
            //write data for video chunk offset box i.e co64 box
            tMuxco64Box = &p3gpmux->MuxCO64Box[VIDEO_TRACK];

            tMuxco64Box->FullBox.Box.BoxSize= 16;

            COPY_FOUR_BYTES(tMuxco64Box->FullBox.Box.BoxType,'c','o','6','4');
            
            tMuxco64Box->FullBox.VersionFlag= 0;
            tMuxco64Box->ChunkCount = 0;
            p3gpmux->VideoBoxesSize += tMuxco64Box->FullBox.Box.BoxSize;
        }

        //frame is of 20ms so in 1 sec 50 frames will get encoded

        //write data for video decoding time to sample box i.e stts box
        tMuxsttsBox = &p3gpmux->MuxSTTSBox[VIDEO_TRACK];

        tMuxsttsBox->FullBox.Box.BoxSize= 16; //entrycount=0 means no samples at all initially

        COPY_FOUR_BYTES(tMuxsttsBox->FullBox.Box.BoxType,'s','t','t','s');
            
        tMuxsttsBox->FullBox.VersionFlag= 0;
        tMuxsttsBox->EntryCount = 0;
        p3gpmux->VideoBoxesSize += tMuxsttsBox->FullBox.Box.BoxSize;

        //write data for video composition time to sample box i.e ctts box
        tMuxcttsBox = &p3gpmux->MuxCTTSBox[VIDEO_TRACK];

        tMuxcttsBox->FullBox.Box.BoxSize= 24;

        COPY_FOUR_BYTES(tMuxcttsBox->FullBox.Box.BoxType,'c','t','t','s');
        
        tMuxcttsBox->FullBox.VersionFlag= 0;
        tMuxcttsBox->EntryCount = 1;
        //since CT and DT do not differ ctts offset is made zero.
        tMuxcttsBox->SampleOffset = 0; //TIME_SCALE/p3gpmux->uiFrameRate;
        p3gpmux->VideoBoxesSize += tMuxcttsBox->FullBox.Box.BoxSize;

        //write data for video sample to chunk box i.e stsc box
        tMuxstscBox = &p3gpmux->MuxSTSCBox[VIDEO_TRACK];

        tMuxstscBox->FullBox.Box.BoxSize= 16;

        COPY_FOUR_BYTES(tMuxstscBox->FullBox.Box.BoxType,'s','t','s','c');
        
        tMuxstscBox->FullBox.VersionFlag= 0;
        tMuxstscBox->EntryCount = 0;
        p3gpmux->VideoBoxesSize += tMuxstscBox->FullBox.Box.BoxSize;

        //write data for video sync sample box i.e stss box
        tMuxstssBox = &p3gpmux->MuxSTSSBox[VIDEO_TRACK];

        tMuxstssBox->FullBox.Box.BoxSize= 16;

        COPY_FOUR_BYTES(tMuxstssBox->FullBox.Box.BoxType,'s','t','s','s');
        
        tMuxstssBox->FullBox.VersionFlag= 0;
        tMuxstssBox->EntryCount = 0;
        p3gpmux->VideoBoxesSize += tMuxstssBox->FullBox.Box.BoxSize;
    }

    if(pInpParam->AudioVideoFlag  == NvMM_AV_AUDIO_VIDEO_BOTH)
    {
        tMuxedtsBox = &p3gpmux->MuxEDTSBox[VIDEO_TRACK];

        tMuxedtsBox->Box.BoxSize= 8;

        COPY_FOUR_BYTES(tMuxedtsBox->Box.BoxType,'e','d','t','s');
        
        p3gpmux->VideoBoxesSize += tMuxedtsBox->Box.BoxSize;
        

        //write data for video chunk offset box i.e elst box
        tMuxelstBox = &p3gpmux->MuxELSTBox[VIDEO_TRACK];

        tMuxelstBox->FullBox.Box.BoxSize= 16;
        
        COPY_FOUR_BYTES(tMuxelstBox->FullBox.Box.BoxType,'e','l','s','t');
    
        tMuxelstBox->FullBox.VersionFlag= 1;
        tMuxelstBox->EntryCount = 1;
        p3gpmux->VideoBoxesSize += tMuxelstBox->FullBox.Box.BoxSize;
    }
}

void WriterDeallocVideo(Nv3gpWriter *p3gpmux, NvMM3GpMuxInputParam *pInpParam)
{
    NvMMFileAVSWriteMsg Message;     
    
    Message.pData = NULL;
    Message.DataSize = 0;
    Message.ThreadShutDown = NV_TRUE;

    if(p3gpmux->MsgQueueVideo)
    {
        NvMMQueueEnQ(p3gpmux->MsgQueueVideo, &Message, 0);
    }
    
    //Stop the thread
    if(p3gpmux->VideoWriteSema)
    {
        NvOsSemaphoreSignal(p3gpmux->VideoWriteSema);   
    }

    if(p3gpmux->VideoWriteDoneSema)
    {
        NvOsSemaphoreWait(p3gpmux->VideoWriteDoneSema);
    }

    if(p3gpmux->FileWriterVideoThread)
    {
        NvOsThreadJoin (p3gpmux->FileWriterVideoThread); 
        p3gpmux->FileWriterVideoThread = NULL;
    }

    if(p3gpmux->VideoWriteSema)
    {
        NvOsSemaphoreDestroy(p3gpmux->VideoWriteSema);
        p3gpmux->VideoWriteSema = NULL;
    }

    if(p3gpmux->VideoWriteDoneSema)
    {
        NvOsSemaphoreDestroy(p3gpmux->VideoWriteDoneSema);   
        p3gpmux->VideoWriteDoneSema = NULL;
    }

    if(p3gpmux->MsgQueueVideo)
    {
        NvMMQueueDestroy(&p3gpmux->MsgQueueVideo);
        p3gpmux->MsgQueueVideo = NULL;
    }

    if(p3gpmux->MuxELSTBox[VIDEO_TRACK].pElstTable)
    {
        NvOsFree(p3gpmux->MuxELSTBox[VIDEO_TRACK].pElstTable);
        p3gpmux->MuxELSTBox[VIDEO_TRACK].pElstTable = NULL;
    }
    
    if(p3gpmux->PingVideoBuffer)
    {
        NvOsFree(p3gpmux->PingVideoBuffer);
        p3gpmux->PingVideoBuffer= NULL;
    }

    if(p3gpmux->PongVideoBuffer)
    {
        NvOsFree(p3gpmux->PongVideoBuffer);
        p3gpmux->PongVideoBuffer= NULL;
    }            

    if(p3gpmux->PingSyncBuffer)
    {
        NvOsFree(p3gpmux->PingSyncBuffer);
        p3gpmux->PingSyncBuffer= NULL;
    }

    if(p3gpmux->PongSyncBuffer)
    {
        NvOsFree(p3gpmux->PongSyncBuffer);
        p3gpmux->PongSyncBuffer= NULL;
    }                  

    if(p3gpmux->MuxAVCCBox.pPspsTable )
    {
        NvOsFree(p3gpmux->MuxAVCCBox.pPspsTable);
        p3gpmux->MuxAVCCBox.pPspsTable= NULL;
    }
    
    if(p3gpmux->MuxAVCCBox.pPpsTable)
    {
        NvOsFree(p3gpmux->MuxAVCCBox.pPpsTable);
        p3gpmux->MuxAVCCBox.pPpsTable = NULL;
    }

    if(p3gpmux->FileVideoHandle)
    {
        NvOsFclose(p3gpmux->FileVideoHandle);
        p3gpmux->FileVideoHandle = NULL;
    }            

    if(pInpParam->ZeroVideoDataFlag== 1)
    {
        if(p3gpmux->URIConcatVideo[0])
        {
            NvOsFremove((const char *)p3gpmux->URIConcatVideo[0]);
        }
    }
    
    if(p3gpmux->FileSyncHandle)
    {
        NvOsFclose(p3gpmux->FileSyncHandle);
        p3gpmux->FileSyncHandle = NULL;
    }                

    if(pInpParam->ZeroVideoDataFlag== 1)
    {
        if(p3gpmux->URIConcatSync[0])
        {
            NvOsFremove((const char *)p3gpmux->URIConcatSync[0]);
        }
    }        

    if(p3gpmux->URIConcatVideo[0])
    {
        NvOsFree(p3gpmux->URIConcatVideo[0]);
        p3gpmux->URIConcatVideo[0] = NULL;
    }
    
    if(p3gpmux->URIConcatSync[0])
    {
        NvOsFree(p3gpmux->URIConcatSync[0]);
        p3gpmux->URIConcatSync[0] = NULL;
    }            
}
