/*
 * Copyright (c) 2006-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "OMX_Core.h"
#include "OMX_Video.h"
#include "nvmmlite_video.h"
#include "NvxComponent.h"

/* NvMMLite and OMX codec profiles and levels mapping */
#define MAX_H263_LEVELS     9
#define MAX_H263_PROFILES   10

#define MAX_AVC_LEVELS     17
#define MAX_AVC_PROFILES   8

#define MAX_MPEG2_LEVELS     7
#define MAX_MPEG2_PROFILES   5

#define MAX_MPEG4_LEVELS     9
#define MAX_MPEG4_PROFILES   17

#define MAX_VP8_LEVELS     5
#define MAX_VP8_PROFILES   2

void NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideoCodingType eType,
                                        OMX_U32 nNvmmLitePfl,
                                        OMX_U32 nNvmmLiteLvl,
                                        OMX_U32 *pOmxPfl,
                                        OMX_U32 *pOmxLvl);

static const OMX_U32 NvxH263NvMMLiteOmxProfileMap[MAX_H263_PROFILES][2] =
{
    {OMX_VIDEO_H263ProfileBaseline          , NvMMLiteVideo_H263ProfileBaseline          },
    {OMX_VIDEO_H263ProfileH320Coding        , NvMMLiteVideo_H263ProfileH320Coding        },
    {OMX_VIDEO_H263ProfileBackwardCompatible, NvMMLiteVideo_H263ProfileBackwardCompatible},
    {OMX_VIDEO_H263ProfileISWV2             , NvMMLiteVideo_H263ProfileISWV2             },
    {OMX_VIDEO_H263ProfileISWV3             , NvMMLiteVideo_H263ProfileISWV3             },
    {OMX_VIDEO_H263ProfileHighCompression   , NvMMLiteVideo_H263ProfileHighCompression   },
    {OMX_VIDEO_H263ProfileInternet          , NvMMLiteVideo_H263ProfileInternet          },
    {OMX_VIDEO_H263ProfileInterlace         , NvMMLiteVideo_H263ProfileInterlace         },
    {OMX_VIDEO_H263ProfileHighLatency       , NvMMLiteVideo_H263ProfileHighLatency       },
    {OMX_VIDEO_H263ProfileMax               , NvMMLiteVideo_H263ProfileUnknown           },
};

static const OMX_U32 NvxH263NvMMLiteOmxLevelMap[MAX_H263_LEVELS][2] =
{
    {OMX_VIDEO_H263Level10 , NvMMLiteVideo_H263Level10     },
    {OMX_VIDEO_H263Level20 , NvMMLiteVideo_H263Level20     },
    {OMX_VIDEO_H263Level30 , NvMMLiteVideo_H263Level30     },
    {OMX_VIDEO_H263Level40 , NvMMLiteVideo_H263Level40     },
    {OMX_VIDEO_H263Level45 , NvMMLiteVideo_H263Level45     },
    {OMX_VIDEO_H263Level50 , NvMMLiteVideo_H263Level50     },
    {OMX_VIDEO_H263Level60 , NvMMLiteVideo_H263Level60     },
    {OMX_VIDEO_H263Level70 , NvMMLiteVideo_H263Level70     },
    {OMX_VIDEO_H263LevelMax, NvMMLiteVideo_H263LevelUnknown},
};

static const OMX_U32 NvxAVCNvMMLiteOmxProfileMap[MAX_AVC_PROFILES][2] =
{
    {OMX_VIDEO_AVCProfileBaseline, NvMMLiteVideo_AVCProfileBaseline},
    {OMX_VIDEO_AVCProfileMain    , NvMMLiteVideo_AVCProfileMain    },
    {OMX_VIDEO_AVCProfileExtended, NvMMLiteVideo_AVCProfileExtended},
    {OMX_VIDEO_AVCProfileHigh    , NvMMLiteVideo_AVCProfileHigh    },
    {OMX_VIDEO_AVCProfileHigh10  , NvMMLiteVideo_AVCProfileHigh10  },
    {OMX_VIDEO_AVCProfileHigh422 , NvMMLiteVideo_AVCProfileHigh422 },
    {OMX_VIDEO_AVCProfileHigh444 , NvMMLiteVideo_AVCProfileHigh444 },
    {OMX_VIDEO_AVCProfileMax     , NvMMLiteVideo_AVCProfileUnknown },
};

static const OMX_U32 NvxAVCNvMMLiteOmxLevelMap[MAX_AVC_LEVELS][2] =
{
    {OMX_VIDEO_AVCLevel1  , NvMMLiteVideo_AVCLevel1      },
    {OMX_VIDEO_AVCLevel1b , NvMMLiteVideo_AVCLevel1b     },
    {OMX_VIDEO_AVCLevel11 , NvMMLiteVideo_AVCLevel11     },
    {OMX_VIDEO_AVCLevel12 , NvMMLiteVideo_AVCLevel12     },
    {OMX_VIDEO_AVCLevel13 , NvMMLiteVideo_AVCLevel13     },
    {OMX_VIDEO_AVCLevel2  , NvMMLiteVideo_AVCLevel2      },
    {OMX_VIDEO_AVCLevel21 , NvMMLiteVideo_AVCLevel21     },
    {OMX_VIDEO_AVCLevel22 , NvMMLiteVideo_AVCLevel22     },
    {OMX_VIDEO_AVCLevel3  , NvMMLiteVideo_AVCLevel3      },
    {OMX_VIDEO_AVCLevel31 , NvMMLiteVideo_AVCLevel31     },
    {OMX_VIDEO_AVCLevel32 , NvMMLiteVideo_AVCLevel32     },
    {OMX_VIDEO_AVCLevel4  , NvMMLiteVideo_AVCLevel4      },
    {OMX_VIDEO_AVCLevel41 , NvMMLiteVideo_AVCLevel41     },
    {OMX_VIDEO_AVCLevel42 , NvMMLiteVideo_AVCLevel42     },
    {OMX_VIDEO_AVCLevel5  , NvMMLiteVideo_AVCLevel5      },
    {OMX_VIDEO_AVCLevel51 , NvMMLiteVideo_AVCLevel51     },
    {OMX_VIDEO_AVCLevelMax, NvMMLiteVideo_AVCLevelUnknown},
};

static const OMX_U32 NvxMPEG2NvMMLiteOmxLevelMap[MAX_MPEG2_PROFILES][2] =
{
    {OMX_VIDEO_MPEG2LevelLL , NvMMLiteVideo_MPEG2LevelLL     },
    {OMX_VIDEO_MPEG2LevelML , NvMMLiteVideo_MPEG2LevelML     },
    {OMX_VIDEO_MPEG2LevelH14, NvMMLiteVideo_MPEG2LevelH14    },
    {OMX_VIDEO_MPEG2LevelHL , NvMMLiteVideo_MPEG2LevelHL     },
    {OMX_VIDEO_MPEG2LevelMax, NvMMLiteVideo_MPEG2LevelUnknown},
};

static const OMX_U32 NvxMPEG2NvMMLiteOmxProfileMap[MAX_MPEG2_LEVELS][2] =
{
    {OMX_VIDEO_MPEG2ProfileSimple , NvMMLiteVideo_MPEG2ProfileSimple },
    {OMX_VIDEO_MPEG2ProfileMain   , NvMMLiteVideo_MPEG2ProfileMain   },
    {OMX_VIDEO_MPEG2Profile422    , NvMMLiteVideo_MPEG2Profile422    },
    {OMX_VIDEO_MPEG2ProfileSNR    , NvMMLiteVideo_MPEG2ProfileSNR    },
    {OMX_VIDEO_MPEG2ProfileSpatial, NvMMLiteVideo_MPEG2ProfileSpatial},
    {OMX_VIDEO_MPEG2ProfileHigh   , NvMMLiteVideo_MPEG2ProfileHigh   },
    {OMX_VIDEO_MPEG2ProfileMax    , NvMMLiteVideo_MPEG2ProfileUnknown},
};

static const OMX_U32 NvxMPEG4NvMMLiteOmxProfileMap[MAX_MPEG4_PROFILES][2] =
{
    {OMX_VIDEO_MPEG4ProfileSimple           , NvMMLiteVideo_MPEG4ProfileSimple          },
    {OMX_VIDEO_MPEG4ProfileSimpleScalable   , NvMMLiteVideo_MPEG4ProfileSimpleScalable  },
    {OMX_VIDEO_MPEG4ProfileCore             , NvMMLiteVideo_MPEG4ProfileCore            },
    {OMX_VIDEO_MPEG4ProfileMain             , NvMMLiteVideo_MPEG4ProfileMain            },
    {OMX_VIDEO_MPEG4ProfileNbit             , NvMMLiteVideo_MPEG4ProfileNbit            },
    {OMX_VIDEO_MPEG4ProfileScalableTexture  , NvMMLiteVideo_MPEG4ProfileScalableTexture },
    {OMX_VIDEO_MPEG4ProfileSimpleFace       , NvMMLiteVideo_MPEG4ProfileSimpleFace      },
    {OMX_VIDEO_MPEG4ProfileSimpleFBA        , NvMMLiteVideo_MPEG4ProfileSimpleFBA       },
    {OMX_VIDEO_MPEG4ProfileBasicAnimated    , NvMMLiteVideo_MPEG4ProfileBasicAnimated   },
    {OMX_VIDEO_MPEG4ProfileHybrid           , NvMMLiteVideo_MPEG4ProfileHybrid          },
    {OMX_VIDEO_MPEG4ProfileAdvancedRealTime , NvMMLiteVideo_MPEG4ProfileAdvancedRealTime},
    {OMX_VIDEO_MPEG4ProfileCoreScalable     , NvMMLiteVideo_MPEG4ProfileCoreScalable    },
    {OMX_VIDEO_MPEG4ProfileAdvancedCoding   , NvMMLiteVideo_MPEG4ProfileAdvancedCoding  },
    {OMX_VIDEO_MPEG4ProfileAdvancedCore     , NvMMLiteVideo_MPEG4ProfileAdvancedCore    },
    {OMX_VIDEO_MPEG4ProfileAdvancedScalable , NvMMLiteVideo_MPEG4ProfileAdvancedScalable},
    {OMX_VIDEO_MPEG4ProfileAdvancedSimple   , NvMMLiteVideo_MPEG4ProfileAdvancedSimple  },
    {OMX_VIDEO_MPEG4ProfileMax              , NvMMLiteVideo_MPEG4ProfileUnknown         },
};

static const OMX_U32 NvxMPEG4NvMMLiteOmxLevelMap[MAX_MPEG4_LEVELS][2] =
{
    {OMX_VIDEO_MPEG4Level0  , NvMMLiteVideo_MPEG4Level0      },
    {OMX_VIDEO_MPEG4Level0b , NvMMLiteVideo_MPEG4Level0b     },
    {OMX_VIDEO_MPEG4Level1  , NvMMLiteVideo_MPEG4Level1      },
    {OMX_VIDEO_MPEG4Level2  , NvMMLiteVideo_MPEG4Level2      },
    {OMX_VIDEO_MPEG4Level3  , NvMMLiteVideo_MPEG4Level3      },
    {OMX_VIDEO_MPEG4Level4  , NvMMLiteVideo_MPEG4Level4      },
    {OMX_VIDEO_MPEG4Level4a , NvMMLiteVideo_MPEG4Level4a     },
    {OMX_VIDEO_MPEG4Level5  , NvMMLiteVideo_MPEG4Level5      },
    {OMX_VIDEO_MPEG4LevelMax, NvMMLiteVideo_MPEG4LevelUnknown},
};

static const OMX_U32 NvxVP8NvMMLiteOmxProfileMap[MAX_VP8_PROFILES][2] =
{
    {NVX_VIDEO_VP8ProfileMain   , NvMMLiteVideo_VP8ProfileMain   },
    {NVX_VIDEO_VP8ProfileMax    , NvMMLiteVideo_VP8ProfileUnknown},
};

static const OMX_U32 NvxVP8NvMMLiteOmxLevelMap[MAX_VP8_LEVELS][2] =
{
    {NVX_VIDEO_VP8Level_Version0    , NvMMLiteVideo_VP8LevelVersion0},
    {NVX_VIDEO_VP8Level_Version1    , NvMMLiteVideo_VP8LevelVersion1},
    {NVX_VIDEO_VP8Level_Version2    , NvMMLiteVideo_VP8LevelVersion2},
    {NVX_VIDEO_VP8Level_Version3    , NvMMLiteVideo_VP8LevelVersion3},
    {NVX_VIDEO_VP8LevelMax          , NvMMLiteVideo_VP8LevelUnknown },
};

void NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideoCodingType eType,
                                        OMX_U32 nNvmmLitePfl,
                                        OMX_U32 nNvmmLiteLvl,
                                        OMX_U32 *pOmxPfl,
                                        OMX_U32 *pOmxLvl)
{
    OMX_U32 *pLevelMap = NULL;
    OMX_U32 *pProfileMap = NULL;
    OMX_U8 nMaxLevels = 0;
    OMX_U8 nMaxProfiles = 0;
    OMX_U8 i = 0;

    switch (eType)
    {
        case NvMMLiteVideo_CodingTypeH264:
        {
            pLevelMap     = (OMX_U32 *)NvxAVCNvMMLiteOmxLevelMap;
            pProfileMap   = (OMX_U32 *)NvxAVCNvMMLiteOmxProfileMap;
            nMaxLevels    = MAX_AVC_LEVELS;
            nMaxProfiles  = MAX_AVC_PROFILES;
            break;
        }
        case NvMMLiteVideo_CodingTypeMPEG4:
        {
            pLevelMap     = (OMX_U32 *)NvxMPEG4NvMMLiteOmxLevelMap;
            pProfileMap   = (OMX_U32 *)NvxMPEG4NvMMLiteOmxProfileMap;
            nMaxLevels    = MAX_MPEG4_LEVELS;
            nMaxProfiles  = MAX_MPEG4_PROFILES;
            break;
        }
        case NvMMLiteVideo_CodingTypeMPEG2:
        {
            pLevelMap     = (OMX_U32 *)NvxMPEG2NvMMLiteOmxLevelMap;
            pProfileMap   = (OMX_U32 *)NvxMPEG2NvMMLiteOmxProfileMap;
            nMaxLevels    = MAX_MPEG2_LEVELS;
            nMaxProfiles  = MAX_MPEG2_PROFILES;
            break;
        }
        case NvMMLiteVideo_CodingTypeH263:
        {
            pLevelMap     = (OMX_U32 *)NvxH263NvMMLiteOmxLevelMap;
            pProfileMap   = (OMX_U32 *)NvxH263NvMMLiteOmxProfileMap;
            nMaxLevels    = MAX_H263_LEVELS;
            nMaxProfiles  = MAX_H263_PROFILES;
            break;
        }
        case NvMMLiteVideo_CodingTypeVP8:
        {
            pLevelMap     = (OMX_U32 *)NvxVP8NvMMLiteOmxLevelMap;
            pProfileMap   = (OMX_U32 *)NvxVP8NvMMLiteOmxProfileMap;
            nMaxLevels    = MAX_VP8_LEVELS;
            nMaxProfiles  = MAX_VP8_PROFILES;
        }
        default:
        {
            *pOmxPfl = nNvmmLitePfl;
            *pOmxLvl = nNvmmLiteLvl;
            return;
        }
    }

    for (i=0; i<nMaxLevels; i++)
    {
        if (pLevelMap[i*2+1] == nNvmmLiteLvl)
        {
            *pOmxLvl = pLevelMap[i*2];
            break;
        }
    }

    for (i=0; i<nMaxProfiles; i++)
    {
        if (pProfileMap[i*2+1] == nNvmmLitePfl)
        {
           *pOmxPfl = pProfileMap[i*2];
            break;
        }
    }

    return;
}

