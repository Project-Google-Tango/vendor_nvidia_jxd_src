/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_QUERY_PINMUX_GPIO_H
#define INCLUDED_NVODM_QUERY_PINMUX_GPIO_H

/**
 * Board personalities describe different topologies for
 * externally attached peripherals (devices attached outside of
 * the SOC).  Changing a personality is the equivalent of
 * re-wiring the target hardware (even if this is achieved via
 * some clever switching mechanisms on a single target device).
 * The way in which these topologies are rewired is entirely
 * dependent on the board implementation.
 *
 * The ODM Kit treats different personalities as if they were
 * different boards (or product SKUs).  Therefore, it is
 * necessary to recompile the ODM Kit whenever the personality
 * configuration is changed (via DIP switches, etc...).
 *
 * For the reference implementation, board-specific personality
 * definitions are shared between ODM pin-mux and ODM gpio
 * implementations.  The following definitions only apply to
 * development systems with E920 (Vail) and E915 (Concorde)
 * motherboards.
 */
typedef enum
{
    NvOdmQueryBoardPersonality_ScrollWheel_WiFi = 0x01, // Personality 1

    NvOdmQueryBoardPersonality_Force32 = 0x7FFFFFFF,
} NvOdmQueryBoardPersonality;

/**
 * Defines the current board personality (default is 3)
 */
#define NVODM_QUERY_BOARD_PERSONALITY   0x01

#endif // INCLUDED_NVODM_QUERY_PINMUX_GPIO_H
