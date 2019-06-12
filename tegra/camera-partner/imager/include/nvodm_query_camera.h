/**
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef __PCL_QUERY_H__
#define __PCL_QUERY_H__

#include "nvodm_query_discovery.h"

/* This portion should be synchronized with kernel/include/camera.h */
#define PCL_MAX_STR_LEN (32)

#define CAMERA_INT_MASK                 0xf0000000
#define CAMERA_TABLE_WAIT_US            (CAMERA_INT_MASK | 1)
#define CAMERA_TABLE_WAIT_MS            (CAMERA_INT_MASK | 2)
#define CAMERA_TABLE_END                (CAMERA_INT_MASK | 9)
#define CAMERA_TABLE_PWR                (CAMERA_INT_MASK | 20)
#define CAMERA_TABLE_PINMUX             (CAMERA_INT_MASK | 25)
#define CAMERA_TABLE_INX_PINMUX         (CAMERA_INT_MASK | 26)
#define CAMERA_TABLE_GPIO_ACT           (CAMERA_INT_MASK | 30)
#define CAMERA_TABLE_GPIO_DEACT         (CAMERA_INT_MASK | 31)
#define CAMERA_TABLE_GPIO_INX_ACT       (CAMERA_INT_MASK | 32)
#define CAMERA_TABLE_GPIO_INX_DEACT     (CAMERA_INT_MASK | 33)
#define CAMERA_TABLE_REG_NEW_POWER      (CAMERA_INT_MASK | 40)
#define CAMERA_TABLE_INX_POWER          (CAMERA_INT_MASK | 41)
#define CAMERA_TABLE_INX_CLOCK          (CAMERA_INT_MASK | 50)
#define CAMERA_TABLE_INX_CGATE          (CAMERA_INT_MASK | 51)
#define CAMERA_TABLE_EDP_STATE          (CAMERA_INT_MASK | 60)

#define CAMERA_TABLE_PWR_FLAG_MASK      0xf0000000
#define CAMERA_TABLE_PWR_FLAG_ON        0x80000000
#define CAMERA_TABLE_PINMUX_FLAG_MASK   0xf0000000
#define CAMERA_TABLE_PINMUX_FLAG_ON     0x80000000
#define CAMERA_TABLE_CLOCK_VALUE_BITS   24
#define CAMERA_TABLE_CLOCK_VALUE_MASK   \
                        ((u32)(-1) >> (32 - CAMERA_TABLE_CLOCK_VALUE_BITS))
#define CAMERA_TABLE_CLOCK_INDEX_BITS   (32 - CAMERA_TABLE_CLOCK_VALUE_BITS)
#define CAMERA_TABLE_CLOCK_INDEX_MASK   \
                        ((u32)(-1) << (32 - CAMERA_TABLE_CLOCK_INDEX_BITS))

#define EDP_MIN_PRIO                    19
#define EDP_MAX_PRIO                    0

#define CAMERA_MAX_EDP_ENTRIES          16
#define CAMERA_MAX_NAME_LENGTH          32
#define CAMDEV_INVALID                  0xffffffff


/*
 * Enum defining device bus type, i2c/spi etc.
 */
typedef enum PCLDeviceBusTypes{
    CAMERA_DEVICE_TYPE_I2C,
    CAMERA_DEVICE_TYPE_MAX_NUM,
} PCLDevBus;

/**
 * Struct container for a register, or memory location
 */
typedef struct PCLHwRegRec
{
    NvU32 addr;
    NvU32 val;
} PCLHwReg;

/* End of portion synchronized with kernel/include/camera.h */

/**
 * A generic Calibration structure definition
 */
typedef struct PCLCalibrationRec
{
    NvU64 size;
    void *pData;
} PCLCalibration;

/**
 * enum to assist in classifying device type instance
 */
typedef enum PCLDeviceType{
    PCLDeviceType_Info = 0, // Use is TBD
    PCLDeviceType_Empty,    // Added to potentially simplify camera module definition, if practical is TBD
    PCLDeviceType_Sensor,
    PCLDeviceType_Focuser,
    PCLDeviceType_Flash,
    PCLDeviceType_ROM,
    PCLDeviceType_FORCE32 = 0x7FFFFFFF
} PCLDeviceType;

/**
 * Enum defining how memory (register) values should be interpreted and handled
 */
typedef enum PCLRegFlow{
    PCLRegFlow_DEFAULT = 0, // I2C in most cases right now
    PCLRegFlow_I2C,
    PCLRegFlow_GPIO,
    PCLRegFlow_SPI,
    PCLRegFlow_SLEEP,
    PCLRegFlow_CSI3,    // TBD
    PCLRegFlow_BEGIN_VENDOR_EXTENSIONS = 0x10000000,
    PCLRegFlow_FORCE32 = 0x7FFFFFFF,
} PCLRegFlow;

typedef struct PCLDeviceInfoRec {
    NvU32 BusType;
    NvU8 BusNum;
    NvU8 Slave;
} PCLDeviceInfo;

/**
 * Struct to define a list of registers grouped together sequentially
 */
typedef struct PCLHwRegList
{
    NvU16 regdata_length;
    NvU8 regaddr_bits;
    NvU8 regval_bits;
    PCLHwReg *regdata_p;
} PCLHwRegList;

/**
 * Struct to define a submodule hardware device instance
 */
typedef struct PCLHwBlobRec
{
    NvU32 blob_version;
    PCLDeviceType dev_type;         // TODO: consider even a string for dev_type for leveraging future
    char dev_name[PCL_MAX_STR_LEN]; // identify the sub-module device
    char sub_name[PCL_MAX_STR_LEN]; // to help distinguish platform/device variations (ie. hw revs, ROM location, etc)
    NvU32 dev_addr;                 // ie. i2c addr; help administrate and validate blob
    PCLHwRegList regList;
} PCLHwBlob;

#define PCL_MAX_SIZE_FUSEID (16)

/**
 * A standardized container for all FuseIDs.
 */
typedef struct PCLFuseIDRec
{
    NvU8 FuseID[PCL_MAX_SIZE_FUSEID];
    NvU8 numBytes;
} PCLFuseID;

#define PCL_EDP_TABLE_END ((NvU32)-1)

typedef struct PCLEdpConfigRec
{
    NvU32 *States;
    NvU32 Num;
    NvU32 E0Index;
    int Priority;
} PCLEdpConfig;

typedef struct PCLDeviceDetectRec
{
    NvU32 BytesRead;
    NvU32 Offset;
    NvU32 Mask;
    NvU32 TrueValue;
} PCLDeviceDetect;

/**
 * PCLDriverData is for handling and delivering information from PCLController framework
 * PCLDrivers fill in the values according to their PCL<type>Object in their PCLDriver_<type>.h file
 * This structure is independent of any PCLDriver context, and PCLDrivers should still keep their
 * own private context in the struct PCLDriver
 */
typedef struct PCLDriverDataRec
{
    /**
     * ObjectList is defined by PCL<type>Object in their PCLDriver_<type>.h file
     * Currently, defined as follows:
     *  Sensor  : List of varying sensor mode definitions
     *  Focuser : Unutilized; using numObjects of 1
     *  Flash   : List of LEDs available and their properties
     */
    void *ObjectList;
    NvU32 numObjects;
} PCLDriverData;

typedef struct PCLDeviceSpecRec {
    NvU64 GUID;
    NvU32 Present;
    NvU32 Position;
    /// The list of all I/Os and buses connecting this peripheral to the AP.
    PCLDeviceInfo DevInfo;
    NvU32 RegLength;
    NvU32 PinMuxGrp;
    NvU32 GpioUsed;
    NvU32 RegulatorNum;
    const char **RegNames;
    NvU32 PowerOnSequenceNum;
    PCLHwReg *PowerOnSequence;
    NvU32 PowerOffSequenceNum;
    PCLHwReg *PowerOffSequence;
    NvU32 ClockNum;
    const char **ClockNames;
    PCLEdpConfig Edp;
    const char *AlterName;
    const PCLDeviceDetect *Detect;
    NvU32 DevId;
} PCLDeviceSpec;

/**
 * DeviceProfile is for identifying static device/driver information.
 * Defines the full bus connectivity for camera devices connected to the
 * application processor.
 */
typedef struct DeviceProfileRec
{
    /// The chip name of the device defined in kernel.
    char *ChipName;
    /// individual index ID within the device table.
    /// so devices within the table can connect to each other.
    /// CANNOT be ZERO!
    NvU32 Index;
    /// Device type: camera sensor/focuser/flash/eeprom, etc
    PCLDeviceType Type;
    /// Device specific data of resources used
    PCLDeviceSpec Device;
    /// Connectivity layout.
    NvU32 *Connects;
    // PCLHwBlob identifies how software recognizes the hardware instance (device name, address, etc)
    PCLHwBlob devBlob;
    // isOlNvOdmDriver is a flag for the system to recognize (inside PCL framework and camera/core)
    NvBool isOldNvOdmDriver;
    // FuseID is the unique identifier (a serial ID)
    PCLFuseID FuseID;
    // Data contains specific driver-type capabilities information
    PCLDriverData Data;
    // private data carried
    void *pData;
    // size of data
    NvU32 DSize;
} DeviceProfile;

/**
 * A Camera Module container.
 */
typedef struct PCLModuleDeviceRec {
    NvU32 Module_ID;
    // Non allocated members/devices should be NULL
    DeviceProfile Sensor;
    DeviceProfile Focuser;
    DeviceProfile Flash;
    DeviceProfile Rom;
    PCLCalibration CalibrationData;
} PCLModuleDevice;

const void *NvOdmCameraQuery(NvU32 *NumModules);

#endif /* __PCL_QUERY_H__ */
