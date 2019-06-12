/**
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * data_layout.h - Definitions for the nvbuildbct data layout code.
 */

/*
 * TODO / Notes
 * - Add doxygen commentary
 */

#ifndef INCLUDED_DATA_LAYOUT_H
#define INCLUDED_DATA_LAYOUT_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    UpdateEndState_None = 0,
    /* BeginUpdate() */
    UpdateEndState_EraseB0_JournalErasure,
    UpdateEndState_WriteEmptyBctB0S0,
    UpdateEndState_WriteBctB0S1,
    UpdateEndState_EraseJournalBlock,
    UpdateEndState_WriteJournalBctOld,
    UpdateEndState_EraseBlock0_Normal,

    /* WriteBootloaders() */
    /* The following must remain in consecutive order. Violating this
     * constraint requires a change to the code in WriteBootloaders().
     */
    UpdateEndState_WriteBl0,
    UpdateEndState_WriteBl1,
    UpdateEndState_WriteBl2,
    UpdateEndState_WriteBl3,

    /* Finish Update() */
    UpdateEndState_WriteJournalBctNew,
    UpdateEndState_WriteBctB0S0,

    /* TODO: Add more states here. */

    UpdateEndState_Complete, /* Must be the last usable state. */

    UpdateEndState_Force32 = 0x7fffffff
} UpdateEndState;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_DATA_LAYOUT_H */
