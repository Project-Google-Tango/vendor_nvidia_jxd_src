/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_local.h"
#include "nvos.h"
#include "nvassert.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define NV_TIO_TCP_DEBUG        0
#define NV_TIO_TCP_SELFTEST     (NV_DEBUG && NVOS_IS_LINUX)

//###########################################################################
//############################### CODE ######################################
//###########################################################################

#if NV_TIO_TCP_SELFTEST
static void NvTioParseIpAddrTest(void)
{
    struct {
        char *name;
        NvTioTcpAddrInfo info;
    } list[] = {
    { "tcp:",                  {"",            "",     {0,},          0,    }},
    { "tcp::",                 {"",            "",     {0,},          0,    }},
    { "tcp:127.1.255.17",      {"127.1.255.17","",     {127,1,255,17},0,    }},
    { "tcp:111.0.222.1:",      {"111.0.222.1", "",     {111,0,222,1}, 0,    }},
    { "tcp:43.245.211.1:1234", {"43.245.211.1","1234", {43,245,211,1},1234, }},
    { "tcp:43.245.211.1:1",    {"43.245.211.1","1",    {43,245,211,1},1,    }},
    { "tcp:43.245.211.1:65535",{"43.245.211.1","65535",{43,245,211,1},65535,}},
    { "tcp::9999",             {"",            "9999", {0,},          9999, }},
    { "tcp:65532",             {"",            "65532",{0,},          65532,}},
    { "43.245.211.1:65535",    {"43.245.211.1","65535",{43,245,211,1},65535,}},
    { ":9999",                 {"",            "9999", {0,},          9999, }},
    { "65532",                 {"",            "65532",{0,},          65532,}},
    { 0,},
    { "tcp:x",                 },
    { "tcp:1.1.1.1.1",         },
    { "tcp:1.1.1.",            },
    { "tcp:1.1.1",             },
    { "tcp:.1.1.1.1",          },
    { "tcp:1..1.1.1",          },
    { "tcp::1.1.1.1",          },
    { "tcp:1.256.1.1",         },
    { "tcp:1.1.1.1:x",         },
    { "tcp:1.1.1.1:1:",        },
    { "tcp:1.1.1.1:1.",        },
    { "tcp:1.1.1.1:1x",        },
    { "tcp:1.1.1.1:1.1.1.1",   },
    { "tcp:1.1.1.1.1:1",       },
    { "tcp:1.1.1.:1",          },
    { "tcp:1.1.1:1",           },
    { "tcp:.1.1.1.1:1",        },
    { "tcp:1..1.1.1:1",        },
    { "tcp::1.1.1.1:1",        },
    { "tcp:1.256.1.1:1",       },
    { "tcp:1.1.1.1:65537",     },
    { 0,},
    };
    int i;
    NvTioTcpAddrInfo info;
    NvError err;

    DB(("====== Begin Ip ADDR parse test - ignore errors below here\n"));

    //
    // test valid addresses
    //
    for (i=0; list[i].name; i++) {
        NvOsMemset(&info,0,sizeof(info));
        err = NvTioParseIpAddr(list[i].name, 0, &info);
        (void)err;
        NV_ASSERT(!err);
        if (list[i].info.addrStr[0]) {
            list[i].info.defaultAddr = 0;
        } else {
            list[i].info.defaultAddr = 1;
            list[i].info.addrVal[0] = 127;
            list[i].info.addrVal[1] = 0;
            list[i].info.addrVal[2] = 0;
            list[i].info.addrVal[3] = 1;
            NvOsStrncpy(
                    list[i].info.addrStr,
                    "127.0.0.1",
                    sizeof(list[i].info.addrStr));
        }
        if (list[i].info.portStr[0]) {
            list[i].info.defaultPort = 0;
        } else {
            list[i].info.defaultPort = 1;
            list[i].info.portVal = NV_TESTIO_TCP_PORT;
            NvOsSnprintf(
                    list[i].info.portStr,
                    sizeof(list[i].info.portStr),
                    "%d",
                    NV_TESTIO_TCP_PORT);
        }
        NV_ASSERT(info.addrVal[0]  == list[i].info.addrVal[0]);
        NV_ASSERT(info.addrVal[1]  == list[i].info.addrVal[1]);
        NV_ASSERT(info.addrVal[2]  == list[i].info.addrVal[2]);
        NV_ASSERT(info.addrVal[3]  == list[i].info.addrVal[3]);
        NV_ASSERT(info.portVal     == list[i].info.portVal);
        NV_ASSERT(info.defaultAddr == list[i].info.defaultAddr);
        NV_ASSERT(info.defaultPort == list[i].info.defaultPort);
        NV_ASSERT(!NvOsStrcmp(info.addrStr, list[i].info.addrStr));
        NV_ASSERT(!NvOsStrcmp(info.portStr, list[i].info.portStr));
    }

    //
    // test invalid addresses
    //
    for (i++; list[i].name; i++) {
        err = NvTioParseIpAddr(list[i].name, 0, &info);
        NV_ASSERT(err);
    }
    DB(("====== End   Ip ADDR parse test - ignore errors above here\n"));
}
#endif

//===========================================================================
// NvTioParseIpAddr() - parse name into IP address string and port number
//===========================================================================
//
// Parse various address formats supplied in addr
//
// NULL               - use default  IP addr and default  port
// ""                 - use default  IP addr and default  port
// "tcp:"             - use default  IP addr and default  port
// "tcp:1.1.1.1"      - use supplied IP addr and default  port
// "tcp:1.1.1.1:"     - use supplied IP addr and default  port
// "tcp:1.1.1.1:1234" - use supplied IP addr and supplied port
// "tcp::1234"        - use default  IP addr and supplied port
// "tcp:1234"         - use default  IP addr and supplied port
//
NvError NvTioParseIpAddr(const char *name, int any, NvTioTcpAddrInfo *info)
{
    int empty = 1;
    NvU32 val = 0;
    NvU32 vals[5];
    const char *s;
    const char *colon = 0;
    int valIdx = 0;
    int c=1;
    size_t addrLen = 0;
    size_t portLen = 0;

#if NV_TIO_TCP_SELFTEST
    {
        //
        // self test first time around
        //
        static int first=1;
        if (first) {
            first = 0;
            NvTioParseIpAddrTest();
        }
    }
#endif

#if NV_TIO_TCP_DEBUG
    NV_DEBUG_PRINTF(("Parsing: '%s'\n",name?name:""));
#endif

    if (name && (!NvOsStrncmp(name, "tcp:", 4)))
        name += 4;

    s = name;

    //
    // parse the name into values
    //
    while (c && s) { // if name==NULL, this while loop won't run at all
        c = *(s++);
        switch (c) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                val *= 10;
                val += c - '0';
                empty = 0;
                if (val > 0xffff)
                    return DBERR(NvError_BadValue);
                break;

            case '.':
                if (empty || valIdx > 3 || colon)
                    return DBERR(NvError_BadValue);
                // fall through
            case 0:
            case ':':
                if (!empty) {
                    if (valIdx > 4)
                        return DBERR(NvError_BadValue);
                    vals[valIdx++] = val;
                    val = 0;
                    empty = 1;
                }

                if (c==':') {
                    if (colon)
                        return DBERR(NvError_BadValue);
                    if (valIdx != 0  && valIdx != 4)
                        return DBERR(NvError_BadValue);
                    colon = s-1;
                }
                break;

            default:
                return DBERR(NvError_BadValue);
        }
    }

    //
    // calc string length of each part
    //
    s--;
    if (colon) { // if name==NULL, colon will be 0, since while loop is skipped
        addrLen = colon - name;
        portLen = s - (colon + 1);
    } else if (valIdx > 1) { // if name==NULL, valIdx will be 0
        addrLen = s - name;
        portLen = 0;
    } else { // if name==NULL, s will be NULL, so portLen will be 0
        portLen = s - name;
        addrLen = 0;
        colon   = name-1;
    }

    //
    // calc string length of each part
    //
    switch(valIdx) {
        case 0:
            if (addrLen || portLen)
                return DBERR(NvError_BadValue);
            break;
        case 1:
            if (addrLen || !portLen)
                return DBERR(NvError_BadValue);
            vals[4] = vals[0];
            break;
        case 4:
            if (!addrLen || portLen)
                return DBERR(NvError_BadValue);
            break;
        case 5:
            if (!addrLen || !portLen)
                return DBERR(NvError_BadValue);
            break;
        default:
            return DBERR(NvError_BadValue);
    }

    if (addrLen+1 > sizeof(info->addrStr))
        return DBERR(NvError_BadValue);
    if (portLen+1 > sizeof(info->portStr))
        return DBERR(NvError_BadValue);

    NvOsMemset(info, 0, sizeof(*info));

    //
    // Set address
    //
    if (addrLen) {
        size_t i = NvOsStrlen(name);
        if (addrLen > i)
            addrLen = i;
        NvOsMemcpy(info->addrStr, name, addrLen);
        info->addrStr[addrLen] = 0;
        for (i=0; i<4; i++) {
            if (vals[i] > 0xff)
                return DBERR(NvError_BadValue);
            info->addrVal[i] = vals[i];
        }
        info->defaultAddr = 0;
    } else {
        NvOsStrncpy(
                info->addrStr,
                "127.0.0.1",
                sizeof(info->addrStr));
        info->addrVal[0] = 127;
        info->addrVal[1] = 0;
        info->addrVal[2] = 0;
        info->addrVal[3] = 1;
        info->defaultAddr = 1;
    }

    //
    // Set port
    //
    if (portLen) {
        NvOsMemcpy(info->portStr, colon+1, portLen);
        info->portStr[portLen] = 0;
        info->portVal = vals[4];
        info->defaultPort = 0;
    } else {
        NvOsSnprintf(
                info->portStr,
                sizeof(info->portStr),
                "%d",
                NV_TESTIO_TCP_PORT);
        info->portVal = NV_TESTIO_TCP_PORT;
        info->defaultPort = 1;
    }

    info->any = any ? 1 : 0;

#if NV_TIO_TCP_DEBUG
    NvOsDebugPrintf("Parsed (def=%d) %d %d %d %d (=%s) : (def=%d) %d (=%s)\n",
        info->defaultAddr,
        info->addrVal[0],
        info->addrVal[1],
        info->addrVal[2],
        info->addrVal[3],
        info->addrStr,
        info->defaultPort,
        info->portVal,
        info->portStr);
#endif

    return NvSuccess;
}
