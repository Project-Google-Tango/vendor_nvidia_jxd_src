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

#include <ctype.h>

#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvos.h"
#include "nvmetafile_asxparser.h"
#include "nvmm_sock.h"

typedef struct AsxparserRec
{
    NvU32 Maxfilenum;
    NvU8* Asxdata;//data pointer
    NvU64 Asxdatasize;
    NvU64* EntryStartOffsets;
    NvU64* EntryEndOffsets;
}Asxparser;


static NvError GetOffsets(Asxparser* parsercontext);
static NvError GetFileCount(Asxparser* parsercontext); 
static NvError  CheckSyntax(Asxparser* Asxparsercontext);

static int Nvstrincmp(
                      const unsigned char *s1,
                      const char *s2,
                      size_t n)
{

    while (n && *s2) 
    {
        int delta = toupper((char)*s1) - toupper(*s2);
        if (delta)
            return delta;
        s1++;
        s2++;
        n--;
    }

    return 0;
}


NvError AsxParserOpen(NvU8* filename, 
                      NvMetaFileParserHandle *handle, 
                      NvU32* nTotalFileCount)
{
    NvError Status = NvSuccess;
    NvOsFileHandle  hInputFile;
    size_t bytesRead;
    NvU64  filesize=0;
    Asxparser * Asxparsercontext=NULL;

    Asxparsercontext= (Asxparser *)NvOsAlloc(sizeof(Asxparser));
    if (!Asxparsercontext)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(Asxparsercontext, 0, sizeof(Asxparser));


    if ( (NvOsStrlen((char*)filename) > 5) && !NvOsStrncmp((char*)filename, "http:",5))
    {  
        char* fileBuffer=NULL;
        NvS64 fileLen = 0;

        Status = NvMMSockGetHTTPFile((char*)filename, &fileBuffer, &fileLen);
        if (NvSuccess != Status || fileLen <= 0)
        {
            NvOsDebugPrintf("Failed to find URL %s\n", filename);
            goto FAIL;
        }

        Asxparsercontext->Asxdata=(NvU8*)fileBuffer;
        Asxparsercontext->Asxdatasize=(NvU64)fileLen;

    }
    else
    {
        //NvOsDebugPrintf("input file %s ",filename);
        Status =  NvOsFopen((const char *)filename, NVOS_OPEN_READ, &hInputFile);
        if (hInputFile == NULL)
        {
            NvOsDebugPrintf("Input file open fails\n");
            goto FAIL;
        }
        //Parse ASX to get number of files in playlist

        NvOsFseek(hInputFile,0,NvOsSeek_End);
        NvOsFtell(hInputFile , &filesize);
        NvOsFseek(hInputFile,0, NvOsSeek_Set);

        // Alloc the memory for ASX Content
        Asxparsercontext->Asxdata = (NvU8*)NvOsAlloc((size_t)filesize);
        if(!Asxparsercontext->Asxdata)
        {
            NvOsDebugPrintf("memory allocation failed");
            Status =  NvError_InsufficientMemory;
            goto FAIL;
        }
        Asxparsercontext->Asxdatasize=filesize;
        // Read ASX Content and Parse
        Status =  NvOsFread(hInputFile,Asxparsercontext->Asxdata,(NvU32) filesize, &bytesRead);
        NvOsFclose(hInputFile);
    }
    if(Nvstrincmp(Asxparsercontext->Asxdata,"<ASX",4))
    {
        NvOsDebugPrintf("This is not a valid ASX file"); 
        Status =  NvError_NotSupported;
        goto FAIL;
    }
    Status =  CheckSyntax(Asxparsercontext);
    if(Status != NvSuccess )
    {
        NvOsDebugPrintf("SYNTAX FAILED***********");
        goto FAIL;
    }
    Status = GetFileCount(Asxparsercontext); 
    if(Status != NvSuccess )
    {
        NvOsDebugPrintf("SYNTAX FAILED***********");
        goto FAIL;
    }
    Asxparsercontext->EntryStartOffsets=NvOsAlloc(sizeof(NvU64)*(Asxparsercontext->Maxfilenum+1));
    Asxparsercontext->EntryEndOffsets=NvOsAlloc( sizeof(NvU64)*(Asxparsercontext->Maxfilenum+1));
    if( !Asxparsercontext->EntryStartOffsets || !Asxparsercontext->EntryEndOffsets)
    {
        Status =  NvError_InsufficientMemory;
        goto FAIL;
    }

    NvOsMemset(Asxparsercontext->EntryStartOffsets, 0, sizeof(NvU64)*(Asxparsercontext->Maxfilenum+1));
    NvOsMemset(Asxparsercontext->EntryEndOffsets, 0, sizeof(NvU64)*(Asxparsercontext->Maxfilenum+1));
    //get all entry element offsets
    Status =GetOffsets(Asxparsercontext);
    if(Status != NvSuccess )
    {
        NvOsDebugPrintf("SYNTAX FAILED***********");
        goto FAIL;
    }

    *nTotalFileCount=Asxparsercontext->Maxfilenum ;
    *handle=(NvMetaFileParserHandle *)Asxparsercontext;

    return Status;
FAIL:

    NvOsFree(Asxparsercontext->EntryEndOffsets);
    NvOsFree(Asxparsercontext->EntryStartOffsets);
    NvOsFree( Asxparsercontext->Asxdata);
    NvOsFree(Asxparsercontext);
    return Status;
}


NvError CheckSyntax(Asxparser* Asxparsercontext)
{
    NvError Status=NvSuccess;
    NvU8* Validdata; 
    NvU8*  Validdatastart;
    NvU8*  Validdatatemp;
    NvU8* bufferrun;
    NvU8* bufferrunend;
    //int tagstart=1;
    int length=0;int space=0;

    bufferrun = Asxparsercontext->Asxdata;
    bufferrunend = Asxparsercontext->Asxdata+Asxparsercontext->Asxdatasize;
    Validdata=NvOsAlloc((size_t)Asxparsercontext->Asxdatasize);
    NvOsMemset(Validdata,0,(size_t)Asxparsercontext->Asxdatasize);
    Validdatastart=Validdata;
    Validdatatemp=Validdata;

    for (;bufferrun<bufferrunend;)  //no space alllowed between "<" and tagname.
    {
        while ((*bufferrun != '<') &&  (bufferrun < bufferrunend))
        {

            if (*bufferrun != '\n' && *bufferrun != '\r' )
            { 
                *Validdata=*bufferrun;
                bufferrun++;
                Validdata++;
            }
            else  bufferrun++;
        }

        *Validdata=*bufferrun;
        bufferrun++;
        Validdata++;

        while ((*bufferrun == ' ') && (bufferrun<bufferrunend)) //skip white spaces after '<'
        {
            bufferrun++;
            space =1;
        }
        // bufferrun++;
        if ((((*bufferrun != '/') && space) ||  (*(bufferrun+1)== ' ')) && (*bufferrun == '/') )
        {
            NvOsDebugPrintf("error enconterd");
            Status=NvError_MetadataFailure;
            NvOsFree(Validdatatemp);
            return Status;
        }
        {   
            space=0;
            while ((*bufferrun != '>')  && (bufferrun<bufferrunend))
            {
                if (*bufferrun != ' ')
                {
                    *Validdata++=*bufferrun;

                }
                bufferrun++;
            }
            //bufferrun++;

            *Validdata++=*bufferrun;
            bufferrun++;
            //tagstart^=tagstart;
        }
    }
    while ((*Validdatastart++ != 0) && (length <= Asxparsercontext->Asxdatasize))
        length++;

    Asxparsercontext->Asxdatasize=length;
    NvOsFree(Asxparsercontext->Asxdata);
    Asxparsercontext->Asxdata=Validdatatemp;
    // for(i=0;i<length;i++)
    //printf(" %c",*(Validdatatemp+i));
    return Status;
}

static NvError GetFileCount(Asxparser* context)
{ 
    NvError status=NvSuccess;
    Asxparser* parsercontext=(Asxparser*)context; 
    NvU32 entryendcount=0;
    NvU32 entrystartcount=0;
    NvU8* bufferrun;
    NvU8* bufferrunend;
    bufferrun = parsercontext->Asxdata;
    bufferrunend = bufferrun+parsercontext->Asxdatasize;

    for(;bufferrun<bufferrunend;)
    {
        if(!(Nvstrincmp(bufferrun,"<entry>",7)))
        {
            bufferrun+=7;
            entrystartcount++;
        }
        else if(!(Nvstrincmp(bufferrun,"</entry>",8)) )
        {
            bufferrun+=8;
            entryendcount++;
        }
        else
        {
            bufferrun+=1;
        }
    }
    if(entrystartcount != entryendcount )
        return NvError_MetadataFailure;
    parsercontext->Maxfilenum = entrystartcount;

    return status;
}

static NvError GetOffsets(Asxparser *context)
{
    NvError status=NvSuccess;
    Asxparser* parsercontext=(Asxparser*)context; 
    NvU32 i;
    NvU32 entrystartcount=1;
    NvU32 entryendcount=1;
    NvU8* bufferrun;
    NvU8* bufferrunend;
    NvU8* bufferstart;
    bufferstart = parsercontext->Asxdata;
    bufferrun = parsercontext->Asxdata;
    bufferrunend = bufferrun+parsercontext->Asxdatasize;
    for(;bufferrun<bufferrunend;)
    {
        if(!(Nvstrincmp(bufferrun,"<entry>",7)))
        {
            parsercontext->EntryStartOffsets[entrystartcount]=bufferrun-bufferstart;
            bufferrun+=7;
            entrystartcount++;
        }
        else if(!(Nvstrincmp(bufferrun,"</entry>",8)) )
        {
            parsercontext->EntryEndOffsets[entryendcount]=bufferrun-bufferstart;
            bufferrun+=8;
            entryendcount++;
        }
        else
        {
            bufferrun+=1;
        }
    }

    parsercontext->EntryStartOffsets[0]=0;  //For Global Playlist Data
    parsercontext->EntryEndOffsets[0]=parsercontext->EntryStartOffsets[1];
    for (i=0;i<parsercontext->Maxfilenum;i++)
    {
        if( parsercontext->EntryStartOffsets[i] > parsercontext->EntryEndOffsets[i] ||  parsercontext->EntryEndOffsets[i] > parsercontext->EntryStartOffsets[i+1])

            return NvError_MetadataFailure;
    }

    /*   for(i=0;i<=parsercontext->Maxfilenum;i++)
    NvOsDebugPrintf("Entry element  %d offsets %llu %llu ",i,parsercontext->EntryStartOffsets[i],parsercontext->EntryEndOffsets[i]);*/
    return status;
}



NvError AsxFileInfo(NvMetaFileParserHandle handle, 
                    NvU32 nIndex, 
                    NvU32 nfilesToRead, 
                    NvFileMetaData **fileInfoBuffer)
{
    NvError status = NvSuccess;
    Asxparser* parsercontext=(Asxparser*)handle; 
    NvU8* buffer;
    NvU8* bufferrun;
    NvU8* bufferrunend;
    NvFileMetaData *metadatarun;
    NvU32 i,filesread=0;
    NvU32 j;
    NvU32 titlefound,authorfound,reffound,copyrightfound;
    buffer=parsercontext->Asxdata;


    for(i=nIndex;i<(nIndex+nfilesToRead);i++)
    {   
        titlefound=0,authorfound=0,reffound=0,copyrightfound=0;
        metadatarun=(*fileInfoBuffer)+filesread;
        for(j=0;j<NvMetaDataInfo_Max;j++)
        {
            if(metadatarun->length[j] != 0)
                metadatarun->length[j]=0;

        }
        //NvOsDebugPrintf("Entry element  %d offsets %llu %llu ",i,parsercontext->EntryStartOffsets[i],parsercontext->EntryEndOffsets[i]);

        bufferrun = buffer+parsercontext->EntryStartOffsets[i];
        bufferrunend = buffer+parsercontext->EntryEndOffsets[i];

        for(;bufferrun<bufferrunend;)
        {
            if(!(Nvstrincmp(bufferrun,"<Title>",7)) && !titlefound)
            {
                bufferrun+=7;

                while((Nvstrincmp(bufferrun,"<",1)) && bufferrun<bufferrunend )
                {
                    NvFileMetaData *info=metadatarun;
                    if(info->info[NvMetaDataInfo_Title])
                        *(info->info[NvMetaDataInfo_Title]+info->length[NvMetaDataInfo_Title])=*bufferrun;
                    info->length[NvMetaDataInfo_Title]++;
                    bufferrun++;
                }
                titlefound=1;
            }
            else if(!(Nvstrincmp(bufferrun,"<author>",8)) && !authorfound)
            {
                bufferrun+=8;
                while((Nvstrincmp(bufferrun,"<",1)) && bufferrun<bufferrunend )
                {
                    NvFileMetaData *info=metadatarun;
                    if(info->info[NvMetaDataInfo_Author])
                        *(info->info[NvMetaDataInfo_Author]+info->length[NvMetaDataInfo_Author])=*bufferrun;
                    info->length[NvMetaDataInfo_Author]++;
                    bufferrun++;
                }
                authorfound=1;
            }
            else if(!(Nvstrincmp(bufferrun,"<ref",4))  && !reffound)
            {
                bufferrun+=4;
                while((Nvstrincmp(bufferrun,"href",4)))
                    bufferrun++;
                while(*bufferrun++ != '"') ;//skip space after href

                while(*bufferrun != '"' && bufferrun<bufferrunend )
                {
                    NvFileMetaData *info=metadatarun;
                    if(info->info[NvMetaDataInfo_URL])
                        *(info->info[NvMetaDataInfo_URL]+info->length[NvMetaDataInfo_URL])=*bufferrun;
                    info->length[NvMetaDataInfo_URL]++;
                    bufferrun++;
                }
                bufferrun+=1;
                reffound=1;
            }
            else if(!(Nvstrincmp(bufferrun,"<Copyright>",11))  && !copyrightfound)
            {
                bufferrun+=11;
                while((Nvstrincmp(bufferrun,"<",1)) && bufferrun<bufferrunend )
                {
                    NvFileMetaData *info=metadatarun;
                    if(info->info[NvMetaDataInfo_Copyright])
                        *(info->info[NvMetaDataInfo_Copyright]+info->length[NvMetaDataInfo_Copyright])=*bufferrun;
                    info->length[NvMetaDataInfo_Copyright]++;
                    bufferrun++;
                }
                copyrightfound=1;
            }
            else 
            {
                bufferrun+=1;
            }

        }
        filesread++;
    }


    return status;
}


NvError AsxParserClose(NvMetaFileParserHandle handle)
{
    NvError status = NvSuccess;
    Asxparser* parsercontext=(Asxparser*)handle; 
    NvOsFree(parsercontext->Asxdata);
    NvOsFree(parsercontext->EntryStartOffsets);
    NvOsFree(parsercontext->EntryEndOffsets);
    NvOsFree(parsercontext);
    return status;
}
