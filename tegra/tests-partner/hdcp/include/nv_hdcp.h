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
#ifndef _LINUX_NVHDCP_H_
#define _LINUX_NVHDCP_H_

#include <linux/fb.h>
#include <linux/types.h>
#include <asm/ioctl.h>

#define TEGRA_NVHDCP_MAX_DEVS 127

#define TEGRA_NVHDCP_FLAG_AN 0x0001
#define TEGRA_NVHDCP_FLAG_AKSV 0x0002
#define TEGRA_NVHDCP_FLAG_BKSV 0x0004
#define TEGRA_NVHDCP_FLAG_BSTATUS 0x0008
#define TEGRA_NVHDCP_FLAG_CN 0x0010
#define TEGRA_NVHDCP_FLAG_CKSV 0x0020
#define TEGRA_NVHDCP_FLAG_DKSV 0x0040
#define TEGRA_NVHDCP_FLAG_KP 0x0080
#define TEGRA_NVHDCP_FLAG_S 0x0100
#define TEGRA_NVHDCP_FLAG_CS 0x0200
#define TEGRA_NVHDCP_FLAG_V 0x0400
#define TEGRA_NVHDCP_FLAG_MP 0x0800
#define TEGRA_NVHDCP_FLAG_BKSVLIST 0x1000

#define TEGRA_NVHDCP_RESULT_SUCCESS 0
#define TEGRA_NVHDCP_RESULT_UNSUCCESSFUL 1
#define TEGRA_NVHDCP_RESULT_PENDING 0x103
#define TEGRA_NVHDCP_RESULT_LINK_FAILED 0xc0000013

#define TEGRA_NVHDCP_RESULT_INVALID_PARAMETER 0xc000000d
#define TEGRA_NVHDCP_RESULT_INVALID_PARAMETER_MIX 0xc0000030

#define TEGRA_NVHDCP_RESULT_NO_MEMORY 0xc0000017

struct tegra_nvhdcp_packet {
 __u32 value_flags;
 __u32 packet_results;

 __u64 c_n;
 __u64 c_ksv;

 __u32 b_status;
 __u64 hdcp_status;
 __u64 cs;

 __u64 k_prime;
 __u64 a_n;
 __u64 a_ksv;
 __u64 b_ksv;
 __u64 d_ksv;
 __u8 v_prime[20];
 __u64 m_prime;

 __u32 num_bksv_list;

 __u64 bksv_list[TEGRA_NVHDCP_MAX_DEVS];
};

#define TEGRA_NVHDCP_POLICY_ON_DEMAND 0
#define TEGRA_NVHDCP_POLICY_ALWAYS_ON 1

#define TEGRAIO_NVHDCP_ON _IO('F', 0x70)
#define TEGRAIO_NVHDCP_OFF _IO('F', 0x71)
#define TEGRAIO_NVHDCP_SET_POLICY _IOW('F', 0x72, __u32)
#define TEGRAIO_NVHDCP_READ_M _IOWR('F', 0x73, struct tegra_nvhdcp_packet)
#define TEGRAIO_NVHDCP_READ_S _IOWR('F', 0x74, struct tegra_nvhdcp_packet)
#define TEGRAIO_NVHDCP_RENEGOTIATE _IO('F', 0x75)
#define TEGRAIO_NVHDCP_HDCP_STATE   _IOR('F', 0x76, struct tegra_nvhdcp_packet)

#endif
