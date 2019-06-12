// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
package icera.DebugAgent;

import java.util.ArrayList;
import java.util.List;

import android.view.View;
import android.view.ViewGroup;

interface IClkListener{
    public void handleClick(View view);
}

public class TestItem {

    private String name;
    private boolean isChecked;
//    public ToggleButton btn = null;
//    public TextView txtV = null;
    private IClkListener mItemClk;
    public boolean isFocusable;
    int mId;
    public List<ItemWidget<?>> mItems = new ArrayList<ItemWidget<?>>();
    
    public TestItem(String nm, int id, boolean focus){
    //    mItems = new ArrayList<ItemWidget>();
        name = nm;
    //    isChecked = check;
        mId = id;
        isFocusable = focus;
    }
    public void addItem(ItemWidget<?> wd){
        mItems.add(wd);
    }
    public void setView(View parent){
        if (!isFocusable){
            ((ViewGroup)parent).setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);    
        }    
        for(ItemWidget<?> wd: mItems){
            wd.setView(parent);
        }
    }
    
    public String getName() {
        return name;
    }
    public void setName(String name) {
        this.name = name;
    }
    public boolean getChecked() {
        return isChecked;
    }
    public void setChecked(boolean check) {
        this.isChecked = check;
    }
    public void setClkListener(IClkListener itemClk){
        mItemClk = itemClk;
    }/*
    public void setBtnClkListener(IClkListener btnClk){
        mBtnClk = btnClk;
    }   */     
    public void handleClkEvent(View view){
        mItemClk.handleClick(view);
    }/*
    public void handleBtnClkEvent(View view){
        mBtnClk.handleClick(view);
    } */

    public void setId(int id){
        mId = id;
    }
    public int getId(){
        return mId;
    }
    public void setItemsEvent(){
        for(ItemWidget<?> wd: mItems){
            wd.handleClkEvent();
        }
    }
}
