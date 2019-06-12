/**
* @file
* @brief <b>NVIDIA APX ODM Kit: Debug Package: NAND Debug Utility</b>
*
* @b Description:
*/

/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NAND_DEBUG_UTILITY_H
#define INCLUDED_NAND_DEBUG_UTILITY_H

#define NEW_STRAT_DEBUG 0
#define NEW_TT_DEBUG 0
#define TRACE_READ_WRITE 0

#if TRACE_READ_WRITE
#define RW_TRACE(X) { \
        NvOsDebugPrintf X; \
}
#else
    #define RW_TRACE(X)
#endif

#if NEW_TT_DEBUG
#define DEBUG_TT_INFO(X) { \
        NvOsDebugPrintf X; \
}
#else
    #define DEBUG_TT_INFO(X)
#endif

#if NEW_STRAT_DEBUG
#define ERR_DEBUG_INFO(X) { \
        NvOsDebugPrintf X; \
}
#else
    #define ERR_DEBUG_INFO(X)
#endif

//#define ENABLE_NAND_DEBUG
#ifdef ENABLE_NAND_DEBUG
    //#define NV_NAND_DEBUG_INFO NvOsDebugPrintf
    // #define TT_DEBUG
    // #define TAT_DEBUG
    // #define STRAT_DEBUG
    //#define IL_STRAT_DEBUG
    // #define COLLECT_TIME_STAMPS
    // #define ASSERT_RETURN
#else
    #define NV_NAND_DEBUG_INFO (void)
    #undef TT_DEBUG
    #undef TAT_DEBUG
    #undef STRAT_DEBUG
    #undef IL_STRAT_DEBUG
    #undef ASSERT_RETURN
    #undef GENERATE_NAND_ERRORS
#endif

// Enable this macro to see how the error hanlding works case of
// read/write/copyback errors.
// Enabling this sends the read/write/copyback errors from
// low level to the top level.
// #define GENERATE_NAND_ERRORS
// The number of page operations after which an error is generated.
#define NAND_ERROR_FRQUENCY 10001

// Enabling this macro will let the UI show the options
// 'burn keys' and 'Update FW image'
// #define ENABLE_BURNKEYS_AND_UPDATE_FW



// Number of lba's to track.
#define NUMBER_OF_LBAS_TO_TRACK 5

#include "nvos.h"
//#include "nand_interleave_ttable.h"


/**
 * Holds a flag indicating whether display is allowed.
 */
// NvBool isDisplayAllowed(DisplayDebugInfoType d) {return NV_TRUE; }
// #define HOW(X) NvOsDebugPrintf X

/*************************************************************************/
#ifdef ASSERT_RETURN
    #define RETURN(X) { \
        NvOsDebugPrintf("\r\n RETURNING FROM ASSERT_RETURN %s ",#X);\
        NV_ASSERT(NV_FALSE); \
        return X; \
    }
#else
    #define RETURN(X) { \
        return X; \
    }
#endif
#ifdef ASSERT_RETURN
    #define RETURN_VOID { \
        NvOsDebugPrintf("\r\n RETURNING FROM RETURN_VOID");\
        NV_ASSERT(NV_FALSE); \
        return; \
    }
#else
    #define RETURN_VOID return
#endif
/*************************************************************************/
#ifdef STRAT_DEBUG
    #define SHOW(X) { \
        if (NandDebugUtility::OBJ().debugEnabled()) \
        { \
            NvOsDebugPrintf X; \
        }\
    }
#else
    #define SHOW(X)
#endif
/*************************************************************************/
#ifdef IL_STRAT_DEBUG
    #define LOG_NAND_DEBUG_INFO(X,d) { \
            NvOsDebugPrintf X; \
    }
#else
    #define LOG_NAND_DEBUG_INFO(X, d)
#endif
/*************************************************************************/
#ifdef IL_STRAT_DEBUG
    #define LOG_TIME(t, s) \
     // LogTime logTime(t, s)
#else
    #define LOG_TIME(t, s)
#endif
/*************************************************************************/
#ifdef IL_STRAT_DEBUG
    #define LOG_NAND_DEBUG_INFO_CONSOLE(X) { \
        // EnableDebug debug;
        NvOsDebugPrintf X; \
    }
#else
    #define LOG_NAND_DEBUG_INFO_CONSOLE(X)
#endif
/*************************************************************************/
/**
 * @par Header:
 * Declared in %nand_debug_utility.h
 * @ingroup debugConsts
 */
#define ChkILTTErr(sts, rtnSts) { \
    if (sts != NvSuccess)   \
    {   \
        RETURN(rtnSts); \
    }\
}
/*************************************************************************/
/**
 * @par Header:
 * Declared in %nand_debug_utility.h
 * @ingroup debugConsts
 */
#define ChkILTATErr(sts, rtnSts) { \
    if (sts != NvSuccess)   \
    {   \
        RETURN(rtnSts); \
    }\
}

/*************************************************************************/

#define ChkTLErr(sts, rtnSts) { \
    if (sts != NvSuccess)   \
    {   \
        RETURN(rtnSts); \
    }\
}

#define CheckNvError(sts, rtnSts) { \
    if (sts != NvSuccess)   \
    {   \
        RETURN(rtnSts); \
    }\
}

#ifdef ASSERT_RETURN
#define ExitOnTLErr(sts) { \
    if (sts != NvSuccess)   \
    {   \
        NvOsDebugPrintf("\r\n RETURNING FROM ExitOnTLErr");\
         NV_ASSERT(NV_FALSE); \
         goto errorExit; \
    }\
}
#else
#define ExitOnTLErr(sts) { \
    if (sts != NvSuccess)   \
    {   \
        goto errorExit; \
    }\
}
#endif
/*************************************************************************/
#ifdef GENERATE_NAND_ERRORS
    #define GENERATE_READ_ERROR { \
        static NvS32 s_readCount = 0; \
        readCount++; \
        if (readCount >= NAND_ERROR_FRQUENCY) \
        { \
            readCount = 0; \
            status = ERROR_CORRECTION_THRESHOLD_REACHED; \
            LOG_NAND_DEBUG_INFO_CONSOLE(("\r\n Generated Read Error")); \
        }\
    }
#else
    #define GENERATE_READ_ERROR
#endif
/*************************************************************************/
#ifdef GENERATE_NAND_ERRORS
    #define GENERATE_WRITE_ERROR { \
        static NvS32 s_writeCount = 0; \
        writeCount++; \
        if (writeCount >= NAND_ERROR_FRQUENCY) \
        { \
            writeCount = 0; \
            status = HwError_WRITE_ERROR; \
            LOG_NAND_DEBUG_INFO_CONSOLE(("\r\n Generated Write Error")); \
        }\
    }
#else
    #define GENERATE_WRITE_ERROR
#endif
/*************************************************************************/
#ifdef GENERATE_NAND_ERRORS
    #define GENERATE_COPYBACK_ERROR { \
        static NvS32 s_copybackCount = 0; \
        copybackCount++; \
        if (copybackCount >= NAND_ERROR_FRQUENCY) \
        { \
            copybackCount = 0; \
            status = HwError_WRITE_ERROR; \
            LOG_NAND_DEBUG_INFO_CONSOLE(("\r\n\r\n Generated Copyback Error")); \
        }\
    }
#else
    #define GENERATE_COPYBACK_ERROR
#endif

struct NandDriverDebugTiming
{
    NvU64 CmdSemaWaitStartTime;
    NvU64 CmdSemaWaitEndTime;
    NvU64 CmdSemaSignalStartTime;
    NvU64 CmdSemaSignalEndTime;

    NvU64 DmaSemaWaitStartTime;
    NvU64 DmaSemaWaitEndTime;
    NvU64 DmaSemaSignalStartTime;
    NvU64 DmaSemaSignalEndTime;

    NvU64 OperationStartTime;
    NvU64 OperationEndTime;
};

enum DisplayDebugInfoType
{
    // The display message is from NandStrategyInterleave class.
    IL_STRAT_DEBUG_INFO = 0,
    // The display message is from NandStrategyInterleave class during read.
    IL_STRAT_READ_DEBUG_INFO,
    // The display message is from NandStrategyInterleave class during write.
    IL_STRAT_WRITE_DEBUG_INFO,
    // The display message request during bad block replacement/Marking.
    BAD_BLOCK_DEBUG_INFO,
    // The display message request during bad block replacement/Marking.
    WEAR_LEVEL_DEBUG_INFO,
    // The display message request during the Erase/known/Unknown error(s).
    ERROR_DEBUG_INFO,
    // The display message is from NandInterleaveTTable class
    // and is related to block allocation.
    TT_DEBUG_INFO,
    // The display message is from NandInterleaveTTable class
    // and is related to TT page operation.
    TT_PAGE_DEBUG_INFO,
    // The display message is from NandInterleaveTAT class.
    TAT_DEBUG_INFO,
    // The display message is from HwNand class.
    HW_NAND_DEBUG_INFO,
    // The display message is from low level Nand driver class.
    NANDDRIVER_DEBUG_INFO,
    // The display message is from Nand Format class.
    NAND_FORMAT_DEBUG_INFO
};

/**
 * Defines the desplay message NAND time information type.
 */
enum DisplayNandTimeInfoType
{
    // Nand Read time
    NAND_READ_TIME = 0,
    // Nand Write time
    NAND_WRITE_TIME,
    // Nand copyback time
    NAND_COPYBACK_TIME,
    // Nand Erase time
    NAND_ERASE_TIME
};

#endif// INCLUDED_NAND_DEBUG_UTILITY_H
