/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbl_assembly.h"

/* all base address */
#define PMC_PA_BASE                    0x7000E400
#define CLK_RST_PA_BASE                0x60006000
#define FLOW_PA_BASE                   0x60007000
#define TIMERUS_PA_BASE                0x60005010

#ifdef TEGRA_14x_SOC
#define EMC_PA_BASE                    0x7001B000
#endif
#ifdef TEGRA_12x_SOC
#define EMC_PA_BASE                    0x7001B000
#endif
#ifdef TEGRA_11x_SOC
#define EMC_PA_BASE                    0x7001B000
#endif
#ifdef TEGRA_3x_SOC
#define EMC_PA_BASE                    0x7000F400
#endif

#define EMC_CFG                        0xc
#define EMC_ADR_CFG                    0x10
#define EMC_REQ_CTRL                   0x2b0
#define EMC_TIMING_CONTROL             0x28
#define EMC_EMC_STATUS                 0x2b4
#define EMC_SELF_REF                   0xe0
#define EMC_ZCAL_INTERVAL              0x2e0
#define EMC_AUTO_CAL_INTERVAL          0x2a8
#define EMC_AUTO_CAL_STATUS            0x2ac
#define EMC_XM2VTTGENPADCTRL           0x310
#define EMC_XM2VTTGENPADCTRL2          0x314

#define PMC_CTRL                       0x0
#define PMC_IO_DPD_REQ                 0x1b8
#define PMC_IO_DPD_STATUS              0x1bc

#define CLK_RESET_CCLK_BURST           0x20
#define CLK_RESET_CCLK_DIVIDER         0x24
#define CLK_RESET_SCLK_BURST           0x28
#define CLK_RESET_SCLK_DIVIDER         0x2c

#define CLK_RESET_PLLC_BASE            0x80
#define CLK_RESET_PLLP_BASE            0xa0
#define CLK_RESET_PLLA_BASE            0xb0
#define CLK_RESET_PLLX_BASE            0xe0

#define CLK_RESET_CLK_SOURCE_MSELECT   0x3b4

#define PMC_PLLP_WB0_OVERRIDE          0xf8
#define PMC_PLLM_WB0_OVERRIDE          0x1dc

#define FLOW_CTLR_HALT_CPU_IRQ         (1 << 10)
#define FLOW_CTLR_HALT_CPU_FIQ         (1 << 8)

#define TEGRA_IRAM_BASE                0x40000000
#define TEGRA_IRAM_CODE_AREA           (TEGRA_IRAM_BASE + 4096)

/* PMC_SCRATCH2 is used for PLLM boot state if PLLM auto-restart is enabled */
#define PMC_SCRATCH2                   0x58
#define PMC_SCRATCH37                  0x130
#define PMC_SCRATCH38                  0x134

#define FLOW_CTLR_CSR_INTR_FLAG        (1 << 15)
#define FLOW_CTLR_CSR_EVENT_FLAG       (1 << 14)
#define FLOW_CTLR_HALT_LIC_IRQ         (1 << 11)
#define FLOW_CTLR_HALT_LIC_FIQ         (1 << 10)
#define FLOW_CTLR_CSR_ENABLE           (1 << 0)
#define FLOW_CTLR_IMMEDIATE_WAKE       (1 << 3)
#define FLOW_CTLR_WAITEVENT            (2 << 29)
#define FLOW_CTLR_WAIT_FOR_INTERRUPT   (4 << 29)

#define TEGRA_POWER_SIDE_EFFECT_LP0    (1 << 14)  /* enter LP0 when CPU pwr gated */
#define MSELECT_CLKM                   (0x3 << 30)
#define TEGRA_POWER_LP1_AUDIO          (1 << 25)

/*
 * CR1 bits (CP#15 CR1)
 */
#define CR_M                           (1 << 0) // MMU enable
#define CR_C                           (1 << 2) // Dcache enable
#define CR_Z                           (1 << 11) // Implementation defined
#define CR_I                           (1 << 12) // Icache enable

    .section .text
    .align 2

.macro emc_device_mask, rd, base
    ldr    \rd, [\base, #EMC_ADR_CFG]
    tst     \rd, #0x1
    moveq    \rd, #(0x1<<8)        @ just 1 device
    movne    \rd, #(0x3<<8)        @ 2 devices
.endm

.macro emc_timing_update, rd, base
    mov    \rd, #1
    str    \rd, [\base, #EMC_TIMING_CONTROL]
1001:
    ldr    \rd, [\base, #EMC_EMC_STATUS]
    tst    \rd, #(0x1<<23)        @ wait until EMC_STATUS_TIMING_UPDATE_STALLED is clear
    bne    1001b
.endm

/* waits until the microsecond counter (base) ticks, for exact timing loops */
.macro wait_for_us, rd, base, tmp
    ldr    \rd, [\base]
1001: ldr    \tmp, [\base]
    cmp    \rd, \tmp
    beq    1001b
    mov    \tmp, \rd
.endm

/* waits until the microsecond counter (base) is > rn */
.macro wait_until, rn, base, tmp
    add \rn, \rn, #1
1002: ldr \tmp, [\base]
    sub \tmp, \tmp, \rn
    ands    \tmp, \tmp, #0x80000000
    dmb
    bne 1002b
.endm

/* returns the offset of the flow controller halt register for a cpu */
.macro cpu_to_halt_reg rd, rcpu
    cmp    \rcpu, #0
    subne    \rd, \rcpu, #1
    movne    \rd, \rd, lsl #3
    addne    \rd, \rd, #0x14
    moveq    \rd, #0
.endm

/* returns the offset of the flow controller csr register for a cpu */
.macro cpu_to_csr_reg rd, rcpu
    cmp    \rcpu, #0
    subne    \rd, \rcpu, #1
    movne    \rd, \rd, lsl #3
    addne    \rd, \rd, #0x18
    moveq    \rd, #8
.endm

/* returns the ID of the current processor */
.macro cpu_id, rd
    mrc p15, 0, \rd, c0, c0, 5
    and \rd, \rd, #0xF
.endm

/* loads a 32-bit value into a register without a data access */
.macro mov32, reg, val
    movw    \reg, #:lower16:\val
    movt    \reg, #:upper16:\val
.endm

/*
 *    NvBlLp0CoreResume
 *
 *      CPU boot vector when restarting the a CPU following
 *      an LP2 transition. Also branched to by LP0 and LP1 resume after
 *      re-enabling sdram.
 */
    .globl  NvBlLp0CoreResume
NvBlLp0CoreResume :
    msr cpsr_fsxc, #0xdf
    mov32   r13, main_stack

    // Blow away any stale state that may exist in the processor
    cpsid aif, #0x13
    msr spsr_fsxc, #0
    cpsid aif, #0x11
    msr spsr_fsxc, #0
    cpsid aif, #0x12
    msr spsr_fsxc, #0
    cpsid aif, #0x17
    msr spsr_fsxc, #0
    cpsid aif, #0x1B
    msr spsr_fsxc, #0
    cpsid aif, #0x1f

    // Set up system control register
    mrc  p15, 0, r4, c1, c0, 0
    bic  r4, r4, #(1<<12)
    bic  r4, r4, #(1<<11)
    bic  r4, r4, #(1<<2)
    bic  r4, r4, #(1<<0)
    mcr  p15, 0, r4, c1, c0, 0

    bl      nvaosConfigureCache

    /* these two instructions are needed to make vldr instructions work */
    mvn r0, #0
    mcr p15, 0, r0, c1, c0, 2

    .word  0xF57FF06F        //DCD     0xF57FF06F                      // !!!DELETEME!!! BUG 830289
/* set the stack pointer */

    bl  initFpu

    mrc  p15, 0, r1, c1, c0, 0 /* Read SCTLR */
    orr  r1, r1, #(1<<11)      /* Enable branch predictor */
    orr  r1, r1, #(1<<1)      /* Turn on strict alignment checking */
    mcr  p15, 0, r1, c1, c0, 0 /* Write SCTLR */
    bl       NvBlLp0StartResume
    .type NvBlLp0CoreResume, %function
    .size NvBlLp0CoreResume, . - NvBlLp0CoreResume

/*
 * tegra_turn_off_mmu
 *
 * r0 = v2p
 * r1 = physical address to jump to with mmu off
 */
tegra_turn_off_mmu:
    /*
     * change page table pointer to tegra_pgd_phys, so that IRAM
     * and MMU shut-off will be mapped virtual == physical
     */
    mov    r2, #0
    mcr    p15, 0, r2, c8, c3, 0    @ invalidate TLB
    mcr    p15, 0, r2, c7, c5, 6    @ flush BTAC
    mcr    p15, 0, r2, c7, c5, 0    @ flush instruction cache

    mov32    r3, tegra_shut_off_mmu
    //Hangs here if you remove the below comment
    //add    r3, r3, r0

    mov    r0, r1
    mov    pc, r3
    .type tegra_turn_off_mmu, %function
    .size tegra_turn_off_mmu, . - tegra_turn_off_mmu

/*
 * tegra_shut_off_mmu
 *
 * r0 = physical address to jump to with mmu off
 *
 * called with VA=PA mapping
 * turns off MMU, icache, dcache and branch prediction
 */
    .align    L1_CACHE_SHIFT
tegra_shut_off_mmu:
    mrc p15, 0, r3, c1, c0, 0
    movw   r2, #CR_I | CR_Z | CR_C | CR_M
    bic    r3, r3, r2
    dsb
    mcr    p15, 0, r3, c1, c0, 0
    isb
    mov    pc, r0


/*
 * NvBlLp0CoreSuspend(unsigned long int)
 *
 * enters suspend in LP0 or LP1 by turning off the mmu and jumping to
 * g_NvBlLp0TearDownCore in IRAM
 */
    .globl  NvBlLp0CoreSuspend
NvBlLp0CoreSuspend :
    bl nvaosFlushCache
    bl NvOsDataCacheWritebackInvalidate
    /* preload all the address literals that are needed for the
     * CPU power-gating process, to avoid loads from SDRAM (which are
     * not supported once SDRAM is put into self-refresh.
     * LP0 / LP1 use physical address, since the MMU needs to be
     * disabled before putting SDRAM into self-refresh to avoid
     * memory access due to page table walks */
    mov32    r4, PMC_PA_BASE
    mov32    r5, CLK_RST_PA_BASE
    mov32    r6, FLOW_PA_BASE
    mov32    r7, TIMERUS_PA_BASE

    mov32    r1, g_NvBlLp0TearDownCore
    mov32    r2, g_NvBlLp0IramStart
    sub    r1, r1, r2
    mov32    r2, TEGRA_IRAM_CODE_AREA
    add    r1, r1, r2
    b    tegra_turn_off_mmu
    .type NvBlLp0CoreSuspend, %function
    .size NvBlLp0CoreSuspend, . - NvBlLp0CoreSuspend

/* START OF ROUTINES COPIED TO IRAM */
    .align L1_CACHE_SHIFT
    .globl g_NvBlLp0IramStart
g_NvBlLp0IramStart:

    .align    L1_CACHE_SHIFT
    .type    tegra3_sdram_pad_save, %object
tegra3_sdram_pad_save:
    .word    0
    .word    0
    .word    0
    .word    0
    .word    0
    .word    0
    .word    0
    .word    0

tegra3_sdram_pad_address:
    .word    EMC_PA_BASE + EMC_CFG                @0x0
    .word    EMC_PA_BASE + EMC_ZCAL_INTERVAL            @0x4
    .word    EMC_PA_BASE + EMC_AUTO_CAL_INTERVAL            @0x8
    .word    EMC_PA_BASE + EMC_XM2VTTGENPADCTRL            @0xc
    .word    EMC_PA_BASE + EMC_XM2VTTGENPADCTRL2            @0x10
    .word    EMC_PA_BASE + PMC_IO_DPD_STATUS            @0x14
    .word    CLK_RST_PA_BASE + CLK_RESET_CLK_SOURCE_MSELECT    @0x18
    .word    CLK_RST_PA_BASE + CLK_RESET_SCLK_BURST        @0x1c

tegra3_sdram_pad_size:
    .word    tegra3_sdram_pad_address - tegra3_sdram_pad_save

/*
 * g_NvBlLp0TearDownCore
 *
 * copied into and executed from IRAM
 * puts memory in self-refresh for LP0 and LP1
 */
g_NvBlLp0TearDownCore:
    bl    tegra3_sdram_self_refresh
    b    tegra3_enter_sleep

/*
 * tegra3_enter_sleep
 *
 * uses flow controller to enter sleep state
 * executes from IRAM with SDRAM in selfrefresh when target state is LP0 or LP1
 * executes from SDRAM with target state is LP2
 * r4 = TEGRA_PMC_BASE
 * r5 = TEGRA_CLK_RESET_BASE
 * r6 = TEGRA_FLOW_CTLR_BASE
 * r7 = TEGRA_TMRUS_BASE
 */
tegra3_enter_sleep:

    dsb
    cpu_id    r1

    cpu_to_csr_reg    r2, r1
    ldr    r0, [r6, r2]
    /* TODO: why to configure flow ctlr again? Its failing if I try to remove*/
    orr    r0, r0, #FLOW_CTLR_CSR_INTR_FLAG | FLOW_CTLR_CSR_EVENT_FLAG
    orr    r0, r0, #FLOW_CTLR_CSR_ENABLE
    str    r0, [r6, r2]

#if defined(TEGRA_11x_SOC)
    tst    r0, #FLOW_CTLR_IMMEDIATE_WAKE
    movne    r0, #FLOW_CTLR_WAITEVENT
    moveq    r0, #FLOW_CTLR_WAIT_FOR_INTERRUPT
    orr    r0, r0, #FLOW_CTLR_HALT_LIC_IRQ | FLOW_CTLR_HALT_LIC_FIQ
#else
    mov    r0, #FLOW_CTLR_WAIT_FOR_INTERRUPT
    orr    r0, r0, #FLOW_CTLR_HALT_CPU_IRQ | FLOW_CTLR_HALT_CPU_FIQ
#endif
    cpu_to_halt_reg r2, r1
    str    r0, [r6, r2]
    dsb
    ldr    r0, [r6, r2] /* memory barrier */

#ifndef CONFIG_ARM_SAVE_DEBUG_CONTEXT_NO_LOCK
    /* Set the Debug OS Double Lock for Debug Arch v7.1 or greater.
       With this lock set, the debugger is completely locked out.
       Disable this to debug WFI/powergating failures.
    */
    mrc    p15, 0, r3, c0, c1, 2    @ ID_DFR0
    and    r3, r3, #0xF        @ coprocessor debug model
    cmp    r3, #5            @ debug arch >= v7.1?

    mov32    r1, 0xC5ACCE55
    mcrge    p14, 0, r1, c1, c3, 4    @ DBGOSDLR
#endif

halted:
    isb
    dsb
    wfi    /* CPU should be power gated here */

    /* !!!FIXME!!! Implement halt failure handler */
    b    halted

/*
 * tegra3_sdram_self_refresh
 *
 * called with MMU off and caches disabled
 * puts sdram in self refresh
 * must execute from IRAM
 * r4 = TEGRA_PMC_BASE
 * r5 = TEGRA_CLK_RESET_BASE
 * r6 = TEGRA_FLOW_CTLR_BASE
 * r7 = TEGRA_TMRUS_BASE
 */

tegra3_sdram_self_refresh:

    adr    r2, tegra3_sdram_pad_address
    adr    r8, tegra3_sdram_pad_save
    mov    r9, #0

padsave:
    ldr    r0, [r2, r9]            @ r0 is emc register address

    ldr    r1, [r0]
    str    r1, [r8, r9]            @ save emc register

    add    r9, r9, #4
    ldr    r0, tegra3_sdram_pad_size
    cmp    r0, r9
    bne    padsave
padsave_done:

    dsb

    mov32    r0, EMC_PA_BASE            @ r0 reserved for emc base

    mov    r1, #0
    str    r1, [r0, #EMC_ZCAL_INTERVAL]
    str    r1, [r0, #EMC_AUTO_CAL_INTERVAL]
    ldr    r1, [r0, #EMC_CFG]
    bic    r1, r1, #(1<<28)
    str    r1, [r0, #EMC_CFG]        @ disable DYN_SELF_REF

    emc_timing_update r1, r0

    ldr    r1, [r7]
    add    r1, r1, #5
    wait_until r1, r7, r2
/*
emc_wait_audo_cal:
    ldr    r1, [r0, #EMC_AUTO_CAL_STATUS]
    tst    r1, #(0x1<<31)        @ wait until AUTO_CAL_ACTIVE is clear
    bne    emc_wait_audo_cal
*/
    mov    r1, #3
    str    r1, [r0, #EMC_REQ_CTRL]        @ stall incoming DRAM requests

emcidle:
    ldr    r1, [r0, #EMC_EMC_STATUS]
    tst    r1, #4
    beq    emcidle

    mov    r1, #1
    str    r1, [r0, #EMC_SELF_REF]

    emc_device_mask r1, r0

emcself:
    ldr    r2, [r0, #EMC_EMC_STATUS]
    and    r2, r2, r1
    cmp    r2, r1
    bne    emcself                @ loop until DDR in self-refresh

    ldr    r1, [r0, #EMC_XM2VTTGENPADCTRL]
    mov32    r2, 0xF8F8FFFF        @ clear XM2VTTGEN_DRVUP and XM2VTTGEN_DRVDN
    and    r1, r1, r2
    str    r1, [r0, #EMC_XM2VTTGENPADCTRL]
    ldr    r1, [r0, #EMC_XM2VTTGENPADCTRL2]
    orr    r1, r1, #7            @ set E_NO_VTTGEN
    str    r1, [r0, #EMC_XM2VTTGENPADCTRL2]

    emc_timing_update r1, r0

    ldr    r1, [r4, #PMC_CTRL]
    tst    r1, #TEGRA_POWER_SIDE_EFFECT_LP0
    bne    pmc_io_dpd_skip
    mov32    r1, 0x8EC00000
    str    r1, [r4, #PMC_IO_DPD_REQ]
pmc_io_dpd_skip:

    dsb

    mov    pc, lr

    .ltorg
/* dummy symbol for end of IRAM */
    .align L1_CACHE_SHIFT
    .globl g_NvBlLp0IramEnd
g_NvBlLp0IramEnd:
    b    .
