/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 
#ifndef ADS7846_REG_HEADER
#define ADS7846_REG_HEADER

#if defined(__cplusplus)
extern "C"
{
#endif


// Input Configuration (DIN)
/* Single-Ended Reference Mode (SER/#DRF HIGH)
 * --------------------------------------------------------------------------
 * | A2 | A1 | A0 | Y-Pos | X-Pos | Z1-Pos | Z2-Pos | X-Drivers | Y-Drivers |
 * --------------------------------------------------------------------------
 * | 0  | 0  | 0  |       |       |        |        |    off    |    off    |
 * --------------------------------------------------------------------------
 * | 0  | 0  | 1  |Measure|       |        |        |    off    |    ON     |
 * --------------------------------------------------------------------------
 * | 0  | 1  | 0  |       |       |        |        |    off    |    off    |
 * --------------------------------------------------------------------------
 * | 0  | 1  | 1  |       |       |Measure |        |  X-, ON   |  Y+, ON   |
 * --------------------------------------------------------------------------
 * | 1  | 0  | 0  |       |       |        |Measure |  X-, ON   |  Y+, ON   |
 * --------------------------------------------------------------------------
 * | 1  | 0  | 1  |       |Measure|        |        |    ON     |    off    |
 * --------------------------------------------------------------------------
 * | 1  | 1  | 0  |       |       |        |        |    off    |    off    |
 * --------------------------------------------------------------------------
 * | 1  | 1  | 1  |       |       |        |        |    off    |    off    |
 * --------------------------------------------------------------------------
 *
 * Differential Reference Mode (SER/#DFR LOW) - Optimal Touch Screnn Perfomance
 * -------------------------------------------------------------
 * | A2 | A1 | A0 | Y-Pos | X-Pos | Z1-Pos | Z2-Pos |Drivers ON|
 * -------------------------------------------------------------
 * | 0  | 0  | 1  |Measure|       |        |        |   Y+, Y- |   
 * -------------------------------------------------------------
 * | 0  | 1  | 1  |       |       |Measure |        |   Y+, X- |
 * -------------------------------------------------------------
 * | 1  | 0  | 0  |       |       |        |Measure |   Y+, X- |
 * -------------------------------------------------------------
 * | 1  | 0  | 1  |       |Measure|        |        |   X+, X- | 
 * -------------------------------------------------------------
 * 
 * ADS7846 utilize command-based rather than register-based operation
 * to control touch device.
 *
 * Control Byte
 * --------------------------------------------------------------
 * |Bit 7| Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2  | Bit 1 | Bit 0|
 * |(MSB)|       |       |       |       |        |       | (LSB)|
 * --------------------------------------------------------------
 * |  S  |  A2   |  A1   |  A0   | MODE  |SER/#DFR|  PD1  | PD0  |
 * ---------------------------------------------------------------
 * S: Start Bit. Control byte start with first high bit on DIN. A new control
 *    byte can start every 15th clock cycle in 12-bit conversion mode or every 
 *    11th clock cycle in 8-bit conversion mode.
 * A2-A0: Channel Select Bits. Along with SER/#DFR bit, these bits control the
 *        setting of the multiplexer input, touch driver switches, and reference
 *        input (see above table.)
 * MODE: 12-Bit(LOW)/8-Bit(HIGH) conversion select bit.
 * SER/#DFR: Single Ended(HIGH)/Differential Reference(LOW) Mode select bit.
 * PD1-PD0: Power Down Mode Select Bit.
 * ----------------------------------------------------------------------------
 * |PD1|PD0|#PENIRQ|                        Description                       |
 * ----------------------------------------------------------------------------
 * | 0 | 0 |Enabled|Power-Down Between Conversions. When each conversion is   |
 * |   |   |       |finished, the converter enters a low-power mode. At the   |
 * |   |   |       |start of the next conversion, the device instantly powers |
 * |   |   |       |up to full power. There is no need for additional delay   |
 * |   |   |       |to assure full operation and the very first conversion is |
 * |   |   |       |valid. The Y- switch is on when in power-down.            |
 * ----------------------------------------------------------------------------
 * | 0 | 1 |Disabled|Reference is off and ADC is on                           |
 * ----------------------------------------------------------------------------
 * | 1 | 0 |Enabled|Reference is on and ADC is off                            |
 * ----------------------------------------------------------------------------
 * | 1 | 1 |Disabled|Device is always powered. Reference is on and ADC is on  |
 * ----------------------------------------------------------------------------
 *
 * Basic Driver Operation
 * On Receive interrupt (PEN UP/PEN DOWN/PEN TIMER)
 * If PEN DOWN
 *  Sample touch screen
 *  Set edge detection to rising edge
 *  Return coordinates/tip state = PEN DOWN
 * If PEN TIMER
 *  Sample touch screen
 *  Return coordinates/tip state = PEN DOWN
 * If PEN UP
 *  Set edge detection to falling edge
 *  Return tip state = PEN UP
 *
 * 24 clock-per-conversion x 1 samples
 * CLK| 8 |    8    |    8    |
 *  WR|CMD|    0    |    0    |
 *  RD| 0 |DATA[6:0]|DATA[7:3]|
 *
 * 16 clock-per-conversion
 * CLK| 8 |    8    |    8    |    8    |    8    |    8    |    8    |    8    |
 *  WR|CMD|    0    |   CMD   |    0    |   CMD   |    0    |CMD_PWRDN|    0    |
 *  RD| 0 |DATA[6:0]|DATA[7:3]|DATA[6:0]|DATA[7:3]|DATA[6:0]|DATA[7:3]|    0    |
 *
 * 15 clock-per-conversion
 * CLK|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|
 *  WR|    CMD        |0  0  0  0  0  0  0|CMD            |0  0  0  0  0  0  0|
 *  RD|0 0 0 0 0 0 0 0 0|11|10| 9| 8| 7| 6|5|4|3|2|1|0| | |0|11|10| 9| 8| 7| 6
*/

#define ADS_START               (1 << 7)
#define ADS_A2A1A0_READ_Y          (1 << 4)        /* read y position */
#define ADS_A2A1A0_READ_Z1         (3 << 4)        /* read z1 position */
#define ADS_A2A1A0_READ_Z2         (4 << 4)        /* read z2 position */
#define ADS_A2A1A0_READ_X          (5 << 4)        /* read x position */
#define ADS_MODE_8_BIT               (1 << 3)
#define ADS_MODE_12_BIT              (0 << 3)
#define ADS_SER                 (1 << 2)            /* single ended */
#define ADS_DFR                 (0 << 2)        /* differential */
#define ADS_PD10_PDOWN          (0 << 0)        /* lowpower mode + penirq */
#define ADS_PD10_ADC_ON         (1 << 0)        /* ADC on */
#define ADS_PD10_REF_ON         (2 << 0)        /* vREF on + penirq */
#define ADS_PD10_ALL_ON         (3 << 0)        /* ADC + vREF on */
 
#define MAX_12BIT       ((1<<12)-1)
 
/* leave ADC powered up (disables penirq) between differential samples */
#define READ_12BIT_DFR(x, adc, vref) (ADS_START | ADS_A2A1A0_d_ ## x \
        | ADS_12_BIT | ADS_DFR | \
         (adc ? ADS_PD10_ADC_ON : 0) | (vref ? ADS_PD10_REF_ON : 0))
 
#define READ_Y(vref)    (READ_12BIT_DFR(y,  1, vref))
#define READ_Z1(vref)   (READ_12BIT_DFR(z1, 1, vref))
#define READ_Z2(vref)   (READ_12BIT_DFR(z2, 1, vref))
 
#define READ_X(vref)    (READ_12BIT_DFR(x,  1, vref))
#define PWRDOWN         (READ_12BIT_DFR(y,  0, 0))      /* LAST */
 
/* single-ended samples need to first power up reference voltage;
 * we leave both ADC and VREF powered
 */
#define READ_12BIT_SER(x) (ADS_START | ADS_A2A1A0_ ## x \
         | ADS_12_BIT | ADS_SER)
 
#define REF_ON  (READ_12BIT_DFR(x, 1, 1))
#define REF_OFF (READ_12BIT_DFR(y, 0, 0))

#if defined(__cplusplus)
}
#endif


#endif //ADS7846_REG_HEADER


