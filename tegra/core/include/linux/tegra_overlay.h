/* TODO: this should live in bionic/libc/kernel/common/ */
/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef __LINUX_TEGRA_OVERLAY_H
#define __LINUX_TEGRA_OVERLAY_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/tegrafb.h>

struct tegra_overlay_windowattr {
 __s32 index;
 __u32 buff_id;
 __u32 blend;
 __u32 offset;
 __u32 offset_u;
 __u32 offset_v;
 __u32 stride;
 __u32 stride_uv;
 __u32 pixformat;
 __u32 x;
 __u32 y;
 __u32 w;
 __u32 h;
 __u32 out_x;
 __u32 out_y;
 __u32 out_w;
 __u32 out_h;
 __u32 z;
 __u32 pre_syncpt_id;
 __u32 pre_syncpt_val;
 __u32 hfilter;
 __u32 vfilter;
 __u32 do_not_use__tiled; /* compatibility */
 __u32 flags;
};

#define TEGRA_OVERLAY_FLIP_FLAG_BLEND_REORDER (1 << 0)

struct tegra_overlay_flip_args {
 struct tegra_overlay_windowattr win[TEGRA_FB_FLIP_N_WINDOWS];
 __u32 post_syncpt_id;
 __u32 post_syncpt_val;
 __u32 flags;
};

#define TEGRA_OVERLAY_IOCTL_MAGIC 'O'

#define TEGRA_OVERLAY_IOCTL_OPEN_WINDOW _IOWR(TEGRA_OVERLAY_IOCTL_MAGIC, 0x40, __u32)
#define TEGRA_OVERLAY_IOCTL_CLOSE_WINDOW _IOW(TEGRA_OVERLAY_IOCTL_MAGIC, 0x41, __u32)
#define TEGRA_OVERLAY_IOCTL_FLIP _IOW(TEGRA_OVERLAY_IOCTL_MAGIC, 0x42, struct tegra_overlay_flip_args)
#define TEGRA_OVERLAY_IOCTL_SET_NVMAP_FD _IOW(TEGRA_OVERLAY_IOCTL_MAGIC, 0x43, __u32)

#define TEGRA_OVERLAY_IOCTL_MIN_NR _IOC_NR(TEGRA_OVERLAY_IOCTL_OPEN_WINDOW)
#define TEGRA_OVERLAY_IOCTL_MAX_NR _IOC_NR(TEGRA_OVERLAY_IOCTL_SET_NVMAP_FD)

#endif
