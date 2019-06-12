/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_ARSPI_H
#define INCLUDED_NVDDK_ARSPI_H


// Register SPI_COMMAND_0
#define SPI_COMMAND_0                                   _MK_ADDR_CONST(0x0)
#define SPI_COMMAND_0_RESET_VAL                  _MK_MASK_CONST(0x43D00000)
#define SPI_COMMAND_0_PIO_RANGE                         31:31
#define SPI_COMMAND_0_PIO_PIO                           _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_M_S_RANGE                         30:30
#define SPI_COMMAND_0_M_S_SLAVE                         _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_M_S_MASTER                        _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_MODE_RANGE                        29:28
#define SPI_COMMAND_0_MODE_Mode0                        _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_MODE_Mode1                        _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_MODE_Mode2                        _MK_ENUM_CONST(2)
#define SPI_COMMAND_0_MODE_Mode3                        _MK_ENUM_CONST(3)
#define SPI_COMMAND_0_CS_SEL_RANGE                      27:26
#define SPI_COMMAND_0_CS_SEL_CS0                        _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_CS_SEL_CS1                        _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_CS_SEL_CS2                        _MK_ENUM_CONST(2)
#define SPI_COMMAND_0_CS_SEL_CS3                        _MK_ENUM_CONST(3)
#define SPI_COMMAND_0_CS_SW_HW_RANGE                    21:21
#define SPI_COMMAND_0_CS_SW_HW_HARDWARE                 _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_CS_SW_HW_SOFTWARE                 _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_CS_SW_VAL_RANGE                   20:20
#define SPI_COMMAND_0_CS_SW_VAL_LOW                     _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_CS_SW_VAL_HIGH                    _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_IDLE_SDA_RANGE                    19:18
#define SPI_COMMAND_0_IDLE_SDA_DRIVE_LOW                _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_IDLE_SDA_DRIVE_HIGH               _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_IDLE_SDA_PULL_LOW                 _MK_ENUM_CONST(2)
#define SPI_COMMAND_0_IDLE_SDA_PULL_HIGH                _MK_ENUM_CONST(3)
#define SPI_COMMAND_0_Rx_EN_RANGE                       12:12
#define SPI_COMMAND_0_Rx_EN_DISABLE                     _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_Rx_EN_ENABLE                      _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_Tx_EN_RANGE                       11:11
#define SPI_COMMAND_0_Tx_EN_DISABLE                     _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_Tx_EN_ENABLE                      _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_PACKED_RANGE                      5:5
#define SPI_COMMAND_0_PACKED_DISABLE                    _MK_ENUM_CONST(0)
#define SPI_COMMAND_0_PACKED_ENABLE                     _MK_ENUM_CONST(1)
#define SPI_COMMAND_0_BIT_LENGTH_SHIFT                  _MK_SHIFT_CONST(0)
#define SPI_COMMAND_0_BIT_LENGTH_RANGE                  4:0


// Register SPI_COMMAND2_0
#define SPI_COMMAND2_0                                  _MK_ADDR_CONST(0x4)
#define SPI_COMMAND2_0_SECURE                           0x0
#define SPI_COMMAND2_0_Tx_Clk_TAP_DELAY_RANGE           11:6
#define SPI_COMMAND2_0_Rx_Clk_TAP_DELAY_RANGE            5:0


// Register SPI_TIMING_REG1_0
#define SPI_TIMING_REG1_0                               _MK_ADDR_CONST(0x8)
#define SPI_TIMING_REG1_0_CS_SETUP_3_RANGE              31:28
#define SPI_TIMING_REG1_0_CS_HOLD_3_RANGE               27:24
#define SPI_TIMING_REG1_0_CS_SETUP_2_RANGE              23:20
#define SPI_TIMING_REG1_0_CS_HOLD_2_RANGE               19:16
#define SPI_TIMING_REG1_0_CS_SETUP_1_RANGE              15:12
#define SPI_TIMING_REG1_0_CS_HOLD_1_RANGE               11:8
#define SPI_TIMING_REG1_0_CS_SETUP_0_RANGE               7:4
#define SPI_TIMING_REG1_0_CS_HOLD_0_RANGE                3:0


// Register SPI_TIMING_REG2_0
#define SPI_TIMING_REG2_0                                   _MK_ADDR_CONST(0xc)
#define SPI_TIMING_REG2_0_CS_ACTIVE_BETWEEN_PACKETS_3_RANGE       29:29
#define SPI_TIMING_REG2_0_CYCLES_BETWEEN_PACKETS_3_RANGE          28:24
#define SPI_TIMING_REG2_0_CS_ACTIVE_BETWEEN_PACKETS_2_RANGE       21:21
#define SPI_TIMING_REG2_0_CYCLES_BETWEEN_PACKETS_2_RANGE          20:16
#define SPI_TIMING_REG2_0_CS_ACTIVE_BETWEEN_PACKETS_1_RANGE       13:13
#define SPI_TIMING_REG2_0_CYCLES_BETWEEN_PACKETS_1_RANGE          12:8
#define SPI_TIMING_REG2_0_CS_ACTIVE_BETWEEN_PACKETS_0_RANGE       5:5
#define SPI_TIMING_REG2_0_CYCLES_BETWEEN_PACKETS_0_RANGE          4:0


// Register SPI_TRANSFER_STATUS_0
#define SPI_TRANSFER_STATUS_0                         _MK_ADDR_CONST(0x10)
#define SPI_TRANSFER_STATUS_0_RDY_RANGE                    30:30
#define SPI_TRANSFER_STATUS_0_RDY_NOT_READY          _MK_ENUM_CONST(0)
#define SPI_TRANSFER_STATUS_0_RDY_READY              _MK_ENUM_CONST(1)


// Register SPI_FIFO_STATUS_0
#define SPI_FIFO_STATUS_0                           _MK_ADDR_CONST(0x14)
#define SPI_FIFO_STATUS_0_CS_INACTIVE_RANGE                31:31
#define SPI_FIFO_STATUS_0_RX_FIFO_FULL_COUNT_RANGE         29:23
#define SPI_FIFO_STATUS_0_TX_FIFO_EMPTY_COUNT_RANGE        22:16
#define SPI_FIFO_STATUS_0_RX_FIFO_FLUSH_RANGE              15:15
#define SPI_FIFO_STATUS_0_TX_FIFO_FLUSH_RANGE              14:14
#define SPI_FIFO_STATUS_0_ERR_RANGE                         8:8
#define SPI_FIFO_STATUS_0_TX_FIFO_OVF_RANGE                 7:7
#define SPI_FIFO_STATUS_0_TX_FIFO_UNF_RANGE                 6:6
#define SPI_FIFO_STATUS_0_RX_FIFO_OVF_RANGE                 5:5
#define SPI_FIFO_STATUS_0_RX_FIFO_UNF_RANGE                 4:4
#define SPI_FIFO_STATUS_0_TX_FIFO_FULL_RANGE                3:3
#define SPI_FIFO_STATUS_0_TX_FIFO_EMPTY_RANGE               2:2
#define SPI_FIFO_STATUS_0_RX_FIFO_FULL_RANGE                1:1


// Register SPI_TX_DATA_0
#define SPI_TX_DATA_0                              _MK_ADDR_CONST(0x18)

// Register SPI_RX_DATA_0
#define SPI_RX_DATA_0                              _MK_ADDR_CONST(0x1c)

// Register SPI_DMA_CTL_0
#define SPI_DMA_CTL_0                              _MK_ADDR_CONST(0x20)

// Register SPI_DMA_BLK_SIZE_0
#define SPI_DMA_BLK_SIZE_0                         _MK_ADDR_CONST(0x24)
#define SPI_DMA_BLK_SIZE_0_DMA_BLOCK_SIZE_RANGE           15:0

// Register SPI_TX_FIFO_0
#define SPI_TX_FIFO_0                              _MK_ADDR_CONST(0x108)

// Register SPI_RX_FIFO_0
#define SPI_RX_FIFO_0                              _MK_ADDR_CONST(0x188)

#endif // INCLUDED_NVDDK_ARSPI_H

