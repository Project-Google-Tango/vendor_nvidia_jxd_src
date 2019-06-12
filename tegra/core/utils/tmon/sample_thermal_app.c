/*
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


/* It is a sample application to show how to use thermal APIs to
 * 1. Read current local/remote temperature
 * 2. Set local/remote alert/shutdown thermal threshold
 * 3. Get loca/remote alert/shutdowm thermal threshold
 */

#include <stdio.h>
#include <stdlib.h>
#include "tmon.h"
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#define MONITORING_INTERVAL 1    // temperature monitoring interval in seconds

// Reading different threshold values from respective registers
#define PRINT_THRESHOLD_VALUES(_tmon_handle, _thresholdData)        \
    if (!get_threshold(_tmon_handle, _thresholdData))               \
        printf("\nlocal_alert_low %d local_alert_high %d "                 \
            "local_shutdown_limit %d\nremote_alert_low %d "            \
            "remote_alert_high %d remote_shutdown_limit %d\n\n",         \
                (_thresholdData)->local_alert_low.val,                \
                (_thresholdData)->local_alert_high.val,               \
                (_thresholdData)->local_shutdown_limit.val,           \
                (_thresholdData)->remote_alert_low.val,               \
                (_thresholdData)->remote_alert_high.val,              \
                (_thresholdData)->remote_shutdown_limit.val);         \
    else {                                                           \
        perror("\nFailed to get threshold values.");                   \
        goto cleanup;                                                \
    }

// Reading value from local/remote temp register
#define PRINT_CURRENT_TEMP(_zone_type)                               \
    zone = (_zone_type) ? remote : local;                            \
    if (!read_curr_temp(tmonHandle, &currTemp, _zone_type))          \
        printf("current %s temp is %d\n", zone, currTemp);           \
    else {                                                           \
        perror("Failed to read temperature register");               \
        goto cleanup;                                                \
    }

static void *tmonHandle = NULL;
static int gpioFD = -1;

void sighandler(int sig);
void print_help(void);

void sighandler(int sig)
{
    if (tmonHandle)
        release_tmon_handle(tmonHandle);
    if (gpioFD >= 0)
        close(gpioFD);
    exit(0);
}

void print_help()
{
    printf("\nHelp: this tool takes i2c number, therm alert GPIO, remote "
        "alert high temperature limit, local alert high temperature limit"
        " and operating mode as arguments!\n");
    printf("\nSyntax: ./sample_thermal_app -n <i2c_number> -g <alert_gpio_no>"
        " -r <remote_alert_high_temp> -l <local_alert_high_temp> -m <mode>\n");
    printf("\nwhere,\n"
        "-n	<i2c_number>		1 for P852 and 4 for P1852, Harmony and E1853\n"
        "-g	<alert_gpio_no>		28 for P852, 179 for P1852 and 139 for E1853\n"
        "-r	<new_remote_alert>	new remote alert temperature to be set\n"
        "-l	<new_local_alert>	new local alert temperature to be set\n"
        "-m	<mode>			set mode to either 0 (normal) or 1 (extended)\n"
        "-e				print values once and then exit\n\n");
}

int main(int argc, char *argv[]){

    trigger_threshold thresholdData;
    struct pollfd fds[1];
    unsigned int configReg, statusReg, conversionRateReg = 256, mode = 0;
    int currTemp, newRemoteAlertHighTemp = 105, opt, len, lRet;
    int noAlert = 1, newLocalAlertHighTemp = 105, currLocal, currRemote;
    char buf[2], gpioToTest[32] = "/sys/class/gpio/gpio", gpioEdge[32];
    char command[64] = "echo falling > ", i2cDevice[16] = "/dev/i2c-";
    char local[8] = "local", remote[8] = "remote";
    char command_export_gpio[64] = "echo ";
    char *zone;
    bool newRemoteAlertTemp = false, alertCondition = false;
    bool printAndExit = false, setMode = false, newLocalAlertTemp = false;
    bool got_n = false, got_g = false;
    struct stat st;

    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);

    while ((opt = getopt(argc, argv, "n:g:r:l:em:h")) != -1) {
        switch (opt) {
            case 'n':
                strcat(i2cDevice, optarg);
                got_n = true;
                break;
            case 'g':
                strcat(gpioToTest, optarg);
                strcat(command_export_gpio, optarg);
                got_g = true;
                break;
            case 'r':
                newRemoteAlertHighTemp = atoi(optarg);
                newRemoteAlertTemp = true;
                break;
            case 'l':
                newLocalAlertHighTemp = atoi(optarg);
                newLocalAlertTemp = true;
                break;
            case 'e':
                printAndExit = true;
                break;
            case 'm':
                mode = atoi(optarg);
                setMode = true;
                break;
            case 'h':
            default:
                print_help();
                return -1;
        }
    }

    if (!got_n || !got_g) {
        printf("ERROR: -n and -g options are mandatory!\n");
        print_help();
        return -1;
    }

    tmonHandle = get_tmon_handle(i2cDevice);
    if (!tmonHandle) {
        perror("Could not initialize tmon");
        return -1;
    }

    if (printAndExit) {
        PRINT_THRESHOLD_VALUES(tmonHandle, &thresholdData);
        PRINT_CURRENT_TEMP(LOCAL);
        PRINT_CURRENT_TEMP(REMOTE);
        return 0;
    }

    if (stat(gpioToTest, &st))
    {
        strcat(command_export_gpio, " > /sys/class/gpio/export");
        system(command_export_gpio);
    }

    strcpy(gpioEdge, gpioToTest);
    strcat(gpioEdge, "/edge");
    strcat(gpioToTest, "/value");

    /* Changing operating mode of temperature monitor */
    if (setMode) {
        if ((mode != 0) && (mode != 1)) {
            printf("Error: incorrect value given with -m option!\n");
            printf("Provide 0 for normal mode and 1 for extended mode.\n");
            goto cleanup;
        }

        if (read_reg(tmonHandle, CONFIGURATION_REG_READ, &configReg)) {
            perror("Reading configuration register failed!\n");
            goto cleanup;
        }
        if (((configReg >> TMON_MODE_SHIFT) & 0x1) != mode) {
            /* Enabling standby mode */
            configReg |= TMON_STANDBY_MASK;
            if (write_reg(tmonHandle, CONFIGURATION_REG_WRITE, configReg)) {
                perror("Writing to configuration register failed!\n");
                goto cleanup;
            }
            /* Setting limits before changing mode so that they are
             * interpreted according to the mode. */
            if (mode) {
                thresholdData.remote_alert_low.val = -58 + TMON_OFFSET;
                thresholdData.remote_alert_high.val = 105 + TMON_OFFSET;
                thresholdData.remote_shutdown_limit.val = 110 + TMON_OFFSET;
                thresholdData.local_alert_low.val = -64 + TMON_OFFSET;
                thresholdData.local_alert_high.val = 105 + TMON_OFFSET;
                thresholdData.local_shutdown_limit.val = 105 + TMON_OFFSET;
            }
            else {
                thresholdData.remote_alert_low.val = 6 - TMON_OFFSET;
                thresholdData.remote_alert_high.val = 105 - TMON_OFFSET;
                thresholdData.remote_shutdown_limit.val = 110 - TMON_OFFSET;
                thresholdData.local_alert_low.val = 0 - TMON_OFFSET;
                thresholdData.local_alert_high.val = 105 - TMON_OFFSET;
                thresholdData.local_shutdown_limit.val = 105 - TMON_OFFSET;
            }

            thresholdData.remote_alert_low.need_update = TRUE;
            thresholdData.remote_alert_high.need_update = TRUE;
            thresholdData.remote_shutdown_limit.need_update = TRUE;
            thresholdData.local_alert_low.need_update = TRUE;
            thresholdData.local_alert_high.need_update = TRUE;
            thresholdData.local_shutdown_limit.need_update = TRUE;

            if (set_threshold(tmonHandle, thresholdData)) {
                perror("Setting threshold values failed");
                goto cleanup;
            }

            /* Disabling standby mode and setting extended mode */
            configReg &= ~(0x1 << TMON_MODE_SHIFT);
            configReg &= ~TMON_STANDBY_MASK;
            configReg |= (mode << 2);
            if (write_reg(tmonHandle, CONFIGURATION_REG_WRITE, configReg)) {
                perror("Writing to configuration register failed!\n");
                goto cleanup;
            }
            if (read_reg(tmonHandle, CONVERSION_REG_READ, &conversionRateReg)) {
                perror("Reading conversion register failed!\n");
                goto cleanup;
            }
        }
    }

    PRINT_THRESHOLD_VALUES(tmonHandle, &thresholdData);

    /* Wait for at least one measurement cycle since the temperature range
     * was changed before measuring the temperature again!
     */
    if (conversionRateReg & 0xff)
        usleep(1000000 / (2 ^ (conversionRateReg - 5)));

    if (newRemoteAlertTemp || newLocalAlertTemp) {
        thresholdData.remote_alert_low.need_update = FALSE;
        thresholdData.remote_shutdown_limit.need_update = FALSE;
        thresholdData.local_alert_low.need_update = FALSE;
        thresholdData.local_shutdown_limit.need_update = FALSE;

        if (newRemoteAlertTemp) {
            thresholdData.remote_alert_high.val = newRemoteAlertHighTemp;
            thresholdData.remote_alert_high.need_update = TRUE;
        }
        else
            thresholdData.remote_alert_high.need_update = FALSE;

        if (newLocalAlertTemp) {
            thresholdData.local_alert_high.val = newLocalAlertHighTemp;
            thresholdData.local_alert_high.need_update = TRUE;
        }
        else
            thresholdData.local_alert_high.need_update = FALSE;

        if (set_threshold(tmonHandle, thresholdData)) {
            perror("Setting threshold values failed");
            goto cleanup;
        }
    }

    PRINT_THRESHOLD_VALUES(tmonHandle, &thresholdData);

    gpioFD = open(gpioToTest, O_RDWR);
    if (gpioFD < 0) {
        printf("error opening file");
        goto cleanup;
    }
    fds[0].fd = gpioFD;
    fds[0].events = POLLPRI;
    fds[0].revents = 0;

    strcat(command, gpioEdge);
    system(command);

    while (1) {
        PRINT_CURRENT_TEMP(LOCAL);
        currLocal = currTemp;
        PRINT_CURRENT_TEMP(REMOTE);
        currRemote = currTemp;

        if (!alertCondition) {
            lseek(fds[0].fd, SEEK_SET, 0);
            len = read(fds[0].fd, buf, 1);
            buf[1] = '\0';

            if (!atoi(buf))
                noAlert = 0;
            else {
                lRet = poll(fds, 1, 1000 * MONITORING_INTERVAL);

                if (lRet < 0)
                    perror("Poll returned error!");
                else if (!lRet)
                    noAlert = 1;
                else
                    noAlert = 0;
            }
        }
        else
            sleep(MONITORING_INTERVAL);

        if (!noAlert) {
            if ((currRemote < thresholdData.remote_alert_high.val) &&
                (currRemote > thresholdData.remote_alert_low.val) &&
                (currLocal < thresholdData.local_alert_high.val) &&
                (currLocal > thresholdData.local_alert_low.val)) {
                if (read_reg(tmonHandle, STATUS_REG, &statusReg)) {
                    perror("Can't read status register\n");
                    goto cleanup;
                }

                if (read_ara(tmonHandle))
                    perror("Can't read Alert Response Address (ARA). Hence can't reset Alert GPIO\n");
                else
                    alertCondition = false;
            }
            else {
                alertCondition = true;
                printf("WARNING: Alert Temperature reached!\n");
            }
        }
    }

cleanup:
    if (tmonHandle)
        release_tmon_handle(tmonHandle);
    if (gpioFD >= 0)
        close(gpioFD);
    return -1;
}

