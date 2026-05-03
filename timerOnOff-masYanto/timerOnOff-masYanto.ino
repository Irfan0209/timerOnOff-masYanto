#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

RTC_DS3231 rtc;
SoftwareSerial espSerial(2, 3);

struct Jadwal {
  byte active;
  byte onH, onM, offH, offM;
};
Jadwal jadwal[7];

#define RELAY_PIN 4

// Buffer untuk menerima data Serial tanpa alokasi dinamis (RAM stabil)
const byte BUFFER_SIZE = 30;
char serialBuffer[BUFFER_SIZE];
byte bufferIndex = 0;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  if (!rtc.begin()) {
    // Gunakan makro F() agar teks disimpan di Flash, bukan RAM
    Serial.println(F("RTC Tidak Terdeteksi!")); 
    while (1);
  }
  loadFromEEPROM();
}

void loop() {
  // 1. Pembacaan Serial Non-Blocking (CPU Efficient)
  while (espSerial.available() > 0) {
    char c = espSerial.read();
    if (c == '\n') {
      serialBuffer[bufferIndex] = '\0'; // Tutup string
      parseJadwal(serialBuffer);        // Proses data
      bufferIndex = 0;                  // Reset index untuk data berikutnya
    } else if (bufferIndex < BUFFER_SIZE - 1) {
      serialBuffer[bufferIndex++] = c;
    }
  }

  // 2. Kontrol Relay (Non-blocking delay)
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck >= 10000) { // Cek tiap 10 detik
    checkTimer();
    lastCheck = millis();
  }
}

// Parsing super cepat dengan C-String dan Pointer
void parseJadwal(char* raw) {
  char* token = strtok(raw, ","); // Ambil token pertama ("S")
  
  if (token == NULL || token[0] != 'S') return; // Validasi awal untuk keamanan

  byte h = atoi(strtok(NULL, ",")); // Ambil indeks Hari
  
  // Masukkan langsung ke struct jadwal
  jadwal[h].active = atoi(strtok(NULL, ","));
  jadwal[h].onH    = atoi(strtok(NULL, ","));
  jadwal[h].onM    = atoi(strtok(NULL, ","));
  jadwal[h].offH   = atoi(strtok(NULL, ","));
  jadwal[h].offM   = atoi(strtok(NULL, ","));

  EEPROM.put(h * sizeof(Jadwal), jadwal[h]);
  
  // Feedback untuk debugging menggunakan makro F()
  Serial.print(F("Jadwal Hari "));
  Serial.print(h);
  Serial.println(F(" Terupdate."));
}

void checkTimer() {
  DateTime now = rtc.now();
  byte h = now.dayOfTheWeek();
  
  if (jadwal[h].active == 0) {
    digitalWrite(RELAY_PIN, HIGH);
    return;
  }

  // Optimasi: Operasi bitwise / integer math ringan
  uint16_t currMin = (now.hour() * 60) + now.minute();
  uint16_t onMin = (jadwal[h].onH * 60) + jadwal[h].onM;
  uint16_t offMin = (jadwal[h].offH * 60) + jadwal[h].offM;

  if (onMin < offMin) {
    digitalWrite(RELAY_PIN, (currMin >= onMin && currMin < offMin) ? LOW : HIGH);
  } else { 
    digitalWrite(RELAY_PIN, (currMin >= onMin || currMin < offMin) ? LOW : HIGH);
  }
}

void loadFromEEPROM() {
  for (byte i = 0; i < 7; i++) {
    EEPROM.get(i * sizeof(Jadwal), jadwal[i]);
  }
}
