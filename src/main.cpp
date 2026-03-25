#include <Arduino.h>

#define WavSerial Serial1

// Button pins - each adjacent to a GND pin
const int BUTTON_PINS[] = {2, 6, 10, 13, 17, 21};
const int NUM_BUTTONS = 6;

// Track mapping
const int TRACK_MAP[] = {1, 2, 3, 4, 5, 6};

// Debounce variables
bool lastButtonState[NUM_BUTTONS];
bool debouncedButtonState[NUM_BUTTONS];
unsigned long lastDebounceTime[NUM_BUTTONS];
const unsigned long DEBOUNCE_DELAY = 50;

// Function declarations
void triggerTrack(int trackNum);
void setMasterVolume(int gain);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Wav Trigger with 6 buttons starting...");
  
  // Setup buttons with internal pull-ups
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    lastButtonState[i] = HIGH;
    debouncedButtonState[i] = HIGH;
    lastDebounceTime[i] = 0;
    Serial.print("Setup button ");
    Serial.print(i + 1);
    Serial.print(" on GP");
    Serial.println(BUTTON_PINS[i]);
  }
  
  Serial.println("Starting Wav Trigger serial...");
  WavSerial.begin(57600);
  delay(500);
  
  Serial.println("Setting volume...");
  setMasterVolume(-10);
  delay(100);
  
  Serial.println("Ready! Press buttons...");
}

void loop() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    int reading = digitalRead(BUTTON_PINS[i]);
    
    // If reading changed, reset timer
    if (reading != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
      lastButtonState[i] = reading;
    }
    
    // If stable for long enough, update debounced state
    if ((millis() - lastDebounceTime[i]) > DEBOUNCE_DELAY) {
      // Check if debounced state changed
      if (reading != debouncedButtonState[i]) {
        debouncedButtonState[i] = reading;
        
        // Trigger on press
        if (debouncedButtonState[i] == LOW) {
          Serial.print("Button ");
          Serial.print(i + 1);
          Serial.print(" pressed - triggering track ");
          Serial.println(TRACK_MAP[i]);
          
          triggerTrack(TRACK_MAP[i]);
        }
      }
    }
  }
}

void triggerTrack(int trackNum) {
  // Correct WAV Trigger format: SOM1, SOM2, LENGTH, CMD, PLAY_CODE, TRACK_LOW, TRACK_HIGH, EOM
  WavSerial.write(0xF0);           // SOM1
  WavSerial.write(0xAA);           // SOM2
  WavSerial.write(0x08);           // Message length (8 bytes total)
  WavSerial.write(0x03);           // Command: CONTROL_TRACK
  WavSerial.write(0x01);           // Play code: 0x01 = PLAY_POLY (or 0x00 for PLAY_SOLO)
  WavSerial.write((byte)trackNum); // Track number low byte
  WavSerial.write((byte)(trackNum >> 8)); // Track number high byte
  WavSerial.write(0x55);           // EOM
}

void setMasterVolume(int gain) {
  // gain range: -70dB to +10dB
  // Sent as signed 16-bit integer, little-endian
  WavSerial.write(0xF0);           // SOM1
  WavSerial.write(0xAA);           // SOM2
  WavSerial.write(0x07);           // Message length (7 bytes)
  WavSerial.write(0x05);           // Command: MASTER_VOLUME
  WavSerial.write((byte)gain);     // Gain low byte
  WavSerial.write((byte)(gain >> 8)); // Gain high byte
  WavSerial.write(0x55);           // EOM
}