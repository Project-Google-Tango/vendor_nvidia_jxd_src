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

package com.nvidia.inputmethod.keyboard.internal;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.util.Xml;
import android.view.InflateException;

import com.nvidia.inputmethod.keyboard.Key;
import com.nvidia.inputmethod.keyboard.Keyboard;
import com.nvidia.inputmethod.keyboard.KeyboardId;
import com.nvidia.inputmethod.latin.LocaleUtils.RunInLocale;
import com.nvidia.inputmethod.latin.R;
import com.nvidia.inputmethod.latin.ResourceUtils;
import com.nvidia.inputmethod.latin.StringUtils;
import com.nvidia.inputmethod.latin.SubtypeLocale;
import com.nvidia.inputmethod.latin.XmlParseUtils;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.Arrays;
import java.util.Locale;

/**
 * Keyboard Building helper.
 *
 * This class parses Keyboard XML file and eventually build a Keyboard.
 * The Keyboard XML file looks like:
 * <pre>
 *   &lt;!-- xml/keyboard.xml --&gt;
 *   &lt;Keyboard keyboard_attributes*&gt;
 *     &lt;!-- Keyboard Content --&gt;
 *     &lt;Row row_attributes*&gt;
 *       &lt;!-- Row Content --&gt;
 *       &lt;Key key_attributes* /&gt;
 *       &lt;Spacer horizontalGap="32.0dp" /&gt;
 *       &lt;include keyboardLayout="@xml/other_keys"&gt;
 *       ...
 *     &lt;/Row&gt;
 *     &lt;include keyboardLayout="@xml/other_rows"&gt;
 *     ...
 *   &lt;/Keyboard&gt;
 * </pre>
 * The XML file which is included in other file must have &lt;merge&gt; as root element,
 * such as:
 * <pre>
 *   &lt;!-- xml/other_keys.xml --&gt;
 *   &lt;merge&gt;
 *     &lt;Key key_attributes* /&gt;
 *     ...
 *   &lt;/merge&gt;
 * </pre>
 * and
 * <pre>
 *   &lt;!-- xml/other_rows.xml --&gt;
 *   &lt;merge&gt;
 *     &lt;Row row_attributes*&gt;
 *       &lt;Key key_attributes* /&gt;
 *     &lt;/Row&gt;
 *     ...
 *   &lt;/merge&gt;
 * </pre>
 * You can also use switch-case-default tags to select Rows and Keys.
 * <pre>
 *   &lt;switch&gt;
 *     &lt;case case_attribute*&gt;
 *       &lt;!-- Any valid tags at switch position --&gt;
 *     &lt;/case&gt;
 *     ...
 *     &lt;default&gt;
 *       &lt;!-- Any valid tags at switch position --&gt;
 *     &lt;/default&gt;
 *   &lt;/switch&gt;
 * </pre>
 * You can declare Key style and specify styles within Key tags.
 * <pre>
 *     &lt;switch&gt;
 *       &lt;case mode="email"&gt;
 *         &lt;key-style styleName="f1-key" parentStyle="modifier-key"
 *           keyLabel=".com"
 *         /&gt;
 *       &lt;/case&gt;
 *       &lt;case mode="url"&gt;
 *         &lt;key-style styleName="f1-key" parentStyle="modifier-key"
 *           keyLabel="http://"
 *         /&gt;
 *       &lt;/case&gt;
 *     &lt;/switch&gt;
 *     ...
 *     &lt;Key keyStyle="shift-key" ... /&gt;
 * </pre>
 */

public class KeyboardBuilder<KP extends KeyboardParams> {
    private static final String BUILDER_TAG = "Keyboard.Builder";
    private static final boolean DEBUG = false;

    // Keyboard XML Tags
    private static final String TAG_KEYBOARD = "Keyboard";
    private static final String TAG_ROW = "Row";
    private static final String TAG_KEY = "Key";
    private static final String TAG_SPACER = "Spacer";
    private static final String TAG_INCLUDE = "include";
    private static final String TAG_MERGE = "merge";
    private static final String TAG_SWITCH = "switch";
    private static final String TAG_CASE = "case";
    private static final String TAG_DEFAULT = "default";
    public static final String TAG_KEY_STYLE = "key-style";

    private static final int DEFAULT_KEYBOARD_COLUMNS = 10;
    private static final int DEFAULT_KEYBOARD_ROWS = 4;

    protected final KP mParams;
    protected final Context mContext;
    protected final Resources mResources;
    private final DisplayMetrics mDisplayMetrics;

    private int mCurrentY = 0;
    private KeyboardRow mCurrentRow = null;
    private boolean mLeftEdge;
    private boolean mTopEdge;
    private Key mRightEdgeKey = null;

    public KeyboardBuilder(final Context context, final KP params) {
        mContext = context;
        final Resources res = context.getResources();
        mResources = res;
        mDisplayMetrics = res.getDisplayMetrics();

        mParams = params;

        params.GRID_WIDTH = res.getInteger(R.integer.config_keyboard_grid_width);
        params.GRID_HEIGHT = res.getInteger(R.integer.config_keyboard_grid_height);
    }

    public void setAutoGenerate(final KeysCache keysCache) {
        mParams.mKeysCache = keysCache;
    }

    public KeyboardBuilder<KP> load(final int xmlId, final KeyboardId id) {
        mParams.mId = id;
        final XmlResourceParser parser = mResources.getXml(xmlId);
        try {
            parseKeyboard(parser);
        } catch (XmlPullParserException e) {
            Log.w(BUILDER_TAG, "keyboard XML parse error: " + e);
            throw new IllegalArgumentException(e);
        } catch (IOException e) {
            Log.w(BUILDER_TAG, "keyboard XML parse error: " + e);
            throw new RuntimeException(e);
        } finally {
            parser.close();
        }
        return this;
    }

    // For test only
    public void disableTouchPositionCorrectionDataForTest() {
        mParams.mTouchPositionCorrection.setEnabled(false);
    }

    public void setProximityCharsCorrectionEnabled(final boolean enabled) {
        mParams.mProximityCharsCorrectionEnabled = enabled;
    }

    public Keyboard build() {
        return new Keyboard(mParams);
    }

    private int mIndent;
    private static final String SPACES = "                                             ";

    private static String spaces(final int count) {
        return (count < SPACES.length()) ? SPACES.substring(0, count) : SPACES;
    }

    private void startTag(final String format, final Object ... args) {
        Log.d(BUILDER_TAG, String.format(spaces(++mIndent * 2) + format, args));
    }

    private void endTag(final String format, final Object ... args) {
        Log.d(BUILDER_TAG, String.format(spaces(mIndent-- * 2) + format, args));
    }

    private void startEndTag(final String format, final Object ... args) {
        Log.d(BUILDER_TAG, String.format(spaces(++mIndent * 2) + format, args));
        mIndent--;
    }

    private void parseKeyboard(final XmlPullParser parser)
            throws XmlPullParserException, IOException {
        if (DEBUG) startTag("<%s> %s", TAG_KEYBOARD, mParams.mId);
        int event;
        while ((event = parser.next()) != XmlPullParser.END_DOCUMENT) {
            if (event == XmlPullParser.START_TAG) {
                final String tag = parser.getName();
                if (TAG_KEYBOARD.equals(tag)) {
                    parseKeyboardAttributes(parser);
                    startKeyboard();
                    parseKeyboardContent(parser, false);
                    break;
                } else {
                    throw new XmlParseUtils.IllegalStartTag(parser, TAG_KEYBOARD);
                }
            }
        }
    }

    private void parseKeyboardAttributes(final XmlPullParser parser) {
        final int displayWidth = mDisplayMetrics.widthPixels;
        final TypedArray keyboardAttr = mContext.obtainStyledAttributes(
                Xml.asAttributeSet(parser), R.styleable.Keyboard, R.attr.keyboardStyle,
                R.style.Keyboard);
        final TypedArray keyAttr = mResources.obtainAttributes(Xml.asAttributeSet(parser),
                R.styleable.Keyboard_Key);
        try {
            final int displayHeight = mDisplayMetrics.heightPixels;
            final String keyboardHeightString = ResourceUtils.getDeviceOverrideValue(
                    mResources, R.array.keyboard_heights, null);
            final float keyboardHeight;
            if (keyboardHeightString != null) {
                keyboardHeight = Float.parseFloat(keyboardHeightString)
                        * mDisplayMetrics.density;
            } else {
                keyboardHeight = keyboardAttr.getDimension(
                        R.styleable.Keyboard_keyboardHeight, displayHeight / 2);
            }
            final float maxKeyboardHeight = ResourceUtils.getDimensionOrFraction(keyboardAttr,
                    R.styleable.Keyboard_maxKeyboardHeight, displayHeight, displayHeight / 2);
            float minKeyboardHeight = ResourceUtils.getDimensionOrFraction(keyboardAttr,
                    R.styleable.Keyboard_minKeyboardHeight, displayHeight, displayHeight / 2);
            if (minKeyboardHeight < 0) {
                // Specified fraction was negative, so it should be calculated against display
                // width.
                minKeyboardHeight = -ResourceUtils.getDimensionOrFraction(keyboardAttr,
                        R.styleable.Keyboard_minKeyboardHeight, displayWidth, displayWidth / 2);
            }
            final KeyboardParams params = mParams;
            // Keyboard height will not exceed maxKeyboardHeight and will not be less than
            // minKeyboardHeight.
            params.mOccupiedHeight = (int)Math.max(
                    Math.min(keyboardHeight, maxKeyboardHeight), minKeyboardHeight);
            params.mOccupiedWidth = params.mId.mWidth;
            params.mTopPadding = (int)ResourceUtils.getDimensionOrFraction(keyboardAttr,
                    R.styleable.Keyboard_keyboardTopPadding, params.mOccupiedHeight, 0);
            params.mBottomPadding = (int)ResourceUtils.getDimensionOrFraction(keyboardAttr,
                    R.styleable.Keyboard_keyboardBottomPadding, params.mOccupiedHeight, 0);
            params.mHorizontalEdgesPadding = (int)ResourceUtils.getDimensionOrFraction(
                    keyboardAttr,
                    R.styleable.Keyboard_keyboardHorizontalEdgesPadding,
                    mParams.mOccupiedWidth, 0);

            params.mBaseWidth = params.mOccupiedWidth - params.mHorizontalEdgesPadding * 2
                    - params.mHorizontalCenterPadding;
            params.mDefaultKeyWidth = (int)ResourceUtils.getDimensionOrFraction(keyAttr,
                    R.styleable.Keyboard_Key_keyWidth, params.mBaseWidth,
                    params.mBaseWidth / DEFAULT_KEYBOARD_COLUMNS);
            params.mHorizontalGap = (int)ResourceUtils.getDimensionOrFraction(keyboardAttr,
                    R.styleable.Keyboard_horizontalGap, params.mBaseWidth, 0);
            params.mVerticalGap = (int)ResourceUtils.getDimensionOrFraction(keyboardAttr,
                    R.styleable.Keyboard_verticalGap, params.mOccupiedHeight, 0);
            params.mBaseHeight = params.mOccupiedHeight - params.mTopPadding
                    - params.mBottomPadding + params.mVerticalGap;
            params.mDefaultRowHeight = (int)ResourceUtils.getDimensionOrFraction(keyboardAttr,
                    R.styleable.Keyboard_rowHeight, params.mBaseHeight,
                    params.mBaseHeight / DEFAULT_KEYBOARD_ROWS);

            params.mKeyVisualAttributes = KeyVisualAttributes.newInstance(keyAttr);

            params.mMoreKeysTemplate = keyboardAttr.getResourceId(
                    R.styleable.Keyboard_moreKeysTemplate, 0);
            params.mMaxMoreKeysKeyboardColumn = keyAttr.getInt(
                    R.styleable.Keyboard_Key_maxMoreKeysColumn, 5);

            params.mThemeId = keyboardAttr.getInt(R.styleable.Keyboard_themeId, 0);
            params.mIconsSet.loadIcons(keyboardAttr);
            final String language = params.mId.mLocale.getLanguage();
            params.mCodesSet.setLanguage(language);
            params.mTextsSet.setLanguage(language);
            final RunInLocale<Void> job = new RunInLocale<Void>() {
                @Override
                protected Void job(Resources res) {
                    params.mTextsSet.loadStringResources(mContext);
                    return null;
                }
            };
            // Null means the current system locale.
            final Locale locale = SubtypeLocale.isNoLanguage(params.mId.mSubtype)
                    ? null : params.mId.mLocale;
            job.runInLocale(mResources, locale);

            final int resourceId = keyboardAttr.getResourceId(
                    R.styleable.Keyboard_touchPositionCorrectionData, 0);
            if (resourceId != 0) {
                final String[] data = mResources.getStringArray(resourceId);
                params.mTouchPositionCorrection.load(data);
            }
        } finally {
            keyAttr.recycle();
            keyboardAttr.recycle();
        }
    }

    private void parseKeyboardContent(final XmlPullParser parser, final boolean skip)
            throws XmlPullParserException, IOException {
        int event;
        while ((event = parser.next()) != XmlPullParser.END_DOCUMENT) {
            if (event == XmlPullParser.START_TAG) {
                final String tag = parser.getName();
                if (TAG_ROW.equals(tag)) {
                    final KeyboardRow row = parseRowAttributes(parser);
                    if (DEBUG) startTag("<%s>%s", TAG_ROW, skip ? " skipped" : "");
                    if (!skip) {
                        startRow(row);
                    }
                    parseRowContent(parser, row, skip);
                } else if (TAG_INCLUDE.equals(tag)) {
                    parseIncludeKeyboardContent(parser, skip);
                } else if (TAG_SWITCH.equals(tag)) {
                    parseSwitchKeyboardContent(parser, skip);
                } else if (TAG_KEY_STYLE.equals(tag)) {
                    parseKeyStyle(parser, skip);
                } else {
                    throw new XmlParseUtils.IllegalStartTag(parser, TAG_ROW);
                }
            } else if (event == XmlPullParser.END_TAG) {
                final String tag = parser.getName();
                if (DEBUG) endTag("</%s>", tag);
                if (TAG_KEYBOARD.equals(tag)) {
                    endKeyboard();
                    break;
                } else if (TAG_CASE.equals(tag) || TAG_DEFAULT.equals(tag)
                        || TAG_MERGE.equals(tag)) {
                    break;
                } else {
                    throw new XmlParseUtils.IllegalEndTag(parser, TAG_ROW);
                }
            }
        }
    }

    private KeyboardRow parseRowAttributes(final XmlPullParser parser)
            throws XmlPullParserException {
        final TypedArray a = mResources.obtainAttributes(Xml.asAttributeSet(parser),
                R.styleable.Keyboard);
        try {
            if (a.hasValue(R.styleable.Keyboard_horizontalGap)) {
                throw new XmlParseUtils.IllegalAttribute(parser, "horizontalGap");
            }
            if (a.hasValue(R.styleable.Keyboard_verticalGap)) {
                throw new XmlParseUtils.IllegalAttribute(parser, "verticalGap");
            }
            return new KeyboardRow(mResources, mParams, parser, mCurrentY);
        } finally {
            a.recycle();
        }
    }

    private void parseRowContent(final XmlPullParser parser, final KeyboardRow row,
            final boolean skip) throws XmlPullParserException, IOException {
        int event;
        while ((event = parser.next()) != XmlPullParser.END_DOCUMENT) {
            if (event == XmlPullParser.START_TAG) {
                final String tag = parser.getName();
                if (TAG_KEY.equals(tag)) {
                    parseKey(parser, row, skip);
                } else if (TAG_SPACER.equals(tag)) {
                    parseSpacer(parser, row, skip);
                } else if (TAG_INCLUDE.equals(tag)) {
                    parseIncludeRowContent(parser, row, skip);
                } else if (TAG_SWITCH.equals(tag)) {
                    parseSwitchRowContent(parser, row, skip);
                } else if (TAG_KEY_STYLE.equals(tag)) {
                    parseKeyStyle(parser, skip);
                } else {
                    throw new XmlParseUtils.IllegalStartTag(parser, TAG_KEY);
                }
            } else if (event == XmlPullParser.END_TAG) {
                final String tag = parser.getName();
                if (DEBUG) endTag("</%s>", tag);
                if (TAG_ROW.equals(tag)) {
                    if (!skip) {
                        endRow(row);
                    }
                    break;
                } else if (TAG_CASE.equals(tag) || TAG_DEFAULT.equals(tag)
                        || TAG_MERGE.equals(tag)) {
                    break;
                } else {
                    throw new XmlParseUtils.IllegalEndTag(parser, TAG_KEY);
                }
            }
        }
    }

    private void parseKey(final XmlPullParser parser, final KeyboardRow row, final boolean skip)
            throws XmlPullParserException, IOException {
        if (skip) {
            XmlParseUtils.checkEndTag(TAG_KEY, parser);
            if (DEBUG) {
                startEndTag("<%s /> skipped", TAG_KEY);
            }
        } else {
            final Key key = new Key(mResources, mParams, row, parser);
            if (DEBUG) {
                startEndTag("<%s%s %s moreKeys=%s />", TAG_KEY,
                        (key.isEnabled() ? "" : " disabled"), key,
                        Arrays.toString(key.mMoreKeys));
            }
            XmlParseUtils.checkEndTag(TAG_KEY, parser);
            endKey(key);
        }
    }

    private void parseSpacer(final XmlPullParser parser, final KeyboardRow row, final boolean skip)
            throws XmlPullParserException, IOException {
        if (skip) {
            XmlParseUtils.checkEndTag(TAG_SPACER, parser);
            if (DEBUG) startEndTag("<%s /> skipped", TAG_SPACER);
        } else {
            final Key.Spacer spacer = new Key.Spacer(mResources, mParams, row, parser);
            if (DEBUG) startEndTag("<%s />", TAG_SPACER);
            XmlParseUtils.checkEndTag(TAG_SPACER, parser);
            endKey(spacer);
        }
    }

    private void parseIncludeKeyboardContent(final XmlPullParser parser, final boolean skip)
            throws XmlPullParserException, IOException {
        parseIncludeInternal(parser, null, skip);
    }

    private void parseIncludeRowContent(final XmlPullParser parser, final KeyboardRow row,
            final boolean skip) throws XmlPullParserException, IOException {
        parseIncludeInternal(parser, row, skip);
    }

    private void parseIncludeInternal(final XmlPullParser parser, final KeyboardRow row,
            final boolean skip) throws XmlPullParserException, IOException {
        if (skip) {
            XmlParseUtils.checkEndTag(TAG_INCLUDE, parser);
            if (DEBUG) startEndTag("</%s> skipped", TAG_INCLUDE);
        } else {
            final AttributeSet attr = Xml.asAttributeSet(parser);
            final TypedArray keyboardAttr = mResources.obtainAttributes(attr,
                    R.styleable.Keyboard_Include);
            final TypedArray keyAttr = mResources.obtainAttributes(attr,
                    R.styleable.Keyboard_Key);
            int keyboardLayout = 0;
            float savedDefaultKeyWidth = 0;
            int savedDefaultKeyLabelFlags = 0;
            int savedDefaultBackgroundType = Key.BACKGROUND_TYPE_NORMAL;
            try {
                XmlParseUtils.checkAttributeExists(keyboardAttr,
                        R.styleable.Keyboard_Include_keyboardLayout, "keyboardLayout",
                        TAG_INCLUDE, parser);
                keyboardLayout = keyboardAttr.getResourceId(
                        R.styleable.Keyboard_Include_keyboardLayout, 0);
                if (row != null) {
                    if (keyAttr.hasValue(R.styleable.Keyboard_Key_keyXPos)) {
                        // Override current x coordinate.
                        row.setXPos(row.getKeyX(keyAttr));
                    }
                    // TODO: Remove this if-clause and do the same as backgroundType below.
                    savedDefaultKeyWidth = row.getDefaultKeyWidth();
                    if (keyAttr.hasValue(R.styleable.Keyboard_Key_keyWidth)) {
                        // Override default key width.
                        row.setDefaultKeyWidth(row.getKeyWidth(keyAttr));
                    }
                    savedDefaultKeyLabelFlags = row.getDefaultKeyLabelFlags();
                    // Bitwise-or default keyLabelFlag if exists.
                    row.setDefaultKeyLabelFlags(keyAttr.getInt(
                            R.styleable.Keyboard_Key_keyLabelFlags, 0)
                            | savedDefaultKeyLabelFlags);
                    savedDefaultBackgroundType = row.getDefaultBackgroundType();
                    // Override default backgroundType if exists.
                    row.setDefaultBackgroundType(keyAttr.getInt(
                            R.styleable.Keyboard_Key_backgroundType,
                            savedDefaultBackgroundType));
                }
            } finally {
                keyboardAttr.recycle();
                keyAttr.recycle();
            }

            XmlParseUtils.checkEndTag(TAG_INCLUDE, parser);
            if (DEBUG) {
                startEndTag("<%s keyboardLayout=%s />",TAG_INCLUDE,
                        mResources.getResourceEntryName(keyboardLayout));
            }
            final XmlResourceParser parserForInclude = mResources.getXml(keyboardLayout);
            try {
                parseMerge(parserForInclude, row, skip);
            } finally {
                if (row != null) {
                    // Restore default keyWidth, keyLabelFlags, and backgroundType.
                    row.setDefaultKeyWidth(savedDefaultKeyWidth);
                    row.setDefaultKeyLabelFlags(savedDefaultKeyLabelFlags);
                    row.setDefaultBackgroundType(savedDefaultBackgroundType);
                }
                parserForInclude.close();
            }
        }
    }

    private void parseMerge(final XmlPullParser parser, final KeyboardRow row, final boolean skip)
            throws XmlPullParserException, IOException {
        if (DEBUG) startTag("<%s>", TAG_MERGE);
        int event;
        while ((event = parser.next()) != XmlPullParser.END_DOCUMENT) {
            if (event == XmlPullParser.START_TAG) {
                final String tag = parser.getName();
                if (TAG_MERGE.equals(tag)) {
                    if (row == null) {
                        parseKeyboardContent(parser, skip);
                    } else {
                        parseRowContent(parser, row, skip);
                    }
                    break;
                } else {
                    throw new XmlParseUtils.ParseException(
                            "Included keyboard layout must have <merge> root element", parser);
                }
            }
        }
    }

    private void parseSwitchKeyboardContent(final XmlPullParser parser, final boolean skip)
            throws XmlPullParserException, IOException {
        parseSwitchInternal(parser, null, skip);
    }

    private void parseSwitchRowContent(final XmlPullParser parser, final KeyboardRow row,
            final boolean skip) throws XmlPullParserException, IOException {
        parseSwitchInternal(parser, row, skip);
    }

    private void parseSwitchInternal(final XmlPullParser parser, final KeyboardRow row,
            final boolean skip) throws XmlPullParserException, IOException {
        if (DEBUG) startTag("<%s> %s", TAG_SWITCH, mParams.mId);
        boolean selected = false;
        int event;
        while ((event = parser.next()) != XmlPullParser.END_DOCUMENT) {
            if (event == XmlPullParser.START_TAG) {
                final String tag = parser.getName();
                if (TAG_CASE.equals(tag)) {
                    selected |= parseCase(parser, row, selected ? true : skip);
                } else if (TAG_DEFAULT.equals(tag)) {
                    selected |= parseDefault(parser, row, selected ? true : skip);
                } else {
                    throw new XmlParseUtils.IllegalStartTag(parser, TAG_KEY);
                }
            } else if (event == XmlPullParser.END_TAG) {
                final String tag = parser.getName();
                if (TAG_SWITCH.equals(tag)) {
                    if (DEBUG) endTag("</%s>", TAG_SWITCH);
                    break;
                } else {
                    throw new XmlParseUtils.IllegalEndTag(parser, TAG_KEY);
                }
            }
        }
    }

    private boolean parseCase(final XmlPullParser parser, final KeyboardRow row, final boolean skip)
            throws XmlPullParserException, IOException {
        final boolean selected = parseCaseCondition(parser);
        if (row == null) {
            // Processing Rows.
            parseKeyboardContent(parser, selected ? skip : true);
        } else {
            // Processing Keys.
            parseRowContent(parser, row, selected ? skip : true);
        }
        return selected;
    }

    private boolean parseCaseCondition(final XmlPullParser parser) {
        final KeyboardId id = mParams.mId;
        if (id == null) {
            return true;
        }
        final TypedArray a = mResources.obtainAttributes(Xml.asAttributeSet(parser),
                R.styleable.Keyboard_Case);
        try {
            final boolean keyboardLayoutSetElementMatched = matchTypedValue(a,
                    R.styleable.Keyboard_Case_keyboardLayoutSetElement, id.mElementId,
                    KeyboardId.elementIdToName(id.mElementId));
            final boolean modeMatched = matchTypedValue(a,
                    R.styleable.Keyboard_Case_mode, id.mMode, KeyboardId.modeName(id.mMode));
            final boolean navigateNextMatched = matchBoolean(a,
                    R.styleable.Keyboard_Case_navigateNext, id.navigateNext());
            final boolean navigatePreviousMatched = matchBoolean(a,
                    R.styleable.Keyboard_Case_navigatePrevious, id.navigatePrevious());
            final boolean passwordInputMatched = matchBoolean(a,
                    R.styleable.Keyboard_Case_passwordInput, id.passwordInput());
            final boolean clobberSettingsKeyMatched = matchBoolean(a,
                    R.styleable.Keyboard_Case_clobberSettingsKey, id.mClobberSettingsKey);
            final boolean shortcutKeyEnabledMatched = matchBoolean(a,
                    R.styleable.Keyboard_Case_shortcutKeyEnabled, id.mShortcutKeyEnabled);
            final boolean hasShortcutKeyMatched = matchBoolean(a,
                    R.styleable.Keyboard_Case_hasShortcutKey, id.mHasShortcutKey);
            final boolean languageSwitchKeyEnabledMatched = matchBoolean(a,
                    R.styleable.Keyboard_Case_languageSwitchKeyEnabled,
                    id.mLanguageSwitchKeyEnabled);
            final boolean isMultiLineMatched = matchBoolean(a,
                    R.styleable.Keyboard_Case_isMultiLine, id.isMultiLine());
            final boolean imeActionMatched = matchInteger(a,
                    R.styleable.Keyboard_Case_imeAction, id.imeAction());
            final boolean localeCodeMatched = matchString(a,
                    R.styleable.Keyboard_Case_localeCode, id.mLocale.toString());
            final boolean languageCodeMatched = matchString(a,
                    R.styleable.Keyboard_Case_languageCode, id.mLocale.getLanguage());
            final boolean countryCodeMatched = matchString(a,
                    R.styleable.Keyboard_Case_countryCode, id.mLocale.getCountry());
            final boolean selected = keyboardLayoutSetElementMatched && modeMatched
                    && navigateNextMatched && navigatePreviousMatched && passwordInputMatched
                    && clobberSettingsKeyMatched && shortcutKeyEnabledMatched
                    && hasShortcutKeyMatched && languageSwitchKeyEnabledMatched
                    && isMultiLineMatched && imeActionMatched && localeCodeMatched
                    && languageCodeMatched && countryCodeMatched;

            if (DEBUG) {
                startTag("<%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s>%s", TAG_CASE,
                        textAttr(a.getString(
                                R.styleable.Keyboard_Case_keyboardLayoutSetElement),
                                "keyboardLayoutSetElement"),
                        textAttr(a.getString(R.styleable.Keyboard_Case_mode), "mode"),
                        textAttr(a.getString(R.styleable.Keyboard_Case_imeAction),
                                "imeAction"),
                        booleanAttr(a, R.styleable.Keyboard_Case_navigateNext,
                                "navigateNext"),
                        booleanAttr(a, R.styleable.Keyboard_Case_navigatePrevious,
                                "navigatePrevious"),
                        booleanAttr(a, R.styleable.Keyboard_Case_clobberSettingsKey,
                                "clobberSettingsKey"),
                        booleanAttr(a, R.styleable.Keyboard_Case_passwordInput,
                                "passwordInput"),
                        booleanAttr(a, R.styleable.Keyboard_Case_shortcutKeyEnabled,
                                "shortcutKeyEnabled"),
                        booleanAttr(a, R.styleable.Keyboard_Case_hasShortcutKey,
                                "hasShortcutKey"),
                        booleanAttr(a, R.styleable.Keyboard_Case_languageSwitchKeyEnabled,
                                "languageSwitchKeyEnabled"),
                        booleanAttr(a, R.styleable.Keyboard_Case_isMultiLine,
                                "isMultiLine"),
                        textAttr(a.getString(R.styleable.Keyboard_Case_localeCode),
                                "localeCode"),
                        textAttr(a.getString(R.styleable.Keyboard_Case_languageCode),
                                "languageCode"),
                        textAttr(a.getString(R.styleable.Keyboard_Case_countryCode),
                                "countryCode"),
                        selected ? "" : " skipped");
            }

            return selected;
        } finally {
            a.recycle();
        }
    }

    private static boolean matchInteger(final TypedArray a, final int index, final int value) {
        // If <case> does not have "index" attribute, that means this <case> is wild-card for
        // the attribute.
        return !a.hasValue(index) || a.getInt(index, 0) == value;
    }

    private static boolean matchBoolean(final TypedArray a, final int index, final boolean value) {
        // If <case> does not have "index" attribute, that means this <case> is wild-card for
        // the attribute.
        return !a.hasValue(index) || a.getBoolean(index, false) == value;
    }

    private static boolean matchString(final TypedArray a, final int index, final String value) {
        // If <case> does not have "index" attribute, that means this <case> is wild-card for
        // the attribute.
        return !a.hasValue(index)
                || StringUtils.containsInArray(value, a.getString(index).split("\\|"));
    }

    private static boolean matchTypedValue(final TypedArray a, final int index, final int intValue,
            final String strValue) {
        // If <case> does not have "index" attribute, that means this <case> is wild-card for
        // the attribute.
        final TypedValue v = a.peekValue(index);
        if (v == null) {
            return true;
        }
        if (ResourceUtils.isIntegerValue(v)) {
            return intValue == a.getInt(index, 0);
        } else if (ResourceUtils.isStringValue(v)) {
            return StringUtils.containsInArray(strValue, a.getString(index).split("\\|"));
        }
        return false;
    }

    private boolean parseDefault(final XmlPullParser parser, final KeyboardRow row,
            final boolean skip) throws XmlPullParserException, IOException {
        if (DEBUG) startTag("<%s>", TAG_DEFAULT);
        if (row == null) {
            parseKeyboardContent(parser, skip);
        } else {
            parseRowContent(parser, row, skip);
        }
        return true;
    }

    private void parseKeyStyle(final XmlPullParser parser, final boolean skip)
            throws XmlPullParserException, IOException {
        TypedArray keyStyleAttr = mResources.obtainAttributes(Xml.asAttributeSet(parser),
                R.styleable.Keyboard_KeyStyle);
        TypedArray keyAttrs = mResources.obtainAttributes(Xml.asAttributeSet(parser),
                R.styleable.Keyboard_Key);
        try {
            if (!keyStyleAttr.hasValue(R.styleable.Keyboard_KeyStyle_styleName)) {
                throw new XmlParseUtils.ParseException("<" + TAG_KEY_STYLE
                        + "/> needs styleName attribute", parser);
            }
            if (DEBUG) {
                startEndTag("<%s styleName=%s />%s", TAG_KEY_STYLE,
                        keyStyleAttr.getString(R.styleable.Keyboard_KeyStyle_styleName),
                        skip ? " skipped" : "");
            }
            if (!skip) {
                mParams.mKeyStyles.parseKeyStyleAttributes(keyStyleAttr, keyAttrs, parser);
            }
        } finally {
            keyStyleAttr.recycle();
            keyAttrs.recycle();
        }
        XmlParseUtils.checkEndTag(TAG_KEY_STYLE, parser);
    }

    private void startKeyboard() {
        mCurrentY += mParams.mTopPadding;
        mTopEdge = true;
    }

    private void startRow(final KeyboardRow row) {
        addEdgeSpace(mParams.mHorizontalEdgesPadding, row);
        mCurrentRow = row;
        mLeftEdge = true;
        mRightEdgeKey = null;
    }

    private void endRow(final KeyboardRow row) {
        if (mCurrentRow == null) {
            throw new InflateException("orphan end row tag");
        }
        if (mRightEdgeKey != null) {
            mRightEdgeKey.markAsRightEdge(mParams);
            mRightEdgeKey = null;
        }
        addEdgeSpace(mParams.mHorizontalEdgesPadding, row);
        mCurrentY += row.mRowHeight;
        mCurrentRow = null;
        mTopEdge = false;
    }

    private void endKey(final Key key) {
        mParams.onAddKey(key);
        if (mLeftEdge) {
            key.markAsLeftEdge(mParams);
            mLeftEdge = false;
        }
        if (mTopEdge) {
            key.markAsTopEdge(mParams);
        }
        mRightEdgeKey = key;
    }

    private void endKeyboard() {
        // nothing to do here.
    }

    private void addEdgeSpace(final float width, final KeyboardRow row) {
        row.advanceXPos(width);
        mLeftEdge = false;
        mRightEdgeKey = null;
    }

    private static String textAttr(final String value, final String name) {
        return value != null ? String.format(" %s=%s", name, value) : "";
    }

    private static String booleanAttr(final TypedArray a, final int index, final String name) {
        return a.hasValue(index)
                ? String.format(" %s=%s", name, a.getBoolean(index, false)) : "";
    }
}
