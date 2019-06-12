/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvflash_commands.h"
#include "nvutil.h"
#include "nvapputil.h"
#include "nvassert.h"
#include "nvflash_verifylist.h"
#include <nv3p.h>

#define NV_FLASH_MAX_COMMANDS (32)
#define MAX_SUPPORTED_DEVICES 127

#ifdef NV_EMBEDDED_BUILD
#if NVOS_IS_LINUX || NVOS_IS_UNIX
#define PATH_DELIMITER '/'
#else
#define PATH_DELIMITER '\\'
#endif
#endif

typedef struct CommandRec
{
    NvFlashCommand cmd;
    void *arg;
} Command;

#ifdef NV_EMBEDDED_BUILD
static char *s_NvFlash_BaseDir;
#endif
static NvU32 s_NumCommands;
static Command s_Commands[NV_FLASH_MAX_COMMANDS];
static NvFlashBctFiles s_OptionBct;
static const char *s_OptionConfigFile;
static const char *s_OptionBlob;
static const char *s_OptionBootLoader;
static NvFlashCmdDiskImageOptions s_DiskImg_options;
static NvU32 s_OptionOdmData;
static const NvU8 *s_OptionBootDevType;
static NvU32 s_OptionBootDevConfig;
static NvBool s_OptionQuiet;
static NvBool s_OptionWait;
static NvBool s_FormatAll;
static NvBool s_OptionVerifyPartitionsEnabled;
static NvBool s_OptionSetBct;
static NvBool s_OptionSetVerifySdram;
static NvBool s_OptionSkipAutoDetect;
static NvU32 s_OptionVerifySdramvalue;
static NvU32 s_OptionTransportMode = 0;
static NvU32 s_OptionEmulationDevice = 0;
static NvFlashCmdDevParamOptions s_DevParams;
static NvU32 s_OptionDevInstance = 0;
static NvFlashSkipPart s_OptionSkipPart;
static NvFlashNCTPart s_OptionNCTPart;
static const char *s_LastError;
static NvU32 s_CurrentCommand;
static NvBool s_CreateCommandOccurred;
static char s_Buffer[256];
static NvFlashVersionStruct s_VersionInfo;
static NvBool AddPartitionToVerify(const char *Partition);
static NvFlashFuseBypass s_OptionFuseBypass = {"fuse_bypass.txt", 0,
                                               NV_FALSE, NV_FALSE};
static NvU32 s_OptionRamDump;
static NvU32 s_Freq;
static NvBool s_SkipSync = NV_FALSE;
static const char *s_OptionBctSection;
static NvFlashAutoFlash s_AutoFlash = {NV_FALSE, 0};
static NvBool s_OptionListBoardId = NV_FALSE;
static const char *s_OptionDtbFile;
static NvBool s_OptionReportFlashProgress;
static NvBool s_OptionLimitedPowerMode;
static NvU32 s_OpMode;
static char *s_AppletFile;
#if ENABLE_NVDUMPER
static const char *s_DumpRamFile;
#endif

#define NVFLASH_INVALID_ENTRY    0xffffffff
static Nv3pCmdDownloadBootloader s_OptionEntryAndAddress;

NvError
NvFlashCommandParse( NvU32 argc, const char *argv[] )
{
    NvU32 i;
    NvU32 idx = 0;
    const char *arg;
    NvBool bOpt;

    s_LastError = 0;
    s_NumCommands = 0;
    s_CurrentCommand = 0;
    s_OptionBct.BctOrCfgFile = 0;
    s_OptionBct.OutputBctFile = 0;
    s_OptionConfigFile = 0;
    s_OptionBlob = 0;
    s_OptionBootLoader = 0;
    s_OptionVerifyPartitionsEnabled = NV_FALSE;
    s_CreateCommandOccurred = NV_FALSE;
    s_OptionSetBct = NV_FALSE;
    s_OptionSetVerifySdram = NV_FALSE;
    s_OptionVerifySdramvalue = 0;
    s_OptionSkipPart.number = 0;
    s_OptionBctSection = 0;
    s_OptionNCTPart.nctfilename = 0;
    s_OptionDtbFile = 0;

    NvOsMemset( s_Commands, 0, sizeof(s_Commands) );

    s_OptionEntryAndAddress.EntryPoint = NVFLASH_INVALID_ENTRY;
#ifdef NV_EMBEDDED_BUILD
    {
        NvU32 len;
        s_NvFlash_BaseDir = NvOsAlloc(NvOsStrlen(argv[0]) + 1);
        if(!s_NvFlash_BaseDir)
        {
            s_LastError = "--NvFlash_BaseDir allocation failure";
            goto fail;
        }

        NvOsStrncpy(s_NvFlash_BaseDir, argv[0], NvOsStrlen(argv[0]) + 1);
        len = NvOsStrlen(s_NvFlash_BaseDir);
        while (len > 0)
        {
            if (s_NvFlash_BaseDir[len] == PATH_DELIMITER)
                break;
            len--;
        }

        s_NvFlash_BaseDir[len] = '\0';
    }
#endif
    for( i = 1; i < argc; i++ )
    {
        arg = argv[i];
        bOpt = NV_FALSE;

        if(( NvOsStrcmp( arg, "--help" ) == 0 )|| (NvOsStrcmp( arg, "-h" ) == 0 ))
        {
            // really should just skip this command, but this is an amusing
            // easter egg.
            s_Commands[idx].cmd = NvFlashCommand_Help;
        }
        else if(( NvOsStrcmp( arg, "--cmdhelp" ) == 0 ) || ( NvOsStrcmp( arg, "-ch" ) == 0 ))
        {
            NvFlashCmdHelp *a;
            s_Commands[idx].cmd = NvFlashCommand_CmdHelp;
            if( i + 1 >= argc )
            {
                s_LastError = "--cmdhelp argument missing";
                goto fail;
            }
            a = NvOsAlloc( sizeof(NvFlashCmdHelp) );
            if( !a )
            {
                s_LastError = "--getbit allocation failure";
                goto fail;
            }
            i++;
            a->cmdname = argv[i];
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--resume" ) == 0 ||
            NvOsStrcmp( arg, "-r" ) == 0 )
        {
            if( i == 1 )
            {
                continue;
            }
            else
            {
                s_LastError = "resume must be specified first in the "
                    "command line";
                goto fail;
            }
        }
        else if( NvOsStrcmp( arg, "--create" ) == 0 )
        {
            s_CreateCommandOccurred = NV_TRUE;
            s_Commands[idx].cmd = NvFlashCommand_Create;
        }
        else if( NvOsStrcmp( arg, "--setboot" ) == 0 )
        {
            NvFlashCmdSetBoot *a;

            if( i + 1 >= argc )
            {
                s_LastError = "--setboot argument missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_SetBoot;
            a = NvOsAlloc( sizeof(NvFlashCmdSetBoot) );
            if( !a )
            {
                s_LastError = "--setboot allocation failure";
                goto fail;
            }

            i++;
            a->PartitionID = NvUStrtoul( argv[i], 0, 0 );
            if (!a->PartitionID)
                a->PartitionName = argv[i];
            else
                a->PartitionName = NULL;
            s_Commands[idx].arg = a;
        }
        else if(( NvOsStrcmp( arg, "--format_partition" ) == 0 ) ||
            ( NvOsStrcmp( arg, "--delete_partition" ) == 0 ))
        {
            NvFlashCmdFormatPartition *a;

            if( i + 1 >= argc )
            {
                s_LastError = "--format_partition argument missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_FormatPartition;
            a = NvOsAlloc( sizeof(NvFlashCmdFormatPartition) );
            if( !a )
            {
                s_LastError = "--format_partition allocation failure";
                goto fail;
            }

            i++;
            a->PartitionID = NvUStrtoul( argv[i], 0, 0 );
            if (!a->PartitionID)
                a->PartitionName = argv[i];
            else
                a->PartitionName = NULL;
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--verifypart" ) == 0 )
        {
            NvBool PartitionAdded = NV_FALSE;

            if( i + 1 >= argc )
            {
                s_LastError = "--verifypart argument missing";
                goto fail;
            }

            i++;
            PartitionAdded = AddPartitionToVerify(argv[i]);
            if (!PartitionAdded)
            {
                goto fail;
            }

            s_OptionVerifyPartitionsEnabled = NV_TRUE;
            bOpt = NV_TRUE;

        }
        else if( NvOsStrcmp( arg, "--download" ) == 0 )
        {
            NvFlashCmdDownload *a;

            if( i + 1 >= argc )
            {
                s_LastError = "--download arguments missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_Download;
            a = NvOsAlloc(sizeof(NvFlashCmdDownload));
            NvOsMemset(a, 0x0, sizeof(*a));

            if( !a )
            {
                s_LastError = "--download allocation failure";
                goto fail;
            }

            i++;

            if ((i + 1 >= argc) || (argv[i + 1][0] == '-'))
            {
                a->filename = argv[i];
            }
            else
            {
                if( i + 1 >= argc )
                {
                    s_LastError = "--download arguments missing";
                    goto fail;
                }

                a->PartitionID = NvUStrtoul( argv[i], 0, 0 );
                if (!a->PartitionID)
                    a->PartitionName = argv[i];
                else
                    a->PartitionName = NULL;
                i++;
                a->filename = argv[i];
            }

            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--read" ) == 0 )
        {
            NvFlashCmdRead *a;

            if( i + 2 >= argc )
            {
                s_LastError = "--read arguments missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_Read;
            a = NvOsAlloc( sizeof(NvFlashCmdRead) );
            if( !a )
            {
                s_LastError = "--read allocation failure";
                goto fail;
            }

            i++;
            a->PartitionID = NvUStrtoul( argv[i], 0, 0 );
            if (!a->PartitionID)
                a->PartitionName = argv[i];
            else
                a->PartitionName = NULL;
            i++;
            a->filename = argv[i];
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--updatebct" ) == 0 )
        {
            if( i + 1 >= argc )
            {
                s_LastError = "--updatebct arguments missing";
                goto fail;
            }
            i++;
            s_OptionBctSection = argv[i];
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--rawdeviceread" ) == 0 )
        {
            NvFlashCmdRawDeviceReadWrite *a;

            if( i + 3 >= argc )
            {
                s_LastError = "--rawdeviceread arguments missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_RawDeviceRead;
            a = NvOsAlloc( sizeof(NvFlashCmdRawDeviceReadWrite) );
            if( !a )
            {
                s_LastError = "--rawdeviceread allocation failure";
                goto fail;
            }

            i++;
            a->StartSector = NvUStrtoul( argv[i], 0, 0 );
            i++;
            a->NoOfSectors = NvUStrtoul( argv[i], 0, 0 );
            i++;
            a->filename = argv[i];
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--rawdevicewrite" ) == 0 )
        {
            NvFlashCmdRawDeviceReadWrite *a;

            if( i + 3 >= argc )
            {
                s_LastError = "--rawdevicewrite arguments missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_RawDeviceWrite;
            a = NvOsAlloc( sizeof(NvFlashCmdRawDeviceReadWrite) );
            if( !a )
            {
                s_LastError = "--rawdevicewrite allocation failure";
                goto fail;
            }

            i++;
            a->StartSector = NvUStrtoul( argv[i], 0, 0 );
            i++;
            a->NoOfSectors = NvUStrtoul( argv[i], 0, 0 );
            i++;
            a->filename = argv[i];
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--getpartitiontable" ) == 0 )
        {
            NvFlashCmdGetPartitionTable *a;

            if( i + 1 >= argc )
            {
                s_LastError = "--getpartitiontable argument missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_GetPartitionTable;
            a = NvOsAlloc( sizeof(NvFlashCmdGetPartitionTable) );
            if( !a )
            {
                s_LastError = "--getpartitiontable allocation failure";
                goto fail;
            }

            i++;
            a->filename = argv[i];
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--getbct" ) == 0 )
        {
            s_Commands[idx].cmd = NvFlashCommand_GetBct;
        }
        else if( NvOsStrcmp( arg, "--getbit" ) == 0 )
        {
            NvFlashCmdGetBit *a;

            if( i + 1 >= argc )
            {
                s_LastError = "--getbit argument missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_GetBit;
            a = NvOsAlloc( sizeof(NvFlashCmdGetBit) );
            if( !a )
            {
                s_LastError = "--getbit allocation failure";
                goto fail;
            }

            i++;
            a->filename = argv[i];
            s_Commands[idx].arg = a;
        }
        else if ( NvOsStrcmp( arg, "--dumpbit" ) == 0 )
        {
            NvFlashCmdDumpBit *a;
            char DefaultOption[] = "anything";

            s_Commands[idx].cmd = NvFlashCommand_DumpBit;
            a = NvOsAlloc( sizeof(NvFlashCmdDumpBit) );
            if( !a )
            {
                s_LastError = "--dumpbit allocation failure";
                goto fail;
            }

            a->DumpbitOption = DefaultOption;
            if (argv[i + 1][0] != '-')
            {
                i++;
                a->DumpbitOption = argv[i];
            }
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--odm" ) == 0 )
        {
            NvFlashCmdSetOdmCmd *a;

            if( i + 1 >= argc )
            {
                s_LastError = "--odm argument missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_SetOdmCmd;

            a = NvOsAlloc( sizeof(NvFlashCmdSetOdmCmd) );
            if( !a )
            {
                s_LastError = "--odm allocation failure";
                goto fail;
            }
            i++;
            if( NvOsStrcmp(argv[i], "fuelgaugefwupgrade" ) == 0)
            {
                a->odmExtCmd = NvFlashOdmExtCmd_FuelGaugeFwUpgrade;
                if ( i + 1 >= argc )
                {
                    s_LastError = "fuelgaugefwupgrade argument missing";
                    goto fail;
                }
                i++;
                a->odmExtCmdParam.fuelGaugeFwUpgrade.filename1 = argv[i];
                if( i + 1 < argc && argv[i+1][0] != '-' )
                {
                    i++;
                    a->odmExtCmdParam.fuelGaugeFwUpgrade.filename2 = argv[i];
                }
                else
                {
                    a->odmExtCmdParam.fuelGaugeFwUpgrade.filename2 = NULL;
                }
            }
            else if (NvOsStrcmp(argv[i], "verifysdram") == 0)
            {
                s_OptionSetVerifySdram = NV_TRUE;
                if (i + 1 >= argc)
                {
                    s_LastError = "verifysdram argument missing";
                    goto fail;
                }
                i++;
                s_OptionVerifySdramvalue = NvUStrtoul(argv[i], 0, 0);
                bOpt = NV_TRUE;
            }
            else if (NvOsStrcmp(argv[i], "get_tnid") == 0)
            {
                a->odmExtCmd = NvFlashOdmExtCmd_GetTN_ID;
                if( i + 1 >= argc )
                {
                    s_LastError = "get_tid argument missing";
                    goto fail;
                }
                i++;
                if (NvOsStrcmp("hostdisplay", argv[i]))
                    a->odmExtCmdParam.tnid_file.filename = argv[i];
                else
                    a->odmExtCmdParam.tnid_file.filename = NULL;
            }
            else if (NvOsStrcmp(argv[i], "verify_tid") == 0)
            {
                a->odmExtCmd = NvFlashOdmExtCmd_VerifyT_id;
                if( i + 1 >= argc )
                {
                    s_LastError = "verify_tid argument missing";
                    goto fail;
                }
                i++;
                a->odmExtCmdParam.tnid_file.filename = argv[i];
            }
            else if(NvOsStrcmp(argv[i], "updatecontrollerfw") == 0)
            {
                a->odmExtCmd = NvFlashOdmExtCmd_UpdateuCFw;
                if (i + 1 >= argc)
                {
                    s_LastError = "updatecontrollerfw argument missing";
                    goto fail;
                }
                i++;
                a->odmExtCmdParam.updateuCFw.filename = argv[i];
            }
            else if( NvOsStrcmp(argv[i], "limitedpowermode") == 0)
            {
                s_OptionLimitedPowerMode = NV_TRUE;
                a->odmExtCmd = NvFlashOdmExtCmd_LimitedPowerMode;
            }
            else if(NvOsStrcmp(argv[i], "tegratabfusekeys") == 0)
            {
                a->odmExtCmd = NvFlashOdmExtCmd_TegraTabFuseKeys;
                if (i + 1 >= argc)
                {
                    s_LastError = "TegraTabFuseKeys filename missing";
                    goto fail;
                }
                i++;
                a->odmExtCmdParam.tegraTabFuseKeys.filename = argv[i];
            }
            else if(NvOsStrcmp(argv[i], "tegratab_verifyfuse") == 0)
            {
                a->odmExtCmd = NvFlashOdmExtCmd_TegraTabVerifyFuse;
                if (i + 1 >= argc)
                {
                    s_LastError = "TegraTabFuseKeys filename missing";
                    goto fail;
                }
                i++;
                a->odmExtCmdParam.tegraTabFuseKeys.filename = argv[i];
            }
            else if (NvOsStrcmp(argv[i], "upgradedevicefirmware") == 0)
            {
                a->odmExtCmd = NvFlashOdmExtCmd_UpgradeDeviceFirmware;
                if (i + 1 >= argc)
                {
                    s_LastError = "firmware upgrade filename missing";
                    goto fail;
                }
                i++;
                a->odmExtCmdParam.upgradedevicefirmware.filename =  argv[i] ;
            }
            else
            {
                s_LastError = "--odm invalid command";
                goto fail;
            }
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--go" ) == 0 )
        {
            s_Commands[idx].cmd = NvFlashCommand_Go;
        }
        else if( NvOsStrcmp( arg, "--sync" ) == 0 )
        {
            s_Commands[idx].cmd = NvFlashCommand_Sync;
        }
        else if( NvOsStrcmp( arg, "--obliterate" ) == 0 )
        {
            s_Commands[idx].cmd = NvFlashCommand_Obliterate;
        }
        else if( NvOsStrcmp( arg, "--configfile" ) == 0 )
        {
            if( i + 1 >= argc )
            {
                s_LastError = "--configfile argument missing";
                goto fail;
            }

            i++;
            s_OptionConfigFile = argv[i];
            bOpt = NV_TRUE;
        }
        else if(NvOsStrcmp(arg, "--blob" ) == 0)
        {
            if(i + 1 >= argc)
            {
                s_LastError = "--blob argument missing";
                goto fail;
            }

            i++;
            s_OptionBlob = argv[i];
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--bct" ) == 0 )
        {
            if( i + 1 >= argc )
            {
                s_LastError = "--bct argument missing";
                goto fail;
            }

            i++;
            s_OptionBct.BctOrCfgFile = argv[i];
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--bl" ) == 0 )
        {
            if( i + 1 >= argc )
            {
                s_LastError = "--bl argument missing";
                goto fail;
            }

            i++;
            s_OptionBootLoader = argv[i];
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--odmdata" ) == 0)
        {
            if( i + 1 >= argc )
            {
                s_LastError = "--odmdata argument missing";
                goto fail;
            }

            i++;
            s_OptionOdmData = NvUStrtoul( argv[i], 0, 0 );
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--deviceid" ) == 0)
        {
            if( i + 1 >= argc )
            {
                s_LastError = "deviceid argument missing";
                goto fail;
            }

            i++;
            s_OptionEmulationDevice = NvUStrtoul( argv[i], 0, 0 );
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--transport" ) == 0)
        {
            if( i + 1 >= argc )
            {
                s_LastError = "transport mode argument missing";
                goto fail;
            }

            i++;
            if(NvOsStrcmp(argv[i], (const char *)"simulation") == 0)
                s_OptionTransportMode = NvFlashTransportMode_Simulation;
#if NVODM_BOARD_IS_FPGA
            else if(NvOsStrcmp(argv[i], (const char *)"jtag") == 0)
                s_OptionTransportMode = NvFlashTransportMode_Jtag;
#endif
            else if(NvOsStrcmp(argv[i], (const char *)"usb") == 0)
                s_OptionTransportMode = NvFlashTransportMode_Usb;
            else
                s_OptionTransportMode = NvFlashTransportMode_default;

            bOpt = NV_TRUE;
        }
        else if (NvOsStrcmp(arg, "--setbootdevtype") == 0)
        {
            if (i + 1 >= argc)
            {
                s_LastError = "--setbootdevtype argument missing";
                goto fail;
            }

            i++;
            s_OptionBootDevType = (const NvU8 *)argv[i];
            bOpt = NV_TRUE;
        }
        else if (NvOsStrcmp(arg, "--setbootdevconfig") == 0)
        {
            if (i + 1 >= argc)
            {
                s_LastError = "--setbootdevconfig argument missing";
                goto fail;
            }

            i++;
            s_OptionBootDevConfig = NvUStrtoul(argv[i],0,0);
            bOpt = NV_TRUE;
        }
        else if ( NvOsStrcmp( arg, "--diskimgopt" ) == 0 )
        {
            if ( i + 1 >= argc )
            {
                s_LastError = "--diskimgopt argument missing";
                goto fail;
            }

            i++;
            s_DiskImg_options.BlockSize = NvUStrtoul( argv[i], 0, 0 );
            bOpt = NV_TRUE;
        }
        else if ( NvOsStrcmp( arg, "--devparams" ) == 0 )
        {
            if ( i + 3 >= argc )
            {
                s_LastError = "--devparams argument missing";
                goto fail;
            }

            i++;
            s_DevParams.PageSize = NvUStrtoul( argv[i], 0, 0 );
            i++;
            s_DevParams.BlockSize = NvUStrtoul( argv[i], 0, 0 );
            i++;
            s_DevParams.TotalBlocks = NvUStrtoul( argv[i], 0, 0 );
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--quiet" ) == 0 ||
            NvOsStrcmp( arg, "-q" ) == 0 )
        {
            s_OptionQuiet = NV_TRUE;
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--wait" ) == 0 ||
            NvOsStrcmp( arg, "-w" ) == 0 )
        {
            s_OptionWait = NV_TRUE;
            bOpt = NV_TRUE;
        }
        else if(( NvOsStrcmp( arg, "--delete_all" ) == 0 ) ||
            (NvOsStrcmp( arg, "--format_all" ) == 0 ))
        {
            s_FormatAll = NV_TRUE;
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--setbct" ) == 0 )
        {
            s_OptionSetBct= NV_TRUE;
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--settime" ) == 0 )
        {
            s_Commands[idx].cmd = NvFlashCommand_SetTime;
        }
        else if( NvOsStrcmp( arg, "--instance" ) == 0 )
        {
            NvU32 argval = 0;

            if( i + 1 >= argc )
            {
                s_LastError = "instance argument missing";
                goto fail;
            }

            i++;

            // Check input for usb device path string or instance number
            if (argv[i][0] >= '0' && argv[i][0] <= '9')
            {
                argval = NvUStrtoul(argv[i], 0, 0);
                //validate if device instance is crossing max supported devices
                if(argval > MAX_SUPPORTED_DEVICES)
                {
                    s_LastError = "--instance wrong parameter";
                    goto fail;
                }
            }
            else
            {
                argval = NvFlashEncodeUsbPath(argv[i]);
                if (argval == 0xFFFFFFFF)
                    goto fail;
            }

            s_OptionDevInstance = argval;
            bOpt = NV_TRUE;
        }
        // added to set entry & address thru command line
        else if (NvOsStrcmp( arg, "--setentry" ) == 0)
        {
            if( i + 2 >= argc )
            {
                s_LastError = "--set entry argument missing";
                goto fail;
            }

            i++;
            s_OptionEntryAndAddress.EntryPoint = NvUStrtoul(argv[i], 0, 0);
            i++;
            s_OptionEntryAndAddress.Address = NvUStrtoul(argv[i], 0, 0);
            bOpt = NV_TRUE;
        }
        else if (NvOsStrcmp(arg, "--skip_part") == 0)
        {
            if (i + 1 >= argc)
            {
                s_LastError = "--skip_part argument missing";
                goto fail;
            }

            i++;
            s_OptionSkipPart.pt_name[s_OptionSkipPart.number] = argv[i];
            s_OptionSkipPart.number++;
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--internaldata" ) == 0 )
        {
            NvFlashCmdNvPrivData *a;
            if( i + 1 >= argc )
            {
                s_LastError = "--NvInternal-data argument missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_NvPrivData;
            a = NvOsAlloc( sizeof(NvFlashCmdNvPrivData));
            if( !a )
            {
                s_LastError = "--NvInternal-data allocation failure";
                goto fail;
            }

            i++;
            a->filename = argv[i];
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--runbdktest" ) == 0 )
        {
            NvFlashCmdBDKTest *a;

            if( i + 3 >= argc )
            {
                s_LastError = "--runbdktest argument missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_RunBDKTest;
            a = NvOsAlloc( sizeof(NvFlashCmdBDKTest) );
            if( !a )
            {
                s_LastError = "--runbdktest allocation failure";
                goto fail;
            }

            i++;
            a->cfgfilename = argv[i];
            i++;
            if (NvOsStrcmp("hostdisplay", argv[i]))
                a->outfilename = argv[i];
            else
                a->outfilename = NULL;
            i++;
                a->opts = NvUStrtoul( argv[i], 0, 0 );
            s_Commands[idx].arg = a;
        }
        else if(NvOsStrcmp(arg, "--fusebypass_config") == 0)
        {
            if(i + 1 >= argc)
            {
                s_LastError = "--fusebypass_config argument missing";
                goto fail;
            }

            i++;
            s_OptionFuseBypass.config_file = argv[i];
            s_OptionFuseBypass.override_config_file = NV_TRUE;
            bOpt = NV_TRUE;

        }
        else if(NvOsStrcmp(arg, "--sku_to_bypass") == 0 ||
                NvOsStrcmp(arg, "-s") == 0)
        {
            if(i + 1 >= argc)
            {
                s_LastError = "--sku_to_bypass argument missing";
                goto fail;
            }

            i++;
            s_OptionFuseBypass.pSkuId = argv[i];
            bOpt = NV_TRUE;
            s_OptionFuseBypass.force_bypass = NV_FALSE;
            s_OptionFuseBypass.ForceDownload = NV_FALSE;

            if((i + 1 < argc) && !NvOsStrcmp(argv[i + 1], "forcebypass"))
            {
                s_OptionFuseBypass.force_bypass = NV_TRUE;
                i++;
            }
            if((i + 1 < argc) && !NvOsStrcmp(argv[i + 1], "forcedownload"))
            {
                s_OptionFuseBypass.ForceDownload = NV_TRUE;
                i++;
            }
        }
        else if(NvOsStrcmp( arg, "--freq" ) == 0 ||
                NvOsStrcmp(arg, "-f") == 0)
        {
            if( i + 1 >= argc )
            {
                s_LastError = "--freq arguement missing";
                goto fail;
            }

            i++;
            s_Freq= NvUStrtoul( argv[i], 0, 0 );
            bOpt = NV_TRUE;
        }
        else if (NvOsStrcmp(arg, "--buildbct") == 0)
        {
            if(i + 1 >= argc)
            {
                s_LastError = "--configfile is missing";
                goto fail;
            }
            i++;
            s_OptionBct.BctOrCfgFile = argv[i];
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                s_OptionBct.OutputBctFile = argv[i];
            }
            bOpt = NV_TRUE;
        }
        else if ( NvOsStrcmp( arg, "--writefuse") == 0)
        {
            NvFlashCmdFuseWrite *a;
            if ( i + 1 >= argc )
            {
                s_LastError = "--writefuse argument missing";
                goto fail;
            }
            s_Commands[idx].cmd = NvFlashCommand_FuseWrite;
            a = NvOsAlloc( sizeof(NvFlashCmdSetBoot) );
            if( !a )
            {
            s_LastError = "--writefuse allocation failure";
            goto fail;
            }
            i++;
            a->config_file = argv[i];
            s_Commands[idx].arg = a;
        }
        else if (NvOsStrcmp(arg, "--symkeygen") == 0)
        {
            NvFlashCmdSymkeygen *a;

            if (i + 1 >= argc)
            {
                s_LastError = "--symkeygen s_pubfile is missing ";
                goto fail;
            }
            s_Commands[idx].cmd = NvFlashCommand_Symkeygen;
            a = NvOsAlloc(sizeof(NvFlashCmdSymkeygen));
            if(!a)
            {
                s_LastError = "--symkeygen allocation failure";
                goto fail;
            }
            i++;
            a->spub_filename = argv[i];
            if (argv[i + 1][0] != '-')
            {
                i++;
                a->outfilename = argv[i];
            }
            else
                a->outfilename = NULL;
            s_Commands[idx].arg = a;
        }
        else if (NvOsStrcmp(arg, "--d_fuseburn") == 0)
        {
            NvFlashCmdDFuseBurn *a;
            if (i + 1 >= argc)
            {
                s_LastError = "--d_fuseburn blob_file is missing ";
                goto fail;
            }
            s_Commands[idx].cmd = NvFlashCommand_DFuseBurn;
            a = NvOsAlloc(sizeof(NvFlashCmdDFuseBurn));
            if(!a)
            {
                s_LastError = "--d_fuseburn allocation failure";
                goto fail;
            }
            i++;
            a->blob_dsession = argv[i];
            s_Commands[idx].arg = a;
        }
#if ENABLE_NVDUMPER
	 else if (NvOsStrcmp( arg, "--nvtbootramdump" ) == 0)
        {
            NvFlashCmdNvtbootRAMDump *a;

            if( i + 3 >= argc )
            {
                s_LastError = "--nvtbootramdump arguments missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_NvtbootRAMDump;
            a = NvOsAlloc( sizeof(NvFlashCmdNvtbootRAMDump) );
            if( !a )
            {
                s_LastError = "--dumpram allocation failure";
                goto fail;
            }

            i++;
            a->Offset = NvUStrtoul( argv[i], 0, 0 );
            i++;
            a->Length = NvUStrtoul( argv[i], 0, 0 );
            i++;
            a->filename = argv[i];
            s_Commands[idx].arg = a;
        }
#endif
        else if (NvOsStrcmp(arg, "--versioninfo") == 0)
        {
            s_VersionInfo.IsVersionInfo = NV_TRUE;
            if(i + 1 >= argc)
            {
                // do nothing;
            }
            else
            {
                i++;
                s_VersionInfo.MajorNum1 = NvUStrtoul(argv[i], 0, 0);
                if(i + 1 >= argc)
                {
                    NvAuPrintf("2nd argument required\n");
                    goto fail;
                }
                else
                {
                    i++;
                    s_VersionInfo.MajorNum2 = NvUStrtoul(argv[i], 0, 0);
                }
            }
        }
        else if (NvOsStrcmp(arg, "--skipsync") == 0)
        {
            s_Commands[idx].cmd = NvFlashCommand_SkipSync;
            s_SkipSync = NV_TRUE;
        }
        else if( NvOsStrcmp(arg, "--skipautodetect") == 0)
        {
            s_OptionSkipAutoDetect = NV_TRUE;
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--reset" ) == 0)
        {
            NvFlashCmdReset *a;
            if( i + 2 >= argc )
            {
                s_LastError = "--reset mode argument missing";
                goto fail;
            }
            i++;
            s_Commands[idx].cmd = NvFlashCommand_Reset;
            a = NvOsAlloc(sizeof(NvFlashCmdReset));
            if(!a)
            {
                s_LastError = "--reset allocation failure";
                goto fail;
            }
            if(NvOsStrcmp(argv[i], (const char *)"normal") == 0)
                a->reset = NvFlashResetType_Normal;
            else if(NvOsStrcmp(argv[i], (const char *)"recovery") == 0)
                a->reset = NvFlashResetType_RecoveryMode;
            i++;
                a->delay_ms = NvUStrtoul( argv[i], 0, 0 );
            s_Commands[idx].arg = a;
        }
        else if (NvOsStrcmp(arg, "--autoflash") == 0)
        {
            if (!s_CreateCommandOccurred)
            {
                s_CreateCommandOccurred = NV_TRUE;
                s_Commands[idx].cmd = NvFlashCommand_Create;
                idx++;
                s_Commands[idx].cmd = NvFlashCommand_Go;
            }
            else
            {
                bOpt = NV_TRUE;
            }

            s_AutoFlash.DoAutoFlash = NV_TRUE;

            if (i + 2 <= argc && argv[i + 1][0] != '-')
            {
                i++;
                s_AutoFlash.BoardId = NvUStrtoul(argv[i], 0, 0);
            }
        }
        else if (NvOsStrcmp(arg, "--NCT") == 0 || NvOsStrcmp(arg, "--nct") == 0 )
        {
            if (i + 1 >= argc)
            {
                s_LastError = "--NCT argument missing";
                goto fail;
            }

            i++;
            s_OptionNCTPart.nctfilename = argv[i];
            bOpt = NV_TRUE;
        }
        else if(NvOsStrcmp(arg, "--list") == 0)
        {
            s_OptionListBoardId = NV_TRUE;
            bOpt = NV_TRUE;
        }
        else if(NvOsStrcmp(arg, "--dtbfile") == 0)
        {
            if(i + 1 >= argc)
            {
                s_LastError = "--dtbfile is missing";
                goto fail;
            }
            i++;
            s_OptionDtbFile = argv[i];
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--reportflashprogress" ) == 0 )
        {
            s_OptionReportFlashProgress = NV_TRUE;
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--readnctitem" ) == 0 )
        {
            NvFlashCmdReadWriteNctItem *a;

            if( i + 1 >= argc )
            {
                s_LastError = "--readnctitem arguments missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_ReadNctItem;
            a = NvOsAlloc( sizeof(NvFlashCmdReadWriteNctItem) );
            if( !a )
            {
                s_LastError = "--readnctitem allocation failure";
                goto fail;
            }

            i++;
            a->EntryIdx = NvUStrtoul( argv[i], 0, 0 );
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--writenctitem" ) == 0 )
        {
            NvFlashCmdReadWriteNctItem *a;

            if( i + 2 >= argc )
            {
                s_LastError = "--writenctitem arguments missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_WriteNctItem;
            a = NvOsAlloc( sizeof(NvFlashCmdReadWriteNctItem) );
            if( !a )
            {
                s_LastError = "--writenctitem allocation failure";
                goto fail;
            }

            i++;
            a->EntryIdx = NvUStrtoul( argv[i], 0, 0 );
            i++;
            a->Data = argv[i];
            s_Commands[idx].arg = a;
        }
        else if( NvOsStrcmp( arg, "--opmode" ) == 0)
        {
            if( i + 1 >= argc )
            {
                s_LastError = "opmode argument missing";
                goto fail;
            }

            i++;
            s_OpMode = NvUStrtoul( argv[i], 0, 0 );;
            if (s_OpMode > 6)
            {
               s_LastError = "unsupported operating mode";
               goto fail;
            }
            bOpt = NV_TRUE;
        }
        else if( NvOsStrcmp( arg, "--applet" ) == 0)
        {
            if( i + 1 >= argc )
            {
                s_LastError = "--applet argument missing";
                    goto fail;
            }
            i++;
            s_AppletFile = argv[i];
            bOpt = NV_TRUE;
        }
#if ENABLE_NVDUMPER
	 else if (NvOsStrcmp( arg, "--nvtbootramdump" ) == 0)
        {
            if( i + 1 >= argc )
            {
                s_LastError = "--nvtbootramdump arguments missing";
                goto fail;
            }

            s_Commands[idx].cmd = NvFlashCommand_NvtbootRAMDump;
            i++;
            s_DumpRamFile = argv[i];
        }
#endif
        else
        {
            NvOsSnprintf( s_Buffer, sizeof(s_Buffer), "unknown command: %s",
                arg );
            s_LastError = s_Buffer;
            goto fail;
        }

        if( !bOpt )
        {
            idx++;
        }

        if( idx == NV_FLASH_MAX_COMMANDS )
        {
            s_LastError = "too many commands";
            goto fail;
        }
    }

    s_NumCommands = idx;

    // Constraint: verify_partition option is to be used in conjunction with
    // --create command.
    if (s_OptionVerifyPartitionsEnabled && !s_CreateCommandOccurred)
    {
        s_LastError = "--verify_partition used without --create";
        goto fail;
    }

    return NvSuccess;

fail:
    return NvError_BadParameter;
}

NvU32
NvFlashGetNumCommands( void )
{
    return s_NumCommands;
}

NvError
NvFlashCommandGetCommand( NvFlashCommand *cmd, void **arg )
{
    NV_ASSERT( cmd );
    NV_ASSERT( arg );

    s_LastError = 0;

    if( s_CurrentCommand == s_NumCommands )
    {
        s_LastError = "invalid command requested";
        // FIXME: better error code
        return NvError_BadParameter;
    }

    *cmd = s_Commands[s_CurrentCommand].cmd;
    *arg = s_Commands[s_CurrentCommand].arg;
    s_CurrentCommand++;
    return NvSuccess;
}

NvError
NvFlashCommandGetOption( NvFlashOption opt, void **data )
{
    NV_ASSERT( data );

    s_LastError = 0;

    switch( opt ) {
    case NvFlashOption_Bct:
        *data = (void *)&s_OptionBct;
        break;
    case NvFlashOption_ConfigFile:
        *data = (void *)s_OptionConfigFile;
        break;
    case NvFlashOption_Blob:
        *data = (void *)s_OptionBlob;
        break;
    case NvFlashOption_Bootloader:
        *data = (void *)s_OptionBootLoader;
        break;
    case NvFlashOption_DevParams:
        *data = (void *)&s_DevParams;
        break;
    case NvFlashOption_DiskImgOpt:
        *data = (void *)&s_DiskImg_options;
        break;
    case NvFlashOption_OdmData:
        *data = (void *)(NvUPtr)s_OptionOdmData;
        break;
    case NvFlashOption_EmulationDevId:
        *data = (void *)(NvUPtr)s_OptionEmulationDevice;
        break;
    case NvFlashOption_TransportMode:
        *data = (void *)(NvUPtr)s_OptionTransportMode;
        break;
    case NvFlashOption_SetBootDevType:
        *data = (void *)s_OptionBootDevType;
        break;
    case NvFlashOption_SetBootDevConfig:
        *data = (void *)(NvUPtr)s_OptionBootDevConfig;
        break;
    case NvFlashOption_FormatAll:
        *(NvBool *)data = s_FormatAll;
        break;
    case NvFlashOption_Quiet:
        *(NvBool *)data = s_OptionQuiet;
        break;
    case NvFlashOption_Wait:
        *(NvBool *)data = s_OptionWait;
        break;
    case NvFlashOption_VerifyEnabled:
        *(NvBool *)data = s_OptionVerifyPartitionsEnabled;
        break;
    case NvFlashOption_SetBct:
        *(NvBool *)data = s_OptionSetBct;
        break;
    case NvFlashOption_DevInstance:
        *(NvU32 *)data = s_OptionDevInstance;
        break;
    case NvFlashOption_EntryAndAddress:
        if (s_OptionEntryAndAddress.EntryPoint == NVFLASH_INVALID_ENTRY)
            return NvError_BadParameter;

        ((Nv3pCmdDownloadBootloader *)data)->EntryPoint = s_OptionEntryAndAddress.EntryPoint;
        ((Nv3pCmdDownloadBootloader *)data)->Address = s_OptionEntryAndAddress.Address;
        break;
    case NvFlashOption_SetOdmCmdVerifySdram:
        *(NvBool *)data = s_OptionSetVerifySdram;
        break;
    case NvFlashOption_OdmCmdVerifySdramVal:
        *(NvU32 *)data = s_OptionVerifySdramvalue;
        break;
#ifdef NV_EMBEDDED_BUILD
    case NvFlashOption_NvFlash_BaseDir:
        *data = (void *)s_NvFlash_BaseDir;
        break;
#endif
    case NvFlashOption_SkipPartition:
        // CHECK 1: Skip cannot be called without create.
        if (!s_CreateCommandOccurred)
        {
            s_LastError = "--skip_part used without --create";
            return NvError_BadParameter;
        }
        *data = (void *)&s_OptionSkipPart;
        break;
    case NvFlashOption_FuseBypass:
        *(NvFlashFuseBypass *)data = s_OptionFuseBypass;
        break;
    case NvFlashOption_Freq:
        *(NvU32 *)data = s_Freq;
        break;
    case NvFlashOption_Buildbct:
        *data = (void *)&s_OptionBct;
        break;
    case NvFlashOption_RamDump:
        *(NvU32 *)data = s_OptionRamDump;
        break;
    case NvFLashOption_VersionInfo:
        *data = (void *)&s_VersionInfo;
        break;
    case NvFlashOption_SkipSync:
        *(NvBool *)data = s_SkipSync;
        break;
    case NvFlashOption_UpdateBct:
        *data = (void *)s_OptionBctSection;
        break;
    case NvFlashOption_SkipAutoDetect:
        *(NvBool *)data = s_OptionSkipAutoDetect;
        break;
    case NvFlashOption_AutoFlash:
        *data = (void *)&s_AutoFlash;
        break;
    case NvFlashOption_NCTPart:
        *data = (void *)&s_OptionNCTPart;
        break;
    case NvFlashOption_ListBoardId:
        *(NvBool *)data = s_OptionListBoardId;
        break;
    case NvFlashOption_DtbFileName:
        *data = (void *)s_OptionDtbFile;
        break;
    case NvFlashOption_ReportFlashProgress:
        *(NvBool *)data = s_OptionReportFlashProgress;
        break;
    case NvFlashOption_OdmCmdLimitedPowerMode:
        *(NvBool *)data = s_OptionLimitedPowerMode;
        break;
    case NvFlashOption_DeviceOpMode:
        *data = (void *)s_OpMode;
        break;
    case NvFlashOption_AppletFile:
        *data = (void *)s_AppletFile;
        break;
    default:
        s_LastError = "invalid option requested";
        return NvError_BadParameter;
    }

    return NvSuccess;
}

const char *
NvFlashCommandGetLastError( void )
{
    const char *ret;

    if( !s_LastError )
    {
        return "success";
    }

    ret = s_LastError;
    s_LastError = 0;
    return ret;
}

NvBool
AddPartitionToVerify(const char *Partition)
{

    NvError e = NvSuccess;
    static NvBool s_VerifyAllPartitions = NV_FALSE;

   if (s_VerifyAllPartitions)
   {
       // Do nothing. Checking at this stage is better than going through
       // the list of partitions to be verified once.
       s_LastError = " WARNING: Verify All Partitions already Enabled...\n";
       return NV_TRUE;
   }

   if (!NvOsStrcmp(Partition, "-1"))
   {
        // Delete any nodes already existing in the list since all partitions
        // is enough to ensure all specific cases.
        NvFlashVerifyListDeInitLstPartitions();

        s_VerifyAllPartitions  = NV_TRUE;
        // Add node with Partition Id = 0xFFFFFFFF
   }

    // Mark partition for verification
    NV_CHECK_ERROR_CLEANUP(NvFlashVerifyListAddPartition(Partition));
    return NV_TRUE;

fail:
    return NV_FALSE;
}

void
NvFlashStrrev(char *pstr)
{
    NvU32 i, j, length = 0;
    char temp;

    length = NvOsStrlen(pstr);
    for (i = 0, j = length - 1; i < j; i++, j--)
    {
        temp = pstr[i];
        pstr[i] = pstr[j];
        pstr[j] = temp;
    }
}

NvU32
NvFlashEncodeUsbPath(const char *usbpath)
{
    char *pusb = NULL;
    NvU32 count = 0, index = 0, size = 0, retval = 0;

    // Note: This type of encoding technique is strictly based on the
    // assumption that the input path has the usbdirno and usbdevno in
    // the title i.e. : /xxx/yyy
    // If this format is not followed , error is thrown.
    if (!usbpath)
    {
        s_LastError = "Null instance device path string";
        return -1;
    }
    index = NvOsStrlen(usbpath);
    if (index < 8 || usbpath[index - 4] != '/' || usbpath[index - 8] != '/')
    {
        s_LastError = "Invalid instance device path string (Correct format:../xxx/yyy)";
        return -1;
    }

    while (1)
    {
        index--;
        if (usbpath[index] == '/')
        {
            count++;
            // Break extraction when 2nd '/' is encountered
            if (count == 2)
                break;
            continue;
        }
        else
        {
            size++;
            pusb = NvOsRealloc(pusb, size);
            pusb[size - 1] = usbpath[index];
        }
    }
    // The encoded number is prefixed by 1 to indicate that input is usb path string.
    size++;
    pusb = NvOsRealloc(pusb, size);
    pusb[size - 1] = '0' + 1;

    // String reverse to keep the format 1xxxyyy
    NvFlashStrrev(pusb);
    size++;
    pusb[size] = '\0';

    retval = NvUStrtoul(pusb, 0, 0);
    if (pusb)
        NvOsFree(pusb);
    return retval;
}

char NvFlashToLower(const char character)
{
    if(character >= 'A' && character <= 'Z')
        return ('a' + character - 'A');

    return character;
}

NvS32 NvFlashIStrcmp(const char *string1, const char *string2)
{
    NvS32 ret;

    for(; *string1 && *string2; string1++, string2++)
    {
        if(NvFlashToLower(*string1) != NvFlashToLower(*string2))
            break;
    }

    ret = NvFlashToLower(*string1) - NvFlashToLower(*string2);

    return ret;
}

NvS32 NvFlashIStrncmp(const char *pString1, const char *pString2, NvU32 NumChars)
{
    NvS32 Ret;

    NumChars--;
    for(; *pString1 && *pString2 && (NumChars > 0);
           NumChars--, pString1++, pString2++)
    {
        if(NvFlashToLower(*pString1) != NvFlashToLower(*pString2))
            break;
    }

    Ret = NvFlashToLower(*pString1) - NvFlashToLower(*pString2);

    return Ret;
}

static void NvFlashSetDefaultOdmData(NvU32 BoardId)
{
    if (s_OptionOdmData != 0)
        return;

    s_OptionOdmData = 0x4009C000;

    switch (BoardId)
    {
        case 1780:
        case 1781:
            s_OptionOdmData = 0x4009C000;
            break;
    }

    NvAuPrintf("Using 0x%08x as ODM data\n", s_OptionOdmData);
}

static void NvFlashSetDefaultBCT(NvU32 BoardId)
{
    if (s_OptionBct.BctOrCfgFile != NULL)
        return;

    s_OptionBct.BctOrCfgFile = "bct.cfg";

    switch (BoardId)
    {
        case 1780:
        case 1781:
            s_OptionBct.BctOrCfgFile = "bct.cfg";
            break;
    }

    NvAuPrintf("Using %s as bct\n", s_OptionBct.BctOrCfgFile);
}

void NvFlashSetDefaultFlashValues()
{
    NvU32 BoardId = 0;

    BoardId = s_AutoFlash.BoardId;

    NvFlashSetDefaultOdmData(BoardId);
    NvFlashSetDefaultBCT(BoardId);
    s_OptionSetBct= NV_TRUE;

    if (s_OptionConfigFile == NULL)
    {
        s_OptionConfigFile = "flash.cfg";
        NvAuPrintf("Using %s as flash config file\n", s_OptionConfigFile);
    }

    if (s_OptionBootLoader == NULL)
    {
        s_OptionBootLoader = "bootloader.bin";
        NvAuPrintf("Using %s as bootloader\n", s_OptionBootLoader);
    }
}

