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

#include <unistd.h>
#include <linux/input.h>

#include "recovery_ui.h"
#include "common.h"

char* MENU_HEADERS[] = {
    "Use arrow keys to highlight, enter to select.",
    "",
    NULL
};

char* MENU_ITEMS[] = {
    "Reboot system now [Alt+R]",
    "Apply update from USB (update.zip) [Alt+S]",
    "Wipe data/factory reset [Alt+W]",
    "Wipe cache partition",
    NULL
};

int device_toggle_display(volatile char* key_pressed, int key_code) {
    // alt+L (either alt key)
    return (key_pressed[KEY_LEFTALT] || key_pressed[KEY_RIGHTALT]) &&
            key_code == KEY_L;
}

int device_reboot_now(volatile char* key_pressed, int key_code) {
    // alt+R (either alt key)
    return (key_pressed[KEY_LEFTALT] || key_pressed[KEY_RIGHTALT]) &&
            key_code == KEY_R;
}

int device_handle_key(int key, int visible) {
    int alt = ui_key_pressed(KEY_LEFTALT) || ui_key_pressed(KEY_RIGHTALT);

    if (key == KEY_BACK && ui_key_pressed(KEY_HOME)) {
        // Wait for the keys to be released, to avoid triggering
        // special boot modes (like coming back into recovery!).
        while (ui_key_pressed(KEY_BACK) ||
               ui_key_pressed(KEY_HOME)) {
            usleep(1000);
        }
        return ITEM_REBOOT;
    } else if (alt && key == KEY_W) {
        return ITEM_WIPE_DATA;
    } else if (alt && key == KEY_S) {
        return ITEM_APPLY_SDCARD;
    } else if (visible) {
        switch (key) {
            case KEY_DOWN:
                return HIGHLIGHT_DOWN;
            case KEY_UP:
                return HIGHLIGHT_UP;
            case KEY_LEFT:
                return HIGHLIGHT_LEFT;
            case KEY_RIGHT:
                return HIGHLIGHT_RIGHT;
            case BTN_MOUSE:
            case KEY_ENTER:
                return SELECT_ITEM;
        }
    }

    return NO_ACTION;
}

int device_perform_action(int which) {
    return which;
}

int device_wipe_data() {
    return 0;
}

int device_recovery_start() {
    return 0;
}

void device_ui_init(UIParameters* ui_parameters) {
}
