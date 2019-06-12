/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.tests.graphics.nvblit;

public class NvBlitTestResult
{
    public static final int RESULT_FAIL = 0;
    public static final int RESULT_PASS = 1;
    public static final int RESULT_UNKNOWN = 2;

    private int result;

    public NvBlitTestResult() {
        result = RESULT_UNKNOWN;
    }

    public NvBlitTestResult(int result) {
        this.result = result;
    }

    public int getResult() {
        return result;
    }

    public void setResult(int result) {
        this.result = result;
    }

}
