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

#define ENABLE_MORE_FLASH_DEVICES

static const char *OV5640_ClkName[] = {"default_mclk"};
static const char *OV5640_RegName[] = {"avdd", "dvdd"};
static PCLHwReg OV5640_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_ACT, 147}, // cam2_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, 144}, // cam_reset
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_GPIO_ACT, 144}, // cam_reset
    {CAMERA_TABLE_GPIO_DEACT, 147}, // cam2_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg OV5640_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, 144}, // cam_reset
    {CAMERA_TABLE_GPIO_ACT, 147}, // cam2_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect OV5640_Detect[] =
{
    {0x02, 0x300A, 0xFFFF, 0x5640},
    {0, 0, 0, 0},
};

static NvU32 OV5640_EDPStates[] = { };

static const char *OV5693_ClkName[] = {"default_mclk"};
static const char *OV5693_RegName[] = {"avdd_ov5693", "dvdd", "dovdd"};
static PCLHwReg OV5693_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_GPIO_ACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg OV5693_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect OV5693_Detect[] =
{
    {0x02, 0x300A, 0xFFFF, 0x5690},
    {0, 0, 0, 0},
};

static NvU32 OV5693_EDPStates[] = { };

static const char *AR0833_ClkName[] = {"default_mclk"};
static const char *AR0833_RegName[] = {"vana", "vdig", "vif"};
static PCLHwReg AR0833_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, 144}, // cam_reset
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_GPIO_ACT, 144}, // cam_reset
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg AR0833_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, 146}, // cam_reset
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect AR0833_Detect[] =
{
    {0x02, 0x0000, 0xFFFF, 0x4B03},
    {0, 0, 0, 0},
};

static NvU32 AR0833_EDPStates[] = { };

static const char *IMX135_ClkName[] = {"default_mclk"};
static const char *IMX135_RegName[] = {"vana", "vdig", "vif"};
static PCLHwReg IMX135_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, 144}, // cam_reset
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_GPIO_ACT, 144}, // cam_reset
    {CAMERA_TABLE_GPIO_ACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_US, 300},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg IMX135_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, 144}, // cam_reset
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect IMX135_Detect[] =
{
    {0x02, 0x0016, 0xFFFF, 0x0135 },
    {0, 0, 0, 0},
};

static NvU32 IMX135_EDPStates[] = { };

// IMX132 device description
static const char *IMX132_ClkName[] = {"default_mclk"};
static const char *IMX132_RegName[] = {"vana_imx132", "vdig", "vif"};

static PCLHwReg IMX132_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, 147}, // cam2_pwdn pin
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_WAIT_US, 1},
    {CAMERA_TABLE_GPIO_ACT, 147}, // cam2_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg IMX132_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, 147}, // cam2_pwdn pin
    {CAMERA_TABLE_WAIT_US, 1},
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect IMX132_Detect[] =
{
    {0x02, 0x0000, 0xFFFF, 0x0132},
    {0, 0, 0, 0},
};


static NvU32 IMX132_EDPStates[] = { };

// IMX091 device description
static const char *IMX091_ClkName[] = {"default_mclk"};
static const char *IMX091_RegName[] = {"vdig", "vana", "vif", "af_vdd"};

static PCLHwReg IMX091_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 3},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_GPIO_ACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_US, 300},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg IMX091_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_GPIO_DEACT, 146}, // cam1_pwdn pin
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 3},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect IMX091_Detect[] =
{
    {0x02, 0x0000, 0xFFFF, 0x0091},
    {0, 0, 0, 0},
};

static NvU32 IMX091_EDPStates[] = { };

// AD5816 device description
static const char *AD5816_ClkName[] = {};
static const char *AD5816_RegName[] = {"vdd", "vdd_i2c"};

static PCLHwReg AD5816_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg AD5816_PwrOffSeq[] = {
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect AD5816_Detect[] =
{
    {0x02, 0x0000, 0xFF00, 0x2400},
    {0, 0, 0, 0},
};

static NvU32 AD5816_EDPStates[] = { };

// AD5823 device description
static const char *AD5823_ClkName[] = {};
static const char *AD5823_RegName[] = {"vdd", "vdd_i2c"};

static PCLHwReg AD5823_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_GPIO_ACT, 148}, // af_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg AD5823_PwrOffSeq[] = {
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_GPIO_DEACT, 148}, // af_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect AD5823_Detect[] =
{
    {0x02, 0x0006, 0xFFFF, 0x0010},
    {0, 0, 0, 0},
};

static NvU32 AD5823_EDPStates[] = { };

// DW9718 device description
static const char *DW9718_ClkName[] = {};
static const char *DW9718_RegName[] = {"vdd", "vdd_i2c"};

static PCLHwReg DW9718_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_GPIO_ACT, 148}, // af_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg DW9718_PwrOffSeq[] = {
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_GPIO_DEACT, 148}, // af_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect DW9718_Detect[] =
{
    {0x02, 0x0004, 0xFFFF, 0x0060},
    {0, 0, 0, 0},
};

static NvU32 DW9718_EDPStates[] = { };

// MAX77387 device description
static const char *MAX77387_ClkName[] = {};
static const char *MAX77387_RegName[] = {"vdd", "vin", "af_vdd"};

static PCLHwReg MAX77387_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg MAX77387_PwrOffSeq[] = {
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect MAX77387_Detect[] =
{
    {0x02, 0x0000, 0xFF00, 0x9100},
    {0, 0, 0, 0},
};

static NvU32 MAX77387_EDPStates[] = { };

#ifdef ENABLE_MORE_FLASH_DEVICES
// LM3565 device description
static const char *LM3565_ClkName[] = {};
static const char *LM3565_RegName[] = {"vi2c", "vin"};

static PCLHwReg LM3565_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_US, 1000},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg LM3565_PwrOffSeq[] = {
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect LM3565_Detect[] =
{
    {0x02, 0x0000, 0xFF00, 0x3400},
    {0, 0, 0, 0},
};

static NvU32 LM3565_EDPStates[] = { };

// AS3648 device description
static const char *AS3648_ClkName[] = {};
static const char *AS3648_RegName[] = {"vi2c", "vin"};

static PCLHwReg AS3648_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_US, 1000},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg AS3648_PwrOffSeq[] = {
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect AS3648_Detect[] =
{
    {0x01, 0x0000, 0xF0, 0xb0},
    {0, 0, 0, 0},
};

static NvU32 AS3648_EDPStates[] = { };
#endif

static DeviceProfile s_CameraDevices[] =
{
    CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(
        1, IMX132, SENSOR_BAYER_IMX132_GUID, 0x36, 3, 0x0132, "imx132"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
        2, IMX091, IMAGER_NVC_GUID, 0x10, 3, 0x0091, "imx091"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
        3, IMX135, SENSOR_BAYER_IMX135_GUID, 0x10, 3, 0x0135, "imx135"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
        4, OV5693, SENSOR_BAYER_OV5693_GUID, 0x10, 3, 0x5693, "ov5693"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
        5, AR0833, SENSOR_BAYER_AR0833_GUID, 0x36, 3, 0x4B03, "ar0833"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
        6, OV5640, SENSOR_YUV_OV5640_GUID, 0x3c, 3, 0x5640, "ov5640"),
    CAMERA_FOCUSER_I2C2_DETECT_DESC_DEFAULT(
        7, AD5816, FOCUSER_NVC_GUID, 0x0E, 3, 0x5816, "ad5816"),
    CAMERA_FOCUSER_I2C2_DETECT_DESC_DEFAULT(
        8, AD5823, FOCUSER_NVC_GUID, 0x0C, 3, 0x5823, "ad5823"),
    CAMERA_FOCUSER_I2C2_DETECT_DESC_DEFAULT(
        9, DW9718, FOCUSER_NVC_GUID, 0x0C, 3, 0x9718, "dw9718"),
    CAMERA_FLASH_I2C2_DETECT_DESC_DEFAULT(
        10, MAX77387, TORCH_NVC_GUID, 0x4A, 3, 0x77387, "max77387"),
#ifdef ENABLE_MORE_FLASH_DEVICES
    CAMERA_FLASH_I2C2_DETECT_DESC_DEFAULT(
        11, LM3565, TORCH_NVC_GUID, 0x30, 3, 0x3565, "lm3565"),
    CAMERA_FLASH_I2C2_DETECT_DESC_DEFAULT(
        12, AS3648, TORCH_NVC_GUID, 0x30, 3, 0x3648, "as3648"),
#endif
};
