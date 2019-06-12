package nvidia.android.didim.calibration;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.Toast;

public class DidimCalibration extends Activity {
	
	public static final int INVISIBLE = 4;
	public static final int VISIBLE = 0;
	
	// UI components that need to be accessed in code
	private EditText textField;
	private RadioGroup radioGroup;
	private Button fullscreenButton;
	private Button nextCalibrationButton;
	private View mainView;
	
	// Amount to increment while calibrating
	private int increment = 10;
	
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        // Assign field variables based on the main.xml id's that are assigned
        textField = (EditText)findViewById(R.id.editText1);
        radioGroup = (RadioGroup)findViewById(R.id.radioGroup1);
        fullscreenButton = (Button)findViewById(R.id.button2);
        mainView = findViewById(R.id.layout1);
        nextCalibrationButton = (Button)findViewById(R.id.button1);
    }
    
    public void enterFullScreenMode(View view)
    {
    	radioGroup.setVisibility(INVISIBLE);
    	textField.setVisibility(INVISIBLE);
    	fullscreenButton.setVisibility(INVISIBLE);
    	nextCalibrationButton.setText("");
    }
    
    public void changeBackgroundBacklight(View view)
    {
    	if(textField.getText().toString().length() == 0)
    	{
    		// Text fields weren't correctly filled in
    		Toast.makeText(this, "Please enter valid information", Toast.LENGTH_LONG).show();
    		return;
    	}
    	
    	// Grab the value currently in the text field
    	int value = Integer.parseInt(textField.getText().toString());
    	
    	// Determine which radio button is selected
    	int radioButton = radioGroup.getCheckedRadioButtonId();
    	
    	int currColor;
    	
    	// Assign variable for each radio button in the radio group
    	RadioButton lightButton = (RadioButton)findViewById(R.id.radio0);
    	RadioButton redButton = (RadioButton)findViewById(R.id.radio1);
    	RadioButton greenButton = (RadioButton)findViewById(R.id.radio2);
    	RadioButton blueButton = (RadioButton)findViewById(R.id.radio3);
    	RadioButton greyButton = (RadioButton)findViewById(R.id.radio4);
    	
    	// Set the text for each radio button: switch statement should change each of these values anyway
    	redButton.setText("Red: 0");
		greenButton.setText("Green: 0");
		blueButton.setText("Blue: 0");
		greyButton.setText("Grey: n/a");
		lightButton.setText("Backlight: 128");
    	
    	switch(radioButton)
    	{
    	// Light intensity button
    	case R.id.radio0:
    		
    		// Start by clearing the text field
    		textField.getText().clear();
    		
    		if(value <= 0)
    		{
    			// Don't want to shut off the display; that would suck
        		Toast.makeText(this, "Backlight can't be set to 0: please enter a number [1,255]", Toast.LENGTH_LONG).show();
        		return;
    		}
    		else if(value >= 255)
    		{
    			// Don't want to assign light intensity to greater than 255
    			// So if value >= 255, set light intensity to 255 and select
    			// the next radio button
    			textField.getText().append('0');
    			lightButton.setChecked(false);
    			greyButton.setChecked(true);
    			modifyBrightness(255);
    			currColor = Color.rgb(128, 128, 128);
    			redButton.setText("Red: 128");
    			greenButton.setText("Green: 128");
    			blueButton.setText("Blue: 128");
    			lightButton.setText("Backlight: 255");
    			break;
    		}
    		else
    		{
    			// Normal increment by 10, except since lowest value is 1 for light intensity,
    			// after value is 1, add increment but subtract 1.
    			if(value == 1)
    			{
    				textField.getText().append(String.valueOf(value + increment - 1));
    				lightButton.setText("Backlight: " + (value-1));
    			}
    			else
    			{
    				textField.getText().append(String.valueOf(value + increment));
    				lightButton.setText("Backlight: " + value);
    			}
    		}
    		
    		// Assign all values and text fields
    		redButton.setText("Red: 128");
			greenButton.setText("Green: 128");
			blueButton.setText("Blue: 128");
    		currColor = Color.rgb(128, 128, 128);
    		modifyBrightness(value);
    		break;
    	// Grey button - BL is 255 and R,G,B are all the same value
    	case R.id.radio4:
    		textField.getText().clear();
    		if(value >= 255)
    		{
    			// Handle case where we reached the max value of red
    			currColor = Color.rgb(255, 255, 255);
    			textField.getText().append('0');
    			greyButton.setChecked(false);
    			redButton.setChecked(true);
    			redButton.setText("Red: 255");
    			greenButton.setText("Green: 255");
    			blueButton.setText("Blue: 255");
    			lightButton.setText("Backlight: 255");
    			break;
    		}
    		else
    		{
    			// Update red text field normally
    			textField.getText().append(String.valueOf(value + increment));
    			greyButton.setText("Grey: " + value);
    			redButton.setText("Red: n/a");
    			greenButton.setText("Green: n/a");
    			blueButton.setText("Blue: n/a");
    			lightButton.setText("Backlight: 255");
    		}
    		
    		// Make sure brightness and color are set correctly
    		modifyBrightness(255);
    		currColor = Color.rgb(value, value, value);
    		break;
    	// Red button - keep G/B as 0 and light intensity as 128
    	case R.id.radio1:
    		textField.getText().clear();
    		if(value >= 255)
    		{
    			// Handle case where we reached the max value of red
    			currColor = Color.rgb(255, 0, 0);
    			textField.getText().append('0');
    			redButton.setChecked(false);
    			greenButton.setChecked(true);
    			redButton.setText("Red: 255");
    			greenButton.setText("Green: 0");
    			blueButton.setText("Blue: 0");
    			lightButton.setText("Backlight: 128");
    			break;
    		}
    		else
    		{
    			// Update red text field normally
    			textField.getText().append(String.valueOf(value + increment));
    			redButton.setText("Red: " + value);
    		}
    		
    		// Make sure brightness and color are set correctly
    		modifyBrightness(128);
    		currColor = Color.rgb(value, 0, 0);
    		break;
    	// Green button - keep R/B as 0 and light intensity as 128
    	case R.id.radio2:
    		textField.getText().clear();
    		if(value >= 255)
    		{
    			// Handle case where we reached the max value of green
    			currColor = Color.rgb(0, 255, 0);
    			textField.getText().append('0');
    			greenButton.setChecked(false);
    			blueButton.setChecked(true);
    			redButton.setText("Red: 0");
    			greenButton.setText("Green: 255");
    			blueButton.setText("Blue: 0");
    			lightButton.setText("Backlight: 128");
    			break;
    		}
    		else
    		{
    			// Update green text field normally
    			textField.getText().append(String.valueOf(value + increment));
    			greenButton.setText("Green: " + value);
    		}
    		
    		// Make sure brightness and color are set correctly
    		modifyBrightness(128);
    		currColor = Color.rgb(0, value, 0);
    		break;
    	// Blue button - keep R/G as 0 and light intensity as 128
    	case R.id.radio3:
    		textField.getText().clear();
    		if(value >= 255)
    		{
    			// Handle case where we reached the max value of blue
    			currColor = Color.rgb(0, 0, 255);
    			textField.getText().append(String.valueOf(1));
    			blueButton.setChecked(false);
    			lightButton.setChecked(true);
    			redButton.setText("Red: 0");
    			greenButton.setText("Green: 0");
    			blueButton.setText("Blue: 255");
    			lightButton.setText("Backlight: 128");
    			break;
    		}
    		else
    		{
    			// Update blue text field normally
    			textField.getText().append(String.valueOf(value + increment));
    			blueButton.setText("Blue: " + value);
    		}
    		
    		// Make sure brightness and color are set correctly
    		modifyBrightness(128);
    		currColor = Color.rgb(0, 0, value);
    		break;
    	// Can't imagine how you'd get in here...
    	default:
    		modifyBrightness(128);
    		currColor = Color.rgb(128, 128, 128);
    		break;
    	}
    	
    	if(currColor == Color.rgb(128, 128, 128) && value == 10)
    	{
    		radioGroup.setVisibility(VISIBLE);
        	textField.setVisibility(VISIBLE);
        	fullscreenButton.setVisibility(VISIBLE);
        	nextCalibrationButton.setText("x\n\n\n\n\n\n\n\n\n\n\n\n\n\nx\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tx\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tx\n\n\n\n\n\n\n\n\n\n\n\n\n\nx");
    	}
    	
    	// Set button's background color
    	view.setBackgroundColor(currColor);
    	mainView.setBackgroundColor(currColor);
    }
    
    public void modifyBrightness(int val)
    {  	
    	// Assign the screen brightness to a value between 0.0 and 1.0
    	WindowManager.LayoutParams lp = getWindow().getAttributes();
    	lp.screenBrightness = val / 255.0f;
    	getWindow().setAttributes(lp);
    }
}