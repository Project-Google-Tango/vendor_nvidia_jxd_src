/*************************************************************************************************
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup fild
 * @{
 */

/**
 * @file fild_fs.c FILD file system server.
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "fild.h"
#include "obex_transport.h"
#include "obex_file.h"
#include "obex_server.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

static int GetSize(int fd);
static void CloseFsChannel(void);
static obex_ResCode ServerOpen(void);
static void ServerClose(void);
static obex_ResCode ServerRead(char *buffer, int count);
static obex_ResCode ServerWrite(const char *buffer, int count);

/**************************************************************************************************************/
/* Private Type Declarations (if needed) */
/**************************************************************************************************************/

/** Privrate fs config */
typedef struct
{
    fildChannel channel;
    int max_frame_size;
    int is_hsic;
} fsConfig;

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

/** OBEX data transport fucntions over HIF */
static obex_TransportFuncs obex_transport_funcs = {
    ServerOpen,
    ServerClose,
    ServerRead,
    ServerWrite};

/** OBEX file operations */
static obex_FileFuncs obex_file_funcs = {
    .open     = fild_Open,
    .close    = fild_Close,
    .read     = fild_Read,
    .write    = fild_Write,
    .lseek    = fild_Lseek,
    .truncate = fild_Truncate,
    .lgetsize = GetSize,
    .remove   = fild_Remove,
    .copy     = fild_Copy,
    .rename   = fild_Rename,
    .stat     = fild_Stat
};

/** OBEX dir operations */
static obex_DirFuncs obex_dir_funcs = {
    .opendir  = fild_OpenDir,
    .closedir = fild_CloseDir,
    .readdir  = fild_ReadDir
};

/** FILD file system config */
static fsConfig fild_fs;

static int fild_fs_initialised = 0;

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 * Open file system server channel
 * Loop until it is opened or reboot detected.
 */
static obex_ResCode ServerOpen(void)
{
    int ok = 0, ret;
    fildBootState boot;

    if(fild_GetBootState() == FILD_NO_BOOT)
    {
        /* Open device */
        ALOGI("%s: Opening %s as server channel...\n",
             __FUNCTION__,
             fild_fs.channel.name);
        do
        {
            fild_fs.channel.handle = fild_Open(fild_fs.channel.name,
                                               O_RDWR|O_NOCTTY,
                                               0);
            if(fild_fs.channel.handle != -1)
            {
                ALOGI("%s: %s channel open.\n", __FUNCTION__, fild_fs.channel.name);
                ok = 1;
                if(fild_fs.channel.scheme == FILD_UART)
                {
                    ALOGD("%s: Configuring %s at %d\n",
                         __FUNCTION__,
                         fild_fs.channel.name,
                         fild_fs.channel.speed);
                    ret = fild_ConfigUart(fild_fs.channel.handle,
                                          fild_fs.channel.speed,
                                          fild_fs.channel.fctrl);
                    if(ret == -1)
                    {
                        ALOGE("%s: Fail to config channel %s. %s.\n",
                             __FUNCTION__,
                             fild_fs.channel.name,
                             fild_LastError());
                        CloseFsChannel();
                        ok = 0;
                    }
                }
                /* Send a dummy message to unlock modem OBEX client */
                if ((fild_GetBt2CompatibilityVersion() >= VERSION_SHM_OBEX_NOP) &&
                    (fild_fs.channel.scheme == FILD_SHM))
                {
                    ALOGI("%s: send SHM scheme OBEX unlock message.\n", __FUNCTION__);
                    char buf = '\0';
                    fild_WriteOnChannel(fild_fs.channel.handle, &buf, 1);
                }
            }
            else
            {
                /** wait 20ms before trying to open in order to avoid cpu
                 *  load when channel not available */
                fild_Usleep(20000);
            }

            boot = fild_GetBootState();
            if(boot != FILD_NO_BOOT)
            {
                /** While trying to open fs channel, a BB reset has been
                 *  detected. Exit. */
                ALOGI("%s: reboot detected.", __FUNCTION__);
                if(fild_fs.channel.handle != -1)
                {
                    CloseFsChannel();
                }
                ok=0;
            }
        } while(!ok && (boot == FILD_NO_BOOT));
    }

    return ok ? OBEX_OK:OBEX_ERR;
}

/**
 * Close file system server channel
 */
static void ServerClose(void)
{
    CloseFsChannel();
}

/**
 * Write function for Obex server transport layer.
 *
 * @param buffer  pointer to data storage.
 * @param count   amount of data to transmit.
 *
 * @return obex_ResCode
 */
static obex_ResCode ServerWrite(const char *buffer, int count)
{
    char *ptr = (char *)buffer;
    int res=OBEX_OK;
    int bytes_to_write, bytes_written;

    while(count>0)
    {
        /* Check possible HIF limitation */
        bytes_to_write = (fild_fs.max_frame_size == -1) ? count :fild_fs.max_frame_size;

        /* Transmit on HIF */
        bytes_written = fild_WriteOnChannel(fild_fs.channel.handle,
                                            ptr,
                                            bytes_to_write);
        if(bytes_written != bytes_to_write)
        {
            ALOGE("%s: write failure.\n", __FUNCTION__);
            if(bytes_written == 0)
            {
                res = OBEX_EOF;
            }
            else
            {
                res = OBEX_ERR;
            }
            break;
        }
        count -= bytes_written;
        ptr += bytes_written;
    }

    return res;
}

/**
 * Read function for Obex server transport layer.
 *
 * @param buffer  pointer to data storage.
 * @param count   amount to get.
 *
 * @return obex_ResCode
 */
static obex_ResCode ServerRead(char *buffer, int count)
{
    int read_bytes = 0, res=OBEX_OK;
    do
    {
        int bytes_read = 0;

        /* Get data from HIF */
        bytes_read = fild_Read(fild_fs.channel.handle,
                               buffer + read_bytes,
                               count - read_bytes);
        if(bytes_read <= 0)
        {
            if(bytes_read == 0)
            {
                ALOGE("%s: EOF on FS channel", __FUNCTION__);
                res=OBEX_EOF;
                break;
            }
            res = OBEX_ERR;
            ALOGE("%s: read failure. %s\n", __FUNCTION__, fild_LastError());
            break;
        }
        read_bytes += bytes_read;
    } while(count > read_bytes);

    return res;
}

/**
 * Get size of an opened file.
 * Implementation for obex_TransportFuncs lgetsize.
 *
 * @param fd  file descriptor
 *
 * @return int file size or -1
 */
static int GetSize(int fd)
{
    int size = -1;

    do
    {
        /* Get current offset */
        int current_offset = fild_Lseek(fd, 0, SEEK_CUR);
        if(current_offset == -1)
        {
            ALOGE("%s: Fail to get curent file offset. %s",
                  __FUNCTION__,
                  fild_LastError());
            break;
        }

        /* Get file size */
        size = fild_Lseek(fd, 0, SEEK_END);
        if(size == -1)
        {
            ALOGE("%s: Fail to get file size. %s",
                  __FUNCTION__,
                  fild_LastError());
            break;
        }

        /* Put back current offset */
        if(fild_Lseek(fd, current_offset, SEEK_SET) == -1)
        {
            ALOGE("%s: Fail to move back file descriptor. %s",
                  __FUNCTION__,
                  fild_LastError());
            size = -1;
        }

    } while(0);

    return size;
}

/**
 * Close file system channel
 */
static void CloseFsChannel(void)
{
    if(fild_fs.channel.handle != -1)
    {
        ALOGI("%s : Closing %s\n", __FUNCTION__, fild_fs.channel.name);
        fild_Close(fild_fs.channel.handle);
        fild_fs.channel.handle = -1;
        ALOGI("%s: %s closed.\n", __FUNCTION__, fild_fs.channel.name);
    }
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
int fild_IsFsUsed(void)
{
    return fild_fs.channel.used;
}

void fild_SetFsUnused(void)
{
    fild_fs.channel.used = 0;
}

void fild_FsServerDisconnect(void)
{
    if(fild_fs.channel.handle != -1)
    {
        /* Indicate server should now be considered as disconnected */
        obex_SetServerDisconnected();

        /* Close device...: this should cause any on-going read/write access to fail.
           Failure to be detected by server that will stop running since it is supposed
           to be disconnected... */
        CloseFsChannel();
    }
}

void fild_FsServerInit(fildFs *fs_cfg, fildHif *hif)
{
    /* Initialise OBEX server layer */
    obex_ServerInitialise();

    /* Register OBEX transport layer functional interface */
    obex_TransportRegister(&obex_transport_funcs);

    /* Register OBEX file layer functional interface */
    obex_FileRegister(&obex_file_funcs, &obex_dir_funcs);

    /* Update daemon inbox value with FILD root directory */
    ALOGD("File system root dir: %s\n", fs_cfg->inbox);
    obex_SetInbox(fs_cfg->inbox);

    /* Init fild_fs state */
    fild_fs.channel.used = 1;
    fild_fs.channel.scheme = hif->scheme;
    strncpy(fild_fs.channel.name, fs_cfg->channel, MAX_FILENAME_LEN);
    ALOGD("File system channel: %s\n", fild_fs.channel.name);
    fild_fs.channel.handle = -1;
    fild_fs.max_frame_size = fs_cfg->max_frame_size;
    ALOGD("Max frame size: %d\n", fild_fs.max_frame_size);
    if(hif->scheme == FILD_UART)
    {
        fild_fs.channel.speed   = hif->baudrate;
        fild_fs.channel.fctrl   = hif->fctrl;
    }
    fild_fs.is_hsic = fs_cfg->is_hsic;

    fild_fs_initialised = 1;
}

void fild_FsServer(void)
{
    if(fild_fs_initialised)
    {
        if(fild_fs.channel.scheme == FILD_UART)
        {
            if(fild_fs.is_hsic)
            {
                /** Always unload/reload USB stack to handle correctly MDM
                 *  enumeration that will occur - disable autosuspend */
                fild_UnloadUsbStack();
                fild_Sleep(1);
                fild_ReloadUsbStack();
                fild_UsbHostAlwaysOnActive();
            }
            else
            {
                /** Simply wait: we've just finished boot, BT3 was maybe
                 *  using FS server serial channel. In order to allow
                 *  de-enumeration... */
                fild_Sleep(1);
            }
        }

        /* Open server HIF channel */
        if(ServerOpen() == OBEX_OK)
        {
            /* USB HSIC connected, enable host controller autosuspend */
            if(fild_fs.is_hsic)
                fild_UsbHostAlwaysOnInactive();

            /* Start obex server... */
            obex_ServerRun();
        }
    }

    return;
}
