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

 function: maintain the info structure, info <-> header packets

 ********************************************************************/

/* general handling of the header and the vorbis_info structure (and
   substructures) */

#include <stdlib.h>
#include <string.h>
#include "nvmm_oggdefines.h"
#include "nvmm_ogg.h"
#include "nvmm_oggprototypes.h"
#include "nvmm_oggbookunpack.h"

/* helpers */
static void NvVReadstring(oggpack_buffer *o,char *buf,NvS32 bytes){
  while(bytes--){
    *buf++=(NvS8)NvOggpackRead(o,8);
  }
}

/* This is more or less the same as strncasecmp - but that doesn't exist
 * everywhere, and this is a fairly trivial function, so we include it */
/*static NvS32 tagcompare(const char *s1, const char *s2, NvS32 n){
  NvS32 c=0;
  while(c < n){
    if(toupper(s1[c]) != toupper(s2[c]))
      return !0;
    c++;
  }
  return 0;
}*/

/*static char *vorbis_comment_query(vorbis_comment *vc, char *tag, NvS32 count){
  NvS32 i;
  NvS32 found = 0;
  NvS32 taglen = strlen(tag)+1; // +1 for the = we append
  char *fulltag = (char *)alloca(taglen+ 1);

  strcpy(fulltag, tag);
  strcat(fulltag, "=");

  for(i=0;i<vc->comments;i++){
    if(!tagcompare(vc->user_comments[i], fulltag, taglen)){
      if(count == found)
    // We return a pointer to the data, not a copy
        return vc->user_comments[i] + taglen;
      else
    found++;
    }
  }
  return NULL; // didn't find anything
}

static int vorbis_comment_query_count(vorbis_comment *vc, char *tag){
  int i,count=0;
  int taglen = strlen(tag)+1; // +1 for the = we append
  char *fulltag = (char *)alloca(taglen+1);
  strcpy(fulltag,tag);
  strcat(fulltag, "=");

  for(i=0;i<vc->comments;i++){
    if(!tagcompare(vc->user_comments[i], fulltag, taglen))
      count++;
  }

  return count;
}*/

void NvVorbisCommentClear(vorbis_comment *vc){
  if(vc){
    NvS32 i;
    for(i=0;i<vc->comments;i++)
      if(vc->user_comments[i])_ogg_free(vc->user_comments[i]);
    if(vc->user_comments)_ogg_free(vc->user_comments);
    if(vc->comment_lengths)_ogg_free(vc->comment_lengths);
    if(vc->vendor)_ogg_free(vc->vendor);
    memset(vc,0,sizeof(*vc));
  }
}

/* blocksize 0 is guaranteed to be short, 1 is guarantted to be NvS32.
   They may be equal, but short will never ge greater than NvS32 */
/*static int vorbis_info_blocksize(vorbis_info *vi,int zo){
  NvCodecSetupInfo *ci = (NvCodecSetupInfo *)vi->codec_setup;
  return ci ? ci->blocksizes[zo] : -1;
}*/

void NvVorbisInfoClear(vorbis_info *vi){
  NvCodecSetupInfo     *ci=(NvCodecSetupInfo *)vi->codec_setup;
  NvS32 i;

  if(ci){

    for(i=0;i<ci->modes;i++)
      if(ci->mode_param[i])_ogg_free(ci->mode_param[i]);

      for(i=0;i<ci->maps;i++) {/* unpack does the range checking */
          if(ci->map_param[i])
#ifdef CODEBOOK_INFO_CHANGE
            NvMapping0FreeInfo(ci->map_param[i]);
#else
            _mapping_P[ci->map_type[i]]->free_info(ci->map_param[i]);
#endif
      }
      for(i=0;i<ci->floors;i++) { /* unpack does the range checking */
          if(ci->floor_param[i]) {
#ifdef CODEBOOK_INFO_CHANGE
              switch(ci->floor_type[i]) {
              case 0:
                  NvFloor0FreeInfo(ci->floor_param[i]);
                  break;
              case 1:
                  NvFloor1FreeInfo(ci->floor_param[i]);
                  break;
              default:
                  /* error not handled here */
                  break;
              }
#else
              _floor_P[ci->floor_type[i]]->free_info(ci->floor_param[i]);
#endif
          }
      }
      for(i=0;i<ci->residues;i++) { /* unpack does the range checking */
          if(ci->residue_param[i]) {
#ifdef CODEBOOK_INFO_CHANGE
            NvRes0FreeInfo(ci->residue_param[i]);
#else
            _residue_P[ci->residue_type[i]]->free_info(ci->residue_param[i]);
#endif
          }
      }

    for(i=0;i<ci->books;i++){
      if(ci->book_param[i]){
    /* knows if the book was not alloced */
    NvVorbisStaticbookDestroy(ci->book_param[i]);
      }
      if(ci->fullbooks)
    NvVorbisBookClear(ci->fullbooks+i);
    }
    if(ci->fullbooks)
    _ogg_free(ci->fullbooks);

    _ogg_free(ci);
  }

  memset(vi,0,sizeof(*vi));
}

/* Header packing/unpacking ********************************************/

static NvS32 NvVorbisUnpackInfo(vorbis_info *vi,oggpack_buffer *opb){
  NvCodecSetupInfo     *ci=(NvCodecSetupInfo *)vi->codec_setup;
  if(!ci)return(OV_EFAULT);

  vi->version=NvOggpackRead(opb,32);
  if(vi->version!=0)return(OV_EVERSION);

  vi->channels=NvOggpackRead(opb,8);
  vi->rate=NvOggpackRead(opb,32);

  vi->bitrate_upper=NvOggpackRead(opb,32);
  vi->bitrate_nominal=NvOggpackRead(opb,32);
  vi->bitrate_lower=NvOggpackRead(opb,32);

  ci->blocksizes[0]=1<<NvOggpackRead(opb,4);
  ci->blocksizes[1]=1<<NvOggpackRead(opb,4);

  if(vi->rate<1)goto err_out;
  if(vi->channels<1)goto err_out;
  if(ci->blocksizes[0]<64)goto err_out;
  if(ci->blocksizes[1]<ci->blocksizes[0])goto err_out;
  if(ci->blocksizes[1]>8192)goto err_out;

  if(NvOggpackRead(opb,1)!=1)goto err_out; /* EOP check */

  return(0);
 err_out:
  NvVorbisInfoClear(vi);
  return(OV_EBADHEADER);
}

static NvS32 NvVorbisUnpackComment(vorbis_comment *vc,oggpack_buffer *opb){
  NvS32 i;
  NvS32 vendorlen=NvOggpackRead(opb,32);
  if(vendorlen<0)goto err_out;
  vc->vendor=(char *)_ogg_calloc(vendorlen+1,1);
  NvVReadstring(opb,vc->vendor,vendorlen);
  vc->comments=NvOggpackRead(opb,32);
  if(vc->comments<0)goto err_out;
  vc->user_comments=(char **)_ogg_calloc(vc->comments+1,sizeof(*vc->user_comments));
  vc->comment_lengths=(NvS32 *)_ogg_calloc(vc->comments+1, sizeof(*vc->comment_lengths));

  for(i=0;i<vc->comments;i++){
    NvS32 len=NvOggpackRead(opb,32);
    if(len<0)goto err_out;
    vc->comment_lengths[i]=len;
    vc->user_comments[i]=(char *)_ogg_calloc(len+1,1);
    NvVReadstring(opb,vc->user_comments[i],len);
  }
  if(NvOggpackRead(opb,1)!=1)goto err_out; /* EOP check */

  return(0);
 err_out:
  NvVorbisCommentClear(vc);
  return(OV_EBADHEADER);
}

/* all of the real encoding details are here.  The modes, books,
   everything */
static NvS32 NvVorbisUnpackBooks(vorbis_info *vi,oggpack_buffer *opb){
  NvCodecSetupInfo     *ci=(NvCodecSetupInfo *)vi->codec_setup;
  NvS32 i;
  if(!ci)return(OV_EFAULT);

  /* codebooks */
  ci->books=NvOggpackRead(opb,8)+1;
  /*ci->book_param=_ogg_calloc(ci->books,sizeof(*ci->book_param));*/
  for(i=0;i<ci->books;i++){
    ci->book_param[i]=(NvStaticCodebook *)_ogg_calloc(1,sizeof(*ci->book_param[i]));
    if(NvVorbisStaticbookUnpack(opb,ci->book_param[i]))goto err_out;
  }

  /* time backend settings */
  ci->times=NvOggpackRead(opb,6)+1;
  /*ci->time_type=_ogg_malloc(ci->times*sizeof(*ci->time_type));*/
  /*ci->time_param=_ogg_calloc(ci->times,sizeof(void *));*/
  for(i=0;i<ci->times;i++){
    ci->time_type[i]=NvOggpackRead(opb,16);
    if(ci->time_type[i]<0 || ci->time_type[i]>=VI_TIMEB)goto err_out;
    /* ci->time_param[i]=_time_P[ci->time_type[i]]->unpack(vi,opb);
       Vorbis I has no time backend */
    /*if(!ci->time_param[i])goto err_out;*/
  }

  /* floor backend settings */
  ci->floors=NvOggpackRead(opb,6)+1;
  /*ci->floor_type=_ogg_malloc(ci->floors*sizeof(*ci->floor_type));*/
  /*ci->floor_param=_ogg_calloc(ci->floors,sizeof(void *));*/
  for(i=0;i<ci->floors;i++){
    ci->floor_type[i]=NvOggpackRead(opb,16);
    if(ci->floor_type[i]<0 || ci->floor_type[i]>=VI_FLOORB)goto err_out;
#ifdef CODEBOOK_INFO_CHANGE

    switch(ci->floor_type[i]) {
        case 0:
            ci->floor_param[i] = NvFloor0Unpack(vi,opb);
            break;
        case 1:
            ci->floor_param[i] = NvFloor1Unpack(vi,opb);
            break;
        case 2:
            ci->floor_param[i] = NvRes0Unpack(vi,opb);
            break;
        default:
            goto err_out;
    }

#else
    ci->floor_param[i]=_floor_P[ci->floor_type[i]]->unpack(vi,opb);
#endif

    if(!ci->floor_param[i])goto err_out;
  }

  /* residue backend settings */
  ci->residues=NvOggpackRead(opb,6)+1;
  /*ci->residue_type=_ogg_malloc(ci->residues*sizeof(*ci->residue_type));*/
  /*ci->residue_param=_ogg_calloc(ci->residues,sizeof(void *));*/
  for(i=0;i<ci->residues;i++){
    ci->residue_type[i]=NvOggpackRead(opb,16);
    if(ci->residue_type[i]<0 || ci->residue_type[i]>=VI_RESB)goto err_out;

#ifdef CODEBOOK_INFO_CHANGE
    ci->residue_param[i]=NvRes0Unpack(vi,opb);
#else
    ci->residue_param[i]=_residue_P[ci->residue_type[i]]->unpack(vi,opb);
#endif

    if(!ci->residue_param[i])goto err_out;
  }

  /* map backend settings */
  ci->maps=NvOggpackRead(opb,6)+1;
  /*ci->map_type=_ogg_malloc(ci->maps*sizeof(*ci->map_type));*/
  /*ci->map_param=_ogg_calloc(ci->maps,sizeof(void *));*/
  for(i=0;i<ci->maps;i++){
    ci->map_type[i]=NvOggpackRead(opb,16);
    if(ci->map_type[i]<0 || ci->map_type[i]>=VI_MAPB)goto err_out;
#ifdef CODEBOOK_INFO_CHANGE
    ci->map_param[i]=NvMapping0Unpack(vi,opb);
#else
    ci->map_param[i]=_mapping_P[ci->map_type[i]]->unpack(vi,opb);
#endif
    if(!ci->map_param[i])goto err_out;
  }

  /* mode settings */
  ci->modes=NvOggpackRead(opb,6)+1;
  /*vi->mode_param=_ogg_calloc(vi->modes,sizeof(void *));*/
  for(i=0;i<ci->modes;i++){
    ci->mode_param[i]=(NvVorbisInfoMode *)_ogg_calloc(1,sizeof(*ci->mode_param[i]));
    ci->mode_param[i]->blockflag=NvOggpackRead(opb,1);
    ci->mode_param[i]->windowtype=NvOggpackRead(opb,16);
    ci->mode_param[i]->transformtype=NvOggpackRead(opb,16);
    ci->mode_param[i]->mapping=NvOggpackRead(opb,8);

    if(ci->mode_param[i]->windowtype>=VI_WINDOWB)goto err_out;
    if(ci->mode_param[i]->transformtype>=VI_WINDOWB)goto err_out;
    if(ci->mode_param[i]->mapping>=ci->maps)goto err_out;
  }

  if(NvOggpackRead(opb,1)!=1)goto err_out; /* top level EOP check */

  return(0);
 err_out:
  NvVorbisInfoClear(vi);
  return(OV_EBADHEADER);
}

void NvVorbisCommentInit(vorbis_comment *vc){
  memset(vc,0,sizeof(*vc));
}

/* used by synthesis, which has a full, alloced vi */
void NvVorbisInfoInit(vorbis_info *vi){
  memset(vi,0,sizeof(*vi));
  vi->codec_setup=(NvCodecSetupInfo *)_ogg_calloc(1,sizeof(NvCodecSetupInfo));
}

/* The Vorbis header is in three packets; the initial small packet in
   the first page that identifies basic parameters, a second packet
   with bitstream comments and a third packet that holds the
   codebook. */

NvS32 NvVorbisSynthesisHeaderin(vorbis_info *vi,vorbis_comment *vc,NvOggPacket *op){
  oggpack_buffer opb;

  if(op && (op->packet != NULL)){
    NvOggpackReadinit(&opb,op->packet);

    /* Which of the three types of header is this? */
    /* Also verify header-ness, vorbis */
    {
      char buffer[6];
      NvS32 packtype=NvOggpackRead(&opb,8);
      memset(buffer,0,6);
      NvVReadstring(&opb,buffer,6);
      if(memcmp(buffer,"vorbis",6)){
    /* not a vorbis header */
    return(OV_ENOTVORBIS);
      }
      switch(packtype){
      case 0x01: /* least significant *bit* is read first */
    if(!op->b_o_s){
      /* Not the initial packet */
      return(OV_EBADHEADER);
    }
    if(vi->rate!=0){
      /* previously initialized info header */
      return(OV_EBADHEADER);
    }

    return(NvVorbisUnpackInfo(vi,&opb));

      case 0x03: /* least significant *bit* is read first */
    if(vi->rate==0){
      /* um... we didn't get the initial header */
      return(OV_EBADHEADER);
    }

    return(NvVorbisUnpackComment(vc,&opb));

      case 0x05: /* least significant *bit* is read first */
    if(vi->rate==0 || vc->vendor==NULL){
      /* um... we didn;t get the initial header or comments yet */
      return(OV_EBADHEADER);
    }

    return(NvVorbisUnpackBooks(vi,&opb));

      default:
    /* Not a valid vorbis header type */
    return(OV_EBADHEADER);
    //break;
      }
    }
  }
  return(OV_EBADHEADER);
}

void OggParseInfoClear(vorbis_info *vi)
{
    NvS32 i;
    NvCodecSetupInfo *ci = (NvCodecSetupInfo *)vi->codec_setup;
    
    if (ci)
    {
        for (i = 0; i < ci->books; i++)
        {
            if (ci->book_param[i])
            {
                NvVorbisStaticbookDestroy(ci->book_param[i]);
            }
        }

        for (i = 0; i < ci->modes; i++)
        {
           if (ci->mode_param[i])
               _ogg_free(ci->mode_param[i]);
        }

        _ogg_free(ci);
    } 
   
}

