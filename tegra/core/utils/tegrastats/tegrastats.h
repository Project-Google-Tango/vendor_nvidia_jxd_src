/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRASTATS_H
#define TEGRASTATS_H

#define ARRAYSIZE(arr) (sizeof(arr)/sizeof(arr[0]))

#define NVMAP_BASE_PATH "/sys/devices/platform/tegra-nvmap/misc/nvmap/"
#define CARVEOUT(x) NVMAP_BASE_PATH "heap-generic-0/" # x
#define IRAM(x)     NVMAP_BASE_PATH "heap-iram/" # x

#define EDP_LIMIT_DEBUGFS_PATH "/sys/kernel/debug/edp/vdd_cpu/edp_limit"
#define DVFS_CLOCKS_BASE_PATH "/sys/kernel/debug/clock/"

typedef struct _ClockNode
{
    const char* path;
    int64_t multiplier;
} ClockNode;

typedef enum _Chip
{
    AP20 = 0,
    T30 = 1,
    T114 = 2,
    T148 = 3,
    T124 = 4
} Chip;

static const ClockNode CPUFreqNodeTable[] = {
    /*AP_20*/ { "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", 1000 },
    /*T30*/ { "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", 1000 },
    /*T114*/ { "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", 1000 },
    /*T148*/ { "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", 1000 },
    /*T124*/ { "/sys/kernel/debug/clock/cpu/rate", 1 },
};

static const ClockNode GPUFreqNodeTable[] = {
    /*AP_20*/ { "/sys/kernel/debug/clock/3d/rate", 1 },
    /*T30*/ { "/sys/kernel/debug/clock/3d/rate", 1 },
    /*T114*/ { "/sys/kernel/debug/clock/3d/rate", 1 },
    /*T148*/ { "/sys/kernel/debug/clock/3d/rate", 1 },
    /*T124*/ { "/sys/kernel/debug/clock/gk20a.gbus/rate", 1 },
};

static const ClockNode GPUActmonNodeTable[] = {
    /*AP_20*/ { "/sys/kernel/debug/tegra_host/3d_actmon_avg_norm", 10 },
    /*T30*/ { "/sys/kernel/debug/tegra_host/3d_actmon_avg_norm", 10 },
    /*T114*/ { "/sys/kernel/debug/tegra_host/3d_actmon_avg_norm", 10 },
    /*T148*/ { "/sys/kernel/debug/tegra_host/3d_actmon_avg_norm", 10 },
    /*T124*/ { "/sys/devices/platform/host1x/gk20a.0/load", 10 },
};

static const ClockNode EMCFreqNodeTable[] = {
    /*AP_20*/ { "/sys/kernel/debug/clock/emc/rate", 1 },
    /*T30*/ { "/sys/kernel/debug/clock/emc/rate", 1 },
    /*T114*/ { "/sys/kernel/debug/clock/emc/rate", 1 },
    /*T148*/ { "/sys/kernel/debug/clock/emc/rate", 1 },
    /*T124*/ { "/sys/kernel/debug/clock/emc/rate", 1 },
};

static const ClockNode EMCActmonNodeTable[] = {
    /*AP_20*/ { "/sys/kernel/debug/tegra_actmon/emc/avg_activity", 1000 },
    /*T30*/ { "/sys/kernel/debug/tegra_actmon/emc/avg_activity", 1000 },
    /*T114*/ { "/sys/kernel/debug/tegra_actmon/emc/avg_activity", 1000 },
    /*T148*/ { "/sys/kernel/debug/tegra_actmon/emc/avg_activity", 1000 },
    /*T124*/ { "/sys/kernel/debug/tegra_actmon/emc/avg_activity", 1000 },
};

static const ClockNode AVPFreqNodeTable[] = {
    /*AP_20*/ { "/sys/kernel/debug/clock/avp.sclk/rate", 1 },
    /*T30*/ { "/sys/kernel/debug/clock/avp.sclk/rate", 1 },
    /*T114*/ { "/sys/kernel/debug/clock/avp.sclk/rate", 1 },
    /*T148*/ { "/sys/kernel/debug/clock/avp.sclk/rate", 1 },
    /*T124*/ { "/sys/kernel/debug/clock/avp.sclk/rate", 1 },
};

static const ClockNode AVPActmonNodeTable[] = {
    /*AP_20*/ { "/sys/kernel/debug/tegra_actmon/avp/avg_activity", 1000 },
    /*T30*/ { "/sys/kernel/debug/tegra_actmon/avp/avg_activity", 1000 },
    /*T114*/ { "/sys/kernel/debug/tegra_actmon/avp/avg_activity", 1000 },
    /*T148*/ { "/sys/kernel/debug/tegra_actmon/avp/avg_activity", 1000 },
    /*T124*/ { "/sys/kernel/debug/tegra_actmon/avp/avg_activity", 1000 },
};

static const ClockNode VDEFreqNodeTable[] = {
    /*AP_20*/ { "/sys/kernel/debug/clock/vde/rate", 1 },
    /*T30*/ { "/sys/kernel/debug/clock/vde/rate", 1 },
    /*T114*/ { "/sys/kernel/debug/clock/vde/rate", 1 },
    /*T148*/ { "/sys/kernel/debug/clock/vde/rate", 1 },
    /*T124*/ { "/sys/kernel/debug/clock/vde/rate", 1 },
};

#define CPU_FREQ_MAX_NODE "/dev/cpu_freq_max"
#define CPU_FREQ_MIN_NODE "/dev/cpu_freq_min"
#define GPU_FREQ_MAX_NODE "/dev/gpu_freq_max"
#define GPU_FREQ_MIN_NODE "/dev/gpu_freq_min"
#define MIN_ONLINE_CPU_NODE "/dev/min_online_cpus"
#define MAX_ONLINE_CPU_NODE "/dev/max_online_cpus"
#define FPS_MAX_PROPERTY "persist.sys.NV_FPSLIMIT"

/* use thermal_zone[0..n]/temp and /type */
#define TEMP_SENSORS_PATH "/sys/devices/virtual/thermal/"
#define TEMP_DIVIDER 1

#define NUM_SLOTS           11
#ifndef PAGE_SIZE
#define PAGE_SIZE         4096
#endif
#define FREQUENCY_CONVERT 1000
#define AP20_CHIPID         20
#define T30_CHIPID          30
#define T114_CHIPID         35
#define T124_CHIPID         40
#define T148_CHIPID         14
#define T124_CHIPID         40
#define ALLOC_BUFFER_SIZE 1024
#define PRINT_BUFF_SIZE 1024
#define MAX_PATH           256
#define USECS_PER_SAMPLE   1000

typedef struct _CPUInfo
{
    int64_t online;
    int64_t currentLoad;
    int64_t frequency;
    int64_t jiffieCount;
    int64_t idleJiffieCount;
    int64_t previousLoads[10];
} CPUInfo;

typedef struct _SamplingInfo
{
    int64_t  cpuCount;
    CPUInfo cpu[4];
    int64_t totalRAMkB;
    int64_t freeRAMkB;
    int64_t buffersRAMkB;
    int64_t cachedRAMkB;
    int64_t totalCarveoutB;
    int64_t freeCarveoutB;
    int64_t largestFreeCarveoutBlockB;
    int64_t edp_limit;
    int64_t throttle_count;
    int64_t totalGARTkB;
    int64_t freeGARTkB;
    int64_t largestFreeGARTBlockkB;
    int64_t gr3d_avg;
    int64_t emc_clk_avg;
    int64_t avp_clk_avg;
    int64_t emcClk;
    int64_t avpClk;
    int64_t vdeClk;
    int64_t gr3dClk;
    int64_t largestFreeRAMBlockB;
    int64_t numLargestRAMBlock;
    int64_t largestFreeIRAMBlockB;
    int64_t totalIRAMB;
    int64_t freeIRAMB;

    //PBC Info
    int64_t fpsMax;

    // Frequency limit information
    int64_t pmqosCpuMaxFreq;
    int64_t pmqosCpuMinFreq;
    int64_t pmqosGpuMaxFreq;
    int64_t pmqosGpuMinFreq;
    int64_t pmqosMinOnlineCpu;
    int64_t pmqosMaxOnlineCpu;
} SamplingInfo;

// ATrace flags

#define ATRACE_GPU_UTIL (0x1 << 0)
#define ATRACE_GPU_FREQUENCY (0x1 << 1)
#define ATRACE_CPU_UTIL (0x1 << 2)
#define ATRACE_CPU_FREQUENCY (0x1 << 3)
#define ATRACE_EMC_UTIL (0x1 << 4)
#define ATRACE_EMC_FREQUENCY (0x1 << 5)
#define ATRACE_MEMORY (0x1 << 6)
#define ATRACE_VDE (0x1 << 7)
#define ATRACE_AVP (0x1 << 8)
#define ATRACE_EDP (0x1 << 9)
#define ATRACE_MISC (0x1 << 10)
#define ATRACE_PBC (0x1 << 11)
#define ATRACE_TEMP (0x1 << 12)
#define ATRACE_GPU_LIMIT (0x1 << 13)
#define ATRACE_CPU_LIMIT (0x1 << 14)

// ATrace strings

#define CPU0_FREQ_ATRACE "CPU0 Freq(MHz)"
#define CPU1_FREQ_ATRACE "CPU1 Freq(MHz)"
#define CPU2_FREQ_ATRACE "CPU2 Freq(MHz)"
#define CPU3_FREQ_ATRACE "CPU3 Freq(MHz)"
#define CPU0_LOAD_ATRACE "CPU0 Load(%)"
#define CPU1_LOAD_ATRACE "CPU1 Load(%)"
#define CPU2_LOAD_ATRACE "CPU2 Load(%)"
#define CPU3_LOAD_ATRACE "CPU3 Load(%)"
#define GPU_USAGE_ATRACE "GPU Usage(%)"
#define GPU_FREQ_ATRACE "GPU Freq(MHz)"
#define EMC_USAGE_ATRACE "EMC Usage(%)"
#define EMC_FREQ_ATRACE "EMC Freq(MHz)"
#define VDE_FREQ_ATRACE "VDE Freq(MHz)"
#define AVP_FREQ_ATRACE "AVP Freq(MHz)"
#define AVP_USAGE_ATRACE "AVP Usage(%)"
#define FREE_IRAM_ATRACE "Free IRAM"
#define THROTTLE_COUNT_ATRACE "Throttle Count"
#define EDP_LIMIT_ATRACE "EDP Limit(MHz)"
#define FREE_MEMORY_ATRACE "Free Memory"
#define BUFFERED_MEMORY_ATRACE "Buffer Memory"
#define CACHED_MEMORY_ATRACE "Cached Memory"
#define GPU_FREQ_MAX_ATRACE "PMQoS GPU Frequency Max(MHz)"
#define GPU_FREQ_MIN_ATRACE "PMQoS GPU Frequency Min(MHz)"
#define CPU_FREQ_MAX_ATRACE "PMQoS CPU Frequency Max(MHz)"
#define CPU_FREQ_MIN_ATRACE "PMQoS CPU Frequency Min(MHz)"
#define MIN_ONLINE_CPU_ATRACE "Min Online Cpu"
#define MAX_ONLINE_CPU_ATRACE "Max Online Cpu"
#define FPS_MAX_ATRACE "FPS Limit"
#define TEMP_ATRACE_PREFIX "Temp "

typedef struct _LimitInfo {
    int atraceFlag;
    const char* nodePath;
    const char* atraceName;
    int isFrequency;
    int64_t* sampleData;
} LimitInfo;

#endif //TEGRASTATS_H
