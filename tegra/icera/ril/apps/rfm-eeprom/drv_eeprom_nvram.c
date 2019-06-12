/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 ************************************************************************************************/

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "drv_m24m01.h"
#include "drv_eeprom_nvram.h"

#define LOG_TAG "RFEEPROM"
#include <utils/Log.h>


/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

/* EEPROM address can be adjusted per board but is currently always been fixed as 0x3 */
#define EEPROM_I2C_ADDRESS      0x3

#define EEPROM_CAL_0_HDR_OFFSET 0x0
#define EEPROM_BLOCK_SIZE       M24M01_PAGE_SIZE
#define EEPROM_DATA_OFFSET      EEPROM_BLOCK_SIZE
#define EEPROM_SIZE             M24M01_DEVICE_SIZE

/* Only 200 chars instead of the max of 235 just in case we need to put */
/*   other stuff in this header later. */
#define EEPROM_HDR_FILENAME_MAX  200

#define EEPROM_HDR_CHECKWORD    0x1ce0001
#define EEPROM_HDR_VERSION      0x2
#define EEPROM_HDR_VER_WCHKSUM  0x2

/** TMP cal file read from eeprom.
 *  NOTE: this file must be in the same physical partition of
 *  the final cal file in order to avoid rename error. By
 *  default, set TMP_CAL_FILE in the same folder of real cal
 *  file. */
#define TMP_CAL_FILE "/data/rfs/data/factory/tmp_cal.bin"

typedef struct
{
    /**
     * Structure checkword
     */
    uint32_t checkword;

    /**
     * Structure version number
     */
    uint32_t version;

    /**
     * Data start absolute address & length (0 if not present)
     */
    uint32_t data_addr;
    uint32_t length;

    /**
     * Simple checksum for EEPROM contents
     */
    uint32_t checksum;

    /**
     * Unique identifying name for this EEPROM "file".
     */
    char filename[EEPROM_HDR_FILENAME_MAX+1];

} drv_eeprom_header;

/* Mapping from os-abs to linux */
#define DEV_ASSERT(a)             assert(a)
#define COM_MIN(X,Y)              ((X) < (Y) ? (X) : (Y))

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

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 * Write EEPROM header page containing file details
 * @param hdr_offset        EEPROM address of header.
 * @param eeprom_filename   Filename.
 * @param data_addr         Starting offset of file.
 * @param length            File length (or 0 if none).
 * @param checksum          Checksum of contents.
 *
 * @return            0 if ok, != 0 if failed
 */
static int eeprom_write_header(drv_M24M01Handle_t * handle,
                               uint32_t             hdr_offset,
                               const char         * eeprom_filename,
                               uint32_t             data_addr,
                               uint32_t             length,
                               uint32_t             checksum)
{
    drv_eeprom_header hdr;

    /* Initial header structure */
    hdr.version     = EEPROM_HDR_VERSION;
    hdr.data_addr   = data_addr;
    hdr.length      = length;
    hdr.checksum    = checksum;

    /* Detect if invalidating a header by looking for empty file. */
    hdr.checkword = (length == 0) ? ~EEPROM_HDR_CHECKWORD : EEPROM_HDR_CHECKWORD;

    strncpy(hdr.filename, eeprom_filename, EEPROM_HDR_FILENAME_MAX); // EEPROM_HDR_FILENAME_MAX == 200, max hdr.filename[201]
    hdr.filename[EEPROM_HDR_FILENAME_MAX] = '\0';

    int ret = drv_M24M01WriteBlock(handle, hdr_offset, (uint8_t*)&hdr, sizeof(drv_eeprom_header));
    return ret;
}

/**
 * Fill header structure.
 * @param hdr         Header structure to fill.
 *
 * @return            1 if valid, 0 if not
 */
static int eeprom_read_header(drv_M24M01Handle_t *handle, uint32_t hdr_offset, drv_eeprom_header *hdr)
{

    /* Read and fill header from EEPROM. */
    int ok = (drv_M24M01ReadBlock(handle, hdr_offset, (uint8_t *)hdr, sizeof(drv_eeprom_header)) == 0);

    /* Check header details. */
    ok &= (hdr->checkword == EEPROM_HDR_CHECKWORD);
    ok &= (hdr->version <= EEPROM_HDR_VERSION);

    /* Copy in the calibration file name to allow migration. */
    if (ok && (hdr_offset == EEPROM_CAL_0_HDR_OFFSET) && ((hdr->filename[0] == '\0') || (hdr->filename[0] == '\xff')))
    {
        strncpy(hdr->filename, CAL_0_FILENAME, EEPROM_HDR_FILENAME_MAX);
    }
    else if (ok && (hdr->filename[0] == '\xff'))
    {
        hdr->filename[0] = '\0';
    }
    hdr->filename[EEPROM_HDR_FILENAME_MAX] = '\0';

    return ok;
}

/**
 * Get the next header offset, from the current header.
 */
static uint32_t eeprom_get_next_header_offset(uint32_t hdr_offset, drv_eeprom_header * hdr)
{
    /*   This header,                unused space, file data. */
    hdr_offset = hdr->data_addr + hdr->length;

    /* Headers are always aligned to blocks. */
    /* Align returned header offset to next block. */
    if (hdr_offset % EEPROM_BLOCK_SIZE)
    {
        hdr_offset += EEPROM_BLOCK_SIZE;
        hdr_offset -= (hdr_offset % EEPROM_BLOCK_SIZE);
    }

    return hdr_offset;
}

/**
 * Find the header offset of a given filename.
 *
 * @return 1 if found, 0 if not found.
 *         The provider hdr is filled.
 */
static int eeprom_find_header(drv_M24M01Handle_t *handle, const char *eeprom_filename, drv_eeprom_header *hdr)
{
    /* Walk though the filesystem chain of headers looking for a matching filename. */
    int ret = 0;

    /* Index to keep track of where in EEPROM we are. */
    uint32_t addr = EEPROM_CAL_0_HDR_OFFSET; /* CAL_0 file is always first. */
    while (addr < (EEPROM_SIZE - EEPROM_BLOCK_SIZE))
    {
        /* Read in this header. */
        /* If this shows an invalid header then immediately return error as the */
        /*   end of the chain has been reached. */
        if (!eeprom_read_header(handle, addr, hdr))
        {
            break;
        }

        /* Compare the filename. */
        if (strncmp(eeprom_filename, hdr->filename, EEPROM_HDR_FILENAME_MAX) == 0)
        {
            ret = 1;
            break;
        }

        /* Move EEPROM index onto the next header.*/
        addr = eeprom_get_next_header_offset(addr, hdr);
    }

    return ret;
}

/**
 * Calculate a simple checksum (Fletcher 16).
 */
static uint32_t eeprom_checksum(uint32_t last, uint8_t* data, int count)
{
    int i;
    uint16_t sum1;
    uint16_t sum2;

    sum1 = (last >> 16) & 0xFFFF;
    sum2 = (last >> 0) & 0xFFFF;

    for (i = 0; i < count; i++)
    {
        sum1 = (sum1 + data[i]) % 0xFF;
        sum2 = (sum2 + sum1) % 0xFF;
    }

    return (((uint32_t)sum2) << 16) | sum1;
}

/**
 * Copy calibration file to EEPROM
 *
 * @return            0 if ok, != 0 if failed
 */
static int eeprom_copy_to(drv_M24M01Handle_t *handle, uint32_t hdr_offset, const char * fs_filename, const char * eeprom_filename)
{
    int err = 0;

    /* Try and open user specified file */
    int fd = open(fs_filename, O_RDONLY);
    if(fd >= 0)
    {
        uint8_t   buffer[EEPROM_BLOCK_SIZE];
        uint32_t  eoffset = 0;
        uint32_t  data_addr = hdr_offset + EEPROM_DATA_OFFSET;
        uint32_t  data_n_bytes = 0;
        int       bytes_read;
        uint32_t  calc_checksum = 0;

        /* Invalidate current EEPROM header in-case of write failure during write */
        err = eeprom_write_header(handle, hdr_offset, "", 0, 0, 0);
        if (!err)
        {
            /* Copy CAL file from flash to EEPROM */
            do
            {
                bytes_read = read(fd, (char *)buffer, EEPROM_BLOCK_SIZE);
                if(bytes_read > 0)
                {
                    err = drv_M24M01WriteBlock(handle, data_addr + eoffset, buffer, bytes_read);
                    DEV_ASSERT(!err);

                    /* Calculate a simple checksum of contents */
                    calc_checksum = eeprom_checksum(calc_checksum, buffer, bytes_read);

                    eoffset += bytes_read;
                    data_n_bytes += bytes_read;
                }
            }
            while (bytes_read && !err);
        }

        close(fd);
        fd = -1;

        /* Update EEPROM header if the file is not empty and the data has been */
        /*   copied without error. */
        if (eoffset > 0 && !err)
        {
            /* Invalidate the next block to make erase safe. */
            if(eoffset%EEPROM_BLOCK_SIZE)
            {
                /* Header must always be aligned on EEPROM_BLOCK_SIZE boundary */
                eoffset = eoffset + (EEPROM_BLOCK_SIZE-(eoffset%EEPROM_BLOCK_SIZE));
            }
            eeprom_write_header(handle, data_addr + eoffset, "", 0, 0, 0);

            /* Update header block with file details */
            err = eeprom_write_header(
                                      handle,           /* drv_M24M01Handle_t * handle */
                                      hdr_offset,       /* uint32_t             hdr_offset */
                                      eeprom_filename,  /* const char         * eeprom_filename */
                                      data_addr,        /* uint32_t             data_addr */
                                      data_n_bytes,     /* uint32_t             length */
                                      calc_checksum     /* uint32_t             checksum */
                                     );
        }
        else
        {
            /* Invalidate contents of EEPROM */
            eeprom_write_header(handle, hdr_offset, "", 0, 0, 0);
            err = 1;
        }
    }
    else
    {
        err = 1;
        fprintf(stderr, "EEPROM: Could not open file for reading\n");
    }

    return err;
}

/**
 * Copy calibration file from EEPROM to FS
 *
 * @return            0 if ok, != 0 if failed
 */
static int eeprom_copy_from(drv_M24M01Handle_t *handle, const char * eeprom_filename, const char *fs_filename)
{
    int err = 0;
    uint8_t   buffer[EEPROM_BLOCK_SIZE];
    uint32_t  offset = 0;
    int       bytes_to_write;
    int       bytes_written;
    drv_eeprom_header hdr;
    uint32_t  calc_checksum = 0;

    /* EEPROM has CAL file? */
    if (eeprom_find_header(handle, eeprom_filename, &hdr))
    {
        /* Try and create a file */
        int fd = open(fs_filename, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd >= 0)
        {
            /* Compare contents of EEPROM to CAL file */
            do
            {
                bytes_to_write = COM_MIN(EEPROM_BLOCK_SIZE, (hdr.length - offset));

                /* Read block from EEPROM */
                err = drv_M24M01ReadBlock(handle, hdr.data_addr + offset, buffer, bytes_to_write);
                if (!err)
                {
                    /* Write block to file */
                    bytes_written = write(fd, buffer, bytes_to_write);
                    DEV_ASSERT(bytes_written == bytes_to_write);

                    /* Calculate a simple checksum of contents */
                    calc_checksum = eeprom_checksum(calc_checksum, buffer, bytes_to_write);

                    offset += bytes_written;
                }
            }
            while ((hdr.length - offset) > 0 && !err);

            close(fd);
        }
        else
        {
            err = 1;
            fprintf(stderr, "EEPROM: Could not open file for writing\n");
            ALOGE("%s: Could not open file %s for writing",
                  __FUNCTION__,
                  fs_filename);
        }

        /* Double check validity of EEPROM's data */
        if (!err && hdr.version >= EEPROM_HDR_VER_WCHKSUM)
        {
            if (calc_checksum != hdr.checksum)
            {
                /* Remove the board cal files to stop user from running with corrupted files */
                fprintf(stderr, "EEPROM: Error - EEPROM contents checksum mismatch!\n");
                fprintf(stderr, "EEPROM: calc_checksum=0x%08x, hdr.checksum=0x%08x.\n", calc_checksum, hdr.checksum);
                ALOGE("%s: EEPROM contents checksum mismatch!", __FUNCTION__);
                err = 1;
                remove(fs_filename);
            }
        }
    }
    else
    {
        err = 1;
        fprintf(stderr, "EEPROM: No valid header found\n");
        ALOGE("%s: No valid header found", __FUNCTION__);
    }

    return err;
}

/**
 * Find the end of the EEPROM data and put the address of the next available
     header into a provided pointer.
 * All files are stored in a chain so the only file which can be removed without
 *   re-writing the whole chain is the last one.
 *
 * @return 0 error, 1 if space is found.
 */
static uint32_t eeprom_find_free_space(drv_M24M01Handle_t *handle, uint32_t *addr)
{
    drv_eeprom_header hdr;
    int found = 0;

    /* Index to keep track of where in EEPROM we are. */
    while (*addr < (EEPROM_SIZE - EEPROM_BLOCK_SIZE))
    {
        /* Read in this header. */
        /* If this shows an invalid header then immediately return as the */
        /*   end of the chain has been reached. */
        if (!eeprom_read_header(handle, *addr, &hdr))
        {
            found = 1;
            break;
        }

        /* Move EEPROM index onto the next header. */
        *addr = eeprom_get_next_header_offset(*addr, &hdr);
    }

    return found;
}

/**
 * Copy calibration file from EEPROM to FS
 *
 * @return            0 if ok, != 0 if failed
 */
static int eeprom_dump(drv_M24M01Handle_t *handle, const char *fs_filename)
{
    int err = 0;

    /* Try and create a file */
    int fd = open(fs_filename, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fd >= 0)
    {
        uint8_t   buffer[EEPROM_BLOCK_SIZE];
        uint32_t  eoffset = 0;
        int       bytes_to_write;
        int       bytes_written;

        do
        {
          bytes_to_write = COM_MIN(EEPROM_BLOCK_SIZE, (M24M01_DEVICE_SIZE-eoffset));

          /* Read block from EEPROM */
          err = drv_M24M01ReadBlock(handle, eoffset, buffer, bytes_to_write);
          if (!err)
          {
              /* Write block to file */
              bytes_written = write(fd, buffer, bytes_to_write);
              DEV_ASSERT(bytes_written == bytes_to_write);

              eoffset += bytes_written;
          }
        }
        while ((M24M01_DEVICE_SIZE - eoffset) > 0 && !err);

        close(fd);
        fd = -1;
    }
    else
    {
        err = 1;
        fprintf(stderr, "EEPROM: Could not open file for writing\n");
    }

    return err;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

int drv_eeprom_nvram_read(const char *device_path, const char * eeprom_filename, const char *fs_filename)
{
    int err = -1;

    printf("%s(%s, %s, %s)\n", __FUNCTION__, device_path, eeprom_filename, fs_filename);

    /* Init EEPROM driver */
    drv_M24M01Handle_t *handle = drv_M24M01Open(device_path, EEPROM_I2C_ADDRESS);
    if (handle)
    {
        printf("Read file from EEPROM\n");
        ALOGI("%s: start.",__FUNCTION__);

        /** Copy from EEPROM to FS: 1st get EEPROM content in a tmp
         *  file and once all OK do a rename to required filepath.
         *  This to handle the fact that read from EEPROM may take a
         *  while.
         *  At 1st boot, if cal file need to be changed, it "may" be a
         *  problem if modem appli is trying to access cal file before
         *  it is ready, but for next boot where cal file doesn't need
         *  to be changed, all will be OK. */
        err = eeprom_copy_from(handle, eeprom_filename, TMP_CAL_FILE);
        if (err)
        {
            fprintf(stderr, "EEPROM: Could not read file from EEPROM\n");
            ALOGE("%s: Could not read file from EEPROM", __FUNCTION__);
        }
        else
        {
            /* All went well: rename TMP file to filepath */

            /** Change process umask to generate file with r/w access for
             *  other than root */
            err = rename(TMP_CAL_FILE, fs_filename);
            if(err)
            {
                fprintf(stderr, "EEPROM: fail to rename %s to %s.\n",
                        TMP_CAL_FILE,
                        fs_filename);
                ALOGE("%s: fail to rename %s to %s. %s",
                      __FUNCTION__,
                      TMP_CAL_FILE,
                      fs_filename,
                      strerror(errno));
            }
        }

        drv_M24M01Close(handle);
        handle = NULL;
        ALOGI("%s: done.", __FUNCTION__);
    }

    return err;
}

int drv_eeprom_nvram_write(const char *device_path, const char *fs_filename, const char *eeprom_filename)
{
    int err = -1;

    printf("%s(%s, %s, %s)\n", __FUNCTION__, device_path, fs_filename, eeprom_filename);

    /* Init EEPROM driver */
    drv_M24M01Handle_t *handle = drv_M24M01Open(device_path, EEPROM_I2C_ADDRESS);
    if (handle)
    {
        printf("Writing file to EEPROM\n");

        /* Find the next free space. */
        /* Files must be written in order so call this func with CAL_0 file first. */
        uint32_t hdr_offset = EEPROM_CAL_0_HDR_OFFSET;
        int found = eeprom_find_free_space(handle, &hdr_offset);
        if (found)
        {
            /* Copy to EEPROM from FS */
            err = eeprom_copy_to(handle, hdr_offset, fs_filename, eeprom_filename);
            if (err)
            {
                fprintf(stderr, "EEPROM: Could not write file to EEPROM\n");
            }
        }
        else
        {
            fprintf(stderr, "EEPROM: Could not find free space in EEPROM\n");
            err = 1;
        }

        drv_M24M01Close(handle);
        handle = NULL;
    }

    return err;
}

int drv_eeprom_nvram_erase(const char *device_path)
{
    int err = -1;

    printf("%s(%s)\n", __FUNCTION__, device_path);

    /* Init EEPROM driver */
    drv_M24M01Handle_t *handle = drv_M24M01Open(device_path, EEPROM_I2C_ADDRESS);
    if (handle)
    {
        printf("Erase EEPROM\n");

        /* Walk though the filesystem chain of headers invalidating the checkwords. */
        drv_eeprom_header hdr;
        uint32_t addr = 0; /* index to keep track of where in eeprom we are. */
        while (addr < (EEPROM_SIZE - EEPROM_BLOCK_SIZE))
        {
            /* Read in this header. */
            /* If this shows an invalid header then immediately return as the */
            /*   end of the chain has been reached. */
            if (!eeprom_read_header(handle, addr, &hdr))
            {
                /* This is not an error */
                err = 0;
                break;
            }

            /* Invalidate the header. */
            hdr.checkword = ~EEPROM_HDR_CHECKWORD;
            err = drv_M24M01WriteBlock(handle, addr, (uint8_t*)&hdr, sizeof(drv_eeprom_header));
            if (err)
            {
                fprintf(stderr, "EEPROM %s(): Could not write header. addr=0x%x\n", __func__, addr);
            }

            /* Move EEPROM index onto the next header. */
            addr = eeprom_get_next_header_offset(addr, &hdr);
        }

        drv_M24M01Close(handle);
        handle = NULL;
    }

    return err;
}

int drv_eeprom_nvram_dump(const char *device_path, const char *fs_filename)
{
    int err = -1;

    printf("%s(%s, %s)\n", __FUNCTION__, device_path, fs_filename);

    /* Init EEPROM driver */
    drv_M24M01Handle_t *handle = drv_M24M01Open(device_path, EEPROM_I2C_ADDRESS);
    if (handle)
    {
        printf("Read contents from EEPROM\n");

        /* Dump EEPROM data */
        err = eeprom_dump(handle, fs_filename);
        if (err)
        {
            fprintf(stderr, "EEPROM: Could not read contents from EEPROM\n");
        }

        drv_M24M01Close(handle);
        handle = NULL;
    }

    return err;
}

int drv_eeprom_valid_contents(const char *device_path)
{
    int result = 0;

    /* Init EEPROM driver */
    drv_M24M01Handle_t *handle = drv_M24M01Open(device_path, EEPROM_I2C_ADDRESS);
    if (handle)
    {
        drv_eeprom_header hdr;

        printf("Check contents of EEPROM\n");

        const char * str_valid   = "Contains valid %s file.\n";
        const char * str_invalid = "DOES NOT contain valid %s file.\n";

        /* EEPROM has CAL file? */
        if (eeprom_find_header(handle, CAL_0_FILENAME, &hdr))
        {
            printf(str_valid, CAL_0_FILENAME);
            result = 1;
        }
        else
        {
            printf(str_invalid, CAL_0_FILENAME);
            result = 0;
        }

        /* EEPROM has RFM file? */
        if (eeprom_find_header(handle, RFM_PLAT_CFG_FILENAME, &hdr))
        {
            printf(str_valid, RFM_PLAT_CFG_FILENAME);
        }
        else
        {
            printf(str_invalid, RFM_PLAT_CFG_FILENAME);
        }

        drv_M24M01Close(handle);
        handle = NULL;
    }
    else
    {
        fprintf(stderr, "EEPROM: Could not open EEPROM device\n");
    }

    return result;
}
