#include <U8g2lib.h>
#include <Wire.h>
#include <Arduino.h>
#include <avr/pgmspace.h>

// Pins Definition
const byte keyPin = 2;
const byte ledPin = A1;
const byte buzzerPin = 3;
const byte batteryPin = A7;

// Timing definitions
const byte dotThreshold = 50;
const byte dashThreshold = 150;
const byte debounceDelay = 50;
const int letterSpace = 600;
const int wordSpace = 2000;
const unsigned long idleTimeout = 10000; // 10 seconds idle time |  used toflush buffers after that!

char morseCode[10] = "";
byte morseIndex = 0;
unsigned long pressStart = 0;
unsigned long lastReleaseTime = 0;
bool keyWasPressed = false;
bool keyIsPressed = false;
bool newWord = false;
char decodedChars[5] = {' ', ' ', ' ', ' ', ' '}; // holds last 5 decoded chars
int decodedIndex = 0;

// WPM Calculation holders
unsigned long totalDotDuration = 0;
byte dotCount = 0;
int wpm = 0;

// display related timmings
const int screenUpdateDelay = 1000;
const int bootScreenDelay = 3000;

// Battery Voltage Calculation
const float R1 = 120000.0;
const float R2 = 330000.0;
const float refVoltage = 1.1;
const float minVoltage = 3.0; 
const float maxVoltage = 4.2; 
float batteryVoltage = 0.0;
int batteryPercentage = 0;

// morse code <> chars holder
const char charTable[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
const char morseTable[][6] PROGMEM = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--", "--..",
  ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----.", "-----"
};

// system / welcome screen messages 
const char* sysMessages[] PROGMEM = {"--| MDNT |--", "CW Trainer v1", "waiting for cw signal..."};

// U8G2 I2C instance setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);



void setup() {

  pinMode(keyPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(batteryPin, INPUT);

  pinMode(9, OUTPUT);
  digitalWrite(9, 0);
  u8g2.begin();

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.setCursor(20, 20);
  u8g2.print(sysMessages[0]);

  u8g2.setCursor(20, 40);
  u8g2.print(sysMessages[1]);

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(5, 60);
  u8g2.print(sysMessages[2]);

  u8g2.sendBuffer();

  delay(bootScreenDelay);
}


void loop() {

  int keyState = digitalRead(keyPin);
  unsigned long currentTime = millis();
  static unsigned long lastBatteryUpdate = 0; // here for being reseted on every loop

  // update full display (including battery info) every 1 second when idle.
  if (currentTime - lastBatteryUpdate >= screenUpdateDelay && !keyIsPressed) {
    fullDisplayUpdate();
    lastBatteryUpdate = currentTime;
  }
  
  // clearing decoder char in idle timeout
  if (currentTime - lastReleaseTime >= idleTimeout) {
    clearDecodedChars();
  }
  
  if (keyState == LOW) {
    
    if (!keyWasPressed) {
      pressStart = currentTime;
      keyWasPressed = true;
      keyIsPressed = true;
      delay(debounceDelay);
    }

    digitalWrite(ledPin, HIGH);

  } else {

    if (keyIsPressed) {
      unsigned long pressDuration = currentTime - pressStart;
      
      if (pressDuration < dashThreshold) { // Dot detected
        
        if (morseIndex < sizeof(morseCode) - 1) {
          morseCode[morseIndex++] = '.';
          morseCode[morseIndex] = '\0';
        }

        totalDotDuration += pressDuration;
        dotCount++;
        
        if (dotCount > 0) {
          wpm = 1200 / (totalDotDuration / dotCount);
        }

      } else { // Dash detected

        if (morseIndex < sizeof(morseCode) - 1) {
          morseCode[morseIndex++] = '-';
          morseCode[morseIndex] = '\0';
        }

      }
      keyIsPressed = false;
      keyWasPressed = false;
      lastReleaseTime = currentTime;
      newWord = false;
    }

    digitalWrite(ledPin, LOW);
  }
  
  if (keyIsPressed) {
    tone(buzzerPin, (currentTime - pressStart < dashThreshold) ? 1000 : 500);
  } else {
    noTone(buzzerPin);
  }
  
  if (keyState == HIGH) {
    unsigned long releaseDuration = currentTime - lastReleaseTime;

    if (!newWord && morseIndex == 0 && releaseDuration >= wordSpace) {
      newWord = true;
    }

    if (morseIndex > 0 && releaseDuration >= letterSpace) {
      char decodedChar = morseToChar(morseCode);
      for (int i = 0; i < 4; i++) {
        decodedChars[i] = decodedChars[i + 1];
      }
      decodedChars[4] = decodedChar;
      
      // Do a full display update (which draws both battery info and Morse info)
      fullDisplayUpdate();
      
      morseCode[0] = '\0';
      morseIndex = 0;
    }
  }

}



void clearDecodedChars() {

  for (int i = 0; i < 5; i++) {
    decodedChars[i] = ' ';
  }

  // instead of clearing the entire screen, just update the Morse region.
  fullDisplayUpdate();

}


char morseToChar(const char* code) {

  for (byte i = 0; i < 36; i++) {

    if (strcmp_P(code, morseTable[i]) == 0) {
      return pgm_read_byte(&charTable[i]);
    }

  }

  return '?';

}



void fullDisplayUpdate() {

  updateBatteryVariables(); // update battery globals before drawing
  u8g2.firstPage();

  do {
    drawBatteryInfo();
    drawMorseInfo();
  } while (u8g2.nextPage());

}



void drawMorseInfo() {

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(5, 10);
  u8g2.print("WPM: ");
  u8g2.print(wpm);
  
  u8g2.drawHLine(0, 15, 128);
  
  u8g2.setFont(u8g2_font_fub20_tf);
  for (int i = 0; i < 5; i++) {
    u8g2.setCursor(10 + i * 24, 42);
    u8g2.print(decodedChars[i]);
  }
  
  u8g2.drawHLine(0, 50, 128);
  
  u8g2.setFont(u8g2_font_7x14_tf);
  u8g2.setCursor(20, 60);
  u8g2.print(morseCode);

}



void updateBatteryVariables() {

  analogReference(DEFAULT);  // use Vcc as reference
  delay(10);
  int aVal = analogRead(batteryPin);
  float supplyVoltage = readVcc() / 1000.0;  // vcc in volts, measured dynamically
  
  // Calculate battery voltage using the divider ratio
  batteryVoltage = (aVal * supplyVoltage / 1023.0) * ((R1 + R2) / R2);
  batteryPercentage = map(batteryVoltage * 100, minVoltage * 100, maxVoltage * 100, 0, 100);
  batteryPercentage = constrain(batteryPercentage, 0, 100);

}


void drawBatteryInfo() {

  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(65, 10);
  u8g2.print(batteryVoltage, 2);
  u8g2.print("v | ");
  u8g2.print(batteryPercentage);
  u8g2.print("%");

}


long readVcc() {

  // Set the references to AVcc and the measurement to the internal 1.1V reference
  ADMUX = (1 << REFS0) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1);
  delay(2); // Allow voltage to settle
  ADCSRA |= (1 << ADSC); // Start conversion
  while (ADCSRA & (1 << ADSC)); // Wait for conversion to finish
  uint8_t low  = ADCL; // Read low byte first
  uint8_t high = ADCH; // Then high byte
  long result = (high << 8) | low;
  
  return 1125300L / result;

}



