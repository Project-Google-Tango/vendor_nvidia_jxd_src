/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup downloader
 * @{
 */

/**
 * @file dld_main.c Icera archive file downloader via the host
 *       interface.
 *
 */

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef ANDROID
#include <unistd.h>
#endif

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "Updater.h"

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/
/* Main must be declared __cdecl when using MSVC */
#ifdef _MSC_VER
#define DLD_CDECL __cdecl
#else
#define DLD_CDECL
#endif

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
static void usage(void)
{
    fprintf(stdout, "Icera archive downloader version %s\n\n"
                    "Usage:\n"
                    "downloader <options> ARCHIVE_FILE_LIST\n"
                    "   -h              Display this help\n"
                    "   -v level        Verbose level (default: -v %d)\n"
                    "   -d name         Device name\n"
                    "   -c              Check if f/w update is needed:\n"
                    "                    will compare embedded SHA1 and new SHA1\n"
                    "                    if match, f/w removed from ARCHIVE_FILE_LIST.\n"
#ifdef _WIN32
					"                    *COM port (-d COM#),\n"
					"                    *socket (-d tcp:3456:192.168.48.2),\n"
#ifdef ENABLE_MBIM_DSS
					"                    *mbim interface (-d mbim) or (-d mbim:{EC27BE9A-A6F7-42C8-A8C0-09C23544A67E})\n"
#endif /* ENABLE_MBIM_DSS */
#endif
                    "   -s baud_rate    Device baud rate (default: -s %d)\n"
                    "   -b block_sz     Download block size (default: -b %d)\n"
                    "   -f              Flash update\n"
                    "   -p              Read device PCID\n", DLD_VERSION, VERBOSE_INFO_LEVEL, DEFAULT_SPEED, DEFAULT_BLOCK_SZ);
#ifdef _WIN32
    fprintf(stdout, "   -x enum         Could be use to autodetect the COM port:\n"
                    "                   enum is something like \"USB#Vid_XXXX&Pid_YYYY&MI_ZZ\"\n"
                    "   -y service      Used with -x option (use registry method for oldest\n"
                    "                                        hostdriver versions):\n"
                    "                   service should located under:\n"
                    "                   HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\n");
#endif
    fprintf(stdout, "   -m mode         Specify the mode at the end (default: -m %d)\n"
                    "   -w secs         Timeout to reopen the port after mode switching\n"
                    "                   (default: -w %d)\n\n", MODE_DEFAULT, DEFAULT_DELAY);
#ifdef _WIN32
    fprintf(stdout,"   ARCHIVE_FILE_LIST: path1\\file1[;path2\\file2;...;pathN\\fileN]\n");
#else
    fprintf(stdout,"   ARCHIVE_FILE_LIST: path1/file1\n"
                   "                  or: \"path1/file1[;path2/file2;...;pathN/fileN]\"\n");
#endif
}

#ifdef _WIN32
typedef struct
{
    char path[MAX_PATH];
    char service[MAX_PATH];
    char port[sizeof("COM255")]; /* 6 + 1 */
} filterinfo;

static int filter_enum(unsigned int index, char* enumpath, HKEY key, void* ctx)
{
	filterinfo* info = (filterinfo *)ctx;

    if (strstr(strupr(enumpath), strupr(info->path)))
    {
        unsigned long length = sizeof(info->port);
        memset(info->port, 0, length);
        if (RegQueryValueEx(key, "PortName", NULL, NULL, info->port, &length) == ERROR_SUCCESS)
        {
            return status_found;
        }
    }
	return status_not_found;
}

static int filter_service(char* servicename, HKEY key, void* ctx)
{
    return WalkOnEnums(servicename, filter_enum, ctx);
}

static int filter_setupdi(void* handle, unsigned long dev_inst, char* dev_path, HKEY key, void* ctx)
{
	filterinfo* info = (filterinfo *)ctx;

    if (dev_path)
    {
        if (strstr(strupr(dev_path), strupr(info->path)))
        {
            unsigned long length = sizeof(info->port);
            memset(info->port, 0, length);
            if (RegQueryValueEx(key, "PortName", NULL, NULL, info->port, &length) == ERROR_SUCCESS)
            {
                return status_found;
            }
        }
    }
    else
    {
        printf("TODO filter on class not yet implemented (option -z)\n");
    }
	return status_not_found;
}
#endif

static int check(bool test, int* error)
{
    if (test)
        *error = 1;
    return *error;
}

static int next(int* index, int* error, int max)
{
    *index = *index + 1;
    return check(*index >= max, error);
}

static int convert_status(const int dllcode)
{
    /*
    status: 0   No error
            1   Usage error
            2   Block size out of bound
            3   Unknow error
            4   Error during download
            5   At command error
            6   Device not found or in use
            7   File not found
    */
    if (dllcode == ErrorCode_Sucess)
        return 0;
    else if (dllcode == ErrorCode_DownloadError)
        return 4;
    else if (dllcode == ErrorCode_AT)
        return 5;
    else if (dllcode == ErrorCode_DeviceNotFound)
        return 6;
    else if (dllcode == ErrorCode_FileNotFound)
        return 7;
    return 3;
}

static void display_status(int verbose, const int level, int error)
{
    if (verbose >= level)
    {
        switch (error)
        {
            case 0:     fprintf(stdout, "Download Success\n"); break;
            case 9:     fprintf(stdout, "ERROR : device name option missing \"-d\"\n"); break;
            case 8:     fprintf(stderr, "ERROR : device autodetection failed\n"); break;
            case 7:     fprintf(stderr, "ERROR : File not found\n"); break;
            case 6:     fprintf(stderr, "ERROR : Device not found\n"); break;
            case 5:     fprintf(stderr, "ERROR : At command\n"); break;
            case 4:     fprintf(stderr, "ERROR : Error during download\n"); break;
            case 2:     fprintf(stderr, "ERROR : Block size out of bound\n"); break;
            case 1:     fprintf(stderr, "ERROR : Usage error\n"); break;
            default:    fprintf(stderr, "ERROR : Unknow Error\n");
        }
    }
}

int DLD_CDECL main(int argc, char *argv[])
{
    long raw_size;
    int i;
    char *option_dev_name = NULL;
    char *option_archname = NULL;
    unsigned short option_block_sz = 0;
    int option_delay = 0;
    int option_verbose = VERBOSE_INFO_LEVEL;
    int option_modechange = MODE_INVALID;
    int option_speed = 0;
    int option_flash = 0;
    int option_pcid = 0;
    int error = 0;
    int option_fw_upd_cond = 0;
    int option_logging_state = 1;
#ifdef _WIN32
    int option_class = 0;
	filterinfo info;
	unsigned char* pathfilter = NULL;
	unsigned char* service = NULL;
#endif

    /* Parse command line options */
    for (i = 1 ; i < argc ; i++)
    {
        if ((argv[i][0] == '-') && (argv[i][1] != '\0') && (argv[i][2] == '\0'))
        {
            switch (argv[i][1])
            {
            case 'c':
                option_fw_upd_cond = 1;
                break;

            case 'd':
                if (next(&i, &error, argc))
                    break;
                option_dev_name = argv[i];
                break;

            case 'l':
                if (next(&i, &error, argc))
                    break;
                option_logging_state = atoi(argv[i]);
                break;

            case 's':
                if (next(&i, &error, argc))
                    break;
                if (check(sscanf(argv[i],"%d", &option_speed) != 1, &error))
                    break;
                check(option_speed < 0, &error);
                break;

            case 'w':
                if (next(&i, &error, argc))
                    break;
                if (check(sscanf(argv[i],"%d", &option_delay) != 1, &error))
                    break;
                check(option_delay < 0, &error);
                break;

            case 'b':
                if (next(&i, &error, argc))
                    break;
                if (check(sscanf(argv[i],"%ld", &raw_size) != 1, &error))
                    break;
                if ((raw_size <= MIN_BLOCK_SZ) || (raw_size > MAX_BLOCK_SZ))
                {
                    error = 2;
                    break;
                }
                option_block_sz = (unsigned short)raw_size;
                break;

            case 'f':
                option_flash = 1;
                break;
#ifdef _WIN32
            case 'x':
                if (next(&i, &error, argc))
                    break;
                pathfilter = argv[i];
                check(strlen(pathfilter) >= sizeof(info.path), &error);
                break;

            case 'y':
                if (next(&i, &error, argc))
                    break;
                service = argv[i];
                check(strlen(service) >= sizeof(info.service), &error);
                break;

            case 'z':
                option_class = 1;
                break;
#endif
            case 'v':
                if (next(&i, &error, argc))
                    break;
                check(sscanf(argv[i],"%d", &option_verbose) != 1, &error);
                break;

            case 'm':
                if (next(&i, &error, argc))
                    break;
                if (check(sscanf(argv[i],"%d", &option_modechange) != 1, &error))
                    break;
                check(option_modechange < 0, &error);
                break;

      case 'p':
                option_pcid = 1;
                break;
            }
        }
        else if (argv[i][0] != '\0')
        {
            option_archname = argv[i];
        }
        if (error > 0)
            break;
    }
    if ((error == 0) && ((i != argc) || (i < 3)))
    {
        error = 1;
    }
    if (error == 0)
    {
#ifdef _WIN32
		Updater_Initialize(UPDATER_INITIALIZE);
        if ((option_dev_name == NULL) && pathfilter)
        {
            #define NUM_OF_GUID_TO_SEARCH    2
            CONST   GUID devclass_guids[] = { DEVCLASS_MODEM_GUID, DEVCLASS_COMPORT_GUID };
            CONST   GUID class_guids[] = { CLASS_MODEM_GUID, CLASS_COMPORT_GUID };
            int	    guid_indx;

            ZeroMemory(&info, sizeof(info));
            strcpy(info.path, pathfilter);

            if (service)
            {
                /* COM port autodetection: registry method */
                strcpy(info.service, service);
                if (WalkOnServices(service, filter_service, &info) != status_found)
                {
                     error = 8;
                }
            }
            else
            {
                /* COM port autodetection: setupdi method */
                for (guid_indx = 0 ; guid_indx < NUM_OF_GUID_TO_SEARCH; guid_indx++)
                {
                    if (status_found == WalkOnDevices(option_class ? (GUID *)&class_guids[guid_indx] : (GUID *)&devclass_guids[guid_indx], filter_setupdi, &info) )
                        break;
                }
                if (NUM_OF_GUID_TO_SEARCH == guid_indx)	
                {
                     error = 8;
                }
            }
            option_dev_name = info.port;
        }
		
#endif /* _WIN32 */
        if (error != 8)
        {
            if ((option_dev_name == NULL) || (option_dev_name[0] == '\x00'))
            {
                error = 9;
            }
            else
            {
#ifdef ANDROID
                /* In general started by RIL that has just been stopped.
                 * Wait a while for last AT cmds sent by RIL to be handled on the MDM side.
                 */
                sleep(4);
#endif
                /* Open option_dev_name */
                UPD_Handle handle = Open(option_dev_name);
                if (handle)
                {
                    /* Enable/Disable logging & verbosity */
                    SetLogSysState(handle, option_logging_state);
                    VerboseLevelEx(handle, option_verbose);

                    if (option_fw_upd_cond)
                    {
                        /* Remove already programmed files from list */
                        CheckAlreadyProgrammedFiles(handle, option_archname);
                    }

                    if (strlen(option_archname))
                    {
                        SetModeChange(handle, option_modechange);
                        SetSpeed(handle, option_speed);
                        SetDelay(handle, option_delay);
                        SetBlockSize(handle, option_pcid);
                        int res = UpdaterEx(handle, option_archname, option_flash, option_pcid);
                        error = convert_status(res);
                    }

                    Close(handle);
                }
            }
        }
#ifdef _WIN32
		Updater_Initialize(UPDATER_DEINITIALIZE);
#endif /* _WIN32 */
    }
    if (error == 1)
    {
        usage();
    }
    else
    {
        display_status(option_verbose, VERBOSE_STD_ERR_LEVEL, error);
    }
    return error;
}

