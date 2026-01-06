#include <TM1637Display.h>

/*
 * SPOT WELDER CONTROLLER (Arduino Nano)
 * Controls 800W Transformer via Triac with Phase Angle Control
 */

// --- Hardware Pin Definitions ---
const int PIN_ZERO_CROSS = 2;  // Must be Interrupt Pin (INT0)
const int PIN_TRIAC      = 3;  // Trigger pin
const int PIN_FOOT_SW    = 4;  // Foot switch
const int PIN_CLK        = 5;  // Display CLK
const int PIN_DIO        = 6;  // Display DIO
const int PIN_POT_TIME   = A0; // Time adjustment
const int PIN_POT_POWER  = A1; // Power (Phase) adjustment

// --- Settings ---
const int MIN_TIME_MS = 10;    // Minimum weld time (1 cycle approx 20ms)
const int MAX_TIME_MS = 500;   // Maximum weld time
const int DEBOUNCE_MS = 500;   // Foot switch debounce

// --- Objects ---
TM1637Display display(PIN_CLK, PIN_DIO);

// --- Global Variables ---
volatile bool zeroCrossDetected = false;
volatile int firingDelay = 0;       // Delay in microseconds before firing Triac
volatile bool weldingActive = false; // Is welding currently happening?

int setTimeVal = 0;   // Time setting (0-100) -> maps to ms
int setPowerVal = 0;  // Power setting (0-100%)
unsigned long lastWeldTime = 0;

void setup() {
  pinMode(PIN_TRIAC, OUTPUT);
  digitalWrite(PIN_TRIAC, LOW);
  
  pinMode(PIN_FOOT_SW, INPUT_PULLUP);
  pinMode(PIN_ZERO_CROSS, INPUT);

  // Setup Interrupt for Zero Crossing
  attachInterrupt(digitalPinToInterrupt(PIN_ZERO_CROSS), zeroCrossISR, RISING);

  display.setBrightness(0x0f); // Max brightness
  
  // Show "Hi" on startup
  uint8_t data[] = { 
    SEG_B | SEG_C | SEG_E | SEG_F | SEG_G, // H
    SEG_E | SEG_F,                         // I
    0, 0 
  };
  display.setSegments(data);
  delay(1000);
}

// Interrupt Service Routine: Runs every time AC wave crosses zero
void zeroCrossISR() {
  if (weldingActive) {
    // Phase Control Logic:
    // 50Hz AC = 10ms half cycle (10,000us)
    // 100% Power = Fire immediately (0us delay)
    // 10% Power = Fire late (e.g. 9000us delay)
    
    // Using delayMicroseconds inside ISR is generally not recommended for complex tasks,
    // but for simple Phase Control with dedicated MCU, it is the most precise method without timers.
    
    if (firingDelay > 0) {
      delayMicroseconds(firingDelay); 
    }
    
    // Fire Triac
    digitalWrite(PIN_TRIAC, HIGH);
    delayMicroseconds(50); // Short pulse to trigger
    digitalWrite(PIN_TRIAC, LOW);
  }
}

void loop() {
  readPotentiometers();
  updateDisplay();

  // Check Trigger
  if (digitalRead(PIN_FOOT_SW) == LOW) {
    if (millis() - lastWeldTime > DEBOUNCE_MS) {
      performWeld();
      lastWeldTime = millis();
    }
  }
}

void readPotentiometers() {
  // Read Time (Map 0-1023 to 1-50 pulses approx)
  // Let's display 01-99 for user friendly interface
  int rawTime = analogRead(PIN_POT_TIME);
  setTimeVal = map(rawTime, 0, 1023, 1, 50); // 1 = 20ms (1 cycle), 50 = 1000ms
  
  // Read Power (Map 0-1023 to 0-100%)
  int rawPower = analogRead(PIN_POT_POWER);
  setPowerVal = map(rawPower, 0, 1023, 10, 100); // 10% to 100%
  
  // Calculate Firing Delay for ISR
  // 50Hz AC half wave is 10000us. 
  // We leave some margin: 0us (100%) to 8500us (Low power)
  // Inverse relationship: High power = Low delay
  firingDelay = map(setPowerVal, 100, 10, 100, 8500); 
}

void updateDisplay() {
  // Display format: Time on left (2 digits), Power on right (2 digits)
  // Example: "10.99" -> Time level 10, Power 99%
  int dispNum = (setTimeVal * 100) + setPowerVal;
  display.showNumberDecEx(dispNum, 0b01000000, true); // Turn on colon
}

void performWeld() {
  // Calculate total duration in milliseconds
  // Assuming 1 unit of setTimeVal is roughly 20ms (1 AC cycle @ 50Hz)
  unsigned long weldDuration = setTimeVal * 20;
  
  weldingActive = true;
  delay(weldDuration); // Use simple delay while ISR handles the phase chopping
  weldingActive = false;
  
  // Safety: Ensure Triac is off
  digitalWrite(PIN_TRIAC, LOW);
}