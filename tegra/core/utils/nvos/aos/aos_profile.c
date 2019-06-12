/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "aos.h"
#include "nvassert.h"

/** gprof profiling support */

/* This is copied from the gmon_out.h header file */
#define GMON_MAGIC      "gmon"  /* magic cookie */
#define GMON_VERSION    1       /* version number */

struct gmon_hdr
{
    char cookie[4];
    char version[4];
    char spare[3 * 4];
};

/* types of records in this file: */
typedef enum
{
    GMON_TAG_TIME_HIST = 0,
    GMON_TAG_CG_ARC = 1,
    GMON_TAG_BB_COUNT = 2
} GMON_Record_Tag;

struct gmon_hist_hdr
{
    char low_pc[sizeof (char *)];  /* base pc address of sample buffer */
    char high_pc[sizeof (char *)]; /* max pc address of sampled buffer */
    char hist_size[4];             /* size of sample buffer */
    char prof_rate[4];             /* profiling clock rate */
    char dimen[15];                /* phys. dim., usually "seconds" */
    char dimen_abbrev;             /* usually 's' for "seconds" */
};

/* start profiling */
#if !__GNUC__
extern unsigned int Image$$MainRegion$$Base; // from the linker
#else
extern unsigned int _start;
#endif

#define NVAOS_SAMPLE_POINTS (512*1024)

typedef struct AosProfileRec
{
    struct gmon_hdr hdr;
    char tagTime;
    struct gmon_hist_hdr hist_hdr;
    NvU16 prof_data[NVAOS_SAMPLE_POINTS];
} AosProfile;

AosProfile *s_prof;
NvBool s_ProfileEnabled = NV_FALSE;

void
nvaosProfTimerHandler( void )
{
    NvU32 excAddr;
    NvU32 startAddr;
    NvU32 lastAddr;

    if( s_ProfileEnabled == NV_FALSE )
    {
        return;
    }

    excAddr = nvaosGetExceptionAddr();
#if !__GNUC__
    startAddr = (NvU32)&Image$$MainRegion$$Base;
#else
    startAddr = (NvU32)&_start;
#endif
    
    lastAddr = startAddr + (NVAOS_SAMPLE_POINTS * 4) - 4;

    if( excAddr < startAddr || excAddr > lastAddr )
    {
        // may be executing from an aperture that's not profiled
        return; 
    }

    excAddr -= startAddr;
    excAddr = excAddr >> 2; // divide by 4

    s_prof->prof_data[excAddr]++;
}

static void
nvaosProfileFormatData( AosProfile *prof )
{
#if !__GNUC__
    NvU32 start_addr = (NvU32)&Image$$MainRegion$$Base;
#else
    NvU32 start_addr = (NvU32)&_start;
#endif    
    NvU32 end_addr   = start_addr + (NVAOS_SAMPLE_POINTS * 4);
    int num_buckets  = NVAOS_SAMPLE_POINTS;
    int version      = GMON_VERSION;
    int prof_rate = 1000; // totally arbitrary

    /* setup the gmon header */
    NvOsMemset(&prof->hdr, 0, sizeof(struct gmon_hdr));
    NvOsStrncpy(prof->hdr.cookie, GMON_MAGIC, NvOsStrlen(GMON_MAGIC));
    NvOsMemcpy(&prof->hdr.version, &version, sizeof(version));

    NvOsMemcpy(&prof->hist_hdr.low_pc, &start_addr, sizeof(start_addr));
    NvOsMemcpy(&prof->hist_hdr.high_pc, &end_addr, sizeof(end_addr));    
    NvOsMemcpy(&prof->hist_hdr.hist_size, &num_buckets, sizeof(num_buckets));
    NvOsMemcpy(&prof->hist_hdr.prof_rate, &prof_rate, sizeof(prof_rate));    
    NvOsStrncpy(prof->hist_hdr.dimen, "seconds", NvOsStrlen("seconds"));
    prof->hist_hdr.dimen_abbrev = 's';

    prof->tagTime = GMON_TAG_TIME_HIST;
}

void
NvOsProfileApertureSizes( NvU32 *apertures, NvU32 *sizes )
{
    NV_ASSERT( apertures );

    *apertures = 1;
    if( sizes )
    {
        *sizes = sizeof(AosProfile);
    }
}

void
NvOsProfileStart( void **apertures )
{
    nvaosDisableInterrupts();

    s_ProfileEnabled = NV_TRUE;
    s_prof = *(AosProfile **)apertures;
    NvOsMemset( s_prof, 0, sizeof(AosProfile) );

    nvaosEnableInterrupts();
}

void
NvOsProfileStop( void **apertures )
{
    nvaosDisableInterrupts();

    s_ProfileEnabled = NV_FALSE;
    nvaosProfileFormatData( s_prof );
    s_prof = 0;

    nvaosEnableInterrupts();
}

NvError
NvOsProfileWrite( NvOsFileHandle file, NvU32 index, void *aperture )
{
    NvError e;
    AosProfile *prof = (AosProfile *)aperture;

    NV_CHECK_ERROR_CLEANUP(
        NvOsFwrite( file, &prof->hdr, sizeof(prof->hdr) )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvOsFwrite( file, &prof->tagTime, sizeof(prof->tagTime) )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvOsFwrite( file, &prof->hist_hdr, sizeof(prof->hist_hdr) )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvOsFwrite( file, prof->prof_data, sizeof(prof->prof_data) )
    );

    NvOsFclose( file );

fail:
    return e;
}
