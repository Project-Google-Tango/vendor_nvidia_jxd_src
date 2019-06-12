/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup fild
 * @{
 */

/**
 * @file fild_port.c FILD OS abs layer code.
 *
 */

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

#include "fild.h"
#include "icera-util.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

#include <stdio.h>      /** sprintf... */
#include <errno.h>      /** errno... */
#include <sys/types.h>  /** open/lseek */
#include <sys/stat.h>   /** S_IRUSR,... */
#include <fcntl.h>      /** O_RDONLY,... */
#include <unistd.h>     /** close/read/write/lseek */
#include <termios.h>    /** B3500000, tcflush, tcsetattr,... */
#include <dirent.h>     /** readdir... */
#include <stdlib.h>     /** atoi */
#include <sys/mman.h>   /** mmap */
#include <poll.h>       /** poll */
#include <sys/sha1.h>   /** SHA1Init...: */

#ifndef FILD_DIRECT_IPC
#include <signal.h>      /** struct sigevent,... */
#include <sys/inotify.h> /** inotify_init,...*/
#endif

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/**
 * Will write amount of data on a serial device channel chunk
 * per chunk.
 * As kernel will always try to allocate contiguous memory for
 * entire data (at least default TTY_ACM driver), it is more
 * robust to ask for smaller chunks when necessary than to
 * create possible memory allocation errors.
 */
#define FILD_SERIAL_CHUNKS 2048 /** 2kB */

/** */
#define MAX_INT_STR_LEN 11 /** len(str(0xffffffff)) + '\0' */

#ifdef FILD_DIRECT_IPC

#define IPC_MAILBOX_OFFSET     0x0000
#define DRV_IPC_MSG_SHIFT      16
#define DRV_IPC_MSG_MASK       0xFFFF

/** Generate IPC MSG */
#define DRV_IPC_MESSAGE(id)           (((~(id)) << DRV_IPC_MSG_SHIFT) | id)
#define DRV_GET_IPC_MESSAGE(value)    (value & DRV_IPC_MSG_MASK)
#endif

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

const int fild_baudrate[] = {
    50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,9600, 19200, 38400,
    57600, 115200, 230400, 460800, 500000, 576000, 921600, 1000000, 1152000, 1500000,
    2000000, 2500000, 3000000, 3500000, 4000000,
    -1
};

static int baudcode[] = {
    B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800, B2400, B4800, B9600, B19200, B38400,
    B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000,
    B2000000, B2500000, B3000000, B3500000, B4000000
};

/** Is USB stack handling framework initialised ? 1 if OK */
static int modem_usb_stack_initialised = 0;

/** Name of sysfs folder in file system */
char fild_shm_sysfs_name[MAX_DIRNAME_LEN];

/** Name of private memory device in file system */
char fild_shm_priv_dev_name[MAX_DIRNAME_LEN];

/** Name of ipc memory device in file system */
char fild_shm_ipc_dev_name[MAX_DIRNAME_LEN];

/** Name of sysfs IPC status file in file system  */
static char fild_shm_sysfs_status_name[MAX_FILENAME_LEN];

/** File descriptor of private memory device */
static int fild_shm_private_fd;

/** File descriptor of ipc memory device */
static int fild_shm_ipc_fd;

#ifdef FILD_DIRECT_IPC
/** mmap'd ipc mem region*/
static unsigned int *fild_shm_ipc_mem = NULL;

/** last IPC message received */
static fildIpcId last_ipc = IPC_ID_MAX_MSG;
#else

/** latest IPC msg */
static int fild_ipc;

/** an IPC message is available or not */
static bool fild_ipc_msg = false;

/** used to protect update of fild_ipc_msg */
static pthread_mutex_t fild_ipc_mutex;

/** used to wait change of fild_ipc_msg status */
static pthread_cond_t fild_ipc_cond;

/** thread used to poll for new IPC msg */
static pthread_t fild_ipc_thread;

#endif

/** Coredump storage data */
static LoggingData fild_log;
static bool fild_log_initialised = false;
static char fild_log_time_str[MAX_TIME_LENGTH+1];
static char fild_log_dated_dir[PATH_MAX];

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

#ifdef  FILD_EXTERNAL_LOG
#include <stdarg.h>
void Out(int level, const char *fmt, ...)
{
    struct tm *local;
    time_t t;

    t = time(NULL);
    local = localtime(&t);
    if(level <= fild_verbosity)
    {
        va_list args;
        fprintf(stdout, "%04d%02d%02d_%02d%02d%02d  ",
                 (1900+local->tm_year),
                 (local->tm_mon+1),
                 local->tm_mday,
                 local->tm_hour,
                 local->tm_min,
                 local->tm_sec);
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);
        fprintf(stdout, "\n");
        fflush(stdout);
    }
}
#endif

/**************************************************
 * File system or device access
 *************************************************/
/**
 * Fully remove a directory even if not empty.
 *
 * @param pathname
 *
 * @return int 0 on success, -1 on failure and errno is set
 *         appropriately
 */
static int RemoveDir(const char *path)
{
    int err = 0;
    struct dirent *p;
    struct stat estat;
    char *buf;
    size_t len, path_len = strlen(path), sep_len = strlen("/");

    ALOGI("%s: %s", __FUNCTION__, path);

    DIR *d = opendir(path);
    if(d)
    {
        p = readdir(d);
        while(p)
        {
            if (strcmp(p->d_name, ".") && strcmp(p->d_name, ".."))
            {
                asprintf(&buf, "%s/%s", path, p->d_name);
                err = fild_Remove(buf);
                free(buf);
                if(err)
                {
                    break;
                }
            }
            p = readdir(d);
        }
        closedir(d);
    }
    else
    {
        err = -1;
    }

    if(!err)
    {
        /* Folder should now be empty */
        err = remove(path);
    }

    return err;

}

int fild_Open(const char *pathname, int flags, int mode)
{
    return open(pathname, flags, mode);
}

int fild_Close(int fd)
{
    return close(fd);
}

int fild_Read(int fd, void *buf, int count)
{
    return read(fd, buf, count);
}

int fild_Write(int fd, const void *buf, int count)
{
    return write(fd, buf, count);
}

int fild_WriteOnChannel(int handle, const void *buf, int count)
{
    char *ptr = (char *)buf;
    int chunk, written=0, num=count;
    int err = 0;

    while(num>0)
    {
        chunk = (num > FILD_SERIAL_CHUNKS) ? FILD_SERIAL_CHUNKS:num;
        written = write(handle, ptr, chunk);
        if(written > 0)
        {
            num -= written;
            ptr += written;
        }
        else if(written < 0)
        {
            if (errno != EINTR) {
                ALOGE("%s: failure. %s", __FUNCTION__, strerror(errno));
                err=1;
                break;
            }
            else
            {
                ALOGW("%s: failure. %s", __FUNCTION__, strerror(errno));
            }
        }
        else
        {
            /** written == 0... */
            ALOGE("%s: EOF on channel.", __FUNCTION__);
            err=1;
            break;
        }
    }

    return err ? written:count;
}

int fild_Lseek(int fd, int offset, int whence)
{
    return lseek(fd, offset, whence);
}

int fild_Copy(const char *src, const char *dst)
{
    int err = -1;
    int from = -1, to = -1;
    char *buf = malloc(FILE_BLOCK);
    int src_size;
    int remaining_bytes, bytes_to_read = FILE_BLOCK;

    ALOGD("%s: %s to %s\n", __FUNCTION__, src, dst);

    do
    {
        /* Open src and get size */
        from = fild_Open(src, O_RDONLY, 0);
        if( from == -1)
        {
            ALOGE("%s: cannot open src file: %s\n", __FUNCTION__, src);
            break;
        }
        src_size = fild_Lseek(from, 0, SEEK_END);
        fild_Lseek(from, 0, SEEK_SET);
        remaining_bytes = src_size;

        /* Open src & dst files */
        to = fild_Open(dst, O_CREAT | O_WRONLY, DEFAULT_CREAT_FILE_MODE);
        if(to == -1)
        {
            ALOGE("%s: cannot open dst: %s\n", __FUNCTION__, dst);
            fild_Close(from);
            break;
        }

        /* Copy src to dst */
        while(remaining_bytes > 0)
        {
            bytes_to_read = remaining_bytes < FILE_BLOCK ? remaining_bytes : FILE_BLOCK;
            fild_Read(from, buf, bytes_to_read);
            fild_Write(to, buf, bytes_to_read);
            remaining_bytes -= bytes_to_read;
        }
        fild_Close(from);
        fild_Close(to);
        err = 0;
    } while(0);

    free(buf);

    return err;
}

int fild_Poll(int fd, char *buf, int count, int timeout)
{
    fd_set set;
    struct timeval t;
    int res, numread = -1;

    FD_ZERO(&set);
    FD_SET(fd, &set);

    t.tv_usec = timeout * 1000;
    t.tv_sec = t.tv_usec / 1000000;
    t.tv_usec = t.tv_usec % 1000000;

    res = select(fd+1, &set, NULL, NULL, (timeout == -1) ? NULL:&t);

    if (res == 1 && FD_ISSET(fd, &set))
    {
        numread = fild_Read(fd, buf, count);
    }
    else
    {
        ALOGE("%s: Read timeout.\n", __FUNCTION__);
    }

    return numread;
}

char *fild_LastError(void)
{
    return strerror(errno);
}

int fild_Rename(const char *oldpath, const char *newpath)
{
#ifdef WIN32
    /* No way to rename a file if dest already exists... */
    remove(new_file_name);
#endif
    return rename(oldpath, newpath);
}

int fild_Remove(const char *pathname)
{
    int err=0;
    struct stat fstat;
    err = stat(pathname, &fstat);
    if(!err)
    {
        if (S_ISDIR(fstat.st_mode))
        {
            err = RemoveDir(pathname);
        }
        else
        {
            err = remove(pathname);
        }
    }
    return err;
}

int fild_Truncate(const char *pathname, int offset)
{
    int fd = -1, ret = -1;

    fd = fild_Open(pathname, O_RDWR, 0);
    if(fd != -1)
    {
        ret = ftruncate(fd, offset);
        fild_Close(fd);
    }
    else
    {
        ALOGE("%s: fail to open %s. %s", __FUNCTION__, pathname, fild_LastError());
    }

    return ret;
}

int fild_Stat(const char *path, obex_Stat *buf)
{
    return stat(path, buf);
}

int fild_CheckFileExistence(const char *pathname)
{
    int fd = -1;

    fd = fild_Open(pathname, O_RDONLY, 0);
    if(fd != -1)
    {
        fild_Close(fd);
    }

    return fd;
}

int fild_GetFileSize(const char *pathname)
{
    int size = -1, fd;
    fd = fild_Open(pathname, O_RDONLY, 0);
    if(fd != -1)
    {
        size = fild_Lseek(fd, 0, SEEK_END);
        fild_Close(fd);
    }

    return size;
}

obex_Dir* fild_OpenDir(const char *name)
{
    return opendir(name);
}

int fild_CloseDir(obex_Dir *dir)
{
     return closedir(dir);
}

obex_OsDirEnt *fild_ReadDir(obex_Dir *dir)
{
    return readdir(dir);
}

int fild_Mkdir(const char *pathname, int mode)
{
    return mkdir(pathname, mode);
}

/**************************************************
 * Shared memory
 *************************************************/
/**
 * Read 1st line of a sysfs file as an int value.
 *
 * @param entry_name
 * @param dest
 *
 * @return int 0 if OK, 1 if NOK
 */
static int GetSysFsIntEntry(char *entry_name, int fd, void *dest)
{
    int err=0, i, num;
    char buf[MAX_INT_STR_LEN];
    char *p = buf;
    int *dst = (int *)dest;

    do
    {
        if(entry_name)
        {
            fd = open(entry_name, O_RDONLY, 0);
            if(fd == -1)
            {
                ALOGE("%s: Fail to open [%s]. %s",
                     __FUNCTION__,
                     entry_name,
                     strerror(errno));
                err = 1;
                break;
            }
        }
        for(i=0; i<MAX_INT_STR_LEN; i++)
        {
            num = read(fd, p, 1);
            if(num != 1)
            {
                ALOGE("%s: Fail to read. %s",
                     __FUNCTION__,
                     strerror(errno));
                buf[0] = '\0';
                err = 1;
                break;
            }
            if(*p=='\n')
            {
                *p = '\0';
                break;
            }
            p++;
        }
        if(!err)
        {
            *dst = atoi(buf);
        }
    } while(0);

    if(entry_name && (fd != -1))
    {
        close(fd);
    }

    return err;
}

#ifdef ANDROID
/** assumption is that ANDROID code will run on target so
 *  that poll operation can be used on sysfs device, whereas
 *  for another Linux system, used for test, poll is not
 *  relevant on a single file and replaced with inotify usage */
static void *IpcPollRoutine(void *arg)
{
    (void) arg;
    int err=0;
    struct pollfd fds;
    int res;
    int fd = -1;

    while(1)
    {
        /* open status IPC file */
        fd = open(fild_shm_sysfs_status_name, O_RDWR);
        if(fd == -1)
        {
            ALOGE("%s: fail to open [%s]. %s",
                 __FUNCTION__,
                 fild_shm_sysfs_status_name,
                 strerror(errno));
            break;
        }

        /* read existing value */
        pthread_mutex_lock(&fild_ipc_mutex);
        err = GetSysFsIntEntry(NULL, fd, &fild_ipc);
        if(err)
        {
            pthread_mutex_unlock(&fild_ipc_mutex);
            ALOGE("%s: IPC 1st read error.", __FUNCTION__);
        }
        else
        {
            ALOGD("%s: Got IPC: 0x%x",
                 __FUNCTION__,
                 fild_ipc);

            /* signal an IPC message is available */
            fild_ipc_msg = true;
            pthread_cond_signal(&fild_ipc_cond);
            pthread_mutex_unlock(&fild_ipc_mutex);

            /* poll for next request */
            fds.fd = fd;
            fds.events = POLLPRI | POLLERR;
            res = poll(&fds, 1, -1);
            if(res < 0)
            {
                ALOGE("%s: poll error. %s",
                     __FUNCTION__,
                     strerror(errno));
            }
        }
        close(fd);
    }

    return NULL;
}
#else
#ifndef FILD_DIRECT_IPC
#define EVENT_SIZE     (sizeof (struct inotify_event))
#define EVENT_BUF_LEN  (1024 * ( EVENT_SIZE + 16 ))
static void *IpcPollRoutine(void *arg)
{
    int fd=-1,wd=-1,sfd=-1;
    int length, i = 0;
    char buffer[EVENT_BUF_LEN];


    /** creating the INOTIFY instance*/
    fd = inotify_init();
    if(fd == -1)
    {
        ALOGE("%s: fail to init inotify. %s",
             __FUNCTION__,
             strerror(errno));
        return NULL;
    }

    /** adding the fild_shm_sysfs_status_name into watch list.*/
    wd = inotify_add_watch(fd,
                           fild_shm_sysfs_status_name,
                           IN_MODIFY);
    if(wd == -1)
    {
        ALOGE("%s: fail to add %s in watch list. %s",
             __FUNCTION__,
             fild_shm_sysfs_status_name,
             strerror(errno));
        close(fd);
        return NULL;
    }

    while(1)
    {
        /* wait for event notification.... */
        ALOGD("%s: wait for notification on %s...",__FUNCTION__, fild_shm_sysfs_status_name);
        length = read(fd, buffer, EVENT_BUF_LEN);
        if(length < 0)
        {
            ALOGE("%s: read error. %s",
                 __FUNCTION__,
                 strerror(errno));
            break;
        }

        /* parse notification */
        while(i < length)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if(event->mask & IN_MODIFY)
            {
                /* status file modified: read the new value */
                sfd = open(fild_shm_sysfs_status_name, O_RDONLY);
                if(sfd == -1)
                {
                    ALOGE("%s: fail to open %s to get new value. %s",
                         __FUNCTION__,
                         fild_shm_sysfs_status_name,
                         strerror(errno));
                    break;
                }
                if(GetSysFsIntEntry(NULL, sfd, &fild_ipc))
                {
                    break;
                }
                ALOGD("%s: Got IPC: 0x%x",
                     __FUNCTION__,
                     fild_ipc);
                sem_post(&fild_ipc_sem);
                break;
            }
            i += EVENT_SIZE + event->len;
        }
        if(sfd != -1)
        {
            close(sfd);
        }
    }

    if(wd != -1)
    {
        inotify_rm_watch(fd, wd);
    }

    if(fd != -1)
    {
        close(fd);
    }

    return NULL;
}
#endif /* #ifndef FILD_DIRECT_IPC */
#endif

int fild_ShmBootInit(fildShmCtx *shm_ctx)
{
    int err = 0;
    struct stat dstat;
    char filename[MAX_FILENAME_LEN];

    do
    {
        /* Check sysfs directory existence */
        if(stat(fild_shm_sysfs_name, &dstat)==-1)
        {
            ALOGE("%s: cannot find shm sysfs interface [%s]. %s",
                __FUNCTION__,
                fild_shm_sysfs_name,
                strerror(errno));
            err = 1;
            break;
        }

        /* Check shared memory devices existence */
        if(stat(fild_shm_priv_dev_name, &dstat)==-1)
        {
            ALOGE("%s: cannot find shm private interface [%s]. %s",
                __FUNCTION__,
                fild_shm_priv_dev_name,
                strerror(errno));
            err = 1;
            break;
        }
        if(stat(fild_shm_ipc_dev_name, &dstat)==-1)
        {
            ALOGE("%s: cannot find shm ipc interface [%s]. %s",
                __FUNCTION__,
                fild_shm_ipc_dev_name,
                strerror(errno));
            err = 1;
            break;
        }

        /* Set and check sysfs entries */
        // status: we set/get IPC here
        snprintf(fild_shm_sysfs_status_name, MAX_FILENAME_LEN, "%s/status", fild_shm_sysfs_name);

        // private_size: to contain shm private window size
        snprintf(filename, MAX_FILENAME_LEN, "%s/priv_size", fild_shm_sysfs_name);
        err = GetSysFsIntEntry(filename, -1, &shm_ctx->private_size);
        if(err)
        {
            break;
        }
        ALOGD("%s: shm private size [%s] is %d (0x%x) bytes",
             __FUNCTION__,
             filename,
             shm_ctx->private_size,
             shm_ctx->private_size);

        // ipc size: to contain shm ipc window size
        snprintf(filename, MAX_FILENAME_LEN, "%s/ipc_size", fild_shm_sysfs_name);
        err = GetSysFsIntEntry(filename, -1, &shm_ctx->ipc_size);
        if(err)
        {
            break;
        }
        ALOGD("%s: shm ipc size [%s] is %d (0x%x) bytes",
             __FUNCTION__,
             filename,
             shm_ctx->ipc_size,
             shm_ctx->ipc_size);

        /* Set and check shared mem device entries */
        // mem_private: device for private shm window
        fild_shm_private_fd = open(fild_shm_priv_dev_name, O_RDWR, 0);
        if(fild_shm_private_fd == -1)
        {
            ALOGE("%s: Fail to open [%s]. %s",
                 __FUNCTION__,
                 fild_shm_priv_dev_name,
                 strerror(errno));
            err = 1;
            break;
        }
        shm_ctx->private_addr = mmap(NULL,
                                     shm_ctx->private_size,
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED,
                                     fild_shm_private_fd,
                                     0);
        ALOGD("%s: [%s] mapped at 0x%x.",
             __FUNCTION__,
             fild_shm_priv_dev_name,
             (unsigned int)shm_ctx->private_addr);
        ALOGD("%s: 0x%x bytes in secured area", __FUNCTION__, shm_ctx->secured);
        if (shm_ctx->extmem_size)
        {
            if (shm_ctx->extmem_size > shm_ctx->private_size)
            {
                ALOGE("%s: Invalid BBC private settings: 0x%x 0x%x.",
                    __FUNCTION__,
                    shm_ctx->private_size,
                    shm_ctx->extmem_size);
                err = 1;
                break;
            }
            shm_ctx->reserved = shm_ctx->private_size - shm_ctx->extmem_size;
        }
        ALOGD("%s: 0x%x bytes reserved in private window.",
             __FUNCTION__,
             shm_ctx->reserved);

        // mem_ipc: device for IPC shm window
        fild_shm_ipc_fd = open(fild_shm_ipc_dev_name, O_RDWR, 0);
        if(fild_shm_ipc_fd == -1)
        {
            ALOGE("%s: Fail to open [%s]. %s",
                 __FUNCTION__,
                 fild_shm_ipc_dev_name,
                 strerror(errno));
            err = 1;
            break;
        }
        shm_ctx->ipc_addr = mmap(NULL,
                                 shm_ctx->ipc_size,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 fild_shm_ipc_fd,
                                 0);
#ifdef FILD_DIRECT_IPC
        fild_shm_ipc_mem = (unsigned int*)shm_ctx->ipc_addr;
#endif
        ALOGD("%s: [%s] mapped at 0x%x.",
             __FUNCTION__,
             fild_shm_ipc_dev_name,
             (unsigned int)shm_ctx->ipc_addr);

        /** Indicate COLD boot in IPC mailbox: this init is called
         *  when starting FILD, that's to say at platform start-up.
         *  We need to propose a clean mailbox
         *  to BROM before FILD takes BBC out of reset */
        fild_ShmIpcPost(IPC_ID_BOOT_COLD_BOOT_IND);

#ifndef FILD_DIRECT_IPC
        /* Init IPC wait loop... */
        if (pthread_mutex_init(&fild_ipc_mutex, NULL))
        {
            ALOGE("%s: fail to init mutex: %s", __FUNCTION__, strerror(errno));
            return 1;
        }
        if (pthread_cond_init(&fild_ipc_cond, NULL))
        {
            ALOGE("%s: fail to init cond: %s", __FUNCTION__, strerror(errno));
            return 1;
        }
        if (pthread_create(&fild_ipc_thread, NULL, IpcPollRoutine, NULL))
        {
            ALOGE("%s: fail to create thread: %s", __FUNCTION__, strerror(errno));
            return 1;
        }
#endif

    } while(0);

    return err;
}

void fild_ShmIpcWait(fildIpcId *ipc)
{
#ifndef FILD_DIRECT_IPC
    /* Get latest IPC msg */
    ALOGD("%s: wait for next IPC msg", __FUNCTION__);
    pthread_mutex_lock(&fild_ipc_mutex);
    while(!fild_ipc_msg)
        pthread_cond_wait(&fild_ipc_cond, &fild_ipc_mutex);
    fild_ipc_msg = false;
    *ipc = fild_ipc;
    pthread_mutex_unlock(&fild_ipc_mutex);
#else
  /* Get IPC message direct from IPC mmap'd file */
  if (fild_shm_ipc_mem)
  {
      /* Loop until message changes */
      do
      {
        *ipc = DRV_GET_IPC_MESSAGE(fild_shm_ipc_mem[IPC_MAILBOX_OFFSET / 4]);
        usleep(1000);
      }
      while (last_ipc == *ipc);

      last_ipc = *ipc;
  }
#endif
}

void fild_ShmIpcPost(fildIpcId ipc)
{
#ifndef FILD_DIRECT_IPC
    char ipc_str[MAX_INT_STR_LEN];
    int fd = -1;
    int num;

    snprintf(ipc_str, sizeof(ipc_str), "%d\n", ipc);
    ALOGD("%s: posting [0x%x] IPC. %s",
         __FUNCTION__,
         ipc,
         ipc_str);
    fd = open(fild_shm_sysfs_status_name, O_RDWR);
    if(fd == -1)
    {
        ALOGE("%s: fail to open [%s]. %s",
             __FUNCTION__,
             fild_shm_sysfs_status_name,
             strerror(errno));
    }
    else
    {
        num = write(fd, ipc_str, strlen(ipc_str));
        if(num == -1)
        {
            ALOGE("%s: fail to write in [%s]. %s",
                 __FUNCTION__,
                 fild_shm_sysfs_status_name,
                 strerror(errno));
        }
        else
        {
            if((unsigned int)num != strlen(ipc_str))
            {
                ALOGE("%s: fail to write entire IPC. %d instead of %d",
                     __FUNCTION__,
                     num,
                     strlen(ipc_str));
            }
        }
        fild_Close(fd);
    }
    return;
#else
    if (fild_shm_ipc_mem)
    {
        fild_shm_ipc_mem[IPC_MAILBOX_OFFSET / 4] = DRV_IPC_MESSAGE(ipc);
        last_ipc = ipc;
    }
#endif
}

/**************************************************
 * Modem power control
 *************************************************/

void fild_DisableModemPowerControl(void)
{
#ifdef ANDROID
    if(strcmp(fild_modem_power_control, "enabled")==0)
    {
        /* Disable modem power control only if enabled... */
        property_set("gsm.modem.powercontrol", "fil_disabled");
    }
#endif
}

void fild_RestoreModemPowerControl(void)
{
#ifdef ANDROID
    if(strcmp(fild_modem_power_control, "fil_disabled")==0)
    {
        /** FILD has disabled gsm.modem.powercontrol and has been
         *  re-started: this means modem power control must be
         *  re-enabled */
        property_set("gsm.modem.powercontrol", "enabled");
        ALOGW("%s: fixed gsm.modem.powercontrol startup value", __FUNCTION__);
        fild_modem_power_control = "enabled";
    }
    else
    {
        property_set("gsm.modem.powercontrol", fild_modem_power_control);
    }
#endif
}

/**************************************************
 * RIL control
 *************************************************/

static int allow_ril_start = 0;

void fild_RilStart(void)
{
#ifdef ANDROID
    int nvmtt_mode, modemtest_mode;
    char prop[PROPERTY_VALUE_MAX];

    if (allow_ril_start)
    {
        /*
         * Modem testmode: dedicated USB profile with AT command and modem
         * debug ports forwarded to gadget ACM interfaces
         * Mode = 1, RIL is to be stopped - do not start it here
         * Mode = 2, RIL is to be running
         */
        property_get("persist.modem.icera.testmode", prop, "0");
        modemtest_mode = strtol(prop, NULL, 10);
        if (modemtest_mode == 1)
        {
            ALOGI("Modem testmode (1), RIL not started");
            return;
        }

        /*
         * NVMTT mode: dedicated RIL TEST service is running instead
         * of usual RIL
         */
        property_get("ril.testmode", prop, "0");
        nvmtt_mode = strtol(prop, NULL, 10);
        if (nvmtt_mode == 0)
        {
            ALOGI("Start RIL");
            property_set("gsm.modem.ril.enabled", "1");
        }
        else if (nvmtt_mode == 1)
        {
            ALOGI("Start RIL TEST");
            property_set("gsm.modem.riltest.enabled", "1");
        }
    }
#endif
}

void fild_RilStop(void)
{
#ifdef ANDROID
    ALOGI("Stop RIL / RIL TEST");
    property_set("gsm.modem.ril.enabled", "0");
    property_set("gsm.modem.riltest.enabled", "0");
#endif
}

void fild_RilStartEnable(void)
{
    allow_ril_start = 1;
}

void fild_RilStartDisable(void)
{
    allow_ril_start = 0;
}


/**************************************************
 * Threads...
 *************************************************/

int fild_ThreadCreate(fildThread *thread, void *(*start_routine)(void *), void *arg)
{
    return pthread_create(thread, NULL, start_routine, arg);
}

int fild_SemInit(fildSem *sem, int shared, int value)
{
    return sem_init(sem, shared, value);
}

int fild_SemPost(fildSem *sem)
{
    return sem_post(sem);
}

int fild_SemWait(fildSem *sem)
{
    return sem_wait(sem);
}

#ifndef FILD_DIRECT_IPC
/**************************************************
 * Timers
 *************************************************/
int fild_TimerCreate(fildTimer *t, fildTimerHandler handler)
{
    struct sigevent se;
    se.sigev_signo             = 0;
    se.sigev_notify            = SIGEV_THREAD;
    se.sigev_value.sival_ptr   = t;
    se.sigev_notify_function   = (void (*)(union sigval))handler;
    se.sigev_notify_attributes = NULL;

    return timer_create(CLOCK_REALTIME, &se, t);
}

int fild_TimerStart(fildTimer t, int secs)
{
    struct itimerspec ts;
    ts.it_value.tv_sec = secs;
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    return timer_settime(t, 0, &ts, 0);
}

int fild_TimerStop(fildTimer t)
{
    struct itimerspec ts;
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    return timer_settime(t, 0, &ts, 0);
}

int fild_TimerDelete(fildTimer t)
{
    return timer_delete(t);
}
#endif /* #ifndef FILD_DIRECT_IPC */

/**************************************************
 * UART
 *************************************************/

int fild_ConfigUart(int com_hdl, int rate, int fctrl)
{
    int ret, i;
    int speed = B0;
    struct termios options;

    /* Get corresponding baudcode.
       rate is supposed to be OK since it has been checked at init... */
    for(i = 0; fild_baudrate[i] > 0; i++)
        if (fild_baudrate[i] == rate)
            speed = baudcode[i];

    tcgetattr(com_hdl, &options);

    /* Raw mode */
    options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
    options.c_cflag &= ~(CSIZE|PARENB);
    options.c_cflag |= CS8 | CLOCAL | CREAD;
    if(fctrl)
    {
        /* flow control */
        options.c_cflag |= CRTSCTS;
    }
    else
    {
        /* no flow control */
        options.c_cflag &= ~CRTSCTS;
    }

    /* Apply...*/
    do
    {
        ret = cfsetispeed(&options, speed);
        if(ret == -1)
        {
            break;
        }

        ret = cfsetospeed(&options, speed);
        if(ret == -1)
        {
            break;
        }

        ret = tcsetattr(com_hdl, TCSANOW, &options);
    } while(0);

    return ret;
}

/**************************************************
 * LP0 handling...
 *************************************************/
void fild_WakeLockInit(void)
{
#ifdef ANDROID
    IceraWakeLockRegister("fildLock", false);
#endif
}

void fild_WakeLock(void)
{
#ifdef ANDROID
    IceraWakeLockAcquire();
#endif
}

void fild_WakeUnLock(void)
{
#ifdef ANDROID
    IceraWakeLockRelease();
#endif
}

/**************************************************
 * Modem USB stack handling
 *************************************************/
void fild_UsbStackInit(void)
{
#ifdef ANDROID
    modem_usb_stack_initialised = IceraUsbStackConfig();
#endif
}

void fild_UnloadUsbStack(void)
{
    if(modem_usb_stack_initialised)
    {
#ifdef ANDROID
        IceraUnloadUsbStack();
#endif
    }
}

void fild_ReloadUsbStack(void)
{
    if(modem_usb_stack_initialised)
    {
#ifdef ANDROID
        IceraReloadUsbStack();
#endif
    }
}

void fild_UsbHostAlwaysOnActive(void)
{
    if(modem_usb_stack_initialised)
    {
#ifdef ANDROID
        IceraUsbHostAlwaysOnActive();
#endif
    }
}

void fild_UsbHostAlwaysOnInactive(void)
{
    if(modem_usb_stack_initialised)
    {
#ifdef ANDROID
        IceraUsbHostAlwaysOnInactive();
#endif
    }
}


/**************************************************
 * Misc...
 *************************************************/
void fild_Sleep(unsigned int seconds)
{
    sleep(seconds);
    return;
}

void fild_Usleep(unsigned int useconds)
{
    usleep(useconds);
    return;
}

unsigned int fild_Time(void)
{
    return (unsigned int)time(NULL);
}

/**************************************************
 * Modem logs storage handling
 *************************************************/
void fild_LogInit(char *root_dir, int limit)
{
    strncpy(fild_log.dir, root_dir, PATH_MAX);
    fild_log.dir[PATH_MAX-1] = '\0';
    fild_log.max = limit;
    strncpy(fild_log.prefix, COREDUMP_NAME_PREFIX, PREFIX_MAX);
    fild_log_initialised = true;
}

int fild_LogStart(void)
{
    int fd = -1;
    if (fild_log_initialised)
    {
        /* Get timestamp for coredump files/dir */
        IceraLogGetTimeStr(fild_log_time_str, sizeof(fild_log_time_str));

        /** Create new dated dir to store coredump */
        int err = IceraLogNewDatedDir(&fild_log,
                fild_log_time_str,
                fild_log_dated_dir);
        if (!err)
        {
            /* Create/Open coredump file in dated dir */
            char *filename;
            asprintf(&filename,
                     "%s/%s_%s%s",
                     fild_log_dated_dir,
                     fild_log.prefix,
                     fild_log_time_str,
                     COREDUMP_NAME_EXTENSION);
            fd = fild_Open(filename, O_CREAT | O_WRONLY, DEFAULT_CREAT_FILE_MODE);
            if(fd == -1)
            {
                ALOGE("%s: fail to open %s. %s",
                      __FUNCTION__,
                      filename,
                      fild_LastError());
            }
            free(filename);
        }
    }
    return fd;
}

void fild_LogStop(int fd)
{
    if (fd != -1)
    {
        /* Close opened coredump file */
        fild_Close(fd);

        /* Add AP logs */
        IceraLogStore(
                COREDUMP_MODEM,
                fild_log_dated_dir,
                fild_log_time_str);
    }
}

/**************************************************
 * Modem f/w update check.
 *************************************************/
#define FILE_HASHES_LIST "/data/rfs/data/persistent/update_ind"
#define FILE_HASHES_LIST_PART FILE_HASHES_LIST ".part"
#define MDM_PATH "/system/vendor/firmware/app/modem.wrapped"
#define MDM_FILES_DIR "/data/rfs/data/modem"
#define MDM_MEP_FILENAME "NRAM2_ME_PERSONALISATION.bin"

/** Num of files handled:
 *
 *  TODAY: only modem file is useful for f/w update
 *  detection
 *
 *  TODO: update code to support multiple files */
#define NUM_OF_SUPPORTED_FILES 1

typedef struct
{
    char filename[PATH_MAX];
    unsigned char hash_str[SHA1_DIGEST_LENGTH*2+1];
}HashListElem;

/**
 * Compute SHA1 of a file
 *
 * @param filename input file
 * @param hash allocated buffer to store computed SHA1 as a
 *             string value.
 *
 * @return int
 */
static int GetFileSha1(const char *filename, unsigned char *hash)
{
    int err = 0, ret, i;
    SHA1_CTX context;
    unsigned char bin_hash[SHA1_DIGEST_LENGTH];
    unsigned char buf[4096];

    /* Open file */
    int fd  = open(filename, O_RDONLY);
    if (fd == -1)
    {
        ALOGE("%s: fail to open %s. %s\n", __FUNCTION__, filename, strerror(errno));
        err = 1;
    }
    else
    {
        /* Build file SHA1 */
        SHA1Init(&context);
        do
        {
            ret = read(fd, buf, 4096);
            if (ret < 0)
            {
                ALOGE("%s: %s read failure. %s\n",
                        __FUNCTION__,
                        filename,
                        strerror(errno));
                err = 1;
                break;
            }
            SHA1Update(&context, buf, ret);
        } while(ret>0);
        close(fd);
        SHA1Final(bin_hash, &context);

        /* Store it as a string value */
        for (i=0; i<SHA1_DIGEST_LENGTH; i++)
        {
            snprintf((char *)&hash[2*i], 2*SHA1_DIGEST_LENGTH, "%02x", bin_hash[i]);
        }
        hash[2*SHA1_DIGEST_LENGTH] = '\0';
    }
    return err;
}

/**
 * Build list of file(s) hash(es)
 *
 * TODAY: only modem SHA1 comparison is useful so that only
 * modem SHA1 is computed.
 *
 * TODO: update hash list with other file hash if required.
 *
 */
static int BuildHashList(HashListElem *hash_list)
{
    int err = 0;

    /* Get MDM hash */
    snprintf(hash_list[0].filename, PATH_MAX, "%s", MDM_PATH);
    if (GetFileSha1(MDM_PATH, hash_list[0].hash_str) != 0)
    {
        err++;
    }

    return err;
}

/**
 * Store list of file(s) hash(es) in file system.
 *
 */
static void StoreHashList(HashListElem *hash_list)
{
    int i;
    FILE *fp = fopen(FILE_HASHES_LIST_PART, "w");
    if (fp)
    {
        for (i=0; i<NUM_OF_SUPPORTED_FILES; i++)
        {
            /* Store hash in list... */
            fprintf(fp, "%s %s\n",
                    hash_list[0].hash_str,
                    hash_list[0].filename);
        }
        fclose(fp);
        rename(FILE_HASHES_LIST_PART, FILE_HASHES_LIST);
    }
    else
    {
        ALOGE("%s: fail to (re-)build hash list. %s",
              __FUNCTION__,
              strerror(errno));
        remove(FILE_HASHES_LIST_PART);
        remove(FILE_HASHES_LIST);
    }
}

/**
 * Compare content of FILE_HASHES_LIST with files hashes in
 * local file system.
 *
 * @return bool true if file(s) differ between FILE_HASHES_LIST
 *         and file system, false otherwise.
 */
static bool FwUpdateDone(HashListElem *hash_list)
{
    bool fw_update_done = false;
    char *line=NULL;
    size_t len = 0;
    int i = 0;

    /* Get corresponding hash from list... */
    FILE *fp = fopen(FILE_HASHES_LIST, "r");
    if (fp)
    {
        while (getline(&line, &len, fp) != -1)
        {
            if (memcmp(line, hash_list[i].hash_str, (SHA1_DIGEST_LENGTH*2)))
            {
                /* One diff is found: f/w update has been done */
                ALOGI("%s: %s differs since last start-up.",
                      __FUNCTION__,
                      hash_list[i].filename);
                fw_update_done = true;
            }
            i++;
        }
        free(line);
        fclose(fp);
    }
    else
    {
        /* Fail to get stored hashes: consider f/w update has been done...*/
        ALOGE("%s: fail to open %s. %s",
              __FUNCTION__,
              FILE_HASHES_LIST,
              strerror(errno));
        remove(FILE_HASHES_LIST);
        fw_update_done = true;
    }

    return fw_update_done;
}

/**
 * Clean modem files that may be source of backward
 * incompatibility after a f/w update:
 *
 * NRAM2 files except MEP file
 */
static void CleanModemFiles(void)
{
    struct dirent *p;
    char filename[PATH_MAX];
    DIR *d = opendir(MDM_FILES_DIR);
    if(d)
    {
        p = readdir(d);
        while(p)
        {
            if (strcmp(p->d_name, MDM_MEP_FILENAME))
            {
                /* Not a "protected" file: remove it */
                snprintf(filename, PATH_MAX, "%s/%s", MDM_FILES_DIR, p->d_name);
                remove(filename);
            }
            p = readdir(d);
        }
        closedir(d);
    }
}

void fild_CheckFwUpdateDone(void)
{
    bool fw_update_done = false;
    HashListElem hash_list[NUM_OF_SUPPORTED_FILES];

    /* Build file(s) hash(es) list */
    int err = BuildHashList(hash_list);

    if (!err)
    {
        if (access(FILE_HASHES_LIST, F_OK) == 0)
        {
            /** Hash list already stored, let's compare with new one */
            fw_update_done = FwUpdateDone(hash_list);
        }
        else
        {
            /** No hash list: consider f/w update has been done. */
            ALOGI("%s: no f/w hash list stored since last start-up", __FUNCTION__);
            fw_update_done = true;
        }
    }
    else
    {
        ALOGE("%s: fail to build hash list.", __FUNCTION__);
        fw_update_done = true;
    }

    if (fw_update_done)
    {
        if (!err)
        {
            /* Store hash list in file system */
            StoreHashList(hash_list);
        }

        /* Clean some modem files */
        CleanModemFiles();
    }
}
