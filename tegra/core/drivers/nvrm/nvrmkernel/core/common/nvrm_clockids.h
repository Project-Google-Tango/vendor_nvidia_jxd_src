/*
 * Copyright (c) 2007-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file nvrm_clockids.h
    Clock List & string names
*/

/* This is the list of all clock sources available on AP20, T30 and T11x.
 */

// 32 KHz clock - A.K.A relaxation oscillator.
NVRM_CLOCK_SOURCE('C', 'l', 'k', 'S', ' ', ' ', ' ', ' ', ClkS)
// Main clock (crystal or input)
NVRM_CLOCK_SOURCE('C', 'l', 'k', 'M', ' ', ' ', ' ', ' ', ClkM)
// Always double the Clock M
NVRM_CLOCK_SOURCE('C', 'l', 'k', 'D', ' ', ' ', ' ', ' ', ClkD)

// PLL clocks
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'A', '0', ' ', ' ', ' ', PllA0)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'A', '1', ' ', ' ', ' ', PllA1)

NVRM_CLOCK_SOURCE('P', 'l', 'l', 'C', '0', ' ', ' ', ' ', PllC0)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'C', '1', ' ', ' ', ' ', PllC1)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'D', '0', ' ', ' ', ' ', PllD0)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'D', '2', '0', ' ', ' ', PllD20)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'E', '0', ' ', ' ', ' ', PllE0)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'M', '0', ' ', ' ', ' ', PllM0)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'M', '1', ' ', ' ', ' ', PllM1)

NVRM_CLOCK_SOURCE('P', 'l', 'l', 'P', '0', ' ', ' ', ' ', PllP0)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'P', '1', ' ', ' ', ' ', PllP1)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'P', '2', ' ', ' ', ' ', PllP2)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'P', '3', ' ', ' ', ' ', PllP3)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'P', '4', ' ', ' ', ' ', PllP4)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'S', '0', ' ', ' ', ' ', PllS0)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'U', '0', ' ', ' ', ' ', PllU0)
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'X', '0', ' ', ' ', ' ', PllX0)

// External and recovered bit clock sources
NVRM_CLOCK_SOURCE('E', 'x', 't', 'S', 'p', 'd', 'f', ' ', ExtSpdf)
NVRM_CLOCK_SOURCE('E', 'x', 't', 'I', '2', 's', '1', ' ', ExtI2s1)
NVRM_CLOCK_SOURCE('E', 'x', 't', 'I', '2', 's', '2', ' ', ExtI2s2)
NVRM_CLOCK_SOURCE('E', 'x', 't', 'A', 'c', '9', '7', ' ', ExtAc97)
NVRM_CLOCK_SOURCE('E', 'x', 't', 'A', 'u', 'd', 'i', '1', ExtAudio1)
NVRM_CLOCK_SOURCE('E', 'x', 't', 'A', 'u', 'd', 'i', '2', ExtAudio2)
NVRM_CLOCK_SOURCE('E', 'x', 't', 'V', 'i', ' ', ' ', ' ', ExtVi)

// Audio Clocks
NVRM_CLOCK_SOURCE('A', 'u', 'd', 'i', 'S', 'y', 'n', 'c', AudioSync)
NVRM_CLOCK_SOURCE('M', 'p', 'e', 'A', 'u', 'd', 'o', ' ', MpeAudio)

// Internal bus sources
NVRM_CLOCK_SOURCE('C', 'p', 'u', 'B', 'u', 's', ' ', ' ', CpuBus)
NVRM_CLOCK_SOURCE('C', 'p', 'u', 'B', 'r', 'd', 'g', ' ', CpuBridge)
NVRM_CLOCK_SOURCE('S', 'y', 's', 't', 'B', 'u', 's', ' ', SystemBus)
NVRM_CLOCK_SOURCE('A', 'h', 'B', 'u', 's', ' ', ' ', ' ', Ahb)
NVRM_CLOCK_SOURCE('A', 'p', 'B', 'u', 's', ' ', ' ', ' ', Apb)
NVRM_CLOCK_SOURCE('V', 'd', 'e', 'B', 'u', 's', ' ', ' ', Vbus)

// PLL reference clock
NVRM_CLOCK_SOURCE('P', 'l', 'l', 'R', 'e', 'f', ' ', ' ', PllRef)
