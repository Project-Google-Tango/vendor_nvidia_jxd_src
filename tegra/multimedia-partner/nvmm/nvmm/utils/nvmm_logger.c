/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvmm_logger.h"
#include "nvmm_logger_internal.h"
#include "nvutil.h"
#include <stdarg.h>

typedef struct LoggerClientRec
{
    char* pClientTag;
    NvLoggerLevel Level;
}LoggerClient;

static LoggerClient s_LoggerClient[NVLOG_MAX_CLIENTS] =
{
        {"NvmmMp3Dec",                  NVLOG_NONE},
        {"NvmmMP4Dec",                  NVLOG_NONE},
        {"NvmmAviDec",                    NVLOG_NONE},
        {"NvmmWmaDec",                   NVLOG_NONE},
        {"NvmmAacDec",                   NVLOG_NONE},
        {"NvmmWavDec",                 NVLOG_NONE},
        {"NvmmVideoDec",                NVLOG_NONE},
        {"NvmmVideoRenderer",       NVLOG_NONE},
        {"NvmmMixer",                      NVLOG_NONE},
        {"NvmmWMDRM",                 NVLOG_NONE},
        {"NvmmMp3Parser",              NVLOG_NONE},
        {"NvmmMp4Parser",              NVLOG_NONE},
        {"NvmmAviParser",                NVLOG_NONE},
        {"NvmmAsfParser",               NVLOG_NONE},
        {"NvmmAacParser",               NVLOG_NONE},
        {"NvmmAmrParser",               NVLOG_NONE},
        {"NvmmSuperParser",            NVLOG_NONE},
        {"NvmmTrackList",                 NVLOG_NONE},
        {"NvmmMp4Writer",               NVLOG_NONE},
        {"NvmmContentPipe",            NVLOG_NONE},
        {"NvmmBuffering",                 NVLOG_NONE},
        {"NvmmTransport",               NVLOG_NONE},
        {"NvOMXILClient",                NVLOG_NONE},
        {"NvmmManager",                NVLOG_NONE},
        {"NvmmBaseWriter",            NVLOG_NONE},
        {"NvmmMkvParser",             NVLOG_NONE},
        {"NvmmNemParser",             NVLOG_NONE},
        {"NvxDrmPlay",                NVLOG_NONE}
};

void NvLoggerPrint(NvLoggerClient ClientID, NvLoggerLevel Level, const char* pFormat, ...)
{
#if !NV_IS_AVP
    static NvU8 s_LoggerInitialized = 0;
    char ConfigName[NVOS_PATH_MAX];
    char ConfigValue[NVOS_PATH_MAX];
    NvLoggerLevel DefaultLevel;
    NvU32 i;

    if (!s_LoggerInitialized)
    {
        /* check for enable all  mode. In this case, we treat all clients and all levels to be ENABLED */
#if !defined(ANDROID)
        DefaultLevel = (NvLoggerLevel)((NvOsGetConfigString("NV_LOG_LEVEL", ConfigValue, sizeof(ConfigValue)) == NvSuccess) ?
                                                    NvUStrtoul(ConfigValue, NULL, 10) : NVLOG_NONE);
#else
        DefaultLevel = (NvLoggerLevel)((NvOsGetConfigString("nvlog.level", ConfigValue, sizeof(ConfigValue)) == NvSuccess) ?
                                                    NvUStrtoul(ConfigValue, NULL, 10) : NVLOG_NONE);
#endif

        /* populate the client levels from registry */
        for (i = 0; i  < NVLOG_MAX_CLIENTS; i++)
        {
            NvOsSnprintf(ConfigName, sizeof(ConfigName), "nvlog.%d.level", i);
            s_LoggerClient[i].Level = (NvLoggerLevel)((NvOsGetConfigString(ConfigName, ConfigValue, sizeof(ConfigValue)) == NvSuccess) ?
                                                                NvUStrtoul(ConfigValue, NULL, 0) : DefaultLevel);
        }

        s_LoggerInitialized = 1;
    }
#endif

    if (Level > NVLOG_NONE && Level <= s_LoggerClient[ClientID].Level)
    {
        va_list ap;
        va_start( ap, pFormat );

        NvLoggerPrintf(Level, s_LoggerClient[ClientID].pClientTag, pFormat, ap);
#if !defined(ANDROID)
        NvOsDebugPrintf("\n");
#endif
        va_end(ap);

    }

}

#if !NV_IS_AVP
NvLoggerLevel NvLoggerGetLevel(NvLoggerClient ClientID)
{
    char ConfigClient[NVOS_PATH_MAX];
    char ConfigValue[NVOS_PATH_MAX];
    NvLoggerLevel Level = NVLOG_NONE;

    if (ClientID < NVLOG_MAX_CLIENTS)
    {
        NvOsSnprintf(ConfigClient, sizeof(ConfigClient), "nvlog.%d.level", ClientID);
        if (NvOsGetConfigString(ConfigClient, ConfigValue, sizeof(ConfigValue)) == NvSuccess ||
              NvOsGetConfigString("nvlog.level", ConfigValue, sizeof(ConfigValue)) == NvSuccess)
        {
            Level = (NvLoggerLevel)NvUStrtoul(ConfigValue, NULL, 10);
        }
    }

    return Level;
}
#endif


#if NV_IS_AVP
void NvLoggerSetLevel(NvLoggerClient ClientID, NvLoggerLevel Level)
{
    if (ClientID < NVLOG_MAX_CLIENTS)
    {
        s_LoggerClient[ClientID].Level = Level;
    }
}
#endif
