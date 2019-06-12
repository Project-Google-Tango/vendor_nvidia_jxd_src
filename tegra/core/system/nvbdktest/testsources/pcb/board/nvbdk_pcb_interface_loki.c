/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbdk_pcb_interface.h"
#include "nvbdk_pcb_api.h"

/* For DVT1 and DVT2 */
#define SANDISK_EMMC_CID_MID_CBX_OID 0x00450100
#define SANDISK_EMMC_CID_PRODUCT_NAME_1 0x53454d33
#define SANDISK_EMMC_CID_PRODUCT_NAME_2 0x32470000

/* pcb:emmc:xxxxx */
static NvError EmmcValidationFunc(void *pInfo)
{
    NvError e = NvError_BadParameter;;
    NvU32 cid[4];

    if (pInfo)
    {
        NvOsMemcpy(&cid, pInfo, sizeof(cid));
        if (((cid[3] & 0x00FFFFFF) == SANDISK_EMMC_CID_MID_CBX_OID)
            && (cid[2] == SANDISK_EMMC_CID_PRODUCT_NAME_1)
            && ((cid[1] & 0xFFFF0000) == SANDISK_EMMC_CID_PRODUCT_NAME_2))
        {
            e  = NvSuccess;
        }
        else
        {
            NvOsDebugPrintf("EMMC_CID_MID_CBX_OID    [0x%X]"
                            "EMMC_CID_PRODUCT_NAME_1 [0x%X]"
                            "EMMC_CID_PRODUCT_NAME_2 [0x%X]\n",
                            cid[3] & 0x00FFFFFF, cid[2], cid[1] & 0xFFFF0000);
        }
    }
    return e;
}

/* pcb:emmc:xxxxx */
static EmmcTestPrivateData sEmmcData = {
    3, EmmcValidationFunc
};

/* pcb:osc:xxxxx */
static OscTestPrivateData sOscData = {
    500, 100
};

/* pcb:pmu:xxxxx */
static NvU8 sTPS65913_I2cWBuf[] = { 0x57 };
static I2cTestPrivateData sI2cTPS65913[] = {
    { 4, 0x5A, _I2C_CMD_WRITE_, {1, sTPS65913_I2cWBuf}, {0, NULL} },
    { 4, 0x5A, _I2C_CMD_READ_,  {1, NULL}, {0, NULL} },
    { 0, 0x00, _I2C_CMD_END_,   {0, NULL}, {0, NULL} },
};

/* default data table */
static PcbTestData sPcbTestDataList_Default[] = {
    { "osc", "osc_test",   NvApiOscCheck, (void *)&sOscData },
    { "emmc", "emmc_test", NvApiEmmcCheck , (void *)&sEmmcData},
    { "pmu", "pmu_test",   NvApiI2cTransfer, (void *)sI2cTPS65913 },
    { NULL, NULL, NULL, NULL },
};

PcbTestData *NvGetPcbTestDataList(void)
{
    return sPcbTestDataList_Default;
}
