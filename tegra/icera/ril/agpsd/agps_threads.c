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
 * @file agps_threads.c NVidia aGPS Downlink and Uplink threads
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "agps_threads.h"
#include "agps_port.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/un.h>
#include <errno.h>
#include <stddef.h>
#include <cutils/sockets.h>
#include <fcntl.h>
#include <termios.h>


/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/
#define AGPS_TRANSPORT_OPEN_MIN_DELAY   1
#define AGPS_TRANSPORT_OPEN_MAX_DELAY   16
#define AGPS_TTY_MAX_READ_SIZE          4096
#define AGPS_SOCKET_MAX_READ_SIZE       4096
#define AGPS_MODEM_ERROR_MSG "ERROR\r"
#define AGPS_MODEM_ERROR_MSG_LEN sizeof(AGPS_MODEM_ERROR_MSG)
/* Max number of attempts to write to the tty or to the socket */
#define AGPS_TTY_MAX_WRITE     (AGPS_SOCKET_MAX_READ_SIZE * 2)
#define AGPS_SOCKET_MAX_WRITE  (AGPS_SOCKET_MAX_READ_SIZE * 2)
#define AGPS_FAILURE 0
#define AGPS_OK 1
#define AGPS_WAIT_FOR_MODEM_ATTEMPT 10

/* For test purpose only, do not activate this flag for normal use*/
// #define DUMMY_CLOCK_CALIBRATION

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/
static int open_tty_port(const char * device_path);
static void close_tty_port(void);
static void close_socket_port(void);
static int send_tty_data(const char * buff, int len);
static int send_socket_data(const char * buff, int len);
static int read_socket_data(char *buff, int len);
static int read_tty_data(char *buff, int len, char ** error_str);
static int tty_set_nonblocking_read(void);
static int tty_set_blocking_read(void);
static int wait_for_modem_ready(void);


/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/


/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

static agps_PortHandlers agps_portHandlers = {.agps_socket_fd = -1, .client_socket_fd = -1, .modem_tty_fd = -1, .threads_must_quit =0, .tty_port_mutex = PTHREAD_MUTEX_INITIALIZER, .socket_port_mutex = PTHREAD_MUTEX_INITIALIZER };
static char agps_uplink_buffer[AGPS_SOCKET_MAX_READ_SIZE+1];
static char agps_downlink_buffer[AGPS_TTY_MAX_READ_SIZE+1];

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/
static int open_tty_port(const char * device_path)
{
    int fd ;
    int arg;
    int status = AGPS_FAILURE;
    int delay = AGPS_TRANSPORT_OPEN_MIN_DELAY;

    agps_PortMutexLock(& agps_portHandlers.tty_port_mutex);
    if (agps_portHandlers.modem_tty_fd == -1)
    {
        ALOGI("%s: Opening %s \n", __FUNCTION__, device_path );

        do
        {
            /* open device file */
            fd = open(device_path, O_RDWR, 0);

            /* configure terminal */
            if (fd>=0)
            {
                struct termios options;

                ALOGI("%s: Configuring %s \n", __FUNCTION__, device_path );

                /* get current options */
                tcgetattr( fd, &options );

                /* switch to RAW mode - disable echo, ... */
                options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
                options.c_oflag &= ~OPOST;
                options.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
                options.c_cflag &= ~(CSIZE|PARENB);
                options.c_cflag |= CS8 | CLOCAL | CREAD;

                /* apply new settings */
                tcsetattr(fd, TCSANOW, &options);

                /* set DTR */
                arg = TIOCM_DTR;
                ioctl(fd, TIOCMBIS, &arg);

                agps_portHandlers.modem_tty_fd = fd;
                status = AGPS_OK;

                break;
            }
            else
            {
                if (delay < AGPS_TRANSPORT_OPEN_MAX_DELAY)
                {
                    ALOGE("%s: Failed to open file [%s] - errno = %d \n", __FUNCTION__, device_path, fd);
                }

                sleep(delay);

                if (delay < AGPS_TRANSPORT_OPEN_MAX_DELAY)
                {
                    delay *= 2;

                    if (delay == AGPS_TRANSPORT_OPEN_MAX_DELAY )
                    {
                        ALOGE("%s: Retrying  \n", __FUNCTION__);
                    }
                }
            }
        } while (1);
    };
    agps_PortMutexUnlock(& agps_portHandlers.tty_port_mutex);

    return (status);
}

static int tty_set_nonblocking_read()
{
    int flags = fcntl(agps_portHandlers.modem_tty_fd, F_GETFL, 0);

    if (flags < 0)
    {
        ALOGE("%s: flag error !! \n", __FUNCTION__);
        return AGPS_FAILURE;
    }

    if (fcntl(agps_portHandlers.modem_tty_fd, F_SETFL, flags | O_NONBLOCK) <0)
    {
        ALOGE("%s: set flag error !! \n", __FUNCTION__);
        return AGPS_FAILURE;
    }

    return AGPS_OK;
}

static int tty_set_blocking_read()
{
    int flags = fcntl(agps_portHandlers.modem_tty_fd, F_GETFL, 0);
    if (flags < 0)
    {
        ALOGE("%s: get flag error !! \n", __FUNCTION__);
        return AGPS_FAILURE;
    }

    if (fcntl(agps_portHandlers.modem_tty_fd, F_SETFL, flags ^ O_NONBLOCK) <0)
    {
        ALOGE("%s: set flag error !! \n", __FUNCTION__);
        return AGPS_FAILURE;
    }

    return AGPS_OK;
}


static int wait_for_modem_ready()
{
    int status = AGPS_FAILURE, read_status;
    unsigned int loop =0 , nb_read_bytes =0 ;
    char * tty_read_error;

    /* This command disables the default unsolited messages, GPS doesn't need/want them */
    const char cmd[]= "AT%IDBC=1\r\n";
    const unsigned int cmd_length = strlen(cmd);
    const char rsp[]= "OK\r\n";
    const unsigned int rsp_length = strlen(rsp);
    ALOGI("%s: cmd_length %d\n", __FUNCTION__, cmd_length);
    ALOGI("%s: rsp_length %d\n", __FUNCTION__, rsp_length);

    if (tty_set_nonblocking_read())
    {
        while ( (loop < AGPS_WAIT_FOR_MODEM_ATTEMPT) && (nb_read_bytes < AGPS_TTY_MAX_READ_SIZE ))
        {
            ALOGI("%s: loop %d\n", __FUNCTION__, loop);
            ALOGI("%s: Sending AT%%IDBC=1 to the modem\n", __FUNCTION__);

            /* Exit now as there's something wrong with the tty port, might need to re-open it later */
            if (send_tty_data(cmd, cmd_length) < 0)
            {
                break;
            }

            sleep(1);

            read_status = read_tty_data(&agps_downlink_buffer[nb_read_bytes], AGPS_TTY_MAX_READ_SIZE-nb_read_bytes, &tty_read_error);

            /* Exit now as there's something wrong with the tty port, might need to re-open it later */
            if ( (read_status <0) && (strstr(tty_read_error, "Try again") == NULL))
            {
                break;
            }
            if (read_status >0)
            {
                nb_read_bytes += read_status;
            }
            ALOGI("%s: nb_read_bytes = %d\n", __FUNCTION__, nb_read_bytes);
            if (nb_read_bytes >= rsp_length)
            {
                /*Found the expected OK response ?*/
                agps_downlink_buffer[nb_read_bytes]='\0';
                if (strstr(&agps_downlink_buffer[0], rsp) != NULL)
                {
                    status = AGPS_OK;
                    break;
                }
            }
            loop++;
        }
    }

    if ( (status != AGPS_OK) || (!tty_set_blocking_read()))
    {
        status = AGPS_FAILURE;
    }

    return status;
}


static void close_tty_port()
{
    ALOGI("%s: entering \n", __FUNCTION__);

   /*Lock a mutex to avoid trying to close a port while the other thread is writing data to it */
   agps_PortMutexLock(& agps_portHandlers.tty_port_mutex);
   if (agps_portHandlers.modem_tty_fd != -1)
   {
       ALOGI("%s: closing \n", __FUNCTION__);
       close(agps_portHandlers.modem_tty_fd);
       agps_portHandlers.modem_tty_fd = -1;
       ALOGI("%s: tty port is now closed\n", __FUNCTION__);
   }
   else
   {
       ALOGI("%s: tty port is already closed\n", __FUNCTION__);
   }

   agps_PortMutexUnlock(& agps_portHandlers.tty_port_mutex);
   ALOGI("%s: exiting \n", __FUNCTION__);
}

static void close_socket_port()
{
   ALOGI("%s: entering \n", __FUNCTION__);

   /* Lock a mutex to prevent from closing a port while the other thread is writing data to it */
   agps_PortMutexLock(& agps_portHandlers.socket_port_mutex);

   if (agps_portHandlers.client_socket_fd != -1)
   {
       ALOGI("%s: closing \n", __FUNCTION__);
       agps_PortShutdownSocket(agps_portHandlers.client_socket_fd);
       agps_portHandlers.client_socket_fd = -1;
       ALOGI("%s: socket port is now closed\n", __FUNCTION__);
   }
   else
   {
       ALOGI("%s: socket port is already closed\n", __FUNCTION__);
   }

   agps_PortMutexUnlock(& agps_portHandlers.socket_port_mutex);
   ALOGI("%s: exiting \n", __FUNCTION__);
}

static int send_tty_data(const char * buff, int len)
{
    int status = -1;

    /* Lock a mutex to prevent the port from being closed by the other thread during the operation */
    agps_PortMutexLock(& agps_portHandlers.tty_port_mutex);
    if (agps_portHandlers.modem_tty_fd != -1)
    {
        status = write(agps_portHandlers.modem_tty_fd, buff, len );
        if (status == -1)
        {
            ALOGE("%s: write to tty port error = %s\n", __FUNCTION__, agps_reportLastStringError());
        }
        else
        {
            ALOGI("%s: write complete, nbytes= %d\n",__FUNCTION__, status);
        }
    }
    agps_PortMutexUnlock(& agps_portHandlers.tty_port_mutex);

    return (status);
}


static int send_socket_data(const char * buff, int len)
{
    int status = -1;

    /* Lock a mutex to prevent the port from being accessed by the other thread, until we have finished */
    agps_PortMutexLock(& agps_portHandlers.socket_port_mutex);
    if (agps_portHandlers.client_socket_fd != -1)
    {
        status = write(agps_portHandlers.client_socket_fd, buff, len );
        if (status == -1)
        {
            ALOGE("%s: write to socket error = %s\n", __FUNCTION__, agps_reportLastStringError());
        }
        else
        {
            ALOGE("%s: write complete, nbytes= %d\n", __FUNCTION__, status);
        }
    }
    agps_PortMutexUnlock(& agps_portHandlers.socket_port_mutex);
    return (status);
}

static int read_socket_data(char *buff, int len)
{
    int status = -1;

    if (agps_portHandlers.client_socket_fd != -1)
    {
        /* blocking read */
        status = read(agps_portHandlers.client_socket_fd, buff, len);
        if (status <= 0 )
        {
            ALOGE("%s: read socket nb bytes=%d, status = %s\n", __FUNCTION__, status, agps_reportLastStringError());
        }
        else
        {
            /*status returns the number of bytes read*/
            * (buff + status) = '\0';
            ALOGI("%s: read socket, nb= %d, data = %s\n", __FUNCTION__, status, buff);
        }
    }
    return (status);
}

static int read_tty_data(char *buff, int len, char ** error_str)
{
    int status = -1;

    if (agps_portHandlers.modem_tty_fd != -1)
    {
        /* blocking read */
        status = read(agps_portHandlers.modem_tty_fd, buff, len);
        if (status <=0)
        {
            if (error_str != NULL)
            {
                * error_str = agps_reportLastStringError();
            }
            ALOGE("%s: read tty error = %s, status=%d\n", __FUNCTION__, agps_reportLastStringError(), status);
        }
        else
        {
            * (buff + status) = '\0';
            ALOGI("%s: read tty port nb = %d, data = %s\n", __FUNCTION__, status, buff);
        }
    }
    return (status);
}


/*************************************************************************************************
* Public function definitions (Not doxygen commented, as they should be exported in the header
* file)
************************************************************************************************/

void *agps_uplinkThread(void *arg)
{
    int file_op_sts,  nb_bytes_to_send, max_write_attempts ;
    char *current_time;

    ALOGD("%s: Entering \n", __FUNCTION__);

    /*Gives some time for the modem reboot and ready*/

    /* Open device tty port */
    while (open_tty_port(((agps_DeviceInfo *) arg)->device_path) != AGPS_OK)
    {
        ALOGE("%s: Failed to open tty [%s] because %s\n", __FUNCTION__, ((agps_DeviceInfo *) arg)->device_path, agps_reportLastStringError());
     /*   agps_portHandlers.threads_must_quit =1;
        ALOGE("%s: Exit \n", __FUNCTION__);
        return NULL; */
    }

    /* Poll the modem until it responds */
    if (wait_for_modem_ready() != AGPS_OK)
    {
        ALOGE("%s: Modem not answering to AT, close the port and retry \n", __FUNCTION__);

        close(agps_portHandlers.modem_tty_fd);
        agps_portHandlers.modem_tty_fd = -1;

        while (open_tty_port(((agps_DeviceInfo *) arg)->device_path) != AGPS_OK)
        {
            ALOGE("%s: Failed to open tty [%s] because %s\n", __FUNCTION__, ((agps_DeviceInfo *) arg)->device_path, agps_reportLastStringError());
        }

       /* agps_portHandlers.threads_must_quit =1;
        ALOGE("%s: Exit \n", __FUNCTION__);
        return NULL;*/
    }

    /* Get control of the socket and wait for the connection from client */
    agps_portHandlers.agps_socket_fd  = agps_AcceptFirstSocketConnection(((agps_DeviceInfo *) arg)->socket_path, &(agps_portHandlers.client_socket_fd));
    if ( agps_portHandlers.agps_socket_fd == -1)
    {
        ALOGE("%s: Failed to connect the socket\n", __FUNCTION__);
        goto ERROR;
    }

    ALOGI("%s: Enter main loop \n", __FUNCTION__ );
    do
    {
        /* Returns -1 if read failure */
        file_op_sts = read_socket_data(&agps_uplink_buffer[0], AGPS_SOCKET_MAX_READ_SIZE);

        if (file_op_sts <= 0 )
        /* socket proabaly closed, exit and restart AGPSD */
        {
            /* We may need to add later a reset command sent to modem*/
            goto ERROR;
        }

#ifdef DUMMY_CLOCK_CALIBRATION
        agps_uplink_buffer[file_op_sts] = '\0';
        if (strstr(&agps_uplink_buffer[0], "AT+REFCLKFREQ?"))
        {
            ALOGI("%s: detected AT+REFCLKFREQ?\n", __FUNCTION__ );
            ALOGI("%s: sending  +REFCLKFREQ:0,38400000,300,1 \n", __FUNCTION__ );
            send_socket_data("+REFCLKFREQ:0,38400000,300,1\r", strlen("+REFCLKFREQ:0,38400000,300,1\r"));
            ALOGI("%s: sending  OK\n", __FUNCTION__ );
            send_socket_data("OK\r", strlen("OK\r"));
            file_op_sts = 0;
        }
        else
            if (strstr(&agps_uplink_buffer[0], "AT+REFCLKFREQ=1") != NULL)
            {
                ALOGI("%s: detected AT+REFCLKFREQ=1\n", __FUNCTION__ );
                ALOGI("%s: sending  OK\n", __FUNCTION__ );
                send_socket_data("OK\r", strlen("OK\r"));
                file_op_sts = 0;
            }
#endif

        nb_bytes_to_send = file_op_sts;
        max_write_attempts = 0;

        while ((file_op_sts > 0) && (nb_bytes_to_send) && (max_write_attempts < AGPS_TTY_MAX_WRITE))
        {
            file_op_sts = send_tty_data(&agps_uplink_buffer[0], nb_bytes_to_send);

            if (file_op_sts < 0 )
            /* Modem is not answering, may have crashed; exit, and restart AGPSD. GPS will re-send the AT init sequence */
            {
                goto ERROR;
            }

            /* if file_op_sts == -1, we will exit this loop anyway */
            nb_bytes_to_send -= file_op_sts;
            max_write_attempts++;
        }
    } while (1);

ERROR:
    /* tell downlinkThread to exit in case it's still waiting for init complete */
    agps_portHandlers.threads_must_quit = 1;

    /* An error happened, close the socket and tty port (if not already closed), and exit*/
    close_socket_port();
    close_tty_port();
    ALOGE("%s: Exit \n", __FUNCTION__);
    return NULL;
}

void *agps_downlinkThread(void *arg)
{
    (void) arg;
    int file_op_sts, nb_bytes_to_send, max_write_attempts, offset, packet_size, next_packet  ;
    char * newLine_ptr;

    ALOGI("%s: Entering \n", __FUNCTION__);

    /* Wait until the tty and socket ports are initialized */
    while ((agps_portHandlers.modem_tty_fd < 0) || (agps_portHandlers.agps_socket_fd < 0)
        || (agps_portHandlers.client_socket_fd <0))
    {
        sleep(1);

        if (agps_portHandlers.threads_must_quit == 1)
        {
            ALOGE("%s: Exit \n", __FUNCTION__);
            return NULL;
        }
    }

    ALOGD("%s: Enter main loop \n", __FUNCTION__ );
    do
    {
        /* Returns -1 if read failure */
        nb_bytes_to_send = read_tty_data(&agps_downlink_buffer[0], AGPS_TTY_MAX_READ_SIZE, NULL);

        if (nb_bytes_to_send <= 0)
        {
            goto ERROR;
        }

        max_write_attempts = 0;
        offset = 0;

        /* main loop to send the data to the socket */
        while (nb_bytes_to_send && (max_write_attempts < AGPS_SOCKET_MAX_WRITE))
        {
            packet_size = nb_bytes_to_send;

            if (packet_size)
            {
                file_op_sts = send_socket_data(&agps_downlink_buffer[offset], packet_size);

                if (file_op_sts > 0)
                {
                    offset += file_op_sts;
                    /* Complete packet transmitted ?*/
                    if (file_op_sts == packet_size)
                    {
                        next_packet = 1;
                    }
                    else
                    {
                        usleep(30000);
                    }
                    packet_size -= file_op_sts;
                    nb_bytes_to_send -= file_op_sts;
                }
                else
                {
                    ALOGE("%s: Error, couldn't write to the socket\n", __FUNCTION__);
                    goto ERROR;
                }

                max_write_attempts++;
            }
        }
        if ( (max_write_attempts == AGPS_SOCKET_MAX_WRITE) && nb_bytes_to_send)
        {
            ALOGE("%s: Error, couldn't send all the bytes to the socket, remaining=%d\n", __FUNCTION__, nb_bytes_to_send);
            goto ERROR;
        }
    }
    while (1);

ERROR:
    /* An error happened, close the socket and tty port (if not already closed), and exit*/
    close_socket_port();
    close_tty_port();
    ALOGE("%s: Exit \n", __FUNCTION__);
    return NULL;
}


void *agps_testThreadAndroid(void *arg)
{
    int         sockfd, client_fd, len, i;
    int         result;
    char        received_data[500]       ;

    ALOGI("%s: Entering  \n", __FUNCTION__ );

    /* wait for socket init in the uplink_thread */

    sockfd = agps_PortCreateSocket();
    if (sockfd == -1)
    {
        ALOGE("%s: Failed to create the socket because %s\n", __FUNCTION__, agps_reportLastStringError());
        return NULL;
    }

    len = 30; // max attempts to connect to the socket
    while ((agps_PortConnectSocket(sockfd, ((char *) arg)) == -1))
    {
        ALOGE("%s: Failed to connect to %s, will try again\n", __FUNCTION__, ((char *) arg));
        sleep(1);
        len--;
        if (len ==0)
        {
            ALOGE("%s: Failed to connect to AGPS socket [%s] because %s\n", __FUNCTION__, (char *) arg, agps_reportLastStringError());
            return NULL;
        }
    }
    ALOGI("%s: Connected to socket  \n", __FUNCTION__ );


    ALOGI("%s: Start sending data \n", __FUNCTION__);
    write(sockfd, "AT+cfun?\r", strlen("AT+cfun?\r"));

    ALOGI("%s: IDUMMY test, should receive 100x commands from modem (10ms period) \n", __FUNCTION__);
    write(sockfd, "AT%IDUMMYU=10,1,1\r\n", strlen("AT%IDUMMYU=10,1,1\r\n"));
    len = 100;
    while (len)
    {
        result = read(sockfd, &received_data, 500);
        received_data[result] = '\0';
        len--;
    }

    /* Stop IDUMMYY messages */
    write(sockfd, "AT%IDUMMYU=1,0\r\n", strlen("AT%IDUMMYU=1,0\r\n"));
    result = read(sockfd, &received_data, 500);
    received_data[result] = '\0';

    for (i=0; i<100; i++)
    {
        write(sockfd, "AT%IECELLID=1\r\n", strlen("AT%IECELLID=1\r\n"));
        result = read(sockfd, &received_data, 500);
        received_data[result] = '\0';
        sleep(5);
    }

    sleep(10);
    close_tty_port();
    agps_PortShutdownSocket(sockfd);
    return NULL;
}




