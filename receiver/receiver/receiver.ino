#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

const char* ssid = "Lia_Smart_Lamp";
const char* password = "password123";

AsyncWebServer server(80);

/*/ Simpan UI ke Flash Memory (Sangat menghemat RAM ESP-01)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<!-- Masukkan kode HTML dan JS kamu persis di sini -->
</html>
)rawliteral"; */

//===============================

// Simpan UI ke Flash Memory ESP-01
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Sistem Kontrol Lampu</title>
<style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #e9ecef; margin: 0; padding: 20px; display: flex; justify-content: center; }
    .card { background: #fff; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); width: 100%; max-width: 450px; }
    h2 { text-align: center; color: #333; margin-top: 0; font-size: 22px; }
    .row { display: flex; align-items: center; justify-content: space-between; padding: 12px 0; border-bottom: 1px solid #ddd; transition: 0.3s; }
    .row.disabled { opacity: 0.4; background-color: #f8f9fa; }
    .day-label { font-weight: 600; width: 60px; color: #555; }
    .time-box { display: flex; flex-direction: column; gap: 4px; }
    .time-box label { font-size: 11px; color: #666; font-weight: bold; }
    input[type="time"] { padding: 6px; border: 1px solid #ccc; border-radius: 4px; font-size: 14px; outline: none; }
    
    /* Tombol Save */
    .btn { display: block; width: 100%; background: #007bff; color: white; border: none; padding: 14px; border-radius: 6px; font-size: 16px; font-weight: bold; cursor: pointer; margin-top: 20px; transition: 0.2s; }
    .btn:hover { background: #0056b3; }
    .btn:disabled { background: #999; cursor: not-allowed; }

    /* Desain Toggle Switch Aktif/Nonaktif */
    .switch { position: relative; display: inline-block; width: 44px; height: 24px; flex-shrink: 0; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 24px; }
    .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: #28a745; }
    input:checked + .slider:before { transform: translateX(20px); }
</style>
</head>
<body>

<div class="card">
    <h2>Jadwal Timer Lampu</h2>
    <div id="schedule">
        <!-- JavaScript akan mengisi baris hari di sini -->
    </div>
    <button class="btn" onclick="saveData()" id="btnSave">Simpan Konfigurasi</button>
</div>

<script>
    const days = ["Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"];
    const container = document.getElementById('schedule');
    
    // Generate UI secara dinamis untuk menghemat ukuran file HTML
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

    // Fungsi memberikan efek redup saat hari dimatikan
    function toggleStatus(i) {
        const row = document.getElementById(`row_${i}`);
        if(document.getElementById(`chk_${i}`).checked) {
            row.classList.remove('disabled');
        } else {
            row.classList.add('disabled');
        }
    }

    // Fungsi mengumpulkan data dan mengirim JSON ke ESP-01
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
            if(res.ok) alert("Jadwal Berhasil Disimpan ke EEPROM Nano!");
            else alert("Terjadi kesalahan sistem.");
        })
        .catch(err => alert("Gagal terhubung ke ESP-01!"))
        .finally(() => {
            btn.innerText = 'Simpan Konfigurasi';
            btn.disabled = false;
        });
    }
</script>

</body>
</html>
)rawliteral";

//===============================
void setup() {
  Serial.begin(9600);
  WiFi.softAP(ssid, password);

  // Serve UI dari PROGMEM
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest * request) {}, NULL,
    [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      
      // Ukuran 512 bytes sudah sangat presisi untuk array 7 hari
      StaticJsonDocument<512> doc; 
      DeserializationError error = deserializeJson(doc, (const char*)data);
      
      if (error) {
        request->send(400, "text/plain", "JSON Error");
        return;
      }

      JsonArray arr = doc.as<JsonArray>();

      // Kirim ke Nano
      for (byte i = 0; i < 7; i++) {
        const char* onT = arr[i]["on"];
        const char* offT = arr[i]["off"];
        byte active = arr[i]["active"];
        
        // Parsing manual yang lebih ringan dari substring()
        char onH[3] = {onT[0], onT[1], '\0'};
        char onM[3] = {onT[3], onT[4], '\0'};
        char offH[3] = {offT[0], offT[1], '\0'};
        char offM[3] = {offT[3], offT[4], '\0'};

        Serial.printf("S,%d,%d,%d,%d,%d,%d\n", i, active, atoi(onH), atoi(onM), atoi(offH), atoi(offM));
        
        // Memberi waktu kepada background task Wi-Fi (Penting untuk stabilitas ESP!)
        yield(); 
        delay(50); // Jeda aman untuk buffer SoftwareSerial Nano
      }
      request->send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  // ESP8266 sangat menyukai loop yang kosong atau yield()
}
