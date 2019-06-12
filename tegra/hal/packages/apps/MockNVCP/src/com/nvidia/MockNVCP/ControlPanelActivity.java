package com.nvidia.MockNVCP;

import java.util.Arrays;
import java.util.Vector;

import android.graphics.PixelFormat;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceCategory;
import android.preference.ListPreference;
import android.preference.PreferenceActivity;
import android.view.Display;
import android.util.Log;
import com.nvidia.display.MutableDisplay;
import com.nvidia.display.DisplayMode;

public class ControlPanelActivity extends PreferenceActivity {
    static final String TAG = "ControlPanelActivity";
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.layout.main);
        // populate list preference for all connected displays
        MutableDisplay displays[] = MutableDisplay.getAll(true);
        for (int i = 0; i<displays.length; ++i) {
            ListPreference list = (ListPreference)findPreference("mode_"+i);
            setupModeListPreference(list, displays[i]);
        }
        // mark unconnected displays as such
        for (int i = displays.length; i<2; ++i) { 
            ListPreference list = (ListPreference)findPreference("mode_"+i);
            list.setTitle(R.string.not_connected);
        }
        // set up other settings
        findPreference("match_resolution_to_display").setOnPreferenceChangeListener(new MatchResolutionListener());
        findPreference("play_audio_via_hdmi").setOnPreferenceChangeListener(new PlayAudioListener());
    }

    static void setupModeListPreference(ListPreference pref, MutableDisplay display) {
        // generate modes strings
        int index = 0; // index of current mode
        Vector<String> modeStrings = new Vector<String>();
        DisplayMode current = display.getDisplayMode();
        DisplayMode supported[] = display.getSupportedDisplayModes();
        for (DisplayMode m: supported) { // loop through supported modes...
            if (
                current.width==m.width && 
                current.height==m.height && 
                // current.pixelFormat==m.pixelFormat && // TODO: dispmgr returns 0, should be fixed
                current.refreshRate==m.refreshRate 
            )
                index = modeStrings.size();
            modeStrings.add(getModeDescription(m));
        }
        // fill list preference with mode strings
        pref.setEntries(modeStrings.toArray(new CharSequence[0]));
        pref.setEntryValues(modeStrings.toArray(new CharSequence[0]));
        pref.setValueIndex(index);
        pref.setTitle(modeStrings.elementAt(index));
        if (modeStrings.size()==1)
            pref.setSummary(R.string.mode_summary_one);
        else { // enable the listbox
            pref.setSummary(R.string.mode_summary_many);
            pref.setEnabled(true);
        }
        // add listener to list preference
        pref.setOnPreferenceChangeListener(new ModeChangeListener(display));
    }
    
    private static int getBitsPerPixel(int pixelFormatIndex) {
        PixelFormat pixelFormat = new PixelFormat();
        PixelFormat.getPixelFormatInfo(pixelFormatIndex, pixelFormat);
        return pixelFormat.bitsPerPixel;
    }
    
    private static String getModeDescription(DisplayMode mode) {
        String result = mode.width + "x" + mode.height;
        result += ", " + ((int)(mode.refreshRate+0.5)) + " Hz";
        result += ", " + (getBitsPerPixel(mode.pixelFormat)<20? "thousands": "millions") + " of colors";
        return result;
    }
    
    private static class ModeChangeListener implements Preference.OnPreferenceChangeListener {
        ModeChangeListener(MutableDisplay display) {
            mDisplay = display;
        }
        
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            ListPreference list = (ListPreference)preference;
            int selected = list.findIndexOfValue(newValue.toString());   
            if (selected != -1) { // something was selected
                DisplayMode supported[] = mDisplay.getSupportedDisplayModes();
                if (supported.length==list.getEntries().length) // the list hasn't changed
                    mDisplay.setDisplayMode(supported[selected]); // set the display mode
                setupModeListPreference(list, mDisplay); // update list preference
            }   
            // let the preference setting proceed
            return true;
        }
        
        private MutableDisplay mDisplay;
    }
    
    private static class MatchResolutionListener implements Preference.OnPreferenceChangeListener {
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            // don't know how to change the desktop resolution until boot time
            return true;
        }
    }
    
    private static class PlayAudioListener implements Preference.OnPreferenceChangeListener {
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            MutableDisplay.setRouteAudioToHdmi(newValue.toString()!="false");
            return true;
        }
    }
}
