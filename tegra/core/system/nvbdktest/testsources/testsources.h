/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nv3p.h"
#include "error_handler.h"
#include "registerer.h"

// Function prototypes go here

//Se
NvError NvSeAesVerifyTest(NvBDKTest_TestPrivData data);
void NvSeShaVerifyTest(void);
void NvSeRsaVerifyTest(void);
void NvSeAesClearSBKTest(void);
void NvSeAesLocksskTest(void);
NvError NvSeRngVerifyTest(void);
NvError NvSeAesPerformanceTest(void);
NvError se_init_reg(void);

//Dsi
NvError NvDsiBasicTest(NvBDKTest_TestPrivData data);
NvError dsi_init_reg(void);

//uart
NvError NvUartWriteVerifyTest(void);
NvError NvUartWritePerfTest(void);
NvError uart_init_reg(void);

//i2c
NvError NvI2cEepromWRTest(void);
NvError NvReadBoardID(void);
NvError i2c_init_reg(void);

//Usb
NvError NvUsbVerifyWriteRead(NvBDKTest_TestPrivData data);
NvError NvUsbVerifyWriteReadFull(NvBDKTest_TestPrivData data);
NvError NvUsbVerifyWriteReadSpeed(NvBDKTest_TestPrivData data);
NvError Usb_init_reg(void);

// Sd
NvError NvSdWriteReadVerifyTest(NvBDKTest_TestPrivData data);
NvError NvSdVerifyWriteReadFull(NvBDKTest_TestPrivData data);
NvError NvSdVerifyWriteReadSpeed(NvBDKTest_TestPrivData data);
NvError NvSdVerifyWriteReadSpeed_BootArea(NvBDKTest_TestPrivData data);
NvError NvSdVerifyErase(NvBDKTest_TestPrivData data);
NvError sd_init_reg(void);

//PMU
NvError pmu_init_reg(void);
NvError NvBdkPmuTestMain(void);

//pwm
/**
 * Intializes the Pwm Controller and Configures the GPIO pin as SFIO to attach
 * to PWM. Configures pwm to lowest in the predefined frequency range.
 * Increases duty cycle by 1, from 0 to 100 in each predefined time interval
 * And then decreases by 1, from 100 to 0 in each predefined time interval.
 * Does the same for different frequencies that predefined frequency range.
 *
 * @retval NvSuccess The Pwm pulse generation is successful.
 * @retval NvError_InsufficientMemory
 * @retval NvError_BadParameter
 */
NvError NvPwmBasicTest(void);
NvError pwm_init_reg(void);

//Fuse
NvError NvFuseReadTest(NvBDKTest_TestPrivData Instance);
NvError NvFuseBypassTest(NvBDKTest_TestPrivData Instance);
NvError fuse_init_reg(void);

//USBf
NvError NvUsbfPerfTest(void);
NvError usbf_init_reg(void);

NvError mipibif_init_reg(void);

//PCB
NvError pcb_init_reg(void);
