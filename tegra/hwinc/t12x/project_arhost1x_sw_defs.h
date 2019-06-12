#if !defined(PROJECT_tlit1_arhost1x_sw_defs_H) && !defined(PROJECT_t124_arhost1x_sw_defs_H)
#define PROJECT_tlit1_arhost1x_sw_defs_H

// verify that we haven't also included a project_*.h file from another chip
#if !defined(SOME_PROJECT_arhost1x_sw_defs_H)
#define SOME_PROJECT_arhost1x_sw_defs_H
#else
#error a single file has included project_arhost1x_sw_defs.h from two chips! This is very bad
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
// project_tlit1_arhost1x_sw_defs.h - tlit1 litter arhost1x sw exported defines
//
// This file should only contain #define lines and comments


// Define the number of channels (up to 8 channels allowed) and
// read DMA ports (up to 4 ports allowed) in the design.

#define NV_HOST1X_CHANNELS         12
#define NV_HOST1X_SYNCPT_NB_PTS    192
#define NV_HOST1X_SYNCPT_NB_BASES  64

#define NV_HOST1X_SYNCPT_NB_PTS_FPGA    64
#define NV_HOST1X_SYNCPT_NB_BASES_FPGA  32

#define NV_HOST1X_DMA_DMAR_NBPORTS       1

#define NV_HOST1XW2MC_SW_RFIFODEPTH      32

#define NV_HOST1X_SYNCPT_WIDTH            32
#define NV_HOST1X_SYNCPT_THRESH_WIDTH     32
#define NV_HOST1X_SYNCPT_INT_THRESH_WIDTH NV_HOST1X_SYNCPT_THRESH_WIDTH
#define NV_HOST1X_SYNCPT_BASE_WIDTH       NV_HOST1X_SYNCPT_THRESH_WIDTH
#define NV_HOST1X_SYNCPT_OFFSET_WIDTH     NV_HOST1X_SYNCPT_THRESH_WIDTH

// arhot1x_sync.spec supports values of NB_MLOCKS
// which are multiples of 4 with a max value of 32
#define NV_HOST1X_NB_MLOCKS 16

#endif
