/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All Rights Reserved.
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "icera-util.h"
#include "icera-util-local.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>  /* DIR,... */
#include <stdlib.h>

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/
/**
 *  Dated storage stuff
 *
 *  Dir is always with following format:
 *  <prefix><idx><date>
 *  This to allow easier generic parsing of files in different
 *   fs directories.
 *
 */
#define LOG_DIR_FORMAT   "%s_%d_%s"  /** prefixstr_idx_yyyymmdd_hhmmss */

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/
static int Remove(const char *pathname);

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/
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
    char *buf;
    size_t len, path_len = strlen(path), sep_len = strlen("/");

    DIR *d = opendir(path);
    if(d)
    {
        p = readdir(d);
        while(p)
        {
            if (strcmp(p->d_name, ".") && strcmp(p->d_name, ".."))
            {
                asprintf(&buf, "%s/%s", path, p->d_name);
                err = Remove(buf);
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

/**
 * Remove a file or a directory
 *
 * @param pathname
 *
 * @return int 0 on success, -1 on failure and errno is set
 *         appropriately.
 */
static int Remove(const char *pathname)
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

/**
 * Scan dir and look forward directories with given
 * LOG_DIR_FORMAT and remove older one if storage limit is reached.
 *
 * @param data
 * @return int next idx to use in folder naming
 */
static int CheckLimit(LoggingData *data)
{
    char *older = NULL;
    char tmp[PATH_MAX];
    int max_idx = 0;
    DIR *dp = opendir(data->dir);
    if(dp)
    {
        int num_of_file = 0;
        int idx, min_idx;
        struct dirent *dentry = readdir(dp);
        while(dentry)
        {
            if(strncmp(dentry->d_name, data->prefix, strlen(data->prefix)) == 0)
            {
                num_of_file++;
                if (sscanf(dentry->d_name, "%[^_]_%d", tmp, &idx) != EOF)
                {
                    if (!older)
                    {
                        min_idx = idx;
                        max_idx = idx;
                        older = dentry->d_name;
                    }
                    else
                    {
                        /* Assumption: will never wrap... */
                        if (idx < min_idx)
                        {
                            min_idx = idx;
                            older = dentry->d_name;
                        }
                        max_idx = (idx > max_idx) ? idx:max_idx;
                    }
                }
                else
                {
                    ALOGW("%s: fail to extract infos from %s. %s",
                          __FUNCTION__,
                          dentry->d_name,
                          strerror(errno));
                }
            }
            dentry = readdir(dp);
        }
        closedir(dp);
        if((num_of_file >= data->max) && older)
        {
            /* Remove older dump... */
            char name[PATH_MAX];
            snprintf(name, PATH_MAX, "%s/%s", data->dir, older);
            ALOGD("%s: Removing %s.", __FUNCTION__, name);
            if(Remove(name) == -1)
            {
                ALOGE("%s: Fail to remove %s. %s",
                      __FUNCTION__,
                      name,
                      strerror(errno));
            }
        }
    }
    return max_idx+1;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
void IceraLogStoreModemLoggingData(char *src_dir, char *dst_dir)
{
    /* Make hardlinks of existing logging data from src_dir to dst_dir.
     * This today apply only for deferred logging data and .iom file will
     * continue to be updated until it reaches the size limit... */
    char *cmd;
    int index = 0;
    DIR *dp = opendir(src_dir);
    if(dp)
    {
        struct dirent *dentry = readdir(dp);
        while(dentry)
        {
            if(strncmp(dentry->d_name, LOGGING_DEFERRED_PREFIX, strlen(LOGGING_DEFERRED_PREFIX)) == 0)
            {
                index++;
                asprintf(&cmd,
                        "ln %s/%s %s/%s",
                        src_dir,
                        dentry->d_name,
                        dst_dir,
                        dentry->d_name);
                ALOGD("%s: %s", __FUNCTION__, cmd);
                int err = system(cmd);
                free(cmd);
                if (err)
                {
                    ALOGE("%s: hardlink failure.", __FUNCTION__);
                }
            }
            dentry = readdir(dp);
        }
        closedir(dp);
    }
}

void IceraLogGetTimeStr(char *str, size_t len)
{
    struct tm *local;
    time_t t;

    t = time(NULL);
    local = localtime(&t);

    /* FIXME size should be passed as argument */
    snprintf(str, len,
            "%04d%02d%02d_%02d%02d%02d",
            (1900+local->tm_year),
            (local->tm_mon+1),
            local->tm_mday,
            local->tm_hour,
            local->tm_min,
            local->tm_sec);
    return;
}

int IceraLogNewDatedDir(LoggingData *data, char *time_str, char *dir)
{

    /* Remove older directory if needed and get idx for new one... */
    int new_idx = CheckLimit(data);

    /* Create new dated directory */
    snprintf(dir, PATH_MAX, "%s/%s_%d_%s", data->dir, data->prefix, new_idx, time_str);
    if(mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
    {
        ALOGE("%s: fail to create dir [%s]. %s", __FUNCTION__, dir, strerror(errno));
        return -1;
    }

    /* Change symlink to latest new dated directory... */
    char new_link[PATH_MAX];
    int err=0;
    struct stat fstat;
    snprintf(new_link, PATH_MAX, "%s/latest_%s", data->dir, data->prefix);
    err = lstat(new_link, &fstat);
    if(!err)
    {
        if (remove(new_link))
        {
            ALOGE("%s: [%s] remove failure. %s",
                  __FUNCTION__,
                  new_link,
                  strerror(errno));
            return -1;
        }
    }

    if (symlink(dir, new_link))
    {
        ALOGE("%s: [%s] to [%s] symlink failure. %s",
              __FUNCTION__,
              dir,
              new_link,
              strerror(errno));
        return -1;
    }

    return 0;
}

void IceraLogStore(
        LoggingType type,
        char *dir,
        char *time_str)
{
    /* Store values to share with icera-crashlogs binary */
    if (type == COREDUMP_LOADER)
    {
        property_set("gsm.modem.crashlogs.type", "COREDUMP_LOADER");
    }
    else if (type == COREDUMP_MODEM)
    {
        property_set("gsm.modem.crashlogs.type", "COREDUMP_MODEM");
    }
    else
    {
        /* Any other type is generic for icera-crashlogs... */
        property_set("gsm.modem.crashlogs.type", "GENERIC");
    }
    if (dir)
    {
        property_set("gsm.modem.crashlogs.directory", dir);
    }
    if (time_str)
    {
        property_set("gsm.modem.crashlogs.timestamp", time_str);
    }

    /** Start icera-crashlogs to store additional AP logs:
     * On some platforms, in case of crash, starting icera-crashlogs
     * may trigger a stop of RIL daemon, so makes the diff here between
     * call after logging and call after coredump retreival. */
    if (type == MODEM_LOGGING)
    {
        property_set("gsm.modem.logging", "1");
    }
    else
    {
        property_set("gsm.modem.crashlogs", "1");
    }
}
