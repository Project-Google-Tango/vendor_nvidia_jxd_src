/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef __TEGRA_DTV_H__
#define __TEGRA_DTV_H__
#include <linux/ioctl.h>
#define TEGRA_DTV_MAGIC 'v'
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define TEGRA_DTV_IOCTL_START _IO(TEGRA_DTV_MAGIC, 0)
#define TEGRA_DTV_IOCTL_STOP _IO(TEGRA_DTV_MAGIC, 1)
struct tegra_dtv_hw_config {
 int clk_edge;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int byte_swz_enabled;
 int bit_swz_enabled;
 int protocol_sel;
 int clk_mode;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int fec_size;
 int body_size;
 int body_valid_sel;
 int start_sel;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int err_pol;
 int psync_pol;
 int valid_pol;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct tegra_dtv_profile {
 unsigned int bufsize;
 unsigned int bufnum;
 unsigned int cpuboost;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 unsigned int bitrate;
};
#define TEGRA_DTV_IOCTL_SET_HW_CONFIG _IOW(TEGRA_DTV_MAGIC, 2,   const struct tegra_dtv_hw_config *)
#define TEGRA_DTV_IOCTL_GET_HW_CONFIG _IOR(TEGRA_DTV_MAGIC, 3,   struct tegra_dtv_hw_config *)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define TEGRA_DTV_IOCTL_GET_PROFILE _IOR(TEGRA_DTV_MAGIC, 4,   struct tegra_dtv_profile *)
#define TEGRA_DTV_IOCTL_SET_PROFILE _IOW(TEGRA_DTV_MAGIC, 5,   const struct tegra_dtv_profile *)
enum {
 TEGRA_DTV_CLK_RISE_EDGE = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 TEGRA_DTV_CLK_FALL_EDGE,
};
enum {
 TEGRA_DTV_SWZ_DISABLE = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 TEGRA_DTV_SWZ_ENABLE,
};
enum {
 TEGRA_DTV_PROTOCOL_NONE = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 TEGRA_DTV_PROTOCOL_ERROR,
 TEGRA_DTV_PROTOCOL_PSYNC,
};
enum {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 TEGRA_DTV_CLK_DISCONTINUOUS = 0,
 TEGRA_DTV_CLK_CONTINUOUS,
};
enum {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 TEGRA_DTV_BODY_VALID_IGNORE = 0,
 TEGRA_DTV_BODY_VALID_GATE,
};
enum {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 TEGRA_DTV_START_RESERVED = 0,
 TEGRA_DTV_START_PSYNC,
 TEGRA_DTV_START_VALID,
 TEGRA_DTV_START_BOTH,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum {
 TEGRA_DTV_ERROR_POLARITY_HIGH = 0,
 TEGRA_DTV_ERROR_POLARITY_LOW,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum {
 TEGRA_DTV_PSYNC_POLARITY_HIGH = 0,
 TEGRA_DTV_PSYNC_POLARITY_LOW,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
enum {
 TEGRA_DTV_VALID_POLARITY_HIGH = 0,
 TEGRA_DTV_VALID_POLARITY_LOW,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#endif
