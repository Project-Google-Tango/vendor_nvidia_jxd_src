/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvflash_parser.h"
#include "nvapputil.h"
#include "nvutil.h"

NvBool case_sensitive = NV_FALSE;

#define VERIFY( exp, code ) \
    if( !(exp) ) { \
        code; \
    }

NvError
nvflash_concate_strings(char **str1, char *str2)
{
    NvError e    = 0;
    NvU32 len1   = NvOsStrlen(*str1);
    NvU32 len2   = NvOsStrlen(str2);

    *str1 = NvOsRealloc(*str1, len1 + len2 + 1);

    if(str1 == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NvOsMemcpy(*str1 + len1, str2, len2 + 1);

fail:
    return e;
}

NvError
nvflash_chomp_string(char **string)
{
    char cur_char;
    int start = 0;
    int len   = 0;
    int end   = 0;
    NvError e = 0;

    if(*string == NULL)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    len = NvOsStrlen(*string);
    end = len - 1;

    while(1)
    {
        cur_char = (*string)[start];
        if(cur_char == '\0' || (cur_char != ' ' && cur_char != '\n'))
            break;
        start++;
    }

    while(end >= 0)
    {
        cur_char = (*string)[end];
        if(cur_char == '\0' || (cur_char != ' ' && cur_char != '\n'))
            break;
        end--;
    }

    if((end - start + 1) == len)
        goto fail;

    if(end < start)
        end = start = len;

    len = end - start + 1;

    if(start != 0)
        NvOsMemmove(*string, *string + start, len);

    (*string)[len] = '\0';

fail:
    return e;
}

NvError
nvflash_get_nextToken(char *string, NvU32 *pos, const char *delims,
                      char **token)
{
    NvU32 offset  = *pos;
    int tokenSize = 0;
    const char *delim   = NULL;
    NvError e     = 0;
    char cur_char;

    if(delims == NULL || string == NULL || pos == NULL)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    while(1)
    {
        cur_char = string[offset];

        if(cur_char >= 'A' && cur_char <= 'Z' && !case_sensitive)
            string[offset] = 'a' + (cur_char - 'A');

        if(cur_char == '\0')
        {
            e = NvError_BadValue;
            offset++;
            break;
        }

        for(delim = delims; *delim != '\0' && cur_char != *delim; delim++);

        offset++;
        if(*delim != '\0')
            break;
    }

    tokenSize = offset - *pos;

    if(tokenSize <= 0)
    {
        e = NvError_BadParameter;
        goto end;
    }

    *token = NvOsAlloc(tokenSize);
    NvOsMemset(*token, 0, tokenSize);
    tokenSize--;

    if(*token == NULL)
    {
        NvAuPrintf("Memory allocation failed in get_next_token\n");
        e = NvError_InsufficientMemory;
    }
    else
    {
        NvOsMemcpy(*token, string + *pos, tokenSize);
        (*token)[tokenSize] = '\0';
    }
end:
    *pos = offset;
fail:
    return e;
}

/*
 * Parses the section and fills the vector of <key, value> pair
 *
 * string: String which is to parsed
 * rec: Vector which is to be filled
 * status: Previous status of parser
 *
 * Return: Current status of parser
 *
 */
static NvFlashParserStatus
parse_section(char *string, NvU32 *offset, NvFlashVector *rec,
                                  NvFlashParserStatus status)
{
    NvU32 index = 0;
    const char *err_str = NULL;
    NvError e;
    char *token;
    NvU32 nPairs;
    NvU32 maxPairs;
    NvFlashSectionPair *pairs;

    if(string == NULL || offset == NULL || rec == NULL)
        goto fail;

    nPairs = rec->n;
    maxPairs = rec->max;
    pairs = rec->data;

    index = *offset;
    while(1)
    {
        switch(status)
        {
            case P_KEY:
            case P_PKEY:
                token = NULL;
                e = nvflash_get_nextToken(string, &index, ":", &token);

                nvflash_chomp_string(&token);

                if(nPairs >= maxPairs)
                {
                    maxPairs = maxPairs + 4;
                    pairs = (NvFlashSectionPair *)NvOsRealloc(pairs,
                                                  sizeof(*pairs) * maxPairs);
                    VERIFY(pairs, err_str = "Parser structure reallocation"
                                  "failed"; goto fail);
                }
                if(status == P_KEY)
                {
                    pairs[nPairs].key = token;
                }
                else
                {
                    nvflash_concate_strings(&pairs[nPairs].key, token);
                    NvOsFree(token);
                }

                if(e == NvError_BadValue)
                    status = P_PKEY;
                else if(e == NvSuccess)
                    status = P_VALUE;
                else
                    status = P_ERROR;
                break;

            case P_VALUE:
            case P_PVALUE:
                token = NULL;
                e = nvflash_get_nextToken(string, &index, ";>", &token);
                nvflash_chomp_string(&token);
                if(status == P_VALUE)
                {
                    pairs[nPairs].value = token;
                }
                else
                {
                    nvflash_concate_strings(&pairs[nPairs].value, token);
                    NvOsFree(token);
                }

                if(e == NvError_BadValue)
                    status = P_PVALUE;
                else if(e == NvSuccess)
                {
                    status = P_KEY;
                    nPairs++;
                }
                else
                    status = P_ERROR;

                break;

            case P_COMPLETE:
            case P_CONTINUE:
            case P_STOP:
            case P_ERROR:
                break;
        }

        if(status == P_PVALUE || status == P_PKEY)
            break;

        if(status == P_ERROR)
            break;

        if(string[index - 1] == '>')
        {
            status = P_COMPLETE;
            goto end;
        }
        else if(string[index] == '\0')
        {
            goto end;
        }
    }

end:
    rec->n = nPairs;
    rec->max = maxPairs;
    rec->data = pairs;
    *offset = index;
fail:
    if(err_str)
    {
        status = P_ERROR;
        NvAuPrintf("%s\n", err_str);
    }
    return status;
}

static void
nvflash_free_parser_vector(NvFlashVector *vec)
{
    if(vec)
    {
        NvFlashSectionPair *Pairs = vec->data;
        if(Pairs != NULL)
        {
            NvU32 i;
            for(i = 0; i < vec->n; i++)
            {
                NvOsFree(Pairs[i].key);
                NvOsFree(Pairs[i].value);
                Pairs[i].key = NULL;
                Pairs[i].value = NULL;
            }
            NvOsFree(vec->data);
            vec->data = NULL;
        }
    }
}

NvError
nvflash_parser(NvOsFileHandle hFile, parse_callback *call_back,
                       NvU64 *file_offset, void *aux)
{
    NvOsStatType stat;
    char *buffer     = NULL;
    char *token      = NULL;
    char *err_str    = NULL;
    NvError e        = 0;
    NvU64 total_size = 0;
    NvU32 block_size = 4096;
    NvU32 chunk      = 0;
    size_t bytes     = 0;
    NvU32 offset     = 0;
    NvFlashParserStatus pstatus;
    NvFlashVector *rec = NULL;
    NvU64 file_size    = 0;

    VERIFY(hFile, err_str = "Null file pointer"; goto fail);

    e = NvOsFstat(hFile, &stat);
    VERIFY(e == NvSuccess, err_str = "Could not stat file"; goto fail);

    total_size = stat.size;

    if(total_size < block_size)
        chunk  = (NvU32) total_size;
    else
        chunk  = block_size;

    buffer = NvOsAlloc(chunk + 1);

    VERIFY(buffer, err_str = "Could not allocate parser buffer size";
                   goto fail);

    file_size = total_size;
    pstatus = P_COMPLETE;
    while(total_size)
    {
        NvOsMemset(buffer, 0, chunk + 1);

        chunk = total_size > chunk ? chunk : (NvU32) total_size;
        total_size -= chunk;
        offset = 0;

        e = NvOsFread(hFile, buffer, chunk, &bytes);
        VERIFY(e == NvSuccess, err_str = "File read error" ; goto fail);
        while(offset < bytes)
        {
            if(pstatus == P_COMPLETE)
            {
                if(rec == NULL)
                {
                    rec = NvOsAlloc(sizeof(*rec));
                    NvOsMemset(rec, 0, sizeof(*rec));
                    VERIFY(rec, err_str = "Parser structure allocation failed";
                           goto fail);
                }

                token = NULL;
                e = nvflash_get_nextToken(buffer, &offset, "<", &token);
                if(e == NvError_BadValue)
                {
                    e = NvSuccess;
                    if(token)
                        NvOsFree(token);
                    continue;
                }
                else
                {
                    VERIFY(e == NvSuccess, err_str = "Parsing Error";
                           goto fail);
                }

                if(token)
                    NvOsFree(token);

                pstatus = P_KEY;
            }

            pstatus = parse_section(buffer, &offset, rec, pstatus);
            VERIFY(pstatus != P_ERROR, err_str="Parsing section failed";
                   goto fail);

            if(pstatus == P_COMPLETE)
            {
                if(file_offset)
                {
                    *file_offset = file_size - total_size + offset - chunk;
                }
                if(call_back)
                {
                    pstatus = (*call_back)(rec, aux);
                    nvflash_free_parser_vector(rec);

                    if(pstatus == P_STOP)
                    {
                        e = NvSuccess;
                        goto end;
                    }
                    if(pstatus == P_ERROR)
                    {
                        e = NvError_ParserFailure;
                        goto fail;
                    }
                }

                pstatus = P_COMPLETE;
                NvOsFree(rec);
                rec = NULL;
            }
        }
    }

fail:
    if(err_str)
    {
        NvAuPrintf("%s\n", err_str);
        pstatus = P_ERROR;
    }

    if(pstatus == P_ERROR)
        e = NvError_ParserFailure;

end:
    nvflash_free_parser_vector(rec);

    if(rec)
        NvOsFree(rec);

    if(buffer)
        NvOsFree(buffer);
    return e;
}
NvDdkFuseDataType
nvflash_get_fusetype(const char *fuse_name)
{
    NvDdkFuseDataType type = NvDdkFuseDataType_Force32;

    if(!fuse_name)
        goto end;

    if(NvFlashIStrcmp(fuse_name, "device_key") == 0)
        type = NvDdkFuseDataType_DeviceKey;
    else if(NvFlashIStrcmp(fuse_name, "ignore_dev_sel_straps") == 0)
        type = NvDdkFuseDataType_SkipDevSelStraps;
    else if(NvFlashIStrcmp(fuse_name, "odm_production_mode") == 0)
        type = NvDdkFuseDataType_OdmProduction;
    else if(NvFlashIStrcmp(fuse_name, "odm_reserved") == 0)
        type = NvDdkFuseDataType_ReservedOdm;
    else if(NvFlashIStrcmp(fuse_name, "sec_boot_dev_cfg") == 0)
        type = NvDdkFuseDataType_SecBootDeviceConfig;
    else if(NvFlashIStrcmp(fuse_name, "jtag_disable") == 0)
        type = NvDdkFuseDataType_JtagDisable;
    else if(NvFlashIStrcmp(fuse_name, "sec_boot_dev_sel") == 0)
        type = NvDdkFuseDataType_SecBootDeviceSelect;
    else if(NvFlashIStrcmp(fuse_name, "secure_boot_key") == 0)
        type = NvDdkFuseDataType_SecureBootKey;
    else if(NvFlashIStrcmp(fuse_name, "sw_reserved") == 0)
        type = NvDdkFuseDataType_SwReserved;
    else if(NvFlashIStrcmp(fuse_name, "pkc_disable") == 0)
        type = NvDdkFuseDataType_PkcDisable;
    else if(NvFlashIStrcmp(fuse_name, "vp8_enable") == 0)
        type = NvDdkFuseDataType_Vp8Enable;
    else if(NvFlashIStrcmp(fuse_name, "odm_lock") == 0)
        type = NvDdkFuseDataType_OdmLock;
    else if(NvFlashIStrcmp(fuse_name, "public_key_hash") == 0)
        type = NvDdkFuseDataType_PublicKeyHash;
    else if(NvFlashIStrcmp(fuse_name, "skip_fuseburn") == 0)
        type = NvDdkFuseDataType_SkipFuseBurn;
end:
    return type;
}

NvBool nvflash_fuse_value_alloc(NvFuseWrite *fusedata,
                        NvFlashSectionPair *pairs, NvU8 index1, NvU8 index2)
{
    char *tmp;
    char *position;
    NvU8 counter = 1;
    NvU8 i = 0;

    tmp = pairs[index1].value;
    position = tmp;
    while(*tmp != '\0')
    {
        if (*tmp == ' ')
        {
            *tmp = '\0';
            counter++;
        }
        tmp++;
    }
    while(counter--)
    {
        fusedata->fuses[index2].fuse_value[i++] =
                    NvUStrtoul(position, &position, 0);
        position++;
    }
    return NV_TRUE;
}


NvFlashParserStatus
nvflash_fusewrite_parser_callback(NvFlashVector *rec, void *aux)
{
    NvU32 i = 0;
    NvU32 j = 0;
    NvFlashParserStatus ret    = P_CONTINUE;
    NvU32 nPairs               = rec->n;
    NvFlashSectionPair *Pairs  = rec->data;
    const char *err_str        = NULL;
    NvBool unknown_token       = NV_FALSE;
    NvFuseWriteCallbackArg *a = (NvFuseWriteCallbackArg *) aux;
    NvFuseWrite *fusedata             = &(a->FuseInfo);

    for (i = 0; i < nPairs; i++)
    {
         switch (Pairs[i].key[0])
         {
             case 'f':
                if(NvFlashIStrncmp(Pairs[i].key, "fuse", 4) == 0)
                {
                    for(j = 0; j < MAX_NUM_FUSE; j++)
                    {
                        if(fusedata->fuses[j].fuse_type == 0)
                            break;
                    }
                    VERIFY(j < MAX_NUM_FUSE, err_str = "More than max no. of fuses"; goto end);
                    fusedata->fuses[j].fuse_type =
                                      nvflash_get_fusetype(Pairs[i].value);
                }
                else
                {
                    unknown_token = NV_TRUE;
                    goto end;
                }
                break;

             case 'v':
                /* Fuse section without fuse token has fuse type zero. j points
                   to the current fuse entry for which value is to
                   be set. Unknown and known fuses have non zero value for
                   fuse_type.*/
                if(fusedata->fuses[j].fuse_type == 0)
                {
                    err_str = "fuse token should be present"
                              " before value token";
                    ret = P_ERROR;
                    goto end;
                }

                if(NvFlashIStrcmp(Pairs[i].key, "value") == 0)
                {
                    nvflash_fuse_value_alloc(fusedata,Pairs,i,j);
                }
                else
                {
                    unknown_token = NV_TRUE;
                    goto end;
                }
                break;

            default:
                unknown_token = NV_TRUE;
                goto end;
        }
    }

end:
    if(unknown_token)
    {
        NvAuPrintf("Invalid token in config file: %s\n", Pairs[i].key);
        ret = P_ERROR;
    }

    if(err_str)
    {
        NvAuPrintf("%s\n", err_str);
        ret = P_ERROR;
    }
    return ret;
}
void NvParseCaseSensitive(NvBool format)
{
    if (format)
        case_sensitive = NV_TRUE;
    else
        case_sensitive = NV_FALSE;
    return;
}

NvBool nvflash_check_cfgtype(const char* pCfgName)
{
    NvOsFileHandle hFile = 0;
    size_t Bytes = 0;
    char Read;
    NvBool String = NV_FALSE;
    NvBool Comment = NV_FALSE;
    NvBool Ret = NV_FALSE;
    NvError e = NvSuccess;

    NV_CHECK_ERROR_CLEANUP(
        NvOsFopen(pCfgName, NVOS_OPEN_READ, &hFile)
    );
    while (NvOsFread(hFile, &Read, sizeof(Read), &Bytes) == NvSuccess)
    {
        switch (Read)
        {
            /* this is to capture strings in comments */
            case '/':
            {
                if (!Comment)
                    String ^= NV_TRUE;
                break;
            }
            /* capture comments that start with # */
            case '#':
            {
                Comment = NV_TRUE;
                break;
            }
            case '\r':
            case '\n':
            {
                Comment = NV_FALSE;
                break;
            }
            case '\t':
            case ' ':
            {
                // Ignore the spaces
                break;
            }
            default:
            {
                if (!(String || Comment))
                {
                    if (Read == '<')
                        Ret = NV_TRUE;
                    else
                        Ret = NV_FALSE;

                    goto clean;
                }
                break;
            }
        }
    }
fail:
    NvAuPrintf("Common cfg check failed\n");

clean:
    if (hFile)
      NvOsFclose(hFile);
    return Ret;
}
