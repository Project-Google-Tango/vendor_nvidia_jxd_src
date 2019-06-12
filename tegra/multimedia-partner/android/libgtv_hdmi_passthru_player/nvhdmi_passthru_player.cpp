/*
**
** Copyright 2011, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "NVIDIAHDMIPassthruPlayer"
#include <cutils/log.h>

#include <sys/prctl.h>

#include <SkBitmap.h>
#include <SkCanvas.h>
#include <SkPaint.h>
#include <SkScalar.h>
#include <SkTypeface.h>

#include <sstream>

#include "nvhdmi_passthru_player.h"
#include <fcntl.h>

namespace android {

// Provide a way to create a HDMIInputMonitor from mediaserver.
void* createGoogleTVHDMIInputMonitor() {
  // TODO: Implement this.
  return NULL;
}

sp<MediaPlayerBase> createGoogleTVHDMIPassthruPlayer(player_type playerType) {
    ALOGE("createGoogleTVHDMIPassthruPlayer");
    return new nvidia::NvHDMIPassthruPlayer(playerType);
}

}  // namespace android

using namespace nvidia;
using namespace std;

namespace {

// Helper class that locks a Surface and wraps it with a Skia bitmap.
SkBitmap::Config convertFormat(int surfaceFormat) {
    switch(surfaceFormat) {
        case android::PIXEL_FORMAT_RGBA_8888:
            return SkBitmap::kARGB_8888_Config;

        case android::PIXEL_FORMAT_RGB_565:
            return SkBitmap::kRGB_565_Config;

        default:
            return SkBitmap::kNo_Config;
    }
}

class SurfaceBitmap {
  public:
    explicit SurfaceBitmap(const android::sp<android::Surface>& surface)
            : surface_(surface), is_locked_(false) {}

    ~SurfaceBitmap() {
        if (is_locked_)
            Unlock();
    }

    // Lock the surface
    bool Lock() {
        assert(!is_locked_);

        android::Surface::SurfaceInfo surface_info;
        if (surface_->lock(&surface_info) != android::OK) {
            LOGE("Unable to lock surface");
            return false;
        }

        const SkBitmap::Config config = convertFormat(surface_info.format);
        if (SkBitmap::kNo_Config == config) {
            LOGE("Unhandled color space");
            return false;
        }

        // Create an SkBitmap that wraps the surface buffer
        int row_bytes = surface_info.s *
                SkBitmap::ComputeBytesPerPixel(config);
        width_ = surface_info.w;
        height_ = surface_info.h;
        bitmap_.setConfig(config, width_, height_, row_bytes);
        bitmap_.setPixels(surface_info.bits);
        bitmap_.setIsOpaque(true);

        is_locked_ = true;
        return true;
    }

    // Return an SkBitmap representing the locked surface buffer
    SkBitmap& bitmap() {
        assert(is_locked_);
        return bitmap_;
    }

    int width() {
        return width_;
    }

    int height() {
        return height_;
    }

    // Unlock and post the surface
    void Unlock() {
        assert(is_locked_);
        surface_->unlockAndPost();
        bitmap_.reset();
        is_locked_ = false;
    }

  private:
    android::sp<android::Surface> surface_;
    SkBitmap bitmap_;
    bool is_locked_;
    int width_, height_;
};

// Specifies sizes of UI components expressed
// in percents relative to screen size
const int kVerticalBarsSize = 70;
const int kHorizontalBarsSize = 10;
const int kBarsSize = kVerticalBarsSize + kHorizontalBarsSize;
const int kChannelNumberSize = 20;
const int kCallSignSize = 20;
const int kTimeDisplaySize = 10;

const SkColor kColorLightGray = SkColorSetRGB(204, 204, 204);
const SkColor kColorDarkGray = SkColorSetRGB(115, 115, 115);
const SkColor kColorYellow = SkColorSetRGB(205, 204, 0);
const SkColor kColorCyan = SkColorSetRGB(0, 204, 203);
const SkColor kColorGreen = SkColorSetRGB(1, 204, 1);
const SkColor kColorMagenta = SkColorSetRGB(205, 0, 203);
const SkColor kColorRed = SkColorSetRGB(204, 0, 1);
const SkColor kColorBlue = SkColorSetRGB(1, 0, 204);

// Given font size doesn't reflect the actual height,
// calculate it for capital letters
int getTextHeight(const SkPaint& paint) {
    SkRect rect;
    paint.measureText("X", 1, &rect, 0);
    return rect.height();
}

// Render color stripes
void renderBars(SkCanvas& canvas, SkPaint& paint, int top, int bottom,
                int screen_width, const SkColor colors[], int num_bars) {
    int bar_width = screen_width / num_bars;
    int left, right = 0;
    for (int i = 0; i < num_bars; i++) {
        left = right;
        right = (i + 1) * screen_width / num_bars;
        SkRect rect = SkRect::MakeLTRB(left, top, right, bottom);
        paint.setColor(colors[i]);
        canvas.drawRect(rect, paint);
    }
}

// Draw striped background + placeholder
// for channel information and time simulation
void renderBackground(SkCanvas& canvas, int screen_width, int screen_height) {
    SkPaint paint;

    // render vertical bars
    const SkColor kVerticalColors[] = {
            kColorLightGray,
            kColorYellow,
            kColorCyan,
            kColorGreen,
            kColorMagenta,
            kColorRed,
            kColorBlue };
    int top = 0;
    int bottom = screen_height * kVerticalBarsSize / 100;
    renderBars(canvas, paint, top, bottom, screen_width,
            kVerticalColors, NELEM(kVerticalColors));

    // render horizontal bars
    const SkColor kHorizontalColors[] = {
        kColorBlue,
        SK_ColorBLACK,
        kColorMagenta,
        SK_ColorBLACK,
        kColorCyan,
        SK_ColorBLACK,
        kColorLightGray };
    top = bottom;
    bottom = screen_height * kBarsSize / 100;
    renderBars(canvas, paint, top, bottom, screen_width,
            kHorizontalColors, NELEM(kHorizontalColors));

    // render channel number/name/metadata background
    top = bottom;
    bottom = screen_height;
    int left = 0;
    int right = screen_width * kChannelNumberSize / 100;
    SkRect rect = SkRect::MakeLTRB(left, top, right, bottom);
    paint.setColor(kColorDarkGray);
    canvas.drawRect(rect, paint);
    left = right;
    right = screen_width * (kChannelNumberSize + kCallSignSize) / 100;
    rect = SkRect::MakeLTRB(left, top, right, bottom);
    paint.setColor(kColorLightGray);
    canvas.drawRect(rect, paint);
    left = right;
    right = screen_width;
    rect = SkRect::MakeLTRB(left, top, right, bottom);
    paint.setColor(SK_ColorBLACK);
    canvas.drawRect(rect, paint);
}

// Draws channel number and call sign
void renderLogo(SkCanvas& canvas,
                int screen_width, int screen_height) {
    static SkTypeface * const kTypeface = SkTypeface::CreateFromName(
            "Sans", SkTypeface::kBold);
    static float kChannelHeights[] =
            { 0.8, 0.8, 0.8, 0.8, 0.6, 0.48 };
    static float kCallSignHeights[] =
            { 0.7, 0.7, 0.7, 0.7, 0.525, 0.42, 0.35, 0.3 };
    static int channel = 0;

    // chose channel numer size so it would fit based on numer length
    int bar_height = (100 - kBarsSize) * screen_height / 100;
    stringstream channel_number;
    channel_number << channel++;
    unsigned int length = channel_number.str().length();
    int text_size;
    if (length < NELEM(kChannelHeights)) {
        text_size = kChannelHeights[length] * bar_height;
    } else {
        ALOGV("Unexpected channel number length %d", length);
        text_size = 0.5 * bar_height;
    }

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setTextSize(text_size);
    paint.setColor(kColorLightGray);
    paint.setTextAlign(SkPaint::kCenter_Align);
    paint.setTypeface(kTypeface);
    int x = screen_width * kChannelNumberSize / 100 / 2;
    int y = screen_height - (bar_height - getTextHeight(paint)) / 2;
    canvas.drawText(channel_number.str().c_str(), length, x, y, paint);

    length = 4;
    if (length < NELEM(kCallSignHeights)) {
        text_size = kCallSignHeights[length] * bar_height;
    } else {
        ALOGV("Unexpected call sign length %d", length);
        text_size = 0.3 * bar_height;
    }

    paint.setTextSize(text_size);
    paint.setTextScaleX(0.8);
    paint.setColor(SK_ColorBLACK);
    paint.setTextAlign(SkPaint::kCenter_Align);
    x = screen_width * (kChannelNumberSize + (kCallSignSize / 2)) / 100;
    y = screen_height - (bar_height - getTextHeight(paint)) / 2;
    canvas.drawText("KDVR", length, x, y, paint);
}

// Draw program/recording name with some meta information if available
void renderMetadata(SkCanvas& canvas,
                    int screen_width, int screen_height) {
    // the dimenstions are in % of screen height
    const int kFontSize = 3;
    const int kMargin = 2;

    int text_size = kFontSize * screen_width / 100;
    int margin = kMargin * screen_width / 100;

    // set clipping rect to leave space for running time
    int top = kBarsSize * screen_height / 100;
    int left = (kChannelNumberSize + kCallSignSize) * screen_width / 100;
    int right = (100 - kTimeDisplaySize) * screen_width / 100;
    SkRect clip_rect = SkRect::MakeLTRB(left, top, right, screen_height);
    canvas.save();
    canvas.clipRect(clip_rect);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setTextSize(text_size);
    paint.setColor(SK_ColorWHITE);
    paint.setTextAlign(SkPaint::kLeft_Align);
    left += margin;
    top += margin + getTextHeight(paint);
    canvas.drawText("News", 4,
                    left, top, paint);

    canvas.restore();
}

// Draw simulated time elapse
void renderClock(SkCanvas& canvas,
                     int screen_width, int screen_height) {
    // size is expressed in % of screen height
    const int kFontSize = 3;

    int text_size = kFontSize * screen_height / 100;

    char text[10];
    // print time hh:mm:ss
    sprintf(text, "10:05:23");

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setTextAlign(SkPaint::kCenter_Align);
    paint.setTextSize(text_size);
    paint.setColor(SK_ColorWHITE);
    int x = screen_width * (100 - (kTimeDisplaySize / 2)) / 100;
    int y = (screen_height * (100 + kBarsSize) / 100 +
            getTextHeight(paint)) / 2;
    canvas.drawText(text, strlen(text), x, y, paint);
}

// Performs the rendering
void render(SkBitmap& bitmap) {
    SkCanvas canvas(bitmap);
    canvas.drawColor(SK_ColorWHITE, SkXfermode::kSrc_Mode);

    renderBackground(canvas, bitmap.width(), bitmap.height());
    renderLogo(canvas, bitmap.width(), bitmap.height());
    renderMetadata(canvas, bitmap.width(), bitmap.height());
    renderClock(canvas, bitmap.width(), bitmap.height());
}

}  // anonymous namespace

namespace nvidia {

NvHDMIPassthruPlayer::NvHDMIPassthruPlayer(android::player_type playerType) :
    HDMIPassthruPlayer(playerType), work_thread_(0) {

    ALOGV("constructor");
    mPlayer = new android::MooPlayerDriver(playerType);

    // Set the callbacks
    if (mPlayer != NULL) {
        mPlayer->setNotifyCallback(this, notify);
    }

//    int fd = open("/sdcard/rat.mp4", O_RDONLY);
//    ALOGV("FILE OPEN RAT.MP4 %d", fd);
//    mPlayer->setDataSource(fd, 0, 11193445);

}

NvHDMIPassthruPlayer::~NvHDMIPassthruPlayer() {
    ALOGV("deconstructor");
    if (work_thread_) {
        stop();
    }
}

android::status_t NvHDMIPassthruPlayer::initCheck() {
    ALOGV("initcheck");
    return android::OK;
}

android::status_t NvHDMIPassthruPlayer::setDataSource(
        const char *url,
        const android::KeyedVector<android::String8,
        android::String8> *headers) {
    ALOGV("setdatasource url %s", url);
    mPlayer->setDataSource(url, headers);
    return android::OK;
}

android::status_t NvHDMIPassthruPlayer::setDataSource(
        int fd, int64_t offset, int64_t length) {
    ALOGV("setdatasource fd %d", fd);
    mPlayer->setDataSource(fd, offset, length);
    return android::OK;
}

android::status_t NvHDMIPassthruPlayer::setVideoSurfaceTexture(
        const android::sp<android::ISurfaceTexture>& surfaceTexture) {
  ALOGV("setVideoSurfaceTexture");
  mPlayer->setVideoSurfaceTexture(surfaceTexture);
  return android::OK;
}

android::status_t NvHDMIPassthruPlayer::prepareAsync() {
    ALOGV("prepareasync");
//    sendEvent(android::MEDIA_PREPARED);
    mPlayer->prepareAsync();
    return android::OK;
}

android::status_t NvHDMIPassthruPlayer::start() {
    ALOGV("start");
    if (0 != work_thread_) {
        LOGE("Player is already playing");
        return android::INVALID_OPERATION;
    }
    thread_stop_ = false;
    pthread_create(&work_thread_, NULL, staticWorkThreadEntry, this);

    mPlayer->start();
    return android::OK;
}

android::status_t NvHDMIPassthruPlayer::stop() {
    ALOGV("stop");
    if (!work_thread_) {
        LOGE("Player is not running");
        return android::INVALID_OPERATION;
    }

    void* thread_ret;
    thread_stop_ = true;
    pthread_join(work_thread_, &thread_ret);
    work_thread_ = 0;
    mPlayer->stop();

    return android::OK;
}

bool NvHDMIPassthruPlayer::isPlaying() {
    ALOGV("isplaying");
    return (0 != work_thread_);
}

android::status_t NvHDMIPassthruPlayer::reset() {
    ALOGV("reset");
    mPlayer->reset();
    return android::OK;
}

status_t NvHDMIPassthruPlayer::setVideoRectangle(
        int x, int y, int width, int height) {
    ALOGV("Unhandled setVideoRectangle called");
    return android::OK;
}

android::status_t NvHDMIPassthruPlayer::setVolume(
        float leftVolume, float rightVolume) {
    ALOGV("setvolume");
    return android::OK;
}

bool NvHDMIPassthruPlayer::getActiveAudioSubstreamInfo(
        ActiveAudioSubstreamInfo* info) {
    ALOGV("getactiveaudio");
    return false;
}

bool NvHDMIPassthruPlayer::getActiveVideoSubstreamInfo(
        ActiveVideoSubstreamInfo* info) {
    ALOGV("getactivevideo");
    return false;
}


void* NvHDMIPassthruPlayer::staticWorkThreadEntry(void* thiz) {
    NvHDMIPassthruPlayer* player = reinterpret_cast<NvHDMIPassthruPlayer*>(thiz);
    assert(player);

    ALOGV("worker thread");
    // Give our thread a name to assist in debugging.
    prctl(PR_SET_NAME, "hdmi_passthru_worker");

    return player->workThreadEntry();
}

void* NvHDMIPassthruPlayer::workThreadEntry() {

    while(!thread_stop_) {
/*
        SurfaceBitmap bitmap(surface_);
        if (bitmap.Lock()) {
            render(bitmap.bitmap());
        } else {
            LOGE("Unable to lock surface");
        }
*/
        usleep(1000000 / 30);
    }
    return NULL;
}

void NvHDMIPassthruPlayer::notify(void* cookie, int msg, int ext1, int ext2, const android::Parcel *obj) {
    NvHDMIPassthruPlayer* player = static_cast<NvHDMIPassthruPlayer*>(cookie);

    if (player == NULL) {
        ALOGE("notify cookie invalid");
        return;
    }

    player->sendEvent(msg, ext1, ext2, obj);
}

}  // namespace nvidia
