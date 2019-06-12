/*
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _TMON_H_
#define _TMON_H_

#define STATUS_REG                  0x02
#define CONFIGURATION_REG_READ      0x03
#define CONFIGURATION_REG_WRITE     0x09
#define CONVERSION_REG_READ         0x04
#define CONVERSION_REG_WRITE        0x0a
#define TMON_OFFSET                 0x40
#define TMON_MODE_SHIFT             2
#define TMON_STANDBY_MASK           0x40

typedef enum {LOCAL, REMOTE} therm_zone;
typedef enum {FALSE, TRUE} boolean;

typedef struct {
    int val;
    /* Setting need_update = TRUE, update threshold value with set_threshold call */
    boolean need_update;
} threshold;

typedef struct {
    /* Local refers to thermal sensor chip and remote is tegra chip.
     * Local temperature is expected to be lower than the remote temperature.
     * Alert will be triggered when an out-of-limit masurement is detected
     * based on four alert limit threshold value. The thermal sensor also assters
     * over temperature line when current local/remote temperature crosses any
     * shutdown limit.
     */

    /* Alert limit */
    threshold local_alert_low;
    threshold local_alert_high;
    threshold remote_alert_low;
    threshold remote_alert_high;

    /* Shutdown limit */
    threshold local_shutdown_limit;
    threshold remote_shutdown_limit;
} trigger_threshold;

typedef struct {
    /* i2c_bus refers to the bus which is connected to thermal sensor chip */
    char *i2c_bus;
    int fd;
} sensor_data;


/* Create tmon handle */
void * get_tmon_handle(char *i2c_bus);

/* return current local/romote temperature based on thermal zone */
int read_curr_temp(sensor_data *tmp_board, int *read_data,
        therm_zone zone_type);

/* provides all local/remote and alert/shutdown threshold values */
int get_threshold(sensor_data *tmp_board, trigger_threshold *data);

int read_reg(sensor_data *tmp_board, int offset, unsigned int *val);

int write_reg(sensor_data *tmp_board, int offset, unsigned int val);

int read_ara(sensor_data *tmp_board);

/* update alert/shutdown threshold based on need_update flag in threshold_data */
int set_threshold(sensor_data *tmp_board,  trigger_threshold data);

/* Release tmon handle */
void release_tmon_handle(sensor_data* tmp_board);
#endif

