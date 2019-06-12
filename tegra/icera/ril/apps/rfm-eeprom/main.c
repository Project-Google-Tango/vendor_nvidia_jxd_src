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
#include "drv_eeprom_nvram.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
int main(int argc, char *argv[])
{
    int opt, long_val, long_opt_index = 0;;
    int opt_help = 0;
    int opt_erase = 0;
    int opt_read = 0;
    int opt_write = 0;
    int opt_dump = 0;
    int opt_check = 0;
    int opt_cal = 0;
    int opt_rfm = 0;
    char *fs_filename=NULL;
    char *fs_cal0_filename=NULL;
    char *fs_rfm_filename=NULL;
    char *device=NULL;
    int err = 0;

    /* Give group and others a read access to any file created by this tool. */
    umask(0);

    /* Parse args */
    struct option long_options[] = {
        /* long option array, where "name" is case sensitive*/
        {"help", 0, NULL, 'h'},             /* --help or -h will do the same... */
        {"cal", 1, &long_val, 'c'},         /* --cal <filename>  */
        {"rfm", 1, &long_val, 'r'},         /* --rfm <filename>  */
        {0, 0, 0, 0}                        /* usual array terminating item */
    };

    opterr = 0;
    while((opt = getopt_long(argc, argv, "hecrwx:d:", long_options, &long_opt_index)) != -1)
    {
       switch (opt)
       {
          /* Help */
          case 'h':
              opt_help = 1;
              break;
          /* Erase */
          case 'e':
              opt_erase = 1;
              break;
          /* Check */
          case 'c':
              opt_check = 1;
              break;
          /* Dump */
          case 'x':
              opt_dump = 1;
              asprintf(&fs_filename, "%s", optarg);
              break;
          /* Device */
          case 'd':
              asprintf(&device, "%s", optarg);
              break;
          /* Read */
          case 'r':
              opt_read = 1;
              break;
          /* Read */
          case 'w':
              opt_write = 1;
              break;
          case 0: /* a "long" option */
              switch(long_val)
              {
                case 'c': /* --cal <filename> */
                    opt_cal = 1;
                    asprintf(&fs_cal0_filename, "%s", optarg);
                    break;
                case 'r': /* --rfm <filename> */
                    opt_rfm = 1;
                    asprintf(&fs_rfm_filename, "%s", optarg);
                    break;
              }
              break;
          default:
              fprintf(stderr, "Unknown option\n");
              err = 1;
              break;
       }
    }

    if(opt_read || opt_write)
    {
        if( !opt_cal || !opt_rfm)
        {
            fprintf(stderr, "Both --cal <Cal0 filename> and --rfm <RFM config filename> need to be specified together for read/write.\n");
            err = 1;
        }
    }

    if (opt_read && opt_write)
    {
        fprintf(stderr, "Cannot do both read and write with the same command.\n");
        err = 1;
    }

    if (!opt_help && !device)
    {
        printf("Device not specified\n");
        err = 1;
    }

    /* Parse command line args */
    if (opt_help || err)
    {
        fprintf(stderr, "\nHelp:\n");
        fprintf(stderr, "-c                 Check if EEPROM contains valid contents.\n");
        fprintf(stderr, "-d /dev/i2c-1      I2C device path.\n");
        fprintf(stderr, "-e                 Erase EEPROM.\n");
        fprintf(stderr, "-x local_filename  Dump entire contents of EEPROM to file.\n");
        fprintf(stderr, "-r                 Read files from EEPROM.\n");
        fprintf(stderr, "-w                 Write files to EEPROM.\n");
        fprintf(stderr, "--cal filename     Calibration file name.\n"
                        "                   Destination name for read operation.\n"
                        "                   Source name for write operation.\n");
        fprintf(stderr, "--rfm filename     Dump entire contents of EEPROM to file.\n"
                        "                   Destination name for read operation.\n"
                        "                   Source name for write operation.\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "--cal <filename> and --rfm <filename> must always be given with -r or -w\n");
        fprintf(stderr, "\n   Read data from EEPROM and store it in local file system:\n"
                        "       icera-rfm-eeprom -d /dev/i2c-1 -r --cal calibration_0.bin --rfm rfmConfig.xml\n"
                        "     Calibration will be stored in calibration.bin, RFM config in rfmConfig.xml\n");
        fprintf(stderr, "\n   Write data to EEPROM from local file system:\n"
                        "       icera-rfm-eeprom -d /dev/i2c-1 -w --cal calibration_0.bin --rfm platformConfig.xml\n"
                        "     Calibration data taken from calibration.bin, RFM config from platformConfig.xml\n");
        fprintf(stderr, "\n");
        return opt_help ? 0 : -1;
    }

    int ret = 0;
    if (opt_erase)
    {
        ret = drv_eeprom_nvram_erase(device);
    }
    else if (opt_check)
    {
        ret = drv_eeprom_valid_contents(device) ? 0 : -1;
    }
    else if (opt_dump)
    {
        ret = drv_eeprom_nvram_dump(device, fs_filename);
    }
    else if (opt_read)
    {
        int ret0 = drv_eeprom_nvram_read(device, CAL_0_FILENAME, fs_cal0_filename);
        if (ret0)
        {
            fprintf(stderr, "Cal read failed.\n");
        }

        int ret1 = drv_eeprom_nvram_read(device, RFM_PLAT_CFG_FILENAME, fs_rfm_filename);
        if (ret1)
        {
            fprintf(stderr, "RFM read failed.\n");
        }
        ret = (ret0 || ret1);
    }
    else if (opt_write)
    {
        ret = drv_eeprom_nvram_erase(device);
        if (ret)
        {
            fprintf(stderr, "Initial erase stage of write failed.\n");
            return ret;
        }

        ret = drv_eeprom_nvram_write(device, fs_cal0_filename, CAL_0_FILENAME);
        if (ret)
        {
            fprintf(stderr, "Cal stage of write failed.\n");
            return ret;
        }

        ret = drv_eeprom_nvram_write(device, fs_rfm_filename, RFM_PLAT_CFG_FILENAME);
        if (ret)
        {
            fprintf(stderr, "RFM stage of write failed.\n");
            return ret;
        }
    }
    else
    {
        printf("Missing command\n");
        ret = -1;
    }

    free(fs_filename);
    free(fs_cal0_filename);
    free(fs_rfm_filename);
    free(device);
    return ret;
}
