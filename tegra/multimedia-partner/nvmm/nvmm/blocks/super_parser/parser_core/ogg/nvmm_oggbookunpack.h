/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
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

#ifndef _NVMM_OGGBOOKUNPACK_H
#define _NVMM_OGGBOOKUNPACK_H

#ifdef CODEBOOK_INFO_CHANGE

//This is from codec_internal.h

/* This structure encapsulates huffman and VQ style encoding books; it
   doesn't do anything specific to either.

   valuelist/quantlist are nonNULL (and q_* significant) only if
   there's entry->value mapping to be done.

   If encode-side mapping must be done (and thus the entry needs to be
   hunted), the auxiliary encode pointer will point to a decision
   tree.  This is true of both VQ and huffman, but is mostly useful
   with VQ.

*/

typedef struct
{
  NvS32   dim;            /* codebook dimensions (elements per vector) */
  NvS32   entries;        /* codebook entries */
  NvS32  *lengthlist;     /* codeword lengths in bits */

  /* mapping ***************************************************************/
  NvS32    maptype;        /* 0=none
                1=implicitly populated values from map column
                2=listed arbitrary values */

  /* The below does a linear, single monotonic sequence mapping. */
  NvS32     q_min;       /* packed 32 bit float; quant value 0 maps to minval */
  NvS32     q_delta;     /* packed 32 bit float; val 1 - val 0 == delta */
  NvS32      q_quant;     /* bits: 0 < quant <= 16 */
  NvS32      q_sequencep; /* bitflag */

  NvS32     *quantlist;  /* map == 1: (int)(entries^(1/dim)) element column map
               map == 2: list of dim*entries quantized entry vals
            */
} NvStaticCodebook;

typedef struct
{
  NvS32 dim;           /* codebook dimensions (elements per vector) */
  NvS32 entries;       /* codebook entries */
  NvS32 used_entries;  /* populated codebook entries */

  /* the below are ordered by bitreversed codeword and only used
     entries are populated */
  NvS32           binarypoint;
  ogg_int32_t  *valuelist;  /* list of dim*entries actual entry values */
  ogg_uint32_t *codelist;   /* list of bitstream codewords for each entry */

  NvS32          *dec_index;
  NvS8         *dec_codelengths;
  ogg_uint32_t *dec_firsttable;
  NvS32           dec_firsttablen;
  NvS32           dec_maxlength;

  NvS32     q_min;       /* packed 32 bit float; quant value 0 maps to minval */
  NvS32     q_delta;     /* packed 32 bit float; val 1 - val 0 == delta */

} NvCodebook;

typedef void vorbis_look_mapping;
typedef void vorbis_look_floor;
typedef void vorbis_look_residue;
typedef void vorbis_look_transform;
typedef void vorbis_info_floor;
typedef void vorbis_info_residue;
typedef void vorbis_info_mapping;

/* mode ************************************************************/
typedef struct {
  NvS32 blockflag;
  NvS32 windowtype;
  NvS32 transformtype;
  NvS32 mapping;
} NvVorbisInfoMode;

typedef struct
{
  /* local lookup storage */
  const void             *window[2];

  /* backend lookups are tied to the mode, not the backend or naked mapping */
  NvS32                     modebits;
  vorbis_look_mapping   **mode;

  ogg_int64_t sample_count;

} NvPrivateState;

typedef struct
{
/* block-partitioned VQ coded straight residue */
  NvS32  begin;
  NvS32  end;

  /* first stage (lossless partitioning) */
  NvS32    grouping;         /* group n vectors per partition */
  NvS32    partitions;       /* possible codebooks for a partition */
  NvS32    groupbook;        /* huffbook for partitioning */
  NvS32    secondstages[64]; /* expanded out to pointers in lookup */
  NvS32    booklist[256];    /* list of second stage books */
} NvVorbisInfoResidue0;

typedef struct {
  NvVorbisInfoResidue0 *info;
  NvS32         map;

  NvS32         parts;
  NvS32         stages;
  NvCodebook   *fullbooks;
  NvCodebook   *phrasebook;
  NvCodebook ***partbooks;

  NvS32         partvals;
  NvS32       **decodemap;

} NvVorbisLookResidue0;

typedef struct
{
  NvS32   submaps;  /* <= 16 */
  NvS32   chmuxlist[256];   /* up to 256 channels in a Vorbis stream */

  NvS32   floorsubmap[16];   /* [mux] submap to floors */
  NvS32   residuesubmap[16]; /* [mux] submap to residue */

  NvS32   psy[2]; /* by blocktype; impulse/padding for short,
                   transition/normal for NvS32 */

  NvS32   coupling_steps;
  NvS32   coupling_mag[256];
  NvS32   coupling_ang[256];
} NvVorbisInfoMapping0;

typedef struct{
  NvS32   order;
  NvS32  rate;
  NvS32  barkmap;

  NvS32   ampbits;
  NvS32   ampdB;

  NvS32   numbooks; /* <= 16 */
  NvS32   books[16];

} NvVorbisInfoFloor0;

typedef struct {
  NvS32 n;
  NvS32 ln;
  NvS32  m;
  NvS32 *linearmap;

  NvVorbisInfoFloor0 *vi;
  ogg_int32_t *lsp_look;

} NvVorbisLookFloor0;

/* this would all be simpler/shorter with templates, but.... */
/* Transform backend generic *************************************/

/* only mdct right now.  Flesh it out more if we ever transcend mdct
   in the transform domain */

/* Floor backend generic *****************************************/
typedef struct{
  vorbis_info_floor     *(*unpack)(vorbis_info *,oggpack_buffer *);
  vorbis_look_floor     *(*look)  (vorbis_dsp_state *,NvVorbisInfoMode *,
                   vorbis_info_floor *);
  void (*free_info) (vorbis_info_floor *);
  void (*free_look) (vorbis_look_floor *);
  void *(*inverse1)  (struct vorbis_block *,vorbis_look_floor *);
  NvS32   (*inverse2)  (struct vorbis_block *,vorbis_look_floor *,
             void *buffer,ogg_int32_t *);
} NvVorbisFuncFloor;

/* Residue backend generic *****************************************/
typedef struct{
  vorbis_info_residue *(*unpack)(vorbis_info *,oggpack_buffer *);
  vorbis_look_residue *(*look)  (vorbis_dsp_state *,NvVorbisInfoMode *,
                 vorbis_info_residue *);
  void (*free_info)    (vorbis_info_residue *);
  void (*free_look)    (vorbis_look_residue *);
  NvS32  (*inverse)      (struct vorbis_block *,vorbis_look_residue *,
            ogg_int32_t **,NvS32 *,NvS32);
} NvVorbisFuncResidue;

/* simplistic, wasteful way of doing this (unique lookup for each
   mode/submapping); there should be a central repository for
   identical lookups.  That will require minor work, so I'm putting it
   off as low priority.

   Why a lookup for each backend in a given mode?  Because the
   blocksize is set by the mode, and low backend lookups may require
   parameters from other areas of the mode/mapping */

typedef struct {
  NvVorbisInfoMode *mode;
  NvVorbisInfoMapping0 *map;

  vorbis_look_floor **floor_look;

  vorbis_look_residue **residue_look;

  NvVorbisFuncFloor **floor_func;
  NvVorbisFuncResidue **residue_func;

  NvS32 ch;
  NvS32 lastframe; /* if a different mode is called, we need to
             invalidate decay */
} NvVorbisLookMapping0;

/* codec_setup_info contains all the setup information specific to the
   specific compression/decompression mode in progress (eg,
   psychoacoustic settings, channel setup, options, codebook
   etc).
*********************************************************************/

typedef struct
{

  /* Vorbis supports only short and long blocks, but allows the
     encoder to choose the sizes */

  NvS32 blocksizes[2];

  /* modes are the primary means of supporting on-the-fly different
     blocksizes, different channel mappings (LR or M/A),
     different residue backends, etc.  Each mode consists of a
     blocksize flag and a mapping (along with the mapping setup */

  NvS32        modes;
  NvS32        maps;
  NvS32        times;
  NvS32        floors;
  NvS32        residues;
  NvS32        books;

  NvVorbisInfoMode       *mode_param[64];
  NvS32                     map_type[64];
  vorbis_info_mapping    *map_param[64];
  NvS32                     time_type[64];
  NvS32                     floor_type[64];
  vorbis_info_floor      *floor_param[64];
  NvS32                     residue_type[64];
  vorbis_info_residue    *residue_param[64];
  NvStaticCodebook        *book_param[256];
  NvCodebook               *fullbooks;

  NvS32    passlimit[32];     /* iteration limit per couple/quant pass */
  NvS32    coupling_passes;
} NvCodecSetupInfo;


//The above is from codec_internal.h



//This is from registry.h, backends.h

#define VI_TRANSFORMB 1
#define VI_WINDOWB 1
#define VI_TIMEB 1
#define VI_FLOORB 2
#define VI_RESB 3
#define VI_MAPB 1

#define VIF_POSIT 63
#define VIF_CLASS 16
#define VIF_PARTS 31
typedef struct{
  NvS32   partitions;                /* 0 to 31 */
  NvS32   partitionclass[VIF_PARTS]; /* 0 to 15 */

  NvS32   class_dim[VIF_CLASS];        /* 1 to 8 */
  NvS32   class_subs[VIF_CLASS];       /* 0,1,2,3 (bits: 1<<n poss) */
  NvS32   class_book[VIF_CLASS];       /* subs ^ dim entries */
  NvS32   class_subbook[VIF_CLASS][8]; /* [VIF_CLASS][subs] */


  NvS32   mult;                      /* 1 2 3 or 4 */
  NvS32   postlist[VIF_POSIT+2];    /* first two implicit */

} NvVorbisInfoFloor1;

/* Mapping backend generic *****************************************/
typedef struct{
  vorbis_info_mapping *(*unpack)(vorbis_info *,oggpack_buffer *);
  vorbis_look_mapping *(*look)  (vorbis_dsp_state *,NvVorbisInfoMode *,
                 vorbis_info_mapping *);
  void (*free_info)    (vorbis_info_mapping *);
  void (*free_look)    (vorbis_look_mapping *);
  NvS32  (*inverse)      (struct vorbis_block *vb,vorbis_look_mapping *);
} vorbis_func_mapping;

typedef struct {
  NvS32 forward_index[VIF_POSIT+2];

  NvS32 hineighbor[VIF_POSIT];
  NvS32 loneighbor[VIF_POSIT];
  NvS32 posts;

  NvS32 n;
  NvS32 quant_q;
  NvVorbisInfoFloor1 *vi;

} NvVorbisLookFloor1;
//The above is from registry.h, backends.h


//typedef void vorbis_info_floor;
//typedef void vorbis_info_residue;
//typedef void vorbis_info_mapping;

extern vorbis_info_floor *NvFloor0Unpack (vorbis_info *vi,oggpack_buffer *opb);
extern vorbis_info_floor *NvFloor1Unpack (vorbis_info *vi,oggpack_buffer *opb);
extern vorbis_info_floor *NvRes0Unpack (vorbis_info *vi,oggpack_buffer *opb);
extern vorbis_info_mapping *NvMapping0Unpack(vorbis_info *vi,oggpack_buffer *opb);
extern void NvFloor1FreeInfo(vorbis_info_floor *i);
extern void NvFloor0FreeInfo(vorbis_info_floor *i);
extern void NvRes0FreeInfo(vorbis_info_residue *i);
extern void NvMapping0FreeInfo(vorbis_info_mapping *i);
extern void NvVorbisStaticbookDestroy(NvStaticCodebook *b);
extern void NvVorbisBookClear(NvCodebook *b);
extern NvS32 NvVorbisStaticbookUnpack(oggpack_buffer *opb,NvStaticCodebook *s);
extern void NvRes0FreeLook(vorbis_look_residue *i);
extern void NvAdvHalt(oggpack_buffer *b);
extern NvS32 NvHaltOne(oggpack_buffer *b);
extern NvS32 NvBookMaptype1Quantvals(const NvStaticCodebook *b);
extern void NvVorbisStaticbookClear(NvStaticCodebook *b);
#endif //CODEBOOK_INFO_CHANGE

#endif //_NVMM_OGGBOOKUNPACK_H

