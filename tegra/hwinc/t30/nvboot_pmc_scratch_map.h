/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines fields in the PMC scratch registers used by the Boot ROM code.
 */

#ifndef INCLUDED_NVBOOT_PMC_SCRATCH_MAP_H
#define INCLUDED_NVBOOT_PMC_SCRATCH_MAP_H

/**
 * The PMC module in the Always On domain of the chip provides 43
 * scratch registers and 6 secure scratch registers. These offer SW a
 * storage space that preserves values across LP0 transitions.
 *
 * These are used by to store state information for on-chip controllers which
 * are to be restored during Warm Boot, whether WB0 or WB1.
 *
 * Scratch registers offsets are part of PMC HW specification - "arapbpm.spec".
 *
 * This header file defines the allocation of scratch register space for
 * storing data needed by Warm Boot.
 *
 * Each of the scratch registers has been sliced into bit fields to store
 * parameters for various on-chip controllers. Every bit field that needs to
 * be stored has a matching bit field in a scratch register. The width matches
 * the original bit fields.
 *
 * Scratch register fields have been defined with self explanatory names
 * and with bit ranges compatible with nvrm_drf macros.
 *
 * Ownership Issues: Important!!!!
 *
 * Register APBDEV_PMC_SCRATCH0_0 is the *only* scratch register cleared on
 * power-on-reset. This register will also be used by RM and OAL. This holds
 * several important flags for the Boot ROM:
 *     WARM_BOOT0_FLAG: Tells the Boot ROM to perform WB0 upon reboot instead
 *                    of a cold boot.
 *     FORCE_RECOVERY_FLAG:
 *         Forces the Boot ROM to enter RCM instead of performing a cold
 *         boot or WB0.
 *     BL_FAIL_BACK_FLAG:
 *         One of several indicators that the the Boot ROM should fail back
 *         older generations of BLs if the newer generations fail to load.
 *     FUSE_ALIAS_FLAG:
 *         Indicates that the Boot ROM should alias the fuses with values
 *         stored in PMC SCRATCH registers and restart.
 *     STRAP_ALIAS_FLAG:
 *         Same as FUSE_ALIAS_FLAG, but for strap aliasing.
 *
 * The assignment of bit fields used by BootROM is *NOT* to be changed.
 *
 */

/**
 * FUSE_ALIAS:
 *   Desc: Enables fuse aliasing code when set to 1.  In Pre-Production mode,
 *     the Boot ROM will alias the fuses and re-start the boot process using
 *     the new fuse values. Note that the Boot ROM clears this flag when it
 *     performs the aliasing to avoid falling into an infinite loop.
 *     Unlike AP15/AP16, the alias values are pulled from PMC scratch registers
 *     to survive an LP0 transition.
 * STRAP_ALIAS:
 *   Desc: Enables strap aliasing code when set to 1.  In Pre-Production mode,
 *     the Boot ROM will alias the straps and re-start the boot process using
 *     the new strap values. Note that the Boot ROM does not clear this flag
 *     when it aliases the straps, as this cannot lead to an infinite loop.
 *     Unlike AP15/AP16, the alias values are pulled from PMC scratch registers
 *     to survive an LP0 transition.
 */
#define APBDEV_PMC_SCRATCH0_0_WARM_BOOT0_FLAG_RANGE                        0: 0
#define APBDEV_PMC_SCRATCH0_0_FORCE_RECOVERY_FLAG_RANGE                    1: 1
#define APBDEV_PMC_SCRATCH0_0_BL_FAIL_BACK_FLAG_RANGE                      2: 2
#define APBDEV_PMC_SCRATCH0_0_FUSE_ALIAS_FLAG_RANGE                        3: 3
#define APBDEV_PMC_SCRATCH0_0_STRAP_ALIAS_FLAG_RANGE                       4: 4
// Bits 31:5 reserved for SW

// Likely needed for SW in same place        
#define APBDEV_PMC_SCRATCH1_0_PTR_TO_RECOVERY_CODE_RANGE                  31: 0

// TODO: These register bits may not be needed w/PLLM auto-restart.
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_BASE_PLLM_DIVM_RANGE            4: 0
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_BASE_PLLM_DIVN_RANGE           14: 5
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_BASE_PLLM_DIVP_RANGE           17:15
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_MISC_LFCON_RANGE               21:18
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_MISC_CPCON_RANGE               25:22
// Bits 31:26 available

// Note: APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_CHOICE_RANGE identifies the choice
//       of PLL to start w/PLLX parameters: X or C.
// TODO: Make it so!
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_BASE_PLLX_DIVM_RANGE            4: 0
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_BASE_PLLX_DIVN_RANGE           14: 5
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_BASE_PLLX_DIVP_RANGE           17:15
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_MISC_LFCON_RANGE               21:18
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_MISC_CPCON_RANGE               25:22
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_CHOICE_RANGE                   26:26
// Bits 31:27 available

#define APBDEV_PMC_SCRATCH4_0_PLLM_STABLE_TIME_RANGE                       9: 0
#define APBDEV_PMC_SCRATCH4_0_PLLX_STABLE_TIME_RANGE                      19:10
#define APBDEV_PMC_SCRATCH4_0_CPU_WAKEUP_CLUSTER_RANGE                    31:31
// Bits 30:20 available

// SDRAM PMC registers
// SDRAM PMC register generated by tool warmboot_code_gen
// DDR3 only
#define APBDEV_PMC_SCRATCH5_0_EMC_WARM_BOOT_EMR3_0_EMRS_ADR_RANGE           13: 0
// DDR3 only
#define APBDEV_PMC_SCRATCH5_0_EMC_WARM_BOOT_EMR2_0_EMRS_ADR_RANGE           27:14
// DDR3 only
#define APBDEV_PMC_SCRATCH5_0_EMC_WARM_BOOT_EMR3_0_EMRS_BA_RANGE            29:28
// DDR3 only
#define APBDEV_PMC_SCRATCH5_0_EMC_WARM_BOOT_EMR2_0_EMRS_BA_RANGE            31:30
// LPDDR2 only
#define APBDEV_PMC_SCRATCH5_0_EMC_MRW_LPDDR2_ZCAL_WARM_BOOT_0_MRW_MA_RANGE   7: 0
// LPDDR2 only
#define APBDEV_PMC_SCRATCH5_0_EMC_MRW_LPDDR2_ZCAL_WARM_BOOT_0_MRW_OP_RANGE  15: 8
// LPDDR2 only
#define APBDEV_PMC_SCRATCH5_0_EMC_WARM_BOOT_MRW1_0_MRW_MA_RANGE             23:16
// LPDDR2 only
#define APBDEV_PMC_SCRATCH5_0_EMC_WARM_BOOT_MRW1_0_MRW_OP_RANGE             31:24

// DDR3 only
#define APBDEV_PMC_SCRATCH6_0_EMC_WARM_BOOT_MRS_EXTRA_0_MRS_ADR_RANGE       13: 0
// DDR3 only
#define APBDEV_PMC_SCRATCH6_0_EMC_WARM_BOOT_MRS_0_MRS_ADR_RANGE             27:14
// DDR3 only
#define APBDEV_PMC_SCRATCH6_0_EMC_WARM_BOOT_MRS_EXTRA_0_MRS_BA_RANGE        29:28
// DDR3 only
#define APBDEV_PMC_SCRATCH6_0_EMC_WARM_BOOT_MRS_0_MRS_BA_RANGE              31:30
// LPDDR2 only
#define APBDEV_PMC_SCRATCH6_0_EMC_WARM_BOOT_MRW3_0_MRW_MA_RANGE              7: 0
// LPDDR2 only
#define APBDEV_PMC_SCRATCH6_0_EMC_WARM_BOOT_MRW3_0_MRW_OP_RANGE             15: 8
// LPDDR2 only
#define APBDEV_PMC_SCRATCH6_0_EMC_WARM_BOOT_MRW2_0_MRW_MA_RANGE             23:16
// LPDDR2 only
#define APBDEV_PMC_SCRATCH6_0_EMC_WARM_BOOT_MRW2_0_MRW_OP_RANGE             31:24

// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_EMRS_0_EMRS_ADR_RANGE           13: 0
// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_EMRS_0_EMRS_BA_RANGE            15:14
// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_EMR3_0_USE_EMRS_LONG_CNT_RANGE  16:16
// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_EMR2_0_USE_EMRS_LONG_CNT_RANGE  17:17
// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_MRS_EXTRA_0_USE_MRS_LONG_CNT_RANGE\
                                                                            18:18
// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_MRS_0_USE_MRS_LONG_CNT_RANGE    19:19
// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_EMRS_0_USE_EMRS_LONG_CNT_RANGE  20:20
// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_ZQ_CAL_DDR3_WARM_BOOT_0_ZQ_CAL_CMD_RANGE  21:21
// DDR3 only
#define APBDEV_PMC_SCRATCH7_0_EMC_ZQ_CAL_DDR3_WARM_BOOT_0_ZQ_CAL_LENGTH_RANGE\
                                                                            22:22
// LPDDR2 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_MRW_EXTRA_0_MRW_MA_RANGE         7: 0
// LPDDR2 only
#define APBDEV_PMC_SCRATCH7_0_EMC_WARM_BOOT_MRW_EXTRA_0_MRW_OP_RANGE        15: 8
#define APBDEV_PMC_SCRATCH7_0_EMC_RFC_0_RFC_RANGE                           31:23

#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_EN_RANGE             0: 0
#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_OVERRIDE_EN_RANGE    1: 1
#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_STALL_RW_UNTIL_LOCK_RANGE\
                                                                             2: 2
#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_LOWSPEED_RANGE       3: 3
#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_MODE_RANGE           5: 4
#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_UDSET_RANGE          9: 6
#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_OVERRIDE_VAL_RANGE  19:10
#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_ALARM_DISABLE_RANGE 20:20
#define APBDEV_PMC_SCRATCH8_0_EMC_CFG_DIG_DLL_0_CFG_DLL_LOCK_LIMIT_RANGE    22:21
#define APBDEV_PMC_SCRATCH8_0_EMC_XM2CMDPADCTRL_0_EMC2TMC_CFG_XM2CMD_E_PREEMP_RANGE\
                                                                            23:23
#define APBDEV_PMC_SCRATCH8_0_EMC_XM2CMDPADCTRL_0_CFG_XM2CMD_CAL_SELECT_RANGE\
                                                                            24:24
#define APBDEV_PMC_SCRATCH8_0_EMC_XM2CMDPADCTRL_0_EMC2PMACRO_CFG_XM2CMD_DIGTRIM_RANGE\
                                                                            31:25

#define APBDEV_PMC_SCRATCH9_0_EMC_DLL_XFORM_DQS0_0_XFORM_DQS0_MULT_RANGE     4: 0
#define APBDEV_PMC_SCRATCH9_0_EMC_DLL_XFORM_DQS0_0_XFORM_DQS0_OFFS_RANGE    19: 5
#define APBDEV_PMC_SCRATCH9_0_EMC_XM2CLKPADCTRL_0_EMC2PMACRO_CFG_DYN_PULLS_ON_CLKDIS_RANGE\
                                                                            20:20
#define APBDEV_PMC_SCRATCH9_0_EMC_XM2CLKPADCTRL_0_EMC2TMC_CFG_XM2CLK_E_PREEMP_RANGE\
                                                                            21:21
#define APBDEV_PMC_SCRATCH9_0_EMC_XM2CLKPADCTRL_0_EMC2PMACRO_CFG_XM2CLK_DRVDN_RANGE\
                                                                            26:22
#define APBDEV_PMC_SCRATCH9_0_EMC_XM2CLKPADCTRL_0_EMC2PMACRO_CFG_XM2CLK_DRVUP_RANGE\
                                                                            31:27

#define APBDEV_PMC_SCRATCH10_0_EMC_DLL_XFORM_QUSE0_0_XFORM_QUSE0_MULT_RANGE  4: 0
#define APBDEV_PMC_SCRATCH10_0_EMC_DLL_XFORM_QUSE0_0_XFORM_QUSE0_OFFS_RANGE 19: 5
#define APBDEV_PMC_SCRATCH10_0_EMC_AUTO_CAL_CONFIG_0_AUTO_CAL_PU_OFFSET_RANGE\
                                                                            24:20
#define APBDEV_PMC_SCRATCH10_0_EMC_AUTO_CAL_CONFIG_0_AUTO_CAL_PD_OFFSET_RANGE\
                                                                            29:25
#define APBDEV_PMC_SCRATCH10_0_EMC_AUTO_CAL_CONFIG_0_AUTO_CAL_OVERRIDE_RANGE\
                                                                            30:30
#define APBDEV_PMC_SCRATCH10_0_EMC_CFG_0_PERIODIC_QRST_RANGE                31:31

#define APBDEV_PMC_SCRATCH11_0_EMC_DLL_XFORM_DQ0_0_XFORM_TXDQ0_MULT_RANGE    4: 0
#define APBDEV_PMC_SCRATCH11_0_EMC_DLL_XFORM_DQ0_0_XFORM_TXDQ0_OFFS_RANGE   19: 5
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_POP_CLK_SEL_DPD_EN_RANGE  20:20
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_POP_CA_SEL_DPD_EN_RANGE   21:21
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_CLK_SEL_DPD_EN_RANGE      22:22
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_CA_SEL_DPD_EN_RANGE       23:23
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_RESET_SEL_DPD_EN_RANGE    24:24
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_ODT_SEL_DPD_EN_RANGE      25:25
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_DATA_SEL_DPD_EN_RANGE     26:26
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_QUSE_SEL_DPD_EN_RANGE     27:27
#define APBDEV_PMC_SCRATCH11_0_EMC_SEL_DPD_CTRL_0_SEL_DPD_DLY_RANGE         30:28
#define APBDEV_PMC_SCRATCH11_0_EMC_ADR_CFG_0_EMEM_ROW_MSB_ON_CS1_RANGE      31:31

#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_EARLY_TRFC_8_CLK_RANGE            0: 0
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_PIN_CONFIG_RANGE                  2: 1
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_DONT_GEN_EARLY_TRFC_DONE_RANGE    3: 3
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_ACPD_WAKEUP_NO_COND_RANGE         4: 4
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_USE_PER_DEVICE_DLY_TRIM_IB_RANGE  5: 5
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_USE_PER_DEVICE_DLY_TRIM_OB_RANGE  6: 6
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_DONT_GEN_EARLY_MRS_DONE_RANGE     7: 7
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_BYPASS_POP_PIPE1_STAGE_RANGE      8: 8
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_BYPASS_POP_PIPE2_STAGE_RANGE      9: 9
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_ISSUE_PCHGALL_AFTER_REF_RANGE    10:10
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_DONT_USE_REF_REQACK_IFC_RANGE    11:11
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_ALLOW_REF_DURING_CC_PRE_EXE_RANGE\
                                                                            12:12
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_DRAMC_WD_CHK_POLICY_RANGE        14:13
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_DONT_CLR_TIMING_COUNTER_WHEN_CLKCHANGE_RANGE\
                                                                            15:15
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_CLR_ACT_BANK_INUSE_WHEN_BANK_CLOSE_RANGE\
                                                                            16:16
#define APBDEV_PMC_SCRATCH12_0_EMC_CFG_2_0_IGNORE_MC_A_BUS_RANGE            17:17
#define APBDEV_PMC_SCRATCH12_0_EMC_TREFBW_0_TREFBW_RANGE                    31:18

#define APBDEV_PMC_SCRATCH13_0_EMC_ZCAL_MRW_CMD_0_ZQ_MRW_OP_RANGE            7: 0
#define APBDEV_PMC_SCRATCH13_0_EMC_ZCAL_MRW_CMD_0_ZQ_MRW_MA_RANGE           15: 8
#define APBDEV_PMC_SCRATCH13_0_EMC_DLL_XFORM_DQS1_0_XFORM_DQS1_OFFS_RANGE   30:16
#define APBDEV_PMC_SCRATCH13_0_MC_EMEM_ADR_CFG_0_EMEM_NUMDEV_RANGE          31:31

#define APBDEV_PMC_SCRATCH14_0_EMC_DLL_XFORM_DQS2_0_XFORM_DQS2_OFFS_RANGE   14: 0
#define APBDEV_PMC_SCRATCH14_0_EMC_DLL_XFORM_DQS3_0_XFORM_DQS3_OFFS_RANGE   29:15
#define APBDEV_PMC_SCRATCH14_0_EMC_DEV_SELN_RANGE                           31:30

#define APBDEV_PMC_SCRATCH15_0_EMC_DLL_XFORM_DQS4_0_XFORM_DQS4_OFFS_RANGE   14: 0
#define APBDEV_PMC_SCRATCH15_0_EMC_DLL_XFORM_DQS5_0_XFORM_DQS5_OFFS_RANGE   29:15
#define APBDEV_PMC_SCRATCH15_0_EMC_DIG_DLL_PERIOD_WARM_BOOT_RANGE           31:30

#define APBDEV_PMC_SCRATCH16_0_EMC_DLL_XFORM_DQS6_0_XFORM_DQS6_OFFS_RANGE   14: 0
#define APBDEV_PMC_SCRATCH16_0_EMC_DLL_XFORM_DQS7_0_XFORM_DQS7_OFFS_RANGE   29:15
#define APBDEV_PMC_SCRATCH16_0_MC_EMEM_ARB_MISC0_0_MC_EMC_SAME_FREQ_RANGE   30:30
#define APBDEV_PMC_SCRATCH16_0_EMC_EXTRA_MODE_REG_WRITE_ENABLE_RANGE        31:31

#define APBDEV_PMC_SCRATCH17_0_EMC_DLL_XFORM_QUSE1_0_XFORM_QUSE1_OFFS_RANGE 14: 0
#define APBDEV_PMC_SCRATCH17_0_EMC_DLL_XFORM_QUSE2_0_XFORM_QUSE2_OFFS_RANGE 29:15
#define APBDEV_PMC_SCRATCH17_0_MC_CLK_EN_OVERRIDE_ALL_WARM_BOOT_RANGE       30:30
#define APBDEV_PMC_SCRATCH17_0_EMC_CLK_EN_OVERRIDE_ALL_WARM_BOOT_RANGE      31:31

#define APBDEV_PMC_SCRATCH18_0_EMC_DLL_XFORM_QUSE3_0_XFORM_QUSE3_OFFS_RANGE 14: 0
#define APBDEV_PMC_SCRATCH18_0_EMC_DLL_XFORM_QUSE4_0_XFORM_QUSE4_OFFS_RANGE 29:15
#define APBDEV_PMC_SCRATCH18_0_EMC_MRS_WARM_BOOT_ENABLE_RANGE               30:30
#define APBDEV_PMC_SCRATCH18_0_EMC_ZCAL_WARM_BOOT_ENABLE_RANGE              31:31

#define APBDEV_PMC_SCRATCH19_0_EMC_DLL_XFORM_QUSE5_0_XFORM_QUSE5_OFFS_RANGE 14: 0
#define APBDEV_PMC_SCRATCH19_0_EMC_DLL_XFORM_QUSE6_0_XFORM_QUSE6_OFFS_RANGE 29:15
#define APBDEV_PMC_SCRATCH19_0_EMC_FBIO_CFG5_0_DRAM_TYPE_RANGE              31:30

#define APBDEV_PMC_SCRATCH22_0_EMC_DLL_XFORM_QUSE7_0_XFORM_QUSE7_OFFS_RANGE 14: 0
#define APBDEV_PMC_SCRATCH22_0_EMC_DLL_XFORM_DQ1_0_XFORM_TXDQ1_OFFS_RANGE   29:15
#define APBDEV_PMC_SCRATCH22_0_EMC_FBIO_CFG5_0_DRAM_BURST_RANGE             31:30

#define APBDEV_PMC_SCRATCH23_0_EMC_DLL_XFORM_DQ2_0_XFORM_TXDQ2_OFFS_RANGE   14: 0
#define APBDEV_PMC_SCRATCH23_0_EMC_DLL_XFORM_DQ3_0_XFORM_TXDQ3_OFFS_RANGE   29:15
#define APBDEV_PMC_SCRATCH23_0_CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_SRC_RANGE\
                                                                            31:30

#define APBDEV_PMC_SCRATCH24_0_EMC_ZCAL_INTERVAL_0_ZCAL_INTERVAL_HI_RANGE   13: 0
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_E_RX_FT_REC_RANGE\
                                                                            14:14
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_E_PREEMP_RANGE\
                                                                            15:15
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_E_CTT_HIZ_DQ_RANGE\
                                                                            16:16
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_E_CTT_HIZ_DQS_RANGE\
                                                                            17:17
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_E_VREF_DQ_RANGE\
                                                                            18:18
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_RX_DLI_EN_RANGE\
                                                                            19:19
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_TXDQ_DLI_EN_RANGE\
                                                                            20:20
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_TXDQS_DLI_EN_RANGE\
                                                                            21:21
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_QUSE_DLI_EN_RANGE\
                                                                            22:22
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_E_SCHMT_RANGE\
                                                                            23:23
#define APBDEV_PMC_SCRATCH24_0_EMC_XM2DQSPADCTRL2_0_EMC2TMC_CFG_XM2DQS_VREF_DQ_RANGE\
                                                                            27:24
#define APBDEV_PMC_SCRATCH24_0_EMC_RRD_0_RRD_RANGE                          31:28

#define APBDEV_PMC_SCRATCH25_0_EMC_XM2VTTGENPADCTRL_0_EMC2TMC_CFG_XM2VTTGEN_SHORT_RANGE\
                                                                             0: 0
#define APBDEV_PMC_SCRATCH25_0_EMC_XM2VTTGENPADCTRL_0_EMC2TMC_CFG_XM2VTTGEN_VCLAMP_LEVEL_RANGE\
                                                                             3: 1
#define APBDEV_PMC_SCRATCH25_0_EMC_XM2VTTGENPADCTRL_0_EMC2TMC_CFG_XM2VTTGEN_VAUXP_LEVEL_RANGE\
                                                                             6: 4
#define APBDEV_PMC_SCRATCH25_0_EMC_XM2VTTGENPADCTRL_0_EMC2TMC_CFG_XM2VTTGEN_DRVDN_RANGE\
                                                                             9: 7
#define APBDEV_PMC_SCRATCH25_0_EMC_XM2VTTGENPADCTRL_0_EMC2TMC_CFG_XM2VTTGEN_DRVUP_RANGE\
                                                                            12:10
#define APBDEV_PMC_SCRATCH25_0_MC_EMEM_ARB_OVERRIDE_0_ARB_EMEM_AP_OVERRIDE_RANGE\
                                                                            13:13
#define APBDEV_PMC_SCRATCH25_0_MC_EMEM_ARB_OVERRIDE_0_ARB_HUM_FIFO_OVERRIDE_RANGE\
                                                                            14:14
#define APBDEV_PMC_SCRATCH25_0_MC_EMEM_ARB_OVERRIDE_0_ARB_EMEM_COALESCE_OVERRIDE_RANGE\
                                                                            15:15
#define APBDEV_PMC_SCRATCH25_0_MC_EMEM_ARB_OVERRIDE_0_ALLOC_ONE_BQ_PER_CLIENT_RANGE\
                                                                            16:16
#define APBDEV_PMC_SCRATCH25_0_MC_EMEM_ARB_OVERRIDE_0_OBSERVED_DIRECTION_OVERRIDE_RANGE\
                                                                            17:17
#define APBDEV_PMC_SCRATCH25_0_MC_EMEM_ARB_OVERRIDE_0_OVERRIDE_RESERVED_BYTE0_RANGE\
                                                                            25:18
#define APBDEV_PMC_SCRATCH25_0_EMC_RAS_0_RAS_RANGE                          31:26

#define APBDEV_PMC_SCRATCH26_0_EMC_REFRESH_0_REFRESH_RANGE                   9: 0
#define APBDEV_PMC_SCRATCH26_0_EMC_ZCAL_WAIT_CNT_0_ZCAL_WAIT_CNT_RANGE      19:10
#define APBDEV_PMC_SCRATCH26_0_EMC_XM2VTTGENPADCTRL2_0_EMC2TMC_CFG_XM2VTTGEN_E_NO_VTTGEN_RANGE\
                                                                            22:20
#define APBDEV_PMC_SCRATCH26_0_EMC_XM2VTTGENPADCTRL2_0_EMC2TMC_CFG_XM2VTTGEN_MEMCLK_E_TEST_BIAS_RANGE\
                                                                            24:23
#define APBDEV_PMC_SCRATCH26_0_EMC_XM2VTTGENPADCTRL2_0_EMC2TMC_CFG_XM2VTTGEN_MEM_E_TEST_BIAS_RANGE\
                                                                            26:25
#define APBDEV_PMC_SCRATCH26_0_EMC_XM2VTTGENPADCTRL2_0_EMC2TMC_CFG_XM2VTTGEN_MEM2_E_TEST_BIAS_RANGE\
                                                                            28:27
#define APBDEV_PMC_SCRATCH26_0_EMC_FBIO_CFG6_0_CFG_QUSE_LATE_RANGE          31:29

#define APBDEV_PMC_SCRATCH27_0_EMC_XM2QUSEPADCTRL_0_EMC2TMC_CFG_XM2QUSE_E_RX_FT_REC_RANGE\
                                                                             0: 0
#define APBDEV_PMC_SCRATCH27_0_EMC_XM2QUSEPADCTRL_0_EMC2TMC_CFG_XM2QUSE_E_PREEMP_RANGE\
                                                                             1: 1
#define APBDEV_PMC_SCRATCH27_0_EMC_XM2QUSEPADCTRL_0_EMC2TMC_CFG_XM2QUSE_E_CTT_HIZ_RANGE\
                                                                             2: 2
#define APBDEV_PMC_SCRATCH27_0_EMC_XM2QUSEPADCTRL_0_EMC2TMC_CFG_XM2QUSE_E_IVREF_RANGE\
                                                                             3: 3
#define APBDEV_PMC_SCRATCH27_0_EMC_XM2QUSEPADCTRL_0_EMC2TMC_CFG_XM2QUSE_E_SCHMT_RANGE\
                                                                             4: 4
#define APBDEV_PMC_SCRATCH27_0_EMC_XM2QUSEPADCTRL_0_EMC2TMC_CFG_XM2QUSE_VREF_RANGE\
                                                                             8: 5
#define APBDEV_PMC_SCRATCH27_0_MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_COLWIDTH_RANGE\
                                                                            11: 9
#define APBDEV_PMC_SCRATCH27_0_MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_BANKWIDTH_RANGE\
                                                                            13:12
#define APBDEV_PMC_SCRATCH27_0_MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_RANGE\
                                                                            17:14
#define APBDEV_PMC_SCRATCH27_0_MC_EMEM_ADR_CFG_DEV1_0_EMEM_DEV1_COLWIDTH_RANGE\
                                                                            20:18
#define APBDEV_PMC_SCRATCH27_0_MC_EMEM_ADR_CFG_DEV1_0_EMEM_DEV1_BANKWIDTH_RANGE\
                                                                            22:21
#define APBDEV_PMC_SCRATCH27_0_MC_EMEM_ADR_CFG_DEV1_0_EMEM_DEV1_DEVSIZE_RANGE\
                                                                            26:23
#define APBDEV_PMC_SCRATCH27_0_EMC_R2W_0_R2W_RANGE                          31:27

#define APBDEV_PMC_SCRATCH28_0_EMC_FBIO_SPARE_0_CFG_FBIO_SPARE_3_RANGE       7: 0
#define APBDEV_PMC_SCRATCH28_0_EMC_CFG_RSV_0_CFG_RESERVED_BYTE0_RANGE       15: 8
#define APBDEV_PMC_SCRATCH28_0_EMC_XM2COMPPADCTRL_0_EMC2TMC_CFG_XM2COMP_VREF_SEL_RANGE\
                                                                            19:16
#define APBDEV_PMC_SCRATCH28_0_EMC_XM2COMPPADCTRL_0_EMC2TMC_CFG_XM2COMP_BIAS_SEL_RANGE\
                                                                            22:20
#define APBDEV_PMC_SCRATCH28_0_EMC_XM2COMPPADCTRL_0_CFG_XM2COMP_VREF_CAL_EN_RANGE\
                                                                            23:23
#define APBDEV_PMC_SCRATCH28_0_MC_EMEM_ARB_RSV_0_EMEM_ARB_RESERVED_BYTE0_RANGE\
                                                                            31:24

#define APBDEV_PMC_SCRATCH29_0_EMC_TIMING_CONTROL_WAIT_RANGE                 7: 0
#define APBDEV_PMC_SCRATCH29_0_EMC_ZCAL_WARM_BOOT_WAIT_RANGE                15: 8
#define APBDEV_PMC_SCRATCH29_0_EMC_AUTO_CAL_TIME_RANGE                      23:16
#define APBDEV_PMC_SCRATCH29_0_WARM_BOOT_WAIT_RANGE                         31:24

#define APBDEV_PMC_SCRATCH30_0_EMC_PIN_PROGRAM_WAIT_RANGE                    7: 0
#define APBDEV_PMC_SCRATCH30_0_EMC_RC_0_RC_RANGE                            14: 8
#define APBDEV_PMC_SCRATCH30_0_EMC_DLI_TRIM_TXDQS0_0_EMC2PMACRO_CFG_TXDQS0_DLI_RANGE\
                                                                            21:15
#define APBDEV_PMC_SCRATCH30_0_EMC_DLI_TRIM_TXDQS1_0_EMC2PMACRO_CFG_TXDQS1_DLI_RANGE\
                                                                            28:22
#define APBDEV_PMC_SCRATCH30_0_EMC_EXTRA_REFRES_NUM_RANGE                   31:29

#define APBDEV_PMC_SCRATCH31_0_EMC_DLI_TRIM_TXDQS2_0_EMC2PMACRO_CFG_TXDQS2_DLI_RANGE\
                                                                             6: 0
#define APBDEV_PMC_SCRATCH31_0_EMC_DLI_TRIM_TXDQS3_0_EMC2PMACRO_CFG_TXDQS3_DLI_RANGE\
                                                                            13: 7
#define APBDEV_PMC_SCRATCH31_0_EMC_DLI_TRIM_TXDQS4_0_EMC2PMACRO_CFG_TXDQS4_DLI_RANGE\
                                                                            20:14
#define APBDEV_PMC_SCRATCH31_0_EMC_DLI_TRIM_TXDQS5_0_EMC2PMACRO_CFG_TXDQS5_DLI_RANGE\
                                                                            27:21
#define APBDEV_PMC_SCRATCH31_0_EMC_REXT_0_REXT_RANGE                        31:28

#define APBDEV_PMC_SCRATCH32_0_EMC_DLI_TRIM_TXDQS6_0_EMC2PMACRO_CFG_TXDQS6_DLI_RANGE\
                                                                             6: 0
#define APBDEV_PMC_SCRATCH32_0_EMC_DLI_TRIM_TXDQS7_0_EMC2PMACRO_CFG_TXDQS7_DLI_RANGE\
                                                                            13: 7
#define APBDEV_PMC_SCRATCH32_0_EMC_RP_0_RP_RANGE                            19:14
#define APBDEV_PMC_SCRATCH32_0_EMC_W2P_0_W2P_RANGE                          25:20
#define APBDEV_PMC_SCRATCH32_0_EMC_RD_RCD_0_RD_RCD_RANGE                    31:26

#define APBDEV_PMC_SCRATCH33_0_EMC_WR_RCD_0_WR_RCD_RANGE                     5: 0
#define APBDEV_PMC_SCRATCH33_0_EMC_TFAW_0_TFAW_RANGE                        11: 6
#define APBDEV_PMC_SCRATCH33_0_EMC_TRPAB_0_TRPAB_RANGE                      17:12
#define APBDEV_PMC_SCRATCH33_0_EMC_ODT_WRITE_0_ODT_WR_DELAY_RANGE           20:18
#define APBDEV_PMC_SCRATCH33_0_EMC_ODT_WRITE_0_DRIVE_BOTH_ODT_RANGE         21:21
#define APBDEV_PMC_SCRATCH33_0_EMC_ODT_WRITE_0_ODT_B4_WRITE_RANGE           22:22
#define APBDEV_PMC_SCRATCH33_0_EMC_ODT_WRITE_0_ENABLE_ODT_DURING_WRITE_RANGE\
                                                                            23:23
#define APBDEV_PMC_SCRATCH33_0_EMC_XM2DQSPADCTRL3_0_EMC2PMACRO_CFG_XM2DQS_E_STRPULL_DQS_RANGE\
                                                                            24:24
#define APBDEV_PMC_SCRATCH33_0_EMC_XM2DQSPADCTRL3_0_EMC2PMACRO_CFG_XM2DQS_E_VREF_DQS_RANGE\
                                                                            25:25
#define APBDEV_PMC_SCRATCH33_0_EMC_XM2DQSPADCTRL3_0_EMC2PMACRO_CFG_XM2DQS_VREF_DQS_RANGE\
                                                                            29:26
#define APBDEV_PMC_SCRATCH33_0_AHB_ARBITRATION_XBAR_CTRL_0_MEM_INIT_DONE_RANGE\
                                                                            30:30
#define APBDEV_PMC_SCRATCH33_0_EMC_FBIO_CFG5_0_DIFFERENTIAL_DQS_RANGE       31:31

#define APBDEV_PMC_SCRATCH40_0_EMC_W2R_0_W2R_RANGE                           4: 0
#define APBDEV_PMC_SCRATCH40_0_EMC_R2P_0_R2P_RANGE                           9: 5
#define APBDEV_PMC_SCRATCH40_0_EMC_QUSE_0_QUSE_RANGE                        14:10
#define APBDEV_PMC_SCRATCH40_0_EMC_QRST_0_QRST_RANGE                        19:15
#define APBDEV_PMC_SCRATCH40_0_EMC_RDV_0_RDV_RANGE                          24:20
#define APBDEV_PMC_SCRATCH40_0_EMC_CTT_0_CTT_RANGE                          29:25
#define APBDEV_PMC_SCRATCH40_0_EMC_FBIO_CFG5_0_CTT_TERMINATION_RANGE        30:30
#define APBDEV_PMC_SCRATCH40_0_EMC_FBIO_CFG5_0_DQS_PULLD_RANGE              31:31

#define APBDEV_PMC_SCRATCH42_0_EMC_WDV_0_WDV_RANGE                           3: 0
#define APBDEV_PMC_SCRATCH42_0_EMC_QSAFE_0_QSAFE_RANGE                       7: 4
#define APBDEV_PMC_SCRATCH42_0_EMC_WEXT_0_WEXT_RANGE                        11: 8
#define APBDEV_PMC_SCRATCH42_0_MEMORY_TYPE_RANGE                            14:12
#define APBDEV_PMC_SCRATCH42_0_CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_DIVISOR_RANGE\
                                                                            22:15
#define APBDEV_PMC_SCRATCH42_0_EMC_FBIO_CFG5_0_EMC2PMACRO_CFG_QUSE_MODE_RANGE\
                                                                            25:23
#define APBDEV_PMC_SCRATCH42_0_EMC_FBIO_CFG5_0_CMD_2T_TIMING_RANGE          26:26
#define APBDEV_PMC_SCRATCH42_0_CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_USE_PLLM_UD_RANGE\
                                                                            27:27
#define APBDEV_PMC_SCRATCH42_0_EMC_QUSE_EXTRA_0_QUSE_EXTRA_RANGE            31:28

#define APBDEV_PMC_SCRATCH44_0_EMC_XM2CMDPADCTRL_0_EMC2TMC_CFG_XM2CMD_CLK_SEL_RANGE\
                                                                             0: 0
#define APBDEV_PMC_SCRATCH44_0_EMC_XM2CMDPADCTRL_0_EMC2PMACRO_CFG_XM2CMD_PHASESHIFT_RANGE\
                                                                             1: 1
#define APBDEV_PMC_SCRATCH44_0_EMC_AR2PDEN_0_AR2PDEN_RANGE                  10: 2
#define APBDEV_PMC_SCRATCH44_0_EMC_FBIO_SPARE_0_CFG_FBIO_SPARE_2_RANGE      18:11
#define APBDEV_PMC_SCRATCH44_0_EMC_CFG_RSV_0_CFG_RESERVED_BYTE1_RANGE       26:19
#define APBDEV_PMC_SCRATCH44_0_EMC_CTT_TERM_CTRL_0_TERM_OFFSET_RANGE        31:27

#define APBDEV_PMC_SCRATCH45_0_EMC_PCHG2PDEN_0_PCHG2PDEN_RANGE               5: 0
#define APBDEV_PMC_SCRATCH45_0_EMC_ACT2PDEN_0_ACT2PDEN_RANGE                11: 6
#define APBDEV_PMC_SCRATCH45_0_EMC_RW2PDEN_0_RW2PDEN_RANGE                  17:12
#define APBDEV_PMC_SCRATCH45_0_EMC_XM2CMDPADCTRL2_0_EMC2PMACRO_CFG_XM2CMD_DRVDN_RANGE\
                                                                            22:18
#define APBDEV_PMC_SCRATCH45_0_EMC_XM2CMDPADCTRL2_0_EMC2PMACRO_CFG_XM2CMD_DRVUP_RANGE\
                                                                            27:23
#define APBDEV_PMC_SCRATCH45_0_EMC_XM2CMDPADCTRL2_0_EMC2PMACRO_CFG_XM2CMD_DRVDN_SLWR_RANGE\
                                                                            31:28

#define APBDEV_PMC_SCRATCH46_0_EMC_XM2DQSPADCTRL_0_EMC2PMACRO_CFG_XM2DQS_DRVDN_TERM_RANGE\
                                                                             4: 0
#define APBDEV_PMC_SCRATCH46_0_EMC_XM2DQSPADCTRL_0_EMC2PMACRO_CFG_XM2DQS_DRVUP_TERM_RANGE\
                                                                             9: 5
#define APBDEV_PMC_SCRATCH46_0_EMC_XM2DQSPADCTRL_0_EMC2PMACRO_CFG_XM2DQS_DRVDN_RANGE\
                                                                            14:10
#define APBDEV_PMC_SCRATCH46_0_EMC_XM2DQSPADCTRL_0_EMC2PMACRO_CFG_XM2DQS_DRVUP_RANGE\
                                                                            19:15
#define APBDEV_PMC_SCRATCH46_0_EMC_XM2DQPADCTRL_0_EMC2PMACRO_CFG_XM2DQ_DRVDN_RANGE\
                                                                            24:20
#define APBDEV_PMC_SCRATCH46_0_EMC_XM2DQPADCTRL_0_EMC2PMACRO_CFG_XM2DQ_DRVUP_RANGE\
                                                                            29:25
#define APBDEV_PMC_SCRATCH46_0_EMC_CFG_2_0_MRR_BYTESEL_RANGE                31:30

#define APBDEV_PMC_SCRATCH47_0_EMC_XM2CMDPADCTRL2_0_EMC2PMACRO_CFG_XM2CMD_DRVUP_SLWF_RANGE\
                                                                             3: 0
#define APBDEV_PMC_SCRATCH47_0_EMC_XM2DQSPADCTRL_0_EMC2PMACRO_CFG_XM2DQS_DRVDN_SLWR_RANGE\
                                                                             7: 4
#define APBDEV_PMC_SCRATCH47_0_EMC_XM2DQSPADCTRL_0_EMC2PMACRO_CFG_XM2DQS_DRVUP_SLWF_RANGE\
                                                                            11: 8
#define APBDEV_PMC_SCRATCH47_0_EMC_XM2DQPADCTRL_0_EMC2PMACRO_CFG_XM2DQ_DRVDN_SLWR_RANGE\
                                                                            15:12
#define APBDEV_PMC_SCRATCH47_0_EMC_XM2DQPADCTRL_0_EMC2PMACRO_CFG_XM2DQ_DRVUP_SLWF_RANGE\
                                                                            19:16
#define APBDEV_PMC_SCRATCH47_0_EMC_XM2CLKPADCTRL_0_EMC2PMACRO_CFG_XM2CLK_DRVDN_SLWR_RANGE\
                                                                            23:20
#define APBDEV_PMC_SCRATCH47_0_EMC_XM2CLKPADCTRL_0_EMC2PMACRO_CFG_XM2CLK_DRVUP_SLWF_RANGE\
                                                                            27:24
#define APBDEV_PMC_SCRATCH47_0_EMC_CTT_TERM_CTRL_0_TERM_SLOPE_RANGE         30:28
#define APBDEV_PMC_SCRATCH47_0_EMC_CFG_0_MAN_PRE_RD_RANGE                   31:31

#define APBDEV_PMC_SCRATCH48_0_EMC_XM2DQPADCTRL2_0_EMC2TMC_CFG_XM2DQ0_DLYIN_TRM_RANGE\
                                                                             2: 0
#define APBDEV_PMC_SCRATCH48_0_EMC_XM2DQPADCTRL2_0_EMC2TMC_CFG_XM2DQ1_DLYIN_TRM_RANGE\
                                                                             5: 3
#define APBDEV_PMC_SCRATCH48_0_EMC_XM2DQPADCTRL2_0_EMC2TMC_CFG_XM2DQ2_DLYIN_TRM_RANGE\
                                                                             8: 6
#define APBDEV_PMC_SCRATCH48_0_EMC_XM2DQPADCTRL2_0_EMC2TMC_CFG_XM2DQ3_DLYIN_TRM_RANGE\
                                                                            11: 9
#define APBDEV_PMC_SCRATCH48_0_EMC_XM2CLKPADCTRL_0_EMC2TMC_CFG_XM2CLK_DLY_TRM_P_RANGE\
                                                                            14:12
#define APBDEV_PMC_SCRATCH48_0_EMC_CFG_2_0_MRR_BYTESEL_X16_RANGE            16:15
#define APBDEV_PMC_SCRATCH48_0_EMC_CFG_0_MAN_PRE_WR_RANGE                   17:17

// macros for variables with different names but sharing the same registers
#define EMC_MRW_LPDDR2_ZCAL_WARM_BOOT_0_MRW_MA_RANGE \
        EMC_MRW_0_MRW_MA_RANGE
#define EMC_MRW_LPDDR2_ZCAL_WARM_BOOT_0_MRW_OP_RANGE \
        EMC_MRW_0_MRW_OP_RANGE
#define EMC_WARM_BOOT_EMR3_0_USE_EMRS_LONG_CNT_RANGE \
        EMC_EMRS_0_USE_EMRS_LONG_CNT_RANGE
#define EMC_WARM_BOOT_EMR3_0_EMRS_BA_RANGE \
        EMC_EMRS_0_EMRS_BA_RANGE
#define EMC_WARM_BOOT_EMR3_0_EMRS_ADR_RANGE \
        EMC_EMRS_0_EMRS_ADR_RANGE
#define EMC_WARM_BOOT_EMR2_0_USE_EMRS_LONG_CNT_RANGE \
        EMC_EMRS_0_USE_EMRS_LONG_CNT_RANGE
#define EMC_WARM_BOOT_EMR2_0_EMRS_BA_RANGE \
        EMC_EMRS_0_EMRS_BA_RANGE
#define EMC_WARM_BOOT_EMR2_0_EMRS_ADR_RANGE \
        EMC_EMRS_0_EMRS_ADR_RANGE
#define EMC_WARM_BOOT_MRS_EXTRA_0_USE_MRS_LONG_CNT_RANGE \
        EMC_MRS_0_USE_MRS_LONG_CNT_RANGE
#define EMC_WARM_BOOT_MRS_EXTRA_0_MRS_BA_RANGE \
        EMC_MRS_0_MRS_BA_RANGE
#define EMC_WARM_BOOT_MRS_EXTRA_0_MRS_ADR_RANGE \
        EMC_MRS_0_MRS_ADR_RANGE
#define EMC_WARM_BOOT_MRW1_0_MRW_MA_RANGE \
        EMC_MRW_0_MRW_MA_RANGE
#define EMC_WARM_BOOT_MRW1_0_MRW_OP_RANGE \
        EMC_MRW_0_MRW_OP_RANGE
#define EMC_WARM_BOOT_MRW3_0_MRW_MA_RANGE \
        EMC_MRW_0_MRW_MA_RANGE
#define EMC_WARM_BOOT_MRW3_0_MRW_OP_RANGE \
        EMC_MRW_0_MRW_OP_RANGE
#define EMC_WARM_BOOT_MRW2_0_MRW_MA_RANGE \
        EMC_MRW_0_MRW_MA_RANGE
#define EMC_WARM_BOOT_MRW2_0_MRW_OP_RANGE \
        EMC_MRW_0_MRW_OP_RANGE
#define EMC_WARM_BOOT_MRS_0_USE_MRS_LONG_CNT_RANGE \
        EMC_MRS_0_USE_MRS_LONG_CNT_RANGE
#define EMC_WARM_BOOT_MRS_0_MRS_BA_RANGE \
        EMC_MRS_0_MRS_BA_RANGE
#define EMC_WARM_BOOT_MRS_0_MRS_ADR_RANGE \
        EMC_MRS_0_MRS_ADR_RANGE
#define EMC_WARM_BOOT_EMRS_0_USE_EMRS_LONG_CNT_RANGE \
        EMC_EMRS_0_USE_EMRS_LONG_CNT_RANGE
#define EMC_WARM_BOOT_EMRS_0_EMRS_BA_RANGE \
        EMC_EMRS_0_EMRS_BA_RANGE
#define EMC_WARM_BOOT_EMRS_0_EMRS_ADR_RANGE \
        EMC_EMRS_0_EMRS_ADR_RANGE
#define EMC_WARM_BOOT_MRW_EXTRA_0_MRW_MA_RANGE \
        EMC_MRW_0_MRW_MA_RANGE
#define EMC_WARM_BOOT_MRW_EXTRA_0_MRW_OP_RANGE \
        EMC_MRW_0_MRW_OP_RANGE
#define EMC_ZQ_CAL_DDR3_WARM_BOOT_0_ZQ_CAL_CMD_RANGE \
        EMC_ZQ_CAL_0_ZQ_CAL_CMD_RANGE
#define EMC_ZQ_CAL_DDR3_WARM_BOOT_0_ZQ_CAL_LENGTH_RANGE \
        EMC_ZQ_CAL_0_ZQ_CAL_LENGTH_RANGE

// End of generated code by warmboot_code_gen

/**
 * Fuse aliasing values.
 * These map to the decoded/parsed fuse fields used by the Boot ROM and others.
 * When aliasing fuses, the values parsed from the real fuses are overwritten
 * by these values.  Subsequent reads from these fuse registers read back
 * these values, not the orignal fuse values, until the fuses are re-sensed.
 *
 * APBDEV_PMC_SCRATCHn_0_FUSE_ALIAS_0_field_RANGE maps onto the FUSE_field_0
 * register.
 *
 * The fuse fields have the same meanings as with AP15/AP16.  There are a
 * few new ones or small differences (like the sizes of the fields).
 * The new ones do not affect the Boot ROM per se.  For example, OBS_DIS 
 * disables the observation bus, something that is only relevant for testing.
 *
 * PACKAGE_INFO: This identifies if the package is POP or non-POP.  At
 * one point, this was thought to be needed to select the correct pinmuxing
 * in the Boot ROM.  Towards the end of the project, there were enough cases
 * where the pinmuxing had to be selected for other reasons that use of the
 * bit was dropped as the selection critereon.
 *
 * SKU_INFO: There are only two bits used to control HW in the Boot ROM -
 * the rest are simply values for reference.
 */

// Save PMC scratch register space by using one word of input from which
// the UID, SBK, and DK can be derived.  Note that this is only for internal
// testing purposes and therefore does not pose a security risk.
// TODO: Complete the support for this
#define APBDEV_PMC_SCRATCH34_0_FUSE_ALIAS_0_ID_RANGE                      31: 0

#define APBDEV_PMC_SCRATCH35_0_FUSE_ALIAS_0_BOOT_DEVICE_INFO_RANGE        15: 0
#define APBDEV_PMC_SCRATCH35_0_FUSE_ALIAS_0_SKU_INFO_RANGE                23:16
#define APBDEV_PMC_SCRATCH35_0_FUSE_ALIAS_0_RESERVED_SW_RANGE             31:24

#define APBDEV_PMC_SCRATCH36_0_FUSE_ALIAS_0_SECURITY_MODE_RANGE            0: 0
#define APBDEV_PMC_SCRATCH36_0_FUSE_ALIAS_0_PRODUCTION_MODE_RANGE          1: 1
#define APBDEV_PMC_SCRATCH36_0_FUSE_ALIAS_0_FA_RANGE                       2: 2
#define APBDEV_PMC_SCRATCH36_0_FUSE_ALIAS_0_ARM_DEBUG_DIS_RANGE            3: 3
#define APBDEV_PMC_SCRATCH36_0_FUSE_ALIAS_0_PACKAGE_INFO_RANGE             5: 4
// BR Strap alias bits.
#define APBDEV_PMC_SCRATCH36_0_STRAP_ALIAS_0_BOOT_SRC_NOR_BOOT_RANGE       6: 6
#define APBDEV_PMC_SCRATCH36_0_STRAP_ALIAS_0_RAM_CODE_RANGE               10: 7
#define APBDEV_PMC_SCRATCH36_0_STRAP_ALIAS_0_BOOT_SRC_USB_RECOVERY_MODE_RANGE\
                                                                          11:11
#define APBDEV_PMC_SCRATCH36_0_STRAP_ALIAS_0_BOOT_SELECT_RANGE            15:12
// Bits 31:16 available

// Storage for the location of the encrypted SE context 
#define APBDEV_PMC_SCRATCH43_0_SE_ENCRYPTED_CONTEXT_RANGE                 31: 0

// Storage for the SSK
/** 
 * Keep secure scratch for the SSK reserved as SCRATCH0-3 for now. The bootrom
 * has the ability to fallback to the AP20 method of saving the secure scratch
 * register(which uses SCRATCH0-3). If the T30 method for SSK 
 * save/restoration is used, these secure scratch registers can be repurposed. 
 */
#define APBDEV_PMC_SECURE_SCRATCH0_0_SSK_0_SSK0_RANGE                     31: 0
#define APBDEV_PMC_SECURE_SCRATCH1_0_SSK_0_SSK1_RANGE                     31: 0
#define APBDEV_PMC_SECURE_SCRATCH2_0_SSK_0_SSK2_RANGE                     31: 0
#define APBDEV_PMC_SECURE_SCRATCH3_0_SSK_0_SSK3_RANGE                     31: 0

// Storage for the SRK
/**
 * The SE will save the SRK key to SCRATCH4-7 when a CTX_SAVE operation with
 * destination SRK is started. 
 */
#define APBDEV_PMC_SECURE_SCRATCH4_0_SRK_0_SRK0_RANGE                     31: 0
#define APBDEV_PMC_SECURE_SCRATCH5_0_SRK_0_SRK1_RANGE                     31: 0
#define APBDEV_PMC_SECURE_SCRATCH6_0_SRK_0_SRK2_RANGE                     31: 0
#define APBDEV_PMC_SECURE_SCRATCH7_0_SRK_0_SRK3_RANGE                     31: 0

#endif // INCLUDED_NVBOOT_APBDEV_PMC_SCRATCH_MAP_H
