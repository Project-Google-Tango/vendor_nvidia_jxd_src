/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA ORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_MAX17047_CORE_DRIVER_H
#define INCLUDED_MAX17047_CORE_DRIVER_H

#include "nvodm_pmu.h"
#include "pmu/nvodm_fuel_gauge_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NTC_10K_TGAIN   0xBC94
#define NTC_10K_TOFF    0x84BA

struct max17042_config_data {
    NvU16 valrt_thresh;
    NvU16 talrt_thresh;
    NvU16 soc_alrt_thresh;
    NvU16 shdntimer;
    NvU16 design_cap;
    NvU16 at_rate;
    NvU16 tgain;
    NvU16 toff;
    NvU16 vempty;
    NvU16 qrtbl00;
    NvU16 qrtbl10;
    NvU16 qrtbl20;
    NvU16 qrtbl30;
    NvU16 full_soc_thresh;
    NvU16 rcomp0;
    NvU16 tcompc0;
    NvU16 ichgt_term;
    NvU16 temp_nom;
    NvU16 temp_lim;
    NvU16 filter_cfg;
    NvU16 config;
    NvU16 learn_cfg;
    NvU16 misc_cfg;
    NvU16 fullcap;
    NvU16 fullcapnom;
    NvU16 lavg_empty;
    NvU16 dqacc;
    NvU16 dpacc;
    NvU16 fctc;
    NvU16 kempty0;
};

#define MAX17042_CHARACTERIZATION_DATA_SIZE 48
#define dQ_ACC_DIV      4
#define dP_ACC_100      0x1900
#define dP_ACC_200      0x3200

#define RIGHTMASK 0x00ff
#define LEFTMASK 0xff00

#define MAX17047_STATUS 0x00
#define MAX17047_VALRT 0x01
#define MAX17047_TALRT 0x02
#define MAX17047_SOCALRT 0x03
#define MAX17047_ATRATE 0x04
#define MAX17047_REPCAP 0x05
#define MAX17047_REPSOC 0x06
#define MAX17047_TEMP 0x08
#define MAX17047_VCELL 0X09
#define MAX17047_REMCAP 0x0F
#define MAX17047_CAPACITY 0x10
#define MAX17047_TTE 0x11
#define MAX17047_QRTABLE00 0x12
#define MAX17047_FULLSOCTHR 0x13
#define MAX17047_CYCLES 0x17
#define MAX17047_DESIGNCAP 0x18
#define MAX17047_CONFIG 0x1D
#define MAX17042_ICHGTERM 0x1E
#define MAX17047_QRTABLE10 0x22
#define MAX17047_FULLCAPNOM 0x23
#define MAX17047_TEMPNOM 0x24
#define MAX17047_TEMPLIM 0x25
#define MAX17047_LEARNCFG 0x28
#define MAX17047_FILTERCFG 0x29
#define MAX17047_MISCCFG 0x2B
#define MAX17047_TGAIN 0x2C
#define MAX17047_TOFF 0x2D
#define MAX17047_QRTABLE20 0x32
#define MAX17047_FULLCAP0 0x35
#define MAX17047_LAVGEMPTY 0x36
#define MAX17047_FCTC 0x37
#define MAX17047_RCOMP0 0x38
#define MAX17047_TEMPCO 0x39
#define MAX17047_VEMPTY 0x3A
#define MAX17047_KEMPTY0 0x3B
#define MAX17047_SDNTMR 0x3F
#define MAX17047_QRTABLE30 0x42
#define MAX17047_DQACC 0x45
#define MAX17047_DPACC 0x46
#define MAX17047_VFSOC0 0x48
#define MAX17047_QH0 0x4C
#define MAX17047_QH 0x4D
#define MAX17047_VFSOC0EN 0x60
#define MAX17047_UNLOCK1 0x62
#define MAX17047_UNLOCK2 0x63
#define MAX17047_VFSOC 0xFF
#define MAX17047_MODELCHRTBL 128

NvOdmFuelGaugeDriverInfoHandle
Max17047FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice);

void
Max17047FuelGaugeDriverClose(NvOdmFuelGaugeDriverInfoHandle hfg);

//Reads the fuel gauge registers and outputs the value of the current cell
//voltage and the State Of Charge(SOC) in terms of millivolts and percent(%)
//respectively.
NvBool
Max17047FuelGaugeDriverGetData(NvOdmFuelGaugeDriverInfoHandle hfg, NvU16* voltage, NvU16* soc);

#if defined(__cplusplus)
}
#endif

#endif
