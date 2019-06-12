/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/
 /**
  * @mainpage Introduction
  *This document describes the ICERA API functions in the Updater.dll which is the dynamic link library for downloader tools.\n
  *The API functions in the dll can be used for downloader tools.\n
  *When customers make their own tool to update Icera software files, this document can be a reference for it.
  *
  *This document has the following sections.\n
  *  1.Icera API functions and handle structure for multiple instances in the dll\n
  *  2.Description and figure for Download Protocol \n
  *  3.Flow chart and sample python codes for downloading all files \n
  *  4.How to generate a downloader tool in CodeBlocks\n
  *
  *Below is the section for the Icera API functions and handle structure for multiple instances in the dll.\n
  *This section is devided into 4 detailed parts with the following link. \n
  *The detailed descriptions and figures for each part can be read if the link is clicked.\n
  *
  * @section updaterfunc < The functions description for Updater >
  * This part describes the purpose, parameters and return values of each main updater function in Updater.h\n
  *
  * @link updater The functions description for Updater dll
  * @endlink \n
  * @section comportfunc < The functions description for Com Port >
  * This part describes the functions which are used to make a device communicate with a host and transfer commands and data through COM port.\n
  *
  * @link Comport The functions description for Com port
  * @endlink \n
  * @section atcmdfunc < The functions description for AT command >
  * This part describes the functions which are used to make a device send AT commands to a host and receive a response for it from the host.\n
  *
  * @link ATcmd The functions description for AT Command
  * @endlink \n
  * @section handle < Handle structure for multiple instances >
  * This part show the structure for the handle which is an identifier for each instance. \n
  * This handle structure makes it possible that the same function is used at the same time in parallel for multi-download. \n
  *
  * @link handle_diagram UPD Handle structure diagram
  * @endlink \n
  * @link UPD_Handle Handle structure for multiple instances
  * @endlink \n
  *
  *Below is the section for Download Protocol.\n
  *The linked figure illustrates the message sequence of the download procedure between a device and a host.\n
  *The downloader tool is running on the host side and Livanto, AT CI(Command Interpreter), Loader application are on the device side. \n
  *For more information, please refer to the Icera document AN002.\n
  *
  * @link download Download Protocol
  * @endlink \n
  *
  *Below is the section for the usage example of the API functions.\n
  *The first linked flow chart illustrates how to update Icera Software and how to use the ICERA API functions of the dll in "Release Update"
  *of "Flashing" tab of Ristretto(Icera downloader tool) for example. \n
  *The second linked sample python codes show how to make codes for downloading all selected files.
  *For more information, please refer to "download_all.py" under tools folder of the delivered full source codes.\n
  *
  * @link example Example : Flow Chart for downloading all selected files
  * @endlink \n
  *
  * @link sample Sample python codes for downloading all selected files
  * @endlink \n
  *
  *Below is the section for how to generate a downloader tool in CodeBlocks.\n
  *We are using CodeBlocks to develope internal downloader tools. So, you can refer to this section for how to generate
  *a downloader tool for example.\n
  *The linked procedure explains how to build a project file and generate the downloader tool like downloader.exe in CodeBlocks.\n
  *The downloader.exe is delivered as a sample on how to generate a .exe, embedding UpdaterLib as static libraries.\n
  *Please use "integration_tools/downloader/build/downloader.cbp" to generate it. This project rebuild UpdaterLib static libray
  *and finally linking that in downloader.exe. We also deliver "integration_tools/updaterlib/build/updaterlib.cbp" from which Updater.dll
  *can be generated.\n
  *
  * @link build How to generate a downloader tool in CodeBlocks
  * @endlink \n
  *
  * @page handle_diagram UPD Handle structure diagram
  * @image html UPD_handle.png
  * @page download Download Protocol
  * @image html download_protocol.png
  * @page example Example : Flow Chart for downloading all selected files
  * @image html release_update.png
  * @page sample Sample python codes for downloading all selected files
  * @image html download_all_sample.png
  * @page build How to generate a downloader tool in CodeBlocks
  * @image html how_to_build.png
  */

/**
 * @file Updater.h Header file for Icera archive file downloader via the host
 *       interface.
 *
 */

#ifndef ICERA_UPDATERLIB_H
#define ICERA_UPDATERLIB_H

#include "Globals.h"
#include <stdio.h>

#ifdef _WIN32

#include <dbt.h>
#include <winuser.h>
#include <devguid.h>
#include <assert.h>

#define MAX_REGISTERED	10

#endif /* _WIN32 */

#ifdef _WIN32
#define MUTEX_TYPE          HANDLE
#define MUTEX_CREATE(M)     (M) = CreateMutex(NULL, FALSE, "UpdaterLib")
#define MUTEX_DESTROY(M)    CloseHandle(M)
#define MUTEX_LOCK(M)       WaitForSingleObject((M), INFINITE)
#define MUTEX_UNLOCK(M)     ReleaseMutex(M)
#else
#if defined(__unix__) || defined(__MAC_OS__) || defined(ANDROID)
#include <pthread.h>
#define MUTEX_TYPE          pthread_mutex_t
#define MUTEX_CREATE(M)     pthread_mutex_init(&(M), NULL)
#define MUTEX_DESTROY(M)    pthread_mutex_destroy(&(M))
#define MUTEX_LOCK(M)       pthread_mutex_lock(&(M))
#define MUTEX_UNLOCK(M)     pthread_mutex_unlock(&(M))
#endif
#endif

#define USE_STDINT_H
#include "com_api.h"
#include "drv_arch_type.h"

#define MAX_LOGFILE_SIZE (1024*1024)     /* 1 MB */
#define MAX_LOGMSG_SIZE  (16384)         /* 16 kB */

extern const int ICERA_API ErrorCode_Error;
extern const int ICERA_API ErrorCode_Success;
extern const int ICERA_API ErrorCode_FileNotFound;
extern const int ICERA_API ErrorCode_DeviceNotFound;
extern const int ICERA_API ErrorCode_AT;
extern const int ICERA_API ErrorCode_DownloadError;
extern const int ICERA_API ErrorCode_ConnectionError;
extern const int ICERA_API ErrorCode_InvalidHeader;

/* Kept for backward compatibility */
extern const int ICERA_API ErrorCode_Sucess;

extern const int ICERA_API ProgressStep_Error;
extern const int ICERA_API ProgressStep_Init;
extern const int ICERA_API ProgressStep_Download;
extern const int ICERA_API ProgressStep_Flash;
extern const int ICERA_API ProgressStep_Finalize;
extern const int ICERA_API ProgressStep_Finish;


typedef unsigned long UPD_Handle;

extern const int ICERA_API status_success;
extern const int ICERA_API status_error;
extern const int ICERA_API status_found;
extern const int ICERA_API status_not_found;
extern const int ICERA_API status_no_enough_memory;
extern const int ICERA_API status_bad_parameter;

/**
* @details This CALLBACK function is called when SetLogCallbackFunc or SetLogCallbackFuncEx is used.
* @param msg -Is an ANSI string pointer that contains logging message.
*/
typedef void (*log_callback_type)(char *msg);

#ifdef _WIN32
/**
* @details This CALLBACK function is called for each registered PNP event.
* @param pnp_event -PNP event number. (See WM_DEVICECHANGE in the MSDN for more details)
* @param dev_broadcast -Device broadast structure. (See DEV_BROADCAST_HDR in the MSDN for more details)
* @param handle -UPD_Handle on the current instance. (Needed by some others DLL functions)
* @retval true stop to wait PNP events.
* @retval false carry on to wait any others registered PNP events.
*/
typedef int (*pnp_callback_type)(unsigned int pnp_event, unsigned char* dev_broadcast, UPD_Handle handle);

/**
* @details This CALLBACK function is called for each item of a specified class.
* @param handle -HDEVINFO handle. (See MSDN for more details)
* @param dev_inst -Device instance number. (See SP_DEVINFO_DATA in the MSDN for more details)
* @param dev_path -Device path found in DeviceClasses. Null for Class guid. (See SP_DEVICE_INTERFACE_DETAIL_DATA in the MSDN for more details)
* @param key -Handle on hardware key. (See SetupDiOpenDevRegKey in the MSDN for more details)
* @param ctx -Context pointer given by the caller and directly given to the callback. (Optional)
* @retval true stop registry parsing.
* @retval false carry on to parse registry.
*/
typedef int (*discovery_callback_type)(void* handle, unsigned long dev_inst, char* dev_path, HKEY key, void* ctx);

/**
* @details This CALLBACK function is called on servicename entries that match with filter.
* @param index -Zero based index found in [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\"servicename"\Enum\"index"]
* @param enumpath -Full registry path for the "index" instance [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\...]
* @param key -Handle on the [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\...\Device Parameters] registry key.
* @param ctx -Context pointer given by the caller and directly given to the callback. (Optional)
* @retval true stop registry parsing.
* @retval false carry on to parse registry.
*/
typedef int (*enums_callback_type)(unsigned int index, char* enumpath, HKEY key, void* ctx);

/**
* @details This CALLBACK function is called on servicename entries that match with filter.
* @param servicename -Service name found in [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services].
* @param key -Handle on the [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\"servicename"\Parameters] registry key.
* @param ctx -Context pointer given by the caller and directly given to the callback. (Optional)
* @retval true stop registry parsing.
* @retval false carry on to parse registry.
*/
typedef int (*services_callback_type)(char* servicename, HKEY key, void* ctx);

#ifndef ICERA_EXPORTS
#define UPDATER_INITIALIZE		DLL_PROCESS_ATTACH
#define UPDATER_DEINITIALIZE	DLL_PROCESS_DETACH
/**
* @details This function is used to initialize and deinitialize the Updater Lib when it's not built as a DLL.
* @param fdwReason - UPDATER_INITIALIZE to initialize, UPDATER_DEINITIALIZE to deinitialize.
* @retval true for Success.
* @retval false for Failure.
*/
BOOL WINAPI Updater_Initialize(DWORD fdwReason);
#endif
#endif /* _WIN32 */

/**
 * @defgroup UPD_Handle UPD handle structure
 * @{
 */

/**
 * UPD Handle s structure
 */
typedef struct
{
    /* Configuration options */
    int              option_verbose;
    int              option_modechange;
    int              option_speed;
    int              option_delay;
    unsigned short   option_block_sz;
    char             *option_dev_name;
    bool             option_check_mode_restore;
    int              log_sys_state;
    int              option_send_nvclean;
    int              option_check_pkgv;
    int              option_update_in_modem_mode;

    /* Data */
    /* I/O buffers for download protocol */
    unsigned char    tx_buf[2/* AAxx */ + MAX_BLOCK_SZ + 1/* Data */ + 2/* Checksum */];
    unsigned char    rx_buf[0x400];
    int              com_mode;
    COM_Handle       com_hdl;
    COM_Settings     com_settings;
    struct {
        int         step;
        int         percent;
        int         totalweight;
        struct {
            int      base;
            int      weight;
        }            current;
    }                progress;
    bool             old_iso_support;
    /* Whether we have logged a particular mode or not */
    int logging_done[MODE_LOADER + 1];
    /* convenience buffer for log messages */
    char log_buffer[MAX_LOGMSG_SIZE];
    /* Logging callback */
    log_callback_type log_callback;
    FILE *log_file;
    MUTEX_TYPE  mutex_log;
    #ifdef _WIN32
    /* PNP notification */
    char devicepath[MAX_PATH];
    pnp_callback_type pnp_callback;
    HDEVNOTIFY hdev[MAX_REGISTERED];
    HWND hwnd;
	DWORD threadid;
	UINT timeout;
    #endif /* _WIN32 */

    /* UART server info */
    bool uart_srv_started;
    
    bool updating_in_modem_mode;
    bool disable_full_coredump;
    int used;
} UPD_Handle_s;

extern UPD_Handle_s handles[];

#define HDATA(index) handles[((index) - 1)]

/** @} END OF FILE */

/* .dll internal functions */
extern unsigned short ComputeChecksum16(void * p, int lg);
extern void OutError(UPD_Handle handle, const char *f, ...);
extern void OutStandard(UPD_Handle handle, const char *f, ...);
extern void OutDebug(UPD_Handle handle, const char *f, ...);
extern void SetUpdaterDebugLevel(int level);
 /**
  * @defgroup Comport The functions description for Com port
  *
  * @{
  */
/* Serial raw access functions */

/**
*
* @details  This function is used to open COM port through which data is transferred.\n
*               The handle is to be used in other functions calls and there can be multiple instances in parallel.
* @param  dev_name : Modem Comport (ex : COM10, com10, /./COM10)
* @retval   0 : Invalid Handle
* @retval   1~10 : Valid Handle
*/
ICERA_API UPD_Handle Open(const char *dev_name);

/**
* @details  This function is used to close the COM port which is opened and used.\n
*               Com port has to be closed for next operation.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @retval   0 : Fail
* @retval   1 : Success
*/
ICERA_API int Close(UPD_Handle handle);

/**
* @details  This function is used to abort the COM port which is opened and used.\n
*               This is currently used to abort MBIM operations only
* @param handle -the handle which is allocated for one comport where a device is connected.
* @retval   0 : Fail
* @retval   1 : Success
*/
ICERA_API int Abort_Handle(UPD_Handle handle);

/**
* @details  This function is used to write data through the COM port which is opened and used.\n
*               buf memory should already be allocated with the size "sz" Bytes.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param  buf : pointer to the buffer to be written
* @param  sz   : buffer size, Bytes
* @retval   0 : Fail
* @retval   1 : Success
*/
ICERA_API int Write(UPD_Handle handle, void *buf, int sz);

/**
* @details  This function is used to read data through the COM port which is opened and used.
*
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param  buf : pointer to the buffer where read data is stored
* @param  sz   : buffer size, Bytes
* @retval   0 : Fail
* @retval   1 : Success
*/
ICERA_API int Read(UPD_Handle handle, void *buf, int sz);

/**
* @brief  Set Flow Control
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param value => 0 : disabled, 1 : Hardware
* @retval 1 AT_ERR_SUCCESS
* @retval -6 AT_ERR_CTS
*/
ICERA_API int SetFlowControl(UPD_Handle handle, int value);

/** @} END OF FILE */

 /**
  * @defgroup ATcmd The functions description for AT Command
  *
  * @{
  */
/* AT functions */
/**

* @details Set timeout time waiting for the response from the board after AT command is sent.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param timeout : seconds
*/
ICERA_API void SetCommandTimeout(UPD_Handle handle, long timeout);

/* AT functions */
/**
* @details Set timeout time waiting for the response from the board after AT command is sent.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param format : the pointer which indicates the string input for AT command.
* @retval 1 AT_ERR_SUCCESS
* @retval 0 AT_ERR_FAIL
* @retval -1 AT_ERR_TIMEOUT
* @retval -2 AT_ERR_IO_WRITE
* @retval -3 AT_ERR_COM_SPEED
* @retval -4 AT_ERR_SIGNALS
* @retval -5 AT_ERR_NOTSUPPORTED
* @retval -6 AT_ERR_CTS
*/
ICERA_API int SendCmd(UPD_Handle handle, const char *format, ...);

/**
* @details Send AT command and wait AT response until it contains ok or error string for timeout time.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param ok - response with "ok" string.
* @param error - response with "error" string.
* @param format : the pointer which indicates the string input for AT command.
* @retval 1 AT_ERR_SUCCESS
* @retval 0 AT_ERR_FAIL
* @retval -1 AT_ERR_TIMEOUT
* @retval -2 AT_ERR_IO_WRITE
* @retval -3 AT_ERR_COM_SPEED
* @retval -4 AT_ERR_SIGNALS
* @retval -5 AT_ERR_NOTSUPPORTED
* @retval -6 AT_ERR_CTS
*/
ICERA_API int SendCmdAndWaitResponse(UPD_Handle handle, const char *ok, const char *error, const char *format, ...);

ICERA_API const char *GetResponse(void);

/** @} END OF FILE */

 /**
  * @defgroup updater The functions description for Updater dll
  *
  * @{
  */

/* Updater functions */

ICERA_API int VerboseLevel(int level);

ICERA_API int Updater(char* archlist, char *dev_name, int speed, int delay, unsigned short block_sz, int flash, int verbose, int modechange, int pcid);

ICERA_API void Progress(int *step, int *percent);

ICERA_API int CheckHostCompatibility(char* response_to_aticompat, char* firmware_version, char* host_version);

ICERA_API char* CheckFirmwareVersion(char* firmware_version, char* firmware_file_list);

ICERA_API void SetLogCallbackFunc(log_callback_type callback);

ICERA_API void UpdaterLogToFile(char *file);

ICERA_API void UpdaterLogToFileW(wchar_t *filename);

/**
* @details This function is used to get the version of the library.
* @param version - the pointer which indicates the string for version
* @param size - the size of the string of version
* @retval version string if version string < size
* @retval NULL if not version string < size
*/
ICERA_API char* GetLibVersion(char* version, int size);

/* Updater functions for use with handles - for multi download */
/**
* @details Find unused handle, set initial values. Start at 1 to allow a default handle.\n
*               Updater functions for use with handles - for multi download
*/
ICERA_API UPD_Handle CreateHandle(void);

/**
* @details Free unused handle, set the number of used handle as 0.\n
*               Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
*/
ICERA_API void FreeHandle(UPD_Handle handle);

/**
* @details return I/O RX buffers for download protocol.\n
*              Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @retval AT command response in rx buffer
*/
ICERA_API const char *GetResponseEx(UPD_Handle handle);

/**
* @details  Verbose Level can be set with this function for information.\n
*               Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param level=> 0: VERB_SILENT, 1 :  VERB_INFO, 2 :  VERB_ERROR, 3: VERB_DUMP
* @retval it returns "option_verbose" of the structure UPD_Handle_s
*/
ICERA_API int VerboseLevelEx(UPD_Handle handle, int level);

/**
* @details This function is the main updater function.\n
*               The arguments of this function include the options such as speed, flash, verbose etc.\n
*               Updater functions for use with handles - for multi download
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param archlist -file list to download.
* @param flash - the option for downloading or not. True : downloading, False : no downloading
* @param pcid - read public chip id. True : read pcid, False : no read pcid
* @retval 0 ErrorCode_Error
* @retval 1 ErrorCode_Sucess
* @retval 2 ErrorCode_FileNotFound
* @retval 3 ErrorCode_DeviceNotFound
* @retval 4 ErrorCode_AT
* @retval 5 ErrorCode_DownloadError
* @retval 6 ErrorCode_ConnectionError
* @retval 7 ErrorCode_InvalidHeader
*/
ICERA_API int UpdaterEx(UPD_Handle handle, char* archlist, int flash, int pcid);

/**
* @details  Show update progress step and percentage.\n
*               Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param  step -progress step
* @param  percent - progress percentage
*/
ICERA_API void ProgressEx(UPD_Handle handle, int *step, int *percent);

/**
* @details This function is for checking the compatibility between host and target.\n
*              Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param response_to_aticompat - the string retrieved from AT%%ICOMPAT
* @param firmware_version - the string retrieved from AT%%IFWR
* @param host_version - host version
* @retval  0 -Syncronized
* @retval  1 -Compatible
* @retval  -1 - Incompatible
*/
ICERA_API int CheckHostCompatibilityEx(UPD_Handle handle, char* response_to_aticompat, char* firmware_version, char* host_version);

/**
* @details Compare the FW version in the board with the FW version of the files.\n
*               Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param firmware_version - the string retrieved from AT%%IFWR
* @param firmware_file_list - the files to download
*/
ICERA_API char* CheckFirmwareVersionEx(UPD_Handle handle, char* firmware_version, char* firmware_file_list);

/**
* @details Set the callback function for logging. Select how to log with log file. If it doesn't log with a file, log is just displayed in console.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param callback- log callback
*/
ICERA_API void SetLogCallbackFuncEx(UPD_Handle handle, log_callback_type callback);

/**
* @details This function is used to store Updater log which occurs while downloading in the specific file.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param file - the pointer which indicates file to store
*/
ICERA_API void UpdaterLogToFileEx(UPD_Handle handle, char *file);

/**
* @details This function is used to store Updater log which occurs while downloading in the specific file for wide characters under Win32.\n
                Wide characters only supported under Win32 at the moment
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param file - the pointer which indicates file to store
*/
ICERA_API void UpdaterLogToFileWEx(UPD_Handle handle, wchar_t *file);

/**
* @details Set mode to be changed into in advance after update.\n
*               Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param modechange => 0 : modem mode, 1 : loader mode, 2 : IFT mode
* @retval 0 ErrorCode_Error
* @retval 1 ErrorCode_Sucess
*/
ICERA_API int  SetModeChange(UPD_Handle handle, int modechange);

/**
* @details Set speed to be changed into in advance before AT command is sent.\n
*          Will be used for host COM port if succesfully changed
*          on the target through AT+IPR.
*               Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param speed - baud rate.
* @retval 0 ErrorCode_Error
* @retval 1 ErrorCode_Sucess
*/
ICERA_API int SetSpeed(UPD_Handle handle, int speed);

/**
 * @details Set baud rate for COM handle.
 *         Useful when communicating on physical UART port.
 *         UpdaterLib default is 115200.
 *         No impact when used on logical UART port (physical
 *         USB...)
 *
 * @param handle -the handle which is allocated for one comport where a device is connected.
 * @param speed - baud rate.
 *
 * @return int ErrorCode_Success or ErrorCode_Error
 */
ICERA_API int SetBaudrate(UPD_Handle handle, int speed);

/**
* @details Set delay time which we want in advance before it is used.\n
*             Delay time can be used when waiting until com port is opened.\n
*             pls refer to the function "SelectMode()" for example.\n
*             Updater functions for use with handles - for multi download
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param delay - seconds
* @retval 0 ErrorCode_Error
* @retval 1 ErrorCode_Sucess
*/
ICERA_API int SetDelay(UPD_Handle handle, int delay);

/**
* @details Set the range of block size which we want into in advance before it is used.\n
*               Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param block_sz Bytes
* @retval 0 ErrorCode_Error
* @retval 1 ErrorCode_Sucess
*/
ICERA_API int SetBlockSize(UPD_Handle handle, int block_sz);

/**
* @details Change into the mode we want to go.\n
*               Updater functions for use with handles - for multi download.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param mode => 0 : modem mode, 1 : loader mode, 2 : IFT mode
* @retval 0 ErrorCode_Error
* @retval 1 ErrorCode_Sucess
* @retval 2 ErrorCode_FileNotFound
* @retval 3 ErrorCode_DeviceNotFound
* @retval 4 ErrorCode_AT
* @retval 5 ErrorCode_DownloadError
* @retval 6 ErrorCode_ConnectionError
* @retval 7 ErrorCode_InvalidHeader
*/

ICERA_API int SwitchMode(UPD_Handle handle, unsigned int mode);

/**
* @details Enable or disable sending of AT%NVCLEAN when upgrading a modem application\n
*               Updater functions for use with handles - for multi download.
*               This is enabled by default.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param enable - 1 to enable, 0 to disable
*/
ICERA_API void SetSendNvclean(UPD_Handle handle, int enable);

/**
* @details Enable or disable checking AT%IPKGV when upgrading files\n
*               Updater functions for use with handles - for multi download.
*               This is enabled by default.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param enable - 1 to enable, 0 to disable
*/
ICERA_API void SetCheckPkgv(UPD_Handle handle, int enable);

/**
* @details Enable or disable updating in modem mode
*               Updater functions for use with handles - for multi download.
*               This is disabled by default, unless building with MBIM support enabled.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param enable - 1 to enable, 0 to disable
*/
ICERA_API void SetUpdateInModemMode(UPD_Handle handle, int enable);

/**
* @details Global setting for performing Disabling full coredump after update.\n
* @param disable_full_coredump_required => 0 : do nothing, 1 : Disable Full coredump
*/
ICERA_API void SetGlobalDisableFullCoredumpSetting(bool disable_full_coredump_required);

/**
* @details Per Handle setting for performing Disabling full coredump after update.\n
* @param disable_full_coredump_required => 0 : do nothing, 1 : Disable Full coredump
*/
ICERA_API void SetDisableFullCoredumpSetting(UPD_Handle handle, bool disable_full_coredump_required);

/**
* @details Global setting for checking if Mode Restore successfully after update.\n
* @param   check => 0 : don't check; 1 : check if mode restore successfully
*/
ICERA_API void SetGlobalCheckModeRestore(bool check);

/**
* @details Per Handle setting for checking if Mode Restore successfully after update.\n
* @param   check => 0 : don't check; 1 : check if mode restore successfully
*/
ICERA_API void SetCheckModeRestore(UPD_Handle handle, bool check);

#ifdef _WIN32
/**
* @details This function is used to get the path of the device which is connected to comport in registry.
* @param comport -the pointer which indicates comport.
* @param buffer -the pointer which indicates the string for the path
* @param bufferlength -the pointer which indicates the size of the buffer
* @retval False fail
* @retval True  success
*/
ICERA_API int GetDevicePathFromComPort(char* comport, char* buffer, unsigned short* bufferlength);

/**
* @details This function is used to unregister plug&play event which is registered.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param register_handle -the registered handle for plug&play event
* @retval -1 bad parameter
* @retval -2 already unregistered
*/
ICERA_API int UnregisterPnpEventByInterface(UPD_Handle handle, int register_handle);

/**
* @details This function is used to register plug&play event.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param guid -the pointer which indicates comport guid
* @retval -1 startPnpEvent function should be called before
* @retval -2 out of range for maximum registered GUID
* @retval -3 register device notification error
*/
ICERA_API int RegisterPnpEventByInterface(UPD_Handle handle, GUID* guid);

/**
* @details This function is used to stop plug&play event.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @retval False fail
* @retval True  success
*/
ICERA_API int StopPnpEvent(UPD_Handle handle);

/**
* @details This function is used to start plug&play event.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param pnp_callback -plug&play callback function. (See pnp_callback_type for more details)
* @param hwnd -the pointer which indicates handle window object.
* @retval False fail
* @retval True  success
*/
ICERA_API int StartPnpEvent(UPD_Handle handle, pnp_callback_type pnp_callback, HWND* hwnd/* optional, could be NULL */);

/**
* @details This function is used to wait plug&play event until window notifies with the message of the connection.
* @param handle -the handle which is allocated for one comport where a device is connected.
* @param threadid -thread ID.
* @retval False fail
* @retval True  success
*/
ICERA_API int WaitPnpEvent(UPD_Handle handle, unsigned int timeout_in_ms/* 0: infinite */, unsigned long* threadid/* optional, could be NULL */);

/**
* @details This function call once or more the given callback when the given servicename matches with service found in:
*          [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\"servicename"\Enum] registry key.
* @param servicename -Filter applied on service names. (Wildcard "*" could be used)
* @param callback -Callback called when servicename is found. (See enums_callback_type for more details)
* @param ctx -Context pointer given by the caller and directly given to the callback. (Optional)
* @retval > 0  success
* @retval <= 0 on error
*/
ICERA_API int WalkOnEnums(char* servicename, enums_callback_type callback, void* ctx/* optional, could be NULL */);

/**
* @details This function call once or more the given callback when the given servicename matches with service found in:
*          [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\"servicename"] registry key.
* @param servicename -Filter applied on service names. (Wildcard "*" could be used)
* @param callback -Callback called when servicename is found. (See services_callback_type for more details)
* @param ctx -Context pointer given by the caller and directly given to the callback. (Optional)
* @retval > 0  success
* @retval <= 0 on error
*/
ICERA_API int WalkOnServices(char* servicename, services_callback_type callback, void* ctx/* optional, could be NULL */);

/**
* @details This function call the given callback for each item found in following registry key if it exists:
*          [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{????????-????-????-????-????????????}]
*          or
*          [HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\DeviceClasses\{????????-????-????-????-????????????}]
*          where {????????-????-????-????-????????????} is the given guid.
* @param guid -Class GUID.
* @param callback -Callback called when servicename is found. (See discovery_callback_type for more details)
* @param ctx -Context pointer given by the caller and directly given to the callback. (Optional)
* @retval > 0  success
* @retval <= 0 on error
*/
ICERA_API int WalkOnDevices(GUID* guid, discovery_callback_type callback, void* ctx/* optional, could be NULL */);
#endif /* _WIN32 */

/* UART server functions */

/**
* @details This function is used to make local PC stopped as UART server
* @param handle -the handle which is allocated for one comport where a device is connected.
*/
ICERA_API void UartStopServer(UPD_Handle handle);

/**
* @details This function is used to make a local PC server for the boot from UART feature.
* @param server_handle -the information for one comport where a device is connected
* @param arch_name - the pointer which indicates the files(loader, BT2) for boot from UART
* @param dev_name -the poniter which indicates the comport
* @param verbose => 0: VERB_SILENT, 1 :  VERB_INFO, 2 :  VERB_ERROR, 3: VERB_DUMP, -1 : VERBOSE_GET_DATA
* @retval 0 ErrorCode_Error
* @retval 1 ErrorCode_Sucess
* @retval 2 ErrorCode_FileNotFound
* @retval 3 ErrorCode_DeviceNotFound
* @retval 4 ErrorCode_AT
* @retval 5 ErrorCode_DownloadError
* @retval 6 ErrorCode_ConnectionError
* @retval 7 ErrorCode_InvalidHeader
*/
ICERA_API int UartFileServer(UPD_Handle server_handle, char* arch_name, char *dev_name, int verbose);
ICERA_API int UartDiscoveryServer(UPD_Handle server_handle, char *dev_name, int verbose);

ICERA_API int SetDevice(UPD_Handle handle, const char *dev_name);
ICERA_API void SetLogSysState(UPD_Handle handle, int state);
ICERA_API int SelectMode(UPD_Handle handle, int mode);

/**
 * Remove from given list of file(s) the one(s) already programmed in modem file system.
 * File per file, compares SHA1.
 * SHA1 of file in modem file system retreived thanks to AT%IGETARCHDIGEST
 *
 * @param handle return value of successful call to Open
 * @param filelist list of file(s) to update separated with ";"
 */
ICERA_API void CheckAlreadyProgrammedFiles(UPD_Handle handle, char *filelist);

// TODO to be removed ---------------------------------------
extern const int ICERA_API RSASigByteLen;
extern const int ICERA_API NonceByteLen;
extern const int ICERA_API SigTypeRsa;
extern const int ICERA_API KeyIdByteLen;
extern const int ICERA_API PcidByteLen;
extern const int ICERA_API ArchHdrByteLen;
extern const int ICERA_API ImeiLen;
extern const int ICERA_API MaxHeaderLen;
extern const int ICERA_API ZipArchVer;
ICERA_API void DisplayAvailableArchTypes(void);
ICERA_API int GetMagic(int chip_value, unsigned int *magic);
ICERA_API int GetBuildFileProperties(char *name, int *arch_id, int *is_appli);
ICERA_API unsigned int GetBt2TrailerMagic(void);
ICERA_API void DisplayAvailableArchTypesEx(UPD_Handle handle);
// ----------------------------------------------------------

#endif
/** @} END OF FILE */
