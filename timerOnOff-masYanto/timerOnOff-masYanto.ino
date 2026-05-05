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
  //loadFromEEPROM();
}

void loop() {
  // 1. Pembacaan Serial Non-Blocking (CPU Efficient)
  while (espSerial.available() > 0) {
    char c = espSerial.read();
    if (c == '\n') {
      serialBuffer[bufferIndex] = '\0'; // Tutup string
      prosesPerintah(serialBuffer);        // Proses data
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

void prosesPerintah(char* raw) {
  char* token = strtok(raw, ","); 
  if (token == NULL) return;

  if (token[0] == 'S') { // Jika perintah Jadwal (Schedule)
    byte h = atoi(strtok(NULL, ","));
    jadwal[h].active = atoi(strtok(NULL, ","));
    jadwal[h].onH    = atoi(strtok(NULL, ","));
    jadwal[h].onM    = atoi(strtok(NULL, ","));
    jadwal[h].offH   = atoi(strtok(NULL, ","));
    jadwal[h].offM   = atoi(strtok(NULL, ","));

    EEPROM.put(h * sizeof(Jadwal), jadwal[h]);
    Serial.print(F("Jadwal Hari ")); Serial.print(h); Serial.println(F(" Terupdate."));
  } 
  else if (token[0] == 'M') { // Jika perintah Manual
    modeManual = atoi(strtok(NULL, ","));
    Serial.print(F("Mode Manual: ")); Serial.println(modeManual);
    
    // Langsung eksekusi relay tanpa menunggu siklus 10 detik
    if (modeManual == 1) {
        digitalWrite(RELAY_PIN, LOW); // Lampu ON
    } else if (modeManual == 0) {
        digitalWrite(RELAY_PIN, HIGH); // Lampu OFF
    } else {
        checkTimer(); // Jika kembali ke AUTO, langsung cek jadwal saat ini
    }
  }
  else if (token[0] == 'T') { // Jika perintah Sinkronisasi Waktu
    int y = atoi(strtok(NULL, ","));
    int m = atoi(strtok(NULL, ","));
    int d = atoi(strtok(NULL, ","));
    int hh = atoi(strtok(NULL, ","));
    int mm = atoi(strtok(NULL, ","));
    int ss = atoi(strtok(NULL, ","));
    
    // Perbarui waktu di modul RTC DS3231
    rtc.adjust(DateTime(y, m, d, hh, mm, ss));
    
    Serial.println(F("Waktu RTC Berhasil Disinkronkan!"));
  }
}

/*/ Parsing super cepat dengan C-String dan Pointer
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
}*/

void checkTimer() {

  // GEMBOK MANUAL: Jika tidak sedang mode AUTO, abaikan pengecekan jam
  if (modeManual != 2) return;
  
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
