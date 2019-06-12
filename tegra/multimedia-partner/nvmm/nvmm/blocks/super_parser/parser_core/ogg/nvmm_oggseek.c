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
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: stdio-based convenience library seeking

 ********************************************************************/

#include "nvmm_ogg.h"
#include "nvmm_oggprototypes.h"

#define USE_SEEK_FUNCTIONS

#ifdef USE_SEEK_FUNCTIONS

/* Page granularity seek (faster than sample granularity because we
   don't do the last bit of decode to find a specific sample).

   Seek to the last [granule marked] page preceeding the specified pos
   location, such that decoding past the returned point will quickly
   arrive at the requested position. */
//static int ov_pcm_seek_page(OggVorbis_File *vf,ogg_int64_t pos){
 NvS32 NvOvPcmSeekPage(NvOggParser *pState,OggVorbis_File *vf,ogg_int64_t pos)
 {
  int link=-1;
  ogg_int64_t result=0;
  ogg_int64_t total=NvOvPcmTotal(vf,-1);   
  ogg_page og={0,0,0,0};
  NvOggPacket op={0,0,0,0,0,0};

  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable)return(OV_ENOSEEK);
  if(pos<0 || pos>total)return(OV_EINVAL);

  /* which bitstream section does this pcm offset occur in? */
  for(link=vf->links-1;link>=0;link--){
    total-=vf->pcmlengths[link*2+1];
    if(pos>=total)break;
  }

  /* search within the logical bitstream for the page with the highest
     pcm_pos preceeding (or equal to) pos.  There is a danger here;
     missing pages or incorrect frame number information in the
     bitstream could make our task impossible.  Account for that (it
     would be an error condition) */

  /* new search algorithm by HB (Nicholas Vinen) */
  {
    ogg_int64_t end=vf->offsets[link+1];
    ogg_int64_t begin=vf->offsets[link];
    ogg_int64_t begintime = vf->pcmlengths[link*2];
    ogg_int64_t endtime = vf->pcmlengths[link*2+1]+begintime;
    ogg_int64_t target=pos-total+begintime;
    ogg_int64_t best=begin;

    while(begin<end){
      ogg_int64_t bisect;

      if(end-begin<CHUNKSIZE){
    bisect=begin;
      }else{
    /* take a (pretty decent) guess. */
    bisect=begin +
      (target-begintime)*(end-begin)/(endtime-begintime) - CHUNKSIZE;
    if(bisect<=begin)
      bisect=begin+1;
      }

      NvSeekHelper(pState,vf,bisect); 

      while(begin<end){
    // passing NULL since this call will be one time -- SG 05 dec 07
    result=NvGetNextPage(pState,vf,&og,end-vf->offset);
    if(result==OV_EREAD) goto seek_error;
    if(result<0){
      if(bisect<=begin+1)
        end=begin; /* found it */
      else{
        if(bisect==0) goto seek_error;
        bisect-=CHUNKSIZE;
        if(bisect<=begin)bisect=begin+1;
        NvSeekHelper(pState,vf,bisect); 
      }
    }else{
      ogg_int64_t granulepos=NvOggPageGranulepos(&og); 
      if(granulepos==-1)continue;
      if(granulepos<target){
        best=result;  /* raw offset of packet with granulepos */
        begin=vf->offset; /* raw offset of next page */
        begintime=granulepos;

        if(target-begintime>44100)break;
        bisect=begin; /* *not* begin + 1 */
      }else{
        if(bisect<=begin+1)
          end=begin;  /* found it */
        else{
          if(end==vf->offset){ /* we're pretty close - we'd be stuck in */
        end=result;
        bisect-=CHUNKSIZE; /* an endless loop otherwise. */
        if(bisect<=begin)bisect=begin+1;
        NvSeekHelper(pState,vf,bisect);
          }else{
        end=result;
        endtime=granulepos;
        break;
          }
        }
      }
    }
      }
    }

    /* found our page. seek to it, update pcm offset. Easier case than
       raw_seek, don't keep packets preceeding granulepos. */
    {

      /* seek */
      NvSeekHelper(pState,vf,best);
      vf->pcm_offset=-1;
      // passing NULL since this call will be only for seek -- SG 05 dec 07
      if(NvGetNextPage(pState,vf,&og,-1)<0){
    NvOggPageRelease(&og);                      //ogg_page_release_dec
    return(OV_EOF); /* shouldn't happen */
      }

      if(link!=vf->current_link){
    /* Different link; dump entire decode machine */
    //_decode_clear(vf);

    vf->current_link=link;
    vf->current_serialno=NvOggPageSerialno(&og);        //ogg_page_serialno_dec
    vf->ready_state=STREAMSET;

      }else{
        NvVorbisSynthesisRestart(&vf->vd);                        //vorbis_synthesis_restart
      }

      NvOggStreamResetSerialno(vf->os,vf->current_serialno);
      NvOggStreamPagein(vf->os,&og);

      /* pull out all but last packet; the one with granulepos */
      while(1){
    result=NvOggStreamPacketpeek(vf->os,&op);          //ogg_stream_packetpeek_dec
    if(result==0){
      /* !!! the packet finishing this page originated on a
             preceeding page. Keep fetching previous pages until we
             get one with a granulepos or without the 'continued' flag
             set.  Then just use raw_seek for simplicity. */

      NvSeekHelper(pState,vf,best);

      while(1){
        result=NvGetPrevPage(pState,vf,&og); 
        if(result<0) goto seek_error;
        if(NvOggPageGranulepos(&og)>-1 ||     
           !Nv0ggPageContinued(&og)){   
          return NvOvRawSeek(pState,vf,result);  
        }
        vf->offset=result;
      }
    }
    if(result<0){
      result = OV_EBADPACKET;
      goto seek_error;
    }
    if(op.granulepos!=-1){
      vf->pcm_offset=op.granulepos-vf->pcmlengths[vf->current_link*2];
      if(vf->pcm_offset<0)vf->pcm_offset=0;
      vf->pcm_offset+=total;
      break;
    }else
      result=NvOggStreamPacketout(vf->os,NULL);          //ogg_stream_packetout_dec
      }
    }
  }

  /* verify result */
  if(vf->pcm_offset>pos || pos>NvOvPcmTotal(vf,-1)){ 
    result=OV_EFAULT;
    goto seek_error;
  }
  vf->bittrack=0;
  vf->samptrack=0;

  NvOggPageRelease(&og);                  //ogg_page_release_dec
  NvOggPacketRelease(&op);               //ogg_packet_release_dec
  return(0);

 seek_error:

  NvOggPageRelease(&og);
  NvOggPacketRelease(&op);

  /* dump machine so we're in a known state */
  vf->pcm_offset=-1;
  //_decode_clear(vf);
  return (int)result;
}


/* page-granularity version of ov_time_seek
   returns zero on success, nonzero on failure */
//static int ov_time_seek_page(OggVorbis_File *vf,ogg_int64_t milliseconds){
 NvS32 NvOvTimeSeekPage(NvOggParser *pState,OggVorbis_File *vf,ogg_int64_t milliseconds)
 {
  /* translate time to PCM position and call ov_pcm_seek */

  int link=-1;
  ogg_int64_t pcm_total=NvOvPcmTotal(vf,-1);
  ogg_int64_t time_total=NvOvTimeTotal(vf,-1);   

  if(vf->ready_state<OPENED)return(OV_EINVAL);
  if(!vf->seekable)return(OV_ENOSEEK);
  if(milliseconds<0 || milliseconds>time_total)return(OV_EINVAL); 

  /* which bitstream section does this time offset occur in? */
  for(link=vf->links-1;link>=0;link--){
    pcm_total-=vf->pcmlengths[link*2+1];
    time_total-=NvOvTimeTotal(vf,link);  
    if(milliseconds>=time_total)break;  
  }

  /* enough information to convert time offset to pcm offset */
  {
    ogg_int64_t target=pcm_total+(milliseconds-time_total)*vf->vi[link].rate/1000;
    return(NvOvPcmSeekPage(pState,vf,target));                              //ov_pcm_seek_page
  }
}


#endif //USE_SEEK_FUNCTIONS
