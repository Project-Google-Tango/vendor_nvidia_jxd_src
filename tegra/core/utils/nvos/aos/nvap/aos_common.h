/*
 * Copyright (c) 2007-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_AOS_COMMON_H
#define INCLUDED_AOS_COMMON_H

#include "nvrm_hardware_access.h"
#include "nvrm_drf.h"

/* Regster base addresses that are common across all SOCs. */
#define CLK_RST_PA_BASE     0x60006000  // Base address for arclk_rst.h registers
#define CSITE_PA_BASE       0x70040000  // Base address for arcsite.h registers
#define EMC_PA_BASE         0x7000f400  // Base address for aremc.h registers
#define EVP_PA_BASE         0x6000F000  // Base address for arevp.h registers
#define FLOW_PA_BASE        0x60007000  // Base address for arflow_ctlr.h registers
#define FUSE_PA_BASE        0x7000F800  // Base adderss for arfuse.h registers
#define GIC_PA_BASE         0x50040000  // Base address for arfic_dist.h regsiters
#define GIC_PA_SIZE         0x2000      // Size of arfic_dist.h registers
#define IRAM_PA_BASE        0x40000000  // Base address for IRAM
#define LIC_PA_BASE         0x60004000  // Base address for arictlr.h registers
#define LIC_PA_SIZE         0x100       // Size of arictlr.h registers
#define MISC_PA_BASE        0x70000000  // Base address for arapb_misc.h registers
#define MISC_PA_LEN         4096        // Size of arapb_misc.h registers
#define MSELECT_PA_BASE     0x50042000  // Base address for armselect.h registers
#define PG_UP_PA_BASE       0x60000000  // Base address for arpg.h registers
#define PMC_PA_BASE         0x7000E400  // Base address for arapbpm.h registers

#define SYSCTR0_PA_BASE     0x700F0000  // Base address for arsysctr0.h

#define SECURE_BOOT_PA_BASE 0x6000C200  // Base address for arsecure_boot.h registers
#define TIMERUS_PA_BASE     0x60005010  // Base address for artimerus.h registers
#define TIMER0_PA_BASE      0x60005000  // Base address for artimer.h (instance 0) registers
#define TIMER1_PA_BASE      0x60005008  // Base address for artimer.h (instance 1) registers
#define GPIO3_I_PA_BASE     0x6000d200  // Base address of GPIO.PI for argpio.h registers
#define GPIO3_K_PA_BASE     0x6000d208  // Base address of GPIO.PK for argpio.h registers
#define VDE_PA_BASE         0x6001a000  // Base address for arvde2x.h registers

#define NV_AHB_ARBC_REGR(pArb, reg)             NV_READ32( (((NvUPtr)(pArb)) + AHB_ARBITRATION_##reg##_0))
#define NV_AHB_ARBC_REGW(pArb, reg, val)        NV_WRITE32((((NvUPtr)(pArb)) + AHB_ARBITRATION_##reg##_0), (val))

#define NV_AHB_GIZMO_REGR(pAhbGizmo, reg)       NV_READ32( (((NvUPtr)(pAhbGizmo)) + AHB_GIZMO_##reg##_0))
#define NV_AHB_GIZMO_REGW(pAhbGizmo, reg, val)  NV_WRITE32((((NvUPtr)(pAhbGizmo)) + AHB_GIZMO_##reg##_0), (val))

#define NV_CAR_REGR(pCar, reg)                  NV_READ32( (((NvUPtr)(pCar)) + CLK_RST_CONTROLLER_##reg##_0))
#define NV_CAR_REGW(pCar, reg, val)             NV_WRITE32((((NvUPtr)(pCar)) + CLK_RST_CONTROLLER_##reg##_0), (val))

#define NV_CSITE_REGR(pCsite, reg)              NV_READ32( (((NvUPtr)(pCsite)) + CSITE_##reg##_0))
#define NV_CSITE_REGW(pCsite, reg, val)         NV_WRITE32((((NvUPtr)(pCsite)) + CSITE_##reg##_0), (val))

#define NV_EMC_REGR(pEmc, reg)                  NV_READ32( (((NvUPtr)(pEmc)) + EMC_##reg##_0))
#define NV_EMC_REGW(pEmc, reg, val)             NV_WRITE32((((NvUPtr)(pEmc)) + EMC_##reg##_0), (val))

#define NV_EVP_REGR(pEvp, reg)                  NV_READ32( (((NvUPtr)(pEvp)) + EVP_##reg##_0))
#define NV_EVP_REGW(pEvp, reg, val)             NV_WRITE32((((NvUPtr)(pEvp)) + EVP_##reg##_0), (val))

#define NV_FLOW_REGR(pFlow, reg)                NV_READ32((((NvUPtr)(pFlow)) + FLOW_CTLR_##reg##_0))
#define NV_FLOW_REGW(pFlow, reg, val)           NV_WRITE32((((NvUPtr)(pFlow)) + FLOW_CTLR_##reg##_0), (val))

#define NV_FUSE_REGR(pFuse, reg)                NV_READ32( (((NvUPtr)(pFuse)) + FUSE_##reg##_0))

#define NV_MC_REGR(pMc, reg)                    NV_READ32( (((NvUPtr)(pMc)) + MC_##reg##_0))
#define NV_MC_REGW(pMc, reg, val)               NV_WRITE32((((NvUPtr)(pMc)) + MC_##reg##_0), (val))

#define NV_MISC_REGR(pMisc, reg)                NV_READ32( (((NvUPtr)(pMisc)) + APB_MISC_##reg##_0))
#define NV_MISC_REGW(pMisc, reg, val)           NV_WRITE32((((NvUPtr)(pMisc)) + APB_MISC_##reg##_0), (val))

#define NV_PMC_REGR(pPmc, reg)                  NV_READ32( (((NvUPtr)(pPmc)) + APBDEV_PMC_##reg##_0))
#define NV_PMC_REGW(pPmc, reg, val)             NV_WRITE32((((NvUPtr)(pPmc)) + APBDEV_PMC_##reg##_0), (val))

#define NV_TIMERUS_REGR(pTimer, reg)            NV_READ32((((NvUPtr)(pTimer)) + TIMERUS_##reg##_0))
#define NV_TIMERUS_REGW(pTimer, reg, val)       NV_WRITE32((((NvUPtr)(pTimer)) + TIMERUS_##reg##_0), (val))

#define NV_GPIO_REGR(pGpio, reg)                NV_READ32((((NvUPtr)(pGpio)) + GPIO_##reg##_0))
#define NV_GPIO_REGW(pGpio, reg, val)           NV_WRITE32((((NvUPtr)(pGpio)) + GPIO_##reg##_0), (val))

#define NV_CEIL(time, clock)     (((time) + (clock) - 1)/(clock))

/// Calculate clock fractional divider value from reference and target frequencies
#define CLK_DIVIDER(REF, FREQ)  ((((REF) * 2) / FREQ) - 2)

/// Calculate clock frequency value from reference and clock divider value
#define CLK_FREQUENCY(REF, REG)  (((REF) * 2) / (REG + 2))

// Number of IRQs per interrupt controller
#define IRQS_PER_INTR_CTLR     32

#define LIC_INTERRUPT_PENDING( ctlr ) \
    (LIC_PA_BASE + ((ctlr) * LIC_PA_SIZE) + ICTLR_VIRQ_CPU_0)

#define LIC_INTERRUPT_SET( ctlr ) \
    (LIC_PA_BASE + ((ctlr) * LIC_PA_SIZE) + ICTLR_CPU_IER_SET_0)

#define LIC_INTERRUPT_CLR( ctlr ) \
    (LIC_PA_BASE + ((ctlr) * LIC_PA_SIZE) + ICTLR_CPU_IER_CLR_0)

#define GIC_INTERRUPT_INT_CLR( inst ) \
    (GIC_PA_BASE + FIC_DIST_ENABLE_CLEAR_0_0 + ((inst) * 4))

#define GIC_INTERRUPT_INT_SET( inst ) \
    (GIC_PA_BASE + FIC_DIST_ENABLE_SET_0_0 + ((inst) * 4))

#define AVP_IRQ_TIMER_0         (0)
#define AVP_IRQ_TIMER_1         (1)
#define AVP_IRQ_TIMER_2         (9)
#define AVP_IRQ_TIMER_3         (10)

#define CPU_IRQ_TIMER_0         (AVP_IRQ_TIMER_0 + 32)
#define CPU_IRQ_TIMER_1         (AVP_IRQ_TIMER_1 + 32)
#define CPU_IRQ_TIMER_2         (AVP_IRQ_TIMER_2 + 32)
#define CPU_IRQ_TIMER_3         (AVP_IRQ_TIMER_3 + 32)

/* Exception Vectors */
#define VECTOR_RESET            (VECTOR_BASE + 0)
#define VECTOR_UNDEF            (VECTOR_BASE + 4)
#define VECTOR_SWI              (VECTOR_BASE + 8)
#define VECTOR_PREFETCH_ABORT   (VECTOR_BASE + 12)
#define VECTOR_DATA_ABORT       (VECTOR_BASE + 16)
#define VECTOR_IRQ              (VECTOR_BASE + 24)
#define VECTOR_FIQ              (VECTOR_BASE + 28)

/* Processor Modes */
#define MODE_I              (0x80) // IRQ enable
#define MODE_F              (0x40) // FIQ enable
#define MODE_T              (0x20) // Thumb
#define MODE_DISABLE_INTR   (0xc0)
#define MODE_USR            (0x10)
#define MODE_FIQ            (0x11)
#define MODE_IRQ            (0x12)
#define MODE_SVC            (0x13)
#define MODE_ABT            (0x17)
#define MODE_UND            (0x1B)
#define MODE_SYS            (0x1F)
#define MODE_MON            (0x16) // only available with TrustZone

/**
 * @brief CPU complex id.
 */
typedef enum NvBlCpuClusterId_t
{
    /// Unknown CPU complex
    NvBlCpuClusterId_Unknown = 0,

    /// CPU complex 0
    NvBlCpuClusterId_Fast,
    NvBlCpuClusterId_G = NvBlCpuClusterId_Fast,

    /// CPU complex 1
    NvBlCpuClusterId_Slow,
    NvBlCpuClusterId_LP = NvBlCpuClusterId_Slow,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvBlCpuClusterId__Force32 = 0x7FFFFFFF

} NvBlCpuClusterId;

#endif // INCLUDED_AOS_COMMON_H
