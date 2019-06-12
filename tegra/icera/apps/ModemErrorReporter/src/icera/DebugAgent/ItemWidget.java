// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
package icera.DebugAgent;

import android.view.View;


public class ItemWidget<T extends View> {

    public T mView;
    private int mId;
    private String mName;
    boolean mIsChecked = false;
    private IClkListener mItemClk;
//    private Context mContext;
    
    public ItemWidget(int id, String name, boolean check){
        mId = id;
        mName = name;
        mIsChecked = check;

    }
    public boolean isChecked(){
        return mIsChecked;
    }
    public void setChecked(boolean check){
        mIsChecked = check;
    }
    public T getView(){
        return mView;
    }
    
    public int getId(){
        return mId;
    }
    public void setId(int id){
        mId = id;
    }
    public String getName(){
        return mName;
    }
    public void setName(String name){
        mName = name;
    }
    public void setClkListener(IClkListener itemClk){
        mItemClk = itemClk;
    }
    public void handleClkEvent(){
        if (mView != null && mItemClk != null){
            mView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mItemClk.handleClick(v);
                }
            });
        }
    }
    public void setView(View parent){
        mView = (T)parent.findViewById(mId);
    }

}