/*
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvapputil.h"
#include "nvflash_usage.h"

void
nvflash_usage(void)
{
    char *cmds_list[]=
    {
    "\n==========================================================================================\n"
    "Basic commands/options:\n"
    "1)--help               : displays usage of all available nvflash commands\n",
    "2)--cmdhelp            : displays usage of specified nvflash command\n",
    "3)--bct                : used to specify bct/cfg file containing device specific parametrs\n",
    "4)--setbct             : used to download bct to IRAM\n",
    "5)--odmdata            : specifies the 32 bit odmdata integer\n",
    "6)--configfile         : used to specify configuration file\n",
    "7)--create             : initiates full intialization of targed device\n",
    "8)--bl                 : used to specify bootloader which will run 3pserver on device side\n",
    "9)--blob               : used for nvflash in odm secure devices\n",
    "10)--go                : used to boot bootloader after nvflash completes\n",
    "\nCommands/Options which require already flashed device:\n"
    "11)--setboot           : used to set a partition as bootable\n",
    "12)--format_partition  : used to format a partition\n",
    "13)--download          : used to download filename into a partition\n",
    "14)--read              : used to read a partition into filename\n",
    "15)--rawdevicewrite    : used to write filename into a sector region of media\n",
    "16)--rawdeviceread     : used to read a sector region of media into a filename\n",
    "17)--getpartitiontable : used to read partition table in text\n",
    "18)--getbct            : used to read back the BCT\n",
    "19)--skip_part         : used to indicate a partition to skip\n",
    "20)--format_all        : used to format/delete all existing partitions\n",
    "21)--obliterate        : used to erase all partitions and bad blocks\n",
    "22)--updatebct         : used to update some section of system bct(bctsection)\n",
    "\nOther commands/options:\n"
    "23)--resume            : used when device is looping in 3pserver.\n",
    "24)--verifypart        : used to verify contents of a partition in device\n",
    "25)--getbit            : used to read back the bit table to filename in binary form\n",
    "26)--dumpbit           : used to display the bit table read from device in text form\n",
    "27)--odm               : used to do some special diagnostics\n",
    "28)--deviceid          : used to set the device id of target in simulation mode\n",
    "29)--devparams         : used to pass metadata(pagesize etc.) in simulation mode\n",
    "30)--quiet             : used to suppress the exccessive console o/p while host-device comm\n",
    "31)--wait              : used to wait for USB cable connection before execution starts\n",
    "32)--instance          : used when multiple devices connected\n",
    "33)--transport         : used to specify the means of communication between host and target\n",
    "34)--setbootdevtype    : used to set boot device type fuse\n",
    "35)--setbootdevconfig  : used to set boot device config fuse\n",
    "36)--diskimgopt        : used to convert .dio or a .store.bin file to the new format (dio2)\n",
    "37)--internaldata      : used to flash the sku-info,serial-id,mac-id,prod-id\n",
    "38)--setentry          : used to specify the entry point and load address of the bootloader\n",
    "39)--settime           : used to update the PMU RTC HW clock\n",
    "40)--buildbct          : used to build bct from the config file of the given bct \n",
    "41)--writefuse         : used to specify the config file for fuse burning \n",
    "42)--versioninfo       : used to get the version information of nvflash and 3pserver\n",
    "43)--sync              : used to write 64 copies of bct in IRAM to partition\n",
    "44)--runbdktest        : used to run diagnostics tests in bdktestframework \n",
    "45)--skipautodetect    : used to avoid boardid reading for autodetect operations at miniloader level\n",
    "46)--opmode            : used to set the operating mode in simulation mode\n",
    "47)--applet            : used to pass the applet to be downloaded by bootrom\n"
#if ENABLE_NVDUMPER
    "48)--nvtbootramdump    : used to dump RAM at miniloader level for debugging\n"
#endif
    "============================================================================================\n"
    "\nUse --cmdhelp (--command name)/(command index) for detailed description & usage\n\n",
     };

    NvU32 i;

    for (i = 0; i < sizeof(cmds_list)/sizeof(cmds_list[0]); i++)
    {
        NvAuPrintf(cmds_list[i]);
    }
}

void
nvflash_cmd_usage(NvFlashCmdHelp *a)
{
    if (NvOsStrcmp(a->cmdname, "--help" ) == 0 ||
        NvOsStrcmp(a->cmdname, "-h") == 0 ||
        NvOsStrcmp(a->cmdname, "1") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf("displays all supported nvflash commands with usage and description\n\n");
    }
    else if (NvOsStrcmp(a->cmdname, "--cmdhelp") == 0 ||
        NvOsStrcmp(a->cmdname, "-ch") == 0 ||
        NvOsStrcmp(a->cmdname, "2") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --cmdhelp <cmd>\n"
            "-----------------------------------------------------------------------\n"
            "used to display usage and description about a particular command cmd\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--bct") == 0 ||
             NvOsStrcmp(a->cmdname, "3") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "as in full fledged nvflash command\n"
            "-----------------------------------------------------------------------\n"
            "used to specify bct file containing device specific parametrs like\n"
            "SDRAM configs, boot device configs etc. This is again modified by device\n"
            "to include info about bootloader, partition table etc.\n"
            "Also supports cfg file, as input, which is converted to BCT internally\n"
            "by the buildbct library.\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--setbct") == 0 ||
             NvOsStrcmp(a->cmdname, "4") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "as in full fledged nvflash command\n"
            "-----------------------------------------------------------------------\n"
            "used to download bct to IRAM, must be used with --sync command to update\n"
            "it in mass storage.\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--odmdata") == 0 ||
             NvOsStrcmp(a->cmdname, "5") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "as in full fledged nvflash command\n"
            "-----------------------------------------------------------------------\n"
            "specifies a 32 bit integer used to select particular instance of UART\n"
            "determine SDRAM size to be used etc. Also used for some platform specific\n"
            "operations, it is stored in bct by miniloader\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--configfile") == 0 ||
             NvOsStrcmp(a->cmdname, "6") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "as in full fledged nvflash command\n"
            "-----------------------------------------------------------------------\n"
            "used to specify configuration file containing info about various partitions\n"
            "to be made into device, their sizes, name and other attributes\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--create") == 0 ||
             NvOsStrcmp(a->cmdname, "7") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "--bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to do full initialization of the target device using the config file\n"
            "cfg start from creating all partitions, formatting them all, download\n"
            "images to their respective partitions and syncing bct at end\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--bl") == 0 ||
             NvOsStrcmp(a->cmdname, "8") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "as in full fledged nvflash command\n"
            "-----------------------------------------------------------------------\n"
            "used to specify bootloader which will run 3pserver on device side after\n"
            "it is downloaded by miniloader in SDRAM, normally used with all commands\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--blob" ) == 0 ||
             NvOsStrcmp(a->cmdname, "9") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --blob <blob> --bct <bct> --setbct --configfile <cfg> --create "
            "--odmdata <N> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used for nvflash in odm secure devices, can be used with any nvflash commands\n"
            "blob contains encrypted and signed RCM messages for communication b/w nvflash\n"
            "and bootrom at start, encrypted hash of encrypted bootloader(used with --bl\n"
            "option) for it's validation by miniloader and nvsbktool version info\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--go") == 0 ||
             NvOsStrcmp(a->cmdname, "10") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "as in full fledged nvflash command\n"
            "-----------------------------------------------------------------------\n"
            "used to boot bootloader after nvflash completes instead of looping in\n"
            "3pserver in resume mode, however sync command is also required to get\n"
            "out of resume mode, can be used with all nvflash commands\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--setboot") == 0 ||
             NvOsStrcmp(a->cmdname, "11") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --setboot <N/pname> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to set partition N/pname as bootable to already flashed device so that\n"
            "next time on coldboot, device will boot from bootloader at partition N/pname\n"
            "must be used only for bootloader partitions\n"
            "Partition number N can be found from cfg file used for flash earlier or\n"
            "Partition name(pname) can also be used as an argument\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--format_partition") == 0 ||
             NvOsStrcmp(a->cmdname, "--delete_partition") == 0 ||
             NvOsStrcmp(a->cmdname, "12") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --format_partition <N/pname> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to format patition N/pname in already flashed device making it empty\n"
            "Partition number N can be found from cfg file used for flash earlier or\n"
            "Partition name(pname) can also be used as an argument\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--download") == 0 ||
             NvOsStrcmp(a->cmdname, "13") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n\n",a->cmdname);
        NvAuPrintf(
            "1. nvflash --download <N/pname> <filename> --bl <bootloader> --go \n"
            "2. nvflash --blob <blob> --bct <BCT> --updatebct BLINFO --download <N/pname> "
            "<filename> --bl <bootloader> --go \n"
            "3 nvflash --blob <blob> --download <N/pname> <filename> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to download filename in partition N/pname into already flashed device\n"
            "1 is used in case of NvProduction mode , 2 is used for downloading bootloader \n"
            "partition type in secure mode pkc or sbk and 3 is used to download other partition\n"
            "type in secure mode pkc or sbk"
            "secure mode. Partition number N can be found from cfg file used for flash \n"
            "earlier or Partition name(pname) can also be used as an argument\n"
            "In case of 2 securetool will be run before running nvflash . This provides security"
            "of the bootloader being downloaded\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--read") == 0 ||
             NvOsStrcmp(a->cmdname, "14") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --read <N/pname> <filename> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to read partition N/pname from already flashed device into filename\n"
            "Partition number N can be found from cfg file used for flash earlier or\n"
            "Partition name(pname) can also be used as an argument\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--rawdevicewrite") == 0 ||
             NvOsStrcmp(a->cmdname, "15") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --rawdevicewrite S N <filename> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to write filename into N sectors of media starting from sector S to\n"
            "already flashed device\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--rawdeviceread") == 0 ||
             NvOsStrcmp(a->cmdname, "16") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --rawdeviceread S N <filename> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to read N sectors of media starting from sector S from already\n"
            "flashed device into filename\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--getpartitiontable") == 0 ||
             NvOsStrcmp(a->cmdname, "17") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --getpartitiontable <filename> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to read partition table in text from already flashed device into filename\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--getbct") == 0 ||
             NvOsStrcmp(a->cmdname, "18") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <filename> --getbct --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to read back the BCT from already flashed device to user specified\n"
            "filename, read in clear form whether device is secure or non-secure\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--skip_part") == 0 ||
             NvOsStrcmp(a->cmdname, "19") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "--bl <bootloader> --skip_part <part> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to indicate a partition to skip using following commands:\n"
            " --create: partition skipped with --create is not formatted.\n"
            "<part> is partition name as it appears in config file.\n"
            "This command can only be used associated with --create command.It will\n"
            "fail if config file used differ with the one used for previous device\n"
            "formatting. For this reason, command will also fail if device is not\n"
            "already flashed and formatted.\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--delete_all" ) == 0 ||
             NvOsStrcmp(a->cmdname, "--format_all" ) == 0 ||
             NvOsStrcmp(a->cmdname, "20") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --delete_all/--format_all --bl <bootloader> --go \n"
            "-----------------------------------------------------------------------\n"
            "used to format/delete all existing partitions on already flashed device\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--obliterate") == 0 ||
             NvOsStrcmp(a->cmdname, "21") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --configfile <cfg> --obliterate --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to erase all partitions and bad blocks in already flashed device\n"
            "using partition info from configuration file cfg used in nvflash earlier\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--updatebct") == 0 ||
             NvOsStrcmp(a->cmdname, "22") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "1.nvflash --bct <bct> --updatebct <bctsection> --bl <bootloader> --sync --go\n"
            "2.nvflash --blob <blob> --bct <bct> --updatebct <bctsection> --sync --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to update some section of system bct(bctsection) from bct specified\n"
            "this command is run in 3pserver. As of now, bctsection can be SDRAM which\n"
            "updates SdramParams and NumSdramSets field of bct, DEVPARAM updates\n"
            "DevParams, DevType and NumParamSets fields and BOOTDEVINFO updates\n"
            "BlockSizeLog2, PageSizeLog2 and PartitionSize fields and BLINFO which must be\n"
            "used with --download command\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--resume") == 0 ||
        NvOsStrcmp(a->cmdname, "-r") == 0 ||
        NvOsStrcmp(a->cmdname, "23") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --resume --read <part id> <filename> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "must be first option in command line, used when device is looping\n"
            "in 3pserver which is achieved when a command of nvflash is executed\n"
            "either without sync or go or both, can be used with all nvflash commands\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--verifypart") == 0 ||
             NvOsStrcmp(a->cmdname, "24") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "--verifypart <N/pname> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to verify contents of partition N/pname in device, hash is calculated\n"
            "and stored while downloading the data in partition and after all images\n"
            "are downloaded, content is read back from partition N/pname calculating hash\n"
            "and matching it with stored hash, use N=-1 for verifying all partitions\n"
            "must be used with --create command\n"
            "Partition number N can be found from cfg file used for flash earlier or\n"
            "Partition name(pname) can also be used as an argument\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--getbit") == 0 ||
             NvOsStrcmp(a->cmdname, "25") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --getbit <filename> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to read back the bit table to filename in binary form in miniloader\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if(NvOsStrcmp(a->cmdname, "--dumpbit") == 0 ||
            NvOsStrcmp(a->cmdname, "26") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --dumpbit --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to display the bit table read from device in text form on user\n"
            "terminal. Also gives info about bct and various bootloaders present in\n"
            "media, normally used for debugging cold boot failures\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--odm") == 0 ||
             NvOsStrcmp(a->cmdname, "27") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --odm CMD data --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to do some special diagnostics like sdram validation, fuelgauge etc\n"
            "each CMD has data associated with it\n"
            "For e.g.\n"
            "1. Sdram verification:\n"
            "   nvflash --bct <bct> --setbct --configile <cfg> --create --odmdata <N> "
            "   --odm verifysdram <N> --bl <bootloader> --go\n"
            "   ---------------------------------------------------------------------\n"
            "   used to verify sdram initialization i.e. verify sdram params in bct is\n"
            "   in accordance with actual SDRAM attached in device or not.\n"
            "   N can be either 0 or 1. '0' implies soft test where known word patterns\n"
            "   are written for each mb and read back. This word pattern validates all\n"
            "   the bits of the word. '1' implies stress test where known word patterns\n"
            "   are written on entire sdram area available and read back. This tests every\n"
            "   bit of the the entire available sdram area\n"
            "-----------------------------------------------------------------------\n\n"
            "2. FuelGaugeFwUpgrade\n"
            "   nvflash --odmdata <N>  --odm fuelgaugefwupgrade <filename1> <filename2> \n"
            "   --bl <bootloader> --go\n"
            "   ---------------------------------------------------------------------\n"
            "   To flash the fuelgauge firmware. file1 is the bqfs file having the firmware\n"
            "   details with corresponding data flash formatting data.file2 is the optional\n"
            "   dffs file and will carry the data flash related information alone.The \n"
            "   firmware upgrade tol reads the files and send the required info to the fuel\n"
            "   gauge device.\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--deviceid") == 0 ||
             NvOsStrcmp(a->cmdname, "28") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "--deviceid <devid> --transport <transport_mode> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to set the device id of target in simulation mode\n"
            "depending on chip, can be hex or dec.\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--devparams") == 0 ||
             NvOsStrcmp(a->cmdname, "29") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "--deviceid <devid> --transport <transport_mode> --devparams <pagesize> "
            "<blocksize> <totalblocks>  --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to pass logical pagesize, erase block unit size and total number of\n"
            "blocks for nvflash in simulation mode. As of this write, pagesize = 4K\n"
            "blocksize = 2M and totalblocks = media_size/blocksize. These values can be\n"
            "obtained using NvDdkBlockDevGetDeviceInfo api, a wrapper of SdGetDeviceInfo\n"
            "api in block driver, used only for nvflash in simulation mode\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--quiet") == 0 ||
             NvOsStrcmp(a->cmdname, "-q") == 0 ||
             NvOsStrcmp(a->cmdname, "30") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --quiet --bct <bct> --setbct --configfile <cfg> --create "
            "--odmdata <N> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to suppress the exccessive console output like status info while send\n"
            "or recieve data b/w host and device, can be used with all nvflash commands\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--wait") == 0 ||
             NvOsStrcmp(a->cmdname, "-w" ) == 0 ||
             NvOsStrcmp(a->cmdname, "31") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --wait --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            " --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to wait for USB cable connection before start executing any command\n"
            "can be used with any commands of nvflash\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--instance" ) == 0 ||
             NvOsStrcmp(a->cmdname, "32") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "1. nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "   --instance <N> --bl <bootloader> --go\n"
            "2. nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "   --instance <path> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "--instance is used to flash number of devices connected to system through\n"
            "  usb cable from different terminals at one go\n"
            "1. used to flash Nth usb device attached. The instance number N is given\n"
            "   as input and the Nth device in the enumeration list is flashed\n"
            "2. used to flash a specific device found at:/dev/bus/usb/xxx/yyy .This\n"
            "   path string is given as an input argument\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }

    else if (NvOsStrcmp(a->cmdname, "--transport") == 0 ||
             NvOsStrcmp(a->cmdname, "33") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "--deviceid <devid> --transport <transport_mode> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to specify the means of communication between host and target\n"
            "USB and Simulation are two modes supported as of now.Simulation is\n"
            "for nvflash in simulation mode and default is USB,used in normal cases\n"
            "while interaction with device\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--setbootdevtype") == 0 ||
             NvOsStrcmp(a->cmdname, "34") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --setbootdevtype S --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to set boot device type fuse to S which tells boot media to boot from\n"
            "used only in miniloader since cold boot uses bootrom and all settings get\n"
            "reset at that time. Unlike miniloader, BL/kernel uses pinmuxes not fuses\n"
            "S can be emmc, nand_x8, spi etc\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--setbootdevconfig") == 0 ||
             NvOsStrcmp(a->cmdname, "35") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --setbootdevconfig N --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to set boot device config fuse to N which tells slot of boot device\n"
            "to boot from, used only in miniloader since cold boot uses bootrom and\n"
            "all settings get reset at that time. BL/kernel uses pinmuxes not fuses\n"
            "N is 33 for sdmmc4 pop slot and 17 for sdmmc4 planar slot of whistler\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--diskimgopt") == 0 ||
             NvOsStrcmp(a->cmdname, "36") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --diskimgopt <N> "
            "--odmdata <N> --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to convert .dio or a .store.bin file to the new format (dio2) by\n"
            "inserting the required compaction blocks of size N, can be used with\n"
            "any nvflash commands which downloads an image to device\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--internaldata" ) == 0 ||
             NvOsStrcmp(a->cmdname, "37") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "   --instance <N> --bl <bootloader> --internaldata <blob> --go\n"
            "------------------------------------------------------------------------\n"
            "--internaldata is used to flash the sku-info,serial-id,mac-id,prod-id.\n"
            "Input is the blob containing the data in offset,size,data format.\n"
            "This blob file is created using the internal tool nvskuflash.\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--setentry" ) == 0 ||
             NvOsStrcmp(a->cmdname, "38") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --setentry N1 N2 --bl <bootloader> --go\n"
            "------------------------------------------------------------------------\n"
            "--setentry is used to specify the entry point and load address of the \n"
            "bootloader via Nvflash commandline. The first argument N1 is the bootloader\n"
            "entrypoint and second argument N2 is the bootloader loadaddress.\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--settime" ) == 0 ||
             NvOsStrcmp(a->cmdname, "39") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --settime --bl <bootloader> --go\n"
            "------------------------------------------------------------------------\n"
            "--settime is used to update the PMU RTC HW clock. The clock is updated with \n"
            "epoch time as base.The total time in seconds is received from nvflash host\n"
            "with base time as 1970.\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--buildbct") == 0 ||
            NvOsStrcmp(a->cmdname, "40") == 0)
    {
        NvAuPrintf("\n%s command usage -> \r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --buildbct <bct_cfg> <outputfile> --deviceid <deviceid>\n"
            "----------------------------------------------------\n"
            "--buildbct command creates bct file from cfg file \n"
            "bct_cfg is the cfg file of bct & is mandatory to give \n"
            "outputfile is the output bct file.It is optional & if \n"
            "not given the output file will be flash.bct \n"
            "--deviceid is optional to give & can be 0x30,0x35 etc.\n"
            "-------------------------------------------------------\n"
            );
    }
    else if (NvOsStrcmp(a->cmdname, "--writefuse") == 0 ||
             NvOsStrcmp(a->cmdname, "41") == 0)
    {
        NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --writefuse_config <config_file> --bl <bootloader> --go\n"
            "----------------------------------------------------------------\n"
            "used to specify the config file for burning fuses\n"
            "The fuse specified in the config_file will be burned.\n"
            "---------------------------------------------------------------\n"
                    );
    }
    else if (NvOsStrcmp (a->cmdname, "--versioninfo") == 0 ||
            NvOsStrcmp(a->cmdname, "43") == 0)
    {
        NvAuPrintf("\n%s command usage -> \r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --versioninfo\n"
            "        or            \n"
            "nvflash --versioninfo <majorno1> <majorno2>\n"
            "----------------------------------------------------\n"
            "used to display nvflash version information and also the\n"
            "changes between two specified major versions.\n"
            "-------------------------------------------------------\n"
            );
    }
    else if (NvOsStrcmp (a->cmdname, "--sync") == 0 ||
            NvOsStrcmp(a->cmdname, "44") == 0)
    {
        NvAuPrintf("\n%s command usage -> \r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bctfile> --setbct --configfile <cfg> "
            "--odmdata <odmdata> --sync --bl bootloader.bin --go\n"
            "----------------------------------------------------\n"
            "Writes 64 copies of the bctfile on the partition\n"
            "While coldboot the bct written on partition will be\n"
            "validated by bootrom \n"
            "---------------------------------------------------\n"
            );
    }
    else if (NvOsStrcmp (a->cmdname, "--runbdktest") == 0 ||
            NvOsStrcmp(a->cmdname, "45") == 0)
    {
        NvAuPrintf("\n%s command usage -> \r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --runbdktest <testplanfile> <display_mode> "
            "--bl nvbdktestbl.bin --go\n"
            "----------------------------------------------------\n"
            "used to run diagnostic tests. Input is given in testplan.txt.\n"
            "Display_mode can be hostdisplay or filename. Hostdisplay will\n"
            "print information on display while filename can be given to \n"
            "store results in a file \n"
            "For more information follow readme of bdktestframework\n"
            "---------------------------------------------------\n"
            );
    }
    else if (NvOsStrcmp (a->cmdname, "--skipautodetect") == 0 ||
            NvOsStrcmp(a->cmdname, "46") == 0)
    {
        NvAuPrintf("\n%s command usage -> \r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "--skipautodetect --bl <bootloader> --go\n"
            "----------------------------------------------------\n"
            "used to avoid boardID reading at miniloader level required for autodetect operations\n"
            "this can be used when boardid is not programmed or EEPROM doesnt exist\n"
            "-------------------------------------------------------\n"
            );
    }
    else if (NvOsStrcmp (a->cmdname, "--opmode") == 0 ||
            NvOsStrcmp(a->cmdname, "47") == 0)
    {
        NvAuPrintf("\n%s command usage -> \r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --bct <bct> --setbct --configfile <cfg> --create --odmdata <N> "
            "--deviceid <devid> --transport <transport_mode> --devparams <pagesize> "
            "<blocksize> <totalblocks>  --bl <bootloader> --go\n"
            "-----------------------------------------------------------------------\n"
            "used to set the operating mode in simulation. Prior that we have use nvsecuretool\n"
            " to generate signed binaries, used only for nvflash in simulation mode\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
    else if (NvOsStrcmp (a->cmdname, "--applet") == 0 ||
            NvOsStrcmp(a->cmdname, "48") == 0)
    {
        NvAuPrintf("\n%s command usage -> \r\n",a->cmdname);
        NvAuPrintf(
            "nvflash --applet <file> [commands] [options] --bl bootloader.bin --go\n"
            "-----------------------------------------------------------------------\n"
            "used to pass the applet to be downloaded by bootrom. Bootrom will give\n"
            "handle to applet after download. This applet will be responsible for\n"
            "initializing sdram and downloading bootloader.\n"
            "-----------------------------------------------------------------------\n\n"
            );
    }
#if ENABLE_NVDUMPER
    else if (NvOsStrcmp(a->cmdname, "--nvtbootramdump" ) == 0 ||
	  NvOsStrcmp(a->cmdname, "49") == 0)
    {
	  NvAuPrintf("\n%s command usage ->\r\n",a->cmdname);
	  NvAuPrintf(
	     "nvflash --nvtbootramdump <offset> <length> <filename>\n"
	     "------------------------------------------------------------------------\n"
	     "--nvtbootramdump is used to dump RAM data for debugging.This command is used in\n"
	     "recovery mod and you should use in RAM dump mode at Nvtboot level.\n"
	   );
    }
#endif

    else
    {
        NvAuPrintf("Unknown command %s \r\n",a->cmdname);
    }
}

