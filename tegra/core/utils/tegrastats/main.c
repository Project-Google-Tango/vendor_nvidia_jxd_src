/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

#ifdef TEGRASTATS_ATRACE
    #include <cutils/properties.h>
    #include <cutils/trace.h>
    #define traceCounter(tag, name, value) atrace_int(tag, name, value)
#else
    #define traceCounter(tag, name, value)
#endif

#if !NV_IS_LDK
    #include <utils/Log.h>
    #undef LOG_TAG
    #define LOG_TAG "TegraStats"
    #undef LOGE
    #define LOGE ALOGE
    #undef LOGI
    #define LOGI ALOGI
#else
    #include <string.h>
    #define LOGE(...)        \
    do {                     \
        printf(__VA_ARGS__); \
        printf("\n");        \
    } while (0)

    #define LOGI(...)        \
    do {                     \
        printf(__VA_ARGS__); \
        printf("\n");        \
    } while (0)
#endif

#define PRINTE(toLog, logFile,  ...)    \
do{                                     \
    if (toLog && (logFile != NULL))     \
    {                                   \
        fprintf(logFile, __VA_ARGS__);  \
        fprintf(logFile, "\n");         \
    }                                   \
    else                                \
    {                                   \
        LOGE(__VA_ARGS__);              \
    }                                   \
}while(0)

#include "tegrastats.h"

/* Prototypes. */

int main(int argc, char *argv[]);

static void get_temps(int argc, char *argv[]);
static void logFlush(void);
static int B2MB(int bytes);
static int kB2MB(int kiloBytes);
static int B2kB(int bytes);
static int SmartB2Str(char* str, size_t size, int bytes);
static void setFreq(int setMax);
static long processdir(const struct dirent *dir);
static int get_tegrastats_pid(void);
static void signal_handler(int signal);
static int filter(const struct dirent *dir);
static int getChipId(void);
static void sampleCPULoad(void);
static int readValue(const char* path, int64_t* value);

static int bToLog = 0;
static int bOnce = 0;
static int chipId = 0;
static Chip chipType = AP20;
static int bThrottle = 0;
static unsigned int sleepMS = 1000;

SamplingInfo sampleData;

static int aTraceInCaptureMode = 0;
static int aTraceFlags = 0;

/* extra arbitrary data to include in the output */
/* format is:  [--extra <label> <path_file_name>] */
/* up to 10 extra data elements supported        */
/* only handles integer data elements            */

int nExtra=0;
#define MAX_EXTRAS 20
struct
{
  char path[MAX_PATH];
  char label[MAX_PATH];
  int64_t  data;
} extras[MAX_EXTRAS];


/* [--extra <label> <path>] */
static void sampleExtras(int argc, char *argv[])
{
  int i=0;
  static int init_done=0;

  for (i=1;(i<argc) && (nExtra<MAX_EXTRAS) && (init_done == 0);i++)
  {
     if (!strcmp(argv[i], "--extra"))
     {
        strncpy(extras[nExtra].label,  argv[++i],MAX_PATH-1);
        strncpy(extras[nExtra++].path, argv[++i],MAX_PATH-1);
     }
  }
  init_done=1;

  for (i=0;i<nExtra;i++)
  {
     readValue(extras[i].path, &extras[i].data);
  }
}

const int CONVERT_KHZ = 1;
const int CONVERT_MHZ = 2;
const int CONVERT_GHZ = 3;

static int64_t convertFrequency(int64_t value, int units)
{
    int i;
    for(i = 0; i < units; i++)
    {
        // If this conversion will reduce our data to a single digit
        // don't perform it.  Our units may be off as far as reporting
        // in the logs, but at the very least data won't be lost.
        if(value / FREQUENCY_CONVERT < 10)
            break;

        value /= FREQUENCY_CONVERT;
    }

    return value;
}

/* temperature zone info */
int nTemps=0;
#define MAX_TEMP_ZONES 10
struct
{
  char enable;
  char pathname[MAX_PATH];
  char type[MAX_PATH];
  int  temp;
} temps[MAX_TEMP_ZONES];

static void get_temps(int argc, char *argv[])
{
  DIR * dirp;
  struct dirent *dp;
  FILE *f;
  char fn[MAX_PATH];
  int i=0;
  static int init_done=0;

  if (init_done == 0)
  {
     for (i=0; i<MAX_TEMP_ZONES; i++)
     {
        temps[i].type[0] = '\0';
        temps[i].enable = 0;
     }
  }

  nTemps=0;
  dirp = opendir(TEMP_SENSORS_PATH);
  while ( ((dp = readdir(dirp)) != NULL) && (nTemps < MAX_TEMP_ZONES))
  {
     if (!fnmatch("thermal_zone*", dp->d_name, 0))
     {
        sprintf(temps[nTemps].pathname,"%s%s",TEMP_SENSORS_PATH,dp->d_name);
        strncpy(fn,temps[nTemps].pathname,MAX_PATH-6);
        strcat(fn,"/type");

        f = fopen(fn, "r");
        if (f)
        {
            fscanf(f, "%20s", temps[nTemps].type);
            fclose(f);
        }

        strncpy(fn,temps[nTemps].pathname,MAX_PATH-6);
        strcat(fn,"/temp");

        f = fopen(fn, "r");
        if (f)
        {
            fscanf(f, "%d", &temps[nTemps].temp);
            fclose(f);
        }

        for (i=0;i<argc && init_done == 0;i++)
        {
           if (!strncmp(argv[i], temps[nTemps].type, MAX_PATH))
           {
               temps[nTemps].enable = 1;
               break;
           }
        }

         nTemps++;
      }
  }
  closedir(dirp);
  init_done=1;
}

/* Store clk values to restore later */
// Assuming clk frequencies are same for both CPUs
unsigned int cpuclk[2];

FILE *f        = NULL;
FILE *logFile  = NULL;

/* Functions. */

static int readValueExt(const char* path, int64_t* value, int verbose)
{
    *value = 0;

    if (!path)
        return 0;

    f = fopen(path, "r");
    if (f)
    {
        (void) fscanf(f, "%lld", value);
        fclose(f);
    }
    else
    {
        if (verbose)
            PRINTE(bToLog, logFile, "Failed to open %s", path);
        return 0;
    }

    return 1;
}

static int readValue(const char* path, int64_t* value)
{
    return readValueExt(path, value, 0);
}

static void logFlush(void)
{
#if NV_IS_LDK
    // need to fflush on LDK to make output redirectable
    fflush(stdout);
#endif
    if ((logFile != NULL) && (fileno(logFile) != -1))
    {
        fflush(logFile);
    }
}

static int B2MB(int bytes)
{
    bytes += (1<<19)-1;       // rounding
    return bytes >> 20;
}

static int kB2MB(int kiloBytes)
{
    kiloBytes += (1<<9)-1;    // rounding
    return kiloBytes >> 10;
}

static int B2kB(int bytes)
{
    bytes += (1<<9)-1;        // rounding
    return bytes >> 10;
}

static int SmartB2Str(char* str, size_t size, int bytes)
{
    if (bytes < 1024)
    {
        return snprintf(str, size, "%dB", bytes);
    }
    else if (bytes < 1024*1024)
    {
        return snprintf(str, size, "%dkB", B2kB(bytes));
    }
    else
    {
        return snprintf(str, size, "%dMB", B2MB(bytes));
    }
}

static void setFreq(int setMax)
{
    FILE* f;
    LOGI("setFreq %d", setMax);

    if (!cpuclk[0])
    {
        f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies", "r");
        if (f)
        {
            fscanf(f, "%u", &cpuclk[0]);
            while(fscanf(f, "%u", &cpuclk[1]) != EOF);

            LOGI("cpuclk: minfreq = %u maxfreq = %u\n", cpuclk[0], cpuclk[1]);
            fclose(f);
        }
        else
        {
            LOGE("Error opening file scaling_available_frequencies");
        }
    }
    if (setMax)
    {
        // set CPU frequency to highest value
        f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "w");
        if (f)
        {
            fprintf(f, "%u", cpuclk[1]);
            fclose(f);
        }
        else
        {
            LOGE("Error opening file scaling_min_freq\n");
        }
    }
    else
    {
        // set default
        f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "w");
        if (f)
        {
            fprintf(f, "%u", cpuclk[0]);
            fclose(f);
        }
        else
        {
            LOGE("Error opening file scaling_min_freq\n");
        }
    }
}

static long processdir(const struct dirent *dir)
{
     char path[256];
     char linkinfo[256];

     memset(path, 0, sizeof path);
     memset(linkinfo, 0, sizeof linkinfo);

     strcpy(path, "/proc/");
     strcat(path, dir->d_name);
     strcat(path, "/exe");
     readlink(path, linkinfo, sizeof linkinfo);
     if (strstr(linkinfo, "tegrastats") != NULL)
     {
        return strtol(dir->d_name, (char **) NULL, 10);
     }
     return 0;
}

// get the pid of running tegrastats other than itself
static int get_tegrastats_pid(void)
{
    struct dirent **namelist;
    int n;
    int pid = 0;

    n = scandir("/proc", &namelist, filter, 0);
    if (n < 0)
        perror("Not enough memory.");
    else
    {
        while (n--)
        {
            pid = processdir(namelist[n]);
            if ((pid != 0) && (getpid() != (pid_t) pid))
            {
                return pid;
            }
            free(namelist[n]);
        }
        free(namelist);
    }
    return pid;
}

static void signal_handler(int signal)
{
    if ((f != NULL) && (fileno(f) != -1))
    {
        fclose(f);
    }
    logFlush();
    if ((logFile != NULL) && (fileno(logFile) != -1))
    {
        fclose(logFile);
    }

    exit(0);
}

static int filter(const struct dirent *dir)
{
    return !fnmatch("[1-9]*", dir->d_name, 0);
}

static int getChipId(void)
{
    char *contents = NULL;
    char *tegraid  = NULL;
    int count  = 0;
    int chipid = 0;

    /* open file */
    int fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0)
    {
        printf("Couldn't open %s\n", "/proc/cmdline");
        goto failout;
    }

    /* allocate enough memory */
    contents = malloc(ALLOC_BUFFER_SIZE);
    if (!contents)
    {
        printf("Couldn't allocate mem %d bytes\n", ALLOC_BUFFER_SIZE);
        goto failout;
    }

    /* read the contents of the file */
    count = read(fd, contents, ALLOC_BUFFER_SIZE-1);
    if (count < 0)
    {
        printf("Couldn't read the file %s\n", "/proc/cmdline");
        goto failout;
    }

    /* add zero to make it a string */
    contents[count] = '\0';

    tegraid = strstr(contents, "tegraid=");
    if (tegraid)
    {
        tegraid += strlen("tegraid=");
        chipid = atoi(tegraid);
    }

failout:
    if (fd >= 0)
    {
        close(fd);
    }
    free(contents);
    return chipid;
}

static void printReport(void)
{
    char buff[PRINT_BUFF_SIZE];
    char* buff_cur = buff;
    char* buff_end = buff + PRINT_BUFF_SIZE;

    char lfbRAM[10];
    char lfbCarveout[10];
    char lfbGART[10];
    char lfbIRAM[10];

    int i;

    buff[0] = '\0';

    SmartB2Str(lfbRAM, 10, sampleData.largestFreeRAMBlockB);
    SmartB2Str(lfbCarveout, 10, sampleData.largestFreeCarveoutBlockB);
    SmartB2Str(lfbGART, 10, sampleData.largestFreeGARTBlockkB * 1024);
    SmartB2Str(lfbIRAM, 10, sampleData.largestFreeIRAMBlockB);

    // Append RAM data
    buff_cur += snprintf(buff_cur, buff_end - buff_cur, "RAM %d/%dMB (lfb %dx%s)",
        kB2MB(sampleData.totalRAMkB-sampleData.freeRAMkB-sampleData.buffersRAMkB-sampleData.cachedRAMkB), kB2MB(sampleData.totalRAMkB),
        (int)sampleData.numLargestRAMBlock, lfbRAM);

    if (sampleData.totalCarveoutB != 0)
    {
        buff_cur += snprintf(buff_cur, buff_end - buff_cur, " Carveout %d/%dMB (lfb %s)",
            B2MB(sampleData.totalCarveoutB-sampleData.freeCarveoutB), B2MB(sampleData.totalCarveoutB), lfbCarveout);
    }

    // Append IRAM data
    buff_cur += snprintf(buff_cur, buff_end - buff_cur, " IRAM %d/%dkB(lfb %s)", B2kB(sampleData.totalIRAMB-sampleData.freeIRAMB),
        B2kB(sampleData.totalIRAMB), lfbIRAM);

    // Append CPU data
    buff_cur += snprintf(buff_cur, buff_end - buff_cur, " cpu [");
    for (i = 0; i < sampleData.cpuCount; i++)
    {
        if (sampleData.cpu[i].online)
        {
            buff_cur += snprintf(buff_cur, buff_end - buff_cur, "%lld%%", sampleData.cpu[i].currentLoad);
        } else {
            buff_cur += snprintf(buff_cur, buff_end - buff_cur, "off");
        }
        if (i < sampleData.cpuCount - 1)
        {
            buff_cur += snprintf(buff_cur, buff_end - buff_cur, ",");
        }
    }
    buff_cur += snprintf(buff_cur, buff_end - buff_cur, "]@%lld", convertFrequency(sampleData.cpu[0].frequency, CONVERT_MHZ));

    // Append EMC data
    if (sampleData.emcClk)
        buff_cur += snprintf(buff_cur, buff_end - buff_cur, " EMC %lld%%@%lld", 100 * sampleData.emc_clk_avg/sampleData.emcClk, convertFrequency(sampleData.emcClk, CONVERT_MHZ));

    // Append AVP data
    if (sampleData.avpClk)
        buff_cur += snprintf(buff_cur, buff_end - buff_cur, " AVP %lld%%@%lld", 100 * sampleData.avp_clk_avg/sampleData.avpClk, convertFrequency(sampleData.avpClk, CONVERT_MHZ));

    // Append VDE data
    buff_cur += snprintf(buff_cur, buff_end - buff_cur, " VDE %lld", convertFrequency(sampleData.vdeClk, CONVERT_MHZ));

    if (chipId != AP20_CHIPID && chipId != T30_CHIPID)
    {
        buff_cur += snprintf(buff_cur, buff_end - buff_cur,
            " GR3D %lld%%@%lld",
            sampleData.gr3d_avg / 100,
            convertFrequency(sampleData.gr3dClk, CONVERT_MHZ));
    }

    for (i=0;i<nTemps;i++)
    {
        if (temps[i].enable)
        {
            buff_cur += snprintf(buff_cur, buff_end - buff_cur,
                " %s %d ", temps[i].type, temps[i].temp/TEMP_DIVIDER);
        }
    }

    if (bThrottle)
    {
        buff_cur += snprintf(buff_cur, buff_end - buff_cur, " throttle %lld", sampleData.throttle_count);
    }

    buff_cur += snprintf(buff_cur, buff_end - buff_cur,
                            " EDP limit %lld", convertFrequency(sampleData.edp_limit, CONVERT_KHZ));

    for (i=0;i<nExtra;i++)
    {
         buff_cur += snprintf(buff_cur, buff_end - buff_cur,
             " %s %lld ", extras[i].label, extras[i].data);
    }

    PRINTE(bToLog, logFile, "%s", buff);
}

static int sampleCPUOnline(int core)
{
    char filePath[64];
    int64_t result = -1;

    sprintf(filePath, "/sys/devices/system/cpu/cpu%i/online", core);

    if (!readValue(filePath, &result))
    {
        PRINTE(bToLog, logFile, "Failed to sample CPU online state for CPU %i", core);
    }

    return (int)result;
}

static void sampleCPULoad(void)
{
    // CPU load
    FILE* f = fopen("/proc/stat", "r");
    if (f)
    {
        int c[40];
        int l[40];
        int i;
        int check_zero = 0;

        //
        // from http://www.mjmwired.net/kernel/Documentation/filesystems/proc.txt
        // Various pieces of information about kernel activity are available
        // in the /proc/stat file. All of the numbers reported in this file
        // are aggregates since the system first booted. The very first
        // "cpu" line aggregates the numbers in all of the other "cpuN"
        // lines. These numbers identify the amount of time the CPU has spent
        // performing different kinds of work. Time units are in USER_HZ
        // (typically hundredths of a second). The meanings of the columns
        // are as follows, from left to right:
        //  - user: normal processes executing in user mode
        //  - nice: niced processes executing in user mode
        //  - system: processes executing in kernel mode
        //  - idle: twiddling thumbs
        //  - iowait: waiting for I/O to complete
        //  - irq: servicing interrupts
        //  - softirq: servicing softirqs
        //  - steal: involuntary wait
        //  - guest: running a normal guest
        //  - guest_nice: running a niced guest
        //

        memset(c, 0, sizeof(c));

        for (;;)
        {
            int *p;
            int r;
            char cpunum;

            r = fscanf(f, "cpu%c", &cpunum);

            if (r != 1)
                break;
            if (cpunum < '0' || cpunum > '3')
            {
                (void) fscanf(f, "%*[^\n]\n");
                continue;
            }

            p = (int*)c + (cpunum - '0') * 10;

            r = fscanf(f, "%d %d %d %d %d %d %d %d %d %d\n",
                        p, p+1, p+2, p+3, p+4, p+5, p+6, p+7, p+8, p+9);

            if (r != 10)
                memset(p, 0, 10 * sizeof(int));
        }

        fclose(f);

        //
        // cpu load = (time spent on something else but idle since the last
        // update) / (total time spent since the last update)
        //

        for (i = 0; i < sampleData.cpuCount; i++)
        {
            sampleData.cpu[i].jiffieCount = 0;
            check_zero = 0;

            int j;
            for (j = 0; j < 10; j++)
            {
                int idx = i * 10 + j;
                check_zero |= sampleData.cpu[i].previousLoads[j];
                l[idx] = c[idx] - sampleData.cpu[i].previousLoads[j];
                sampleData.cpu[i].jiffieCount += l[idx];
                sampleData.cpu[i].previousLoads[j] = c[idx];
            }

            sampleData.cpu[i].idleJiffieCount = l[i * 10 + 3];

            if (sampleData.cpu[i].jiffieCount && check_zero && sampleData.cpu[i].online)
            {
                char buffer[64];

                sampleData.cpu[i].currentLoad = 100 * (sampleData.cpu[i].jiffieCount - sampleData.cpu[i].idleJiffieCount) / sampleData.cpu[i].jiffieCount;

                sprintf(buffer, "CPU%i Load(%%)", i);
                if (aTraceFlags & ATRACE_CPU_UTIL)
                {
                    traceCounter(ATRACE_TAG_ALWAYS, buffer, sampleData.cpu[i].currentLoad);
                }
            }
            else
            {
                sampleData.cpu[i].currentLoad = 0;
            }
        }
    }
    else
    {
        PRINTE(bToLog, logFile, "Failed to open /proc/stat");
    }
}

static int64_t sampleCPUFreq(void)
{
    int64_t value;
    if (readValue(CPUFreqNodeTable[chipType].path, &value))
        return value * CPUFreqNodeTable[chipType].multiplier;
    return -1;
}

static int countCPUs(void)
{
    int64_t online;

    if (readValue("/sys/devices/system/cpu/cpu1/online", &online))
    {
        if (readValue("/sys/devices/system/cpu/cpu2/online", &online))
        {
            return 4;
        }
        return 2;
    }
    return 1;
}

static void sampleCPU(void)
{
    int i;
    for (i = 0; i < sampleData.cpuCount; i++)
    {
        sampleData.cpu[i].online = sampleCPUOnline(i);
        sampleData.cpu[i].frequency = sampleData.cpu[i].online ? sampleCPUFreq() : 0;
    }

    sampleCPULoad();
}

static uint64_t getCurrentUSec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec + tv.tv_sec * 1000000;
}

static void sampleRAM(void)
{
    FILE* f = fopen("/proc/meminfo", "r");
    if (f)
    {
        (void) fscanf(f, "MemTotal: %lld kB\n", &sampleData.totalRAMkB);
        (void) fscanf(f, "MemFree: %lld kB\n", &sampleData.freeRAMkB);
        if (aTraceFlags & ATRACE_MEMORY)
        {
            traceCounter(ATRACE_TAG_ALWAYS, FREE_MEMORY_ATRACE, sampleData.freeRAMkB);
        }
        (void) fscanf(f, "Buffers: %lld kB\n", &sampleData.buffersRAMkB);
        if (aTraceFlags & ATRACE_MEMORY)
        {
            traceCounter(ATRACE_TAG_ALWAYS, BUFFERED_MEMORY_ATRACE, sampleData.buffersRAMkB);
        }
        (void) fscanf(f, "Cached: %lld kB\n", &sampleData.cachedRAMkB);
        if (aTraceFlags & ATRACE_MEMORY)
        {
            traceCounter(ATRACE_TAG_ALWAYS, CACHED_MEMORY_ATRACE, sampleData.cachedRAMkB);
        }

        fclose(f);
    }
    else
    {
        PRINTE(bToLog, logFile, "Failed to open /proc/meminfo");
    }
}

static void sampleCarveout(void)
{
    readValueExt(CARVEOUT(total_size), &sampleData.totalCarveoutB, 0);
    readValueExt(CARVEOUT(free_size), &sampleData.freeCarveoutB, 0);
    readValueExt(CARVEOUT(free_max), &sampleData.largestFreeCarveoutBlockB, 0);
}

static void sampleUsageAverages(void)
{
    if (chipId != AP20_CHIPID)
    {
        if (readValue(EMCActmonNodeTable[chipType].path, &sampleData.emc_clk_avg))
        {
            sampleData.emc_clk_avg *= EMCActmonNodeTable[chipType].multiplier;

            if(aTraceFlags & ATRACE_EMC_UTIL)
            {
                traceCounter(ATRACE_TAG_ALWAYS,EMC_USAGE_ATRACE, 100 * sampleData.emc_clk_avg/(sampleData.emcClk / 1000));
            }
        }

        if (readValue(AVPActmonNodeTable[chipType].path, &sampleData.avp_clk_avg))
        {
            sampleData.avp_clk_avg *= AVPActmonNodeTable[chipType].multiplier;

            if(aTraceFlags & ATRACE_AVP)
            {
                traceCounter(ATRACE_TAG_ALWAYS, AVP_USAGE_ATRACE, 100 * sampleData.avp_clk_avg/(sampleData.avpClk / 1000));
            }
        }

        if (chipId != T30_CHIPID)
        {
            if (readValue(GPUActmonNodeTable[chipType].path, &sampleData.gr3d_avg))
            {
                sampleData.gr3d_avg *= GPUActmonNodeTable[chipType].multiplier;

                if(aTraceFlags & ATRACE_GPU_UTIL)
                {
                    traceCounter(ATRACE_TAG_ALWAYS, GPU_USAGE_ATRACE, sampleData.gr3d_avg / 100);
                }
            }
        }
    }
}

static void sampleEDP(void)
{
    if (readValue(EDP_LIMIT_DEBUGFS_PATH, &sampleData.edp_limit) && (aTraceFlags & ATRACE_EDP))
    {
        traceCounter(ATRACE_TAG_ALWAYS, EDP_LIMIT_ATRACE, convertFrequency(sampleData.edp_limit, CONVERT_KHZ));
    }
}

static void sampleTemps(int argc, char *argv[])
{
    get_temps(argc,argv);

    if (aTraceFlags & ATRACE_TEMP) {
        int i;
        for (i = 0; i < nTemps; i++) {
            char tag[MAX_PATH+ARRAYSIZE(TEMP_ATRACE_PREFIX)];
            sprintf(tag, TEMP_ATRACE_PREFIX "%s", temps[i].type);
            traceCounter(ATRACE_TAG_ALWAYS, temps[i].type, temps[i].temp);
        }
    }
}

static void sampleThrottleCount(void)
{
    if (bThrottle)
    {
        if (readValue("/sys/devices/system/cpu/cpu0/cpufreq/stats/throttle_count", &sampleData.throttle_count) && (aTraceFlags & ATRACE_MISC))
        {
            traceCounter(ATRACE_TAG_ALWAYS, THROTTLE_COUNT_ATRACE, sampleData.throttle_count);
        }
    }
}

static void sampleGART(void)
{
    if (chipId == AP20_CHIPID)
    {
        FILE* f = fopen("/proc/iovmminfo", "r");
        if (f)
        {
            char tmp[4];
            // add if (blah) {} to get around compiler warning
            if (fscanf(f, "\ngroups\n\t<unnamed> (device: iovmm-%4c)\n\t\tsize: "
                        "%lldKiB free: %lldKiB largest: %lldKiB",
                        &tmp[0], &sampleData.totalGARTkB, &sampleData.freeGARTkB,
                        &sampleData.largestFreeGARTBlockkB))
            {}
            fclose(f);
        }
        else
        {
            PRINTE(bToLog, logFile, "Failed to open /proc/iovmminfo");
        }

        // If the largest free GART block is -1, change it to 0.
        if (sampleData.largestFreeGARTBlockkB == -1)
        {
            sampleData.largestFreeGARTBlockkB = 0;
        }
    }
}

static void sampleIRAM(void)
{
    readValue(IRAM(total_size), &sampleData.totalIRAMB);
    readValue(IRAM(free_size), &sampleData.freeIRAMB);
    readValue(IRAM(free_max), &sampleData.largestFreeIRAMBlockB);

    if (aTraceFlags & ATRACE_MEMORY)
    {
        traceCounter(ATRACE_TAG_ALWAYS, FREE_IRAM_ATRACE, sampleData.freeIRAMB);
    }
}


static void sampleDFS(void)
{
    if (readValue(EMCFreqNodeTable[chipType].path, &sampleData.emcClk))
    {
        sampleData.emcClk *= EMCFreqNodeTable[chipType].multiplier;

        if(aTraceFlags & ATRACE_EMC_FREQUENCY)
        {
            traceCounter(ATRACE_TAG_ALWAYS, EMC_FREQ_ATRACE, convertFrequency(sampleData.emcClk, CONVERT_MHZ));
        }
    }

    if (readValue(AVPFreqNodeTable[chipType].path, &sampleData.avpClk))
    {
        sampleData.avpClk *= AVPFreqNodeTable[chipType].multiplier;

        if((aTraceFlags & ATRACE_AVP))
        {
            traceCounter(ATRACE_TAG_ALWAYS, AVP_FREQ_ATRACE, convertFrequency(sampleData.avpClk, CONVERT_MHZ));
        }
    }

    if (readValue(VDEFreqNodeTable[chipType].path, &sampleData.vdeClk))
    {
        sampleData.vdeClk *= VDEFreqNodeTable[chipType].multiplier;

        if(aTraceFlags & ATRACE_VDE)
        {
            traceCounter(ATRACE_TAG_ALWAYS, VDE_FREQ_ATRACE, convertFrequency(sampleData.vdeClk, CONVERT_MHZ));
        }
    }

    if (readValue(GPUFreqNodeTable[chipType].path, &sampleData.gr3dClk))
    {
        sampleData.gr3dClk *= GPUFreqNodeTable[chipType].multiplier;

        if(aTraceFlags & ATRACE_GPU_FREQUENCY)
        {
            traceCounter(ATRACE_TAG_ALWAYS, GPU_FREQ_ATRACE, convertFrequency(sampleData.gr3dClk, CONVERT_MHZ));
        }
    }
}

static void sampleBuddyInfo(void)
{
    FILE* f = fopen("/proc/buddyinfo", "r");
    if (f)
    {
        char line[256];
        int lineNum = 0;
        int slots[NUM_SLOTS];
        int i;

        //
        // Get the number of free blocks for each size.
        // Separation into nodes and zones is not kept.
        //
        while (fgets(line, sizeof(line), f))
        {
            int j = 0;
            int n;
            int tmpSlots[NUM_SLOTS];
            char* buf = line;

            (void) sscanf(buf, "Node %*d, zone %*s%n", &n);
            buf += n;

            while (sscanf(buf, "%d%n", &tmpSlots[j], &n) == 1)
            {
                buf += n;
                slots[j] = lineNum ? slots[j] + tmpSlots[j] : tmpSlots[j];
                j++;
            }

            lineNum++;
        }

        fclose(f);

        // Extract info about the largest available blocks
        i = NUM_SLOTS - 1;
        while (slots[i] == 0 && i > 0)
            i--;
        sampleData.numLargestRAMBlock = slots[i];
        sampleData.largestFreeRAMBlockB = (1 << i) * PAGE_SIZE;
    }
    else
    {
        PRINTE(bToLog, logFile, "Failed to open /proc/buddyinfo");
    }
}

static void samplePBC(void)
{
#ifdef TEGRASTATS_ATRACE
    {
        char value[PROPERTY_VALUE_MAX];
        property_get(FPS_MAX_PROPERTY, value, "60");
        int fpsMax = atoi(value);

        if ((fpsMax > 0) && (aTraceFlags & ATRACE_PBC) && (fpsMax != sampleData.fpsMax || fpsMax < 60))
        {
            sampleData.fpsMax = fpsMax;
            traceCounter(ATRACE_TAG_ALWAYS, FPS_MAX_ATRACE, sampleData.fpsMax);
        }
    }
#endif
}

LimitInfo limitInfo[] = {
    {
        .atraceFlag = ATRACE_GPU_LIMIT,
        .nodePath = GPU_FREQ_MAX_NODE,
        .atraceName = GPU_FREQ_MAX_ATRACE,
        .isFrequency = 1,
        .sampleData = &sampleData.pmqosGpuMaxFreq
    },
    {
        .atraceFlag = ATRACE_GPU_LIMIT,
        .nodePath = GPU_FREQ_MIN_NODE,
        .atraceName = GPU_FREQ_MIN_ATRACE,
        .isFrequency = 1,
        .sampleData = &sampleData.pmqosGpuMinFreq
    },
    {
        .atraceFlag = ATRACE_CPU_LIMIT,
        .nodePath = CPU_FREQ_MAX_NODE,
        .atraceName = CPU_FREQ_MAX_ATRACE,
        .isFrequency = 1,
        .sampleData = &sampleData.pmqosCpuMaxFreq
    },
    {
        .atraceFlag = ATRACE_CPU_LIMIT,
        .nodePath = CPU_FREQ_MIN_NODE,
        .atraceName = CPU_FREQ_MIN_ATRACE,
        .isFrequency = 1,
        .sampleData = &sampleData.pmqosCpuMinFreq
    },
    {
        .atraceFlag = ATRACE_CPU_LIMIT,
        .nodePath = MIN_ONLINE_CPU_NODE,
        .atraceName = MIN_ONLINE_CPU_ATRACE,
        .isFrequency = 0,
        .sampleData = &sampleData.pmqosMinOnlineCpu
    },
    {
        .atraceFlag = ATRACE_CPU_LIMIT,
        .nodePath = MAX_ONLINE_CPU_NODE,
        .atraceName = MAX_ONLINE_CPU_ATRACE,
        .isFrequency = 0,
        .sampleData = &sampleData.pmqosMaxOnlineCpu
    }
};

static void sampleLimits(void)
{
    unsigned int i;
    for (i = 0; i < ARRAYSIZE(limitInfo); i++) {
        LimitInfo* info = &limitInfo[i];
        // PMQoS values are stored in binary as an 4 byte int, so we cant use readValue
        FILE* f = fopen(info->nodePath, "r");
        if (f)
        {
            int limit;
            fread(&limit, sizeof(int), 1, f);
            fclose(f);

            // Only report if the values changed
            if ((info->atraceFlag & aTraceFlags) &&
                (limit != *(info->sampleData))) {
                *(info->sampleData) = limit;
                if (info->isFrequency) {
                    traceCounter(ATRACE_TAG_ALWAYS, info->atraceName, convertFrequency(limit, CONVERT_MHZ));
                } else {
                    traceCounter(ATRACE_TAG_ALWAYS, info->atraceName, limit);
                }
            }
        }
    }
}

static void atraceDefaults(void)
{
    int64_t value;

    if (aTraceFlags & (ATRACE_CPU_FREQUENCY | ATRACE_CPU_LIMIT))
    {
        if (readValue("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", &value))
        {
            if (aTraceFlags & ATRACE_CPU_FREQUENCY) {
                traceCounter(ATRACE_TAG_ALWAYS, CPU0_FREQ_ATRACE, convertFrequency(value, CONVERT_KHZ));
                traceCounter(ATRACE_TAG_ALWAYS, CPU0_FREQ_ATRACE, 0);
                if (sampleData.cpuCount >= 2)
                {
                    traceCounter(ATRACE_TAG_ALWAYS, CPU1_FREQ_ATRACE, convertFrequency(value, CONVERT_KHZ));
                    traceCounter(ATRACE_TAG_ALWAYS, CPU1_FREQ_ATRACE, 0);
                }
                if (sampleData.cpuCount >= 3)
                {
                    traceCounter(ATRACE_TAG_ALWAYS, CPU2_FREQ_ATRACE, convertFrequency(value, CONVERT_KHZ));
                    traceCounter(ATRACE_TAG_ALWAYS, CPU2_FREQ_ATRACE, 0);
                }
                if (sampleData.cpuCount >= 4)
                {
                    traceCounter(ATRACE_TAG_ALWAYS, CPU3_FREQ_ATRACE, convertFrequency(value, CONVERT_KHZ));
                    traceCounter(ATRACE_TAG_ALWAYS, CPU3_FREQ_ATRACE, 0);
                }
            }
            if (aTraceFlags & ATRACE_CPU_LIMIT) {
                traceCounter(ATRACE_TAG_ALWAYS, CPU_FREQ_MAX_ATRACE, convertFrequency(value, CONVERT_KHZ));
                traceCounter(ATRACE_TAG_ALWAYS, CPU_FREQ_MAX_ATRACE, 0);
                traceCounter(ATRACE_TAG_ALWAYS, CPU_FREQ_MIN_ATRACE, convertFrequency(value, CONVERT_KHZ));
                traceCounter(ATRACE_TAG_ALWAYS, CPU_FREQ_MIN_ATRACE, 0);
            }
        }
    }
    if (aTraceFlags & ATRACE_CPU_LIMIT) {
        traceCounter(ATRACE_TAG_ALWAYS, MIN_ONLINE_CPU_ATRACE, sampleData.cpuCount);
        traceCounter(ATRACE_TAG_ALWAYS, MIN_ONLINE_CPU_ATRACE, 0);
        traceCounter(ATRACE_TAG_ALWAYS, MAX_ONLINE_CPU_ATRACE, sampleData.cpuCount);
        traceCounter(ATRACE_TAG_ALWAYS, MAX_ONLINE_CPU_ATRACE, 0);
    }

    if (aTraceFlags & ATRACE_CPU_UTIL)
    {
        traceCounter(ATRACE_TAG_ALWAYS, CPU0_LOAD_ATRACE, 100);
        traceCounter(ATRACE_TAG_ALWAYS, CPU1_LOAD_ATRACE, 100);
        traceCounter(ATRACE_TAG_ALWAYS, CPU2_LOAD_ATRACE, 100);
        traceCounter(ATRACE_TAG_ALWAYS, CPU3_LOAD_ATRACE, 100);
        traceCounter(ATRACE_TAG_ALWAYS, CPU0_LOAD_ATRACE, 0);
        traceCounter(ATRACE_TAG_ALWAYS, CPU1_LOAD_ATRACE, 0);
        traceCounter(ATRACE_TAG_ALWAYS, CPU2_LOAD_ATRACE, 0);
        traceCounter(ATRACE_TAG_ALWAYS, CPU3_LOAD_ATRACE, 0);
    }

    if (aTraceFlags & ATRACE_GPU_UTIL)
    {
        traceCounter(ATRACE_TAG_ALWAYS, GPU_USAGE_ATRACE, 100);
        traceCounter(ATRACE_TAG_ALWAYS, GPU_USAGE_ATRACE, 0);
    }

    if (aTraceFlags & ATRACE_EMC_UTIL)
    {
         traceCounter(ATRACE_TAG_ALWAYS,EMC_USAGE_ATRACE, 100);
         traceCounter(ATRACE_TAG_ALWAYS,EMC_USAGE_ATRACE, 0);
    }


    if (aTraceFlags & (ATRACE_GPU_FREQUENCY | ATRACE_GPU_LIMIT))
    {
        char *p_grmax = NULL;
        if (chipId != T124_CHIPID)
        {
            p_grmax = "/sys/kernel/debug/clock/3d/max";
        }
        else
        {
            p_grmax = "/sys/kernel/debug/clock/gk20a.gbus/max";
        }

        if (readValue(p_grmax, &value))
        {
            if (aTraceFlags & ATRACE_GPU_FREQUENCY) {
                traceCounter(ATRACE_TAG_ALWAYS, GPU_FREQ_ATRACE, convertFrequency(value, CONVERT_MHZ));
                traceCounter(ATRACE_TAG_ALWAYS, GPU_FREQ_ATRACE, 0);
            }
            if (aTraceFlags & ATRACE_GPU_LIMIT) {
                traceCounter(ATRACE_TAG_ALWAYS, GPU_FREQ_MAX_ATRACE, convertFrequency(value, CONVERT_MHZ));
                traceCounter(ATRACE_TAG_ALWAYS, GPU_FREQ_MIN_ATRACE, convertFrequency(value, CONVERT_MHZ));
                traceCounter(ATRACE_TAG_ALWAYS, GPU_FREQ_MAX_ATRACE, 0);
                traceCounter(ATRACE_TAG_ALWAYS, GPU_FREQ_MIN_ATRACE, 0);
            }
        }
    }

    if (aTraceFlags & ATRACE_EMC_FREQUENCY)
    {
        if (readValue("/sys/kernel/debug/clock/emc/max", &value))
        {
            traceCounter(ATRACE_TAG_ALWAYS, EMC_FREQ_ATRACE, convertFrequency(value, CONVERT_MHZ));
            traceCounter(ATRACE_TAG_ALWAYS, EMC_FREQ_ATRACE, 0);
        }
    }

    if (aTraceFlags & ATRACE_VDE)
    {
        if (readValue("/sys/kernel/debug/clock/vde/max", &value))
        {
            traceCounter(ATRACE_TAG_ALWAYS, VDE_FREQ_ATRACE, convertFrequency(value, CONVERT_MHZ));
            traceCounter(ATRACE_TAG_ALWAYS, VDE_FREQ_ATRACE, 0);
        }
    }

    if (aTraceFlags & ATRACE_AVP)
    {
        if (readValue("/sys/kernel/debug/clock/avp.sclk/max", &value))
        {
            traceCounter(ATRACE_TAG_ALWAYS, AVP_FREQ_ATRACE, convertFrequency(value, CONVERT_MHZ));
            traceCounter(ATRACE_TAG_ALWAYS, AVP_FREQ_ATRACE, 0);
        }

        traceCounter(ATRACE_TAG_ALWAYS, AVP_USAGE_ATRACE, 100);
        traceCounter(ATRACE_TAG_ALWAYS, AVP_USAGE_ATRACE, 0);
    }
}

static void performSampling(int argc, char *argv[])
{
    uint64_t usecCounter = getCurrentUSec();
    uint64_t defaultTimestamp = usecCounter;
    uint64_t usageTimestamp = usecCounter;
    uint64_t dfsTimestamp = usecCounter;
    int haveDFS = 0;

    sampleData.cpuCount = countCPUs();
    sampleData.fpsMax = 60;

    while (1)
    {
        int enabledThisTick = 0;

        usecCounter = getCurrentUSec();

        // Sample usage data at 1ms intervals
        if ((usecCounter > usageTimestamp + 1000) && aTraceFlags)
        {
            // When we aren't in capture mode sample this value often so we can place
            // the min/max values for an atrace field right at the head of the graph
            if (!aTraceInCaptureMode)
            {
                int64_t value;

                if (readValue("/sys/kernel/debug/tracing/tracing_on", &value))
                {
                    if (value)
                    {
                        aTraceInCaptureMode = 1;
                        // If we don't wait atrace won't capture our max values properly
                        usleep(200 * 1000);
                        atraceDefaults();
                        enabledThisTick = 1;
                    }
                }
            }

            usageTimestamp = usecCounter;

            // DFS data is required to sample usage, so if we have yet to sample it do so now
            if (!haveDFS || enabledThisTick)
            {
                sampleDFS();
                haveDFS = 1;
            }

            sampleUsageAverages();
        }

        // Sample usage data at 16ms intervals
        if ((usecCounter > dfsTimestamp + 16000) && aTraceFlags)
        {
            dfsTimestamp = usecCounter;
            sampleDFS();
        }

        // Sample basic data at the requested rate
        if (((usecCounter > defaultTimestamp + 1000 * sleepMS) || enabledThisTick) || !aTraceFlags)
        {
            if (!enabledThisTick)
            {
                // When we are in capture mode we aren't as concerned with disabling right
                // when systrace ends, so sample this value less (1HZ)
                if (aTraceInCaptureMode)
                {
                    int64_t value;

                    if (readValue("/sys/kernel/debug/tracing/tracing_on", &value))
                    {
                        if (!value)
                        {
                            aTraceInCaptureMode = 0;
                        }
                    }
                }
            }

            defaultTimestamp = usecCounter;
            sampleCPU();
            sampleRAM();
            sampleCarveout();
            sampleEDP();
            sampleTemps(argc, argv);
            sampleExtras(argc, argv);
            sampleThrottleCount();
            sampleGART();
            sampleIRAM();
            sampleBuddyInfo();
            samplePBC();
            sampleLimits();

            if (!aTraceFlags)
            {
                sampleDFS();
                sampleUsageAverages();
            }

            printReport();
            logFlush();
        }

        enabledThisTick = 0;

        if (bOnce)
          break;

        if (aTraceFlags)
        {
            usleep(USECS_PER_SAMPLE);
        }
        else
        {
            usleep(1000 * sleepMS);
        }
    }
}

int main (int argc, char *argv[])
{
    int i;
    int bStart = 0;
    int bStop  = 0;
    int pid    = 0;
    char logName[256] = {0};

    aTraceFlags = 0;

    memset(&sampleData, 0, sizeof(SamplingInfo));
    sampleData.largestFreeGARTBlockkB = -1;

    for (i = 1; i < argc; i++)
    {
        if (argv && argv[i])
        {
            LOGE("argv[%d] = %s\n", i, argv[i]);

            if (argv[i][0] == '-')
            {
                if (!strcmp(argv[i], "-max"))
                {
                    setFreq(1);
                    LOGI("Set all components to max frequency");
                    return 0;
                }
                else if (!strcmp(argv[i], "-default"))
                {
                    setFreq( 0);
                    return 0;
                }
                else if (!strcmp(argv[i], "--start"))
                {
                    bStart = 1;
                }
                else if (!strcmp(argv[i], "--stop"))
                {
                    bStop = 1;
                }
                else if (!strcmp(argv[i], "--logfile"))
                {
                    if ((i+1) <argc)
                    {
                        strcpy(logName, argv[i+1]);
                        i++;
                        bToLog = 1;
                    }
                }
                else if (!strcmp(argv[i], "-once"))
                {
                    bOnce = 1;
                }
                else if (!strcmp(argv[i], "-throttle"))
                {
                    bThrottle = 1;
                }
                else if (!strcmp(argv[i], "--systrace"))
                {
                    aTraceFlags |= ATRACE_GPU_UTIL |
                                  ATRACE_GPU_FREQUENCY |
                                  ATRACE_CPU_UTIL |
                                  ATRACE_EMC_UTIL |
                                  ATRACE_EMC_FREQUENCY |
                                  ATRACE_EDP |
                                  ATRACE_PBC;
                }
                else if (!strcmp(argv[i], "--systrace-temp")) {
                    aTraceFlags |= ATRACE_TEMP | ATRACE_PBC;
                }
                else if (!strcmp(argv[i], "--systrace-limits")) {
                    aTraceFlags |= ATRACE_CPU_LIMIT | ATRACE_GPU_LIMIT | ATRACE_PBC;
                }
            }
            else
            {
                sscanf(argv[1], "%d", &sleepMS);
                if (sleepMS < 100)
                    sleepMS = 100;
            }
        }
    }

    if (bStop)
    {
        if ((pid = get_tegrastats_pid()) > 0)
        {
            kill((pid_t) pid, SIGTERM);
        }
        return 0;
    }
    else if (bStart)
    {
        // run in background
        pid = fork();
        if (pid > 0)
        {
            // parent exit now..
            exit(0);
        }
        else if (pid == 0)
        {
            setsid();
            setpgrp();
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            signal(SIGINT, signal_handler);
            signal(SIGTERM, signal_handler);
        }
        else
        {
            // print warning, but do not exit..
            LOGE("failed to fork a child process \n");
        }
    }

    // make sure one copy is running
    if ((pid = get_tegrastats_pid()) > 0)
    {
        fprintf(stderr, "can't run another as instance (pid = %d) exists\n", pid);
        exit(1);
    }

    if (strlen(logName))
    {
        if ((logFile = fopen(logName, "a")) == NULL)
        {
            LOGE("failed to open %s \n", logName);
            bToLog = 0;
        }
    }

    chipId = getChipId();

    switch(chipId)
    {
        case AP20_CHIPID:
            chipType = AP20;
            break;
        case T30_CHIPID:
            chipType = T30;
            break;
        case T114_CHIPID:
            chipType = T114;
            break;
        case T148_CHIPID:
            chipType = T148;
            break;
        case T124_CHIPID:
            chipType = T124;
            break;
        default:
            LOGE("failed to identify chipType for chipID %i\n", chipId);
            break;
    }

    performSampling(argc, argv);

    return 0;
}

/* example contents of /proc/meminfo
MemTotal:         450164 kB
MemFree:          269628 kB
Buffers:            2320 kB
Cached:            69008 kB
SwapCached:            0 kB
Active:            89476 kB
Inactive:          63612 kB
Active(anon):      82272 kB
Inactive(anon):        0 kB
Active(file):       7204 kB
Inactive(file):    63612 kB
Unevictable:           0 kB
Mlocked:               0 kB
SwapTotal:             0 kB
SwapFree:              0 kB
Dirty:                 0 kB
Writeback:             0 kB
AnonPages:         81764 kB
Mapped:            35148 kB
Slab:               5204 kB
SReclaimable:       1760 kB
SUnreclaim:         3444 kB
PageTables:         4316 kB
NFS_Unstable:          0 kB
Bounce:                0 kB
WritebackTmp:          0 kB
CommitLimit:      225080 kB
Committed_AS:    1054316 kB
VmallocTotal:     450560 kB
VmallocUsed:       45964 kB
VmallocChunk:     340056 kB

http://www.linuxweblog.com/meminfo
    * MemTotal: Total usable ram (i.e. physical ram minus a few reserved bits
      and the kernel binary code)
    * MemFree: Is sum of LowFree+HighFree (overall stat)
    * MemShared: 0 is here for compat reasons but always zero.
    * Buffers: Memory in buffer cache. mostly useless as metric nowadays
    * Cached: Memory in the pagecache (diskcache) minus SwapCache
    * SwapCache: Memory that once was swapped out, is swapped back in but still
      also is in the swapfile (if memory is needed it doesn't need to be swapped
      out AGAIN because it is already in the swapfile. This saves I/O)

VM splits the cache pages into "active" and "inactive" memory. The idea is that
if you need memory and some cache needs to be sacrificed for that, you take it
from inactive since that's expected to be not used. The vm checks what is used
on a regular basis and moves stuff around. When you use memory, the CPU sets a
bit in the pagetable and the VM checks that bit occasionally, and based on that,
it can move pages back to active. And within active there's an order of "longest
ago not used" (roughly, it's a little more complex in reality).
    * Active: Memory that has been used more recently and usually not reclaimed
      unless absolutely necessary.
    * Inact_dirty: Dirty means "might need writing to disk or swap." Takes more
      work to free. Examples might be files that have not been written to yet.
      They aren't written to memory too soon in order to keep the I/O down. For
      instance, if you're writing logs, it might be better to wait until you have
      a complete log ready before sending it to disk.
    * Inact_clean: Assumed to be easily freeable. The kernel will try to keep
      some clean stuff around always to have a bit of breathing room.
    * Inact_target: Just a goal metric the kernel uses for making sure there are
      enough inactive pages around. When exceeded, the kernel will not do work to
      move pages from active to inactive. A page can also get inactive in a few
      other ways, e.g. if you do a long sequential I/O, the kernel assumes you're
      not going to use that memory and makes it inactive preventively. So you can
      get more inactive pages than the target because the kernel marks some cache
      as "more likely to be never used" and lets it cheat in the "last used"
      order.
    * HighTotal: is the total amount of memory in the high region. Highmem is all
      memory above (approx) 860MB of physical RAM. Kernel uses indirect tricks to
      access the high memory region. Data cache can go in this memory region.
    * LowTotal: The total amount of non-highmem memory.
    * LowFree: The amount of free memory of the low memory region. This is the
      memory the kernel can address directly. All kernel datastructures need to go
      into low memory.
    * SwapTotal: Total amount of physical swap memory.
    * SwapFree: Total amount of swap memory free.
    * Committed_AS: An estimate of how much RAM you would need to make a 99.99%
      guarantee that there never is OOM (out of memory) for this workload. Normally
      the kernel will overcommit memory. The Committed_AS is a guesstimate of how
      much RAM/swap you would need worst-case.
*/
