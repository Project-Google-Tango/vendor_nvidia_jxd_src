/*
 * Copyright (c) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AOS_AVP_PMU_H
#define INCLUDED_AOS_AVP_PMU_H

#include "nvcommon.h"

/* Writes the data to pmu
 *
 * @param Addr Slave address of pmu
 * @param Offset Internal offset of pmu
 * @param Data 8 bit data to be written
 */
void NvBlPmuWrite(NvU16 Addr, NvU8 Offset, NvU8 Data);

/* Reads up to 32 bits from pmu
 *
 * @param Addr Slave address of pmu
 * @param NumBytes Number of bytes to be read (max 4 bytes)
 *
 * @return value read from pmu
 */
NvU32 NvBlPmuRead(NvU16 Addr, NvU8 Offset, NvU8 NumBytes);

/* Closes the open pmu slaves */
void NvBlDeinitPmuSlaves(void);

#endif
