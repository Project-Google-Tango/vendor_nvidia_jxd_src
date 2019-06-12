/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package com.nvidia.inputmethod.research;

import com.nvidia.inputmethod.keyboard.Keyboard;

public class Statistics {
    // Number of characters entered during a typing session
    int mCharCount;
    // Number of letter characters entered during a typing session
    int mLetterCount;
    // Number of number characters entered
    int mNumberCount;
    // Number of space characters entered
    int mSpaceCount;
    // Number of delete operations entered (taps on the backspace key)
    int mDeleteKeyCount;
    // Number of words entered during a session.
    int mWordCount;
    // Whether the text field was empty upon editing
    boolean mIsEmptyUponStarting;
    boolean mIsEmptinessStateKnown;

    // Timers to count average time to enter a key, first press a delete key,
    // between delete keys, and then to return typing after a delete key.
    final AverageTimeCounter mKeyCounter = new AverageTimeCounter();
    final AverageTimeCounter mBeforeDeleteKeyCounter = new AverageTimeCounter();
    final AverageTimeCounter mDuringRepeatedDeleteKeysCounter = new AverageTimeCounter();
    final AverageTimeCounter mAfterDeleteKeyCounter = new AverageTimeCounter();

    static class AverageTimeCounter {
        int mCount;
        int mTotalTime;

        public void reset() {
            mCount = 0;
            mTotalTime = 0;
        }

        public void add(long deltaTime) {
            mCount++;
            mTotalTime += deltaTime;
        }

        public int getAverageTime() {
            if (mCount == 0) {
                return 0;
            }
            return mTotalTime / mCount;
        }
    }

    // To account for the interruptions when the user's attention is directed elsewhere, times
    // longer than MIN_TYPING_INTERMISSION are not counted when estimating this statistic.
    public static final int MIN_TYPING_INTERMISSION = 2 * 1000;  // in milliseconds
    public static final int MIN_DELETION_INTERMISSION = 10 * 1000;  // in milliseconds

    // The last time that a tap was performed
    private long mLastTapTime;
    // The type of the last keypress (delete key or not)
    boolean mIsLastKeyDeleteKey;

    private static final Statistics sInstance = new Statistics();

    public static Statistics getInstance() {
        return sInstance;
    }

    private Statistics() {
        reset();
    }

    public void reset() {
        mCharCount = 0;
        mLetterCount = 0;
        mNumberCount = 0;
        mSpaceCount = 0;
        mDeleteKeyCount = 0;
        mWordCount = 0;
        mIsEmptyUponStarting = true;
        mIsEmptinessStateKnown = false;
        mKeyCounter.reset();
        mBeforeDeleteKeyCounter.reset();
        mDuringRepeatedDeleteKeysCounter.reset();
        mAfterDeleteKeyCounter.reset();

        mLastTapTime = 0;
        mIsLastKeyDeleteKey = false;
    }

    public void recordChar(int codePoint, long time) {
        final long delta = time - mLastTapTime;
        if (codePoint == Keyboard.CODE_DELETE) {
            mDeleteKeyCount++;
            if (delta < MIN_DELETION_INTERMISSION) {
                if (mIsLastKeyDeleteKey) {
                    mDuringRepeatedDeleteKeysCounter.add(delta);
                } else {
                    mBeforeDeleteKeyCounter.add(delta);
                }
            }
            mIsLastKeyDeleteKey = true;
        } else {
            mCharCount++;
            if (Character.isDigit(codePoint)) {
                mNumberCount++;
            }
            if (Character.isLetter(codePoint)) {
                mLetterCount++;
            }
            if (Character.isSpaceChar(codePoint)) {
                mSpaceCount++;
            }
            if (mIsLastKeyDeleteKey && delta < MIN_DELETION_INTERMISSION) {
                mAfterDeleteKeyCounter.add(delta);
            } else if (!mIsLastKeyDeleteKey && delta < MIN_TYPING_INTERMISSION) {
                mKeyCounter.add(delta);
            }
            mIsLastKeyDeleteKey = false;
        }
        mLastTapTime = time;
    }

    public void recordWordEntered() {
        mWordCount++;
    }

    public void setIsEmptyUponStarting(final boolean isEmpty) {
        mIsEmptyUponStarting = isEmpty;
        mIsEmptinessStateKnown = true;
    }
}
