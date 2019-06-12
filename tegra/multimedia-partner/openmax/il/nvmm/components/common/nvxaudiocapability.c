/* Copyright (c) 2010-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "OMX_Core.h"
#include "OMX_Audio.h"
#include "nvmmlite_aac.h"
#include "nvmm_wma.h"
#include "nvmmlite_mp3.h"
#include "nvmm_amr.h"


#define MAX_AAC_FORMATS        8
#define MAX_WMA_FORMATS        5
#define MAX_MP3_FORMATS        4
#define MAX_AMR_FORMATS        7
#define MAX_AAC_MODES          5
#define MAX_MP3_MODES          5
#define MAX_AAC_PROFILES       11
#define MAX_WMA_PROFILES       15

void NvxConvertProfileModeFormatToOmx(NvMMBlockType oBlockType, NvU32 eProfile,
                                                                OMX_U32 *eOmxProfile, NvU32 eMode, OMX_U32 *eOmxMode,
                                                                NvU32 eFormat, OMX_U32 *eOmxFormat);
static const OMX_U32 NvxAACNvMMOmxFormatMap[MAX_AAC_FORMATS][2] =
{
    {OMX_AUDIO_AACStreamFormatMP2ADTS , NvMMLiteAudio_AACStreamFormatMP2ADTS },
    {OMX_AUDIO_AACStreamFormatMP4ADTS , NvMMLiteAudio_AACStreamFormatMP4ADTS },
    {OMX_AUDIO_AACStreamFormatMP4LOAS , NvMMLiteAudio_AACStreamFormatMP4LOAS },
    {OMX_AUDIO_AACStreamFormatMP4LATM , NvMMLiteAudio_AACStreamFormatMP4LATM },
    {OMX_AUDIO_AACStreamFormatADIF    , NvMMLiteAudio_AACStreamFormatADIF    },
    {OMX_AUDIO_AACStreamFormatMP4FF   , NvMMLiteAudio_AACStreamFormatMP4FF   },
    {OMX_AUDIO_AACStreamFormatRAW     , NvMMLiteAudio_AACStreamFormatRAW     },
    {OMX_AUDIO_AACStreamFormatMax     , NvMMLiteAudio_AACStreamFormatUnknown },
};

static const OMX_U32 NvxAACNvMMOmxModeMap[MAX_AAC_MODES][2] =
{
    {OMX_AUDIO_ChannelModeStereo      , NvMMLiteAudio_ChannelModeStereo      },
    {OMX_AUDIO_ChannelModeJointStereo , NvMMLiteAudio_ChannelModeJointStereo },
    {OMX_AUDIO_ChannelModeDual        , NvMMLiteAudio_ChannelModeDual        },
    {OMX_AUDIO_ChannelModeMono        , NvMMLiteAudio_ChannelModeMono        },
    {OMX_AUDIO_ChannelModeMax         , NvMMLiteAudio_ChannelModeUnknown     },
};

static const OMX_U32 NvxAACNvMMOmxProfileMap[MAX_AAC_PROFILES][2] =
{
    {OMX_AUDIO_AACObjectMain     ,  NvMMLiteAudio_AACObjectMain    },
    {OMX_AUDIO_AACObjectLC       ,  NvMMLiteAudio_AACObjectLC      },
    {OMX_AUDIO_AACObjectSSR      ,  NvMMLiteAudio_AACObjectSSR     },
    {OMX_AUDIO_AACObjectLTP      ,  NvMMLiteAudio_AACObjectLTP     },
    {OMX_AUDIO_AACObjectHE       ,  NvMMLiteAudio_AACObjectHE      },
    {OMX_AUDIO_AACObjectScalable , NvMMLiteAudio_AACObjectScalable },
    {OMX_AUDIO_AACObjectERLC     , NvMMLiteAudio_AACObjectERLC     },
    {OMX_AUDIO_AACObjectLD       , NvMMLiteAudio_AACObjectLD       },
    {OMX_AUDIO_AACObjectHE_PS    , NvMMLiteAudio_AACObjectHE_PS    },
    {OMX_AUDIO_AACObjectMax      , NvMMLiteAudio_AACObjectHE_MPS   },
    {OMX_AUDIO_AACObjectMax      , NvMMLiteAudio_AACObjectUnknown  },
};

static const OMX_U32 NvxWMANvMMOmxFormatMap[MAX_WMA_FORMATS][2] =
{
    {OMX_AUDIO_WMAFormat7   , Nv_AUDIO_WMAFormat7  },
    {OMX_AUDIO_WMAFormat8   , Nv_AUDIO_WMAFormat8  },
    {OMX_AUDIO_WMAFormat9   , Nv_AUDIO_WMAFormat9  },
    {OMX_AUDIO_WMAFormatMax , Nv_AUDIO_WMAFormat10 },
    {OMX_AUDIO_WMAFormatMax , Nv_AUDIO_WMAFormatMax},
};

static const OMX_U32 NvxWMANvMMOmxProfileMap[MAX_WMA_PROFILES][2] =
{
    {OMX_AUDIO_WMAProfileL1  , Nv_AUDIO_WMAProfileL1 },
    {OMX_AUDIO_WMAProfileL2  , Nv_AUDIO_WMAProfileL2 },
    {OMX_AUDIO_WMAProfileL3  , Nv_AUDIO_WMAProfileL3 },
	/*Wma Profiles L,M0a,M0b,M1,M2,M3,M,N1,N2,N3,N not present in Omx Structure so map them to default case*/
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileL  },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileM0a },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileM0b },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileM1  },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileM2  },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileM3  },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileM   },/*Wma-Pro Profiles*/
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileN1  },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileN2  },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileN3  },
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileN   },/*Wma LSL Profile*/
    {OMX_AUDIO_WMAProfileMax , Nv_AUDIO_WMAProfileMax },
};

static const OMX_U32 NvxMP3NvMMOmxFormatMap[MAX_MP3_FORMATS][2] =
{
    {OMX_AUDIO_MP3StreamFormatMP1Layer3   , Nv_AUDIO_MP3StreamFormatMP1Layer3   },
    {OMX_AUDIO_MP3StreamFormatMP2Layer3   , Nv_AUDIO_MP3StreamFormatMP2Layer3   },
    {OMX_AUDIO_MP3StreamFormatMP2_5Layer3 , Nv_AUDIO_MP3StreamFormatMP2_5Layer3 },
    {OMX_AUDIO_MP3StreamFormatMax         , Nv_AUDIO_MP3StreamFormatMax         },
};

static const OMX_U32 NvxMP3NvMMOmxModeMap[MAX_MP3_MODES][2] =
{
    {OMX_AUDIO_ChannelModeStereo      , Nv_AUDIO_ChannelModeStereo      },
    {OMX_AUDIO_ChannelModeJointStereo , Nv_AUDIO_ChannelModeJointStereo },
    {OMX_AUDIO_ChannelModeDual        , Nv_AUDIO_ChannelModeDual        },
    {OMX_AUDIO_ChannelModeMono        , Nv_AUDIO_ChannelModeMono        },
    {OMX_AUDIO_ChannelModeMax         , Nv_AUDIO_ChannelModeMax         },
};

static const OMX_U32 NvxAMRNvMMOmxFormatMap[MAX_AMR_FORMATS][2] =
{
    {OMX_AUDIO_AMRFrameFormatConformance , NvMM_AMRFrameFormatConformance },
    {OMX_AUDIO_AMRFrameFormatIF1         , NvMM_AMRFrameFormatIF1         },
    {OMX_AUDIO_AMRFrameFormatIF2         , NvMM_AMRFrameFormatIF2         },
    {OMX_AUDIO_AMRFrameFormatFSF         , NvMM_AMRFrameFormatFSF         },
    {OMX_AUDIO_AMRFrameFormatRTPPayload  , NvMM_AMRFrameFormatRTPPayload  },
    {OMX_AUDIO_AMRFrameFormatITU         , NvMM_AMRFrameFormatITU         },
    {OMX_AUDIO_AMRFrameFormatMax         , NvMM_AMRFrameFormatMax          },
};

void NvxConvertProfileModeFormatToOmx(NvMMBlockType oBlockType, NvU32 eProfile, OMX_U32 *eOmxProfile,
                                                            NvU32 eMode, OMX_U32 *eOmxMode, NvU32 eFormat, OMX_U32 *eOmxFormat)
{
    OMX_U32 *pProfileMap = NULL;
    OMX_U8  nMaxProfiles = 0;

    OMX_U32 *pModeMap = NULL;
    OMX_U8  nMaxModes = 0;

    OMX_U32 *pFormatMap = NULL;
    OMX_U8  nMaxFormats = 0;

    OMX_U8 i = 0;

    switch(oBlockType)
    {
        case NvMMBlockType_DecAAC:
        case NvMMBlockType_DecEAACPLUS:
        {
            pProfileMap = (OMX_U32 *)NvxAACNvMMOmxProfileMap;
            nMaxProfiles    = MAX_AAC_PROFILES;
            pModeMap = (OMX_U32 *)NvxAACNvMMOmxModeMap;
            nMaxModes    = MAX_AAC_MODES;
            pFormatMap = (OMX_U32 *)NvxAACNvMMOmxFormatMap;
            nMaxFormats    = MAX_AAC_FORMATS;
        }
            break;
        case NvMMBlockType_DecMP3:
        case NvMMBlockType_DecSUPERAUDIO:
        {
            pModeMap = (OMX_U32 *)NvxMP3NvMMOmxModeMap;
            nMaxModes    = MAX_MP3_MODES;
            pFormatMap = (OMX_U32 *)NvxMP3NvMMOmxFormatMap;
            nMaxFormats    = MAX_MP3_FORMATS;
        }
            break;
        case NvMMBlockType_DecWMA:
        case NvMMBlockType_DecWMAPRO:
        case NvMMBlockType_DecWMALSL:
        case NvMMBlockType_DecWMASUPER:
        {
            pProfileMap = (OMX_U32 *)NvxWMANvMMOmxProfileMap;
            nMaxProfiles    = MAX_WMA_PROFILES;
            pFormatMap = (OMX_U32 *)NvxWMANvMMOmxFormatMap;
            nMaxFormats    = MAX_WMA_FORMATS;
        }
            break;
        case NvMMBlockType_DecAMRNB:
        case NvMMBlockType_DecAMRWB:
        {
            pFormatMap = (OMX_U32 *)NvxAMRNvMMOmxFormatMap;
            nMaxFormats    = MAX_AMR_FORMATS;
        }
            break;
        default:
            *eOmxProfile = eProfile;
            *eOmxMode = eMode;
            *eOmxFormat = eFormat;
            break;

    }

    for (i=0; i<nMaxProfiles; i++)
    {
        if (pProfileMap[i*2+1] == eProfile)
        {
            *eOmxProfile = pProfileMap[i*2];
            break;
        }
    }

    for (i=0; i<nMaxModes; i++)
     {
         if (pModeMap[i*2+1] == eMode)
         {
             *eOmxMode = pModeMap[i*2];
             break;
         }
     }

    for (i=0; i<nMaxFormats; i++)
    {
        if (pFormatMap[i*2+1] == eFormat)
        {
            *eOmxFormat = pFormatMap[i*2];
            break;
        }
    }
}

