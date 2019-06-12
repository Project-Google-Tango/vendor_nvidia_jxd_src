/*
 * Copyright (C) 2008 The Android Open Source Project
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

import android.content.Context;
import android.text.TextUtils;

import com.nvidia.inputmethod.keyboard.Keyboard;
import com.nvidia.inputmethod.keyboard.ProximityInfo;
import com.nvidia.inputmethod.latin.SuggestedWords.SuggestedWordInfo;

import java.io.File;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Locale;
import java.util.concurrent.ConcurrentHashMap;

/**
 * This class loads a dictionary and provides a list of suggestions for a given sequence of
 * characters. This includes corrections and completions.
 */
public final class Suggest {
    public static final String TAG = Suggest.class.getSimpleName();

    // Session id for
    // {@link #getSuggestedWords(WordComposer,CharSequence,ProximityInfo,boolean,int)}.
    public static final int SESSION_TYPING = 0;
    public static final int SESSION_GESTURE = 1;

    // TODO: rename this to CORRECTION_OFF
    public static final int CORRECTION_NONE = 0;
    // TODO: rename this to CORRECTION_ON
    public static final int CORRECTION_FULL = 1;

    public interface SuggestInitializationListener {
        public void onUpdateMainDictionaryAvailability(boolean isMainDictionaryAvailable);
    }

    private static final boolean DBG = LatinImeLogger.sDBG;

    private Dictionary mMainDictionary;
    private ContactsBinaryDictionary mContactsDict;
    private final ConcurrentHashMap<String, Dictionary> mDictionaries =
            CollectionUtils.newConcurrentHashMap();

    public static final int MAX_SUGGESTIONS = 18;

    private float mAutoCorrectionThreshold;

    // Locale used for upper- and title-casing words
    private final Locale mLocale;

    public Suggest(final Context context, final Locale locale,
            final SuggestInitializationListener listener) {
        initAsynchronously(context, locale, listener);
        mLocale = locale;
    }

    /* package for test */ Suggest(final Context context, final File dictionary,
            final long startOffset, final long length, final Locale locale) {
        final Dictionary mainDict = DictionaryFactory.createDictionaryForTest(context, dictionary,
                startOffset, length /* useFullEditDistance */, false, locale);
        mLocale = locale;
        mMainDictionary = mainDict;
        addOrReplaceDictionary(mDictionaries, Dictionary.TYPE_MAIN, mainDict);
    }

    private void initAsynchronously(final Context context, final Locale locale,
            final SuggestInitializationListener listener) {
        resetMainDict(context, locale, listener);
    }

    private static void addOrReplaceDictionary(
            final ConcurrentHashMap<String, Dictionary> dictionaries,
            final String key, final Dictionary dict) {
        final Dictionary oldDict = (dict == null)
                ? dictionaries.remove(key)
                : dictionaries.put(key, dict);
        if (oldDict != null && dict != oldDict) {
            oldDict.close();
        }
    }

    public void resetMainDict(final Context context, final Locale locale,
            final SuggestInitializationListener listener) {
        mMainDictionary = null;
        if (listener != null) {
            listener.onUpdateMainDictionaryAvailability(hasMainDictionary());
        }
        new Thread("InitializeBinaryDictionary") {
            @Override
            public void run() {
                final DictionaryCollection newMainDict =
                        DictionaryFactory.createMainDictionaryFromManager(context, locale);
                addOrReplaceDictionary(mDictionaries, Dictionary.TYPE_MAIN, newMainDict);
                mMainDictionary = newMainDict;
                if (listener != null) {
                    listener.onUpdateMainDictionaryAvailability(hasMainDictionary());
                }
            }
        }.start();
    }

    // The main dictionary could have been loaded asynchronously.  Don't cache the return value
    // of this method.
    public boolean hasMainDictionary() {
        return null != mMainDictionary && mMainDictionary.isInitialized();
    }

    public Dictionary getMainDictionary() {
        return mMainDictionary;
    }

    public ContactsBinaryDictionary getContactsDictionary() {
        return mContactsDict;
    }

    public ConcurrentHashMap<String, Dictionary> getUnigramDictionaries() {
        return mDictionaries;
    }

    /**
     * Sets an optional user dictionary resource to be loaded. The user dictionary is consulted
     * before the main dictionary, if set. This refers to the system-managed user dictionary.
     */
    public void setUserDictionary(UserBinaryDictionary userDictionary) {
        addOrReplaceDictionary(mDictionaries, Dictionary.TYPE_USER, userDictionary);
    }

    /**
     * Sets an optional contacts dictionary resource to be loaded. It is also possible to remove
     * the contacts dictionary by passing null to this method. In this case no contacts dictionary
     * won't be used.
     */
    public void setContactsDictionary(ContactsBinaryDictionary contactsDictionary) {
        mContactsDict = contactsDictionary;
        addOrReplaceDictionary(mDictionaries, Dictionary.TYPE_CONTACTS, contactsDictionary);
    }

    public void setUserHistoryDictionary(UserHistoryDictionary userHistoryDictionary) {
        addOrReplaceDictionary(mDictionaries, Dictionary.TYPE_USER_HISTORY, userHistoryDictionary);
    }

    public void setAutoCorrectionThreshold(float threshold) {
        mAutoCorrectionThreshold = threshold;
    }

    public SuggestedWords getSuggestedWords(
            final WordComposer wordComposer, CharSequence prevWordForBigram,
            final ProximityInfo proximityInfo, final boolean isCorrectionEnabled, int sessionId) {
        LatinImeLogger.onStartSuggestion(prevWordForBigram);
        if (wordComposer.isBatchMode()) {
            return getSuggestedWordsForBatchInput(
                    wordComposer, prevWordForBigram, proximityInfo, sessionId);
        } else {
            return getSuggestedWordsForTypingInput(wordComposer, prevWordForBigram, proximityInfo,
                    isCorrectionEnabled);
        }
    }

    // Retrieves suggestions for the typing input.
    private SuggestedWords getSuggestedWordsForTypingInput(
            final WordComposer wordComposer, CharSequence prevWordForBigram,
            final ProximityInfo proximityInfo, final boolean isCorrectionEnabled) {
        final int trailingSingleQuotesCount = wordComposer.trailingSingleQuotesCount();
        final BoundedTreeSet suggestionsSet = new BoundedTreeSet(sSuggestedWordInfoComparator,
                MAX_SUGGESTIONS);

        final String typedWord = wordComposer.getTypedWord();
        final String consideredWord = trailingSingleQuotesCount > 0
                ? typedWord.substring(0, typedWord.length() - trailingSingleQuotesCount)
                : typedWord;
        LatinImeLogger.onAddSuggestedWord(typedWord, Dictionary.TYPE_USER_TYPED);

        final WordComposer wordComposerForLookup;
        if (trailingSingleQuotesCount > 0) {
            wordComposerForLookup = new WordComposer(wordComposer);
            for (int i = trailingSingleQuotesCount - 1; i >= 0; --i) {
                wordComposerForLookup.deleteLast();
            }
        } else {
            wordComposerForLookup = wordComposer;
        }

        for (final String key : mDictionaries.keySet()) {
            final Dictionary dictionary = mDictionaries.get(key);
            suggestionsSet.addAll(dictionary.getSuggestions(
                    wordComposerForLookup, prevWordForBigram, proximityInfo));
        }

        final CharSequence whitelistedWord;
        if (suggestionsSet.isEmpty()) {
            whitelistedWord = null;
        } else if (SuggestedWordInfo.KIND_WHITELIST != suggestionsSet.first().mKind) {
            whitelistedWord = null;
        } else {
            whitelistedWord = suggestionsSet.first().mWord;
        }

        // The word can be auto-corrected if it has a whitelist entry that is not itself,
        // or if it's a 2+ characters non-word (i.e. it's not in the dictionary).
        final boolean allowsToBeAutoCorrected = (null != whitelistedWord
                && !whitelistedWord.equals(consideredWord))
                || (consideredWord.length() > 1 && !AutoCorrection.isInTheDictionary(mDictionaries,
                        consideredWord, wordComposer.isFirstCharCapitalized()));

        final boolean hasAutoCorrection;
        // TODO: using isCorrectionEnabled here is not very good. It's probably useless, because
        // any attempt to do auto-correction is already shielded with a test for this flag; at the
        // same time, it feels wrong that the SuggestedWord object includes information about
        // the current settings. It may also be useful to know, when the setting is off, whether
        // the word *would* have been auto-corrected.
        if (!isCorrectionEnabled || !allowsToBeAutoCorrected || !wordComposer.isComposingWord()
                || suggestionsSet.isEmpty() || wordComposer.hasDigits()
                || wordComposer.isMostlyCaps() || wordComposer.isResumed()
                || !hasMainDictionary()) {
            // If we don't have a main dictionary, we never want to auto-correct. The reason for
            // this is, the user may have a contact whose name happens to match a valid word in
            // their language, and it will unexpectedly auto-correct. For example, if the user
            // types in English with no dictionary and has a "Will" in their contact list, "will"
            // would always auto-correct to "Will" which is unwanted. Hence, no main dict => no
            // auto-correct.
            hasAutoCorrection = false;
        } else {
            hasAutoCorrection = AutoCorrection.suggestionExceedsAutoCorrectionThreshold(
                    suggestionsSet.first(), consideredWord, mAutoCorrectionThreshold);
        }

        final ArrayList<SuggestedWordInfo> suggestionsContainer =
                CollectionUtils.newArrayList(suggestionsSet);
        final int suggestionsCount = suggestionsContainer.size();
        final boolean isFirstCharCapitalized = wordComposer.isFirstCharCapitalized();
        final boolean isAllUpperCase = wordComposer.isAllUpperCase();
        if (isFirstCharCapitalized || isAllUpperCase || 0 != trailingSingleQuotesCount) {
            for (int i = 0; i < suggestionsCount; ++i) {
                final SuggestedWordInfo wordInfo = suggestionsContainer.get(i);
                final SuggestedWordInfo transformedWordInfo = getTransformedSuggestedWordInfo(
                        wordInfo, mLocale, isAllUpperCase, isFirstCharCapitalized,
                        trailingSingleQuotesCount);
                suggestionsContainer.set(i, transformedWordInfo);
            }
        }

        for (int i = 0; i < suggestionsCount; ++i) {
            final SuggestedWordInfo wordInfo = suggestionsContainer.get(i);
            LatinImeLogger.onAddSuggestedWord(wordInfo.mWord.toString(), wordInfo.mSourceDict);
        }

        if (!TextUtils.isEmpty(typedWord)) {
            suggestionsContainer.add(0, new SuggestedWordInfo(typedWord,
                    SuggestedWordInfo.MAX_SCORE, SuggestedWordInfo.KIND_TYPED,
                    Dictionary.TYPE_USER_TYPED));
        }
        SuggestedWordInfo.removeDups(suggestionsContainer);

        final ArrayList<SuggestedWordInfo> suggestionsList;
        if (DBG && !suggestionsContainer.isEmpty()) {
            suggestionsList = getSuggestionsInfoListWithDebugInfo(typedWord, suggestionsContainer);
        } else {
            suggestionsList = suggestionsContainer;
        }

        return new SuggestedWords(suggestionsList,
                // TODO: this first argument is lying. If this is a whitelisted word which is an
                // actual word, it says typedWordValid = false, which looks wrong. We should either
                // rename the attribute or change the value.
                !allowsToBeAutoCorrected /* typedWordValid */,
                hasAutoCorrection, /* willAutoCorrect */
                false /* isPunctuationSuggestions */,
                false /* isObsoleteSuggestions */,
                !wordComposer.isComposingWord() /* isPrediction */);
    }

    // Retrieves suggestions for the batch input.
    private SuggestedWords getSuggestedWordsForBatchInput(
            final WordComposer wordComposer, CharSequence prevWordForBigram,
            final ProximityInfo proximityInfo, int sessionId) {
        final BoundedTreeSet suggestionsSet = new BoundedTreeSet(sSuggestedWordInfoComparator,
                MAX_SUGGESTIONS);

        // At second character typed, search the unigrams (scores being affected by bigrams)
        for (final String key : mDictionaries.keySet()) {
            // Skip User history dictionary for lookup
            // TODO: The user history dictionary should just override getSuggestionsWithSessionId
            // to make sure it doesn't return anything and we should remove this test
            if (key.equals(Dictionary.TYPE_USER_HISTORY)) {
                continue;
            }
            final Dictionary dictionary = mDictionaries.get(key);
            suggestionsSet.addAll(dictionary.getSuggestionsWithSessionId(
                    wordComposer, prevWordForBigram, proximityInfo, sessionId));
        }

        for (SuggestedWordInfo wordInfo : suggestionsSet) {
            LatinImeLogger.onAddSuggestedWord(wordInfo.mWord.toString(), wordInfo.mSourceDict);
        }

        final ArrayList<SuggestedWordInfo> suggestionsContainer =
                CollectionUtils.newArrayList(suggestionsSet);
        final int suggestionsCount = suggestionsContainer.size();
        final boolean isFirstCharCapitalized = wordComposer.wasShiftedNoLock();
        final boolean isAllUpperCase = wordComposer.isAllUpperCase();
        if (isFirstCharCapitalized || isAllUpperCase) {
            for (int i = 0; i < suggestionsCount; ++i) {
                final SuggestedWordInfo wordInfo = suggestionsContainer.get(i);
                final SuggestedWordInfo transformedWordInfo = getTransformedSuggestedWordInfo(
                        wordInfo, mLocale, isAllUpperCase, isFirstCharCapitalized,
                        0 /* trailingSingleQuotesCount */);
                suggestionsContainer.set(i, transformedWordInfo);
            }
        }

        SuggestedWordInfo.removeDups(suggestionsContainer);
        // In the batch input mode, the most relevant suggested word should act as a "typed word"
        // (typedWordValid=true), not as an "auto correct word" (willAutoCorrect=false).
        return new SuggestedWords(suggestionsContainer,
                true /* typedWordValid */,
                false /* willAutoCorrect */,
                false /* isPunctuationSuggestions */,
                false /* isObsoleteSuggestions */,
                false /* isPrediction */);
    }

    private static ArrayList<SuggestedWordInfo> getSuggestionsInfoListWithDebugInfo(
            final String typedWord, final ArrayList<SuggestedWordInfo> suggestions) {
        final SuggestedWordInfo typedWordInfo = suggestions.get(0);
        typedWordInfo.setDebugString("+");
        final int suggestionsSize = suggestions.size();
        final ArrayList<SuggestedWordInfo> suggestionsList =
                CollectionUtils.newArrayList(suggestionsSize);
        suggestionsList.add(typedWordInfo);
        // Note: i here is the index in mScores[], but the index in mSuggestions is one more
        // than i because we added the typed word to mSuggestions without touching mScores.
        for (int i = 0; i < suggestionsSize - 1; ++i) {
            final SuggestedWordInfo cur = suggestions.get(i + 1);
            final float normalizedScore = BinaryDictionary.calcNormalizedScore(
                    typedWord, cur.toString(), cur.mScore);
            final String scoreInfoString;
            if (normalizedScore > 0) {
                scoreInfoString = String.format("%d (%4.2f)", cur.mScore, normalizedScore);
            } else {
                scoreInfoString = Integer.toString(cur.mScore);
            }
            cur.setDebugString(scoreInfoString);
            suggestionsList.add(cur);
        }
        return suggestionsList;
    }

    private static final class SuggestedWordInfoComparator
            implements Comparator<SuggestedWordInfo> {
        // This comparator ranks the word info with the higher frequency first. That's because
        // that's the order we want our elements in.
        @Override
        public int compare(final SuggestedWordInfo o1, final SuggestedWordInfo o2) {
            if (o1.mScore > o2.mScore) return -1;
            if (o1.mScore < o2.mScore) return 1;
            if (o1.mCodePointCount < o2.mCodePointCount) return -1;
            if (o1.mCodePointCount > o2.mCodePointCount) return 1;
            return o1.mWord.toString().compareTo(o2.mWord.toString());
        }
    }
    private static final SuggestedWordInfoComparator sSuggestedWordInfoComparator =
            new SuggestedWordInfoComparator();

    private static SuggestedWordInfo getTransformedSuggestedWordInfo(
            final SuggestedWordInfo wordInfo, final Locale locale, final boolean isAllUpperCase,
            final boolean isFirstCharCapitalized, final int trailingSingleQuotesCount) {
        final StringBuilder sb = new StringBuilder(wordInfo.mWord.length());
        if (isAllUpperCase) {
            sb.append(wordInfo.mWord.toString().toUpperCase(locale));
        } else if (isFirstCharCapitalized) {
            sb.append(StringUtils.toTitleCase(wordInfo.mWord.toString(), locale));
        } else {
            sb.append(wordInfo.mWord);
        }
        for (int i = trailingSingleQuotesCount - 1; i >= 0; --i) {
            sb.appendCodePoint(Keyboard.CODE_SINGLE_QUOTE);
        }
        return new SuggestedWordInfo(sb, wordInfo.mScore, wordInfo.mKind, wordInfo.mSourceDict);
    }

    public void close() {
        final HashSet<Dictionary> dictionaries = CollectionUtils.newHashSet();
        dictionaries.addAll(mDictionaries.values());
        for (final Dictionary dictionary : dictionaries) {
            dictionary.close();
        }
        mMainDictionary = null;
    }
}
