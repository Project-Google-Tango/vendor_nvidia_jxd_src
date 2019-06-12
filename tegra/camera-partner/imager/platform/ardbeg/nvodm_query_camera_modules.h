/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
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

#define CAMERA_RST_GPIO     219  //PBB3 RESET FRONT CAMERA 
#define CAMERA1_PWDN_GPIO   221  //PBB5 REAR PWDN
#define CAMERA2_PWDN_GPIO   222  //PBB6 FRONT PWDN
#define CAMERA_AF_PWDN      223
#if 0
static const char *IMX135_ClkName[] = {"mclk"};
static const char *IMX135_RegName[] = {"vana", "vdig_lv", "vif"};
static PCLHwReg IMX135_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_MS, 5},
    {CAMERA_TABLE_GPIO_ACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_US, 300},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg IMX135_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
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
#endif
#if 0
// DW9718 device description
static const char *DW9718_ClkName[] = {};
static const char *DW9718_RegName[] = {"vdd_cam_1v1_cam", "vdd_lcd_1v2_s", "vdd_2v7_hv"};// {"vdd_cam_1v1_cam", "vdd_lcd_1v2_s", "imx135_reg2","vdd_2v7_hv"};

static PCLHwReg DW9718_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
//	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 3},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_GPIO_ACT, CAMERA1_PWDN_GPIO}, // af_pwdn pin
    {CAMERA_TABLE_GPIO_ACT, CAMERA_AF_PWDN}, // af_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg DW9718_PwrOffSeq[] = {
 //   {CAMERA_TABLE_PWR, 3},
	{CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
	{CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // af_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_AF_PWDN}, // af_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect DW9718_Detect[] =
{
    {0x02, 0x0004, 0xFFFF, 0x0060},
    {0, 0, 0, 0},
};

static NvU32 DW9718_EDPStates[] = { };
#endif
#if 0

// IMX179
static const char *IMX179_ClkName[] = {"mclk"};
static const char *IMX179_RegName[] = {"vana", "vdig", "vif", "imx135_reg1", "imx135_reg2"};
static PCLHwReg IMX179_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_ACT, CAMERA_AF_PWDN}, // cam1_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 3},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 4},
    {CAMERA_TABLE_WAIT_MS, 5},
    {CAMERA_TABLE_GPIO_ACT, CAMERA_RST_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_US, 300},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg IMX179_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_AF_PWDN}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, 4},
    {CAMERA_TABLE_PWR, 3},
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect IMX179_Detect[] =
{
    {0x02, 0x0002, 0xFFFF, 0x8179 },
    {0, 0, 0, 0},
};

static NvU32 IMX179_EDPStates[] = { };
#endif
#if 1
// OV5693 device description
static const char *OV5693_ClkName[] = {"mclk"};
static const char *OV5693_RegName[] = {"bus_v1v8","ov5693_1v2", "vdd_2v7_hv","ov5693_2v8"};
static PCLHwReg OV5693_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 3},
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_GPIO_ACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg OV5693_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, 3},
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
#endif
#if 0
// AR0261 device description
static const char *AR0261_ClkName[] = {"mclk2"};
static const char *AR0261_RegName[] = {"vana", "vdig", "vif"};

static PCLHwReg AR0261_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam rst pin
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_WAIT_MS, 40},
    {CAMERA_TABLE_GPIO_ACT, CAMERA_RST_GPIO}, // cam rst pin
    {CAMERA_TABLE_WAIT_MS, 20},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg AR0261_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam rst pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect AR0261_Detect[] =
{
    {0x02, 0x0003, 0xFFFF, 0x060A},
    {0, 0, 0, 0},
};

static NvU32 AR0261_EDPStates[] = { };

// MT9M114 device description
static const char *MT9M114_ClkName[] = {"mclk2"};
static const char *MT9M114_RegName[] = {"vana", "vif"};

static PCLHwReg MT9M114_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam rst pin
    {CAMERA_TABLE_GPIO_ACT, CAMERA2_PWDN_GPIO}, // cam2 pwdn pin
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_WAIT_MS, 10},
    {CAMERA_TABLE_GPIO_DEACT, CAMERA2_PWDN_GPIO}, // cam2 pwdn pin
    {CAMERA_TABLE_GPIO_ACT, CAMERA_RST_GPIO}, // cam rst pin
    {CAMERA_TABLE_WAIT_MS, 20},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg MT9M114_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_ACT, CAMERA2_PWDN_GPIO}, // cam2 pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam rst pin
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect MT9M114_Detect[] =
{
    {0x02, 0x0000, 0xFFFF, 0x2481},
    {0, 0, 0, 0},
};

static NvU32 MT9M114_EDPStates[] = { };

// OV7695 device description
static const char *OV7695_ClkName[] = {"mclk2"};
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
#endif

#if 0
// AD5816 device description
static const char *AD5816_ClkName[] = {"mclk"};
static const char *AD5816_RegName[] = {"vdd_lcd_1v2_s","imx135_reg2", "vdd_2v7_hv","vdd_1v2_cam"};

static PCLHwReg AD5816_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
	{CAMERA_TABLE_GPIO_DEACT, CAMERA_AF_PWDN}, // af_pwdn pin
    
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 3},
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_GPIO_ACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
	{CAMERA_TABLE_GPIO_ACT, CAMERA_AF_PWDN}, // af_pwdn pin
    
    {CAMERA_TABLE_WAIT_MS, 10},
    {CAMERA_TABLE_END, 0},

};

static PCLHwReg AD5816_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
	{CAMERA_TABLE_GPIO_DEACT, CAMERA_AF_PWDN}, // af_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, 3},
	{CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},

};

static const PCLDeviceDetect AD5816_Detect[] =
{
    {0x02, 0x0006, 0xFFFF, 0x0180},
    {0, 0, 0, 0},
};

static NvU32 AD5816_EDPStates[] = { };
#endif
#if 0

// AD5823 device description
static const char *AD5823_ClkName[] = {"mclk"};
static const char *AD5823_RegName[] = {"bus_v1v8","ov5693_1v2", "vdd_2v7_hv","ov5693_2v8"};

static PCLHwReg AD5823_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 3},
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_GPIO_ACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 10},
    {CAMERA_TABLE_END, 0},

};

static PCLHwReg AD5823_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA1_PWDN_GPIO}, // cam1_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, 3},
	{CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_END, 0},

};

static const PCLDeviceDetect AD5823_Detect[] =
{
    {0x02, 0x0006, 0xFFFF, 0x0010},
    {0, 0, 0, 0},
};

static NvU32 AD5823_EDPStates[] = { };




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

// MAX77387 device description
static const char *MAX77387_ClkName[] = {};
static const char *MAX77387_RegName[] = {"vdd", "vin"};

static PCLHwReg MAX77387_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg MAX77387_PwrOffSeq[] = {
    {CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_WAIT_US, 10},
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect MAX77387_Detect[] =
{
    {0x02, 0x0000, 0xFF00, 0x9100},
    {0, 0, 0, 0},
};

static NvU32 MAX77387_EDPStates[] = { };
#endif
#if 0
// OV5693.1 device description
static const char *OV5693_front_ClkName[] = {"mclk2"};
static const char *OV5693_front_RegName[] = {"bus_v1v8", "vana", "ov5693_2v8"};
//static const char *OV5693_front_RegName[] = {"bus_v1v8", "ov5693_1v2", "ov5693_2v8"};

static PCLHwReg OV5693_front_PwrOnSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 10000}, // enable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA2_PWDN_GPIO}, // cam2_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam_rst pin
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
	/*{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
	{CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},*/
    {CAMERA_TABLE_WAIT_MS, 1},
    {CAMERA_TABLE_GPIO_ACT, CAMERA2_PWDN_GPIO}, // cam2_pwdn pin
    {CAMERA_TABLE_GPIO_ACT, CAMERA_RST_GPIO}, // cam_rst pin
    {CAMERA_TABLE_WAIT_MS, 10},
    {CAMERA_TABLE_END, 0},
};

static PCLHwReg OV5693_front_PwrOffSeq[] = {
    {CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
    {CAMERA_TABLE_GPIO_DEACT, CAMERA2_PWDN_GPIO}, // cam2_pwdn pin
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam_rst pin
    {CAMERA_TABLE_WAIT_MS, 1},
	{CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},
    {CAMERA_TABLE_PWR, 0},
    /*{CAMERA_TABLE_PWR, 0},
    {CAMERA_TABLE_PWR, 2},
    {CAMERA_TABLE_PWR, 1},*/
    {CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect OV5693_front_Detect[] =
{
    {0x02, 0x300A, 0xFFFF, 0x5690},
    {0, 0, 0, 0},
};

static NvU32 OV5693_front_EDPStates[] = { };

#endif
#if 0
// SP2529 device description
static const char *SP2529_ClkName[] = {"mclk2"};
static const char *SP2529_RegName[] = {"avdd_af1_cam", "vdd_lcd_1v2_s","vdd_cam_1v1_cam"};
static PCLHwReg SP2529_PwrOnSeq[] = {
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 0},
    {CAMERA_TABLE_WAIT_MS, 10},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 1},//dovdd
    {CAMERA_TABLE_WAIT_MS, 10},
    {CAMERA_TABLE_PWR, CAMERA_TABLE_PWR_FLAG_ON | 2},//dvdd
    {CAMERA_TABLE_WAIT_MS, 200},
    {CAMERA_TABLE_INX_CLOCK, 24000}, // enable mclk
    {CAMERA_TABLE_WAIT_MS, 30},
    {CAMERA_TABLE_GPIO_DEACT, CAMERA2_PWDN_GPIO}, // cam2_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 50},
    {CAMERA_TABLE_GPIO_ACT, CAMERA2_PWDN_GPIO}, // cam2_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 50},
    {CAMERA_TABLE_GPIO_DEACT, CAMERA2_PWDN_GPIO}, // cam2_pwdn pin
    {CAMERA_TABLE_WAIT_MS, 200},
    {CAMERA_TABLE_GPIO_ACT, CAMERA_RST_GPIO}, // cam_rst pin
    {CAMERA_TABLE_WAIT_MS, 50},
    {CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam_rst pin
    {CAMERA_TABLE_WAIT_MS, 50},
	{CAMERA_TABLE_GPIO_ACT, CAMERA_RST_GPIO}, // cam_rst pin
    {CAMERA_TABLE_WAIT_MS, 50},
	{CAMERA_TABLE_END, 0},
};

static PCLHwReg SP2529_PwrOffSeq[] = {
	
	{CAMERA_TABLE_GPIO_ACT, CAMERA2_PWDN_GPIO}, // cam2_pwdn pin
	{CAMERA_TABLE_WAIT_MS, 5},
	{CAMERA_TABLE_GPIO_DEACT, CAMERA_RST_GPIO}, // cam_rst pin
	{CAMERA_TABLE_WAIT_MS, 5},
	{CAMERA_TABLE_INX_CLOCK, 0}, // disable mclk
	{CAMERA_TABLE_WAIT_MS, 5},
	{CAMERA_TABLE_PWR, 2},
	{CAMERA_TABLE_PWR, 1},
	{CAMERA_TABLE_PWR, 0},
	{CAMERA_TABLE_END, 0},
};

static const PCLDeviceDetect SP2529_Detect[] =
{
    {0x01, 0x02, 0xFF, 0x25},
    {0, 0, 0, 0},
};

static NvU32 SP2529_EDPStates[] = { };

#endif


static DeviceProfile s_CameraDevices[] =
{
    CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
        1, OV5693, SENSOR_BAYER_OV5693_GUID, 0x36, 3, 0x5693, "ov5693"), 
	
/*	CAMERA_FOCUSER_I2C2_DETECT_DESC_DEFAULT(
		2, AD5816, FOCUSER_NVC_GUID, 0xe, 3, 0x5816, "ad5816"),  

	CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
    	3, IMX135, SENSOR_BAYER_IMX135_GUID, 0x10, 3, 0x0135, "imx135"),

	CAMERA_FOCUSER_I2C2_DETECT_DESC_DEFAULT(
	 	4, DW9718, FOCUSER_NVC_GUID, 0x0C, 3, 0x9718, "dw9718"),*/

	//CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(
    //    1, OV5693_front, SENSOR_BAYER_OV5693_FRONT_GUID, 0x36, 3, 0x5693, "ov5693.1"),
	//CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(
    //    1, OV5693_front, SENSOR_BAYER_OV5693_FRONT_GUID, 0x36, 2, 0x5693, "ov5693.1"),
	//CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
     //   2, OV5693, SENSOR_BAYER_OV5693_GUID, 0x36, 3, 0x5693, "ov5693"), 	
	/*
    CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(
        3, AR0261, SENSOR_BAYER_AR0261_GUID, 0x36, 3, 0x0261, "ar0261"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(
        4, OV7695, SENSOR_YUV_OV7695_GUID, 0x21, 3, 0x7695, "ov7695"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(
        5, MT9M114, SENSOR_YUV_MT9M114_GUID, 0x48, 3, 0x1040, "mt9m114"),


    CAMERA_FLASH_I2C2_DETECT_DESC_DEFAULT(
        8, AS3648, TORCH_NVC_GUID, 0x30, 3, 0x3648, "as3648"),
    CAMERA_FLASH_I2C2_DETECT_DESC_DEFAULT(
        9, MAX77387, TORCH_NVC_GUID, 0x4A, 3, 0x77387, "max77387"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_REAR_DEFAULT_NOPINMUX(
        10, IMX179, SENSOR_BAYER_IMX179_GUID, 0x10, 3, 0x0179, "imx179"),
    CAMERA_SENSOR_I2C2_DETECT_DESC_FRONT_DEFAULT_NOPINMUX(
        11, OV5693_front, SENSOR_BAYER_OV5693_FRONT_GUID, 0x36, 2, 0x5693, "ov5693.1")
	CAMERA_I2C2_DETECT_DESC_DEFAULT(3, PCLDeviceType_Sensor, SP2529, SENSOR_YUV_SP2529_GUID, 
		1, 0x30, 1, 0xffff, 2, 0x25, "sp2529"),

*/


};
