/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2003    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: stdio-based convenience library for opening/seeking/decoding
 last mod: $Id: vorbisfile.c,v 1.6 2003/03/30 23:40:56 xiphmont Exp $

 ********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nvmm_oggdefines.h"
#include "nvmm_ogg.h"
#include "nvmm_oggprototypes.h"
#include "nvmm_oggbookunpack.h"
#include "nvos.h"
#include "nverror.h"
#include "nvtest.h"
#include "nvmm_oggwrapper.h"

extern OggVorbis_File vfdecode;
#define OPEN2_MAX_SEARCH_BYTES_LIMIT      50*1024

/* A 'chained bitstream' is a Vorbis bitstream that contains more than
   one logical bitstream arranged end to end (the only form of Ogg
   multiplexing allowed in a Vorbis bitstream; grouping [parallel
   multiplexing] is not allowed in Vorbis) */

/* A Vorbis file can be played beginning to end (streamed) without
   worrying ahead of time about chaining (see decoder_example.c).  If
   we have the whole file, however, and want random access
   (seeking/scrubbing) or desire to know the total length/time of a
   file, we need to account for the possibility of chaining. */

/* We can handle things a number of ways; we can determine the entire
   bitstream structure right off the bat, or find pieces on demand.
   This example determines and caches structure for the entire
   bitstream, but builds a virtual decoder on the fly when moving
   between links in the chain. */

/* There are also different ways to implement seeking.  Enough
   information exists in an Ogg bitstream to seek to
   sample-granularity positions in the output.  Or, one can seek by
   picking some portion of the stream roughly in the desired area if
   we only want coarse navigation through the stream. */

/*************************************************************************
 * Many, many internal helpers.  The intention is not to be confusing;
 * rampant duplication and monolithic function implementation would be
 * harder to understand anyway.  The high level functions are last.  Begin
 * grokking near the end of the file */

#ifdef DBG_BUF_PRNT
    NvU32 mallocTotal = 0;
#endif

/* read a little more data from the file/pipe into the ogg_sync framer */
 NvS32 NvGetData(NvOggParser * pState,OggVorbis_File *vf){
  //errno=0; //-- this is vc98 specific error handling, removing for ARM
  if(vf->datasource){
    NvU8 *buffer=NvOggSyncBufferin(vf->oy,CHUNKSIZE);
    NvU32 size = 0;
    NvError err; 
    CPuint64 askBytes64 = 0;
    CP_CHECKBYTESRESULTTYPE eResult = CP_CheckBytesOk;
    
    //NvError err =  NvOsFread(vf->datasource, buffer, CHUNKSIZE, (size_t *)&size);
    size = CHUNKSIZE;
    err = pState->pPipe->cpipe.CheckAvailableBytes(pState->cphandle, size, &eResult);

    if(eResult == CP_CheckBytesInsufficientBytes)
    {
        err = pState->pPipe->GetAvailableBytes(pState->cphandle, &askBytes64);         
        size = (NvU32)askBytes64;
    }
    if (eResult == CP_CheckBytesAtEndOfStream)
    {
        size = 0;
    }
    
    err = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)buffer, size);
    
    if(!err){}
    //NvS32 bytes=(vf->callbacks.read_func)(buffer,1,CHUNKSIZE,vf->datasource);
    if(size>0)NvOggSyncWrote(vf->oy,size);
    //if(bytes==0 /*&& errno*/)return(-1);
    return(size);
  }else
    return(0);
}

/* save a tiny smidge of verbosity to make the code more readable */
 void NvSeekHelper(NvOggParser * pState,OggVorbis_File *vf,ogg_int64_t offset){
  if(vf->datasource){
    
    //NvError err = NvOsFseek(vf->datasource, offset, NvOsSeek_Set);
    NvError err = pState->pPipe->cpipe.SetPosition(pState->cphandle, (CPuint)offset, CP_OriginBegin);   
    
    if(err)
    {/*doing nothing */}
    //(vf->callbacks.seek_func)(vf->datasource, offset, SEEK_SET);
    vf->offset=offset;
    NvOggSyncReset(vf->oy);
  }else{
    /* shouldn't happen unless someone writes a broken callback */
    return;
  }
}

/* The read/seek functions track absolute position within the stream */

/* from the head of the stream, get the next page.  boundary specifies
   if the function is allowed to fetch more data from the stream (and
   how much) or only use internally buffered data.

   boundary: -1) unbounded search
              0) read no additional data; use cached only
          n) search for a new page beginning for n bytes

   return:   <0) did not find a page (OV_FALSE, OV_EOF, OV_EREAD)
              n) found a page at absolute offset n

              produces a refcounted page */

 ogg_int64_t NvGetNextPage(NvOggParser * pState,OggVorbis_File *vf,ogg_page *og,
                  ogg_int64_t boundary){
  if(boundary>0)boundary+=vf->offset;
  while(1){
    NvS32 more;

    if(boundary>0 && vf->offset>=boundary)return(OV_FALSE);
    more=NvOggSyncPageseek(vf->oy,og);

    if(more<0){
      /* skipped n bytes */
      vf->offset-=more;
    }else{
      if(more==0){
    /* send more paramedics */
    if(!boundary)return(OV_FALSE);
    {
      NvS32 ret=NvGetData(pState,vf);
      if(ret==0)return(OV_EOF);
      if(ret<0)return(OV_EREAD);
    }
      }else{
    /* got a page.  Return the offset at the page beginning,
           advance the internal offset past the page end */
    ogg_int64_t ret=vf->offset;
    vf->offset+=more;
    return(ret);

      }
    }
  }
}

/* find the latest page beginning before the current stream cursor
   position. Much dirtier than the above as Ogg doesn't have any
   backward search linkage.  no 'readp' as it will certainly have to
   read. */
/* returns offset or OV_EREAD, OV_FAULT and produces a refcounted page */

 ogg_int64_t NvGetPrevPage(NvOggParser * pState,OggVorbis_File *vf,ogg_page *og){
  ogg_int64_t begin=vf->offset;
  ogg_int64_t end=begin;
  ogg_int64_t ret;
  ogg_int64_t offset=-1;

  while(offset==-1){
    begin-=CHUNKSIZE;
    if(begin<0)
      begin=0;
    NvSeekHelper(pState,vf,begin);
    while(vf->offset<end){
      ret=NvGetNextPage(pState,vf,og,end-vf->offset);
      if(ret==OV_EREAD)return(OV_EREAD);
      if(ret<0){
    break;
      }else{
    offset=ret;
      }
    }
  }

  /* we have the offset.  Actually snork and hold the page now */
  NvSeekHelper(pState,vf,offset);
  ret=NvGetNextPage(pState,vf,og,CHUNKSIZE);
  if(ret<0)
    /* this shouldn't be possible */
    return(OV_EFAULT);

  return(offset);
}

ogg_int64_t NvGetPrevPageOpen2(NvOggParser * pState,OggVorbis_File *vf,ogg_page *og){
  ogg_int64_t begin=vf->offset;
  ogg_int64_t end=begin;
  ogg_int64_t ret;
  ogg_int64_t offset=-1;
  NvU32 SearchSizeInBytes = 0;

  while(offset==-1)
  {
        begin-=CHUNKSIZE;
        
        if(begin<0)
          begin=0;
        
        NvSeekHelper(pState,vf,begin);
        
        while(vf->offset<end)
        {
              ret=NvGetNextPage(pState,vf,og,end-vf->offset);
              
              if(ret==OV_EREAD)return(OV_EREAD);
              
              if(ret<0)
              {
                    break;
              }else
              {
                    offset=ret;
              }
        }

        SearchSizeInBytes +=CHUNKSIZE;       
        if(SearchSizeInBytes >= OPEN2_MAX_SEARCH_BYTES_LIMIT)
        {
               offset = SearchSizeInBytes;
               break;
        }           
  }

  /* we have the offset.  Actually snork and hold the page now */
  NvSeekHelper(pState,vf,offset);
  ret=NvGetNextPage(pState,vf,og,CHUNKSIZE);
  if(ret<0)
    /* this shouldn't be possible */
    return(OV_EFAULT);

  return(offset);
}

 static void NvFloor0FreeLook(vorbis_look_floor *i){
  NvVorbisLookFloor0 *look=(NvVorbisLookFloor0 *)i;
  if(look){

    if(look->linearmap)_ogg_free(look->linearmap);
    if(look->lsp_look)_ogg_free(look->lsp_look);
    memset(look,0,sizeof(*look));
    _ogg_free(look);
  }
}

static void NvFloor1FreeLook(vorbis_look_floor *i){
  NvVorbisLookFloor1 *look=(NvVorbisLookFloor1 *)i;
  if(look){
    memset(look,0,sizeof(*look));
    _ogg_free(look);
  }
}

 void NvRes0FreeLook(vorbis_look_residue *i){
  NvS32 j;
  if(i){

    NvVorbisLookResidue0 *look=(NvVorbisLookResidue0 *)i;

    for(j=0;j<look->parts;j++)
      if(look->partbooks[j])_ogg_free(look->partbooks[j]);
    _ogg_free(look->partbooks);
    for(j=0;j<look->partvals;j++)
      _ogg_free(look->decodemap[j]);
    _ogg_free(look->decodemap);

    memset(look,0,sizeof(*look));
    _ogg_free(look);
  }
}

 static void NvMapping0FreeLook(vorbis_look_mapping *look){
  NvS32 i;
  NvVorbisLookMapping0 *l=(NvVorbisLookMapping0 *)look;
  if(l){

    for(i=0;i<l->map->submaps;i++){
#ifdef CODEBOOK_INFO_CHANGE
        switch(i) {
        case 0:
            NvFloor0FreeLook(l->floor_look[i]);
            break;
        case 1:
            NvFloor1FreeLook(l->floor_look[i]);
            break;
        default:
            //error not handled here
            break;
        }
      NvRes0FreeLook(l->residue_look[i]);
#else
      l->floor_func[i]->free_look(l->floor_look[i]);
      l->residue_func[i]->free_look(l->residue_look[i]);
#endif
    }

    _ogg_free(l->floor_func);
    _ogg_free(l->residue_func);
    _ogg_free(l->floor_look);
    _ogg_free(l->residue_look);
    memset(l,0,sizeof(*l));
    _ogg_free(l);
  }
}


 void NvVorbisDspClear(vorbis_dsp_state *v){
  NvS32 i;
  
  if (v == NULL)
        return;
  
  if(v)
  {    

    vorbis_info *vi=v->vi;
    
        if(vi) 
        {
                NvCodecSetupInfo *ci=(NvCodecSetupInfo *)(vi->codec_setup);

                NvPrivateState *b=(NvPrivateState *)v->backend_state;

                if(v->pcm)
                {
                  for(i=0;i<vi->channels;i++)
                      if(v->pcm[i]) {
                        _ogg_free(v->pcm[i]);
                        v->pcm[i] = NULL;
                      }
                  _ogg_free(v->pcm);
                  v->pcm = NULL;
                  if(v->pcmret) {
                      _ogg_free(v->pcmret);
                      v->pcmret = NULL;
                  }
                }

                /* free mode lookups; these are actually vorbis_look_mapping structs */
                if(ci)
                {
                  for(i=0;i<ci->modes;i++){
                //int mapnum=ci->mode_param[i]->mapping;
                //int maptype=ci->map_type[mapnum];
#ifdef CODEBOOK_INFO_CHANGE
                if(b && b->mode) NvMapping0FreeLook(b->mode[i]);
#else
                if(b && b->mode)_mapping_P[maptype]->free_look(b->mode[i]);
#endif
                  }
                }

                if(b){
                    if(b->mode) {
                      _ogg_free(b->mode);
                      b->mode = NULL;
                    }
                  _ogg_free(b);
                  b = NULL;
                }
        }

    memset(v,0,sizeof(*v));
  }
  
}

/* finds each bitstream link one at a time using a bisection search
   (has to begin by knowing the offset of the lb's initial page).
   Recurses for each link so it can alloc the link storage after
   finding them all, then unroll and fill the cache at the same time */
 NvS32 NvBisectForwardSerialno(NvOggParser * pState,OggVorbis_File *vf,
                    ogg_int64_t begin,
                    ogg_int64_t searched,
                    ogg_int64_t end,
                    ogg_uint32_t currentno,
                    NvS32 m){
  ogg_int64_t endsearched=end;
  ogg_int64_t next=end;
  ogg_page og={0,0,0,0};
  ogg_int64_t ret;

  /* the below guards against garbage seperating the last and
     first pages of two links. */
  while(searched<endsearched){
    ogg_int64_t bisect;

    if(endsearched-searched<CHUNKSIZE){
      bisect=searched;
    }else{
      bisect=(searched+endsearched)/2;
    }

    NvSeekHelper(pState,vf,bisect);
    ret=NvGetNextPage(pState,vf,&og,-1);
    if(ret==OV_EREAD)return(OV_EREAD);
    if(ret<0 || NvOggPageSerialno(&og)!=currentno){
      endsearched=bisect;
      if(ret>=0)next=ret;
    }else{
      searched=ret+og.header_len+og.body_len;
    }
    NvOggPageRelease(&og);
  }

  NvSeekHelper(pState,vf,next);
  ret=NvGetNextPage(pState,vf,&og,-1);
  if(ret==OV_EREAD)return(OV_EREAD);

  if(searched>=end || ret<0){
    NvOggPageRelease(&og);
    vf->links=m+1;
    vf->offsets=_ogg_malloc((vf->links+1)*sizeof(*vf->offsets));
    vf->serialnos=_ogg_malloc(vf->links*sizeof(*vf->serialnos));
    vf->offsets[m+1]=searched;
  }else{
    ret=NvBisectForwardSerialno(pState,vf,next,vf->offset,
                 end,NvOggPageSerialno(&og),m+1);
    NvOggPageRelease(&og);
    if(ret==OV_EREAD)return(OV_EREAD);
  }

  vf->offsets[m]=begin;
  vf->serialnos[m]=currentno;
  return(0);
}

/* uses the local ogg_stream storage in vf; this is important for
   non-streaming input sources */
/* consumes the page that's passed in (if any) */

 NvS32 NvFetchHeaders(NvOggParser * pState,OggVorbis_File *vf,
              vorbis_info *vi,
              vorbis_comment *vc,
              ogg_uint32_t *serialno,
              ogg_page *og_ptr){
  ogg_page og={0,0,0,0};
  NvOggPacket op={0,0,0,0,0,0};
  NvS32 i,ret;

  if(!og_ptr){
    ogg_int64_t llret=NvGetNextPage(pState,vf,&og,CHUNKSIZE);
    if(llret==OV_EREAD)return(OV_EREAD);
    if(llret<0)return OV_ENOTVORBIS;
    og_ptr=&og;
  }

  NvOggStreamResetSerialno(vf->os,NvOggPageSerialno(og_ptr));
  if(serialno)*serialno=vf->os->serialno;
  vf->ready_state=STREAMSET;

  /* extract the initial header from the first page and verify that the
     Ogg bitstream is in fact Vorbis data */

  NvVorbisInfoInit(vi);
  NvVorbisCommentInit(vc);

  i=0;
  while(i<3){
    NvOggStreamPagein(vf->os,og_ptr);
    while(i<3){
      NvS32 result=NvOggStreamPacketout(vf->os,&op);
      if(result==0)break;
      if(result==-1){
    ret=OV_EBADHEADER;
    goto bail_header;
      }
      ret=NvVorbisSynthesisHeaderin(vi,vc,&op);
      if(ret){
    goto bail_header;
      }
      i++;
    }
    if(i<3)
      if(NvGetNextPage(pState,vf,og_ptr,CHUNKSIZE)<0){
    ret=OV_EBADHEADER;
    goto bail_header;
      }
  }

  NvOggPacketRelease(&op);
  NvOggPageRelease(&og);
  return 0;

 bail_header:
  NvOggPacketRelease(&op);
  NvOggPageRelease(&og);
  NvVorbisInfoClear(vi);
  NvVorbisCommentClear(vc);
  vf->ready_state=OPENED;

  return ret;
}

/* last step of the OggVorbis_File initialization; get all the
   vorbis_info structs and PCM positions.  Only called by the seekable
   initialization (local stream storage is hacked slightly; pay
   attention to how that's done) */

/* this is void and does not propogate errors up because we want to be
   able to open and use damaged bitstreams as well as we can.  Just
   watch out for missing information for links in the OggVorbis_File
   struct */
 void NvPrefetchAllHeaders(NvOggParser * pState,OggVorbis_File *vf, ogg_int64_t dataoffset){
  ogg_page og={0,0,0,0};
  NvS32 i;
  ogg_int64_t ret;

  vf->vi=_ogg_realloc(vf->vi,vf->links*sizeof(*vf->vi));
  vf->vc=_ogg_realloc(vf->vc,vf->links*sizeof(*vf->vc));
  vf->pcmlengths=_ogg_malloc(vf->links*2*sizeof(*vf->pcmlengths));
  //vf->dataoffsets = malloc(sizeof(NvS32) * 4);
  vf->dataoffsets=_ogg_malloc(vf->links*sizeof(*vf->dataoffsets) + 0x20);

  for(i=0;i<vf->links;i++){
    if(i==0){
      /* we already grabbed the initial header earlier.  Just set the offset */
      vf->dataoffsets[i]=dataoffset;
      NvSeekHelper(pState,vf,dataoffset);

    }else{

      /* seek to the location of the initial header */

      NvSeekHelper(pState,vf,vf->offsets[i]);
      if(NvFetchHeaders(pState,vf,vf->vi+i,vf->vc+i,NULL,NULL)<0){
        vf->dataoffsets[i]=-1;
      }else{
    vf->dataoffsets[i]=vf->offset;
      }
    }

    /* fetch beginning PCM offset */

    if(vf->dataoffsets[i]!=-1){
      ogg_int64_t accumulated=0,pos;
      NvS32        lastblock=-1;
      NvS32         result;

      NvOggStreamResetSerialno(vf->os,vf->serialnos[i]);

      while(1){
    NvOggPacket op={0,0,0,0,0,0};

    ret=NvGetNextPage(pState,vf,&og,-1);
    if(ret<0)
      /* this should not be possible unless the file is
             truncated/mangled */
      break;

    if(NvOggPageSerialno(&og)!=vf->serialnos[i])
      break;

    pos=NvOggPageGranulepos(&og);

    /* count blocksizes of all frames in the page */
    NvOggStreamPagein(vf->os,&og);
    while(1){
      result=NvOggStreamPacketout(vf->os,&op);
      if(!result) break;
      if(result>0){ /* ignore holes */
        NvS32 thisblock=NvVorbisPacketBlocksize(vf->vi+i,&op);
        if(lastblock!=-1)
          accumulated+=(lastblock+thisblock)>>2;
        lastblock=thisblock;
      }
    }
    NvOggPacketRelease(&op);

    if(pos!=-1){
      /* pcm offset of last packet on the first audio page */
      accumulated= pos-accumulated;
      break;
    }
      }

      /* less than zero?  This is a stream with samples trimmed off
         the beginning, a normal occurrence; set the offset to zero */
      if(accumulated<0)accumulated=0;

      vf->pcmlengths[i*2]=accumulated;
    }

    /* get the PCM length of this link. To do this,
       get the last page of the stream */
    {
      ogg_int64_t end=vf->offsets[i+1];
      NvSeekHelper(pState,vf,end);

      while(1){
    ret=NvGetPrevPage(pState,vf,&og);
    if(ret<0){
      /* this should not be possible */
      NvVorbisInfoClear(vf->vi+i);
      NvVorbisCommentClear(vf->vc+i);
      break;
    }
    if(NvOggPageGranulepos(&og)!=-1){
      vf->pcmlengths[i*2+1]=NvOggPageGranulepos(&og)-vf->pcmlengths[i*2];
      break;
    }
    vf->offset=ret;
      }
    }
  }
  NvOggPageRelease(&og);
}

/* if, eg, 64 bit stdio is configured by default, this will build with
   fseek64 */
 static NvS32 NvFseek64Wrap(FILE *f,ogg_int64_t off,NvS32 whence){
  if(f==NULL)return(-1);
  return fseek(f,(NvS32)off,whence);
}

 NvS32 NvOvOpen1(NvOggParser * pState,OggVorbis_File *vf,NvS8 *initial,
             NvS32 ibytes, ov_callbacks callbacks){
  NvS32 ret;
  NvError status; 
  
  //NvError err = NvOsFseek(f, 0, NvOsSeek_Cur);

  status = pState->pPipe->cpipe.SetPosition(pState->cphandle, 0, CP_OriginCur);   
  
  if(!status)
  {
      vf->seekable=1;/* doing nothing as offsettest is already -1 */
  }
  //NvS32 offsettest=(f?callbacks.seek_func(f,0,SEEK_CUR):-1);

  //memset(vf,0,sizeof(*vf));
  vf->datasource=pState->cphandle ; //f;
  vf->callbacks = callbacks;

  /* init the framing state */
  vf->oy=NvOggSyncCreate();

  /* perhaps some data was previously read into a buffer for testing
     against other stream types.  Allow initialization from this
     previously read data (as we may be reading from a non-seekable
     stream) */
  if(initial){
    NvU8 *buffer=NvOggSyncBufferin(vf->oy,ibytes);
    NvOsMemcpy(buffer, initial, ibytes);
    //memcpy(buffer,initial,ibytes);
    NvOggSyncWrote(vf->oy,ibytes);
  }

  /* can we seek? Stevens suggests the seek test was portable */
  //if(offsettest!=-1)vf->seekable=1;

  /* No seeking yet; Set up a 'single' (current) logical bitstream
     entry for partial open */
  vf->links=1;
  vf->vi=_ogg_calloc(vf->links,sizeof(*vf->vi));
  vf->vc=_ogg_calloc(vf->links,sizeof(*vf->vc));
  vf->os=NvOggStreamCreate(-1); /* fill in the serialno later */

  /* Try to fetch the headers, maintaining all the storage */
  if((ret=NvFetchHeaders(pState,vf,vf->vi,vf->vc,&vf->current_serialno,NULL))<0){
    vf->datasource=NULL;
    NvOvClear(vf);
  }else if(vf->ready_state < PARTOPEN)
    vf->ready_state=PARTOPEN;
  return(ret);
}

/* reap the chain, pull the ripcord */
 void NvVorbisBlockRipcord(vorbis_block *vb){
  /* reap the chain */
  struct alloc_chain *reap=vb->reap;
  while(reap){
    struct alloc_chain *next=reap->next;
    _ogg_free(reap->ptr);
    memset(reap,0,sizeof(*reap));
    _ogg_free(reap);
    reap=next;
  }
  /* consolidate storage */
  if(vb->totaluse){
    vb->localstore=_ogg_realloc(vb->localstore,vb->totaluse+vb->localalloc);
    vb->localalloc+=vb->totaluse;
    vb->totaluse=0;
  }

  /* pull the ripcord */
  vb->localtop=0;
  vb->reap=NULL;
}

 NvS32 NvVorbisBlockClear(vorbis_block *vb){
  NvVorbisBlockRipcord(vb);
  if(vb->localstore) {
      _ogg_free(vb->localstore);
      vb->localstore = NULL;
  }

  memset(vb,0,sizeof(*vb));
  return(0);
}

/* clear out the OggVorbis_File struct */
NvS32 NvOvClear(OggVorbis_File *vf){
  if(vf){

    NvVorbisBlockClear(&vf->vb);
    // even thought this is used by the decoder, clearing
    // here is of no harm -- SG
    NvVorbisDspClear(&vf->vd);
    NvOggStreamDestroyEnd(vf->os);

    if(vf->vi && vf->links){
      NvS32 i;
      for(i=0;i<vf->links;i++){
    NvVorbisInfoClear(vf->vi+i);
    NvVorbisCommentClear(vf->vc+i);
      }
      _ogg_free(vf->vi);
      _ogg_free(vf->vc);
    }

    if(vf->dataoffsets)_ogg_free(vf->dataoffsets);
    if(vf->pcmlengths)_ogg_free(vf->pcmlengths);
    if(vf->serialnos)_ogg_free(vf->serialnos);
    if(vf->offsets)_ogg_free(vf->offsets);
    NvOggSyncDestroy(vf->oy);
    NvOggPageRelease(vf->og); 
    if (vf->og)_ogg_free(vf->og);

    if(vf->datasource)(vf->callbacks.close_func)(vf->datasource);
    memset(vf,0,sizeof(*vf));
  }
#ifdef DEBUG_LEAKS
  _VDBG_dump();
#endif
  return(0);
}

/* inspects the OggVorbis file and finds/documents all the logical
   bitstreams contained in it.  Tries to be tolerant of logical
   bitstream sections that are truncated/woogie.

   return: -1) error
            0) OK
*/
NvS32 NvOvopen(NvOggParser *pState,OggVorbis_File *vf,NvS8 *initial,NvS32 ibytes){

  ov_callbacks callbacks = {
    (size_t (*)(void *, size_t, size_t, void *))  fread,
    (int (*)(void *, ogg_int64_t, NvS32))              NvFseek64Wrap,
    (int (*)(void *))                             fclose,
    (long (*)(void *))                            ftell
  };
 
   return NvOvOpenCallbacks(pState, vf, initial, ibytes, callbacks);
}

/* Only partially open the vorbis file; test for Vorbisness, and load
   the headers for the first chain.  Do not seek (although test for
   seekability).  Use ov_test_open to finish opening the file, else
   NvOvClear to close/free it. Same return codes as open. */

NvS32 NvOvTestCallbacks(void *f,OggVorbis_File *vf,NvS8 *initial,NvS32 ibytes,
    ov_callbacks callbacks)
{
  return NvOvOpen1(f,vf,initial,ibytes,callbacks);
}
/*
NvS32 ov_test(FILE *f,OggVorbis_File *vf,NvS8 *initial,NvS32 ibytes){
  ov_callbacks callbacks = {
    (size_t (*)(void *, size_t, size_t, void *))  fread,
    (NvS32 (*)(void *, ogg_int64_t, NvS32))              NvFseek64Wrap,
    (NvS32 (*)(void *))                             fclose,
    (NvS32 (*)(void *))                            ftell
  };

  return NvOvTestCallbacks((void *)f, vf, initial, ibytes, callbacks);
}

int ov_test_open(OggVorbis_File *vf){
  if(vf->ready_state!=PARTOPEN)return(OV_EINVAL);
  return _ov_open2(vf);
}

// How many logical bitstreams in this physical bitstream?
NvS32 ov_streams(OggVorbis_File *vf){
  return vf->links;
}
*/
/* Is the FILE * associated with vf seekable? */
NvS32 NvOvSeekable(OggVorbis_File *vf){
  return vf->seekable;
}

/* returns the bitrate for a given logical bitstream or the entire
   physical bitstream.  If the file is open for random access, it will
   find the *actual* average bitrate.  If the file is streaming, it
   returns the nominal bitrate (if set) else the average of the
   upper/lower bounds (if set) else -1 (unset).

   If you want the actual bitrate field settings, get them from the
   vorbis_info structs */
/*
static long ov_bitrate(OggVorbis_File *vf,int i){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(i>=vf->links)return(OV_EINVAL);
  if(!vf->seekable && i!=0)return(ov_bitrate(vf,0));
  if(i<0){
    ogg_int64_t bits=0;
    int i;
    for(i=0;i<vf->links;i++)
      bits+=(vf->offsets[i+1]-vf->dataoffsets[i])*8;
    // This once read: return(rint(bits/ov_time_total(vf,-1)));
     // gcc 3.x on x86 miscompiled this at optimisation level 2 and above,
     // so this is slightly transformed to make it work.
     //
    return(bits*1000/ov_time_total(vf,-1));
  }else{
    if(vf->seekable){
      // return the actual bitrate
      return((vf->offsets[i+1]-vf->dataoffsets[i])*8000/ov_time_total(vf,i));
    }else{
      // return nominal if set
      if(vf->vi[i].bitrate_nominal>0){
    return vf->vi[i].bitrate_nominal;
      }else{
    if(vf->vi[i].bitrate_upper>0){
      if(vf->vi[i].bitrate_lower>0){
        return (vf->vi[i].bitrate_upper+vf->vi[i].bitrate_lower)/2;
      }else{
        return vf->vi[i].bitrate_upper;
      }
    }
    return(OV_FALSE);
      }
    }
  }
}
*/

/* returns the actual bitrate since last call.  returns -1 if no
   additional data to offer since last call (or at beginning of stream),
   EINVAL if stream is only partially open
*/
/*
long ov_bitrate_instant(OggVorbis_File *vf){
  int link=(vf->seekable?vf->current_link:0);
  long ret;
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(vf->samptrack==0)return(OV_FALSE);
  ret=vf->bittrack/vf->samptrack*vf->vi[link].rate;
  vf->bittrack=0;
  vf->samptrack=0;
  return(ret);
}

// Guess
long ov_serialnumber(OggVorbis_File *vf,int i){
  if(i>=vf->links)return(ov_serialnumber(vf,vf->links-1));
  if(!vf->seekable && i>=0)return(ov_serialnumber(vf,-1));
  if(i<0){
    return(vf->current_serialno);
  }else{
    return(vf->serialnos[i]);
  }
}
*/
/* returns: total raw (compressed) length of content if i==-1
            raw (compressed) length of that logical bitstream for i==0 to n
        OV_EINVAL if the stream is not seekable (we can't know the length)
        or if stream is only partially open
*/
/*
ogg_int64_t ov_raw_total(OggVorbis_File *vf,int i){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable || i>=vf->links)return(OV_EINVAL);
  if(i<0){
    ogg_int64_t acc=0;
    int i;
    for(i=0;i<vf->links;i++)
      acc+=ov_raw_total(vf,i);
    return(acc);
  }else{
    return(vf->offsets[i+1]-vf->offsets[i]);
  }
}
*/

/* returns: total PCM length (samples) of content if i==-1 PCM length
        (samples) of that logical bitstream for i==0 to n
        OV_EINVAL if the stream is not seekable (we can't know the
        length) or only partially open
*/
ogg_int64_t NvOvPcmTotal(OggVorbis_File *vf,NvS32 i){
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable || i>=vf->links)return(OV_EINVAL);
  if(i<0){
    ogg_int64_t acc=0;
    NvS32 i;
    for(i=0;i<vf->links;i++)
      acc+=NvOvPcmTotal(vf,i);
    return(acc);
  }else{
    return(vf->pcmlengths[i*2+1]);
  }
}


/* returns: total milliseconds of content if i==-1
            milliseconds in that logical bitstream for i==0 to n
        OV_EINVAL if the stream is not seekable (we can't know the
        length) or only partially open
*/

ogg_int64_t NvOvTimeTotal(OggVorbis_File *vf,int i){
  ogg_int64_t total_time = 0;
  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable || i>=vf->links)return(OV_EINVAL);
  if(i<0){
    ogg_int64_t acc=0;
    int i;
    for(i=0;i<vf->links;i++)
      acc+=NvOvTimeTotal(vf,i);
    return(acc);
  }else{  
    if(vf->vi[i].rate !=0)
        total_time = ((ogg_int64_t)vf->pcmlengths[i*2+1])*1000/vf->vi[i].rate;    
     return total_time;        
  }
}


/* seek to an offset relative to the *compressed* data. This also
   scans packets to update the PCM cursor. It will cross a logical
   bitstream boundary, but only if it can't get any packets out of the
   tail of the bitstream we seek to (so no surprises).

   returns zero on success, nonzero on failure */

 NvS32 NvOvRawSeek(NvOggParser * pState,OggVorbis_File *vf,ogg_int64_t pos){
  ogg_stream_state *work_os=NULL;
  ogg_page og={0,0,0,0};
  NvOggPacket op={0,0,0,0,0,0};

  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable)
    return(OV_ENOSEEK); /* don't dump machine if we can't seek */

  if(pos<0 || pos>vf->end)return(OV_EINVAL);

  /* don't yet clear out decoding machine (if it's initialized), in
     the case we're in the same link.  Restart the decode lapping, and
     let _fetch_and_process_packet deal with a potential bitstream
     boundary */
  vf->pcm_offset=-1;
  NvOggStreamResetSerialno(vf->os,
                vf->current_serialno); /* must set serialno */
  NvVorbisSynthesisRestart(&vf->vd);

  NvSeekHelper(pState,vf,pos);

  /* we need to make sure the pcm_offset is set, but we don't want to
     advance the raw cursor past good packets just to get to the first
     with a granulepos.  That's not equivalent behavior to beginning
     decoding as immediately after the seek position as possible.

     So, a hack.  We use two stream states; a local scratch state and
     the shared vf->os stream state.  We use the local state to
     scan, and the shared state as a buffer for later decode.

     Unfortuantely, on the last page we still advance to last packet
     because the granulepos on the last page is not necessarily on a
     packet boundary, and we need to make sure the granpos is
     correct.
  */

  {
    NvS32 lastblock=0;
    NvS32 accblock=0;
    NvS32 thisblock;
    NvS32 eosflag=0;

    work_os=NvOggStreamCreate(vf->current_serialno); /* get the memory ready */
    while(1){
      if(vf->ready_state>=STREAMSET){
    /* snarf/scan a packet if we can */
    NvS32 result=NvOggStreamPacketout(work_os,&op);

    if(result>0){

      if(vf->vi[vf->current_link].codec_setup){
        thisblock=NvVorbisPacketBlocksize(vf->vi+vf->current_link,&op);
        if(thisblock<0){
          NvOggStreamPacketout(vf->os,NULL);
          thisblock=0;
        }else{

          if(eosflag)
        NvOggStreamPacketout(vf->os,NULL);
          else
        if(lastblock)accblock+=(lastblock+thisblock)>>2;
        }

        if(op.granulepos!=-1){
          NvS32 i,link=vf->current_link;
          ogg_int64_t granulepos=op.granulepos-vf->pcmlengths[link*2];
          if(granulepos<0)granulepos=0;

          for(i=0;i<link;i++)
        granulepos+=vf->pcmlengths[i*2+1];
          vf->pcm_offset=granulepos-accblock;
          break;
        }
        lastblock=thisblock;
        continue;
      }else
        NvOggStreamPacketout(vf->os,NULL);
    }
      }

      if(!lastblock){
    if(NvGetNextPage(pState,vf,&og,-1)<0){
      vf->pcm_offset=NvOvPcmTotal(vf,-1);
      break;
    }
      }else{
    /* huh?  Bogus stream with packets but no granulepos */
    vf->pcm_offset=-1;
    break;
      }

      /* has our decoding just traversed a bitstream boundary? */
      if(vf->ready_state>=STREAMSET)
    if(vf->current_serialno!=NvOggPageSerialno(&og)){
      //_decode_clear(vf); /* clear out stream state */
      //otmsg->decClearFlag = 1;
      NvOggStreamDestroy(work_os);
    }

      if(vf->ready_state<STREAMSET){
    int link;

    vf->current_serialno=NvOggPageSerialno(&og);
    for(link=0;link<vf->links;link++)
      if(vf->serialnos[link]==vf->current_serialno)break;
    if(link==vf->links)
      goto seek_error; /* sign of a bogus stream.  error out,
                  leave machine uninitialized */

    vf->current_link=link;

    NvOggStreamResetSerialno(vf->os,vf->current_serialno);
    NvOggStreamResetSerialno(work_os,vf->current_serialno);
    vf->ready_state=STREAMSET;

      }

      {
    ogg_page dup;
    NvOggPageDup(&dup,&og);
    eosflag=NvOggPageEos(&og);
    NvOggStreamPagein(vf->os,&og);
    NvOggStreamPagein(work_os,&dup);
      }
    }
  }

  NvOggPacketRelease(&op);
  NvOggPageRelease(&og);
  NvOggStreamDestroy(work_os);
  vf->bittrack=0;
  vf->samptrack=0;
  return(0);

 seek_error:
  NvOggPacketRelease(&op);
  NvOggPageRelease(&og);

  /* dump the machine so we're in a known state */
  vf->pcm_offset=-1;
  NvOggStreamDestroy(work_os);
  //_decode_clear(vf);
  //otmsg->decClearFlag = 1;
  return OV_EBADLINK;
}

 NvS32 NvOpenSeekable2(NvOggParser * pState,OggVorbis_File *vf){
  ogg_uint32_t serialno=vf->current_serialno;
  ogg_uint32_t tempserialno;
  ogg_int64_t dataoffset = vf->offset, tellpos = 0;
  ogg_int32_t end;
  ogg_page og={0,0,0,0};
  NvError errtell;

  /* we're partially open and have a first link header state in
     storage in vf */
  /* we can seek, so set out learning all about this file */
  //NvError err = NvOsFseek(vf->datasource, 0, NvOsSeek_End);
  NvError err = pState->pPipe->SetPosition64(pState->cphandle, 0, CP_OriginEnd);   
  if(err) {/* doing nothing */}
  //(vf->callbacks.seek_func)(vf->datasource,0,SEEK_END);
  //errtell = NvOsFtell(vf->datasource, (NvU64 *)&tellpos);

  errtell = pState->pPipe->GetPosition64(pState->cphandle, (CPuint64*)&tellpos);  
  if(errtell) {/* doing nothing */}
  vf->offset=vf->end=tellpos;//(vf->callbacks.tell_func)(vf->datasource);

  /* We get the offset for the last page of the physical bitstream.
     Most OggVorbis files will contain a single logical bitstream */
  //end=(NvS32)NvGetPrevPage(pState,vf,&og);
  end=(NvS32)NvGetPrevPageOpen2(pState,vf,&og);
  if(end<0)return(end);

  /* more than one logical bitstream? */
  tempserialno=NvOggPageSerialno(&og);
  NvOggPageRelease(&og);

  if(tempserialno!=serialno){

    /* Chained bitstream. Bisect-search each logical bitstream
       section.  Do so based on serial number only */
    if(NvBisectForwardSerialno(pState,vf,0,0,end+1,serialno,0)<0)return(OV_EREAD);

  }else{

    /* Only one logical bitstream */
    if(NvBisectForwardSerialno(pState,vf,0,end,end+1,serialno,0))return(OV_EREAD);

  }

  /* the initial header memory is referenced by vf after; don't free it */
  NvPrefetchAllHeaders(pState,vf,dataoffset);
  return(NvOvRawSeek(pState,vf,(ogg_int64_t)0));
}

 NvS32 NvOvOpen2(NvOggParser * pState,OggVorbis_File *vf){
  if(vf->ready_state < OPENED)
    vf->ready_state=OPENED;
  if(vf->seekable){
    NvS32 ret=NvOpenSeekable2(pState,vf);
    if(ret){
      vf->datasource=NULL;
      NvOvClear(vf);
    }
    return(ret);
  }
  return 0;
}

NvS32 NvOvOpenCallbacks(NvOggParser *pState,OggVorbis_File *vf,NvS8 *initial,NvS32 ibytes, ov_callbacks callbacks)
{

  NvS32 ret=NvOvOpen1(pState,vf,initial,ibytes,callbacks);
  if(ret)return ret;
  return NvOvOpen2(pState,vf);
  
}

/*  link:   -1) return the vorbis_info struct for the bitstream section
                currently being decoded
           0-n) to request information for a specific bitstream section

    In the case of a non-seekable bitstream, any call returns the
    current bitstream.  NULL in the case that the machine is not
    initialized */

vorbis_info *NvOvInfo(OggVorbis_File *vf,NvS32 link){
  if(vf->seekable){
    if(link<0)
      if(vf->ready_state>=STREAMSET)
    return vf->vi+vf->current_link;
      else
      return vf->vi;
    else
      if(link>=vf->links)
    return NULL;
      else
    return vf->vi+link;
  }else{
    return vf->vi;
  }
}

/* grr, strong typing, grr, no templates/inheritence, grr */
vorbis_comment *NvOvComment(OggVorbis_File *vf,NvS32 link){
  if(vf->seekable){
    if(link<0)
      if(vf->ready_state>=STREAMSET)
    return vf->vc+vf->current_link;
      else
    return vf->vc;
    else
      if(link>=vf->links)
    return NULL;
      else
    return vf->vc+link;
  }else{
    return vf->vc;
  }
}

#if 0
/*
   This function reads CHUNKSIZE bytes of data from the file everytime.
   That is again buffered to 'ogginbuf'. ogginbuf is parsed to find a
   page. The found page is returned through og
*/
#ifdef DIFFERENT_STRUCT_FOR_READ_AND_DECODE
NvS32 oggfread(OggVorbis_File *vf, ogg_page *og, NvS32 readp, NvS32 spanp) {
#else
NvS32 oggfread(OggVorbis_File *vf, NvS32 readp, NvS32 spanp) {
    ogg_page og = {0,0,0,0};
#endif
    ogg_packet op={0,0,0,0,0,0};
    NvS32 ret = 0;

    if(vf->ready_state>=OPENED){
#ifdef DIFFERENT_STRUCT_FOR_READ_AND_DECODE
        if((ret=(NvS32)_get_next_page(vf,og,-1))<0){
#else
        if((ret=(NvS32)_get_next_page(vf,&og,-1))<0){
#endif
            ret=OV_EOF; /* eof. leave unitialized */
            goto read_cleanup;
        }

        /* bitrate tracking; add the header's bytes here, the body bytes
        are done by packet above */
#ifdef DIFFERENT_STRUCT_FOR_READ_AND_DECODE
        vf->bittrack+=og->header_len*8;
#else
        vf->bittrack+=og.header_len*8;
#endif

        /* has our decoding just traversed a bitstream boundary? */
        if(vf->ready_state==INITSET){
#ifdef DIFFERENT_STRUCT_FOR_READ_AND_DECODE
            if(vf->current_serialno!=NvOggPageSerialno(og)){
#else
            if(vf->current_serialno!=NvOggPageSerialno(&og)){
#endif
                /* This should be messaged to the decoder for clearing its
                   own state in case of bitstream boundary.For now, return
                   a suitable value to the app
                */
                //_decode_clear(vf);
                ret = OV_DECCLEAR;

                // since we use a file input, it is seekable. so this
                // check is not required
                //if(!vf->seekable){
                //  vorbis_info_clear(vf->vi);
                //  vorbis_comment_clear(vf->vc);
                //}
            }
        }
    }

read_cleanup:
    NvOggPacketRelease(&op);

    if(ret==OV_EOF)
        return(0);
    if(ret<=0)
        return(ret);

    return ret;
}
#endif //#if 0

/*
    This is one time read for getting all the file-level control
    parameters. Those are maintained in parser. Also, they are
    sent to decoder which reads & keeps it's required parameters
*/
NvS32 NvOggFirstFread(NvOggParser *pState,OggVorbis_File *vf, ogg_page *og, NvS32 readp, NvS32 spanp) {

    //ogg_page og = {0,0,0,0};
    NvOggPacket op={0,0,0,0,0,0};
    NvS32 ret = 0;
        if(vf->ready_state>=OPENED){
            if(!readp){
                ret=0;
                goto read_cleanup;
            }

            if((ret=(NvS32)NvGetNextPage(pState,vf,og,-1))<0){
                ret=OV_EOF; /* eof. leave unitialized */
                goto read_cleanup;
            }

            /* bitrate tracking; add the header's bytes here, the body bytes
            are done by packet above */
            vf->bittrack+=og->header_len*8;

            /* has our decoding just traversed a bitstream boundary? */
            if(vf->ready_state==INITSET){
                if(vf->current_serialno!=NvOggPageSerialno(og)){
                    if(!spanp){
                        ret=OV_EOF;
                        goto read_cleanup;
                    }
                    /* There is no point in having this because the decoder
                       would not have started decoding. Moreover hf here is
                       read structure */
                    //_decode_clear(vf);
                }
            }
        }

        /* Do we need to load a new machine before submitting the page? */
        /* This is different in the seekable and non-seekable cases.

        In the seekable case, we already have all the header
        information loaded and cached; we just initialize the machine
        with it and continue on our merry way.

        In the non-seekable (streaming) case, we'll only be at a
        boundary if we just left the previous logical bitstream and
        we're now nominally at the header of the next bitstream
        */

        if(vf->ready_state!=INITSET){
            NvS32 link;

            if(vf->ready_state<STREAMSET){
                if(vf->seekable){
                    vf->current_serialno=NvOggPageSerialno(og);

                    /* match the serialno to bitstream section.  We use this rather than
                    offset positions to avoid problems near logical bitstream
                    boundaries */
                    for(link=0;link<vf->links;link++)
                        if(vf->serialnos[link]==vf->current_serialno)
                            break;

                    if(link==vf->links){
                        ret=OV_EBADLINK; /* sign of a bogus stream.  error out,
                        leave machine uninitialized */
                        goto read_cleanup;
                    }

                    vf->current_link=link;

                    NvOggStreamResetSerialno(vf->os,vf->current_serialno);
                    vf->ready_state=STREAMSET;

                }
            } //if(vf->ready_state<STREAMSET)
        } //if(vf->ready_state!=INITSET)
        //ogg_stream_pagein(vf->os,&og);

read_cleanup:
    NvOggPacketRelease(&op);
    //NvOggPageRelease(&og);

    if(ret==OV_EOF)
        return(0);
    if(ret<=0)
        return(ret);

    vf->readflag = 0;
    return ret;
}

void *NvMyMalloc(NvU32 cnt) {
#ifdef DBG_BUF_PRNT
    void *lptr = malloc(cnt);
    //printf("\nALLOCATED %x OF %ud BYTES\n", lptr, cnt);
    mallocTotal += cnt;
    return lptr;
#else
    void *lptr = NvOsAlloc(cnt);
    //NvTestPrintf("\nALLOCATED %x OF %ud BYTES\n", lptr, cnt);
    return lptr;
#endif
}

void *NvMyCalloc(NvU32 init, NvU32 cnt) {
#ifdef DBG_BUF_PRNT
    void *lptr = calloc(init, cnt);
    //printf("\nALLOCATED %x OF %ud BYTES\n", lptr, cnt);
    mallocTotal += cnt;
    return lptr;
#else
    void *ptr = NvOsAlloc(init*cnt);
    if(ptr)
    {
        NvOsMemset(ptr, 0, (init*cnt));
    }
    return ptr;//calloc(init, cnt);
#endif
}

void *NvMyRealloc(void *ptr, NvU32 cnt) {
#ifdef DBG_BUF_PRNT
    void *lptr = realloc(ptr, cnt);
    //printf("\nRE-ALLOCATED %x OF %ud BYTES\n", lptr, cnt);
    return lptr;
#else
    return NvOsRealloc(ptr, (size_t)cnt);
    //return NvOsRealloc(ptr, cnt);
#endif
}

void NvMyFree(void *ptr) {
    //NvTestPrintf("\n*******FREED %x *********\n", ptr);
#ifdef DBG_BUF_PRNT
    //printf("\n*******FREED %x *********\n", ptr);
    free(ptr);
#else
    NvOsFree(ptr);
    //free(ptr);
#endif
}

