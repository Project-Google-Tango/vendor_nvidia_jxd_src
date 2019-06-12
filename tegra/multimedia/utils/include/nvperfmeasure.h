/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

 /**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Video Perf Measurement API </b>
 *
 * @b Description:
 *           Defines the API for Perf Measurement using EMC_STAT Register
 */

#ifndef INCLUDED_NVPERFMEASURE_H
#define INCLUDED_NVPERFMEASURE_H

#ifdef __cplusplus
extern "C" {
#endif

/* This API ONLY works from a root shell or an app that runs as root
 * TODO : Fix API to run for for non-root apps
 */

#ifndef NV_IS_AVP
#define NV_IS_AVP 0
#endif

#ifndef RUNNING_IN_SIMULATION
#define RUNNING_IN_SIMULATION 0
#endif
#include "nvperfmeasure_inc.h"

#if RUNNING_IN_SIMULATION
NvError NvGetHwUnitPLLMValue(
     NvPerfMeasureInfo *pPerfMeasureInfo,
     NvS32 *HwUnitClockSource);

NvError NvGetHwUnitPLLMValue(
     NvPerfMeasureInfo *pPerfMeasureInfo,NvS32 *HwUnitClockSource)
{
    switch (pPerfMeasureInfo->HwUnitClockResetRegOffset)
    {
     case CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_IN_0:/*SPDIF_IN*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_IN_0_SPDIF_IN_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SPI2_0:/*SPI2*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SPI2_0_SPI2_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SPI3_0:/*SPI3*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SPI3_0_SPI3_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2C1_0:/*I2C1*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_I2C1_0_I2C1_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2C5_0:/*I2C5*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_I2C5_0_I2C5_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SPI1_0:/*SPI1*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SPI1_0_SPI1_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_DISP1_0:/*DISP1*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_DISP1_0_DISP1_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_DISP2_0:/*DISP2*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_DISP2_0_DISP2_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_ISP_0:/*ISP*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_ISP_0_ISP_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_VI_0:/*VI*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_VI_0_VI_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC1_0:/*SDMMC1*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC1_0_SDMMC1_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC2_0:/*SDMMC2*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC2_0_SDMMC2_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC4_0:/*SDMMC4*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC4_0_SDMMC4_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_VFIR_0:/*VFIR*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_VFIR_0_VFIR_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_HSI_0:/*HSI*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_HSI_0_HSI_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_UARTA_0:/*UARTA*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_UARTA_0_UARTA_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_UARTB_0:/*UARTB*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_UARTB_0_UARTB_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_HOST1X_0:/*HOST1X*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_HOST1X_0_HOST1X_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_HDMI_0:/*HDMI*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_HDMI_0_HDMI_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2C2_0:/*I2C2*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_I2C2_0_I2C2_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0:/*EMC*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_SRC_PLLM_UD;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_UARTC_0:/*UARTC*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_UARTC_0_UARTC_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR_0:/*VI_SENSOR*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR_0_VI_SENSOR_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_SPI4_0:/*SPI4*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SPI4_0_SPI4_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_I2C3_0:/*I2C3*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_I2C3_0_I2C3_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC3_0:/*SDMMC3*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC3_0_SDMMC3_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_UARTD_0:/*UARTD*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_UARTD_0_UARTD_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_VDE_0:/*VDE*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_VDE_0_VDE_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_OWR_0:/*OWR*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_OWR_0_OWR_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_NOR_0:/*NOR*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_NOR_0_NOR_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_CSITE_0:/*CSITE*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_CSITE_0_CSITE_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_MSENC_0:/*MSENC*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_MSENC_0_MSENC_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_TSEC_0:/*TSEC*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_TSEC_0_TSEC_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_LA_0:/*LA*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_LA_0_LA_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_MSELECT_0:/*MSELECT*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_MSELECT_0_MSELECT_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_I2C4_0:/*I2C4*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_I2C4_0_I2C4_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_SPI5_0:/*SPI5*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SPI5_0_SPI5_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_SPI6_0:/*SPI6*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SPI6_0_SPI6_CLK_SRC_PLLM_OUT0;
          break;
     case  CLK_RST_CONTROLLER_CLK_SOURCE_HDA2CODEC_2X_0:/*HDA2CODEC_2X*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_HDA2CODEC_2X_0_HDA2CODEC_2X_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SOR0_0:/*SOR*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SOR0_0_SOR0_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SATA_OOB_0:/*SATA_OOB*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SATA_OOB_0_SATA_OOB_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SATA_0:/*SATA*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SATA_0_SATA_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_HDA_0:/*SATA*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_HDA_0_HDA_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SE_0:/*SE*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SE_0_SE_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_DVFS_REF_0:/*DVFS_REF*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_DVFS_REF_0_DVFS_REF_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_DVFS_SOC_0:/*DVFS_SOC*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_DVFS_SOC_0_DVFS_SOC_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_TRACECLKIN_0:/*TRACECLKIN*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_TRACECLKIN_0_TRACECLKIN_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_EMC_LATENCY_0:/*EMC_LATENCY*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_EMC_LATENCY_0_EMC_LATENCY_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_SOC_THERM_0:/*SOC_THERM*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_SOC_THERM_0_SOC_THERM_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR2_0:/*VI_SENSOR2*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR2_0_VI_SENSOR2_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2C6_0:/*I2C6*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_I2C6_0_I2C6_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL_0:/*EMC_DLL*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL_0_EMC_DLL_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_VIC_0:/*VIC*/
          *HwUnitClockSource = (NvS32) CLK_RST_CONTROLLER_CLK_SOURCE_VIC_0_VIC_CLK_SRC_PLLM_OUT0;
          break;
     case CLK_RST_CONTROLLER_CLK_SOURCE_PWM_0:/*PWM*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2S1_0:/*I2S1*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2S2_0:/*I2S2*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_OUT_0:/*SPDIF_OUT*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2S0_0:/*I2S0*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_DTV_0:/*DTV*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_TSENSOR_0:/*TSENSOR*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2S3_0:/*I2S3*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2S4_0:/*I2S4*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_AUDIO_0:/*AUDIO*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_DAM0_0:/*DAM0*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_DAM1_0:/*DAM1*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_DAM2_0:/*DAM2*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_ACTMON_0:/*ACTMON*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_EXTPERIPH1_0:/*EXTPERIPH1*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_EXTPERIPH2_0:/*EXTPERIPH2*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_EXTPERIPH3_0:/*EXTPERIPH3*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_I2C_SLOW_0:/*I2C_SLOW*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_SYS_0:/*SYS*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_HOST_0:/*XUSB_CORE_HOST*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FALCON_0:/*XUSB_FALCON*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FS_0:/*XUSB_FS*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_DEV_0:/*XUSB_CORE_DEV*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_SS_0:/*XUSB_SS*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_CILAB_0:/*CILAB*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_CILCD_0:/*CILCD*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_CILE_0:/*CILE*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_DSIA_LP_0:/*DSIA_LP*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_DSIB_LP_0:/*DSIB_LP*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_ENTROPY_0:/*ENTROPY*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_ADX0_0:/*ADX0*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_AMX0_0:/*AMX0*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_HDMI_AUDIO_0:/*HDMI_AUDIO*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_CLK72MHZ_0:/*CLK72MHZ*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_ADX1_0:/*ADX1*/
     case CLK_RST_CONTROLLER_CLK_SOURCE_AMX1_0:/*AMX1*/
          *HwUnitClockSource = (NvS32) INVALID_PLLM;
          break;
     }

     return NvSuccess;
}
#endif

NvError NvPerfMeasureInit(
#if RUNNING_IN_SIMULATION
     NvU32 HwUnitClockResetRegOffset,
     NvU32 HwUnitClockDivisor,
#endif
     NvPerfMeasureInfo *pPerfMeasureInfo)
{
#if RUNNING_IN_SIMULATION
    NvU32 RegValue = 0;
    NvU32 EMCPLLSource = 0;
    NvS32 HwUnitClockSource = 0;
    NvU32 ClockDivisor = 0;
#endif
#if !NV_IS_AVP
    NvU32 EMCRegbanksize;
    NvU32 CARRegbanksize;
#endif

    if (pPerfMeasureInfo == NULL)
    {
        return NvError_InsufficientMemory;
    }

    /*EMC Register needs to mapped in order to access it from CPU */
    pPerfMeasureInfo->EMC_base = 0;
    pPerfMeasureInfo->CLK_RST_base = 0;
#if !NV_IS_AVP
    pPerfMeasureInfo->EMCRegBaseAdd = 0;
    pPerfMeasureInfo->CARRegBaseAdd = 0;
    NvRmOpen(&(pPerfMeasureInfo->hRmDevice), 0);
    pPerfMeasureInfo->ModuleId = NVRM_MODULE_ID(NvRmPrivModuleID_ExternalMemoryController, 0);
#endif

#if !NV_IS_AVP
    if (pPerfMeasureInfo->EMCRegBaseAdd == 0)
    {
        NvRmModuleGetBaseAddress(pPerfMeasureInfo->hRmDevice, pPerfMeasureInfo->ModuleId, &(pPerfMeasureInfo->EMCRegBaseAdd), &EMCRegbanksize);
        NvRmPhysicalMemMap(pPerfMeasureInfo->EMCRegBaseAdd, EMCRegbanksize, NVOS_MEM_READ_WRITE,
                    NvOsMemAttribute_Uncached, (void **)&(pPerfMeasureInfo->EMC_base));
    }
#else
#if (__TEGRA_ARCH == 30)
    pPerfMeasureInfo->EMC_base = EMC_BASE;
#else
    pPerfMeasureInfo->EMC_base = T11X_EMC_BASE;
#endif
#endif
    if (pPerfMeasureInfo->EMC_base == 0)
    {
        return NvError_InsufficientMemory;
    }

    /*Map the HW Unit's Clock and Reset Register */
#if !NV_IS_AVP
    pPerfMeasureInfo->ModuleId = NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0);
    if (pPerfMeasureInfo->CARRegBaseAdd == 0)
    {
        NvRmModuleGetBaseAddress(pPerfMeasureInfo->hRmDevice, pPerfMeasureInfo->ModuleId, &(pPerfMeasureInfo->CARRegBaseAdd), &CARRegbanksize);
        NvRmPhysicalMemMap(pPerfMeasureInfo->CARRegBaseAdd, CARRegbanksize, NVOS_MEM_READ_WRITE,
                    NvOsMemAttribute_Uncached, (void **)&(pPerfMeasureInfo->CLK_RST_base));
    }
#else
    pPerfMeasureInfo->CLK_RST_base = CLK_RST_BASE;
#endif
    if (pPerfMeasureInfo->CLK_RST_base == 0)
    {
        return NvError_InsufficientMemory;
    }

    /*Set the EMC PLL Source to PLLM_UD*/
#if RUNNING_IN_SIMULATION
    EMCPLLSource = (CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_SRC_PLLM_UD) << (CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_SRC_SHIFT) ;
    CLK_RST_REGW(pPerfMeasureInfo->CLK_RST_base,CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0,EMCPLLSource);

    /*Set the HW Unit's PLL Source and Clock Divisor */
    pPerfMeasureInfo->HwUnitClockDivisor = HwUnitClockDivisor;
    pPerfMeasureInfo->HwUnitClockResetRegOffset = HwUnitClockResetRegOffset;

    HwUnitClockDivisor = (HwUnitClockDivisor - 1);
    HwUnitClockDivisor = (HwUnitClockDivisor) * 2;
    ClockDivisor = (NvU32) (HwUnitClockDivisor);

    NvGetHwUnitPLLMValue(pPerfMeasureInfo,&HwUnitClockSource);

    if(HwUnitClockSource == INVALID_PLLM)
    {
       /*For Hw Unit's which don't support PLLM set only Clock divisor*/
       RegValue = CLK_RST_REGR(pPerfMeasureInfo->CLK_RST_base,HwUnitClockResetRegOffset);
       RegValue = (RegValue & 0xFFFFFF00);
       RegValue = ((RegValue) | (ClockDivisor & 0xFF));
       CLK_RST_REGW(pPerfMeasureInfo->CLK_RST_base,HwUnitClockResetRegOffset,RegValue);
    }
    else
    {
       /*For Hw Unit's PLL source to PLLM set and Clock divisor bits*/
       RegValue = (((HwUnitClockSource) << 29) | (ClockDivisor & 0xFF));
       CLK_RST_REGW(pPerfMeasureInfo->CLK_RST_base,HwUnitClockResetRegOffset,RegValue);
    }
#endif
    return NvSuccess;
}

NvError NvPerfMeasureStart(NvPerfMeasureInfo *pPerfMeasureInfo)
{

    if (pPerfMeasureInfo == NULL)
    {
        return NvError_InsufficientMemory;
    }

    if (pPerfMeasureInfo->EMC_base == 0)
    {
        return NvError_InsufficientMemory;
    }

    /*Clear EMC Stats Collection Register*/
    EMC_REGW(pPerfMeasureInfo->EMC_base,EMC_STAT_CONTROL_0,EMC_STAT_CONTROL_0_RESET_VAL);

    /* Set Limit for EMC cycle Count Value*/
    EMC_REGW(pPerfMeasureInfo->EMC_base,EMC_STAT_DRAM_CLOCK_LIMIT_LO_0,EMC_STAT_DRAM_CLOCK_LIMIT_LO_0_RESET_VAL);
    EMC_REGW(pPerfMeasureInfo->EMC_base,EMC_STAT_DRAM_CLOCK_LIMIT_HI_0,EMC_STAT_DRAM_CLOCK_LIMIT_HI_0_RESET_VAL);

    /*Turn ON EMC Stats Collection Register*/
    EMC_REGW(pPerfMeasureInfo->EMC_base,EMC_STAT_CONTROL_0,EMC_STAT_CONTROL_0_DRAM_GATHER_FIELD);

    return NvSuccess;
}

NvError NvPerfMeasureStop(NvPerfMeasureInfo *pPerfMeasureInfo,NvU64 *HwCycleCount)
{
    NvU32 RegVal = 0;
    NvU64 EmcCycleCount = 0;

    if (pPerfMeasureInfo == NULL)
    {
        return NvError_InsufficientMemory;
    }

    if (pPerfMeasureInfo->EMC_base == 0)
    {
        return NvError_InsufficientMemory;
    }

    /*Turn OFF EMC Stats Collection Register*/
    RegVal = EMC_STAT_CONTROL_0_DRAM_GATHER_DISABLE << EMC_STAT_CONTROL_0_DRAM_GATHER_SHIFT;
    EMC_REGW(pPerfMeasureInfo->EMC_base,EMC_STAT_CONTROL_0,RegVal);

    /*READ EMC Cycle Count*/
    RegVal  =  EMC_REGR(pPerfMeasureInfo->EMC_base,EMC_STAT_DRAM_CLOCKS_LO_0);
    EmcCycleCount =  EMC_REGR(pPerfMeasureInfo->EMC_base,EMC_STAT_DRAM_CLOCKS_HI_0);
    EmcCycleCount = (EmcCycleCount << 32);
    EmcCycleCount = (EmcCycleCount) | (RegVal) ;

#if RUNNING_IN_SIMULATION
    *HwCycleCount = (NvU64)(EmcCycleCount/pPerfMeasureInfo->HwUnitClockDivisor) ;
#else
    *HwCycleCount = EmcCycleCount;
#endif
    return NvSuccess;
}

NvError NvPerfMeasureDeInit(NvPerfMeasureInfo *pPerfMeasureInfo)
{
    if (pPerfMeasureInfo == NULL)
    {
        return NvError_InsufficientMemory;
    }

#if RUNNING_IN_SIMULATION
    NvRmClose(pPerfMeasureInfo->hRmDevice);
    NvOsFree(pPerfMeasureInfo);
#endif

    if (pPerfMeasureInfo != NULL)
    {
        return NvError_InsufficientMemory;
    }

    return NvSuccess;
}

#ifdef __cplusplus
}
#endif

#endif
