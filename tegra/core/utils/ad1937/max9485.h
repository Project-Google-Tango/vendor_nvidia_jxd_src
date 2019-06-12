/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef MAX9485_H
#define MAX9485_H

#define MAX9485_I2C_ADDR                                    0x60

#define MAX9485_MCLK_MASK                                   (1 << 7)
#define MAX9485_MCLK_DISABLE                                (0 << 7)
#define MAX9485_MCLK_ENABLE                                 (1 << 7)
#define MAX9485_CLK_OUT2_MASK                               (1 << 6)
#define MAX9485_CLK_OUT2_DISABLE                            (0 << 6)
#define MAX9485_CLK_OUT2_ENABLE                             (1 << 6)
#define MAX9485_CLK_OUT1_MASK                               (1 << 5)
#define MAX9485_CLK_OUT1_DISABLE                            (0 << 5)
#define MAX9485_CLK_OUT1_ENABLE                             (1 << 5)
#define MAX9485_SAMPLING_RATE_MASK                          (1 << 4)
#define MAX9485_SAMPLING_RATE_STANDARD                      (0 << 4)
#define MAX9485_SAMPLING_RATE_DOUBLED                       (1 << 4)
#define MAX9485_OUTPUT_SCALING_FACTOR_MASK                  (3 << 2)
#define MAX9485_OUTPUT_SCALING_FACTOR_256                   (0 << 2)
#define MAX9485_OUTPUT_SCALING_FACTOR_384                   (1 << 2)
#define MAX9485_OUTPUT_SCALING_FACTOR_768                   (2 << 2)
#define MAX9485_SAMPLING_FREQUENCY_MASK                     (3 << 0)
#define MAX9485_SAMPLING_FREQUENCY_12KHZ                    (0 << 0)
#define MAX9485_SAMPLING_FREQUENCY_32KHZ                    (1 << 0)
#define MAX9485_SAMPLING_FREQUENCY_44_1KHZ                  (2 << 0)
#define MAX9485_SAMPLING_FREQUENCY_48KHZ                    (3 << 0)

#endif /* MAX9485_H */
