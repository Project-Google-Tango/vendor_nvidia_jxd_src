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

// Supported sampling rates for AAC encoding
static NvU32 AacSamplingRates[] =
{
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000
};

static void NvmmAudioFileWriteThread(void *arg)
{
    Nv3gpWriter *pgp3GPPMuxStatus = (Nv3gpWriter *)arg;
    NvMMFileAVSWriteMsg pMessage; 
    NvBool bShutDown = NV_FALSE;
    NvError status = NvSuccess;
    NvU64 CurrentWritePosition = 0;

    while (bShutDown == NV_FALSE)
    {
        NvOsSemaphoreWait(pgp3GPPMuxStatus->AudioWriteSema);

        status = NvMMQueueDeQ(pgp3GPPMuxStatus->MsgQueueAudio, &pMessage);
        if(status != NvSuccess)
        {
            continue;
        }
        else
        {

            if( pMessage.ThreadShutDown)
            {
                bShutDown = NV_TRUE;

                NvOsSemaphoreSignal(pgp3GPPMuxStatus->AudioWriteDoneSema);
                break;
            }
            else
            {
                status = NvOsFwrite(pgp3GPPMuxStatus->FileAudioHandle, (CPbyte *)(pMessage.pData), pMessage.DataSize);
                if(status != NvSuccess)
                {
                    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_ERROR, "--NvmmAudioFileWriteThread::Write failed:bail out...\n"));
                    // Write failed
                    pgp3GPPMuxStatus->AudioWriteFailed = NV_TRUE;
                    pgp3GPPMuxStatus->AudioWriteStatus = status;
                    // Delete the temporary file to create room for writing pending mdat info
                    status = NvMM3GpMuxRecDelReservedFile(*pgp3GPPMuxStatus->Context3GpMux);
                    if(NvSuccess == status)
                    {
                        status = NvOsFseek(pgp3GPPMuxStatus->FileAudioHandle, CurrentWritePosition, NvOsSeek_Set);
                        if(NvSuccess == status)
                        {
                            //Once again try to write the data if it fails, it fails
                            status = NvOsFwrite(pgp3GPPMuxStatus->FileAudioHandle, (CPbyte *)(pMessage.pData), pMessage.DataSize);
                            if(status != NvSuccess)
                                NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmAudioFileWriteThread:: File write failed second time:recorded file is unusable\n"));
                        }
                        else
                            NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmAudioFileWriteThread::Seek failed recorded file is unusable\n"));
                    }
                    else
                        NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmAudioFileWriteThread::Deleting reserved partition failed\n"));
                }
                CurrentWritePosition += pMessage.DataSize;


                NvOsSemaphoreSignal(pgp3GPPMuxStatus->AudioWriteDoneSema);
             }
         }
    }
    

 
}


NvError AudioInit(Nv3gpWriter *p3gpmux, NvU32 length1, NvU32 length2, NvMM3GpMuxInputParam *pInpParam)
{
    NvError status=NvSuccess;
    char *szURI;
    NvU32 flags;        
    
    p3gpmux->URIConcatAudio[0] =       
        (char *)NvOsAlloc(sizeof(char) * (length1 + length2 + 1));
    
    if(p3gpmux->URIConcatAudio[0] == NULL)
    {
        status = NvError_InsufficientMemory;    
        return status;
    }
    
    status = NvOsSemaphoreCreate(&p3gpmux->AudioWriteSema, 0);
    if ( status != NvSuccess)
    {
        return status;
    }

    status = NvOsSemaphoreCreate(&p3gpmux->AudioWriteDoneSema, 0);
    if ( status != NvSuccess)
    {
        return status;
    }

    status = NvMMQueueCreate( &p3gpmux->MsgQueueAudio,
                                               MAX_QUEUE_LEN, 
                                               sizeof(NvMMFileAVSWriteMsg), NV_TRUE);
    if(status != NvSuccess)
    {
        return status;
    }    

    status = NvOsThreadCreate( NvmmAudioFileWriteThread, 
                                              (void *)(p3gpmux),
                                              &(p3gpmux->FileWriterAudioThread));
    if(status != NvSuccess)
    {
        return status;
    }

    if(pInpParam->SpeechAudioFlag != 10)
    {
        /*
        * Absolute memory allocation
        * (sizeof(telstTable) * (pInpParam->maxAudioFrames));
        * In actual, only a table with 2 entries is required
        */
        p3gpmux->MuxELSTBox[SOUND_TRACK].pElstTable =  NvOsAlloc((sizeof(NvMM3GpMuxELSTTable) * 2));
        if(p3gpmux->MuxELSTBox[SOUND_TRACK].pElstTable  == NULL)
        {
            status = NvError_InsufficientMemory;                
            return status;
        }



        // File to dump STTS, STSZ,STSC,CO64
        flags =    NVOS_OPEN_CREATE| NVOS_OPEN_WRITE;
        szURI = "audiM.dat";

        NvMM3GpMuxWriterStrcat( p3gpmux->URIConcatAudio, 
                                         pInpParam->pFilePath,length1, szURI,length2);            

        status = NvOsFopen((const char *)p3gpmux->URIConcatAudio[0], (NvU32)flags, 
                             &p3gpmux->FileAudioHandle);

        if (status != NvSuccess)
        {
            return status;
        }                    

        //////////////// ping and pong buffer to be alloc
        // sizeof(STTS Table) + sizeof (STSZ) + sizeof(STSC) + sizeof(CO64) = 32 Bytes
        p3gpmux->PingAudioBuffer = NvOsAlloc((TOTAL_ENTRIESPERATOM *
            SIZEOFATOMS * sizeof(NvU8)) + ACTUAL_ATOM_ENTRIES);
        if(p3gpmux->PingAudioBuffer  == NULL)
        {
            status = NvError_InsufficientMemory;                        
            return status;
        }



        // sizeof(STTS Table) + sizeof (STSZ) + sizeof(STSC) + sizeof(CO64) = 32 Bytes
        p3gpmux->PongAudioBuffer = NvOsAlloc((TOTAL_ENTRIESPERATOM *
            SIZEOFATOMS * sizeof(NvU8)) + ACTUAL_ATOM_ENTRIES);
        if(p3gpmux->PongAudioBuffer  == NULL)
        {
            status = NvError_InsufficientMemory;                        
            return status;
        }   



        p3gpmux->pPAudioBuffer = p3gpmux->PingAudioBuffer;
        
    }

    p3gpmux->SpeechBoxesSize = 0;
    p3gpmux->SpeechFrameCnt = 0;    
    p3gpmux->SpeechModeSet = pInpParam->SpeechModeSet;
    p3gpmux->FAudioDataToBeDumped = 0;    
    p3gpmux->AudioCount = 0;    
    p3gpmux->SpeechOffset = 0;
    p3gpmux->AudInitFlag = 0;    

    if(pInpParam->SpeechAudioFlag != 10)
    {
        if(pInpParam->SpeechAudioFlag != 2)
        {
            pInpParam->AacSamplingFreqIndex = 0;
            pInpParam->AacObjectType = 0; 
            pInpParam->AacChannelNumber = 0;
            pInpParam->AacAvgBitRate = 0;
            pInpParam->AacMaxBitRate = 0;
        }

        if(pInpParam->SpeechAudioFlag == 0)
        {
            switch(p3gpmux->SpeechModeSet)
            {
                case 0:
                    p3gpmux->SpeechSampleSize = 104>>3;
                    p3gpmux->SpeechModeDump = 1;
                    break;
                case 1:
                    p3gpmux->SpeechSampleSize = 112>>3;
                    p3gpmux->SpeechModeDump = 2;
                    break;
                case 2:
                    p3gpmux->SpeechSampleSize = 128>>3;
                    p3gpmux->SpeechModeDump = 4;
                    break;
                case 3:
                    p3gpmux->SpeechSampleSize = 144>>3;
                    p3gpmux->SpeechModeDump = 8;
                    break;
                case 4:
                    p3gpmux->SpeechSampleSize = 160>>3;
                    p3gpmux->SpeechModeDump = 16;
                    break;
                case 5:
                    p3gpmux->SpeechSampleSize = 168>>3;
                    p3gpmux->SpeechModeDump = 32;
                    break;
                case 6:
                    p3gpmux->SpeechSampleSize = 216>>3;
                    p3gpmux->SpeechModeDump = 64;
                    break;
                case 7:
                    p3gpmux->SpeechSampleSize = 256>>3;
                    p3gpmux->SpeechModeDump = 128;
                    break;
                default:
                    p3gpmux->SpeechSampleSize = 48>>3;
                    p3gpmux->SpeechModeDump = 256;
                    break;
            }
        }
        else if(pInpParam->SpeechAudioFlag == 1)
        {
            switch(p3gpmux->SpeechModeSet)
            {
                case 0:
                    p3gpmux->SpeechSampleSize = 17;
                    p3gpmux->SpeechModeDump = 1;
                    break;
                case 1:
                    p3gpmux->SpeechSampleSize = 23;
                    p3gpmux->SpeechModeDump = 2;
                    break;
                case 2:
                    p3gpmux->SpeechSampleSize = 32;
                    p3gpmux->SpeechModeDump = 4;
                    break;
                case 3:
                    p3gpmux->SpeechSampleSize = 36;
                    p3gpmux->SpeechModeDump = 8;
                    break;
                case 4:
                    p3gpmux->SpeechSampleSize = 40;
                    p3gpmux->SpeechModeDump = 16;
                    break;
                case 5:
                    p3gpmux->SpeechSampleSize = 46;
                    p3gpmux->SpeechModeDump = 32;
                    break;
                case 6:
                    p3gpmux->SpeechSampleSize = 50;
                    p3gpmux->SpeechModeDump = 64;
                    break;
                case 7:
                    p3gpmux->SpeechSampleSize = 58;
                    p3gpmux->SpeechModeDump = 128;
                    break;
                case 8:
                    p3gpmux->SpeechSampleSize = 60;
                    p3gpmux->SpeechModeDump = 256;
                    break;
                default:
                    p3gpmux->SpeechSampleSize = 12;
                    p3gpmux->SpeechModeDump = 512;
                    break;
            }
        }
    }

    return status;
}

void InitAudioTrakAtom(Nv3gpWriter *p3gpmux, NvMM3GpMuxInputParam *pInpParam)
{
    NvMM3GpMuxSTSCBox *tMuxstscBox= NULL;
    NvMM3GpMuxTRAKBox *tMuxtrakBox= NULL;
    NvMM3GpMuxTKHD *tMuxtkhdBox= NULL;
    NvMM3GpMuxMDIA *tMuxmdiaBox= NULL;
    NvMM3GpMuxMDHDBox *tMuxmdhdBox= NULL;
    NvMM3GpMuxHDLRBox *tMuxhdlrBox= NULL;
    NvMM3GpMuxMINFBox *tMuxminfBox= NULL;
    NvMM3GpMuxSMHDBox *tMuxsmhdBox= NULL;
    NvMM3GpMuxDINFBox *tMuxdinfBox= NULL;
    NvMM3GpMuxDREFBox *tMuxdrefBox= NULL;
    NvMM3GpMuxSTBLBox *tMuxstblBox= NULL;
    NvMM3GpMuxSTSDBox *tMuxstsdBox= NULL;
    NvMM3GpMuxSQCPBox *tMuxsqcpBox= NULL;
    NvMM3GpMuxMP4ABox *tMuxmp4aBox= NULL;
    NvMM3GpMuxSAMRBox *tMuxsamrBox= NULL;
    NvMM3GpMuxDQCP *tMuxdqcpBox= NULL;   
    NvMM3GpMuxESDSBox *tMuxesdsBox= NULL;
    NvMM3GpMuxDAMRBox *tMuxdamrBox= NULL;
    NvMM3GpMuxSTSZBox *tMuxstszBox= NULL;
    NvMM3GpMuxCO64Box *tMuxco64Box= NULL;
    NvMM3GpMuxSTCOBox *tMuxstcoBox= NULL; 
    NvMM3GpMuxSTTSBox *tMuxsttsBox= NULL;
    NvMM3GpMuxCTTSBox *tMuxcttsBox= NULL;
    NvMM3GpMuxSTSSBox *tMuxstssBox= NULL;
    NvMM3GpMuxEDTSBox *tMuxedtsBox= NULL;
    NvMM3GpMuxELST *tMuxelstBox= NULL;  
    NvU32 SamplingFreq=8000;
    NvU16 NumElements;
    NvU32 SampleRate;
    NvU8 Index;
    NvU8 ObjectType; // AAC-LC 
    
    if(pInpParam->SpeechAudioFlag != 10)
    {
        //write data for sound track i.e trak box
        tMuxtrakBox = &p3gpmux->MuxTRAKBox[SOUND_TRACK];

        tMuxtrakBox->Box.BoxSize= 8;
       
        COPY_FOUR_BYTES(tMuxtrakBox->Box.BoxType,'t','r','a','k');        

        p3gpmux->SpeechBoxesSize += tMuxtrakBox->Box.BoxSize;

        //write data for track header i.e tkhd box
        tMuxtkhdBox = &p3gpmux->MuxTKHDBox[SOUND_TRACK];
#ifdef SUPPORT_64BIT_3GP
        tMuxtkhdBox->FullBox.Box.BoxSize= 104;

        COPY_FOUR_BYTES(tMuxtkhdBox->FullBox.Box.BoxType,'t','k','h','d');        
        
        tMuxtkhdBox->FullBox.VersionFlag= 1;
        tMuxtkhdBox->ulCreationTime = CREATION_TIME ;
        tMuxtkhdBox->ulModTime = MODIFICATION_TIME;
        tMuxtkhdBox->ulTrackID = SOUND_TRACK+1;
        p3gpmux->SpeechBoxesSize += tMuxtkhdBox->FullBox.Box.BoxSize;

#else
        tMuxtkhdBox->FullBox.Box.BoxSize= 92;

        COPY_FOUR_BYTES(tMuxtkhdBox->FullBox.Box.BoxType,'t','k','h','d');        
        
        tMuxtkhdBox->FullBox.VersionFlag= 0;
        tMuxtkhdBox->CreationTime = CREATION_TIME ;
        tMuxtkhdBox->ModTime = MODIFICATION_TIME;
        tMuxtkhdBox->TrackID = SOUND_TRACK+1;
        p3gpmux->SpeechBoxesSize += tMuxtkhdBox->FullBox.Box.BoxSize;

#endif

        //write data for sound media i.e mdia box
        tMuxmdiaBox = &p3gpmux->MuxMDIABox[SOUND_TRACK];

        tMuxmdiaBox->Box.BoxSize= 8;

        COPY_FOUR_BYTES(tMuxmdiaBox->Box.BoxType,'m','d','i','a');        

        p3gpmux->SpeechBoxesSize += tMuxmdiaBox->Box.BoxSize;

        //write data for media header i.e mdhd box
        tMuxmdhdBox = &p3gpmux->MuxMDHDBox[SOUND_TRACK];
#ifdef SUPPORT_64BIT_3GP
        tMuxmdhdBox->FullBox.Box.BoxSize= 44;
        
        COPY_FOUR_BYTES(tMuxmdhdBox->FullBox.Box.BoxType,'m','d','h','d');        
        
        tMuxmdhdBox->FullBox.VersionFlag= 1;
        tMuxmdhdBox->CreationTime = CREATION_TIME ;
        tMuxmdhdBox->ModTime = MODIFICATION_TIME;
        tMuxmdhdBox->TimeScale = TIME_SCALE;
        tMuxmdhdBox->Language = LANGUAGE_CODE;
        p3gpmux->SpeechBoxesSize += tMuxmdhdBox->FullBox.Box.BoxSize;

#else
        tMuxmdhdBox->FullBox.Box.BoxSize= 32;

        COPY_FOUR_BYTES(tMuxmdhdBox->FullBox.Box.BoxType,'m','d','h','d');        

        tMuxmdhdBox->FullBox.VersionFlag= 0;
        tMuxmdhdBox->CreationTime = CREATION_TIME ;
        tMuxmdhdBox->ModTime = MODIFICATION_TIME;
        tMuxmdhdBox->TimeScale = TIME_SCALE;
        tMuxmdhdBox->Language = LANGUAGE_CODE;
        p3gpmux->SpeechBoxesSize += tMuxmdhdBox->FullBox.Box.BoxSize;

#endif

        //write data for handler i.e hdlr box
        tMuxhdlrBox = &p3gpmux->MuxHDLRBox[SOUND_TRACK];

        tMuxhdlrBox->FullBox.Box.BoxSize= 45;

        COPY_FOUR_BYTES(tMuxhdlrBox->FullBox.Box.BoxType,'h','d','l','r');        
        
        tMuxhdlrBox->FullBox.VersionFlag= 0;
        tMuxhdlrBox->HandlerType[0] = 's';
        tMuxhdlrBox->HandlerType[1] = 'o';
        tMuxhdlrBox->HandlerType[2] = 'u';
        tMuxhdlrBox->HandlerType[3] = 'n';

        NvOsStrncpy((char *)tMuxhdlrBox->StringName, "SoundHandler",13 * sizeof(NvU8));

        p3gpmux->SpeechBoxesSize += tMuxhdlrBox->FullBox.Box.BoxSize;

        //write data for sound media information i.e minf box
        tMuxminfBox = &p3gpmux->MuxMINFBox[SOUND_TRACK];

        tMuxminfBox->Box.BoxSize= 8;

        COPY_FOUR_BYTES(tMuxminfBox->Box.BoxType,'m','i','n','f');        
        
        p3gpmux->SpeechBoxesSize += tMuxminfBox->Box.BoxSize;

         //write data for sound media information header i.e smhd box
        tMuxsmhdBox = &p3gpmux->MuxSMHDBox;

        tMuxsmhdBox->FullBox.Box.BoxSize= 16;

        COPY_FOUR_BYTES(tMuxsmhdBox->FullBox.Box.BoxType,'s','m','h','d');        
        
        tMuxsmhdBox->FullBox.VersionFlag= 0;
        p3gpmux->SpeechBoxesSize += tMuxsmhdBox->FullBox.Box.BoxSize;

        //write data for data information for sound i.e dinf box
        tMuxdinfBox = &p3gpmux->MuxDINFBox[SOUND_TRACK];

        tMuxdinfBox->Box.BoxSize= 36;

        COPY_FOUR_BYTES(tMuxdinfBox->Box.BoxType,'d','i','n','f');        
        
        p3gpmux->SpeechBoxesSize += tMuxdinfBox->Box.BoxSize;

        //write data for data reference for sound i.e dref box
        tMuxdrefBox = &p3gpmux->MuxDREFBox[SOUND_TRACK];

        tMuxdrefBox->FullBox.Box.BoxSize= 28;
        
        COPY_FOUR_BYTES(tMuxdrefBox->FullBox.Box.BoxType,'d','r','e','f');
        
        tMuxdrefBox->FullBox.VersionFlag= 0;
        tMuxdrefBox->EntryCount = 1;
        tMuxdrefBox->EntryVersionFlag = 0xC;
        p3gpmux->SpeechBoxesSize += tMuxdrefBox->FullBox.Box.BoxSize;

        //write data for sound sample table box i.e stbl box
        tMuxstblBox = &p3gpmux->MuxSTBLBox[SOUND_TRACK];

        tMuxstblBox->Box.BoxSize= 8;
        
        COPY_FOUR_BYTES(tMuxstblBox->Box.BoxType,'s','t','b','l');
        
        p3gpmux->SpeechBoxesSize += tMuxstblBox->Box.BoxSize;

        //write data for sound sample table description box i.e stsd box
        tMuxstsdBox = &p3gpmux->MuxSTSDBox[SOUND_TRACK];

        if(pInpParam->SpeechAudioFlag == 3)
        {
            tMuxstsdBox->FullBox.Box.BoxSize= 66;
        }
        else if (pInpParam->SpeechAudioFlag == 4)
        {
            tMuxstsdBox->FullBox.Box.BoxSize= 254; //238+16
        }
        else if (pInpParam->SpeechAudioFlag == 2)
        {
             tMuxstsdBox->FullBox.Box.BoxSize= 0x5D;
        }
        else
        {
            tMuxstsdBox->FullBox.Box.BoxSize= 69;
        }

        COPY_FOUR_BYTES(tMuxstsdBox->FullBox.Box.BoxType,'s','t','s','d');
        
        tMuxstsdBox->FullBox.VersionFlag= 0;
        tMuxstsdBox->EntryCount = 1;
        p3gpmux->SpeechBoxesSize += tMuxstsdBox->FullBox.Box.BoxSize;

        if (pInpParam->SpeechAudioFlag == 3)
        {
            //write data for qcelp sample entry box i.e sqcp box
            tMuxsqcpBox = &p3gpmux->MuxSQCPBox;

            tMuxsqcpBox->BoxIndexTime.Box.BoxSize= 50;

            COPY_FOUR_BYTES(tMuxsqcpBox->BoxIndexTime.Box.BoxType,'s','q','c','p');            

            tMuxsqcpBox->BoxIndexTime.DataRefIndex= 1;
            tMuxsqcpBox->BoxIndexTime.TimeScale= TIME_SCALE;
        }
        else if (pInpParam->SpeechAudioFlag == 4)
        {
            //fill mp4audiosampleentry box
            tMuxmp4aBox = &p3gpmux->MuxMP4ABox;
            tMuxmp4aBox->BoxIndexTime.Box.BoxSize= 238;
            
            COPY_FOUR_BYTES(tMuxmp4aBox->BoxIndexTime.Box.BoxType,'m','p','4','a');            

            tMuxmp4aBox->BoxIndexTime.DataRefIndex= 1;
            //tMuxmp4aBox->ulTimeScale = TIME_SCALE;
            tMuxmp4aBox->BoxIndexTime.TimeScale= (NvU16)SamplingFreq; //sampling frequency
        }
        else if(pInpParam->SpeechAudioFlag == 2)
        {
            //fill mp4audiosampleentry box
            tMuxmp4aBox = &p3gpmux->MuxMP4ABox;
            tMuxmp4aBox->BoxIndexTime.Box.BoxSize= 0x4D;
           
            COPY_FOUR_BYTES(tMuxmp4aBox->BoxIndexTime.Box.BoxType,'m','p','4','a');            
            
            tMuxmp4aBox->BoxIndexTime.DataRefIndex= 1;
            //tMuxmp4aBox->ulTimeScale = TIME_SCALE;
            tMuxmp4aBox->BoxIndexTime.TimeScale= (NvU16)SamplingFreq; //sampling frequency
        }
        else
        {
            //write data for amr sample entry box i.e samr box
            tMuxsamrBox = &p3gpmux->MuxSAMRBox;

            tMuxsamrBox->BoxIndexTime.Box.BoxSize= 53;
            if(pInpParam->SpeechAudioFlag == 0)
            {
                COPY_FOUR_BYTES(tMuxsamrBox->BoxIndexTime.Box.BoxType,'s','a','m','r');                            
            }
            else if(pInpParam->SpeechAudioFlag == 1)
            {
                COPY_FOUR_BYTES(tMuxsamrBox->BoxIndexTime.Box.BoxType,'s','a','w','b');                                            
            }
            tMuxsamrBox->BoxIndexTime.DataRefIndex= 1;
            tMuxsamrBox->BoxIndexTime.TimeScale= TIME_SCALE;
        }

        if (pInpParam->SpeechAudioFlag == 3)
        {
            //write data for QCELP specific box i.e dqcp box
            tMuxdqcpBox = &p3gpmux->MuxDQCPBox;

            tMuxdqcpBox->Box.BoxSize= 14;
            
            COPY_FOUR_BYTES(tMuxdqcpBox->Box.BoxType,'d','q','c','p');
            
            tMuxdqcpBox->Vendor[0] = 'N';
            tMuxdqcpBox->Vendor[1] = 'V';
            tMuxdqcpBox->Vendor[2] = 'D';
            tMuxdqcpBox->Vendor[3] = 'A';
            tMuxdqcpBox->DecoderVersion = 0;
            tMuxdqcpBox->FramesPerSample = 1;
        }
        else if (pInpParam->SpeechAudioFlag == 4)
        {
            tMuxesdsBox = &p3gpmux->MuxESDSBox[SOUND_TRACK];

            tMuxesdsBox->FullBox.Box.BoxSize= 202;
            
            COPY_FOUR_BYTES(tMuxesdsBox->FullBox.Box.BoxType,'e','s','d','s');        
            
            tMuxesdsBox->FullBox.VersionFlag= 0;
            tMuxesdsBox->ES_DescrTag = 3;
            tMuxesdsBox->DataLen1 = (NvU16)181; //to be made extensible if can't be represented in NvS8
            tMuxesdsBox->ES_ID = (NvU16)0x02;
            tMuxesdsBox->FlagPriority = 0;
            tMuxesdsBox->DecoderConfigDescrTag = 4;
            tMuxesdsBox->DataLen2 = (NvU16)178; //to be made extensible if can't be represented in NvS8
            tMuxesdsBox->ObjectTypeIndication = 0xE1;//E1
            tMuxesdsBox->StreamType = 0x15;
            tMuxesdsBox->DecSpecificInfoTag = 5;
            //Add all to datalen3 sans chunksize, sampling frequency, packet, block and sample-size(s) num-rates, ratemaptable
            tMuxesdsBox->DataLen3 = (NvU16)162; 
            tMuxesdsBox->SLConfigDescrTag = 6;
            tMuxesdsBox->DataLen4 = 1;
            tMuxesdsBox->SLConfigDescrInfo[0] = 0x02;
        }
        else if (pInpParam->SpeechAudioFlag == 2)
        {
            tMuxesdsBox = &p3gpmux->MuxESDSBox[SOUND_TRACK];

            tMuxesdsBox->FullBox.Box.BoxSize= 0x29;   //4 
            
            COPY_FOUR_BYTES(tMuxesdsBox->FullBox.Box.BoxType,'e','s','d','s');        
            
            tMuxesdsBox->FullBox.VersionFlag= 0; 
            tMuxesdsBox->ES_DescrTag = 3; 
            tMuxesdsBox->DataLen1 = (NvU16)0x1B; //1 
            tMuxesdsBox->ES_ID = (NvU16)0; 
            tMuxesdsBox->FlagPriority = 0; 
            tMuxesdsBox->DecoderConfigDescrTag = 4; //1 
            tMuxesdsBox->DataLen2 = (NvU16)0x13; //1 
            tMuxesdsBox->ObjectTypeIndication = 0x40;// 1 byte
            tMuxesdsBox->StreamType = 0x15; //1 
            tMuxesdsBox->BufferSizeDB = 0;  
            tMuxesdsBox->MaxBitrate = pInpParam->AacMaxBitRate; // 4 bytes
            tMuxesdsBox->AvgBitrate = pInpParam->AacAvgBitRate; // 4 bytes
            tMuxesdsBox->DecSpecificInfoTag = 5; //1 
            tMuxesdsBox->DataLen3 = (NvU16)4; //1 
            tMuxesdsBox->DecoderSpecificInfo[0] = 0;
            // Object type is 5 bits
            tMuxesdsBox->DecoderSpecificInfo[0] = pInpParam->AacObjectType << 3; //MSB 5bits are Object type
            // Index is 4 bits
            tMuxesdsBox->DecoderSpecificInfo[0] |= ((pInpParam->AacSamplingFreqIndex >> 1) & 0x07); // LSB 3bits

            tMuxesdsBox->DecoderSpecificInfo[1] = 0;
            tMuxesdsBox->DecoderSpecificInfo[1] = pInpParam->AacSamplingFreqIndex << 7; //MSB bit
            // Channel number is 4 bits
            tMuxesdsBox->DecoderSpecificInfo[1] |= ((pInpParam->AacChannelNumber << 3) & 0x78); //2nd to 5th bits from MSB

            tMuxesdsBox->DecoderSpecificInfo[2] = 0;
            tMuxesdsBox->DecoderSpecificInfo[3] = 0;

            tMuxesdsBox->SLConfigDescrTag = 6; // 1byte
            tMuxesdsBox->DataLen4 = 1;         //1   
            tMuxesdsBox->SLConfigDescrInfo[0] = 0x02; //

            // If SBR is present, then we need to specify additional information in ucDecoderSpecificInfo
            if (pInpParam->AacObjectType == 5)
            {
                // Supported sampling rates for AAC encoding
                NumElements = sizeof(AacSamplingRates) / sizeof(NvU32);
                // Extension sampling rate
                SampleRate = 2 * AacSamplingRates[pInpParam->AacSamplingFreqIndex];
                ObjectType = 2; // AAC-LC

                for (Index = 0; Index < NumElements; ++Index)
                {
                    if (AacSamplingRates[Index] == SampleRate)
                    {
                        // This index will give the Extension Sampling Rate Index
                        break;
                    }
                }
                if (Index == NumElements)
                {
                    //printf(" Invalid sampling rate, taking default value");
                     // Default is 4 corresponding to sampling rate 441000
                    Index = 4;
                }
                // Index is 4 bits
                tMuxesdsBox->DecoderSpecificInfo[1] |= ((Index >> 1) & 0x07); // LSB 3 bits
                tMuxesdsBox->DecoderSpecificInfo[2] = Index << 7; // MSB bit
                // Object type is 5 bits
                tMuxesdsBox->DecoderSpecificInfo[2] |= ((ObjectType << 2) & 0x7C); // 2nd to 6th bits MSB
            }
        }
        else if ( (pInpParam->SpeechAudioFlag == 0) || (pInpParam->SpeechAudioFlag == 1) )
        {
            //write data for amr specific box i.e damr box
            tMuxdamrBox = &p3gpmux->MuxDAMRBox;

            tMuxdamrBox->Box.BoxSize= 17;

            COPY_FOUR_BYTES(tMuxdamrBox->Box.BoxType,'d','a','m','r');        
            
            tMuxdamrBox->Vendor[0] = 'N';
            tMuxdamrBox->Vendor[1] = 'V';
            tMuxdamrBox->Vendor[2] = 'D';
            tMuxdamrBox->Vendor[3] = 'A';
            tMuxdamrBox->DecoderVersion = 0;
            tMuxdamrBox->ModeSet = p3gpmux->SpeechModeDump;
            tMuxdamrBox->ModeChangePeriod = 0;
            tMuxdamrBox->FramesPerSample = 1;
        }
        //write data for sound sample size box i.e stsz box
        tMuxstszBox = &p3gpmux->MuxSTSZBox[SOUND_TRACK];

        tMuxstszBox->FullBox.Box.BoxSize= 20;
        
        COPY_FOUR_BYTES(tMuxstszBox->FullBox.Box.BoxType,'s','t','s','z');        
            
        tMuxstszBox->FullBox.VersionFlag= 0;
        tMuxstszBox->SampleSize = 0;
        tMuxstszBox->SampleCount = 0;//updated at the end
        p3gpmux->SpeechBoxesSize += tMuxstszBox->FullBox.Box.BoxSize;

        if(p3gpmux->Mode == NvMM_Mode32BitAtom)
        {
            //write data for sound chunk offset box i.e stco box
            tMuxstcoBox = &p3gpmux->MuxSTCOBox[SOUND_TRACK];

            tMuxstcoBox->FullBox.Box.BoxSize= 16;

            COPY_FOUR_BYTES(tMuxstcoBox->FullBox.Box.BoxType,'s','t','c','o');        
            
            tMuxstcoBox->FullBox.VersionFlag= 0;
            tMuxstcoBox->ChunkCount = 0;
            p3gpmux->SpeechBoxesSize += tMuxstcoBox->FullBox.Box.BoxSize;
        }
        else
        {
            //write data for sound chunk offset box i.e co64 box
            tMuxco64Box = &p3gpmux->MuxCO64Box[SOUND_TRACK];

            tMuxco64Box->FullBox.Box.BoxSize= 16;
            
            COPY_FOUR_BYTES(tMuxco64Box->FullBox.Box.BoxType,'c','o','6','4');        
            
            tMuxco64Box->FullBox.VersionFlag= 0;
            tMuxco64Box->ChunkCount = 0;
            p3gpmux->SpeechBoxesSize += tMuxco64Box->FullBox.Box.BoxSize;
        }

        //write data for sound decoding time to sample box i.e stts box
        tMuxsttsBox = &p3gpmux->MuxSTTSBox[SOUND_TRACK];
        tMuxsttsBox->FullBox.Box.BoxSize= 16;
        
        COPY_FOUR_BYTES(tMuxsttsBox->FullBox.Box.BoxType,'s','t','t','s');        
        
        tMuxsttsBox->FullBox.VersionFlag= 0;
        tMuxsttsBox->EntryCount = 0;
        p3gpmux->SpeechBoxesSize += tMuxsttsBox->FullBox.Box.BoxSize;

        //write data for sound composition time to sample box i.e ctts box
        tMuxcttsBox = &p3gpmux->MuxCTTSBox[SOUND_TRACK];

        tMuxcttsBox->FullBox.Box.BoxSize= 24;

        COPY_FOUR_BYTES(tMuxcttsBox->FullBox.Box.BoxType,'c','t','t','s');        
        
        tMuxcttsBox->FullBox.VersionFlag= 0;
        tMuxcttsBox->EntryCount = 1;
        tMuxcttsBox->SampleOffset = 0;//each speech frame is of 20ms => 50 frames/sec will get encoded
        p3gpmux->SpeechBoxesSize += tMuxcttsBox->FullBox.Box.BoxSize;
        //write data for sound sample to chunk box i.e stsc box

        tMuxstscBox = &p3gpmux->MuxSTSCBox[SOUND_TRACK];

        tMuxstscBox->FullBox.Box.BoxSize= 16;
        
        COPY_FOUR_BYTES(tMuxstscBox->FullBox.Box.BoxType,'s','t','s','c');        
     
        tMuxstscBox->FullBox.VersionFlag= 0;
        tMuxstscBox->EntryCount = 0;
        p3gpmux->SpeechBoxesSize += tMuxstscBox->FullBox.Box.BoxSize;

        //write data for sound sync sample box i.e stss box
        tMuxstssBox = &p3gpmux->MuxSTSSBox[SOUND_TRACK];

        tMuxstssBox->FullBox.Box.BoxSize= 20;

        COPY_FOUR_BYTES(tMuxstssBox->FullBox.Box.BoxType,'s','t','s','s');        
        
        tMuxstssBox->FullBox.VersionFlag= 0;
        p3gpmux->SpeechBoxesSize += tMuxstssBox->FullBox.Box.BoxSize;
    }

    if(pInpParam->AudioVideoFlag  == NvMM_AV_AUDIO_VIDEO_BOTH)
    {
        tMuxedtsBox = &p3gpmux->MuxEDTSBox[SOUND_TRACK];

        tMuxedtsBox->Box.BoxSize= 8;

        COPY_FOUR_BYTES(tMuxedtsBox->Box.BoxType,'e','d','t','s');
        
        p3gpmux->SpeechBoxesSize += tMuxedtsBox->Box.BoxSize;

        tMuxelstBox = &p3gpmux->MuxELSTBox[SOUND_TRACK];

        tMuxelstBox->FullBox.Box.BoxSize= 16;
        
        COPY_FOUR_BYTES(tMuxelstBox->FullBox.Box.BoxType,'e','l','s','t');      
        
        tMuxelstBox->FullBox.VersionFlag= 1;
        tMuxelstBox->EntryCount = 1;
        p3gpmux->SpeechBoxesSize += tMuxelstBox->FullBox.Box.BoxSize;
    }
}

void WriterDeallocAudio(Nv3gpWriter *p3gpmux, NvMM3GpMuxInputParam *pInpParam)
{
    NvMMFileAVSWriteMsg Message;     
    
    Message.pData = NULL;
    Message.DataSize = 0;
    Message.ThreadShutDown = NV_TRUE;

    if(p3gpmux->MsgQueueAudio)
    {
        NvMMQueueEnQ(p3gpmux->MsgQueueAudio, &Message, 0);
    }
    //Stop the thread
    if(p3gpmux->AudioWriteSema)
    {
        NvOsSemaphoreSignal(p3gpmux->AudioWriteSema);
    }

    if(p3gpmux->AudioWriteDoneSema)
    {
        NvOsSemaphoreWait(p3gpmux->AudioWriteDoneSema);
    }

    if(p3gpmux->FileWriterAudioThread)
    {
        NvOsThreadJoin (p3gpmux->FileWriterAudioThread); 
        p3gpmux->FileWriterAudioThread = NULL;
    }

    if(p3gpmux->AudioWriteSema)
    {
        NvOsSemaphoreDestroy(p3gpmux->AudioWriteSema);
        p3gpmux->AudioWriteSema = NULL;
    }

    if(p3gpmux->AudioWriteDoneSema)
    {
        NvOsSemaphoreDestroy(p3gpmux->AudioWriteDoneSema);   
        p3gpmux->AudioWriteDoneSema = NULL;
    }

    if(p3gpmux->MsgQueueAudio)
    {
        NvMMQueueDestroy(&p3gpmux->MsgQueueAudio); 
        p3gpmux->MsgQueueAudio = NULL;
    }

    if(p3gpmux->MuxELSTBox[SOUND_TRACK].pElstTable)
    {
        NvOsFree(p3gpmux->MuxELSTBox[SOUND_TRACK].pElstTable);
        p3gpmux->MuxELSTBox[SOUND_TRACK].pElstTable = NULL;
    }

    if(p3gpmux->PingAudioBuffer)
    {
        NvOsFree(p3gpmux->PingAudioBuffer);
        p3gpmux->PingAudioBuffer= NULL;
    }

    if(p3gpmux->PongAudioBuffer)
    {
        NvOsFree(p3gpmux->PongAudioBuffer);
        p3gpmux->PongAudioBuffer= NULL;
    }

    if(p3gpmux->FileAudioHandle)
    {
        NvOsFclose(p3gpmux->FileAudioHandle);
        p3gpmux->FileAudioHandle = NULL;
    }

    if(pInpParam->ZeroSpeechDataFlag== 1)
    {
        if(p3gpmux->URIConcatAudio[0])
        {
            NvOsFremove((const char *)p3gpmux->URIConcatAudio[0]);
        }            
    }

    if(p3gpmux->URIConcatAudio[0])
    {
        NvOsFree(p3gpmux->URIConcatAudio[0]);
        p3gpmux->URIConcatAudio[0] = NULL;
    }
}
