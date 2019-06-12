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

//
// DO NOT EDIT - generated by simspec!
//

#ifndef ___ARTIMER_H_INC_
#define ___ARTIMER_H_INC_

// Register TIMER_TMR_PTV_0
#define TIMER_TMR_PTV_0                 _MK_ADDR_CONST(0x0)
#define TIMER_TMR_PTV_0_SECURE                  0x0
#define TIMER_TMR_PTV_0_WORD_COUNT                      0x1
#define TIMER_TMR_PTV_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_RESET_MASK                      _MK_MASK_CONST(0xdfffffff)
#define TIMER_TMR_PTV_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_READ_MASK                       _MK_MASK_CONST(0xdfffffff)
#define TIMER_TMR_PTV_0_WRITE_MASK                      _MK_MASK_CONST(0xdfffffff)
#define TIMER_TMR_PTV_0_EN_SHIFT                        _MK_SHIFT_CONST(31)
#define TIMER_TMR_PTV_0_EN_FIELD                        _MK_FIELD_CONST(0x1, TIMER_TMR_PTV_0_EN_SHIFT)
#define TIMER_TMR_PTV_0_EN_RANGE                        31:31
#define TIMER_TMR_PTV_0_EN_WOFFSET                      0x0
#define TIMER_TMR_PTV_0_EN_DEFAULT                      _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_EN_DEFAULT_MASK                 _MK_MASK_CONST(0x1)
#define TIMER_TMR_PTV_0_EN_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_EN_SW_DEFAULT_MASK                      _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_EN_DISABLE                      _MK_ENUM_CONST(0)
#define TIMER_TMR_PTV_0_EN_ENABLE                       _MK_ENUM_CONST(1)

#define TIMER_TMR_PTV_0_PER_SHIFT                       _MK_SHIFT_CONST(30)
#define TIMER_TMR_PTV_0_PER_FIELD                       _MK_FIELD_CONST(0x1, TIMER_TMR_PTV_0_PER_SHIFT)
#define TIMER_TMR_PTV_0_PER_RANGE                       30:30
#define TIMER_TMR_PTV_0_PER_WOFFSET                     0x0
#define TIMER_TMR_PTV_0_PER_DEFAULT                     _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_PER_DEFAULT_MASK                        _MK_MASK_CONST(0x1)
#define TIMER_TMR_PTV_0_PER_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_PER_SW_DEFAULT_MASK                     _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_PER_DISABLE                     _MK_ENUM_CONST(0)
#define TIMER_TMR_PTV_0_PER_ENABLE                      _MK_ENUM_CONST(1)

#define TIMER_TMR_PTV_0_TMR_PTV_SHIFT                   _MK_SHIFT_CONST(0)
#define TIMER_TMR_PTV_0_TMR_PTV_FIELD                   _MK_FIELD_CONST(0x1fffffff, TIMER_TMR_PTV_0_TMR_PTV_SHIFT)
#define TIMER_TMR_PTV_0_TMR_PTV_RANGE                   28:0
#define TIMER_TMR_PTV_0_TMR_PTV_WOFFSET                 0x0
#define TIMER_TMR_PTV_0_TMR_PTV_DEFAULT                 _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_TMR_PTV_DEFAULT_MASK                    _MK_MASK_CONST(0x1fffffff)
#define TIMER_TMR_PTV_0_TMR_PTV_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define TIMER_TMR_PTV_0_TMR_PTV_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


// Register TIMER_TMR_PCR_0
#define TIMER_TMR_PCR_0                 _MK_ADDR_CONST(0x4)
#define TIMER_TMR_PCR_0_SECURE                  0x0
#define TIMER_TMR_PCR_0_WORD_COUNT                      0x1
#define TIMER_TMR_PCR_0_RESET_VAL                       _MK_MASK_CONST(0x0)
#define TIMER_TMR_PCR_0_RESET_MASK                      _MK_MASK_CONST(0x5fffffff)
#define TIMER_TMR_PCR_0_SW_DEFAULT_VAL                  _MK_MASK_CONST(0x0)
#define TIMER_TMR_PCR_0_SW_DEFAULT_MASK                         _MK_MASK_CONST(0x0)
#define TIMER_TMR_PCR_0_READ_MASK                       _MK_MASK_CONST(0x5fffffff)
#define TIMER_TMR_PCR_0_WRITE_MASK                      _MK_MASK_CONST(0x40000000)
#define TIMER_TMR_PCR_0_INTR_CLR_SHIFT                  _MK_SHIFT_CONST(30)
#define TIMER_TMR_PCR_0_INTR_CLR_FIELD                  _MK_FIELD_CONST(0x1, TIMER_TMR_PCR_0_INTR_CLR_SHIFT)
#define TIMER_TMR_PCR_0_INTR_CLR_RANGE                  30:30
#define TIMER_TMR_PCR_0_INTR_CLR_WOFFSET                        0x0
#define TIMER_TMR_PCR_0_INTR_CLR_DEFAULT                        _MK_MASK_CONST(0x0)
#define TIMER_TMR_PCR_0_INTR_CLR_DEFAULT_MASK                   _MK_MASK_CONST(0x1)
#define TIMER_TMR_PCR_0_INTR_CLR_SW_DEFAULT                     _MK_MASK_CONST(0x0)
#define TIMER_TMR_PCR_0_INTR_CLR_SW_DEFAULT_MASK                        _MK_MASK_CONST(0x0)

#define TIMER_TMR_PCR_0_TMR_PCV_SHIFT                   _MK_SHIFT_CONST(0)
#define TIMER_TMR_PCR_0_TMR_PCV_FIELD                   _MK_FIELD_CONST(0x1fffffff, TIMER_TMR_PCR_0_TMR_PCV_SHIFT)
#define TIMER_TMR_PCR_0_TMR_PCV_RANGE                   28:0
#define TIMER_TMR_PCR_0_TMR_PCV_WOFFSET                 0x0
#define TIMER_TMR_PCR_0_TMR_PCV_DEFAULT                 _MK_MASK_CONST(0x0)
#define TIMER_TMR_PCR_0_TMR_PCV_DEFAULT_MASK                    _MK_MASK_CONST(0x1fffffff)
#define TIMER_TMR_PCR_0_TMR_PCV_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define TIMER_TMR_PCR_0_TMR_PCV_SW_DEFAULT_MASK                 _MK_MASK_CONST(0x0)


//
// REGISTER LIST
//
#define LIST_ARTIMER_REGS(_op_) \
_op_(TIMER_TMR_PTV_0) \
_op_(TIMER_TMR_PCR_0)


//
// ADDRESS SPACES
//

#define BASE_ADDRESS_TIMER      0x00000000

//
// ARTIMER REGISTER BANKS
//

#define TIMER0_FIRST_REG 0x0000 // TIMER_TMR_PTV_0
#define TIMER0_LAST_REG 0x0004 // TIMER_TMR_PCR_0

// To satisfy various compilers and platforms,
// we let users control the types and syntax of certain constants, using macros.
#ifndef _MK_SHIFT_CONST
  #define _MK_SHIFT_CONST(_constant_) _constant_
#endif
#ifndef _MK_MASK_CONST
  #define _MK_MASK_CONST(_constant_) _constant_
#endif
#ifndef _MK_ENUM_CONST
  #define _MK_ENUM_CONST(_constant_) (_constant_ ## UL)
#endif
#ifndef _MK_ADDR_CONST
  #define _MK_ADDR_CONST(_constant_) _constant_
#endif
#ifndef _MK_FIELD_CONST
  #define _MK_FIELD_CONST(_mask_, _shift_) (_MK_MASK_CONST(_mask_) << _MK_SHIFT_CONST(_shift_))
#endif

#endif // ifndef ___ARTIMER_H_INC_
