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

import java.lang.reflect.Field;

public final class SettingsSecureCompatUtils {
    private static final Field FIELD_ACCESSIBILITY_SPEAK_PASSWORD = CompatUtils.getField(
            android.provider.Settings.Secure.class, "ACCESSIBILITY_SPEAK_PASSWORD");

    private SettingsSecureCompatUtils() {
        // This class is non-instantiable.
    }

    /**
     * Whether to speak passwords while in accessibility mode.
     */
    public static final String ACCESSIBILITY_SPEAK_PASSWORD = (String) CompatUtils.getFieldValue(
            null, null, FIELD_ACCESSIBILITY_SPEAK_PASSWORD);
}
