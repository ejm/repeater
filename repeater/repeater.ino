/*

The MIT License (MIT)
=====================

Copyright (c) 2017 Evan Markowitz (techkid6/KD2IZW) for 721MCB (WC2FD)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// ID callout
unsigned long lastId;
bool armed = false;
bool transmit = false;
const long idDelay = 600000; // 10 minutes -> ms

// Timeout
bool useTimeout;
unsigned long pttStartTime;
const long timeout = 45000; // 45 seconds -> ms

// DRA818 IO pins
const int pttPin = 25;
const int squelchPin = 23;
const int pttRxPin = 26; // This pin is always high, but if we ever wanted to control it later we could
const int pdTxPin = 24; // This pin is always high, but if we ever wanted to control it later we could
const int pdRxPin = 22; // This pin is always high, but if we ever wanted to control it later we could

// Shield ID pins
const int shieldResetPin = -1;
const int shieldCSPin = 7;
const int shieldDCSPin = 6;
const int cardCSPin = 4;
const int dReqPin = 3;

// Audio tracks (names follow the 8.3 filename format)
const char PROGMEM track_id[] = "callsign.mp3";
const char PROGMEM track_courtesy[] = "courtesy.mp3";

Adafruit_VS1053_FilePlayer audioPlayer = Adafruit_VS1053_FilePlayer(shieldResetPin, shieldCSPin, shieldDCSPin, dReqPin, cardCSPin);

void setup() {
  // Set up ID callout
  lastId = millis();

  // Set up timeout
  useTimeout = false; // Set to TRUE for use as a regular repeater, in emergency settings, keeping it FALSE is advised
    
  // Set up Serial connections
  Serial.begin(9600);  // Debug output
  Serial1.begin(9600); // TX (UHF) (The DRA818 uses a 9600baud)
  Serial2.begin(9600); // RX (VHF)

  // Set up repeater frequencies
  Serial.println("Setting frequencies...");
  Serial1.println("AT+DMOSETGROUP=0,446.6000,446.6000,0000,4,0000"); // Set TX frequency
  Serial2.println("AT+DMOSETGROUP=0,145.5200,145.5200,0000,4,0000"); // Set RX frequency
  Serial2.println("AT+DMOSETVOLUME=2"); // Set RX volume
  Serial.println("Set.");
    
  // Set up IO pins
  pinMode(pttPin, OUTPUT);
  pinMode(pttRxPin, OUTPUT);
  pinMode(pdRxPin, OUTPUT);
  pinMode(pdTxPin, OUTPUT);
  pinMode(squelchPin, INPUT);

  // Silently turn off the transmitter
  digitalWrite(pttPin, HIGH);

  // Silently turn the radios on
  digitalWrite(pttRxPin, HIGH);
  digitalWrite(pdTxPin, HIGH);
  digitalWrite(pdRxPin, HIGH);
  
  // Set up audio player
  if (!audioPlayer.begin()){
    Serial.println("Audio player not found!");
    while (true);
  }

  if (!SD.begin(cardCSPin)){
    Serial.println("SD card not found!");
    while (true);  
  }
  
  audioPlayer.setVolume(20, 20); // Lower is LOUDER
}

void loop() {
  // Check first for if the ID should be called
  if (armed){
    if (tryId()){
      Serial.println("Automated 10 minute timer.");
      armed = false;
    }
  }

  // Serial.println(digitalRead(squelchPin)); // DEBUG MODE: When you inevitably need to make sure the pins are working properly
  // Run the squelch detection code (Remember, squelch is high by default)
  squelchDetect(!digitalRead(squelchPin));
}

void playId(){
  // Start transmitting if need be and play the station ID
  Serial.println("Station Identification Playing...");
  playAudio(track_id);
  Serial.println("Station Identification Done.");
  lastId = millis();
}

void playAudio(const char* track){
  // Temporary variable to reflect the initial state of the transmitter
  bool tx = transmit;
  // Start the transmitter if it isn't keyed up already
  if (!tx){
    transmit = true;
    startPtt();
  }

  // Play the actual file...
  audioPlayer.playFullFile(track);

  // If we started the transmitter, kill it
  if (!tx){
    endPtt();
    transmit = false;
  }
}

void playCourtesy(){
  // Put courtesy tone code here
  Serial.println("Courtesy Tone Playing...");
  playAudio(track_courtesy);
  Serial.println("Courtesy Tone Done.");
}

bool tryId(){
  // Has the right amount of time passed between when we played the tone last and now?
  if ((millis() - lastId) > idDelay){
    playId();
    return true;
  }
  return false;
}

void startPtt(){
  // Set the PTT pin to HIGH, triggering the transmitter
  digitalWrite(pttPin, LOW);
  pttStartTime = millis();
  Serial.println("Starting Transmit...");  
}

void endPtt(){
  // Set the PTT pin to HIGH, cutting off the transmitter
  digitalWrite(pttPin, HIGH);
  Serial.println("Ending Transmit.");  
}

void squelchDetect(bool squelch){
  if (squelch){
    // Rising edge detection
    if (!transmit){
      startTransmit();
    }
    else {
      // If we're already transmitting, see if we've been transmitting too long
      if (useTimeout){
        if ((millis() - pttStartTime) > timeout){
          Serial.println("Timed out.");
          endTransmit();
        }
      }
    }
  }
  else {
    // Falling edge detection
    if (transmit){
      endTransmit();
    }
  }
}

void startTransmit(){
  // Not to be confused with startPtt(), this deals with the squelch loop
  transmit = true;
  startPtt();
  // Play the ID if this is the first transmission of a new conversation
  if (!armed) {
    armed = true;
    playId();
  }
  // Check if we need to play the ID again mid conversation
  else{
    tryId();
  }
}


void endTransmit(){
  // Not to be confused with endPtt(), this deals with the squelch loop
  playCourtesy();
  endPtt();
  transmit = false;
}

