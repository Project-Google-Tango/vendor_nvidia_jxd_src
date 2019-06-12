/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.nvidia.inputmethod.compat;

import android.content.Context;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.util.Log;

import com.nvidia.inputmethod.latin.CollectionUtils;
import com.nvidia.inputmethod.latin.LatinImeLogger;
import com.nvidia.inputmethod.latin.SuggestedWords;
import com.nvidia.inputmethod.latin.SuggestionSpanPickedNotificationReceiver;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Locale;

public final class SuggestionSpanUtils {
    private static final String TAG = SuggestionSpanUtils.class.getSimpleName();
    // TODO: Use reflection to get field values
    public static final String ACTION_SUGGESTION_PICKED =
            "android.text.style.SUGGESTION_PICKED";
    public static final String SUGGESTION_SPAN_PICKED_AFTER = "after";
    public static final String SUGGESTION_SPAN_PICKED_BEFORE = "before";
    public static final String SUGGESTION_SPAN_PICKED_HASHCODE = "hashcode";
    public static final boolean SUGGESTION_SPAN_IS_SUPPORTED;

    private static final Class<?> CLASS_SuggestionSpan = CompatUtils
            .getClass("android.text.style.SuggestionSpan");
    private static final Class<?>[] INPUT_TYPE_SuggestionSpan = new Class<?>[] {
            Context.class, Locale.class, String[].class, int.class, Class.class };
    private static final Constructor<?> CONSTRUCTOR_SuggestionSpan = CompatUtils
            .getConstructor(CLASS_SuggestionSpan, INPUT_TYPE_SuggestionSpan);
    public static final Field FIELD_FLAG_EASY_CORRECT =
            CompatUtils.getField(CLASS_SuggestionSpan, "FLAG_EASY_CORRECT");
    public static final Field FIELD_FLAG_MISSPELLED =
            CompatUtils.getField(CLASS_SuggestionSpan, "FLAG_MISSPELLED");
    public static final Field FIELD_FLAG_AUTO_CORRECTION =
            CompatUtils.getField(CLASS_SuggestionSpan, "FLAG_AUTO_CORRECTION");
    public static final Field FIELD_SUGGESTIONS_MAX_SIZE
            = CompatUtils.getField(CLASS_SuggestionSpan, "SUGGESTIONS_MAX_SIZE");
    public static final Integer OBJ_FLAG_EASY_CORRECT = (Integer) CompatUtils
            .getFieldValue(null, null, FIELD_FLAG_EASY_CORRECT);
    public static final Integer OBJ_FLAG_MISSPELLED = (Integer) CompatUtils
            .getFieldValue(null, null, FIELD_FLAG_MISSPELLED);
    public static final Integer OBJ_FLAG_AUTO_CORRECTION = (Integer) CompatUtils
            .getFieldValue(null, null, FIELD_FLAG_AUTO_CORRECTION);
    public static final Integer OBJ_SUGGESTIONS_MAX_SIZE = (Integer) CompatUtils
            .getFieldValue(null, null, FIELD_SUGGESTIONS_MAX_SIZE);

    static {
        SUGGESTION_SPAN_IS_SUPPORTED =
                CLASS_SuggestionSpan != null && CONSTRUCTOR_SuggestionSpan != null;
        if (LatinImeLogger.sDBG) {
            if (SUGGESTION_SPAN_IS_SUPPORTED
                    && (OBJ_FLAG_AUTO_CORRECTION == null || OBJ_SUGGESTIONS_MAX_SIZE == null
                            || OBJ_FLAG_MISSPELLED == null || OBJ_FLAG_EASY_CORRECT == null)) {
                throw new RuntimeException("Field is accidentially null.");
            }
        }
    }

    private SuggestionSpanUtils() {
        // This utility class is not publicly instantiable.
    }

    public static CharSequence getTextWithAutoCorrectionIndicatorUnderline(
            Context context, CharSequence text) {
        if (TextUtils.isEmpty(text) || CONSTRUCTOR_SuggestionSpan == null
                || OBJ_FLAG_AUTO_CORRECTION == null || OBJ_SUGGESTIONS_MAX_SIZE == null
                || OBJ_FLAG_MISSPELLED == null || OBJ_FLAG_EASY_CORRECT == null) {
            return text;
        }
        final Spannable spannable = text instanceof Spannable
                ? (Spannable) text : new SpannableString(text);
        final Object[] args =
                { context, null, new String[] {}, (int)OBJ_FLAG_AUTO_CORRECTION,
                        (Class<?>) SuggestionSpanPickedNotificationReceiver.class };
        final Object ss = CompatUtils.newInstance(CONSTRUCTOR_SuggestionSpan, args);
        if (ss == null) {
            Log.w(TAG, "Suggestion span was not created.");
            return text;
        }
        spannable.setSpan(ss, 0, text.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE | Spanned.SPAN_COMPOSING);
        return spannable;
    }

    public static CharSequence getTextWithSuggestionSpan(Context context,
            CharSequence pickedWord, SuggestedWords suggestedWords, boolean dictionaryAvailable) {
        if (!dictionaryAvailable || TextUtils.isEmpty(pickedWord)
                || CONSTRUCTOR_SuggestionSpan == null
                || suggestedWords == null || suggestedWords.size() == 0
                || suggestedWords.mIsPrediction || suggestedWords.mIsPunctuationSuggestions
                || OBJ_SUGGESTIONS_MAX_SIZE == null) {
            return pickedWord;
        }

        final Spannable spannable;
        if (pickedWord instanceof Spannable) {
            spannable = (Spannable) pickedWord;
        } else {
            spannable = new SpannableString(pickedWord);
        }
        final ArrayList<String> suggestionsList = CollectionUtils.newArrayList();
        for (int i = 0; i < suggestedWords.size(); ++i) {
            if (suggestionsList.size() >= OBJ_SUGGESTIONS_MAX_SIZE) {
                break;
            }
            final CharSequence word = suggestedWords.getWord(i);
            if (!TextUtils.equals(pickedWord, word)) {
                suggestionsList.add(word.toString());
            }
        }

        // TODO: We should avoid adding suggestion span candidates that came from the bigram
        // prediction.
        final Object[] args =
                { context, null, suggestionsList.toArray(new String[suggestionsList.size()]), 0,
                        (Class<?>) SuggestionSpanPickedNotificationReceiver.class };
        final Object ss = CompatUtils.newInstance(CONSTRUCTOR_SuggestionSpan, args);
        if (ss == null) {
            return pickedWord;
        }
        spannable.setSpan(ss, 0, pickedWord.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return spannable;
    }
}
