/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H
#include <stdlib.h>
#include <string.h>

/* TODO: change this to be GFSDK function? */
#include <stdio.h> 
#define TEST_FAIL_W_MSG4(_MSG_,_A1_,_A2_,_A3_,_A4_) \
   ( printf("FAILED at %s:%d" _MSG_ "\n", __FILE__, __LINE__, (_A1_),(_A2_),(_A3_),(_A4_)), \
     fflush(stdout), \
     exit(1) \
   )
#define TEST_ASSERT4(_X_,_FMT_,_A1_,_A2_,_A3_,_A4_) \
   do { if (!(_X_)) TEST_FAIL_W_MSG4(": assertion " #_X_ " failed" _FMT_, _A1_,_A2_,_A3_,_A4_); } while(0)

#define TEST_FAIL_W_MSG(_MSG_) TEST_FAIL_W_MSG4(": " _MSG_,0,0,0,0) 
#define TEST_FAIL()            TEST_FAIL_W_MSG4("",0,0,0,0) 
#define TEST_FAIL_SWITCH_VALUE(_X_)     TEST_FAIL_W_MSG4(": unexpected switch value " #_X_ "= %lu%s%s%s",(unsigned long)(_X_),"","","")

#define TEST_ASSERT(_X_)         TEST_ASSERT4(_X_,        "%s%s%s%s","","","","")
#define TEST_ASSERT_EQ(_X_,_Y_)  TEST_ASSERT4(_X_ == _Y_, ": "#_X_" = %ld, "#_Y_" = %ld%s%s", (long)(_X_),(long)(_Y_),"","")
#define TEST_ASSERT_NE(_X_,_Y_)  TEST_ASSERT4(_X_ != _Y_, ": "#_X_" = %ld, "#_Y_" = %ld%s%s", (long)(_X_),(long)(_Y_),"","")
#define TEST_ASSERT_GT(_X_,_Y_)  TEST_ASSERT4(_X_ > _Y_,  ": "#_X_" = %ld, "#_Y_" = %ld%s%s", (long)(_X_),(long)(_Y_),"","")
#define TEST_ASSERT_LT(_X_,_Y_)  TEST_ASSERT4(_X_ < _Y_,  ": "#_X_" = %ld, "#_Y_" = %ld%s%s", (long)(_X_),(long)(_Y_),"","")
#define TEST_ASSERT_GE(_X_,_Y_)  TEST_ASSERT4(_X_ >= _Y_, ": "#_X_" = %ld, "#_Y_" = %ld%s%s", (long)(_X_),(long)(_Y_),"","")
#define TEST_ASSERT_LE(_X_,_Y_)  TEST_ASSERT4(_X_ <= _Y_, ": "#_X_" = %ld, "#_Y_" = %ld%s%s", (long)(_X_),(long)(_Y_),"","")

#define TEST_ASSERT_NOTERROR(_X_) TEST_ASSERT4((_X_) == OMX_ErrorNone, ": "#_X_" = %#lx%s%s%s", (long)(_X_),"","","")
#define TEST_ASSERT_ERROR(_X_)    TEST_ASSERT4((_X_) & 0x80000000, ": "#_X_" = %#lx%s%s%s", (long)(_X_),"","","")

#define TEST_SUCCEEDED()    ( printf("SUCCESS\n"), fflush(stdout), exit(0) )

#endif /*TEST_ASSERT_H*/
