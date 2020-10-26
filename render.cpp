// ECS7012P Music and Audio Programming
// School of Electronic Engineering and Computer Science
// ECS7012 Music and Audio Programming
// Queen Mary University of London
// Spring 2020
// assignment 3 : Formant Filter
// Student name : Selim Sheta
// Student ID : 190295033

#include <Bela.h>
#include <bpfilter.h>
#include <cmath>
#include <libraries/Gui/Gui.h>
#include <libraries/GuiController/GuiController.h>
#include <libraries/Scope/Scope.h>

 // Oscilloscope 
Scope gScope;

// browser-based GUI to adjust parameters
Gui gui;
GuiController controller;

const int gSampleBufferLength = 512;		// The length of the buffer in frames
float gSampleBuffer[gSampleBufferLength];	// Buffer that holds the wavetable
float gReadPointer = 0;						// Position of the last frame we played 
float gAmplitude = 0.9;						// Amplitude of the saw wave
float gFrequency = 220.0;					// Frequency of the saw wave
float gDryMix = 0.1;						// Amount of original signal in the output
float kOutputGain = 20.0;					// Gain 

bpfilter bandpassFilter1 = bpfilter::bpfilter(); // Bandpass filter for first formant Frequency
bpfilter bandpassFilter2 = bpfilter::bpfilter(); // Bandpass filter for second formant Frequency
bpfilter bandpassFilter3 = bpfilter::bpfilter(); // Bandpass filter for third formant Frequency
bpfilter bandpassFilter4 = bpfilter::bpfilter(); // Bandpass filter for fourth formant Frequency
float gFilterCenterFrequency[4] = {600, 1040, 2250, 2450};	// Frequencies of the filters
float gFilterQ[4] = {20, 30, 40, 40};						// Q factors of the filters
float gFilterGain[4] = {0, -7, -9, -9};						// Output gains of the filters
float gThroatSize = 1.0;										// Throat size

int kButtonPin = 7; 	// Digital Pin for the button
int kVowelTypePin = 0;	// Analogue Pin for the potentiomer controlling the vowel type
int kThroatSizePin = 1; // Analogue Pin for the potentiomer controlling the throat size

// Arrays containing the specifications for each vowel (A,E,I,O,U)
float kvowelSpecA[3][4] = {{600, 1040, 2250, 2450},{20,30,41,41},{0,-7,-9,-9}}; 
float kvowelSpecE[3][4] = {{400, 1620, 2400, 2800},{20,40,48,47},{0,-12,-9,-12}};
float kvowelSpecI[3][4] = {{250, 1750, 2600, 3050},{8,39,52,51},{0,-30,-16,-22}};
float kvowelSpecO[3][4] = {{400, 750, 2400, 2600},{20,19,48,43},{0,-11,-21,-20}};
float kvowelSpecU[3][4] = {{350, 600, 2400, 2675},{17,15,48,45},{0,-20,-32,-28}};

// States for the different vowel types
enum {
	kState1 = 0,
	kState2,
	kState3,
	kState4,
	kState5,
};

// Current vowel type
int gState = kState1;

// Updates the filter parameters depending on the current state (ie selected vowel)
void updateFilterSpecs(){
	if (gState == kState1){ // "A"
		for (int i = 0; i <4; i++){
		gFilterCenterFrequency[i] = kvowelSpecA[0][i];
		gFilterQ[i] = gThroatSize*kvowelSpecA[1][i];
		gFilterGain[i] = kvowelSpecA[2][i];
		}
	}
	else if (gState == kState2){ // "E"
		for (int i = 0; i <4; i++){
		gFilterCenterFrequency[i] = kvowelSpecE[0][i];
		gFilterQ[i] = gThroatSize*kvowelSpecE[1][i];
		gFilterGain[i] = kvowelSpecE[2][i];
		}
	}
	else if (gState == kState3){ // "I"
		for (int i = 0; i <4; i++){
		gFilterCenterFrequency[i] = kvowelSpecI[0][i];
		gFilterQ[i] = gThroatSize*kvowelSpecI[1][i];
		gFilterGain[i] = kvowelSpecI[2][i];
		}
	}
	else if (gState == kState4){ // "O"
		for (int i = 0; i <4; i++){
		gFilterCenterFrequency[i] = kvowelSpecO[0][i];
		gFilterQ[i] = gThroatSize*kvowelSpecO[1][i];
		gFilterGain[i] = kvowelSpecO[2][i];
		}
	}
	else if (gState == kState5){ // "U"
		for (int i = 0; i <4; i++){
		gFilterCenterFrequency[i] = kvowelSpecU[0][i];
		gFilterQ[i] = gThroatSize*kvowelSpecU[1][i];
		gFilterGain[i] = kvowelSpecU[2][i];
		}
	}
}

// Repurposed from assignment 1
// Written by Andrew Mcpherson
// Read a sample from the wavetable and increase the read pointer
float wavetable_read(float sampleRate, float frequency)
{
	// The pointer will take a fractional index. Look for the sample on
	// either side which are indices we can actually read into the buffer.
	// If we get to the end of the buffer, wrap around to 0.
	int indexBelow = floorf(gReadPointer);
	int indexAbove = indexBelow + 1;
	if(indexAbove >= gSampleBufferLength)
		indexAbove = 0;
	
	// For linear interpolation, we need to decide how much to weigh each
	// sample. The closer the fractional part of the index is to 0, the
	// more weight we give to the "below" sample. The closer the fractional
	// part is to 1, the more weight we give to the "above" sample.
	float fractionAbove = gReadPointer - indexBelow;
	float fractionBelow = 1.0 - fractionAbove;
	
	// Calculate the weighted average of the "below" and "above" samples
    float out = (fractionBelow * gSampleBuffer[indexBelow] + fractionAbove * gSampleBuffer[indexAbove]);

    // Increment read pointer and reset to 0 when end of table is reached
    gReadPointer += gSampleBufferLength * frequency / sampleRate;
    while(gReadPointer >= gSampleBufferLength)
        gReadPointer -= gSampleBufferLength;
        
    return out;
}

bool setup(BelaContext *context, void *userData)
{
	// Initialise GPIO pins
	pinMode(context, 0, kButtonPin, INPUT);
	
	// Generate a sawtooth waveform (a ramp from -1 to 1) and store it in the buffer.
	for(unsigned int n = 0; n < gSampleBufferLength; n++) {
		gSampleBuffer[n] = -1.0 + 2.0 * (float)n / (float)(gSampleBufferLength - 1);
	}
	
	// Set up the GUI
	 gui.setup(context->projectName);
	 controller.setup(&gui, "Sawtooth Controller");
	
	 controller.addSlider("Frequency (Hz)", 220, 110, 880, 10);	// Generator Frequency slider
	 controller.addSlider("Amplitude (dB)", -10.5, -60, -0.5, 0.5);	// Generator Amplitude slider
	 controller.addSlider("Dry Mix (dB)", -60, -60, -0.5, 0.5);		// Dry mix slider
	 
	 // To control the filter without potentiomers (via the gui) uncomment the following:
	 // controller.addSlider("Vowel (type)", 0, 0, 5, 1); 			// Vowel selection slider
	 // controller.addSlider("Throat Size (size)", 0, -15, 20, 0.1);	// Throat size slider
	 
	 // Set up Oscilloscope
	 gScope.setup(1, context->audioSampleRate);
	 
	return true;
}

void render(BelaContext *context, void *userData)
{
	gFrequency = controller.getSliderValue(0); // Frequency of sawtooth wave is first slider
	gAmplitude = pow(10,controller.getSliderValue(1)/20); // Amplitude of sawtooth wave is second slider
	gDryMix = pow(10,controller.getSliderValue(2)/20);    // Amount of original signal to mix back into the output
	gState = int(map(analogRead(context, 0, kVowelTypePin),0,0.825,0,6)); // Getting the vowel type
	gThroatSize = pow(10,map(analogRead(context, 0, kThroatSizePin),0,0.825,-15,20)/20); // Getting the throat size
	
	// To control the filter without potentiomers (via the gui) replace the last two lines by the following:
	// gState = int(controller.getSliderValue(3)); // Vowel type is third slider
	// gThroatSize = pow(10,controller.getSliderValue(4)/20); // Throat size is fourth slider
	
	// Update filter specifications depending on gState
	updateFilterSpecs();
	
	// Set the filters' parameters
	bandpassFilter1.setParameters(context->audioSampleRate, gFilterCenterFrequency[0], gFilterQ[0], gFilterGain[0]);
	bandpassFilter2.setParameters(context->audioSampleRate, gFilterCenterFrequency[1], gFilterQ[1], gFilterGain[1]);
	bandpassFilter3.setParameters(context->audioSampleRate, gFilterCenterFrequency[2], gFilterQ[2], gFilterGain[2]);
	bandpassFilter4.setParameters(context->audioSampleRate, gFilterCenterFrequency[3], gFilterQ[3], gFilterGain[3]);
	
	// Reading the value of the button
	int currentButtonValue = digitalRead(context, 0, kButtonPin);
	
    for(unsigned int n = 0; n < context->audioFrames; n++) {
    	float out = 0;
    	// Load a sample from the buffer
    	float sample = gAmplitude * wavetable_read(context->audioSampleRate, gFrequency);
    	
    	// Filter the sample
    	float xbp1 = bandpassFilter1.process(sample);
    	float xbp2 = bandpassFilter2.process(sample);
    	float xbp3 = bandpassFilter3.process(sample);
    	float xbp4 = bandpassFilter4.process(sample);
    	
    	// If button is pressed, write the processed sample to output
    	if (currentButtonValue == 0) {
    		out = kOutputGain*(xbp1 + xbp2 + xbp3 + xbp4 + gDryMix*sample)/5;
    	}
        
    	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			// Write the output to every audio output channel
    		audioWrite(context, n, channel, out);
    	}
    	
    	// Pass signal to Oscilloscope
    	gScope.log(out); 
    }
}

void cleanup(BelaContext *context, void *userData)
{

}
