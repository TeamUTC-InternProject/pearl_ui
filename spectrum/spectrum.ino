// Audio Spectrum Display
// Copyright 2013 Tony DiCola (tony@tonydicola.com)

// This code is part of the guide at http://learn.adafruit.com/fft-fun-with-fourier-transforms/

#define ARM_MATH_CM4
#include <arm_math.h>
#include <Adafruit_NeoPixel.h>

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// Create the Audio components.  These should be created in the
// order data flows, inputs/sources -> processing -> outputs

// Use these with the Teensy 3.5 & 3.6 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

AudioPlaySdRaw            playRaw1;          
AudioAnalyzeFFT1024       myFFT;
AudioAnalyzeNoteFrequency signalFreq;
AudioOutputAnalog         dac1; 

//Waveform used for testing  
AudioSynthWaveformSine sinewave;

//ADC in to FFT Functions
//AudioConnection patchCord1(adc1, 0, myFFT, 0);
AudioConnection patchCord2(playRaw1, dac1); 

////Uncomment to use for testing 
//AudioConnection patchCord2(sinewave, 0, myFFT, 0);
//AudioConnection patchCord2(sinewave, 0, dac1, 0); 
//AudioConnection patchCord4(sinewave, 0, signalFreq, 0);

////////////////////////////////////////////////////////////////////////////////
// CONIFIGURATION 
// These values can be changed to alter the behavior of the spectrum display.
////////////////////////////////////////////////////////////////////////////////

int CURRENT_MODE = 6;
int MODE = 6;
int NEXTMODE = 6;
int SAMPLE_RATE_HZ = 9000;             // Sample rate of the audio in hertz.
float SPECTRUM_MIN_DB = 30.0;          // Audio intensity (in decibels) that maps to low LED brightness.
float SPECTRUM_MAX_DB = 60.0;          // Audio intensity (in decibels) that maps to high LED brightness.
int LEDS_ENABLED = 1;                  // Control if the LED's should display the spectrum or not.  1 is true, 0 is false.
                                       // Useful for turning the LED display on and off with commands from the serial port.
const int FFT_SIZE = 256;              // Size of the FFT.  Realistically can only be at most 256 
                                       // without running out of memory for buffers and other state.
const int AUDIO_INPUT_PIN = 14;        // Input ADC pin for audio data.
const int ANALOG_READ_RESOLUTION = 10; // Bits of resolution for the ADC.
const int ANALOG_READ_AVERAGING = 16;  // Number of samples to average with each ADC reading.
const int POWER_LED_PIN = 13;          // Output pin for power LED (pin 13 to use Teensy 3.0's onboard LED).
const int NEO_PIXEL_PIN = 3;           // Output pin for neo pixels.
const int NEO_PIXEL_COUNT = 4;         // Number of neo pixels.  You should be able to increase this without
                                       // any other changes to the program.
const int MAX_CHARS = 65;              // Max size of the input command buffer

const int ACTIVE = 1;
const int PASSIVE = 2;

int state = PASSIVE;

int p_ledPin = 13;
int p_ledState = HIGH;

int a_ledPin = 12;
int a_ledState = LOW;

unsigned long previousMillis = 0;

const int ACTIVE_MODE_S1 = 1;
const int ACTIVE_MODE_S2 = 2;
const int ACTIVE_MODE_S3 = 3;
const int ACTIVE_MODE_S4 = 4;
const int ACTIVE_MODE_S5 = 5;
const int LISTEN_MODE = 6;


////////////////////////////////////////////////////////////////////////////////
// INTERNAL STATE
// These shouldn't be modified unless you know what you're doing.
////////////////////////////////////////////////////////////////////////////////

IntervalTimer samplingTimer;
float samples[FFT_SIZE*2];
float magnitudes[FFT_SIZE];
int sampleCounter = 0;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEO_PIXEL_COUNT, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);
char commandBuffer[MAX_CHARS];
float frequencyWindow[NEO_PIXEL_COUNT+1];
float hues[NEO_PIXEL_COUNT];


////////////////////////////////////////////////////////////////////////////////
// MAIN SKETCH FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

void setup() {

  Serial2.begin(9600);
  
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(30);

  //Begin the Center Frequency Determination Algorithm
  signalFreq.begin(.15);

  // Configure the window algorithm to use for the FFT
  myFFT.windowFunction(AudioWindowHanning1024);
  //myFFT.windowFunction(NULL);

    // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here if no SD card, but print a message
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  // Create a synthetic sine wave, for DAC testing
  // To use this, edit the connections above
  sinewave.amplitude(10);
  sinewave.frequency(200);
  
  // Set up serial port.
  Serial.begin(38400);
  
  // Set up ADC and audio input.
  pinMode(AUDIO_INPUT_PIN, INPUT);
  analogReadResolution(ANALOG_READ_RESOLUTION);
  analogReadAveraging(ANALOG_READ_AVERAGING);
  
  // Turn on the power indicator LED.
  pinMode(a_ledPin, OUTPUT);
  pinMode(p_ledPin, OUTPUT);
  //digitalWrite(POWER_LED_PIN, HIGH);
  
  // Initialize neo pixel library and turn off the LEDs
  pixels.begin();
  pixels.show(); 
  
  // Clear the input command buffer
  memset(commandBuffer, 0, sizeof(commandBuffer));
  
  // Initialize spectrum display
  spectrumSetup();
  
  // Begin sampling audio
  samplingBegin();
}

void loop() {
  // Calculate FFT if a full sample is available.
  if (samplingIsDone()) {
    // Run FFT on sample data.
    arm_cfft_radix4_instance_f32 fft_inst;
    arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);
    arm_cfft_radix4_f32(&fft_inst, samples);
    // Calculate magnitude of complex numbers output by the FFT.
    arm_cmplx_mag_f32(samples, magnitudes, FFT_SIZE);
  
    if (LEDS_ENABLED == 1)
    {
      spectrumLoop();
    }
  
    // Restart audio sampling.
    samplingBegin();
  }
    
  // Parse any pending commands.
  parserLoop();

  // check to see if it's time to change the state
  unsigned long currentMillis = millis();

  switch (CURRENT_MODE) {
  case 1:
    if ((state == PASSIVE) && (currentMillis - previousMillis >= 20000))
    {
      p_ledState = LOW;
      a_ledState = HIGH;
      state = ACTIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState);
      if (NEXTMODE != LISTEN_MODE)      
        playRaw1.play("wav1.raw");
      // update mode
      if (NEXTMODE != CURRENT_MODE) {
        CURRENT_MODE = NEXTMODE;
        Serial2.write(CURRENT_MODE);
      }
    }
    else if ((state == ACTIVE) && !playRaw1.isPlaying())
    {
      p_ledState = HIGH;
      a_ledState = LOW;
      state = PASSIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState); 
      playRaw1.stop(); 
      Serial2.write(LISTEN_MODE);
    }
    break;
  case 2:
    if ((state == PASSIVE) && (currentMillis - previousMillis >= 40000))
    {
      p_ledState = LOW;
      a_ledState = HIGH;
      state = ACTIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState);
      if (NEXTMODE != LISTEN_MODE)        
        playRaw1.play("wav2.raw");
      // update mode
      if (NEXTMODE != CURRENT_MODE) {
        CURRENT_MODE = NEXTMODE;
        Serial2.write(CURRENT_MODE);
      }
    }
    else if ((state == ACTIVE) && !playRaw1.isPlaying())
    {
      p_ledState = HIGH;
      a_ledState = LOW;
      state = PASSIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState); 
      playRaw1.stop(); 
      Serial2.write(LISTEN_MODE);
    }
    break;
  case 3:
    if ((state == PASSIVE) && (currentMillis - previousMillis >= 10000))
    {
      p_ledState = LOW;
      a_ledState = HIGH;
      state = ACTIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState);
      if (NEXTMODE != LISTEN_MODE)      
        playRaw1.play("wav3.raw");
      // update mode
      if (NEXTMODE != CURRENT_MODE) {
        CURRENT_MODE = NEXTMODE;
        Serial2.write(CURRENT_MODE);
      }
    }
    else if ((state == ACTIVE) && !playRaw1.isPlaying())
    {
      p_ledState = HIGH;
      a_ledState = LOW;
      state = PASSIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState); 
      playRaw1.stop(); 
      Serial2.write(LISTEN_MODE);
    }
    break;
  case 4:
    if ((state == PASSIVE) && (currentMillis - previousMillis >= 80000))
    {
      p_ledState = LOW;
      a_ledState = HIGH;
      state = ACTIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState);
      if (NEXTMODE != LISTEN_MODE)
        playRaw1.play("wav4.raw");
      // update mode
      if (NEXTMODE != CURRENT_MODE) {
        CURRENT_MODE = NEXTMODE;
        Serial2.write(CURRENT_MODE);
      }
    }
    else if ((state == ACTIVE) && !playRaw1.isPlaying())
    {
      p_ledState = HIGH;
      a_ledState = LOW;
      state = PASSIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState);  
      playRaw1.stop();
      Serial2.write(LISTEN_MODE);
    }
    break;
  case 5:
    if ((state == PASSIVE) && (currentMillis - previousMillis >= 20000))
    {
      p_ledState = LOW;
      a_ledState = HIGH;
      state = ACTIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState);
      if (NEXTMODE != LISTEN_MODE)
        playRaw1.play("wav5.raw");
      // update mode
      if (NEXTMODE != CURRENT_MODE) {
        CURRENT_MODE = NEXTMODE;
        Serial2.write(CURRENT_MODE);
      }
    }
    else if ((state == ACTIVE) && !playRaw1.isPlaying())
    {
      p_ledState = HIGH;
      a_ledState = LOW;
      state = PASSIVE;
      previousMillis = currentMillis;
      digitalWrite(p_ledPin, p_ledState);
      digitalWrite(a_ledPin, a_ledState);  
      playRaw1.stop();
      Serial2.write(LISTEN_MODE);
    }
    break;
  case 6:
    p_ledState = HIGH;
    a_ledState = LOW;
    state = PASSIVE;
    digitalWrite(p_ledPin, p_ledState);
    digitalWrite(a_ledPin, a_ledState);  
    // update mode
    if (NEXTMODE != CURRENT_MODE) {
      CURRENT_MODE = NEXTMODE;
      Serial2.write(CURRENT_MODE);
    }
    break;
  default:
    break;
  }
  
}


////////////////////////////////////////////////////////////////////////////////
// UTILITY FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

// Compute the average magnitude of a target frequency window vs. all other frequencies.
void windowMean(float* magnitudes, int lowBin, int highBin, float* windowMean, float* otherMean) {
    *windowMean = 0;
    *otherMean = 0;
    // Notice the first magnitude bin is skipped because it represents the
    // average power of the signal.
    for (int i = 1; i < FFT_SIZE/2; ++i) {
      if (i >= lowBin && i <= highBin) {
        *windowMean += magnitudes[i];
      }
      else {
        *otherMean += magnitudes[i];
      }
    }
    *windowMean /= (highBin - lowBin) + 1;
    *otherMean /= (FFT_SIZE / 2 - (highBin - lowBin));
}

// Convert a frequency to the appropriate FFT bin it will fall within.
int frequencyToBin(float frequency) {
  float binFrequency = float(SAMPLE_RATE_HZ) / float(FFT_SIZE);
  return int(frequency / binFrequency);
}

// Convert from HSV values (in floating point 0 to 1.0) to RGB colors usable
// by neo pixel functions.
uint32_t pixelHSVtoRGBColor(float hue, float saturation, float value) {
  // Implemented from algorithm at http://en.wikipedia.org/wiki/HSL_and_HSV#From_HSV
  float chroma = value * saturation;
  float h1 = float(hue)/60.0;
  float x = chroma*(1.0-fabs(fmod(h1, 2.0)-1.0));
  float r = 0;
  float g = 0;
  float b = 0;
  if (h1 < 1.0) {
    r = chroma;
    g = x;
  }
  else if (h1 < 2.0) {
    r = x;
    g = chroma;
  }
  else if (h1 < 3.0) {
    g = chroma;
    b = x;
  }
  else if (h1 < 4.0) {
    g = x;
    b = chroma;
  }
  else if (h1 < 5.0) {
    r = x;
    b = chroma;
  }
  else // h1 <= 6.0
  {
    r = chroma;
    b = x;
  }
  float m = value - chroma;
  r += m;
  g += m;
  b += m;
  return pixels.Color(int(255*r), int(255*g), int(255*b));
}


////////////////////////////////////////////////////////////////////////////////
// SPECTRUM DISPLAY FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

void spectrumSetup() {
  // Set the frequency window values by evenly dividing the possible frequency
  // spectrum across the number of neo pixels.
  float windowSize = (SAMPLE_RATE_HZ / 2.0) / float(NEO_PIXEL_COUNT);
  for (int i = 0; i < NEO_PIXEL_COUNT+1; ++i) {
    frequencyWindow[i] = i*windowSize;
  }
  // Evenly spread hues across all pixels.
  for (int i = 0; i < NEO_PIXEL_COUNT; ++i) {
    hues[i] = 360.0*(float(i)/float(NEO_PIXEL_COUNT-1));
  }
}

void spectrumLoop() {
  // Update each LED based on the intensity of the audio 
  // in the associated frequency window.
  float intensity, otherMean;
  for (int i = 0; i < NEO_PIXEL_COUNT; ++i) {
    windowMean(magnitudes, 
               frequencyToBin(frequencyWindow[i]),
               frequencyToBin(frequencyWindow[i+1]),
               &intensity,
               &otherMean);
    // Convert intensity to decibels.
    intensity = 20.0*log10(intensity);
    // Scale the intensity and clamp between 0 and 1.0.
    intensity -= SPECTRUM_MIN_DB;
    intensity = intensity < 0.0 ? 0.0 : intensity;
    intensity /= (SPECTRUM_MAX_DB-SPECTRUM_MIN_DB);
    intensity = intensity > 1.0 ? 1.0 : intensity;
    pixels.setPixelColor(i, pixelHSVtoRGBColor(hues[i], 1.0, intensity));
  }
  pixels.show();
}


////////////////////////////////////////////////////////////////////////////////
// SAMPLING FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

void samplingCallback() {
  // Read from the ADC and store the sample data
  samples[sampleCounter] = (float32_t)analogRead(AUDIO_INPUT_PIN);
  // Complex FFT functions require a coefficient for the imaginary part of the input.
  // Since we only have real data, set this coefficient to zero.
  samples[sampleCounter+1] = 0.0;
  // Update sample buffer position and stop after the buffer is filled
  sampleCounter += 2;
  if (sampleCounter >= FFT_SIZE*2) {
    samplingTimer.end();
  }
}

void samplingBegin() {
  // Reset sample buffer position and start callback at necessary rate.
  sampleCounter = 0;
  samplingTimer.begin(samplingCallback, 1000000/SAMPLE_RATE_HZ);
}

boolean samplingIsDone() {
  return sampleCounter >= FFT_SIZE*2;
}


////////////////////////////////////////////////////////////////////////////////
// COMMAND PARSING FUNCTIONS
// These functions allow parsing simple commands input on the serial port.
// Commands allow reading and writing variables that control the device.
//
// All commands must end with a semicolon character.
// 
// Example commands are:
// GET SAMPLE_RATE_HZ;
// - Get the sample rate of the device.
// SET SAMPLE_RATE_HZ 400;
// - Set the sample rate of the device to 400 hertz.
// 
////////////////////////////////////////////////////////////////////////////////

void parserLoop() {
  // Process any incoming characters from the serial port
  while (Serial.available() > 0) {
    char c = Serial.read();
    // Add any characters that aren't the end of a command (semicolon) to the input buffer.
    if (c != ';') {
      c = toupper(c);
      strncat(commandBuffer, &c, 1);
    }
    else
    {
      // Parse the command because an end of command token was encountered.
      parseCommand(commandBuffer);
      // Clear the input buffer
      memset(commandBuffer, 0, sizeof(commandBuffer));
    }
  }
}

// Macro used in parseCommand function to simplify parsing get and set commands for a variable
#define GET_AND_SET(variableName) \
  else if (strcmp(command, "GET " #variableName) == 0) { \
    Serial.println(variableName); \
  } \
  else if (strstr(command, "SET " #variableName " ") != NULL) { \
    variableName = (typeof(variableName)) atof(command+(sizeof("SET " #variableName " ")-1)); \
  }

void parseCommand(char* command) {
  if (strcmp(command, "GET MODE") == 0) {
    Serial.println(MODE);    
  }
  else if (strcmp(command, "GET MAGNITUDES") == 0) {
    for (int i = 0; i < FFT_SIZE; ++i) {
      Serial.println(magnitudes[i]);
    }
  }
  else if (strcmp(command, "GET SAMPLES") == 0) {
    for (int i = 0; i < FFT_SIZE*2; i+=2) {
      Serial.println(samples[i]);
    }
  }
  else if (strcmp(command, "GET FFT_SIZE") == 0) {
    Serial.println(FFT_SIZE);
  }
  else if (strcmp(command, "GET CURRENT_MODE") == 0){
    Serial.println(CURRENT_MODE);
  }
  GET_AND_SET(CURRENT_MODE)
  GET_AND_SET(MODE)
  GET_AND_SET(SAMPLE_RATE_HZ)
  GET_AND_SET(LEDS_ENABLED)
  GET_AND_SET(SPECTRUM_MIN_DB)
  GET_AND_SET(SPECTRUM_MAX_DB)
  
  // Update spectrum display values if sample rate was changed.
  if (strstr(command, "SET SAMPLE_RATE_HZ ") != NULL) {
    spectrumSetup();
  }
  else if (strstr(command, "SET MODE ") != NULL) {
    NEXTMODE = MODE;
  }
  
  // Turn off the LEDs if the state changed.
  if (LEDS_ENABLED == 0) {
    for (int i = 0; i < NEO_PIXEL_COUNT; ++i) {
      pixels.setPixelColor(i, 0);
    }
    pixels.show();
  }
}
//Used to determine the frequency of the signal being read on the ADC
void signalAnalysis(){ 
    // read back fundamental frequency
    if (signalFreq.available()) {
        float note = signalFreq.read();
        float prob = signalFreq.probability();
        Serial.printf("Note: %3.2f | Probability: %.2f\n", note, prob);
    }   
}

//Play a waveform on the SD Card 
void startPlaying() {
  Serial.println("startPing");
  playRaw1.play("WAVEFORM_1.RAW");
}

//Stop playing a waveform on the SD Card 
void stopPlaying() {
  Serial.println("stopPing");
  playRaw1.stop();
}

