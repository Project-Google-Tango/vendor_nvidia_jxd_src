/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_VERSIONINFO_H
#define INCLUDED_VERSIONINFO_H

typedef struct ChangeDescriptionRec
{
    NvU8 Major;
    NvU8 Minor;
    NvU16 Branch;
    NvU8 Desc[256];
}ChangeDescription;

ChangeDescription ChangeDesc[] =
{
    {00, 00, 0000, "Change details not available"},
    {01, 00, 0000, "Change details not available"},
    {02, 00, 0000, "Change details not available"},
    {03, 00, 0000, "Change details not available"},
    {04, 00, 0000, "New version scheme for main"},
    {04, 01, 0000, "NvSecuretool: Cleanup and pkc/sbk mode usage changes: 209862"},
    {04, 02, 0000, "max size correction for partition with 0x808 attribute: 213539"},
    {04, 03, 0000, "oem field addition in blob: 1047405"},
    {04, 04, 0000, "nv3pserver: Read from usb and write to storage device "
                   "parallely while downloading a partition: 161111"},
    {04, 05, 0000, "Support for kernel boot without any emmc access: 216043"},
    {04, 06, 0000, "enable nvflash visual studio build: 213536"},
    {04, 07, 0000, "Removal of --setblhash & enhancement in updatebct: 918781"},
    {04,  8, 0000, "nvflash: Handle padding of read-only bootloader while "
                   "downloading: 161516"},
    {04,  8, 0000, "nvflash: Do not write bct to a file if .cfg is used while "
                   "flashing: 161520"},
    {04,  8, 0000, "nvflash:nv3pserver: Increase USB transfer size from 64KB "
                   "to 1MB for single transaction of read and download "
                   "partition: 216394"},
    {04,  8, 0000, "nvflash: UberSku support for T148: 229254"},
    {04,  9, 0000, "nvflash: UberSku support for T124: 269099"},
    {04,  9, 0000, "nvflash: Sku selection based on SPEEDO/IDDQ value read "
                   "from board: 268520"},
    {04, 10, 0000, "nvflash: common cfg selection based on board sku "
                   "and chip sku: 276278"},
    {04, 11, 0000, "nvflash: added support for dtb parsing: 227788"},
    {04, 12, 0000, "migration nvflash, nv3pserver and nvbdktest from rel-tegratab/rel-17"
                   "to support for TN8/TN7C manufacturing tool"},
    {04, 13, 0000, "Force download fuse bypass information: 351390"}
};

#endif //INCLUDED_VERSIONINFO_H

