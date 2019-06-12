/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMU_DIAG_H__
#define __NVMU_DIAG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmu_types.h"

#define NVMU_DIAG_MAX_DESC_SIZE (100)
#define NVMU_DIAG_MAX_ENTRY_SIZE (50)

/**
 * \brief Diagnostic server handle type
 */
typedef void* NvMU_DiagServer;

/**
 * \brief Diagnostic client handle type
 */
typedef void* NvMU_DiagClient;

/**
 * \brief Diagnostic app handle type
 */
typedef void* NvMU_DiagAppCtx;

/**
 * \brief Diagnostic callback type
 */
typedef enum {
    NVMU_DIAG_CB_MSG_ERROR,
    NVMU_DIAG_CB_MSG_INFO,
    NVMU_DAIG_CB_MSG_UND = 0x7FFFFFFF
} NvMU_DiagCbMsgType;

/**
 * \brief Diagnostic callback message struct
 */
typedef struct {
    void *pData;
    U32  uDataSize;
    NvMU_DiagCbMsgType eType;
    NvMU_DiagAppCtx hCtx;
} NvMU_DiagCbMsg;

/**
 * \brief Diagnostic callback prototype
 */
typedef NvMU_Status
(*NvMU_DiagCb)(NvMU_DiagCbMsg *pMsg);

/**
 * \brief Diagnostic configuration
 */
typedef struct {
    /*! IPC path */
    char *pPath;

    /*! Max clients allowed */
    U32 uMaxNumDiags;
    /*! Max states per client */
    U32 uMaxNumStates;
    /*! Max number of entries per state */
    U32 uMaxNumEntries;

    /*! Will the caller ask for overall diagnostic info */
    Bool bNeedDiagInfo;

    /*! Diagnostic app context */
    NvMU_DiagAppCtx hCtx;
    /*! Diagnostic app callback */
    NvMU_DiagCb hCb;
} NvMU_DiagConfig;

/**
 * \brief Diagnostics entry type
 */
typedef enum {
    NVMU_DIAG_ENTRY_TYPE_STR = 0,
    NVMU_DIAG_ENTRY_TYPE_U8  = 1,
    NVMU_DIAG_ENTRY_TYPE_U16 = 2,
    NVMU_DIAG_ENTRY_TYPE_U32 = 3,
    NVMU_DIAG_ENTRY_TYPE_U64 = 4,
    NVMU_DIAG_ENTRY_TYPE_S32 = 5,
    NVMU_DIAG_ENTRY_TYPE_S64 = 6,
    NVMU_DIAG_ENTRY_TYPE_UND = 0x7FFFFFFF
} NvMU_DiagEntryType;

/**
 * \brief type to hold single diagnostic state entry
 */
typedef struct {
    NvMU_DiagEntryType eType;
    char Desc[NVMU_DIAG_MAX_DESC_SIZE];
    union {
        char DataStr[NVMU_DIAG_MAX_ENTRY_SIZE];
        U8   uDataU8;
        U16  uDataU16;
        U32  uDataU32;
        U64  uDataU64;
        S32  sDataS32;
        S64  sDataS64;
    } Entry;
} NvMU_DiagStateEntry;

/**
 * \brief type to hold single diagnostic state
 */
typedef struct {
    U32  uNumEntries;
    NvMU_DiagStateEntry *pEntries;
} NvMU_DiagState;

/**
 * \brief type to hold diagnostic states for a client
 */
typedef struct {
    U32  uNumStates;
    NvMU_DiagState *pStates;
} NvMU_DiagStateInfo;

/**
 * \brief type to hold states of all the clients
 */
typedef struct {
    U32  uNumDiags;
    NvMU_DiagStateInfo *pInfo;
} NvMU_DiagInfo;


/**
 * \brief Open a diagnostic server instance
 *
 * \param[out] phDiag pointer to diagnostic handle
 * \param[in] pDiagConfig pointer to diagnostic config
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_DiagServerOpen(NvMU_DiagServer *phDiag, NvMU_DiagConfig *pConfig);

/**
 * \brief Set server state
 *
 * \param[in] hDiag diagnostic handle
 * \param[in] pState pointer to server state
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_DiagServerSetState(NvMU_DiagClient hDiag, NvMU_DiagState *pState);

/**
 * \brief Get overall diagnostic information
 *
 * \param[in] hDiag diagnostic handle
 * \param[out] ppDiagInfo pointer to overall diagnostic information.
 *                        This memory is owned by implementation.
 *                        This gets updated with every GetDiagInfo call.
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_DiagServerGetDiagInfo(NvMU_DiagClient hDiag, NvMU_DiagInfo **pDiagInfo);

/**
 * \brief Close diagnostic server instance
 *
 * \param[in] phDiag pointer to diagnostic handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_DiagServerClose(NvMU_DiagServer *phDiag);

/**
 * \brief Open a diagnostic client instance
 *
 * \param[out] phDiag pointer to diagnostic handle
 * \param[in] pDiagConfig pointer to diagnostic config
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_DiagClientOpen(NvMU_DiagClient *phDiag, NvMU_DiagConfig *pConfig);

/**
 * \brief Set client diagnostic state
 *
 * \param[in] hDiag diagnostic handle
 * \param[in] pState pointer to diagnostic state
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_DiagClientSetState(NvMU_DiagClient hDiag, NvMU_DiagState *pState);

/**
 * \brief Get overall diagnostic information
 *
 * \param[in] hDiag diagnostic handle
 * \param[in] ppDiagInfo pointer to overall diagnostic information
 *                       This memory is owned by implementation.
 *                       This gets updated with every GetDiagInfo call.
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_DiagClientGetDiagInfo(NvMU_DiagClient hDiag, NvMU_DiagInfo **pDiagInfo);

/**
 * \brief Close diagnostic client instance
 *
 * \param[in] phDiag pointer to diagnostic handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_DiagClientClose(NvMU_DiagClient *phDiag);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NVMU_DIAG_H__ */

