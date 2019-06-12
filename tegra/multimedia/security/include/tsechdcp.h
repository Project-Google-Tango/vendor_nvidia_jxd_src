/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */


#ifndef TSECHDCP_H
#define TSECHDCP_H

#define ALIGNMENT_256                   256

#define SCRATCH_SIZE                    2304
#define SCRATCH_SIZE_ALIGNED            2304

#define DCP_KPUB_SIZE                   388
#define DCP_KPUB_SIZE_ALIGNED           512

#define SRM_SIZE                        5120
#define SRM_SIZE_ALIGNED                5120

#define CERT_SIZE                       522
#define CERT_SIZE_ALIGNED               768

#define MTHD_RPLY_BUF_SIZE              256

// HDCP 1.x can have as many as 128 total HDCP devices including the repeater
// meaning the bksv list can have maximum 127 entries.
// HDCP 2.x can have 40 total HDCP devices.
// The size needed for rcvr_id/bksv = 127*5 = 635
#define MTHD_RCVR_ID_LIST_SIZE          635

#define CONTENT_BUF_SIZE                2048

#define MTHD_FLAGS_SB                   0x00000001
#define MTHD_FLAGS_DCP_KPUB             0x00000002
#define MTHD_FLAGS_SRM                  0x00000004
#define MTHD_FLAGS_CERT                 0x00000008
#define MTHD_FLAGS_ID_LIST              0x00000010
#define MTHD_FLAGS_INPUT_BUFFER         0x00000020
#define MTHD_FLAGS_OUTPUT_BUFFER        0x00000040

#define DBG_CORRUPT_SB                  0x00000001
#define DBG_CORRUPT_DCP_KPUB            0x00000002
#define DBG_CORRUPT_SRM                 0x00000004
#define DBG_CORRUPT_CERT                0x00000008
#define DBG_PRINT_SB                    0x00010000
#define DBG_PRINT_DCP_KPUB              0x00020000
#define DBG_PRINT_SRM                   0x00040000
#define DBG_PRINT_CERT                  0x00080000
#define DBG_PRINT_MTHBUF                0x00100000

// Types
typedef unsigned char BYTE;
typedef unsigned char byte;
typedef signed   int  NvS32;
typedef unsigned char BOOL;
typedef unsigned char bool;

#define FALSE  0
#define TRUE   1

/*!
 * Tunable parameters
 *
 * HDCP_MAX_SESSIONS    : This is the maximum number of sessions that will be
 *                        supported by HDCP application. Modifying this number
 *                        will affect the scratch buffer size.
 * HDCP_POR_NO_RECEIVERS: This is the total number of active receivers supported
 *                        by HDCP app. Increasing this number will directly
 *                        affect the DMEM usage. Should be always less than
 *                        HDCP_MAX_SESSIONS
 * HDCP_TEST_RCVR_COUNT : Number of test receivers currently known to the
 *                        HDCP app. Modifying this number need corresponding
 *                        change in hdcptestdata source file.
 * HDCP_ENCRYPT_BUFFER_DEPTH: Depth of the queue used for loading input contents
 *                            from FB for encryption. Increasing this count will
 *                            reduce the DMA transactional delay. But it also
 *                            increases heap usage.
 * HDCP_ENCRYPT_BUFFER_SIZE : Not a tunable one. But a derivation from the previous
 *                            define. Generally doubles the BUFFER_DEPTH to be able
 *                            to store both the input from FB and output to FB.
 *
 */
#define HDCP_MAX_SESSIONS              3
#define HDCP_POR_NO_RECEIVERS          2
#define HDCP_TEST_RCVR_COUNT           2
#define HDCP_ENCRYPT_BUFFER_DEPTH      4    //4*256 Bytes can be loaded from/to FB
                                            //at once (always > 0).
#define HDCP_ENCRYPT_BUFFER_SIZE       (HDCP_MAX_DMA_SIZE * HDCP_ENCRYPT_BUFFER_DEPTH) * 2

/*!
 * Constants from falcon arch
 *
 * Falcon arch has some restrictions on DMA capabilities and these defines
 * dictate those restrictions to the code writers.
 */
#define HDCP_MAX_DMA_SIZE              256
#define HDCP_MIN_DMA_SIZE              4
#define HDCP_MIN_DMA_ENC_SIZE          2     //2^(2) - IAS sec 1.6.3
#define HDCP_MAX_DMA_ENC_SIZE          8     //2^(8) - IAS sec 1.6.3
#define HDCP_AES_ENC_BLOCK_SIZE        16    //bytes

/*!
 * Key table indexes
 *
 * These defines are used while selecting SCP key table indexes
 *
 * _SESSION   : Key index used for encryption session details.
 *              Need not be same across chips
 * _EPAIR     : Key index used for encrypting/decrypting pairing
 *              info. Should be same across all chips
 */
#define HDCP_SCP_KEY_INDEX_SESSION     60
#define HDCP_SCP_KEY_INDEX_EPAIR       60

/*!
 * Scratch Buffer layout
 *
 * Scratch buffer is allocated by client in FB and used by HDCP app in TSEC
 * to store the intermediate session details. Along with session details,
 * HDCP app also stores other details such as keys and header describing
 * the session store. All those individual items need to be properly laid out
 * in Scratch buffer with 256 alignment restriction. Modifying position or size
 * of one item affects all the other items in the layout..
 *
 * KEYS_OFFSET     : Offset from where keys are stored.
 * KEYS_BLOCK_SIZE : Size of the keys offset
 * HEADER_OFFSET   : Offset at which scratch buffer header is stored
 * HEADER_SIZE     : Size of the header. Pls look at hdcptypes for header
 *                   data structure
 * SESSION_OFFSET  : Offset from where all the session store starts
 * SESSION_SIZE    : Size of each session. This should be always >= to
 *                   size of hdcp_session struct. While changing this,
 *                   please do change the class header as well.
 */
#define HDCP_SB_KEYS_OFFSET            0
#define HDCP_SB_KEYS_BLOCK_SIZE        256
#define HDCP_SB_HEADER_OFFSET          (HDCP_SB_KEYS_OFFSET + HDCP_SB_KEYS_BLOCK_SIZE)
#define HDCP_SB_HEADER_SIZE            512
#define HDCP_SB_SESSION_OFFSET         (HDCP_SB_HEADER_OFFSET + HDCP_SB_HEADER_SIZE)
#define HDCP_SB_SESSION_SIZE           512

/*!
 * Constants for open standard implementations
 *
 * Constants used by some open-standard implementations.
 * These values rarely changes.
 */
#define SHA256_DIGEST_SIZE             ( 256 / 8)
#define SHA256_BLOCK_SIZE              ( 512 / 8)
#define SHA256_MSG_BLOCK               (sizeof(NvU32)*16)
#define SHA256_HASH_BLOCK              (sizeof(NvU32)*8)
#define HMAC_BLOCK                     64
#define RSAOAEP_BUFFERSIZE             128
#define BIGINT_MAX_DW                  96

/*!
 * Constants defined based on HDCP spec
 *
 * These defines are mainly for SIZEs of various entities
 * involved in HDCP
 */
#define HDCP_SIZE_SHA1_HASH_8          20
#define HDCP_SIZE_SHA1_HASH_32         (HDCP_SIZE_SHA1_HASH_8/4)
#define HDCP_SIZE_SHA2_HASH_8          32
#define HDCP_SIZE_SHA2_HASH_64         (HDCP_SIZE_SHA2_HASH_8/8)
#define HDCP_SIZE_KS_8                 (128/8)
#define HDCP_SIZE_KS_64                (HDCP_SIZE_KS_8/8)
#define HDCP_SIZE_STREAM_CTR_8         (32/8)
#define HDCP_SIZE_STREAM_CTR_32        (HDCP_SIZE_STREAM_CTR_8/4)
#define HDCP_SIZE_INPUT_CTR_8          (64/8)
#define HDCP_SIZE_INPUT_CTR_64         (HDCP_SIZE_INPUT_CTR_8/8)
#define HDCP_SIZE_KM_8                 (128/8)
#define HDCP_SIZE_KM_64                (HDCP_SIZE_KM_8/8)
#define HDCP_SIZE_DKEY_8               (128/8)
#define HDCP_SIZE_DKEY_64              (HDCP_SIZE_DKEY_8/8)
#define HDCP_SIZE_LC_8                 (128/8)
#define HDCP_SIZE_LC_64                (HDCP_SIZE_LC_8/8)
#define HDCP_SIZE_RX_MODULUS_ROOT_8    (3072/8)
#define HDCP_SIZE_RX_MODULUS_8         128
#define HDCP_SIZE_RX_EXPONENT_8        3
#define HDCP_SEQ_NUM_M_MAX             (0xFFFFFF)
#define HDCP_SEQ_NUM_V_MAX             (0xFFFFFF)
#define HDCP_SIZE_KD_8                 (HDCP_SIZE_DKEY_8 * 2)
#define HDCP_SIZE_KD_64                (HDCP_SIZE_DKEY_64 * 2)
#define HDCP_SIZE_STREAMID_MPRIME_8    (HCLASS(SIZE_CONTENT_ID_8)    +  \
                                       HCLASS(SIZE_CONTENT_TYPE_8)   +  \
                                       HDCP_SIZE_STREAM_CTR_8)
#define HDCP_SIZE_MPRIME_DATA_8        ((HDCP_SIZE_STREAMID_MPRIME_8 *  \
                                       HCLASS(MAX_STREAMS_PER_RCVR)) +  \
                                       HCLASS(SIZE_SEQ_NUM_M_8))

/*!
 * Constants used in coding
 */
#define HDCP_SIZE_SESS_SIGN_8          HDCP_SIZE_SHA2_HASH_8
#define HDCP_SIZE_SESS_SIGN_64         HDCP_SIZE_SHA2_HASH_64
#define HDCP_TOTAL_METHODS             APPMETHODARRAYSIZE + CMNMETHODARRAYSIZE

/*!
 * IMEM and DMEM size in Bytes
 */
#if defined(HDCP_BUILD_MSVLD) && (HDCP_BUILD_MSVLD == 1)

#define HDCP_TARGET_MSVLD
#define HDCP_IMEM_SIZE                (8 * 1024)
#define HDCP_DMEM_SIZE                (4 * 1024)

#else

#define HDCP_TARGET_TSEC
#define HDCP_IMEM_SIZE                (32 * 1024)
#define HDCP_DMEM_SIZE                (16 * 1024)

#endif

#define SECOP_CLASS_DEF(x)          NV95A1_ ## x
#define HCLASS(x)    SECOP_CLASS_DEF(HDCP_##x)

// Defines
#define HDCP_MAX_RECEIVERS             32            // Total number of receivers allowed
#define HDCP_MTHD_RESPONSE_SIZE        256           // Size of response surface
#define HDCP_CONTENT_BUFFER_SIZE       20480         // 2KB of buffer provided per
                                                     // known receiver
#define HDCP_EPAIRS_OUTPUT_FILE        "epairs.txt"
#define SECOP_HDCP_STANDALONE_APP      5
#define SECOP_HDCP_TRANSMITTER_APP     6

typedef struct tegra_nvhdcp_packet HdcpPacket;

typedef struct NvTsecWaitSyncPtState
{
    NvU32 SyncPtID;
    NvU32 Count;
} TsecWaitSyncPtState;

typedef struct
{
    NvRmDeviceHandle        hRm;
    NvRmChannelHandle       hChannel;
    NvRmStream              *pStream;
    NvData32                *pb;
    NvU32                   TsecSyncPtID;
    NvU32                   BaseSyncPointID;
    NvBool                  ClientChannel;
    NvU32                   MutexID;
    TsecWaitSyncPtState     WaitSyncPtState;
    NvU32                   TsecPowerClientID;
    NvSchedClient           sched;

    NvRmMemHandle           hScratch;
    void                    *vaddrScratch;
    NvU32                   paddrScratch;
    NvRmMemHandle           hDcpKpub;
    void                    *vaddrDcpKpub;
    NvU32                   paddrDcpKpub;
    NvRmMemHandle           hSrm;
    void                    *vaddrSrm;
    NvU32                   paddrSrm;

    NvRmMemHandle           hCert;
    void                    *vaddrCert;
    NvU32                   paddrCert;
    NvRmMemHandle           hMthdRply;
    void                    *vaddrMthdRply;
    NvU32                   paddrMthdRply;
    NvRmMemHandle           hRcvrIDList;
    void                    *vaddrRcvrIDList;
    NvU32                   paddrRcvrIDList;
    NvRmMemHandle           hInputBuf;
    void                    *vaddrInputBuf;
    NvU32                   paddrInputBuf;
    NvRmMemHandle           hOutputBuf;
    void                    *vaddrOutputBuf;
    NvU32                   paddrOutputBuf;

}TsecHdcpAppCtx;

typedef enum HDCP_RCV_STATE
{
    STATE_INVALID,
    STATE_CURRENT,
    STATE_ACTIVE
}HDCP_RCV_STATE;

typedef enum HDCP_RCV_STAGE
{
    STAGE_NONE,
    STAGE_AKE_INIT,
    STAGE_VERIFY_CERT,
    STAGE_GENERATE_EKM,
    STAGE_VERIFY_HPRIME,
    STAGE_GENERATE_LC_INIT,
    STAGE_GET_RTT_CHALLENGE,
    STAGE_VERIFY_LPRIME,
    STAGE_GENERATE_SKE_INIT,
    STAGE_ENCRYPT_PAIRING_INFO,
    STAGE_DECRYPT_PAIRING_INFO,
    STAGE_VERIFY_VPRIME,
}HDCP_RCV_STAGE;

typedef struct _hdcp_receiver
{
    BOOL  bIsTestReceiver;
    NvU32 rcvId;                  // Index for response surf
    NvU32 maskIndex;              // Index into activeRecvMask
    NvU32 sessionID;
    byte  rtx[HCLASS(SIZE_RTX_8)];
    BYTE  eKm[HCLASS(SIZE_E_KM_8)];
    BYTE  eKs[HCLASS(SIZE_E_KS_8)];
    BYTE  rn[HCLASS(SIZE_RN_8)];
    BYTE  ePair[HCLASS(SIZE_EPAIR_8)];
    BYTE  riv[HCLASS(SIZE_RIV_8)];
    BYTE  L128l[HCLASS(SIZE_LPRIME_8)/2];
    HDCP_RCV_STATE      state;
    HDCP_RCV_STAGE      stage;
    BYTE  seqNumM[HCLASS(SIZE_SEQ_NUM_M_8)];
    NvU32 streamCtr[HCLASS(MAX_STREAMS_PER_RCVR)];
    struct hdcp_receiver_owned
    {
        BYTE  rrx[HCLASS(SIZE_RRX_8)];
        BYTE  eKhKm[HCLASS(SIZE_EKH_KM_8)];
        BYTE  hPrime[HCLASS(SIZE_HPRIME_8)];
        BYTE  lPrime[HCLASS(SIZE_LPRIME_8)];
        NvU32 hdcpVer;
        BOOL  bIsRepeater;
        BOOL  bRecvPreComputeSupport;
        BYTE  contentID[HCLASS(MAX_STREAMS_PER_RCVR)]
                   [HCLASS(SIZE_CONTENT_ID_8)];
        BYTE  strType[HCLASS(MAX_STREAMS_PER_RCVR)]
                   [HCLASS(SIZE_CONTENT_TYPE_8)];

    }rcvGenState;
}hdcp_receiver;

typedef struct _hdcp_verify_vpr
{
   NvU32  flags;                               // <<in
   NvU32  chipId;                              // <<in
   NvU32  retCode;                             // >>out
   NvU32  signature;                           // >>out
} hdcp_verify_vpr;

typedef enum _NvTsecHdcpStatus
{
    NvTsecHdcpStatus_Success = 0,
    NvTsecHdcpStatus_NotImplemented = -1,
    NvTsecHdcpStatus_GenericFailure = -2,
    NvTsecHdcpStatus_InsufficientMemory = -3,
    NvTsecHdcpStatus_InvalidParam = -4,
} NvTsecHdcpStatus;

// Hdcp session status
typedef enum _hdcp_session_status
{
    SESSION_FREE,
    SESSION_IN_USE,
    SESSION_ACTIVE
} HDCP_SESSION_STATUS_EN;

// Session encryption state
typedef struct _hdcp_enc_state
{
    //TODO: Account for multiple streams for a program
    NvU64         inputCtr[HCLASS(MAX_STREAMS_PER_RCVR)];     // Input ctr
    NvU32         streamCtr[HCLASS(MAX_STREAMS_PER_RCVR)];    // Stream ctr
    NvU64         ks[HDCP_SIZE_KS_64];                        // Ks
    NvU64         riv;                                        // riv
    NvU32         noOfStreams;                                // Number of streams in
                                                              // this receiver
} hdcp_enc_state;

// Session state
typedef struct _hdcp_sess_state
{
    NvU64         dkeyCtr;                                    // Internal ctr
    NvU64         kd[HDCP_SIZE_KD_64];                        // Kd
    NvU64         rtx;                                        // Rtx
    NvU64         rrx;                                        // Rrs
    NvU64         rn;                                         // Rn
    NvU64         km[HDCP_SIZE_KM_64];                        // Km
}hdcp_sess_state;

// Hdcp session
typedef struct _hdcp_session
{
    // Signature should be the first element of this struct
    NvU64                  signature[HDCP_SIZE_SESS_SIGN_64]; // session signature
    NvU32                  id;
    NvU32                  hdcpVersion;                       // Rcvr's hdcp ver
    NvU32                  seqNumM;                           // Seq number for mprime
    HDCP_SESSION_STATUS_EN status;                            // session status
    HDCP_RCV_STAGE         stage;                             // State-machine
    hdcp_sess_state        sessState;                         // session state
    hdcp_enc_state         encState;                          // encryption state
    bool                   bRepeater;                         // Is Repeater?
    BYTE                   rcvId[HCLASS(SIZE_RECV_ID_8)];     // Recv ID
    BYTE                   modulus[HDCP_SIZE_RX_MODULUS_8];   // Modulus in pub cert
    BYTE                   exponent[HDCP_SIZE_RX_EXPONENT_8]; // Exponent in pub cert
    bool                   bIsUseTestKeys;                    // Use test keys - DEBUG MODE
    BYTE                   testKeysIndex;                     // Index in testkeys ds
    bool                   bIsRevocationCheckDone;            // Revocation check done?
    BYTE                   bIsPreComputeSupported;            // Rcvr Pre-compute support?
    NvU64                  L[HCLASS(SIZE_LPRIME_64)];         // Saves L..
} hdcp_session;

// Header for scratch buffer
typedef struct _hdcp_sb_header
{
    NvU32        version;                                   // HDCP app version
    NvU32        noOfSessionsInUse;                         // No used sessions
    NvU32        headerFlags;                               // Flags
    NvU32        sessIdSeqCnt;                              // Will be used for sessId
    NvU32        streamCtr;                                 // Global stream counter
    BYTE         sessStatusMask[(HDCP_MAX_SESSIONS/8)+1];   // Sess status masks
}hdcp_sb_header;

#define HDCP_HEADER_FLAGS_INIT                       0:0
#define HDCP_HEADER_FLAGS_INIT_DONE                  0x00000001
#define HDCP_HEADER_FLAGS_INIT_NONE                  0x00000000

// Transfer contents from FB->DMEM or DMEM->FB
typedef struct _hdcp_mem_xfer_desc
{
    NvU32        DMB;                                       // dmbase
    NvU32        fbOff;                                     // FB offset from DMB aligned to size
    NvU32        dmemOff;                                   // DMEM offset aligned to size
    NvU32        size;                                      // Size
    bool         bIsSync;                                   // Dmwait?
    bool         bIsSrcDmem;                                // Controls the flow of data
}hdcp_mem_xfer_desc;

// Active sessions
typedef struct _hdcp_session_active_rec
{
    NvU32                    sessionID;                     // Session ID
    HDCP_SESSION_STATUS_EN   actSessStatus;                 // Active session status
    bool                     bIsDemoSession;                // Demo session
} hdcp_session_active_rec;

// Encryption states of those active sessions
typedef struct _hdcp_session_active_enc
{
    hdcp_session_active_rec  rec;
    hdcp_enc_state           encState;                      // Encryption state
}__attribute__ ((aligned(16))) hdcp_session_active_enc;

// Overlay properties
typedef struct _hdcp_ovly_prop{
    NvU32 startOff;
    NvU32 stopOff;
    BYTE  *pSign;
    BYTE  name[10];
} hdcp_ovly_prop;

// Mthd -> Ovly map
typedef struct _hdcp_mthd_ovlys
{
    NvU32 ovlySelectionBitMask;
} hdcp_mthd_ovlys;

// Global data
typedef struct _hdcp_global_data
{
    BYTE                    randNo[16]
                            __attribute__ ((aligned(16)));  // Random number - Should be 1st member
                                                            // TODO: Do we need this?
    hdcp_session_active_enc actSess[HDCP_POR_NO_RECEIVERS]; // Stores active session in encrypted form..
    hdcp_session_active_rec actRec[HDCP_POR_NO_RECEIVERS];  // Session IDs in active Sessions. The sessionID
                                                            // at particular Index will match session details
                                                            // stored in actSessions
    NvU32                   sbOffset;                       // Scratch buffer offset
    hdcp_sb_header          header;                         // Header for SB
    NvU32                   supportedHdcpversionsMask;      // Versions supported
    NvU32                   chipId;                         // Unique generated chipId. Used in upstream

    NvU32                   noOfReceivers;
    NvU32                   activeReceiversMask;
    hdcp_receiver           **pReceivers;
    hdcp_read_caps_param    caps;
    NvU32                   falconAppSize;
    NvU32                   argc;
    char                    **argv;
    NvU32                   dmaAddr;
    NvU32                   sboff;

}hdcp_global_data;

NvU16	dcp_pub_key[192] __attribute__ ((aligned (256)))= {

0xB0E9, 0xAA45, 0xF129, 0xBA0A, 0x1CBE, 0x1757, 0x28EB, 0x2B4E,
0x8FD0, 0xC06A, 0xAD79, 0x980F, 0x8D43, 0x8D47, 0x04B8, 0x2BF4,
0x1521, 0x5619, 0x0140, 0x013B, 0xD091, 0x9062, 0x9E89, 0xC227,
0x8ECF, 0xB6DB, 0xCE3F, 0x7210, 0x5093, 0x8C23, 0x2983, 0x7B80,
0x64A7, 0x59E8, 0x6167, 0x4CBC, 0xD858, 0xB8F1, 0xD4F8, 0x2C37,
0x9816, 0x260E, 0x4EF9, 0x4EEE, 0x24DE, 0xCCD1, 0x4B4B, 0xC506,
0x7AFB, 0x4965, 0xE6C0, 0x0083, 0x481E, 0x8E42, 0x2A53, 0xA0F5,
0x3729, 0x2B5A, 0xF973, 0xC59A, 0xA1B5, 0xB574, 0x7C06, 0xDC7B,
0x7CDC, 0x6C6E, 0x826B, 0x4988, 0xD41B, 0x25E0, 0xEED1, 0x79BD,
0x3985, 0xFA4F, 0x25EC, 0x7019, 0x23C1, 0xB9A6, 0xD97E, 0x3EDA,
0x48A9, 0x58E3, 0x1814, 0x1E9F, 0x307F, 0x4CA8, 0xAE53, 0x2266,
0x2BBE, 0x24CB, 0x4766, 0xFC83, 0xCF5C, 0x2D1E, 0x3AAB, 0xAB06,
0xBE05, 0xAA1A, 0x9B2D, 0xB7A6, 0x54F3, 0x632B, 0x97BF, 0x93BE,
0xC1AF, 0x2139, 0x490C, 0xE931, 0x90CC, 0xC2BB, 0x3C02, 0xC4E2,
0xBDBD, 0x2F84, 0x639B, 0xD2DD, 0x783E, 0x90C6, 0xC5AC, 0x1677,
0x2E69, 0x6C77, 0xFDED, 0x8A4D, 0x6A8C, 0xA3A9, 0x256C, 0x21FD,
0xB294, 0x0C84, 0xAA07, 0x2926, 0x46F7, 0x9B3A, 0x1987, 0xE09F,
0xEB30, 0xA8F5, 0x64EB, 0x07F1, 0xE9DB, 0xF9AF, 0x2C8B, 0x697E,
0x2E67, 0x393F, 0xF3A6, 0xE5CD, 0xDA24, 0x9BA2, 0x7872, 0xF0A2,
0x27C3, 0xE025, 0xB4A1, 0x046A, 0x5980, 0x27B5, 0xDAB4, 0xB453,
0x973B, 0x2899, 0xACF4, 0x9627, 0x0F7F, 0x300C, 0x4AAF, 0xCB9E,
0xD871, 0x2824, 0x3EBC, 0x3515, 0xBE13, 0xEBAF, 0x4301, 0xBD61,
0x2454, 0x349F, 0x733E, 0xB510, 0x9FC9, 0xFC80, 0xE84D, 0xE332,
0x968F, 0x8810, 0x2325, 0xF3D3, 0x3E6E, 0x6DBB, 0xDC29, 0x66EB

};

//Helper macros
#define CHECK_NULL(var, msg)  if(!var){NvTestPrintf(msg); rc=NvTsecHdcpStatus_Success;           \
                                goto label_return;}
#define CHECK_FALSE(var, msg) if(var == False){printf(msg); rc=NvTsecHdcpStatus_Success;   \
                                goto label_return;}
#define CHECK_RC(r)           if((rc = r)!= NvTsecHdcpStatus_Success) goto label_return;

#define ALIGN_UP(val, alig)       (((val) + ((alig) - 1)) & ~((alig)-1))
#define ALIGN_TO_256(x)           ALIGN_UP(x, 256)
#define ALIGN_TO_16(x)            ALIGN_UP(x, 16)

NvTsecHdcpStatus NvTsecHdcpMemAlloc(NvRmDeviceHandle hRm, NvRmMemHandle* phMem, NvU32 Size, NvU32 Alignment, NvU32 *paddr, void **vaddr);
NvTsecHdcpStatus NvTsecHdcpVprMemAlloc(NvRmDeviceHandle hRm, NvRmMemHandle* phMem, NvU32 Size, NvU32 Alignment, NvU32 *paddr, void **vaddr);
void             NvTsecHdcpMemFree(NvRmMemHandle hMem);
void             NvTsecHdcpCleanUp(TsecHdcpAppCtx *pAppCtx);

NvTsecHdcpStatus NvTsecHdcpInit(TsecHdcpAppCtx **ppAppCtx);
NvTsecHdcpStatus NvTsecHdcpOpen(TsecHdcpAppCtx *pAppCtx);
void             NvTsecHdcpClose(TsecHdcpAppCtx *pAppCtx);
NvTsecHdcpStatus NvTsecHdcpFlush(TsecHdcpAppCtx *pAppCtx, NvRmFence *pFence);
void             NvTsecHdcpHostClassWaitForSyncPnt(NvData32 *nvCurrent, NvU32 SyncPointId);
int              NvTsecHdcpSendMthd(TsecHdcpAppCtx *pAppCtx, NvU32 mid, NvRmMemHandle *hMem, NvU32 flags);
void             NvTsecHdcpWriteMethodU(NvData32 **pb, NvU32 mid, NvU32 data);
void             NvTsecHdcpWriteMethodReloc(NvRmStream *pStream, NvData32 **pb, NvU32 mid, NvRmMemHandle data);
NvTsecHdcpStatus NvTsecHdcpTsecMode(TsecHdcpAppCtx *pAppCtx, NvU32 mode);

void             NvTsecHdcpSetupSB(TsecHdcpAppCtx *pAppCtx);
void             NvTsecHdcpPrintSB(TsecHdcpAppCtx *pAppCtx);
void             NvTsecHdcpPrintDcpBuf(TsecHdcpAppCtx *pAppCtx);
void             NvTsecHdcpPrintMthdBuf(TsecHdcpAppCtx *pAppCtx);
void             NvTsecHdcpPrintMthdBufByte(TsecHdcpAppCtx *pAppCtx);
void             NvTsecHdcpPrintCertBuf(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv);
void             NvTsecHdcpPrintSrmBuf(TsecHdcpAppCtx *pAppCtx);

void             NvTsecHdcpDumpReg(TsecHdcpAppCtx *pAppCtx, NvU32 addr, NvU32 size);
void             NvTsecHdcpWriteReg(TsecHdcpAppCtx *pAppCtx, NvU32 addr, NvU32 size, NvU32 write_value);

NvTsecHdcpStatus NvTsecHdcpCopyDcpKey(TsecHdcpAppCtx *pAppCtx);
NvTsecHdcpStatus NvTsecHdcpCopyCert(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv);
NvTsecHdcpStatus NvTsecHdcpCopySrm(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv);
NvTsecHdcpStatus NvTsecHdcpReadSrm(TsecHdcpAppCtx *pAppCtx, NvU32 SrmVersion, NvU32 *SrmSize);
NvTsecHdcpStatus NvTsecHdcpCopyIDList(TsecHdcpAppCtx *pAppCtx);
NvTsecHdcpStatus NvTsecHdcpCopyVprime_2_0(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv, void* dest_ptr);
NvTsecHdcpStatus NvTsecHdcpCopyVprime_2_1(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv, void* dest_ptr);
NvTsecHdcpStatus NvTsecHdcpCopyVprime_1X(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv, void* dest_ptr);
NvTsecHdcpStatus NvTsecHdcpCopySeqNumV(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv, void* dest_ptr);

NvTsecHdcpStatus NvTsecHdcpReadCaps(TsecHdcpAppCtx *pAppCtx);
NvTsecHdcpStatus NvTsecHdcpInitParam(TsecHdcpAppCtx *pAppCtx);
NvTsecHdcpStatus NvTsecHdcpVerifyVprime(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv, HdcpPacket *pkt);
NvTsecHdcpStatus NvTsecHdcpExchangeInfo(TsecHdcpAppCtx *pAppCtx, hdcp_receiver *pRcv, NvU32 methodFlag, NvU8 *version, NvU16 *caps);
#endif
