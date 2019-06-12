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

  function: packing variable sized words into an octet stream

 ********************************************************************/


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nvmm_oggdefines.h"
#include "nvmm_ogg.h"
#include "nvmm_oggprototypes.h"
#include "nvmm_oggbookunpack.h"

#ifdef CODEBOOK_INFO_CHANGE

static NvU32 mask[]=
{0x00000000,0x00000001,0x00000003,0x00000007,0x0000000f,
 0x0000001f,0x0000003f,0x0000007f,0x000000ff,0x000001ff,
 0x000003ff,0x000007ff,0x00000fff,0x00001fff,0x00003fff,
 0x00007fff,0x0000ffff,0x0001ffff,0x0003ffff,0x0007ffff,
 0x000fffff,0x001fffff,0x003fffff,0x007fffff,0x00ffffff,
 0x01ffffff,0x03ffffff,0x07ffffff,0x0fffffff,0x1fffffff,
 0x3fffffff,0x7fffffff,0xffffffff };

static NvS32 NvIcount(NvU32 v){
  NvS32 ret=0;
  while(v){
    ret+=v&1;
    v>>=1;
  }
  return(ret);
}

static NvS32 NvIlog(NvU32 v){
  NvS32 ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

static NvS32 NvMIlog(NvU32 v){
  NvS32 ret=0;
  if(v)--v;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

void NvFloor0FreeInfo(vorbis_info_floor *i){
  NvVorbisInfoFloor0 *info=(NvVorbisInfoFloor0 *)i;
  if(info){
    memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}


/* spans forward and finds next byte.  Never halts */
static void NvSpanOne(oggpack_buffer *b){
  while(b->headend<1){
    if(b->head->next){
      b->count+=b->head->length;
      b->head=b->head->next;
      b->headptr=b->head->buffer->data+b->head->begin;
      b->headend=b->head->length;
    }else
      break;
  }
}

/* mark read process as having run off the end */
void NvAdvHalt(oggpack_buffer *b){
  b->headptr=b->head->buffer->data+b->head->begin+b->head->length;
  b->headend=-1;
  b->headbit=0;
}

/* spans forward, skipping as many bytes as headend is negative; if
   headend is zero, simply finds next byte.  If we're up to the end
   of the buffer, leaves headend at zero.  If we've read past the end,
   halt the decode process. */
static void NvSpan(oggpack_buffer *b){
  while(b->headend<1){
    if(b->head->next){
      b->count+=b->head->length;
      b->head=b->head->next;
      b->headptr=b->head->buffer->data+b->head->begin-b->headend;
      b->headend+=b->head->length;
    }else{
      /* we've either met the end of decode, or gone past it. halt
         only if we're past */
      if(b->headend<0 || b->headbit)
        /* read has fallen off the end */
        NvAdvHalt(b);

      break;
    }
  }
}

void NvOggpackReadinit(oggpack_buffer *b,ogg_reference *r){
  memset(b,0,sizeof(*b));

  b->tail=b->head=r;
  b->count=0;
  b->headptr=b->head->buffer->data+b->head->begin;
  b->headend=b->head->length;
  NvSpan(b);
}

NvS32 NvHaltOne(oggpack_buffer *b){
  if(b->headend<1){
    NvAdvHalt(b);
    return -1;
  }
  return 0;
}

/* bits <= 32 */
NvS32 NvOggpackRead(oggpack_buffer *b,NvS32 bits){
  NvU32 m=mask[bits];
  ogg_uint32_t ret=-1;

  bits+=b->headbit;

  if(bits >= b->headend<<3){

    if(b->headend<0)return -1;

    if(bits){
      if (NvHaltOne(b)) return -1;
      ret=*b->headptr>>b->headbit;

      if(bits>=8){
        ++b->headptr;
        --b->headend;
        NvSpanOne(b);
        if(bits>8){
          if (NvHaltOne(b)) return -1;
          ret|=*b->headptr<<(8-b->headbit);

          if(bits>=16){
            ++b->headptr;
            --b->headend;
            NvSpanOne(b);
            if(bits>16){
              if (NvHaltOne(b)) return -1;
              ret|=*b->headptr<<(16-b->headbit);

              if(bits>=24){
                ++b->headptr;
                --b->headend;
                NvSpanOne(b);
                if(bits>24){
                  if (NvHaltOne(b)) return -1;
                  ret|=*b->headptr<<(24-b->headbit);

                  if(bits>=32){
                    ++b->headptr;
                    --b->headend;
                    NvSpanOne(b);
                    if(bits>32){
                      if (NvHaltOne(b)) return -1;
                      if(b->headbit)ret|=*b->headptr<<(32-b->headbit);

                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }else{

    ret=b->headptr[0]>>b->headbit;
    if(bits>8){
      ret|=b->headptr[1]<<(8-b->headbit);
      if(bits>16){
        ret|=b->headptr[2]<<(16-b->headbit);
        if(bits>24){
          ret|=b->headptr[3]<<(24-b->headbit);
          if(bits>32 && b->headbit){
            ret|=b->headptr[4]<<(32-b->headbit);
          }
        }
      }
    }

    b->headptr+=bits/8;
    b->headend-=bits/8;
  }

  ret&=m;
  b->headbit=bits&7;
  return ret;
}

NvS32 NvVorbisPacketBlocksize(vorbis_info *vi,NvOggPacket *op){
  NvCodecSetupInfo     *ci=(NvCodecSetupInfo *)vi->codec_setup;
  oggpack_buffer       opb;
  NvS32                  mode;

  NvOggpackReadinit(&opb,op->packet);

  /* Check the packet type */
  if(NvOggpackRead(&opb,1)!=0){
    /* Oops.  This is not an audio data packet */
    return(OV_ENOTAUDIO);
  }

  {
    NvS32 modebits=0;
    NvS32 v=ci->modes;
    while(v>1){
      modebits++;
      v>>=1;
    }

    /* read our mode and pre/post windowsize */
    mode=NvOggpackRead(&opb,modebits);
  }
  if(mode==-1)return(OV_EBADPACKET);
  return(ci->blocksizes[ci->mode_param[mode]->blockflag]);
}

NvS32 NvVorbisSynthesisRestart(vorbis_dsp_state *v){
  vorbis_info *vi=v->vi;
  NvCodecSetupInfo *ci;

  if(!v->backend_state)return -1;
  if(!vi)return -1;
  ci=vi->codec_setup;
  if(!ci)return -1;

  v->centerW=ci->blocksizes[1]/2;
  v->pcm_current=v->centerW;

  v->pcm_returned=-1;
  v->granulepos=-1;
  v->sequence=-1;
  ((NvPrivateState *)(v->backend_state))->sample_count=-1;

  return(0);
}

vorbis_info_floor *NvFloor0Unpack (vorbis_info *vi,oggpack_buffer *opb){
  NvCodecSetupInfo     *ci=(NvCodecSetupInfo *)vi->codec_setup;
  NvS32 j;

  NvVorbisInfoFloor0 *info=(NvVorbisInfoFloor0 *)_ogg_malloc(sizeof(*info));
  info->order=NvOggpackRead(opb,8);
  info->rate=NvOggpackRead(opb,16);
  info->barkmap=NvOggpackRead(opb,16);
  info->ampbits=NvOggpackRead(opb,6);
  info->ampdB=NvOggpackRead(opb,8);
  info->numbooks=NvOggpackRead(opb,4)+1;

  if(info->order<1)goto err_out;
  if(info->rate<1)goto err_out;
  if(info->barkmap<1)goto err_out;
  if(info->numbooks<1)goto err_out;

  for(j=0;j<info->numbooks;j++){
    info->books[j]=NvOggpackRead(opb,8);
    if(info->books[j]<0 || info->books[j]>=ci->books)goto err_out;
  }
  return(info);

 err_out:
  NvFloor0FreeInfo(info);
  return(NULL);
}

void NvFloor1FreeInfo(vorbis_info_floor *i){
  NvVorbisInfoFloor1 *info=(NvVorbisInfoFloor1 *)i;
  if(info){
    memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

vorbis_info_floor *NvFloor1Unpack (vorbis_info *vi,oggpack_buffer *opb){
  NvCodecSetupInfo     *ci=(NvCodecSetupInfo *)vi->codec_setup;
  NvS32 j,k,count=0,maxclass=-1,rangebits;

  NvVorbisInfoFloor1 *info=(NvVorbisInfoFloor1 *)_ogg_calloc(1,sizeof(*info));
  /* read partitions */
  info->partitions=NvOggpackRead(opb,5); /* only 0 to 31 legal */
  for(j=0;j<info->partitions;j++){
    info->partitionclass[j]=NvOggpackRead(opb,4); /* only 0 to 15 legal */
    if(maxclass<info->partitionclass[j])maxclass=info->partitionclass[j];
  }

  /* read partition classes */
  for(j=0;j<maxclass+1;j++){
    info->class_dim[j]=NvOggpackRead(opb,3)+1; /* 1 to 8 */
    info->class_subs[j]=NvOggpackRead(opb,2); /* 0,1,2,3 bits */
    if(info->class_subs[j]<0)
      goto err_out;
    if(info->class_subs[j])info->class_book[j]=NvOggpackRead(opb,8);
    if(info->class_book[j]<0 || info->class_book[j]>=ci->books)
      goto err_out;
    for(k=0;k<(1<<info->class_subs[j]);k++){
      info->class_subbook[j][k]=NvOggpackRead(opb,8)-1;
      if(info->class_subbook[j][k]<-1 || info->class_subbook[j][k]>=ci->books)
    goto err_out;
    }
  }

  /* read the post list */
  info->mult=NvOggpackRead(opb,2)+1;     /* only 1,2,3,4 legal now */
  rangebits=NvOggpackRead(opb,4);

  for(j=0,k=0;j<info->partitions;j++){
    count+=info->class_dim[info->partitionclass[j]];
    for(;k<count;k++)
    {
        if (rangebits >=0)
        {
            NvS32 t=info->postlist[k+2]=NvOggpackRead(opb,rangebits);
            if(t<0 || t>=(1<<rangebits))
            goto err_out;
        }
    }
  }
  info->postlist[0]=0;
  info->postlist[1]=1<<rangebits;

  return(info);

 err_out:
  NvFloor1FreeInfo(info);
  return(NULL);
}

void NvRes0FreeInfo(vorbis_info_residue *i){
  NvVorbisInfoResidue0 *info=(NvVorbisInfoResidue0 *)i;
  if(info){
    memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

/* vorbis_info is for range checking */
vorbis_info_residue *NvRes0Unpack(vorbis_info *vi,oggpack_buffer *opb){
  NvS32 j,acc=0;
  NvVorbisInfoResidue0 *info=(NvVorbisInfoResidue0 *)_ogg_calloc(1,sizeof(*info));
  NvCodecSetupInfo     *ci=(NvCodecSetupInfo *)vi->codec_setup;

  info->begin=NvOggpackRead(opb,24);
  info->end=NvOggpackRead(opb,24);
  info->grouping=NvOggpackRead(opb,24)+1;
  info->partitions=NvOggpackRead(opb,6)+1;
  info->groupbook=NvOggpackRead(opb,8);

  for(j=0;j<info->partitions;j++){
    NvS32 cascade=NvOggpackRead(opb,3);

    if(NvOggpackRead(opb,1))
    {
        if (cascade >=0)
            cascade|=(NvOggpackRead(opb,5)<<3);
    }
    
    info->secondstages[j]=cascade;

    if(cascade >=0)
        acc+=NvIcount(cascade);
  }
  for(j=0;j<acc;j++)
    info->booklist[j]=NvOggpackRead(opb,8);

  if(info->groupbook>=ci->books)goto errout;
  for(j=0;j<acc;j++)
    if(info->booklist[j]>=ci->books)goto errout;

  return(info);
 errout:
  NvRes0FreeInfo(info);
  return(NULL);
}

void NvMapping0FreeInfo(vorbis_info_mapping *i){
  NvVorbisInfoMapping0 *info=(NvVorbisInfoMapping0 *)i;
  if(info){
    memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

/* also responsible for range checking */
vorbis_info_mapping *NvMapping0Unpack(vorbis_info *vi,oggpack_buffer *opb){
  NvS32 i;
  NvVorbisInfoMapping0 *info=(NvVorbisInfoMapping0 *)_ogg_calloc(1,sizeof(*info));
  NvCodecSetupInfo     *ci=(NvCodecSetupInfo *)vi->codec_setup;
  memset(info,0,sizeof(*info));

  if(NvOggpackRead(opb,1))
    info->submaps=NvOggpackRead(opb,4)+1;
  else
    info->submaps=1;

  if(NvOggpackRead(opb,1)){
    info->coupling_steps=NvOggpackRead(opb,8)+1;

    for(i=0;i<info->coupling_steps;i++){
      NvS32 testM=info->coupling_mag[i]=NvOggpackRead(opb,NvMIlog(vi->channels));
      NvS32 testA=info->coupling_ang[i]=NvOggpackRead(opb,NvMIlog(vi->channels));

      if(testM<0 ||
     testA<0 ||
     testM==testA ||
     testM>=vi->channels ||
     testA>=vi->channels) goto err_out;
    }

  }

  if(NvOggpackRead(opb,2)>0)goto err_out; /* 2,3:reserved */

  if(info->submaps>1){
    for(i=0;i<vi->channels;i++){
      info->chmuxlist[i]=NvOggpackRead(opb,4);
      if(info->chmuxlist[i]>=info->submaps)goto err_out;
    }
  }
  for(i=0;i<info->submaps;i++){
    NvS32 temp=NvOggpackRead(opb,8);
    if(temp>=ci->times)goto err_out;
    info->floorsubmap[i]=NvOggpackRead(opb,8);
    if(info->floorsubmap[i]>=ci->floors)goto err_out;
    info->residuesubmap[i]=NvOggpackRead(opb,8);
    if(info->residuesubmap[i]>=ci->residues)goto err_out;
  }

  return info;

 err_out:
  NvMapping0FreeInfo(info);
  return(NULL);
}

void NvVorbisBookClear(NvCodebook *b){
  /* static book is not cleared; we're likely called on the lookup and
     the static codebook belongs to the info struct */
  if(b->valuelist)_ogg_free(b->valuelist);
  if(b->codelist)_ogg_free(b->codelist);

  if(b->dec_index)_ogg_free(b->dec_index);
  if(b->dec_codelengths)_ogg_free(b->dec_codelengths);
  if(b->dec_firsttable)_ogg_free(b->dec_firsttable);

  memset(b,0,sizeof(*b));
}

/* there might be a straightforward one-line way to do the below
   that's portable and totally safe against roundoff, but I haven't
   thought of it.  Therefore, we opt on the side of caution */
NvS32 NvBookMaptype1Quantvals(const NvStaticCodebook *b){
  /* get us a starting hint, we'll polish it below */
  NvS32 bits=NvIlog(b->entries);
  NvS32 vals=b->entries>>((bits-1)*(b->dim-1)/b->dim);

  while(1){
    NvS32 acc=1;
    NvS32 acc1=1;
    NvS32 i;
    for(i=0;i<b->dim;i++){
      acc*=vals;
      acc1*=vals+1;
    }
    if(acc<=b->entries && acc1>b->entries){
      return(vals);
    }else{
      if(acc>b->entries){
    vals--;
      }else{
    vals++;
      }
    }
  }
}

void NvVorbisStaticbookClear(NvStaticCodebook *b){
  if(b->quantlist)_ogg_free(b->quantlist);
  if(b->lengthlist)_ogg_free(b->lengthlist);
  memset(b,0,sizeof(*b));

}

void NvVorbisStaticbookDestroy(NvStaticCodebook *b){
  NvVorbisStaticbookClear(b);
  _ogg_free(b);
}

/* unpacks a codebook from the packet buffer into the NvCodebook struct,
   readies the codebook auxiliary structures for decode *************/
NvS32 NvVorbisStaticbookUnpack(oggpack_buffer *opb,NvStaticCodebook *s){
  NvS32 i,j;
  memset(s,0,sizeof(*s));

  /* make sure alignment is correct */
  if(NvOggpackRead(opb,24)!=0x564342)goto _eofout;

  /* first the basic parameters */
  s->dim=NvOggpackRead(opb,16);
  s->entries=NvOggpackRead(opb,24);
  if(s->entries==-1)goto _eofout;

  /* codeword ordering.... length ordered or unordered? */
  switch((NvS32)NvOggpackRead(opb,1)){
  case 0:
    /* unordered */
    s->lengthlist=(NvS32 *)_ogg_malloc(sizeof(*s->lengthlist)*s->entries);

    /* allocated but unused entries? */
    if(NvOggpackRead(opb,1)){
      /* yes, unused entries */

      for(i=0;i<s->entries;i++){
    if(NvOggpackRead(opb,1)){
      NvS32 num=NvOggpackRead(opb,5);
      if(num==-1)goto _eofout;
      s->lengthlist[i]=num+1;
    }else
      s->lengthlist[i]=0;
      }
    }else{
      /* all entries used; no tagging */
      for(i=0;i<s->entries;i++){
    NvS32 num=NvOggpackRead(opb,5);
    if(num==-1)goto _eofout;
    s->lengthlist[i]=num+1;
      }
    }

    break;
  case 1:
    /* ordered */
    {
      NvS32 length=NvOggpackRead(opb,5)+1;
      s->lengthlist=(NvS32 *)_ogg_malloc(sizeof(*s->lengthlist)*s->entries);

      for(i=0;i<s->entries;){
    NvS32 num=NvOggpackRead(opb,NvIlog(s->entries-i));
    if(num==-1)goto _eofout;
    for(j=0;j<num && i<s->entries;j++,i++)
      s->lengthlist[i]=length;
    length++;
      }
    }
    break;
  default:
    /* EOF */
    return(-1);
  }

  /* Do we have a mapping to unpack? */
  switch((s->maptype=NvOggpackRead(opb,4))){
  case 0:
    /* no mapping */
    break;
  case 1: case 2:
    /* implicitly populated value mapping */
    /* explicitly populated value mapping */

    s->q_min=NvOggpackRead(opb,32);
    s->q_delta=NvOggpackRead(opb,32);
    s->q_quant=NvOggpackRead(opb,4)+1;
    s->q_sequencep=NvOggpackRead(opb,1);

    {
      NvS32 quantvals=0;
      switch(s->maptype){
      case 1:
    quantvals=NvBookMaptype1Quantvals(s);
    break;
      case 2:
    quantvals=s->entries*s->dim;
    break;
      }

      /* quantized values */
      s->quantlist=(NvS32 *)_ogg_malloc(sizeof(*s->quantlist)*quantvals);
      for(i=0;i<quantvals;i++)
    s->quantlist[i]=NvOggpackRead(opb,s->q_quant);

      if(quantvals&&s->quantlist[quantvals-1]==-1)goto _eofout;
    }
    break;
  default:
    goto _errout;
  }

  /* all set */
  return(0);

 _errout:
 _eofout:
  NvVorbisStaticbookClear(s);
  return(-1);
}

#endif
