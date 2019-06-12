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

package com.nvidia.inputmethod.latin;

import android.inputmethodservice.InputMethodService;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;

import com.nvidia.inputmethod.keyboard.Keyboard;
import com.nvidia.inputmethod.latin.define.ProductionFlag;
import com.nvidia.inputmethod.research.ResearchLogger;

import java.util.Locale;
import java.util.regex.Pattern;

/**
 * Enrichment class for InputConnection to simplify interaction and add functionality.
 *
 * This class serves as a wrapper to be able to simply add hooks to any calls to the underlying
 * InputConnection. It also keeps track of a number of things to avoid having to call upon IPC
 * all the time to find out what text is in the buffer, when we need it to determine caps mode
 * for example.
 */
public final class RichInputConnection {
    private static final String TAG = RichInputConnection.class.getSimpleName();
    private static final boolean DBG = false;
    private static final boolean DEBUG_PREVIOUS_TEXT = false;
    private static final boolean DEBUG_BATCH_NESTING = false;
    // Provision for a long word pair and a separator
    private static final int LOOKBACK_CHARACTER_NUM = BinaryDictionary.MAX_WORD_LENGTH * 2 + 1;
    private static final Pattern spaceRegex = Pattern.compile("\\s+");
    private static final int INVALID_CURSOR_POSITION = -1;

    /**
     * This variable contains the value LatinIME thinks the cursor position should be at now.
     * This is a few steps in advance of what the TextView thinks it is, because TextView will
     * only know after the IPC calls gets through.
     */
    private int mCurrentCursorPosition = INVALID_CURSOR_POSITION; // in chars, not code points
    /**
     * This contains the committed text immediately preceding the cursor and the composing
     * text if any. It is refreshed when the cursor moves by calling upon the TextView.
     */
    private StringBuilder mCommittedTextBeforeComposingText = new StringBuilder();
    /**
     * This contains the currently composing text, as LatinIME thinks the TextView is seeing it.
     */
    private StringBuilder mComposingText = new StringBuilder();
    /**
     * This is a one-character string containing the character after the cursor. Since LatinIME
     * never touches it directly, it's never modified by any means other than re-reading from the
     * TextView when the cursor position is changed by the user.
     */
    private CharSequence mCharAfterTheCursor = "";
    // A hint on how many characters to cache from the TextView. A good value of this is given by
    // how many characters we need to be able to almost always find the caps mode.
    private static final int DEFAULT_TEXT_CACHE_SIZE = 100;

    private final InputMethodService mParent;
    InputConnection mIC;
    int mNestLevel;
    public RichInputConnection(final InputMethodService parent) {
        mParent = parent;
        mIC = null;
        mNestLevel = 0;
    }

    private void checkConsistencyForDebug() {
        final ExtractedTextRequest r = new ExtractedTextRequest();
        r.hintMaxChars = 0;
        r.hintMaxLines = 0;
        r.token = 1;
        r.flags = 0;
        final ExtractedText et = mIC.getExtractedText(r, 0);
        final CharSequence beforeCursor = getTextBeforeCursor(DEFAULT_TEXT_CACHE_SIZE, 0);
        final StringBuilder internal = new StringBuilder().append(mCommittedTextBeforeComposingText)
                .append(mComposingText);
        if (null == et || null == beforeCursor) return;
        final int actualLength = Math.min(beforeCursor.length(), internal.length());
        if (internal.length() > actualLength) {
            internal.delete(0, internal.length() - actualLength);
        }
        final String reference = (beforeCursor.length() <= actualLength) ? beforeCursor.toString()
                : beforeCursor.subSequence(beforeCursor.length() - actualLength,
                        beforeCursor.length()).toString();
        if (et.selectionStart != mCurrentCursorPosition
                || !(reference.equals(internal.toString()))) {
            final String context = "Expected cursor position = " + mCurrentCursorPosition
                    + "\nActual cursor position = " + et.selectionStart
                    + "\nExpected text = " + internal.length() + " " + internal
                    + "\nActual text = " + reference.length() + " " + reference;
            ((LatinIME)mParent).debugDumpStateAndCrashWithException(context);
        } else {
            Log.e(TAG, Utils.getStackTrace(2));
            Log.e(TAG, "Exp <> Actual : " + mCurrentCursorPosition + " <> " + et.selectionStart);
        }
    }

    public void beginBatchEdit() {
        if (++mNestLevel == 1) {
            mIC = mParent.getCurrentInputConnection();
            if (null != mIC) {
                mIC.beginBatchEdit();
            }
        } else {
            if (DBG) {
                throw new RuntimeException("Nest level too deep");
            } else {
                Log.e(TAG, "Nest level too deep : " + mNestLevel);
            }
        }
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
    }

    public void endBatchEdit() {
        if (mNestLevel <= 0) Log.e(TAG, "Batch edit not in progress!"); // TODO: exception instead
        if (--mNestLevel == 0 && null != mIC) {
            mIC.endBatchEdit();
        }
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
    }

    public void resetCachesUponCursorMove(final int newCursorPosition) {
        mCurrentCursorPosition = newCursorPosition;
        mComposingText.setLength(0);
        mCommittedTextBeforeComposingText.setLength(0);
        final CharSequence textBeforeCursor = getTextBeforeCursor(DEFAULT_TEXT_CACHE_SIZE, 0);
        if (null != textBeforeCursor) mCommittedTextBeforeComposingText.append(textBeforeCursor);
        mCharAfterTheCursor = getTextAfterCursor(1, 0);
        if (null != mIC) {
            mIC.finishComposingText();
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_finishComposingText();
            }
        }
    }

    private void checkBatchEdit() {
        if (mNestLevel != 1) {
            // TODO: exception instead
            Log.e(TAG, "Batch edit level incorrect : " + mNestLevel);
            Log.e(TAG, Utils.getStackTrace(4));
        }
    }

    public void finishComposingText() {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
        mCommittedTextBeforeComposingText.append(mComposingText);
        mCurrentCursorPosition += mComposingText.length();
        mComposingText.setLength(0);
        if (null != mIC) {
            mIC.finishComposingText();
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_finishComposingText();
            }
        }
    }

    public void commitText(final CharSequence text, final int i) {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
        mCommittedTextBeforeComposingText.append(text);
        mCurrentCursorPosition += text.length() - mComposingText.length();
        mComposingText.setLength(0);
        if (null != mIC) {
            mIC.commitText(text, i);
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_commitText(text, i);
            }
        }
    }

    /**
     * Gets the caps modes we should be in after this specific string.
     *
     * This returns a bit set of TextUtils#CAP_MODE_*, masked by the inputType argument.
     * This method also supports faking an additional space after the string passed in argument,
     * to support cases where a space will be added automatically, like in phantom space
     * state for example.
     * Note that for English, we are using American typography rules (which are not specific to
     * American English, it's just the most common set of rules for English).
     *
     * @param inputType a mask of the caps modes to test for.
     * @param locale what language should be considered.
     * @param hasSpaceBefore if we should consider there should be a space after the string.
     * @return the caps modes that should be on as a set of bits
     */
    public int getCursorCapsMode(final int inputType, final Locale locale,
            final boolean hasSpaceBefore) {
        mIC = mParent.getCurrentInputConnection();
        if (null == mIC) return Constants.TextUtils.CAP_MODE_OFF;
        if (!TextUtils.isEmpty(mComposingText)) {
            if (hasSpaceBefore) {
                // If we have some composing text and a space before, then we should have
                // MODE_CHARACTERS and MODE_WORDS on.
                return (TextUtils.CAP_MODE_CHARACTERS | TextUtils.CAP_MODE_WORDS) & inputType;
            } else {
                // We have some composing text - we should be in MODE_CHARACTERS only.
                return TextUtils.CAP_MODE_CHARACTERS & inputType;
            }
        }
        // TODO: this will generally work, but there may be cases where the buffer contains SOME
        // information but not enough to determine the caps mode accurately. This may happen after
        // heavy pressing of delete, for example DEFAULT_TEXT_CACHE_SIZE - 5 times or so.
        // getCapsMode should be updated to be able to return a "not enough info" result so that
        // we can get more context only when needed.
        if (TextUtils.isEmpty(mCommittedTextBeforeComposingText) && 0 != mCurrentCursorPosition) {
            mCommittedTextBeforeComposingText.append(
                    getTextBeforeCursor(DEFAULT_TEXT_CACHE_SIZE, 0));
        }
        // This never calls InputConnection#getCapsMode - in fact, it's a static method that
        // never blocks or initiates IPC.
        return StringUtils.getCapsMode(mCommittedTextBeforeComposingText, inputType, locale,
                hasSpaceBefore);
    }

    public int getCodePointBeforeCursor() {
        if (mCommittedTextBeforeComposingText.length() < 1) return Constants.NOT_A_CODE;
        return Character.codePointBefore(mCommittedTextBeforeComposingText,
                mCommittedTextBeforeComposingText.length());
    }

    public CharSequence getTextBeforeCursor(final int i, final int j) {
        // TODO: use mCommittedTextBeforeComposingText if possible to improve performance
        mIC = mParent.getCurrentInputConnection();
        if (null != mIC) return mIC.getTextBeforeCursor(i, j);
        return null;
    }

    public CharSequence getTextAfterCursor(final int i, final int j) {
        mIC = mParent.getCurrentInputConnection();
        if (null != mIC) return mIC.getTextAfterCursor(i, j);
        return null;
    }

    public void deleteSurroundingText(final int i, final int j) {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        final int remainingChars = mComposingText.length() - i;
        if (remainingChars >= 0) {
            mComposingText.setLength(remainingChars);
        } else {
            mComposingText.setLength(0);
            // Never cut under 0
            final int len = Math.max(mCommittedTextBeforeComposingText.length()
                    + remainingChars, 0);
            mCommittedTextBeforeComposingText.setLength(len);
        }
        if (mCurrentCursorPosition > i) {
            mCurrentCursorPosition -= i;
        } else {
            mCurrentCursorPosition = 0;
        }
        if (null != mIC) {
            mIC.deleteSurroundingText(i, j);
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_deleteSurroundingText(i, j);
            }
        }
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
    }

    public void performEditorAction(final int actionId) {
        mIC = mParent.getCurrentInputConnection();
        if (null != mIC) {
            mIC.performEditorAction(actionId);
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_performEditorAction(actionId);
            }
        }
    }

    public void sendKeyEvent(final KeyEvent keyEvent) {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (keyEvent.getAction() == KeyEvent.ACTION_DOWN) {
            if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
            // This method is only called for enter or backspace when speaking to old
            // applications (target SDK <= 15), or for digits.
            // When talking to new applications we never use this method because it's inherently
            // racy and has unpredictable results, but for backward compatibility we continue
            // sending the key events for only Enter and Backspace because some applications
            // mistakenly catch them to do some stuff.
            switch (keyEvent.getKeyCode()) {
                case KeyEvent.KEYCODE_ENTER:
                    mCommittedTextBeforeComposingText.append("\n");
                    mCurrentCursorPosition += 1;
                    break;
                case KeyEvent.KEYCODE_DEL:
                    if (0 == mComposingText.length()) {
                        if (mCommittedTextBeforeComposingText.length() > 0) {
                            mCommittedTextBeforeComposingText.delete(
                                    mCommittedTextBeforeComposingText.length() - 1,
                                    mCommittedTextBeforeComposingText.length());
                        }
                    } else {
                        mComposingText.delete(mComposingText.length() - 1, mComposingText.length());
                    }
                    if (mCurrentCursorPosition > 0) mCurrentCursorPosition -= 1;
                    break;
                case KeyEvent.KEYCODE_UNKNOWN:
                    if (null != keyEvent.getCharacters()) {
                        mCommittedTextBeforeComposingText.append(keyEvent.getCharacters());
                        mCurrentCursorPosition += keyEvent.getCharacters().length();
                    }
                    break;
                default:
                    final String text = new String(new int[] { keyEvent.getUnicodeChar() }, 0, 1);
                    mCommittedTextBeforeComposingText.append(text);
                    mCurrentCursorPosition += text.length();
                    break;
            }
        }
        if (null != mIC) {
            mIC.sendKeyEvent(keyEvent);
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_sendKeyEvent(keyEvent);
            }
        }
    }

    public void setComposingRegion(final int start, final int end) {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
        mCurrentCursorPosition = end;
        final CharSequence textBeforeCursor =
                getTextBeforeCursor(DEFAULT_TEXT_CACHE_SIZE + (end - start), 0);
        final int indexOfStartOfComposingText =
                Math.max(textBeforeCursor.length() - (end - start), 0);
        mComposingText.append(textBeforeCursor.subSequence(indexOfStartOfComposingText,
                textBeforeCursor.length()));
        mCommittedTextBeforeComposingText.setLength(0);
        mCommittedTextBeforeComposingText.append(
                textBeforeCursor.subSequence(0, indexOfStartOfComposingText));
        if (null != mIC) {
            mIC.setComposingRegion(start, end);
        }
    }

    public void setComposingText(final CharSequence text, final int i) {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
        mCurrentCursorPosition += text.length() - mComposingText.length();
        mComposingText.setLength(0);
        mComposingText.append(text);
        // TODO: support values of i != 1. At this time, this is never called with i != 1.
        if (null != mIC) {
            mIC.setComposingText(text, i);
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_setComposingText(text, i);
            }
        }
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
    }

    public void setSelection(final int from, final int to) {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
        if (null != mIC) {
            mIC.setSelection(from, to);
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_setSelection(from, to);
            }
        }
        mCurrentCursorPosition = from;
        mCommittedTextBeforeComposingText.setLength(0);
        mCommittedTextBeforeComposingText.append(getTextBeforeCursor(DEFAULT_TEXT_CACHE_SIZE, 0));
    }

    public void commitCorrection(final CorrectionInfo correctionInfo) {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
        // This has no effect on the text field and does not change its content. It only makes
        // TextView flash the text for a second based on indices contained in the argument.
        if (null != mIC) {
            mIC.commitCorrection(correctionInfo);
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_commitCorrection(correctionInfo);
            }
        }
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
    }

    public void commitCompletion(final CompletionInfo completionInfo) {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
        final CharSequence text = completionInfo.getText();
        mCommittedTextBeforeComposingText.append(text);
        mCurrentCursorPosition += text.length() - mComposingText.length();
        mComposingText.setLength(0);
        if (null != mIC) {
            mIC.commitCompletion(completionInfo);
            if (ProductionFlag.IS_EXPERIMENTAL) {
                ResearchLogger.richInputConnection_commitCompletion(completionInfo);
            }
        }
        if (DEBUG_PREVIOUS_TEXT) checkConsistencyForDebug();
    }

    public CharSequence getNthPreviousWord(final String sentenceSeperators, final int n) {
        mIC = mParent.getCurrentInputConnection();
        if (null == mIC) return null;
        final CharSequence prev = mIC.getTextBeforeCursor(LOOKBACK_CHARACTER_NUM, 0);
        if (DEBUG_PREVIOUS_TEXT && null != prev) {
            final int checkLength = LOOKBACK_CHARACTER_NUM - 1;
            final String reference = prev.length() <= checkLength ? prev.toString()
                    : prev.subSequence(prev.length() - checkLength, prev.length()).toString();
            final StringBuilder internal = new StringBuilder()
                    .append(mCommittedTextBeforeComposingText).append(mComposingText);
            if (internal.length() > checkLength) {
                internal.delete(0, internal.length() - checkLength);
                if (!(reference.equals(internal.toString()))) {
                    final String context =
                            "Expected text = " + internal + "\nActual text = " + reference;
                    ((LatinIME)mParent).debugDumpStateAndCrashWithException(context);
                }
            }
        }
        return getNthPreviousWord(prev, sentenceSeperators, n);
    }

    /**
     * Represents a range of text, relative to the current cursor position.
     */
    public static final class Range {
        /** Characters before selection start */
        public final int mCharsBefore;

        /**
         * Characters after selection start, including one trailing word
         * separator.
         */
        public final int mCharsAfter;

        /** The actual characters that make up a word */
        public final String mWord;

        public Range(int charsBefore, int charsAfter, String word) {
            if (charsBefore < 0 || charsAfter < 0) {
                throw new IndexOutOfBoundsException();
            }
            this.mCharsBefore = charsBefore;
            this.mCharsAfter = charsAfter;
            this.mWord = word;
        }
    }

    private static boolean isSeparator(int code, String sep) {
        return sep.indexOf(code) != -1;
    }

    // Get the nth word before cursor. n = 1 retrieves the word immediately before the cursor,
    // n = 2 retrieves the word before that, and so on. This splits on whitespace only.
    // Also, it won't return words that end in a separator (if the nth word before the cursor
    // ends in a separator, it returns null).
    // Example :
    // (n = 1) "abc def|" -> def
    // (n = 1) "abc def |" -> def
    // (n = 1) "abc def. |" -> null
    // (n = 1) "abc def . |" -> null
    // (n = 2) "abc def|" -> abc
    // (n = 2) "abc def |" -> abc
    // (n = 2) "abc def. |" -> abc
    // (n = 2) "abc def . |" -> def
    // (n = 2) "abc|" -> null
    // (n = 2) "abc |" -> null
    // (n = 2) "abc. def|" -> null
    public static CharSequence getNthPreviousWord(final CharSequence prev,
            final String sentenceSeperators, final int n) {
        if (prev == null) return null;
        String[] w = spaceRegex.split(prev);

        // If we can't find n words, or we found an empty word, return null.
        if (w.length < n || w[w.length - n].length() <= 0) return null;

        // If ends in a separator, return null
        char lastChar = w[w.length - n].charAt(w[w.length - n].length() - 1);
        if (sentenceSeperators.contains(String.valueOf(lastChar))) return null;

        return w[w.length - n];
    }

    /**
     * @param separators characters which may separate words
     * @return the word that surrounds the cursor, including up to one trailing
     *   separator. For example, if the field contains "he|llo world", where |
     *   represents the cursor, then "hello " will be returned.
     */
    public String getWordAtCursor(String separators) {
        // getWordRangeAtCursor returns null if the connection is null
        Range r = getWordRangeAtCursor(separators, 0);
        return (r == null) ? null : r.mWord;
    }

    private int getCursorPosition() {
        mIC = mParent.getCurrentInputConnection();
        if (null == mIC) return INVALID_CURSOR_POSITION;
        final ExtractedText extracted = mIC.getExtractedText(new ExtractedTextRequest(), 0);
        if (extracted == null) {
            return INVALID_CURSOR_POSITION;
        }
        return extracted.startOffset + extracted.selectionStart;
    }

    /**
     * Returns the text surrounding the cursor.
     *
     * @param sep a string of characters that split words.
     * @param additionalPrecedingWordsCount the number of words before the current word that should
     *   be included in the returned range
     * @return a range containing the text surrounding the cursor
     */
    public Range getWordRangeAtCursor(String sep, int additionalPrecedingWordsCount) {
        mIC = mParent.getCurrentInputConnection();
        if (mIC == null || sep == null) {
            return null;
        }
        CharSequence before = mIC.getTextBeforeCursor(1000, 0);
        CharSequence after = mIC.getTextAfterCursor(1000, 0);
        if (before == null || after == null) {
            return null;
        }

        // Going backward, alternate skipping non-separators and separators until enough words
        // have been read.
        int start = before.length();
        boolean isStoppingAtWhitespace = true;  // toggles to indicate what to stop at
        while (true) { // see comments below for why this is guaranteed to halt
            while (start > 0) {
                final int codePoint = Character.codePointBefore(before, start);
                if (isStoppingAtWhitespace == isSeparator(codePoint, sep)) {
                    break;  // inner loop
                }
                --start;
                if (Character.isSupplementaryCodePoint(codePoint)) {
                    --start;
                }
            }
            // isStoppingAtWhitespace is true every other time through the loop,
            // so additionalPrecedingWordsCount is guaranteed to become < 0, which
            // guarantees outer loop termination
            if (isStoppingAtWhitespace && (--additionalPrecedingWordsCount < 0)) {
                break;  // outer loop
            }
            isStoppingAtWhitespace = !isStoppingAtWhitespace;
        }

        // Find last word separator after the cursor
        int end = -1;
        while (++end < after.length()) {
            final int codePoint = Character.codePointAt(after, end);
            if (isSeparator(codePoint, sep)) {
                break;
            }
            if (Character.isSupplementaryCodePoint(codePoint)) {
                ++end;
            }
        }

        int cursor = getCursorPosition();
        if (start >= 0 && cursor + end <= after.length() + before.length()) {
            String word = before.toString().substring(start, before.length())
                    + after.toString().substring(0, end);
            return new Range(before.length() - start, end, word);
        }

        return null;
    }

    public boolean isCursorTouchingWord(final SettingsValues settingsValues) {
        CharSequence before = getTextBeforeCursor(1, 0);
        CharSequence after = getTextAfterCursor(1, 0);
        if (!TextUtils.isEmpty(before) && !settingsValues.isWordSeparator(before.charAt(0))
                && !settingsValues.isSymbolExcludedFromWordSeparators(before.charAt(0))) {
            return true;
        }
        if (!TextUtils.isEmpty(after) && !settingsValues.isWordSeparator(after.charAt(0))
                && !settingsValues.isSymbolExcludedFromWordSeparators(after.charAt(0))) {
            return true;
        }
        return false;
    }

    public void removeTrailingSpace() {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        final CharSequence lastOne = getTextBeforeCursor(1, 0);
        if (lastOne != null && lastOne.length() == 1
                && lastOne.charAt(0) == Keyboard.CODE_SPACE) {
            deleteSurroundingText(1, 0);
        }
    }

    public boolean sameAsTextBeforeCursor(final CharSequence text) {
        final CharSequence beforeText = getTextBeforeCursor(text.length(), 0);
        return TextUtils.equals(text, beforeText);
    }

    /* (non-javadoc)
     * Returns the word before the cursor if the cursor is at the end of a word, null otherwise
     */
    public CharSequence getWordBeforeCursorIfAtEndOfWord(final SettingsValues settings) {
        // Bail out if the cursor is in the middle of a word (cursor must be followed by whitespace,
        // separator or end of line/text)
        // Example: "test|"<EOL> "te|st" get rejected here
        final CharSequence textAfterCursor = getTextAfterCursor(1, 0);
        if (!TextUtils.isEmpty(textAfterCursor)
                && !settings.isWordSeparator(textAfterCursor.charAt(0))) return null;

        // Bail out if word before cursor is 0-length or a single non letter (like an apostrophe)
        // Example: " -|" gets rejected here but "e-|" and "e|" are okay
        CharSequence word = getWordAtCursor(settings.mWordSeparators);
        // We don't suggest on leading single quotes, so we have to remove them from the word if
        // it starts with single quotes.
        while (!TextUtils.isEmpty(word) && Keyboard.CODE_SINGLE_QUOTE == word.charAt(0)) {
            word = word.subSequence(1, word.length());
        }
        if (TextUtils.isEmpty(word)) return null;
        // Find the last code point of the string
        final int lastCodePoint = Character.codePointBefore(word, word.length());
        // If for some reason the text field contains non-unicode binary data, or if the
        // charsequence is exactly one char long and the contents is a low surrogate, return null.
        if (!Character.isDefined(lastCodePoint)) return null;
        // Bail out if the cursor is not at the end of a word (cursor must be preceded by
        // non-whitespace, non-separator, non-start-of-text)
        // Example ("|" is the cursor here) : <SOL>"|a" " |a" " | " all get rejected here.
        if (settings.isWordSeparator(lastCodePoint)) return null;
        final char firstChar = word.charAt(0); // we just tested that word is not empty
        if (word.length() == 1 && !Character.isLetter(firstChar)) return null;

        // We only suggest on words that start with a letter or a symbol that is excluded from
        // word separators (see #handleCharacterWhileInBatchEdit).
        if (!(Character.isLetter(firstChar)
                || settings.isSymbolExcludedFromWordSeparators(firstChar))) {
            return null;
        }

        return word;
    }

    public boolean revertDoubleSpace() {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        // Here we test whether we indeed have a period and a space before us. This should not
        // be needed, but it's there just in case something went wrong.
        final CharSequence textBeforeCursor = getTextBeforeCursor(2, 0);
        if (!". ".equals(textBeforeCursor)) {
            // Theoretically we should not be coming here if there isn't ". " before the
            // cursor, but the application may be changing the text while we are typing, so
            // anything goes. We should not crash.
            Log.d(TAG, "Tried to revert double-space combo but we didn't find "
                    + "\". \" just before the cursor.");
            return false;
        }
        deleteSurroundingText(2, 0);
        commitText("  ", 1);
        return true;
    }

    public boolean revertSwapPunctuation() {
        if (DEBUG_BATCH_NESTING) checkBatchEdit();
        // Here we test whether we indeed have a space and something else before us. This should not
        // be needed, but it's there just in case something went wrong.
        final CharSequence textBeforeCursor = getTextBeforeCursor(2, 0);
        // NOTE: This does not work with surrogate pairs. Hopefully when the keyboard is able to
        // enter surrogate pairs this code will have been removed.
        if (TextUtils.isEmpty(textBeforeCursor)
                || (Keyboard.CODE_SPACE != textBeforeCursor.charAt(1))) {
            // We may only come here if the application is changing the text while we are typing.
            // This is quite a broken case, but not logically impossible, so we shouldn't crash,
            // but some debugging log may be in order.
            Log.d(TAG, "Tried to revert a swap of punctuation but we didn't "
                    + "find a space just before the cursor.");
            return false;
        }
        deleteSurroundingText(2, 0);
        commitText(" " + textBeforeCursor.subSequence(0, 1), 1);
        return true;
    }

    /**
     * Heuristic to determine if this is an expected update of the cursor.
     *
     * Sometimes updates to the cursor position are late because of their asynchronous nature.
     * This method tries to determine if this update is one, based on the values of the cursor
     * position in the update, and the currently expected position of the cursor according to
     * LatinIME's internal accounting. If this is not a belated expected update, then it should
     * mean that the user moved the cursor explicitly.
     * This is quite robust, but of course it's not perfect. In particular, it will fail in the
     * case we get an update A, the user types in N characters so as to move the cursor to A+N but
     * we don't get those, and then the user places the cursor between A and A+N, and we get only
     * this update and not the ones in-between. This is almost impossible to achieve even trying
     * very very hard.
     *
     * @param oldSelStart The value of the old cursor position in the update.
     * @param newSelStart The value of the new cursor position in the update.
     * @return whether this is a belated expected update or not.
     */
    public boolean isBelatedExpectedUpdate(final int oldSelStart, final int newSelStart) {
        // If this is an update that arrives at our expected position, it's a belated update.
        if (newSelStart == mCurrentCursorPosition) return true;
        // If this is an update that moves the cursor from our expected position, it must be
        // an explicit move.
        if (oldSelStart == mCurrentCursorPosition) return false;
        // The following returns true if newSelStart is between oldSelStart and
        // mCurrentCursorPosition. We assume that if the updated position is between the old
        // position and the expected position, then it must be a belated update.
        return (newSelStart - oldSelStart) * (mCurrentCursorPosition - newSelStart) >= 0;
    }
}
