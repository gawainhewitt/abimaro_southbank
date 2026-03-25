#include <Arduino.h>

#define WavSerial Serial1

// Button pins - each adjacent to a GND pin
const int BUTTON_PINS[] = {2, 6, 10, 13, 17, 21};
const int NUM_BUTTONS = 6;

// Track mapping
const int TRACK_MAP[] = {1, 2, 3, 4, 5, 6};

// Polyphony settings
const int MAX_VOICES = 3;
int activeVoices[MAX_VOICES] = {0, 0, 0};  // Track which tracks are playing
int voiceIndex = 0;  // Round-robin index for voice stealing

// Debounce variables
bool lastButtonState[NUM_BUTTONS];
bool debouncedButtonState[NUM_BUTTONS];
unsigned long lastDebounceTime[NUM_BUTTONS];
const unsigned long DEBOUNCE_DELAY = 50;

// Attract mode (idle playback)
const unsigned long IDLE_TIMEOUT = 15UL * 60UL * 1000UL;  // 15 minutes in milliseconds
const unsigned long TRACK_GAP = 5000;  // 5 seconds between tracks
unsigned long lastButtonPressTime = 0;
bool attractModeActive = false;
bool waitingForTrackEnd = false;
int currentAttractTrack = -1;
unsigned long trackEndTime = 0;

// Track pool for attract mode
int trackPool[NUM_BUTTONS];
int tracksRemaining = 0;

// Function declarations
void triggerTrack(int trackNum);
void stopTrack(int trackNum);
void setMasterVolume(int gain);
bool isTrackPlaying(int trackNum);
void removeTrackFromVoices(int trackNum);
void handleAttractMode();
void exitAttractMode();
void refillTrackPool();
int drawRandomTrack();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Wav Trigger with 6 buttons starting...");
  Serial.println("3-voice polyphony with voice stealing");
  Serial.println("Attract mode after 15 minutes idle");
  
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
  setMasterVolume(-20);
  delay(100);
  
  // Initialize random seed
  randomSeed(analogRead(26));  // Use floating ADC pin for randomness
  
  lastButtonPressTime = millis();
  
  Serial.println("Ready! Press buttons...");
}

void loop() {
  bool buttonPressed = false;
  
  // Check all buttons
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
          buttonPressed = true;
          lastButtonPressTime = millis();
          
          // Exit attract mode if active
          if (attractModeActive) {
            exitAttractMode();
          }
          
          int trackNum = TRACK_MAP[i];
          
          Serial.print("Button ");
          Serial.print(i + 1);
          Serial.print(" pressed - track ");
          Serial.print(trackNum);
          
          // Check if this track is already playing
          if (isTrackPlaying(trackNum)) {
            Serial.println(" already playing, retriggering");
            // Stop the existing instance
            stopTrack(trackNum);
            // Remove it from voice tracking
            removeTrackFromVoices(trackNum);
          } else {
            Serial.println();
          }
          
          // Now find a slot for the new track
          // Check if we need to steal a voice
          if (activeVoices[voiceIndex] != 0) {
            Serial.print("  Stealing voice ");
            Serial.print(voiceIndex + 1);
            Serial.print(", stopping track ");
            Serial.println(activeVoices[voiceIndex]);
            stopTrack(activeVoices[voiceIndex]);
          }
          
          // Assign this track to the current voice slot
          activeVoices[voiceIndex] = trackNum;
          triggerTrack(trackNum);
          
          // Move to next voice slot (round-robin)
          voiceIndex = (voiceIndex + 1) % MAX_VOICES;
        }
      }
    }
  }
  
  // Handle attract mode
  handleAttractMode();
}

void refillTrackPool() {
  // Fill pool with all track indices
  for (int i = 0; i < NUM_BUTTONS; i++) {
    trackPool[i] = i;
  }
  tracksRemaining = NUM_BUTTONS;
  Serial.println("Attract mode: refilled track pool");
}

int drawRandomTrack() {
  // If pool is empty, exit attract mode and restart timer
  if (tracksRemaining == 0) {
    Serial.println("Attract mode: all tracks played, exiting");
    attractModeActive = false;
    waitingForTrackEnd = false;
    lastButtonPressTime = millis();  // Restart 15 minute timer
    return -1;  // Signal to not play anything
  }
  
  // Pick a random index from remaining tracks
  int randomIdx = random(tracksRemaining);
  int trackIndex = trackPool[randomIdx];
  
  // Remove this track from pool by swapping with last element
  trackPool[randomIdx] = trackPool[tracksRemaining - 1];
  tracksRemaining--;
  
  return TRACK_MAP[trackIndex];
}

void handleAttractMode() {
  unsigned long idleTime = millis() - lastButtonPressTime;
  
  // Check if we should enter attract mode
  if (!attractModeActive && idleTime >= IDLE_TIMEOUT) {
    Serial.println("=== ENTERING ATTRACT MODE ===");
    attractModeActive = true;
    waitingForTrackEnd = false;
    currentAttractTrack = -1;
    refillTrackPool();
  }
  
  // If in attract mode, play tracks sequentially
  if (attractModeActive) {
    // If waiting for track to end, check if time is up
    if (waitingForTrackEnd) {
      if (millis() >= trackEndTime) {
        waitingForTrackEnd = false;
        Serial.println("Attract mode: track finished");
      }
    }
    
    // If not waiting, start a new track
    if (!waitingForTrackEnd) {
      // Pick a random track from pool
      int trackNum = drawRandomTrack();
      
      // If -1, attract mode has ended
      if (trackNum == -1) {
        return;
      }
      
      Serial.print("Attract mode: playing track ");
      Serial.print(trackNum);
      Serial.print(" (");
      Serial.print(tracksRemaining);
      Serial.println(" remaining in pool)");
      
      // Clear all voices and play this track solo
      for (int i = 0; i < MAX_VOICES; i++) {
        if (activeVoices[i] != 0) {
          stopTrack(activeVoices[i]);
          activeVoices[i] = 0;
        }
      }
      
      // Play the track
      activeVoices[0] = trackNum;
      triggerTrack(trackNum);
      currentAttractTrack = trackNum;
      waitingForTrackEnd = true;
      
      // Set end time (5 second gap)
      trackEndTime = millis() + TRACK_GAP;
    }
  }
}

void exitAttractMode() {
  Serial.println("=== EXITING ATTRACT MODE ===");
  attractModeActive = false;
  waitingForTrackEnd = false;
  
  // Stop current attract track
  if (currentAttractTrack != -1) {
    stopTrack(currentAttractTrack);
    removeTrackFromVoices(currentAttractTrack);
    currentAttractTrack = -1;
  }
  
  // Clear all voices
  for (int i = 0; i < MAX_VOICES; i++) {
    activeVoices[i] = 0;
  }
}

bool isTrackPlaying(int trackNum) {
  for (int i = 0; i < MAX_VOICES; i++) {
    if (activeVoices[i] == trackNum) {
      return true;
    }
  }
  return false;
}

void removeTrackFromVoices(int trackNum) {
  for (int i = 0; i < MAX_VOICES; i++) {
    if (activeVoices[i] == trackNum) {
      activeVoices[i] = 0;  // Clear this slot
      return;
    }
  }
}

void triggerTrack(int trackNum) {
  // Correct WAV Trigger format: SOM1, SOM2, LENGTH, CMD, PLAY_CODE, TRACK_LOW, TRACK_HIGH, EOM
  WavSerial.write(0xF0);           // SOM1
  WavSerial.write(0xAA);           // SOM2
  WavSerial.write(0x08);           // Message length (8 bytes total)
  WavSerial.write(0x03);           // Command: CONTROL_TRACK
  WavSerial.write(0x01);           // Play code: 0x01 = PLAY_POLY
  WavSerial.write((byte)trackNum); // Track number low byte
  WavSerial.write((byte)(trackNum >> 8)); // Track number high byte
  WavSerial.write(0x55);           // EOM
}

void stopTrack(int trackNum) {
  // Stop track command: same format but with STOP code (0x04)
  WavSerial.write(0xF0);           // SOM1
  WavSerial.write(0xAA);           // SOM2
  WavSerial.write(0x08);           // Message length (8 bytes total)
  WavSerial.write(0x03);           // Command: CONTROL_TRACK
  WavSerial.write(0x04);           // Play code: 0x04 = STOP
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