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

 function: subsumed libogg includes

 ********************************************************************/
#ifndef _NVMM_OGGPROTOTYPES_H
#define _NVMM_OGGPROTOTYPES_H

#include "nvmm_ogg.h"
#include "nvmm_oggbookunpack.h"
#include "nvmm_oggparser.h"

//#define CHUNKSIZE (1024)

#ifdef __cplusplus
extern "C" {
#endif

//#include "os_types.h"
/*
typedef struct ogg_buffer_state{
  struct ogg_buffer    *unused_buffers;
  struct ogg_reference *unused_references;
  int                   outstanding;
  int                   shutdown;
} ogg_buffer_state;


typedef struct ogg_buffer {
  unsigned char      *data;
  long                size;
  int                 refcount;

  union {
    ogg_buffer_state  *owner;
    struct ogg_buffer *next;
  } ptr;
} ogg_buffer;

typedef struct ogg_reference {
  ogg_buffer    *buffer;
  long           begin;
  long           length;

  struct ogg_reference *next;
} ogg_reference;

typedef struct oggpack_buffer {
  int            headbit;
  unsigned char *headptr;
  long           headend;

  // memory management
  ogg_reference *head;
  ogg_reference *tail;

  // render the byte/bit counter API constant time
  long              count; // doesn't count the tail
} oggpack_buffer;
*/

typedef struct 
{
  ogg_reference *baseref;

  ogg_reference *ref;
  NvU8 *ptr;
  NvS32           pos;
  NvS32           end;
} NvOggByteBuffer;

/*
typedef struct ogg_sync_state {
  // decode memory management pool
  ogg_buffer_state *bufferpool;

  // stream buffers
  ogg_reference    *fifo_head;
  ogg_reference    *fifo_tail;
  long              fifo_fill;

  // stream sync management
  int               unsynced;
  int               headerbytes;
  int               bodybytes;

} ogg_sync_state;

typedef struct ogg_stream_state {
  ogg_reference *header_head;
  ogg_reference *header_tail;
  ogg_reference *body_head;
  ogg_reference *body_tail;

  int            e_o_s;    // set when we have buffered the last
                           //   packet in the logical bitstream
  int            b_o_s;    // set after we've written the initial page
                           //   of a logical bitstream
  long           serialno;
  long           pageno;
  ogg_int64_t    packetno; // sequence number for decode; the framing
                           //   knows where there's a hole in the data,
                           //   but we need coupling so that the codec
                           //   (which is in a seperate abstraction
                           //   layer) also knows about the gap
  ogg_int64_t    granulepos;

  int            lacing_fill;
  ogg_uint32_t   body_fill;

  // decode-side state data
  int            holeflag;
  int            spanflag;
  int            clearflag;
  int            laceptr;
  ogg_uint32_t   body_fill_next;

} ogg_stream_state;
*/

typedef struct {
  ogg_reference *packet;
  NvS32           bytes;
  NvS32           b_o_s;
  NvS32           e_o_s;
  ogg_int64_t    granulepos;
  ogg_int64_t    packetno;     // sequence number for decode; the framing
                               //   knows where there's a hole in the data,
                               //   but we need coupling so that the codec
                               //   (which is in a seperate abstraction
                               //   layer) also knows about the gap
} NvOggPacket;
/*
typedef struct {
  ogg_reference *header;
  int            header_len;
  ogg_reference *body;
  long           body_len;
} ogg_page;

typedef struct OneTimeInitFrameMessage {
    ogg_page *og;
    OggVorbis_File *vfptr;
    int decClearFlag;
    int readcount;
} OneTimeInitFrameMessage;

typedef struct FrameMessage {
    ogg_page *og;
    int decClearFlag;
    int readStatus;
    char *outbufptr;
    unsigned long outpcmlen;
} FrameMessage;
*/
/* Ogg BITSTREAM PRIMITIVES: bitstream ************************/

extern void  NvOggpackReadinit(oggpack_buffer *b,ogg_reference *r);
extern NvS32  NvOggpackLook(oggpack_buffer *b,NvS32 bits);
extern void  NvOggpackAdv(oggpack_buffer *b,NvS32 bits);
extern NvS32  NvOggpackRead(oggpack_buffer *b,NvS32 bits);
extern NvS32  NvOggpackBytes(oggpack_buffer *b);
extern NvS32  NvOggpackBits(oggpack_buffer *b);
extern NvS32   NvOggpackEop(oggpack_buffer *b);

extern void     NvVorbisInfoInit(vorbis_info *vi);
extern void     NvVorbisInfoClear(vorbis_info *vi);
//extern NvS32      vorbis_info_blocksize(vorbis_info *vi,NvS32 zo);
extern void     NvVorbisCommentInit(vorbis_comment *vc);
//extern void     vorbis_comment_add(vorbis_comment *vc, char *comment);
//extern void     vorbis_comment_add_tag(vorbis_comment *vc,
//                       char *tag, char *contents);
//extern char    *vorbis_comment_query(vorbis_comment *vc, char *tag, NvS32 count);
//extern NvS32      vorbis_comment_query_count(vorbis_comment *vc, char *tag);
extern void     NvVorbisCommentClear(vorbis_comment *vc);

// Vorbis PRIMITIVES: synthesis layer *******************************
extern NvS32      NvVorbisSynthesisHeaderin(vorbis_info *vi,vorbis_comment *vc,
                      NvOggPacket *op);

extern NvS32      NvVorbisSynthesisInit(vorbis_dsp_state *v,vorbis_info *vi);
extern NvS32      NvVorbisSynthesisRestart(vorbis_dsp_state *v);
extern NvS32      NvVorbisSynthesis(vorbis_block *vb,NvOggPacket *op,NvS32 decodep);
extern NvS32      NvVorbisSynthesisBlockin(vorbis_dsp_state *v,vorbis_block *vb);
extern NvS32      NvVorbisSynthesisPcmout(vorbis_dsp_state *v,ogg_int32_t ***pcm);
extern NvS32      NvVorbisSynthesisRead(vorbis_dsp_state *v,NvS32 samples);
extern NvS32     NvVorbisPacketBlocksize(vorbis_info *vi,NvOggPacket *op);

extern NvS32 NvOvopen(NvOggParser *pState,OggVorbis_File *vf,NvS8 *initial,NvS32 ibytes);
extern NvS32 NvOggFirstFread(NvOggParser *pState,OggVorbis_File *vf,ogg_page *og,NvS32 readp, NvS32 spanp);
extern vorbis_info *NvOvInfo(OggVorbis_File *vf,NvS32 link);
extern vorbis_comment *NvOvComment(OggVorbis_File *vf,NvS32 link);
extern ogg_int64_t NvOvPcmTotal(OggVorbis_File *vf,NvS32 i);
extern ogg_int64_t NvOvTimeTotal(OggVorbis_File *vf,int i);
extern NvS32 NvOvSeekable(OggVorbis_File *vf);
extern NvS32 NvOvTestCallbacks(void *f,OggVorbis_File *vf,NvS8 *initial,NvS32 ibytes,
    ov_callbacks callbacks);
extern NvS32 NvOvOpenCallbacks(NvOggParser *pState,OggVorbis_File *vf,NvS8 *initial,NvS32 ibytes,
    ov_callbacks callbacks);
extern NvS32 NvOvClear(OggVorbis_File *vf);

extern NvS32      Nv0ggPageContinued(ogg_page *og);
extern ogg_int64_t  NvOggPageGranulepos(ogg_page *og);
extern ogg_uint32_t NvOggPageSerialno(ogg_page *og);
extern ogg_sync_state *NvOggSyncCreate(void);
extern NvS32      NvOggSyncDestroy(ogg_sync_state *oy);
extern NvU8 *NvOggSyncBufferin(ogg_sync_state *oy, NvS32 size);
extern NvS32      NvOggSyncWrote(ogg_sync_state *oy, NvS32 bytes);
extern NvS32      NvOggSyncReset(ogg_sync_state *oy);
extern NvS32     NvOggSyncPageseek(ogg_sync_state *oy,ogg_page *og);
extern ogg_stream_state *NvOggStreamCreate(NvS32 serialno);
extern NvS32      NvOggStreamDestroy(ogg_stream_state *os);
// added to handle end of input buffer destroy
extern NvS32      NvOggStreamDestroyEnd(ogg_stream_state *os);
extern NvS32      NvOggStreamPagein(ogg_stream_state *os, ogg_page *og);
extern NvS32      NvOggStreamReset(ogg_stream_state *os);
extern NvS32      NvOggStreamResetSerialno(ogg_stream_state *os,NvS32 serialno);
extern NvS32      NvOggStreamPacketout(ogg_stream_state *os,NvOggPacket *op);
extern NvS32      NvOggStreamPacketpeek(ogg_stream_state *os,NvOggPacket *op);
extern NvS32      NvOggPacketRelease(NvOggPacket *op);
extern NvS32      NvOggPageRelease(ogg_page *og);
extern void     NvOggPageDup(ogg_page *d, ogg_page *s);
extern NvS32      NvOggPageEos(ogg_page *og);
extern NvS32 NvOvTimeSeekPage(NvOggParser *pState,OggVorbis_File *vf,ogg_int64_t milliseconds);
 NvS32 NvOvPcmSeekPage(NvOggParser *pState,OggVorbis_File *vf,ogg_int64_t pos);
extern void NvOggBufferDestroy(ogg_buffer_state *bs);
extern ogg_buffer *NvFetchBuffer(ogg_buffer_state *bs,NvS32 bytes); 
extern ogg_reference *NvFetchRef(ogg_buffer_state *bs);
extern  ogg_reference *NvOggBufferAlloc(ogg_buffer_state *bs,NvS32 bytes);
extern  void NvOggBufferRealloc(ogg_reference *or,NvS32 bytes);
extern  void NvOggBufferMarkOne(ogg_reference *or);
extern  void NvOggBufferMark(ogg_reference *or);
extern  ogg_reference *NvOggBufferSub(ogg_reference *or,NvS32 begin,NvS32 length);
extern  ogg_reference *NvOggBufferDup(ogg_reference *or);
extern  ogg_reference *NvOggBufferSplit(ogg_reference **tail,ogg_reference **head,NvS32 pos);
extern  void NvOggBufferReleaseOne(ogg_reference *or);
extern  void NvOggBufferRelease(ogg_reference *or);
extern  ogg_reference *NvOggBufferPretruncate(ogg_reference *or,NvS32 pos);
extern  ogg_reference *NvOggBufferCat(ogg_reference *tail, ogg_reference *head);
extern  void NvPositionB(NvOggByteBuffer *b,NvS32 pos);
extern  void NvPositionF(NvOggByteBuffer *b,NvS32 pos);
extern  NvS32 NvOggbyteInit(NvOggByteBuffer *b,ogg_reference *or);
extern  void NvOggbyteSet4(NvOggByteBuffer *b,ogg_uint32_t val,NvS32 pos);
extern  NvU8 NvOggbyteRead1(NvOggByteBuffer *b,NvS32 pos);
extern  ogg_uint32_t NvOggbyteRead4(NvOggByteBuffer *b,NvS32 pos);
extern ogg_int64_t NvOggbyteRead8(NvOggByteBuffer *b,NvS32 pos);
extern  NvS32 NvOggPageVersion(ogg_page *og);
extern  NvS32 NvOggPageBos(ogg_page *og);
extern  ogg_uint32_t NvOggPagePageno(ogg_page *og);
extern  NvS32 ogg_page_packets(ogg_page *og);
extern  ogg_uint32_t NvChecksum(ogg_reference *or, NvS32 bytes);
extern  int ogg_sync_pageout(ogg_sync_state *oy, ogg_page *og);
extern  void NvNextLace(NvOggByteBuffer *ob,ogg_stream_state *os);
extern  void NVSpanQueuedPage(ogg_stream_state *os);
extern  NvS32 NvPacketout(ogg_stream_state *os,NvOggPacket *op,NvS32 adv);

extern NvS32 NvGetData(NvOggParser * pState,OggVorbis_File *vf);
extern void NvSeekHelper(NvOggParser * pState,OggVorbis_File *vf,ogg_int64_t offset);
extern ogg_int64_t NvGetNextPage(NvOggParser * pState,OggVorbis_File *vf,ogg_page *og,ogg_int64_t boundary);
extern ogg_int64_t NvGetPrevPage(NvOggParser * pState,OggVorbis_File *vf,ogg_page *og);
ogg_int64_t NvGetPrevPageOpen2(NvOggParser * pState,OggVorbis_File *vf,ogg_page *og);
extern void NvVorbisDspClear(vorbis_dsp_state *v);
extern NvS32 NvBisectForwardSerialno(NvOggParser * pState,OggVorbis_File *vf,
                    ogg_int64_t begin,
                    ogg_int64_t searched,
                    ogg_int64_t end,
                    ogg_uint32_t currentno,
                    NvS32 m);
extern NvS32 NvFetchHeaders(NvOggParser * pState,OggVorbis_File *vf,
              vorbis_info *vi,
              vorbis_comment *vc,
              ogg_uint32_t *serialno,
              ogg_page *og_ptr);
extern void NvPrefetchAllHeaders(NvOggParser * pState,OggVorbis_File *vf, ogg_int64_t dataoffset);
extern NvS32 NvOvOpen1(NvOggParser * pState,OggVorbis_File *vf,NvS8 *initial, NvS32 ibytes, ov_callbacks callbacks);
extern void NvVorbisBlockRipcord(vorbis_block *vb);
extern NvS32 NvVorbisBlockClear(vorbis_block *vb);
extern long ov_bitrate(OggVorbis_File *vf,int i);
NvS32 NvOvRawSeek(NvOggParser * pState,OggVorbis_File *vf,ogg_int64_t pos);
extern NvS32 NvOpenSeekable2(NvOggParser * pState,OggVorbis_File *vf);
extern NvS32 NvOvOpen2(NvOggParser * pState,OggVorbis_File *vf);

extern void OggParseInfoClear(vorbis_info *);

/* Ogg BITSTREAM PRIMITIVES: decoding **************************/

//extern ogg_sync_state *NvOggSyncCreate(void);
//extern int      ogg_sync_destroy(ogg_sync_state *oy);
//extern int      ogg_sync_reset(ogg_sync_state *oy);

//extern NvU8 *ogg_sync_bufferin(ogg_sync_state *oy, long size);
//extern int      NvOggSyncWrote(ogg_sync_state *oy, long bytes);
//extern long     ogg_sync_pageseek(ogg_sync_state *oy,ogg_page *og);
//extern int      ogg_sync_pageout(ogg_sync_state *oy, ogg_page *og);
//extern int      ogg_stream_pagein(ogg_stream_state *os, ogg_page *og);
//extern int      ogg_stream_packetout(ogg_stream_state *os,ogg_packet *op);
//extern int      ogg_stream_packetpeek(ogg_stream_state *os,ogg_packet *op);

/* Ogg BITSTREAM PRIMITIVES: general ***************************/

//extern ogg_stream_state *ogg_stream_create(int serialno);
//extern int      ogg_stream_destroy(ogg_stream_state *os);
//extern int      ogg_stream_reset(ogg_stream_state *os);
//extern int      ogg_stream_reset_serialno(ogg_stream_state *os,int serialno);
//extern int      ogg_stream_eos(ogg_stream_state *os);

//extern int      ogg_page_checksum_set(ogg_page *og);

//extern int      ogg_page_version(ogg_page *og);
//extern int      Nv0ggPageContinued(ogg_page *og);
//extern int      ogg_page_bos(ogg_page *og);
//extern int      ogg_page_eos(ogg_page *og);
//extern ogg_int64_t  ogg_page_granulepos(ogg_page *og);
//extern ogg_uint32_t ogg_page_serialno(ogg_page *og);
//extern ogg_uint32_t ogg_page_pageno(ogg_page *og);
//extern int      ogg_page_packets(ogg_page *og);
//extern int      ogg_page_getbuffer(ogg_page *og, NvU8 **buffer);

//extern int      ogg_packet_release(ogg_packet *op);
//extern int      ogg_page_release(ogg_page *og);

//extern void     ogg_page_dup(ogg_page *d, ogg_page *s);

// added to handle end of input buffer destroy
//extern int      ogg_stream_destroy_end(ogg_stream_state *os);

/* Ogg BITSTREAM PRIMITIVES: return codes ***************************/
/*
#define  OGG_SUCCESS   0

#define  OGG_HOLE     -10
#define  OGG_SPAN     -11
#define  OGG_EVERSION -12
#define  OGG_ESERIAL  -13
#define  OGG_EINVAL   -14
#define  OGG_EEOS     -15
*/

//#ifdef OVERRIDE_RUNTIMEMEMALLOC_FUNC
extern void *NvMyMalloc(NvU32);
extern void *NvMyCalloc(NvU32, NvU32);
extern void *NvMyRealloc(void *, NvU32);
extern void NvMyFree(void *);

/* make it easy on the folks that want to compile the libs with a
   different malloc than stdlib */
#define _ogg_malloc  NvMyMalloc
#define _ogg_calloc  NvMyCalloc
#define _ogg_realloc NvMyRealloc
#define _ogg_free    NvMyFree

#ifdef __cplusplus
}
#endif

#endif  /* _NVMM_OGGPROTOTYPES_H */

