/*
 * Copyright (c) 2008-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief        Contains function declaration of the 3GP writer library
 */

#ifndef INCLUDED_CORE_3GPWRITER_H
#define INCLUDED_CORE_3GPWRITER_H

#include "nvmm_queue.h"
#include "nvcommon.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm_writer.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Required for logging information
#define NVLOG_CLIENT NVLOG_MP4_WRITER
// MDAT buffers are 2K aligned. If alignment has to
// be changes then we need to change the following macro
#define OPTIMAL_BUFFER_ALIGN (2048)
#define TOTAL_USER_DATA_BOXES 13
#define MAX_TRACK 2 //maximum no of tracks to be present in the 3GPP file
#define SOUND_TRACK 0
#define VIDEO_TRACK 1
#define MAX_VIDEO_FRAMES (21600*2)
#define MAX_AUDIO_FRAMES (33000*2) //(MAX_VIDEO_FRAMES/30)*50
#define MAX_QUEUE_LEN 2
// To set Optimal Read and Writes size basing on File System performance on different OS
#define OPTIMAL_WRITE_SIZE  (256 * 1024)
#define OPTIMAL_READ_SIZE  (256 * 1024)
//sizeof(STTS Table) + sizeof (STSZ) + sizeof(STSC) + sizeof(CO64) 
#define SIZEOFATOMS 32
// entries per atom  4 atoms STSS (8 bytes), STSZ(4 bytes), STSC (12 bytes),STCO(8 bytes)
#define TOTAL_ENTRIESPERATOM (OPTIMAL_WRITE_SIZE/SIZEOFATOMS)
#define CREATION_TIME 0 //time in year 2003
#define MODIFICATION_TIME 0 //time in year 2003
#define LANGUAGE_CODE 0 //undetermined
#define TIME_SCALE 10000 //time scale is 10000 i.e 10000 parts in 1 sec
#define ACTUAL_ATOM_ENTRIES (8 *4)
#define LITTLE_TO_BIG_ENDIAN(Dest,Src) \
    Dest[0] = (NvU8)(Src>>24); \
    Dest[1] = (NvU8)((Src>>16) & 0xFF); \
    Dest[2] = (NvU8)((Src>>8) & 0xFF); \
    Dest[3] = (NvU8)((Src) & 0xFF); 

#define GET_BOX_NAME(Dest, Boxstruct) \
    Dest[0] = Boxstruct.BoxType[0]; \
    Dest[1] = Boxstruct.BoxType[1];\
    Dest[2] = Boxstruct.BoxType[2];\
    Dest[3] = Boxstruct.BoxType[3];

#define COPY_FOUR_BYTES(p, a1, a2, a3, a4) ((*p) = (NvU8)a1); (*(p +1) = (NvU8)a2); \
 (*(p+2) = (NvU8)a3); (*(p+3) = (NvU8)a4);

#define COPY_TWO_BYTES(p, a1, a2) ((*p) = (NvU8)a1); (*(p +1) = (NvU8)a2);

typedef enum
{
    NvMM_AV_AUDIO_PRESENT = 1,
    NvMM_AV_VIDEO_PRESENT,
    NvMM_AV_AUDIO_VIDEO_BOTH,
    NvMM_AV_Force32 = 0x7FFFFFFF
}NvMMAVPresent;

typedef enum
{
    NvMM_Mode32BitAtom,
    NvMM_Mode64BitAtom,
    NvMM_Mode_Force32 = 0x7FFFFFFF
}NvMM3gpWriterMode;

typedef enum 
{
    NvMMWriterStreamType_OTHER = 0x0,   // Other, not supported tracks
    NvMMWriterStreamType_AUDIO = 0x001, // Audio stream
    NvMMWriterStreamType_VIDEO,    //Video Stream
    NvMMWriterStreamType_Force32 = 0x7FFFFFFF
}NvMMWriterStreamType;

typedef struct NvMM3GpMuxInpUserDataRec
{
    NvU8 usrBoxType[NVMM_MAX_ATOM_SIZE];
    NvU32 sizeOfUserData;
    NvU8 *pUserData;
    NvBool boxPresent;
}NvMM3GpMuxInpUserData;

// Structures
typedef struct NvMM3GpMuxInputParamRec
{
    NvMMWriterStreamType StreamType;
    NvU32 MaxAudioFrames;
    NvU32 MaxVideoFrames;
    NvU32 AudioFrameCount;
    NvU16 VOPWidth;
    NvU16 VOPHeight;
    NvU16 VOPFrameRate;
    NvU16 VideoBitRate;
    NvU16 SpeechModeSet;
    NvU16 SpeechBitRate;
    NvU16 Profile;
    NvU16 Level;
    NvU16 Mp4H263Flag; // 0=MP4 and 1=H263 and 2=AVC
    NvBool OverflowFlag;
    NvU16 ZeroSpeechDataFlag;
    NvU16 ZeroVideoDataFlag;
    NvU16 SpeechAudioFlag; // 0=NBAMR 1=WBAMR 2=AAC 3=QCELP, 4=QCELP with mp4a support
    NvU16 DTXon;
    NvS8 StreamState;
    NvU16 AudioVideoFlag; //indicates presence of audio or video or both in the flag
    //There are 13 supported frequencies (frequency indices 13..14 are invalid): 
    //0: 96000 Hz 
    //1: 88200 Hz 
    //2: 64000 Hz 
    //3: 48000 Hz 
    //4: 44100 Hz 
    //5: 32000 Hz 
    //6: 24000 Hz 
    //7: 22050 Hz 
    //8: 16000 Hz 
    //9: 12000 Hz 
    //10: 11025 Hz 
    //11: 8000 Hz 
    //12: 7350 Hz 
    //15: frequency is written explictly 
    NvU8 AacSamplingFreqIndex;
   //These are the possible object types: 
    //
    //0: NULL 
    //1: AAC Main 
    //2: AAC Low complexity 
    //3: AAC SSR 
    //4: AAC Long term prediction 
    //5: AAC High efficiency 
    //6: Scalable 
    //7: TwinVQ 
    //8: CELP 
    //9: HVXC 
    //10: Reserved 
    //11: Reserved 
    //12: TTSI 
    //13: Main synthetic 
    //14: Wavetable synthesis 
    //15: General MIDI 
    //16: Algorithmic Synthesis and Audio FX 
    //17: AAC Low complexity with error recovery 
    //18: Reserved 
    //19: AAC Long term prediction with error recovery 
    //20: AAC scalable with error recovery 
    //21: TwinVQ with error recovery 
    //22: BSAC with error recovery 
    //23: AAC LD with error recovery 
    //24: CELP with error recovery 
    //25: HXVC with error recovery 
    //26: HILN with error recovery 
    //27: Parametric with error recovery 
    //28: Reserved 
    //29: Reserved 
    //30: Reserved 
    //31: Reserved 
    NvU8 AacObjectType;
    //These are the channel configurations: 
    //0: custom configuration (TODO) 
    //1: 1 channel: front-center 
    //2: 2 channels: front-left, front-right 
    //3: 3 channels: front-center, front-left, front-right 
    //4: 4 channels: front-center, front-left, front-right, back-center 
    //5: 5 channels: front-center, front-left, front-right, back-left, back-right 
    //6: 6 channels: front-center, front-left, front-right, back-left, back-right, LFE-channel 
    //7: 8 channels: front-center, front-left, front-right, side-left, side-right, back-left, back-right, LFE-channel 
    NvU8 AacChannelNumber;
    NvU32 AacAvgBitRate;
    NvU32 AacMaxBitRate;
    NvBool IsSyncFrame;
    NvBool IsAMRIF1;
    NvBool IsAMRIF2;
    NvU32 FrameDuration;
    char *pFilePath;

    char FileExt[MAX_FILE_EXT];
    NvError CpStaus;
    NvMM3GpMuxInpUserData InpUserData[TOTAL_USER_DATA_BOXES];
    NvU32 AudioSampleCount;
    NvU32 VideoSampleCount;
    NvBool bVideoDurationLimit;
    NvBool bAudioDurationLimit;
    NvU64 MaxTrakDurationLimit;
    NvBool bFirstAudioEntry;
    NvU32 AudioCount;
    NvU32 PrevAudioDelta;
    NvBool bFirstVideoEntry;
    NvU32 VideoCount;
    NvU32 PrevVideoDelta;
    NvBool bFirstVideoSTSCEntry;
    NvU32 VideoStcoIndex;
    NvU64 PrevVideoFileOffSet;
    NvU32 VidStscEntries;
    NvU32 AudioStcoIndex;
    NvU64 PrevAudioFileOffSet;
    NvU32 AudStscEntries;
    NvU32 AOffsetStscEntries;
    NvU32 ASttsEntries;
    NvU32 VOffsetStscEntries;
    NvU32 VSttsEntries;
    NvU16 MaxVideoBitRate;
} NvMM3GpMuxInputParam;

typedef struct NvMM3GpAVParamRec
{
    NvU8 *pSpeech;
    NvU8 *pVideo;
    NvU32 SpeechDataSize;
    NvU32 VideoDataSize;
    NvU16 FVideoSyncStatus;
    NvU64 VideoTS;
    NvU64 AudioTS;
}NvMM3GpAVParam;

 typedef struct  NvMM3GpMuxContextRec
{
    NvU32 IntMemSize;
    void *pIntMem;
}NvMM3GpMuxContext;

typedef struct AtomBasicBoxRec
{
    NvU32 BoxSize;
    NvU8 BoxType[4];
}AtomBasicBox;

typedef struct AtomFullBoxRec
{
    AtomBasicBox Box;
    NvU32 VersionFlag;
}AtomFullBox;

//file identification box 
typedef struct NvMM3GpMuxFTYPBoxRec
{
    AtomFullBox FullBox;
    NvS8 Brand[4];
    NvS8 CompBrand[4];
} NvMM3GpMuxFTYPBox;

    //media data box
typedef struct NvMM3GpMuxMDATBoxRec
{
    AtomBasicBox Box;
}NvMM3GpMuxMDATBox;

//moov box i.e movie box or container of meta-data
typedef struct NvMM3GpMuxMOOVBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxMOOVBox;

//movie header box containing overall decleration
typedef struct NvMM3GpMuxMVHDBoxRec
{
    AtomFullBox FullBox;
    NvU32 CreationTime;
    NvU32 ModTime;
    NvU32 TimeScale;
    NvU32 Duration;
    NvU32 NextTrackID;
} NvMM3GpMuxMVHDBox;

//track box i.e container of individual track or stream
typedef struct NvMM3GpMuxTRAKBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxTRAKBox;

typedef struct NvMM3GpMuxEDTSBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxEDTSBox;


typedef struct NvMM3GpMuxELSTTableRec
{
    //table entries for Sample to Chunk atom
    NvU32 SegmentDuration;
    NvS32 MediaTime;
    NvS16 MediaRateInteger;
    NvS16 MediaRateFraction;
}NvMM3GpMuxELSTTable;

typedef struct NvMM3GpMuxELSTBoxRec
{
    AtomFullBox FullBox;
    NvU32 EntryCount;
    NvMM3GpMuxELSTTable *pElstTable;
}NvMM3GpMuxELST;

//track header box
typedef struct NvMM3GpMuxTKHDBoxRec
{
    AtomFullBox FullBox;
    NvU32 CreationTime;
    NvU32 ModTime;
    NvU32 TrackID;
    NvU32 Duration;
}NvMM3GpMuxTKHD;

//media box
typedef struct NvMM3GpMuxMDIABox
{
    AtomBasicBox Box;
} NvMM3GpMuxMDIA;

//media header box
typedef struct  NvMM3GpMuxMDHDBoxRec
{
    AtomFullBox FullBox;
    NvU32 CreationTime;
    NvU32 ModTime;
    NvU32 TimeScale;
    NvU32 Duration;
    NvU16 Language;
} NvMM3GpMuxMDHDBox;

//handler reference box
typedef struct NvMM3GpMuxHDLRBoxRec
{
    AtomFullBox FullBox;
    NvS8 HandlerType[4];
    NvS8 StringName[20];
} NvMM3GpMuxHDLRBox;


//media information box
typedef struct NvMM3GpMuxMINFBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxMINFBox;


//sound media information header box
typedef struct NvMM3GpMuxSMHDBoxRec
{
    AtomFullBox FullBox;
} NvMM3GpMuxSMHDBox;

//video media information header box
typedef struct NvMM3GpMuxVMHDBoxRec
{
    AtomFullBox FullBox;
} NvMM3GpMuxVMHDBox;


//data information box
typedef struct NvMM3GpMuxDINFBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxDINFBox;

//data reference box
typedef struct NvMM3GpMuxDREFBoxRec
{
    AtomFullBox FullBox;
    NvU32 EntryCount;
    NvU32 EntryVersionFlag;
} NvMM3GpMuxDREFBox;


//sample table box
typedef struct NvMM3GpMuxSTBLBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxSTBLBox;


//sample description box 
typedef struct NvMM3GpMuxSTSDBoxRec
{
    AtomFullBox FullBox;
    NvU32 EntryCount;
} NvMM3GpMuxSTSDBox;

typedef struct AtomWidthHeightBoxRec
{
    AtomBasicBox Box;
    NvU16 Width;
    NvU16 Height;
}AtomWidthHeightBox;

//H263 sample entry box
typedef struct NvMM3GpMuxS263BoxRec
{
    AtomWidthHeightBox BoxWidthHeight;
    NvU16 usDataRefIndex;
} NvMM3GpMuxS263Box;

//mp4v sample entry box
typedef struct NvMM3GpMuxMP4VBoxRec
{
    AtomWidthHeightBox BoxWidthHeight;
    NvU16 DataRefIndex;
} NvMM3GpMuxMP4VBox;

//avc sample entry box 
typedef struct NvMM3GpMuxAVC1BoxRec
{
    AtomWidthHeightBox BoxWidthHeight;
    NvU16 DataRefIndex;
} NvMM3GpMuxAVC1Box;

typedef struct AtomRefindexTimeScaleBoxRec
{
    AtomBasicBox Box;
    NvU16 DataRefIndex;
    NvU32 TimeScale;
}AtomRefindexTimeScale;

//amr sample entry box
typedef struct NvMM3GpMuxSAMRBoxRec
{
    AtomRefindexTimeScale BoxIndexTime;
} NvMM3GpMuxSAMRBox;

//mp4a sample entry box
typedef struct NvMM3GpMuxMP4ABoxRec
{
    AtomRefindexTimeScale BoxIndexTime;
} NvMM3GpMuxMP4ABox;

//QCELP sample entry box
typedef struct NvMM3GpMuxSQCPBoxRec
{
    AtomRefindexTimeScale BoxIndexTime;
} NvMM3GpMuxSQCPBox;

//AMR specific box
typedef struct NvMM3GpMuxDAMRBoxRec
{
    AtomBasicBox Box;
    NvS8 Vendor[4];
    NvS8 DecoderVersion;
    NvU16 ModeSet;
    NvS8 ModeChangePeriod;
    NvS8 FramesPerSample;
} NvMM3GpMuxDAMRBox;


//QCELP specific box
typedef struct NvMM3GpMuxDQCPBox
{
    AtomBasicBox Box;
    NvS8 Vendor[4];
    NvS8 DecoderVersion;
    NvS8 FramesPerSample;
} NvMM3GpMuxDQCP;


//mp4 elementary stream descriptor box
typedef struct NvMM3GpMuxESDSBoxRec
{
    AtomFullBox FullBox;
    NvS8 ES_DescrTag;
    NvU16 DataLen1;
    NvU16 ES_ID;
    NvS8 FlagPriority;
    NvS8 DecoderConfigDescrTag;
    NvU16 DataLen2;
    NvU8 ObjectTypeIndication;
    NvS8 StreamType;
    NvU32 BufferSizeDB;
    NvU32 MaxBitrate;
    NvU32 AvgBitrate;
    NvS8 DecSpecificInfoTag;
    NvU16 DataLen3;
    NvS8 DecoderSpecificInfo[162];
    NvS8 SLConfigDescrTag;
    NvS8 DataLen4;
    NvU8 SLConfigDescrInfo[16];
} NvMM3GpMuxESDSBox;

//sequence parameter set box
typedef struct NvMM3GpMuxTSPSTableRec
{
    NvU16 SequenceParameterSetLength;  
    NvU8 SequenceParameterSetNALUnit[12];
}NvMM3GpMuxTSPSTable;

//picture parameter set box
typedef struct NvMM3GpMuxPPSTableRec
{
    NvU16 PictureParameterSetLength;
    NvU8 PictureParameterSetNALUnit[8];
}NvMM3GpMuxPPSTable;

//avcC box 
typedef struct NvMM3GpMuxAVCCBoxRec
{
    AtomBasicBox Box;
    NvS8  ConfigurationVersion; 
    NvU8 AVCProfileIndication;
    NvU8 Profile_compatibility;
    NvU8 AVCLevelIndication;
    NvU16 LengthSizeMinusOne;
    NvU32 NumOfSequenceParameterSets;
    NvMM3GpMuxTSPSTable *pPspsTable;
    NvU8 NumOfPictureParameterSets;
    NvMM3GpMuxPPSTable *pPpsTable;
} NvMM3GpMuxAVCCBox;

//H263 specific box
typedef struct NvMM3GpMuxD263BoxRec
{
    AtomBasicBox Box;
    NvS8 Vendor[4];
    NvS8 DecoderVersion;
    NvS8 H263_Level;
    NvS8 H263_Profile;
} NvMM3GpMuxD263Box;

//sample size box
typedef struct NvMM3GpMuxSTSZBoxRec
{
    AtomFullBox FullBox;
    NvU32 SampleSize;
    NvU32 SampleCount;
    NvU32 *pSizeEntry;
} NvMM3GpMuxSTSZBox;

//chunk offset box 64-bit offsets
typedef struct NvMM3GpMuxCO64BoxRec
{
    AtomFullBox FullBox;
    NvU32 ChunkCount;
    NvU64 *pChunkOffset;
} NvMM3GpMuxCO64Box;

//chunk offset box 32-bit offset
typedef struct NvMM3GpMuxSTCOBoxRec
{
    AtomFullBox FullBox;
    NvU32 ChunkCount;
    NvU32 *pChunkOffset;
} NvMM3GpMuxSTCOBox;

//Decoding time to sample box
typedef struct NvMM3GpMuxSTTSTableRec
{
    NvU32 SampleCount;  
    NvU32 SampleDelta;
}NvMM3GpMuxSTTSTable;

//Decoding time to sample box
typedef struct NvMM3GpMuxSTTSBoxRec
{
    AtomFullBox FullBox;
    NvU32 EntryCount; //only 1 entry will be kept
    NvMM3GpMuxSTTSTable *pSttsTable;
} NvMM3GpMuxSTTSBox;

//Composition time to sample box
typedef struct NvMM3GpMuxCTTSBoxRec
{
    AtomFullBox FullBox;
    NvU32 EntryCount;
    NvU32 SampleCount;
    NvU32 SampleOffset;
} NvMM3GpMuxCTTSBox;

//sample to chunk box
//different samples for different chunks. The table below keeps a run of such chunks
typedef struct NvMM3GpMuxSTSCTableRec
{
    //table entries for Sample to Chunk atom
    NvU32 FirstChunk;
    NvU32 SamplesPerChunk; 
    NvU32 SampleDescriptionIndex;
}NvMM3GpMuxSTSCTable;

typedef struct NvMM3GpMuxSTSCBoxRec
{
    AtomFullBox FullBox;
    NvU32 EntryCount;
    NvMM3GpMuxSTSCTable *pStscTable;
} NvMM3GpMuxSTSCBox;

//Sync sample box
typedef struct NvMM3GpMuxSTSSBoxRec
{
    AtomFullBox FullBox;
    NvU32 EntryCount;
    NvU32 *pSampleNumber;
} NvMM3GpMuxSTSSBox;

// User Data - 26244-900 doc - 3 gp spec
typedef struct NvMM3GpMuxUDTABoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxUDTABox;

// User Data - Title
typedef struct NvMM3GpMuxTITLEBoxRec
{
    AtomFullBox FullBox;
    NvU16 Language; // pad - 1bit, langugage - 15 bits
    NvU8  *pTitle;
} NvMM3GpMuxTITLEBox;


// User Data - Description
typedef struct NvMM3GpMuxDSCPBoxRec
{
    AtomFullBox FullBox;
    NvU16 Language; // pad - 1 bit, language - 15 bits
    NvU8  *pDescription;   
} NvMM3GpMuxDSCPBox;

//   User Data - copyright
typedef struct NvMM3GpMuxCPRTBoxRec
{
    AtomFullBox FullBox;
    NvU16 Language; // pad 1-bit, language - 15 bits
    NvU8  *pCopyright;
} NvMM3GpMuxCPRTBox;

// User Data - Performer
typedef struct NvMM3GpMuxPERFBoxRec
{
    AtomFullBox FullBox;
    NvU16 Language; // pad 1-bit, language - 15 bits
    NvU8  *pPerformer;
} NvMM3GpMuxPERFBox;


// User Data - Author
typedef struct NvMM3GpMuxAUTHBoxRec
{
    AtomFullBox FullBox;
    NvU16 Language; // pad 1-bit, language - 15 bits
    NvU8  *pAuthor;
} NvMM3GpMuxAUTHBox;

// User Data - Genre
typedef struct NvMM3GpMuxGNREBoxRec
{
    AtomFullBox FullBox;
    NvU16 Language;// pad 1-bit, language - 15 bits
    NvU8  *pGenre;
} NvMM3GpMuxGNREBox;

// User Data - Rating
typedef struct NvMM3GpMuxRTNGBoxRec
{
    AtomFullBox FullBox;
    NvU32 RatingEntity; // http://www.movie-ratings.net/, 
    NvU32 RatingCriteria;
    NvU16 Language; // pad 1-bit, language - 15 bits
    NvU8  *pRatingInfo;
} NvMM3GpMuxRTNGBox;

// User Data - classification
typedef struct NvMM3GpMuxCLSFBoxRec
{
    AtomFullBox FullBox;
    NvU32 ClassificationEntity;// http://www.movie-ratings.net/, 
    NvU16 ClassificationTable;
    NvU16 Language; // pad 1-bit, language - 15 bits
    NvU8  *pClassificationInfo;   
} NvMM3GpMuxCLSFBox;

// User Data - KeyWords
typedef struct KeywordStruct
{
    NvU8 KeyWordSize;
    NvU8 *pKeyWordInfo;
}structKeyWord;

typedef struct NvMM3GpMuxKYWDBoxRec
{
    AtomFullBox FullBox;
    NvU16 Language;      // pad 1-bit, language - 15 bits
    NvU8 KeywordCntr;
    structKeyWord *keyword; // no of keywords will be basing on keywordCntr
} NvMM3GpMuxKYWDBox;

// User Data - location
typedef struct NvMM3GpMuxLOCIBoxRec
{
    AtomFullBox FullBox;
    NvU16 usLanguage;      // pad 1-bit, language - 15 bits
    NvU8 Name[100];
    NvU8 Role;
    NvU32 Longitude;
    NvU32 Latitude;
    NvU32 Altitude;
    NvU8 Astronomical_body[100];
    NvU8 Additional_notes[100];
} NvMM3GpMuxLOCIBox;

// User Data - Album
typedef struct NvMM3GpMuxALBMBoxRec
{
    AtomFullBox FullBox;
    NvU16 Language;      // pad 1-bit, language - 15 bits
    NvU8 *pAlbumtitle;
    NvU8 TrackNumber;
} NvMM3GpMuxALBMBox;


// User Data - Recording Year
typedef struct NvMM3GpMuxYRRCBoxRec
{
    AtomFullBox FullBox;
    NvU16 RecordingYear;
} NvMM3GpMuxYRRCBox;

// User Data - user defined box type
typedef struct NvMM3GpMuxUSERBoxRec
{
    AtomBasicBox Box;
    NvU8 *pUserData;
} NvMM3GpMuxUSERBox;

typedef struct  NvMM3GpMuxMVEXBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxMVEXBox;

typedef struct  NvMM3GpMuxTREXBoxRec
{
    AtomFullBox FullBox;
    NvU32 TrackID;
    NvU32 DefSampleDescrIndex;
    NvU32 DefSampleDuration;
    NvU32 DefSampleSize;
    NvU32 DefSampleFlags;
    //12:reserved(0), 3:SamplePaddingVal, 1:SampleIsDiffSample, 16:SampleDegradationPriority
} NvMM3GpMuxTREXBox;

typedef struct NvMM3GpMuxMOOFBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxMOOFBox;

typedef struct NvMM3GpMuxMFHDBoxRec
{
    AtomFullBox FullBox;
    NvU32 SequenceNum;
} NvMM3GpMuxMFHDBox;

typedef struct NvMM3GpMuxTRAFBoxRec
{
    AtomBasicBox Box;
} NvMM3GpMuxTRAFBox;

typedef struct NvMM3GpMuxTFHDBoxRec
{
    AtomFullBox FullBox;
    NvU32 TrackID;
    //optional fields : tf_flags set to 0x000001
    NvU64 BaseDataOffset; //Check if this is present: typedef unsigned __int64  uint64
} NvMM3GpMuxTFHDBox;

typedef struct NvMM3GpMuxTRUNVBoxRec
{
    AtomFullBox FullBox;
    NvU32 SampleCount;
    NvS32 DataOffset;
    NvU32 SampleDuration;
    NvU32 SampleSize;
    NvU32 SampleFlags;
} NvMM3GpMuxTRUNVBox;

typedef struct  NvMM3GpMuxTRUNSBoxRec
{
    AtomFullBox FullBox;
    NvU32 SampleCount;
    NvS32 DataOffset;
    NvU32 SampleSize;//Actually an array of sample-sizes.
} NvMM3GpMuxTRUNSBox;

//structures for moov and moof
typedef struct NvMM3GpMuxMoovRec
{
    NvU32 SeekPos;
    NvU32 Data;
} NvMM3GpMuxMoov;

typedef struct NvMM3GpMuxMoofRec
{
    NvU64 SeekPos;
    NvU32 Data;
} NvMM3GpMuxMoof;

typedef struct NvMM3GpMuxSplitModeRec
{
    NvU8 *ReInitBuffer;
    NvBool SplitFlag;
    NvBool ResetAudioTS;
    NvU64 TsVideoOffset;
    NvU64 TsAudioOffset;
} NvMM3GpMuxSplitMode;

typedef struct NvMMFileAVSWriteMsgRec
{
    NvU8 *pData;
    NvU32 DataSize ;
    NvBool ThreadShutDown;
    NvU8 LoopCntr;
} NvMMFileAVSWriteMsg;


//global data structure for 3GPP MUX
typedef struct Nv3gpMux
{
    NvU8 *pSpeech;
    NvU8 *pVideo;
    NvU16 SpeechModeSet;
    NvU16 SpeechModeDump;
    NvS8   PrevRateFlag;
    NvU8  RateMapTable[16];
    NvU32 NumRates;
    NvU32 SpeechSampleSize;
    NvU32 SpeechBuffDataSize;
    NvU32 Max_Samples_Per_Frame;
    NvU32 VideoBuffDataSize;
    NvU64 FileOffset;
    NvU32 FileOffset32;        
    NvU64 FileSize;
    NvU32 SpeechBoxesSize;
    NvU32 VideoBoxesSize;
    NvU32 VideoFrameCnt;
    NvU32 SpeechFrameCnt;
    NvU16 VOPWidth;
    NvU16 VOPHeight;
    NvU16 FrameRate;
    NvU16 VideoBitRate;
    NvU16 VARrate;
    NvU8 *pTempBuff;
    NvU32 *pTrunSampleSizeArray;
    NvU32 *pTrunSBuf;
    NvU32 *pTrunVBuf;
    NvU32 TrunSOffset;
    NvU32 TrunVOffset;
    NvU16 DTXon;
    NvU16 InsertTitle;
    NvU16 InsertAuthor;
    NvU16 InsertDescription;
    NvU16 FVideoSyncStatus;
    NvU16 FVideoDataToBeDumped;
    NvU16 FAudioDataToBeDumped;
    NvS8  AudInitFlag;
    NvS8 VidInitFlag;
    NvS8 SCloseMuxFlag;
    NvBool KeyFrame;
    NvU64 PrevVideoTS;
    NvU64 PrevAudioTS;
    NvU64 CurrVideoTS;
    NvU64 CurrAudioTS;
    NvU64 FirstVideoTS;
    NvU64 FirstAudioTS;
      //Added for 3g2 only
    NvS32 Cnt;
    NvS32 MoovCount;
    NvS32 MoofCount;
    NvS32 MoovChkComplete;
    NvS32 MoofChkComplete;
    NvMM3GpMuxMoov SMoov[100];
    NvMM3GpMuxMoof SMoof[100];
    NvMM3GpMuxFTYPBox MuxFTYPBox;
    NvMM3GpMuxMDATBox MuxMDATBox;
    NvMM3GpMuxMOOVBox  MuxMOOVBox;
    NvMM3GpMuxMVHDBox MuxMVHDBox;
    NvMM3GpMuxTRAKBox MuxTRAKBox[MAX_TRACK];
    NvMM3GpMuxEDTSBox MuxEDTSBox[MAX_TRACK];
    NvMM3GpMuxELST MuxELSTBox[MAX_TRACK];
    NvMM3GpMuxTKHD MuxTKHDBox[MAX_TRACK];
    NvMM3GpMuxMDIA MuxMDIABox[MAX_TRACK];
    NvMM3GpMuxMDHDBox MuxMDHDBox[MAX_TRACK];
    NvMM3GpMuxHDLRBox MuxHDLRBox[MAX_TRACK];
    NvMM3GpMuxMINFBox MuxMINFBox[MAX_TRACK];
    NvMM3GpMuxSMHDBox MuxSMHDBox;
    NvMM3GpMuxVMHDBox MuxVMHDBox;
    NvMM3GpMuxDINFBox MuxDINFBox[MAX_TRACK];
    NvMM3GpMuxDREFBox MuxDREFBox[MAX_TRACK];
    NvMM3GpMuxSTBLBox MuxSTBLBox[MAX_TRACK];
    NvMM3GpMuxSTSDBox MuxSTSDBox[MAX_TRACK];
    NvMM3GpMuxMP4ABox MuxMP4ABox;
    NvMM3GpMuxSAMRBox MuxSAMRBox;
    NvMM3GpMuxSQCPBox MuxSQCPBox;
    NvMM3GpMuxMP4VBox MuxMP4VBox;
    NvMM3GpMuxAVC1Box MuxAVC1Box;
    NvMM3GpMuxAVCCBox MuxAVCCBox;
    NvMM3GpMuxS263Box MuxS263Box;
    NvMM3GpMuxESDSBox MuxESDSBox[MAX_TRACK];
    NvMM3GpMuxDAMRBox MuxDAMRBox;
    NvMM3GpMuxDQCP MuxDQCPBox; 
    NvMM3GpMuxD263Box MuxD263Box;
    NvMM3GpMuxSTSZBox MuxSTSZBox[MAX_TRACK];
    NvMM3GpMuxCO64Box MuxCO64Box[MAX_TRACK];
    NvMM3GpMuxSTCOBox MuxSTCOBox[MAX_TRACK];
    NvMM3GpMuxSTTSBox MuxSTTSBox[MAX_TRACK];
    NvMM3GpMuxCTTSBox MuxCTTSBox[MAX_TRACK];
    NvMM3GpMuxSTSCBox MuxSTSCBox[MAX_TRACK];
    NvMM3GpMuxSTSSBox MuxSTSSBox[MAX_TRACK];
    NvMM3GpMuxUDTABox MuxUDTABox;
    NvMM3GpMuxTITLEBox MuxTITLEBox;
    NvMM3GpMuxDSCPBox MuxDSCPBox;
    NvMM3GpMuxAUTHBox MuxAUTHBox;
    NvMM3GpMuxALBMBox MuxALBMBox;
    NvMM3GpMuxCPRTBox MuxCPRTBox;
    NvMM3GpMuxPERFBox MuxPERFBox;
    NvMM3GpMuxGNREBox MuxGNREBox;
    NvMM3GpMuxRTNGBox MuxRTNGBox;
    NvMM3GpMuxCLSFBox MuxCLSFBox;
    NvMM3GpMuxKYWDBox MuxKYWDBox;
    NvMM3GpMuxYRRCBox MuxYRRCBox;
    NvMM3GpMuxUSERBox MuxUSERBox;
    NvMM3GpMuxMVEXBox MuxMVEXBox;
    NvMM3GpMuxMOOFBox MuxMOOFBox;
    NvMM3GpMuxMFHDBox MuxMFHDBox;
    NvMM3GpMuxLOCIBox MuxLOCIBox;
    NvMM3GpMuxTRAFBox MuxTRAFBox[MAX_TRACK];
    NvMM3GpMuxTREXBox MuxtTREXBox[MAX_TRACK];
    NvMM3GpMuxTFHDBox MuxTFHDBox[MAX_TRACK];
    NvMM3GpMuxTRUNVBox MuxTRUNVBox;
    NvMM3GpMuxTRUNSBox MuxTRUNSBox;
    NvOsFileHandle FileAudioHandle; // STTS, STSZ, STSC,CO64 for audio
    NvU8 * PingAudioBuffer;
    NvU8 * PongAudioBuffer;
    NvU8 *pPAudioBuffer;
    NvU32 AudioCount;
    NvOsSemaphoreHandle AudioWriteSema;
    NvOsSemaphoreHandle AudioWriteDoneSema;
    NvMMQueueHandle  MsgQueueAudio;
    NvOsThreadHandle FileWriterAudioThread;
    NvBool AudioWriteSemaInitialized;
    NvOsFileHandle  FileVideoHandle; // STTS, STSZ, STSC,CO64 for video
    NvU8 * PingVideoBuffer;
    NvU8 * PongVideoBuffer;
    NvU8 *pPVideoBuffer;
    NvU32 VideoCount;
    NvOsSemaphoreHandle VideoWriteSema;
    NvOsSemaphoreHandle VideoWriteDoneSema;
    NvMMQueueHandle MsgQueueVideo;
    NvOsThreadHandle FileWriterVideoThread;
    NvBool VideoWriteSemaInitialized;
    NvOsFileHandle FileSyncHandle; // STSS for video
    NvU8 * PingSyncBuffer;
    NvU8 * PongSyncBuffer;
    NvU8 *pPSyncBuffer;
    NvU32 SyncCount;
    NvU32 SpeechOffset;
    NvU32 ConsumedDataSize;
    NvU8 *pPResvBuffer;
    NvU8 *pResvRunBuffer;
    NvBool ResvFileWriteFailed;
    NvError ResvThreadStatus;
    NvBool ResvFlag;
    NvOsFileHandle  FileResvHandle; //temp file
    NvOsSemaphoreHandle ResvWriteSema;
    NvOsSemaphoreHandle ResvWriteDoneSema;
    NvMMQueueHandle MsgQueueResv;
    NvOsThreadHandle FileWriterResvThread;
    char *URIConcatResv[1];
    char *URIConcatAudio[1];
    char *URIConcatVideo[1];
    char *URIConcatSync[1]; 
    NvU32 Mode;
    NvU64 HeadSize; 
    NvU64 GraceSize;
    NvU32 AccumEntries;
    NvS32 AccumFracTS;
    NvS32 AccumFracAudioTS;
    NvBool VideoWriteFailed;
    NvError AudioWriteStatus;
    NvBool AudioWriteFailed;
    NvError VideoWriteStatus;
    NvMM3GpMuxContext *Context3GpMux;
    NvU16 MaxVideoBitRate;
 }  Nv3gpWriter;

    // Function Prototypes
    // Video prototypes

NvError
NvMM3GpMuxRec_3gp2WriteInit(
    NvMM3GpMuxInputParam *pstructInpParam,
    NvU8 *pBufferOut, 
    NvU32 *Offset,
    void *StructMem,
    char *szuri,
    char *szuritemppath,
    NvBool mode64To32);

NvError
NvMM3GpMuxRecWriteRun(
    NvMM3GpMuxSplitMode *pSpMode,
    NvMM3GpAVParam *pstructSVParam,
    NvMM3GpMuxInputParam *pstructInpParam,
    NvU8 *pBufferOut,
    NvU32 *Offset,
    NvMM3GpMuxContext Context3GpMux);

NvError
NvMM3GpMuxRecWriteClose(
    NvMM3GpMuxInputParam *pstructInpParam,
    CPhandle cphandle,
    CP_PIPETYPE *pPipe,
    NvMM3GpMuxContext Context3GpMux);

NvError
NvMM3GpMuxRecDelReservedFile(
    NvMM3GpMuxContext Context3GpMux);


NvError
NvMM3GpMuxMuxWriteDataFromBuffer(
    NvU8 *pBuffer,
    NvU32 BytesToBeWritten, 
    CP_PIPETYPE *pPipe,
    CPhandle Cphandle);

void
NvMM3GpMuxWriterStrcat(
    char **c3,
    char *c1,
    NvU32 length1,
    const char *c2,
    NvU32 length2);

NvError
VideoInit(
    Nv3gpWriter *p3gpmux,
    NvU32 length1,
    NvU32 length2,
    NvMM3GpMuxInputParam *pInpParam);

void
InitVideoTrakAtom(
    Nv3gpWriter *p3gpmux,
    NvMM3GpMuxInputParam *pInpParam);

void
WriterDeallocVideo(
    Nv3gpWriter *p3gpmux,
    NvMM3GpMuxInputParam *pInpParam);

// Audio prototypes
NvError
AudioInit(
    Nv3gpWriter *p3gpmux,
    NvU32 length1,
    NvU32 length2,
    NvMM3GpMuxInputParam *pInpParam);

void
InitAudioTrakAtom(
    Nv3gpWriter *p3gpmux,
    NvMM3GpMuxInputParam *pInpParam);

void
WriterDeallocAudio(
    Nv3gpWriter *p3gpmux,
    NvMM3GpMuxInputParam *pInpParam);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_CORE_3GPWRITER_H

