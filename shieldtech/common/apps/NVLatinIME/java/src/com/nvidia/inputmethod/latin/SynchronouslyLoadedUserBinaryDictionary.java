/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.nvidia.inputmethod.latin;

import android.content.Context;

import com.nvidia.inputmethod.keyboard.ProximityInfo;
import com.nvidia.inputmethod.latin.SuggestedWords.SuggestedWordInfo;

import java.util.ArrayList;

public final class SynchronouslyLoadedUserBinaryDictionary extends UserBinaryDictionary {

    public SynchronouslyLoadedUserBinaryDictionary(final Context context, final String locale) {
        this(context, locale, false);
    }

    public SynchronouslyLoadedUserBinaryDictionary(final Context context, final String locale,
            final boolean alsoUseMoreRestrictiveLocales) {
        super(context, locale, alsoUseMoreRestrictiveLocales);
    }

    @Override
    public synchronized ArrayList<SuggestedWordInfo> getSuggestions(final WordComposer codes,
            final CharSequence prevWordForBigrams, final ProximityInfo proximityInfo) {
        syncReloadDictionaryIfRequired();
        return super.getSuggestions(codes, prevWordForBigrams, proximityInfo);
    }

    @Override
    public synchronized boolean isValidWord(CharSequence word) {
        syncReloadDictionaryIfRequired();
        return isValidWordInner(word);
    }
}
