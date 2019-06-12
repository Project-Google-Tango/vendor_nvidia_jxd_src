;/*
; * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All Rights Reserved.
; *
; * NVIDIA Corporation and its licensors retain all intellectual property and
; * proprietary rights in and to this software and related documentation.  Any
; * use, reproduction, disclosure or distribution of this software and related
; * documentation without an express license agreement from NVIDIA Corporation
; * is strictly prohibited.
; */

#if !__GNUC__

; override default arm library init functions

    PRESERVE8
    AREA |overridearmstuff|, CODE, READONLY

    EXPORT _sys_exit
    EXPORT __rt_raise
    EXPORT _fp_init

_sys_exit
__rt_raise
_fp_init
    bx lr

    END

#endif // !__GNUC__
