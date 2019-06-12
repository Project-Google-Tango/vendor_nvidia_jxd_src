/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include <string.h>
#include "i2c-util.h"
#include "board.h"

static NvError NvOsPrivExtractSkuInfo(NvU32 *info, NvU32 field)
{
    char    buf[30] = { 0 };
    FILE*   skuinfo;
    char    *p;

    skuinfo = fopen("/proc/skuinfo","r");
    if(!skuinfo)
    {
        fprintf(stderr, "%s() Warning: Could not retrieve sku info.\n", __func__);
        return NvError_NotSupported;
    }

    fread(buf, 1, sizeof(buf), skuinfo);
    fclose(skuinfo);

    p = strtok(buf, "-");
    while( field-- && p)
    {
        p = strtok(NULL, "-");
    }

    if (!p || 0 == (*info = strtoul(p, NULL, 10)))
    {
        fprintf(stderr, "%s() Warning: Could not retrieve sku info.\n", __func__);
        return NvSuccess;
    }

    return NvSuccess;
}

static int IsE1888(int fd)
{
    NvError err;

    err = i2c_write_subaddr(fd, CPLD_I2C_ADDR, CPLD_WRITE_ENABLE_OFFSET, CPLD_WRITE_ENABLE_VALUE);
    // if able to program CPLD that means base board is E1888
    if (err == 0)
    {
        return 1;
    }
    return 0;
}

static int IsE1860(int fd)
{
    NvError err;
    unsigned char val[NVPN_LEN];
    int i;

    for(i=0; i<NVPN_LEN; i++) {
        err = i2c_read_subaddr(fd, EEPROM_ADDRESS , i, &val[i]);
        if(err)
            return 0;
    }
    return 1;
}

static int IsE1155(int fd)
{
    NvError err;
    err = i2c_write_subaddr(fd, CPLD_I2C_ADDR, CPLD_WRITE_ENABLE_OFFSET, CPLD_WRITE_ENABLE_VALUE);
    // if able to program CPLD that means base board is E1888
    if (err == 0)
    {
        return 1;
    }
    return 0;
}

static int ConfigureE1888CPLD(int fd, int num)
{
    NvError err = NvError_BadParameter;
    int i2c_value;
    switch (num) {
        case 0:
            i2c_value = CPLD_ENABLE_DAP1; break;
        case 1:
            i2c_value = CPLD_ENABLE_DAP2; break;
        case 2:
            i2c_value = CPLD_ENABLE_DAP3; break;
        default:
            printf("Not supported \n");
            return err;
    }
    err = i2c_write_subaddr(fd, CPLD_I2C_ADDR , CPLD_WRITE_OFFSET , i2c_value);
    if (err)
    {
        fprintf(stderr, "%s() Error: I2C write on E1888-CPLD failed.\n", __func__);
        return err;
    }
    return 0;
}

static NvError ConfigureE1155ExtDAP(int fd, int num)
{
    int i2c_value;
    NvError err = NvError_BadParameter;

    switch (num) {
        case 0:
            i2c_value = pcf8574_enable_DAP1; break;
        case 1:
            i2c_value = pcf8574_enable_DAP2; break;
        case 2:
            i2c_value = pcf8574_enable_DAP3; break;
        default:
            printf("Not supported \n");
            return err;
    }
    err = i2c_write_subaddr(fd, PCF8574_I2C_ADDR , 0 , i2c_value);
    if (err)
    {
        fprintf(stderr, "%s() Error: I2C write on E1155-PCF8574 failed.\n", __func__);
        return err;
    }
    return 0;
}

static NvError ConfigureE1860(int fd, int num)
{
    return NvSuccess;
}


NvError ConfigureBaseBoard(int fd, CodecMode mode, unsigned int *sku_id)
{
    int dap_port = 0;

    *sku_id = 0;

    NvOsPrivExtractSkuInfo(sku_id, 2);

    if(mode ==  CodecMode_I2s1 || mode ==  CodecMode_Tdm1)
         dap_port = 0;

    if(mode ==  CodecMode_I2s2 || mode ==  CodecMode_Tdm2)
    {
         dap_port = 1;
         if (*sku_id == 2 || *sku_id == 8 || *sku_id == 9 )
            dap_port = 2;
    }

    if(IsE1860(fd)) {
        printf("Board E1860 detected \n");
        return ConfigureE1860(fd, dap_port);
    }
    if(IsE1888(fd)) {
        printf("Board E1888 detected \n");
        return ConfigureE1888CPLD(fd, dap_port);
    }
    if(IsE1155(fd)) {
        printf("Board E1155 detected \n");
        return ConfigureE1155ExtDAP(fd, dap_port);
    }
    printf("Unknown BaseBoard, Audio Codecs not configured \n");
    return NvError_BadParameter;

}


