#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

const char* ssid = "Smart_Lamp";
const char* password = "00001111";

AsyncWebServer server(80);

// Variabel Global untuk menampung JSON
bool adaDataBaru = false;
String payloadJSON = "";

// Simpan UI ke Flash Memory ESP-01
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Sistem Kontrol Lampu</title>
<style>
    body { font-family: 'Segoe UI', sans-serif; background: #e9ecef; margin: 0; padding: 20px; display: flex; justify-content: center; }
    .card { background: #fff; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); width: 100%; max-width: 450px; }
    h2 { text-align: center; color: #333; margin-top: 0; font-size: 22px; }
    .row { display: flex; align-items: center; justify-content: space-between; padding: 12px 0; border-bottom: 1px solid #ddd; transition: 0.3s; }
    .row.disabled { opacity: 0.4; background-color: #f8f9fa; }
    .day-label { font-weight: 600; width: 60px; color: #555; }
    .time-box { display: flex; flex-direction: column; gap: 4px; }
    .time-box label { font-size: 11px; color: #666; font-weight: bold; }
    input[type="time"] { padding: 6px; border: 1px solid #ccc; border-radius: 4px; font-size: 14px; outline: none; }
    .btn { display: block; width: 100%; background: #007bff; color: white; border: none; padding: 14px; border-radius: 6px; font-size: 16px; font-weight: bold; cursor: pointer; margin-top: 20px; transition: 0.2s; }
    .btn:hover { background: #0056b3; }
    .btn:disabled { background: #999; cursor: not-allowed; }
    .switch { position: relative; display: inline-block; width: 44px; height: 24px; flex-shrink: 0; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 24px; }
    .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: #28a745; }
    input:checked + .slider:before { transform: translateX(20px); }
    
    /* Style khusus tombol Manual */
    .manual-group { display: flex; gap: 10px; margin-bottom: 20px; }
    .btn-m { flex: 1; padding: 10px; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; color: white; }
</style>
</head>
<body>

<div class="card">
    <h2>Jadwal Timer Lampu</h2>

    <!-- Fitur Jam Live & Sinkronisasi -->
    <div style="text-align: center; margin-bottom: 20px; padding-bottom: 15px; border-bottom: 1px dashed #ccc;">
        <div id="liveClock" style="font-size: 32px; font-weight: bold; color: #007bff; letter-spacing: 2px;">00:00:00</div>
        <div id="liveDay" style="font-size: 14px; color: #666; margin-bottom: 15px; font-weight: 600;">Menunggu waktu...</div>
        <button class="btn-m" style="background: #17a2b8; width: 100%; border: none; padding: 10px; border-radius: 6px; color: white; font-weight: bold; cursor: pointer; transition: 0.2s;" onclick="syncTime()">⌚ Sinkronkan Jam Alat ke HP</button>
    </div>
    
    <!-- Bagian Tombol Manual Baru -->
    <div style="text-align: center; margin-bottom: 5px; font-size: 14px; color: #666; font-weight: bold;">Kontrol Instan</div>
    <div class="manual-group">
        <button class="btn-m" style="background: #28a745;" onclick="setManual(1)">ON</button>
        <button class="btn-m" style="background: #dc3545;" onclick="setManual(0)">OFF</button>
        <button class="btn-m" style="background: #ffc107; color: #333;" onclick="setManual(2)">AUTO</button>
    </div>
    
    <div id="schedule"></div>
    <button class="btn" onclick="saveData()" id="btnSave">Simpan Konfigurasi</button>
</div>

<script>
    const days = ["Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"];
    const container = document.getElementById('schedule');
    
    days.forEach((day, i) => {
        container.innerHTML += `
        <div class="row" id="row_${i}">
            <label class="switch">
                <input type="checkbox" id="chk_${i}" checked onchange="toggleStatus(${i})">
                <span class="slider"></span>
            </label>
            <div class="day-label">${day}</div>
            <div class="time-box">
                <label>NYALA</label>
                <input type="time" id="on_${i}" value="18:00">
            </div>
            <div class="time-box">
                <label>MATI</label>
                <input type="time" id="off_${i}" value="05:00">
            </div>
        </div>`;
    });

    function toggleStatus(i) {
        const row = document.getElementById(`row_${i}`);
        if(document.getElementById(`chk_${i}`).checked) row.classList.remove('disabled');
        else row.classList.add('disabled');
    }

    // Fungsi fetch untuk tombol manual
    function setManual(val) {
        fetch(`/manual?set=${val}`).then(res => {
            if(res.ok) alert(val === 2 ? "AUTO!" : "OK!");
        });
    }

    function saveData() {
        const btn = document.getElementById('btnSave');
        btn.innerText = 'Menyimpan...';
        btn.disabled = true;

        let payload = [];
        for(let i = 0; i < 7; i++) {
            payload.push({
                active: document.getElementById(`chk_${i}`).checked ? 1 : 0,
                on: document.getElementById(`on_${i}`).value || "00:00",
                off: document.getElementById(`off_${i}`).value || "00:00"
            });
        }

        fetch('/save', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload)
        })
        .then(res => {
            if(res.ok) alert("Jadwal Berhasil Disimpan!");
            else alert("Terjadi kesalahan sistem.");
        })
        .catch(err => alert("Gagal terhubung ke modul!"))
        .finally(() => {
            btn.innerText = 'Simpan Konfigurasi';
            btn.disabled = false;
        });
    }

    // --- FITUR JAM LIVE & SINKRONISASI ---
    const namaHariLokal = ["Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"];
    
    // Update jam di layar setiap 1 detik
    setInterval(() => {
        const now = new Date();
        document.getElementById('liveClock').innerText = now.toLocaleTimeString('id-ID', { hour12: false });
        document.getElementById('liveDay').innerText = namaHariLokal[now.getDay()] + ', ' + now.toLocaleDateString('id-ID');
    }, 1000);

    // Fungsi kirim waktu HP ke ESP-01
    function syncTime() {
        const now = new Date();
        // Format: T,YYYY,MM,DD,HH,MM,SS
        const t = `${now.getFullYear()},${now.getMonth()+1},${now.getDate()},${now.getHours()},${now.getMinutes()},${now.getSeconds()}`;
        
        fetch(`/sync?t=${t}`).then(res => {
            if(res.ok) alert("Waktu disinkronkan!");
        });
    }
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(9600);
  WiFi.softAP(ssid, password);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // PERBAIKAN: Pisahkan respon web dan penerimaan body data
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest * request) {
      // 1. Berikan respon ke HP "OK" segera setelah seluruh data diterima
      request->send(200, "text/plain", "OK");
  }, NULL, 
  [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      
      // 2. Jika ini adalah awalan paket data (chunk pertama), kosongkan variabel
      if (index == 0) {
        payloadJSON = "";
      }
      
      // 3. Susun data satu per satu karakternya (sangat aman dari garbage memory)
      for (size_t i = 0; i < len; i++) {
        payloadJSON += (char)data[i];
      }

      // 4. Jika seluruh paket (total size) sudah tersusun rapi, beri sinyal ke loop
      if (index + len == total) {
        adaDataBaru = true; 
      }
  });

  // Endpoint untuk menerima perintah Manual dari Web
  server.on("/manual", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("set")){
      String val = request->getParam("set")->value();
      
      // Kirim format "M,X" ke Arduino Nano lewat jalur Serial
      Serial.printf("M,%s\n", val.c_str());
      
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

  // Endpoint untuk Sinkronisasi Waktu
  server.on("/sync", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("t")){
      String val = request->getParam("t")->value();
      
      // Kirim format "T,YYYY,MM,DD,HH,MM,SS" ke Arduino Nano
      Serial.printf("T,%s\n", val.c_str());
      
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });
  server.begin();
}

void loop() {
  if (adaDataBaru) {
    // Gunakan DynamicJsonDocument (1024 bytes) agar alokasi aman di tumpukan memori
    DynamicJsonDocument doc(1024); 
    DeserializationError error = deserializeJson(doc, payloadJSON);
    
    if (!error) {
      JsonArray arr = doc.as<JsonArray>();

      for (byte i = 0; i < 7; i++) {
        const char* onT = arr[i]["on"];
        const char* offT = arr[i]["off"];
        byte active = arr[i]["active"];
        
        // Cek agar pointer tidak null sebelum diproses
        if (onT != nullptr && offT != nullptr) {
            char onH[3] = {onT[0], onT[1], '\0'};
            char onM[3] = {onT[3], onT[4], '\0'};
            char offH[3] = {offT[0], offT[1], '\0'};
            char offM[3] = {offT[3], offT[4], '\0'};

            Serial.printf("S,%d,%d,%d,%d,%d,%d\n", i, active, atoi(onH), atoi(onM), atoi(offH), atoi(offM));
            
            // Jeda agar SoftwareSerial Nano tidak kewalahan
            delay(150); 
        }
      }
    } else {
      Serial.println(F("Gagal Parsing JSON Internal"));
      // Jika ingin melihat error yang spesifik, bisa di-uncomment:
      // Serial.println(error.c_str()); 
    }
    
    // Reset data untuk antrean berikutnya
    adaDataBaru = false; 
    payloadJSON = ""; 
  }
}
