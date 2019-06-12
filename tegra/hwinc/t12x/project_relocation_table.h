//
// Copyright (c) 2013 NVIDIA Corporation.
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

// Device Aperture: GPU
#define NV_DEVID_GPU                             161

// Device Aperture: SOR
#define NV_DEVID_SOR                             162

// Device Aperture: DPAUX
#define NV_DEVID_DPAUX                           163

// Device Aperture: COP1
#define NV_DEVID_COP1                            164

// Device Aperture: Image Signal Processor
#define NV_DEVID_ISPB                            165

// Device Aperture: VIC
#define NV_DEVID_VIC                             166

// Non-Aperture Interrupt: COP1 PMU Interrupt
#define NV_DEVID_COP1_INTR                       167

// Device Aperture: NANDB Flash Controller
#define NV_DEVID_NANDBCTRL                       168

// Non Aperture for External clock pin
#define NV_DEVID_EXTERNAL_CLOCK                  169

// Device Aperture: AFC
#define NV_DEVID_AFC                             173

// Device Aperture of clock-control registers for the core cluster
#define NV_DEVID_CLUSTER_CLOCK                   174

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
#define NV_RELOCATION_TABLE_SIZE  993
#define NV_RELOCATION_TABLE_INIT \
          0x00000001,\
          0x00531010, 0x00000000, 0x00000000, \
          0x009f0010, 0x00000000, 0x01000000, \
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
          0x00711010, 0x00000000, 0x00000000, \
          0x009f1110, 0x01000000, 0x01000000, \
          0x00691050, 0x01000000, 0x3f000000, \
          0x009f1110, 0x02000000, 0x0e000000, \
          0x009f1110, 0x10000000, 0x30000000, \
          0x00041110, 0x40000000, 0x00010000, \
          0x00041110, 0x40010000, 0x00010000, \
          0x00041110, 0x40020000, 0x00010000, \
          0x00041110, 0x40030000, 0x00010000, \
          0x009f1110, 0x41000000, 0x07000000, \
          0x009f0010, 0x48000000, 0x01000000, \
          0x00051010, 0x48000000, 0x08000000, \
          0x009f0010, 0x49000000, 0x02000000, \
          0x009f0010, 0x4b000000, 0x05000000, \
          0x00081010, 0x50000000, 0x00034000, \
          0x00201010, 0x50040000, 0x00002000, \
          0x00091020, 0x50040000, 0x00020000, \
          0x00721020, 0x50041000, 0x00001000, \
          0x00721020, 0x50042000, 0x00002000, \
          0x00721020, 0x50044000, 0x00001000, \
          0x00721020, 0x50045000, 0x00001000, \
          0x00721020, 0x50046000, 0x00002000, \
          0x000a1020, 0x50060000, 0x00001000, \
          0x009f0010, 0x51000000, 0x03000000, \
          0x000d3040, 0x54080000, 0x00040000, \
          0x00633040, 0x54080000, 0x00040000, \
          0x00621010, 0x541c0000, 0x00040000, \
          0x00131710, 0x54200000, 0x00040000, \
          0x00131710, 0x54240000, 0x00040000, \
          0x004d1510, 0x54280000, 0x00040000, \
          0x004c1110, 0x54300000, 0x00040000, \
          0x00a62010, 0x54340000, 0x00040000, \
          0x004c1110, 0x54400000, 0x00040000, \
          0x00911010, 0x544c0000, 0x00040000, \
          0x00941010, 0x54500000, 0x00040000, \
          0x00a21010, 0x54540000, 0x00040000, \
          0x00a31010, 0x545c0000, 0x00040000, \
          0x00123040, 0x54600000, 0x00040000, \
          0x00123040, 0x54680000, 0x00040000, \
          0x009f0010, 0x55000000, 0x02000000, \
          0x009f0010, 0x57000000, 0x09000000, \
          0x00a12010, 0x57000000, 0x09000000, \
          0x00141010, 0x60000000, 0x00001000, \
          0x00151010, 0x60001000, 0x00001000, \
          0x00161010, 0x60002000, 0x00001000, \
          0x00171010, 0x60003000, 0x00001000, \
          0x004f1010, 0x60004000, 0x00000040, \
          0x00551010, 0x60004040, 0x000000c0, \
          0x004f1110, 0x60004100, 0x00000100, \
          0x004f1210, 0x60004200, 0x00000100, \
          0x004f1310, 0x60004300, 0x00000100, \
          0x004f1410, 0x60004400, 0x00000100, \
          0x009d1510, 0x60004800, 0x00000100, \
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
          0x00821010, 0x6001dc00, 0x00000400, \
          0x001f1110, 0x60020000, 0x00004000, \
          0x00581010, 0x60021000, 0x00000040, \
          0x00581010, 0x60021040, 0x00000040, \
          0x00581010, 0x60021080, 0x00000040, \
          0x00581010, 0x600210c0, 0x00000040, \
          0x00581010, 0x60021100, 0x00000040, \
          0x00581010, 0x60021140, 0x00000040, \
          0x00581010, 0x60021180, 0x00000040, \
          0x00581010, 0x600211c0, 0x00000040, \
          0x00581010, 0x60021200, 0x00000040, \
          0x00581010, 0x60021240, 0x00000040, \
          0x00581010, 0x60021280, 0x00000040, \
          0x00581010, 0x600212c0, 0x00000040, \
          0x00581010, 0x60021300, 0x00000040, \
          0x00581010, 0x60021340, 0x00000040, \
          0x00581010, 0x60021380, 0x00000040, \
          0x00581010, 0x600213c0, 0x00000040, \
          0x00581010, 0x60021400, 0x00000040, \
          0x00581010, 0x60021440, 0x00000040, \
          0x00581010, 0x60021480, 0x00000040, \
          0x00581010, 0x600214c0, 0x00000040, \
          0x00581010, 0x60021500, 0x00000040, \
          0x00581010, 0x60021540, 0x00000040, \
          0x00581010, 0x60021580, 0x00000040, \
          0x00581010, 0x600215c0, 0x00000040, \
          0x00581010, 0x60021600, 0x00000040, \
          0x00581010, 0x60021640, 0x00000040, \
          0x00581010, 0x60021680, 0x00000040, \
          0x00581010, 0x600216c0, 0x00000040, \
          0x00581010, 0x60021700, 0x00000040, \
          0x00581010, 0x60021740, 0x00000040, \
          0x00581010, 0x60021780, 0x00000040, \
          0x00581010, 0x600217c0, 0x00000040, \
          0x00261360, 0x60030000, 0x00004000, \
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
          0x008a1010, 0x70006500, 0x00000100, \
          0x008b1010, 0x70006600, 0x00000100, \
          0x008c1010, 0x70006700, 0x00000100, \
          0x008d1010, 0x70006800, 0x00000100, \
          0x008e1010, 0x70006900, 0x00000100, \
          0x008f1010, 0x70006a00, 0x00000100, \
          0x00891010, 0x70006c00, 0x00000200, \
          0x00371310, 0x70008000, 0x00000200, \
          0x00a81010, 0x70008200, 0x00000200, \
          0x00383010, 0x70008500, 0x00000100, \
          0x00391010, 0x70008a00, 0x00000200, \
          0x006d1110, 0x70009000, 0x00001000, \
          0x003a1010, 0x7000a000, 0x00000100, \
          0x00881010, 0x7000b000, 0x00001000, \
          0x003c1210, 0x7000c000, 0x00000100, \
          0x003d1010, 0x7000c100, 0x00000100, \
          0x00851010, 0x7000c300, 0x00000100, \
          0x003c1210, 0x7000c400, 0x00000100, \
          0x003c1210, 0x7000c500, 0x00000100, \
          0x006a1110, 0x7000c600, 0x00000100, \
          0x003c1210, 0x7000c700, 0x00000100, \
          0x003c1210, 0x7000d000, 0x00000100, \
          0x003c1210, 0x7000d100, 0x00000100, \
          0x00401210, 0x7000d400, 0x00000200, \
          0x00401210, 0x7000d600, 0x00000200, \
          0x00401210, 0x7000d800, 0x00000200, \
          0x00401210, 0x7000da00, 0x00000200, \
          0x00401210, 0x7000dc00, 0x00000200, \
          0x00401210, 0x7000de00, 0x00000200, \
          0x00421100, 0x7000e000, 0x00000100, \
          0x00431200, 0x7000e200, 0x00000100, \
          0x00441200, 0x7000e400, 0x00000800, \
          0x00451110, 0x7000f800, 0x00000400, \
          0x006f1010, 0x7000fc00, 0x00000400, \
          0x00751010, 0x70010000, 0x00002000, \
          0x007c2210, 0x70012000, 0x00002000, \
          0x007b1010, 0x70014000, 0x00001000, \
          0x00871110, 0x70015000, 0x00001000, \
          0x00811010, 0x70016000, 0x00002000, \
          0x000b2010, 0x70019000, 0x00001000, \
          0x000c1310, 0x7001b000, 0x00001000, \
          0x00801010, 0x70020000, 0x00010000, \
          0x007f1010, 0x70030000, 0x00010000, \
          0x00ae1010, 0x70040000, 0x00040000, \
          0x00921010, 0x70090000, 0x0000a000, \
          0x00a01010, 0x7009f000, 0x00001000, \
          0x00951010, 0x700a0000, 0x00001200, \
          0x006e1010, 0x700b0000, 0x00000200, \
          0x006e1010, 0x700b0200, 0x00000200, \
          0x006e1010, 0x700b0400, 0x00000200, \
          0x006e1010, 0x700b0600, 0x00000200, \
          0x006e1010, 0x700b1000, 0x00000200, \
          0x006e1010, 0x700b2200, 0x00000200, \
          0x006e1010, 0x700b3400, 0x00000200, \
          0x006e1010, 0x700b4600, 0x00000200, \
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
          0x00781110, 0x70300000, 0x00000200, \
          0x007e1110, 0x70300000, 0x00010000, \
          0x00782010, 0x70300200, 0x00000600, \
          0x00791110, 0x70300800, 0x00000800, \
          0x00342110, 0x70301000, 0x00000100, \
          0x00342110, 0x70301100, 0x00000100, \
          0x00342110, 0x70301200, 0x00000100, \
          0x00342110, 0x70301300, 0x00000100, \
          0x00342110, 0x70301400, 0x00000100, \
          0x007a2110, 0x70302000, 0x00000200, \
          0x007a2110, 0x70302200, 0x00000200, \
          0x007a2110, 0x70302400, 0x00000200, \
          0x009a2110, 0x70303000, 0x00000100, \
          0x009a2110, 0x70303100, 0x00000100, \
          0x009b2110, 0x70303800, 0x00000100, \
          0x009b2110, 0x70303900, 0x00000100, \
          0x00332010, 0x70306000, 0x00000100, \
          0x00ad0010, 0x70307000, 0x00000100, \
          0x00ad0010, 0x70307100, 0x00000100, \
          0x00ad0010, 0x70307200, 0x00000100, \
          0x00ad0010, 0x70307300, 0x00000100, \
          0x00ad0010, 0x70307400, 0x00000100, \
          0x00ad0010, 0x70307500, 0x00000100, \
          0x00701010, 0x70800000, 0x00200000, \
          0x009f0010, 0x71000000, 0x07000000, \
          0x009f0010, 0x78000000, 0x01000000, \
          0x009f0010, 0x79000000, 0x03000000, \
          0x00601010, 0x7c000000, 0x00010000, \
          0x007d1010, 0x7c010000, 0x00010000, \
          0x00492010, 0x7d000000, 0x00004000, \
          0x00492110, 0x7d004000, 0x00004000, \
          0x00492210, 0x7d008000, 0x00004000, \
          0x009f0010, 0x7e000000, 0x02000000, \
          0x00020010, 0x80000000, 0x7ff00000, \
          0x00000000, \
          0x85000001, \
          0x82d00208, \
          0x82d0030f, \
          0x82d00404, \
          0x82d0050d, \
          0x84d00616, \
          0x8500070c, \
          0x82d00805, \
          0x85000900, \
          0x82d00a09, \
          0x82d00b02, \
          0x84f00c12, \
          0x84f00d13, \
          0x84d00e1f, \
          0x82d00f0e, \
          0x82d01006, \
          0x8500110e, \
          0x82d0120a, \
          0x82d01301, \
          0x8500140d, \
          0x82d01507, \
          0x8500160b, \
          0x82d0170b, \
          0x84f01811, \
          0x82d01900, \
          0x82d01a03, \
          0x84f01b10, \
          0x82d01c0c, \
          0x84e01e02, \
          0x84e01e03, \
          0x84e02004, \
          0xc4d02a00, \
          0xa4d02a01, \
          0xc4d02a02, \
          0xa4d02a03, \
          0x84e02b05, \
          0xa500320f, \
          0x84d03405, \
          0x84d03709, \
          0x84d0380a, \
          0x84d0390b, \
          0x84d03b08, \
          0x84d03d04, \
          0x84c03e12, \
          0x84d03f0c, \
          0x84f0401f, \
          0x84d04107, \
          0x84d04206, \
          0x84f0451d, \
          0x84f0451e, \
          0xa4a04704, \
          0xc4a04705, \
          0xc4a04706, \
          0xa4a04707, \
          0xa4a0481d, \
          0xc4a0481c, \
          0x84e0501f, \
          0x84e0501a, \
          0x84a05100, \
          0x84a05201, \
          0x84c05409, \
          0x84c0550a, \
          0x84e05619, \
          0x84f05718, \
          0x84f05819, \
          0x84f0591a, \
          0x84f05a1b, \
          0x84f05b1c, \
          0x8500610a, \
          0x84e0611b, \
          0x84e0611c, \
          0x84e0621e, \
          0xa5006308, \
          0xc5006309, \
          0x85006302, \
          0x85006303, \
          0x85006304, \
          0x85006305, \
          0xa4a0641b, \
          0xc4c0641d, \
          0x84c06d16, \
          0x84c06e0d, \
          0x84c06f00, \
          0x84c07001, \
          0x84c07102, \
          0x84c07203, \
          0x84c07317, \
          0x84d07417, \
          0x84d07519, \
          0x84e0761d, \
          0x84a07719, \
          0x84a07912, \
          0x84a07a0b, \
          0xa4a07c1a, \
          0xc4c07c1c, \
          0x84e07d08, \
          0x84e07e09, \
          0x84e07f0a, \
          0x84e0800b, \
          0x84e0810c, \
          0x84e0820d, \
          0x84e0830e, \
          0x84e0840f, \
          0x84e08510, \
          0x84e08611, \
          0x84e08712, \
          0x84e08813, \
          0x84e08914, \
          0x84e08a15, \
          0x84e08b16, \
          0x84e08c17, \
          0x84f08d00, \
          0x84f08e01, \
          0x84f08f02, \
          0x84f09003, \
          0x84f09104, \
          0x84f09205, \
          0x84f09306, \
          0x84f09407, \
          0x84f09508, \
          0x84f09609, \
          0x84f0970a, \
          0x84f0980b, \
          0x84f0990c, \
          0x84f09a0d, \
          0x84f09b0e, \
          0x84f09c0f, \
          0x84a09d09, \
          0x84a09d0a, \
          0x84a09d0c, \
          0x84a09d08, \
          0x84a09d11, \
          0x84c0a204, \
          0x84c0a305, \
          0x84c0a414, \
          0x84c0a50e, \
          0x84d0a61a, \
          0x84e0b300, \
          0x84c0b50f, \
          0x84c0b606, \
          0x84d0b914, \
          0x84d0ba1c, \
          0x84a0bb1e, \
          0x84e0bc18, \
          0x84c0bd15, \
          0x84c0be1f, \
          0x84c0bf1b, \
          0x84d0c012, \
          0x84d0c113, \
          0x84d0c21d, \
          0x84d0c31e, \
          0x84d0c40f, \
          0x84a0c502, \
          0x84d0c615, \
          0x84c0cb1a, \
          0x84a0cd03, \
          0x84d0cf0d, \
          0x84d0d00e, \
          0x84a0d117, \
          0x84a0d10d, \
          0x84d0d211, \
          0x84c0d40b, \
          0x84c0d408, \
          0x84c0d407, \
          0x84c0d511, \
          0x84a0d70e, \
          0x84f0d714, \
          0x84a0d80f, \
          0x84f0d815, \
          0x84a0d913, \
          0x84f0d916, \
          0x84a0da1f, \
          0x84f0da17, \
          0x84c0e319, \
          0x84c0e318, \
          0x84c0e30c, \
          0x84c0e610, \
          0x84c0e613, \
          0x84c0ea1e, \
          0x84e0ec07, \
          0x84a10814, \
          0x84a10915, \
          0x84e10a01, \
          0x00000000
#endif
