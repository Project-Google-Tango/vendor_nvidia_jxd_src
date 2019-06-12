/* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto.  Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an 
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

/* This file is an interface to implement various meta file parsers (for example: m3u, pls meta file parsers).
* This interface is used by meta file extractor util to parse the meta files given by the client. 
* The file has interfaces defined to get file paths, file names and different meta data information of
* each media file entry present in the meta file.
*/


#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvos.h"
#include "nvmetafile_m3uparser.h"
#include "nvmm_sock.h"
typedef struct M3uparser
{
    NvU32 Maxfilenum;
    NvU8* M3udata;//data pointer
    NvU64 M3udatasize;
    NvU64* FileStartOffsets;
    NvU64* FileEndOffsets;
}M3uparser;


static NvError GetOffsets(M3uparser* parsercontext);
static NvError GetFileCount(M3uparser* parsercontext); 

NvError M3uParserOpen(NvU8* filename, 
                      NvMetaFileParserHandle *handle, 
                      NvU32* nTotalFileCount)
{
    NvError Status = NvSuccess;
    NvOsFileHandle  hInputFile;
    size_t bytesRead;
    NvU64  filesize=0;
    M3uparser * M3uparsercontext=NULL;

    M3uparsercontext= (M3uparser *)NvOsAlloc(sizeof(M3uparser));
    if (!M3uparsercontext)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(M3uparsercontext, 0, sizeof(M3uparser));


    NvOsDebugPrintf("input file %s ",filename);

    if ( (NvOsStrlen((char*)filename) > 5) &&  !NvOsStrncmp((char*)filename, "http:",5))
    {  
        char* fileBuffer=NULL;
        NvS64 fileLen = 0;
    
        Status = NvMMSockGetHTTPFile((char*)filename, &fileBuffer, &fileLen);
        if (NvSuccess != Status || fileLen <= 0)
        {
            NvOsDebugPrintf("Failed to find URL %s\n", filename);
            goto FAIL;
        }

        M3uparsercontext->M3udata=(NvU8*)fileBuffer;
        M3uparsercontext->M3udatasize=(NvU64)fileLen;

    }

    else
    {
        Status =  NvOsFopen((const char *)filename, NVOS_OPEN_READ, &hInputFile);
        if (hInputFile == NULL)
        {
            NvOsDebugPrintf("Input file open fails\n");
            goto FAIL;
        }
        //Parse M3u to get number of files in playlist

        NvOsFseek(hInputFile,0,NvOsSeek_End);
        NvOsFtell(hInputFile , &filesize);
        NvOsFseek(hInputFile,0, NvOsSeek_Set);

        // Alloc the memory for M3u Content
        M3uparsercontext->M3udata = (NvU8*)NvOsAlloc((size_t)filesize);
        if(!M3uparsercontext->M3udata)
        {
            NvOsDebugPrintf("memory allocation failed");
            Status =  NvError_InsufficientMemory;
            goto FAIL;
        }
        M3uparsercontext->M3udatasize=filesize;
        // Read M3u Content and Parse
        Status =  NvOsFread(hInputFile,M3uparsercontext->M3udata,(NvU32) filesize, &bytesRead);
        NvOsFclose(hInputFile);
    }

    Status = GetFileCount(M3uparsercontext); 
    if(Status != NvSuccess )
    {
        NvOsDebugPrintf("SYNTAX FAILED***********");
        goto FAIL;
    }
    M3uparsercontext->FileStartOffsets=NvOsAlloc(sizeof(NvU64)*(M3uparsercontext->Maxfilenum+1));
    M3uparsercontext->FileEndOffsets=NvOsAlloc( sizeof(NvU64)*(M3uparsercontext->Maxfilenum+1));
    if( !M3uparsercontext->FileStartOffsets || !M3uparsercontext->FileEndOffsets)
    {
        Status =  NvError_InsufficientMemory;
        goto FAIL;
    }

    NvOsMemset(M3uparsercontext->FileStartOffsets, 0, sizeof(NvU64)*(M3uparsercontext->Maxfilenum+1));
    NvOsMemset(M3uparsercontext->FileEndOffsets, 0, sizeof(NvU64)*(M3uparsercontext->Maxfilenum+1));
    //get all entry element offsets
    Status =GetOffsets(M3uparsercontext);
    if(Status != NvSuccess )
    {
        NvOsDebugPrintf("SYNTAX FAILED***********");
        goto FAIL;
    }

    *nTotalFileCount=M3uparsercontext->Maxfilenum ;
    *handle=(NvMetaFileParserHandle *)M3uparsercontext;

    return Status;
FAIL:

    NvOsFree(M3uparsercontext->FileEndOffsets);
    NvOsFree(M3uparsercontext->FileStartOffsets);
    NvOsFree( M3uparsercontext->M3udata);
    NvOsFree(M3uparsercontext);
    return Status;
}

static NvError GetFileCount(M3uparser* context)
{ 
    NvError status=NvSuccess;
    M3uparser* parsercontext=(M3uparser*)context; 
    NvU32 filecount=0;
    NvU8* bufferrun;
    NvU8* bufferrunend;
    bufferrun = parsercontext->M3udata;
    bufferrunend = bufferrun+parsercontext->M3udatasize;

    for(;bufferrun<bufferrunend;)
    {
        if(*bufferrun  == '#')  //Consume Comment
        {  
            while(*bufferrun++ != '\n' && bufferrun < bufferrunend);

        }
        else if(*bufferrun != ' ' &&  *bufferrun != '#')  // Consume data
        {
            filecount++;
            while(*bufferrun++ != '\n' && bufferrun < bufferrunend);

        }
        else if(*bufferrun == ' ') // Consume blank line
        {
            while(*bufferrun++ != '\n' && bufferrun < bufferrunend);

        }

    } 


    parsercontext->Maxfilenum = filecount;

    return status;
}

static NvError GetOffsets(M3uparser *context)
{
    NvError status=NvSuccess;
    M3uparser* parsercontext=(M3uparser*)context; 
    NvU32 i;
    NvU32 filecount=0;
    NvU8* bufferrun;
    NvU8* bufferrunend;
    NvU8* bufferstart;
    bufferstart = parsercontext->M3udata;
    bufferrun = parsercontext->M3udata;
    bufferrunend = bufferrun+parsercontext->M3udatasize;
    for( ;bufferrun<bufferrunend;)
    {

        if(*bufferrun  == '#')  //Consume Comment
        {  
            while(*bufferrun++ != '\n' && bufferrun < bufferrunend);

        }
        else if(*bufferrun != ' ' &&  *bufferrun != '#')  // Consume data
        {
            filecount++;
            parsercontext->FileStartOffsets[filecount]=bufferrun-bufferstart;
            while(*bufferrun++ != '\n' && bufferrun < bufferrunend);
            parsercontext->FileEndOffsets[filecount]=bufferrun-bufferstart-2; // this '2' accounts for '\r' and '\n'

        }
        else if(*bufferrun == ' ') // Consume blank line
        {
            while(*bufferrun++ != '\n' && bufferrun < bufferrunend);

        }

    }

    parsercontext->FileStartOffsets[0]=0;  //For Global Playlist Data
    parsercontext->FileEndOffsets[0]=0;


    for(i=0;i<=parsercontext->Maxfilenum;i++)
        NvOsDebugPrintf("Entry element  %d offsets %llu %llu ",i,parsercontext->FileStartOffsets[i],parsercontext->FileEndOffsets[i]);
    return status;
}



NvError M3uFileInfo(NvMetaFileParserHandle handle, 
                    NvU32 nIndex, 
                    NvU32 nfilesToRead, 
                    NvFileMetaData **fileInfoBuffer)
{
    NvError status = NvSuccess;
    M3uparser* parsercontext=(M3uparser*)handle; 
    NvU8* buffer;
    NvU8* bufferrun;
    NvU8* bufferrunend;
    NvFileMetaData *metadatarun;
    NvU32 i,filesread=0;
    NvU32 j;
    buffer=parsercontext->M3udata;


    for(i=nIndex;i<(nIndex+nfilesToRead);i++)
    {   
        metadatarun=(*fileInfoBuffer)+filesread;
        for(j=0;j<NvMetaDataInfo_Max;j++)
        {
            if(metadatarun->length[j] != 0)
                metadatarun->length[j]=0;

        }
        NvOsDebugPrintf("Entry element  %d offsets %llu %llu ",i,parsercontext->FileStartOffsets[i],parsercontext->FileEndOffsets[i]);

        bufferrun = buffer+parsercontext->FileStartOffsets[i];
        bufferrunend = buffer+parsercontext->FileEndOffsets[i];

        for( ;bufferrun<bufferrunend;)
        {
            NvFileMetaData *info=metadatarun;
            if(info->info[NvMetaDataInfo_URL])
                *(info->info[NvMetaDataInfo_URL]+info->length[NvMetaDataInfo_URL])=*bufferrun;
            info->length[NvMetaDataInfo_URL]++;
            bufferrun++;

        }

        filesread++;
    }


    return status;
}


NvError M3uParserClose(NvMetaFileParserHandle handle)
{
    NvError status = NvSuccess;
    M3uparser* parsercontext=(M3uparser*)handle; 
    NvOsFree(parsercontext->M3udata);
    NvOsFree(parsercontext->FileStartOffsets);
    NvOsFree(parsercontext->FileEndOffsets);
    NvOsFree(parsercontext);
    return status;
}
