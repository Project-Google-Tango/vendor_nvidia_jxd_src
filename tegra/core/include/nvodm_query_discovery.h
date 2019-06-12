/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         ODM Peripheral Discovery API</b>
 *
 * @b Description: Defines a query interface for enumerating peripherals
 *                 and the board-specific topology that may be called during
 *                 boot and at run-time.
 */

#ifndef INCLUDED_NVODM_QUERY_DISCOVERY_H
#define INCLUDED_NVODM_QUERY_DISCOVERY_H

/**
 * @defgroup nvodm_discovery ODM Peripheral Discovery Interface
 *
 * This is the ODM query interface allowing ODMs to specify the connectivity
 * of peripherals on their boards to I/Os on an NVIDIA<sup>&reg;</sup>
 * Tegra<sup>&reg;</sup> Application Processor without modifying the driver
 * libraries.
 *
 * All peripherals connected to the application processor are referenced by
 * their Global Unique Identifier code, or GUID. The GUID is an ODM-defined
 * 64-bit value (8-character code) that uniquely identifies each peripheral,
 * i.e., each peripheral will have exactly one GUID, and each GUID will refer
 * to exactly one peripheral.
 *
 * The implementation of this API is similar to a simple database: tables are
 * provided by the ODMs that, for every peripheral, define the peripheral's
 * GUID, and specify the bus, or set of buses, to which the peripheral is
 * connected. As an example, an audio codec peripheral with GUID 0 could be
 * connected to the application processor by DAP instance 0, I2C instance 1
 * (address 0x80), and voltage rail 3. The functions provided by this API
 * provide facilities enabling the boot process and driver libraries to
 * enumerate the set of peripherals attached to a specific bus, the set of
 * peripherals with specific functionality (such as display outputs), and the
 * set of buses connected to a specific peripheral.
 * @ingroup nvodm_query
 * @{
 */

#include "nvodm_modules.h"
#include "nvodm_services.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * The NV_ODM_GUID macro is used to convert ASCII 8-character codes into the
 * 64-bit globally-unique identifiers that identify each peripheral.
 *
 * @note GUIDs beginning with the characters <b>nv</b>, <b>Nv</b>, <b>nV</b>,
 * and <b>NV</b> are reserved for use by NVIDIA; all other values may be
 * assigned by the ODM as desired.
 *
 * To access asynchronous Dual-Ported RAM, set:
 * @code
 * NV_ODM_GUID('d','p','r','a','m','d','e','v')
 * @endcode
 */
#define NV_ODM_GUID(a,b,c,d,e,f,g,h) \
    ((NvU64) ((((a)&0xffULL)<<56ULL) | (((b)&0xffULL)<<48ULL) | \
              (((c)&0xffULL)<<40ULL) | (((d)&0xffULL)<<32ULL) | \
              (((e)&0xffULL)<<24ULL) | (((f)&0xffULL)<<16ULL) | \
              (((g)&0xffULL)<< 8ULL) | (((h)&0xffULL))))

/**
 * Reserved GUIDs.
 */
#define NV_VDD_RTC_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','R','T','C'))
#define NV_VDD_CORE_ODM_ID      (NV_ODM_GUID('N','V','D','D','C','O','R','E'))
#define NV_VDD_CPU_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','C','P','U'))

#define NV_VDD_PLLA_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','A'))
#define NV_VDD_PLLM_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','M'))
#define NV_VDD_PLLP_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','P'))
#define NV_VDD_PLLC_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','C'))
#define NV_VDD_PLLD_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','D'))
#define NV_VDD_PLLE_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','E'))
#define NV_VDD_PLLU_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','U'))
#define NV_VDD_PLLU1_ODM_ID     (NV_ODM_GUID('N','V','D','P','L','L','U','1'))
#define NV_VDD_PLLHDMI_ODM_ID   (NV_ODM_GUID('N','V','D','P','L','L','H','D'))
#define NV_VDD_OSC_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','O','S','C'))
#define NV_VDD_PLLS_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','S'))
#define NV_VDD_PLLX_ODM_ID      (NV_ODM_GUID('N','V','D','D','P','L','L','X'))
#define NV_VDD_PLL_USB_ODM_ID   (NV_ODM_GUID('N','V','D','P','L','L','U','S'))
#define NV_VDD_PLL_PEX_ODM_ID   (NV_ODM_GUID('N','V','D','P','L','L','P','X'))

#define NV_VDD_SYS_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','S','Y','S'))
#define NV_VDD_USB_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','U','S','B'))
#define NV_VPP_FUSE_ODM_ID      (NV_ODM_GUID('N','V','P','P','F','U','S','E'))
#define NV_VDD_HDMI_ODM_ID      (NV_ODM_GUID('N','V','D','D','H','D','M','I'))
#define NV_VDD_MIPI_ODM_ID      (NV_ODM_GUID('N','V','D','D','M','I','P','I'))
#define NV_VDD_LCD_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','L','C','D'))
#define NV_VDD_AUD_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','A','U','D'))
#define NV_VDD_DDR_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','D','D','R'))
#define NV_VDD_DDR_RX_ODM_ID    (NV_ODM_GUID('N','V','D','D','D','R','R','X'))
#define NV_VDD_NAND_ODM_ID      (NV_ODM_GUID('N','V','D','D','N','A','N','D'))
#define NV_VDD_UART_ODM_ID      (NV_ODM_GUID('N','V','D','D','U','A','R','T'))
#define NV_VDD_SDIO_ODM_ID      (NV_ODM_GUID('N','V','D','D','S','D','I','O'))
#define NV_VDD_VDAC_ODM_ID      (NV_ODM_GUID('N','V','D','D','V','D','A','C'))
#define NV_VDD_VI_ODM_ID        (NV_ODM_GUID('N','V','D','D','_','_','V','I'))
#define NV_VDD_BB_ODM_ID        (NV_ODM_GUID('N','V','D','D','_','_','B','B'))
#define NV_VDD_VBUS_ODM_ID      (NV_ODM_GUID('N','V','D','D','V','B','U','S'))
#define NV_VDD_USB2_VBUS_ODM_ID (NV_ODM_GUID('N','V','D','V','B','U','S','2'))
#define NV_VDD_USB3_VBUS_ODM_ID (NV_ODM_GUID('N','V','D','V','B','U','S','3'))

#define NV_VDD_HSIC_ODM_ID      (NV_ODM_GUID('N','V','D','D','H','S','I','C'))
#define NV_VDD_USB_IC_ODM_ID    (NV_ODM_GUID('N','V','D','D','U','S','B','I'))
#define NV_VDD_PEX_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','P','E','X'))
#define NV_VDD_PEX_CLK_ODM_ID   (NV_ODM_GUID('N','V','D','D','P','E','X','C'))
#define NV_VDD_SoC_ODM_ID       (NV_ODM_GUID('N','V','D','D','_','S','O','C'))
#define NV_VDD_5V0_SYS_ODM_ID       (NV_ODM_GUID('N','V','D','D','5','V','0','S'))
#define NV_VDD_2V8_DISPLAY_ODM_ID (NV_ODM_GUID('N','V','D','S','I','2','V','8'))
#define NV_VDD_SATA_ODM_ID      (NV_ODM_GUID('N','V','D','D','S','A','T','A'))

#define NV_PMU_TRANSPORT_ODM_ID (NV_ODM_GUID('N','V','P','M','U','T','R','N'))


#define NV_VDD_HDMI_INT_ID      (NV_ODM_GUID('N','V','D','D','H','P','D','0'))

// NvOdmImager Reserved GUIDs
#define NV_IMAGER_NULLYUV_ID    (NV_ODM_GUID('N','V','I','M','G','N','Y','V'))
#define NV_IMAGER_NULLBAYER_ID  (NV_ODM_GUID('N','V','I','M','G','N','B','R'))
#define NV_IMAGER_HOST_ID       (NV_ODM_GUID('N','V','I','M','G','H','S','T'))
#define NV_IMAGER_NULLCSIA_ID   (NV_ODM_GUID('N','V','I','M','G','N','C','A'))
#define NV_IMAGER_NULLCSIB_ID   (NV_ODM_GUID('N','V','I','M','G','N','C','B'))
#define NV_IMAGER_TPGABAYER_ID  (NV_ODM_GUID('N','V','I','M','G','T','A','B'))
#define NV_IMAGER_TPGARGB_ID    (NV_ODM_GUID('N','V','I','M','G','T','A','R'))
#define NV_IMAGER_TPGBBAYER_ID  (NV_ODM_GUID('N','V','I','M','G','T','B','B'))
#define NV_IMAGER_TPGBRGB_ID    (NV_ODM_GUID('N','V','I','M','G','T','B','R'))

#define NV_HDMI_PARAM_ODM_ID    (NV_ODM_GUID('N','V','H','D','M','P','A','M'))

// Touch Feature
#define STYLUS_TO_FINGER_DELAY                (1 << 1)
#define STYLUS_ERASER_CURSOR                  (1 << 2)
#define STYLUS_INSERTION_REMOVAL_DETECTION    (1 << 3)
#define STYLUS_REMOVAL_WAKEUP                 (1 << 4)
#define DIRECTSTYLUS_TOP_LEVEL_MENU           (1 << 5)
#define STYLUS_REMOVAL_EVENT                  (1 << 6)
#define STYLUS_LASSO_BUTTON                   (1 << 7)
#define FINGER_LOCK_BUTTON                    (1 << 8)
#define STYLUS_PRESSURE_SMOOTHING             (1 << 9)
#define STYLUS_WHITELIST                      (1 << 10)
#define STYLUS_SINGLE_INPUT                   (1 << 11)
#define STYLUS_BLOCKED_ON_EDGE_STARTED        (1 << 12)
#define STYLUS_HANDEDNESS_OPTION              (1 << 13)
#define TOUCH_TOOLTYPE_TRACKING_LOCKING       (1 << 14)
#define DIRECTSTYLUS_HELP                     (1 << 15)

#define ENABLE_NONE                     0
#define ENABLE_STYLUS                   (STYLUS_TO_FINGER_DELAY \
                                        | STYLUS_ERASER_CURSOR \
                                        | STYLUS_INSERTION_REMOVAL_DETECTION \
                                        | DIRECTSTYLUS_TOP_LEVEL_MENU \
                                        | FINGER_LOCK_BUTTON)
#define ENABLE_ERASER                   (STYLUS_ERASER_CURSOR \
                                        | STYLUS_SINGLE_INPUT \
                                        | STYLUS_BLOCKED_ON_EDGE_STARTED \
                                        | TOUCH_TOOLTYPE_TRACKING_LOCKING)
#define ENABLE_STYLUS_REMOVAL_WAKEUP    (STYLUS_REMOVAL_WAKEUP)
#define ENABLE_DIRECTSTYLUS_LAUNCHER    (STYLUS_REMOVAL_EVENT \
                                        | DIRECTSTYLUS_HELP)
#define ENABLE_LASSO                    (STYLUS_LASSO_BUTTON)
#define ENABLE_PRESSURE_SMOOTHING       (STYLUS_PRESSURE_SMOOTHING)
#define ENABLE_STYLUS_WHITELIST         (STYLUS_WHITELIST)
#define ENABLE_SINGLE_TOUCH_TRACK       (STYLUS_SINGLE_INPUT \
                                        | TOUCH_TOOLTYPE_TRACKING_LOCKING)
#define ENABLE_HANDEDNESS_OPTION        (STYLUS_BLOCKED_ON_EDGE_STARTED \
                                        | STYLUS_HANDEDNESS_OPTION)

/**
 * Some of the NVIDIA driver libraries enumerate peripherals based on the
 * logical functionality that the peripheral performs, rather than by the
 * bus that connects it. An example of this is the camera driver, which
 * enumerates all peripherals supporting camera functionality on an ODM's
 * system (depending on the target market, 0, 1, or multiple cameras may
 * be present). To support this abstract functionality-based query, each
 * peripheral is assigned a class based on the user-level functionality it
 * provides.
 */
typedef enum
{
    /// Specifies a display output peripheral, such as an LCD or attached TV.
    NvOdmPeripheralClass_Display = 1,

    /// Specifies a camera (imager) input peripheral.
    NvOdmPeripheralClass_Imager,

    /// Specifies a mass-storage device, such as a NAND flash controller.
    NvOdmPeripheralClass_Storage,

    /// Specifies a human-computer input, such as a keypad or touch panel.
    NvOdmPeripheralClass_HCI,

    /// Specifies a peripheral that does not fall into the other classes.
    NvOdmPeripheralClass_Other,

    NvOdmPeripheralClass_Num,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmPeripheralClass_Force32 = 0x7fffffffUL
} NvOdmPeripheralClass;

/**
 * Defines the unique address on a bus where a peripheral is connected.
 */
typedef struct
{
    /// Specifies the type of bus or I/O (I2C, DAP, GPIO) for this connection.
    NvOdmIoModule Interface;

    /**
     * Some buses or I/Os on an application processor have multiple instances,
     * such as the 3 SPI controllers on AP15, or multiple GPIO ports. This
     * value specifies to which instance of a multi-instance bus is the
     * peripheral connected. The instance value should be the same value
     * passed to the instance parameter in the ODM service APIs (e.g.,
     * NvOdmI2cOpen()).
     *
     * @note For GPIOs, this value refers to the GPIO port.
     */
    NvU32 Instance;

    /**
     * Some buses, such as I2C and SPI, support multiple slave devices on
     * a single bus, through the use of peripheral addresses and/or chip
     * select signals. This value specifies the appropriate address or chip
     * select required to communicate with the peripheral.
     *
     * @note This value differs depending on usage:
     *       - For GPIOs, this value specifies the GPIO pin.
     *       - For VDDs, this value is ODM-defined PMU rail ID.
     *       - For board designs where the LCD bus is connected to a main
     *       display and sub-display (e.g., a clam-shell cellular phone),
     *       address 0 should refer to the main panel, and 1 to the sub-panel.
     */
    NvU32 Address;

    /**
     * Specifies purpose of address definition as an adaptation local ID.
     * ODM adaptations can use this member to parse connectivity tables
     * from ODM query, by enumerating all addresses they expect under their
     * GUID, and populating this member with an enum value. Use is optional.
     */
    NvU32 Purpose;

} NvOdmIoAddress;

/**
 * Defines the full bus connectivity for peripherals connected to the
 * application processor.
 */
typedef struct
{
    /// The ODM-defined 64-bit GUID that identifies this peripheral.
    NvU64 Guid;

    /**
     * The list of all I/Os and buses connecting this peripheral to the AP.
     * @see NvOdmIoAddress
     */
    const NvOdmIoAddress *AddressList;

    /// The number of entries in the \a addressList array.
    NvU32 NumAddress;

    /// The functionality class of this peripheral.
    NvOdmPeripheralClass Class;
} NvOdmPeripheralConnectivity;


/// Defines different criteria for searching through the peripheral database.
typedef enum
{
    /// Searches for peripherals that are members of the specified class.
    NvOdmPeripheralSearch_PeripheralClass,

    /// Searches for peripherals connected to the specified I/O module.
    NvOdmPeripheralSearch_IoModule,

    /**
     * Searches for peripherals connected to the specified bus instance.
     *
     * @note This value will be compared against all entries in \a addressList.
     */
    NvOdmPeripheralSearch_Instance,

    /**
     * Searches for peripherals matching the specified address or chip select.
     *
     * @note This value will be compared against all entries in \a addressList.
     */
    NvOdmPeripheralSearch_Address,

    NvOdmPeripheralSearch_Num,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvOdmPeripheralSearch_Force32 = 0x7fffffffUL
} NvOdmPeripheralSearch;

/**
 * Defines the data structure that describes each sub-assembly of the
 * development platform.  This data is read from EEPROMs on each of the sub-
 * assemblies, which is saved with the following format:
 * <pre>
 *      E-xxxx-yyyy-0zz XN
 * </pre>
 *
 * Where:
 *
 * - xxxx is the board ID number 0-9999 (2 byte number)
 * - yyyy is the SKU number 0-9999 (2 byte number)
 * - zz is the FAB number 0-99 (1 byte number)
 * - X is the major revision, (1 byte ASCII character), e.g., 'A', 'B', etc.
 * - N is the minor revision, (1 byte number, 0 - 9)
 */
typedef struct
{
    /// Specifies the board number.
    NvU16 BoardID;

    /// Specifies the SKU number.
    NvU16 SKU;

    /// Specifies the FAB number.
    NvU8 Fab;

    /// Specifies the part revision.
    NvU8 Revision;

    /// Specifies the minor revision level.
    NvU8 MinorRevision;
} NvOdmBoardInfo;

/**
 * Defines the data structure that describes each sub-assembly of the
 * development platform. This data is read from EEPROMs on each of the sub-
 * assemblies, which is saved with the following format.
 */
typedef struct
{
    /// Holds the version number.
    NvU16 Version;

    /// Holds the size of 2 byte value of \a BoardId data that follows.
    NvU16 Size;

    /// Holds the board number.
    NvU16 BoardID;

    /// Holds the SKU number.
    NvU16 SKU;

    /// Holds the FAB number.
    NvU8 Fab;

    /// Holds the part revision.
    NvU8 Revision;

    /// Holds the minor revision level.
    NvU8 MinorRevision;

    /// Holds the memory type.
    NvU8 MemType;

    /// Holds the power configuration values.
    NvU8 PowerConfig;

    /// Holds the SPL reworks, mech. changes.
    NvU8 MiscConfig;

    /// Holds the modem bands value.
    NvU8 ModemBands;

    /// Holds the touch screen configs.
    NvU8 TouchScreenConfigs;

    /// Holds the display configs.
    NvU8 DisplayConfigs;

}NvOdmBoardInfoRev1;

/**
 * Searches through the database of connected peripherals for peripherals
 * matching the specified search criteria.
 *
 * Search criteria are supplied by paired attribute-value lists; if multiple
 * criteria are specified, the returned peripherals represent the intersection
 * (boolean AND) of the supplied search criteria.
 *
 * This function is expected to be called twice with the same search criteria.
 * In the first call, \a guidList should be NULL.  The number of peripherals
 * matching the search criteria will be returned.
 *
 * In the second call, \a guidList should point to an array of NvU64s; the GUIDs
 * of up to \a numGuids peripherals matching the search critiera will be written
 * to \a guidList. The number of stored GUIDs will be returned.
 *
 * @param searchAttrs The array of search attributes (::NvOdmPeripheralSearch).
 * @param searchVals The array of values for each search attribute.
 * @param numCriteria The number of entries in the attribute-value lists.
 * @param guidList The array of output GUIDs. If NULL, no GUIDs are returned.
 * @param numGuids The number of entries in \a guidList.
 * @return The number of GUIDs written to \a guidList, or the total number of
 *          peripherals matching the search criteria if \a guidList is NULL.
 */
NvU32
NvOdmPeripheralEnumerate(
    const NvOdmPeripheralSearch *searchAttrs,
    const NvU32 *searchVals,
    NvU32 numCriteria,
    NvU64 *guidList,
    NvU32 numGuids);

/**
 * Searches through the database of connected peripherals for the peripheral
 * matching the specified GUID and returns that peripheral's connectivity
 * structure.
 *
 * @note If the ODM system supports hot-pluggable peripherals (e.g., an
 * external TV-out display), the connectivity structure should only be returned
 * when the peripheral is useable by the NVIDIA driver libraries. If hot-plug
 * detection is not supported, the peripheral's connectivity structure may
 * always be returned.
 *
 * @see NvOdmPeripheralConnectivity
 *
 * @param searchGuid The GUID value of the queried peripheral.
 *
 * @return A pointer to the peripheral's bus connectivity structure if a
 * peripheral matching \a searchGuid is found, or NULL if no peripheral is found.
 */
const NvOdmPeripheralConnectivity *
NvOdmPeripheralGetGuid(NvU64 searchGuid);

/**
 * Gets the ::NvOdmBoardInfo data structure values for the given board ID.
 *
 * @param BoardId Identifies the board for which the BoardInfo data is to be retrieved.
 *  @note The \a BoardId is in Binary Encoded Decimal (BCD) format. For example,
 *  E920 is identified by a \a BoardId of 0x0920 and E9820 would be 0x9820.
 * @param pBoardInfo A pointer to the location where the requested \a BoardInfo data
 *  shall be saved.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPeripheralGetBoardInfo(
    NvU16 BoardId,
    NvOdmBoardInfo* pBoardInfo);
/**
 * Defines the board module.
 */
typedef enum
{
    /// Gets the details of version in ::NvOdmBoardInfo.
    NvOdmUpdatedBoardModuleInfoType_Version = 0,
    /// Gets the details of size in ::NvOdmBoardInfo.
    NvOdmUpdatedBoardModuleInfoType_Size,
    /// Gets the details of MemType in ::NvOdmPmuBoardInfo.
    NvOdmUpdatedBoardModuleInfoType_MemType,
    /// Gets the details of PowerConfig in ::NvOdmDisplayBoardInfo.
    NvOdmUpdatedBoardModuleInfoType_PowerConfig,
    /// Getss the details of Misc Config in ::NvOdmDisplayBoardInfo.
    NvOdmUpdatedBoardModuleInfoType_MiscConfig,
    /// Gets the details of Modem Bands in ::NvOdmDisplayBoardInfo.
    NvOdmUpdatedBoardModuleInfoType_ModemBands,
    /// Gets the details of touch screen configs in ::NvOdmDisplayBoardInfo.
    NvOdmUpdatedBoardModuleInfoType_TouchScreenConfigs,
    /// Gets the details of display configs  in ::NvOdmDisplayBoardInfo.
    NvOdmUpdatedBoardModuleInfoType_DisplayConfigs,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvOdmUpdatedBoardModuleInfoType_Force32 =  0x7fffffffUL
} NvOdmUpdatedBoardModuleInfoType;

/**
 * Defines the board module type in the functional level.
 */
typedef enum
{
    NvOdmBoardModuleType_None = 0,
    /// Holds the details of processor board in ::NvOdmBoardInfo.
    NvOdmBoardModuleType_ProcessorBoard,
    /// Holds the details of the PMU board in ::NvOdmPmuBoardInfo.
    NvOdmBoardModuleType_PmuBoard,
    /// Holds the details of display board in ::NvOdmDisplayBoardInfo.
    NvOdmBoardModuleType_DisplayBoard,
    /// Holds the details of power supply in ::NvOdmPowerSupplyInfo.
    NvOdmBoardModuleType_PowerSupply,
    /// Holds the details of Audio codec in ::NvOdmAudioCodecBoardInfo.
    NvOdmBoardModuleType_AudioCodec,
    /// Holds the details of cpu current in ::NvOdmMaxCpuCurrentInfo.
    NvOdmBoardModuleType_CpuCurrent,
    /// Holds the details of camera board in ::NvOdmCameraBoardInfo.
    NvOdmBoardModuleType_CameraBoard,
    /// Holds the details of display panel.
    NvOdmBoardModuleType_DisplayPanel,
    /// Holds the details of touch
    NvOdmBoardModuleType_Touch,
} NvOdmBoardModuleType;

/**
 * Holds PMU board information.
 */
typedef struct NvOdmPmuBoardInfo_Rec {
    /// Holds the core electrical design point voltage.
    NvU32 core_edp_mv;

    /// Holds the core electrical design point current.
    NvU32 core_edp_ma;

    /// Holds a flag specifying whether to pass the PMU board info to kernel.
    NvBool isPassBoardInfoToKernel;

    /// Holds the PMU board info details to pass to kernel.
    NvOdmBoardInfo BoardInfo;
} NvOdmPmuBoardInfo;

/**
 * Defines different types of displays.
 */
typedef enum
{
    NvOdmBoardDisplayType_LVDS = 0,
    NvOdmBoardDisplayType_DSI,
    NvOdmBoardDisplayType_HDMI,
    NvOdmBoardDisplayType_EDP
} NvOdmBoardDisplayType;

/**
 * Defines different vendors of displays.
 */
typedef enum
{
    NvOdmBoardDisplayPanelId_Default = 0,
    NvOdmBoardDisplayPanelId_Sharp,
    NvOdmBoardDisplayPanelId_LG,
} NvOdmBoardDisplayPanelId;

/**
 * Defines different vendors of Touch.
 */
typedef enum
{
    NvOdmBoardTouchVendorId_Raydium = 0,
    NvOdmBoardTouchVendorId_Synaptic,
    NvOdmBoardTouchVendorId_Maxim,
    NvOdmBoardTouchVendorId_None,
} NvOdmBoardTouchVendorId;

/**
 * Defines different touch panels.
 */
typedef enum
{
    NvOdmBoardTouchPanelId_Reserved = 0,
    NvOdmBoardTouchPanelId_Wintek,
    NvOdmBoardTouchPanelId_TPK,
    NvOdmBoardTouchPanelId_TouchTurns,
    NvOdmBoardTouchPanelId_Thor_Wintek,
    NvOdmBoardTouchPanelId_Loki_Wintek_5_66_Unlamin,
    NvOdmBoardTouchPanelId_TN7,
    NvOdmBoardTouchPanelId_TN8,
} NvOdmBoardTouchPanelId;

/**
 * Defines different touch features.
 */
typedef enum
{
    NvOdmTouchEnableNone = ENABLE_NONE,
    NvOdmTouchEnableStylus = ENABLE_STYLUS,
    NvOdmTouchEnableEraser = ENABLE_ERASER,
    NvOdmTouchEnableStylusRemovalWakeup = ENABLE_STYLUS_REMOVAL_WAKEUP,
    NvOdmTouchEnableDirectStylusLauncher = ENABLE_DIRECTSTYLUS_LAUNCHER,
    NvOdmTouchEnableLasso = ENABLE_LASSO,
    NvOdmTouchEnablePressureSmoothing = ENABLE_PRESSURE_SMOOTHING,
    NvOdmTouchEnableStylusWhitelist = ENABLE_STYLUS_WHITELIST,
    NvOdmTouchEnableSingleTouchTrack = ENABLE_SINGLE_TOUCH_TRACK,
    NvOdmTouchEnableHandednessOption = ENABLE_HANDEDNESS_OPTION,
} NvOdmTouchFeature;

/**
 * Holds display board information.
 */
typedef struct NvOdmDisplayBoardInfo_Rec {
    /// Holds the panel type to pass to kernel
    NvOdmBoardDisplayType DisplayType;

    /// Holds a flag specifying whether to pass the Display board info to kernel.
    NvBool IsPassBoardInfoToKernel;

    /// Holds the Display board info details to pass to kernel.
    NvOdmBoardInfo BoardInfo;
} NvOdmDisplayBoardInfo;

/**
 * Holds display board information.
 */
typedef struct NvOdmDisplayPanelInfo_Rec {
    NvOdmBoardDisplayPanelId DisplayPanelId;
}NvOdmDisplayPanelInfo;

/**
 * Holds Touch information.
 */
typedef struct NvOdmTouchInfo_Rec {
    NvOdmBoardTouchVendorId TouchVendorId;
    NvOdmBoardTouchPanelId TouchPanelId;
    NvOdmTouchFeature TouchFeature;
} NvOdmTouchInfo;

/**
 * Defines different types of power supplies.
 */
typedef enum
{
    NvOdmBoardPowerSupply_Adapter = 0,
    NvOdmBoardPowerSupply_Battery,
} NvOdmBoardPowerSupply;

/**
 * Holds power supply information.
 */
typedef struct NvOdmPowerSupplyInfo_Rec {
    NvOdmBoardPowerSupply SupplyType;
} NvOdmPowerSupplyInfo;

/**
 * Holds audio codec board information.
 */
typedef struct NvOdmAudioCodecBoardInfo_Rec {
    /// Holds the name of the audio codec
    char AudioCodecName[30];

} NvOdmAudioCodecBoardInfo;

/**
 * Holds maximum cpu current supported by board power logics.
 */
typedef struct NvOdmMaxCpuCurrentInfo_Rec {
    /// Holds the maximum cpu current in mA.
    int MaxCpuCurrentmA;
} NvOdmMaxCpuCurrentInfo;

/**
 * Holds camera board information.
 */
typedef struct NvOdmCameraBoardInfo_Rec {
    /// Holds a flag specifying whether to pass the camera board info to the kernel.
    NvBool IsPassBoardInfoToKernel;
    /// Holds the camera board info details to pass to the kernel.
    NvOdmBoardInfo BoardInfo;
} NvOdmCameraBoardInfo;


/**
 * Gets the board info in the requested data structure.
 *
 * @param mType The module type, whether it is processor board,
 *              PMU board, or display board, etc.
 * @param BoardModuleData A pointer to the data structure
 *              where the information will get stored.
 * @param DataLen Length of the data in bytes. This is size of the structure
 *              passed as \a BoardModuleData.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmQueryGetBoardModuleInfo(
    NvOdmBoardModuleType mType,
    void *BoardModuleData,
    int DataLen);

/**
 * Gets the board info in the requested data structure.
 *
 * @param mType The module type, whether it is processor board,
 *              PMU board, or display board, etc.
 * @param BoardModuleData A pointer to the data structure
 *              where the information will get stored.
 * @param datatype Enum whose value is requested.
 */
NvError
NvOdmQueryGetUpatedBoardModuleInfo(
    NvOdmBoardModuleType mType,
    NvU16 *BoardModuleData,
    NvOdmUpdatedBoardModuleInfoType datatype);
#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_QUERY_DISCOVERY_H
