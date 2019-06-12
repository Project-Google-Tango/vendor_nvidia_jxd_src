/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.nvidia.inputmethod.keyboard;

import android.util.Log;
import android.util.SparseArray;

import com.nvidia.inputmethod.keyboard.internal.KeyVisualAttributes;
import com.nvidia.inputmethod.keyboard.internal.KeyboardIconsSet;
import com.nvidia.inputmethod.keyboard.internal.KeyboardParams;
import com.nvidia.inputmethod.latin.CollectionUtils;

import java.util.ListIterator;
import java.util.TreeMap;
import java.util.Collections;
import java.util.ArrayList;
import java.util.Comparator;


/**
 * Loads an XML description of a keyboard and stores the attributes of the keys. A keyboard
 * consists of rows of keys.
 * <p>The layout file for a keyboard contains XML that looks like the following snippet:</p>
 * <pre>
 * &lt;Keyboard
 *         latin:keyWidth="%10p"
 *         latin:keyHeight="50px"
 *         latin:horizontalGap="2px"
 *         latin:verticalGap="2px" &gt;
 *     &lt;Row latin:keyWidth="32px" &gt;
 *         &lt;Key latin:keyLabel="A" /&gt;
 *         ...
 *     &lt;/Row&gt;
 *     ...
 * &lt;/Keyboard&gt;
 * </pre>
 */
public class Keyboard {
    private static final String TAG = Keyboard.class.getSimpleName();

    /** Some common keys code. Must be positive.
     * These should be aligned with values/keycodes.xml
     */
    public static final int CODE_ENTER = '\n';
    public static final int CODE_TAB = '\t';
    public static final int CODE_SPACE = ' ';
    public static final int CODE_PERIOD = '.';
    public static final int CODE_DASH = '-';
    public static final int CODE_SINGLE_QUOTE = '\'';
    public static final int CODE_DOUBLE_QUOTE = '"';
    public static final int CODE_QUESTION_MARK = '?';
    public static final int CODE_EXCLAMATION_MARK = '!';
    // TODO: Check how this should work for right-to-left languages. It seems to stand
    // that for rtl languages, a closing parenthesis is a left parenthesis. Is this
    // managed by the font? Or is it a different char?
    public static final int CODE_CLOSING_PARENTHESIS = ')';
    public static final int CODE_CLOSING_SQUARE_BRACKET = ']';
    public static final int CODE_CLOSING_CURLY_BRACKET = '}';
    public static final int CODE_CLOSING_ANGLE_BRACKET = '>';

    /** Special keys code. Must be negative.
     * These should be aligned with KeyboardCodesSet.ID_TO_NAME[],
     * KeyboardCodesSet.DEFAULT[] and KeyboardCodesSet.RTL[]
     */
    public static final int CODE_SHIFT = -1;
    public static final int CODE_SWITCH_ALPHA_SYMBOL = -2;
    public static final int CODE_OUTPUT_TEXT = -3;
    public static final int CODE_DELETE = -4;
    public static final int CODE_SETTINGS = -5;
    public static final int CODE_SHORTCUT = -6;
    public static final int CODE_ACTION_ENTER = -7;
    public static final int CODE_ACTION_NEXT = -8;
    public static final int CODE_ACTION_PREVIOUS = -9;
    public static final int CODE_LANGUAGE_SWITCH = -10;
    public static final int CODE_RESEARCH = -11;
    // Code value representing the code is not specified.
    public static final int CODE_UNSPECIFIED = -12;

    public static boolean aPressed = false;
    public static boolean delPressed = false;
    public static boolean lThumbPressed = false;
    public static boolean rBumpPressed = false;
    public static boolean lBumpPressed = false;
    public static boolean spacePressed = false;
    public static boolean alphaPressed = false;

    public final KeyboardId mId;
    public final int mThemeId;

    /** Total height of the keyboard, including the padding and keys */
    public final int mOccupiedHeight;
    /** Total width of the keyboard, including the padding and keys */
    public final int mOccupiedWidth;

    /** The padding above the keyboard */
    public final int mTopPadding;
    /** Default gap between rows */
    public final int mVerticalGap;

    /** Per keyboard key visual parameters */
    public final KeyVisualAttributes mKeyVisualAttributes;

    public final int mMostCommonKeyHeight;
    public final int mMostCommonKeyWidth;

    /** More keys keyboard template */
    public final int mMoreKeysTemplate;

    /** Maximum column for more keys keyboard */
    public final int mMaxMoreKeysKeyboardColumn;

    /** Array of keys and icons in this keyboard */
    public final Key[] mKeys;
    public final Key[] mShiftKeys;
    public final Key[] mAltCodeKeysWhileTyping;
    public final KeyboardIconsSet mIconsSet;

    private final SparseArray<Key> mKeyCache = CollectionUtils.newSparseArray();
    protected ArrayList<ArrayList<Key>> sortedKeys = new ArrayList<ArrayList<Key>>(4);
    private TreeMap<Integer, Integer> height2rowMap = new TreeMap<Integer, Integer>();

    private final ProximityInfo mProximityInfo;
    private final boolean mProximityCharsCorrectionEnabled;

    protected Key selectedKey;
    protected Key lastSelectedKey;

    public boolean mCanLoopHorizontal = true;
    public boolean mCanLoopVertical = true;

    public Keyboard(final KeyboardParams params) {
        mId = params.mId;
        mThemeId = params.mThemeId;
        mOccupiedHeight = params.mOccupiedHeight;
        mOccupiedWidth = params.mOccupiedWidth;
        mMostCommonKeyHeight = params.mMostCommonKeyHeight;
        mMostCommonKeyWidth = params.mMostCommonKeyWidth;
        mMoreKeysTemplate = params.mMoreKeysTemplate;
        mMaxMoreKeysKeyboardColumn = params.mMaxMoreKeysKeyboardColumn;
        mKeyVisualAttributes = params.mKeyVisualAttributes;
        mTopPadding = params.mTopPadding;
        mVerticalGap = params.mVerticalGap;

        mKeys = params.mKeys.toArray(new Key[params.mKeys.size()]);
        mShiftKeys = params.mShiftKeys.toArray(new Key[params.mShiftKeys.size()]);
        mAltCodeKeysWhileTyping = params.mAltCodeKeysWhileTyping.toArray(
                new Key[params.mAltCodeKeysWhileTyping.size()]);
        mIconsSet = params.mIconsSet;

        mProximityInfo = new ProximityInfo(params.mId.mLocale.toString(),
                params.GRID_WIDTH, params.GRID_HEIGHT, mOccupiedWidth, mOccupiedHeight,
                mMostCommonKeyWidth, mMostCommonKeyHeight, mKeys, params.mTouchPositionCorrection);
        mProximityCharsCorrectionEnabled = params.mProximityCharsCorrectionEnabled;
        //Quickly get the heights of the 4 rows and map them to 0-3 in height2rowMap

        //create a mapping from key height to row in the keyboard
        for (int i = 0; i < mKeys.length; i++) {
            height2rowMap.put(mKeys[i].mY,1);
        }
        int k = 0;
        int lastKey = height2rowMap.firstKey();
        height2rowMap.put(lastKey,k);
        for (k = 1; k < height2rowMap.size(); k++) {
            lastKey = height2rowMap.higherKey(lastKey);
            height2rowMap.put(lastKey,k);
        }

        //allocate new array list objects for the array list of array lists
        for (int i = 0; i < height2rowMap.size(); i++) {
            sortedKeys.add(new ArrayList<Key>());
        }
        //if the key is not an invisible spacer key, place it in the array list in the correct row
        for (int i = 0; i < mKeys.length; i++) {
            if (!mKeys[i].isSpacer()) {
                sortedKeys.get(height2rowMap.get(mKeys[i].mY)).add(mKeys[i]);
            }
        }
        //sort the keys left to right in each row
        for (int i = 0; i < sortedKeys.size(); i++) {
            Collections.sort(sortedKeys.get(i),new Comparator <Key>(){
            @Override
            public int compare(Key arg0, Key arg1) {
                return arg0.mX - arg1.mX;
            }});
        }
        //let the keys know where they are
        for (int i = 0; i < sortedKeys.size();i++){
            for (int j = 0; j < sortedKeys.get(i).size(); j++){
                sortedKeys.get(i).get(j).x_ind = j;
                sortedKeys.get(i).get(j).y_ind = i;
            }
        }
    }

    public void selectShortcut() {
        if (mId.mHasShortcutKey && getKey(CODE_SHORTCUT) != null) {
            clearAllKeySelection();
            this.selectedKey = getKey(CODE_SHORTCUT);
            this.selectedKey.onSelected();
        } else {
            selectDefault();
        }
        if (this.selectedKey == null) {
            selectDefault();
        }
    }
    public Key getSelected() {
        return selectedKey;
    }

    public void selectDefault(boolean morewords) {
        clearAllKeySelection();
        selectedKey = null;
        if (morewords) {
            if (sortedKeys.size() > 0 && sortedKeys.get(0).size() > 1 ){
                this.selectedKey = sortedKeys.get(0).get(1);
            } else if (sortedKeys.size() > 0 && sortedKeys.get(0).size() > 0) {
                this.selectedKey = sortedKeys.get(0).get(0);
            }
        } else {
            if (mId.mHasShortcutKey) {
                this.selectedKey = getKey(CODE_SHORTCUT);
            }
            if(selectedKey == null) {
                this.selectedKey = getCenter();
            }
        }
        this.selectedKey.onSelected();
    }
    public void clearLast() {
        lastSelectedKey = null;
    }
    public void selectDefault() {
        selectDefault(false);
    }
    private Key getCenter() {

        if(sortedKeys.size() > 2 && sortedKeys.get(1) != null){
            return sortedKeys.get(1).get(sortedKeys.get(1).size()/2);
        } else {
            return sortedKeys.get(0).get(sortedKeys.get(0).size()/2);
        }
    }
    public void leaveFocus() {
        lastSelectedKey = selectedKey;
        clearAllKeySelection();
    }

    public void gainFocus(int a) {
        int focusKey = 1;
        if (a == 1) {
            focusKey = 4;
        } else if (a == 2) {
            focusKey = 8;
        }
        this.selectedKey = sortedKeys.get(0).get(focusKey);
        this.selectedKey.onSelected();
        lastSelectedKey = selectedKey;
    }


    public void clearAllKeySelection() {
        for (int i = 0; i < mKeys.length; i++) {
            mKeys[i].onDeselected();
            mKeys[i].onReleased();
            if ( (mKeys[i].mCode == CODE_SHIFT && (Keyboard.lThumbPressed || Keyboard.rBumpPressed || Keyboard.lBumpPressed) ) ||
                    (mKeys[i].mCode == CODE_DELETE && Keyboard.delPressed) ||
                    (mKeys[i].mCode == CODE_SWITCH_ALPHA_SYMBOL && Keyboard.alphaPressed) ||
                    (mKeys[i].mCode == 32 && Keyboard.spacePressed) ) {
                mKeys[i].onPressed();
            }
        }
    }

    /**
    * Returns true if b < a < c or if c < a < b
    * that is, if a is between b and c
    **/
    private static boolean between(int a, int b, int c) {
        if ((a >= b && a <= c) || (a >= c && a <= b)) {
            return true;
        }
        return false;
    }

    /**
    * Will select the closet key from this keyboard from the input key in the given row.
    * If row is -1 it will select the closet key from the row that Key is in.
    **/
    public void selectClosest(Key key, int row) {
        clearAllKeySelection();
        ArrayList <Key> tRow;
        if (row >= 0 && row < sortedKeys.size()) {
            tRow = sortedKeys.get(row);
        } else if (row == -1 && key != null) {
            if (height2rowMap.get(key.mY) != null) {
                if (sortedKeys.size() > height2rowMap.get(key.mY)) {
                    tRow = sortedKeys.get( height2rowMap.get(key.mY) );
                }
                else {
                    selectDefault();
                    return;
                }
            } else {
                selectedKey.onSelected();
                Log.e(TAG,"ERROR: no closest key to "+key);
                return;
            }
        } else {
            Log.e(TAG,"ERROR: Invalid row selected");
            if( this.selectedKey == null ) {
                selectDefault();
            } else {
                selectedKey.onSelected();
            }
            return;
        }


        if (key == null) {
            Log.e(TAG,"ERROR: key is null");
            if( this.selectedKey == null ) {
                selectDefault();
            }
            return;
        }

        Key closest = tRow.get(0);
        if (row == -1) {
            //If the x_ind is out of range
            if (tRow.size() <= key.x_ind) {
                closest = tRow.get(tRow.size()-1);
            } else {
                closest = tRow.get(key.x_ind);
            }
        } else {
            for (Key a : tRow) {
                if (a.mWidth != key.mWidth){
                    if ( between(key.mX, a.mX, a.mX+a.mWidth) && between(key.mX+ key.mWidth, a.mX, a.mX+a.mWidth) ) {
                        closest = a;
                        break;
                    }
                }
                if ( Math.abs(key.mX - a.mX) < Math.abs(key.mX - closest.mX) ) {
                    closest = a;
                }
            }
        }
        closest.onSelected();
        this.selectedKey = closest;
    }

    public void selectRight() {
        clearAllKeySelection();
        if (selectedKey == null) {
            selectDefault();
            return;
        }
        int newRow = height2rowMap.get(selectedKey.mY);
        if ( newRow < sortedKeys.size() && newRow >= 0 ) {
            ArrayList <Key> tRow = sortedKeys.get(newRow);
            ListIterator <Key> it = tRow.listIterator();
            while ( it.hasNext() ) {
                Key next = it.next();
                if (selectedKey.equals(next) && it.hasNext()) {
                    selectedKey.onDeselected();
                    selectedKey = it.next();
                    lastSelectedKey = selectedKey;
                    break;
                } else if (selectedKey.equals(next) && !it.hasNext() && mCanLoopHorizontal) {
                    lastSelectedKey = null;
                    selectedKey = tRow.get(0);
                }
            }
            selectedKey.onSelected();
        }
    }

    public void selectLeft() {
        clearAllKeySelection();
        if (selectedKey == null) {
            selectDefault();
            return;
        }
        int newRow = height2rowMap.get(selectedKey.mY);
        if ( newRow < sortedKeys.size() && newRow >= 0 ) {
            ArrayList <Key> tRow = sortedKeys.get(newRow);
            ListIterator <Key> it = tRow.listIterator();
            while ( it.hasNext() ) {
                if (selectedKey.equals(it.next())) {
                    it.previous();
                    if (it.hasPrevious()) {
                        selectedKey.onDeselected();
                        selectedKey = it.previous();
                        lastSelectedKey = selectedKey;
                    } else if (mCanLoopHorizontal) {
                        lastSelectedKey = null;
                        selectedKey = tRow.get(tRow.size()-1);
                    }
                    selectedKey.onSelected();
                    return;
                }
            }
        }
    }

    protected void selectVert(boolean up) {
        final int direction = up ? -1 : 1;
        if (selectedKey == null) {
            selectDefault();
            return;
        }
        int newRow = height2rowMap.get(selectedKey.mY) + direction;
        if ( newRow < sortedKeys.size() && newRow >= 0 || mCanLoopVertical) {
            if (newRow >= sortedKeys.size()) {
                newRow = 0;
            } else if (newRow < 0) {
                newRow = sortedKeys.size() - 1;
            }

            clearAllKeySelection();
            selectedKey.onDeselected();
            if (lastSelectedKey != null && newRow == height2rowMap.get(lastSelectedKey.mY) ) {
                Key tmp = lastSelectedKey;
                lastSelectedKey = selectedKey;
                selectedKey = tmp;
                selectedKey.onSelected();
            } else {
                //if the key is space don't save it as the last key
                if (selectedKey.mCode == 32 && lastSelectedKey != null) {
                    selectPosition(lastSelectedKey, newRow);
                } else {
                    lastSelectedKey = selectedKey;
                    selectClosest(selectedKey, newRow);
                }
            }
        }
    }

    public void selectPosition(Key select, int row) {
        if (sortedKeys.get(row).size() > select.x_ind) {
            this.selectedKey = sortedKeys.get(row).get(select.x_ind);
            selectedKey.onSelected();
        } else {
            selectClosest(select, row);
        }
    }

    public boolean atTop() {
        return height2rowMap.get(selectedKey.mY) == 0;
    }
    /**
    * this method returns 0 1 or 2 depending on
    * if the actively selected key is in the first 3 positions
    * next 4, or in the last 3 positions of the top row
    */
    public int getSuggestedIndex() {
        int i = 0;
        int newRow = height2rowMap.get(selectedKey.mY);
        if ( newRow < sortedKeys.size() && newRow >= 0 ) {
            ArrayList <Key> tRow = sortedKeys.get(newRow);
            ListIterator <Key> it = tRow.listIterator();
            while ( it.hasNext() ) {
                if (selectedKey.equals(it.next()) || i > 6) {
                    if(i > 6) {
                        return 2;
                    }
                    else if (i > 2){
                        return 1;
                    } else {
                        return 0;
                    }
                }
                i++;
            }
            selectedKey.onSelected();
        }
        return 0;
    }

    public void selectUp() {
        selectVert(true);
    }

    public void selectDown() {
        selectVert(false);
    }

    public boolean hasProximityCharsCorrection(final int code) {
        if (!mProximityCharsCorrectionEnabled) {
            return false;
        }
        // Note: The native code has the main keyboard layout only at this moment.
        // TODO: Figure out how to handle proximity characters information of all layouts.
        final boolean canAssumeNativeHasProximityCharsInfoOfAllKeys = (
                mId.mElementId == KeyboardId.ELEMENT_ALPHABET
                || mId.mElementId == KeyboardId.ELEMENT_ALPHABET_AUTOMATIC_SHIFTED);
        return canAssumeNativeHasProximityCharsInfoOfAllKeys || Character.isLetter(code);
    }

    public ProximityInfo getProximityInfo() {
        return mProximityInfo;
    }

    public void dumpBoard() {
        Log.e(TAG, "KEYBOARD " +mKeys.length);
        for (ArrayList<Key> a : sortedKeys) {
            for (Key k:a){
                Log.e(TAG, "KEYBOARD " + k);
            }
        }
    }

    public Key getKey(final int code) {
        if (code == CODE_UNSPECIFIED) {
            return null;
        }
        synchronized (mKeyCache) {
            final int index = mKeyCache.indexOfKey(code);
            if (index >= 0) {
                return mKeyCache.valueAt(index);
            }

            for (final Key key : mKeys) {
                if (key.mCode == code) {
                    mKeyCache.put(code, key);
                    return key;
                }
            }
            mKeyCache.put(code, null);
            return null;
        }
    }

    public boolean hasKey(int codey) {
        if (codey == CODE_UNSPECIFIED) {
            return false;
        }

        for (final Key key : mKeys) {
            if (codey == key.mCode) {
                return true;
            }
        }
        return false;
    }

    public boolean hasKey(final Key aKey) {
        if (aKey.mCode == CODE_UNSPECIFIED) {
            return false;
        }

        if (mKeyCache.indexOfValue(aKey) >= 0) {
            return true;
        }

        for (final Key key : mKeys) {
            if (key == aKey) {
                mKeyCache.put(key.mCode, key);
                return true;
            }
        }
        return false;
    }

    public static boolean isLetterCode(final int code) {
        return code >= CODE_SPACE;
    }

    @Override
    public String toString() {
        return mId.toString();
    }

    /**
     * Returns the array of the keys that are closest to the given point.
     * @param x the x-coordinate of the point
     * @param y the y-coordinate of the point
     * @return the array of the nearest keys to the given point. If the given
     * point is out of range, then an array of size zero is returned.
     */
    public Key[] getNearestKeys(final int x, final int y) {
        // Avoid dead pixels at edges of the keyboard
        final int adjustedX = Math.max(0, Math.min(x, mOccupiedWidth - 1));
        final int adjustedY = Math.max(0, Math.min(y, mOccupiedHeight - 1));
        return mProximityInfo.getNearestKeys(adjustedX, adjustedY);
    }

    public static String printableCode(final int code) {
        switch (code) {
        case CODE_SHIFT: return "shift";
        case CODE_SWITCH_ALPHA_SYMBOL: return "symbol";
        case CODE_OUTPUT_TEXT: return "text";
        case CODE_DELETE: return "delete";
        case CODE_SETTINGS: return "settings";
        case CODE_SHORTCUT: return "shortcut";
        case CODE_ACTION_ENTER: return "actionEnter";
        case CODE_ACTION_NEXT: return "actionNext";
        case CODE_ACTION_PREVIOUS: return "actionPrevious";
        case CODE_LANGUAGE_SWITCH: return "languageSwitch";
        case CODE_UNSPECIFIED: return "unspec";
        case CODE_TAB: return "tab";
        case CODE_ENTER: return "enter";
        default:
            if (code <= 0) Log.w(TAG, "Unknown non-positive key code=" + code);
            if (code < CODE_SPACE) return String.format("'\\u%02x'", code);
            if (code < 0x100) return String.format("'%c'", code);
            return String.format("'\\u%04x'", code);
        }
    }
}
