/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @file icera-ril-fwupdate.c RIL modem firmware update utilities
 *
 * Utilities to handle modem files update.
 *
 * The list of supported modem files is defined in below
 * arch_desc table: no other file handled by this code.
 *
 * On dual flash platforms:
 * ########################
 * At really 1st boot after Android image programming,
 * comparison is done between modem files stored in Android
 * local file system and corresponding files in modem flash file
 * system.
 * Comparison based on hash (SHA1) of each file.
 * If some file differs, it is appended to a "update list" that
 * will be used by triggered icera-loader service to download
 * new file(s) in modem flash file system.
 *
 * If no file differ (and it is the same at next reboots), list
 * of hashes of all modem files found in local file system is
 * stored in file system in order to avoid communicating with
 * modem for next file(s) comparisons.
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "icera-ril.h"
#include "atchannel.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
/* open, ...:*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* bool */
#include <stdbool.h>

/* SHA1Init...: */
#include <sys/sha1.h>

#include <cutils/properties.h> /* property_get,... */
#define LOG_TAG rilLogTag
#include <utils/Log.h> /* ALOGI,...*/

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/
/* Default folder to look for archive file(s) during f/w update checks */
#define DEFAULT_BB_FILES_FOLDER "/data/rfs/data/debug/fw_update"

/* Default folder to find files in system image */
#define SYSTEM_BB_FILES_FOLDER "/system/vendor/firmware/icera"

/* Files created by RIL during modem f/w update */
#define FILE_HASHES_LIST "/data/rfs/data/debug/update_ind"
#define FILE_HASHES_LIST_PART FILE_HASHES_LIST".part"
#define UPDATE_FILE_LIST "/data/rfs/data/debug/update_list"
#define UPDATE_FILE_LIST_PART UPDATE_FILE_LIST".part"

/* Num of supported modem archive: 8
   This includes:
    MDM
    BT2
    LDR
    IMEI
    AUDIOCFG
    UNLOCK
    DEVICECFG
    PRODUCTCFG*/
#define MAX_SUPPORTED_ARCH 8

/* Max len for fwid str */
#define MAX_FWID_LEN 512

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/
/**
 * Structure used to store a file hash information
 */
typedef struct
{
    char name[PATH_MAX];          /* File name */
    char hash[SHA1_DIGEST_LENGTH*2+1];  /* File hash */
}HashFileInfos;

/**
 * Structure used to describe supported archive data
 */
typedef struct
{
    const char *acr;    /* Archive acronym for AT%IGETARCHDIGEST usage */
    const char *path;   /* Archive full path in local file system */
    const char *name;   /* Archive name in local file system */
}ArchDesc;

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/
/**
 * All modem archive are supported for firmware update are indicated here.
 */
static ArchDesc arch_desc[MAX_SUPPORTED_ARCH] =
{
    /* modem appli: */
    {
        .acr = "MDM",
        .path = DEFAULT_BB_FILES_FOLDER"/modem.wrapped",
        .name = "modem.wrapped",
    },
    /* secondary boot appli */
    {
        .acr = "BT2",
        .path = DEFAULT_BB_FILES_FOLDER"/secondary_boot.wrapped",
        .name = "secondary_boot.wrapped",
    },
    /* loader appli */
    {
        .acr = "LDR",
        .path = DEFAULT_BB_FILES_FOLDER"/loader.wrapped",
        .name = "loader.wrapped",
    },
    /* imei file */
    {
        .acr = "IMEI",
        .path = DEFAULT_BB_FILES_FOLDER"/imei.bin",
        .name = "imei.bin",
    },
    /* audio config file */
    {
        .acr = "AUDIOCFG",
        .path = DEFAULT_BB_FILES_FOLDER"/audioConfig.bin",
        .name = "audioConfig.bin",
    },
    /* unlock certificate */
    {
        .acr = "UNLOCK",
        .path = DEFAULT_BB_FILES_FOLDER"/unlock.bin",
        .name = "unlock.bin",
    },
    /* device config file */
    {
        .acr = "DEVICECFG",
        .path = DEFAULT_BB_FILES_FOLDER"/deviceConfig.bin",
        .name = "deviceConfig.bin",
    },
    /* product config file */
    {
        .acr = "PRODUCTCFG",
        .path = DEFAULT_BB_FILES_FOLDER"/productConfig.bin",
        .name = "productConfig.bin",
    }
};

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/
/* Check file existence... */
static bool fileExist(const char *filename)
{
    return access(filename, F_OK) == 0;
}

/**
 * Get hash of a file stored in local file system
 *
 * @param filename file path in local file system
 * @param hash buffer to store hash
 *
 * @return int 1 if hash available, 0 if not
 */
static int getLocalHash(const char *filename, char *hash)
{
    int ok = 1, ret;
    SHA1_CTX context;
    unsigned char bin_hash[SHA1_DIGEST_LENGTH];
    unsigned char buf[4096];

    /* Open file */
    int fd  = open(filename, O_RDONLY);
    if (fd == -1)
    {
        ALOGE("%s: fail to open %s. %s\n", __FUNCTION__, filename, strerror(errno));
        ok = 0;
    }
    else
    {
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
                ok = 0;
                break;
            }
            SHA1Update(&context, buf, ret);
        } while(ret>0);
        close(fd);
        SHA1Final(bin_hash, &context);

        /* Store it as a string value */
        int i;
        for (i=0; i<SHA1_DIGEST_LENGTH; i++)
        {
            sprintf((char *)&hash[2*i], "%02x", bin_hash[i]);
        }
        hash[2*SHA1_DIGEST_LENGTH] = '\0';
    }
    return ok;
}

/**
 * Get hash of a file stored in modem file system
 *
 * @param acr acronym of file in arch_desc table
 * @param hash buffer to store hash if found
 *
 * @return int 1 if modem hash available, 0 if not.
 */
static int getModemHash(const char *acr, char *hash)
{
    int ok = 0;
    char command[64];
    sprintf(command, "AT%%IGETARCHDIGEST=\"%s\"\r\n", acr);
    ATResponse *p_response = NULL;
    int err = at_send_command_singleline(command,"%IGETARCHDIGEST:", &p_response);
    if ((err == 0) && (p_response->success == 1))
    {
        /* isolate ":" */
        char *line = strstr(p_response->p_intermediates->line, ":");
        /* the 2 1st char are ": " and then a 40bytes str of SHA1 value */
        memcpy(hash, &line[2], SHA1_DIGEST_LENGTH*2);
        hash[SHA1_DIGEST_LENGTH*2] = '\0';
        ok = 1;
    }
    at_response_free(p_response);
    p_response = NULL;

    return ok;
}

static const char *getFileAcr(char *path)
{
    int i;

    for (i=0; i<MAX_SUPPORTED_ARCH; i++)
    {
        if (strcmp(arch_desc[i].path, path) == 0)
        {
            return arch_desc[i].acr;
        }
    }
    ALOGE("%s: no ACR for %s", __FUNCTION__, path);
    return NULL;
}

/**
 * Compare HashFileInfos between 2 hash list:
 * - new_hash_list that may be the list of hash of files found
 *   directly in local file system
 * - current_hash_list that may be either the list of hash(es)
 *   stored as FILE_HASHES_LIST in local file system after
 *   previous successfull f/w update, or the list of hashes of
 *   files found directly in modem file system.
 *
 * Doing the comparison, each time a diff is found, local file
 * path from new_hash_list is stored in a list. This diff_list
 * can be used to know which file differ since previous modem
 * reboot or directly as UPDATE_FILE_LIST for f/w
 * update.
 *
 * @param diff_list
 * @param new_hash_list
 * @param new_count
 * @param current_hash_list
 * @param current_count
 *
 * @return int size of diff_list data or -1 in case of error.
 */
static int compareHashLists(char *diff_list,
                            HashFileInfos *new_hash_list,
                            int new_count,
                            HashFileInfos *current_hash_list,
                            int current_count)
{
    int i, count=0, size=0, len=0, ok;
    char modem_hash[SHA1_DIGEST_LENGTH*2+1];

    /* For all supported files locally found in file system... */
    for (i=0; i<new_count; i++)
    {
        int update = 0;
        if (current_count > 0)
        {
            /* Do compare with hash of files used to do previous f/w
               update.

               Assumption here is that previous f/w update was successfull
               so that files in current_hash_list match files that can be found
               in modem file system. */
            int j, found, diff;
            found = 0;
            diff = 0;
            update = 0;

            /* ... and for all supported files locally found at previous f/w update... */
            for (j=0; j<current_count; j++)
            {
                /*... look for corresponding name... */
                if (strcmp(new_hash_list[i].name, current_hash_list[j].name) == 0)
                {
                    found = 1;

                    /*... and compare hash... */
                    if (strncasecmp(new_hash_list[i].hash, current_hash_list[j].hash, SHA1_DIGEST_LENGTH*2))
                    {
                        /* ...it differs. */
                        ALOGI("%s: %s hash differs from one currently used by modem.",
                              __FUNCTION__,
                              new_hash_list[i].name);
                        diff = 1;
                    }
                    break;
                }
            }
            if (!found)
            {
                ALOGI("%s: %s added since last f/w update.",
                      __FUNCTION__,
                      new_hash_list[i].name);

                /* Let's check in modem fs if update is required for this file */
                const char *acr = getFileAcr(new_hash_list[i].name);
                if (acr)
                {
                    ok = getModemHash(acr, modem_hash);
                    ALOGD("%s: modem hash (%d): %s", __FUNCTION__, ok, modem_hash);
                    if (!ok || strncasecmp(new_hash_list[i].hash, modem_hash, SHA1_DIGEST_LENGTH*2))
                    {
                        ALOGI("%s: %s differs with modem version", __FUNCTION__, new_hash_list[i].name);
                        count++;
                        update = 1;
                    }
                    else
                    {
                        ALOGI("%s: %s already programmed in modem file system", __FUNCTION__, new_hash_list[i].name);
                    }
                }
            }
            else if (found && diff)
            {
                count++;
                update = 1;
            }
        } else {
            /* No modem hash [retrieved], may be stuck in loader => full update */
            update = 1;
        }

        if (update)
        {
            /* Add this file to f/w diff list */
            if (count == 1)
            {
                /* 1st file in list... */
                len = snprintf(diff_list, strlen(new_hash_list[i].name)+1, "%s", new_hash_list[i].name);
            }
            else
            {
                /* Not the 1st file in list... */
                len = snprintf(diff_list+size, strlen(new_hash_list[i].name)+2, ";%s", new_hash_list[i].name);
            }
            if (len>0)
            {
                size += len;
            }
            else
            {
                ALOGE("%s: error building diff list.", __FUNCTION__);
                size = -1;
                break;
            }
        }
    }

    return size;
}

/**
 * Extract data from file into HashFileInfos table
 *
 * @param filename file to look for hash infos
 * @param hash_list table to store hash infos
 * @param max_elems max num of elem in above table
 *
 * @return int num of hash info elems found in filename
 */
static int extractFileHashList(const char *filename, HashFileInfos *hash_list, int max_elems)
{
    int count=0;
    FILE *fp = fopen(filename, "r");
    if (fp)
    {
        char *line = NULL;
        size_t len = 0, read_char;
        while (((read_char = getline(&line, &len, fp)) != (size_t)-1) && (count < max_elems))
        {
            /* extract hash value*/
            strncpy(hash_list[count].hash, line, SHA1_DIGEST_LENGTH*2);
            hash_list[count].hash[SHA1_DIGEST_LENGTH*2] = '\0';

            /* extract file path */
            line[read_char-1]='\0';
            strncpy(hash_list[count].name, line+(SHA1_DIGEST_LENGTH*2+1), (read_char > PATH_MAX) ? PATH_MAX:read_char);
            ALOGD("%s: hash: %s, name:%s .", __FUNCTION__, hash_list[count].hash, hash_list[count].name);
            count++;
        }
        free(line);
    }
    else
    {
        ALOGE("%s: fail to open %s. %s",
              __FUNCTION__,
              filename,
              strerror(errno));
    }
    return count;
}


/**
 * Build a HashFileInfos table of all supported files found in
 * modem file system.
 *
 * @param hash_list table to store hash infos
 * @param max_elems max num of elem in above table
 *
 * @return int num of supported files found in modem file system
 */
static int buildModemHashList(HashFileInfos *hash_list, int max_elems)
{
    int i, ok, count=0;
    char hash[SHA1_DIGEST_LENGTH*2];

    for (i=0; i<max_elems; i++)
    {
        strcpy(hash_list[count].name, arch_desc[i].path);
        ok = getModemHash(arch_desc[i].acr, hash_list[count].hash);
        if (ok)
        {
            ALOGD("%s: hash: %s, name:%s .", __FUNCTION__, hash_list[count].hash, hash_list[count].name);
            count++;
        }
    }
    return count;
}

/**
 * Build a list of hashes of all supported files found in local
 * file system.
 *
 * @param hash_list table to store hash infos
 * @param max_elems max num of elem in above table
 *
 * @return int num of supported files found in local file
 *         system, -1 in case of error.
 */
static int buildLocalHashList(HashFileInfos *hash_list, int max_elems)
{
    int i, count = 0, err = 0;

    for (i= 0; i<max_elems; i++)
    {
        if (fileExist(arch_desc[i].path))
        {
            /* Get file name */
            snprintf(hash_list[count].name, PATH_MAX, "%s", arch_desc[i].path);

            /* Get file hash */
            if (getLocalHash(arch_desc[i].path, hash_list[count].hash) == 0)
            {
                ALOGE("%s: error building hash for %s",
                      __FUNCTION__,
                      arch_desc[i].path);
                err++;
            }
            else
            {
                ALOGD("%s: hash: %s, name:%s .", __FUNCTION__, hash_list[count].hash, hash_list[count].name);
            }
            count++;
        }
    }
    return err ? -1:count;
}

/**
 * Store a hash infos table in file system.
 *
 * @param filename file to store data
 * @param tmp_name tmp file to store data
 * @param hash_list hash data table
 * @param num_elems num of elems in above table
 */
static void storeLocalHashList(const char *filename,
                               const char *tmp_name,
                               HashFileInfos *hash_list,
                               int num_elems)
{
    FILE *fp = fopen(tmp_name, "w");
    if (fp)
    {
        int i, err=0;;
        for (i=0; i<num_elems; i++)
        {
            if (fprintf(fp, "%s %s\n", hash_list[i].hash, hash_list[i].name) < 0)
            {
                ALOGE("%s: fail to write %s. %s",
                      __FUNCTION__,
                      tmp_name,
                      strerror(errno));
                err =1 ;
                break;
            }
        }
        fclose(fp);
        if (err)
        {
            remove(tmp_name);
        }
        else
        {
            rename(tmp_name, filename);
        }
    }
    else
    {
        ALOGE("%s: error opening %s. %s",
              __FUNCTION__,
              tmp_name,
              strerror(errno));
    }
}

/**
 * Store a list of file(s) to use for f/w update.
 *
 * @param filename file to store data
 * @param tmp_name tmp file to store data
 * @param update_file_list list of data
 * @param size size of data
 */
static void storeUpdateList(const char *filename,
                            const char *tmp_name,
                            char *update_file_list,
                            int size)
{
    int i;
    FILE *fp = fopen(tmp_name, "w");
    if (fp)
    {
        int ret = fwrite(update_file_list, size, 1, fp);
        fclose(fp);
        if (ret != 1)
        {
            ALOGE("%s: fail to write %s. %s",
                  __FUNCTION__,
                  tmp_name,
                  strerror(errno));
            remove(tmp_name);
        }
        else
        {
            rename(tmp_name, filename);
        }
    }
    else
    {
        ALOGE("%s: error opening %s. %s",
              __FUNCTION__,
              tmp_name,
              strerror(errno));
    }
}

/**
 * Ask BBC for supported FWID sending AT%IGETFWID.
 * BBC supposed to get value into product config file
 *
 * @param fwid buffer to strore fwid value
 *
 * @return int 1 if OK, 0 if KO
 */
static int getModemFwid(char *fwid)
{
    int ok = 0;
    ATResponse *p_response = NULL;
    int err = at_send_command_singleline("AT%IGETFWID\r\n","%IGETFWID:", &p_response);
    if ((err == 0) && (p_response->success == 1))
    {
        /* isolate ":" */
        char *line = strstr(p_response->p_intermediates->line, ":");
        if (line && (strlen(line) > 2))
        {
            /* the 2 1st char are ": " and then a str for fwid value */
            strncpy(fwid, &line[2], MAX_FWID_LEN);
            ALOGI("%s: %s", __FUNCTION__, fwid);
            ok = 1;
        }
        else
        {
            ALOGE("%s: no fwid on the modem side", __FUNCTION__);
        }
    }
    else
    {
        ALOGE("%s: fwid feature not supported by modem", __FUNCTION__);
    }
    at_response_free(p_response);
    p_response = NULL;

    return ok;

}

/**
 * Fill DEFAULT_BB_FILES_FOLDER with links to files to use for
 * f/w udpate checks.
 *
 * 1st get platform fwid to look for specific files
 * Then look in SYSTEM_BB_FILES_FOLDER if file not found.
 *
 */
static void fillFwFolder(void)
{
    int i, found;
    char path[PATH_MAX];
    char lpath[PATH_MAX];
    char fwid[MAX_FWID_LEN];

    /* Create empty dest firmware folder */
    system("rm -rf "DEFAULT_BB_FILES_FOLDER"&& mkdir "DEFAULT_BB_FILES_FOLDER);

    /* Get modem fw ID */
    if (getModemFwid(fwid))
    {
        for (i=0; i<MAX_SUPPORTED_ARCH; i++)
        {
            /* If a file is found, we will link it in dest */
            found = 1;
            snprintf(lpath, PATH_MAX, "%s",
                     arch_desc[i].path);

            /* Look for file for this fwid in dedicated <fwid> folder */
            snprintf(path, PATH_MAX, "%s/%s/%s",
                     SYSTEM_BB_FILES_FOLDER,
                     fwid,
                     arch_desc[i].name);
            if (access(path, F_OK) != 0)
            {
                ALOGE("%s: [%s] not found", __FUNCTION__, path);
                /* If not found, look for it in default system folder */
                snprintf(path, PATH_MAX, "%s/%s",
                         SYSTEM_BB_FILES_FOLDER,
                         arch_desc[i].name);
                if (access(path, F_OK) != 0)
                {
                    /* This file not found... */
                    found = 0;
                }
            }

            if (found)
            {
                /* Link it in dest folder. */
                if (symlink(path, lpath))
                {
                    ALOGE("%s: fail symlink from %s to %s. %s",
                          __FUNCTION__,
                          path,
                          lpath,
                          strerror(errno));
                }
            }
        }
    }
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
void HandleModemFirmwareUpdate(void)
{
    char prop[PROPERTY_VALUE_MAX];

    /* A platform not running fild: need to handle modem flash file system update */
    if (!property_get("gsm.modem.fild.args", prop, ""))
    {
        int do_update = false;
        int size = 0;
        HashFileInfos new_hash_list[MAX_SUPPORTED_ARCH];
        char update_file_list[MAX_SUPPORTED_ARCH*(PATH_MAX+1)];
        int new_count;

        /* Get all files in DEFAULT_BB_FILES_FOLDER */
        fillFwFolder();

        /* Build list of hash for all supported files found in local
         * file system */
        new_count = buildLocalHashList(new_hash_list, MAX_SUPPORTED_ARCH);
        if (new_count > 0)
        {
            /* Will compare with corresponding files found in modem file system
               and build a list of file to use for update if any file differ */
            HashFileInfos current_hash_list[MAX_SUPPORTED_ARCH];
            int current_count;
            if (access(FILE_HASHES_LIST, F_OK) == 0)
            {
                /* Extract current hash infos list from file system */
                current_count = extractFileHashList(FILE_HASHES_LIST, current_hash_list, MAX_SUPPORTED_ARCH);
            }
            else
            {
                /* Build current hash infos list from modem file system */
                current_count = buildModemHashList(current_hash_list, MAX_SUPPORTED_ARCH);
            }

            /* Do per file hash comparison... */
            size = compareHashLists(update_file_list,
                                    new_hash_list,
                                    new_count,
                                    current_hash_list,
                                    current_count);
        }
        else if (new_count == -1)
        {
            size = -1;
        }
        else
        {
            ALOGW("%s: no file found in local file system", __FUNCTION__);
        }

        if (size > 0)
        {
            ALOGI("%s: modem firware update required.", __FUNCTION__);

            /* Store update list */
            ALOGD("%s: file update list: %s", __FUNCTION__, update_file_list);
            storeUpdateList(UPDATE_FILE_LIST,
                            UPDATE_FILE_LIST_PART,
                            update_file_list,
                            size);

            /* Remove the obsolete hashes list, if any */
            ALOGD("Update required, remove hashes list");
            remove(FILE_HASHES_LIST);

            /* Start icera-loader service */
            property_set("gsm.modem.update","1");

            /* RIL will be stopped to allow f/w update: wait... */
            sleep(5);
        }
        else if (size == 0)
        {
            if (new_count > 0)
            {
                ALOGI("%s: modem firware up to date.", __FUNCTION__);

                /* Store new hash list: at next start-up it will be used for f/w update
                   detection */
                storeLocalHashList(FILE_HASHES_LIST,
                                   FILE_HASHES_LIST_PART,
                                   new_hash_list,
                                   new_count);
            }

            /* Remove the obsolete update list, if any */
            remove(UPDATE_FILE_LIST);
        }
        else
        {
            ALOGE("%s: fail.", __FUNCTION__);

            /* Remove obsolete lists, if any... */
            remove(UPDATE_FILE_LIST);
            remove(FILE_HASHES_LIST);
        }
    }
    return;
}

void ForceModemFirmwareUpdate(void)
{
    ALOGI("Force firmware update by removing hashes list");
    remove(FILE_HASHES_LIST);
}
