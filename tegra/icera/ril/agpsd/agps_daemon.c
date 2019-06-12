/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup agps
 * @{
 */

/**
 * @file agps_daemon.c NVidia aGPS RIL Daemon
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "agps_port.h"
#include "agps_threads.h"


/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/

#define AGPS_PROPERTY_PREFIX "agpsd."

#define AGPS_EXIT_SLEEP_TIME_SECONDS 5

/**
 * Dumps a kernel timereference into the current log
 */
#define LogKernelTimeStamp(A) {\
                struct timespec time_stamp;\
                clock_gettime(CLOCK_MONOTONIC, &time_stamp);\
                ALOGD("Kernel time: [%d.%6d]",(int)time_stamp.tv_sec,(int)time_stamp.tv_nsec/1000);\
                }


/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/


/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/


/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/


/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
int main(int argc, char *argv[])
{
    int ret, len, showhelp = 0;
    void * thread_res;
    agps_PortThread dl_thread, ul_thread, test_thread;
    agps_DeviceInfo agps_device_info ;
    agps_ReturnCode agps_ret_code;

    /* Default params values: */
    char *device_path = NULL;
    char *socket_path = NULL;
    int test_mode = 0;

    /**
     * Options
     */
    agps_PortOption agps_daemon_options[] =
    {
        { "help",     'h', AGPS_OPTION_FLAG,   &showhelp,      "Display this help"},
        { "dev_path", 'd', AGPS_OPTION_STRING, &device_path,   "modem device tty path"},
        { "socket_path", 'd', AGPS_OPTION_STRING, &socket_path,   "AGPS socket path"},
        { "test_mode",'t', AGPS_OPTION_INT, &test_mode,     "test mode = 0 :normal / 1: run the test thread"},
        /* terminator */
        { 0, 0, 0, NULL, NULL},
    };

    /* parse options */
    agps_ret_code = agps_PortParseArgs(agps_daemon_options, AGPS_PROPERTY_PREFIX, argc, argv);

    if (agps_ret_code != AGPS_OK)
    {
        ALOGE("%s: Error %d when parsing args\n", __FUNCTION__, agps_ret_code );
        showhelp=1;
    }

    /* the following do while(0) is used to avoid nested if/return statements */
    do
    {
        /* show help menu if required */
        if (showhelp)
        {
            agps_PortUsage(agps_daemon_options);
            ret = -1;
            break;
        }

        /* sanity checks */
        if (NULL == device_path)
        {
            ALOGE("%s: device_path is NULL \n", __FUNCTION__);
            ret = -1;
            break;
        }

        /* sanity checks */
        if (NULL == socket_path)
        {
            ALOGE("%s: socket_path is NULL \n", __FUNCTION__);
            ret = -1;
            break;
        }

        if (agps_CheckSocketPath(socket_path) < 0)
        {
            ALOGE("%s:Incorrect socket path\n", __FUNCTION__);
            ret = -1;
            break;
        }

        LogKernelTimeStamp();
        ALOGD("%s Version : 1.12\n", __FILE__);
        ALOGD("%s Built %s %s\n", __FILE__, __DATE__, __TIME__ );

        if (test_mode == 1)
        {
              /* start test thread */
            if (agps_PortCreateThread(&test_thread, agps_testThreadAndroid, (void*) socket_path) != 0)
            {
                ALOGE("%s: Failed to start agps_testThreadAndroid.\n", __FUNCTION__);
                ret = -1;
                break;
            }
        }

        agps_device_info.device_path = device_path;
        agps_device_info.socket_path = socket_path;

        /* start uplink thread */
        if (agps_PortCreateThread(&ul_thread, agps_uplinkThread, (void*)&agps_device_info) != 0)
        {
            ALOGE("%s: Failed to start agps_uplinkThread.\n", __FUNCTION__);
            ret = -1;
            break;
        }

        /* start downlink thread */
        if (agps_PortCreateThread(&dl_thread, agps_downlinkThread, (void*)&agps_device_info) != 0)
        {
            ALOGE("%s: Failed to start agps_downlinkThread.\n", __FUNCTION__);
            ret = -1;
            break;
        }

        /* wait for uplink thread resource release and termination,
	       no need to wait for the other thread termination*/
        ret = pthread_join(ul_thread, &thread_res);
        if (ret == 0)
        {
            ALOGI("%s: UL thread has terminated\n", __FUNCTION__);
        }
        else
        {
            ALOGE("%s: Failed to wait for ul thread termination , error =%d\n", __FUNCTION__, ret);
            ret = -1;
        }

    } while (0);

    ALOGI("%s: Cleanup \n", __FUNCTION__ );
    /* clean up */
    agps_PortCleanupArgs(agps_daemon_options);

    /* sleep for a bit in order to avoid spinning here if the aGPS service is
       started in a loop */
    sleep(AGPS_EXIT_SLEEP_TIME_SECONDS);
    ALOGI("%s: Terminating AGPSD\n", __FUNCTION__ );

    return ret;
}
