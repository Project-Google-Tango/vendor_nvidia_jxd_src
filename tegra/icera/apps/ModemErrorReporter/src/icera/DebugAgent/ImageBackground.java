// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
package icera.DebugAgent;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.Display;
import android.widget.ImageView;

public class ImageBackground{
    private ImageView mView;
    private Activity mAct;
    public static Bitmap btpPhone, btpTablet;
    public ImageBackground(ImageView view, Activity act){
        mView = view;
        mAct = act;    
    }
    public int calculateRate(BitmapFactory.Options options, int reqWidth, int reqHeight) {
        
        int height = options.outHeight;
        int width = options.outWidth;
        int rate = 1;

        if (height > reqHeight || width > reqWidth) {

            int hRate = Math.round((float)height / (float)reqHeight);
            int wRate = Math.round((float)width / (float)reqWidth);
            rate = hRate > wRate ? hRate : wRate;
        }
//        Log.d(TAG, "bitmap's width is: " + width + "\nbitmap's height is: " + height);
        return rate;
    }
    

    public Bitmap decodeBitmap(Resources res, int resId, int reqWidth, int reqHeight) {

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeResource(res, resId, options);

        options.inSampleSize = calculateRate(options, reqWidth, reqHeight);

        options.inJustDecodeBounds = false;
        return BitmapFactory.decodeResource(res, resId, options);
    }

    public void showBackground(){
        Display display = mAct.getWindowManager().getDefaultDisplay();
        int width = display.getWidth();
        int height = display.getHeight();

        if (width < height) {
            if (btpPhone == null){
                btpPhone = decodeBitmap(mAct.getResources(), R.drawable.phone, width, height);
            }
            if (btpPhone != null)
            mView.setImageBitmap(btpPhone);
        }
        else {
            if (btpTablet == null){
                btpTablet = decodeBitmap(mAct.getResources(), R.drawable.tablet, width, height);
            }
            if (btpTablet != null)
            mView.setImageBitmap(btpTablet);
        }
    }
}