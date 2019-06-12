/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_FALCON_DEBUG
#define INCLUDED_FALCON_DEBUG

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    NvFalconMSENC = 0,
    NvFalconTSEC
}NvFlcnDbg_DeviceType;

typedef enum
{
    FALCON_REG0 = 0x00000000,
    FALCON_REG1,
    FALCON_REG2,
    FALCON_REG3,
    FALCON_REG4,
    FALCON_REG5,
    FALCON_REG6,
    FALCON_REG7,
    FALCON_REG8,
    FALCON_REG9,
    FALCON_REG10,
    FALCON_REG11,
    FALCON_REG12,
    FALCON_REG13,
    FALCON_REG14,
    FALCON_REG15,
    FALCON_IV0,
    FALCON_IV1,
    FALCON_EV = 0x00000013,
    FALCON_SP,
    FALCON_PC,
    FALCON_IMB,
    FALCON_DMB,
    FALCON_CSW,
    FALCON_CCR,
    FALCON_SEC,
    FALCON_CTX,
    FALCON_EXCI
}NvFlcnDbg_FalconReg;

typedef struct FalconRegStruct {
    NvU32 IV0;
    NvU32 IV1;
    NvU32 EV;
    NvU32 SP;
    NvU32 PC;
    NvU32 IMB;
    NvU32 DMB;
    NvU32 CSW;
    NvU32 CCR;
    NvU32 SEC;
    NvU32 CTX;
    NvU32 EXCI;
}NvFlcnDbg_FalconRegStruct;

void *NvFlcnDbg_Init(NvFlcnDbg_DeviceType device);

/* All functions return NvError_NotInitialized if NvFlcnDbg_Init has not been run
 * atleast once for the pointer that is passed as void *NvFlcnDbgHandle.
 */

NvError NvFlcnDbg_Deinit(void *NvFlcnDbgHandle);

/* SetBreakpoint : Set a falcon breakpoint at the given ucode PC location
 * Returns error if all allowed breakpoints have been set.
 * Currently, only two breakpoints are allowed (HW limitation)
 * Falcon goes into debug mode as soon as breakpoint is hit
 */
NvError NvFlcnDbg_SetBreakpoint(void *NvFlcnDbgHandle, NvU32 PC);

/* DisableBreakpoint : Disable the breakpoint at the given ucode PC location.
 * Returns error if no breakpoint is found at PC.
 */
NvError NvFlcnDbg_DisableBreakpoint(void *NvFlcnDbgHandle, NvU32 PC);

/* WaitForBreak : Wait for Falcon to hit a breakpoint that was set earlier
 * Returns NvError_InvalidState if no breakpoint has been set
 */
NvError NvFlcnDbg_WaitForBreak(void *NvFlcnDbgHandle, NvU32 timeout);

/* Resume : Resume execution after a breakpoint has been hit
 * Returns NvError_InvalidState if no breakpoint has been set
 */
NvError NvFlcnDbg_Resume(void *NvFlcnDbgHandle);

/* Step : Execute one instruction and pause in debug mode
 * Returns NvError_InvalidState if no breakpoint has been set
 */
NvError NvFlcnDbg_Step(void *NvFlcnDbgHandle);

/* ReadReg : Read any of the registers defined in NvFlcnDbg_FalconReg.
 * data will contain the value read.
 */
NvError NvFlcnDbg_ReadReg(void *NvFlcnDbgHandle, NvFlcnDbg_FalconReg reg, NvU32 *data);

/* WriteReg : Write to any of the registers defined in NvFlcnDbg_FalconReg except the PC.
 * Returns NvError_InvalidState if not in debug mode.
 */
NvError NvFlcnDbg_WriteReg(void *NvFlcnDbgHandle, NvFlcnDbg_FalconReg reg, NvU32 data);

/* ReadDmem : Read NumOfBytes bytes from successive locations in falcon Dmem
 * beginning at start_addr and prints values onto screen.
 * Returns NvError_InvalidState if not in debug mode
 */
NvError NvFlcnDbg_ReadDmem(void *NvFlcnDbgHandle, NvU32 start_addr, NvU32 NumOfBytes);

/* WriteDmem : Write NumOfBytes bytes to successive locations in Falcon Dmem
 * beginning at start_addr
 * Returns NvError_InvalidState if not in debug mode
 */
NvError NvFlcnDbg_WriteDmem(void *NvFlcnDbgHandle, NvU32 start_addr, NvU8 *surface, NvU32 NumOfBytes);

/* DisplayRegs : Read the set of registers defined in NvFlcnDbg_FalconRegStruct
 * and displays them on screen */
NvError NvFlcnDbg_DisplayRegs(void *NvFlcnDbgHandle);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_FALCON_DEBUG
