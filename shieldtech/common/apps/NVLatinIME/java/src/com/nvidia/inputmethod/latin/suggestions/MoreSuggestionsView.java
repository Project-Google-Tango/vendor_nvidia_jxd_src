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

package com.nvidia.inputmethod.latin.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnKeyListener;
import android.view.KeyEvent;
import android.widget.PopupWindow;

import com.nvidia.inputmethod.keyboard.KeyDetector;
import com.nvidia.inputmethod.keyboard.Keyboard;
import com.nvidia.inputmethod.keyboard.KeyboardActionListener;
import com.nvidia.inputmethod.keyboard.KeyboardView;
import com.nvidia.inputmethod.keyboard.MoreKeysDetector;
import com.nvidia.inputmethod.keyboard.MoreKeysPanel;
import com.nvidia.inputmethod.keyboard.PointerTracker;
import com.nvidia.inputmethod.latin.LatinIME;
import com.nvidia.inputmethod.keyboard.PointerTracker.DrawingProxy;
import com.nvidia.inputmethod.keyboard.PointerTracker.KeyEventHandler;
import com.nvidia.inputmethod.keyboard.PointerTracker.TimerProxy;
import com.nvidia.inputmethod.latin.R;

/**
 * A view that renders a virtual {@link MoreSuggestions}. It handles rendering of keys and detecting
 * key presses and touch movements.
 */
public final class MoreSuggestionsView extends KeyboardView implements MoreKeysPanel {
    private final int[] mCoordinates = new int[2];
    final static String TAG = "MORE_SUGGESTIONS_VIEW";
    final KeyDetector mModalPanelKeyDetector;
    private final KeyDetector mSlidingPanelKeyDetector;

    private Controller mController;
    KeyboardActionListener mListener;
    private int mOriginX;
    private int mOriginY;
    private boolean touched_view = false;

    static final TimerProxy EMPTY_TIMER_PROXY = new TimerProxy.Adapter();

    final KeyboardActionListener mSuggestionsPaneListener =
            new KeyboardActionListener.Adapter() {
        @Override
        public void onPressKey(int primaryCode) {
            mListener.onPressKey(primaryCode);
        }

        @Override
        public void onReleaseKey(int primaryCode, boolean withSliding) {
            mListener.onReleaseKey(primaryCode, withSliding);
        }

        @Override
        public void onCodeInput(int primaryCode, int x, int y) {
            final int index = primaryCode - MoreSuggestions.SUGGESTION_CODE_BASE;
            if (index >= 0 && index < SuggestionStripView.MAX_SUGGESTIONS) {
                mListener.onCustomRequest(index);
            }
        }

        @Override
        public void onCancelInput() {
            mListener.onCancelInput();
        }
    };

    public MoreSuggestionsView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.moreSuggestionsViewStyle);
    }

    public MoreSuggestionsView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        final Resources res = context.getResources();
        mModalPanelKeyDetector = new KeyDetector(/* keyHysteresisDistance */ 0);
        mSlidingPanelKeyDetector = new MoreKeysDetector(
                res.getDimension(R.dimen.more_suggestions_slide_allowance));
        setKeyPreviewPopupEnabled(false, 0);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final Keyboard keyboard = getKeyboard();
        if (keyboard != null) {
            final int width = keyboard.mOccupiedWidth + getPaddingLeft() + getPaddingRight();
            final int height = keyboard.mOccupiedHeight + getPaddingTop() + getPaddingBottom();
            setMeasuredDimension(width, height);
        } else {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    public void updateKeyboardGeometry(final int keyHeight) {
        mKeyDrawParams.updateParams(keyHeight, mKeyVisualAttributes);
    }

    @Override
    public void setKeyboard(Keyboard keyboard) {
        super.setKeyboard(keyboard);
        mModalPanelKeyDetector.setKeyboard(keyboard, -getPaddingLeft(), -getPaddingTop());
        mSlidingPanelKeyDetector.setKeyboard(keyboard, -getPaddingLeft(),
                -getPaddingTop() + mVerticalCorrection);
    }

    @Override
    public KeyDetector getKeyDetector() {
        return mSlidingPanelKeyDetector;
    }

    @Override
    public KeyboardActionListener getKeyboardActionListener() {
        return mSuggestionsPaneListener;
    }

    @Override
    public DrawingProxy getDrawingProxy() {
        return this;
    }

    @Override
    public TimerProxy getTimerProxy() {
        return EMPTY_TIMER_PROXY;
    }

    @Override
    public void setKeyPreviewPopupEnabled(boolean previewEnabled, int delay) {
        // Suggestions pane needs no pop-up key preview displayed, so we pass always false with a
        // delay of 0. The delay does not matter actually since the popup is not shown anyway.
        super.setKeyPreviewPopupEnabled(false, 0);
    }

    @Override
    public void showMoreKeysPanel(View parentView, Controller controller, int pointX, int pointY,
            PopupWindow window, KeyboardActionListener listener) {
        touched_view = false;
        mController = controller;
        mListener = listener;
        final View container = (View)getParent();
        container.setFocusableInTouchMode(true);
        MoreSuggestions moreSug = (MoreSuggestions) MoreSuggestionsView.this.getKeyboard();
        if(moreSug.getSelected() == null) {
            moreSug.selectDefault(true);
        }

        container.setOnKeyListener(new OnKeyListener() {
            MoreSuggestions moreSug = (MoreSuggestions) MoreSuggestionsView.this.getKeyboard();
            boolean apressed = false;
            boolean firstRelease = false;
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                PointerTracker tracker = PointerTracker.getPointerTracker(LatinIME.moreKeysTouchIndex, MoreSuggestionsView.this);
                if (event.getAction() == KeyEvent.ACTION_DOWN) {
                    if(moreSug.getSelected() ==null){
                        moreSug.selectDefault(true);
                    }
                    switch (keyCode) {
                        case KeyEvent.KEYCODE_BUTTON_A:
                            if(firstRelease && !touched_view) {
                                tracker.onDownEvent(moreSug.getSelected().mX + moreSug.getSelected().mWidth/2, moreSug.getSelected().mY + (int)(moreSug.getSelected().mHeight*.8), System.currentTimeMillis(), MoreSuggestionsView.this, MoreSuggestionsView.this.getKeyboard());
                            }
                            break;

                        //these buttons do nothing on key down
                        case KeyEvent.KEYCODE_BUTTON_X:
                        case KeyEvent.KEYCODE_BUTTON_Y:
                            return true;

                        case KeyEvent.KEYCODE_DPAD_UP:
                            if (!touched_view){
                                moreSug.selectUp();
                                invalidateAllKeys();
                                }
                            break;
                        case KeyEvent.KEYCODE_DPAD_DOWN:
                            if (!touched_view){
                                moreSug.selectDown();
                                invalidateAllKeys();
                            }
                            break;
                        case KeyEvent.KEYCODE_DPAD_LEFT:
                            if (!touched_view){
                                moreSug.selectLeft();
                                invalidateAllKeys();
                            }
                            break;
                        case KeyEvent.KEYCODE_DPAD_RIGHT:
                            if (!touched_view){
                                moreSug.selectRight();
                                invalidateAllKeys();
                            }
                            break;

                            //let the B keycode handle normally
                        case KeyEvent.KEYCODE_BUTTON_B:
                        default:
                            return false;
                    }
                    return true;

                } else if (event.getAction() == KeyEvent.ACTION_UP) {
                    //handle a button
                    MoreSuggestions moreSug = (MoreSuggestions) MoreSuggestionsView.this.getKeyboard();
                    switch (keyCode) {
                        case KeyEvent.KEYCODE_BUTTON_A:
                            if (firstRelease) {
                                tracker.onUpEvent(moreSug.getSelected().mX + moreSug.getSelected().mWidth/2, moreSug.getSelected().mY + (int)(moreSug.getSelected().mHeight*.8), System.currentTimeMillis());
                                apressed = false;
                            } else {
                                firstRelease = true;
                            }
                            break;

                        //these buttons do nothing on key up
                        case KeyEvent.KEYCODE_BUTTON_X:
                        case KeyEvent.KEYCODE_BUTTON_Y:
                            return true;

                        case KeyEvent.KEYCODE_DPAD_UP:
                        case KeyEvent.KEYCODE_DPAD_DOWN:
                        case KeyEvent.KEYCODE_DPAD_LEFT:
                        case KeyEvent.KEYCODE_DPAD_RIGHT:
                            return true;

                            //let the B keycode handle normally
                        case KeyEvent.KEYCODE_BUTTON_B:
                        default:
                            return false;
                    }
                }
                return false;
        }});

        final MoreSuggestions pane = (MoreSuggestions)getKeyboard();
        final int defaultCoordX = pane.mOccupiedWidth / 2;
        // The coordinates of panel's left-top corner in parentView's coordinate system.
        final int x = pointX - defaultCoordX - container.getPaddingLeft();
        final int y = pointY - container.getMeasuredHeight() + container.getPaddingBottom();

        window.setContentView(container);
        window.setWidth(container.getMeasuredWidth());
        window.setHeight(container.getMeasuredHeight());
        parentView.getLocationInWindow(mCoordinates);
        window.showAtLocation(parentView, Gravity.NO_GRAVITY,
                x + mCoordinates[0], y + mCoordinates[1]);

        mOriginX = x + container.getPaddingLeft();
        mOriginY = y + container.getPaddingTop();
    }

    private boolean mIsDismissing;

    @Override
    public boolean dismissMoreKeysPanel() {
        if (mIsDismissing || mController == null) return false;
        mIsDismissing = true;
        final boolean dismissed = mController.dismissMoreKeysPanel();
        mIsDismissing = false;
        return dismissed;
    }

    @Override
    public int translateX(int x) {
        return x - mOriginX;
    }

    @Override
    public int translateY(int y) {
        return y - mOriginY;
    }

    private final KeyEventHandler mModalPanelKeyEventHandler = new KeyEventHandler() {
        @Override
        public KeyDetector getKeyDetector() {
            return mModalPanelKeyDetector;
        }

        @Override
        public KeyboardActionListener getKeyboardActionListener() {
            return mSuggestionsPaneListener;
        }

        @Override
        public DrawingProxy getDrawingProxy() {
            return MoreSuggestionsView.this;
        }

        @Override
        public TimerProxy getTimerProxy() {
            return EMPTY_TIMER_PROXY;
        }
    };

    @Override
    public boolean onTouchEvent(MotionEvent me) {
        final int action = me.getAction();
        if (! touched_view) {
            getKeyboard().clearAllKeySelection();
            invalidateAllKeys();
            touched_view = true;
        }
        final long eventTime = me.getEventTime();
        final int index = me.getActionIndex();
        final int id = me.getPointerId(index);
        final PointerTracker tracker = PointerTracker.getPointerTracker(id, this);
        final int x = (int)me.getX(index);
        final int y = (int)me.getY(index);
        tracker.processMotionEvent(action, x, y, eventTime, mModalPanelKeyEventHandler);
        return true;
    }
}
