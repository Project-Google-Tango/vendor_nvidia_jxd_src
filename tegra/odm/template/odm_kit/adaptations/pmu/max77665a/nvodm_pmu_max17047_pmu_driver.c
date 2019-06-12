/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_max17047_pmu_driver.h"

#define MAX17047_I2C_SPEED 400

#define MAX17047_PMUGUID NV_ODM_GUID('m','a','x','1','7','0','4','7')

NvU16 max17047_chartable[] = {
    /* Data to be written from 0x80h */
    0x9380, 0xAB70, 0xAFA0, 0xB3E0, 0xB790, 0xBB40,
    0xBBD0, 0xBC70, 0xBD90, 0xBE30, 0xC0F0, 0xC380,
    0xC710, 0xCA90, 0xCF70, 0xD480,
    /* Data to be written from 0x90h */
    0x00B0, 0x0610, 0x0600, 0x06F0, 0x0700, 0x2410,
    0x2040, 0x2460, 0x1CE0, 0x09F0, 0x0AB0, 0x08E0,
    0x0880, 0x06F0, 0x05D0, 0x05D0,
    /* Data to be written from 0xA0h */
    0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
    0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
    0x0100, 0x0100, 0x0100, 0x0100};

typedef struct Max17047coreDriverRec {
    NvBool Init;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
} Max17047CoreDriver;

static Max17047CoreDriver s_Max17047CoreDriver = {NV_FALSE, NULL, 0};

static struct max17042_config_data config_data;


static NvBool
GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(MAX17047_PMUGUID);
    if (pConnectivity)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_I2c)
            {
                *pI2cModule   = NvOdmIoModule_I2c;
                *pI2cInstance = pConnectivity->AddressList[i].Instance;
                *pI2cAddress  = pConnectivity->AddressList[i].Address;
                return NV_TRUE;
            }
        }
    }
    NvOdmOsPrintf("%s:The system did not find FUEL GAUGE from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

static void Max17047ConfigDataInit(void)
{
    config_data.valrt_thresh = 0xff00;
    config_data.talrt_thresh = 0xff00;
    config_data.soc_alrt_thresh = 0xff00;
    config_data.shdntimer = 0xe000;
    config_data.design_cap = 0x1085;
    config_data.at_rate = 0x0000;
    config_data.tgain = NTC_10K_TGAIN;
    config_data.toff = NTC_10K_TOFF;
    config_data.vempty = 0x93DA;
    config_data.qrtbl00 = 0x2184;
    config_data.qrtbl10 = 0x1300;
    config_data.qrtbl20 = 0x0c00;
    config_data.qrtbl30 = 0x0880;
    config_data.full_soc_thresh = 0x5A00;
    config_data.rcomp0 = 0x0052;
    config_data.tcompc0 = 0x1F2D;
    config_data.ichgt_term = 0x0140;
    config_data.temp_nom = 0x1400;
    config_data.temp_lim = 0x2305;
    config_data.filter_cfg = 0x87A4;
    config_data.config = 0x2210;
    config_data.learn_cfg = 0x2606;
    config_data.misc_cfg = 0x0810;
    config_data.fullcap =  0x1085;
    config_data.fullcapnom = 0x1085;
    config_data.lavg_empty = 0x1000;
    config_data.dqacc = 0x01f4;
    config_data.dpacc = 0x3200;
    config_data.fctc = 0x05e0;
    config_data.kempty0 = 0x0600;
}

static NvU16
Max17047ByteReverse(NvU16 Data)
{
    NvU16 Datal;
    NvU16 Datah;

    Datal = (Data & RIGHTMASK) << 8;
    Datah = (Data & LEFTMASK) >> 8;

    return (Datal + Datah);
}

static NvBool
Max17047Write16(NvOdmPmuI2cHandle handle, NvU8 Reg, NvU16 Data)
{
    NvBool status;
    NvU16 Data1;

    Data1 = Max17047ByteReverse(Data);
    status = NvOdmPmuI2cWrite16(handle, s_Max17047CoreDriver.DeviceAddr,
                                        MAX17047_I2C_SPEED, Reg, Data1);
    if (!status)
    {
        NvOdmOsPrintf("%s: Failed to write the register %x\n", __func__, Reg);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
Max17047Read16(NvOdmPmuI2cHandle handle, NvU8 Reg, NvU16 *val)
{
    NvBool status;
    NvU16 value = 0;
    status = NvOdmPmuI2cRead16(handle, s_Max17047CoreDriver.DeviceAddr,
                                        MAX17047_I2C_SPEED, Reg, &value);
    if (!status)
    {
         NvOdmOsPrintf("%s: Failed to read the register %x\n", __func__, Reg);
         return NV_FALSE;
    }
    *val = Max17047ByteReverse(value);
    return NV_TRUE;
}

static void
OverridePor(NvOdmPmuI2cHandle hOdmPmuI2c, NvU8 Reg, NvU16 Data)
{
    if (Data) {
        if (!Max17047Write16(hOdmPmuI2c, Reg, Data))
            NvOdmOsPrintf("%s: Transaction Failed\n", __func__);
    }
}

static NvBool
Max17047InitConfiguration(NvOdmPmuI2cHandle handle)
{
    if ((!Max17047Write16(handle, MAX17047_CONFIG, config_data.config)) & //write config
        (!Max17047Write16(handle, MAX17047_LEARNCFG, config_data.learn_cfg)) & //learncfg
        (!Max17047Write16(handle, MAX17047_FILTERCFG, config_data.filter_cfg)) & //filter cfg
        (!Max17047Write16(handle, MAX17047_FULLSOCTHR, config_data.full_soc_thresh)))//write fullsoc
    {
        NvOdmOsPrintf("%s: Failed to Init Config\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
Max17047UnLockModel(NvOdmPmuI2cHandle handle)
{
    if ((!Max17047Write16(handle, MAX17047_UNLOCK1, 0x0059)) &
        (!Max17047Write16(handle, MAX17047_UNLOCK2, 0x00c4)))
    {
        NvOdmOsPrintf("%s: Failed to Un-lock Access\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static void
Max17047LockModel(NvOdmPmuI2cHandle handle)
{
    if ((!Max17047Write16(handle, MAX17047_UNLOCK1, 0x0000)) &
        (!Max17047Write16(handle, MAX17047_UNLOCK2, 0x0000)))
        NvOdmOsPrintf("%s: Failed to lock Access\n", __func__);
}

static NvBool
Max17047ReadToCompareModeldata(NvOdmPmuI2cHandle handle)

{
    NvU32 i;
    NvU16 Val = 0;
    NvU16 Value[MAX17042_CHARACTERIZATION_DATA_SIZE];

    for (i=0; i < MAX17042_CHARACTERIZATION_DATA_SIZE; i++)
    {
         if((!Max17047Read16(handle, MAX17047_MODELCHRTBL + i, &Val))) {
            NvOdmOsPrintf("%s: Error in Reading Model Reg %x\n", __func__,
                                                  (MAX17047_MODELCHRTBL + i));
            return NV_FALSE;
         }
         Value[i] = Val;
    }
    for (i=0; i < MAX17042_CHARACTERIZATION_DATA_SIZE; i++)
    {
         if(Value[i] != max17047_chartable[i])
            return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
Max17047VerifyLock(NvOdmPmuI2cHandle handle)
{
    NvU32 i;
    NvU16 Val;
    NvU16 Value[MAX17042_CHARACTERIZATION_DATA_SIZE];

    for (i=0; i < MAX17042_CHARACTERIZATION_DATA_SIZE; i++)
    {
         if((!Max17047Read16(handle, MAX17047_MODELCHRTBL + i, &Val))) {
            NvOdmOsPrintf("%s: Error in Reading Model Reg %x\n", __func__, (MAX17047_MODELCHRTBL + i));
            return NV_FALSE;
         }
         Value[i] = Val;
    }
    for (i=0; i < MAX17042_CHARACTERIZATION_DATA_SIZE; i++)
    {
        if(Value[i])
           return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
Max17047WriteModelData(NvOdmPmuI2cHandle handle)
{
    NvU32 i;
    for (i=0; i < MAX17042_CHARACTERIZATION_DATA_SIZE; i++)
    {
         if((!Max17047Write16(handle, MAX17047_MODELCHRTBL + i, max17047_chartable[i])))
         {
            NvOdmOsPrintf("%s: Error in writing  Model Reg %x\n", __func__, (MAX17047_MODELCHRTBL + i));
            return NV_FALSE;
         }
    }
    return NV_TRUE;
}

static NvBool
Max17047WriteAndVerify(NvOdmPmuI2cHandle handle, NvU8 Addr, NvU16 Data)
{
    NvU16 Value;

    //write
    if ((!Max17047Write16(handle, Addr, Data)))
    {
        NvOdmOsPrintf("%s: Error in writing to the Reg\n", __func__);
        return NV_FALSE;
    }
    //Read
    if ((!Max17047Read16(handle, Addr, &Value)))
    {
        NvOdmOsPrintf("%s: Error in reading the Reg \n", __func__);
        return NV_FALSE;
    }
    //verify
    if (Value != Data)
    {
        NvOdmOsPrintf("%s: Verification of data Failed\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
Max17047WriteCustomParams(NvOdmPmuI2cHandle handle)
{
    if ((!Max17047WriteAndVerify(handle, MAX17047_RCOMP0, config_data.rcomp0)) & //RCOMP0
        (!Max17047WriteAndVerify(handle, MAX17047_TEMPCO, config_data.tcompc0)) & //Tempco
        (!Max17047WriteAndVerify(handle, MAX17047_VEMPTY, config_data.vempty)) & //Vempty
        (!Max17047WriteAndVerify(handle, MAX17047_QRTABLE00, config_data.qrtbl00)) &
        (!Max17047WriteAndVerify(handle, MAX17047_QRTABLE10, config_data.qrtbl10)) &
        (!Max17047WriteAndVerify(handle, MAX17047_QRTABLE20, config_data.qrtbl20)) &
        (!Max17047WriteAndVerify(handle, MAX17047_QRTABLE30, config_data.qrtbl30)) &
        (!Max17047Write16(handle, MAX17042_ICHGTERM, config_data.ichgt_term))) // IchgTerm
        return NV_FALSE;
    // Write tgain
    if ((!Max17047Write16(handle, MAX17047_TGAIN, config_data.tgain))) {
        NvOdmOsPrintf("%s: failed to write Tgain\n", __func__);
        return NV_FALSE;
    }
    // write toffset
    if ((!Max17047Write16(handle, MAX17047_TOFF, config_data.toff))) {
        NvOdmOsPrintf("%s: failed to write Toff\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
Max17047UpdateCapacity(NvOdmPmuI2cHandle handle)
{
    if ((!Max17047WriteAndVerify(handle, MAX17047_CAPACITY, config_data.fullcap)) &
        (!Max17047Write16(handle, MAX17047_DESIGNCAP, config_data.design_cap)) &
        (!Max17047WriteAndVerify(handle, MAX17047_FULLCAPNOM, config_data.fullcapnom)))
        return NV_FALSE;
    return NV_TRUE;
}

static NvBool
Max17047ResetVfSoc0(NvOdmPmuI2cHandle handle)
{
    NvU16 Value = 0;

    //Read Vfsoc
    if ((!Max17047Read16(handle, MAX17047_VFSOC, &Value))) {
        NvOdmOsPrintf("%s: Error in reading the VFSOC REg\n", __func__);
        return NV_FALSE;
    }
    //Enable write to Vfsoc0
    if ((!Max17047Write16(handle, MAX17047_VFSOC0EN, 0x0080))) {
        NvOdmOsPrintf("%s: Error in enabling write access\n", __func__);
        return NV_FALSE;
    }
    //write vfsoc to vfsoc0
    if (!Max17047WriteAndVerify(handle, MAX17047_VFSOC0, Value)) {
        NvOdmOsPrintf("%s: Error in writing to vfsoc0\n", __func__);
        return NV_FALSE;
    }
    //Disable Write access
    if ((!Max17047Write16(handle, MAX17047_VFSOC0EN, 0x0000))) {
        NvOdmOsPrintf("%s: Error in disabling write access\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
Max17047LoadNewCapacityParams(NvOdmPmuI2cHandle handle)
{
    NvU16 Vfsoc;
    NvU16 Vf_fullCap0;
    NvU16 RemCap;
    NvU16 dQ_acc;

    if ((!Max17047Read16(handle, MAX17047_VFSOC, &Vfsoc))) {
        NvOdmOsPrintf("%s: Error in Reading vfsoc reg\n", __func__);
        return NV_FALSE;
    }
    if ((!Max17047Read16(handle, MAX17047_FULLCAP0, &Vf_fullCap0))) {
        NvOdmOsPrintf("%s: Error in Reading full cap\n", __func__);
        return NV_FALSE;
    }
    RemCap = ((Vfsoc >> 8) * (Vf_fullCap0)) / 100;

    if (!Max17047WriteAndVerify(handle, MAX17047_REMCAP, RemCap)) {
        NvOdmOsPrintf("%s: Error in writing RemCap\n", __func__);
        return NV_FALSE;
    }
    if (!Max17047WriteAndVerify(handle, MAX17047_REPCAP, RemCap)) {
        NvOdmOsPrintf("%s: Error in writing RepCap\n", __func__);
        return NV_FALSE;
    }
    dQ_acc = (0x07d0) / dQ_ACC_DIV;

    if ((!Max17047WriteAndVerify(handle, MAX17047_DQACC, dQ_acc)) &
       (!Max17047WriteAndVerify(handle, MAX17047_DQACC, dP_ACC_200)) &
       (!Max17047WriteAndVerify(handle, MAX17047_CAPACITY, Vf_fullCap0)) &
       (!Max17047WriteAndVerify(handle, MAX17047_DESIGNCAP, config_data.design_cap)) &
       (!Max17047WriteAndVerify(handle, MAX17047_FULLCAPNOM, config_data.fullcapnom))) {
        NvOdmOsPrintf("%s: Error in updating capacity params\n", __func__);
        return NV_FALSE;
    }

    //Update SOC
    if ((!Max17047Write16(handle, MAX17047_VFSOC, Vfsoc))) {
        NvOdmOsPrintf("%s: Error in updating Soc\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static void
Max17047OverridePor(void)
{
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_TGAIN, config_data.tgain);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_TOFF, config_data.toff);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_VALRT, config_data.valrt_thresh);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_TALRT, config_data.talrt_thresh);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_SOCALRT, config_data.soc_alrt_thresh);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_CONFIG, config_data.config);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_SDNTMR, config_data.shdntimer);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_DESIGNCAP, config_data.design_cap);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17042_ICHGTERM, config_data.ichgt_term);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_ATRATE, config_data.at_rate);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_LEARNCFG, config_data.learn_cfg);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_FILTERCFG, config_data.filter_cfg);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_MISCCFG, config_data.misc_cfg);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_CAPACITY, config_data.fullcap);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_FULLCAPNOM, config_data.fullcapnom);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_LAVGEMPTY, config_data.lavg_empty);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_DQACC, config_data.dqacc);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_DPACC, config_data.dpacc);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_VEMPTY, config_data.vempty);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_TEMPNOM, config_data.temp_nom);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_TEMPLIM, config_data.temp_lim);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_FCTC, config_data.fctc);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_RCOMP0, config_data.rcomp0);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_TEMPCO, config_data.tcompc0);
    OverridePor(s_Max17047CoreDriver.hOdmPmuI2c, MAX17047_KEMPTY0, config_data.kempty0);
}

NvOdmFuelGaugeDriverInfoHandle
Max17047FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvBool Status;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvU16 Stat = 0;

    if (!s_Max17047CoreDriver.Init)
    {
        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
            return NULL;

        s_Max17047CoreDriver.hOdmPmuI2c =
                    (NvOdmPmuI2cHandle)NvOdmPmuI2cOpen(I2cModule, I2cInstance);
        if (!s_Max17047CoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error MAX17047 Open I2C device.\n", __func__);
            return NULL;
        }
        s_Max17047CoreDriver.DeviceAddr = I2cAddress;

        //InitConfig Data
        Max17047ConfigDataInit();

        // OverRide Por
        Max17047OverridePor();

        //Wait for 500ms
        NvOdmOsWaitUS(500*1000);

        //Initialize Config
        if (!Max17047InitConfiguration(s_Max17047CoreDriver.hOdmPmuI2c))
            return NULL;

        //Unlock Model Acess
        if (!Max17047UnLockModel(s_Max17047CoreDriver.hOdmPmuI2c))
            return NULL;

        //Write Characterization Data
        if (!Max17047WriteModelData(s_Max17047CoreDriver.hOdmPmuI2c))
            return NULL;

        //Compare and Verify Data
        if (!Max17047ReadToCompareModeldata(s_Max17047CoreDriver.hOdmPmuI2c))
            return NULL;

        //Lock Model
        do {
            Max17047LockModel(s_Max17047CoreDriver.hOdmPmuI2c);
        } while(!Max17047VerifyLock(s_Max17047CoreDriver.hOdmPmuI2c));//verify

        //Write Custom Parameters
        if (!Max17047WriteCustomParams(s_Max17047CoreDriver.hOdmPmuI2c))
            return NULL;

        //Update Full Capacity
        if (!Max17047UpdateCapacity(s_Max17047CoreDriver.hOdmPmuI2c))
            return NULL;

        //Wait for 350ms
        NvOdmOsWaitUS(350*1000);

        //Write VfSOC0
        if (!Max17047ResetVfSoc0(s_Max17047CoreDriver.hOdmPmuI2c))
            return NULL;

        //Load New Capacity Params
        if (!Max17047LoadNewCapacityParams(s_Max17047CoreDriver.hOdmPmuI2c))
            return NULL;

        //Clear POR
        if ((!Max17047Read16(s_Max17047CoreDriver.hOdmPmuI2c,
                                                            MAX17047_STATUS, &Stat)))
        {
            NvOdmOsPrintf("%s: Error in reading the STATUS reg\n", __func__);
            return NULL;
        }
        if (!Max17047WriteAndVerify(s_Max17047CoreDriver.hOdmPmuI2c,
                                                   MAX17047_STATUS, (Stat & 0xFFFD)))
        {
            NvOdmOsPrintf("%s: Error in clearing POR\n", __func__);
            return NULL;
        }
    }
    s_Max17047CoreDriver.Init = NV_TRUE;
    return &s_Max17047CoreDriver;
}

void
Max17047FuelGaugeDriverClose(NvOdmFuelGaugeDriverInfoHandle hfg)
{
    NV_ASSERT(hfg);
    if (!hfg)
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);

    s_Max17047CoreDriver.Init = NV_FALSE;
    NvOdmPmuI2cClose(s_Max17047CoreDriver.hOdmPmuI2c);
}

NvBool
Max17047FuelGaugeDriverGetData(NvOdmFuelGaugeDriverInfoHandle hfg,
                                          NvU16* voltage, NvU16* soc)
{
    NvOdmPmuI2cHandle handle = s_Max17047CoreDriver.hOdmPmuI2c;
    NvU16 vol;
    NvU16 Soc;

    NV_ASSERT(hfg);
    if (!hfg)
        NvOdmOsPrintf("%s(): Error in passing the handle\n", __func__);

    // read voltage
    if ((!Max17047Read16(handle, MAX17047_VCELL, &vol))) {
        NvOdmOsPrintf("%s: Error in reading the vol reg\n", __func__);
        return NV_FALSE;
    }

    // Read SOC
    if ((!Max17047Read16(handle, MAX17047_VFSOC, &Soc))) {
        NvOdmOsPrintf("%s: Error in reading the VfSOC reg\n", __func__);
        return NV_FALSE;
    }
    //voltage in mV
    vol = vol * 625/8000;
    //percent(%) SOC
    Soc = (Soc * 39/10000);

    *voltage = vol;
    *soc = Soc;
    return NV_TRUE;
}
