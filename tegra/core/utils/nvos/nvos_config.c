/*
 * Copyright (c) 2010-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvos.h"
#include "nvutil.h"
#include "nvos_internal.h"
#include "nvassert.h"

#define NVOS_MAX_U32_STRING 16
#define NVOS_CONFIG_FILE_PATH "/etc/tegra_config.txt"
#define NVOS_MAX_CONFIG_VARS 256

static NvError NvOsConfigFileRead(const char *name, char *value, NvU32 size);
static NvError NvOsConfigFileWrite(const char *name, const char* value);

NvError NvOsGetConfigU32(const char *name, NvU32 *value)
{
    NvError err = NvOsGetConfigU32Internal(name, value);

    if (err == NvError_NotImplemented)
    {        
        char str[NVOS_MAX_U32_STRING];
        char* end;

        // get value as string
        err = NvOsGetConfigString(name, str, NVOS_MAX_U32_STRING);
        if (err)
            return err;

        // play safe
        str[NVOS_MAX_U32_STRING-1] = 0;

        // try to parse integer
        *value = NvUStrtoul(str, &end, 0);

        // if no chars parsed can't be a proper value
        if (str == end)
            return NvError_BadValue;
    }

    return err;
}

NvError NvOsSetConfigU32(const char *name, NvU32 value)
{
    NvError err = NvOsSetConfigU32Internal(name, value);

    if (err == NvError_NotImplemented)
    {    
        char str[NVOS_MAX_U32_STRING];
        NvS32 wrote;

        wrote = NvOsSnprintf(str, NVOS_MAX_U32_STRING, "%u", value);

        if (wrote == -1 || wrote > NVOS_MAX_U32_STRING)
        {
            NV_ASSERT(!"This should not happen");
            return NvError_BadValue;
        }
        str[NVOS_MAX_U32_STRING-1] = 0;

        return NvOsSetConfigString(name, str);
    }

    return err;
}

NvError NvOsGetConfigString(const char *name, char *value, NvU32 size)
{
    NvError err = NvOsGetConfigStringInternal(name, value, size);

    if (err == NvError_NotImplemented)
        err = NvOsConfigFileRead(name, value, size);

    return err;
}

NvError NvOsGetSysConfigString(const char *name, char *value, NvU32 size)
{
    NvError err = NvOsGetSysConfigStringInternal(name, value, size);

    if (err == NvError_NotImplemented)
        err = NvOsConfigFileRead(name, value, size);

    return err;
}

NvError NvOsSetConfigString(const char *name, const char *value)
{
    NvError err = NvOsSetConfigStringInternal(name, value);

    if (err == NvError_NotImplemented)
        err = NvOsConfigFileWrite(name, value);

    return err;
}

NvError NvOsSetSysConfigString(const char *name, const char *value)
{
    NvError err = NvOsSetSysConfigStringInternal(name, value);

    if (err == NvError_NotImplemented)
        err = NvOsConfigFileWrite(name, value);

    return err;
}

typedef struct
{
    char* buf;
    const char* names[NVOS_MAX_CONFIG_VARS];
    const char* values[NVOS_MAX_CONFIG_VARS];
    int num;
} NvOsConfig;

// The allowed characters for names and values
static int AllowedChar(char c)
{
    return ((c>='0' && c<='9') ||
            (c>='A' && c<='Z') ||
            (c>='a' && c<='z') ||
            (c == '_') || (c == '-') || (c == '.'));
}

static int Whitespace(char c)
{
    return (c == ' ' || c == '\t');
}

static int Newline(char c)
{
    return (c == '\n' || c == '\r');
}

static NvError ParseConfigFile(NvOsConfig* conf)
{
    NvOsFileHandle file = NULL;
    NvError err;
    NvOsStatType stat;
    size_t read;
    char* p;
    int mode = 0;

    conf->buf = NULL;
    conf->num = 0;

    err = NvOsFopen(NVOS_CONFIG_FILE_PATH, NVOS_OPEN_READ, &file);
    if (err) return err;

    err = NvOsFstat(file, &stat);
    if (err) goto clean;

    if (stat.type != NvOsFileType_File)
    {
        err = NvError_InvalidState;
        goto clean;
    }

    conf->buf = NvOsAllocInternal((size_t)stat.size + 1);
    if (!conf->buf)
    {
        err = NvError_InsufficientMemory;
        goto clean;
    }

    err = NvOsFread(file, conf->buf, (size_t)stat.size, &read);
    if (err) goto clean;

    NV_ASSERT(read == stat.size);

    NvOsFclose(file);
    file = NULL;    
    
    for (p = conf->buf; p < (conf->buf + read); p++)
    {
        switch(mode)
        {
        case 0:
            if (AllowedChar(*p))
            {
                mode = 1;
                conf->names[conf->num] = p;
            }
            break;
        case 1:
            if (*p == '=')
            {
                mode = 2;
                *p = 0;
            }
            else if (Whitespace(*p))
            {
                *p = 0;
            }
            else if (!AllowedChar(*p))
            {
                err = NvError_BadValue;
                goto clean;
            }
            break;
        case 2:
            if (AllowedChar(*p))
            {
                mode = 3;
                conf->values[conf->num] = p;
            }
            break;
        case 3:            
            if (Newline(*p) || (p == (conf->buf + read)))
            {
                mode = 0;
                *p = 0;
                conf->num++;
            }
            else if (Whitespace(*p))
            {
                *p = 0;
            }
            else if (!AllowedChar(*p))
            {
                err = NvError_BadValue;
                goto clean;
            }
            break;
        }
    }

 clean:
    if (err && conf->buf)
        NvOsFreeInternal(conf->buf);

    if (file)
        NvOsFclose(file);

    return err;    
}

static NvError DumpConfigFile(NvOsConfig* conf)
{
    NvOsFileHandle file = NULL;
    NvError err;
    int i;

    /* TODO: [ahatala 2010-03-05] what to do if writing fails?
       Should possibly write to a temp file first and copy? */

    err = NvOsFopen(NVOS_CONFIG_FILE_PATH, NVOS_OPEN_WRITE|NVOS_OPEN_CREATE, &file);
    if (err) return err;

    for (i = 0; i < conf->num; i++)
    {
        err = NvOsFwrite(file, conf->names[i], NvOsStrlen(conf->names[i]));
        if (err) break;
        err = NvOsFwrite(file, " = ", 3);
        if (err) break;
        err = NvOsFwrite(file, conf->values[i], NvOsStrlen(conf->values[i]));
        if (err) break;
        err = NvOsFwrite(file, "\n", 1);
        if (err) break;
    }

    NvOsFclose(file);
    return err;
}

NvError NvOsConfigFileWrite(const char *name, const char* value)
{    
    NvError err;
    NvOsConfig conf;
    int i = 0;

    err = ParseConfigFile(&conf);
    if (err && err != NvError_FileNotFound)
        return err;

    for (; i < conf.num; i++)
    {
        if (NvOsStrcmp(conf.names[i], name) == 0)
            break;
    }

    if (i == conf.num)
    {
        conf.num++;
        if (conf.num > NVOS_MAX_CONFIG_VARS)
        {
            NvOsFreeInternal(conf.buf);
            return NvError_InsufficientMemory;
        }
    }

    conf.names[i] = name;
    conf.values[i] = value;

    err = DumpConfigFile(&conf);

    NvOsFreeInternal(conf.buf);

    return err;
}
    
NvError NvOsConfigFileRead(const char *name, char *value, NvU32 size)
{
    NvError err;
    NvOsConfig conf;
    int i = 0;

    err = ParseConfigFile(&conf);
    if (err)
    {
        return (err == NvError_FileNotFound) ? NvError_ConfigVarNotFound : err;
    }

    for (; i < conf.num; i++)
    {
        if (NvOsStrcmp(conf.names[i], name) == 0)
            break;
    }

    if (i == conf.num)
        err = NvError_ConfigVarNotFound;
    else
        NvOsStrncpy(value, conf.values[i], size);

    NvOsFreeInternal(conf.buf);
    return err;
}

//
// Update the state information: stateId specifies what type of state is
// being provided (eg. stereo profiles, gles, video, etc.), so that each
// one can be handled differently if so desired. For 3DV Controls, just
// set the system property. For all other types of state, route it to
// the SetStateInternal() function that can be stubbed out on platforms
// that do not support this feature
//
NvError NvOsConfigSetState(int stateId, const char *name, const char *value,
        int valueSize, int flags)
{
    if ((stateId == NvOsStateId_3dv_Controls) ||
        (stateId == NvOsStateId_Mjolnir_Controls))
    {
        return NvOsSetSysConfigString(name, value);
    }
    else
    {
        return NvOsConfigSetStateInternal(stateId, name, value,
                        valueSize, flags);
    }
}

//
// Get the requested state: stateId specifies what type of state is
// being requested (eg. stereo profiles, gles, video, etc.), so that each
// one can be handled differently if so desired. For 3DV Controls, just
// return the system property. For all other types of state, route it to
// the SetStateInternal() function that can be stubbed out on platforms
// that do not support this feature
//
NvError NvOsConfigGetState(int stateId, const char *name, char *value,
        int valueSize, int flags)
{
    if ((stateId == NvOsStateId_3dv_Controls) ||
        (stateId == NvOsStateId_Mjolnir_Controls))
    {
        return NvOsGetSysConfigString(name, value, valueSize);
    }
    else
    {
        return NvOsConfigGetStateInternal(stateId, name, value,
                        valueSize, flags);
    }
}

//
// This function is used to just query the specified state to determine
// the number of settings. The caller will allocate a 2D buffer that
// can hold the keys. The total number of keys that should be returned
// and the max size of each key is also specified, to avoid buffer overruns
// If called with ppKeys as NULL, then just the max number of keys currently
// existing for this stateId will be returned in pMaxNumKeys. The caller can
// use this to allocate the ppKeys buffer appropriately
//
NvError NvOsConfigQueryState(int stateId, const char **ppKeys, int *pNumKeys,
        int maxKeySize)
{
    return NvOsConfigQueryStateInternal(stateId, ppKeys, pNumKeys, maxKeySize);
}
