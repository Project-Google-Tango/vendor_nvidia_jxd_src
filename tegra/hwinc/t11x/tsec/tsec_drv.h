#ifndef TSEC_DRV_H
#define TSEC_DRV_H
//
// CLASS NV_95A1_TSEC
// ===================
//
//
// 1   -  INTRODUCTION
// TSEC class supports SEC legacy applications, SECOP applications for
// Blue ray usecase, HDCP, HDCP 2.0 for WFD.
//
// 2   -  SECOP
// SECOP has four applications: SessionKey, VMLoader, VMInterpreter, Bitprocess
//
// 2.1 - MEMORY STRUCTURES COMMON TO ALL SECOP APPS
// The SECOP key store buffer is a fixed-size buffer with the following
// data structure. The data structure is shared by all SECOP apps and is
// responsible for all secret keeping.
// The session key subclass is responsible for picking the secret values of
// Kr, Khs, Kc, Kvm as well as calculating Kps. All Kti values and reserved keys
// are undefined at session key establishment, however the hmac must be computed
// correctly.
// The vmloader subclass treats the key store buffer as read-only. It needs
// only the value Kvm to re-encrypt and mac the VM programs.
// The VM interpreter subclass has read-write access to the key store buffer.
// It needs to read Kvm to correctly decrypt and verify the VM program at runtime.
// It also needs to modify the Kt[i] values upon request. When exiting it must
// re-compute the hmac correctly.
// Note (bug 390982, 408877): The Kc should be decremented by the constant defined as
// follows before it is wrapped with Khs. The key_increment value corresponding to SEC
// legacy apps is 0. The key_increment values corresponding to MSVLD3 integrated apps
// is 1.
#define NV95A1_SECOP_BITPROCESS_KEY_INCREMENT                   0x00000001
typedef struct _secop_key_store
{
    unsigned int wrapped_kr[4];    // E(Ks, Kr):  Kr is picked at random
    unsigned int wrapped_sk[4];    // E(Ks, Khs): Khs is picked at random
    unsigned int wrapped_ck[4];    // E(Khs, Kc): Kc is picked at random
    unsigned int wrapped_kps[4];   // E(Kr, Kps): Kps is created by DH
    unsigned int wrapped_kvm[4];   // E(Kr, Kvm): Kvm is picked at random
    unsigned int wrapped_kt[8][4]; // E(Kr, Kti) where i = 0..7
    unsigned int reserved[2][4];   // 2 extra keys, set to 0 for now
    unsigned int hmac[4];          // MAC(Ks, previous 15 blocks)
} secop_key_store;
// The SECOP public-key cryptographic parameter buffer is a fixed-size buffer
// with the following data structure. The data structure is shared by the SESSIONKEY
// and VMLOADER apps and will be validated by the secure microcode. Driver should
// initialize the content of this structure to the constant data array provided by
// the compiled SECOP ucode image.
typedef struct _secop_constants  // 768 bytes
{
    unsigned int dlp_p[64];        // 2048-bit prime p
    unsigned int dlp_r2[64];       // 2048-bit auxiliary value r2 = (1<<4096) % p
    unsigned int ecc_curve[40];    // 160-bit curve parameters
    unsigned int nv_verif_key[10]; // nv root verification key
    unsigned int reserved[6];      // reserved must be zero
    unsigned int checksum_sk[4];   // MAC(E(S_secop, Sig_sessionkey), first 47 blocks)
    unsigned int checksum_vl[4];   // MAC(E(S_secop, Sig_vmloader), first 47 blocks)
} secop_constants;
// 2.2 -  SECOP_SUBCLASS_NV95A1_SECOP_SESSIONKEY
// This subclass implements the authenticated Diffie-Hellman session key
// establishment algorithm between the SECOP and the software player.
// Notice that this is different from the DH key exchange app in SEC
// because it uses ECDSA for two-way authentication.
#define NV95A1_SECOP_SESSIONKEY_UCODE_ID                        0x00000001
#define NV95A1_SECOP_SESSIONKEY_UCODE_LOADER_PARAM              0x00000006
// 2.2.1 MEMORY STRUCTURES
typedef struct _secop_sessionkey_params
{
    // first 256B are signatures
    unsigned char player_verif_key[40];      // [in] Software ECDSA public key for DH
    unsigned char player_certificate[40];    // [in] ECDSA signature of player_verif_key
    unsigned char challenge_signature[40];   // [in] ECDSA signature of challenge data
    unsigned char response_signature[40];    // [out] ECDSA signature of response data
    unsigned char wrapped_signing_key[32];   // [in] Wrapped HW ECDSA signing key
    unsigned char reserved[64];
    // next 256B is challenge, written by driver
    unsigned char challenge[256];            // [in] DH challenge
    // next 256B is response, written by SECOP
    unsigned char response[256];             // [out] DH response
    // last 768B is the cryptographic constants
    secop_constants consts;         // [in] public-key constants
} secop_sessionkey_params;
// The "wrapped_signing_key" contains the wrapped HW ECDSA signing key use by SECOP
// sessionkey app to sign the DH response data. The relationship between the actual
// signing key and the wrapped signing key is as follows:
// wrapped_signing_key = AES-ECBD(AES-ECBE(S_secop, AppSig_sessionkey), {signing_key, 96'b0})
// Where S_secop is the SCP secret 14 with ACL = 5'b0.
// The zero padding (96'b0) is to ensure that ciphertext be aligned to AES block size.
// 2.3 SECOP SUBCLASS NV95AB1_SECOP_VMLOADER
// This subclass implements the public-key decryption (AES-CBC + ELG) and verification
// (ECDSA) of the SECOP VM programs and the symmetric re-encryption (AES-ECB)
// and message authentication (DM hash mac) to reduce VM related overhead
#define NV95A1_SECOP_VMLOADER_UCODE_ID                          0x00000003
#define NV95A1_SECOP_VMLOADER_UCODE_LOADER_PARAM                0x00000008
// 2.3.1 MEMORY STRUCTURES
typedef struct _secop_vmloader_params // 1792 bytes
{
    // first 256B are signaturesThe vmloader subclass treats the key store buffer as read-only. It needs
    // only the value Kvm to re-encrypt and mac the VM programs.
    unsigned char player_verif_key[40];      // [in] Software ECDSA public key for VM
    unsigned char player_certificate[40];    // [in] ECDSA signature of player_verif_key
    unsigned char vm_program_signature[40];  // [in] ECDSA signature of VM program
    unsigned int vm_program_total_size;     // [in] total size of VM byte code
    unsigned int reserved0;
    unsigned int vm_program_offset[16];     // [in] VM program offset
    unsigned short vm_program_blocks[16];   // [in] VM program size (in units of 256-byte blocks)
    unsigned char wrapped_decr_key[32];      // [in] Wrapped HW ELG decryption key
    // next 512B is the ELG ciphertext
    unsigned char vm_program_key_encap[512]; // [in] ELG encapsulation of AES-CBC encryption key
    // next 256B are DM hash macs
    unsigned char vm_program_hmac[16][16];   // [out] DM hash mac of individual VM programs
    // last 768B is the cryptographic constants
    secop_constants consts;         // [in] public-key constants
} secop_vmloader_params;
// The "wrapped_decr_key" contains the wrapped HW ELG decryption key use by SECOP
// vmloader app to decrypt the vm key encapsulation data. The relationship between
// the actual decryption key and the wrapped decryption key is as follows:
// wrapped_decr_key = AES-ECBD(AES-ECBE(S_secop, AppSig_vmloader), decr_key)
// Where S_secop is the SCP secret 14 with ACL = 5'b0.
// 2.4 SECOP SUBCLASS NV95A1_SECOP_VMINTERPRETER
// This subclass implements the standalone SECOP VM interpreter for initialization
// of protected bitstream processing.
#define NV95A1_SECOP_VMINTERPRETER_UCODE_ID                     0x00000002
#define NV95A1_SECOP_VMINTERPRETER_UCODE_LOADER_PARAM           0x00000007
// 2.4.1 MEMORY STRUCTURES
typedef struct _secop_vminterpreter_params //256 bytes
{
    union
    {
        struct
        {
            unsigned int program_size;      // [in] size of VM program, must be multiple of 256B
            unsigned int reserved0[7];
            unsigned int program_offset;    // [in] offset of VM program within the VM surface, must be multiple of 256B
            unsigned int reserved1[7];
        };
        struct
        {
            unsigned int data_size[8];     // [in] size of external data buffers 0 thru 7 where 0 refers to the program
            unsigned int data_offset[8];   // [in] offset of external data buffers 0 thru 7, must be multiple of 256B
        };
    };
    unsigned int hmac[4];                   // [in] MAC of VM program as given by vmloader
    unsigned int final_pc;                  // [out] final PC address of VM program
    unsigned int exception_code;            // [out] exception code
    unsigned int reserved[42];
} secop_vminterpreter_params;
typedef struct _secop_vminterpreter_temp // 16 KB
{
    unsigned char data[16384];               // [temp, internal] HW generated internal data
} secop_vminterpreter_temp;
// 2.5 - SECOP SUBCLASS NV95A1_SECOP_BITPROCESS
// This subclass implements the MPEG-2 transport stream decryption,
// demux / filtering and re-encryption.
#define NV95A1_SECOP_BITPROCESS_UCODE_ID                        0x00000000
#define NV95A1_SECOP_BITPROCESS_UCODE_LOADER_PARAM              0x00000005
// 2.5.1 MEMORY STRUCTURES
// bitstream patching may be applied upon the start of a batch request
// or upon encountering the system use descriptors in a PMT packet
typedef enum _secop_patch_mode
{
    secop_patch_mode_none   = 0,
    secop_patch_mode_pmt    = 1,
    secop_patch_mode_batch  = 2,
} secop_patch_mode;
// pid assignment may be static (prerecorded content)
// or custom (broadcast TV content)
typedef enum _secop_pid_assignment_type
{
    secop_pid_assignment_type_static = 0,
    secop_pid_assignment_type_custom = 1,
} secop_pid_assignment_type;
// drv2secop data uses 48B
typedef struct _secop_flow_ctrl_drv2secop
{
    unsigned int drv_batch_index;
    unsigned char bitstream_id[2];
    unsigned char patch_mode;
    unsigned char patch_vm_blocks;
    unsigned char pid_assignment_type;
    unsigned char reserved[7];
    unsigned int tsin_size;
    unsigned int tsout_size;
    unsigned int es_size[2];
    unsigned int tsin_writeptr;
    unsigned int tsout_readptr;
    unsigned int es_readptr[2];
} secop_flow_ctrl_drv2secop;
// secop2drv data uses 80B
typedef struct _secop_flow_ctrl_secop2drv
{
    unsigned int secop_batch_index;
    unsigned char internal_data_valid;
    unsigned char reserved[11];
    unsigned int ctr_tsout[4];
    unsigned int ctr_es[2][4];
    unsigned int tsin_readptr;
    unsigned int tsout_writeptr;
    unsigned int es_writeptr[2];
} secop_flow_ctrl_secop2drv;
// secop internal state is 3584B
typedef struct _secop_internal_state
{
    unsigned char stream_state[2][512];
    unsigned char patch_fifo_state[16];
    unsigned char reserved0[240];
    unsigned int hmac_stream_state[2][4];
    unsigned char reserved1[224];
    unsigned char patch_fifo_data[2048];
} secop_internal_state;
// main or sub video stream info
// this struct is 1B
typedef struct _secop_video_info
{
    unsigned char enabled            : 1;
    unsigned char codec_type         : 2;
    unsigned char flush_pes          : 1;
    unsigned char reserved           : 4;
} secop_video_info;
// per-batch parameters that are not vm-related (written by driver)
// this struct is 16B
typedef struct _secop_batch_param_header
{
    unsigned int batch_byte_count   : 24;
    unsigned int skip_packet_count  : 8;
    secop_video_info video_info[2];
    unsigned short process_packet_count;
    unsigned int mainvideo_pid      : 13;
    unsigned int subvideo_pid       : 13;
    unsigned int key_index          : 3;
    unsigned int reserved           : 3;
    unsigned int mainaudio_pid      : 13;
    unsigned int subaudio_pid       : 13;
    unsigned int filter_audio       : 1;
    unsigned int patch_param_blocks : 5;
} secop_batch_param_header;
// per-batch status (written by secop after batch completion)
// this struct is 4B
typedef struct _secop_batch_status
{
    unsigned int tsout_endoffset;
} secop_batch_status;
// batch parameters for each batch takes 256B
typedef struct _secop_flow_ctrl_batch_params
{
    secop_batch_param_header hdr;
    unsigned char reserved0[48];
    secop_batch_status status;
    unsigned char reserved1[60];
    unsigned char params_patch[128];
} secop_flow_ctrl_batch_params;
// this struct is 8192B total
typedef struct _secop_flow_ctrl
{
    // first 256B is plain drv2secop
    // secop must not write here
    secop_flow_ctrl_drv2secop drv2secop;
    unsigned char reserved0[208];
    // second 256B is plain secop2drv
    // driver must not write here except during init
    secop_flow_ctrl_secop2drv secop2drv;
    unsigned char reserved1[176];
    // rest 3584B is secop-internal only
    secop_internal_state internal_state;
    // last 4096B are per-batch parameters
    // see struct definition for read/write constraints
    secop_flow_ctrl_batch_params batch_params[16];
} secop_flow_ctrl;
// this struct is 1536B
typedef struct _secop_bitstream_vm
{
    unsigned char vm_decryptor[256];
    unsigned char vm_patch[1024];
    unsigned int hmac_decryptor[4];
    unsigned int hmac_patch[4];
    unsigned int reserved[56];
} secop_bitstream_vm;
/*
 * HDCP
 *
 * Method parameters for HDCP 2.X methods. Also
 * provides status signing, ksv list validation, srm validation and stream
 * validation for HDCP 1.X. Uses HDCP2.0/HDCP2.1 spec.
 * Glossary at end of the file.
 */
// Size in bytes
#define NV95A1_HDCP_SIZE_RECV_ID_8                (40/8)
#define NV95A1_HDCP_SIZE_RTX_8                    (64/8)
#define NV95A1_HDCP_SIZE_RTX_64                   (NV95A1_HDCP_SIZE_RTX_8/8)
#define NV95A1_HDCP_SIZE_RRX_8                    (64/8)
#define NV95A1_HDCP_SIZE_RRX_64                   (NV95A1_HDCP_SIZE_RRX_8/8)
#define NV95A1_HDCP_SIZE_RN_8                     (64/8)
#define NV95A1_HDCP_SIZE_RN_64                    (NV95A1_HDCP_SIZE_RTX_8/8)
#define NV95A1_HDCP_SIZE_RIV_8                    (64/8)
#define NV95A1_HDCP_SIZE_RIV_64                   (NV95A1_HDCP_SIZE_RIV_8/8)
#define NV95A1_HDCP_SIZE_CERT_RX_8                (4176/8)
#define NV95A1_HDCP_SIZE_DCP_KPUB_8               (3072/8)
#define NV95A1_HDCP_SIZE_DCP_KPUB_64              (NV95A1_HDCP_SIZE_DCP_KPUB_8/8)
#define NV95A1_HDCP_SIZE_RX_KPUB_8                (1048/8)
#define NV95A1_HDCP_SIZE_MAX_RECV_ID_LIST_8       640       // bytes,  each receiver ID is 5 bytes long * 128 max receivers
#define NV95A1_HDCP_SIZE_MAX_RECV_ID_LIST_64      (NV95A1_HDCP_SIZE_MAX_RECV_ID_LIST_8/8)
#define NV95A1_HDCP_SIZE_M_8                      (128/8)
#define NV95A1_HDCP_SIZE_M_64                     (NV95A1_HDCP_SIZE_M_8/8)
#define NV95A1_HDCP_SIZE_E_KM_8                   (1024/8)
#define NV95A1_HDCP_SIZE_E_KM_64                  (NV95A1_HDCP_SIZE_E_KM_8/8)
#define NV95A1_HDCP_SIZE_EKH_KM_8                 (128/8)
#define NV95A1_HDCP_SIZE_EKH_KM_64                (NV95A1_HDCP_SIZE_EKH_KM_8/8)
#define NV95A1_HDCP_SIZE_E_KS_8                   (128/8)
#define NV95A1_HDCP_SIZE_E_KS_64                  (NV95A1_HDCP_SIZE_E_KS_8/8)
#define NV95A1_HDCP_SIZE_EPAIR_8                  96          // 96 bytes,  round up from 85 bytes
#define NV95A1_HDCP_SIZE_EPAIR_64                 (NV95A1_HDCP_SIZE_EPAIR_8/8)
#define NV95A1_HDCP_SIZE_EPAIR_SIGNATURE_8        (256/8)
#define NV95A1_HDCP_SIZE_EPAIR_SIGNATURE_64       (NV95A1_HDCP_SIZE_EPAIR_SIGNATURE_8/8)
#define NV95A1_HDCP_SIZE_HPRIME_8                 (256/8)
#define NV95A1_HDCP_SIZE_HPRIME_64                (NV95A1_HDCP_SIZE_HPRIME_8/8)
#define NV95A1_HDCP_SIZE_LPRIME_8                 (256/8)
#define NV95A1_HDCP_SIZE_LPRIME_64                (NV95A1_HDCP_SIZE_LPRIME_8/8)
#define NV95A1_HDCP_SIZE_MPRIME_8                 (256/8)
#define NV95A1_HDCP_SIZE_MPRIME_64                (NV95A1_HDCP_SIZE_MPRIME_8/8)
#define NV95A1_HDCP_SIZE_VPRIME_2X_8              (256/8)
#define NV95A1_HDCP_SIZE_VPRIME_2X_64             (NV95A1_HDCP_SIZE_VPRIME_2X_8/8)
#define NV95A1_HDCP_SIZE_SPRIME_8                 384
#define NV95A1_HDCP_SIZE_SPRIME_64                (NV95A1_HDCP_SIZE_SPRIME_8/8)
#define NV95A1_HDCP_SIZE_SEQ_NUM_V_8              3
#define NV95A1_HDCP_SIZE_SEQ_NUM_M_8              3
#define NV95A1_HDCP_SIZE_CONTENT_ID_8             2
#define NV95A1_HDCP_SIZE_CONTENT_TYPE_8           1
#define NV95A1_HDCP_SIZE_PES_HDR_8                (128/8)
#define NV95A1_HDCP_SIZE_PES_HDR_64               (NV95A1_HDCP_SIZE_PES_HDR_8/8)
#define NV95A1_HDCP_SIZE_CHIP_NAME                (8)
// HDCP1X uses SHA1 for VPRIME which produces 160 bits of output
#define NV95A1_HDCP_SIZE_VPRIME_1X_8              (160/8)
#define NV95A1_HDCP_SIZE_VPRIME_1X_32             (NV95A1_HDCP_SIZE_VPRIME_1X_8/4)
#define NV95A1_HDCP_SIZE_LPRIME_1X_8              (160/8)
#define NV95A1_HDCP_SIZE_LPRIME_1X_32             (NV95A1_HDCP_SIZE_LPRIME_1X_8/4)
#define NV95A1_HDCP_SIZE_QID_1X_8                 (64/8)
#define NV95A1_HDCP_SIZE_QID_1X_64                (NV95A1_HDCP_SIZE_QID_1X_8/8)
// Constants
// Changing this contant will change size of certain structures below
// Please make sure they are resized accordingly.
#define NV95A1_HDCP_MAX_STREAMS_PER_RCVR        2
// HDCP versions
#define NV95A1_HDCP_VERSION_1X                                  (0x00000001)
#define NV95A1_HDCP_VERSION_20                                  (0x00000002)
#define NV95A1_HDCP_VERSION_21                                  (0x00000003)
//COMMON ERROR CODES
#define NV95A1_HDCP_ERROR_UNKNOWN                               (0x80000000)
#define NV95A1_HDCP_ERROR_NONE                                  (0x00000000)
#define NV95A1_HDCP_ERROR_INVALID_SESSION                       (0x00000001)
#define NV95A1_HDCP_ERROR_SB_NOT_SET                            (0x00000002)
#define NV95A1_HDCP_ERROR_NOT_INIT                              (0x00000003)
#define NV95A1_HDCP_ERROR_INVALID_STAGE                         (0x00000004)
#define NV95A1_HDCP_ERROR_MSG_UNSUPPORTED                       (0x00000005)
/*
 * READ_CAPS
 *
 * This method passes the HDCP Tsec application's capabilities back to the
 * client. The capabilities include supported versions, maximum number of
 * simultaneous sessions supported, exclusive dmem support, max scratch
 * buffer needed etc.If DMEM carveout (exclusive DMEM) is not available for
 * HDCP, then the client must allocate a scratch buffer of size 'requiredScratch
 * BufferSize' in FB and pass it TSEC.
 *
 * Depends on: [none]
 */
 typedef struct _hdcp_read_caps_param
 {
    unsigned int  supportedVersionsMask;               // >>out
    unsigned int  maxSessions;                         // >>out
    unsigned int  maxActiveSessions;                   // >>out
    unsigned int  scratchBufferSize;                   // >>out
    unsigned int  maxStreamsPerReceiver;               // >>out
    unsigned int  currentBuildMode;                    // >>out
    unsigned int  falconIPVer;                         // >>out
    unsigned char   reserved[4];
    unsigned char   chipName[NV95A1_HDCP_SIZE_CHIP_NAME];// >>out
    unsigned char   bIsStackTrackEnabled;                // >>out
    unsigned char   bIsImemTrackEnabled;                 // >>out
    unsigned char   bIsDebugChip;                        // >>out
    unsigned char   bIsExclusiveDmemAvailable;           // >>out
    unsigned char   bIsStatusSigningSupported;           // >>out
    unsigned char   bIsStreamValSupported;               // >>out
    unsigned char   bIsKsvListValSupported;              // >>out
    unsigned char   bIsPreComputeSupported;              // >>out
    unsigned int  retCode;                             // >>out
 } hdcp_read_caps_param;
#define NV95A1_HDCP_READ_CAPS_ERROR_NONE                        NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_READ_CAPS_CURRENT_BUILD_MODE_PROD           (0x00)
#define NV95A1_HDCP_READ_CAPS_CURRENT_BUILD_MODE_DEBUG_1        (0x01)
#define NV95A1_HDCP_READ_CAPS_CURRENT_BUILD_MODE_DEBUG_2        (0x02)
#define NV95A1_HDCP_READ_CAPS_CURRENT_BUILD_MODE_DEBUG_3        (0x03)
#define NV95A1_HDCP_READ_CAPS_EXCL_DMEM_AVAILABLE               (0x01)
#define NV95A1_HDCP_READ_CAPS_EXCL_DMEM_UNAVAILABLE             (0x00)
#define NV95A1_HDCP_READ_CAPS_STATUS_SIGNING_SUPPORTED          (0x01)
#define NV95A1_HDCP_READ_CAPS_STATUS_SIGNING_UNSUPPORTED        (0x00)
#define NV95A1_HDCP_READ_CAPS_STREAM_VAL_SUPPORTED              (0x01)
#define NV95A1_HDCP_READ_CAPS_STREAM_VAL_UNSUPPORTED            (0x00)
#define NV95A1_HDCP_READ_CAPS_KSVLIST_VAL_SUPPORTED             (0x01)
#define NV95A1_HDCP_READ_CAPS_KSVLIST_VAL_UNSUPPORTED           (0x00)
#define NV95A1_HDCP_READ_CAPS_PRE_COMPUTE_SUPPORTED             (0x01)
#define NV95A1_HDCP_READ_CAPS_PRE_COMPUTE_UNSUPPORTED           (0x00)
#define NV95A1_HDCP_READ_CAPS_DEBUG_CHIP_YES                    (0x01)
#define NV95A1_HDCP_READ_CAPS_DEBUG_CHIP_NO                     (0x00)
#define NV95A1_HDCP_READ_CAPS_CHIP_NAME_T114                    "t114"
#define NV95A1_HDCP_READ_CAPS_CHIP_NAME_T148                    "t148"
#define NV95A1_HDCP_READ_CAPS_CHIP_NAME_T124                    "t124"
#define NV95A1_HDCP_READ_CAPS_CHIP_NAME_GM107                   "gm107"
/*
 * INIT
 *
 * This method will initialize necessary global data needed for HDCP app
 * in TSEC. This includes decrypting LC128, decrypting upstream priv key
 * and setting up sessions pool. If exclusibe DMEM is not available,
 * SET_SCRATCH_BUFFER should precede this method and other methods as
 * documented below. Size of scratch buffer is communicated to client
 * through READ_CAPS method and TSEC HDCP app assumes the SB is allocated
 * to that precise size aligned to 256 bytes. INIT needs to pass the chipId
 * from PMC_BOOT reg.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * Error codes:
 *
 * REINIT                  - HDCP application already initialized
 * SB_NOT_SET              - Scratch buffer offset not set
 * INVALID_KEYS            - Decrypting the confidential data failed
 * UNKNOWN                 - Unknown errors while Initing.
 *
 * Flags:
 *
 * FORCE_INIT              - Force initialization even if already initialized. Will reset
 *                           all the sessions
 */
 typedef struct _hdcp_init_param
 {
    unsigned int  flags;                               // <<in
    unsigned int  chipId;                              // <<in
    unsigned int  retCode;                             // >>out
 } hdcp_init_param;
#define NV95A1_HDCP_INIT_ERROR_NONE                             NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_INIT_ERROR_REINIT                           (0x00000001)
#define NV95A1_HDCP_INIT_ERROR_SB_NOT_SET                       NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_INIT_ERROR_INVALID_KEYS                     (0x00000003)
#define NV95A1_HDCP_INIT_ERROR_UNKNOWN                          (0x00000004)
#define NV95A1_HDCP_INIT_FLAG_FORCE_INIT                         0:0
#define NV95A1_HDCP_INIT_FLAG_FORCE_INIT_DISABLE                 0
#define NV95A1_HDCP_INIT_FLAG_FORCE_INIT_ENABLE                  1
/*
 * CREATE_SESSION
 *
 * A session is synonymous to a secure channel created between the transmitter
 * and receiver. Session keeps track of progress in establishing secure channel
 * by storing all intermediate states. Number of simultaneous sessions will
 * equal the number of wireless displays we plan to support. This method will
 * fail if client tries to create more sessions than supported. Number of
 * sessions is limited only due to the scratch buffer/DMEM constraint.
 * Session is created for version 2.0 by default. Use Update session to
 * change the version. While creating a session, the client needs to pass
 * the expected number of streams used by the receiver. The number of streams
 * cannot be greater than maxStreamsPerReceiver in READ_CAPS method.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * Error codes:
 *
 * MAX                  - No free sessions available.
 * SB_NOT_SET           - Scratch buffer is not set.
 * NOT_INIT             - HDCP app not initialized yet.
 * MAX_STREAMS          - noOfStreams is greater than max supported.
 */
 typedef struct _hdcp_create_session_param
 {
    unsigned int  noOfStreams;                         // <<in
    unsigned int  sessionID;                           // >>out
    unsigned long long  rtx;                                 // >>out
    unsigned int  retCode;                             // >>out
    unsigned char   reserved1[4];
 } hdcp_create_session_param;
#define NV95A1_HDCP_CREATE_SESSION_ERROR_NONE                   NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_CREATE_SESSION_ERROR_MAX                    (0x00000001)
#define NV95A1_HDCP_CREATE_SESSION_ERROR_SB_NOT_SET             NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_CREATE_SESSION_ERROR_NOT_INIT               NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_CREATE_SESSION_ERROR_INVALID_STREAMS        (0x00000004)
/*
 * VERIFY_CERT_RX
 *
 * Verifies receiver public certificate's signature using DCP's public key. If
 * verification succeeds, all necessary information from the certificate are
 * retained in session before returning back to client. Along with certificate,
 * the client also indicates if the wireless receiver is a repeater.
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_CERT_RX, SET_DCP_KPUB]
 *
 * Error codes:
 *
 * INVALID_SESSION - Session not found
 * SB_NOT_SET      - Scratch buffer not set
 * NOT_INIT        - HDCP app not initialized yet.
 * INVALID_STAGE   - State machine sequence is not followed
 * INVALID_CERT    - Cert validation failed
 * CERT_NOT_SET    - Certiticate offset not set
 * DCP_KPUB_NOT_SET- Dcp public key not set
 * DCP_KPUB_INVALID- Dcp key provided doesn't follow the standards
 *                 - (eg: Null exponent)
 */
 typedef struct _hdcp_verify_cert_rx_param
 {
    unsigned int  sessionID;                           // <<in
    unsigned char   repeater;                            // <<in
    unsigned char   reserved[3];
    unsigned int  retCode;                             // >>out
 } hdcp_verify_cert_rx_param;
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_NONE                   NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_INVALID_SESSION        NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_SB_NOT_SET             NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_NOT_INIT               NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_INVALID_STAGE          NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_INVALID_CERT           (0x00000005)
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_CERT_NOT_SET           (0x00000006)
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_DCP_KPUB_NOT_SET       (0x00000007)
#define NV95A1_HDCP_VERIFY_CERT_RX_ERROR_DCP_KPUB_INVALID       (0x00000008)
/*
 * GENERATE_EKM
 *
 * Generates 128 bit random number Km and encrypts it using receiver's public
 * ID to generate 1024 bit Ekpub(Km). Km is confidential and not passed to the
 * client, but is saved in session state.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION - Session not found
 * SB_NOT_SET      - Scratch Buffer not set
 * NOT_INIT        - HDCP app not initialized yet.
 * INVALID_STAGE   - State machine sequence is not followed
 * DUP_KM          - Session already has a valid Km. Duplicate request
 * RX_KPUB_NOT_SET - Receiver public key not set
 *
 */
 typedef struct _hdcp_generate_ekm_param
 {
    unsigned int  sessionID;                           // <<in
    unsigned char   reserved1[4];
    unsigned long long  eKm[NV95A1_HDCP_SIZE_E_KM_64];       // >>out
    unsigned int  retCode;                             // >>out
    unsigned char   reserved2[4];
 } hdcp_generate_ekm_param;
#define NV95A1_HDCP_GENERATE_EKM_ERROR_NONE                     NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_GENERATE_EKM_ERROR_INVALID_SESSION          NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_GENERATE_EKM_ERROR_SB_NOT_SET               NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_GENERATE_EKM_ERROR_NOT_INIT                 NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_GENERATE_EKM_ERROR_INVALID_STAGE            NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_GENERATE_EKM_ERROR_RX_KPUB_NOT_SET          (0x00000005)
/*
 * REVOCATION_CHECK
 *
 * Validates if SRM is valid. If yes, checks if receiver ID is in revocation
 * list. Client is supposed to take care of checking the version of SRM and
 * invoking REVOCATION check if SRM is found to be a newer version. This method
 * is applicable to both HDCP1.X and HDCP 2.0 devices. Incase of HDCP 1.X,
 * TSEC will read the BKSV from the display hardware.
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_SRM, SET_DCP_KPUB]
 *
 * INVALID_SESSION   - Session not found
 * SB_NOT_SET        - Scratch Buffer not set
 * NOT_INIT          - HDCP app not initialized yet.
 * INVALID_STAGE     - Invalid stage
 * INVALID_SRM_SIZE  - Srm size is not valid
 * SRM_VALD_FAILED   - Srm validation failed
 * RCV_ID_REVOKED    - Receiver ID revoked
 * SRM_NOT_SET       - Srm is not set
 * DCP_KPUB_NOT_SET  - DCP Kpub is not set
 */
 typedef struct _hdcp_revocation_check_param
 {
    union                                       // <<in
    {
        unsigned int  sessionID;
        unsigned int  apIndex;
    }transID;
    unsigned char   isVerHdcp2x;                         // <<in
    unsigned char   reserved[3];
    unsigned int  srmSize;                             // <<in
    unsigned int  retCode;                             // >>out
 } hdcp_revocation_check_param;
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_NONE                 NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_INVALID_SESSION      NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_SB_NOT_SET           NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_NOT_INIT             NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_INVALID_STAGE        NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_INVALID_SRM_SIZE     (0x00000005)
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_SRM_VALD_FAILED      (0x00000006)
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_RCV_ID_REVOKED       (0x00000007)
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_SRM_NOT_SET          (0x00000008)
#define NV95A1_HDCP_REVOCATION_CHECK_ERROR_DCP_KPUB_NOT_SET     (0x00000009)
/*
 * VERIFY_HPRIME
 *
 * Computes H and verifies if H == HPRIME
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet.
 * INVALID_STAGE      - State machine sequence is not followed
 * HPRIME_VALD_FAILED - Hprime validation failed
 */
 typedef struct _hdcp_verify_hprime_param
 {
    unsigned long long  hprime[NV95A1_HDCP_SIZE_HPRIME_64];  // <<in
    unsigned int  sessionID;                           // <<in
    unsigned int  retCode;                             // >>out
 } hdcp_verify_hprime_param;
#define NV95A1_HDCP_VERIFY_HPRIME_ERROR_NONE                    NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_VERIFY_HPRIME_ERROR_INVALID_SESSION         NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_VERIFY_HPRIME_ERROR_SB_NOT_SET              NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_VERIFY_HPRIME_ERROR_NOT_INIT                NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_VERIFY_HPRIME_ERROR_INVALID_STAGE           NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_VERIFY_HPRIME_ERROR_HPRIME_VALD_FAILED      (0x00000005)
/*
 * ENCRYPT_PAIRING_INFO
 *
 * This encrypts Ekh(km),km and m using the secret key and sends back to client
 * for persistent storage. Will be used when the same receiver is discovered
 * later. Uses HMAC to produce a signature to verify the integrity.
 * m = 64 0's appended to rtx and all are in big-endian format
 * EPair = Eaes(rcvId||m||km||Ekh(Km)||SHA256(rcvId||m||km||Ekh(Km)))
 *       = (40 + 128 + 128 + 128 + 256) bits = 85 bytes  round up to 96
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION - Session not found
 * SB_NOT_SET      - Scratch Buffer not set
 * NOT_INIT        - HDCP app not initialized yet
 * INVALID_KM      - Km is not initialized
 * INVALID_M       - m is not initialized
 */
 typedef struct _hdcp_encrypt_pairing_info_param
 {
    unsigned int  sessionID;                           // <<in
    unsigned char   reserved1[4];
    unsigned long long  eKhKm[NV95A1_HDCP_SIZE_EKH_KM_64];   // <<in
    unsigned long long  ePair[NV95A1_HDCP_SIZE_EPAIR_64];    // >>out
    unsigned int  retCode;                             // >>out
    unsigned char   reserved2[4];
 } hdcp_encrypt_pairing_info_param;
#define NV95A1_HDCP_ENCRYPT_PAIRING_INFO_ERROR_NONE             NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_ENCRYPT_PAIRING_INFO_ERROR_INVALID_SESSION  NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_ENCRYPT_PAIRING_INFO_ERROR_SB_NOT_SET       NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_ENCRYPT_PAIRING_INFO_ERROR_NOT_INIT         NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_ENCRYPT_PAIRING_INFO_ERROR_INVALID_KM       (0x00000004)
#define NV95A1_HDCP_ENCRYPT_PAIRING_INFO_ERROR_INVALID_M        (0x00000005)
/*
 * DECRYPT_PAIRING_INFO
 *
 * Decrypts the pairing info encrypted using the method ENCRYPT_PAIRING_INFO
 * Extracts Km from the encrypted info and saves it in session details.
 * Km is confidential and not sent back to client.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION  - Session not found
 * SB_NOT_SET       - Scratch Buffer not set
 * NOT_INIT         - HDCP app not initialized yet
 * INVALID_EPAIR    - Encrypted pairing info signature verification failed.
 */
 typedef struct _hdcp_decrypt_pairing_info_param
 {
    unsigned int  sessionID;                           // <<in
    unsigned char   reserved1[4];
    unsigned long long  ePair[NV95A1_HDCP_SIZE_EPAIR_64];    // <<in
    unsigned long long  m[NV95A1_HDCP_SIZE_M_64];            // >>out
    unsigned long long  eKhKm[NV95A1_HDCP_SIZE_EKH_KM_64];   // >>out
    unsigned int  retCode;                             // >>out
    unsigned char   reserved2[4];
 } hdcp_decrypt_pairing_info_param;
#define NV95A1_HDCP_DECRYPT_PAIRING_INFO_ERROR_NONE             NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_DECRYPT_PAIRING_INFO_ERROR_INVALID_SESSION  NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_DECRYPT_PAIRING_INFO_ERROR_SB_NOT_SET       NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_DECRYPT_PAIRING_INFO_ERROR_NOT_INIT         NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_DECRYPT_PAIRING_INFO_ERROR_INVALID_EPAIR    (0x00000004)
/*
 * UPDATE_SESSION
 *
 * Updates the session parameters which are determined during key exchange
 * and after it.(like displayid-session mapping)
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION      - Session not found
 * SB_NOT_SET           - Scratch Buffer not set
 * NOT_INIT             - HDCP app not initialized yet
 * INVALID_STAGE        - State machine sequence is not followed
 * DUP_RRX              - RRX already updated. Duplicate request.
 * INVALID_UPD_MASK     - Update mask is incorrect
 * HDCP_VER_UNSUPPORTED - Version is not supported.
 */
 typedef struct _hdcp_update_session_param
 {
    unsigned long long  rrx;                                 // <<in
    unsigned int  sessionID;                           // <<in
    unsigned int  head;                                // <<in
    unsigned int  orIndex;                             // <<in
    unsigned int  hdcpVer;                             // <<in
    unsigned int  updmask;                             // <<in
    unsigned int  bRecvPreComputeSupport;              // <<in
    unsigned int  retCode;                             // >>out
    unsigned char   reserved[4];
 } hdcp_update_session_param;
#define NV95A1_HDCP_UPDATE_SESSION_ERROR_NONE                   NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_UPDATE_SESSION_ERROR_INVALID_SESSION        NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_UPDATE_SESSION_ERROR_SB_NOT_SET             NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_UPDATE_SESSION_ERROR_NOT_INIT               NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_UPDATE_SESSION_ERROR_INVALID_STAGE          NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_UPDATE_SESSION_ERROR_DUP_RRX                (0x00000005)
#define NV95A1_HDCP_UPDATE_SESSION_ERROR_INVALID_UPD_MASK       (0x00000006)
#define NV95A1_HDCP_UPDATE_SESSION_ERROR_HDCP_VER_UNSUPPORTED   (0x00000007)
#define NV95A1_HDCP_UPDATE_SESSION_MASK_HEAD_PRESENT            (0x00000000)
#define NV95A1_HDCP_UPDATE_SESSION_MASK_ORINDEX_PRESENT         (0x00000001)
#define NV95A1_HDCP_UPDATE_SESSION_MASK_RRX_PRESENT             (0x00000002)
#define NV95A1_HDCP_UPDATE_SESSION_MASK_VERSION_PRESENT         (0x00000003)
#define NV95A1_HDCP_UPDATE_SESSION_MASK_PRE_COMPUTE_PRESENT     (0x00000004)
#define NV95A1_HDCP_UPDATE_SESSION_PRE_COMPUTE_SUPPORTED        (0x01)
#define NV95A1_HDCP_UPDATE_SESSION_PRE_COMPUTE_UNSUPPORTED      (0x00)
/*
 * GENERATE_LC_INIT
 *
 * Generates 64 bit random number rn.
 * Checks for completed authentication as precondition. Completed auth
 * imply non-null rrx, rtx, km.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION  - Session not found
 * SB_NOT_SET       - Scratch Buffer not set
 * NOT_INIT         - HDCP app not initialized yet
 * INVALID_STAGE    - State machine sequence is not followed or
 *                    LC has already succeded for this receiver
 */
 typedef struct _hdcp_generate_lc_init_param
 {
    unsigned int  sessionID;                           // <<in
    unsigned char   reserved1[4];
    unsigned long long  rn;                                  // >>out
    unsigned int  retCode;                             // >>out
    unsigned char   reserved2[4];
 } hdcp_generate_lc_init_param;
#define NV95A1_HDCP_GENERATE_LC_INIT_ERROR_NONE                 NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_GENERATE_LC_INIT_ERROR_INVALID_SESSION      NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_GENERATE_LC_INIT_ERROR_SB_NOT_SET           NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_GENERATE_LC_INIT_ERROR_NOT_INIT             NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_GENERATE_LC_INIT_ERROR_INVALID_STAGE        NV95A1_HDCP_ERROR_INVALID_STAGE
/*
 * VERIFY_LPRIME
 *
 * Computes L and verifies if L == LPRIME. Incase of HDCP-2.1 receiver
 * with PRE_COMPUTE support only most significant 128 bits of Lprime is used
 * for comparison.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet
 * INVALID_STAGE      - State machine sequence is not followed or
 *                      RTT_CHALLENGE not used for HDCP2.1 receiver
 *                      with PRE_COMPUTE support
 * LPRIME_VALD_FAILED - Lprime validation failed
 */
 typedef struct _hdcp_verify_lprime_param
 {
    unsigned long long  lprime[NV95A1_HDCP_SIZE_LPRIME_64];  // <<in
    unsigned int  sessionID;                           // <<in
    unsigned int  retCode;                             // >>out
 } hdcp_verify_lprime_param;
#define NV95A1_HDCP_VERIFY_LPRIME_ERROR_NONE                    NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_VERIFY_LPRIME_ERROR_INVALID_SESSION         NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_VERIFY_LPRIME_ERROR_SB_NOT_SET              NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_VERIFY_LPRIME_ERROR_NOT_INIT                NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_VERIFY_LPRIME_ERROR_INVALID_STAGE           NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_VERIFY_LPRIME_ERROR_LPRIME_VALD_FAILED      (0x00000005)
/*
 * GENERATE_SKE_INIT
 *
 * Generates 64 bit random number Riv and encrypted 128 bit session key.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION  - Session not found
 * SB_NOT_SET       - Scratch Buffer not set
 * NOT_INIT         - HDCP app not initialized yet
 * LC_INIT_NOT_DONE - LC init phase is not completed yet
 */
 typedef struct _hdcp_generate_ske_init_param
 {
    unsigned int  sessionID;                           // <<in
    unsigned char   reserved1[4];
    unsigned long long  eKs[NV95A1_HDCP_SIZE_E_KS_64];       // >>out
    unsigned long long  riv;                                 // >>out
    unsigned int  retCode;                             // >>out
    unsigned char   reserved2[4];
 } hdcp_generate_ske_init_param;
#define NV95A1_HDCP_GENERATE_SKE_INIT_ERROR_NONE                NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_GENERATE_SKE_INIT_ERROR_INVALID_SESSION     NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_GENERATE_SKE_INIT_ERROR_SB_NOT_SET          NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_GENERATE_SKE_INIT_ERROR_NOT_INIT            NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_GENERATE_SKE_INIT_ERROR_INVALID_STAGE       NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_GENERATE_SKE_INIT_ERROR_LC_INIT_NOT_DONE    (0x00000005)
/*
 * VERIFY_VPRIME
 *
 * Computes V and verifies if V == VPRIME.
 * Does revocation check on the receiver ids got from the repeater along
 * with bstatus checks. maxdevs exceeded and maxcascadeexceeded are considered
 * false. Client should have checked for those values even before calling this
 * method. Applies to both HDCP1.x and HDCP2.x spec.revoID will hold revoked
 * received ID if any. Incase of HDCP-2.1 repeater, vprime holds the most
 * significant 128-bits which will be compared against the most significant
 * 128 bits of V. Incase of successful comparison, least-significant 128 bits
 * of V is returned to the client via v128l. Client needs to populate seqNumV,
 * bHasHdcp20Repeater and bHasHdcp1xDevice only if repeater supports HDCP-2.1
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_SRM, SET_DCP_KPUB, SET_RECEIVER_ID_LIST]
 *
 * INVALID_SESSION     - Session not found
 * SB_NOT_SET          - Scratch Buffer not set
 * NOT_INIT            - HDCP app not initialized yet
 * INVALID_SRM_SIZE    - Srm size not valid
 * VPRIME_VALD_FAILED  - Vprime validation failed
 * INVALID_APINDEX     - Either Head or OR is not valid
 * SRM_VALD_FAILED     - Srm validation failed
 * RCVD_ID_REVOKED     - Found a revoked receiver ID in receiver Id list
 * SRM_NOT_SET         - Srm not set
 * DCP_KPUB_NOT_SET    - DCP public key not set
 * RCVD_ID_LIST_NOT_SET- Receiver ID list not set
 * SEQ_NUM_V_ROLLOVER  - Seq_Num_V rolled over
 */
 typedef struct _hdcp_verify_vprime_param
 {
    unsigned long long  Vprime[NV95A1_HDCP_SIZE_VPRIME_2X_64];  // <<in
    union                                          // <<in
    {
        unsigned int  sessionID;
        unsigned int  apIndex;
    }transID;
    unsigned int  srmSize;                                // <<in
    unsigned int  bStatus;                                // <<in
    unsigned char   isVerHdcp2x;                            // <<in
    unsigned char   depth;                                  // <<in
    unsigned char   deviceCount;                            // <<in
    unsigned char   bHasHdcp20Repeater;                     // <<in
    unsigned char   seqNumV[NV95A1_HDCP_SIZE_SEQ_NUM_V_8];  // <<in
    unsigned char   bHasHdcp1xDevice;                       // <<in
    unsigned char   revoID[NV95A1_HDCP_SIZE_RECV_ID_8];     // >>out
    unsigned char   reserved1[7];
    unsigned long long  V128l[NV95A1_HDCP_SIZE_VPRIME_2X_64/2]; // >>out
    unsigned int  retCode;                                // >>out
    unsigned char   reserved2[4];
 } hdcp_verify_vprime_param;
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_NONE                    NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_INVALID_SESSION         NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_SB_NOT_SET              NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_NOT_INIT                NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_INVALID_SRM_SIZE        (0x00000004)
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_VPRIME_VALD_FAILED      (0x00000005)
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_INVALID_APINDEX         (0x00000006)
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_SRM_VALD_FAILED         (0x00000007)
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_RCVD_ID_REVOKED         (0x00000008)
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_SRM_NOT_SET             (0x00000009)
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_DCP_KPUB_NOT_SET        (0x000000010)
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_RCVD_ID_LIST_NOT_SET    (0x000000011)
#define NV95A1_HDCP_VERIFY_VPRIME_ERROR_SEQ_NUM_V_ROLLOVER      (0x000000012)
/*
 * ENCRYPTION_RUN_CTRL
 *
 * To start/stop/pause the encryption for a particular session
 * Incase of HDCP1.X version, apIndex will be used to stop encryption.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION  - Session not found
 * SB_NOT_SET       - Scratch Buffer not set
 * NOT_INIT         - HDCP app not initialized yet
 * INVALID_STATE    - Invalid control state
 * INVALID_APINDEX  - Either Head or OR is not valid
 * INVALID_FLAG     - Control flag is invalid
 */
 typedef struct _hdcp_encryption_run_ctrl_param
 {
    union                                       // <<in
    {
        unsigned int  sessionID;
        unsigned int  apIndex;
    }transID;                                   // <<in
    unsigned int  ctrlFlag;                            // <<in
    unsigned int  retCode;                             // >>out
 } hdcp_encryption_run_ctrl_param;
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL_ERROR_NONE              NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL_ERROR_INVALID_SESSION   NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL_ERROR_SB_NOT_SET        NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL_ERROR_NOT_INIT          NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL_ERROR_INVALID_STAGE     NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL_ERROR_INVALID_APINDEX   (0x00000005)
#define NV95A1_HDCP_ENCRYPTION_RUN_CTRL_ERROR_INVALID_FLAG      (0x00000006)
#define NV95A1_HDCP_ENCRYPTING_RUN_CTRL_FLAG_START              (0x00000001)
#define NV95A1_HDCP_ENCRYPTING_RUN_CTRL_FLAG_STOP               (0x00000002)
#define NV95A1_HDCP_ENCRYPTING_RUN_CTRL_FLAG_PAUSE              (0x00000003)
/*
 * SESSION_CTRL
 *
 * To activate/reset/delete a session. If deleted, Any future references
 * to this session becomes illegal and this sessionID will be deleted.
 * At any time there may be multiple sessions authenticated, but only
 * few sessions should be activated at a time. This method will help
 * the clients choose which session should be activated/deactivated.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet
 * INVALID_STAGE      - Activation failed because session is not in proper
 *                    - stage or revocation check is not done.
 * SESSION_ACTIVE     - Error trying to reset or delete an active session.
 *                      This error will also be flagged if client tries to
 *                      activate already active session.
 * SESSION_NOT_ACTIVE - Deactivating a non-active session
 * SESSION_ACTIVE_MAX - Cannot activate a session. Maximum already active.
 */
 typedef struct _hdcp_session_ctrl_param
 {
    unsigned int  sessionID;                           // <<in
    unsigned int  ctrlFlag;                            // <<in
    unsigned int  retCode;                             // >>out
 } hdcp_session_ctrl_param;
#define NV95A1_HDCP_SESSION_CTRL_ERROR_NONE                     NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_SESSION_CTRL_ERROR_INVALID_SESSION          NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_SESSION_CTRL_ERROR_SB_NOT_SET               NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_SESSION_CTRL_ERROR_NOT_INIT                 NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_SESSION_CTRL_ERROR_INVALID_STAGE            NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_SESSION_CTRL_ERROR_SESSION_ACTIVE           (0x00000005)
#define NV95A1_HDCP_SESSION_CTRL_ERROR_SESSION_NOT_ACTIVE       (0x00000006)
#define NV95A1_HDCP_SESSION_CTRL_ERROR_SESSION_ACTIVE_MAX       (0x00000007)
#define NV95A1_HDCP_SESSION_CTRL_FLAG_DELETE                    (0x00000001)
#define NV95A1_HDCP_SESSION_CTRL_FLAG_ACTIVATE                  (0x00000002)
#define NV95A1_HDCP_SESSION_CTRL_FLAG_DEACTIVATE                (0x00000003)
/*
 * VALIDATE_SRM
 *
 * Verifies SRM signature using DCP's public key.
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_SRM, SET_DCP_KPUB]
 *
 * INVALID_SRM_SIZE  - Srm size invalid
 * SB_NOT_SET        - Scratch Buffer not set
 * NOT_INIT          - HDCP app not initialized yet
 * SRM_VALD_FAILED   - Srm validation failed
 * SRM_NOT_SET       - Srm not set
 * DCP_KPUB_NOT_SET  - DCP public key not set
 */
 typedef struct _hdcp_validate_srm_param
 {
    unsigned int  srmSize;                             // <<in
    unsigned int  retCode;                             // >>out
 } hdcp_validate_srm_param;
#define NV95A1_HDCP_VALIDATE_SRM_ERROR_NONE                     NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_VALIDATE_SRM_ERROR_INVALID_SRM_SIZE         (0x00000001)
#define NV95A1_HDCP_VALIDATE_SRM_ERROR_SB_NOT_SET               NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_VALIDATE_SRM_ERROR_NOT_INIT                 NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_VALIDATE_SRM_ERROR_SRM_VALD_FAILED          (0x00000004)
#define NV95A1_HDCP_VALIDATE_SRM_ERROR_SRM_NOT_SET              (0x00000005)
#define NV95A1_HDCP_VALIDATE_SRM_ERROR_DCP_KPUB_NOT_SET         (0x00000006)
/*
 * COMPUTE_SPRIME
 *
 * Computes Sprime using S, CS, Rtx, Receiver ID, Cn
 * Sprime is Ekprivtx(S || CS || Rtx || ReceiverID || Cn)
 * Size of Sprime is not confirmed yet.. If client needs only
 * plain status and not signature, set bPlainStatus to TRUE
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_SPRIME]
 *
 * INVALID_SESSION - Session not found
 * SB_NOT_SET      - Scratch Buffer not set
 * NOT_INIT        - HDCP app not initialized yet
 * INVALID_APINDEX - Either Head or OR is invalid
 * SPRIME_NOT_SET  - SPrime offset not set
 */
 typedef struct _hdcp_compute_sprime_param
 {
    union                                       // <<in
    {
        unsigned int  sessionID;
        unsigned int  apIndex;
    }transID;
    unsigned int  cn;                                  // <<in
    unsigned char   isVerHdcp20;                         // <<in
    unsigned char   bPlainStatus;                        // <<in
    unsigned char   reserved1[2];
    unsigned int  status;                              // >>out
    unsigned int  cs;                                  // >>out
    unsigned int  retCode;                             // >>out
 } hdcp_compute_sprime_param;
#define NV95A1_HDCP_COMPUTE_SPRIME_ERROR_NONE                   NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_COMPUTE_SPRIME_ERROR_INVALID_SESSION        NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_COMPUTE_SPRIME_ERROR_SB_NOT_SET             NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_COMPUTE_SPRIME_ERROR_NOT_INIT               NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_COMPUTE_SPRIME_ERROR_INVALID_APINDEX        (0x00000004)
#define NV95A1_HDCP_COMPUTE_SPRIME_ERROR_SPRIME_NOT_SET         (0x00000005)
#define NV95A1_HDCP_COMPUTE_SPRIME_PLAIN_STATUS_TRUE            (0x01)
#define NV95A1_HDCP_COMPUTE_SPRIME_PLAIN_STATUS_FALSE           (0x00)
/*
 * VALIDATE_DP_STREAM
 *
 * Validates the dp1.2 stream (lprime validation). Only applies to HDCP 1.x.
 * Assumes Vprime is already validated.
 *
 * Depends on: [none]
 *
 * LPRIME_VALD_FAILED  - Lprime validation failed
 * INVALID_APINDEX     - Either head or OR is not valid
 *
 * TODO Incomplete
 */
 typedef struct _hdcp_validate_dp_stream_param
 {
     unsigned long long  qID;                                   // <<in
     unsigned int  apIndex;                               // <<in
     unsigned int  lprime[NV95A1_HDCP_SIZE_LPRIME_1X_32]; // <<in
     unsigned int  vprime[NV95A1_HDCP_SIZE_VPRIME_1X_32]; // <<in
     unsigned char   dpStreamID;                            // <<in
     unsigned char   reserved1[3];
     unsigned int  retCode;                               // >>out
     unsigned char   reserved2[4];
 } hdcp_validate_dp_stream_param;
#define NV95A1_HDCP_VALIDATE_DP_STREAM_ERROR_NONE               NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_VALIDATE_DP_STREAM_ERROR_LPRIME_VALD_FAILED (0x00000001)
#define NV95A1_HDCP_VALIDATE_DP_STREAM_ERROR_INVALID_APINDEX    (0x00000002)
/*
 * TEST_SECURE_STATUS
 *
 * Tests the secure status access in TSEC.
 *
 * Depends on: [none]
 *
 * FAILED - Test secure status failed.
 */
 typedef struct _hdcp_test_secure_status_param
 {
    unsigned int  retCode;                            // >>out
 } hdcp_test_secure_status_param;
#define NV95A1_HDCP_TEST_SECURE_STATUS_ERROR_NONE               NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_TEST_SECURE_STATUS_ERROR_FAILED             (0x00000001)
/*
 * ENCRYPT
 *
 * This method will be used after successfully activating an authenticated
 * session which encrypts the provided plain content using HDCP2.0
 * standard encryption. Method returns the input and stream counter
 * used for encrypting the first block(16-bytes) in the buffer and the client
 * is supposed to derive those counters for successive blocks. This method
 * expects the input and output buffer to be 256 byte aligned and expects
 * proper padding if size of a block in input buffer is less than 16-bytes.
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_ENC_INPUT_BUFFER, SET_ENC_OUTPUT_BUFFER]
 *
 * INVALID_SESSION      - Session not found
 * SB_NOT_SET           - Scratch Buffer not set
 * NOT_INIT             - HDCP app not initialized yet
 * INVALID_STAGE        - State machine sequence is not followed
 * SESSION_NOT_ACTIVE   - Session is not active for encryption
 * INPUT_BUFFER_NOT_SET - Input buffer not set
 * OUPUT_BUFFER_NOT_SET - Output buffer not set
 * INVALID_STREAM       - Stream ID passed is invalid
 * INVALID_ALIGN        - Either INPUT,OUTPUT or encOffset is not in expected alignment
 */
 typedef struct _hdcp_encrypt_param
 {
    unsigned int sessionID;                            // <<in
    unsigned int noOfInputBlocks;                      // <<in
    unsigned int streamID;                             // <<in
    unsigned int encOffset;                            // <<in
    unsigned int streamCtr;                            // >>out
    unsigned char  reserved1[4];
    unsigned long long inputCtr;                             // >>out
    unsigned long long pesPriv[NV95A1_HDCP_SIZE_PES_HDR_64]; // >>out
    unsigned int retCode;                              // >>out
    unsigned char  reserved2[4];
 } hdcp_encrypt_param;
#define NV95A1_HDCP_ENCRYPT_ERROR_NONE                          NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_ENCRYPT_ERROR_INVALID_SESSION               NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_ENCRYPT_ERROR_SB_NOT_SET                    NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_ENCRYPT_ERROR_NOT_INIT                      NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_ENCRYPT_ERROR_INVALID_STAGE                 NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_ENCRYPT_ERROR_SESSION_NOT_ACTIVE            (0x00000005)
#define NV95A1_HDCP_ENCRYPT_ERROR_INPUT_BUFFER_NOT_SET          (0x00000006)
#define NV95A1_HDCP_ENCRYPT_ERROR_OUTPUT_BUFFER_NOT_SET         (0x00000007)
#define NV95A1_HDCP_ENCRYPT_ERROR_INVALID_STREAM                (0x00000008)
#define NV95A1_HDCP_ENCRYPT_ERROR_INVALID_ALIGN                 (0x00000009)
/*
 * GET_RTT_CHALLENGE
 *
 * This method works only for HDCP-2.1 receivers. This is part of Locality
 * check which will generate 256 bit L and sends least significant 128 (128l)bits to
 * the receiver.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet
 * INVALID_STAGE      - RTT challenge is requested in wrong stage
 * MSG_UNSUPPORTED    - Message applies only to HDCP2.1 receiver with
 *                      Pre-compute support
 *
 */
 typedef struct _hdcp_get_rtt_challenge_param
 {
    unsigned int sessionID;                            // <<in
    unsigned char  reserved1[4];
    unsigned long long L128l[NV95A1_HDCP_SIZE_LPRIME_64/2];  // <<out
    unsigned int retCode;                              // <<out
    unsigned char  reserved2[4];
 } hdcp_get_rtt_challenge_param;
#define NV95A1_HDCP_GET_RTT_CHALLENGE_ERROR_NONE                NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_GET_RTT_CHALLENGE_ERROR_INVALID_SESSION     NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_GET_RTT_CHALLENGE_ERROR_SB_NOT_SET          NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_GET_RTT_CHALLENGE_ERROR_NOT_INIT            NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_GET_RTT_CHALLENGE_ERROR_INVALID_STAGE       NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_GET_RTT_CHALLENGE_ERROR_MSG_UNSUPPORTED     NV95A1_HDCP_ERROR_MSG_UNSUPPORTED
/*
 * STREAM_MANAGE
 *
 * This method works only for HDCP-2.1 receivers. Helps in Stream_Manage and
 * Stream_Ready. The input contentID is expected to be in BIG-ENDIAN format.
 * The number of contentIDs and strTypes equals the noOfStreams passed while
 * creating the session. The output seqNumM and streamCtr will have valid
 * values only from index 0 to (noOfStreams-1). Rest will be invalid values.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet
 * INVALID_STAGE      - RTT challenge is requested in wrong stage
 * MSG_UNSUPPORTED    - Message applies only to HDCP2.1 receiver
 * MPRIME_VALD_FAILED - Mprime validation has failed
 * SEQ_NUM_M_ROLLOVER - Sequence number M has rolled over.
 *
 * FLAGS
 *
 * MANAGE  - Setting this flag will let TSEC return the stream counters
 *           for the video and audio streams associated with session ID.
 *           Only input needed is the sessionID.
 * READY   - Synonymous to STREAM_READY message. Setting this flag will let
 *           TSEC compute M and compare with Mprime. The inputs needed are
 *           contentID, strType (streamType) and Mprime
 */
 typedef struct _hdcp_stream_manage_param
 {
    unsigned long long mprime[NV95A1_HDCP_SIZE_MPRIME_64];         // <<in
    unsigned int sessionID;                                  // <<in
    unsigned int manageFlag;                                 // <<in
    unsigned char  contentID[NV95A1_HDCP_MAX_STREAMS_PER_RCVR]
                   [NV95A1_HDCP_SIZE_CONTENT_ID_8];   // <<in
    unsigned char  strType[NV95A1_HDCP_MAX_STREAMS_PER_RCVR]
                   [NV95A1_HDCP_SIZE_CONTENT_TYPE_8]; // <<in
    unsigned char  seqNumM[NV95A1_HDCP_SIZE_SEQ_NUM_M_8];      // <<out
    unsigned char  reserved1[7];
    unsigned int streamCtr[NV95A1_HDCP_MAX_STREAMS_PER_RCVR];// <<out
    unsigned int retCode;                                    // <<out
 } hdcp_stream_manage_param;
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_NONE                    NV95A1_HDCP_ERROR_NONE
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_INVALID_SESSION         NV95A1_HDCP_ERROR_INVALID_SESSION
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_SB_NOT_SET              NV95A1_HDCP_ERROR_SB_NOT_SET
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_NOT_INIT                NV95A1_HDCP_ERROR_NOT_INIT
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_INVALID_STAGE           NV95A1_HDCP_ERROR_INVALID_STAGE
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_MSG_UNSUPPORTED         NV95A1_HDCP_ERROR_MSG_UNSUPPORTED
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_MPRIME_VALD_FAILED      (0x00000006)
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_SEQ_NUM_M_ROLLOVER      (0x00000007)
#define NV95A1_HDCP_STREAM_MANAGE_ERROR_INVALID_FLAG            (0x00000008)
#define NV95A1_HDCP_STREAM_MANAGE_FLAG_MANAGE                   (0x00000001)
#define NV95A1_HDCP_STREAM_MANAGE_FLAG_READY                    (0x00000002)
/*
HDCP Glossary:
---------
HDCP Tsec Application  - An application running in TSEC which will handle HDCP
                         related methods
Client                 - An UMD or KMD component sending methods to HDCP Tsec
                         application to perform any HDCP operation.
Exclusive Dmem         - HDCP Tsec app needs to save persistent states. But
                         since TSEC OS is stateless, we need to wipe-out entire
                         DMEM before context switching to a different
                         application. So we either need to dump all the
                         persistent states to FB or request an exclusive DMEM
                         space that is owned only by HDCP Tsec application and
                         will not be wiped out at all.
Session                - Synonymous to a secure channel between GPU and the
                         wireless display. Number of simultaneous sessioins
                         equals the number of hdcp supported wireless displays
                         discovered.
LC128                  - A global constant provided by DCP LLC to all HDCP
                         adopters.
private key            - RSA private key
public key             - RSA publick key
cert                   - RSA certificate signed by DCP LLC
SRM                    - System renewability message
DCP                    - Digital Content Protection
SB                     - Scratch Buffer
*/
#endif
