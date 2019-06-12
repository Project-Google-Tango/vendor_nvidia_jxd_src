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

package com.android.gallery3d.ui;

import android.os.Bundle;
import android.widget.Toast;

import com.android.gallery3d.R;
import com.android.gallery3d.app.AbstractGalleryActivity;
import com.android.gallery3d.app.AlbumPage;
import com.android.gallery3d.util.MediaSetUtils;

public class ImportCompleteListener extends WakeLockHoldingProgressListener {
    private static final String WAKE_LOCK_LABEL = "Gallery Album Import";

    public ImportCompleteListener(AbstractGalleryActivity galleryActivity) {
        super(galleryActivity, WAKE_LOCK_LABEL);
    }

    @Override
    public void onProgressComplete(int result) {
        super.onProgressComplete(result);
        int message;
        if (result == MenuExecutor.EXECUTION_RESULT_SUCCESS) {
            message = R.string.import_complete;
            goToImportedAlbum();
        } else {
            message = R.string.import_fail;
        }
        Toast.makeText(getActivity().getAndroidContext(), message, Toast.LENGTH_LONG).show();
    }

    private void goToImportedAlbum() {
        String pathOfImportedAlbum = "/local/all/" + MediaSetUtils.IMPORTED_BUCKET_ID;
        Bundle data = new Bundle();
        data.putString(AlbumPage.KEY_MEDIA_PATH, pathOfImportedAlbum);
        getActivity().getStateManager().startState(AlbumPage.class, data);
    }
}
