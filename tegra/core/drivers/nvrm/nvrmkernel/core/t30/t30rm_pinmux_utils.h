/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef T30RM_PINMUX_UTILS_H
#define T30RM_PINMUX_UTILS_H

/*
 * t30rm_pinmux_utils.h defines the pinmux macros to implement for the resource
 * manager.
 */

#include "nvrm_pinmux_utils.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

/* When the state is BranchLink, this is the number of words to increment
 * the current "PC"
 */
#define MUX_ENTRY_0_BRANCH_ADDRESS_RANGE 31:2
//  The incr1 offset from PINMUX_AUX_ULPI_DATA0_0 to the pad's mux register
#define MUX_ENTRY_0_MUX_REG_OFFSET_RANGE 31:23
// Thebit position of pad input mode enable/disable
#define MUX_ENTRY_0_PAD_INPUT_SHIFT_RANGE 22:18
//  The input enable/disable for the pad
#define MUX_ENTRY_0_PAD_INPUT_MASK_RANGE 17:17
//  The input enable/disable for the pad
#define MUX_ENTRY_0_PAD_INPUT_RANGE 16:16
//  The bit position of pad mux
#define MUX_ENTRY_0_PAD_MUX_SHIFT_RANGE 15:11
//  The mask for the pad -- expanded to 3b for forward-compatibility
#define MUX_ENTRY_0_PAD_MUX_MASK_RANGE 10:8
//  When a pad needs to be owned (or disowned), this value is applied
#define MUX_ENTRY_0_PAD_MUX_SET_RANGE 7:5
//  This value is compared against, to determine if the pad should be disowned
#define MUX_ENTRY_0_PAD_MUX_UNSET_RANGE 4:2
//  for extended opcodes, this field is set with the extended opcode
#define MUX_ENTRY_0_OPCODE_EXTENSION_RANGE 3:2
//  The state for this entry
#define MUX_ENTRY_0_STATE_RANGE 1:0

/*  This macro is used to generate 32b value to program the  pad mux control
 *  registers for config/unconfig for a pad
 */
#define PIN_MUX_ENTRY(REGOFF,INSHIFT,INMASK,IN,MUXSHIFT,MUXMASK,MUXSET, \
    MUXUNSET,STAT) \
    (NV_DRF_NUM(MUX, ENTRY, MUX_REG_OFFSET, REGOFF)     | \
     NV_DRF_NUM(MUX, ENTRY, PAD_INPUT_SHIFT, INSHIFT)   | \
     NV_DRF_NUM(MUX, ENTRY, PAD_INPUT_MASK, INMASK)     | \
     NV_DRF_NUM(MUX, ENTRY, PAD_INPUT, IN)              | \
     NV_DRF_NUM(MUX, ENTRY, PAD_MUX_SHIFT, MUXSHIFT)    | \
     NV_DRF_NUM(MUX, ENTRY, PAD_MUX_MASK, MUXMASK)      | \
     NV_DRF_NUM(MUX, ENTRY, PAD_MUX_SET, MUXSET)        | \
     NV_DRF_NUM(MUX, ENTRY, PAD_MUX_UNSET, MUXUNSET)    | \
     NV_DRF_NUM(MUX, ENTRY, STATE,STAT))

//  This is used to program the pad mux registers for a pad
#define CONFIG_VAL(PAD, IN, MUX)                                            \
    (PIN_MUX_ENTRY(((PINMUX_AUX_##PAD##_0 - PINMUX_AUX_ULPI_DATA0_0)>>2),   \
                     PINMUX_AUX_##PAD##_0_E_INPUT_SHIFT,                    \
                     PINMUX_AUX_##PAD##_0_E_INPUT_DEFAULT_MASK,             \
                     PINMUX_AUX_##PAD##_0_E_INPUT_##IN,                     \
                     PINMUX_AUX_##PAD##_0_PM_SHIFT,                         \
                     PINMUX_AUX_##PAD##_0_PM_DEFAULT_MASK,                  \
                     PINMUX_AUX_##PAD##_0_PM_##MUX,                         \
                     0,                                                     \
                     PinMuxConfig_Set))

/* This macro is used to compare a pad mux against a potentially conflicting
 * enum (where the conflict is caused by setting a new config), and to resolve
 * the conflict by setting the conflicting pad mux to a different,
 * non-conflicting option. Read this as: if (PADMUX) is equal to
 * (CONFLICTMUX), replace it with (RESOLUTIONMUX)
 */
#define UNCONFIG_VAL(PAD, IN, CONFLICTMUX, RESOLUTIONMUX)                   \
    (PIN_MUX_ENTRY(((PINMUX_AUX_##PAD##_0 - PINMUX_AUX_ULPI_DATA0_0) >> 2), \
                     PINMUX_AUX_##PAD##_0_E_INPUT_SHIFT,                    \
                     PINMUX_AUX_##PAD##_0_E_INPUT_DEFAULT_MASK,             \
                     PINMUX_AUX_##PAD##_0_E_INPUT_##IN,                     \
                     PINMUX_AUX_##PAD##_0_PM_SHIFT,                         \
                     PINMUX_AUX_##PAD##_0_PM_DEFAULT_MASK,                  \
                     PINMUX_AUX_##PAD##_0_PM_##RESOLUTIONMUX,               \
                     PINMUX_AUX_##PAD##_0_PM_##CONFLICTMUX,                 \
                     PinMuxConfig_Unset))

#if NVRM_PINMUX_DEBUG_FLAG
#define CONFIG(PAD, INPUT, MUX) \
    (CONFIG_VAL(PAD, INPUT, MUX)), \
    (NvU32)(const void*)("PINMUX_AUX_0_" #PAD "_PM to " #MUX), \
    (NvU32)(const void*)("PINMUX_AUX_0_" #PAD "_TRISTATE")

#define UNCONFIG(PAD, INPUT, CONFLICTMUX, RESOLUTIONMUX) \
    (UNCONFIG_VAL(PAD, INPUT, CONFLICTMUX, RESOLUTIONMUX)), \
    (NvU32)(const void*) \
        ("PINMUX_AUX_0_" #PAD "_PM from " #CONFLICTMUX " to " #RESOLUTIONMUX), \
    (NvU32)(const void*)(NULL)
#else
#define CONFIG(PAD, INPUT, MUX) \
    (CONFIG_VAL(PAD, INPUT, MUX))
#define UNCONFIG(PAD, INPUT, CONFLICTMUX, RESOLUTIONMUX) \
    (UNCONFIG_VAL(PAD, INPUT, CONFLICTMUX, RESOLUTIONMUX))
#endif

//  The below entries define the table format for GPIO Port/Pin-to-Tristate register mappings
//  Each table entry is 16b, and one is stored for every GPIO Port/Pin on the chip
#define MUX_GPIOMAP_0_TS_OFFSET_RANGE 8:0

#define TRISTATE_ENTRY(TSOFFS) \
    ((NvU16)(NV_DRF_NUM(MUX,GPIOMAP,TS_OFFSET,(TSOFFS))))

#define GPIO_TRISTATE(TRIREG) \
    (TRISTATE_ENTRY((PINMUX_AUX_##TRIREG##_0 - PINMUX_AUX_ULPI_DATA0_0)>>2))

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // T30RM_PINMUX_UTILS_H

