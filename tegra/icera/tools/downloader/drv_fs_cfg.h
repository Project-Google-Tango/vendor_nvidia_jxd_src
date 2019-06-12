/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @defgroup filesystem Flash File System
 *
 */

/**
 * @file drv_fs_cfg.h Some filesystem configurations
 *
 */

#ifndef DRV_FS_CFG_H
#define DRV_FS_CFG_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/
#include "icera_global.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/**
 * Root directories
 *
 * List of misc root dir names used as mount point for a given
 * device.
 */
#define DRV_FS_DIR_NAME_PARTITION1 "/"        /** Used to mount DRV_FS_DEVICE_NAME_PARTITION1 */
#define DRV_FS_DIR_NAME_ZERO_CD    "/zero-cd" /** Used to mount DRV_FS_DEVICE_NAME_ZERO_CD */
#define DRV_FS_DIR_NAME_BACKUP     "/backup"  /** Used to mount DRV_FS_DEVICE_NAME_BACKUP */
#define DRV_FS_DIR_NAME_TIIO       "/tiio"

/**
 * Directories
 *
 * Some directories used to store different files...
 * (created through mkdir)
 */
#define DRV_FS_APP_DIR_NAME            "/app"
#define DRV_FS_DATA_DIR_NAME           "/data"
#define DRV_FS_DATA_FACT_DIR_NAME      "/data/factory" /* MUST be considered as a R/O folder at runtime... */
#define DRV_FS_DATA_MODEM_NV_DIR_NAME  "/data/modem"
#define DRV_FS_DATA_CONFIG_DIR_NAME    "/data/config" /* MUST be considered as a R/O folder at runtime... */
#define DRV_FS_DATA_MODEM_DIR_NAME DRV_FS_DATA_DIR_NAME"/modem"
#define DRV_FS_DEBUG_DATA_DIR_NAME DRV_FS_DATA_DIR_NAME"/debug"
#define DRV_FS_DATA_SSL_DIR_NAME DRV_FS_DATA_DIR_NAME"/ssl"
#define DRV_FS_DATA_GPS_DIR_NAME DRV_FS_DATA_DIR_NAME"/gps"

/** Path to explicit remote files that may never exist in a
 *  flash file system...: */
#define DRV_FS_DATA_REMOTES_DIR_NAME DRV_FS_DATA_DIR_NAME"/remotes"
#define DRV_FS_DATA_REMOTE_SIM_DIR_NAME  DRV_FS_DATA_REMOTES_DIR_NAME"/sim"
#define DRV_FS_DATA_REMOTE_EDP_DIR_NAME  DRV_FS_DATA_REMOTES_DIR_NAME"/edp"
#define DRV_FS_DATA_REMOTE_ISO_DIR_NAME  DRV_FS_DATA_REMOTES_DIR_NAME"/iso"
#define DRV_FS_DATA_REMOTE_RF_DIR_NAME   DRV_FS_DATA_REMOTES_DIR_NAME"/rf"

#define DRV_FS_WEBUI_DIR_NAME DRV_FS_DIR_NAME_ZERO_CD"/webui"

/**
 * File names
 *
 */
#define WRITE_TEST_FILE            DRV_FS_DATA_REMOTES_DIR_NAME"/write_test_file"     /** File to test the FS write mechanism */
#define TEST_REPORT_FILE           DRV_FS_DATA_REMOTES_DIR_NAME"/test_report.log"     /** Log file for some tests */
#define AUDIO_ECAL_FILE            DRV_FS_DATA_DIR_NAME"/audioEcal.bin"               /** File containing audio electrical calibration */
#define AUDIO_TCAL_FILE            DRV_FS_DATA_DIR_NAME"/audioTcal.bin"               /** File containing audio transducer calibration */
#define HIF_CONFIG_FILE            DRV_FS_DATA_DIR_NAME"/hifConfig.bin"               /** File containing host interface configuration */
#define EXTAUTH_FILE               DRV_FS_DATA_DIR_NAME"/extAuth.bin"                 /** File containing external authentication attributes */
#define NVRAM_RW_FILE              DRV_FS_DATA_DIR_NAME"/nvram-rw.bin"
#define CAL_PATCH_HISTORY          DRV_FS_DATA_DIR_NAME"/calibration_change.log"
#define FT_CUSTOM_FILE             DRV_FS_DATA_DIR_NAME"/ft_custom.bin"
#define FT_PREV_PROC_FILE          DRV_FS_DATA_DIR_NAME"/ft_prev_proc.bin"
#define FT_DATA_FILE               DRV_FS_DATA_DIR_NAME"/ft_data.bin"
#define MDMBACKUP_APPLICATION_FILE DRV_FS_APP_DIR_NAME"/modem.backup.wrapped"         /** MDM backup application binary */
#define TEMP_UPDATE_FILE           DRV_FS_APP_DIR_NAME"/dummy_archive.tmp"            /** Temporary file created during archive update */
#define ENGINEERING_MODE_FILE      DRV_FS_DATA_MODEM_DIR_NAME"/engineering_mode.bin"
#define BOOT_CONFIG_FILE           DRV_FS_DATA_FACT_DIR_NAME"/bootConfig.bin"

/** SIM voltage remote files: */
#define SIM0_ENABLE_FILE           DRV_FS_DATA_REMOTE_SIM_DIR_NAME"/sim0_enable"
#define SIM0_VOLTAGE_FILE          DRV_FS_DATA_REMOTE_SIM_DIR_NAME"/sim0_voltage"
#define SIM1_ENABLE_FILE           DRV_FS_DATA_REMOTE_SIM_DIR_NAME"/sim1_enable"
#define SIM1_VOLTAGE_FILE          DRV_FS_DATA_REMOTE_SIM_DIR_NAME"/sim1_voltage"

/** RF voltage remote files */
#define RF1V7_MODE_FILE            DRV_FS_DATA_REMOTE_RF_DIR_NAME"/rf1v7_mode"
#define RF1V7_VOLTAGE_FILE         DRV_FS_DATA_REMOTE_RF_DIR_NAME"/rf1v7_voltage"
#define RF2V7_MODE_FILE            DRV_FS_DATA_REMOTE_RF_DIR_NAME"/rf2v7_mode"
#define RF2V7_VOLTAGE_FILE         DRV_FS_DATA_REMOTE_RF_DIR_NAME"/rf2v7_voltage"

/* EDP-ISO remote/ sysFS variables */
#define EDP_IMAX_STATES_FILE DRV_FS_DATA_REMOTE_EDP_DIR_NAME"/i_max"
#define EDP_STATE_SET_STATE_FILE DRV_FS_DATA_REMOTE_EDP_DIR_NAME"/request"
#define EDP_LOAN_THRESHOLD_FILE DRV_FS_DATA_REMOTE_EDP_DIR_NAME"/threshold"

#define ISO_REGISTER_FILE DRV_FS_DATA_REMOTE_ISO_DIR_NAME"/register"
#define ISO_RESERVE_FILE DRV_FS_DATA_REMOTE_ISO_DIR_NAME"/reserve"
#define ISO_REALIZE_FILE DRV_FS_DATA_REMOTE_ISO_DIR_NAME"/realize"
#define ISO_RESERVE_REALIZE_FILE DRV_FS_DATA_REMOTE_ISO_DIR_NAME"/reserve_realize"

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

#endif /* #ifndef DRV_FS_CFG_H*/

/** @} END OF FILE */

