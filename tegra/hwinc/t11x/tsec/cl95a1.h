// WARNING!!! THIS HEADER INCLUDES SOFTWARE METHODS!!!
// ********** DO NOT USE IN HW TREE.  ********** 
/* 
* _NVRM_COPYRIGHT_BEGIN_
*
* Copyright 1993-2004 by NVIDIA Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to NVIDIA
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of NVIDIA Corporation is prohibited.
*
* _NVRM_COPYRIGHT_END_
*/

#ifndef _cl95a1_h_
#define _cl95a1_h_

#ifdef __cplusplus
extern "C" {
#endif

#define NV95A1_TSEC                                                                (0x000095A1)

typedef volatile struct {
    NvU32 Reserved00[0x40];
    NvU32 Nop;                                                                  // 0x00000100 - 0x00000103
    NvU32 Reserved01[0xF];
    NvU32 PmTrigger;                                                            // 0x00000140 - 0x00000143
    NvU32 Reserved02[0x2F];
    NvU32 SetApplicationID;                                                     // 0x00000200 - 0x00000203
    NvU32 SetWatchdogTimer;                                                     // 0x00000204 - 0x00000207
    NvU32 Reserved03[0xE];
    NvU32 SemaphoreA;                                                           // 0x00000240 - 0x00000243
    NvU32 SemaphoreB;                                                           // 0x00000244 - 0x00000247
    NvU32 SemaphoreC;                                                           // 0x00000248 - 0x0000024B
    NvU32 Reserved04[0x2D];
    NvU32 Execute;                                                              // 0x00000300 - 0x00000303
    NvU32 SemaphoreD;                                                           // 0x00000304 - 0x00000307
    NvU32 Reserved05[0x7E];
    NvU32 HdcpInit;                                                             // 0x00000500 - 0x00000503
    NvU32 HdcpCreateSession;                                                    // 0x00000504 - 0x00000507
    NvU32 HdcpVerifyCertRx;                                                     // 0x00000508 - 0x0000050B
    NvU32 HdcpGenerateEKm;                                                      // 0x0000050C - 0x0000050F
    NvU32 HdcpRevocationCheck;                                                  // 0x00000510 - 0x00000513
    NvU32 HdcpVerifyHprime;                                                     // 0x00000514 - 0x00000517
    NvU32 HdcpEncryptPairingInfo;                                               // 0x00000518 - 0x0000051B
    NvU32 HdcpDecryptPairingInfo;                                               // 0x0000051C - 0x0000051F
    NvU32 HdcpUpdateSession;                                                    // 0x00000520 - 0x00000523
    NvU32 HdcpGenerateLcInit;                                                   // 0x00000524 - 0x00000527
    NvU32 HdcpVerifyLprime;                                                     // 0x00000528 - 0x0000052B
    NvU32 HdcpGenerateSkeInit;                                                  // 0x0000052C - 0x0000052F
    NvU32 HdcpVerifyVprime;                                                     // 0x00000530 - 0x00000533
    NvU32 HdcpEncryptionRunCtrl;                                                // 0x00000534 - 0x00000537
    NvU32 HdcpSessionCtrl;                                                      // 0x00000538 - 0x0000053B
    NvU32 HdcpComputeSprime;                                                    // 0x0000053C - 0x0000053F
    NvU32 Reserved06[0x30];
    NvU32 NvsiSwapAreaOffset;                                                   // 0x00000600 - 0x00000603
    NvU32 NvsiSwapAreaSize;                                                     // 0x00000604 - 0x00000607
    NvU32 NvsiShaderCodeOffset;                                                 // 0x00000608 - 0x0000060B
    NvU32 NvsiShaderCodeSize;                                                   // 0x0000060C - 0x0000060F
    NvU32 NvsiLoader2CodeSize;                                                  // 0x00000610 - 0x00000613
    NvU32 NvsiShaderDataOffset;                                                 // 0x00000614 - 0x00000617
    NvU32 NvsiShaderDataSize;                                                   // 0x00000618 - 0x0000061B
    NvU32 NvsiShaderVolDataSize;                                                // 0x0000061C - 0x0000061F
    NvU32 NvsiSharedSegOffset;                                                  // 0x00000620 - 0x00000623
    NvU32 NvsiSharedSegSize;                                                    // 0x00000624 - 0x00000627
    NvU32 NvsiMiscDataOffset;                                                   // 0x00000628 - 0x0000062B
    NvU32 NvsiMiscDataSize;                                                     // 0x0000062C - 0x0000062F
    NvU32 Reserved07[0x34];
    NvU32 HdcpValidateSrm;                                                      // 0x00000700 - 0x00000703
    NvU32 HdcpValidateStream;                                                   // 0x00000704 - 0x00000707
    NvU32 HdcpTestSecureStatus;                                                 // 0x00000708 - 0x0000070B
    NvU32 HdcpSetDcpKpub;                                                       // 0x0000070C - 0x0000070F
    NvU32 HdcpSetRxKpub;                                                        // 0x00000710 - 0x00000713
    NvU32 HdcpSetCertRx;                                                        // 0x00000714 - 0x00000717
    NvU32 HdcpSetScratchBuffer;                                                 // 0x00000718 - 0x0000071B
    NvU32 HdcpSetSrm;                                                           // 0x0000071C - 0x0000071F
    NvU32 HdcpSetReceiverIdList;                                                // 0x00000720 - 0x00000723
    NvU32 HdcpSetSprime;                                                        // 0x00000724 - 0x00000727
    NvU32 HdcpSetEncInputBuffer;                                                // 0x00000728 - 0x0000072B
    NvU32 HdcpSetEncOutputBuffer;                                               // 0x0000072C - 0x0000072F
    NvU32 HdcpGetRttChallenge;                                                  // 0x00000730 - 0x00000733
    NvU32 HdcpStreamManage;                                                     // 0x00000734 - 0x00000737
    NvU32 HdcpReadCaps;                                                         // 0x00000738 - 0x0000073B
    NvU32 HdcpEncrypt;                                                          // 0x0000073C - 0x0000073F
    NvU32 Reserved08[0x130];
    NvU32 SetContentInitialVector[4];                                           // 0x00000C00 - 0x00000C0F
    NvU32 SetCtlCount;                                                          // 0x00000C10 - 0x00000C13
    NvU32 SetMdecH2MKey;                                                        // 0x00000C14 - 0x00000C17
    NvU32 SetMdecM2HKey;                                                        // 0x00000C18 - 0x00000C1B
    NvU32 SetMdecFrameKey;                                                      // 0x00000C1C - 0x00000C1F
    NvU32 SetUpperSrc;                                                          // 0x00000C20 - 0x00000C23
    NvU32 SetLowerSrc;                                                          // 0x00000C24 - 0x00000C27
    NvU32 SetUpperDst;                                                          // 0x00000C28 - 0x00000C2B
    NvU32 SetLowerDst;                                                          // 0x00000C2C - 0x00000C2F
    NvU32 SetUpperCtl;                                                          // 0x00000C30 - 0x00000C33
    NvU32 SetLowerCtl;                                                          // 0x00000C34 - 0x00000C37
    NvU32 SetBlockCount;                                                        // 0x00000C38 - 0x00000C3B
    NvU32 SetStretchMask;                                                       // 0x00000C3C - 0x00000C3F
    NvU32 Reserved09[0x30];
    NvU32 SetUpperFlowCtrlInsecure;                                             // 0x00000D00 - 0x00000D03
    NvU32 SetLowerFlowCtrlInsecure;                                             // 0x00000D04 - 0x00000D07
    NvU32 Reserved10[0x2];
    NvU32 SetUcodeLoaderParams;                                                 // 0x00000D10 - 0x00000D13
    NvU32 Reserved11[0x1];
    NvU32 SetUpperFlowCtrlSecure;                                               // 0x00000D18 - 0x00000D1B
    NvU32 SetLowerFlowCtrlSecure;                                               // 0x00000D1C - 0x00000D1F
    NvU32 Reserved12[0x5];
    NvU32 SetUcodeLoaderOffset;                                                 // 0x00000D34 - 0x00000D37
    NvU32 Reserved13[0x72];
    NvU32 SetSessionKey[4];                                                     // 0x00000F00 - 0x00000F0F
    NvU32 SetContentKey[4];                                                     // 0x00000F10 - 0x00000F1F
    NvU32 Reserved14[0x7D];
    NvU32 PmTriggerEnd;                                                         // 0x00001114 - 0x00001117
    NvU32 Reserved15[0x3BA];
} NV95A1_TSECControlPio;

#define NV95A1_NOP                                                              (0x00000100)
#define NV95A1_NOP_PARAMETER                                                    31:0
#define NV95A1_PM_TRIGGER                                                       (0x00000140)
#define NV95A1_PM_TRIGGER_V                                                     31:0
#define NV95A1_SET_APPLICATION_ID                                               (0x00000200)
#define NV95A1_SET_APPLICATION_ID_ID                                            31:0
#define NV95A1_SET_APPLICATION_ID_ID_HDCP                                       (0x00000001)
#define NV95A1_SET_APPLICATION_ID_ID_NVSI                                       (0x00000002)
#define NV95A1_SET_APPLICATION_ID_ID_CTR64                                      (0x00000005)
#define NV95A1_SET_APPLICATION_ID_ID_STRETCH_CTR64                              (0x00000006)
#define NV95A1_SET_APPLICATION_ID_ID_MDEC_LEGACY                                (0x00000007)
#define NV95A1_SET_APPLICATION_ID_ID_UCODE_LOADER                               (0x00000008)
#define NV95A1_SET_APPLICATION_ID_ID_SCRAMBLER                                  (0x00000009)
#define NV95A1_SET_WATCHDOG_TIMER                                               (0x00000204)
#define NV95A1_SET_WATCHDOG_TIMER_TIMER                                         31:0
#define NV95A1_SEMAPHORE_A                                                      (0x00000240)
#define NV95A1_SEMAPHORE_A_UPPER                                                7:0
#define NV95A1_SEMAPHORE_B                                                      (0x00000244)
#define NV95A1_SEMAPHORE_B_LOWER                                                31:0
#define NV95A1_SEMAPHORE_C                                                      (0x00000248)
#define NV95A1_SEMAPHORE_C_PAYLOAD                                              31:0
#define NV95A1_EXECUTE                                                          (0x00000300)
#define NV95A1_EXECUTE_NOTIFY                                                   0:0
#define NV95A1_EXECUTE_NOTIFY_DISABLE                                           (0x00000000)
#define NV95A1_EXECUTE_NOTIFY_ENABLE                                            (0x00000001)
#define NV95A1_EXECUTE_NOTIFY_ON                                                1:1
#define NV95A1_EXECUTE_NOTIFY_ON_END                                            (0x00000000)
#define NV95A1_EXECUTE_NOTIFY_ON_BEGIN                                          (0x00000001)
#define NV95A1_EXECUTE_AWAKEN                                                   8:8
#define NV95A1_EXECUTE_AWAKEN_DISABLE                                           (0x00000000)
#define NV95A1_EXECUTE_AWAKEN_ENABLE                                            (0x00000001)
#define NV95A1_SEMAPHORE_D                                                      (0x00000304)
#define NV95A1_SEMAPHORE_D_STRUCTURE_SIZE                                       0:0
#define NV95A1_SEMAPHORE_D_STRUCTURE_SIZE_ONE                                   (0x00000000)
#define NV95A1_SEMAPHORE_D_STRUCTURE_SIZE_FOUR                                  (0x00000001)
#define NV95A1_SEMAPHORE_D_AWAKEN_ENABLE                                        8:8
#define NV95A1_SEMAPHORE_D_AWAKEN_ENABLE_FALSE                                  (0x00000000)
#define NV95A1_SEMAPHORE_D_AWAKEN_ENABLE_TRUE                                   (0x00000001)
#define NV95A1_SEMAPHORE_D_OPERATION                                            17:16
#define NV95A1_SEMAPHORE_D_OPERATION_RELEASE                                    (0x00000000)
#define NV95A1_SEMAPHORE_D_OPERATION_RESERVED0                                  (0x00000001)
#define NV95A1_SEMAPHORE_D_OPERATION_RESERVED1                                  (0x00000002)
#define NV95A1_SEMAPHORE_D_OPERATION_TRAP                                       (0x00000003)
#define NV95A1_SEMAPHORE_D_FLUSH_DISABLE                                        21:21
#define NV95A1_SEMAPHORE_D_FLUSH_DISABLE_FALSE                                  (0x00000000)
#define NV95A1_SEMAPHORE_D_FLUSH_DISABLE_TRUE                                   (0x00000001)
#define NV95A1_HDCP_INIT                                                        (0x00000500)
#define NV95A1_HDCP_INIT_PARAM_OFFSET                                           31:0
#define NV95A1_HDCP_CREATE_SESSION                                              (0x00000504)
#define NV95A1_HDCP_CREATE_SESSION_PARAM_OFFSET                                 31:0
#define NV95A1_HDCP_VERIFY_CERT_RX                                              (0x00000508)
#define NV95A1_HDCP_VERIFY_CERT_RX_PARAM_OFFSET                                 31:0
#define NV95A1_HDCP_GENERATE_EKM                                                (0x0000050C)
#define NV95A1_HDCP_GENERATE_EKM_PARAM_OFFSET                                   31:0
#define NV95A1_HDCP_REVOCATION_CHECK                                            (0x00000510)
#define NV95A1_HDCP_REVOCATION_CHECK_PARAM_OFFSET                               31:0
#define NV95A1_HDCP_VERIFY_HPRIME                                               (0x00000514)
#define NV95A1_HDCP_VERIFY_HPRIME_PARAM_OFFSET                                  31:0
#define NV95A1_HDCP_ENCRYPT_PAIRING_INFO                                        (0x00000518)
#define NV95A1_HDCP_ENCRYPT_PAIRING_INFO_PARAM_OFFSET                           31:0
#define NV95A1_HDCP_DECRYPT_PAIRING_INFO                                        (0x0000051C)
#define NV95A1_HDCP_DECRYPT_PAIRING_INFO_PARAM_OFFSET                           31:0
#define NV95A1_HDCP_UPDATE_SESSION                                              (0x00000520)
#define NV95A1_HDCP_UPDATE_SESSION_PARAM_OFFSET                                 31:0
#define NV95A1_HDCP_GENERATE_LC_INIT                                            (0x00000524)
#define NV95A1_HDCP_GENERATE_LC_INIT_PARAM_OFFSET                               31:0
#define NV95A1_HDCP_VERIFY_LPRIME                                               (0x00000528)
#define NV95A1_HDCP_VERIFY_LPRIME_PARAM_OFFSET                                  31:0
#define NV95A1_HDCP_GENERATE_SKE_INIT                                           (0x0000052C)
#define NV95A1_HDCP_GENERATE_SKE_INIT_PARAM_OFFSET                              31:0
#define NV95A1_HDCP_VERIFY_VPRIME                                               (0x00000530)
#define NV95A1_HDCP_VERIFY_VPRIME_PARAM_OFFSET                                  31:0
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL                                         (0x00000534)
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL_PARAM_OFFSET                            31:0
#define NV95A1_HDCP_SESSION_CTRL                                                (0x00000538)
#define NV95A1_HDCP_SESSION_CTRL_PARAM_OFFSET                                   31:0
#define NV95A1_HDCP_COMPUTE_SPRIME                                              (0x0000053C)
#define NV95A1_HDCP_COMPUTE_SPRIME_PARAM_OFFSET                                 31:0
#define NV95A1_NVSI_SWAP_AREA_OFFSET                                            (0x00000600)
#define NV95A1_NVSI_SWAP_AREA_OFFSET_OFFSET                                     31:0
#define NV95A1_NVSI_SWAP_AREA_SIZE                                              (0x00000604)
#define NV95A1_NVSI_SWAP_AREA_SIZE_VALUE                                        31:0
#define NV95A1_NVSI_SHADER_CODE_OFFSET                                          (0x00000608)
#define NV95A1_NVSI_SHADER_CODE_OFFSET_OFFSET                                   31:0
#define NV95A1_NVSI_SHADER_CODE_SIZE                                            (0x0000060C)
#define NV95A1_NVSI_SHADER_CODE_SIZE_VALUE                                      31:0
#define NV95A1_NVSI_LOADER2_CODE_SIZE                                           (0x00000610)
#define NV95A1_NVSI_LOADER2_CODE_SIZE_VALUE                                     31:0
#define NV95A1_NVSI_SHADER_DATA_OFFSET                                          (0x00000614)
#define NV95A1_NVSI_SHADER_DATA_OFFSET_OFFSET                                   31:0
#define NV95A1_NVSI_SHADER_DATA_SIZE                                            (0x00000618)
#define NV95A1_NVSI_SHADER_DATA_SIZE_VALUE                                      31:0
#define NV95A1_NVSI_SHADER_VOL_DATA_SIZE                                        (0x0000061C)
#define NV95A1_NVSI_SHADER_VOL_DATA_SIZE_VALUE                                  31:0
#define NV95A1_NVSI_SHARED_SEG_OFFSET                                           (0x00000620)
#define NV95A1_NVSI_SHARED_SEG_OFFSET_OFFSET                                    31:0
#define NV95A1_NVSI_SHARED_SEG_SIZE                                             (0x00000624)
#define NV95A1_NVSI_SHARED_SEG_SIZE_VALUE                                       31:0
#define NV95A1_NVSI_MISC_DATA_OFFSET                                            (0x00000628)
#define NV95A1_NVSI_MISC_DATA_OFFSET_OFFSET                                     31:0
#define NV95A1_NVSI_MISC_DATA_SIZE                                              (0x0000062C)
#define NV95A1_NVSI_MISC_DATA_SIZE_VALUE                                        31:0
#define NV95A1_HDCP_VALIDATE_SRM                                                (0x00000700)
#define NV95A1_HDCP_VALIDATE_SRM_PARAM_OFFSET                                   31:0
#define NV95A1_HDCP_VALIDATE_STREAM                                             (0x00000704)
#define NV95A1_HDCP_VALIDATE_STREAM_PARAM_OFFSET                                31:0
#define NV95A1_HDCP_TEST_SECURE_STATUS                                          (0x00000708)
#define NV95A1_HDCP_TEST_SECURE_STATUS_PARAM_OFFSET                             31:0
#define NV95A1_HDCP_SET_DCP_KPUB                                                (0x0000070C)
#define NV95A1_HDCP_SET_DCP_KPUB_OFFSET                                         31:0
#define NV95A1_HDCP_SET_RX_KPUB                                                 (0x00000710)
#define NV95A1_HDCP_SET_RX_KPUB_OFFSET                                          31:0
#define NV95A1_HDCP_SET_CERT_RX                                                 (0x00000714)
#define NV95A1_HDCP_SET_CERT_RX_OFFSET                                          31:0
#define NV95A1_HDCP_SET_SCRATCH_BUFFER                                          (0x00000718)
#define NV95A1_HDCP_SET_SCRATCH_BUFFER_OFFSET                                   31:0
#define NV95A1_HDCP_SET_SRM                                                     (0x0000071C)
#define NV95A1_HDCP_SET_SRM_OFFSET                                              31:0
#define NV95A1_HDCP_SET_RECEIVER_ID_LIST                                        (0x00000720)
#define NV95A1_HDCP_SET_RECEIVER_ID_LIST_OFFSET                                 31:0
#define NV95A1_HDCP_SET_SPRIME                                                  (0x00000724)
#define NV95A1_HDCP_SET_SPRIME_OFFSET                                           31:0
#define NV95A1_HDCP_SET_ENC_INPUT_BUFFER                                        (0x00000728)
#define NV95A1_HDCP_SET_ENC_INPUT_BUFFER_OFFSET                                 31:0
#define NV95A1_HDCP_SET_ENC_OUTPUT_BUFFER                                       (0x0000072C)
#define NV95A1_HDCP_SET_ENC_OUTPUT_BUFFER_OFFSET                                31:0
#define NV95A1_HDCP_GET_RTT_CHALLENGE                                           (0x00000730)
#define NV95A1_HDCP_GET_RTT_CHALLENGE_PARAM_OFFSET                              31:0
#define NV95A1_HDCP_STREAM_MANAGE                                               (0x00000734)
#define NV95A1_HDCP_STREAM_MANAGE_PARAM_OFFSET                                  31:0
#define NV95A1_HDCP_READ_CAPS                                                   (0x00000738)
#define NV95A1_HDCP_READ_CAPS_PARAM_OFFSET                                      31:0
#define NV95A1_HDCP_ENCRYPT                                                     (0x0000073C)
#define NV95A1_HDCP_ENCRYPT_PARAM_OFFSET                                        31:0
#define NV95A1_SET_CONTENT_INITIAL_VECTOR(b)                                    (0x00000C00 + (b)*0x00000004)
#define NV95A1_SET_CONTENT_INITIAL_VECTOR_VALUE                                 31:0
#define NV95A1_SET_CTL_COUNT                                                    (0x00000C10)
#define NV95A1_SET_CTL_COUNT_VALUE                                              31:0
#define NV95A1_SET_MDEC_H2_MKEY                                                 (0x00000C14)
#define NV95A1_SET_MDEC_H2_MKEY_HOST_SKEY                                       15:0
#define NV95A1_SET_MDEC_H2_MKEY_HOST_KEY_HASH                                   23:16
#define NV95A1_SET_MDEC_H2_MKEY_DEC_ID                                          31:24
#define NV95A1_SET_MDEC_M2_HKEY                                                 (0x00000C18)
#define NV95A1_SET_MDEC_M2_HKEY_MPEG_SKEY                                       15:0
#define NV95A1_SET_MDEC_M2_HKEY_SELECTOR                                        23:16
#define NV95A1_SET_MDEC_M2_HKEY_MPEG_KEY_HASH                                   31:24
#define NV95A1_SET_MDEC_FRAME_KEY                                               (0x00000C1C)
#define NV95A1_SET_MDEC_FRAME_KEY_VALUE                                         15:0
#define NV95A1_SET_UPPER_SRC                                                    (0x00000C20)
#define NV95A1_SET_UPPER_SRC_OFFSET                                             7:0
#define NV95A1_SET_LOWER_SRC                                                    (0x00000C24)
#define NV95A1_SET_LOWER_SRC_OFFSET                                             31:0
#define NV95A1_SET_UPPER_DST                                                    (0x00000C28)
#define NV95A1_SET_UPPER_DST_OFFSET                                             7:0
#define NV95A1_SET_LOWER_DST                                                    (0x00000C2C)
#define NV95A1_SET_LOWER_DST_OFFSET                                             31:0
#define NV95A1_SET_UPPER_CTL                                                    (0x00000C30)
#define NV95A1_SET_UPPER_CTL_OFFSET                                             7:0
#define NV95A1_SET_LOWER_CTL                                                    (0x00000C34)
#define NV95A1_SET_LOWER_CTL_OFFSET                                             31:0
#define NV95A1_SET_BLOCK_COUNT                                                  (0x00000C38)
#define NV95A1_SET_BLOCK_COUNT_VALUE                                            31:0
#define NV95A1_SET_STRETCH_MASK                                                 (0x00000C3C)
#define NV95A1_SET_STRETCH_MASK_VALUE                                           31:0
#define NV95A1_SET_UPPER_FLOW_CTRL_INSECURE                                     (0x00000D00)
#define NV95A1_SET_UPPER_FLOW_CTRL_INSECURE_OFFSET                              7:0
#define NV95A1_SET_LOWER_FLOW_CTRL_INSECURE                                     (0x00000D04)
#define NV95A1_SET_LOWER_FLOW_CTRL_INSECURE_OFFSET                              31:0
#define NV95A1_SET_UCODE_LOADER_PARAMS                                          (0x00000D10)
#define NV95A1_SET_UCODE_LOADER_PARAMS_BLOCK_COUNT                              7:0
#define NV95A1_SET_UCODE_LOADER_PARAMS_SECURITY_PARAM                           15:8
#define NV95A1_SET_UPPER_FLOW_CTRL_SECURE                                       (0x00000D18)
#define NV95A1_SET_UPPER_FLOW_CTRL_SECURE_OFFSET                                7:0
#define NV95A1_SET_LOWER_FLOW_CTRL_SECURE                                       (0x00000D1C)
#define NV95A1_SET_LOWER_FLOW_CTRL_SECURE_OFFSET                                31:0
#define NV95A1_SET_UCODE_LOADER_OFFSET                                          (0x00000D34)
#define NV95A1_SET_UCODE_LOADER_OFFSET_OFFSET                                   31:0
#define NV95A1_SET_SESSION_KEY(b)                                               (0x00000F00 + (b)*0x00000004)
#define NV95A1_SET_SESSION_KEY_VALUE                                            31:0
#define NV95A1_SET_CONTENT_KEY(b)                                               (0x00000F10 + (b)*0x00000004)
#define NV95A1_SET_CONTENT_KEY_VALUE                                            31:0
#define NV95A1_PM_TRIGGER_END                                                   (0x00001114)
#define NV95A1_PM_TRIGGER_END_V                                                 31:0

#define NV95A1_ERROR_NONE                                                       (0x00000000)
#define NV95A1_OS_ERROR_EXECUTE_INSUFFICIENT_DATA                               (0x00000001)
#define NV95A1_OS_ERROR_SEMAPHORE_INSUFFICIENT_DATA                             (0x00000002)
#define NV95A1_OS_ERROR_INVALID_METHOD                                          (0x00000003)
#define NV95A1_OS_ERROR_INVALID_DMA_PAGE                                        (0x00000004)
#define NV95A1_OS_ERROR_UNHANDLED_INTERRUPT                                     (0x00000005)
#define NV95A1_OS_ERROR_EXCEPTION                                               (0x00000006)
#define NV95A1_OS_ERROR_INVALID_CTXSW_REQUEST                                   (0x00000007)
#define NV95A1_OS_ERROR_APPLICATION                                             (0x00000008)
#define NV95A1_OS_INTERRUPT_EXECUTE_AWAKEN                                      (0x00000100)
#define NV95A1_OS_INTERRUPT_BACKEND_SEMAPHORE_AWAKEN                            (0x00000200)
#define NV95A1_OS_INTERRUPT_HALT_ENGINE                                         (0x00000600)

#ifdef __cplusplus
};     /* extern "C" */
#endif
#endif // _cl95a1_h

