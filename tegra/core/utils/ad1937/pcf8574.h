/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef PCF8574_H
#define PCF8574_H

#define PCF8574_I2C_ADDR                                    0x27

#define PCF8574_SERIAL_MASK                                 (1 << 5)
#define PCF8574_SERIAL_OEM1                                 (0 << 5)
#define PCF8574_SERIAL_OEM2                                 (1 << 5)
#define PCF8574_ANALOG_ROUTE_MASK                           (1 << 4)
#define PCF8574_ANALOG_ROUTE_NVTEST                         (0 << 4)
#define PCF8574_ANALOG_ROUTE_JACINTO                        (1 << 4)
#define PCF8574_DAP3_MASK                                   (1 << 3)
#define PCF8574_DAP3_ENABLE                                 (0 << 3)
#define PCF8574_DAP3_DISABLE                                (1 << 3)
#define PCF8574_DAP2_MASK                                   (1 << 2)
#define PCF8574_DAP2_ENABLE                                 (0 << 2)
#define PCF8574_DAP2_DISABLE                                (1 << 2)
#define PCF8574_DAP1_MASK                                   (1 << 1)
#define PCF8574_DAP1_ENABLE                                 (0 << 1)
#define PCF8574_DAP1_DISABLE                                (1 << 1)
#define PCF8574_DIGITAL_ROUTE_MASK                          (1 << 0)
#define PCF8574_DIGITAL_ROUTE_JACINTO                       (0 << 0)
#define PCF8574_DIGITAL_ROUTE_NVTEST                        (1 << 0)

#endif /* PCF8574_H */
