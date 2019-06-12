// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
package icera.DebugAgent;

import java.util.ArrayList;
import java.util.List;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;


class MainListAdapter extends BaseAdapter{
    private Context mContext;
    List<Boolean> mChecked;
    List<TestItem> listItem;
    int item_height = 0;

    
    public MainListAdapter(Context context, List<TestItem> list){
        listItem = new ArrayList<TestItem>();
        listItem = list;
        mContext = context;
        mChecked = new ArrayList<Boolean>();
        for(int i = 0; i < list.size(); i++){
            mChecked.add(list.get(i).getChecked());
        }
    }

    @Override
    public int getCount() {
        return listItem.size();
    }

    @Override
    public Object getItem(int position) {
        return listItem.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        TestItem cur = listItem.get(position);

        if (convertView == null) {
            LayoutInflater mInflater = (LayoutInflater) mContext
                    .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            convertView = mInflater.inflate(cur.getId(), null);
            cur.setView(convertView);
            cur.setItemsEvent();
        }else{
            cur.setView(convertView);
            
        }
        return convertView;
    }
}