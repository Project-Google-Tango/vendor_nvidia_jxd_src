/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef TPS65090_REG_HEADER
#define TPS65090_REG_HEADER

#if defined(__cplusplus)
extern "C"
{
#endif

/* TPS65090 registers */

#define TPS65090_R00_IRQ1                  0
#define TPS65090_R01_IRQ2                  1
#define TPS65090_R02_IRQ1MASK              2
#define TPS65090_R03_IRQ2MASK              3
#define TPS65090_R04_CG_CTRL0              4
#define TPS65090_R05_CG_CTRL1              5
#define TPS65090_R06_CG_CTRL2              6
#define TPS65090_R07_CG_CTRL3              7
#define TPS65090_R08_CG_CTRL4              8
#define TPS65090_R09_CG_CTRL5              9
#define TPS65090_R10_CG_STATUS1            10
#define TPS65090_R11_CG_STATUS2            11
#define TPS65090_R12_DCDC1_CTRL            12
#define TPS65090_R13_DCDC2_CTRL            13
#define TPS65090_R14_DCDC3_CTRL            14
#define TPS65090_R15_FET1_CTRL             15
#define TPS65090_R16_FET2_CTRL             16
#define TPS65090_R17_FET3_CTRL             17
#define TPS65090_R18_FET4_CTRL             18
#define TPS65090_R19_FET5_CTRL             19
#define TPS65090_R20_FET6_CTRL             20
#define TPS65090_R21_FET7_CTRL             21
#define TPS65090_R22_AD_CTRL               22
#define TPS65090_R23_AD_OUT1               23
#define TPS65090_R24_AD_OUT2               24
#define TPS65090_RFF_INVALID             0xFF

#if defined(__cplusplus)
}
#endif

#endif /* TPS65090_REG_HEADER */
