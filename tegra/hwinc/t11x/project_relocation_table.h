//
// Copyright (c) 2012 NVIDIA Corporation.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// Neither the name of the NVIDIA Corporation nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#ifndef PROJECT_RELOCATION_TABLE_SPEC_INC
#define PROJECT_RELOCATION_TABLE_SPEC_INC

// ------------------------------------------------------------
//    hw nvdevids
// ------------------------------------------------------------
// Memory Aperture: Internal Memory
#define NV_DEVID_IMEM                            1

// Memory Aperture: External Memory
#define NV_DEVID_EMEM                            2

// Memory Aperture: TCRAM
#define NV_DEVID_TCRAM                           3

// Memory Aperture: IRAM
#define NV_DEVID_IRAM                            4

// Memory Aperture: NOR FLASH
#define NV_DEVID_NOR                             5

// Memory Aperture: EXIO
#define NV_DEVID_EXIO                            6

// Memory Aperture: GART
#define NV_DEVID_GART                            7

// Device Aperture: Graphics Host (HOST1X)
#define NV_DEVID_HOST1X                          8

// Device Aperture: ARM PERIPH registers
#define NV_DEVID_ARM_PERIPH                      9

// Device Aperture: MSELECT
#define NV_DEVID_MSELECT                         10

// Device Aperture: memory controller
#define NV_DEVID_MC                              11

// Device Aperture: external memory controller
#define NV_DEVID_EMC                             12

// Device Aperture: video input
#define NV_DEVID_VI                              13

// Device Aperture: encoder pre-processor
#define NV_DEVID_EPP                             14

// Device Aperture: video encoder
#define NV_DEVID_MPE                             15

// Device Aperture: 3D engine
#define NV_DEVID_GR3D                            16

// Device Aperture: 2D + SBLT engine
#define NV_DEVID_GR2D                            17

// Device Aperture: Image Signal Processor
#define NV_DEVID_ISP                             18

// Device Aperture: DISPLAY
#define NV_DEVID_DISPLAY                         19

// Device Aperture: UPTAG
#define NV_DEVID_UPTAG                           20

// Device Aperture - SHR_SEM
#define NV_DEVID_SHR_SEM                         21

// Device Aperture - ARB_SEM
#define NV_DEVID_ARB_SEM                         22

// Device Aperture - ARB_PRI
#define NV_DEVID_ARB_PRI                         23

// Obsoleted for AP15
#define NV_DEVID_PRI_INTR                        24

// Obsoleted for AP15
#define NV_DEVID_SEC_INTR                        25

// Device Aperture: Timer Programmable
#define NV_DEVID_TMR                             26

// Device Aperture: Clock and Reset
#define NV_DEVID_CAR                             27

// Device Aperture: Flow control
#define NV_DEVID_FLOW                            28

// Device Aperture: Event
#define NV_DEVID_EVENT                           29

// Device Aperture: AHB DMA
#define NV_DEVID_AHB_DMA                         30

// Device Aperture: APB DMA
#define NV_DEVID_APB_DMA                         31

// Obsolete - use AVP_CACHE
#define NV_DEVID_CC                              32

// Device Aperture: COP Cache Controller
#define NV_DEVID_AVP_CACHE                       32

// Device Aperture: SYS_REG
#define NV_DEVID_SYS_REG                         32

// Device Aperture: System Statistic monitor
#define NV_DEVID_STAT                            33

// Device Aperture: GPIO
#define NV_DEVID_GPIO                            34

// Device Aperture: Vector Co-Processor 2
#define NV_DEVID_VCP                             35

// Device Aperture: Arm Vectors
#define NV_DEVID_VECTOR                          36

// Device: MEM
#define NV_DEVID_MEM                             37

// Obsolete - use VDE
#define NV_DEVID_SXE                             38

// Device Aperture: video decoder
#define NV_DEVID_VDE                             38

// Obsolete - use VDE
#define NV_DEVID_BSEV                            39

// Obsolete - use VDE
#define NV_DEVID_MBE                             40

// Obsolete - use VDE
#define NV_DEVID_PPE                             41

// Obsolete - use VDE
#define NV_DEVID_MCE                             42

// Obsolete - use VDE
#define NV_DEVID_TFE                             43

// Obsolete - use VDE
#define NV_DEVID_PPB                             44

// Obsolete - use VDE
#define NV_DEVID_VDMA                            45

// Obsolete - use VDE
#define NV_DEVID_UCQ                             46

// Device Aperture: BSEA (now in AVP cluster)
#define NV_DEVID_BSEA                            47

// Obsolete - use VDE
#define NV_DEVID_FRAMEID                         48

// Device Aperture: Misc regs
#define NV_DEVID_MISC                            49

// Obsolete
#define NV_DEVID_AC97                            50

// Device Aperture: S/P-DIF
#define NV_DEVID_SPDIF                           51

// Device Aperture: I2S
#define NV_DEVID_I2S                             52

// Device Aperture: UART
#define NV_DEVID_UART                            53

// Device Aperture: VFIR
#define NV_DEVID_VFIR                            54

// Device Aperture: NAND Flash Controller
#define NV_DEVID_NANDCTRL                        55

// Obsolete - use NANDCTRL
#define NV_DEVID_NANDFLASH                       55

// Device Aperture: HSMMC
#define NV_DEVID_HSMMC                           56

// Device Aperture: XIO
#define NV_DEVID_XIO                             57

// Device Aperture: PWM
#define NV_DEVID_PWM                             58

// Device Aperture: MIPI
#define NV_DEVID_MIPI_HS                         59

// Device Aperture: I2C
#define NV_DEVID_I2C                             60

// Device Aperture: TWC
#define NV_DEVID_TWC                             61

// Device Aperture: SLINK
#define NV_DEVID_SLINK                           62

// Device Aperture: SLINK4B
#define NV_DEVID_SLINK4B                         63

// Obsolete - use DTV
#define NV_DEVID_SPI                             64

// Device Aperture: DVC
#define NV_DEVID_DVC                             65

// Device Aperture: RTC
#define NV_DEVID_RTC                             66

// Device Aperture: KeyBoard Controller
#define NV_DEVID_KBC                             67

// Device Aperture: PMIF
#define NV_DEVID_PMIF                            68

// Device Aperture: FUSE
#define NV_DEVID_FUSE                            69

// Device Aperture: L2 Cache Controller
#define NV_DEVID_CMC                             70

// Device Apertuer: NOR FLASH Controller
#define NV_DEVID_NOR_REG                         71

// Device Aperture: EIDE
#define NV_DEVID_EIDE                            72

// Device Aperture: USB
#define NV_DEVID_USB                             73

// Device Aperture: SDIO
#define NV_DEVID_SDIO                            74

// Device Aperture: TVO
#define NV_DEVID_TVO                             75

// Device Aperture: DSI
#define NV_DEVID_DSI                             76

// Device Aperture: HDMI
#define NV_DEVID_HDMI                            77

// Device Aperture: Third Interrupt Controller extra registers
#define NV_DEVID_TRI_INTR                        78

// Device Aperture: Common Interrupt Controller
#define NV_DEVID_ICTLR                           79

// Non-Aperture Interrupt: DMA TX interrupts
#define NV_DEVID_DMA_TX_INTR                     80

// Non-Aperture Interrupt: DMA RX interrupts
#define NV_DEVID_DMA_RX_INTR                     81

// Non-Aperture Interrupt: SW reserved interrupt
#define NV_DEVID_SW_INTR                         82

// Non-Aperture Interrupt: CPU PMU Interrupt
#define NV_DEVID_CPU_INTR                        83

// Device Aperture: Timer Free Running MicroSecond
#define NV_DEVID_TMRUS                           84

// Device Aperture: Interrupt Controller ARB_GNT Registers
#define NV_DEVID_ICTLR_ARBGNT                    85

// Device Aperture: Interrupt Controller DMA Registers
#define NV_DEVID_ICTLR_DRQ                       86

// Device Aperture: AHB DMA Channel
#define NV_DEVID_AHB_DMA_CH                      87

// Device Aperture: APB DMA Channel
#define NV_DEVID_APB_DMA_CH                      88

// Device Aperture: AHB Arbitration Controller
#define NV_DEVID_AHB_ARBC                        89

// Obsolete - use AHB_ARBC
#define NV_DEVID_AHB_ARB_CTRL                    89

// Device Aperture: AHB/APB Debug Bus Registers
#define NV_DEVID_AHPBDEBUG                       91

// Device Aperture: Secure Boot Register
#define NV_DEVID_SECURE_BOOT                     92

// Device Aperture: SPROM
#define NV_DEVID_SPROM                           93

// Non-Aperture Interrupt: External PMU interrupt
#define NV_DEVID_PMU_EXT                         95

// Device Aperture: AHB EMEM to MC Flush Register
#define NV_DEVID_PPCS                            96

// Device Aperture: MMU TLB registers for COP/AVP
#define NV_DEVID_MMU_TLB                         97

// Device Aperture: OVG engine
#define NV_DEVID_VG                              98

// Device Aperture: CSI
#define NV_DEVID_CSI                             99

// Device ID for COP
#define NV_DEVID_AVP                             100

// Device ID for MPCORE
#define NV_DEVID_CPU                             101

// Device Aperture: ULPI controller
#define NV_DEVID_ULPI                            102

// Device Aperture: ARM CONFIG registers
#define NV_DEVID_ARM_CONFIG                      103

// Device Aperture: ARM PL310 (L2 controller)
#define NV_DEVID_ARM_PL310                       104

// Device Aperture: PCIe
#define NV_DEVID_PCIE                            105

// Device Aperture: OWR (one wire)
#define NV_DEVID_OWR                             106

// Device Aperture: AVPUCQ
#define NV_DEVID_AVPUCQ                          107

// Device Aperture: AVPBSEA (obsolete)
#define NV_DEVID_AVPBSEA                         108

// Device Aperture: Sync NOR
#define NV_DEVID_SNOR                            109

// Device Aperture: SDMMC
#define NV_DEVID_SDMMC                           110

// Device Aperture: KFUSE
#define NV_DEVID_KFUSE                           111

// Device Aperture: CSITE
#define NV_DEVID_CSITE                           112

// Non-Aperture Interrupt: ARM Interprocessor Interrupt
#define NV_DEVID_ARM_IPI                         113

// Device Aperture: ARM Interrupts 0-31
#define NV_DEVID_ARM_ICTLR                       114

// Obsolete: use mod_IOBIST
#define NV_DEVID_IOBIST                          115

// Device Aperture: SPEEDO
#define NV_DEVID_SPEEDO                          116

// Device Aperture: LA
#define NV_DEVID_LA                              117

// Device Aperture: VS
#define NV_DEVID_VS                              118

// Device Aperture: VCI
#define NV_DEVID_VCI                             119

// Device Aperture: APBIF
#define NV_DEVID_APBIF                           120

// Device Aperture: AUDIO
#define NV_DEVID_AUDIO                           121

// Device Aperture: DAM
#define NV_DEVID_DAM                             122

// Device Aperture: TSENSOR
#define NV_DEVID_TSENSOR                         123

// Device Aperture: SE
#define NV_DEVID_SE                              124

// Device Aperture: TZRAM
#define NV_DEVID_TZRAM                           125

// Device Aperture: AUDIO_CLUSTER
#define NV_DEVID_AUDIO_CLUSTER                   126

// Device Aperture: HDA
#define NV_DEVID_HDA                             127

// Device Aperture: SATA
#define NV_DEVID_SATA                            128

// Device Aperture: ATOMICS
#define NV_DEVID_ATOMICS                         129

// Device Aperture: IPATCH
#define NV_DEVID_IPATCH                          130

// Device Aperture: Activity Monitor
#define NV_DEVID_ACTMON                          131

// Device Aperture: Watch Dog Timer
#define NV_DEVID_WDT                             132

// Device Aperture: DTV
#define NV_DEVID_DTV                             133

// Device Aperture: Shared Timer
#define NV_DEVID_TMR_SHARED                      134

// Device Aperture: Consumer Electronics Control
#define NV_DEVID_CEC                             135

// Device Aperture: MIPIHSI
#define NV_DEVID_MIPIHSI                         136

// Device Aperture: SATA_IOBIST
#define NV_DEVID_SATA_IOBIST                     137

// Device Aperture: HDMI_IOBIST
#define NV_DEVID_HDMI_IOBIST                     138

// Device Aperture: MIPI_IOBIST
#define NV_DEVID_MIPI_IOBIST                     139

// Device Aperture: LPDDR2_IOBIST
#define NV_DEVID_LPDDR2_IOBIST                   140

// Device Aperture: PCIE_X2_0_IOBIST
#define NV_DEVID_PCIE_X2_0_IOBIST                141

// Device Aperture: PCIE_X2_1_IOBIST
#define NV_DEVID_PCIE_X2_1_IOBIST                142

// Device Aperture: PCIE_X4_IOBIST
#define NV_DEVID_PCIE_X4_IOBIST                  143

// Device Aperture: SPEEDO_PMON
#define NV_DEVID_SPEEDO_PMON                     144

// Device Aperture: MSENC
#define NV_DEVID_MSENC                           145

// Device Aperture: XUSB_HOST
#define NV_DEVID_XUSB_HOST                       146

// Device Aperture: XUSB_DEV
#define NV_DEVID_XUSB_DEV                        147

// Device Aperture: TSEC
#define NV_DEVID_TSEC                            148

// Device Aperture: DDS
#define NV_DEVID_DDS                             149

// Device Aperture: DP2
#define NV_DEVID_DP2                             150

// Device Aperture: APB2JTAG
#define NV_DEVID_APB2JTAG                        151

// Device Aperture: SOC_THERM
#define NV_DEVID_SOC_THERM                       152

// Device Aperture: MIPI_CAL
#define NV_DEVID_MIPI_CAL                        153

// Device Aperture: AMX
#define NV_DEVID_AMX                             154

// Device Aperture: ADX
#define NV_DEVID_ADX                             155

// Device Aperture: SYSCTR
#define NV_DEVID_SYSCTR                          156

// Device Aperture: Interrupt Controller HIER_GROUP1
#define NV_DEVID_ICTLR_HIER_GROUP1               157

// Device Aperture: DVFS
#define NV_DEVID_DVFS                            158

// Device Aperture: Configurable EMEM/MMIO aperture
#define NV_DEVID_EMEMIO                          159

// Device Aperture: XUSB_PADCTL
#define NV_DEVID_XUSB_PADCTL                     160

// ------------------------------------------------------------
//    hw powergroups
// ------------------------------------------------------------
// Always On
#define NV_POWERGROUP_AO                         0

// Main
#define NV_POWERGROUP_NPG                        1

// CPU related blocks
#define NV_POWERGROUP_CPU                        2

// 3D graphics
#define NV_POWERGROUP_TD                         3

// Video encode engine blocks
#define NV_POWERGROUP_VE                         4

// PCIe
#define NV_POWERGROUP_PCIE                       5

// Video decoder
#define NV_POWERGROUP_VDE                        6

// MPEG encoder
#define NV_POWERGROUP_MPE                        7

// SW define for Power Group maximum
#define NV_POWERGROUP_MAX                        8

// non-mapped power group
#define NV_POWERGROUP_INVALID                    0xffff

// SW table for mapping power group define to register enums (NV_POWERGROUP_INVALID = no mapping)
//  use as 'int table[NV_POWERGROUP_MAX] = { NV_POWERGROUP_ENUM_TABLE }'
#define NV_POWERGROUP_ENUM_TABLE                 NV_POWERGROUP_INVALID, NV_POWERGROUP_INVALID, 0, 1, 2, 3, 4, 6

// ------------------------------------------------------------
//    relocation table data (stored in boot rom)
// ------------------------------------------------------------
// relocation table pointer stored at NV_RELOCATION_TABLE_OFFSET
#define NV_RELOCATION_TABLE_PTR_OFFSET 64
#define NV_RELOCATION_TABLE_SIZE  1016
#define NV_RELOCATION_TABLE_INIT \
          0x00000001,\
          0x00531010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x005f1010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x00521010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x00531010, 0x00000000, 0x00000000, \
          0x009f0010, 0x00000000, 0x01000000, \
          0x00711010, 0x00000000, 0x00000000, \
          0x009f0010, 0x01000000, 0x01000000, \
          0x00691050, 0x01000000, 0x3f000000, \
          0x009f0010, 0x02000000, 0x0e000000, \
          0x009f0010, 0x10000000, 0x30000000, \
          0x00041110, 0x40000000, 0x00010000, \
          0x00041110, 0x40010000, 0x00010000, \
          0x00041110, 0x40020000, 0x00010000, \
          0x00041110, 0x40030000, 0x00010000, \
          0x009f0010, 0x41000000, 0x07000000, \
          0x009f0010, 0x48000000, 0x01000000, \
          0x00051010, 0x48000000, 0x08000000, \
          0x009f0010, 0x49000000, 0x02000000, \
          0x009f0010, 0x4b000000, 0x05000000, \
          0x00081010, 0x50000000, 0x00028000, \
          0x00201010, 0x50040000, 0x00002000, \
          0x00091020, 0x50040000, 0x00020000, \
          0x00721020, 0x50041000, 0x00001000, \
          0x00721020, 0x50042000, 0x00002000, \
          0x00721020, 0x50044000, 0x00001000, \
          0x00721020, 0x50045000, 0x00001000, \
          0x00721020, 0x50046000, 0x00002000, \
          0x000a1020, 0x50060000, 0x00001000, \
          0x009f0010, 0x51000000, 0x03000000, \
          0x000f1240, 0x54040000, 0x00040000, \
          0x000d1240, 0x54080000, 0x00040000, \
          0x00631340, 0x54080000, 0x00040000, \
          0x000e1040, 0x540c0000, 0x00040000, \
          0x00121140, 0x54100000, 0x00040000, \
          0x00111110, 0x54140000, 0x00040000, \
          0x00101430, 0x54180000, 0x00040000, \
          0x00621010, 0x541c0000, 0x00040000, \
          0x00131510, 0x54200000, 0x00040000, \
          0x00131510, 0x54240000, 0x00040000, \
          0x004d1410, 0x54280000, 0x00040000, \
          0x004b1110, 0x542c0000, 0x00040000, \
          0x004c1110, 0x54300000, 0x00040000, \
          0x00761010, 0x54340000, 0x00040000, \
          0x00771010, 0x54380000, 0x00040000, \
          0x004c1110, 0x54400000, 0x00040000, \
          0x00911010, 0x544c0000, 0x00040000, \
          0x00941010, 0x54500000, 0x00040000, \
          0x009f0010, 0x55000000, 0x02000000, \
          0x009f2010, 0x57000000, 0x09000000, \
          0x00141010, 0x60000000, 0x00001000, \
          0x00151010, 0x60001000, 0x00001000, \
          0x00161010, 0x60002000, 0x00001000, \
          0x00171010, 0x60003000, 0x00001000, \
          0x004f1010, 0x60004000, 0x00000040, \
          0x00551010, 0x60004040, 0x000000c0, \
          0x004f1110, 0x60004100, 0x00000040, \
          0x00561010, 0x60004140, 0x00000008, \
          0x00561110, 0x60004148, 0x00000008, \
          0x004f1210, 0x60004200, 0x00000040, \
          0x004f1310, 0x60004300, 0x00000040, \
          0x004f1410, 0x60004400, 0x00000040, \
          0x009d1510, 0x60004800, 0x00000040, \
          0x001a1010, 0x60005000, 0x00000008, \
          0x001a1010, 0x60005008, 0x00000008, \
          0x00541010, 0x60005010, 0x00000040, \
          0x001a1010, 0x60005050, 0x00000008, \
          0x001a1010, 0x60005058, 0x00000008, \
          0x001a1010, 0x60005060, 0x00000008, \
          0x001a1010, 0x60005068, 0x00000008, \
          0x001a1010, 0x60005070, 0x00000008, \
          0x001a1010, 0x60005078, 0x00000008, \
          0x001a1010, 0x60005080, 0x00000008, \
          0x001a1010, 0x60005088, 0x00000008, \
          0x00841010, 0x60005100, 0x00000020, \
          0x00841010, 0x60005120, 0x00000020, \
          0x00841010, 0x60005140, 0x00000020, \
          0x00841010, 0x60005160, 0x00000020, \
          0x00841010, 0x60005180, 0x00000020, \
          0x00861010, 0x600051a0, 0x00000020, \
          0x001b1210, 0x60006000, 0x00001000, \
          0x001c1010, 0x60007000, 0x00001000, \
          0x001e1110, 0x60008000, 0x00002000, \
          0x00571010, 0x60009000, 0x00000020, \
          0x00571010, 0x60009020, 0x00000020, \
          0x00571010, 0x60009040, 0x00000020, \
          0x00571010, 0x60009060, 0x00000020, \
          0x001f1110, 0x6000a000, 0x00002000, \
          0x00581010, 0x6000b000, 0x00000020, \
          0x00581010, 0x6000b020, 0x00000020, \
          0x00581010, 0x6000b040, 0x00000020, \
          0x00581010, 0x6000b060, 0x00000020, \
          0x00581010, 0x6000b080, 0x00000020, \
          0x00581010, 0x6000b0a0, 0x00000020, \
          0x00581010, 0x6000b0c0, 0x00000020, \
          0x00581010, 0x6000b0e0, 0x00000020, \
          0x00581010, 0x6000b100, 0x00000020, \
          0x00581010, 0x6000b120, 0x00000020, \
          0x00581010, 0x6000b140, 0x00000020, \
          0x00581010, 0x6000b160, 0x00000020, \
          0x00581010, 0x6000b180, 0x00000020, \
          0x00581010, 0x6000b1a0, 0x00000020, \
          0x00581010, 0x6000b1c0, 0x00000020, \
          0x00581010, 0x6000b1e0, 0x00000020, \
          0x00581010, 0x6000b200, 0x00000020, \
          0x00581010, 0x6000b220, 0x00000020, \
          0x00581010, 0x6000b240, 0x00000020, \
          0x00581010, 0x6000b260, 0x00000020, \
          0x00581010, 0x6000b280, 0x00000020, \
          0x00581010, 0x6000b2a0, 0x00000020, \
          0x00581010, 0x6000b2c0, 0x00000020, \
          0x00581010, 0x6000b2e0, 0x00000020, \
          0x00581010, 0x6000b300, 0x00000020, \
          0x00581010, 0x6000b320, 0x00000020, \
          0x00581010, 0x6000b340, 0x00000020, \
          0x00581010, 0x6000b360, 0x00000020, \
          0x00581010, 0x6000b380, 0x00000020, \
          0x00581010, 0x6000b3a0, 0x00000020, \
          0x00581010, 0x6000b3c0, 0x00000020, \
          0x00581010, 0x6000b3e0, 0x00000020, \
          0x00591010, 0x6000c000, 0x00000150, \
          0x00201010, 0x6000c000, 0x00000300, \
          0x005b1010, 0x6000c150, 0x000000b0, \
          0x005c1010, 0x6000c200, 0x00000100, \
          0x00211010, 0x6000c400, 0x00000400, \
          0x00831010, 0x6000c800, 0x00000400, \
          0x00222110, 0x6000d000, 0x00000100, \
          0x00222110, 0x6000d100, 0x00000100, \
          0x00222110, 0x6000d200, 0x00000100, \
          0x00222110, 0x6000d300, 0x00000100, \
          0x00222110, 0x6000d400, 0x00000100, \
          0x00222110, 0x6000d500, 0x00000100, \
          0x00222110, 0x6000d600, 0x00000100, \
          0x00222110, 0x6000d700, 0x00000100, \
          0x00231010, 0x6000e000, 0x00001000, \
          0x00241010, 0x6000f000, 0x00001000, \
          0x006b1010, 0x60010000, 0x00000100, \
          0x002f1210, 0x60011000, 0x00001000, \
          0x00261360, 0x6001a000, 0x00003c00, \
          0x00821010, 0x6001dc00, 0x00000400, \
          0x009f0010, 0x61000000, 0x07000000, \
          0x00061010, 0x68000000, 0x01000000, \
          0x009f0010, 0x69000000, 0x07000000, \
          0x00312110, 0x70000000, 0x00004000, \
          0x00351310, 0x70006000, 0x00000040, \
          0x00351310, 0x70006040, 0x00000040, \
          0x00361010, 0x70006100, 0x00000100, \
          0x00351310, 0x70006200, 0x00000100, \
          0x00351310, 0x70006300, 0x00000100, \
          0x00351310, 0x70006400, 0x00000100, \
          0x008d1010, 0x70006800, 0x00000100, \
          0x008e1010, 0x70006900, 0x00000100, \
          0x008f1010, 0x70006a00, 0x00000100, \
          0x00891010, 0x70006c00, 0x00000200, \
          0x00371310, 0x70008000, 0x00000200, \
          0x00383010, 0x70008500, 0x00000100, \
          0x00391010, 0x70008a00, 0x00000200, \
          0x006d1110, 0x70009000, 0x00001000, \
          0x003a1010, 0x7000a000, 0x00000100, \
          0x00881010, 0x7000b000, 0x00001000, \
          0x003c1310, 0x7000c000, 0x00000100, \
          0x003d1010, 0x7000c100, 0x00000100, \
          0x00851010, 0x7000c300, 0x00000100, \
          0x003c1310, 0x7000c400, 0x00000100, \
          0x003c1310, 0x7000c500, 0x00000100, \
          0x006a1110, 0x7000c600, 0x00000100, \
          0x003c1310, 0x7000c700, 0x00000100, \
          0x003c1310, 0x7000d000, 0x00000100, \
          0x003e1210, 0x7000d400, 0x00000200, \
          0x003e1210, 0x7000d600, 0x00000200, \
          0x003e1210, 0x7000d800, 0x00000200, \
          0x003e1210, 0x7000da00, 0x00000200, \
          0x003e1210, 0x7000dc00, 0x00000200, \
          0x003e1210, 0x7000de00, 0x00000200, \
          0x00421100, 0x7000e000, 0x00000100, \
          0x00431200, 0x7000e200, 0x00000100, \
          0x00441200, 0x7000e400, 0x00000800, \
          0x00451110, 0x7000f800, 0x00000400, \
          0x006f1010, 0x7000fc00, 0x00000400, \
          0x00751010, 0x70010000, 0x00002000, \
          0x007c2010, 0x70012000, 0x00002000, \
          0x007b1010, 0x70014000, 0x00001000, \
          0x00871010, 0x70015000, 0x00001000, \
          0x00811010, 0x70016000, 0x00002000, \
          0x000b2010, 0x70018000, 0x00001000, \
          0x000b2010, 0x70019000, 0x00001000, \
          0x000b2010, 0x70019000, 0x00001000, \
          0x000c1310, 0x7001a000, 0x00000800, \
          0x000c1310, 0x7001a800, 0x00000800, \
          0x000c1310, 0x7001b000, 0x00000800, \
          0x000c1310, 0x7001b000, 0x00000800, \
          0x000b2010, 0x7001c000, 0x00001000, \
          0x00801010, 0x70020000, 0x00010000, \
          0x007f1010, 0x70030000, 0x00010000, \
          0x00701010, 0x70040000, 0x00040000, \
          0x00781110, 0x70080000, 0x00000200, \
          0x007e1110, 0x70080000, 0x00002000, \
          0x00791110, 0x70080200, 0x00000100, \
          0x00342110, 0x70080300, 0x00000100, \
          0x00342110, 0x70080400, 0x00000100, \
          0x00342110, 0x70080500, 0x00000100, \
          0x00342110, 0x70080600, 0x00000100, \
          0x00342110, 0x70080700, 0x00000100, \
          0x007a1110, 0x70080800, 0x00000100, \
          0x007a1110, 0x70080900, 0x00000100, \
          0x007a1110, 0x70080a00, 0x00000100, \
          0x00332010, 0x70080b00, 0x00000100, \
          0x009a2110, 0x70080c00, 0x00000100, \
          0x009a2110, 0x70080d00, 0x00000100, \
          0x009b2110, 0x70080e00, 0x00000100, \
          0x009b2110, 0x70080f00, 0x00000100, \
          0x00782010, 0x70081000, 0x00000200, \
          0x00921010, 0x70090000, 0x0000a000, \
          0x00a01010, 0x7009f000, 0x00001000, \
          0x00951010, 0x700a0000, 0x00001200, \
          0x00741010, 0x700c0000, 0x00000100, \
          0x00741010, 0x700c0100, 0x00000100, \
          0x00901010, 0x700c8000, 0x00000200, \
          0x00901010, 0x700c8200, 0x00000200, \
          0x00931010, 0x700d0000, 0x0000a000, \
          0x00961010, 0x700e0000, 0x00000100, \
          0x00971010, 0x700e1000, 0x00000200, \
          0x00981010, 0x700e2000, 0x00001000, \
          0x00991010, 0x700e3000, 0x00000100, \
          0x009c1010, 0x700f0000, 0x00010000, \
          0x009c1010, 0x70100000, 0x00010000, \
          0x009e1010, 0x70110000, 0x00000400, \
          0x009f0010, 0x71000000, 0x07000000, \
          0x009f0010, 0x78000000, 0x01000000, \
          0x006e3010, 0x78000000, 0x00000200, \
          0x006e3010, 0x78000200, 0x00000200, \
          0x006e3010, 0x78000400, 0x00000200, \
          0x006e3010, 0x78000600, 0x00000200, \
          0x006e3010, 0x78001000, 0x00000200, \
          0x006e3010, 0x78002200, 0x00000200, \
          0x006e3010, 0x78003400, 0x00000200, \
          0x006e3010, 0x78004600, 0x00000200, \
          0x009f0010, 0x79000000, 0x03000000, \
          0x00601010, 0x7c000000, 0x00010000, \
          0x007d1010, 0x7c010000, 0x00010000, \
          0x00492010, 0x7d000000, 0x00004000, \
          0x00492110, 0x7d004000, 0x00004000, \
          0x00492210, 0x7d008000, 0x00004000, \
          0x009f0010, 0x7e000000, 0x02000000, \
          0x00020010, 0x80000000, 0x7ff00000, \
          0x009f0010, 0xfff00000, 0x00100000, \
          0x00000000, \
          0x85400001, \
          0x82d00108, \
          0x82d0020f, \
          0x82d00304, \
          0x82d0040d, \
          0x85100516, \
          0x8540060c, \
          0x82d00705, \
          0x85400800, \
          0x82d00909, \
          0x82d00a02, \
          0x85300b12, \
          0x85300c13, \
          0x85100d1f, \
          0x82d00e0e, \
          0x82d00f06, \
          0x8540100e, \
          0x82d0110a, \
          0x82d01201, \
          0x8540130d, \
          0x82d01407, \
          0x8540150b, \
          0x82d0160b, \
          0x85301711, \
          0x82d01800, \
          0x82d01903, \
          0x85301a10, \
          0x82d01c0c, \
          0xc5102a00, \
          0xa5102a01, \
          0xc5102a02, \
          0xa5102a03, \
          0x85202b05, \
          0x85103505, \
          0x85103706, \
          0x85103807, \
          0x85103908, \
          0x85103c09, \
          0x85103d0a, \
          0x85103e0b, \
          0x85104404, \
          0x84e04512, \
          0xa4c04904, \
          0xc4c04905, \
          0xc4c04906, \
          0xa4c04907, \
          0xa4c04a1d, \
          0xc4c04a1c, \
          0x8520541f, \
          0x8520541a, \
          0x84c05500, \
          0x84c05601, \
          0x84e05809, \
          0x84e0590a, \
          0x85205a19, \
          0x85305b18, \
          0x85305c19, \
          0x85305d1a, \
          0x85305e1b, \
          0x85305f1c, \
          0x8540650a, \
          0x8520651b, \
          0x8520651c, \
          0x8520661e, \
          0xa5406708, \
          0xc5406709, \
          0x85406702, \
          0x85406703, \
          0x85406704, \
          0x85406705, \
          0xa4c0681b, \
          0xc4e0681d, \
          0xa4c06d1a, \
          0xc4e06d1c, \
          0x85206e08, \
          0x85206f09, \
          0x8520700a, \
          0x8520710b, \
          0x8520720c, \
          0x8520730d, \
          0x8520740e, \
          0x8520750f, \
          0x85207610, \
          0x85207711, \
          0x85207812, \
          0x85207913, \
          0x85207a14, \
          0x85207b15, \
          0x85207c16, \
          0x85207d17, \
          0x85307e00, \
          0x85307f01, \
          0x85308002, \
          0x85308103, \
          0x85308204, \
          0x85308305, \
          0x85308406, \
          0x85308507, \
          0x85308608, \
          0x85308709, \
          0x8530880a, \
          0x8530890b, \
          0x85308a0c, \
          0x85308b0d, \
          0x85308c0e, \
          0x85308d0f, \
          0x84e09216, \
          0x84e0930d, \
          0x84e09400, \
          0x84e09501, \
          0x84e09602, \
          0x84e09703, \
          0x84e09817, \
          0x85109917, \
          0x85109a19, \
          0x85209b1d, \
          0x84c09c19, \
          0x84c09e12, \
          0x84c09f0b, \
          0x84c0a009, \
          0x84c0a00a, \
          0x84c0a00c, \
          0x84c0a008, \
          0x84c0a011, \
          0x84e0a604, \
          0x84f0a608, \
          0x8500a608, \
          0x84e0a705, \
          0x84f0a709, \
          0x8500a709, \
          0x84e0a814, \
          0x84f0a811, \
          0x8500a811, \
          0x84e0a90e, \
          0x84f0a90a, \
          0x8500a90a, \
          0x8510aa1a, \
          0x84f0aa0e, \
          0x8500aa0e, \
          0x84f0ab0f, \
          0x8500ab0f, \
          0x84c0b018, \
          0x8520b300, \
          0x84e0b50f, \
          0x84e0b606, \
          0x84f0b60c, \
          0x8500b60c, \
          0x84f0b70b, \
          0x8500b70b, \
          0x84f0b807, \
          0x8500b807, \
          0x8510b914, \
          0x84f0b90d, \
          0x8500b90d, \
          0x8510ba1c, \
          0x84f0ba10, \
          0x8500ba10, \
          0x84c0bb1e, \
          0x8520bc18, \
          0x84f0bc12, \
          0x8500bc12, \
          0x84e0bd15, \
          0x84f0bd13, \
          0x8500bd13, \
          0x84e0be1b, \
          0x8510bf12, \
          0x8510c013, \
          0x8510c11d, \
          0x8510c21e, \
          0x8510c30f, \
          0x84c0c402, \
          0x8510c515, \
          0x84e0ca1a, \
          0x84c0cc03, \
          0x8530ce1e, \
          0x8510cf0d, \
          0x8530d11f, \
          0x8510d40e, \
          0x8510d711, \
          0x8520da07, \
          0x84f0da01, \
          0x84f0da02, \
          0x84f0da03, \
          0x84f0da04, \
          0x8500da01, \
          0x8500da02, \
          0x8500da03, \
          0x8500da04, \
          0x84e0ea0b, \
          0x84e0ea08, \
          0x84e0ea07, \
          0x84e0eb11, \
          0x84e0f119, \
          0x84e0f118, \
          0x84e0f10c, \
          0x84e0f410, \
          0x84e0f413, \
          0x84e0f81e, \
          0x84c0fb0e, \
          0x8530fb14, \
          0x84c0fc0f, \
          0x8530fc15, \
          0x84c0fd13, \
          0x8530fd16, \
          0x84c0fe1f, \
          0x8530fe17, \
          0x84c10614, \
          0x84c10715, \
          0x85210801, \
          0x00000000
#endif
