/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef FASTBOOT_INCLUDED_H
#define FASTBOOT_INCLUDED_H

#include "nvcommon.h"
#include "nvrm_surface.h"
#include "nvaboot.h"
#include "nvodm_services.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_SERIALNO_LEN 32
#define FASTBOOT_ALIGN(X, A)     (((X) + ((A)-1)) & ~((A)-1))
#define BOOT_TAG_SIZE (2048/sizeof(NvU32))
#ifdef NV_EMBEDDED_BUILD
#define KERNEL_PARTITON "KERNEL_PRIMARY"
#define RECOVERY_KERNEL_PARTITION "KERNEL_RECOVERY"
#else
#define KERNEL_PARTITON "LNX"
#define RECOVERY_KERNEL_PARTITION "SOS"
#endif

#define BITMAP_PARTITON "BMP"
#define ANDROID_MAGIC "ANDROID!"
#define ANDROID_MAGIC_SIZE 8
#define ANDROID_BOOT_NAME_SIZE 16
#define ANDROID_BOOT_CMDLINE_SIZE 512
#define IGNORE_FASTBOOT_CMD "ignorefastboot"

typedef NvU32 (*GetMoreBitsCallback)(NvU8 *pBuffer, NvU32 MaxSize);

typedef struct FastbootWaveRec *FastbootWaveHandle;

typedef struct AndroidBootImgRec
{
    unsigned char magic[ANDROID_MAGIC_SIZE];
    unsigned kernel_size;
    unsigned kernel_addr;

    unsigned ramdisk_size;
    unsigned ramdisk_addr;

    unsigned second_size;
    unsigned second_addr;

    unsigned tags_addr;
    unsigned page_size;
    unsigned unused[2];

    unsigned char name[ANDROID_BOOT_NAME_SIZE];
    unsigned char cmdline[ANDROID_BOOT_CMDLINE_SIZE];

    unsigned id[8];
} AndroidBootImg;

/**
 * Defines Bitmap File Header
 *
 * Contains information about the Bitmap Image file.
 */

typedef struct BitmapFileHeaderRec
{
   NvU16 Magic;
   NvS32 FileSize;
   NvU16 Reserved1;
   NvU16 Reserved2;
   NvU32 StartOffset;
} BitmapFileHeader;

/**
 * Defines Bitmap Information Header
 *
 * Contains information about Bitmap Data in Bitmap Image file
 */

typedef struct BitmapInformationHeaderRec
{
   NvU32 HeaderSize;
   NvS32 Width;
   NvS32 Height;
   NvU16 Planes;
   NvU16 Depth;
   NvU32 CompressionType;
   NvS32 ImageSize;
   NvU32 HorizontalResolution;
   NvU32 VerticalResolution;
   NvU32 NumColors;
   NvU32 NumImpColors;
} BitmapInformationHeader;

typedef struct BitmapFileRec
{
    BitmapFileHeader bfh;
    BitmapInformationHeader bih;
    NvU8 BitmapData[1];
} BitmapFile;

typedef struct IconRec
{
    NvU16 width;
    NvU16 height;
    NvS32 stride;
    NvU8  bpp;
    NvU32 data[1];
} Icon;

typedef struct RadioMenuRec
{
    NvU32 PulseColor;
    NvU16 HorzSpacing;
    NvU16 VertSpacing;
    NvU16 RoundRectRadius;
    NvU8 NumOptions;
    NvU8 CurrOption;
    NvU8 PulseAnim;
    NvS8 PulseSpeed;
    Icon *Images[1];
} RadioMenu;

typedef struct TextMenuRec
{
    NvU8 NumOptions;
    NvU8 CurrOption;
    const char *Texts[1];
} TextMenu;

typedef enum
{
    FileSystem_Basic = 0,
    FileSystem_Yaffs2,
    FileSystem_Ext4
} FastbootFileSystem;

typedef enum
{
    FB_EnableBacklight = 0,
    FB_DisableBacklight,
    FB_TimeoutBacklight,
    FB_Default = 0xFFFFFFFF
} FBBacklightCtrl;

typedef struct PartitionNameMapRec
{
    const char        *LinuxPartName;
    const char        *NvPartName;
    FastbootFileSystem FileSystem;
} PartitionNameMap;

#define FRAMEBUFFER_DOUBLE 0x1
#define FRAMEBUFFER_32BIT  0x2
#define FRAMEBUFFER_CLEAR  0x4

NvBool FastbootInitializeDisplay(
    NvU32        Flags,
    NvRmSurface *pFramebuffer,
    NvRmSurface *pFrontbuffer,
    void       **pPixelData,
    NvBool       UseHdmi);

void FastbootDeinitializeDisplay(
    NvRmSurface *pFramebuffer,
    void       **pPixelData);

TextMenu *FastbootCreateTextMenu(
    NvU32 NumOptions,
    NvU32 InitOption,
    const char **pTexts
    );

void FastbootTextMenuSelect(
    TextMenu *pMenu,
    NvBool    Next);

void FastbootDrawTextMenu(
    TextMenu   *Menu,
    NvU32 X,
    NvU32 Y);

RadioMenu *FastbootCreateRadioMenu(
    NvU32 NumOptions,
    NvU32 InitOption,
    NvU32 PulseSpeed,
    NvU32 PulseColor,
    Icon **pIcons);

void FastbootRadioMenuSelect(
    RadioMenu *pMenu,
    NvBool    Next);

void FastbootDrawRadioMenu(
    NvRmSurface *pSurface,
    void        *pPixels,
    RadioMenu   *Menu);

void FramebufferUpdate( void );

void FastbootError(const char* fmt, ...);

void FastbootStatus(const char* fmt, ...);

NvBool FastbootAddAtag(
    NvU32 Atag,
    NvU32 DataSize,
    const void *Data);

NvError AddSerialBoardInfoTag(void);

NvError FastbootGetSerialNo(char *Str, NvU32 StrLength);

void GetOsAvailableMemSizeParams(
    NvAbootHandle hAboot,
    NvOdmOsOsInfo *OsInfo,
    NvAbootDramPart *DramPart,
    NvU32 *PrimaryMemSize,
    NvU32 *MemSizeBeyondPrimary);

NvError FastbootCommandLine(
    NvAbootHandle hAboot,
    const PartitionNameMap *PartitionList,
    NvU32 NumPartitions,
    NvU32 SecondaryBootDevice,
    const unsigned char *CommandLine,
    NvBool HasInitRamDisk,
    NvU32 NvDumperReserved,
    const unsigned char *ProductName,
    NvBool RecoveryKernel,
    NvBool BootToCharging);

void FastbootSetUsbMode(NvBool Is3p);

void FastbootCloseWave(FastbootWaveHandle pWave);

NvError FastbootPlayWave(
    GetMoreBitsCallback Callback,
    NvU32 SampleRateHz,
    NvBool Stereo,
    NvBool Signed,
    NvU32 BitsPerSample,
    FastbootWaveHandle *hWav);

void FastbootEnableBacklightTimer(FBBacklightCtrl ctrl, NvU32 time);

NvError FastbootBMPRead(
    NvAbootHandle hAboot,
    BitmapFile **bmf,
    const char *PartName);
NvError FastbootDrawBMP(
    BitmapFile *bmf,
    NvRmSurface *pSurface,
    void **pPixels);

extern NvU32 s_BootTags[BOOT_TAG_SIZE];

NvU32 CalcNvDumperReserved(NvAbootHandle hAboot);
NvBool LastRebootInfo(NvU32 NvDumperReserved);
void ClearRebootInfo(NvU32 NvDumperReserved);

#ifdef __cplusplus
}
#endif

#endif
