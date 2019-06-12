/*
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include "tmon.h"
#include <errno.h>

#define I2C_M_WR                    0x00
#define SLAVE_ADDRESS               0x4c
#define LOCAL_TEMP_REG              0x00
#define REMOTE_TEMP_REG             0x01
#define LOCAL_LOW_READ              0x06
#define LOCAL_LOW_WRITE             0x0c
#define LOCAL_HIGH_READ             0x05
#define LOCAL_HIGH_WRITE            0x0b
#define LOCAL_THERM_LIMIT           0x20
#define REMOTE_LOW_READ             0x08
#define REMOTE_LOW_WRITE            0x0e
#define REMOTE_HIGH_READ            0x07
#define REMOTE_HIGH_WRITE           0x0d
#define REMOTE_THERM_LIMIT          0x19
#define ARA                         0x0c
#define REMOTE_OFFSET               0x09 /* Temperature difference between
                                          * remote thermal diode and tegra chip
                                          * hot spot. */

#ifdef TMON_DEBUG
#define DEBUG_PRINT(...) fprintf(stdout,__VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif

#define TMON_DATA_TO_VALUE(x) x - TMON_OFFSET*tmon_mode
#define TMON_VALUE_TO_DATA(x) x + TMON_OFFSET*tmon_mode
#define CONFIG_VALUE 0x04 * tmon_mode

static int tmon_mode = 0;

void* get_tmon_handle(char *i2c_bus) {
    unsigned int config_reg;
    sensor_data* tmp_board;
    tmp_board = (sensor_data*)malloc(sizeof(sensor_data));
    tmp_board->i2c_bus = i2c_bus;
    tmp_board->fd = open(tmp_board->i2c_bus, O_RDWR);

    if (!read_reg(tmp_board, CONFIGURATION_REG_READ, &config_reg))
        tmon_mode = (config_reg >> TMON_MODE_SHIFT) & 0x1;
    else
        DEBUG_PRINT("Error reading configuration register!\n");
    return (void*)tmp_board;
}

static int i2c_write(int fd, unsigned int addr, unsigned int offset,
        unsigned char *buf, unsigned char len)
{
    struct i2c_rdwr_ioctl_data msg_rdwr;
    struct i2c_msg i2cmsg;
    unsigned char _buf[20];
    int error;

    _buf[0] = offset;
    memcpy((_buf+1), buf, len);
    msg_rdwr.msgs = &i2cmsg;
    msg_rdwr.nmsgs = 1;
    i2cmsg.addr = addr;
    i2cmsg.flags = 0;
    i2cmsg.len = 1 + len;
    i2cmsg.buf = _buf;

    error = ioctl(fd, I2C_RDWR, &msg_rdwr);
    if (error  < 0) {
        return error;
    }

    return 0;
}

static int i2c_read(int fd, int addr, int offset, unsigned char *buf, int len)
{
    struct i2c_rdwr_ioctl_data msg_rdwr;
    struct i2c_msg i2cmsg[2];
    int error;

    msg_rdwr.msgs = i2cmsg;
    msg_rdwr.nmsgs = 2;
    buf[0] = offset;
    i2cmsg[0].addr = addr;
    i2cmsg[0].flags = I2C_M_WR | I2C_M_NOSTART;
    i2cmsg[0].len = 1;
    i2cmsg[0].buf = buf;

    i2cmsg[1].addr = addr;
    i2cmsg[1].flags = I2C_M_RD;
    i2cmsg[1].len = len;
    i2cmsg[1].buf = buf;

    error = ioctl(fd, I2C_RDWR, &msg_rdwr);
    if (error  < 0) {
        return error;
    }

    return 0;
}

int read_curr_temp(sensor_data *tmp_board, int *read_data,
        therm_zone zone_type)
{
    unsigned char msg;
    int error;

    if (!tmp_board) {
        DEBUG_PRINT("Tmon handle missing!\n");
        return EINVAL;
    }

    if (tmp_board->fd < 0) {
        DEBUG_PRINT("Error opening i2c device\n");
        return ENOENT;
    }

    if (zone_type == LOCAL){
        error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, LOCAL_TEMP_REG, &msg, 1);
        if (error){
            DEBUG_PRINT("read failure, couldn't read local temp"
                    "register\n");
            return error;
        }
    } else if (zone_type == REMOTE){
        error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, REMOTE_TEMP_REG, &msg, 1);
        if (error){
            DEBUG_PRINT("read failure, couldn't read remote temp"
                    " register\n");
            return  error;
        }
    }else {
        DEBUG_PRINT("temperature reg type wrongly specified\n");
        return  EINVAL;
    }

    *read_data = TMON_DATA_TO_VALUE(msg);
    if (zone_type == REMOTE)
        *read_data += REMOTE_OFFSET;
    DEBUG_PRINT("value read is %d\n",*read_data);
    return 0;
}

int read_reg(sensor_data *tmp_board, int offset, unsigned int *val)
{
    unsigned char msg;
    int error;

    if (!tmp_board) {
        DEBUG_PRINT("Tmon handle missing!\n");
        return EINVAL;
    }

    if (tmp_board->fd < 0) {
        DEBUG_PRINT("Error opening i2c device\n");
        return ENOENT;
    }

    error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, offset, &msg, 1);
    if (error) {
        DEBUG_PRINT("read failure, couldn't read register 0x%x\n", offset);
        return error;
    }

    if (val)
        *val = msg;

    return 0;
}

int write_reg(sensor_data *tmp_board, int offset, unsigned int val)
{
    unsigned char msg;
    int error;

    if (!tmp_board) {
        DEBUG_PRINT("Tmon handle missing!\n");
        return EINVAL;
    }

    if (tmp_board->fd < 0) {
        DEBUG_PRINT("Error opening i2c device\n");
        return ENOENT;
    }

    msg = val;
    error = i2c_write(tmp_board->fd, SLAVE_ADDRESS, offset, &msg, 1);
    if (error) {
        DEBUG_PRINT("write failure, couldn't write register 0x%x\n", offset);
        return error;
    }

    if (offset == CONFIGURATION_REG_WRITE)
        tmon_mode = (val >> TMON_MODE_SHIFT) & 0x1;

    return 0;
}

int read_ara(sensor_data *tmp_board)
{
    struct i2c_rdwr_ioctl_data msg_rdwr;
    struct i2c_msg i2cmsg[1];
    unsigned char buf;
    int error;
    unsigned int val;

    if (!tmp_board) {
        DEBUG_PRINT("Tmon handle missing!\n");
        return EINVAL;
    }

    if (tmp_board->fd < 0) {
        DEBUG_PRINT("Error opening i2c device\n");
        return ENOENT;
    }

    msg_rdwr.msgs = i2cmsg;
    msg_rdwr.nmsgs = 1;
    buf = 0;
    i2cmsg[0].addr = ARA;
    i2cmsg[0].flags = I2C_M_RD;
    i2cmsg[0].len = 1;
    i2cmsg[0].buf = &buf;

    error = ioctl(tmp_board->fd, I2C_RDWR, &msg_rdwr);
    if (error < 0) {
        DEBUG_PRINT("read failure, couldn't read ara register\n");
        return error;
    }

    val = buf;
    val >>= 1;
    if (val == SLAVE_ADDRESS)
        return 0;
    else
        return -1;
}

int get_threshold(sensor_data *tmp_board, trigger_threshold *data)
{
    unsigned char msg;
    int error;

    if (!tmp_board) {
        DEBUG_PRINT("Tmon handle missing!\n");
        return EINVAL;
    }

    if (tmp_board->fd < 0) {
        DEBUG_PRINT("Error opening i2c device\n");
        return  ENOENT;
    }

    error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, LOCAL_LOW_READ, &msg, 1);
    if (error) {
        DEBUG_PRINT("read failure,couldn't read local_alert_low register\n");
        return error;
    }
    data->local_alert_low.val = TMON_DATA_TO_VALUE(msg);

    error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, LOCAL_HIGH_READ, &msg, 1);
    if (error){
        DEBUG_PRINT("read failure,couldn't read local_alert_high register\n");
        return   error;
    }
    data->local_alert_high.val = TMON_DATA_TO_VALUE(msg);

    error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, LOCAL_THERM_LIMIT, &msg, 1);
    if (error){
        DEBUG_PRINT("read failure,couldn't read local shutdown limit "
                "register\n");
        return error;
    }
    data->local_shutdown_limit.val = TMON_DATA_TO_VALUE(msg);

    error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, REMOTE_LOW_READ, &msg, 1);
    if (error){
        DEBUG_PRINT("read failure,couldn't read remote_alert_low"
                "register\n");
        return   error;
    }
    data->remote_alert_low.val = TMON_DATA_TO_VALUE(msg) + REMOTE_OFFSET;

    error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, REMOTE_HIGH_READ, &msg, 1);
    if (error){
        DEBUG_PRINT("read failure,couldn't read remote_alert_high "
                "register\n");
        return  error;
    }
    data->remote_alert_high.val = TMON_DATA_TO_VALUE(msg) + REMOTE_OFFSET;

    error = i2c_read(tmp_board->fd, SLAVE_ADDRESS, REMOTE_THERM_LIMIT, &msg, 1);
    if (error){
        DEBUG_PRINT("read failure,couldn't read remote shutdown limit"
                "register\n");
        return  error;
    }
    data->remote_shutdown_limit.val = TMON_DATA_TO_VALUE(msg) + REMOTE_OFFSET;
    return 0;

}

int set_threshold(sensor_data *tmp_board, trigger_threshold data)
{
    unsigned char msg;
    int error;

    if (!tmp_board) {
        DEBUG_PRINT("Tmon handle missing!\n");
        return EINVAL;
    }

    if (tmp_board->fd < 0) {
        DEBUG_PRINT("Error opening i2c device\n");
        return ENOENT;
    }

    if (data.local_alert_high.need_update == TRUE){
        msg = TMON_VALUE_TO_DATA(data.local_alert_high.val) ;
        error = i2c_write(tmp_board->fd, SLAVE_ADDRESS, LOCAL_HIGH_WRITE, &msg, 1);
        if (error){
            DEBUG_PRINT("write failure,couldn't write to "
                    "local_alert_high register\n");
            return  error;
        }
    }
    if (data.local_alert_low.need_update == TRUE){
        msg = TMON_VALUE_TO_DATA(data.local_alert_low.val) ;
        error = i2c_write(tmp_board->fd, SLAVE_ADDRESS, LOCAL_LOW_WRITE, &msg, 1);
        if (error){
            DEBUG_PRINT("write failure,couldn't write to "
                    "local_alert_low register\n");
            return error;
        }
    }
    if (data.local_shutdown_limit.need_update == TRUE){
        msg = TMON_VALUE_TO_DATA(data.local_shutdown_limit.val) ;
        error = i2c_write(tmp_board->fd, SLAVE_ADDRESS, LOCAL_THERM_LIMIT, &msg, 1);
        if (error){
            DEBUG_PRINT("write failure,couldn't write to local"
                    "shutdown_limit register\n");
            return  error;
        }
    }
    if (data.remote_alert_high.need_update == TRUE){
        msg = TMON_VALUE_TO_DATA(data.remote_alert_high.val) - REMOTE_OFFSET;
        error = i2c_write(tmp_board->fd, SLAVE_ADDRESS, REMOTE_HIGH_WRITE, &msg, 1);
        if (error){
            DEBUG_PRINT("write failure,couldn't write"
                    " to remote_alert_high register\n");
            return error;
        }
    }
    if (data.remote_alert_low.need_update == TRUE){
        msg = TMON_VALUE_TO_DATA(data.remote_alert_low.val) - REMOTE_OFFSET;
        error = i2c_write(tmp_board->fd, SLAVE_ADDRESS, REMOTE_LOW_WRITE, &msg, 1);
        if (error){
            DEBUG_PRINT("write failure,couldn't write to"
                    "remote_alert_low register\n");
            return  error;
        }
    }
    if (data.remote_shutdown_limit.need_update == TRUE){
        msg = TMON_VALUE_TO_DATA(data.remote_shutdown_limit.val) - REMOTE_OFFSET;
        error = i2c_write(tmp_board->fd, SLAVE_ADDRESS, REMOTE_THERM_LIMIT, &msg, 1);
        if (error){
            DEBUG_PRINT("write failure,couldn't write to remote"
                    "shutdown_limit register\n");
            return  error;
        }
    }

    return 0;
}

void release_tmon_handle(sensor_data *tmp_board){
    if (tmp_board)
        close(tmp_board->fd);
    free(tmp_board);
}
