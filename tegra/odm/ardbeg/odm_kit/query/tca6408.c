/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_services.h"
#include "nvodm_services_dev_i2c.h"
#include "nvodm_query_discovery.h"
#include "tca6408.h"


Tca6408Info Tca6408Data;
Tca6408Info *hTca6408Info;
NvOdmDevI2cHandle hOdmDevI2c;


void Tca6408SetDirectionInput(NvU32 GpioNr)
{
    NvBool ret;

    if (GpioNr >= hTca6408Info->PinCount)

    {
        NvOdmOsPrintf("%s(): The gpio nr %d is more than supported\n",
                       __func__, GpioNr);
        return;
    }

    ret = NvOdmDevI2cSetBits(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_Configuration,
                                  TCA6408_PINMASK(GpioNr));
    if (!ret)
        NvOdmOsPrintf("%s() Error in updating the configuration register\n",
                       __func__);
}

void Tca6408SetDirectionOutput(NvU32 GpioNr, NvU32 Value)
{
    NvBool ret;

    if (GpioNr >= hTca6408Info->PinCount)
    {
        NvOdmOsPrintf("%s(): The gpio pin number %d is more than supported\n",
                                  __func__, GpioNr);
        return;
    }

    if (Value)
        ret = NvOdmDevI2cSetBits(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_Output,
                                  TCA6408_PINMASK(GpioNr));
    else
        ret = NvOdmDevI2cClrBits(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_Output,
                                  TCA6408_PINMASK(GpioNr));

    if (!ret) {
        NvOdmOsPrintf("%s(): Error in updating the output port register\n",
                        __func__);
        return;
    }

    ret = NvOdmDevI2cClrBits(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_Configuration,
                                  TCA6408_PINMASK(GpioNr));
    if (!ret)
        NvOdmOsPrintf("%s(): Error in updating the configuration register\n",
                       __func__);
}

NvU8 Tca6408GetPolarity(NvU32 GpioNr)
{
    NvBool ret;
    NvU8 Value;

    if (GpioNr >= hTca6408Info->PinCount)
    {
        NvOdmOsPrintf("%s(): The gpio nr %d is more than supported\n",
                       __func__, GpioNr);
        return 0;
    }

    ret = NvOdmDevI2cRead8(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_PolarityInversion,
                                  &Value);

    if (!ret)
        NvOdmOsPrintf("%s() Error in updating the polarity port register\n",
                       __func__);
    return (Value & TCA6408_PINMASK(GpioNr)) ? 1 : 0 ;

}

void Tca6408SetPolarity(NvU32 GpioNr, NvU32 Value)
{
    NvBool ret;

    if (GpioNr >= hTca6408Info->PinCount)
    {
        NvOdmOsPrintf("%s(): The gpio nr %d is more than supported\n",
               __func__, GpioNr);
        return;
    }
    if (Value)
        ret = NvOdmDevI2cSetBits(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_PolarityInversion,
                                  TCA6408_PINMASK(GpioNr));
    else
        ret = NvOdmDevI2cClrBits(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_PolarityInversion,
                                  TCA6408_PINMASK(GpioNr));
    if (!ret)
        NvOdmOsPrintf("%s() Error in updating the polarity register\n",
                                __func__);
}

NvU8 Tca6408GetInput(NvU32 GpioNr)
{
    NvBool ret;
    NvU8 Value;

    if (GpioNr >= hTca6408Info->PinCount)
    {
        NvOdmOsPrintf("%s(): The gpio nr %d is more than supported\n",
                       __func__, GpioNr);
        return 0;
    }

    ret = NvOdmDevI2cRead8(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_Input,
                                  &Value);
    if (!ret)
        NvOdmOsPrintf("%s() Error in updating the input port register\n",
                       __func__);
    return (Value & TCA6408_PINMASK(GpioNr)) ? 1 : 0 ;
}

NvU8 Tca6408GetOutput(NvU32 GpioNr)
{
    NvBool ret;
    NvU8 Value;

    if (GpioNr >= hTca6408Info->PinCount)
    {
        NvOdmOsPrintf("%s(): The gpio nr %d is more than supported\n",
                       __func__, GpioNr);
        return 0;
    }

    ret = NvOdmDevI2cRead8(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_Output,
                                  &Value);

    if (!ret)
        NvOdmOsPrintf("%s() Error in updating the output port register\n",
                       __func__);
    return (Value & TCA6408_PINMASK(GpioNr)) ? 1 : 0 ;

}


NvBool Tca6408Open(NvU32 Instance)
{
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;

    hOdmDevI2c = NvOdmDevI2cOpen(I2cModule, Instance);

    if (!hOdmDevI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        return NV_FALSE;
    }
    hTca6408Info = &Tca6408Data;

    // set device address and speed
    hTca6408Info->DeviceAddress = TCA6408_I2C_DEVICE_ADDRESS;
    hTca6408Info->SpeedInKHz = TCA6408_I2C_SPEED_KHZ;
    hTca6408Info->PinCount = TCA6408_NUM_PINS;

    // set register address
    hTca6408Info->Reg_Input = TCA6408_REG_INPUT_PORT;
    hTca6408Info->Reg_Output = TCA6408_REG_OUTPUT_PORT;
    hTca6408Info->Reg_PolarityInversion = TCA6408_REG_POLATIRY_INVERSION;
    hTca6408Info->Reg_Configuration = TCA6408_REG_CONFIGURATION;

    // keep all pins in input
    if (!NvOdmDevI2cSetBits(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_Input,
                                  0xFF))
        return NV_FALSE;
    return NV_TRUE;
}

void Tca6408Close(void)
{
    // keep all pins in input
    NvOdmDevI2cSetBits(hOdmDevI2c, hTca6408Info->DeviceAddress,
                                  hTca6408Info->SpeedInKHz,
                                  hTca6408Info->Reg_Input,
                                  0xFF);

    NvOdmDevI2cClose(hOdmDevI2c);
}
