// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
package icera.DebugAgent;
import java.util.ArrayList;
import java.util.List;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.ToggleButton;

public abstract class ListMenu extends Activity{
    protected Prefs mPrefs;
    protected ListView lv;
    protected List<TestItem> items = new ArrayList<TestItem>();
    protected Context mContext;
    protected MainListAdapter adapter;
    
    private int mLayoutId = R.layout.logcat_logging1;
    private int mTestItemLayoutId = R.layout.item_logcat;
    private int mListViewId = R.id.listView2;
    private int mTestItemId = R.id.testItem2;
    private int mTestItemButtonId = R.id.testItemButton2;

    private int mHeightOfItem = 55;
    
    static final String TAG = "ListMenu";
    protected String[] mStr;
    protected boolean[] mBl;
    protected List<IClkListener> togguleListener;
    
    
    public void onCreate(Bundle savedInstanceState) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        super.onCreate(savedInstanceState);
        setContentView(getLayoutId());
        mPrefs = new Prefs(this);
        new ImageBackground((ImageView)findViewById(R.id.imageView1), this).showBackground();
        mContext = getApplicationContext();
        lv = (ListView)findViewById(getListViewId());
        initWidget();
        adapter = new MainListAdapter(mContext, items);
        lv.setAdapter(adapter);
        setListViewHeightBasedOnChildren(lv);  
        setLvFocus(false);
    }
    
    public abstract void setWidgetData();
    
    public void initWidget()
    {
        setWidgetData();
        if (mStr.length == 0 || mStr.length != mBl.length || mStr.length != togguleListener.size())
            return;

        ItemWidget<?>[] wd1, wd2;
        wd1 = new ItemWidget<?>[mStr.length];
        wd2 = new ItemWidget<?>[mBl.length];

         TestItem itm;
        for (int i = 0; i < mStr.length; i++){
            wd1[i] = new ItemWidget<TextView>(getTestItemId(), mStr[i], false){
                @Override
                public void setView(View parent){
                    //super.getView(parent);
                    this.mView = (TextView)parent.findViewById(this.getId());
                    this.mView.setText(this.getName());
                }
            };
            wd2[i] = new ItemWidget<ToggleButton>(getTestItemButtonId(), null, mBl[i]){
                @Override
                public void setView(View parent){
                    //super.getView(parent);
                    this.mView = (ToggleButton)parent.findViewById(this.getId());
                    this.mView.setChecked(this.isChecked());
                    }
                };
            wd2[i].setClkListener(togguleListener.get(i));

            itm = new TestItem(mStr[i], getTestItemLayoutId(), false);
            itm.addItem(wd1[i]);
            itm.addItem(wd2[i]);
            items.add(itm);
        }
    }

    
    public void setLayoutId(int id){
        mLayoutId = id;
    }
    public int getLayoutId(){
        return mLayoutId;
    }
    public void setListViewId(int id){
        mListViewId = id;
    }
    public int getListViewId(){
        return mListViewId;
    }
    
    public void setHeightOfItem(int height){
        mHeightOfItem = height;
    }
    public int getHeightOfItem(){
        return mHeightOfItem;
    }
    public void setTestItemId(int id){
        mTestItemId = id;
    }
    public int getTestItemId(){
        return mTestItemId;
    }
    
    public void setTestItemButtonId(int height){
        mTestItemButtonId = height;
    }
    public int getTestItemButtonId(){
        return mTestItemButtonId;
    }
    
    public void setTestItemLayoutId(int height){
        mTestItemLayoutId = height;
    }
    public int getTestItemLayoutId(){
        return mTestItemLayoutId;
    }
        
    public void setLvFocus(boolean focus){
        lv.setFocusable(focus);
    }
    
    public void setListViewHeightBasedOnChildren(ListView listView) {
          ViewGroup.LayoutParams params = listView.getLayoutParams();
          DisplayMetrics dm = getResources().getDisplayMetrics();
          params.height = (int) ((dm.density * mHeightOfItem + listView.getDividerHeight()) * listView.getAdapter().getCount()) + mHeightOfItem;          
          listView.setLayoutParams(params);
    }
}
