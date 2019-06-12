/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#ifndef INCLUDED_TCA6408_H
#define INCLUDED_TCA6408_H


#if defined(__cplusplus)
extern "C"
{
#endif


typedef struct Tca6408InfoRec
{
    NvU8 DeviceAddress;
    NvU32 SpeedInKHz;
    NvU8 PinCount;
    NvU8 Reg_Input;
    NvU8 Reg_Output;
    NvU8 Reg_PolarityInversion;
    NvU8 Reg_Configuration;
}Tca6408Info;


#define TCA6408_I2C_DEVICE_ADDRESS         0x42
#define TCA6408_I2C_SPEED_KHZ               100

#define TCA6408_NUM_PINS                      8
#define TCA6408_REG_INPUT_PORT             0x00
#define TCA6408_REG_OUTPUT_PORT            0x01
#define TCA6408_REG_POLATIRY_INVERSION     0x02
#define TCA6408_REG_CONFIGURATION          0x03

#define TCA6408_PIN_INDEX(x)                  x
#define TCA6408_GPIO_PIN_LOW                  0
#define TCA6408_GPIO_PIN_HIGH                 1

#define TCA6408_PINMASK(index)   (1 << (index % TCA6408_NUM_PINS))


void Tca6408SetDirectionInput(NvU32 GpioNr);
void Tca6408SetDirectionOutput(NvU32 GpioNr, NvU32 Value);
NvU8 Tca6408GetInput(NvU32 GpioNr);
NvU8 Tca6408GetOutput(NvU32 GpioNr);
NvU8 Tca6408GetPolarity(NvU32 GpioNr);
void Tca6408SetPolarity(NvU32 GpioNr, NvU32 Value);
NvBool Tca6408Open(NvU32 Instance);
void Tca6408Close(void);


#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_TCA6408_H */
