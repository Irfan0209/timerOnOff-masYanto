#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

RTC_DS3231 rtc;
SoftwareSerial espSerial(2, 3);

byte modeManual = 2; // 0 = OFF, 1 = ON, 2 = AUTO

struct Jadwal {
  byte active;
  byte onH, onM, offH, offM;
};
Jadwal jadwal[7];

#define RELAY_PIN 4
#define LED_RELAY 5
#define RUN_LED   6

// Buffer untuk menerima data Serial
const byte BUFFER_SIZE = 30;
char serialBuffer[BUFFER_SIZE];
byte bufferIndex = 0;

static uint8_t m_Counter = 0;
constexpr uint16_t waveStepDelay = 10;  // Delay antar frame LED breathing (ms)
static uint32_t lastWaveMillis = 0;


void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_RELAY,OUTPUT);
  Relay(LOW);

  if (!rtc.begin()) {
    Serial.println(F("RTC Tidak Terdeteksi!")); 
    while (1);
  }
  
  loadFromEEPROM();
}

void loop() {
  // 1. Pembacaan Serial (Hanya menunggu perintah Master)
  while (espSerial.available() > 0) {
    char c = espSerial.read();
    if (c == '\n') {
      serialBuffer[bufferIndex] = '\0'; 
      prosesPerintah(serialBuffer);        
      bufferIndex = 0;                  
    } else if (bufferIndex < BUFFER_SIZE - 1) {
      serialBuffer[bufferIndex++] = c;
    }
  }

  // 2. Kontrol Relay
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck >= 10000) { 
    checkTimer();
    lastCheck = millis();
  }
  getStatusRun();
}

void prosesPerintah(char* raw) {
  char* token = strtok(raw, ","); 
  if (token == NULL) return;

  if (token[0] == 'S') { 
    // ... [Menyimpan Jadwal ke EEPROM - Logika Tetap Sama] ...
    byte h = atoi(strtok(NULL, ","));
    jadwal[h].active = atoi(strtok(NULL, ","));
    jadwal[h].onH    = atoi(strtok(NULL, ","));
    jadwal[h].onM    = atoi(strtok(NULL, ","));
    jadwal[h].offH   = atoi(strtok(NULL, ","));
    jadwal[h].offM   = atoi(strtok(NULL, ","));
    EEPROM.put(h * sizeof(Jadwal), jadwal[h]);
  } 
  else if (token[0] == 'M') { 
    // ... [Kontrol Manual - Logika Tetap Sama] ...
    modeManual = atoi(strtok(NULL, ","));
    if (modeManual == 1) Relay(HIGH);
    else if (modeManual == 0) Relay(LOW);
    else checkTimer(); 
  }
  else if (token[0] == 'T') { 
    // ... [Sinkronisasi Waktu HP - Logika Tetap Sama] ...
    int y = atoi(strtok(NULL, ","));
    int m = atoi(strtok(NULL, ","));
    int d = atoi(strtok(NULL, ","));
    int hh = atoi(strtok(NULL, ","));
    int mm = atoi(strtok(NULL, ","));
    int ss = atoi(strtok(NULL, ","));
    rtc.adjust(DateTime(y, m, d, hh, mm, ss));
  }
  else if (token[0] == 'W') { 
    // FITUR BARU: Menjawab permintaan "Waktu" dari Master ESP-01
    DateTime now = rtc.now();
    espSerial.print(F("R,"));
    espSerial.print(now.year()); espSerial.print(F(","));
    espSerial.print(now.month()); espSerial.print(F(","));
    espSerial.print(now.day()); espSerial.print(F(","));
    espSerial.print(now.hour()); espSerial.print(F(","));
    espSerial.print(now.minute()); espSerial.print(F(","));
    espSerial.println(now.second());
  }
}

void checkTimer() {
  if (modeManual != 2) return;
  
  DateTime now = rtc.now();
  byte h = now.dayOfTheWeek();
  
  if (jadwal[h].active == 0) {
    Relay(LOW);
    return;
  }

  uint16_t currMin = (now.hour() * 60) + now.minute();
  uint16_t onMin = (jadwal[h].onH * 60) + jadwal[h].onM;
  uint16_t offMin = (jadwal[h].offH * 60) + jadwal[h].offM;

  if (onMin < offMin) {
    Relay((currMin >= onMin && currMin < offMin) ? HIGH : LOW);
  } else { 
    Relay((currMin >= onMin || currMin < offMin) ? HIGH : LOW);
  }
}

void loadFromEEPROM() {
  for (byte i = 0; i < 7; i++) {
    EEPROM.get(i * sizeof(Jadwal), jadwal[i]);
  }
}

void Relay(bool state){
  switch(state){
    case 0 :
      digitalWrite(RELAY_PIN,LOW);
      digitalWrite(LED_RELAY,LOW);
    break;

    case 1 :
      digitalWrite(RELAY_PIN,HIGH);
      digitalWrite(LED_RELAY,HIGH);
    break;
  };
}

//============= ANIMASI LED =============//
void getStatusRun() {
  uint32_t now = millis();
  if (now - lastWaveMillis >= waveStepDelay) {
    lastWaveMillis = now;
    updateWaveLED();
  }
}

void updateWaveLED() {
  // brightness naik turun dari 0 - 255 - 0
  uint8_t brightness = (m_Counter < 128) ? m_Counter * 2 : (255 - m_Counter) * 2;
  setLED(brightness);

  m_Counter = (m_Counter + 1) % 256;  // loop kembali ke 0 setelah 255
}

void setLED(uint8_t brightness) {
  analogWrite(RUN_LED, brightness);
}
