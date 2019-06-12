/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: CW2015-driver Interface</b>
  *
  * @b Description: Implements the CW2015 gas guage drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_pmu_cw2015_fuel_gauge_driver.h"

#ifdef NV_USE_NCT
#include "nvnct.h"
#include "nct.h"
#endif

#define CW2015_PMUGUID NV_ODM_GUID('c','w','2','0','1','5','*','*')
#define CW2015_I2C_SPEED  100

#define CW2015_VER        0x00
#define CW2015_VCELL      0x02
#define CW2015_SOC        0x04
#define CW2015_RRT_ALERT  0x06
#define CW2015_CONFIG     0x08
#define CW2015_MODE       0x10
#define CW2015_BATINFO    0x0A
#define CW2015_BATINFO_SIZE      0x40
#define CW2015_START_NORMAL      0x0
#define CW2015_POWERON_RESET     0x0F
#define CONFIG_UPDATE_FLAG       0x02

static void CW2015ReadBatteryProfileData(NvU8 *battery_data)
{
#ifdef NV_USE_NCT
    NvError err;
    nct_item_type  buf;

    err = NvNctReadItem(NCT_ID_BATTERY_MODEL_DATA, &buf);

    if (NvSuccess == err)
    {
        NvOdmOsPrintf("Initialise battery profile data from NCT \n");
        NvOsMemcpy(battery_data, buf.battery_mdata.cw2015_pdata.u8,
               CW2015_BATINFO_SIZE);
        return;
    }
#endif
    NvOdmOsPrintf("Default values are used for Battery profile data\n");
    return;
}

typedef struct CW2015DriverRec {
    NvU32 OpenCount;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddr;
} CW2015CoreDriver;

static CW2015CoreDriver s_CW2015CoreDriver = {0, NULL, 0};

static NvBool
GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(CW2015_PMUGUID);
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
    NvOdmOsPrintf("%s: The system did not discover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmFuelGaugeDriverInfoHandle
CW2015FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice)
{
    NvBool Status = NV_FALSE;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvU8  mode = 0;

    if (!s_CW2015CoreDriver.OpenCount)
    {
        Status = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
        if (!Status)
            return NULL;

        NvOdmOsMemset(&s_CW2015CoreDriver, 0, sizeof(CW2015CoreDriver));
        s_CW2015CoreDriver.hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);

        if (!s_CW2015CoreDriver.hOdmPmuI2c)
        {
            NvOdmOsPrintf("%s: Error CW2015 Open I2C device.\n", __func__);
            return NULL;
        }
        s_CW2015CoreDriver.DeviceAddr = I2cAddress;

        //Read FG mode
        if (!NvOdmPmuI2cRead8(s_CW2015CoreDriver.hOdmPmuI2c,
                   s_CW2015CoreDriver.DeviceAddr,
                   CW2015_I2C_SPEED, CW2015_MODE, &mode))
        {
             NvOdmOsPrintf("%s: Error CW2015 mode read failed.\n", __func__);
             return NULL;
        }

        //Check whether the system is in Sleep mode.
        if((mode & 0xC0) == 0xC0)
        {
            mode = 0;
            //Write the mode register to bring FG to normal mode
            if (!(NvOdmPmuI2cWrite8(s_CW2015CoreDriver.hOdmPmuI2c,
                   s_CW2015CoreDriver.DeviceAddr,
                   CW2015_I2C_SPEED, CW2015_MODE, mode)))
            {
                  NvOdmOsPrintf("%s: Error in writting CW2015 mode.\n", __func__);
                  return NULL;
            }
            NvOdmOsWaitUS(1);
        }
    }
    s_CW2015CoreDriver.OpenCount++;
    return &s_CW2015CoreDriver;
}

void
CW2015FuelGaugeDriverClose(void)
{
    if (!s_CW2015CoreDriver.OpenCount)
        return;
    if (s_CW2015CoreDriver.OpenCount == 1)
    {
        NvOdmPmuI2cClose(s_CW2015CoreDriver.hOdmPmuI2c);
        NvOdmOsMemset(&s_CW2015CoreDriver, 0, sizeof(CW2015CoreDriver));
    }
    s_CW2015CoreDriver.OpenCount--;
}

NvBool
CW2015FuelGaugeDriverReadSoc(NvU32 *pSoc, NvU32 *pVoltage)
{
    NvU8  soc = 0;
    NvU16 vcell;
    static NvBool FGprofileconfig = NV_FALSE;

    NV_ASSERT(pSoc);
    NV_ASSERT(pVoltage);
    if (!pSoc || !pVoltage)
        return NV_FALSE;

   //Check CW2015 FG is accessible
    if (!s_CW2015CoreDriver.hOdmPmuI2c)
    {
         NvOdmOsPrintf("%s: Error CW2015 Open I2C device.\n", __func__);
         return NV_FALSE;
    }

    //Read battery SOC vaue after configuring battery profile
    if (!FGprofileconfig)
    {
        CW2015FuelGaugeConfigbatteryProfile();
        FGprofileconfig  = NV_TRUE;
    }
    //Read the SOC value in percentage
    if (!NvOdmPmuI2cRead8(s_CW2015CoreDriver.hOdmPmuI2c,
                s_CW2015CoreDriver.DeviceAddr,
                CW2015_I2C_SPEED, CW2015_SOC, &soc))
    {
        NvOdmOsPrintf("%s: Error CW2015 soc read failed.\n", __func__);
        *pSoc = 0;
        return NV_FALSE;
    }
    *pSoc = soc;

    //Read the voltage value
    if (!NvOdmPmuI2cRead16(s_CW2015CoreDriver.hOdmPmuI2c,
                s_CW2015CoreDriver.DeviceAddr,
                CW2015_I2C_SPEED, CW2015_VCELL, &vcell))
    {
        NvOdmOsPrintf("%s: Error CW2015 vcell read failed.\n", __func__);
        *pVoltage = 0;
        return NV_FALSE;
    }
    *pVoltage = (vcell * 312) / 1024;
    NvOdmOsPrintf("CW2015 battery voltage %d \n", *pVoltage);
    return NV_TRUE;
}

NvBool CW2015FuelGaugeConfigbatteryProfile(void)
{
    NvU32 i = 0;
    NvU8  regval;
    NvU8  battery_info[CW2015_BATINFO_SIZE] = {0x15,0x7E,0x64,0x63,0x60,0x5A,
                                               0x53,0x50,0x4E,0x4B,0x49,0x46,
                                               0x48,0x43,0x2F,0x22,0x18,0x0F,
                                               0x0D,0x12,0x21,0x36,0x48,0x58,
                                               0x4F,0xA2,0x08,0xF6,0x1D,0x3B,
                                               0x46,0x4C,0x59,0x5C,0x5C,0x60,
                                               0x3F,0x1B,0x6C,0x45,0x26,0x41,
                                               0x20,0x56,0x86,0x95,0x96,0x0E,
                                               0x45,0x6A,0x96,0xC1,0x80,0xB8,
                                               0xF0,0xCB,0x2F,0x7D,0x72,0xA5,
                                               0xB5,0xC1,0x5B,0x1D};
    CW2015ReadBatteryProfileData(battery_info);

    for (i = 0; i < CW2015_BATINFO_SIZE; i++)
    {
        if (!(NvOdmPmuI2cWrite8(s_CW2015CoreDriver.hOdmPmuI2c,
                   s_CW2015CoreDriver.DeviceAddr,
                   CW2015_I2C_SPEED, CW2015_BATINFO + i, battery_info[i])))
            {
              NvOdmOsPrintf("%s: Error in writting Battery profile data\n", __func__);
              return NV_FALSE;
            }
    }

    /* read the profile data and compare with the NCT data */
    for (i = 0; i < CW2015_BATINFO_SIZE; i++)
    {

         if (!NvOdmPmuI2cRead8(s_CW2015CoreDriver.hOdmPmuI2c,
                s_CW2015CoreDriver.DeviceAddr,
                CW2015_I2C_SPEED, CW2015_BATINFO + i, &regval))
         {
              NvOdmOsPrintf("%s: Error in writting Battery profile data\n", __func__);
              return NV_FALSE;
         }
         if (regval != battery_info[i])
             return NV_FALSE;
    }
        /* set config update flag to use new battery info */
    if (!NvOdmPmuI2cRead8(s_CW2015CoreDriver.hOdmPmuI2c,
           s_CW2015CoreDriver.DeviceAddr,
           CW2015_I2C_SPEED, CW2015_BATINFO + i, &regval))
    {
         NvOdmOsPrintf("%s: Error in reading CW2015 Config register\n", __func__);
         return NV_FALSE;
    }
    regval |= CONFIG_UPDATE_FLAG;   /* set UPDATE_FLAG */
    if (!(NvOdmPmuI2cWrite8(s_CW2015CoreDriver.hOdmPmuI2c,
            s_CW2015CoreDriver.DeviceAddr,
            CW2015_I2C_SPEED, CW2015_CONFIG, regval)))
    {
          NvOdmOsPrintf("%s: Error in writting CW2015 Config register\n", __func__);
          return NV_FALSE;
    }
    // Reset FG to use this updated battery profile info
    if (!(NvOdmPmuI2cWrite8(s_CW2015CoreDriver.hOdmPmuI2c,
            s_CW2015CoreDriver.DeviceAddr,
            CW2015_I2C_SPEED, CW2015_MODE, CW2015_POWERON_RESET)))
    {
          NvOdmOsPrintf("%s: Error in writting CW2015 mode register\n", __func__);
          return NV_FALSE;
    }
    NvOdmOsSleepMS(10);
    if (!(NvOdmPmuI2cWrite8(s_CW2015CoreDriver.hOdmPmuI2c,
            s_CW2015CoreDriver.DeviceAddr,
            CW2015_I2C_SPEED, CW2015_MODE, CW2015_START_NORMAL)))
    {
          NvOdmOsPrintf("%s: Error in writting CW2015 mode register\n", __func__);
          return NV_FALSE;
    }
    return NV_TRUE;
  }
