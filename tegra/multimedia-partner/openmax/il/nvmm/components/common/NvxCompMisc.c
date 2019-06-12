/* Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <string.h>
#include <stdint.h>
#include "NVOMX_IndexExtensions.h"
#include "NvxCompMisc.h"

#include "nvassert.h"
#include "nvos.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */
#define ERROR_OK                (0)
#define ERROR_BAD_SYNC          (-1)
#define ERROR_BAD_FRAME         (-2)

#define PREAMBLE_SIZE_BYTES 8

typedef struct {
    OMX_STRING                  compName;
    NVX_H264_DECODE_INFO        supported;
} NVX_COMPONENT_CAP_H264_DEC;

NVX_COMPONENT_CAP_H264_DEC  Nvx_CompnentCapH264DecAP16[] = {
    { "OMX.Nvidia.h264.decode", { OMX_FALSE, OMX_FALSE, 1280, 720 } },
    { "OMX.Nvidia.h264.decode", { OMX_FALSE, OMX_TRUE ,  720, 480 } },
    { 0 },
};

NVX_COMPONENT_CAP_H264_DEC  Nvx_CompnentCapH264DecAP20[] = {
    { "OMX.Nvidia.h264.decode", { OMX_FALSE, OMX_FALSE, 1920, 1080 } },
    { "OMX.Nvidia.h264.decode", { OMX_FALSE, OMX_TRUE , 1280,  720 } },
    { 0 },
};

// read 1 byte from NALU, discard emulation_prevention_three_byte.
static OMX_U32 NVOMX_ReadByte1(NVX_OMX_NALU_BIT_STREAM *bitstm)
{
    OMX_U32 byte = 0;
    if (bitstm->currentPtr >= bitstm->dataEnd)
        return 0;

    byte = *(bitstm->currentPtr)++;

    if (byte == 0)
    {
        bitstm->zeroCount++;

        if (bitstm->zeroCount == 2 &&
            bitstm->currentPtr < bitstm->dataEnd &&
            *(bitstm->currentPtr) == 0x03)
        {
            bitstm->zeroCount = 0;
            bitstm->currentPtr++;
        }
    }
    else
    {
        bitstm->zeroCount = 0;
    }

    return byte;
}

// read 1 bit
static OMX_U32 NVOMX_ReadBit1(NVX_OMX_NALU_BIT_STREAM *bitstm)
{
    if (bitstm->bitCount == 0)
    {
        bitstm->currentBuffer  = NVOMX_ReadByte1(bitstm) << 24;
        bitstm->currentBuffer |= NVOMX_ReadByte1(bitstm) << 16;
        bitstm->currentBuffer |= NVOMX_ReadByte1(bitstm) <<  8;
        bitstm->currentBuffer |= NVOMX_ReadByte1(bitstm);
        bitstm->bitCount = 32;
    }
    bitstm->bitCount--;
    return ( (bitstm->currentBuffer << (31 - bitstm->bitCount)) >> 31);
}

// reads n bitstm
static OMX_U32 NVOMX_ReadBits(NVX_OMX_NALU_BIT_STREAM *bitstm, int count)
{
    OMX_U32 retval;

    if (count <= 0)
        return 0;

    if (count >= bitstm->bitCount)
    {
        retval = 0;
        if (bitstm->bitCount)
        {
            retval = ( (bitstm->currentBuffer << (32 - bitstm->bitCount)) >> (32 - count) );
            count -= bitstm->bitCount;
        }

        bitstm->currentBuffer  = NVOMX_ReadByte1(bitstm) << 24;
        bitstm->currentBuffer |= NVOMX_ReadByte1(bitstm) << 16;
        bitstm->currentBuffer |= NVOMX_ReadByte1(bitstm) <<  8;
        bitstm->currentBuffer |= NVOMX_ReadByte1(bitstm);
        bitstm->bitCount = 32 - count;

        if (count)
        {
            retval |= ( bitstm->currentBuffer >> (32 - count) );
        }
    }
    else
    {
        retval = ( (bitstm->currentBuffer << (32 - bitstm->bitCount)) >> (32 - count) );
        bitstm->bitCount -= count;
    }
    return retval;
}

// read ue(v)
static OMX_U32 NVOMX_ReadUE(NVX_OMX_NALU_BIT_STREAM *bitstm)
{
    OMX_U32 codeNum = 0;
    while (NVOMX_ReadBit1(bitstm) == 0 && codeNum < 32)
        codeNum++;

    return NVOMX_ReadBits(bitstm, codeNum) + ((1 << codeNum) - 1);
}

// read se(v)
static OMX_S32 NVOMX_ReadSE(NVX_OMX_NALU_BIT_STREAM *bitstm)
{
    OMX_U32 codeNum = NVOMX_ReadUE(bitstm);
    OMX_S32 sval = (codeNum + 1) >> 1;
    if (codeNum & 1)
        return sval;
    return -sval;
}

// read scaling list and discard it
static void NVOMX_ScalingList(NVX_OMX_NALU_BIT_STREAM *bitstm, int size)
{
    int id;
    int lastScale = 8;
    int nextScale = 8;
    for (id = 0; id < size; id++)
    {
        int delta_scale;
        if (nextScale != 0)
        {
            delta_scale = NVOMX_ReadSE(bitstm);
            nextScale = (lastScale + delta_scale + 256) % 256;
        }
        lastScale = (nextScale == 0) ? lastScale : nextScale;
    }
    return;
}

// parse sps
OMX_BOOL NVOMX_ParseSPS(OMX_U8* data, int len, NVX_OMX_SPS *pSPS)
{
    int id;
    OMX_U32 chroma_format_idc = 1;
    OMX_U32 pic_order_cnt_type;
    OMX_U32 pic_width_in_mbs_minus1;
    OMX_U32 pic_height_in_map_units_minus1;
    OMX_U32 frame_mbs_only_flag;

    NVX_OMX_NALU_BIT_STREAM nalu = {0};

    if (len <= 1 || !data)
        return OMX_FALSE;

    id = *data++;        // first byte nal_unit_type
    len--;

    if ( (id & 0x1f) != 7)
        return OMX_FALSE;

    nalu.data = data;
    nalu.size = len;
    nalu.dataEnd = data + len;
    nalu.currentPtr = data;

    pSPS->profile_idc = NVOMX_ReadBits(&nalu, 8);

    pSPS->constraint_set0123_flag = NVOMX_ReadBits(&nalu, 4);
    /*reserved_zero_4bits   = */ NVOMX_ReadBits(&nalu, 4);

    pSPS->level_idc = NVOMX_ReadBits(&nalu, 8);
    /*seq_parameter_set_id  = */ NVOMX_ReadUE(&nalu);

    if (66 != pSPS->profile_idc &&
        77 != pSPS->profile_idc &&
        88 != pSPS->profile_idc)
    {
        chroma_format_idc = NVOMX_ReadUE(&nalu);

        if(3 == chroma_format_idc)
        {
            /*residual_colour_transform_flag        = */ NVOMX_ReadBit1(&nalu);
        }
        /*bit_depth_luma_minus8                 = */ NVOMX_ReadUE(&nalu);
        /*bit_depth_chroma_minus8               = */ NVOMX_ReadUE(&nalu);
        /*qpprime_y_zero_transform_bypass_flag  = */ NVOMX_ReadBit1(&nalu);
        if (NVOMX_ReadBit1(&nalu))    // seq_scaling_matrix_present_flag
        {
            for (id = 0; id < 8; id++)
            {
                if (NVOMX_ReadBit1(&nalu)) // seq_scaling_list_present_flag
                {
                    if(id < 6)
                    {
                        NVOMX_ScalingList(&nalu, 16);
                    }
                    else
                    {
                        NVOMX_ScalingList(&nalu, 64);
                    }
                }
            }
        }
    }

    if (83 == pSPS->profile_idc /*AVCDEC_PROFILE_SCALABLE_0*/ ||
        86 == pSPS->profile_idc /*AVCDEC_PROFILE_SCALABLE_1*/ )
    {
        // some data here;
        return OMX_FALSE;
    }

    /*log2_max_frame_num_minus4 = */ NVOMX_ReadUE(&nalu);
    pic_order_cnt_type = NVOMX_ReadUE(&nalu);
    if (pic_order_cnt_type == 0)
    {
        /*log2_max_pic_order_cnt_lsb_minus4 = */ NVOMX_ReadUE(&nalu);
    }
    else if(pic_order_cnt_type == 1)
    {
        OMX_U32 num_ref_frames_in_pic_order_cnt_cycle;
        /*delta_pic_order_always_zero_flag = */ NVOMX_ReadBit1(&nalu);
        /*offset_for_non_ref_pic = */           NVOMX_ReadSE(&nalu);
        /*offset_for_top_to_bottom_field = */   NVOMX_ReadSE(&nalu);
        num_ref_frames_in_pic_order_cnt_cycle = NVOMX_ReadUE(&nalu);
        for (id = 0; id < (int)num_ref_frames_in_pic_order_cnt_cycle; id++)
        {
            /*offset_for_ref_frame = */ NVOMX_ReadSE(&nalu);
        }
    }
    /*num_ref_frames = */NVOMX_ReadUE(&nalu);
    /*gaps_in_frame_num_value_allowed_flag = */NVOMX_ReadBit1(&nalu);
    pic_width_in_mbs_minus1 = NVOMX_ReadUE(&nalu);
    pic_height_in_map_units_minus1 = NVOMX_ReadUE(&nalu);
    frame_mbs_only_flag = NVOMX_ReadBit1(&nalu);
    if (!frame_mbs_only_flag)
        /*mb_adaptive_frame_field_flag = */ NVOMX_ReadBit1(&nalu);
    /*direct_8x8_inference_flag = */ NVOMX_ReadBit1(&nalu);

    pSPS->nWidth = (pic_width_in_mbs_minus1 + 1) << 4;
    pSPS->nHeight = ((pic_height_in_map_units_minus1 + 1) * ((frame_mbs_only_flag)? 1 : 2)) << 4;

    if (NVOMX_ReadBit1(&nalu)) // frame_cropping_flag
    {
        /*frame_crop_left_offset   */ NVOMX_ReadUE(&nalu);
        /*frame_crop_right_offset  */ NVOMX_ReadUE(&nalu);
        /*frame_crop_top_offset    */ NVOMX_ReadUE(&nalu);
        /*frame_crop_bottom_offset */ NVOMX_ReadUE(&nalu);
    }

    /*vui_parameters_present_flag */ NVOMX_ReadBit1(&nalu);

    return OMX_TRUE;
}

// parse pps
static OMX_BOOL NVOMX_ParsePPS(OMX_U8* data, int len, NVX_OMX_PPS *pPPS)
{
    int id;
    NVX_OMX_NALU_BIT_STREAM nalu = {0};

    if (len <= 1 || !data)
        return OMX_FALSE;

    id = *data++;        // first byte nal_unit_type
    len--;

    if ( (id & 0x1f) != 8)
        return OMX_FALSE;

    nalu.data = data;
    nalu.size = len;
    nalu.dataEnd = data + len;
    nalu.currentPtr = data;

    /*pic_parameter_set_id = */ NVOMX_ReadUE(&nalu);
    /*seq_parameter_set_id = */ NVOMX_ReadUE(&nalu);

    pPPS->entropy_coding_mode_flag = NVOMX_ReadBit1(&nalu);

    // more parsing to follow
    return OMX_TRUE;
}

static OMX_STRING NVOMX_FindCompH264(NVX_STREAM_PLATFORM_INFO *pStreamInfo)
{
    NVX_H264_DECODE_INFO *pH264Info = &pStreamInfo->streamInfo.h264;
    NVX_COMPONENT_CAP_H264_DEC *pCompCapH264 = Nvx_CompnentCapH264DecAP16;

    if (pH264Info->bUseSPSAndPPS)
    {
        int i;
        pH264Info->bHasCABAC = OMX_FALSE;
        pH264Info->nWidth = 0;
        pH264Info->nHeight = 0;
        for (i = 0; i < (int)pH264Info->nSPSCount; i++)
        {
            NVX_OMX_SPS sps = {0};
            if (OMX_FALSE == NVOMX_ParseSPS(pH264Info->SPSNAUL[i], pH264Info->SPSNAULLen[i], &sps))
                return NULL;
            if (sps.profile_idc != 66 && sps.profile_idc != 77 &&
                !(sps.constraint_set0123_flag & 8) &&
                !(sps.constraint_set0123_flag & 4))
            {
                return NULL;
            }
            if (sps.nWidth > pH264Info->nWidth)
                pH264Info->nWidth = sps.nWidth;
            if (sps.nHeight > pH264Info->nHeight)
                pH264Info->nHeight = sps.nHeight;
        }
        for (i = 0; i < (int)pH264Info->nPPSCount; i++)
        {
            NVX_OMX_PPS pps = {0};
            if (OMX_FALSE == NVOMX_ParsePPS(pH264Info->PPSNAUL[i], pH264Info->PPSNAULLen[i], &pps))
                return NULL;
            if (pps.entropy_coding_mode_flag)
            {
                pH264Info->bHasCABAC = OMX_TRUE;
                break;
            }
        }
    }

    if (pStreamInfo->nPlatform == 1)
    {
        pCompCapH264 = Nvx_CompnentCapH264DecAP20;
    }

    while (pCompCapH264->compName)
    {
        NVX_H264_DECODE_INFO *pCompCap = &pCompCapH264->supported;

        if ( ((pH264Info->bHasCABAC && pCompCap->bHasCABAC) || !pH264Info->bHasCABAC) &&
            pCompCap->nWidth >= pH264Info->nWidth &&
            pCompCap->nHeight >= pH264Info->nHeight)
        {
            return pCompCapH264->compName;
        }

        pCompCapH264++;
    }

    return NULL;
}

OMX_API OMX_ERRORTYPE NVOMX_FindComponentName(
    OMX_IN  NVX_STREAM_PLATFORM_INFO *pStreamInfo,
    OMX_OUT OMX_STRING  *compName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    *compName = NULL;

    switch (pStreamInfo->eStreamType)
    {
    case NvxStreamType_H264:
        *compName = NVOMX_FindCompH264(pStreamInfo);
        break;

    default:
        break;
    }

    return eError;
}

/* =========================================================================
 * Bit parsing functions
 * ========================================================================= */
#define CBB_Get(mbp, bits)    (read_mod_buf(mbp, bits))

static void init_read_mod_buf(BYPASS_MOD_BUF_PTR *mbp, unsigned int *buffer, int size)
{
    mbp->validbits = 0;
    mbp->bit_count  = 0;
    mbp->word = 0;
    mbp->word2 = (*buffer << 24) | ((*buffer & 0xff00) << 8) | ((*buffer >> 8) & 0xff00) | (*buffer >> 24);
    mbp->wordPtr = buffer + 1;
}

/****************************************************************************
***
***    read_mod_buf()
***
***    reads 'no_of_bits' bits from the bitstream buffer 'mbp'
***    in the variable 'value'
***
***    M.Dietz     FhG IIS/INEL
***
****************************************************************************/
static unsigned int read_mod_buf(BYPASS_MOD_BUF_PTR *mbp, unsigned int no_of_bits )
{
    unsigned int value;
    unsigned int tmp;

    if (no_of_bits == 0)
    {
        return(0);
    }

    mbp->bit_count += no_of_bits;

    if(no_of_bits <= mbp->validbits)
    {
        value = (mbp->word >> (32 - no_of_bits));
        mbp->validbits -= no_of_bits ;
        mbp->word <<= no_of_bits;
        return (value);
    }

    no_of_bits = 32 - no_of_bits;
    value = (mbp->word | (mbp->word2 >> mbp->validbits)) >> no_of_bits;
    mbp->validbits += no_of_bits ;
    mbp->word = mbp->word2 << (32 -  mbp->validbits);

    tmp = *mbp->wordPtr ++;
    mbp->word2 = ((tmp << 24) | ((tmp & 0xff00)<<8) | ((tmp >> 8) & 0xff00) | (tmp >> 24));
    return(value);
}

/* =========================================================================
 * AC3 Parsing functions
 * ========================================================================= */

static const struct {
    const uint32_t bitrate;
    const uint32_t framesize[3];
} framesize_table[38] = {
    { 32,    { 64, 69, 96}},
    { 32,    { 64, 70, 96}},
    { 40,    { 80, 87, 120}},
    { 40,    { 80, 88, 120}},
    { 48,    { 96, 104, 144}},
    { 48,    { 96, 105, 144}},
    { 56,    { 112, 121, 168}},
    { 56,    { 112, 122, 168}},
    { 64,    { 128, 139, 192}},
    { 64,    { 128, 140, 192}},
    { 80,    { 160, 174, 240}},
    { 80,    { 160, 175, 240}},
    { 96,    { 192, 208, 288}},
    { 96,    { 192, 209, 288}},
    { 112,   { 224, 243, 336}},
    { 112,   { 224, 244, 336}},
    { 128,   { 256, 278, 384}},
    { 128,   { 256, 279, 384}},
    { 160,   { 320, 348, 480}},
    { 160,   { 320, 349, 480}},
    { 192,   { 384, 417, 576}},
    { 192,   { 384, 418, 576}},
    { 224,   { 448, 487, 672}},
    { 224,   { 448, 488, 672}},
    { 256,   { 512, 557, 768}},
    { 256,   { 512, 558, 768}},
    { 320,   { 640, 696, 960}},
    { 320,   { 640, 697, 960}},
    { 384,   { 768, 835, 1152}},
    { 384,   { 768, 836, 1152}},
    { 448,   { 896, 975, 1344}},
    { 448,   { 896, 976, 1344}},
    { 512,   { 1024, 1114, 1536}},
    { 512,   { 1024, 1115, 1536}},
    { 576,   { 1152, 1253, 1728}},
    { 576,   { 1152, 1254, 1728}},
    { 640,   { 1280, 1393, 1920}},
    { 640,   { 1280, 1394, 1920}}
};

static const uint32_t samplerates[4] = { 48000, 44100, 32000, 0 };
static const uint32_t acmod_channels[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };
static const uint32_t numblks[4] = { 1, 2, 3, 6 };

static OMX_BOOL ParseAC3Frame(NVX_AUDIO_DECODE_DATA *pContext,
                            uint8_t* inptr,
                            size_t inputsize,
                            int32_t* framesize,
                            int32_t* rate,
                            int32_t* ch,
                            int32_t* blk,
                            int32_t* bsmod)
{
    int32_t lfe_on = 0;
    int32_t bmod = 0;
    int32_t acmod = 0;
    uint8_t fscod;
    uint8_t frmsizecod;

    init_read_mod_buf(&pContext->mbp , (unsigned int *)inptr, inputsize);

    CBB_Get(&pContext->mbp, 32); // skip processed data

    fscod = CBB_Get(&pContext->mbp, 2);
    frmsizecod = CBB_Get(&pContext->mbp, 6);

    if (fscod == 3)
    {
        NvOsDebugPrintf("BypassDecoder: invalid fscod %d\n" , fscod);
        return OMX_FALSE;
    }

    if (frmsizecod >= 38)
    {
        NvOsDebugPrintf("BypassDecoder: invalid frmsizecod %d\n" , frmsizecod);
        return OMX_FALSE;
    }

    CBB_Get(&pContext->mbp, 5); // skip bsid
    bmod = CBB_Get(&pContext->mbp, 3);

    acmod = CBB_Get(&pContext->mbp, 3);
    if ((acmod & 0x1) && (acmod != 0x1))
    {
        // if 3 front channels, skip 2 bits
        CBB_Get(&pContext->mbp, 2);
    }
    if (acmod & 0x4)
    {
        // if a surround channel exists, skip 2 bits
        CBB_Get(&pContext->mbp, 2);
    }
    if (acmod == 0x2)
    {
        // if in 2/0 mode, skip 2 bits
        CBB_Get(&pContext->mbp, 2);
    }

    lfe_on = CBB_Get(&pContext->mbp, 1);

    //NvOsDebugPrintf("AC3 frame: framesize %d bytes  rate %d  channels %d  nblocks %d \n",
    //   framesize_table[frmsizecod].framesize[fscod] * 2, samplerates[fscod],
    //   acmod_channels[acmod] + lfe_on, 6);

    if(framesize)
        *framesize = framesize_table[frmsizecod].framesize[fscod] * 2;

    if(rate)
        *rate = samplerates[fscod];

    if(ch)
        *ch = acmod_channels[acmod] + lfe_on;

    if(blk)
        *blk = 6;

    if (bsmod)
        *bsmod = bmod;

    return OMX_TRUE;
}

static OMX_BOOL ParseEAC3Frame(NVX_AUDIO_DECODE_DATA *pContext,
                            uint8_t * inptr,
                            size_t inputsize,
                            int32_t* framesize,
                            int32_t* rate,
                            int32_t* ch,
                            int32_t* blk,
                            int32_t* bsmod)
{
    uint8_t lfe_on = 0;
    uint8_t acmod = 0;
    uint8_t fscod2, nblks, numblkcode, strmtype;
    uint8_t fscod;
    uint16_t lframesize;
    uint32_t samplerate;

    init_read_mod_buf(&pContext->mbp , (unsigned int *)inptr, inputsize);

    CBB_Get(&pContext->mbp, 16); // skip 16 bits
    strmtype = CBB_Get(&pContext->mbp, 2);

    if (strmtype == 3)
    {
        NvOsDebugPrintf("BypassDecoder: invalid sreamtype %d in EAC3\n", strmtype);
        return OMX_FALSE;
    }

    if (strmtype == 1)
    {
        NvOsDebugPrintf("BypassDecoder: ignoring dependent sreamtype %d in EAC3\n", strmtype);
        return OMX_FALSE;
    }

    CBB_Get(&pContext->mbp, 3);
    lframesize = CBB_Get(&pContext->mbp, 11);
    fscod = CBB_Get(&pContext->mbp, 2);
    if (fscod == 3)
    {
        fscod2 = CBB_Get(&pContext->mbp, 2);
        if (fscod2 == 3)
        {
            NvOsDebugPrintf("BypassDecoder: invalid fscod2 : %d\n", fscod2);
            return OMX_FALSE;
        }
        samplerate = samplerates[fscod2] / 2;
        nblks = 6;
    }
    else
    {
        numblkcode = CBB_Get(&pContext->mbp, 2);
        samplerate = samplerates[fscod];
        nblks = numblks[numblkcode];
    }

    acmod = CBB_Get(&pContext->mbp, 3);
    lfe_on = CBB_Get(&pContext->mbp, 1);

    //NvOsDebugPrintf("BypassDecoder: EAC3 frame: framesize %d bytes  rate %d  channels %d  nblocks %d \n",
    //  (lframesize + 1) * 2 , samplerate, acmod_channels[acmod] + lfe_on, nblks);

    if (framesize)
        *framesize = (lframesize + 1) * 2;

    if (rate)
        *rate = samplerate;

    if (ch)
        *ch = acmod_channels[acmod] + lfe_on;

    if (blk)
        *blk = nblks;

    if (bsmod)
        *bsmod = 0; // TODO : update bsmod from stream

    return OMX_TRUE;
}

static OMX_S32 parseAC3(NVX_AUDIO_DECODE_DATA *pContext, /* in/out */
                    const OMX_U8 *inptr, /* in */
                    OMX_S32 inputSize, /* in */
                    OMX_S32 *outputSize, /* out */
                    OMX_S32 *bsmod, /* out */
                    OMX_BOOL *needByteSwap, /* out */
                    OMX_S32 *rate, /* out */
                    OMX_S32 *channels, /* out */
                    OMX_S32 *framesize) /* out */
{
    OMX_S32 blk;
    OMX_U8 header[10];
    OMX_U8 bsid;
    uint16_t syncword;

    if (inputSize < 10)
    {
        NvOsDebugPrintf("BypassDecoder: incomplete AC3 frame\n");
        return ERROR_BAD_FRAME;
    }

    NvOsMemcpy(header, inptr, sizeof(header));

    syncword = (header[0] << 8) | header[1];

    if (syncword == 0x0B77)
    {
        *needByteSwap = OMX_TRUE;
    }
    else if (syncword == 0x770B)
    {
        *needByteSwap = OMX_FALSE;
    }
    else
    {
        NvOsDebugPrintf("BypassDecoder: invalid sync word 0x%x\n", syncword);
        return ERROR_BAD_SYNC;
    }

    if (!(*needByteSwap))
    {
        OMX_U8 temp;
        OMX_U32 i;
        for (i=0; i<sizeof(header); i+=2)
        {
            temp = header[i];
            header[i] = header[i+1];
            header[i+1] = temp;
        }
    }

    init_read_mod_buf(&pContext->mbp , (unsigned int *)header, inputSize);

    CBB_Get(&pContext->mbp, 16); // skip syncword
    CBB_Get(&pContext->mbp, 24); // skep CRC + fscod + frmsizecod
    bsid = CBB_Get(&pContext->mbp, 5);;

    if (bsid <= 10)
    {
        // parse ac3 frame header
        if(!ParseAC3Frame(pContext, header, inputSize, (int32_t*)framesize,
                          (int32_t*)rate, (int32_t*)channels, (int32_t*)&blk, (int32_t*)bsmod))
            return ERROR_BAD_FRAME;
    }
    else if (bsid <= 16)
    {
        // parse the eac3 frame header
        if(!ParseEAC3Frame(pContext, header, inputSize, (int32_t*)framesize,
                           (int32_t*)rate, (int32_t*)channels, (int32_t*)&blk, (int32_t*)bsmod))
            return ERROR_BAD_FRAME;
    }
    else
    {
        //NvOsDebugPrintf("BypassDecoder: Invalid bsid %d\n", bsid);
        return ERROR_BAD_FRAME;
    }

    *outputSize = 2 * 256 * blk * 2;

    return ERROR_OK;
}

static void packAC3Frame(const uint8_t *inpptr8,
                        uint8_t *outptr8,
                        uint32_t framesize,
                        uint32_t outputsize,
                        uint8_t bsmod,
                        OMX_BOOL byteSwap)
{
    uint16_t *outptr16 = (uint16_t *)outptr8;

    // AC3 packing as per IEC61937 2000
    outptr16[0] = 0xF872; // Pa
    outptr16[1] = 0x4E1F; // Pb

    // Pc = 0000 0000 0000 0001
    outptr16[2] = (bsmod << 9) | 0x1;

    // Pd = payload length in bits
    outptr16[3] = framesize * 8;

    if (byteSwap)
    {
        // copy encoded frame with byte swapping
        uint32_t i;
        uint8_t *dst = outptr8 + PREAMBLE_SIZE_BYTES;
        for (i=0; i<framesize; i+=2)
        {
            dst[i] = inpptr8[i+1];
            dst[i+1] = inpptr8[i];
        }
    }
    else
    {
        // copy encoded frame as it is
        NvOsMemcpy(outptr8 + PREAMBLE_SIZE_BYTES, inpptr8,framesize);
    }

    // stuff zeros
    NvOsMemset(outptr8 + PREAMBLE_SIZE_BYTES + framesize,
                0,
                outputsize - (PREAMBLE_SIZE_BYTES + framesize));
}


void NvxBypassProcessAC3(NVX_AUDIO_DECODE_DATA *pContext,
                         OMX_AUDIO_CODINGTYPE outputCodingType,
                         OMX_U8 *inptr8,
                         OMX_U32 inputSize,
                         OMX_U8 *outptr8,
                         OMX_U32 outputSize)
{
    OMX_S32 error;
    OMX_S32 totalOutputSize, bsmod, sampleRate, channels, framesize;
    OMX_BOOL needByteSwap;

    totalOutputSize = bsmod = sampleRate = channels = framesize = 0;
    needByteSwap = OMX_FALSE;

    error = parseAC3(pContext,
                        inptr8,
                        inputSize,
                        &totalOutputSize,
                        &bsmod,
                        &needByteSwap,
                        &sampleRate,
                        &channels,
                        &framesize);
    if (ERROR_OK != error)
    {
        NvOsDebugPrintf("BypassDecoder: Invalid AC3 frame, skipping\n");
        pContext->outputBytes = 0;
        pContext->bytesConsumed = inputSize;
        pContext->bFrameOK = OMX_FALSE;
    }
    else
    {
        //NvOsDebugPrintf("BypassDecoder: found AC3 frame: framesize: %d rate: %d channels: %d bsmod: %d needByteSwap: %d",
        //            framesize, sampleRate, channels, bsmod, needByteSwap);

        if (OMX_AUDIO_CodingPCM != outputCodingType)
        {
            packAC3Frame(inptr8, outptr8, framesize, totalOutputSize, bsmod, needByteSwap);
        }
        else
        {
            NvOsMemset(outptr8, 0, totalOutputSize);
        }

        //NvOsDebugPrintf("BypassDecoder: pContext->OutputAudioFormat %d\n", pContext->OutputAudioFormat);

        pContext->nSampleRate = sampleRate;
        pContext->nChannels = 2;
        pContext->outputBytes = totalOutputSize;
        pContext->bytesConsumed = framesize;
        pContext->bFrameOK = OMX_TRUE;
    }
}


/* =========================================================================
 * DTS Parsing functions
 * ========================================================================= */

static const int dts_rate[16] =
{
  0,
  8000,
  16000,
  32000,
  64000,
  128000,
  11025,
  22050,
  44100,
  88200,
  176400,
  12000,
  24000,
  48000,
  96000,
  192000
};

static OMX_S32 parseDTS(NVX_AUDIO_DECODE_DATA *pContext, /* in/out */
                    const OMX_U8 *inptr, /* in */
                    OMX_S32 inputSize, /* in */
                    OMX_S32 *outputSize, /* out */
                    OMX_S32 *nsamples, /* out */
                    OMX_BOOL *needByteSwap, /* out */
                    OMX_BOOL *isFormat14bit, /* out */
                    OMX_S32 *rate, /* out */
                    OMX_S32 *channels, /* out */
                    OMX_S32 *framesize) /* out */
{
    OMX_U32 syncword, ftype, nblks, srate_code;
    OMX_U8 header[12];

    if (inputSize < 12)
    {
        NvOsDebugPrintf("BypassDecoder: incomplete DTS frame\n");
        return ERROR_BAD_FRAME;
    }

    NvOsMemcpy(header, inptr, sizeof(header));

    syncword = header[0] << 24 | header[1] << 16 |
                        header[2] << 8 | header[3];

    switch (syncword)
    {
    /* 14 bits LE */
    case 0xff1f00e8:
        /* frame type should be 1 (normal) */
        if ((header[4]&0xf0) != 0xf0 || header[5] != 0x07)
            return -1;
        *isFormat14bit = OMX_TRUE;
        *needByteSwap = OMX_FALSE;
        break;

    /* 14 bits BE */
    case 0x1fffe800:
        /* frame type should be 1 (normal) */
        if (header[4] != 0x07 || (header[5]&0xf0) != 0xf0)
            return -1;
        *isFormat14bit = OMX_TRUE;
        *needByteSwap = OMX_TRUE;
        break;

    /* 16 bits LE */
    case 0xfe7f0180:
        *isFormat14bit = OMX_FALSE;
        *needByteSwap = OMX_FALSE;
        break;

    /* 16 bits BE */
    case 0x7ffe8001:
        *isFormat14bit = OMX_FALSE;
        *needByteSwap = OMX_TRUE;
        break;

    default:
        NvOsDebugPrintf("BypassDecoder: invalid sync word 0x%x\n", syncword);
        return ERROR_BAD_SYNC;
    }

    if (!(*needByteSwap))
    {
        OMX_U8 temp;
        OMX_U32 i;
        for (i=0; i<sizeof(header); i+=2)
        {
            temp = header[i];
            header[i] = header[i+1];
            header[i+1] = temp;
        }
    }

    init_read_mod_buf(&pContext->mbp , (unsigned int *)header, inputSize);

    if (*isFormat14bit)
    {
        NvOsDebugPrintf("DTS: 14-bit format not supported\n");
        return ERROR_BAD_FRAME;
    }
    else
    {
        CBB_Get(&pContext->mbp, 32); // skip syncword
        ftype = CBB_Get(&pContext->mbp, 1); // frame type

        if (ftype != 1)
        {
            NvOsDebugPrintf("DTS: skipping termination frame\n");
            return ERROR_BAD_FRAME;
        }

        CBB_Get(&pContext->mbp, 5); // skip deficit sample count
        CBB_Get(&pContext->mbp, 1); // skip CRC bit

        nblks = CBB_Get(&pContext->mbp, 7);
        nblks++;
        if(nblks != 8 && nblks != 16 && nblks != 32 && nblks != 64 &&
            nblks != 128)
        {
            NvOsDebugPrintf("DTS: invalid nblks %d \n", nblks);
            return ERROR_BAD_FRAME;
        }
        *nsamples = nblks * 32;

        *framesize = CBB_Get(&pContext->mbp, 14);
        if (*framesize < 95 || *framesize > 8191)
        {
            NvOsDebugPrintf("DTS: invalid fsize %d \n", *framesize);
            return ERROR_BAD_FRAME;
        }
        (*framesize)++;

        if (*framesize > inputSize)
        {
            NvOsDebugPrintf("DTS: incomplete frame framesize %d inputsize %d\n",
                                *framesize, inputSize);
            return ERROR_BAD_FRAME;
        }

        CBB_Get(&pContext->mbp, 6); // skip AMODE bits

        srate_code = CBB_Get(&pContext->mbp, 4);

        if(srate_code >= 1 && srate_code <= 15)
            *rate = dts_rate[srate_code];
        else
            *rate = 0;

        *channels = 2;
    }

    *outputSize = (*nsamples) * 2 * 2;

    return ERROR_OK;
}

static void packDTSFrame(const uint8_t *inpptr8,
                        uint8_t *outptr8,
                        uint32_t framesize,
                        uint32_t outputsize,
                        int32_t nsamples,
                        OMX_BOOL byteSwap)
{
    uint8_t *dst = outptr8 + PREAMBLE_SIZE_BYTES;
    uint16_t *outptr16 = (uint16_t *)outptr8;

    // DTS packing as per IEC61937 2000
    outptr16[0] = 0xF872; // Pa
    outptr16[1] = 0x4E1F; // Pb

    // Pc
    switch(nsamples)
    {
    case 512:
        outptr16[2] = 0x000b;      /* DTS-1 (512-sample bursts) */
        break;
    case 1024:
        outptr16[2] = 0x000c;      /* DTS-2 (1024-sample bursts) */
        break;
    case 2048:
        outptr16[2] = 0x000d;      /* DTS-3 (2048-sample bursts) */
        break;
    default:
        NvOsDebugPrintf("DTS: %d sample bursts not supported\n", nsamples);
        outptr16[2] = 0x0000;
        break;
    }

    // Pd = payload length in bits. Aligned to 2 bytes.
    outptr16[3] = ((framesize + 0x2) & ~0x1) * 8;

    if (byteSwap)
    {
        // copy encoded frame with byte swapping
        uint32_t i;

        for (i=0; i<(framesize & ~1); i+=2)
        {
            dst[i] = inpptr8[i+1];
            dst[i+1] = inpptr8[i];
        }

        if (framesize & 1)
        {
            dst[framesize-1] = 0;
            dst[framesize] = inpptr8[framesize-1];
            framesize++;
        }
    }
    else
    {
        // copy encoded frame as it is
        NvOsMemcpy(outptr8 + PREAMBLE_SIZE_BYTES, inpptr8, framesize);
    }

    // stuff zeros
    NvOsMemset(outptr8 + PREAMBLE_SIZE_BYTES + framesize,
                0,
                outputsize - (PREAMBLE_SIZE_BYTES + framesize));
}

void NvxBypassProcessDTS(NVX_AUDIO_DECODE_DATA *pContext,
                         OMX_AUDIO_CODINGTYPE outputCodingType,
                         OMX_U8 *inptr8,
                         OMX_U32 inputSize,
                         OMX_U8 *outptr8,
                         OMX_U32 outputSize)
{
    OMX_S32 error;
    OMX_S32 totalOutputSize, sampleRate, channels, framesize, nsamples;
    OMX_BOOL needByteSwap, isFormat14bit;

    totalOutputSize = sampleRate = channels = framesize = nsamples = 0;
    needByteSwap = isFormat14bit = OMX_FALSE;

    error = parseDTS(pContext,
                        inptr8,
                        inputSize,
                        &totalOutputSize,
                        &nsamples,
                        &needByteSwap,
                        &isFormat14bit,
                        &sampleRate,
                        &channels,
                        &framesize);

    if (ERROR_OK != error)
    {
        NvOsDebugPrintf("BypassDecoder: Invalid DTS frame, skipping\n");
        if (ERROR_BAD_SYNC == error)
        {
            pContext->outputBytes = 0;
            pContext->bytesConsumed = 4; // consume syncword
        }
        else
        {
            pContext->outputBytes = 0;
            pContext->bytesConsumed = inputSize;
        }
        pContext->bFrameOK = OMX_FALSE;
    }
    else
    {
        //NvOsDebugPrintf("BypassDecoder: found DTS frame: framesize: %d rate: %d channels: %d nsamples: %d needByteSwap: %d",
          //          framesize, sampleRate, channels, nsamples, needByteSwap);

        if (OMX_AUDIO_CodingPCM != outputCodingType)
        {
            packDTSFrame(inptr8, outptr8, framesize, totalOutputSize, nsamples, needByteSwap);
        }
        else
        {
            NvOsMemset(outptr8, 0, totalOutputSize);
        }

        //NvOsDebugPrintf("BypassDecoder: pContext->OutputAudioFormat %d\n", pContext->OutputAudioFormat);

        pContext->nSampleRate = sampleRate;
        pContext->nChannels = 2;
        pContext->outputBytes = totalOutputSize;
        pContext->bytesConsumed = framesize;
        pContext->bFrameOK = OMX_TRUE;
    }
}



