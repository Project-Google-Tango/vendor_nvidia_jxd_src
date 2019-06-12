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
#ifndef __TEGRA_THROUGHPUT_IOCTL_H
#define __TEGRA_THROUGHPUT_IOCTL_H
#include <linux/ioctl.h>
#define TEGRA_THROUGHPUT_MAGIC 'g'
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct tegra_throughput_target_fps_args {
 __u32 target_fps;
};
#define TEGRA_THROUGHPUT_IOCTL_TARGET_FPS   _IOW(TEGRA_THROUGHPUT_MAGIC, 1, struct tegra_throughput_target_fps_args)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define TEGRA_THROUGHPUT_IOCTL_MAXNR   (_IOC_NR(TEGRA_THROUGHPUT_IOCTL_TARGET_FPS))
#endif

