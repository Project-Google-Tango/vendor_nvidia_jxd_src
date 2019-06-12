/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define LP8755_REG_Invalid  0
#define LP8755_REG_BUCK0    0x00
#define LP8755_REG_BUCK1    0x03
#define LP8755_REG_BUCK2    0x04
#define LP8755_REG_BUCK3    0x01
#define LP8755_REG_BUCK4    0x05
#define LP8755_REG_BUCK5    0x02
#define LP8755_REG_MAX      0xFF

#define LP8755_BUCK_EN      1 << 7
#define LP8755_BUCK_LINEAR_OUT_MAX    0x76
#define LP8755_BUCK_VOUT_M  0x7F
enum bucks {
    BUCK0 = 0,
    BUCK1,
    BUCK2,
    BUCK3,
    BUCK4,
    BUCK5,
};

enum lp8755_bucks {
    LP8755_BUCK0 = 1,
    LP8755_BUCK1,
    LP8755_BUCK2,
    LP8755_BUCK3,
    LP8755_BUCK4,
    LP8755_BUCK5,
    LP8755_BUCK_MAX,
};

/**
 * multiphase configuration options
 */
enum lp8755_mphase_config {
    MPHASE_CONF0,
    MPHASE_CONF1,
    MPHASE_CONF2,
    MPHASE_CONF3,
    MPHASE_CONF4,
    MPHASE_CONF5,
    MPHASE_CONF6,
    MPHASE_CONF7,
    MPHASE_CONF8,
    MPHASE_CONF_MAX
};

struct lp8755_mphase {
        int nreg;
        int buck_num[LP8755_BUCK_MAX];
};

static const struct lp8755_mphase mphase_buck[MPHASE_CONF_MAX] = {
    {3, {BUCK0, BUCK3, BUCK5}
     },
    {6, {BUCK0, BUCK1, BUCK2, BUCK3, BUCK4, BUCK5}
     },
    {5, {BUCK0, BUCK2, BUCK3, BUCK4, BUCK5}
     },
    {4, {BUCK0, BUCK3, BUCK4, BUCK5}
     },
    {3, {BUCK0, BUCK4, BUCK5}
     },
    {2, {BUCK0, BUCK5}
     },
    {1, {BUCK0}
     },
    {2, {BUCK0, BUCK3}
     },
    {4, {BUCK0, BUCK2, BUCK3, BUCK5}
     },
};

