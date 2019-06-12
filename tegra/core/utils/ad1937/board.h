/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#include "cpld.h"
#include "max9485.h"
#include "pcf8574.h"
#include "i2c-util.h"
#include "nvos.h"
#include "codec.h"

#define EEPROM_ADDRESS 0x50
#define NVPN_LEN 14

/**
 * PCF8574
 */
static const NvU8 pcf8574_enable_DAP1 =
    PCF8574_ANALOG_ROUTE_NVTEST |
    PCF8574_DIGITAL_ROUTE_NVTEST |
    PCF8574_DAP1_ENABLE |
    PCF8574_DAP2_DISABLE |
    PCF8574_DAP3_DISABLE |
    PCF8574_SERIAL_OEM1;

static const NvU8 pcf8574_enable_DAP2 =
    PCF8574_ANALOG_ROUTE_NVTEST |
    PCF8574_DIGITAL_ROUTE_NVTEST |
    PCF8574_DAP1_DISABLE |
    PCF8574_DAP2_ENABLE |
    PCF8574_DAP3_DISABLE |
    PCF8574_SERIAL_OEM1;

static const NvU8 pcf8574_enable_DAP3 =
    PCF8574_ANALOG_ROUTE_NVTEST |
    PCF8574_DIGITAL_ROUTE_NVTEST |
    PCF8574_DAP1_DISABLE |
    PCF8574_DAP2_DISABLE |
    PCF8574_DAP3_ENABLE |
    PCF8574_SERIAL_OEM1;

static const NvU8 pcf8574_disable =
    PCF8574_ANALOG_ROUTE_NVTEST |
    PCF8574_DIGITAL_ROUTE_NVTEST |
    PCF8574_DAP1_DISABLE |
    PCF8574_DAP2_DISABLE |
    PCF8574_DAP3_DISABLE |
    PCF8574_SERIAL_OEM1;

NvError ConfigureBaseBoard(int fd, CodecMode port_num, unsigned int *sku_id);

#endif
