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
#ifndef __OV9726_H__
#define __OV9726_H__
#include <linux/ioctl.h>
#define OV9726_I2C_ADDR 0x20
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define OV9726_IOCTL_SET_MODE _IOW('o', 1, struct ov9726_mode)
#define OV9726_IOCTL_SET_FRAME_LENGTH _IOW('o', 2, __u32)
#define OV9726_IOCTL_SET_COARSE_TIME _IOW('o', 3, __u32)
#define OV9726_IOCTL_SET_GAIN _IOW('o', 4, __u16)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define OV9726_IOCTL_GET_STATUS _IOR('o', 5, __u8)
struct ov9726_mode {
 int mode_id;
 int xres;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int yres;
 __u32 frame_length;
 __u32 coarse_time;
 __u16 gain;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct ov9726_reg {
 __u16 addr;
 __u16 val;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define OV9726_TABLE_WAIT_MS 0
#define OV9726_TABLE_END 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif

