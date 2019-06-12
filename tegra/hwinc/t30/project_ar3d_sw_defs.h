#if !defined(PROJECT_t30_ar3d_sw_defs_H)
#define PROJECT_t30_ar3d_sw_defs_H

// verify that we haven't also included a project_*.h file from another chip
#if !defined(SOME_PROJECT_ar3d_sw_defs_H)
#define SOME_PROJECT_ar3d_sw_defs_H
#else
#error a single file has included project_ar3d_sw_defs.h from two chips! This is very bad
#endif

// --------------------------------------------------------------------------
//
// Copyright (c) 2007, NVIDIA Corp.
// All Rights Reserved.
//
// This is UNPUBLISHED PROPRIETARY SOURCE CODE of NVIDIA Corp.;
// the contents of this file may not be disclosed to third parties, copied or
// duplicated in any form, in whole or in part, without the prior written
// permission of NVIDIA Corp.
//
// RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
// and Computer Software clause at DFARS 252.227-7013, and/or in similar or
// successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
// rights reserved under the Copyright Laws of the United States.
//
// --------------------------------------------------------------------------
//
//
// project_ap15_ar3d_sw_defs.h - ap15 project ar3d sw exported defines 
//
// This file should only contain #define lines and comments
  // SLI
  #define NV_GR3D_HAS_DUAL_GR3D                1

  // CLM - adding geometry defines
  #define NV_GR3D_IDX_NUM_ATTR                 16
  // this is the max internal index size.  it can't be larger than 24 currently
  #define NV_GR3D_IDX_MAX_INDEX_BITS      20

  #define NV_GR3D_VPE_NUM_INSNS                256
  #define NV_GR3D_VPE_INSN_ADDRBITS            log2(NV_GR3D_VPE_NUM_INSNS)
  #define NV_GR3D_VPE_NUM_CONST_VECS           256
  #define NV_GR3D_VPE_CONST_ADDRBITS           log2(NV_GR3D_VPE_NUM_CONST_VECS)

  // compressed texture formats
  #define NV_GR3D_HAS_DXT3                     1
  #define NV_GR3D_HAS_DXT5                     1

  // new texture formats
  #define NV_GR3D_HAS_YUV422                   1
  #define NV_GR3D_HAS_R10G10B10A2              1

  // user clip planes
  #define NV_GR3D_HAS_USER_CLIP_PLANE          1
  #define NV_GR3D_NUM_UC_PLANES                1

  // (partial) additional support for antialiased points/lines, bug 187304
  #define NV_GR3D_HAS_AA_POINTS_LINES          1

  // layout changes for large address space (bug 254552)
  #define NV_GR3D_HAS_LARGE_ADDRESS_SPACE      1

  // ARB_texture_rectangle support
  #define NV_GR3D_HAS_TEXTURE_RECTANGLE        1

  // support for surface tiling/swizzling
  #define NV_GR3D_SURFACE_TILE_SWIZZLE         1

  // defined when an asim exists (in cmod/gr3d/asim)
  #define NV_GR3D_HAS_ASIM                     1

  // screen resolution and precision (note that the raster grid is (+/-)NV_GR3D_XRES_LOG2.NV_GR3D_SNAP_LOG2 bits)
  #define NV_GR3D_SNAP_LOG2                    4
  #define NV_GR3D_SNAP_ONE                     (1 << NV_GR3D_SNAP_LOG2)
  #define NV_GR3D_XRES_LOG2                    12
  #define NV_GR3D_YRES_LOG2                    NV_GR3D_XRES_LOG2 // for now, the same precision in x and y
  #define NV_GR3D_ZRES_LOG2                    16
  #define NV_GR3D_XRES                         (1 << NV_GR3D_XRES_LOG2)
  #define NV_GR3D_YRES                         (1 << NV_GR3D_YRES_LOG2)
  #define NV_GR3D_ZRES                         (1 << NV_GR3D_ZRES_LOG2)
  #define NV_GR3D_XRES_PADDED_LOG2             (NV_GR3D_XRES_LOG2 + 1)
  #define NV_GR3D_YRES_PADDED_LOG2             (NV_GR3D_YRES_LOG2 + 1)
  #define NV_GR3D_XRES_PADDED                  (1 << NV_GR3D_XRES_PADDED_LOG2)
  #define NV_GR3D_YRES_PADDED                  (1 << NV_GR3D_YRES_PADDED_LOG2)

  #define NV_GR3D_Z_SURF_PTR                   0
  #define NV_GR3D_S_SURF_PTR                   2
  #define NV_GR3D_V_SURF_PTR                   3

  #define NV_GR3D_HAS_PSEQ                     1
  #define NV_GR3D_HAS_TEX                      1
  #define NV_GR3D_TEX_HAS_ANISO                1

  // number of entries in ATRAST instruction table
  #define NV_GR3D_ATRS_INST_NUM_WORDS          2
  #define NV_GR3D_NUM_PIXPKT_ATRS_INST         (NV_GR3D_ATRS_INST_NUM_WORDS * NV_GR3D_NUM_PIXPKT_INST)
  #define NV_GR3D_NUM_PIXPKT_ATRS_INST_LOG2    log2(NV_GR3D_NUM_PIXPKT_ATRS_INST)

  // number of triangles allowed in fragment pipe at one time
  #define NV_GR3D_NUM_TRIANGLES_IN_FLIGHT      64
  #define NV_GR3D_NUM_TRIANGLES_IN_FLIGHT_LOG2 log2(NV_GR3D_NUM_TRIANGLES_IN_FLIGHT)

  // total depth of ATRAST's TRAM
  #define NV_GR3D_TRAM_ROWS                    NV_GR3D_NUM_TRIANGLES_IN_FLIGHT
  #define NV_GR3D_TRAM_ROWS_LOG2               log2(NV_GR3D_TRAM_ROWS)
  #define NV_GR3D_TRAM_BANKS                   4
  #define NV_GR3D_TRAM_BANKS_LOG2              log2(NV_GR3D_TRAM_BANKS)

  // TRAM entries are 64-bits each
  #define NV_GR3D_TRAM_ENTRY_SIZE              2
  #define NV_GR3D_TRAM_ENTRY_SIZE_LOG2         log2(NV_GR3D_TRAM_ENTRY_SIZE)

  // total number of 32-bit words in the TRAM
  #define NV_GR3D_TRAM_ENTRIES                 (NV_GR3D_TRAM_ROWS * NV_GR3D_TRAM_BANKS * NV_GR3D_TRAM_ENTRY_SIZE)
  #define NV_GR3D_TRAM_ENTRIES_LOG2            log2(NV_GR3D_TRAM_ENTRIES)

  // maximum number of read surface descriptors
  #define NV_GR3D_NUM_TEXTURES                16
  #define NV_GR3D_NUM_TEXTURES_BY_4           (NV_GR3D_NUM_TEXTURES / 4)
  #define NV_GR3D_NUM_TEXDESC_ENTRIES         (2 * NV_GR3D_NUM_TEXTURES)
  #define NV_GR3D_NUM_SURFACES                16
  #define NV_GR3D_NUM_SURFACES_BY_4           (NV_GR3D_NUM_SURFACES / 4)
  #define NV_GR3D_TEX_INST_NUM_WORDS          1
  #define NV_GR3D_NUM_PIXPKT_TEX_INST         (NV_GR3D_TEX_INST_NUM_WORDS * NV_GR3D_NUM_PIXPKT_INST)

  // maximum recommended value for PSEQ_CTL.MAX_OUT and PSEQ_CTL.MIN_OUT
  // through experiment we found that 388 packets in the fragment loop can cause hangs.
  // setting default to 2 quads less than that for margin
  #define NV_GR3D_PSEQ_MAX_OUT_RECIRC_DEFAULT          356
  #define NV_GR3D_PSEQ_MIN_OUT_RECIRC_DEFAULT          256

  // maximum number of write surface descriptors
  #define NV_GR3D_NUM_WRITE_SURFACES          16

  //number of pseq command instructions/words
  #define NV_GR3D_NUM_PSEQ_WORDS_PER_COMMAND  2
  #define NV_GR3D_NUM_PSEQ_COMMANDS           16
  #define NV_GR3D_NUM_PSEQ_COMMANDS_LOG2      log2(NV_GR3D_NUM_PSEQ_COMMANDS)
  #define NV_GR3D_NUM_PSEQ_COMMAND_WORDS      (NV_GR3D_NUM_PSEQ_COMMANDS * NV_GR3D_NUM_PSEQ_WORDS_PER_COMMAND)

  #define NV_GR3D_DW_INST_NUM_WORDS           1
  #define NV_GR3D_NUM_PIXPKT_DW_INST          (NV_GR3D_DW_INST_NUM_WORDS * NV_GR3D_NUM_PIXPKT_INST)

  #define NV_GR3D_P2CX_INST_NUM_WORDS         1
  #define NV_GR3D_NUM_PIXPKT_P2CX_INST        (NV_GR3D_P2CX_INST_NUM_WORDS * NV_GR3D_NUM_PIXPKT_INST)
  #define NV_GR3D_NUM_PIXPKT_P2CX_INST_LOG2   log2(NV_GR3D_NUM_PIXPKT_P2CX_INST)

  // maximum number of pixel packet rows
  #define NV_GR3D_NUM_PIXPKT_ROWS              4
  // maximum number of pixel packet registers
  #define NV_GR3D_NUM_PIXPKT_REGS              4
  // number of alus
  #define NV_GR3D_NUM_ALUS                     4
  // maximum number of alu scratch registers
  #define NV_GR3D_NUM_ALU_SCRATCH              8
  // maximum number of alu globals (constants)
  #define NV_GR3D_NUM_ALU_GLOBALS              32
  // maximum number instructions in the setup unit
  #define NV_GR3D_NUM_SETUP_INST_LOG2          5
  #define NV_GR3D_NUM_SETUP_INST               (1<<NV_GR3D_NUM_SETUP_INST_LOG2)
  // number high/low-precision instructions per row in raster
  #define NV_GR3D_NUM_RASTER_HP_INST_PER_ROW   4
  #define NV_GR3D_NUM_RASTER_LP_INST_PER_ROW   4
  #define NV_GR3D_NUM_RASTER_INST_PER_ROW      (NV_GR3D_NUM_RASTER_HP_INST_PER_ROW + NV_GR3D_NUM_RASTER_LP_INST_PER_ROW)
  // maximum number instructions in the pixel pipe
  #define NV_GR3D_NUM_PIXPKT_INST              64
  #define NV_GR3D_NUM_PIXPKT_INST_LOG2         log2(NV_GR3D_NUM_PIXPKT_INST)
  // ALU instructions are actually an interleaved array of 64-bit instructions (should really fix simspec instead)
  #define NV_GR3D_ALU_INST_NUM_WORDS           2
  #define NV_GR3D_NUM_PIXPKT_ALU_INST          (NV_GR3D_ALU_INST_NUM_WORDS * NV_GR3D_NUM_ALUS * NV_GR3D_NUM_PIXPKT_INST)
  #define NV_GR3D_NUM_PIXPKT_ALU_INST_LOG2     log2(NV_GR3D_NUM_PIXPKT_ALU_INST)
  // ALU instructions are actually an interleaved array of 64-bit instructions (should really fix simspec instead)
  #define NV_GR3D_PSEQ_INST_NUM_WORDS         1
  #define NV_GR3D_NUM_PIXPKT_PSEQ_INST        (NV_GR3D_PSEQ_INST_NUM_WORDS * NV_GR3D_NUM_PIXPKT_INST)
  #define NV_GR3D_NUM_PIXPKT_PSEQ_INST_LOG2   log2(NV_GR3D_NUM_PIXPKT_PSEQ_INST)
  // number instructions in the pixel pipe divided by 4
  #define NV_GR3D_NUM_PIXPKT_INST_BY_4         (NV_GR3D_NUM_PIXPKT_INST / 4)
  // number of sets of statistics registers
  #define NV_GR3D_NUM_STAT_SETS                2

  // fdc framebuffer access alignment requirements
  #define NV_GR3D_SURF_ADDRESS_ALIGNMENT_BYTES  16
  #define NV_GR3D_SURF_ADDRESS_ALIGNMENT_FAST_BYTES     32

  // maximum texture size on a side, log2
  #define NV_GR3D_TEX_MAX_SIZE_LOG2                     11
  #define NV_GR3D_TEX_MAX_SIZE                          (1<<NV_GR3D_TEX_MAX_SIZE_LOG2)
  // alignment requirement for surface start address in bytes
  #define NV_GR3D_TEX_SURFACE_ALIGNMENT_BYTES_LOG2      5
  #define NV_GR3D_TEX_SURFACE_ALIGNMENT_BYTES           (1 << NV_GR3D_TEX_SURFACE_ALIGNMENT_BYTES_LOG2)
  // alignment requirement for npot surface start address in bytes
  #define NV_GR3D_TEX_NPOT_SURFACE_ALIGNMENT_BYTES_LOG2 4
  #define NV_GR3D_TEX_NPOT_SURFACE_ALIGNMENT_BYTES      (1 << NV_GR3D_TEX_NPOT_SURFACE_ALIGNMENT_BYTES_LOG2)
  // alignment requirement for a power-of-two texture slice (cubemap face, array slice) in bytes
  #define NV_GR3D_TEX_SLICE_ALIGNMENT_BYTES_LOG2        10
  #define NV_GR3D_TEX_SLICE_ALIGNMENT_BYTES             (1 << NV_GR3D_TEX_SLICE_ALIGNMENT_BYTES_LOG2)
  // alignment requirement for a non-power-of-two texture slice (cubemap face, array slice) in bytes
  #define NV_GR3D_TEX_NPOT_SLICE_ALIGNMENT_BYTES_LOG2   10
  #define NV_GR3D_TEX_NPOT_SLICE_ALIGNMENT_BYTES        (1 << NV_GR3D_TEX_NPOT_SLICE_ALIGNMENT_BYTES_LOG2)
  // alignment requirement for individual mipmap level in bytes
  #define NV_GR3D_TEX_MIPMAP_ALIGNMENT_BYTES_LOG2       4
  #define NV_GR3D_TEX_MIPMAP_ALIGNMENT_BYTES            (1 << NV_GR3D_TEX_MIPMAP_ALIGNMENT_BYTES_LOG2)
  // texture level minimum addressable size in bytes
  #define NV_GR3D_TEX_MIN_SIZE_BYTES_LOG2               4
  #define NV_GR3D_TEX_MIN_SIZE_BYTES                    (1 << NV_GR3D_TEX_MIN_SIZE_BYTES_LOG2)
  // texture minimum row length in bytes (the texture cache line allows split addressing of the alignment bytes)
  #define NV_GR3D_TEX_MIN_STRIDE_BYTES_LOG2             4
  #define NV_GR3D_TEX_MIN_STRIDE_BYTES                  (1 << NV_GR3D_TEX_MIN_STRIDE_BYTES_LOG2)
  // texture stride alignment
  #define NV_GR3D_TEX_STRIDE_ALIGNMENT_BYTES_LOG2       4
  #define NV_GR3D_TEX_STRIDE_ALIGNMENT_BYTES            (1 << NV_GR3D_TEX_STRIDE_ALIGNMENT_BYTES_LOG2)
  // texture stride alignment
  #define NV_GR3D_TEX_NPOT_STRIDE_ALIGNMENT_BYTES_LOG2  6
  #define NV_GR3D_TEX_NPOT_STRIDE_ALIGNMENT_BYTES       (1 << NV_GR3D_TEX_NPOT_STRIDE_ALIGNMENT_BYTES_LOG2)

  // texture height alignment
  #define NV_GR3D_TEX_HEIGHT_ALIGNMENT_LINES_LOG2       0
  #define NV_GR3D_TEX_HEIGHT_ALIGNMENT_LINES            (1 << NV_GR3D_TEX_HEIGHT_ALIGNMENT_LINES_LOG2)

  // texture height alignment
  #define NV_GR3D_TEX_NPOT_HEIGHT_ALIGNMENT_LINES_LOG2  4
  #define NV_GR3D_TEX_NPOT_HEIGHT_ALIGNMENT_LINES       (1 << NV_GR3D_TEX_NPOT_HEIGHT_ALIGNMENT_LINES_LOG2)

  // texture LOD_BIAS register fractional bits
  #define NV_GR3D_LOD_BIAS_FRAC_BITS               4
  // maximum number of array slices
  #define NV_GR3D_TEX_MAX_ARRAY_SLICES                  64
  // maximum texture size in bytes
  #define NV_GR3D_TEX_MAX_SIZE_BYTES                    (64*1024*1024)

  #define NV_GR3D_SWIZZLED_TEX_TILE_SIZE            (16) /* each tile is 16 bytes */

  #define NV_GR3D_SWIZZLED_TEX_4BPP_TILE_WIDTH_LOG2   (3) /* 8*4 */
  #define NV_GR3D_SWIZZLED_TEX_4BPP_TILE_HEIGHT_LOG2  (2)
  #define NV_GR3D_SWIZZLED_TEX_8BPP_TILE_WIDTH_LOG2   (2) /* 4x4 */
  #define NV_GR3D_SWIZZLED_TEX_8BPP_TILE_HEIGHT_LOG2  (2)
  #define NV_GR3D_SWIZZLED_TEX_16BPP_TILE_WIDTH_LOG2  (2) /* 4x2 */
  #define NV_GR3D_SWIZZLED_TEX_16BPP_TILE_HEIGHT_LOG2 (1)
  #define NV_GR3D_SWIZZLED_TEX_32BPP_TILE_WIDTH_LOG2  (1) /* 2x2 */
  #define NV_GR3D_SWIZZLED_TEX_32BPP_TILE_HEIGHT_LOG2 (1)
  #define NV_GR3D_SWIZZLED_TEX_64BPP_TILE_WIDTH_LOG2  (0) /* 1x2 */
  #define NV_GR3D_SWIZZLED_TEX_64BPP_TILE_HEIGHT_LOG2 (1)

  #define NV_GR3D_SWIZZLED_TEX_4BPP_TILE_WIDTH   (1 << NV_GR3D_SWIZZLED_TEX_4BPP_TILE_WIDTH_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_4BPP_TILE_HEIGHT  (1 << NV_GR3D_SWIZZLED_TEX_4BPP_TILE_HEIGHT_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_8BPP_TILE_WIDTH   (1 << NV_GR3D_SWIZZLED_TEX_8BPP_TILE_WIDTH_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_8BPP_TILE_HEIGHT  (1 << NV_GR3D_SWIZZLED_TEX_8BPP_TILE_HEIGHT_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_16BPP_TILE_WIDTH  (1 << NV_GR3D_SWIZZLED_TEX_16BPP_TILE_WIDTH_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_16BPP_TILE_HEIGHT (1 << NV_GR3D_SWIZZLED_TEX_16BPP_TILE_HEIGHT_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_32BPP_TILE_WIDTH  (1 << NV_GR3D_SWIZZLED_TEX_32BPP_TILE_WIDTH_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_32BPP_TILE_HEIGHT (1 << NV_GR3D_SWIZZLED_TEX_32BPP_TILE_HEIGHT_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_64BPP_TILE_WIDTH  (1 << NV_GR3D_SWIZZLED_TEX_64BPP_TILE_WIDTH_LOG2)
  #define NV_GR3D_SWIZZLED_TEX_64BPP_TILE_HEIGHT (1 << NV_GR3D_SWIZZLED_TEX_64BPP_TILE_HEIGHT_LOG2)

#endif
