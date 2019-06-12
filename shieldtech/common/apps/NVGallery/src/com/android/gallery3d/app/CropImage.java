/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.gallery3d.app;

import android.annotation.TargetApi;
import android.app.ActionBar;
import android.app.ProgressDialog;
import android.app.WallpaperManager;
import android.content.ContentValues;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapFactory;
import android.graphics.BitmapRegionDecoder;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.provider.MediaStore;
import android.provider.MediaStore.Images;
import android.util.FloatMath;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Toast;

import com.android.camera.Util;
import com.android.gallery3d.R;
import com.android.gallery3d.common.ApiHelper;
import com.android.gallery3d.common.BitmapUtils;
import com.android.gallery3d.common.Utils;
import com.android.gallery3d.data.DataManager;
import com.android.gallery3d.data.LocalImage;
import com.android.gallery3d.data.MediaItem;
import com.android.gallery3d.data.MediaObject;
import com.android.gallery3d.data.Path;
import com.android.gallery3d.exif.ExifData;
import com.android.gallery3d.exif.ExifOutputStream;
import com.android.gallery3d.exif.ExifReader;
import com.android.gallery3d.exif.ExifTag;
import com.android.gallery3d.picasasource.PicasaSource;
import com.android.gallery3d.ui.BitmapScreenNail;
import com.android.gallery3d.ui.BitmapTileProvider;
import com.android.gallery3d.ui.CropView;
import com.android.gallery3d.ui.GLRoot;
import com.android.gallery3d.ui.GLRootView;
import com.android.gallery3d.ui.SynchronizedHandler;
import com.android.gallery3d.ui.TileImageViewAdapter;
import com.android.gallery3d.util.BucketNames;
import com.android.gallery3d.util.Future;
import com.android.gallery3d.util.FutureListener;
import com.android.gallery3d.util.GalleryUtils;
import com.android.gallery3d.util.InterruptableOutputStream;
import com.android.gallery3d.util.ThreadPool.CancelListener;
import com.android.gallery3d.util.ThreadPool.Job;
import com.android.gallery3d.util.ThreadPool.JobContext;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteOrder;
import java.text.SimpleDateFormat;
import java.util.Date;

/**
 * The activity can crop specific region of interest from an image.
 */
public class CropImage extends AbstractGalleryActivity {
    private static final String TAG = "CropImage";
    public static final String ACTION_CROP = "com.android.camera.action.CROP";

    private static final int MAX_PIXEL_COUNT = 5 * 1000000; // 5M pixels
    private static final int MAX_FILE_INDEX = 1000;
    private static final int TILE_SIZE = 512;
    private static final int BACKUP_PIXEL_COUNT = 480000; // around 800x600

    private static final int MSG_LARGE_BITMAP = 1;
    private static final int MSG_BITMAP = 2;
    private static final int MSG_SAVE_COMPLETE = 3;
    private static final int MSG_SHOW_SAVE_ERROR = 4;
    private static final int MSG_CANCEL_DIALOG = 5;

    private static final int MAX_BACKUP_IMAGE_SIZE = 320;
    private static final int DEFAULT_COMPRESS_QUALITY = 90;
    private static final String TIME_STAMP_NAME = "'IMG'_yyyyMMdd_HHmmss";

    public static final String KEY_RETURN_DATA = "return-data";
    public static final String KEY_CROPPED_RECT = "cropped-rect";
    public static final String KEY_ASPECT_X = "aspectX";
    public static final String KEY_ASPECT_Y = "aspectY";
    public static final String KEY_SPOTLIGHT_X = "spotlightX";
    public static final String KEY_SPOTLIGHT_Y = "spotlightY";
    public static final String KEY_OUTPUT_X = "outputX";
    public static final String KEY_OUTPUT_Y = "outputY";
    public static final String KEY_SCALE = "scale";
    public static final String KEY_DATA = "data";
    public static final String KEY_SCALE_UP_IF_NEEDED = "scaleUpIfNeeded";
    public static final String KEY_OUTPUT_FORMAT = "outputFormat";
    public static final String KEY_SET_AS_WALLPAPER = "set-as-wallpaper";
    public static final String KEY_NO_FACE_DETECTION = "noFaceDetection";
    public static final String KEY_SHOW_WHEN_LOCKED = "showWhenLocked";

    private static final String KEY_STATE = "state";

    private static final int STATE_INIT = 0;
    private static final int STATE_LOADED = 1;
    private static final int STATE_SAVING = 2;

    public static final File DOWNLOAD_BUCKET = new File(
            Environment.getExternalStorageDirectory(), BucketNames.DOWNLOAD);

    public static final String CROP_ACTION = "com.android.camera.action.CROP";

    private int mState = STATE_INIT;

    private CropView mCropView;

    private boolean mDoFaceDetection = true;

    private Handler mMainHandler;

    // We keep the following members so that we can free them

    // mBitmap is the unrotated bitmap we pass in to mCropView for detect faces.
    // mCropView is responsible for rotating it to the way that it is viewed by users.
    private Bitmap mBitmap;
    private BitmapTileProvider mBitmapTileProvider;
    private BitmapRegionDecoder mRegionDecoder;
    private Bitmap mBitmapInIntent;
    private boolean mUseRegionDecoder = false;
    private BitmapScreenNail mBitmapScreenNail;

    private ProgressDialog mProgressDialog;
    private Future<BitmapRegionDecoder> mLoadTask;
    private Future<Bitmap> mLoadBitmapTask;
    private Future<Intent> mSaveTask;

    private MediaItem mMediaItem;

    private boolean mIsActionBarFocus = false;

    public boolean isActionBarFocus() {
        return mIsActionBarFocus;
    }

    public void enableActionBarFocus() {
        mIsActionBarFocus = true;
        getActionBar().requestFocus();
    }

    public void disableActionBarFocus() {
        mIsActionBarFocus = false;
        GLRootView rootLayout = (GLRootView) findViewById(R.id.gl_root_view);
        rootLayout.requestFocus();
    }

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        requestWindowFeature(Window.FEATURE_ACTION_BAR);
        requestWindowFeature(Window.FEATURE_ACTION_BAR_OVERLAY);

        // Initialize UI
        setContentView(R.layout.cropimage);
        mCropView = new CropView(this);
        getGLRoot().setContentPane(mCropView);

        ActionBar actionBar = getActionBar();
        int displayOptions = ActionBar.DISPLAY_HOME_AS_UP
                | ActionBar.DISPLAY_SHOW_TITLE;
        actionBar.setDisplayOptions(displayOptions, displayOptions);

        Bundle extra = getIntent().getExtras();
        if (extra != null) {
            if (extra.getBoolean(KEY_SET_AS_WALLPAPER, false)) {
                actionBar.setTitle(getString(R.string.set_wallpaper));
            }
            if (extra.getBoolean(KEY_SHOW_WHEN_LOCKED, false)) {
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
            }
        }

        mMainHandler = new SynchronizedHandler(getGLRoot()) {
            @Override
            public void handleMessage(Message message) {
                switch (message.what) {
                    case MSG_LARGE_BITMAP: {
                        dismissProgressDialogIfShown();
                        onBitmapRegionDecoderAvailable((BitmapRegionDecoder) message.obj);
                        break;
                    }
                    case MSG_BITMAP: {
                        dismissProgressDialogIfShown();
                        onBitmapAvailable((Bitmap) message.obj);
                        break;
                    }
                    case MSG_SHOW_SAVE_ERROR: {
                        dismissProgressDialogIfShown();
                        setResult(RESULT_CANCELED);
                        Toast.makeText(CropImage.this,
                                CropImage.this.getString(R.string.save_error),
                                Toast.LENGTH_LONG).show();
                        finish();
                    }
                    case MSG_SAVE_COMPLETE: {
                        dismissProgressDialogIfShown();
                        setResult(RESULT_OK, (Intent) message.obj);
                        finish();
                        break;
                    }
                    case MSG_CANCEL_DIALOG: {
                        setResult(RESULT_CANCELED);
                        finish();
                        break;
                    }
                }
            }
        };

        setCropParameters();
    }

    @Override
    protected void onSaveInstanceState(Bundle saveState) {
        saveState.putInt(KEY_STATE, mState);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        getMenuInflater().inflate(R.menu.crop, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home: {
                finish();
                break;
            }
            case R.id.cancel: {
                setResult(RESULT_CANCELED);
                finish();
                break;
            }
            case R.id.save: {
                onSaveClicked();
                break;
            }
        }
        return true;
    }

    @Override
    public void onBackPressed() {
        finish();
    }

    private class SaveOutput implements Job<Intent> {
        private final RectF mCropRect;

        public SaveOutput(RectF cropRect) {
            mCropRect = cropRect;
        }

        @Override
        public Intent run(JobContext jc) {
            RectF cropRect = mCropRect;
            Bundle extra = getIntent().getExtras();

            Rect rect = new Rect(
                    Math.round(cropRect.left), Math.round(cropRect.top),
                    Math.round(cropRect.right), Math.round(cropRect.bottom));

            Intent result = new Intent();
            result.putExtra(KEY_CROPPED_RECT, rect);
            Bitmap cropped = null;
            boolean outputted = false;
            if (extra != null) {
                Uri uri = (Uri) extra.getParcelable(MediaStore.EXTRA_OUTPUT);
                if (uri != null) {
                    if (jc.isCancelled()) return null;
                    outputted = true;
                    cropped = getCroppedImage(rect);
                    if (!saveBitmapToUri(jc, cropped, uri)) return null;
                }
                if (extra.getBoolean(KEY_RETURN_DATA, false)) {
                    if (jc.isCancelled()) return null;
                    outputted = true;
                    if (cropped == null) cropped = getCroppedImage(rect);
                    result.putExtra(KEY_DATA, cropped);
                }
                if (extra.getBoolean(KEY_SET_AS_WALLPAPER, false)) {
                    if (jc.isCancelled()) return null;
                    outputted = true;
                    if (cropped == null) cropped = getCroppedImage(rect);
                    if (!setAsWallpaper(jc, cropped)) return null;
                }
            }
            if (!outputted) {
                if (jc.isCancelled()) return null;
                if (cropped == null) cropped = getCroppedImage(rect);
                Uri data = saveToMediaProvider(jc, cropped);
                if (data != null) result.setData(data);
            }
            return result;
        }
    }

    public static String determineCompressFormat(MediaObject obj) {
        String compressFormat = "JPEG";
        if (obj instanceof MediaItem) {
            String mime = ((MediaItem) obj).getMimeType();
            if (mime.contains("png") || mime.contains("gif")) {
              // Set the compress format to PNG for png and gif images
              // because they may contain alpha values.
              compressFormat = "PNG";
            }
        }
        return compressFormat;
    }

    private boolean setAsWallpaper(JobContext jc, Bitmap wallpaper) {
        try {
            WallpaperManager.getInstance(this).setBitmap(wallpaper);
        } catch (IOException e) {
            Log.w(TAG, "fail to set wall paper", e);
        }
        return true;
    }

    private File saveMedia(
            JobContext jc, Bitmap cropped, File directory, String filename, ExifData exifData) {
        // Try file-1.jpg, file-2.jpg, ... until we find a filename
        // which does not exist yet.
        File candidate = null;
        String fileExtension = getFileExtension();
        for (int i = 1; i < MAX_FILE_INDEX; ++i) {
            candidate = new File(directory, filename + "-" + i + "."
                    + fileExtension);
            try {
                if (candidate.createNewFile()) break;
            } catch (IOException e) {
                Log.e(TAG, "fail to create new file: "
                        + candidate.getAbsolutePath(), e);
                return null;
            }
        }
        if (!candidate.exists() || !candidate.isFile()) {
            throw new RuntimeException("cannot create file: " + filename);
        }

        candidate.setReadable(true, false);
        candidate.setWritable(true, false);

        try {
            FileOutputStream fos = new FileOutputStream(candidate);
            try {
                if (exifData != null) {
                    ExifOutputStream eos = new ExifOutputStream(fos);
                    eos.setExifData(exifData);
                    saveBitmapToOutputStream(jc, cropped,
                            convertExtensionToCompressFormat(fileExtension), eos);
                } else {
                    saveBitmapToOutputStream(jc, cropped,
                            convertExtensionToCompressFormat(fileExtension), fos);
                }
            } finally {
                fos.close();
            }
        } catch (IOException e) {
            Log.e(TAG, "fail to save image: "
                    + candidate.getAbsolutePath(), e);
            candidate.delete();
            return null;
        }

        if (jc.isCancelled()) {
            candidate.delete();
            return null;
        }

        return candidate;
    }

    private ExifData getExifData(String path) {
        FileInputStream is = null;
        try {
            is = new FileInputStream(path);
            ExifReader reader = new ExifReader();
            ExifData data = reader.read(is);
            return data;
        } catch (Throwable t) {
            Log.w(TAG, "Cannot read EXIF data", t);
            return null;
        } finally {
            Util.closeSilently(is);
        }
    }

    private static final String EXIF_SOFTWARE_VALUE = "Android Gallery";

    private void changeExifData(ExifData data, int width, int height) {
        data.addTag(ExifTag.TAG_IMAGE_WIDTH).setValue(width);
        data.addTag(ExifTag.TAG_IMAGE_LENGTH).setValue(height);
        data.addTag(ExifTag.TAG_SOFTWARE).setValue(EXIF_SOFTWARE_VALUE);
        data.addTag(ExifTag.TAG_DATE_TIME).setTimeValue(System.currentTimeMillis());
        // Remove the original thumbnail
        // TODO: generate a new thumbnail for the cropped image.
        data.removeThumbnailData();
    }

    private Uri saveToMediaProvider(JobContext jc, Bitmap cropped) {
        if (PicasaSource.isPicasaImage(mMediaItem)) {
            return savePicasaImage(jc, cropped);
        } else if (mMediaItem instanceof LocalImage) {
            return saveLocalImage(jc, cropped);
        } else {
            return saveGenericImage(jc, cropped);
        }
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    private static void setImageSize(ContentValues values, int width, int height) {
        // The two fields are available since ICS but got published in JB
        if (ApiHelper.HAS_MEDIA_COLUMNS_WIDTH_AND_HEIGHT) {
            values.put(Images.Media.WIDTH, width);
            values.put(Images.Media.HEIGHT, height);
        }
    }

    private Uri savePicasaImage(JobContext jc, Bitmap cropped) {
        if (!DOWNLOAD_BUCKET.isDirectory() && !DOWNLOAD_BUCKET.mkdirs()) {
            throw new RuntimeException("cannot create download folder");
        }
        String filename = PicasaSource.getImageTitle(mMediaItem);
        int pos = filename.lastIndexOf('.');
        if (pos >= 0) filename = filename.substring(0, pos);
        ExifData exifData = new ExifData(ByteOrder.BIG_ENDIAN);
        PicasaSource.extractExifValues(mMediaItem, exifData);
        changeExifData(exifData, cropped.getWidth(), cropped.getHeight());
        File output = saveMedia(jc, cropped, DOWNLOAD_BUCKET, filename, exifData);
        if (output == null) return null;

        long now = System.currentTimeMillis() / 1000;
        ContentValues values = new ContentValues();
        values.put(Images.Media.TITLE, PicasaSource.getImageTitle(mMediaItem));
        values.put(Images.Media.DISPLAY_NAME, output.getName());
        values.put(Images.Media.DATE_TAKEN, PicasaSource.getDateTaken(mMediaItem));
        values.put(Images.Media.DATE_MODIFIED, now);
        values.put(Images.Media.DATE_ADDED, now);
        values.put(Images.Media.MIME_TYPE, getOutputMimeType());
        values.put(Images.Media.ORIENTATION, 0);
        values.put(Images.Media.DATA, output.getAbsolutePath());
        values.put(Images.Media.SIZE, output.length());
        setImageSize(values, cropped.getWidth(), cropped.getHeight());

        double latitude = PicasaSource.getLatitude(mMediaItem);
        double longitude = PicasaSource.getLongitude(mMediaItem);
        if (GalleryUtils.isValidLocation(latitude, longitude)) {
            values.put(Images.Media.LATITUDE, latitude);
            values.put(Images.Media.LONGITUDE, longitude);
        }
        return getContentResolver().insert(
                Images.Media.EXTERNAL_CONTENT_URI, values);
    }

    private Uri saveLocalImage(JobContext jc, Bitmap cropped) {
        LocalImage localImage = (LocalImage) mMediaItem;

        File oldPath = new File(localImage.filePath);
        File directory = new File(oldPath.getParent());

        String filename = oldPath.getName();
        int pos = filename.lastIndexOf('.');
        if (pos >= 0) filename = filename.substring(0, pos);
        File output = null;

        ExifData exifData = null;
        if (convertExtensionToCompressFormat(getFileExtension()) == CompressFormat.JPEG) {
            exifData = getExifData(oldPath.getAbsolutePath());
            if (exifData != null) {
                changeExifData(exifData, cropped.getWidth(), cropped.getHeight());
            }
        }
        output = saveMedia(jc, cropped, directory, filename, exifData);
        if (output == null) return null;

        long now = System.currentTimeMillis() / 1000;
        ContentValues values = new ContentValues();
        values.put(Images.Media.TITLE, localImage.caption);
        values.put(Images.Media.DISPLAY_NAME, output.getName());
        values.put(Images.Media.DATE_TAKEN, localImage.dateTakenInMs);
        values.put(Images.Media.DATE_MODIFIED, now);
        values.put(Images.Media.DATE_ADDED, now);
        values.put(Images.Media.MIME_TYPE, getOutputMimeType());
        values.put(Images.Media.ORIENTATION, 0);
        values.put(Images.Media.DATA, output.getAbsolutePath());
        values.put(Images.Media.SIZE, output.length());

        setImageSize(values, cropped.getWidth(), cropped.getHeight());

        if (GalleryUtils.isValidLocation(localImage.latitude, localImage.longitude)) {
            values.put(Images.Media.LATITUDE, localImage.latitude);
            values.put(Images.Media.LONGITUDE, localImage.longitude);
        }
        return getContentResolver().insert(
                Images.Media.EXTERNAL_CONTENT_URI, values);
    }

    private Uri saveGenericImage(JobContext jc, Bitmap cropped) {
        if (!DOWNLOAD_BUCKET.isDirectory() && !DOWNLOAD_BUCKET.mkdirs()) {
            throw new RuntimeException("cannot create download folder");
        }

        long now = System.currentTimeMillis();
        String filename = new SimpleDateFormat(TIME_STAMP_NAME).
                format(new Date(now));

        File output = saveMedia(jc, cropped, DOWNLOAD_BUCKET, filename, null);
        if (output == null) return null;

        ContentValues values = new ContentValues();
        values.put(Images.Media.TITLE, filename);
        values.put(Images.Media.DISPLAY_NAME, output.getName());
        values.put(Images.Media.DATE_TAKEN, now);
        values.put(Images.Media.DATE_MODIFIED, now / 1000);
        values.put(Images.Media.DATE_ADDED, now / 1000);
        values.put(Images.Media.MIME_TYPE, getOutputMimeType());
        values.put(Images.Media.ORIENTATION, 0);
        values.put(Images.Media.DATA, output.getAbsolutePath());
        values.put(Images.Media.SIZE, output.length());

        setImageSize(values, cropped.getWidth(), cropped.getHeight());

        return getContentResolver().insert(
                Images.Media.EXTERNAL_CONTENT_URI, values);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
                setResult(RESULT_CANCELED);
                finish();
                break;
            case KeyEvent.KEYCODE_DPAD_UP: // up
                this.enableActionBarFocus();
                break;
            case KeyEvent.KEYCODE_DPAD_DOWN: //down
                this.disableActionBarFocus();
                break;
            case KeyEvent.KEYCODE_BUTTON_A: //A button
                if (!this.isActionBarFocus()) {
                    onSaveClicked();
                    break;
                }
                else return false;
            default:
                return false;
        }
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return false;
    }

    private boolean saveBitmapToOutputStream(
            JobContext jc, Bitmap bitmap, CompressFormat format, OutputStream os) {
        // We wrap the OutputStream so that it can be interrupted.
        final InterruptableOutputStream ios = new InterruptableOutputStream(os);
        jc.setCancelListener(new CancelListener() {
                @Override
                public void onCancel() {
                    ios.interrupt();
                }
            });
        try {
            bitmap.compress(format, DEFAULT_COMPRESS_QUALITY, ios);
            return !jc.isCancelled();
        } finally {
            jc.setCancelListener(null);
            Utils.closeSilently(ios);
        }
    }

    private boolean saveBitmapToUri(JobContext jc, Bitmap bitmap, Uri uri) {
        try {
            OutputStream out = getContentResolver().openOutputStream(uri);
            try {
                return saveBitmapToOutputStream(jc, bitmap,
                        convertExtensionToCompressFormat(getFileExtension()), out);
            } finally {
                Utils.closeSilently(out);
            }
        } catch (FileNotFoundException e) {
            Log.w(TAG, "cannot write output", e);
        }
        return true;
    }

    private CompressFormat convertExtensionToCompressFormat(String extension) {
        return extension.equals("png")
                ? CompressFormat.PNG
                : CompressFormat.JPEG;
    }

    private String getOutputMimeType() {
        return getFileExtension().equals("png") ? "image/png" : "image/jpeg";
    }

    private String getFileExtension() {
        String requestFormat = getIntent().getStringExtra(KEY_OUTPUT_FORMAT);
        String outputFormat = (requestFormat == null)
                ? determineCompressFormat(mMediaItem)
                : requestFormat;

        outputFormat = outputFormat.toLowerCase();
        return (outputFormat.equals("png") || outputFormat.equals("gif"))
                ? "png" // We don't support gif compression.
                : "jpg";
    }

    private void onSaveClicked() {
        Bundle extra = getIntent().getExtras();
        RectF cropRect = mCropView.getCropRectangle();
        if (cropRect == null) return;
        mState = STATE_SAVING;
        int messageId = extra != null && extra.getBoolean(KEY_SET_AS_WALLPAPER)
                ? R.string.wallpaper
                : R.string.saving_image;
        mProgressDialog = ProgressDialog.show(
                this, null, getString(messageId), true, false);
        mSaveTask = getThreadPool().submit(new SaveOutput(cropRect),
                new FutureListener<Intent>() {
            @Override
            public void onFutureDone(Future<Intent> future) {
                mSaveTask = null;
                if (future.isCancelled()) return;
                Intent intent = future.get();
                if (intent != null) {
                    mMainHandler.sendMessage(mMainHandler.obtainMessage(
                            MSG_SAVE_COMPLETE, intent));
                } else {
                    mMainHandler.sendEmptyMessage(MSG_SHOW_SAVE_ERROR);
                }
            }
        });
    }

    private Bitmap getCroppedImage(Rect rect) {
        Utils.assertTrue(rect.width() > 0 && rect.height() > 0);

        Bundle extras = getIntent().getExtras();
        // (outputX, outputY) = the width and height of the returning bitmap.
        int outputX = rect.width();
        int outputY = rect.height();
        if (extras != null) {
            outputX = extras.getInt(KEY_OUTPUT_X, outputX);
            outputY = extras.getInt(KEY_OUTPUT_Y, outputY);
        }

        if (outputX * outputY > MAX_PIXEL_COUNT) {
            float scale = FloatMath.sqrt((float) MAX_PIXEL_COUNT / outputX / outputY);
            Log.w(TAG, "scale down the cropped image: " + scale);
            outputX = Math.round(scale * outputX);
            outputY = Math.round(scale * outputY);
        }

        // (rect.width() * scaleX, rect.height() * scaleY) =
        // the size of drawing area in output bitmap
        float scaleX = 1;
        float scaleY = 1;
        Rect dest = new Rect(0, 0, outputX, outputY);
        if (extras == null || extras.getBoolean(KEY_SCALE, true)) {
            scaleX = (float) outputX / rect.width();
            scaleY = (float) outputY / rect.height();
            if (extras == null || !extras.getBoolean(
                    KEY_SCALE_UP_IF_NEEDED, false)) {
                if (scaleX > 1f) scaleX = 1;
                if (scaleY > 1f) scaleY = 1;
            }
        }

        // Keep the content in the center (or crop the content)
        int rectWidth = Math.round(rect.width() * scaleX);
        int rectHeight = Math.round(rect.height() * scaleY);
        dest.set(Math.round((outputX - rectWidth) / 2f),
                Math.round((outputY - rectHeight) / 2f),
                Math.round((outputX + rectWidth) / 2f),
                Math.round((outputY + rectHeight) / 2f));

        if (mBitmapInIntent != null) {
            Bitmap source = mBitmapInIntent;
            Bitmap result = Bitmap.createBitmap(
                    outputX, outputY, Config.ARGB_8888);
            Canvas canvas = new Canvas(result);
            canvas.drawBitmap(source, rect, dest, null);
            return result;
        }

        if (mUseRegionDecoder) {
            int rotation = mMediaItem.getFullImageRotation();
            rotateRectangle(rect, mCropView.getImageWidth(),
                    mCropView.getImageHeight(), 360 - rotation);
            rotateRectangle(dest, outputX, outputY, 360 - rotation);

            BitmapFactory.Options options = new BitmapFactory.Options();
            int sample = BitmapUtils.computeSampleSizeLarger(
                    Math.max(scaleX, scaleY));
            options.inSampleSize = sample;

            // The decoding result is what we want if
            //   1. The size of the decoded bitmap match the destination's size
            //   2. The destination covers the whole output bitmap
            //   3. No rotation
            if ((rect.width() / sample) == dest.width()
                    && (rect.height() / sample) == dest.height()
                    && (outputX == dest.width()) && (outputY == dest.height())
                    && rotation == 0) {
                // To prevent concurrent access in GLThread
                synchronized (mRegionDecoder) {
                    return mRegionDecoder.decodeRegion(rect, options);
                }
            }
            Bitmap result = Bitmap.createBitmap(
                    outputX, outputY, Config.ARGB_8888);
            Canvas canvas = new Canvas(result);
            rotateCanvas(canvas, outputX, outputY, rotation);
            drawInTiles(canvas, mRegionDecoder, rect, dest, sample);
            return result;
        } else {
            int rotation = mMediaItem.getRotation();
            rotateRectangle(rect, mCropView.getImageWidth(),
                    mCropView.getImageHeight(), 360 - rotation);
            rotateRectangle(dest, outputX, outputY, 360 - rotation);
            Bitmap result = Bitmap.createBitmap(outputX, outputY, Config.ARGB_8888);
            Canvas canvas = new Canvas(result);
            rotateCanvas(canvas, outputX, outputY, rotation);
            canvas.drawBitmap(mBitmap,
                    rect, dest, new Paint(Paint.FILTER_BITMAP_FLAG));
            return result;
        }
    }

    private static void rotateCanvas(
            Canvas canvas, int width, int height, int rotation) {
        canvas.translate(width / 2, height / 2);
        canvas.rotate(rotation);
        if (((rotation / 90) & 0x01) == 0) {
            canvas.translate(-width / 2, -height / 2);
        } else {
            canvas.translate(-height / 2, -width / 2);
        }
    }

    private static void rotateRectangle(
            Rect rect, int width, int height, int rotation) {
        if (rotation == 0 || rotation == 360) return;

        int w = rect.width();
        int h = rect.height();
        switch (rotation) {
            case 90: {
                rect.top = rect.left;
                rect.left = height - rect.bottom;
                rect.right = rect.left + h;
                rect.bottom = rect.top + w;
                return;
            }
            case 180: {
                rect.left = width - rect.right;
                rect.top = height - rect.bottom;
                rect.right = rect.left + w;
                rect.bottom = rect.top + h;
                return;
            }
            case 270: {
                rect.left = rect.top;
                rect.top = width - rect.right;
                rect.right = rect.left + h;
                rect.bottom = rect.top + w;
                return;
            }
            default: throw new AssertionError();
        }
    }

    private void drawInTiles(Canvas canvas,
            BitmapRegionDecoder decoder, Rect rect, Rect dest, int sample) {
        int tileSize = TILE_SIZE * sample;
        Rect tileRect = new Rect();
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Config.ARGB_8888;
        options.inSampleSize = sample;
        canvas.translate(dest.left, dest.top);
        canvas.scale((float) sample * dest.width() / rect.width(),
                (float) sample * dest.height() / rect.height());
        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG);
        for (int tx = rect.left, x = 0;
                tx < rect.right; tx += tileSize, x += TILE_SIZE) {
            for (int ty = rect.top, y = 0;
                    ty < rect.bottom; ty += tileSize, y += TILE_SIZE) {
                tileRect.set(tx, ty, tx + tileSize, ty + tileSize);
                if (tileRect.intersect(rect)) {
                    Bitmap bitmap;

                    // To prevent concurrent access in GLThread
                    synchronized (decoder) {
                        bitmap = decoder.decodeRegion(tileRect, options);
                    }
                    canvas.drawBitmap(bitmap, x, y, paint);
                    bitmap.recycle();
                }
            }
        }
    }

    private void onBitmapRegionDecoderAvailable(
            BitmapRegionDecoder regionDecoder) {

        if (regionDecoder == null) {
            Toast.makeText(this, R.string.fail_to_load_image, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        mRegionDecoder = regionDecoder;
        mUseRegionDecoder = true;
        mState = STATE_LOADED;

        BitmapFactory.Options options = new BitmapFactory.Options();
        int width = regionDecoder.getWidth();
        int height = regionDecoder.getHeight();
        options.inSampleSize = BitmapUtils.computeSampleSize(width, height,
                BitmapUtils.UNCONSTRAINED, BACKUP_PIXEL_COUNT);
        mBitmap = regionDecoder.decodeRegion(
                new Rect(0, 0, width, height), options);

        mBitmapScreenNail = new BitmapScreenNail(mBitmap);

        TileImageViewAdapter adapter = new TileImageViewAdapter();
        adapter.setScreenNail(mBitmapScreenNail, width, height);
        adapter.setRegionDecoder(regionDecoder);

        mCropView.setDataModel(adapter, mMediaItem.getFullImageRotation());
        if (mDoFaceDetection) {
            mCropView.detectFaces(mBitmap);
        } else {
            mCropView.initializeHighlightRectangle();
        }
    }

    private void onBitmapAvailable(Bitmap bitmap) {
        if (bitmap == null) {
            Toast.makeText(this, R.string.fail_to_load_image, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        mUseRegionDecoder = false;
        mState = STATE_LOADED;

        mBitmap = bitmap;
        BitmapFactory.Options options = new BitmapFactory.Options();
        mCropView.setDataModel(new BitmapTileProvider(bitmap, 512),
                mMediaItem.getRotation());
        if (mDoFaceDetection) {
            mCropView.detectFaces(bitmap);
        } else {
            mCropView.initializeHighlightRectangle();
        }
    }

    private void setCropParameters() {
        Bundle extras = getIntent().getExtras();
        if (extras == null)
            return;
        int aspectX = extras.getInt(KEY_ASPECT_X, 0);
        int aspectY = extras.getInt(KEY_ASPECT_Y, 0);
        if (aspectX != 0 && aspectY != 0) {
            mCropView.setAspectRatio((float) aspectX / aspectY);
        }

        float spotlightX = extras.getFloat(KEY_SPOTLIGHT_X, 0);
        float spotlightY = extras.getFloat(KEY_SPOTLIGHT_Y, 0);
        if (spotlightX != 0 && spotlightY != 0) {
            mCropView.setSpotlightRatio(spotlightX, spotlightY);
        }
    }

    private void initializeData() {
        Bundle extras = getIntent().getExtras();

        if (extras != null) {
            if (extras.containsKey(KEY_NO_FACE_DETECTION)) {
                mDoFaceDetection = !extras.getBoolean(KEY_NO_FACE_DETECTION);
            }

            mBitmapInIntent = extras.getParcelable(KEY_DATA);

            if (mBitmapInIntent != null) {
                mBitmapTileProvider =
                        new BitmapTileProvider(mBitmapInIntent, MAX_BACKUP_IMAGE_SIZE);
                mCropView.setDataModel(mBitmapTileProvider, 0);
                if (mDoFaceDetection) {
                    mCropView.detectFaces(mBitmapInIntent);
                } else {
                    mCropView.initializeHighlightRectangle();
                }
                mState = STATE_LOADED;
                return;
            }
        }

        mProgressDialog = ProgressDialog.show(
                this, null, getString(R.string.loading_image), true, true);
        mProgressDialog.setCanceledOnTouchOutside(false);
        mProgressDialog.setCancelMessage(mMainHandler.obtainMessage(MSG_CANCEL_DIALOG));

        mMediaItem = getMediaItemFromIntentData();
        if (mMediaItem == null) return;

        boolean supportedByBitmapRegionDecoder =
            (mMediaItem.getSupportedOperations() & MediaItem.SUPPORT_FULL_IMAGE) != 0;
        if (supportedByBitmapRegionDecoder) {
            mLoadTask = getThreadPool().submit(new LoadDataTask(mMediaItem),
                    new FutureListener<BitmapRegionDecoder>() {
                @Override
                public void onFutureDone(Future<BitmapRegionDecoder> future) {
                    mLoadTask = null;
                    BitmapRegionDecoder decoder = future.get();
                    if (future.isCancelled()) {
                        if (decoder != null) decoder.recycle();
                        return;
                    }
                    mMainHandler.sendMessage(mMainHandler.obtainMessage(
                            MSG_LARGE_BITMAP, decoder));
                }
            });
        } else {
            mLoadBitmapTask = getThreadPool().submit(new LoadBitmapDataTask(mMediaItem),
                    new FutureListener<Bitmap>() {
                @Override
                public void onFutureDone(Future<Bitmap> future) {
                    mLoadBitmapTask = null;
                    Bitmap bitmap = future.get();
                    if (future.isCancelled()) {
                        if (bitmap != null) bitmap.recycle();
                        return;
                    }
                    mMainHandler.sendMessage(mMainHandler.obtainMessage(
                            MSG_BITMAP, bitmap));
                }
            });
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mState == STATE_INIT) initializeData();
        if (mState == STATE_SAVING) onSaveClicked();

        // TODO: consider to do it in GLView system
        GLRoot root = getGLRoot();
        root.lockRenderThread();
        try {
            mCropView.resume();
        } finally {
            root.unlockRenderThread();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        dismissProgressDialogIfShown();

        Future<BitmapRegionDecoder> loadTask = mLoadTask;
        if (loadTask != null && !loadTask.isDone()) {
            // load in progress, try to cancel it
            loadTask.cancel();
            loadTask.waitDone();
        }

        Future<Bitmap> loadBitmapTask = mLoadBitmapTask;
        if (loadBitmapTask != null && !loadBitmapTask.isDone()) {
            // load in progress, try to cancel it
            loadBitmapTask.cancel();
            loadBitmapTask.waitDone();
        }

        Future<Intent> saveTask = mSaveTask;
        if (saveTask != null && !saveTask.isDone()) {
            // save in progress, try to cancel it
            saveTask.cancel();
            saveTask.waitDone();
        }
        GLRoot root = getGLRoot();
        root.lockRenderThread();
        try {
            mCropView.pause();
        } finally {
            root.unlockRenderThread();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mBitmapScreenNail != null) {
            mBitmapScreenNail.recycle();
            mBitmapScreenNail = null;
        }
    }

    private void dismissProgressDialogIfShown() {
        if (mProgressDialog != null) {
            mProgressDialog.dismiss();
            mProgressDialog = null;
        }
    }

    private MediaItem getMediaItemFromIntentData() {
        Uri uri = getIntent().getData();
        DataManager manager = getDataManager();
        Path path = manager.findPathByUri(uri, getIntent().getType());
        if (path == null) {
            Log.w(TAG, "cannot get path for: " + uri + ", or no data given");
            return null;
        }
        return (MediaItem) manager.getMediaObject(path);
    }

    private class LoadDataTask implements Job<BitmapRegionDecoder> {
        MediaItem mItem;

        public LoadDataTask(MediaItem item) {
            mItem = item;
        }

        @Override
        public BitmapRegionDecoder run(JobContext jc) {
            return mItem == null ? null : mItem.requestLargeImage().run(jc);
        }
    }

    private class LoadBitmapDataTask implements Job<Bitmap> {
        MediaItem mItem;

        public LoadBitmapDataTask(MediaItem item) {
            mItem = item;
        }
        @Override
        public Bitmap run(JobContext jc) {
            return mItem == null
                    ? null
                    : mItem.requestImage(MediaItem.TYPE_THUMBNAIL).run(jc);
        }
    }
}
