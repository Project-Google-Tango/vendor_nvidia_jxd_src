/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVNCT_INCLUDED_H
#define NVNCT_INCLUDED_H

#define NCT_MAGIC_ID       "nVCt" /* 0x7443566E */
#define NCT_MAGIC_ID_LEN   4
#define NCT_PART_NAME      "NCT"

#define NCT_VENDOR_ID      0x955  /* FIXME: TEST */
#define NCT_PRODUCT_ID     0x8000 /* FIXME: TEST */

#define NCT_FORMAT_VERSION    0x00010000 /* 0xABCDabcd (ABCD.abcd) */

#define NCT_ENTRY_OFFSET       0x4000
#define MAX_NCT_ENTRY          512
#define MAX_NCT_DATA_SIZE      1024
#define NCT_ENTRY_SIZE         1040
#define NCT_ENTRY_DATA_OFFSET  12

#define TEGRA_EMC_MAX_FREQS    20

#define CW2015_FG_BATTERY_PROFILE_SIZE    64

typedef enum
{
    NCT_TAG_1B_SINGLE  = 0x10,
    NCT_TAG_2B_SINGLE  = 0x20,
    NCT_TAG_4B_SINGLE  = 0x40,
    NCT_TAG_STR_SINGLE = 0x80,
    NCT_TAG_1B_ARRAY   = 0x1A,
    NCT_TAG_2B_ARRAY   = 0x2A,
    NCT_TAG_4B_ARRAY   = 0x4A,
    NCT_TAG_STR_ARRAY  = 0x8A
} nct_tag_type;

typedef enum
{
    NCT_ID_START = 0,
    NCT_ID_SERIAL_NUMBER = NCT_ID_START, /* ID:1, Tag: 0x80 */
    NCT_ID_WIFI_MAC_ADDR,                /* ID:2, Tag: 0x1A */
    NCT_ID_BT_ADDR,                      /* ID:3, Tag: 0x1A */
    NCT_ID_CM_ID,                        /* ID:4, Tag: 0x20 */
    NCT_ID_LBH_ID,                       /* ID:5, Tag: 0x20 */
    NCT_ID_FACTORY_MODE,                 /* ID:6, Tag: 0x40 */
    NCT_ID_RAMDUMP,                      /* ID:7, Tag: 0x40 */
    NCT_ID_TEST,                         /* ID:8, Tag: 0x1A */
    NCT_ID_BOARD_INFO,                   /* ID:9, Tag: 0x2A */
    NCT_ID_GPS_ID,
    NCT_ID_LCD_ID,
    NCT_ID_ACCELEROMETER_ID,
    NCT_ID_COMPASS_ID,
    NCT_ID_GYROSCOPE_ID,
    NCT_ID_LIGHT_ID,
    NCT_ID_CHARGER_ID,
    NCT_ID_TOUCH_ID,
    NCT_ID_FUELGAUGE_ID,
    NCT_ID_MEMTABLE,
    NCT_ID_MEMTABLE_END = NCT_ID_MEMTABLE + TEGRA_EMC_MAX_FREQS - 1,
    NCT_ID_BATTERY_MODEL_DATA,
    NCT_ID_DEBUG_CONSOLE_PORT_ID,
    NCT_ID_BATTERY_MAKE,
    NCT_ID_BATTERY_COUNT,
    NCT_ID_DDR_TYPE,
    NCT_ID_END = NCT_ID_DDR_TYPE,
    NCT_ID_DISABLED = 0xEEEE,
    NCT_ID_MAX = 0xFFFF
} nct_id_type;

typedef struct
{
    char sn[30];
} nct_serial_number_type;

#define NVIDIA_OUI    0x00044B

typedef struct
{
    unsigned char addr[6];
} nct_wifi_mac_addr_type;

typedef struct
{
    unsigned char addr[6];
} nct_bt_addr_type;

typedef struct
{
    unsigned short id;
} nct_cm_id_type;

typedef struct
{
    unsigned short id;
} nct_lbh_id_type;

typedef struct
{
    unsigned int flag;
} nct_factory_mode_type;

typedef struct
{
    unsigned int flag;
} nct_ramdump_type;

typedef struct
{
    unsigned int proc_board_id;
    unsigned int proc_sku;
    unsigned int proc_fab;
    unsigned int pmu_board_id;
    unsigned int pmu_sku;
    unsigned int pmu_fab;
    unsigned int display_board_id;
    unsigned int display_sku;
    unsigned int display_fab;
} nct_board_info_type;

typedef struct
{
    signed short mrr5;
    signed short mrr6;
    signed short mrr7;
    signed short mrr8;
    signed short index;
} nct_ddr_type_data;

typedef struct
{
    nct_ddr_type_data ddr_type_data[2];
} nct_ddr_type;
typedef union
{
    unsigned char            u8[MAX_NCT_DATA_SIZE];
    unsigned short           u16[MAX_NCT_DATA_SIZE/2];
    unsigned int             u32[MAX_NCT_DATA_SIZE/4];
} nct_tegra_emc_table_type;

typedef struct
{
    unsigned int            rcomp;
    unsigned int            soccheck_A;
    unsigned int            soccheck_B;
    unsigned int            bits;
    unsigned int            alert_threshold;
    unsigned int            one_percent_alerts;
    unsigned int            alert_on_reset;
    unsigned int            rcomp_seg;
    unsigned int            hibernate;
    unsigned int            vreset;
    unsigned int            valert;
    unsigned int            ocvtest;
    unsigned int            custom_data[16];
} max17048_battery_model_data;

typedef struct
{
    unsigned char            u8[CW2015_FG_BATTERY_PROFILE_SIZE];
} cw2015_battery_profile_data;

typedef union
{
    max17048_battery_model_data max17048_mdata;
    cw2015_battery_profile_data cw2015_pdata;
} nct_battery_model_data;

typedef struct
{
    unsigned int port_id;
} nct_debug_port_id_type;

typedef struct
{
    unsigned char make[20];
} nct_battery_make_data;

typedef struct
{
    unsigned int count;
} nct_battery_count_data;

#if NVOS_IS_WINDOWS
#pragma pack(push, 1)
typedef union
{
    nct_serial_number_type   serial_number;
    nct_wifi_mac_addr_type   wifi_mac_addr;
    nct_bt_addr_type         bt_addr;
    nct_cm_id_type           cm_id;
    nct_lbh_id_type          lbh_id;
    nct_factory_mode_type    factory_mode;
    nct_ramdump_type         ramdump;
    nct_board_info_type      board_info;
    nct_lbh_id_type          gps_id;
    nct_lbh_id_type          lcd_id;
    nct_lbh_id_type          accelerometer_id;
    nct_lbh_id_type          compass_id;
    nct_lbh_id_type          gyroscope_id;
    nct_lbh_id_type          light_id;
    nct_lbh_id_type          charger_id;
    nct_lbh_id_type          touch_id;
    nct_lbh_id_type          fuelgauge_id;
    nct_battery_model_data   battery_mdata;
    nct_debug_port_id_type   debug_port;
    nct_battery_make_data    battery_make;
    nct_battery_count_data   battery_count;
    nct_ddr_type   ddr_type;
    nct_tegra_emc_table_type tegra_emc_table;
    unsigned char            u8[MAX_NCT_DATA_SIZE];
    unsigned short           u16[MAX_NCT_DATA_SIZE/2];
    unsigned int             u32[MAX_NCT_DATA_SIZE/4];
} nct_item_type;
#pragma pack(pop)
#else
typedef union
{
    nct_serial_number_type   serial_number;
    nct_wifi_mac_addr_type   wifi_mac_addr;
    nct_bt_addr_type         bt_addr;
    nct_cm_id_type           cm_id;
    nct_lbh_id_type          lbh_id;
    nct_factory_mode_type    factory_mode;
    nct_ramdump_type         ramdump;
    nct_board_info_type      board_info;
    nct_lbh_id_type          gps_id;
    nct_lbh_id_type          lcd_id;
    nct_lbh_id_type          accelerometer_id;
    nct_lbh_id_type          compass_id;
    nct_lbh_id_type          gyroscope_id;
    nct_lbh_id_type          light_id;
    nct_lbh_id_type          charger_id;
    nct_lbh_id_type          touch_id;
    nct_lbh_id_type          fuelgauge_id;
    nct_battery_model_data   battery_mdata;
    nct_debug_port_id_type   debug_port;
    nct_battery_make_data    battery_make;
    nct_battery_count_data   battery_count;
    nct_ddr_type   ddr_type;
    nct_tegra_emc_table_type tegra_emc_table;
    unsigned char            u8data;
    unsigned short           u16data;
    unsigned int             u32data;
    unsigned char            u8[MAX_NCT_DATA_SIZE];
    unsigned short           u16[MAX_NCT_DATA_SIZE/2];
    unsigned int             u32[MAX_NCT_DATA_SIZE/4];
}__attribute__ ((packed))nct_item_type;
#endif

typedef struct
{
    unsigned int index;
    unsigned int reserved[2];
    nct_item_type data;
    unsigned int checksum;
} nct_entry_type;

typedef struct
{
    unsigned int magicId;
    unsigned int vendorId;
    unsigned int productId;
    unsigned int version;
    unsigned int revision;
} nct_part_head_type;

typedef struct NctCustInfoRec
{
    nct_board_info_type NCTBoardInfo;
    nct_battery_make_data BatteryMake;
    nct_battery_count_data BatteryCount;
    nct_ddr_type DDRType;
}NctCustInfo;

#endif
