/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 
#ifndef INCLUDED_TCA6416_EXPANDER_REG_HEADER
#define INCLUDED_TCA6416_EXPANDER_REG_HEADER


// Ports evailable on TCA6416. it is having 2 ports
#define TCA6416_PORT_0  0
#define TCA6416_PORT_1  1


// Each port is having 8 pins 
#define TCA6416_PIN_0  0
#define TCA6416_PIN_1  1
#define TCA6416_PIN_2  2
#define TCA6416_PIN_3  3
#define TCA6416_PIN_4  4
#define TCA6416_PIN_5  5
#define TCA6416_PIN_6  6
#define TCA6416_PIN_7  7


// Registers
#define TCA6416_INPUT_PORT_0            0x00    // For ports 00 to 07
#define TCA6416_INPUT_PORT_1            0x01    // For ports 10 to 17
#define TCA6416_OUTPUT_PORT_0           0x02    // For ports 00 to 07
#define TCA6416_OUTPUT_PORT_1           0x03    // For ports 10 to 17
#define TCA6416_POLARITY_INV_PORT_0     0x04    // For ports 00 to 07
#define TCA6416_POLARITY_INV_PORT_1     0x05    // For ports 10 to 17
#define TCA6416_CONFIG_PORT_0           0x06    // For ports 00 to 07
#define TCA6416_CONFIG_PORT_1           0x07    // For ports 10 to 17



#define TCA6416_INVALID_PORT    0xFF

#endif //INCLUDED_TCA6416_EXPANDER_REG_HEADER

