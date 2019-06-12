/*
 * Copyright (c) 2009-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include <stdio.h>
#include <stdlib.h>
#include "i2c-util.h"
#include "nvos.h"
#include "codec.h"
#include "ad1937.h"
#include "max9485.h"

#define I2C_DEVICE "/dev/i2c-0"
#define I2C_M_WR        0

/**
 * MAX9485
 */
static NvU8 max9485_enable =
    MAX9485_MCLK_DISABLE |
    MAX9485_CLK_OUT1_ENABLE |
    MAX9485_CLK_OUT2_DISABLE |
    MAX9485_SAMPLING_RATE_DOUBLED |
    MAX9485_OUTPUT_SCALING_FACTOR_256;

static const NvU8 max9485_disable =
    MAX9485_MCLK_DISABLE |
    MAX9485_CLK_OUT1_DISABLE |
    MAX9485_CLK_OUT2_DISABLE;

/**
 * AD1937
 */
static NvU8 ad1937_i2s1_enable[17] =
{
    // AD1937_PLL_CLK_CTRL_0
    AD1937_PLL_CLK_CTRL_0_PWR_ON |
    AD1937_PLL_CLK_CTRL_0_MCLKI_512 |
    AD1937_PLL_CLK_CTRL_0_MCLKO_512 |
    AD1937_PLL_CLK_CTRL_0_PLL_INPUT_MCLKI |
    AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_ENABLE,

    // AD1937_PLL_CLK_CTRL_1
    AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK |
    AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK |
    AD1937_PLL_CLK_CTRL_1_VREF_DISABLE,

    // AD1937_DAC_CTRL_0
    AD1937_DAC_CTRL_0_PWR_ON |
    AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ |
    AD1937_DAC_CTRL_0_DSDATA_DELAY_1 |
    AD1937_DAC_CTRL_0_FMT_STEREO,

    // AD1937_DAC_CTRL_1
    AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MIDCYCLE |
    AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_64 |
    AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_LOW |
    AD1937_DAC_CTRL_1_DLRCLK_MASTER |
    AD1937_DAC_CTRL_1_DBCLK_MASTER |
    AD1937_DAC_CTRL_1_DBCLK_SOURCE_INTERNAL |
    AD1937_DAC_CTRL_1_DBCLK_POLARITY_NORMAL,

    // AD1937_DAC_CTRL_2
    AD1937_DAC_CTRL_2_MASTER_UNMUTE |
    AD1937_DAC_CTRL_2_DEEMPHASIS_FLAT |
    AD1937_DAC_CTRL_2_WORD_WIDTH_16_BITS |          // sample size
    AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_NORMAL,

    // AD1937_DAC_MUTE_CTRL
    AD1937_DAC_MUTE_CTRL_DAC1L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC1R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC2L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC2R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC3L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC3R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC4L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC4R_UNMUTE,

    // AD1937_DAC_VOL_CTRL_DAC1L
    0,

    // AD1937_DAC_VOL_CTRL_DAC1R
    0,

    // AD1937_DAC_VOL_CTRL_DAC2L
    0,

    // AD1937_DAC_VOL_CTRL_DAC2R
    0,

    // AD1937_DAC_VOL_CTRL_DAC3L
    0,

    // AD1937_DAC_VOL_CTRL_DAC3R
    0,

    // AD1937_DAC_VOL_CTRL_DAC4L
    0,

    // AD1937_DAC_VOL_CTRL_DAC4R
    0,

    // AD1937_ADC_CTRL_0
    AD1937_ADC_CTRL_0_PWR_ON |
    AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_OFF |
    AD1937_ADC_CTRL_0_ADC1L_UNMUTE |
    AD1937_ADC_CTRL_0_ADC1R_UNMUTE |
    AD1937_ADC_CTRL_0_ADC2L_UNMUTE |
    AD1937_ADC_CTRL_0_ADC2R_UNMUTE |
    AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ,

    // AD1937_ADC_CTRL_1
    AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS |          // sample size
    AD1937_ADC_CTRL_1_ASDATA_DELAY_1 |
    AD1937_ADC_CTRL_1_FMT_STEREO |
    AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MIDCYCLE,

    // AD1937_ADC_CTRL_2
    AD1937_ADC_CTRL_1_ALRCLK_FMT_50_50 |
    AD1937_ADC_CTRL_1_ABCLK_POLARITY_FALLING_EDGE |
    AD1937_ADC_CTRL_1_ALRCLK_POLARITY_LEFT_LOW |
    AD1937_ADC_CTRL_1_ALRCLK_SLAVE |
    AD1937_ADC_CTRL_1_ABCLK_PER_FRAME_64 |
    AD1937_ADC_CTRL_1_ABCLK_SLAVE |
    AD1937_ADC_CTRL_1_ABCLK_SOURCE_ABCLK_PIN
};
static NvU8 ad1937_i2s2_enable[17] =
{
    // AD1937_PLL_CLK_CTRL_0
    AD1937_PLL_CLK_CTRL_0_PWR_ON |
    AD1937_PLL_CLK_CTRL_0_MCLKI_512 |
    AD1937_PLL_CLK_CTRL_0_MCLKO_OFF |
    AD1937_PLL_CLK_CTRL_0_PLL_INPUT_DLRCLK |
    AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_ENABLE,

    // AD1937_PLL_CLK_CTRL_1
    AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK |
    AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK |
    AD1937_PLL_CLK_CTRL_1_VREF_ENABLE,

    // AD1937_DAC_CTRL_0
    AD1937_DAC_CTRL_0_PWR_ON |
    AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ |
    AD1937_DAC_CTRL_0_DSDATA_DELAY_0 |
    AD1937_DAC_CTRL_0_FMT_STEREO,

    // AD1937_DAC_CTRL_1
    AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MIDCYCLE |
    AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_64 |
    AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_HIGH |
    AD1937_DAC_CTRL_1_DLRCLK_SLAVE |
    AD1937_DAC_CTRL_1_DBCLK_SLAVE |
    AD1937_DAC_CTRL_1_DBCLK_SOURCE_DBCLK_PIN |
    AD1937_DAC_CTRL_1_DBCLK_POLARITY_INVERTED,

    // AD1937_DAC_CTRL_2
    AD1937_DAC_CTRL_2_MASTER_UNMUTE |
    AD1937_DAC_CTRL_2_DEEMPHASIS_44_1_KHZ |
    AD1937_DAC_CTRL_2_WORD_WIDTH_16_BITS |          // sample size
    AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_NORMAL,

    // AD1937_DAC_MUTE_CTRL
    AD1937_DAC_MUTE_CTRL_DAC1L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC1R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC2L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC2R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC3L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC3R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC4L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC4R_UNMUTE,

    // AD1937_DAC_VOL_CTRL_DAC1L
    0,

    // AD1937_DAC_VOL_CTRL_DAC1R
    0,

    // AD1937_DAC_VOL_CTRL_DAC2L
    0,

    // AD1937_DAC_VOL_CTRL_DAC2R
    0,

    // AD1937_DAC_VOL_CTRL_DAC3L
    0,

    // AD1937_DAC_VOL_CTRL_DAC3R
    0,

    // AD1937_DAC_VOL_CTRL_DAC4L
    0,

    // AD1937_DAC_VOL_CTRL_DAC4R
    0,

    // AD1937_ADC_CTRL_0
    AD1937_ADC_CTRL_0_PWR_ON |
    AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_OFF |
    AD1937_ADC_CTRL_0_ADC1L_UNMUTE |
    AD1937_ADC_CTRL_0_ADC1R_UNMUTE |
    AD1937_ADC_CTRL_0_ADC2L_UNMUTE |
    AD1937_ADC_CTRL_0_ADC2R_UNMUTE |
    AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ,

    // AD1937_ADC_CTRL_1
    AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS |          // sample size
    AD1937_ADC_CTRL_1_ASDATA_DELAY_1 |
    AD1937_ADC_CTRL_1_FMT_STEREO |
    AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MIDCYCLE,

    // AD1937_ADC_CTRL_2
    AD1937_ADC_CTRL_1_ALRCLK_FMT_50_50 |
    AD1937_ADC_CTRL_1_ABCLK_POLARITY_FALLING_EDGE |
    AD1937_ADC_CTRL_1_ALRCLK_POLARITY_LEFT_LOW |
    AD1937_ADC_CTRL_1_ALRCLK_SLAVE |
    AD1937_ADC_CTRL_1_ABCLK_PER_FRAME_64 |
    AD1937_ADC_CTRL_1_ABCLK_SLAVE |
    AD1937_ADC_CTRL_1_ABCLK_SOURCE_ABCLK_PIN
};

static const NvU8 ad1937_tdm_enable[17] =
{
    // AD1937_PLL_CLK_CTRL_0
    AD1937_PLL_CLK_CTRL_0_PWR_OFF |
    AD1937_PLL_CLK_CTRL_0_MCLKI_512 |
    AD1937_PLL_CLK_CTRL_0_MCLKO_512 |
    AD1937_PLL_CLK_CTRL_0_PLL_INPUT_MCLKI |
    AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_ENABLE,

    // AD1937_PLL_CLK_CTRL_1
    AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK |
    AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK |
    AD1937_PLL_CLK_CTRL_1_VREF_DISABLE,

    // AD1937_DAC_CTRL_0
    AD1937_DAC_CTRL_0_PWR_ON |
    AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ |
    AD1937_DAC_CTRL_0_DSDATA_DELAY_0 |
    AD1937_DAC_CTRL_0_FMT_TDM_SINGLE_LINE,          // TDM

    // AD1937_DAC_CTRL_1
    AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MIDCYCLE |
    AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_256 |         // TDM
    AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_HIGH |   // TDM
    AD1937_DAC_CTRL_1_DLRCLK_MASTER |
    AD1937_DAC_CTRL_1_DBCLK_MASTER |
    AD1937_DAC_CTRL_1_DBCLK_SOURCE_INTERNAL |
    AD1937_DAC_CTRL_1_DBCLK_POLARITY_INVERTED,      // TDM

    // AD1937_DAC_CTRL_2
    AD1937_DAC_CTRL_2_MASTER_UNMUTE |
    AD1937_DAC_CTRL_2_DEEMPHASIS_FLAT |
    AD1937_DAC_CTRL_2_WORD_WIDTH_16_BITS |          // sample size
    AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_NORMAL,

    // AD1937_DAC_MUTE_CTRL
    AD1937_DAC_MUTE_CTRL_DAC1L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC1R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC2L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC2R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC3L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC3R_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC4L_UNMUTE |
    AD1937_DAC_MUTE_CTRL_DAC4R_UNMUTE,

    // AD1937_DAC_VOL_CTRL_DAC1L
    0,

    // AD1937_DAC_VOL_CTRL_DAC1R
    0,

    // AD1937_DAC_VOL_CTRL_DAC2L
    0,

    // AD1937_DAC_VOL_CTRL_DAC2R
    0,

    // AD1937_DAC_VOL_CTRL_DAC3L
    0,

    // AD1937_DAC_VOL_CTRL_DAC3R
    0,

    // AD1937_DAC_VOL_CTRL_DAC4L
    0,

    // AD1937_DAC_VOL_CTRL_DAC4R
    0,

    // AD1937_ADC_CTRL_0
    AD1937_ADC_CTRL_0_PWR_ON |
    AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_ON |
    AD1937_ADC_CTRL_0_ADC1L_UNMUTE |
    AD1937_ADC_CTRL_0_ADC1R_UNMUTE |
    AD1937_ADC_CTRL_0_ADC2L_UNMUTE |
    AD1937_ADC_CTRL_0_ADC2R_UNMUTE |
    AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ,

    // AD1937_ADC_CTRL_1
    AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS |          // sample size
    AD1937_ADC_CTRL_1_ASDATA_DELAY_0 |
    AD1937_ADC_CTRL_1_FMT_TDM_SINGLE_LINE |         // TDM
    AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MIDCYCLE,   // TDM

    // AD1937_ADC_CTRL_2
    AD1937_ADC_CTRL_1_ALRCLK_FMT_50_50 |
    AD1937_ADC_CTRL_1_ABCLK_POLARITY_RISING_EDGE |  // TDM
    AD1937_ADC_CTRL_1_ALRCLK_POLARITY_LEFT_HIGH |   // TDM
    AD1937_ADC_CTRL_1_ALRCLK_SLAVE |
    AD1937_ADC_CTRL_1_ABCLK_PER_FRAME_256 |         // TDM
    AD1937_ADC_CTRL_1_ABCLK_SLAVE |
    AD1937_ADC_CTRL_1_ABCLK_SOURCE_ABCLK_PIN
};

static const NvU8 ad1937_disable[17] =
{
    // AD1937_PLL_CLK_CTRL_0
    AD1937_PLL_CLK_CTRL_0_PWR_OFF,

    // AD1937_PLL_CLK_CTRL_1
    0,

    // AD1937_DAC_CTRL_0
    AD1937_DAC_CTRL_0_PWR_OFF,

    // AD1937_DAC_CTRL_1
    0,

    // AD1937_DAC_CTRL_2
    0,

    // AD1937_DAC_MUTE_CTRL
    0,

    // AD1937_DAC_VOL_CTRL_DAC1L
    0,

    // AD1937_DAC_VOL_CTRL_DAC1R
    0,

    // AD1937_DAC_VOL_CTRL_DAC2L
    0,

    // AD1937_DAC_VOL_CTRL_DAC2R
    0,

    // AD1937_DAC_VOL_CTRL_DAC3L
    0,

    // AD1937_DAC_VOL_CTRL_DAC3R
    0,

    // AD1937_DAC_VOL_CTRL_DAC4L
    0,

    // AD1937_DAC_VOL_CTRL_DAC4R
    0,

    // AD1937_ADC_CTRL_0
    AD1937_ADC_CTRL_0_PWR_OFF,

    // AD1937_ADC_CTRL_1
    0,

    // AD1937_ADC_CTRL_2
    0
};

NvError
CodecOpen(int fd,
          CodecMode mode,
          int sampleRate,
          int sku_id)
{
    NvU8    i;
    NvError err = NvSuccess;


    if (mode != CodecMode_I2s1 &&
        mode != CodecMode_I2s2 &&
        mode != CodecMode_Tdm1 &&
        mode != CodecMode_Tdm2 )
    {
        fprintf(stderr, "%s() Error: Bad paramerter. Invalid code mode.\n", __func__);
        return NvError_BadParameter;
    }

    // Program MAX9485
    switch (sampleRate)
    {
        case 48:
        {
            max9485_enable |= MAX9485_SAMPLING_FREQUENCY_48KHZ;
        }
        break;

        case 44:
        default:
        {
            max9485_enable |= MAX9485_SAMPLING_FREQUENCY_44_1KHZ;
        }
        break;
    }

    err = i2c_write(fd, MAX9485_I2C_ADDR, max9485_enable);
    if (err)
    {
        fprintf(stderr, "%s() Error: I2C write on MAX9485 failed.\n", __func__);
        return err;
    }

    //Operate in RJM mode
    if (sku_id == 13 )
    {
         ad1937_i2s1_enable[2]  = ad1937_i2s2_enable[2]   = AD1937_DAC_CTRL_0_PWR_ON |
                                                            AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ |
                                                            AD1937_DAC_CTRL_0_DSDATA_DELAY_16 |
                                                            AD1937_DAC_CTRL_0_FMT_STEREO;

         ad1937_i2s1_enable[15] = ad1937_i2s2_enable[15]  = AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS |
                                                            AD1937_ADC_CTRL_1_ASDATA_DELAY_16 |
                                                            AD1937_ADC_CTRL_1_FMT_STEREO |
                                                            AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MIDCYCLE;
    }

    // Program AD1937
    if (mode == CodecMode_I2s1)
    {
        for (i = 0; i < 17; ++i)
        {
            err = i2c_write_subaddr(fd, AD1937_I2C_ADDR, i, ad1937_i2s1_enable[i]);
            if (err)
            {
                fprintf(stderr, "%s() Error: I2C write on AD1937 failed for codec mode %d.\n", __func__, mode);
                return err;
            }
        }
    }
    else if (mode == CodecMode_I2s2)
    {
        for (i = 0; i < 17; ++i)
        {
            err = i2c_write_subaddr(fd, AD1937_I2C_ADDR, i, ad1937_i2s2_enable[i]);
            if (err)
            {
                fprintf(stderr, "%s() Error: I2C write on AD1937 failed for codec mode %d.\n", __func__, mode);
                return err;
            }
        }
    }
    else
    {
        for (i = 0; i < 17; ++i)
        {
            err = i2c_write_subaddr(fd, AD1937_I2C_ADDR, i, ad1937_tdm_enable[i]);
            if (err)
            {
                fprintf(stderr, "%s() Error: I2C write on AD1937 failed for codec mode %d.\n", __func__, mode);
                return err;
            }
        }
    }

    return NvSuccess;

}

void
CodecClose(
    int fd
    )
{
    int i;
    {
        // Program AD1937
        for (i = 0; i < 17; ++i)
        {
            i2c_write_subaddr(fd, AD1937_I2C_ADDR, i, ad1937_disable[i]);
        }

        // Program MAX9485
        i2c_write(fd, MAX9485_I2C_ADDR, max9485_disable);

    }
}

NvError
CodecDump(
    int fd
    )
{
    int     i;
    NvU8    val;
    NvError err;

    // Read MAX9485
    err = i2c_read(fd, MAX9485_I2C_ADDR, &val);
    if (err)
    {
        return err;
    }
    printf("MAX9485: 0x%.2x\n", val);

    // Read AD1937
    printf("AD1937 : ");
    for (i = 0; i < 17; ++i)
    {
        err = i2c_read_subaddr(fd, AD1937_I2C_ADDR, i, &val);
        if (err)
        {
            return err;
        }

        printf("0x%.2x ", val);
    }
    printf("\n");

    return NvSuccess;
}
