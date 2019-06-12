/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup fild
 * @{
 */

/**
 * @file fild_forward.c FILD port forward functionalities.
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "fild.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/
#define BLOCK_DATA_SIZE 512    /** Max data read at a time (bytes)... Max observed AT cmd is 347 bytes long. */

#define MDM_NOT_READY_ERR_MSG   "ERROR: MDM NOT READY\n\r"
#define MDM_GONE_ERR_MSG        "ERROR: MDM GONE\r\n"
#define MDM_WRITE_ERR_MSG       "ERROR: WRITE\n\r"
#define MDM_CRASH_ERR_MSG       "ERROR: CRASH\n\r"
#define WAIT                    "WAIT\r\n"
#define READY                   "READY\r\n"

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/
typedef struct
{
    char *src_name; /** UART Port to be forwarded to MDM */
    char *dst_name; /** Port to communicate with MDM */

    int baudrate;   /** UART baudrate */
    int fctrl;      /** UART flow control */

    int src_fd;
    int dst_fd;
} ForwardArgs;

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/
static void *ReadUartThread(void *arg)
{
    ForwardArgs *fwd_data = (ForwardArgs *)arg;

    /* Open/configure UART that will be kept opened... */
    fwd_data->src_fd = fild_Open(fwd_data->src_name, O_RDWR, 0);
    if (fwd_data->src_fd == -1)
    {
        ALOGE("%s: fail to open UART channel. %s", __FUNCTION__, fild_LastError());
        return NULL;
    }
    if (fild_ConfigUart(fwd_data->src_fd, fwd_data->baudrate, fwd_data->fctrl) == -1)
    {
        ALOGE("%s: fail to configure %s. %s", __FUNCTION__, fwd_data->src_name, fild_LastError());
        fild_Close(fwd_data->src_fd);
        fwd_data->src_fd = -1;
        return NULL;
    }

    ALOGI("%s: ready", __FUNCTION__);

    while(1)
    {
        /* Poll on UART indefinitely... */
        char buf[BLOCK_DATA_SIZE]; /* UART read buffer */
        int bytes_read = fild_Read(fwd_data->src_fd, buf, sizeof(buf));
        if (bytes_read > 0)
        {
            if (fwd_data->dst_fd < 1)
            {
                /* Modem TTY port not open */
                fild_WriteOnChannel(fwd_data->src_fd, MDM_NOT_READY_ERR_MSG, strlen(MDM_NOT_READY_ERR_MSG));
            }
            else if (fild_WriteOnChannel(fwd_data->dst_fd, buf, bytes_read) != bytes_read)
            {
                /* Modem TTY port write failed */
                fild_WriteOnChannel(fwd_data->src_fd, MDM_WRITE_ERR_MSG, strlen(MDM_WRITE_ERR_MSG));
            }
        }
    };
    return NULL;
}

/**********************************/
/* MDM -> UART                    */
/**********************************/
static void *ReadMdmThread(void *arg)
{
    ForwardArgs *fwd_data = (ForwardArgs *)arg;

    while (1) {
        /* Open modem TTY port */
        while (fwd_data->dst_fd < 0)
        {
            int fd = fild_Open(fwd_data->dst_name, O_RDWR, 0);
            if (fd < 0)
            {
                fild_WriteOnChannel(fwd_data->src_fd, WAIT, strlen(WAIT));
                sleep(1);
            }
            else
            {
                fwd_data->dst_fd = fd;
            }
        }
        ALOGI("%s: modem TTY port open", __FUNCTION__);
        fild_WriteOnChannel(fwd_data->src_fd, READY, strlen(READY));
        while (fwd_data->dst_fd >= 0)
        {
            char buf[BLOCK_DATA_SIZE];  /* MDM read buffer */
            int bytes_read = fild_Read(fwd_data->dst_fd, buf, sizeof(buf));
            if (bytes_read > 0)
            {
                fild_WriteOnChannel(fwd_data->src_fd, buf, bytes_read);
            }
            else
            {
                fild_WriteOnChannel(fwd_data->src_fd, MDM_GONE_ERR_MSG, strlen(MDM_GONE_ERR_MSG));
                fild_Close(fwd_data->dst_fd);
                ALOGI("%s: %m reading modem TTY port", __FUNCTION__);
                fwd_data->dst_fd = -1;
            }
        }
    }
    return NULL;
}

/**********************************/
/* Args parsing                   */
/**********************************/
/**
 * Get len of string from a given pointer until next space char is found
 */
static int LenBeforeSpace(char *ptr)
{
    int len = 0, s;
    char *tmp = ptr;
    while(*tmp)
    {
        s = isspace(*tmp);
        if (s)
        {
            /* space found... */
            break;
        }
        else
        {
            len++;
        }
        tmp++;
    }
    return len;
}

/**
 *
 */
static ForwardArgs *parseForwardArgs(void *args)
{
    char *cmd_line = (char *)args;
    char *tmp1, *tmp2, *tmp;
    ForwardArgs *fwd_data = NULL;
    int ret, s, len1 = 0, len2 = 0;

    /*  */
    tmp1 = strstr(cmd_line, "uart:"); /* pointer right before "uart:" in cmd line */
    tmp2 = strstr(cmd_line, "mdm:"); /* pointer right before "mdm:" tag */
    if (tmp1 && tmp2)
    {
        tmp1 += strlen("uart:");
        tmp2 += strlen("mdm:");

        /* get len of string right after "uart:" and right after "mdm:" */
        len1 = LenBeforeSpace(tmp1);
        len2 = LenBeforeSpace(tmp2);
        if ((len1 >0) && (len2 > 0))
        {
            fwd_data = malloc(sizeof(ForwardArgs));
            if (fwd_data)
            {
                fwd_data->src_name = malloc(len1+1);
                fwd_data->dst_name = malloc(len2+1);
                fwd_data->src_fd = -1;
                fwd_data->dst_fd = -1;
                if (fwd_data->src_name && fwd_data->dst_name)
                {
                    memcpy(fwd_data->src_name, tmp1, len1);
                    fwd_data->src_name[len1]='\0';
                    memcpy(fwd_data->dst_name, tmp2, len2);
                    fwd_data->dst_name[len2]='\0';
                }
                else
                {
                    ALOGE("%s: fwd data malloc failure", __FUNCTION__);
                    free(fwd_data->src_name);
                    free(fwd_data->dst_name);
                    free(fwd_data);
                    fwd_data = NULL;
                }
            }
            else
            {
                ALOGE("%s: malloc failure", __FUNCTION__);
            }
        }
        else
        {
            ALOGE("%s: incomplete args.", __FUNCTION__);
        }
    }
    else
    {
        ALOGE("%s: invalid args", __FUNCTION__);
    }

    return fwd_data;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
void fild_UartForwardInit(void *arg)
{
    do
    {
        /** Parse fwd args, expected format:
            * "uart:<uart_device_name> mdm:<mdm_device_name>"
            */
        ForwardArgs *fwd_data = parseForwardArgs(arg);
        if (fwd_data == NULL)
        {
            break;
        }
        else
        {
            ALOGI("%s: forward initialised between %s and %s",
                __FUNCTION__,
                fwd_data->src_name,
                fwd_data->dst_name
            );

            /* Some hard coded UART settings... */
            fwd_data->baudrate = 115200;
            fwd_data->fctrl = 0;
        }

        /* Init UART -> MDM forward */
        fildThread read_uart_thread;
        if(fild_ThreadCreate(&read_uart_thread, ReadUartThread, fwd_data) != 0)
        {
            ALOGE("%s: start read uart thread failure.", __FUNCTION__);
            break;
        }

        /* Init MDM -> UART forward */
        fildThread read_mdm_thread;
        if(fild_ThreadCreate(&read_mdm_thread, ReadMdmThread, fwd_data) != 0)
        {
            ALOGE("%s: start read modem thread failure.", __FUNCTION__);
            break;
        }
    }while(0);
}
