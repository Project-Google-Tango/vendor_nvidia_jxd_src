/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvos.h"
#include "nvmm_ogg.h"
#include "nvmm_oggwrapper.h"
#include "nvmm_oggprototypes.h"
#include "nvtest.h"
#include "nvmm_oggparser.h"

void OggParserDealloc(void *);

NvS32 TotalPcmOutputBytes = 0;


NvError NvOGGParserInit(NvOggParser *pState, void **OggParams)
{
    OggVorbis_File       *OGGParams ;//= (OggVorbis_File *)OggParams;
    NvError                 status = NvError_Success;
   /* First page-read happens here, so need a page instance */
    ogg_page              og = {0,0,0,0};
    NvS32                   nBytesRead = 0;

    if(*OggParams == NULL)
    {
        *OggParams = (OggVorbis_File *)NvOsAlloc(sizeof(OggVorbis_File));
        NvOsMemset(*OggParams, 0, sizeof(OggVorbis_File));
        ((OggVorbis_File *)(*OggParams))->og = (ogg_page *)NvOsAlloc(sizeof(ogg_page));
        NvOsMemset(((OggVorbis_File *)(*OggParams))->og, 0, sizeof(ogg_page));
    }

    OGGParams = (OggVorbis_File *)*OggParams;

     /*  initialize parser.i.e create necessary buffers, initialize
        *certain* variables
    */
    if(NvOvopen(pState, OGGParams, NULL, 0) < 0)
    {
        //NvTestPrintf("Input does not appear to be an Ogg bitstream.\n");
        status = NvError_ParserCorruptedStream ;
        goto errorExit;
    }

    TotalPcmOutputBytes = (NvS32)NvOvPcmTotal(OGGParams,-1);

    /*  This is one-time read for headers and other control info */
    nBytesRead = NvOggFirstFread(pState,OGGParams,&og,1,1);

    if(nBytesRead > 0)
    {
    /*  we have got a page from parser. This is
        run by decoder after receiving the proper page.
        Add the page to the decoder, so that decoder
        will start when called
    */
        //OGGParams->og = &og;
        NvOsMemcpy(OGGParams->og, &og, sizeof(ogg_page));
    }
    else
    {
        /* Error: Right now not handled */
    }

errorExit:
    return status;
}

void NvOGGParserDeInit(void **OggParams)
{
    //NvOvClear(((OggVorbis_File *)(*OggParams)));
    if(*OggParams)
    {
        if(((OggVorbis_File *)(*OggParams))->og)
        {
            NvOsFree(((OggVorbis_File *)(*OggParams))->og);
            ((OggVorbis_File *)(*OggParams))->og = NULL;
        }

        /* Deallocation of 'OggVorbis_File' struct members are NOW done within the Ogg_Parser block,
           that were earlier done from the Vorbis_Decoder side */

        OggParserDealloc(*OggParams);

        NvOsFree(*OggParams);
        OggParams = NULL;         
    }    
  
}

void NvOGGParamsInit(OggVorbis_File *OGGParams,
                                  OggVorbis_Trackinfo *OGGTrackInfo,
                                  OggVorbis_Trackinfo2 *OGGTrackInfo2)
{
    /* could have sent OGGParams instead of OGGTrackInfo for initializing decoder.
       but the nvmm message size is 256 bytes, whereas OGGParams is 340. Creating
       a sub-structure of OGGParams, i.e OGGTrackInfo to send the message
    */
    OGGTrackInfo->ready_state      = OGGParams->ready_state;
    OGGTrackInfo->offset           = OGGParams->offset;
    OGGTrackInfo->os               = OGGParams->os;
    OGGTrackInfo->oy               = OGGParams->oy;
    OGGTrackInfo->vi               = OGGParams->vi;
    OGGTrackInfo->vd               = OGGParams->vd;
    OGGTrackInfo->vb               = OGGParams->vb;
    OGGTrackInfo->og               = OGGParams->og;
    OGGTrackInfo->end              = OGGParams->end;
    OGGTrackInfo->vc               = OGGParams->vc;
    OGGTrackInfo->pcm_offset       = OGGParams->pcm_offset;
    OGGTrackInfo->current_serialno = OGGParams->current_serialno;
    OGGTrackInfo->current_link     = OGGParams->current_link;

    /* could have sent OGGParams instead of OGGTrackInfo for initializing decoder.
       but the nvmm message size is 256 bytes, whereas OGGParams is 340. Again creating
       a sub-structure of OGGParams, i.e OGGTrackInfo2 to send the message
    */
    OGGTrackInfo2->links       = OGGParams->links;
    OGGTrackInfo2->offsets     = OGGParams->offsets;
    OGGTrackInfo2->dataoffsets = OGGParams->dataoffsets;
    OGGTrackInfo2->pcmlengths  = OGGParams->pcmlengths;
    OGGTrackInfo2->serialnos   = OGGParams->serialnos;
    OGGTrackInfo2->bittrack    = OGGParams->bittrack;
    OGGTrackInfo2->samptrack   = OGGParams->samptrack;
    OGGTrackInfo2->readflag    = OGGParams->readflag;
}


void OggParserDealloc(void *OggParams)
{
    OggVorbis_File *OggVorbisParameters = (OggVorbis_File *) OggParams;

    if (OggVorbisParameters->dataoffsets)
    {
        NvOsFree(OggVorbisParameters->dataoffsets);
        OggVorbisParameters->dataoffsets = NULL;
    }

    if (OggVorbisParameters->pcmlengths)
    {
        NvOsFree(OggVorbisParameters->pcmlengths);
        OggVorbisParameters->pcmlengths = NULL;
    }

    if (OggVorbisParameters->serialnos)
    {
        NvOsFree(OggVorbisParameters->serialnos);
        OggVorbisParameters->serialnos = NULL;
    }
    if (OggVorbisParameters->offsets)
    {
        NvOsFree(OggVorbisParameters->offsets);
        OggVorbisParameters->offsets = NULL;
    }

    if (OggVorbisParameters->vi)
    {
        OggParseInfoClear(OggVorbisParameters->vi);

        NvOsFree(OggVorbisParameters->vi);
        OggVorbisParameters->vi = NULL;
    }

    if (OggVorbisParameters->vc)
    {
        NvVorbisCommentClear(OggVorbisParameters->vc);

        NvOsFree(OggVorbisParameters->vc);
        OggVorbisParameters->vc = NULL;
    }

    if (OggVorbisParameters->os)
    {
        NvOsFree(OggVorbisParameters->os);
        OggVorbisParameters->os = NULL;
    }

    if (OggVorbisParameters->oy)
    {
        NvOsFree(OggVorbisParameters->oy);
        OggVorbisParameters->oy = NULL;
    }

}


