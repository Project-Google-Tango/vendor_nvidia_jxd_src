/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database Peripheral
 *                 entries for the peripherals on E1612 module.
 */

#define CAMERA_DEVICE_DEFAULT_EDP(name) \
{ \
    .States = name ## _EDPStates, \
    .Num = NV_ARRAY_SIZE(name ## _EDPStates), \
    .E0Index = NV_ARRAY_SIZE(name ## _EDPStates) - 1, \
    .Priority = EDP_MAX_PRIO + 1, \
}

#define CAMERA_I2C2_DETECT_DESC_DEFAULT(index, type, name, guid, pos, slv, rl, pg, gp, did, altn) \
{ \
    .ChipName = "pcl_"#name, \
    .Index = index, \
    .Type = type, \
    .Device = { \
        .GUID = guid, \
        .Present = 0, \
        .Position = pos, \
        .DevInfo = { \
            .BusType = CAMERA_DEVICE_TYPE_I2C, \
            .BusNum = 2, \
            .Slave = slv, \
            }, \
        .RegLength = rl, \
        .PinMuxGrp = pg, \
        .GpioUsed = gp, \
        .RegulatorNum = NV_ARRAY_SIZE(name ## _RegName), \
        .RegNames = name ## _RegName, \
        .PowerOnSequenceNum = NV_ARRAY_SIZE(name ## _PwrOnSeq), \
        .PowerOnSequence = name ## _PwrOnSeq, \
        .PowerOffSequenceNum = NV_ARRAY_SIZE(name ## _PwrOffSeq), \
        .PowerOffSequence = name ## _PwrOffSeq, \
        .ClockNum = NV_ARRAY_SIZE(name ## _ClkName), \
        .ClockNames = name ## _ClkName, \
        .Edp = CAMERA_DEVICE_DEFAULT_EDP(name), \
        .AlterName = altn, \
        .Detect = name ## _Detect, \
        .DevId = did, \
        }, \
    .Connects = NULL, \
    .isOldNvOdmDriver = NV_TRUE, \
}

#define CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(index, name, guid, slv, gp, did, altn) \
    CAMERA_I2C2_DETECT_DESC_DEFAULT(index, PCLDeviceType_Sensor, name, guid, 0, slv, 2, 0xffff, gp, did, altn)

#define CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(index, name, guid, slv, gp, did, altn) \
    CAMERA_I2C2_DETECT_DESC_DEFAULT(index, PCLDeviceType_Sensor, name, guid, 1, slv, 2, 0xffff, gp, did, altn)

#define CAMERA_FOCUSER_I2C2_DETECT_DESC_DEFAULT(index, name, guid, slv, gp, did, altn) \
    CAMERA_I2C2_DETECT_DESC_DEFAULT(index, PCLDeviceType_Focuser, name, guid, 0, slv, 1, 0xffff, gp, did, altn)

#define CAMERA_FLASH_I2C2_DETECT_DESC_DEFAULT(index, name, guid, slv, gp, did, altn) \
    CAMERA_I2C2_DETECT_DESC_DEFAULT(index, PCLDeviceType_Flash, name, guid, 0, slv, 1, 0xffff, gp, did, altn)

#define CAMERA_RST_GPIO     219
#define CAMERA1_PWDN_GPIO   221
#define CAMERA2_PWDN_GPIO   222
#define CAMERA_AF_PWDN      223

// OV7695 device description
static const char *OV7695_ClkName[] = {"mclk"};
static const char *OV7695_RegName[] = {"vana", "vif2"};

static PCLHwReg OV7695_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA2_PWDN_GPIO}, // cam rst pin
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_WAIT_MS, 40},
    {CAMERA_TABLE_GPIO_ACT, CAMERA2_PWDN_GPIO}, // cam rst pin
    {CAMERA_TABLE_WAIT_MS, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg OV7695_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA2_PWDN_GPIO}, // cam rst pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect OV7695_Detect[] =
{
    {0x02, 0x300A, 0xFFFF, 0x7695},
    {0, 0, 0, 0},
};

static NvU32 OV7695_EDPStates[] = { };

static DeviceProfile s_CameraDevices[] =
{
    CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(
        1, OV7695, SENSOR_YUV_OV7695_GUID, 0x21, 3, 0x7695, "ov7695"),
};
