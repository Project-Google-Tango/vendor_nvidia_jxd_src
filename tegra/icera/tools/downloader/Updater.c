/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup updater
 * @{
 *
 */
 /**
 * @file Updater.c Icera archive file downloader via the host
 *       interface.
 *
 */
 /** @} END OF FILE */

/**
 * @addtogroup Comport
 * @{
 *
 */
 /**
 * @file Updater.c Icera archive file downloader via the host
 *       interface.
 *
 */
 /** @} END OF FILE */

 /**
 * @addtogroup ATcmd
 * @{
 *
 */
 /**
 * @file Updater.c Icera archive file downloader via the host
 *       interface.
 *
 */
 /** @} END OF FILE */

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#ifdef __MAC_OS__
#include <stdbool.h>
#endif

#if !defined(VISUAL_EXPORTS)
#include <unistd.h>
#endif

/*************************************************************************************************
 * Icera Header
 ************************************************************************************************/
#include "Updater.h"

#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32

#include <direct.h>
#include <malloc.h>
#include <setupapi.h>

#define DelayInSecond(D)    Sleep((D) * 1000)
#define BackgroundExec      Win32BackgroundExec
#define ReadPipe            Win32ReadPipe
#define PATH_SEP            ((char)'\\')
#define COMMAND_LINE        "cmd.exe /C \"\"%s\\%s\"%s\"%s\\%s\"\""
#define PIPE_HANDLE         HANDLE

#else

#include <syslog.h>

#define DelayInSecond(D)    sleep(D)
#define BackgroundExec      PosixBackgroundExec
#define ReadPipe            PosixReadPipe
#define PATH_SEP            ((char)'/')
#define COMMAND_LINE        "\"%s/%s\" %s \"%s/%s\""
#define PIPE_HANDLE         FILE*

#endif /* _WIN32 */

/* SHA1Init...: */
#include <sys/sha1.h>
/* open...: */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define COMMAND_LINE_LENGTH (strlen(COMMAND_LINE) - (strlen("%s") * 5))

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/
#define FW_NUMBER       "number "
#define FW_VERSION      "version "

#define FW_MODEM        "Modem"
#define FW_FT           "Factory"
#define FW_LDR          "Loader"
#define FW_MASS         "Mass"
#define ISO_ZEROCD      "ZeroCD"

#define TAG_START_NB    "<changelist>"
#define TAG_START_VER   "<versionNumber>"
#define TAG_END_NB      "</changelist>"
#define TAG_END         "</ExtendedHeader>"

#define DIGIT_SET       "0123456789"
#define DIGITS_EXTRA    DIGIT_SET " ."

#define COMMAND_LISTS    3             /* all, modem and loader */

#define MAX_PERCENT_FILE_SIZE   (0x7FFFFFFF / 100)

#ifdef _WIN32
#define OS_TYPE     "windows"
#define ASN1_TOOL   "asn1_tool.exe"
#define IDT_TIMEOUT 333

CONST GUID guids[] = { DEVCLASS_MODEM_GUID, DEVCLASS_COMPORT_GUID };
#define NB_GUIDS    ARRAY_SIZE(guids)
#define GUID_STRING "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}"
#define DEBUG_LOG(M)    OutputDebugString(M)

#else /* _WIN32 */
#if defined(__MAC_OS__)

/* MacOs */
#define OS_TYPE         "macos"
#define ASN1_TOOL       "asn1_macos"
#define DEBUG_LOG(M)    syslog(LOG_ALERT, M)

#else /* __MAC_OS__ */

/* Linux or Android */
#define OS_TYPE         "linux"
#define ASN1_TOOL       "asn1"
#ifndef ANDROID
#define DEBUG_LOG(M)    syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), M)
#else
#define DEBUG_LOG(M)
#endif

#endif /* __MAC_OS__ */
#endif /* _WIN32 */

#define ASN1_OPTION " -s EXTHDR -d -b "

const int ICERA_API ErrorCode_Error = 0;
const int ICERA_API ErrorCode_Success = 1;
const int ICERA_API ErrorCode_FileNotFound = 2;
const int ICERA_API ErrorCode_DeviceNotFound = 3;
const int ICERA_API ErrorCode_AT = 4;
const int ICERA_API ErrorCode_DownloadError = 5;
const int ICERA_API ErrorCode_ConnectionError = 6;
const int ICERA_API ErrorCode_InvalidHeader = 7;

/* Kept for backward compatibility */
const int ICERA_API ErrorCode_Sucess = 1;

const int ICERA_API ProgressStep_Error = 0;
const int ICERA_API ProgressStep_Init = 1;
const int ICERA_API ProgressStep_Download = 2;
const int ICERA_API ProgressStep_Flash = 3;
const int ICERA_API ProgressStep_Finalize = 4;
const int ICERA_API ProgressStep_Finish = 5;

// TODO to be removed ---------------------------------------
const int ICERA_API RSASigByteLen  = RSA_SIGNATURE_SIZE;
const int ICERA_API NonceByteLen   = NONCE_SIZE;
const int ICERA_API SigTypeRsa     = ARCH_SIGN_TYPE__RSA;
const int ICERA_API KeyIdByteLen   = KEY_ID_BYTE_LEN;
const int ICERA_API PcidByteLen    = SHA1_DIGEST_SIZE;
const int ICERA_API ArchHdrByteLen = ARCH_HEADER_BYTE_LEN;
const int ICERA_API ImeiLen        = IMEI_LENGTH;
const int ICERA_API MaxHeaderLen   = sizeof(tAppliFileHeader);
const int ICERA_API ZipArchVer     = ZIP_ARCH_VER_1_0;
// ----------------------------------------------------------

const int ICERA_API status_bad_parameter        = -32000;
const int ICERA_API status_no_enough_memory     = -1;
const int ICERA_API status_error                = 0;
const int ICERA_API status_not_found            = 0;
const int ICERA_API status_success              = 1;
const int ICERA_API status_found                = 1;

static int debug_level = 0;

/* used along with the legacy (deprecated) API, Updater */
static bool disable_full_coredump = 0;
static bool global_check_mode_restore = true;

#define UPD_MAX_HANDLES 10
#define DEFAULT_HANDLE (1)
#define VALID_HANDLE(h) (((h) > DEFAULT_HANDLE) && ((h) <= UPD_MAX_HANDLES))

/* Upgrade reasons */
enum
{
    FILE_NO_UPDATE = 0,          /* File does not need to be upgraded */
    FILE_UPDATE_NO_VERSION_DATA, /* File should be upgraded as there is no version number to tell for sure */
    FILE_UPDATE_NEWER            /* File should be upgraded as it is definitely newer than the existing one */
} ;

#define PKGV_EXTENSION ".pkgv"
#define IS_PKGV_FILE(_file) ((strlen((_file)) > (sizeof(PKGV_EXTENSION) - 1)) && (strcmp((_file) + strlen((_file)) - sizeof(PKGV_EXTENSION) + 1, PKGV_EXTENSION) == 0))
#define PKGV_LEN_VERSION    16
#define PKGV_LEN_DATE       20
#define PKGV_LEN_SVN        2

#define KILOBYTE            1024
#define MEGABYTE            (KILOBYTE * KILOBYTE)
#define GET_FILE_WEIGHT(F)  (((FileSize(F) / KILOBYTE) + 1) * 2)

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/
#ifndef WIN32
static int min(int a, int b)
{
    return (a<b) ? a:b;
}
#endif

static ICERA_API int SwitchModeEx(UPD_Handle handle, unsigned int mode, bool checkSuccess);
/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/

/* Index 0 is used for the case where no handle is used (keep old API working),
 * user level handles start from index 1 (handle 2)
 * Handles start from 1, to be able to use 0 as an invalid handle.
 */
UPD_Handle_s handles[UPD_MAX_HANDLES + 1];

/* Constants */
static const char *arch_sign_type_table[] =
{
    "SHA1",
    "SHA1/RSA",
    "SHA2",
    "SHA2/RSA",
};

static const tArchFileProperty arch_type[] = DRV_ARCH_TYPE_TABLE_ON_PC;
static const unsigned int arch_type_max_id = ARRAY_SIZE(arch_type);

/* These commands are logged in all modes */
static const char *log_commands_all[] =
{
    AT_CMD_GMR,
    AT_CMD_IFWR
};

/* These commands are logged in modem mode */
static const char *log_commands_mdm[] =
{
    AT_CMD_ICOMPAT
};

/* These commands are logged in loader mode */
static const char *log_commands_ldr[] =
{
    AT_CMD_IFLIST,
    AT_CMD_CHIPID
};

static const char **log_commands[COMMAND_LISTS] =
{
    log_commands_all,
    log_commands_mdm,
    log_commands_ldr
};

static const unsigned int log_commands_max[COMMAND_LISTS] =
{
    ARRAY_SIZE(log_commands_all),
    ARRAY_SIZE(log_commands_mdm),
    ARRAY_SIZE(log_commands_ldr)
};

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/
static void LogTime(UPD_Handle handle)
{
    time_t t;

    time(&t);
    fprintf(HDATA(handle).log_file, "%.24s: ", ctime(&t));
}

static void OutLog(UPD_Handle handle, const char *f, va_list ap)
{
    MUTEX_LOCK(HDATA(handle).mutex_log);
    if (HDATA(handle).log_callback)
    {
        int length = vsnprintf(HDATA(handle).log_buffer, MAX_LOGMSG_SIZE, f, ap);
        if ((length < 0) || (length >= MAX_LOGMSG_SIZE))
        {
            HDATA(handle).log_buffer[MAX_LOGMSG_SIZE - 1] = EOS;
        }
        HDATA(handle).log_callback(HDATA(handle).log_buffer);
    }
    if (HDATA(handle).log_file)
    {
        LogTime(handle);

        if (f)
        {
            /* For the debug log, skip leading newlines */
            while (*f == '\n')
            {
                f++;
            }

            vfprintf(HDATA(handle).log_file, f, ap);
        }

        fflush(HDATA(handle).log_file);

        /* TODO: Do we need to check the size of the file here too ?
         *       It is probably fair enough to assume that not too much
         *       will get added through one run, so if it is fine at the
         *       start, it will be fine to ignore it here.
         */
    }
    MUTEX_UNLOCK(HDATA(handle).mutex_log);
}

static void DebugLog(UPD_Handle handle, int level, const char *f, va_list ap)
{
    if (debug_level >= level)
    {
        int length, offset;
        char message[256];
        if (handle == DEFAULT_HANDLE)
        {
            strcpy(message, "Default: ");
        }
        else
        {
            length = snprintf(message, sizeof(message), "Hdl%d: ", (int)handle);
            if ((length < 0) || ((size_t)length >= sizeof(message)))
            {
                message[sizeof(message) - 1] = EOS;
            }
        }
        offset = strlen(message);
        length = vsnprintf(&message[offset], sizeof(message) - offset, f, ap);
        if ((length < 0) || (length >= (int)(sizeof(message) - offset)))
        {
            message[sizeof(message) - 1] = EOS;
        }
        DEBUG_LOG(message);
    }
}

/* Shared with other modules, but not public API */
extern void  OutDebug(UPD_Handle handle, const char *f, ...)
{
    va_list ap;

    va_start(ap, f);
    DebugLog(handle, 1, f, ap);
    va_end(ap);
}

extern void SetUpdaterDebugLevel(int level)
{
	debug_level = level;
}

extern void  OutError(UPD_Handle handle, const char *f, ...)
{
    va_list ap;

    if (HDATA(handle).option_verbose >= VERBOSE_STD_ERR_LEVEL)
    {
        va_start(ap, f);
        vfprintf(stderr, f, ap);
        va_end(ap);
        fflush(stderr);
    }
    va_start(ap, f);
    DebugLog(handle, 2, f, ap);
    va_end(ap);
    /* We always want the output in the debug log */
    va_start(ap, f);
    OutLog(handle, f, ap);
    va_end(ap);
}

extern void OutStandard(UPD_Handle handle, const char *f, ...)
{
    va_list ap;

    if ( (HDATA(handle).option_verbose >= VERBOSE_INFO_LEVEL) && (!HDATA(handle).log_callback) )
    {
        va_start(ap, f);
        vfprintf(stdout, f, ap);
        va_end(ap);
        fflush(stdout);
    }
    va_start(ap, f);
    DebugLog(handle, 3, f, ap);
    va_end(ap);
    /* ABE: I didn't yet find why but this code couldn't be moved above, it breaks Python callback mechanism */
    /* TODO: debug the issue... */
    /* For the moment, please don't try to optimize...*/
    /* We always want the output in the debug log */
    va_start(ap, f);
    OutLog(handle, f, ap);
    va_end(ap);
}

static void OutStandardAtProg(UPD_Handle handle, const char *f, ...)
{
    char        resp[128];
    va_list     ap;
    int         progress, length;

    va_start(ap, f);
    length = vsnprintf(resp, sizeof(resp), f, ap);
    if ((length < 0) || ((size_t)length >= sizeof(resp)))
    {
        resp[sizeof(resp) - 1] = EOS;
    }
    va_end(ap);
    /* Get pourcent value from the AT%PROG=2 response*/
    if (sscanf(resp, "%%PROG: %d", &progress) != 1) progress = -1;
    if (HDATA(handle).option_verbose >= VERBOSE_INFO_LEVEL)
    {
        if (HDATA(handle).option_verbose >= VERBOSE_DUMP_LEVEL)
        {
            /* Display full AT%PROG=2 response */
            OutStandard(handle, "%s", resp);
        }
        else
        {
            if (strstr(resp, "OK"))
            {
                OutStandard(handle, "100%%\n");
            }
            else
            {
                if (progress >= 0)
                {
                    OutStandard(handle, "%2d%%\r", progress);
                }
            }
        }
    }

    if (progress >= 0)
    {
        HDATA(handle).progress.percent = HDATA(handle).progress.current.base + (progress * HDATA(handle).progress.current.weight);
        //printf("Global: %d%%\n", HDATA(handle).progress.percent / HDATA(handle).progress.totalweight);
    }
}

static bool FileExists(char* filename)
{
    FILE* file = fopen(filename, "r");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

static int FileSize(char* filename)
{
    int size = -1;
    FILE* file = fopen(filename, "r");
    if (file)
    {
        fseek(file , 0, SEEK_END);
        size = ftell(file);
        fclose(file);
    }
    return size;
}

static int ReadDebugLevel(void)
{
    int level = 3;
    if (!FileExists("debuglevel.dbg"))
    {
        level--;
        if (!FileExists("debuglevel.err"))
        {
            level--;
            if (!FileExists("debuglevel.std"))
            {
                level--;
            }
        }
    }
    return level;
}

static void InitHandle(UPD_Handle handle)
{
    int idx;

    MUTEX_CREATE(HDATA(handle).mutex_log);
    HDATA(handle).option_verbose            = VERBOSE_DEFAULT_LEVEL;
    HDATA(handle).option_modechange         = MODE_DEFAULT;
    HDATA(handle).option_speed              = DEFAULT_SPEED;
    HDATA(handle).option_delay              = DEFAULT_DELAY;
    HDATA(handle).option_block_sz           = DEFAULT_BLOCK_SZ;
    HDATA(handle).option_dev_name           = NULL;
    HDATA(handle).disable_full_coredump     = 0;
    HDATA(handle).option_check_mode_restore = true;
    HDATA(handle).com_hdl                   = 0;
    HDATA(handle).com_settings.flow_control = 1;
    for (idx=0 ; idx<(int)ARRAY_SIZE(HDATA(handle).logging_done) ; idx++)
    {
        HDATA(handle).logging_done[idx] = false;
    }
    HDATA(handle).log_sys_state = 1;
    HDATA(handle).option_send_nvclean = DEFAULT_SEND_NVCLEAN;
    HDATA(handle).option_check_pkgv = DEFAULT_CHECK_PKGV;
    HDATA(handle).option_update_in_modem_mode = DEFAULT_UPDATE_IN_MODEM;

    HDATA(handle).updating_in_modem_mode = false;

    HDATA(handle).used = 1;
}

static void updater_load(void)
{
	int length;
    char message[256];
    char curdir[FILENAME_MAX];

    // Initialization code
    InitHandle(DEFAULT_HANDLE);
    debug_level = ReadDebugLevel();
    message[0] = '\0';
    strncat(message, "Library version ", sizeof(message) - strlen(message) - 1);
    strncat(message, DLD_VERSION, sizeof(message) - strlen(message) - 1);
    strncat(message, " is loaded", sizeof(message) - strlen(message) - 1);
    DEBUG_LOG(message);
    if (getcwd(curdir, sizeof(curdir)))
    {
        DEBUG_LOG(curdir);
    }
    message[0] = '\0';
    length = snprintf(message, sizeof(message) - 1, "Debug level: %d", debug_level);
    message[length == -1 ? (int)sizeof(message) - 1 : length] = EOS;
    DEBUG_LOG(message);
}

static void updater_unload(void)
{
    unsigned long  idx;
    // free all resources
    for (idx = 1 ; idx < UPD_MAX_HANDLES ; idx++)
    {
        if (handles[idx].used)
        {
            Close(idx + 1);
        }
    }
    FreeHandle(DEFAULT_HANDLE);
    DEBUG_LOG("Library unloaded");
}

static unsigned long ComputeChecksum32(void * src, int lg)
{
    unsigned long *p = (unsigned long *)src;
    unsigned long chksum = 0;
    unsigned long * end = p + lg/sizeof(unsigned long);

    while (p < end)
    {
        chksum ^= *p++;
    }

    return(unsigned long)chksum;
}

/* Return a pointer if one character from the set is found or NULL */
static char* getNext(char* str, char* set)
{
    unsigned int    idx;
    char*           next;

    next = NULL;
    if ((str != NULL) && (set != NULL))
    {
        if (*str != EOS)
        {
            next = str;
            do
            {
                for(idx=0 ; idx < strlen(set) ; idx++)
                    if (*next == set[idx]) return next;
                next++;
            }
            while(*next != EOS);
        }
    }
    return next;
}

/* Replace all character from the separator list to EOS and return the number of substring */
static int splitString(char* list, char* separator)
{
    char* str;
    char* next;
    int count = 0;

    if ((list != NULL) && (separator != NULL))
    {
        if ((*list != EOS) && (*separator != EOS))
        {
            next = list;
            do
            {
                /* Strip */
                do
                {
                    str = next;
                    next = getNext(str, separator);
                    if (next == NULL) return count;
                    if (*next == EOS) return count + 1;
                    *next = EOS;
                    next++;
                }
                while (next == &str[1]);
                count++;
            }
            while (*next != EOS);
        }
    }
    return count;
}

static int getFirmwareInfo(UPD_Handle handle, char* firmware_version, char* type, char *acceptable_characters, char* tag, char* number, int size)
{
    size_t  length;
    char*   scan;

    do
    {
        char* tmp;
        scan = strstr(firmware_version, tag);
        if (scan == NULL)
        {
            OutError(handle, "\n#Error: Application [%s] not found in [%s]\n\n", tag, firmware_version);
            break;
        }
        tmp = scan;
        scan = strstr(tmp, type);
        if (scan == NULL)
        {
            OutError(handle, "\n#Error: Field [%s] not found in [%s]\n\n", type, tmp);
            break;
        }
        memset(number, EOS, size);
        scan += strlen(type);
        length = strspn(scan, acceptable_characters);
        if (length == 0)
        {
            OutError(handle, "\n#Error: Field [%s] empty in [%s]\n\n", type, scan);
            break;
        }
        strncpy(number, scan, length);
        return 1;
    }
    while(0);
    return 0;
}

#ifdef _WIN32

static int Win32ReadPipe(UPD_Handle handle, PIPE_HANDLE pipe, void* buffer, int size)
{
    DWORD read;

    if (!ReadFile(pipe, buffer, size, &read, NULL))
    {
        OutError(handle, "\n#Error: ReadFile error %08X\n\n", GetLastError());
        return -1;
    }
    return (int)read;
}

static int Win32BackgroundExec(UPD_Handle handle, char* command, PIPE_HANDLE* pipe)
{
    int result = 0;

    /* [Caller] outPipe <-[pipe]-o inPipe [ChildProcess]*/
    HANDLE                  inPipe;
    HANDLE                  outPipe = NULL;
    PROCESS_INFORMATION     procInfo;
    STARTUPINFO             startInfo;
    SECURITY_ATTRIBUTES     attrib;

    if (pipe)
    {
        do
        {
            attrib.nLength              = sizeof(attrib);
            attrib.bInheritHandle       = TRUE;
            attrib.lpSecurityDescriptor = NULL;
            if (!CreatePipe(&outPipe, &inPipe, &attrib, 0))
            {
                OutError(handle, "\n#Error: CreatePipe failed %08X\n\n", GetLastError());
                break;
            }
            if (!SetHandleInformation(outPipe, HANDLE_FLAG_INHERIT, 0))
            {
                OutError(handle, "\n#Error: SetHandleInformation failed %08X\n\n", GetLastError());
                break;
            }
            memset(&procInfo, 0, sizeof(procInfo));
            memset(&startInfo, 0, sizeof(startInfo));
            startInfo.cb            = sizeof(startInfo);
            startInfo.wShowWindow   = SW_HIDE;
            startInfo.hStdOutput    = inPipe;
            startInfo.hStdInput     = NULL;
            startInfo.hStdError     = startInfo.hStdOutput;
            startInfo.dwFlags      |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            OutDebug(handle, "Command [%s]", command);
            if (!CreateProcess(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &startInfo, &procInfo))
            {
                OutError(handle, "\n#Error: CreateProcess [%s] failed %08X\n\n", command, GetLastError());
                break;
            }
            CloseHandle(procInfo.hProcess);
            CloseHandle(procInfo.hThread);
            result = 1;
        }
        while(0);
        *pipe = outPipe;
    }
    return result;
}

#else /* _WIN32 */

static int PosixReadPipe(UPD_Handle handle, PIPE_HANDLE pipe, void* buffer, int size)
{
    return fread(buffer, size, 1, pipe);
}

static int PosixBackgroundExec(UPD_Handle handle, char* command, PIPE_HANDLE* pipe)
{
    int result = 0;
    if (pipe)
    {
        result = 1;
        *pipe = popen(command, "r");
        if (*pipe == NULL)
        {
            OutError(handle, "\n#Error: popen [%s] error\n\n", command);
            result = 0;
        }
    }
    return result;
}

#endif /* #ifdef _WIN32 */

static bool SplitFilePath(char* filepath, char** path, char** file)
{
    if (filepath)
    {
        char* sep = strrchr(filepath, PATH_SEP);
        if (file)
        {
            *file = strdup(sep ? sep + 1 : filepath);
            if (**file == EOS)
            {
                return false;
            }
        }
        if (path)
        {
            if (sep && (sep > filepath))
            {
                *sep = EOS;
                *path = strdup(filepath);
                *sep = PATH_SEP;
            }
            else
            {
                *path = strdup(".");
            }
        }
        return true;
    }
    return false;
}

static char* JoinFilePath(char* path, char* file)
{
    int length = strlen(path) + sizeof(PATH_SEP) + strlen(file) + 1;
    char* filepath = (char *)malloc(length);
    if (filepath)
    {
        int size = snprintf(filepath, length, "%s%c%s", path, PATH_SEP, file);
        if ((size < 0) || (size >= length))
        {
            free(filepath);
            filepath = NULL;
        }
    }
    return filepath;
}

static int GetAsn1ExtHdr(UPD_Handle handle, char* file, char* buffer, int bufferLength)
{
    int result = 0;
    PIPE_HANDLE pipe;
    char* path = NULL;
    char* filename = NULL;

    if (file && (bufferLength > 0))
    {
        OutDebug(handle, "GetAsn1ExtHdr [%s]", file);
        /* Extract path from file parameter (split in path+filename) */
        if (SplitFilePath(file, &path, &filename))
        {
            do
            {
                int size;
                struct stat attributs;
                /* Allocate local command line buffer */
                size_t length = strlen(ASN1_TOOL) + strlen(ASN1_OPTION) + strlen(filename) +
                                2 * strlen(path) + COMMAND_LINE_LENGTH + 1/* EOS */;
                char* command = (char *)alloca(length);
                if (!command)
                {
                    OutError(handle, "\n#Error: Alloc on stack %d failed\n\n", length);
                    break;
                }
                /* Check if ASN1 tool */
                size = snprintf(command, length, "%s%c%s", path, PATH_SEP, ASN1_TOOL);
                if ((size < 0) || (size >= (int)length))
                {
                    OutError(handle, "\n#Error: Buffer overflow %d/%d\n\n", strlen(path), length);
                    break;
                }
                stat(command, &attributs);
                if (!S_ISREG(attributs.st_mode))
                {
                    OutDebug(handle, "Warning: ASN1 tool missing in [%s] to decode [%s]\n\n", path, filename);
                    result = -1;
                    break;
                }
                /* Build command line */
                size = snprintf(command, length, COMMAND_LINE, path, ASN1_TOOL, ASN1_OPTION, path, filename);
                if ((size < 0) || (size >= (int)length))
                {
                    OutError(handle, "\n#Error: Buffer overflow %d,%d/%d)\n\n", strlen(path), strlen(filename), length);
                    break;
                }
                OutDebug(handle, "Command %s, file=%s, path=%s", command, filename, path);
                if (BackgroundExec(handle, command, &pipe))
                {
                    int read;
                    DelayInSecond(1);
                    memset(buffer, 0, bufferLength);
                    read = ReadPipe(handle, pipe, buffer, bufferLength - 1/* EOS */);
                    if (read >= 0)
                    {
                        /* TAG_END_NB has been replaced by TAG_END to be compatible with old ASN1 where CL field wasn't present */
                        result = strstr(buffer, TAG_END) ? 1 : 0;
                        if (!result)
                        {
                            OutError(handle, "\n#Error: %s missing in the ReadPipe echo [%s]\n\n", TAG_END, buffer);
                        }
                    }
                    else
                    {
                        OutError(handle, "\n#Error: ReadPipe %d\n\n", read);
                    }
                }
            }
            while (false);
            free(path);
            free(filename);
        }
        OutDebug(handle, "Result %d [%s]", result, buffer);
    }
    return result;
}

static int ReadExtHdrFromFile(UPD_Handle handle, char *file, unsigned int *changelist, unsigned int *version)
{
    size_t          length;
    char*           str;
    char            read[1024/* enough to get the full <ExtendedHeader>...</ExtendedHeader> field */];
    int             background_result = 0, status = 1, i;
    unsigned int    *ptrs[2] = {changelist, version};
    char            *names[2] = {TAG_START_NB, TAG_START_VER};

    background_result = GetAsn1ExtHdr(handle, file, read, sizeof(read));
    if (background_result > 0)
    {
        for (i = 0; i < (int)ARRAY_SIZE(ptrs); i++)
        {
            if (ptrs[i])
            {
                str = strstr(read, names[i]);
                if (str == NULL)
                {
                    OutError(handle, "\n#Error:  No [%s] ASN1 field in [%s]\n\n", names[i], read);
                    status = -1;
                    break;
                }
                str += strlen(names[i]);
                length = strspn(str, DIGIT_SET);
                if (length == 0)
                {
                    OutError(handle, "\n#Error: ASN1 Field [%s] empty in [%s]\n\n", names[i], read);
                    status = 0;
                    break;
                }
                str[length] = EOS;
                *ptrs[i] = atoi(str);
                OutDebug(handle, "ExtHdr%d: (%s/%d) %d=%s", i, names[i], length, *ptrs[i], str);
            }
        }
    }
    else if (background_result != -1)
    {
        OutError(handle, "\n#Error: ASN1 Command on [%s] file is failed\n\n", file);
        status = 0;
    }
    OutDebug(handle, "ExtHdr: %d", status);
    return status;
}

static int CheckFWVers(UPD_Handle handle, unsigned long id, char* firmware_version, char* file)
{
    unsigned int    res = 0, wrapped = 0, current = 0;
    char            number[10/* 32bits */ + 1/*=*/ + 1/* EOS */];

    if (ReadExtHdrFromFile(handle, file, &wrapped, NULL) > 0)
    {
        OutDebug(handle, "ExtHdr: %s number=%d", file, wrapped);
        switch(id)
        {
        case ARCH_ID_APP:  res = getFirmwareInfo(handle, firmware_version, FW_NUMBER, DIGIT_SET, FW_MODEM, number, sizeof(number)); break;
        case ARCH_ID_IFT:  res = getFirmwareInfo(handle, firmware_version, FW_NUMBER, DIGIT_SET, FW_FT,    number, sizeof(number)); break;
        case ARCH_ID_LDR:  res = getFirmwareInfo(handle, firmware_version, FW_NUMBER, DIGIT_SET, FW_LDR,   number, sizeof(number)); break;
        case ARCH_ID_MASS: res = getFirmwareInfo(handle, firmware_version, FW_NUMBER, DIGIT_SET, FW_MASS,  number, sizeof(number)); break;
        }
        if (res)
        {
            OutDebug(handle, "FW info: %s=%s", FW_NUMBER, number);
            current = atoi(number);
            res = (wrapped > current) ? 1 : 0;
            OutStandard(handle, "\nFile [%s] version: %d\nFirmware Id %d version: %d\n%s\n\n",
                        file, wrapped, id, current, res ? "More recent" : "Up to date");
        }
    }
    return res;
}

/**
 * Makes the right decision about updating an ISO image or not
 * The logic is a bit complex as the ISOs did not use to be versioned
 * so we need to maintain legacy behaviour as well as new behaviour, both with respect to the target firmware and the ISO images
 *
 * The logic is:
 *    o If there is version data from both target and new file, then
 *        * If the new version is bigger than the old version, update
 *        * else if the new version is smaller than the current version then do not update
 *        * else if the new changelist is bigger than the current changelist, update
 *        * else do not update (versions are the same and the changelists are the same, or the existing changelist is bigger than the new one)
 *    o If there is version data from the target but not from the new file
 *        * Do not update, as the new file is bound to be older
 *    o If there is no data from target and new file
 *        * Update if firmware needs to be updated
 *    o If there is no data from the target, but data from the new file
 *        * If there is no data from the target because it does not report version data for ISO images, update if firmware needs to be updated (effectively old behaviour)
 *        * If there is no data from the target because the ISO currently flashed does not have version data, update as the new file is bound to be newer
 *
 * @param firmware_version The output from AT%IFWR on the target we're checking the new file against
 * @param new_file_has_data 1 if the new ISO has version data, 0 if not
 * @param target_has_data 1 if the target has version information about the currently loaded ISO image, 0 if not
 * @param new_version The version of the new ISO file if available. Ignored if new_file_has_data is 0
 * @param current_version The version of the currently loaded ISO file if available. Ignored if target_has_data is 0
 * @param new_changelist The CL of the new ISO file if available. Ignored if new_file_has_data is 0
 * @param current_changelist The CL of the currently loaded ISO file if available. Ignored if target_has_data is 0
 *
 * @return FILE_NO_UPDATE if the ISO should not be updated, FILE_UPDATE_NEWER if the new ISO file is definitely newer and should be updated, or
 *         FILE_UPDATE_NO_VERSION_DATA to use the old semantics and update the ISO if any firmware file is updated.
 */
static int UpdateISODecision(char *firmware_version, int new_file_has_data, int target_has_data, int new_version, int current_version, int new_changelist, int current_changelist)
{
    /* Default to no upgrade */
    int             update = FILE_NO_UPDATE;

    if (new_file_has_data && target_has_data)
    {
        /* Ok, we have data from both target and new file for both CL and version: */
        if (new_version > current_version)
        {
            update = FILE_UPDATE_NEWER;
        }
        else if (new_version < current_version)
        {
            update = FILE_NO_UPDATE;
        }
        else if (new_changelist > current_changelist)
        {
            update = FILE_UPDATE_NEWER;
        }
        /* otherwise,  versions are the same and changelists are the same or the existing one is newer than the file we're checking */
    }
    else if (!new_file_has_data && target_has_data)
    {
        /* No data in new file, but data from target, so no upgrade */
        update = FILE_NO_UPDATE;
    }
    else if (!new_file_has_data && !target_has_data)
    {
        /* No data in new file, no data from target - use old style and upgrade if other files are upgraded */
        update = FILE_UPDATE_NO_VERSION_DATA;
    }
    else
    {
        /* data from new file, nothing from target, so check whether there is nothing from target as it is not reported, or because there is no data in the target file */
        if (strstr(firmware_version, ISO_ZEROCD))
        {
            /* There is an entry for Zero CD, so we have no data as there is nothing in the file */
            update = FILE_UPDATE_NEWER;
        }
        else
        {
            /* Target doesn't report ISO data, so use the old style logic as we don't know what is in the old file */
            update = FILE_UPDATE_NO_VERSION_DATA;
        }
    }

    return update;
}


/**
 * Checks whether to update an ISO image or not
 * Attempts to extract the ISO version from the IFWR output, read the data from the extended header of the new ISO and apply some logic to decide whether to
 * update or not
 *
 * @param handle A valid updater library handle
 * @param firmware_version The output from AT%IFWR on the target we're checking the new file against
 * @param file The new ISO image to possibly update with
 *
 * @return FILE_NO_UPDATE if the ISO should not be updated, FILE_UPDATE_NEWER if the new ISO file is definitely newer and should be updated, or
 *         FILE_UPDATE_NO_VERSION_DATA to use the old semantics and update the ISO if any firmware file is updated.
 */
static int CheckISOVers(UPD_Handle handle, char* firmware_version, char* file)
{
    unsigned int    new_changelist = 0, new_version = 0, current_changelist = 0, current_version = 0;
    char            number[16]; /* big enough for both CL and version number */
    int             new_file_has_data = 0;
    int             target_has_data = 0;

    if (getFirmwareInfo(handle, firmware_version, FW_NUMBER, DIGIT_SET, ISO_ZEROCD, number, sizeof(number)))
    {
        OutDebug(handle, "FW info: %s=%s", FW_NUMBER, number);
        current_changelist = atoi(number);
        OutStandard(handle, "\nFile [%s] version: %d\nTarget version: %d\n%s\n\n",
                    file, new_changelist, current_changelist, (new_changelist > current_changelist) ? "More recent" : "Up to date");

        /* Read version number */
        if (getFirmwareInfo(handle, firmware_version, FW_VERSION, DIGITS_EXTRA, ISO_ZEROCD, number, sizeof(number)))
        {
            int parsed;
            unsigned int bytes[4];
            OutDebug(handle, "FW info: %s=%s", FW_VERSION, number);
#ifdef __BIG_ENDIAN__
            parsed = sscanf(number, "%d.%d.%d.%d", &bytes[3], &bytes[2], &bytes[1], &bytes[0]);
#else
            parsed = sscanf(number, "%d.%d.%d.%d", &bytes[0], &bytes[1], &bytes[2], &bytes[3]);
#endif
            if (parsed == 4)
            {
                current_version = ((bytes[0] << 24) & 0xFF000000) | ((bytes[1] << 16) & 0xFF0000) | ((bytes[2] << 8) & 0xFF00) | ((bytes[3]) & 0xFF);
                OutStandard(handle, "\nFile [%s] version: %d\nTarget version: %d\n%s\n\n",
                            file, new_version, current_version, (new_version > current_version) ? "More recent" : "Up to date");

                target_has_data = 1;
            }
            else
            {
                OutError(handle, "\n#Error: Unable to parse field [%s] in [%s]\n\n", FW_VERSION, firmware_version);
            }
        }
        else
        {
            /* No parsable version number in the target data */
            OutError(handle, "\n#Error: Unable to parse field [%s] in [%s]\n\n", FW_VERSION, firmware_version);
        }
    }
    else
    {
        /* No parsable CL in the target data */
        OutError(handle, "\n#Error: Unable to parse field [%s] in [%s]\n\n", FW_NUMBER, firmware_version);
    }

    if (ReadExtHdrFromFile(handle, file, &new_changelist, &new_version) > 0)
    {
        OutDebug(handle, "ExtHdr: %s number=%d version=%d", file, new_changelist, new_version);
        new_file_has_data = 1;
    }
    else
    {
        OutError(handle, "\n#Error: Unable to read extended header in %s\n\n", file);
    }
    return UpdateISODecision(firmware_version, new_file_has_data, target_has_data, new_version, current_version, new_changelist, current_changelist);
}


static int GetArchTableEntry(int arch_id)
{
    unsigned int i;
    int err = -1;

    for(i=0; i< arch_type_max_id; i++)
    {
        if(arch_type[i].arch_id == arch_id)
        {
            return i;
        }
    }
    return err;
}

#ifdef __BIG_ENDIAN__
static  unsigned long endian_swap(unsigned long x)
{
    return (x>>24) |
           ((x<<8) & 0x00FF0000) |
           ((x>>8) & 0x0000FF00) |
           (x<<24);
}

static  unsigned short endian_swap_short(unsigned short x)
{
    return (x>>8) |
           (x<<8);
}

static void convert_endian(void *buffer, int bytes)
{
    unsigned long *long_ptr = (unsigned long *)buffer;

    while ((void *)long_ptr < (void *)((unsigned char *)buffer + bytes))
    {
        *long_ptr = endian_swap(*long_ptr);
        long_ptr++;
    }
}
#endif

/* Note: This function returns the integers in host byte order so the data is usable (it does not change the rfu field) */
static int GetWrappedHeader(UPD_Handle handle, tAppliFileHeader* arch_header, char* file, const char* step)
{
    int arch_entry;
    int arch_file_id;
    int header_sz = 0;
    int size = FileSize(file);

    OutDebug(handle, "%sheader: %s", step ? step : "", file);
    if (size > 0)
    {
        FILE* arch_desc = fopen(file, "rb");
        if (arch_desc)
        {
            do
            {
                memset(arch_header, 0, sizeof(*arch_header));
                OutDebug(handle, "File size: %d", size);
                /* Read Tag and Length */
                if (size < (int)(sizeof(arch_header->tag) + sizeof(arch_header->length)))
                {
                    OutError(handle, "\n#Error: incorrect archive format (size=%d) [%s]\n\n", size, file);
                    break;
                }
                fread(arch_header, sizeof(arch_header->tag) + sizeof(arch_header->length), 1, arch_desc);
                #ifdef __BIG_ENDIAN__
                convert_endian(arch_header, sizeof(arch_header->tag) + sizeof(arch_header->length));
                #endif

                /* Check file format */
                OutDebug(handle, "Header size: %d", arch_header->length);
                if ((arch_header->length <= (sizeof(arch_header->tag) + sizeof(arch_header->length))) ||
                    (size < (int)arch_header->length))
                {
                    OutError(handle, "\n#Error: incorrect archive format [%s]\n\n", file);
                    break;
                }
                OutDebug(handle, "Header tag: %08X", arch_header->tag);

                if  ((arch_header->tag != ARCH_TAG_8060)
                     && (arch_header->tag != ARCH_TAG_9040))
                {
                    OutError(handle, "\n#Error: incorrect archive tag %08X [%s] (not a wrapped file?)\n\n", arch_header->tag, file);
                    break;
                }
                /* Adjust header_sz.
                   [Warning: different calculation of size between BROM A0 and other BROM versions...]
                   header_sz = sizeof(arch_header->length) + sizeof(arch_header->length) + arch_header->length;
                 */

                /* Read Value field */
                fread(&arch_header->file_size, arch_header->length - sizeof(arch_header->tag) - sizeof(arch_header->length), 1, arch_desc);
                #ifdef __BIG_ENDIAN__
                convert_endian(&arch_header->file_size, arch_header->length - sizeof(arch_header->tag) - sizeof(arch_header->length));
                #endif

                /* Check filesize field */
                if (size != (int)(arch_header->file_size + arch_header->length))
                {
                    OutError(handle, "\n#Error: Header has incorrect file size %d instead of %d [%s]\n\n", arch_header->file_size, size - arch_header->length, file);
                    break;
                }

                /* Get file ID */
                arch_file_id = GET_ARCH_ID(arch_header->file_id);
                arch_entry = GetArchTableEntry(arch_file_id);
                if (step)
                {
                    if(arch_entry >= 0)
                    {
                        OutDebug(handle, "%s: Size %d, %s", arch_type[arch_entry].acr,
                                 arch_header->file_size / 1024, arch_sign_type_table[arch_header->sign_type]);
                        OutStandard(handle, "\n%s %s header [%u KB, %s]...\n", step,
                                    arch_type[arch_entry].acr,
                                    arch_header->file_size / 1024,
                                    arch_sign_type_table[arch_header->sign_type]);
                    }
                    else
                    {
                        OutDebug(handle, "Arch ID %d: Size %d, %s", arch_file_id,
                                 arch_header->file_size / 1024, arch_sign_type_table[arch_header->sign_type]);
                        OutStandard(handle, "\n%s header ID=%u [%lu KB, %s]...\n", step,
                                    arch_file_id,
                                    arch_header->file_size / 1024,
                                    arch_sign_type_table[arch_header->sign_type]);
                    }
                }
                /* Check archive header */
                header_sz = arch_header->length;
                if ( ((arch_header->tag != ARCH_TAG_8060)
                      && (arch_header->tag != ARCH_TAG_9040))
                    || (ComputeChecksum32(arch_header, arch_header->length) != 0) )
                {
                    /* Patch header_sz for backward compatibility with BROM A0 */

                    header_sz += sizeof(arch_header->length) + sizeof(arch_header->length);
                    fread(&arch_header->checksum, sizeof(arch_header->length) + sizeof(arch_header->length) , 1, arch_desc);

                    if ( ((arch_header->tag != ARCH_TAG_8060)
                          && (arch_header->tag != ARCH_TAG_9040))
                        || (ComputeChecksum32(arch_header, header_sz) != 0) )
                    {
                        OutError(handle, "\n#Error: incorrect archive file tag=%08X cks=%08X [%s]\n\n", arch_header->tag,
                                 ComputeChecksum32(arch_header, header_sz), file);
                        header_sz = 0;
                        break;
                    }
                }

                #ifdef __BIG_ENDIAN__
                /* Need to put this back as it was read from the wire technically, if somebody wanted to interpret it....it's not 32 bit integers
                 * - at the moment it is PER encoded ASN.1 data
                 */
                convert_endian(&arch_header->rfu, arch_header->length - ARCH_HEADER_BYTE_LEN);
                #endif
            }
            while(0);
            fclose(arch_desc);
        }
    }
    OutDebug(handle, "Header size: %d", header_sz);
    return header_sz;
}

static int FirmwareDownload(UPD_Handle handle, tAppliFileHeader* arch_header, char* file)
{
    time_t time_start, time_current;
    int                 length;
    unsigned int        n;
    unsigned int        index;
    unsigned int        file_sz;
    unsigned short      chksum16;
    int                 error_cnt;
    int                 progress;
    int                 total_sz;
    int                 ratio_sz;
    char                log_file_name[sizeof("log9999.txt")];
    int                 debug_block_nb  = 0;
    FILE                *log_desc       = NULL;
    int                 logging_on      = 1;

    /* Open firmware archive file */
    FILE *arch_desc = fopen(file, "rb");
    OutStandard(handle, "\nOpening [%s] file ... ", file);
    if (arch_desc)
    {
        time(&time_start);
        OutStandard(handle, "Ok\n\n");
        /* Request a firmware download: This command should be sent just before to send the 1rst block */
        if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_LOAD)))
        {
            OutError(handle, "\n#Error: AT%%LOAD command failed\n\n");
            fclose(arch_desc);
            return 0;
        }

        /* Even value */
        HDATA(handle).option_block_sz >>= 1;
        HDATA(handle).option_block_sz <<= 1;

        /* Send StartDownload request */
        fseek (arch_desc , arch_header->length, SEEK_SET);
        error_cnt = 0;
        do
        {
            n = 0;

            HDATA(handle).tx_buf[n++] = 0xAA;
            HDATA(handle).tx_buf[n++] = 0x73;

            memcpy(&HDATA(handle).tx_buf[n], &HDATA(handle).option_block_sz, sizeof(HDATA(handle).option_block_sz));
            #ifdef __BIG_ENDIAN__
            /* Swap to little endian on the wire */
            *(unsigned short *)&HDATA(handle).tx_buf[n] = endian_swap_short(*(unsigned short *)&HDATA(handle).tx_buf[n]);
            #endif
            n += sizeof(HDATA(handle).option_block_sz);
            memcpy(&HDATA(handle).tx_buf[n], arch_header, arch_header->length);
            #ifdef __BIG_ENDIAN__
            /* Convert back to little endian the numbers, the rfu (ext header) field has already been done */
            convert_endian(&HDATA(handle).tx_buf[n], ARCH_HEADER_BYTE_LEN);
            #endif
            n += arch_header->length;

            chksum16 = ComputeChecksum16(HDATA(handle).tx_buf, n);
            memcpy(&HDATA(handle).tx_buf[n], &chksum16, sizeof(chksum16));
            n += sizeof(chksum16);
            length = COM_write(HDATA(handle).com_hdl, HDATA(handle).tx_buf, n);
            if (length < 0)
            {
                OutError(handle, "\n#Error: Get wrapped header COM_write %d failed [%s]\n\n", n, strerror(errno));
                fclose(arch_desc);
                return 0;
            }

            memset(HDATA(handle).rx_buf, 0, sizeof(HDATA(handle).rx_buf));
            n = 0;
            do
            {
#ifdef ANDROID
                if (COM_poll(HDATA(handle).com_hdl, &HDATA(handle).rx_buf[n], 2 - n, 20000) != 1)
                {
                    OutError(handle, "\n#Error: Get wrapped header COM_poll %d failed [%s]\n\n", 2 - n, strerror(errno));
                    fclose(arch_desc);
                    return 0;
                }
#endif
                length = COM_read(HDATA(handle).com_hdl, &HDATA(handle).rx_buf[n], 2 - n);
#ifdef ANDROID
                if (length <= 0)
#else
                if (length < 0)
#endif
                {
                    OutError(handle, "\n#Error: Get wrapped header COM_read %d failed [%s]\n\n", 2 - n, strerror(errno));
                    fclose(arch_desc);
                    return 0;
                }
                n += length;
            }
            while (n < 2);

            if ((HDATA(handle).rx_buf[0] != 0xAA) || (HDATA(handle).rx_buf[1] != 0x01))
            {
                error_cnt++;
            }

            if (error_cnt == MAX_BLOCK_REJECT)
            {
                OutError(handle, "\n#Error: block rejected [0x%.2X%.2X]\n\n", HDATA(handle).rx_buf[0], HDATA(handle).rx_buf[1]);
                fclose(arch_desc);
                return 0;
            }
        }
        while ((HDATA(handle).rx_buf[0] != 0xAA) || (HDATA(handle).rx_buf[1] != 0x01));
        /* Send Blocks ... */
        file_sz = arch_header->file_size;
        ratio_sz = 1;
        while ((file_sz >> ratio_sz) > MAX_PERCENT_FILE_SIZE) ratio_sz++;
        total_sz = file_sz >> ratio_sz;
        HDATA(handle).progress.current.base = HDATA(handle).progress.percent;
        while (file_sz > 0)
        {
            int read_sz;
            int i;

            /* send block */
            error_cnt = 0;
            debug_block_nb++;
            do
            {
                index = 0;
                read_sz = 0;
                if (error_cnt == 0)
                {
                    /* progress status */
                    progress = ((total_sz - (file_sz >> ratio_sz)) * 100) / total_sz;
                    OutStandard(handle, "%2d%%\r", progress);
                    HDATA(handle).progress.percent = HDATA(handle).progress.current.base + (progress * HDATA(handle).progress.current.weight);
                    //printf("Global: %d%%\n", HDATA(handle).progress.percent / HDATA(handle).progress.totalweight);
                    HDATA(handle).tx_buf[index++] = 0xAA;
                    HDATA(handle).tx_buf[index++] = 0x00;

                    read_sz = (int)fread(&HDATA(handle).tx_buf[index], sizeof(unsigned char), HDATA(handle).option_block_sz, arch_desc);
                    index += (unsigned int)read_sz;

                    /* One byte of padding (0x00) appended to the data to transmit */
                    /* For 16bits alignment */
                    /* Note: same modification done on platform side when receiving */
                    if ((index % 2) != 0)
                    {
                        index +=1;
                    }

                    chksum16 = ComputeChecksum16(HDATA(handle).tx_buf, index);
                    memcpy(&HDATA(handle).tx_buf[index], &chksum16, sizeof(chksum16));
                    index += sizeof(chksum16);
                }

                i = 0;
                while (i != (int)index)
                {
                    length = COM_write(HDATA(handle).com_hdl, &HDATA(handle).tx_buf[i], index - i);
                    if (length < 0)
                    {
                        OutError(handle, "\n#Error: Get wrapped block COM_write %d failed [%s]\n\n", index - i, strerror(errno));
                        fclose(arch_desc);
                        return 0;
                    }
                    else
                    {
                        i += length;
                    }
                }

                n = 0;
                memset(HDATA(handle).rx_buf, 0, sizeof(HDATA(handle).rx_buf));
                do
                {
#ifdef ANDROID
                    if (COM_poll(HDATA(handle).com_hdl, HDATA(handle).rx_buf, 2 - n, 20000) != 1)
                    {
                        OutError(handle, "\n#Error: Get wrapped block COM_poll %d failed [%s]\n\n", 2 - n, strerror(errno));
                        fclose(arch_desc);
                        return 0;
                    }
#endif
                    length = COM_read(HDATA(handle).com_hdl, HDATA(handle).rx_buf, 2 - n);
#ifdef ANDROID
                    if (length <= 0)
#else
                    if (length < 0)
#endif
                    {
                        OutError(handle, "\n#Error: Get wrapped block COM_read %d failed [%s]\n\n", 2 - n, strerror(errno));
                        fclose(arch_desc);
                        return 0;
                    }
                    n += length;
                }
                while (n < 2);

                if ((HDATA(handle).rx_buf[0] != 0xAA) || (HDATA(handle).rx_buf[1] != 0x01))
                {
                    int line_nb = 0;
                    int go;

                    /* Open log-file */
                    if (!log_desc && logging_on)
                    {
                        /* Unique-ish file names per handle */
                        int size = snprintf(log_file_name, sizeof(log_file_name), "log%lu.txt", handle);
                        if ((size < 0) || ((size_t)size >= sizeof(log_file_name)))
                        {
                            log_file_name[sizeof(log_file_name) - 1] = EOS;
                        }
                        OutStandard(handle, "Opening [%s] file ... ", log_file_name);
                        log_desc = fopen(log_file_name, "w");
                        if (log_desc == NULL)
                        {
                            OutError(handle, "warning: failed to open [%s] file, errno=%s\n", log_file_name, strerror(errno));
                            logging_on = 0;
                        }
                        OutStandard(handle, "Ok\n");
                    }

                    if (log_desc)
                    {
                        fprintf(log_desc, "BLOCK %d REJECTED [TRY NB %d - STATUS= 0x%.2X%.2X]\n",
                                debug_block_nb, error_cnt, HDATA(handle).rx_buf[0], HDATA(handle).rx_buf[1]);
                    }

                    error_cnt++;

                    /* Bad block is echoed on UART. Get it */
                    memset(HDATA(handle).rx_buf, 0, sizeof(HDATA(handle).rx_buf));
#ifdef ANDROID
                    if (COM_poll(HDATA(handle).com_hdl, HDATA(handle).rx_buf, 0x104, 20000) != 1)
                    {
                        OutError(handle, "\n#Error: Bad block COM_poll %d failed [%s]\n\n", 0x104, strerror(errno));
                        fclose(arch_desc);
                        return 0;
                    }
#endif
                    length = COM_read(HDATA(handle).com_hdl, HDATA(handle).rx_buf, 0x104);
#ifdef ANDROID
                    if (length <= 0)
#else
                    if (length < 0)
#endif
                    {
                        OutError(handle, "\n#Error: Bad block COM_read %d failed [%s]\n\n", 0x104, strerror(errno));
                        fclose(arch_desc);
                        return 0;
                    }

                    /* Dump transmitted and echo blocks */
                    go = log_desc ? 1 : 0;
                    while (go)
                    {
                        for (i = 0; i < 16; i++)
                        {
                            if ((16*line_nb + i) == (int)index)
                            {
                                fprintf(log_desc, "\t\t\t\t\t\t\t\t\t");
                                break;
                            }

                            if (HDATA(handle).tx_buf[16*line_nb + i] != HDATA(handle).rx_buf[16*line_nb + i])
                            {
                                fprintf(log_desc, "%02X+", HDATA(handle).tx_buf[16*line_nb +i]);
                            }
                            else
                            {
                                fprintf(log_desc, "%.2X ", HDATA(handle).tx_buf[16*line_nb + i]);
                            }
                        }
                        fprintf(log_desc, "\t");
                        for (i = 0; i < 16; i++)
                        {
                            if ((16*line_nb +i) == (int)index)
                            {
                                fprintf(log_desc, "\n");
                                go = 0;
                                break;
                            }

                            if (HDATA(handle).tx_buf[16*line_nb + i] != HDATA(handle).rx_buf[16*line_nb + i])
                            {
                                fprintf(log_desc, "%02X+", HDATA(handle).rx_buf[16*line_nb + i]);
                            }
                            else
                            {
                                fprintf(log_desc, "%.2X ", HDATA(handle).rx_buf[16*line_nb + i]);
                            }
                        }

                        fprintf(log_desc, "\n");
                        line_nb++;
                    }

                    if (log_desc)
                    {
                        fprintf(log_desc, "\n");
                        fflush(log_desc);
                    }
                }

                if (error_cnt == MAX_BLOCK_REJECT)
                {
                    OutError(handle, "\n#Error: block rejected %d times. See log.txt.\n\n", error_cnt);
                    if (log_desc)
                    {
                        fclose(log_desc);
                    }
                    fclose(arch_desc);
                    return 0;
                }
            }
            while ((HDATA(handle).rx_buf[0] != 0xAA) || (HDATA(handle).rx_buf[1] != 0x01));
            file_sz -= read_sz;
        }
        HDATA(handle).progress.percent = HDATA(handle).progress.current.base + (100 * HDATA(handle).progress.current.weight);

        fclose(arch_desc);
        time(&time_current);
        OutStandard(handle, "100%%\nDone...[%d seconds]\n\n", time_current - time_start);
        return 1;
    }
    return 0;
}

static bool FirmwareFlash(UPD_Handle handle)
{
    time_t time_start, time_current;

    HDATA(handle).progress.current.base = HDATA(handle).progress.percent;
    OutStandard(handle, "\n\nPrograming flash...\n");
    /* Programing flash takes a while, so update AT comand timeout ... */
    at_SetCommandTimeout(HDATA(handle).com_hdl, 300);
    /* Special OutStandard call back to display progress */
    at_SetOutputCB(HDATA(handle).com_hdl, OutStandardAtProg, handle);
    time(&time_start);
    /* With progress */
    if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_PROG, 2)))
    {
        /* Try without progress if not supported */
        at_SetOutputCB(HDATA(handle).com_hdl, OutStandard, handle);
        if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_PROG, 1)))
        {
            return false;
        }
    }
    time(&time_current);
    at_SetDefaultCommandTimeout(HDATA(handle).com_hdl);
    /* Restore output call back */
    at_SetOutputCB(HDATA(handle).com_hdl, OutStandard, handle);
    OutStandard(handle, "\nDone...[%d seconds]\n\n", time_current - time_start);

    HDATA(handle).progress.percent = HDATA(handle).progress.current.base + (100 * HDATA(handle).progress.current.weight);
    return true;
}

static char *parse_pkgv_line(char *buf, int len, FILE *f)
{
    char buffer[80];

    char *data = fgets(buffer, sizeof(buffer), f);

    if (data)
    {
        int skip = 0;
        int length = 0;

        /* Look for end of line */
        char *end_of_line = strchr(buffer, '\n');

        /* If there was no \n read, skip to the \n or end of file/error */
        if (!end_of_line)
        {
            while (!feof(f) && !ferror(f))
            {
                if ('\n' == fgetc(f))
                {
                    break;
                }
            }
        }

        skip = strspn(buffer, " \t\r\n");
        length = strcspn(buffer + skip, " \t\r\n");
        length = min(len - 1, length);
        memcpy(buf, buffer + skip, length);
        buf[length + 1] = '\0';

        data = &buf[0];
    }
    return data;
}

static int UpdatePackageVersion(UPD_Handle handle, char *file)
{
    /* Read parameters from file, then write it with AT%IPKGV= */
    char package_version[PKGV_LEN_VERSION + 1] = {0};
    char package_date[PKGV_LEN_DATE + 1] = {0};
    char package_svn[PKGV_LEN_SVN + 1] = {0};
    int status = 1;

    FILE *package_desc = fopen(file, "rb");
    OutStandard(handle, "\nOpening [%s] file ... ", file);
    if (package_desc)
    {
        parse_pkgv_line(package_version, sizeof(package_version), package_desc);
        parse_pkgv_line(package_date, sizeof(package_date), package_desc);
        parse_pkgv_line(package_svn, sizeof(package_svn), package_desc);

        if (!AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, "AT%%IPKGV=%s%s%s%s%s\r\n",
                                   package_version,
                                   strlen(package_date)?",":"", package_date,
                                   strlen(package_svn)?",":"", package_svn)))
        {
            OutError(handle, "\n#Error: Could not send IPKGV command\n\n");
        }
        else
        {
            status = 1;
        }
        fclose(package_desc);
        OutStandard(handle, "\nFile [%s] closed", file);
    }
    return status;
}

static void ClearPackageVersion(UPD_Handle handle)
{
    if (!AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, "AT%%IPKGV=\r\n")))
    {
        OutError(handle, "\n#Error: Could not send IPKGV command\n\n");
    }
}

static char *FindPkgvFile(char *filelist)
{
    char *file = NULL;
    char *pkgv_end = strstr(filelist, PKGV_EXTENSION);

    if (pkgv_end)
    {
        char *start = pkgv_end;

        while (start != filelist && *(start - 1) != FILE_SEPARATOR[0])
        {
            start--;
        }

        file = (char *)malloc(pkgv_end - start + sizeof(PKGV_EXTENSION));

        if (file)
        {
            strncpy(file, start, pkgv_end - start + sizeof(PKGV_EXTENSION) - 1);
            file[pkgv_end - start + sizeof(PKGV_EXTENSION) - 1] = '\0';
        }
    }

    return file;
}

static void GetCurrentPkgv(UPD_Handle handle, char *pkgv)
{
    if (AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, "AT%%IPKGV?\r\n")))
    {
        const char *response = strstr(at_GetResponse(HDATA(handle).com_hdl),"%IPKGV: ");

        if (response)
        {
            size_t len = PKGV_LEN_VERSION;
            const char *end = strchr(response, ',');

            response += (sizeof("%IPKGV: ") - 1);
            if (!end)
            {
                end = strstr(response, "\r\n");
            }
            if (end)
            {
                len = end - response;
            }

            strncpy(pkgv, response, len);
            pkgv[PKGV_LEN_VERSION] = '\0';
        }
    }
}

static bool PackageVersionUpdateRequired(UPD_Handle handle, char *filelist)
{
    /* Default is to update */
    bool update = true;
    char *file = FindPkgvFile(filelist);

    if (file)
    {
        char package_version[PKGV_LEN_VERSION + 1] = {0};
        char firmware_version[PKGV_LEN_VERSION + 1] = {0};

        FILE *package_desc = fopen(file, "rb");

        if (package_desc)
        {
            parse_pkgv_line(package_version, sizeof(package_version), package_desc);
            GetCurrentPkgv(handle, &firmware_version[0]);

            /* If the versions are equal, no need to update */
            if (strcmp(firmware_version, package_version) == 0)
            {
                update = false;
            }
            fclose(package_desc);
        }

        free(file);
    }

    return update;
}

static int closePort(UPD_Handle handle)
{
    if(HDATA(handle).com_hdl)
    {
        OutDebug(handle, "Device closed, handle %d\n", HDATA(handle).com_hdl);
        if (COM_close(HDATA(handle).com_hdl) <= 0)
        {
            OutError(handle, "\n#Error: COM_close failed [%s]\n\n", strerror(errno));
        }
        HDATA(handle).com_hdl = 0;
    }
    return 1;
}

static int openPort(UPD_Handle handle, const char* dev_name)
{
    int result = 0; /* Failure */
    if (!HDATA(handle).com_hdl)
    {
        COM_Handle hdl = COM_open(dev_name);
        /* Open COM device */
        if(hdl)
        {
            OutDebug(handle, "Device [%s] Opened get handle %d\n", dev_name, hdl);
            HDATA(handle).com_hdl = hdl;

            /* Specific COM port settings... */
            #ifdef _WIN32
            /* Flow control:*/
            if (COM_setCts(HDATA(handle).com_hdl, HDATA(handle).com_settings.flow_control) <= 0)
            {
                OutError(handle, "com_SetCTS failure [%s]\n", dev_name);
                closePort(handle);
            }
            else
            #endif /* _WIN32 */
            {
                result = 1;
            }
        }
    }
    if(result == 0)
    {
        OutError(handle, "\n#Error: COM_open failed [%s]\n\n", dev_name);
    }

    return result;
}

static int GetCurrentMode(UPD_Handle handle)
{
    const char *start;

    if (AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_MODE)))
    {
        start = strstr(at_GetResponse(HDATA(handle).com_hdl), AT_RESPONSE);
        if (start != NULL)
            return atoi(&start[strlen(AT_RESPONSE)]);
    }
    return MODE_INVALID;
}

static bool LogSystemState(UPD_Handle handle)
{
    unsigned int i = 0;
    int current_mode = 0;

    if (HDATA(handle).com_hdl)
    {
        if(HDATA(handle).log_sys_state)
        {
            current_mode = GetCurrentMode(handle);
            if (current_mode == MODE_INVALID)
            {
                return false;
            }
            if (current_mode < (int)ARRAY_SIZE(HDATA(handle).logging_done))
            {
                if (HDATA(handle).logging_done[current_mode] == false)
                {
                    /* For both modem or loader mode */
                    for (i = 0; i < log_commands_max[0]; i++)
                    {
                        at_SendCmd(HDATA(handle).com_hdl, log_commands[0][i]);
                    }
                    /* Depend on current mode */
                    for (i = 0; i < log_commands_max[current_mode + 1]; i++)
                    {
                        at_SendCmd(HDATA(handle).com_hdl, log_commands[current_mode + 1][i]);
                    }
                    HDATA(handle).logging_done[current_mode] = true;
                }
            }
        }
    }
    return true;
}

static int RestoreSettings(UPD_Handle handle)
{
    int error = ErrorCode_Success;
    /* Get back to default speed */
    if (HDATA(handle).option_speed != DEFAULT_SPEED)
    {
        if (AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_IPR, DEFAULT_SPEED)))
        {
            COM_updateSpeed(HDATA(handle).com_hdl, DEFAULT_SPEED);
        }
        else
        {
            return ErrorCode_AT;
        }
    }

    if (HDATA(handle).disable_full_coredump)
    {
        if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, "AT%%IFULLCOREDUMP=0\r\n")))
        {
            // retry again
            at_SendCmd(HDATA(handle).com_hdl, "AT%%IFULLCOREDUMP=0\r\n");
        }
    }

    if (HDATA(handle).updating_in_modem_mode)
    {
        at_SendCmd(HDATA(handle).com_hdl, "AT%%IRESET\r\n");
    }
    else
    {
        error = SwitchModeEx(handle, HDATA(handle).option_modechange, HDATA(handle).option_check_mode_restore);
    }
    /* Bgz53996 or bgz051430: In any cases we leave the CM to detect the device even if port resource has been changed */
    if ((ErrorCode_ConnectionError == error) || (ErrorCode_Error == error))
    {
        return ErrorCode_Success;
    }
    return error;
}

static bool LoadingFilesPossible(UPD_Handle handle)
{
    bool result = false;

    if (HDATA(handle).option_update_in_modem_mode)
    {
        /* Possible if AT%LOAD? returns OK. */
        if (AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, "AT%%LOAD?\r\n")))
        {
            /* Need to switch to CFUN: 0 - if we fail, enforce a mode change */
            /* TODO: Should we fail the update outright instead if we fail to go to CFUN: 0 ? */

            /* Switching CFUN could take a while */
            at_SetCommandTimeout(HDATA(handle).com_hdl, 60);
            if (AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, "AT+CFUN=0\r\n")))
            {
                result = true;
            }
            at_SetDefaultCommandTimeout(HDATA(handle).com_hdl);
        }
    }

    HDATA(handle).updating_in_modem_mode  = result;

    return result;
}

static int PrepareLoader(UPD_Handle handle)
{
    int error = ErrorCode_Success;
    /* Set echo OFF */
    at_SendCmd(HDATA(handle).com_hdl, AT_CMD_ECHO_OFF);

    if (!LoadingFilesPossible(handle))
    {
        /* Won't actually switch if we're already in MODE_LOADER */
        error = SwitchMode(handle, MODE_LOADER);
        if (error != ErrorCode_Success)
            return error;
    }

    /* Set communication speed */
    if (HDATA(handle).option_speed != DEFAULT_SPEED)
    {
        if (AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_IPR, HDATA(handle).option_speed)))
        {
            COM_updateSpeed(HDATA(handle).com_hdl, HDATA(handle).option_speed);
        }
        else
        {
            OutError(handle, "#Warning: could not set speed to %d bps\nKeeping %d bps\n", HDATA(handle).option_speed, DEFAULT_SPEED);
        }
    }

    /* Set echo OFF (in case we have really change the `mode`) */
    at_SendCmd(HDATA(handle).com_hdl, AT_CMD_ECHO_OFF);

    return error;
}

static void DoNvclean(UPD_Handle handle, char *file)
{
    if (HDATA(handle).option_send_nvclean)
    {
        tAppliFileHeader    arch_header;

        /* If we're updating a modem (ARCH_ID_APP), we need to send AT%NVCLEAN */
        if (GetWrappedHeader(handle, &arch_header, file, NULL) > 0)
        {
            if (ARCH_ID_APP == GET_ARCH_ID(arch_header.file_id))
            {
                at_SendCmd(HDATA(handle).com_hdl, AT_CMD_NVCLEAN);
            }
        }
    }
}

static bool PublicChipId(UPD_Handle handle)
{
    /* Request PCID */
    if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_CHIPID)))
    {
        return false;
    }
    if (HDATA(handle).option_verbose > VERBOSE_SILENT_LEVEL)
    {
        OutStandard(handle, at_GetResponse(HDATA(handle).com_hdl));
    }
    else
    {
        HDATA(handle).option_verbose = VERBOSE_INFO_LEVEL;
        OutStandard(handle, at_GetResponse(HDATA(handle).com_hdl));
        HDATA(handle).option_verbose = VERBOSE_SILENT_LEVEL;
    }
    return true;
}

static bool SendIbackup(UPD_Handle handle)
{
    bool result = true;

    at_SetCommandTimeout(HDATA(handle).com_hdl, 60);

    result = !(AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_IBACKUP)));

    if (HDATA(handle).option_verbose > VERBOSE_SILENT_LEVEL)
    {
        OutStandard(handle, at_GetResponse(HDATA(handle).com_hdl));
    }
    else
    {
        HDATA(handle).option_verbose = VERBOSE_INFO_LEVEL;
        OutStandard(handle, at_GetResponse(HDATA(handle).com_hdl));
        HDATA(handle).option_verbose = VERBOSE_SILENT_LEVEL;
    }
    at_SetDefaultCommandTimeout(HDATA(handle).com_hdl);

    return result;
}

static bool SendIrestore(UPD_Handle handle)
{
    bool result = true;

    at_SetCommandTimeout(HDATA(handle).com_hdl, 60);

    result = !(AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_IRESTORE)));

    if (HDATA(handle).option_verbose > VERBOSE_SILENT_LEVEL)
    {
        OutStandard(handle, at_GetResponse(HDATA(handle).com_hdl));
    }
    else
    {
        HDATA(handle).option_verbose = VERBOSE_INFO_LEVEL;
        OutStandard(handle, at_GetResponse(HDATA(handle).com_hdl));
        HDATA(handle).option_verbose = VERBOSE_SILENT_LEVEL;
    }
    at_SetDefaultCommandTimeout(HDATA(handle).com_hdl);

    return result;
}

static int GetMode(char* line)
{
    int offset = 0;
    if ((strlen(line) == strlen("modeX")) && !strncmp(&line[offset], "mode", strlen("mode")))
    {
        offset += strlen("mode");
        if (strspn(&line[offset], DIGIT_SET) == 1)
        {
            return (int)(MODE_MODEM + line[offset] - '0');
        }
    }
    return MODE_INVALID;
}

static int GetWait(char* line)
{
    int offset = 0;
    if ((strlen(line) <= strlen("waitXXX")) && !strncmp(&line[offset], "wait", strlen("wait")))
    {
        offset += strlen("wait");
        if (strspn(&line[offset], DIGIT_SET) > 0)
        {
            return atoi(&line[offset]);
        }
    }
    return 0;
}

static bool IsIbackup(char* line)
{
    return !strncmp(line, "ibackup", sizeof("ibackup") - 1);
}

static bool IsIrestore(char* line)
{
    return !strncmp(line, "irestore", sizeof("irestore") - 1);
}

static char* OverrideByMethodFile(UPD_Handle handle, char *filelist)
{
    int size;
    tAppliFileHeader arch_header;
    char* filepath;
    char* files = NULL;
    char* path = NULL;
    char* filesdup = strdup(filelist);
    if (filesdup)
    {
        char* tmpfilename = NULL;
        size = strlen(filelist);
        filepath = strtok(filesdup, FILE_SEPARATOR);
        if (filepath)
        {
            if (SplitFilePath(filepath, &path, NULL))
            {
                tmpfilename = JoinFilePath(path, "update.mtd");
                if (tmpfilename)
                {
                    OutDebug(handle, "Method file: %s", tmpfilename);
                    size = FileSize(tmpfilename);
                    if (size > 0)
                    {
                        FILE* file = fopen(tmpfilename, "rb");
                        if (file)
                        {
                            char* content = (char *)malloc(size + 1);
                            if (content)
                            {
                                int length = fread(content, 1, size, file);
                                content[size] = 0;
                                if (length == size)
                                {
                                    int count = 0;
                                    char* backup = strdup(content);
                                    if (backup)
                                    {
                                        char* line = content;
                                        OutDebug(handle, "Content: %s", backup);
                                        while (line && (line < (content + size)))
                                        {
                                            line = strtok(line, "\x0D\x0A");
                                            if (strlen(line) > 0)
                                            {
                                                OutDebug(handle, "Command: %s", line);
                                                if ((GetWait(line) <= 0) && (GetMode(line) == MODE_INVALID) && (!IsIbackup(line)) && (!IsIrestore(line)))
                                                {
                                                    free(tmpfilename);
                                                    tmpfilename = JoinFilePath(path, line);
                                                    if (!tmpfilename)
                                                    {
                                                        OutDebug(handle, "JoinFilePath error: %s+%s", path, line);
                                                        count = 0;
                                                        break;
                                                    }
                                                    if (FileExists(tmpfilename))
                                                    {
                                                        count++;
                                                    }
                                                }
                                            }
                                            line += strlen(line) + 1;
                                        }
                                        if (count)
                                        {
                                            int listsize = size + (count * (strlen(path) + sizeof(PATH_SEP))) + strlen(FILE_SEPARATOR);
                                            files = (char *)malloc(listsize);
                                            if (files)
                                            {
                                                /* Build new file list with path */
                                                files[0] = EOS;
                                                line = backup;
                                                while (line && (line < (backup + size)))
                                                {
                                                    int offset = strlen(files);
                                                    line = strtok(line, "\x0D\x0A");
                                                    if ((GetWait(line) <= 0) && (GetMode(line) == MODE_INVALID) && (!IsIbackup(line)) && (!IsIrestore(line)))
                                                    {
                                                        length = snprintf(&files[offset], listsize - offset, "%s%c%s%s", path, PATH_SEP, line, FILE_SEPARATOR);
                                                    }
                                                    else
                                                    {
                                                        length = snprintf(&files[offset], listsize - offset, "%s%s", line, FILE_SEPARATOR);
                                                    }
                                                    if ((length < 0) || (length >= (listsize - offset)))
                                                    {
                                                        OutDebug(handle, "snprintf error %d/%d: \"%s\"", length, listsize - offset, line);
                                                        free(files);
                                                        files = NULL;
                                                        break;
                                                    }
                                                    line += strlen(line) + 1;
                                                }
                                                if (files)
                                                {
                                                    files[strlen(files) - strlen(FILE_SEPARATOR)] = EOS;
                                                    OutDebug(handle, "Override: %s", files);
                                                }
                                            }
                                        }
                                        free(backup);
                                    }
                                }
                                free(content);
                            }
                        }
                    }
                    free(tmpfilename);
                }
                else
                {
                    OutDebug(handle, "JoinFilePath error: %s+update.mtd", path);
                }
            }
            else
            {
                OutDebug(handle, "SplitFilePath error: %p, %s", path, filepath);
            }
        }
        free(filesdup);
    }
    HDATA(handle).old_iso_support = false;
    HDATA(handle).progress.totalweight = 0;
    if (!files)
    {
        files = strdup(filelist);
        OutDebug(handle, "No override");
    }
    if (files)
    {
        filesdup = strdup(files);
        if (filesdup)
        {
            int pkgv = 0;
            size = strlen(filesdup);
            filepath = filesdup;
            while (filepath && (filepath < (filesdup + size)))
            {
                filepath = strtok(filepath, FILE_SEPARATOR);
                if ((GetWait(filepath) <= 0) && (GetMode(filepath) == MODE_INVALID) && (!IsIbackup(filepath)) && (!IsIrestore(filepath)))
                {
                    OutDebug(handle, "File: %s", filepath);
                    if (FileExists(filepath))
                    {
                        if (IS_PKGV_FILE(filepath))
                        {
                            pkgv++;
                            if (pkgv > 1)
                            {
                                OutError(handle, "\n#Error: More than one PKGV file, download aborted\n\n");
                                free(files);
                                files = NULL;
                                break;
                            }
                        }
                        else
                        {
                            if (GetWrappedHeader(handle, &arch_header, filepath, "Check ") > 0)
                            {
                                int weight = GET_FILE_WEIGHT(filepath);
                                HDATA(handle).progress.totalweight += weight;
                                OutDebug(handle, "Add %s, weight %d/%d", filepath, weight, HDATA(handle).progress.totalweight);
                                if (GET_ARCH_ID(arch_header.file_id) == ARCH_ID_ZEROCD)
                                {
                                    unsigned int cl;
                                    if (ReadExtHdrFromFile(handle, filepath, &cl, NULL) < 0)
                                    {
                                        HDATA(handle).old_iso_support = true;
                                    }
                                    OutDebug(handle, "Old ISO support: %s", HDATA(handle).old_iso_support ? "true" : "false");
                                }
                            }
                        }
                    }
                }
                else
                {
                    OutDebug(handle, "GetWait: %d", GetWait(filepath));
                    OutDebug(handle, "GetMode: %d", GetMode(filepath));
                }
                filepath += strlen(filepath) + 1;
            }
            free(filesdup);
        }
        else
        {
            OutDebug(handle, "strdup error: %d", strlen(files));
            free(files);
            files = NULL;
        }
    }
    return files;
}

static int FirmwareUpdate(UPD_Handle handle, char *filelist, int pcid, int flash)
{
    int error = ErrorCode_Success;
    tAppliFileHeader arch_header;

    do
    {
        char* files;

        if (HDATA(handle).option_check_pkgv)
        {
            if (!PackageVersionUpdateRequired(handle, filelist))
            {
                break;
            }
        }

        if (pcid || (filelist != NULL))
        {
            error = PrepareLoader(handle);
            if (error != ErrorCode_Success)
                break;
        }
        if (pcid)
        {
            if (!PublicChipId(handle))
            {
                error = ErrorCode_AT;
                break;
            }
        }
        files = OverrideByMethodFile(handle, filelist);
        if (files)
        {
            int size = strlen(files);
            char* filepath = files;
            bool pkgv_found = false;

            HDATA(handle).progress.step = flash ? ProgressStep_Flash : ProgressStep_Download;
            while (filepath && (filepath < (files + size)))
            {
                filepath = strtok(filepath, FILE_SEPARATOR);
                if (filepath)
                {
                int wait = GetWait(filepath);
                if (wait > 0)
                {
                    OutDebug(handle, "Wait: %d second", wait);
                    DelayInSecond(wait);
                }
                else
                {
                    int mode = GetMode(filepath);
                    if (mode != MODE_INVALID)
                    {
                        OutDebug(handle, "Switch to mode: %d", mode);
                        error = SwitchMode(handle, mode);
                        if (error != ErrorCode_Success)
                        {
                            OutError(handle, "SwitchMode %d failed: %d", mode, error);
                            break;
                        }
                    }
                    else if(IsIbackup(filepath))
                    {
                        if (!SendIbackup(handle))
                        {
                            error = ErrorCode_Error;
                            OutError(handle, "IBACKUP failed");
                            break;
                        }
                    }
                    else if (IsIrestore(filepath))
                    {
                        if (!SendIrestore(handle))
                        {
                            error = ErrorCode_Error;
                            OutError(handle, "IRESTORE failed");
                            break;
                        }
                    }
                    else
                    {
                        /* Assuming this comes after an actual file update */
                        if (IS_PKGV_FILE(filepath))
                        {
                            pkgv_found = true;
                            if (!HDATA(handle).old_iso_support)
                            {
                                OutDebug(handle, "Pkgv: %s", filepath);
                                if (!UpdatePackageVersion(handle, filepath))
                                {
                                    error = ErrorCode_AT;
                                    break;
                                }
                                OutStandard(handle, "\nPackage version updated\n");
                            }
                            else
                            {
                                OutDebug(handle, "Pkgv: %s ignored (old ISO format)", filepath);
                            }
                        }
                        else
                        {
                            if (GetWrappedHeader(handle, &arch_header, filepath, NULL) > 0)
                            {
                                int arch_entry = GetArchTableEntry(GET_ARCH_ID(arch_header.file_id));
                                bool one_shot = (arch_entry >= 0) ? arch_type[arch_entry].write_file_during_auth : false;
                                HDATA(handle).progress.current.weight = GET_FILE_WEIGHT(filepath);
                                if (!one_shot && flash)
                                {
                                    HDATA(handle).progress.current.weight /= 2;
                                }
                                OutDebug(handle, "Download: %s, Weight: %d/%d", filepath, HDATA(handle).progress.current.weight, HDATA(handle).progress.totalweight);
                                if (!FirmwareDownload(handle, &arch_header, filepath))
                                {
                                    error = ErrorCode_DownloadError;
                                    break;
                                }
                                /* Archive signature check / Firmware update */
                                if (flash)
                                {
                                    OutDebug(handle, "Flash: %s, Weight: %d/%d", filepath, HDATA(handle).progress.current.weight, HDATA(handle).progress.totalweight);
                                    if (!FirmwareFlash(handle))
                                    {
                                        error = ErrorCode_AT;
                                        break;
                                    }
                                    /* Download is successful and we've programmed the new binary -
                                     * check if we need to do NVCLEAN
                                     */
                                    DoNvclean(handle, filepath);
                                }
                                else
                                {
                                    OutStandard(handle, "\n");
                                    if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, AT_CMD_PROG, 0)))
                                    {
                                        error = ErrorCode_AT;
                                        break;
                                    }
                                    OutStandard(handle, "Archive signature ... OK\n");
                                }
                            }
                            else
                            {
                                OutError(handle, "\nInvalid header file: [%s] is ignored\n", filepath);
                            }
                        }
                    }
                }
                    filepath += strlen(filepath) + 1;
                }
            }

            if (!pkgv_found)
            {
                ClearPackageVersion(handle);
            }

            free(files);
        }
        else
        {
            OutStandard(handle, "\nNo file to update\n");
            error = ErrorCode_DownloadError;
            break;
        }
        HDATA(handle).progress.step = ProgressStep_Finalize;
        HDATA(handle).progress.percent = HDATA(handle).progress.totalweight * 100;
    }
    while(0);
    OutStandard(handle, "\nFirmware upgrade done: %d\n", error);
    if (error == ErrorCode_Success)
    {
        error = RestoreSettings(handle);
    }
    return error;
}

static bool IsLoaderInModemModeSupported(UPD_Handle handle)
{
    bool result = false;

    /* 
     * Loader in modem mode is supported if AT%MODE=? response contains ),(2,3)
     * (e.g. %MODE: (1,2,3),(0,2,3)
     */
    if (AT_SUCCESS(at_SendCmd(HDATA(handle).com_hdl, "AT%%MODE=?\r\n")))
    {
        char *second_group = strstr(at_GetResponse(HDATA(handle).com_hdl), "),(");
        if (second_group)
        {
            result = (strstr(second_group, "2,3") != NULL);
        }
    }

    return result;
}

static int DelayedSwitchMode(UPD_Handle handle, char *command, int size, int mode, bool checkSuccess)
{
    int timeout, repeat, length;
    time_t current;
    bool close_and_reopen = true;

    OutDebug(handle, "DelayedSwitchMode: %d", mode);
    
    if (mode == MODE_LOADER)
    {
        close_and_reopen = (IsLoaderInModemModeSupported(handle) == false);
    }
    
    length = snprintf(command, size, AT_CMD_MODEx, mode);
    if ((length < 0) || (length >= size))
    {
        command[size - 1] = EOS;
    }
    /* Switch mode to custom init */
    if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, command)))
    {
        closePort(handle);
        return ErrorCode_AT;
    }
    
    if (close_and_reopen)
    {
        closePort(handle);

        if (checkSuccess)
        {
            timeout = HDATA(handle).option_delay < 1 ? 1 : HDATA(handle).option_delay;
            time(&current);OutDebug(handle, "time: %d", current);
            DelayInSecond(timeout * 4);
            time(&current);OutDebug(handle, "time: %d", current);
            for (repeat = 4 ; repeat > 0 ; repeat--)
            {
                OutDebug(handle, "repeat: %d", repeat);
                if (openPort(handle, HDATA(handle).option_dev_name))
                {
                    OutDebug(handle, "success");
                    return ErrorCode_Success;
                }
                DelayInSecond(timeout);
                time(&current);OutDebug(handle, "time: %d", current);
            }
            time(&current);OutDebug(handle, "failed: %d", current);
            return ErrorCode_DeviceNotFound;
        }
    }
    return ErrorCode_Success;
/*    float timeout;
    time_t start;
    time_t current;
    unsigned int current_mode = MODE_INVALID;

    OutDebug(handle, "DelayedSwitchMode: %d", mode);
    int length = snprintf(command, size, AT_CMD_MODEx, mode);
    if ((length < 0) || (length >= size))
    {
        log_file_name[size - 1] = EOS;
    }
    if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, command)))
    {
        closePort(handle);
        return ErrorCode_AT;
    }
    closePort(handle);
    timeout = (HDATA(handle).option_delay < 1 ? 1 : HDATA(handle).option_delay) * 4;
    time(&start);
    OutDebug(handle, "%d", start);
    do
    {
        if (openPort(handle, HDATA(handle).option_dev_name))
        {
            current_mode =  GetCurrentMode(handle);
            if (current_mode != mode)
            {
                closePort(handle);
            }
        }
        time(&current);
        OutDebug(handle, "%d: %d/%d", current - start, current_mode, mode);
    }
    while ((current_mode != mode) && ((current - start) < timeout));
    OutDebug(handle, "%d: %d/%d", current - start, current_mode, mode);
    return (current_mode != mode) ? ErrorCode_DeviceNotFound : ErrorCode_Success;*/
}

/* Convert one char_str to char_hex value
 * 0 if not in the range of a valid hexit */
static uint8 str2hexit(char p){
    uint8 ret = 0;
    if( p >='0' && p<='9') ret=p-'0';
    else if ( p >='a' && p<='f') ret=p-'a'+10;
    else if ( p >='A' && p<='F') ret=p-'A'+10;
    return ret;
}


static int IsFileFoundInFileSytem(UPD_Handle handle, char *filename)
{
    int found = 0;
    tAppliFileHeader  header;

    do
    {
        /* Get archive header */
        if (GetWrappedHeader(handle, &header, filename, NULL) == 0)
        {
            break;
        }

        /* Get arch entry corresponding to archID */
        int arch_entry = GetArchTableEntry(GET_ARCH_ID(header.file_id));
        if (arch_entry < 0)
        {
            break;
        }

        /* Get SHA1 of archive in file system */
        char command[64];
        char sha1_str_in_fs[SHA1_STR_BYTES_SIZE+1]; /* SHA1 string: 40hexits + '\0' */
        sprintf(command, "AT%%%%IGETARCHDIGEST=\"%s\"\r\n", arch_type[arch_entry].acr);
        at_SetCommandTimeout(HDATA(handle).com_hdl, 10); /* maybe long for modem archive... */
        int res = at_SendCmdAndWaitResponse(HDATA(handle).com_hdl, AT_RSP_OK, AT_RSP_ERROR, command);
        if (res != AT_ERR_SUCCESS)
        {
            OutStandard(handle, "%s: at error: %d\n", __FUNCTION__, res);
            at_SetDefaultCommandTimeout(HDATA(handle).com_hdl);
            break;
        }
        at_SetDefaultCommandTimeout(HDATA(handle).com_hdl);
        const char *resp = at_GetResponse(HDATA(handle).com_hdl);
        char *start = strstr(resp, AT_RESPONSE);
        if (start == NULL)
        {
            break;
        }
        memcpy(sha1_str_in_fs, &start[2], 40);
        sha1_str_in_fs[40] = '\0';

        /* Get SHA1 of archive to update */
        SHA1_CTX context;
        int remaining = header.length + header.file_size;
        int to_read, bytes_read;
        u_char buf[4096];
        int err = 0;
        SHA1Init(&context);
        int fd = open(filename, O_RDONLY);
        while (remaining)
        {
            to_read = remaining > 4096 ? 4096:remaining;
            bytes_read = read(fd, buf, to_read);
            if (bytes_read != to_read)
            {
                err = 1;
                break;
            }
            SHA1Update(&context, buf, bytes_read);
            remaining -= bytes_read;
        }
        close(fd);
        unsigned char sha1_in_file[SHA1_DIGEST_LENGTH];
        SHA1Final(sha1_in_file, &context);
        if (err)
        {
            break;
        }

        /* Convert 40 hexits sha1_str_in_fs into 20bytes sha1_in_fs
         * and compare the 2 SHA1 */
        OutDebug(handle, "%s: %s with SHA1 in flash: %s\n", __FUNCTION__, arch_type[arch_entry].acr, sha1_str_in_fs);
        OutDebug(handle, "%s: %s with SHA1 in file: ", __FUNCTION__, arch_type[arch_entry].acr);
        int i, diff=0;
        char sha1_in_fs[SHA1_DIGEST_LENGTH];

        for (i=0; i<SHA1_DIGEST_LENGTH; i++)
        {
            OutDebug(handle, "%x",sha1_in_file[i]);
            sha1_in_fs[i] = (str2hexit(sha1_str_in_fs[2*i])<<4) | str2hexit(sha1_str_in_fs[2*i+1]);
            if (sha1_in_fs[i] != sha1_in_file[i])
            {
                diff = 1;
            }
        }
        OutDebug(handle, "\n");

        if (!diff)
        {
            OutStandard(handle, "%s already programmed in modem file system\n\n", filename);
            found = 1;
        }

    } while(0);

    return found;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
ICERA_API void CheckAlreadyProgrammedFiles(UPD_Handle handle, char *filelist)
{
    int found = 0;
    char *tmp_list = calloc(strlen(filelist)+1, 1);

    OutDebug(handle, "\n%s: file(s) list before check: %s\n", __FUNCTION__, filelist);

    if (strstr(filelist, FILE_SEPARATOR))
    {
        /* It's a list of files... */
        int count = 0;
        char *dup = strdup(filelist);
        dup = strtok(dup, FILE_SEPARATOR);
        while(dup)
        {
            /* Check file per file if not already in file system */
            found = IsFileFoundInFileSytem(handle, dup);
            if (!found)
            {
                if (count)
                {
                    /* There was a previous needed entry */
                    strcat(tmp_list, FILE_SEPARATOR);
                    count = 0;
                }
                /* Append to new list */
                strcat(tmp_list, dup);
                count++;
            }
            dup = strtok(NULL, FILE_SEPARATOR);
        }
        if (strlen(tmp_list))
        {
            memcpy(filelist, tmp_list, strlen(tmp_list)+1);
        }
        else
        {
            /* Simply "disable" old list... */
            filelist[0] = '\0';
        }
    }
    else
    {
        found = IsFileFoundInFileSytem(handle, filelist);
        if (found)
        {
            /* Simply "disable" old list... */
            filelist[0] = '\0';
        }
    }
    free(tmp_list);

    OutDebug(handle, "%s: file(s) list after check: %s\n", __FUNCTION__, filelist);

    return;
}

/* Handle functions (create, free, set parameters) */
ICERA_API UPD_Handle CreateHandle(void)
{
    unsigned long  i = 0;
    UPD_Handle handle = 0;

    /* Find unused handle, set initial values. Start at 1 to allow a default handle */
    for (i = 1; i < UPD_MAX_HANDLES; i++)
    {
        if ( !handles[i].used)
        {
            /* +1 to allow 0 to indicate invalid handle */
            handle = i + 1;
            break;
        }
    }
    if (handle != 0)
    {
        InitHandle(handle);
    }
    return handle;
}

ICERA_API void FreeHandle(UPD_Handle handle)
{
    /* In any cases callback should be freed */
    SetLogCallbackFuncEx(handle, NULL);
    if (HDATA(DEFAULT_HANDLE).option_dev_name)
    {
        free(HDATA(DEFAULT_HANDLE).option_dev_name);
    }
    MUTEX_DESTROY(HDATA(handle).mutex_log);
    HDATA(handle).used = 0;
}

ICERA_API int  SetModeChange(UPD_Handle handle, int modechange)
{
    if ((modechange > MODE_INVALID) && (modechange < MODE_MAX))
    {
        HDATA(handle).option_modechange = modechange;
        return ErrorCode_Success;
    }

    return ErrorCode_Error;
}

ICERA_API int SetSpeed(UPD_Handle handle, int speed)
{
    if (speed > 0)
    {
        HDATA(handle).option_speed = speed;
        return ErrorCode_Success;
    }

    return ErrorCode_Error;
}

ICERA_API int SetDelay(UPD_Handle handle, int delay)
{
    if (delay > 0)
    {
        HDATA(handle).option_delay = delay;
        return ErrorCode_Success;
    }

    return ErrorCode_Error;
}

ICERA_API int SetBlockSize(UPD_Handle handle, int block_sz)
{
    if (block_sz > 0)
    {
        if (block_sz <= MIN_BLOCK_SZ)
        {
            block_sz = MIN_BLOCK_SZ + 1;
        }
        HDATA(handle).option_block_sz = (unsigned short)block_sz;
        return ErrorCode_Success;
    }

    return ErrorCode_Error;
}

ICERA_API int SetDevice(UPD_Handle handle, const char *dev_name)
{
    if (HDATA(handle).com_hdl)
    {
        closePort(handle);
    }
    if (openPort(handle, dev_name))
    {
        at_SetOutputCB(HDATA(DEFAULT_HANDLE).com_hdl, OutStandard, DEFAULT_HANDLE);
        return ErrorCode_Success;
    }
    return ErrorCode_Error;
}

ICERA_API int SetBaudrate(UPD_Handle handle, int speed)
{
    if (HDATA(handle).com_hdl)
    {
        COM_updateSpeed(HDATA(handle).com_hdl, speed);
        return ErrorCode_Success;
    }

    return ErrorCode_Error;
}

/* I/O, logging */
ICERA_API void SetLogCallbackFuncEx(UPD_Handle handle, log_callback_type callback)
{
    OutDebug(handle, "SetLogCallbackFunc: %p", callback);
    MUTEX_LOCK(HDATA(handle).mutex_log);
    HDATA(handle).log_callback = callback;
    MUTEX_UNLOCK(HDATA(handle).mutex_log);
}

ICERA_API void SetLogCallbackFunc(log_callback_type callback)
{
    SetLogCallbackFuncEx(DEFAULT_HANDLE, callback);
}

ICERA_API void UpdaterLogToFileEx(UPD_Handle handle, char *file)
{
    FILE* log_file = NULL;
    struct stat stat_info;
    char *new_file_name;

    if (file)
    {
        OutDebug(handle, "UpdaterLogToFileEx [%s]", file);
    }
    MUTEX_LOCK(HDATA(handle).mutex_log);
    if (HDATA(handle).log_file)
    {
        fclose(HDATA(handle).log_file);
        HDATA(handle).log_file = NULL;
    }
    MUTEX_UNLOCK(HDATA(handle).mutex_log);
    if (file)
    {
        /* Check that the file hasn't got too big */
        memset(&stat_info, 0, sizeof (stat_info));
        if (stat(file, &stat_info) == 0)
        {
            /* Check if file is too big */
            if (stat_info.st_size > MAX_LOGFILE_SIZE)
            {
                /* Space for old name + .old */
                new_file_name = (char *)alloca(strlen(file) + 4 + 1);

                if (new_file_name != NULL)
                {
                    strcpy(new_file_name, file);
                    strcat(new_file_name, ".old");
                    remove(new_file_name);
                    rename(file, new_file_name);
                }
            }
        }
        log_file = fopen(file, "a");
        if (log_file)
        {
            MUTEX_LOCK(HDATA(handle).mutex_log);
            HDATA(handle).log_file = log_file;
            fprintf(HDATA(handle).log_file, "\n");
            LogTime(handle);
            fprintf(HDATA(handle).log_file, "DLL version: %s\n\n", DLD_VERSION);
            fflush(HDATA(handle).log_file);
            MUTEX_UNLOCK(HDATA(handle).mutex_log);
        }
    }
}

ICERA_API void UpdaterLogToFileWEx(UPD_Handle handle, wchar_t *file)
{
/* Wide characters only supported under Win32 at the moment */
#if defined(WIN32) && defined(UNICODE)
    FILE* log_file = NULL;
    struct _stat stat_info;
    wchar_t *new_file_name;

    if (file)
    {
        OutDebug(handle, "UpdaterLogToFileWEx [%S]", file);
    }
    MUTEX_LOCK(HDATA(handle).mutex_log);
    if (HDATA(handle).log_file)
    {
        fclose(HDATA(handle).log_file);
        HDATA(handle).log_file = NULL;
    }
    MUTEX_UNLOCK(HDATA(handle).mutex_log);
    if (file)
    {
        /* Check that the file hasn't got too big */
        memset(&stat_info, 0, sizeof (stat_info));
        if (_wstat(file, &stat_info) == 0)
        {
            /* Check if file is too big */
            if (stat_info.st_size > MAX_LOGFILE_SIZE)
            {
                /* Space for old name + .old */
                new_file_name = alloca((wcslen(file) + 4 + 1) * sizeof(wchar_t));

                if (new_file_name != NULL)
                {
                    wcscpy(new_file_name, file);
                    wcscat(new_file_name, L".old");
                    _wremove(new_file_name);
                    _wrename(file, new_file_name);
                }
            }
        }
        log_file = _wfopen(file, L"a");
    }
    MUTEX_LOCK(HDATA(handle).mutex_log);
    HDATA(handle).log_file = log_file;
    MUTEX_UNLOCK(HDATA(handle).mutex_log);
#endif
}

ICERA_API void SetLogSysState(UPD_Handle handle, int state)
{
    HDATA(handle).log_sys_state = state;
}

ICERA_API void SetSendNvclean(UPD_Handle handle, int enable)
{
    HDATA(handle).option_send_nvclean = enable;
}

ICERA_API void SetCheckPkgv(UPD_Handle handle, int enable)
{
    HDATA(handle).option_check_pkgv = enable;
}

ICERA_API void SetUpdateInModemMode(UPD_Handle handle, int enable)
{
    HDATA(handle).option_update_in_modem_mode = enable;
}

/* Legacy (deprecated) API */
ICERA_API void SetGlobalDisableFullCoredumpSetting(bool disable_full_coredump_required)
{
    disable_full_coredump = disable_full_coredump_required;
}

ICERA_API void SetDisableFullCoredumpSetting(UPD_Handle handle, bool disable_full_coredump_required)
{
    HDATA(handle).disable_full_coredump = disable_full_coredump_required;
}

/* Legacy (deprecated) API */
ICERA_API void SetGlobalCheckModeRestore(bool check)
{
    global_check_mode_restore = check;
}

ICERA_API void SetCheckModeRestore(UPD_Handle handle, bool check)
{
    HDATA(handle).option_check_mode_restore = check;
}

/* Serial raw access functions */
ICERA_API UPD_Handle Open(const char *dev_name)
{
    /* Get a new handle and go */
    UPD_Handle handle = CreateHandle();
    if (handle)
    {
        if(!openPort(handle, dev_name))
        {
            OutError(handle, "\n#Error: openPort [%s] failed [%s]\n\n", dev_name, strerror(errno));
            FreeHandle(handle);
            return 0;
        }
        HDATA(handle).option_dev_name = strdup(dev_name);
        /* Default output callback */
        at_SetOutputCB(HDATA(handle).com_hdl, OutStandard, handle);
    }
    else
    {
        OutError(DEFAULT_HANDLE, "\n#Error: CreateHandle failed\n\n");
    }
    return handle;
}

ICERA_API int Close(UPD_Handle handle)
{
    int result = closePort(handle);
    if (HDATA(handle).option_dev_name)
    {
        free(HDATA(handle).option_dev_name);
        HDATA(handle).option_dev_name = NULL;
    }
    FreeHandle(handle);
    return result;
}

ICERA_API int Abort_Handle(UPD_Handle handle)
{
    int result = 0;
    
    OutDebug(handle, "Device abort, handle %d\n", HDATA(handle).com_hdl);
    
    if (HDATA(handle).used)
    {
        result = COM_abort(HDATA(handle).com_hdl);
    }

    if (result <= 0)
    {
        OutError(handle, "\n#Error: COM_abort failed [%s]\n\n", strerror(errno));
    }
    
    return result;
}

ICERA_API int Write(UPD_Handle handle, void *buffer, int size)
{
    int     res = COM_write(HDATA(handle).com_hdl, buffer, size);
    if (res < 0)
    {
        OutError(handle, "\n#Error: COM_write %d failed [%s]\n\n", size, strerror(errno));
    }
    return res;
}

ICERA_API int Read(UPD_Handle handle, void *buffer, int size)
{
    int     res = COM_read(HDATA(handle).com_hdl, buffer, size);
    if (res < 0)
    {
        OutError(handle, "\n#Error: COM_read %d failed [%s]\n\n", size, strerror(errno));
    }
    return res;
}

ICERA_API int SetFlowControl(UPD_Handle handle, int value)
{
    HDATA(handle).com_settings.flow_control = value;
    return COM_setCts(HDATA(handle).com_hdl, value);
}

/* AT functions */
ICERA_API void SetCommandTimeout(UPD_Handle handle, long timeout)
{
    at_SetCommandTimeout(HDATA(handle).com_hdl, timeout);
}

ICERA_API int SendCmd(UPD_Handle handle, const char *format, ...)
{
    int     result;
    va_list ap;

    va_start(ap, format);
    result = at_SendCmdAP(HDATA(handle).com_hdl, format, ap);
    va_end(ap);
    return result;
}

ICERA_API int SendCmdAndWaitResponse(UPD_Handle handle, const char *ok, const char *error, const char *format, ...)
{
    int     result;
    va_list ap;

    va_start(ap, format);
    result = at_SendCmdAndWaitResponseAP(HDATA(handle).com_hdl, ok, error, format, ap);
    va_end(ap);
    return result;
}

ICERA_API const char *GetResponseEx(UPD_Handle handle)
{
    return at_GetResponse(HDATA(handle).com_hdl);
}

ICERA_API const char *GetResponse(void)
{
    return GetResponseEx(DEFAULT_HANDLE);
}

/* Updater */
ICERA_API int UpdaterEx(UPD_Handle handle, char* archlist, int flash, int pcid)
{
    int result = ErrorCode_Error;

    HDATA(handle).progress.step = ProgressStep_Init;
    HDATA(handle).progress.percent = 0;

    OutDebug(handle, "Updater");
    OutDebug(handle, "DLL version: %s", DLD_VERSION);
    if (archlist)
    {
        OutDebug(handle, "Upload%s %d:%s", flash ? " and flash" : "", strlen(archlist), archlist);
        /* Assume that we can talk to the modem, so lets log what state it's in */
        if (LogSystemState(handle))
        {
            result = FirmwareUpdate(handle, archlist, pcid, flash);
            if (result == ErrorCode_Success)
            {
                HDATA(handle).progress.step = ProgressStep_Finish;
                LogSystemState(handle);
            }
            else
            {
                OutError(handle, "\n#Error: Upgrade failure: %d\n\n", result);
                HDATA(handle).progress.step = ProgressStep_Error;
            }
        }
    }
    return result;
}

ICERA_API void ProgressEx(UPD_Handle handle, int* step, int* percent)
{
    OutDebug(handle, "ProgressEx");
    if (HDATA(handle).progress.totalweight)
    {
        *step = HDATA(handle).progress.step;
        *percent = HDATA(handle).progress.percent / HDATA(handle).progress.totalweight;
        OutDebug(handle, "Step %d: %d%%", *step, *percent);
    }
    else
    {
        *step = ProgressStep_Init;
        *percent = 0;
    }
}

/* Firmware compatibility */
ICERA_API int CheckHostCompatibilityEx(UPD_Handle handle, char* response_to_aticompat, char* firmware_version, char* host_version)
{
    char    modem[10/* 32bits */ + 1/*=*/ + 1/*\x00*/];
    char*   scan;
    char*   next;

    OutDebug(handle, "CheckHostCompatibility");
    OutDebug(handle, "DLL version: %s", DLD_VERSION);
    do
    {
        if ((response_to_aticompat == NULL) || (firmware_version == NULL) || (host_version == NULL))
        {
            OutError(handle, "\n#Error: Null pointer [%p, %p, %p]\n\n", response_to_aticompat, firmware_version, host_version);
            break;
        }
        OutDebug(handle, "ICompat %d:%s", strlen(response_to_aticompat), response_to_aticompat);
        OutDebug(handle, "Firmware version %d:%s", strlen(firmware_version), firmware_version);
        OutDebug(handle, "Host version %d:%s", strlen(host_version), host_version);
        if (*host_version == EOS)
        {
            OutError(handle, "\n#Error: Empty host_version [%s]\n\n", host_version);
            break;
        }
        if (!getFirmwareInfo(handle, firmware_version, FW_NUMBER, DIGIT_SET, FW_MODEM, modem, sizeof(modem)))
        {
            break;
        }
        scan = strstr(response_to_aticompat, OS_TYPE);
        if (scan == NULL)
        {
            OutError(handle, "\n#Error: OS type [%s] not found in [%s]\n\n", OS_TYPE, response_to_aticompat);
            break;
        }
        strcat(modem, "=");
        next = strstr(scan, modem);
        if (next == NULL)
        {
            OutError(handle, "\n#Error: Firmware number [%s] not found in [%s]\n\n", modem, scan);
            break;
        }
        next = &next[strlen(modem) - 1];
        do
        {
            if ((*next != '=') && (*next != ',')) return -1/* End of line detected */;
            scan = &next[1];
            next = getNext(scan, ",\x0D\x0A");
            if (next == NULL) return -1/* End of file detected */;
        }
        while(strncmp(scan, host_version, (size_t)(next - scan)));
        OutDebug(handle, "CheckHostCompatibility: %s", (scan[-1] == '=') ? "Synchronized" : "Compatible");
        return (scan[-1] == '=') ? 0/* Synchronized */ : 1/* Compatible */;
    }
    while(0);
    OutDebug(handle, "CheckHostCompatibility: Incompatible");
    return -1/* Incompatible */;
}

ICERA_API char* CheckFirmwareVersionEx(UPD_Handle handle, char* firmware_version, char* firmware_file_list)
{
    tAppliFileHeader  header;
    int         count, idx;
    char*       data_file_list;
    char*       file_list;
    char*   mass_storage = NULL;
    char*   pkgv_file = NULL;

    OutDebug(handle, "DLL version: %s", DLD_VERSION);
    do
    {
        if ((firmware_version == NULL) || (firmware_file_list == NULL))
        {
            OutError(handle, "\n#Error: Null pointer [%p, %p]\n\n", firmware_version, firmware_file_list);
            break;
        }
        OutDebug(handle, "Firmware version %d:%s", strlen(firmware_version), firmware_version);
        OutDebug(handle, "Firmware list %d%s", strlen(firmware_file_list), firmware_file_list);
        data_file_list = (char *)alloca(strlen(firmware_file_list) + 1);
        if (data_file_list == NULL)
        {
            OutError(handle, "\n#Error: Alloc on stack %d failed\n\n", strlen(firmware_file_list) + 1);
            *firmware_version = EOS;
            break;
        }
        *data_file_list = EOS;
        file_list = (char *)alloca(strlen(firmware_file_list) + 1);
        if (file_list == NULL)
        {
            OutError(handle, "\n#Error: Alloc on stack %d failed\n\n", strlen(firmware_file_list) + 1);
            *firmware_version = EOS;
            break;
        }
        strcpy(file_list, firmware_file_list);
        *firmware_file_list = EOS;
        count = splitString(file_list, FILE_SEPARATOR);
        for (idx = 0 ; idx < count ; idx++)
        {
            /* Strip all EOS */
            while (*file_list == EOS) file_list++;

            OutDebug(handle, "File %d/%d: [%s]", idx + 1, count, file_list);
            if (IS_PKGV_FILE(file_list))
            {
                pkgv_file = file_list;
                OutDebug(handle, "PKGV detected: %s", pkgv_file);
            }
            else if (GetWrappedHeader(handle, &header, file_list, "Get ") > 0)
            {
                OutDebug(handle, "Arch ID: %d", GET_ARCH_ID(header.file_id));
                switch(GET_ARCH_ID(header.file_id))
                {
                case ARCH_ID_APP:
                case ARCH_ID_IFT:
                case ARCH_ID_LDR:
                case ARCH_ID_MASS:
                    if (CheckFWVers(handle, GET_ARCH_ID(header.file_id), firmware_version, file_list))
                    {
                        OutDebug(handle, "Add: %s", file_list);
                        strcat(firmware_file_list, file_list);
                        strcat(firmware_file_list, FILE_SEPARATOR);
                    }
                    break;

                case ARCH_ID_ZEROCD:
                    if (CheckISOVers(handle, firmware_version, file_list) != FILE_NO_UPDATE)
                    {
                        OutDebug(handle, "Add: %s", file_list);
                        /* Keep track of where the ISO name is */
                        mass_storage = file_list;
                    }
                    break;

                case ARCH_ID_COMPAT:
                case ARCH_ID_AUDIOCFG:
                case ARCH_ID_CALIB_PATCH:
                case ARCH_ID_SSL_CERT:
                case ARCH_ID_PRODUCTCFG:
                case ARCH_ID_FLASHDISK:
                case ARCH_ID_WEBUI_PACKAGE:
                    strcat(data_file_list, file_list);
                    strcat(data_file_list, FILE_SEPARATOR);
                    break;
                }
            }
            /* Next file */
            file_list = &file_list[strlen(file_list) + 1/* EOS */];
        }
        /* Remove last FILE_SEPARATOR if it exists */
        if ((strlen(firmware_file_list) > 0) || (strlen(data_file_list) > 0) || mass_storage || pkgv_file)
        {
            /* Keep the zerocd image at the front of the list if there is one */
            if (mass_storage)
            {
                /*
                 * Move the existing string down far enough to fit the zerocd name at the beginning. We know there is
                 * room for this as we allocated enough memory before.
                 */
                memmove(firmware_file_list + strlen(mass_storage) + strlen(FILE_SEPARATOR), firmware_file_list, strlen(firmware_file_list) + 1);
                memcpy(firmware_file_list, mass_storage, strlen(mass_storage));
                memcpy(&firmware_file_list[strlen(mass_storage)], FILE_SEPARATOR, strlen(FILE_SEPARATOR));
            }
            if (strlen(data_file_list) > 0)
            {
                strcat(firmware_file_list, data_file_list);
                strcat(data_file_list, FILE_SEPARATOR);
            }
            /* Add pkgv file if it is there and we're already updating something (if we're not updating anything, we should not attempt to update the package version) */
            if (pkgv_file)
            {
                strcat(firmware_file_list, pkgv_file);
            }
            else
            {
                /* Suppress the last FILE_SEPARATOR */
                firmware_file_list[strlen(firmware_file_list) - 1/* EOS */] = EOS;
            }
        }
        OutDebug(handle, "Firmware list %d:%s", strlen(firmware_file_list), firmware_file_list);
    }
    while(0);
    return firmware_file_list;
}

/* Misc */
ICERA_API int VerboseLevelEx(UPD_Handle handle, int level)
{
    if (level > VERBOSE_GET_DATA)
    {
        HDATA(handle).option_verbose = COM_VerboseLevel(HDATA(handle).com_hdl, level);
    }

    return HDATA(handle).option_verbose;
}

ICERA_API char* GetLibVersion(char* version, int size)
{
    if (strlen(DLD_VERSION) < (size_t)size)
    {
        strcpy(version, DLD_VERSION);
        return version;
    }
    return NULL;
}

/* Legacy (deprecated) API */
ICERA_API int Updater(char* archlist, char *dev_name, int speed, int delay, unsigned short block_sz, int flash, int verbose, int modechange, int pcid)
{
    int res = ErrorCode_Error;

    if (dev_name && archlist)
    {
        OutDebug(DEFAULT_HANDLE, "Update%s on %s: [%s], Speed=%d, Delay=%d, BlockSize=%d, LastMode=%d, ChipID=%s", flash ? " and flash" : "", dev_name, archlist, speed, delay, block_sz, modechange, pcid ? "yes" : "no");

        InitHandle(DEFAULT_HANDLE);

        /* Open port - note that the Open() API will also set the output callback so no need to do that if using Open() */
        if (openPort(DEFAULT_HANDLE, dev_name))
        {
            HDATA(DEFAULT_HANDLE).option_dev_name = strdup(dev_name);
            /* Default output callback */
            at_SetOutputCB(HDATA(DEFAULT_HANDLE).com_hdl, OutStandard, DEFAULT_HANDLE);

            /* Note, dev_name only valid while this function's stack is valid - but should not be a problem, nobody
             * should depend on it being valid after the function returns, as this function opens and closes the port
             */
            VerboseLevelEx(DEFAULT_HANDLE, verbose);
            SetModeChange(DEFAULT_HANDLE, modechange);
            SetCheckModeRestore(DEFAULT_HANDLE, global_check_mode_restore);
            SetSpeed(DEFAULT_HANDLE, speed);
            SetDelay(DEFAULT_HANDLE, delay);
            SetBlockSize(DEFAULT_HANDLE, block_sz);
            SetDisableFullCoredumpSetting(DEFAULT_HANDLE, disable_full_coredump);
            res = UpdaterEx(DEFAULT_HANDLE, archlist, flash, pcid);
            closePort(DEFAULT_HANDLE);
            free(HDATA(DEFAULT_HANDLE).option_dev_name);
            HDATA(DEFAULT_HANDLE).option_dev_name = NULL;
        }
        else
        {
            res = ErrorCode_DeviceNotFound;
        }
    }
    else
    {
        OutError(DEFAULT_HANDLE, "\n#Error: Null pointer [%p, %p]\n\n", dev_name, archlist);
    }
    return res;
}

ICERA_API void Progress(int* step, int* percent)
{
    ProgressEx(DEFAULT_HANDLE, step, percent);
}

ICERA_API int VerboseLevel(int level)
{
    return VerboseLevelEx(DEFAULT_HANDLE, level);
}

ICERA_API int CheckHostCompatibility(char* response_to_aticompat, char* firmware_version, char* host_version)
{
    return CheckHostCompatibilityEx(DEFAULT_HANDLE, response_to_aticompat, firmware_version, host_version);
}

ICERA_API char* CheckFirmwareVersion(char* firmware_version, char* firmware_file_list)
{
    return CheckFirmwareVersionEx(DEFAULT_HANDLE, firmware_version, firmware_file_list);
}

ICERA_API void UpdaterLogToFile(char *file)
{
    UpdaterLogToFileEx(DEFAULT_HANDLE, file);
}

ICERA_API void UpdaterLogToFileW(wchar_t *file)
{
    UpdaterLogToFileWEx(DEFAULT_HANDLE, file);
}

#ifdef _WIN32
int PNPEvent(unsigned int pnp_event, unsigned char* dev_broadcast, UPD_Handle handle)
{
    MSG msg;

    PDEV_BROADCAST_DEVICEINTERFACE devinterface = (PDEV_BROADCAST_DEVICEINTERFACE)dev_broadcast;
    if (devinterface->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
    {
        if (!stricmp(HDATA(handle).devicepath, devinterface->dbcc_name))
        {
            switch(pnp_event)
            {
            case DBT_DEVICEREMOVECOMPLETE:
                if (HDATA(handle).timeout)
                {
                    PeekMessage(&msg, HDATA(handle).hwnd, WM_TIMER, WM_TIMER, PM_REMOVE);
                    KillTimer(HDATA(handle).hwnd, IDT_TIMEOUT);
                    if (!SetTimer(HDATA(handle).hwnd, IDT_TIMEOUT, HDATA(handle).timeout, NULL))
                    {
                        OutError(handle, "SetTimer failure\n");
                    }
                }
                break;

            case DBT_DEVICEARRIVAL:
                if (HDATA(handle).timeout)
                {
                    PeekMessage(&msg, HDATA(handle).hwnd, WM_TIMER, WM_TIMER, PM_REMOVE);
                    KillTimer(HDATA(handle).hwnd, IDT_TIMEOUT);
                }
                return TRUE;
            }
        }
    }
    return FALSE;
}
#endif /* _WIN32 */

/* Kept for backward compatibility */
ICERA_API int SelectMode(UPD_Handle handle, int mode)
{
    return SwitchMode(handle, (unsigned int)mode);
}

/* TODO: Can make the method public if required. 
 *       Also, it's assumed the PNP events are good for serial ports, 
 *       so checkSuccess has no effect when the mode is COM_MODE_SERIAL */
static ICERA_API int SwitchModeEx(UPD_Handle handle, unsigned int mode, bool checkSuccess)
{
#ifdef _WIN32
    WORD length;
    int register_handle, index;
#endif

    char command[20];
    int status = ErrorCode_Success;
    unsigned int current_mode =  GetCurrentMode(handle);

    if((int)current_mode == MODE_INVALID)
    {
        OutError(handle, "Switch mode: failed to get current mode\n");
        return ErrorCode_AT;
    }
    if (mode != current_mode)
    {
        status = ErrorCode_Error;
        do
        {
            #ifdef _WIN32
            if(COM_mode(HDATA(handle).com_hdl) == COM_MODE_SERIAL)
            {
                int size;
                length = sizeof(HDATA(handle).devicepath);
                index = GetDevicePathFromComPort(HDATA(handle).option_dev_name, HDATA(handle).devicepath, &length);
                if (!index)
                {
                    OutError(handle, "GetDevicePathFromComPort failure\n");
                    break;
                }
                /* Register and wait PNP event */
                if (StartPnpEvent(handle, PNPEvent, NULL) < 0)
                {
                    OutError(handle, "StartPnpEvent failure\n");
                    break;
                }
                register_handle = RegisterPnpEventByInterface(handle, (GUID *)&guids[index - 1]);
                if (register_handle < 0)
                {
                    OutError(handle, "RegisterPnpEventByInterface failure %d\n", register_handle);
                    break;
                }
                /* Reset the board, Port is closed and we wait the re-enumeration to re-open it */
                size = snprintf(command, sizeof(command), AT_CMD_MODEx, mode);
                if ((size < 0) || (size >= (int)sizeof(command)))
                {
                    command[sizeof(command) - 1] = EOS;
                }
                /* Switch mode to custom init */
                if (AT_FAIL(at_SendCmd(HDATA(handle).com_hdl, command)))
                {
                    OutError(handle, "at_SendCmd failure\n");
                    UnregisterPnpEventByInterface(handle, register_handle);
                    closePort(handle);
                    status = ErrorCode_AT;
                    break;
                }
                closePort(handle);
                OutDebug(handle, "WaitPnpEvent started");
                if (!WaitPnpEvent(handle, 30000/* timeout = 30s */, NULL))
                {
                    OutError(handle, "WaitPnpEvent failure\n");
                    break;
                }
                OutDebug(handle, "WaitPnpEvent finished");
                /* Plug or timeout event */
                if (openPort(handle, HDATA(handle).option_dev_name))
                {
                    status = ErrorCode_Success;
                }
                UnregisterPnpEventByInterface(handle, register_handle);
                OutDebug(handle, "UnregisterPnpEventByInterface finished");
            }
            else
            #endif /* _WIN32 */
            {
                status = DelayedSwitchMode(handle, command, sizeof(command), mode, checkSuccess);
                if (status != ErrorCode_Success)
                {
                    OutError(handle, "Delayed switch mode failure %d\n", status);
                }
            }
            if(status == ErrorCode_Success)
            {
                if (HDATA(handle).com_hdl == 0)
                {
                    status = ErrorCode_ConnectionError;
                    break;
                }
                LogSystemState(handle);
            }
        } while(0);
    }
    return status;
}

ICERA_API int SwitchMode(UPD_Handle handle, unsigned int mode)
{
    return SwitchModeEx(handle, mode, true);
}

#ifdef _WIN32
typedef struct
{
    char*           dev_path;
    unsigned short  dev_path_length;
    char            port[sizeof("COM255")];/* 6 + 1 */
} filterinfo;

static int filter(void* handle, unsigned long dev_inst, char* dev_path, HKEY key, void* ctx)
{
    char        port[sizeof("COM255")];/* 6 + 1 */
    DWORD       length = sizeof(port);
    filterinfo* info = (filterinfo *)ctx;

    ZeroMemory(port, length);
    RegQueryValueEx(key, "PortName", NULL, NULL, (LPBYTE)port, &length);
    if (stricmp(port, info->port) == 0)
    {
        strncpy(info->dev_path, dev_path, info->dev_path_length);
        return 1;
    }
    return 0;
}

ICERA_API int GetDevicePathFromComPort(char* comport, char* buffer, unsigned short* bufferlength)
{
    if (*bufferlength > 0)
    {
        filterinfo  info;
    int index;

        if (strlen(comport) < sizeof(info.port))
    {
            ZeroMemory(&info, sizeof(info));
            strcpy(info.port, comport);
            ZeroMemory(buffer, *bufferlength);
            info.dev_path = buffer;
            info.dev_path_length = *bufferlength - 1;
        for (index = 0 ; index < NB_GUIDS ; index++)
        {
                if (WalkOnDevices((GUID *)&guids[index], filter, &info) > 0)
            {
                    *bufferlength = (unsigned short)strlen(info.dev_path);
                                            return index + 1;
                                        }
                                    }
                                }
        }
    *bufferlength = 0;
    return 0;
}

LRESULT CALLBACK WinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int result = FALSE;
    unsigned long idx;

    if (msg == WM_DEVICECHANGE)
    {
        for (idx = 0 ; idx <= UPD_MAX_HANDLES ; idx++)
            if (hwnd == HDATA(idx).hwnd)
                break;
        assert((idx <= UPD_MAX_HANDLES) && HDATA((UPD_Handle)idx).pnp_callback);
        if (HDATA((UPD_Handle)idx).pnp_callback)
            result = HDATA((UPD_Handle)idx).pnp_callback((unsigned int)wParam, (unsigned char *)lParam, (UPD_Handle)idx);
        if (result == TRUE)
            PostThreadMessage(HDATA((UPD_Handle)idx).threadid, WM_QUIT, 0, 0);
        return (LRESULT)TRUE;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

ICERA_API int UnregisterPnpEventByInterface(UPD_Handle handle, int regiter_handle)
{
    BOOL result;

    if (handle <= DEFAULT_HANDLE)
        handle = DEFAULT_HANDLE;
    if (regiter_handle < 0)
    {
        OutError(handle, "\nBad parameter 2: should be higher or equal to 0\n\n");
        return -1;
    }
    if (!HDATA(handle).hdev[regiter_handle])
    {
        OutError(handle, "\nAlready unregistered: %d\n\n", regiter_handle);
        return -2;
    }
    result = UnregisterDeviceNotification(HDATA(handle).hdev[regiter_handle]);
    HDATA(handle).hdev[regiter_handle] = NULL;
    return (int)result;
}

ICERA_API int RegisterPnpEventByInterface(UPD_Handle handle, GUID* guid)
{
    unsigned long idx;
    DEV_BROADCAST_DEVICEINTERFACE notifyFilter;

    if (handle <= DEFAULT_HANDLE)
        handle = DEFAULT_HANDLE;
    if (!HDATA(handle).hwnd)
    {
        OutError(handle, "\nStartPnpEvent function should be called before\n\n");
        return -1;
    }
    memset(&notifyFilter, 0, sizeof(notifyFilter));
    notifyFilter.dbcc_size = sizeof(notifyFilter);
    notifyFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    memcpy(&notifyFilter.dbcc_classguid, guid, sizeof(GUID));
    for (idx = 0 ; idx < MAX_REGISTERED ; idx++)
        if (!HDATA(handle).hdev[idx])
            break;
    if (idx == MAX_REGISTERED)
    {
        OutError(handle, "\nMaximum registered GUID has been reached: %d\n\n", MAX_REGISTERED);
        return -2;
    }
    HDATA(handle).hdev[idx] = RegisterDeviceNotification(HDATA(handle).hwnd, &notifyFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!HDATA(handle).hdev[idx])
    {
        OutError(handle, "\nRegisterDeviceNotification error: %08X\n\n", GetLastError());
        return -3;
    }
    return idx;
}

ICERA_API int StopPnpEvent(UPD_Handle handle)
{
    if (handle <= DEFAULT_HANDLE)
        handle = DEFAULT_HANDLE;
    if (!HDATA(handle).hwnd)
        return FALSE;
    return PostThreadMessage(HDATA(handle).threadid, WM_QUIT, 0, 0);
}

ICERA_API int StartPnpEvent(UPD_Handle handle, pnp_callback_type pnp_callback, HWND* hwnd)
{
    int size;
    char className[18 + 1]; /* Max: UpdaterPnPEvent255 */
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.hInstance = GetModuleHandle(0);
    wc.lpszClassName = className;
    wc.lpfnWndProc = WinProc;

    if (handle <= DEFAULT_HANDLE)
    {
        handle = DEFAULT_HANDLE;
    }
    size = snprintf(className, sizeof(className), "UpdaterPnPEvent%d", (int)handle);
    if ((size < 0) || (size >= (int)sizeof(className)))
    {
        OutError(handle, "\nStartPnpEvent:snprintf function failed: %d\n\n", size);
        return 0;
    }
    if (HDATA(handle).hwnd)
    {
        OutError(handle, "\nStopPnpEvent function should be called before\n\n");
        return 0;
    }
    if (!RegisterClass(&wc))
    {
        OutError(handle, "\nRegisterClass error: %08X\n\n", GetLastError());
        return 0;
    }
    HDATA(handle).pnp_callback = NULL;
    HDATA(handle).hwnd = CreateWindowEx(WS_EX_TOPMOST, className, className, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);
    if (!HDATA(handle).hwnd)
    {
        OutError(handle, "\nCreateWindowEx error: %08X\n\n", GetLastError());
        return 0;
    }
    HDATA(handle).pnp_callback = pnp_callback;
    if (hwnd)
        *hwnd = HDATA(handle).hwnd;
    return 1;
}

ICERA_API int WaitPnpEvent(UPD_Handle handle, unsigned int timeout_in_ms, unsigned long* threadid)
{
    int size;
    MSG msg;
    char className[18 + 1]; /* Max: UpdaterPnPEvent255 */

    if (handle <= DEFAULT_HANDLE)
    {
        handle = DEFAULT_HANDLE;
    }
    size = snprintf(className, sizeof(className), "UpdaterPnPEvent%d", (int)handle);
    if ((size < 0) || (size >= (int)sizeof(className)))
    {
        OutError(handle, "\nWaitPnpEvent:snprintf function failed: %d\n\n", size);
        return 0;
    }    if (!HDATA(handle).hwnd)
    {
        OutError(handle, "\nStartPnpEvent function should be called before\n\n");
        return 0;
    }
    assert(HDATA(handle).pnp_callback);
    HDATA(handle).threadid = GetCurrentThreadId();
    if (threadid)
        *threadid = HDATA(handle).threadid;
    HDATA(handle).timeout = timeout_in_ms;  /* 0 == infinite */
    if (HDATA(handle).timeout)
    {
        if (!SetTimer(HDATA(handle).hwnd, IDT_TIMEOUT, HDATA(handle).timeout, NULL))
        {
            OutError(handle, "\nSetTimer function failed\n\n");
            DestroyWindow(HDATA(handle).hwnd);
            UnregisterClass(className, GetModuleHandle(0));
            HDATA(handle).hwnd = NULL;
            return 0;
        }
    }
    while (TRUE)
    {
        BOOL bRet = GetMessage(&msg, NULL, 0, 0);
        if ((bRet == 0) || (bRet == -1) || (msg.message == WM_QUIT) || (msg.message == WM_TIMER))
            break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (HDATA(handle).timeout)
    {
        PeekMessage(&msg, HDATA(handle).hwnd, WM_TIMER, WM_TIMER, PM_REMOVE);
        KillTimer(HDATA(handle).hwnd, IDT_TIMEOUT);
    }
    DestroyWindow(HDATA(handle).hwnd);
    UnregisterClass(className, GetModuleHandle(0));
    HDATA(handle).hwnd = NULL;
	if (msg.message == WM_TIMER)
	{
		OutError(handle, "Exit on timeout\n");
		return 2;
	}
	/* Success */
	return 1;
}

static int RegGetStringValue(HKEY root, char* keypath, char* value, char* data, unsigned long* datalength)
{
    HKEY    key;
    int     result = status_not_found;

    if (RegOpenKeyEx(root, keypath, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        switch (RegQueryValueEx(key, value, NULL, NULL, (LPBYTE)data, datalength))
        {
            case ERROR_SUCCESS: result = status_found; break;
            case ERROR_MORE_DATA: result = status_no_enough_memory; break;
        }
        RegCloseKey(key);
    }
    return result;
}

static char* GetAllKeys(HKEY root, char* keypath)
{
    HKEY    key;
    LONG    status;
    char*   data = NULL;

    status = RegOpenKeyEx(root, keypath, 0, KEY_READ, &key);
    if (status == ERROR_SUCCESS)
    {
        DWORD   dwSize = 0x1000;
        do
        {
            data = (char *)malloc(dwSize);
            if (data)
            {
                char*   free_data = data;
                DWORD   dwIndex = 0;
                DWORD   free_size = dwSize - 1;

                status = RegEnumKey(key, dwIndex, free_data, free_size);
                while (status == ERROR_SUCCESS)
                {
                    dwIndex++;
                    free_size -= strlen(free_data);
                    strcat(data, FILE_SEPARATOR);
                    free_size -= strlen(FILE_SEPARATOR);
                    free_data += strlen(free_data);
                    status = RegEnumKey(key, dwIndex, free_data, free_size);
                }
                if (status == ERROR_MORE_DATA)
                {
                    dwSize <<= 1;
                    free(data);
                }
                else if (status != ERROR_NO_MORE_ITEMS)
                {
                     free(data);
                     data = NULL;
                }
            }
        }
        while (status == ERROR_MORE_DATA);
        RegCloseKey(key);
    }
    return data;
}

ICERA_API int WalkOnEnums(char* servicename, enums_callback_type callback, void* ctx)
{
    unsigned long length;
    char  enumpath[MAX_PATH];
    char  instance[sizeof("255")];
    int result = status_no_enough_memory;
    char* keypath = (char *)malloc(sizeof(REG_SERVICES) + strlen(servicename) + sizeof("Parameters") + 1);
    if (keypath)
    {
        int index = 0;

        result = status_not_found;
        strcpy(keypath, REG_SERVICES);
        strcat(keypath, "\\");
        strcat(keypath, servicename);
        strcat(keypath, "\\Enum");
        for (index = 0 ; index < 256 ; index++)
        {
            int size = snprintf(instance, sizeof(instance) - 1, "%d", index);
            if ((size > 0) && (size < 4))
            {
                length = sizeof(enumpath);
                if (RegGetStringValue(HKEY_LOCAL_MACHINE, keypath, instance, enumpath, &length) == status_found)
                {
                    HKEY key;

                    char* keyenumpath = (char *)malloc(sizeof(REG_ENUM) + strlen(enumpath) + sizeof("Device Parameters") + 1);
                    if (keyenumpath)
                    {
                        strcpy(keyenumpath, REG_ENUM);
                        strcat(keyenumpath, "\\");
                        strcat(keyenumpath, enumpath);
                        strcat(keyenumpath, "\\Device Parameters");
                        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyenumpath, 0, KEY_READ, &key) == ERROR_SUCCESS)
                        {
                            int res = callback(index, enumpath, key, ctx);
                            if (res)
                            {
                                /* Success */
                                result = res;
                                RegCloseKey(key);
                                free(keyenumpath);
                                break;
                            }
                            RegCloseKey(key);
                        }
                        free(keyenumpath);
                    }
                }
            }
        }
        free(keypath);
    }
    return result;
}

ICERA_API int WalkOnServices(char* servicename, services_callback_type callback, void* ctx)
{
    int     index, count, result = status_bad_parameter + 1;

    if (servicename)
    {
        char* services = GetAllKeys(HKEY_LOCAL_MACHINE, REG_SERVICES);
        if (services)
        {
            char* pos;
            bool found, end = false;
            bool begin = (servicename[0] == '*');
            result = status_not_found;
            if (begin)
            {
                servicename++;
            }
            if (strlen(servicename) > 0)
            {
                end = (servicename[strlen(servicename) - 1] == '*');
                if (end)
                {
                    servicename[strlen(servicename) - 1] = '\0';
                }
            }
            count = splitString(services, FILE_SEPARATOR);
            for (index = 0 ; index < count ; index++)
            {
                /* Strip all EOS */
                while (*services == EOS) services++;
                found = false;
                if (strlen(servicename) == 0)
                {
                    found = true;
                }
                else
                {
                    pos = strstr(services, servicename);
                    if (pos)
                    {
                        found = true;
                        if ((pos > services) && !begin)
                        {
                            found = false;
                        }
                        if ((strlen(pos) > strlen(servicename)) && !end)
                        {
                            found = false;
                        }
                    }
                }
                if (found)
                {
                    HKEY key;

                    char* keypath = (char *)malloc(sizeof(REG_SERVICES) + strlen(services) + sizeof("Parameters") + 1);
                    if (keypath)
                    {
                        strcpy(keypath, REG_SERVICES);
                        strcat(keypath, "\\");
                        strcat(keypath, services);
                        strcat(keypath, "\\Parameters");
                        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keypath, 0, KEY_READ, &key) == ERROR_SUCCESS)
                        {
                            int res = callback(services, key, ctx);
                            if (res)
                            {
                                /* Success */
                                result = res;
                                RegCloseKey(key);
                                free(keypath);
                                break;
                            }
                            RegCloseKey(key);
                        }
                        free(keypath);
                    }
                }
                /* Next file */
                services = &services[strlen(services) + 1/* EOS */];
            }
            free(services);
        }
    }
    return result;
}

ICERA_API int WalkOnDevices(GUID* guid, discovery_callback_type callback, void* ctx)
{
    int     result = status_bad_parameter + 2; /* Missing callback argument */

    if (callback)
    {
        result = status_bad_parameter + 1; /* Unknown GUID value */
        if (guid)
        {
            DWORD   dwSize;
            char    guidstr[sizeof(GUID_STRING)];

            dwSize = sizeof(guidstr);
            if (snprintf(guidstr, dwSize, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                         (unsigned int)guid->Data1, guid->Data2, guid->Data3,
                         guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                         guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]) == strlen(GUID_STRING))
            {
                HDEVINFO        hDevInfo;
                HKEY            key;
                SP_DEVINFO_DATA spdd;
                DWORD           dwIndex = 0;
                char            guidkey[MAX(sizeof(REG_CTRL_CLASS), sizeof(REG_CTRL_DEVCLASS)) + sizeof(guidstr)];

                strcpy(guidkey, REG_CTRL_CLASS);
                strcat(guidkey, "\\");
                strcat(guidkey, guidstr);
                if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, guidkey, 0, KEY_READ, &key) == ERROR_SUCCESS)
                {
                    RegCloseKey(key);
                    result = status_not_found; /* Default error with CLASS GUID value */
                    /* Get device interface info set handle for all devices attached to system */
                    hDevInfo = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT);
                    if (hDevInfo != INVALID_HANDLE_VALUE)
                    {
                        ZeroMemory(&spdd, sizeof(spdd));
                        spdd.cbSize = sizeof(spdd);
                        while (SetupDiEnumDeviceInfo(hDevInfo, dwIndex, &spdd))
                        {
                            dwIndex++;
                            key = SetupDiOpenDevRegKey(hDevInfo, &spdd, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
                            if (key != INVALID_HANDLE_VALUE)
                            {
                                int res = callback(hDevInfo, spdd.DevInst, NULL, key, ctx);
                                if (res)
                                {
                                    /* Success */
                                    result = res;
                                    RegCloseKey(key);
                                    break;
                                }
                                RegCloseKey(key);
                            }
                        }
                        SetupDiDestroyDeviceInfoList(hDevInfo);
                    }
                }
                else
                {
                    strcpy(guidkey, REG_CTRL_DEVCLASS);
                    strcat(guidkey, "\\");
                    strcat(guidkey, guidstr);
                    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, guidkey, 0, KEY_READ, &key) == ERROR_SUCCESS)
                    {
                        RegCloseKey(key);
                        result = status_not_found; /* Default error with DEVCLASS GUID value */
                        /* Get device interface info set handle for all devices attached to system */
                        hDevInfo = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
                        if (hDevInfo != INVALID_HANDLE_VALUE)
                        {
                            /* Retrieve a context structure for a device interface of a device information set */
                            BYTE                                Buffer[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + MAX_PATH];
                            PSP_DEVICE_INTERFACE_DETAIL_DATA    pspdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)Buffer;
                            SP_DEVICE_INTERFACE_DATA            spdid;

                            spdid.cbSize = sizeof(spdid);
                            while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, guid, dwIndex, &spdid))
                            {
                                dwIndex++;
                                dwSize = 0;
                                SetupDiGetDeviceInterfaceDetail(hDevInfo, &spdid, NULL, 0, &dwSize, NULL);
                                /* check the buffer size */
                                if (dwSize && (dwSize <= sizeof(Buffer)))
                                {
                                    pspdidd->cbSize = sizeof(*pspdidd); /* 5 Bytes! */
                                    ZeroMemory(&spdd, sizeof(spdd));
                                    spdd.cbSize = sizeof(spdd);
                                    if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &spdid, pspdidd, dwSize, &dwSize, &spdd))
                                    {
                                        /* Convert Unicode to Ascii */
                                        dwSize -= (sizeof(pspdidd->cbSize) + 1);
                                        if (strlen(pspdidd->DevicePath) < dwSize)
                                        {
                                            int index = 0;
                                            while((pspdidd->DevicePath[index + 1] == 0) && (index < (int)dwSize))
                                            {
                                                if (pspdidd->DevicePath[index + 2] == 0)
                                                    break;
                                                memmove(&pspdidd->DevicePath[index + 1], &pspdidd->DevicePath[index + 2], dwSize - (index + 1));
                                                index++;
                                            }
                                        }
                                        key = SetupDiOpenDevRegKey(hDevInfo, &spdd, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
                                        if (key != INVALID_HANDLE_VALUE)
                                        {
                                            int res = callback(hDevInfo, spdd.DevInst, (char *)pspdidd->DevicePath, key, ctx);
                                            if (res)
                                            {
                                                /* Success */
                                                result = res;
                                                RegCloseKey(key);
                                                break;
                                            }
                                            RegCloseKey(key);
                                        }
                                    }
                                }
                            }
                            SetupDiDestroyDeviceInfoList(hDevInfo);
                        }
                    }
                }
            }
        }
    }
    return result;
}
#endif /* _WIN32 */

extern unsigned short ComputeChecksum16(void * p, int lg)
{
    unsigned short *ptr = (unsigned short *)p;
    unsigned short chksum = 0;
    unsigned short * end = ptr + lg/2;

    while (ptr < end)
    {
        chksum ^= *ptr++;
    }

    return chksum;
}

#ifdef _WIN32
#ifdef ICERA_EXPORTS
// GetProcAddress to have access to any funtions
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
#else
BOOL WINAPI Updater_Initialize(DWORD fdwReason)
#endif
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH: // Called by LoadLibrary
        updater_load();
        break;
    case DLL_PROCESS_DETACH: // Called by FreeLibrary
        updater_unload();
        break;
    }
    return TRUE;
}
#else
#ifndef ANDROID
// dlsym to have access to any funtions
// On Android, we build Updater.c statically to be linked in downloader:
//  the fact of having below symbols in binary is preventing downloader
//  to correctly return at exit...
static void __attribute__ ((constructor)) library_load(void) // Called by dlopen()
{
    updater_load();
}

static void __attribute__ ((destructor)) library_unload(void) // Called by dlclose()
{
    updater_unload();
}
#endif
#endif

// TODO to be removed ---------------------------------------
ICERA_API int GetBuildFileProperties(char *name, int *arch_id, int *is_appli)
{
    int result = -1;
    unsigned int i;

    for(i=0; i < arch_type_max_id; i++)
    {
        if(strcmp(name, arch_type[i].acr) == 0)
        {
            *arch_id    = arch_type[i].arch_id;
            *is_appli   = (arch_type[i].type == ARCH_TYPE_APPLI) ? 1 : 0;
            result = 0;
        }
    }

    return result;
}

ICERA_API int GetMagic(int chip_value, unsigned int *magic)
{
    int result = -1;

    switch(chip_value)
    {
    case 8040:
        *magic = ARCH_TAG_8040;
        result = 0;
        break;
    case 8060:
        *magic = ARCH_TAG_8060;
        result = 0;
        break;

    default:
        break;
    }

    return result;
}

ICERA_API unsigned int GetBt2TrailerMagic(void)
{
    return ARCH_TAG_BT2_TRAILER;
}

ICERA_API void DisplayAvailableArchTypesEx(UPD_Handle handle)
{
    unsigned int i;

    for(i=0; i < arch_type_max_id; i++)
    {
        OutStandard(handle, "%s ", arch_type[i].acr);
    }
    OutStandard(handle, "\n\n");
}

ICERA_API void DisplayAvailableArchTypes(void)
{
    DisplayAvailableArchTypesEx(DEFAULT_HANDLE);
}
// ----------------------------------------------------------
