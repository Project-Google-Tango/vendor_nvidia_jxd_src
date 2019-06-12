/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup downloader
 * @{
 */

/**
 * @file at_cmd.c AT command utilities.
 *
 * This file implement basic functions to send AT
 * commands on host interface.
 */

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "com_api.h"
#include "com_handle.h"

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/
#define AT_MAX_READ_ERRORS      5000    /* consecutive com_Read errors */

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/


/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
COM_Handle_s com_handle_array[COM_MAX_HANDLES];

#if (__STDC_VERSION__ >= 199901L)
static COM_operations com_operations =
{
    .close = com_Close,
    .read = com_Read,
    .write = com_Write,
    .poll = com_Poll,
    .update_speed = com_UpdateSpeed,
    .clear_dtr = com_ClearDTR,
    .set_cts = com_SetCTS,
    .abort = NULL,
};

#ifdef _WIN32
#ifndef _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE
static COM_operations sock_operations =
{
    .close = sock_Close,
    .read = sock_Read,
    .write = sock_Write,
    .update_speed = NULL,
    .clear_dtr = NULL,
    .set_cts = NULL,
    .abort = NULL,
};
#endif /* _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE */
#endif
#else /* __STDC_VERSION__ */
static COM_operations com_operations =
{
    com_Close,
    com_Write,
    com_Read,
    com_Poll,
    com_UpdateSpeed,
    com_ClearDTR,
    com_SetCTS,
    NULL
};

#ifdef _WIN32
#ifndef _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE
static COM_operations sock_operations =
{
    sock_Close,
    sock_Write,
    sock_Read,
    NULL,
    NULL,
    NULL,
    NULL
};
#endif /* _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE */

#ifdef ENABLE_MBIM_DSS
static COM_operations device_service_operations = 
{
	device_service_Close,
	device_service_Write,
	device_service_Read,
	NULL,
	NULL,
	NULL,
	device_service_Abort
};
#endif /* ENABLE_MBIM_DSS */
#endif /* _WIN32 */
#endif /* __STDC_VERSION__ */

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

void at_SetDefaultCommandTimeout(COM_Handle handle)
{
    HDATA(handle).at_cmd_timeout = AT_DEFAULT_TIMEOUT;
}

void at_SetCommandTimeout(COM_Handle handle, long timeout)
{
    HDATA(handle).at_cmd_timeout = timeout;
}

int at_SendCmdAndWaitResponseAP(COM_Handle handle, const char *ok, const char *error, const char *format, va_list ap)
{
    char *tx_str = HDATA(handle).tx_buf;
    int index, nbread, error_counter = 0;
    time_t time_start, time_current;

    /* Send AT command */
    if (format != NULL)
    {
        vsnprintf(tx_str, sizeof(HDATA(handle).tx_buf), format, ap);
        if (HDATA(handle).at_OutputCB)
        {
            HDATA(handle).at_OutputCB(HDATA(handle).at_OutputCBArg, "-> %s", tx_str);
        }
        if (COM_write(handle, tx_str, (int)strlen(tx_str)) == -1)
        {
            PRINT(handle, (stderr, "\nfailed to write [%s], errno=%s\n", tx_str, strerror(errno)));
            return AT_ERR_IO_WRITE;
        }
    }

    /* Wait AT response until it contains ok or error string */
    if ((ok != NULL) && (error != NULL))
    {
        time(&time_start);
        /* Get response. */
        index = 0;
        memset(HDATA(handle).rx_buf, 0 , sizeof(HDATA(handle).rx_buf));
        do
        {
            /* Read /r/n */
            nbread = COM_read(handle, &HDATA(handle).rx_buf[index], sizeof(HDATA(handle).rx_buf) - index);
            if (nbread > 0)
            {
                error_counter = 0;

                HDATA(handle).rx_buf[index + nbread] = '\0';

                if (HDATA(handle).at_OutputCB)
                {
                    if (!index)
                        HDATA(handle).at_OutputCB(HDATA(handle).at_OutputCBArg, "<- ");

                    HDATA(handle).at_OutputCB(HDATA(handle).at_OutputCBArg, "%s", &HDATA(handle).rx_buf[index]);
                }

                index += nbread;
            }
            else if (nbread != 0)
            {
                error_counter++;
                if (error_counter > AT_MAX_READ_ERRORS)
                {
                    /* Unlikely it recovers, don't want to be stuck */
                    return AT_ERR_FAIL;
                }
            }

            /* timeout */
            time(&time_current);
            if (time_current - time_start > HDATA(handle).at_cmd_timeout)
            {
                HDATA(handle).at_OutputCB(HDATA(handle).at_OutputCBArg, "<- %s\n", HDATA(handle).rx_buf);
                PRINT(handle, (stderr, "#ERROR: timeout\n\n"));
                return AT_ERR_TIMEOUT;
            }
        }
        while ((strstr(HDATA(handle).rx_buf, ok) == NULL) && (strstr(HDATA(handle).rx_buf, error) == NULL));
        return (strstr(HDATA(handle).rx_buf, ok) != NULL) ? AT_ERR_SUCCESS : AT_ERR_FAIL;
    }
    return AT_ERR_SUCCESS;
}

int at_SendCmdAP(COM_Handle handle, const char *format, va_list ap)
{
    char *tx_str = HDATA(handle).tx_buf;
    int index, nbread, error_counter = 0;
    time_t time_start, time_current;

    vsnprintf(tx_str, sizeof(HDATA(handle).tx_buf), format, ap);

    if (HDATA(handle).at_OutputCB)
    {
        HDATA(handle).at_OutputCB(HDATA(handle).at_OutputCBArg, "-> %s", tx_str);
    }

    time(&time_start);

    if (COM_write(handle, tx_str, (int)strlen(tx_str)) == -1)
    {
        PRINT(handle, (stderr, "\nfailed to write [%s], errno=%s\n", tx_str, strerror(errno)));
        return AT_ERR_IO_WRITE;
    }

    /* Get response. ended on "\r\nOK\r\n" or "\r\nERROR\r\n" or timeout */
    index = 0;
    memset(HDATA(handle).rx_buf, 0 , sizeof(HDATA(handle).rx_buf));
    do
    {
        /* Read /r/n */
        nbread =COM_read(handle, &HDATA(handle).rx_buf[index], sizeof(HDATA(handle).rx_buf) - index);
        if (nbread > 0)
        {
            error_counter = 0;

            HDATA(handle).rx_buf[index + nbread] = '\0';

            if (HDATA(handle).at_OutputCB)
            {
                if (!index)
                    HDATA(handle).at_OutputCB(HDATA(handle).at_OutputCBArg, "<- ");

                HDATA(handle).at_OutputCB(HDATA(handle).at_OutputCBArg, "%s", &HDATA(handle).rx_buf[index]);
            }

            index += nbread;
        }
        else if (nbread != 0)
        {
            error_counter++;
            if (error_counter > AT_MAX_READ_ERRORS)
            {
                /* Unlikely it recovers, don't want to be stuck */
                return AT_ERR_FAIL;
            }
        }

        /* timeout */
        time(&time_current);
        if (time_current - time_start > HDATA(handle).at_cmd_timeout)
        {
            if (HDATA(handle).at_OutputCB)
            {
                HDATA(handle).at_OutputCB(HDATA(handle).at_OutputCBArg, "<- %s\n", HDATA(handle).rx_buf);
            }
            PRINT(handle, (stderr, "#ERROR: timeout\n\n"));
            return AT_ERR_TIMEOUT;
        }
    }
    while ((strstr(HDATA(handle).rx_buf, AT_RSP_OK) == NULL) && (strstr(HDATA(handle).rx_buf, AT_RSP_ERROR) == NULL));

    return (strstr(HDATA(handle).rx_buf, AT_RSP_OK) != NULL) ? AT_ERR_SUCCESS : AT_ERR_FAIL;
}

int at_SendCmdAndWaitResponse(COM_Handle handle, const char *ok, const char *error, const char *format, ...)
{
    int     result;
    va_list ap;

    va_start(ap, format);
    result = at_SendCmdAndWaitResponseAP(handle, ok, error, format, ap);
    va_end(ap);
    return result;
}

int at_SendCmd(COM_Handle handle, const char *format, ...)
{
    int     result;
    va_list ap;

    va_start(ap, format);
    result = at_SendCmdAP(handle, format, ap);
    va_end(ap);
    return result;
}

const char *at_GetResponse(COM_Handle handle)
{
    return (const char *)HDATA(handle).rx_buf;
}

void at_SetOutputCB(COM_Handle handle, void (*cb)(unsigned long arg, const char *format, ...), unsigned long cb_arg)
{
    HDATA(handle).at_OutputCB = cb;
    HDATA(handle).at_OutputCBArg = cb_arg;
}


COM_Handle COM_CreateHandle(void)
{
    COM_Handle i;

    for (i = 0; i < COM_MAX_HANDLES; i++)
    {
        if (!com_handle_array[i].used)
        {
            com_handle_array[i].used = 1;
            com_handle_array[i].verbose_level = VERBOSE_DEFAULT_LEVEL;
            com_handle_array[i].at_cmd_timeout = AT_DEFAULT_TIMEOUT;
            com_handle_array[i].com_hdl = INVALID_HANDLE;
            return i + 1;
        }
    }

    return 0;
}

void COM_FreeHandle(COM_Handle handle)
{
    HDATA(handle).used = 0;
}

int COM_VerboseLevel(COM_Handle handle, int level)
{
    if (level > VERBOSE_GET_DATA)
    {
        HDATA(handle).verbose_level = level;
    }

    return HDATA(handle).verbose_level;
}

COM_Handle COM_open(const char *dev_name)
{
    COM_Handle hdl = 0;
    char *dev=NULL;

#ifdef _WIN32
    /* Look for 1st field in dev_name, taking ":" as delimiter 
     If socket open required, then dev name should be formatted as follow:
     tcp:<port_num>:<ip_addr>

     2nd field of deb_name always considered as a port number.
     3rd field of dev_name always considered as an IP addr.
     If no <ip_addr>, 127.0.0.1 taken as default
     If no <port_num>, 32768 taken as default

     Ex:
     if dev_name = "tcp:" : socket opened on 127.0.0.1:32768
     if dev_name = "tcp:3456" : socket opened on 127.0.0.1:3456
     if dev_name = "tcp:3456:192.168.48.2" : socket opened on 192.168.48.2:3456     
     */
    char *sock;
    dev = strdup(dev_name);
    sock = strtok(dev, ":");
    if(memcmp(sock, "tcp", strlen("tcp")) == 0)
    {
	#ifndef _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE
        char *addr = NULL;
        int port = 0;

        sock = strtok(NULL, ":");
        if(sock)
        {
            /* 2nd sock field is port */
            port = atoi(sock);                        
        }
        port = port ? port:32768; /* default to 32768 */
        port = port > 65535 ? 32768:port; /* max port number is 65535 */

        sock = strtok(NULL, ":");
        if(sock)
        {
            /* 3rd sock field is addr */
            addr = (char *)malloc(strlen(sock));
            strcpy(addr, sock);
        }

        hdl = addr ? sock_Open(addr, port):sock_Open("127.0.0.1", port);
        if (hdl > 0)   
        {
            HDATA(hdl).ops = &sock_operations;
            HDATA(hdl).mode = COM_MODE_SOCKET;
        }

        if(addr)
        {
            free(addr);
        }
	#endif /* _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE */
    }
#ifdef ENABLE_MBIM_DSS
	else if (strstr(strupr(dev), "MBIM"))
	{
        char *interfaceId = strtok(NULL, ":");
		
		if (interfaceId && strstr(interfaceId, "{") && strstr(interfaceId, "}"))
		{
			hdl = device_service_Open(interfaceId);
		}
		else
		{
			hdl = device_service_Open(NULL);
		}

		if (hdl > 0)   
        {
            HDATA(hdl).ops = &device_service_operations;
			HDATA(hdl).mode = COM_MODE_DEVICE_SERVICE;
        }
	}
#endif
    else
#endif
    {
        if ((hdl = com_Open(dev_name)) > 0)      
        {
            HDATA(hdl).ops = &com_operations;
            HDATA(hdl).mode = COM_MODE_SERIAL;
        }
    }

    if(dev)
    {
        free(dev);
    }

    return hdl;
}

int COM_close(COM_Handle handle)
{
    return HDATA(handle).ops->close(handle);
}

int COM_write(COM_Handle handle, void *buf, int sz)
{
    return HDATA(handle).ops->write(handle, buf, sz);
}

int COM_read(COM_Handle handle, void *buf, int sz)
{
    return HDATA(handle).ops->read(handle, buf, sz);
}

int COM_poll(COM_Handle handle, void *buf, int sz, int timeout)
{
    return HDATA(handle).ops->poll(handle, buf, sz, timeout);
}


int COM_updateSpeed(COM_Handle handle, int speed)
{
    if(HDATA(handle).ops->update_speed)
    {
        return HDATA(handle).ops->update_speed(handle,speed); 
    }
    else
    {
        /* Not supported but successfull... */
        return 1;
    }
}

int COM_clearDtr(COM_Handle handle)
{
    if(HDATA(handle).ops->clear_dtr)
    {
        return HDATA(handle).ops->clear_dtr(handle);
    }
    else
    {
        /* Not supported but successfull... */
        return 1;
    }

}

int COM_setCts(COM_Handle handle, int value)
{
    if(HDATA(handle).ops->set_cts)
    {
        return HDATA(handle).ops->set_cts(handle, value);
    }
    else
    {
        /* Not supported but successfull... */
        return 1;
    }
}

int COM_mode(COM_Handle handle)
{
    return HDATA(handle).mode;
}

int COM_abort(COM_Handle handle)
{
    int result = 0;

    if (HDATA(handle).used)
    {
        if (HDATA(handle).ops->abort)
        {
            result = HDATA(handle).ops->abort(handle);
        }
    }
    
    return result;
}
/** @} END OF FILE */
