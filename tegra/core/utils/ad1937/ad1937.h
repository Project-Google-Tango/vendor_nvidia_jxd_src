/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef AD1937_H
#define AD1937_H

#define AD1937_I2C_ADDR                                     0x07

#define AD1937_PLL_CLK_CTRL_0                               0x00
#define AD1937_PLL_CLK_CTRL_1                               0x01
#define AD1937_DAC_CTRL_0                                   0x02
#define AD1937_DAC_CTRL_1                                   0x03
#define AD1937_DAC_CTRL_2                                   0x04
#define AD1937_DAC_MUTE_CTRL                                0x05
#define AD1937_DAC_VOL_CTRL_DAC1L                           0x06
#define AD1937_DAC_VOL_CTRL_DAC1R                           0x07
#define AD1937_DAC_VOL_CTRL_DAC2L                           0x08
#define AD1937_DAC_VOL_CTRL_DAC2R                           0x09
#define AD1937_DAC_VOL_CTRL_DAC3L                           0x0a
#define AD1937_DAC_VOL_CTRL_DAC3R                           0x0b
#define AD1937_DAC_VOL_CTRL_DAC4L                           0x0c
#define AD1937_DAC_VOL_CTRL_DAC4R                           0x0d
#define AD1937_ADC_CTRL_0                                   0x0e
#define AD1937_ADC_CTRL_1                                   0x0f
#define AD1937_ADC_CTRL_2                                   0x10

#define AD1937_PLL_CLK_CTRL_0_PWR_MASK                      (1 << 0)
#define AD1937_PLL_CLK_CTRL_0_PWR_ON                        (0 << 0)
#define AD1937_PLL_CLK_CTRL_0_PWR_OFF                       (1 << 0)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_MASK                    (3 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_256                     (0 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_384                     (1 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_512                     (2 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_768                     (3 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_MASK                    (3 << 3)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_XTAL                    (0 << 3)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_256                     (1 << 3)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_512                     (2 << 3)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_OFF                     (3 << 3)
#define AD1937_PLL_CLK_CTRL_0_PLL_INPUT_MASK                (3 << 5)
#define AD1937_PLL_CLK_CTRL_0_PLL_INPUT_MCLKI               (0 << 5)
#define AD1937_PLL_CLK_CTRL_0_PLL_INPUT_DLRCLK              (1 << 5)
#define AD1937_PLL_CLK_CTRL_0_PLL_INPUT_ALRCLK              (2 << 5)
#define AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_MASK      (1 << 7)
#define AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_DISABLE   (0 << 7)
#define AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_ENABLE    (1 << 7)


#define AD1937_PLL_CLK_CTRL_1_DAC_CLK_MASK                  (1 << 0)
#define AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL                   (0 << 0)
#define AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK                  (1 << 0)
#define AD1937_PLL_CLK_CTRL_1_ADC_CLK_MASK                  (1 << 1)
#define AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL                   (0 << 1)
#define AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK                  (1 << 1)
#define AD1937_PLL_CLK_CTRL_1_VREF_MASK                     (1 << 2)
#define AD1937_PLL_CLK_CTRL_1_VREF_ENABLE                   (0 << 2)
#define AD1937_PLL_CLK_CTRL_1_VREF_DISABLE                  (1 << 2)
#define AD1937_PLL_CLK_CTRL_1_PLL_MASK                      (1 << 3)
#define AD1937_PLL_CLK_CTRL_1_PLL_NOT_LOCKED                (0 << 3)
#define AD1937_PLL_CLK_CTRL_1_PLL_LOCKED                    (1 << 3)

#define AD1937_DAC_CTRL_0_PWR_MASK                          (1 << 0)
#define AD1937_DAC_CTRL_0_PWR_ON                            (0 << 0)
#define AD1937_DAC_CTRL_0_PWR_OFF                           (1 << 0)
#define AD1937_DAC_CTRL_0_SAMPLE_RATE_MASK                  (3 << 1)
#define AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ        (0 << 1)
#define AD1937_DAC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ        (1 << 1)
#define AD1937_DAC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ     (2 << 1)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_MASK                 (7 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_1                    (0 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_0                    (1 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_8                    (2 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_12                   (3 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_16                   (4 << 3)
#define AD1937_DAC_CTRL_0_FMT_MASK                          (3 << 6)
#define AD1937_DAC_CTRL_0_FMT_STEREO                        (0 << 6)
#define AD1937_DAC_CTRL_0_FMT_TDM_SINGLE_LINE               (1 << 6)
#define AD1937_DAC_CTRL_0_FMT_TDM_AUX                       (2 << 6)
#define AD1937_DAC_CTRL_0_FMT_TDM_DUAL_LINE                 (3 << 6)

#define AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MASK            (1 << 0)
#define AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MIDCYCLE        (0 << 0)
#define AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_PIPELINE        (1 << 0)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_MASK              (3 << 1)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_64                (0 << 1)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_128               (1 << 1)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_256               (2 << 1)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_512               (3 << 1)
#define AD1937_DAC_CTRL_1_DLRCLK_POLARITY_MASK              (1 << 3)
#define AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_LOW          (0 << 3)
#define AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_HIGH         (1 << 3)
#define AD1937_DAC_CTRL_1_DLRCLK_MASK                       (1 << 4)
#define AD1937_DAC_CTRL_1_DLRCLK_SLAVE                      (0 << 4)
#define AD1937_DAC_CTRL_1_DLRCLK_MASTER                     (1 << 4)
#define AD1937_DAC_CTRL_1_DBCLK_MASK                        (1 << 5)
#define AD1937_DAC_CTRL_1_DBCLK_SLAVE                       (0 << 5)
#define AD1937_DAC_CTRL_1_DBCLK_MASTER                      (1 << 5)
#define AD1937_DAC_CTRL_1_DBCLK_SOURCE_MASK                 (1 << 6)
#define AD1937_DAC_CTRL_1_DBCLK_SOURCE_DBCLK_PIN            (0 << 6)
#define AD1937_DAC_CTRL_1_DBCLK_SOURCE_INTERNAL             (1 << 6)
#define AD1937_DAC_CTRL_1_DBCLK_POLARITY_MASK               (1 << 7)
#define AD1937_DAC_CTRL_1_DBCLK_POLARITY_NORMAL             (0 << 7)
#define AD1937_DAC_CTRL_1_DBCLK_POLARITY_INVERTED           (1 << 7)

#define AD1937_DAC_CTRL_2_MASTER_MASK                       (1 << 0)
#define AD1937_DAC_CTRL_2_MASTER_UNMUTE                     (0 << 0)
#define AD1937_DAC_CTRL_2_MASTER_MUTE                       (1 << 0)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_MASK                   (3 << 1)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_FLAT                   (0 << 1)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_48_KHZ                 (1 << 1)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_44_1_KHZ               (2 << 1)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_32_KHZ                 (3 << 1)
#define AD1937_DAC_CTRL_2_WORD_WIDTH_MASK                   (3 << 3)
#define AD1937_DAC_CTRL_2_WORD_WIDTH_24_BITS                (0 << 3)
#define AD1937_DAC_CTRL_2_WORD_WIDTH_20_BITS                (1 << 3)
#define AD1937_DAC_CTRL_2_WORD_WIDTH_16_BITS                (3 << 3)
#define AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_MASK          (1 << 5)
#define AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_NORMAL        (0 << 5)
#define AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_INVERTED      (1 << 5)

#define AD1937_DAC_MUTE_CTRL_DAC1L_MASK                     (1 << 0)
#define AD1937_DAC_MUTE_CTRL_DAC1L_UNMUTE                   (0 << 0)
#define AD1937_DAC_MUTE_CTRL_DAC1L_MUTE                     (1 << 0)
#define AD1937_DAC_MUTE_CTRL_DAC1R_MASK                     (1 << 1)
#define AD1937_DAC_MUTE_CTRL_DAC1R_UNMUTE                   (0 << 1)
#define AD1937_DAC_MUTE_CTRL_DAC1R_MUTE                     (1 << 1)
#define AD1937_DAC_MUTE_CTRL_DAC2L_MASK                     (1 << 2)
#define AD1937_DAC_MUTE_CTRL_DAC2L_UNMUTE                   (0 << 2)
#define AD1937_DAC_MUTE_CTRL_DAC2L_MUTE                     (1 << 2)
#define AD1937_DAC_MUTE_CTRL_DAC2R_MASK                     (1 << 3)
#define AD1937_DAC_MUTE_CTRL_DAC2R_UNMUTE                   (0 << 3)
#define AD1937_DAC_MUTE_CTRL_DAC2R_MUTE                     (1 << 3)
#define AD1937_DAC_MUTE_CTRL_DAC3L_MASK                     (1 << 4)
#define AD1937_DAC_MUTE_CTRL_DAC3L_UNMUTE                   (0 << 4)
#define AD1937_DAC_MUTE_CTRL_DAC3L_MUTE                     (1 << 4)
#define AD1937_DAC_MUTE_CTRL_DAC3R_MASK                     (1 << 5)
#define AD1937_DAC_MUTE_CTRL_DAC3R_UNMUTE                   (0 << 5)
#define AD1937_DAC_MUTE_CTRL_DAC3R_MUTE                     (1 << 5)
#define AD1937_DAC_MUTE_CTRL_DAC4L_MASK                     (1 << 6)
#define AD1937_DAC_MUTE_CTRL_DAC4L_UNMUTE                   (0 << 6)
#define AD1937_DAC_MUTE_CTRL_DAC4L_MUTE                     (1 << 6)
#define AD1937_DAC_MUTE_CTRL_DAC4R_MASK                     (1 << 7)
#define AD1937_DAC_MUTE_CTRL_DAC4R_UNMUTE                   (0 << 7)
#define AD1937_DAC_MUTE_CTRL_DAC4R_MUTE                     (1 << 7)

#define AD1937_DAC_VOL_CTRL_DAC1L_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC1R_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC2L_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC2R_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC3L_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC3R_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC4L_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC4R_MASK                      (0xff << 0)

#define AD1937_ADC_CTRL_0_PWR_MASK                          (1 << 0)
#define AD1937_ADC_CTRL_0_PWR_ON                            (0 << 0)
#define AD1937_ADC_CTRL_0_PWR_OFF                           (1 << 0)
#define AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_MASK             (1 << 1)
#define AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_OFF              (0 << 1)
#define AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_ON               (1 << 1)
#define AD1937_ADC_CTRL_0_ADC1L_MASK                        (1 << 2)
#define AD1937_ADC_CTRL_0_ADC1L_UNMUTE                      (0 << 2)
#define AD1937_ADC_CTRL_0_ADC1L_MUTE                        (1 << 2)
#define AD1937_ADC_CTRL_0_ADC1R_MASK                        (1 << 3)
#define AD1937_ADC_CTRL_0_ADC1R_UNMUTE                      (0 << 3)
#define AD1937_ADC_CTRL_0_ADC1R_MUTE                        (1 << 3)
#define AD1937_ADC_CTRL_0_ADC2L_MASK                        (1 << 4)
#define AD1937_ADC_CTRL_0_ADC2L_UNMUTE                      (0 << 4)
#define AD1937_ADC_CTRL_0_ADC2L_MUTE                        (1 << 4)
#define AD1937_ADC_CTRL_0_ADC2R_MASK                        (1 << 5)
#define AD1937_ADC_CTRL_0_ADC2R_UNMUTE                      (0 << 5)
#define AD1937_ADC_CTRL_0_ADC2R_MUTE                        (1 << 5)
#define AD1937_ADC_CTRL_0_SAMPLE_RATE_MASK                  (3 << 6)
#define AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ        (0 << 6)
#define AD1937_ADC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ        (1 << 6)
#define AD1937_ADC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ     (2 << 6)

#define AD1937_ADC_CTRL_1_WORD_WIDTH_MASK                   (3 << 0)
#define AD1937_ADC_CTRL_1_WORD_WIDTH_24_BITS                (0 << 0)
#define AD1937_ADC_CTRL_1_WORD_WIDTH_20_BITS                (1 << 0)
#define AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS                (3 << 0)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_MASK                 (7 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_1                    (0 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_0                    (1 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_8                    (2 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_12                   (3 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_16                   (4 << 2)
#define AD1937_ADC_CTRL_1_FMT_MASK                          (3 << 5)
#define AD1937_ADC_CTRL_1_FMT_STEREO                        (0 << 5)
#define AD1937_ADC_CTRL_1_FMT_TDM_SINGLE_LINE               (1 << 5)
#define AD1937_ADC_CTRL_1_FMT_TDM_AUX                       (2 << 5)
#define AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MASK            (1 << 7)
#define AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MIDCYCLE        (0 << 7)
#define AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_PIPELINE        (1 << 7)

#define AD1937_ADC_CTRL_1_ALRCLK_FMT_MASK                   (1 << 0)
#define AD1937_ADC_CTRL_1_ALRCLK_FMT_50_50                  (0 << 0)
#define AD1937_ADC_CTRL_1_ALRCLK_FMT_PULSE                  (1 << 0)
#define AD1937_ADC_CTRL_1_ABCLK_POLARITY_MASK               (1 << 1)
#define AD1937_ADC_CTRL_1_ABCLK_POLARITY_FALLING_EDGE       (0 << 1)
#define AD1937_ADC_CTRL_1_ABCLK_POLARITY_RISING_EDGE        (1 << 1)
#define AD1937_ADC_CTRL_1_ALRCLK_POLARITY_MASK              (1 << 2)
#define AD1937_ADC_CTRL_1_ALRCLK_POLARITY_LEFT_LOW          (0 << 2)
#define AD1937_ADC_CTRL_1_ALRCLK_POLARITY_LEFT_HIGH         (1 << 2)
#define AD1937_ADC_CTRL_1_ALRCLK_MASK                       (1 << 3)
#define AD1937_ADC_CTRL_1_ALRCLK_SLAVE                      (0 << 3)
#define AD1937_ADC_CTRL_1_ALRCLK_MASTER                     (1 << 3)
#define AD1937_ADC_CTRL_1_ABCLK_PER_FRAME_MASK              (3 << 4)
#define AD1937_ADC_CTRL_1_ABCLK_PER_FRAME_64                (0 << 4)
#define AD1937_ADC_CTRL_1_ABCLK_PER_FRAME_128               (1 << 4)
#define AD1937_ADC_CTRL_1_ABCLK_PER_FRAME_256               (2 << 4)
#define AD1937_ADC_CTRL_1_ABCLK_PER_FRAME_512               (3 << 4)
#define AD1937_ADC_CTRL_1_ABCLK_MASK                        (1 << 6)
#define AD1937_ADC_CTRL_1_ABCLK_SLAVE                       (0 << 6)
#define AD1937_ADC_CTRL_1_ABCLK_MASTER                      (1 << 6)
#define AD1937_ADC_CTRL_1_ABCLK_SOURCE_MASK                 (1 << 7)
#define AD1937_ADC_CTRL_1_ABCLK_SOURCE_ABCLK_PIN            (0 << 7)
#define AD1937_ADC_CTRL_1_ABCLK_SOURCE_INTERNAL             (1 << 7)

typedef enum _base_board_type
{
    BASE_BOARD_E1888 = 0,
    BASE_BOARD_E1155
} base_board_type;

#endif /* AD1937_H */
