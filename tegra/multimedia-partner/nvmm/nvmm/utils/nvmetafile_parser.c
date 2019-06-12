/**
* Copyright (c) 2009 - 2012 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** This meta file extractor util is used by the client to parse various meta files like m3u, pls or asx etc.
*  The meta file extrator engine can be started using NvMMMetaFileExtractorInit function. There is no limitation on
* the number of meta files that can be openend by this engine. Each meta file has its own unique file handle.
* Thread safety is not implemented in this file as of now and client has to take care of the thread safety.
*
*/

#include <ctype.h>
#include "nvmetadata.h"
#include "nvmetafile_parser.h"
#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvos.h"
#include "nvmetafile_asxparser.h"
#include "nvmetafile_m3uparser.h"
#include "nvutil.h"

#define PARSERLIBRARY_NAMES   "metafileformatlibs.xml"

/* An instance of this structure created to hold the function pointers of
*  3rd party library.
*/


typedef struct NvMetaParserFuncRec
{

    NvError (*CreateHandle)(
        NvU8* filename,
        NvMetaFileParserHandle *handle,
        NvU32 *nTotalFileCount);

    NvError (*DestroyHandle)(NvMetaFileParserHandle handle);

    NvError (*GetFileInfo)(
        NvMetaFileParserHandle handle,
        NvU32 nIndex,
        NvU32 nfilesToRead,
        NvFileMetaData **fileInfoBuffer);

}NNvMetaParserFunc;





/* Instance of this structure created when client calls NvCreateMetaFileHandle() and
member of this structure  "hMetaParserClient" returned to client for further communication.
*/
typedef struct MetafileParserRec
{
    NvMetaFileParserHandle hMetaParserClient;//Parser handle returned to client
    NNvMetaParserFunc *parserfunc;//individual parser functons
    NvMetaFileParserHandle hMetaParserInternal;// handle returned by actual parser
    NvOsLibraryHandle hparserlib; //library handle to interact with 3rd party libs
    NvU8 filetype[16];
    NvU32 fileCount;

}MetafileParserContext;


/* Each metaparser provides three basic API's namely open(),close(),getfiles().
Pointers to these functions exracted from libraries within this function.
*/
static NvError GetParserSpecificPointers(MetafileParserContext * extractfunct);

/* like the standard strrchr function */

static  char *NvStrrchr(const char *string, int character)
{
    char c = (char)character;
    int i;

    // Consider the terminating char as part of the string
    for (i = NvOsStrlen(string); i >= 0; --i) {
        if (string[i] == c)
            return (char*) &string[i];

    }

    return NULL;
}



/* Opens a corresponding metaparser library based on te extension of the input file name
and parses the input metafile to get the total number of media files present.client uses
this information during getfileinfo() calls to get attribute info.
Only input files with valid extension are supported.
*/

NvError NvMetaFileParserOpen(
                             NvU8* sURL,
                             NvMetaFileParserHandle *hMetaParser,
                             NvU32 *nTotalFileCount)
{
    NvError Status = NvSuccess;
    MetafileParserContext * metacontexthandle = NULL;
    char* str=NULL;
    char FileExt[8] = { 0 };


    //Create Eatractor instance for a given metafile
    metacontexthandle= (MetafileParserContext *)NvOsAlloc(sizeof(MetafileParserContext));
    if (!metacontexthandle)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(metacontexthandle, 0, sizeof(MetafileParserContext));


    metacontexthandle->parserfunc=(NNvMetaParserFunc *)NvOsAlloc(sizeof(NNvMetaParserFunc));
    if (!metacontexthandle->parserfunc)
    {
        Status= NvError_InsufficientMemory;
        goto FAIL;
    }

    //File Extension parsing and input file must have extension otherwise not supported
    str = NvStrrchr((char*)sURL,'.');
    if (str)
    {
        str++;
        if (*str != 0 && NvOsStrlen(str) != 0 && NvOsStrlen(str) <= 7)
        {
            NvOsMemcpy(FileExt, str, NvOsStrlen(str) + 1);
            str = FileExt;
            while (*str)
            {
                *str = tolower(*str);
                str++;
            }
        }
    }
    else
    {
        Status=NvError_NotSupported;
        goto FAIL;
    }
    NvOsStrncpy((char*)metacontexthandle->filetype,FileExt,sizeof(FileExt));//check with fileext

    //If input file is web File with valid meta file extension ,get the total web content into a local file
    //and send that file to parser lib.

    if(NvOsStrlen((char*)metacontexthandle->filetype)>0)
    {
        Status=GetParserSpecificPointers(metacontexthandle);
        if(Status != NvSuccess)
        {
            NvOsDebugPrintf("Meta file extension not supported");
            goto FAIL;
        }
    }
    else
    {
        Status=NvError_NotSupported;
        goto FAIL;
    }
    //Call actual parser open and get total file count in playlist.
    Status=metacontexthandle->parserfunc->CreateHandle(sURL,&(metacontexthandle->hMetaParserInternal),&(metacontexthandle->fileCount));
    if((Status != NvSuccess ) || (metacontexthandle->hMetaParserInternal == NULL))
    {
        NvOsDebugPrintf("opening file specific parser failed");
        goto FAIL;
    }


    metacontexthandle->hMetaParserClient=(void*)metacontexthandle;
    *nTotalFileCount=metacontexthandle->fileCount;
    *hMetaParser=metacontexthandle->hMetaParserClient;
    return Status;
FAIL:
    NvOsFree(metacontexthandle->parserfunc);
    NvOsFree(metacontexthandle);
    return Status;
}


static NvError GetParserSpecificPointers(MetafileParserContext* metacontexthandle)
{
    NvError Status = NvSuccess;
    NvOsFileHandle hXmlfile=NULL;
    NvU8* Xmlbuf=NULL;
    NvU64 readBytes,filesize;
    char* lib = NULL;
    /*For Asx,M3u XML parsing not required.*/
    if (!NvOsStrncmp((char*)metacontexthandle->filetype,"asx",3))
    {
        metacontexthandle->parserfunc->CreateHandle=AsxParserOpen; //asx specific functions .
        metacontexthandle->parserfunc->DestroyHandle=AsxParserClose;
        metacontexthandle->parserfunc->GetFileInfo=AsxFileInfo;
        return Status;
    }
    else if (!NvOsStrncmp((char*)metacontexthandle->filetype,"m3u",3))
    {

        metacontexthandle->parserfunc->CreateHandle=M3uParserOpen; //m3u specific functions .
        metacontexthandle->parserfunc->DestroyHandle=M3uParserClose;
        metacontexthandle->parserfunc->GetFileInfo=M3uFileInfo;
        return Status;
    }
    //Xml file parsing
    //NvOsDebugPrintf("path name %s ",PARSERLIBRARY_NAMES);
    Status =  NvOsFopen( PARSERLIBRARY_NAMES , NVOS_OPEN_READ, &hXmlfile);
    if (hXmlfile == NULL)
    {
        NvOsDebugPrintf("Opening XML file  failed\n");
        return Status;
    }

    NvOsFseek(hXmlfile,0,NvOsSeek_End);
    NvOsFtell(hXmlfile , &filesize);
    NvOsFseek(hXmlfile,0, NvOsSeek_Set);

    // Alloc the XML Content
    Xmlbuf = (NvU8*) NvOsAlloc((size_t)filesize);
    if (!Xmlbuf)
    {
        Status= NvError_InsufficientMemory;
        goto fail;
    }

    // Read XML Content and Parse
    Status =  NvOsFread(hXmlfile,Xmlbuf,(size_t) filesize, (size_t*)&readBytes);
    {
        char*  Xmlbufrun=(char*)Xmlbuf;
        char*  Xmlbufrunend=(char*)Xmlbuf+filesize;
        char*  XmlbufrunTemp=NULL;
        int index=0;
        char ext[8]={0};

        while (Xmlbufrun<Xmlbufrunend)
        {
            if (!NvOsStrncmp(Xmlbufrun,"<Ext>",5))
            {   index=0;
            Xmlbufrun+=5;
            while ((NvOsStrncmp(Xmlbufrun,"<",1)) && Xmlbufrun<Xmlbufrunend && index<8)
                ext[index++]=*Xmlbufrun++;
            Xmlbufrun+=6;
            if (!NvOsStrncmp(ext,(char*)metacontexthandle->filetype,NvOsStrlen((char*)metacontexthandle->filetype)))
            {
                index=0;
                while (*Xmlbufrun++ != '<' && Xmlbufrun<Xmlbufrunend) ;
                if (!(NvOsStrncmp(Xmlbufrun,"Lib>",4)) && Xmlbufrun<Xmlbufrunend )
                {
                    Xmlbufrun+=4;
                    XmlbufrunTemp=Xmlbufrun;
                    while ((NvOsStrncmp(Xmlbufrun,"<",1)) && Xmlbufrun<Xmlbufrunend )
                    {
                        index++;
                        Xmlbufrun++;
                    }
                    // Alloc the XML Content
                    lib =  (char*)NvOsAlloc((size_t)index);
                    if (!lib)
                    {
                        Status= NvError_InsufficientMemory;
                        goto fail;
                    }
                    NvOsMemcpy(lib, XmlbufrunTemp,index);
                    break;

                }
            }
            }
            else Xmlbufrun++;

        }

        if (*lib == 0)
        {
            NvOsDebugPrintf("Library not Found %s",lib);
            Status =  NvError_NotSupported;
            return Status;
        }

        NvOsDebugPrintf("Library  Found is %s",lib);

        //Get Procedure pointers form lib
        {

            Status = NvOsLibraryLoad(lib, &(metacontexthandle->hparserlib));
            if (Status != NvSuccess)
            {
                switch (Status)
                {
                case NvError_InsufficientMemory:
                    NvOsDebugPrintf("Insufficient memory to load '%s'\n",
                        lib );
                    break;
                default:
                    NvOsDebugPrintf("Could not load test '%s'\n", lib );
                    break;
                }
                goto fail;
            }
            metacontexthandle->parserfunc->CreateHandle = NvOsLibraryGetSymbol(metacontexthandle->hparserlib, "NvMetaFileParserOpen");
            metacontexthandle->parserfunc->DestroyHandle=NvOsLibraryGetSymbol(metacontexthandle->hparserlib, "NvMetaFileParserClose");
            metacontexthandle->parserfunc->GetFileInfo=NvOsLibraryGetSymbol(metacontexthandle->hparserlib, "NvGetMetaFileInfo");
            if (metacontexthandle->parserfunc->CreateHandle == NULL ||  metacontexthandle->parserfunc->DestroyHandle == NULL
                || metacontexthandle->parserfunc->GetFileInfo == NULL)
            {
                NvOsDebugPrintf("Could not load symbol from lib '%s'\n",lib );
                Status =  NvError_NotSupported;
                goto fail;
            }

        }
        NvOsFree(lib);
    }


    return Status;
fail:
    NvOsLibraryUnload(metacontexthandle->hparserlib);
    NvOsFclose(hXmlfile);
    NvOsFree(Xmlbuf);
    NvOsFree(lib);
    return Status;

}
/* Destroys instance of MetafileParserContext and unloads if any 3rd party meta parser from the process.
*/

NvError NvMetaFileParserClose(NvMetaFileParserHandle hMetaParser)
{
    NvError Status = NvSuccess;
    MetafileParserContext * Contexthandle = (MetafileParserContext*)hMetaParser;
    if (Contexthandle->hMetaParserInternal)
        Contexthandle->parserfunc->DestroyHandle(Contexthandle->hMetaParserInternal);
    NvOsFree( Contexthandle->parserfunc);
    if (Contexthandle->hparserlib)
        NvOsLibraryUnload(Contexthandle->hparserlib);
    NvOsFree( Contexthandle);
    return Status;
}
/* Extracts Attribute data specific to each media file present in meta file .
*/

NvError
NvGetMetaFileInfo(
                  NvMetaFileParserHandle hMetaParser,
                  NvU32 nFileIndex,
                  NvU32 nFileCount,
                  NvFileMetaData **fileInfo)
{
    NvError Status = NvSuccess;
    MetafileParserContext * Contexthandle = (MetafileParserContext*)hMetaParser;
    if (nFileIndex > Contexthandle->fileCount  || nFileCount > (Contexthandle->fileCount+1))
        return NvError_BadParameter;
    Status=Contexthandle->parserfunc->GetFileInfo(Contexthandle->hMetaParserInternal,nFileIndex,nFileCount,fileInfo);

    return Status;
}
