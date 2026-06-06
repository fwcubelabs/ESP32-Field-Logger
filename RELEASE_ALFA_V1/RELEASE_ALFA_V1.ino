#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <time.h>
#include <ESPmDNS.h>
#include <DNSServer.h> // We need this to make the captive portal work when we're offline
#include <HTTPClient.h>       // Used to shoot REST API calls over to Wavelog
#include <WiFiClientSecure.h> // Handles the secure HTTPS stuff for the API

// --- Network & Hostname Tweaks ---
const char* ap_ssid = "POTA_LOGGER"; 
const char* mdns_name = "logger";    

// --- Wavelog API Setup (Hey, don't forget to edit this before flashing!) ---
const char* wavelog_api_url = "https://YOUR_DOMAIN/index.php/api/qso"; // The URL pointing to your Wavelog/Cloudlog API endpoint
const char* wavelog_api_key = "YOUR_API_KEY_HERE";                     // Your Wavelog API key (make sure it has Read/Write permissions)
const char* station_profile_id = "1";                                  // Your station profile ID in Wavelog (it's usually 1)

// --- Pin Definitions ---
const int LED_AZUL = 2; 

// --- Global Variables ---
bool isOfflineMode = false; 
const byte DNS_PORT = 53;

WiFiMulti wifiMulti;
WebServer server(80);
DNSServer dnsServer; // DNS Server to catch requests when we're acting as an Access Point (Offline mode)
const char* logFile = "/log.adi";
const char* queueFile = "/queue.adi"; // The stash file where we keep our offline QSOs until we get internet back
const char* wifiFile = "/wifi.txt";   // Text file where we safely keep custom WiFi networks

// --- HTML & JS UI ---
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Field Logger</title>

  <style>
    body { font-family: Arial, sans-serif; padding: 20px; max-width: 500px; margin: auto; background-color: #121212; color: #ffffff;}
    input, select, button { width: 100%; padding: 12px; margin: 6px 0 15px 0; box-sizing: border-box; border-radius: 5px; border: none;}
    input, select { background-color: #2c2c2c; color: white; border: 1px solid #444; }
    .half-width { width: 48%; display: inline-block; }
    .btn-blue { background-color: #2196F3; color: white; font-weight: bold; cursor: pointer; }
    .btn-green { background-color: #4CAF50; color: white; font-weight: bold; cursor: pointer; }
    .btn-red { background-color: #f44336; color: white; font-weight: bold; cursor: pointer; }
    .btn-orange { background-color: #ff9800; color: white; font-weight: bold; cursor: pointer; }
    .btn-dark { background-color: #333; color: white; font-weight: bold; cursor: pointer; border: 1px solid #555; }
    .clock-panel { background-color: #1e1e1e; padding: 15px; border-radius: 8px; border: 1px solid #333; margin-bottom: 20px; }
    .footer { text-align: center; margin-top: 30px; font-size: 11px; color: #777; letter-spacing: 0.5px; line-height: 1.6; }
    .footer-link { font-size: 10px; font-weight: bold; }
    
    /* Modal Styles */
    .modal-overlay { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.85); z-index: 1000; align-items: center; justify-content: center; }
    .modal-content { background: #1e1e1e; padding: 20px; border-radius: 8px; width: 90%; max-width: 400px; border: 1px solid #444; }

    #map-container { display: none; margin-top: 20px; }
    #map { height: 350px; width: 100%; border-radius: 8px; border: 1px solid #444; z-index: 1; }
    .map-warning { font-size: 11px; color: #aaa; text-align: center; margin-top: 5px; }
  </style>
</head>
<body>
  
  <div style="display:flex; align-items:center; margin-bottom:15px;">
    <!-- TOOLBAR: Top buttons -->
    <button onclick="openWifiModal()" style="width:45px; height:45px; padding:0; margin:0 8px 0 0; background:#333; cursor:pointer; font-size:20px;" title="Add WiFi Network">⚙️</button>
    <button onclick="openManageWifiModal()" style="width:45px; height:45px; padding:0; margin:0 8px 0 0; background:#ff9800; color:white; cursor:pointer; font-size:20px;" title="Manage Saved WiFis">🗑️</button>
    <button onclick="toggleMap()" style="width:45px; height:45px; padding:0; margin:0 8px 0 0; background:#9C27B0; color:white; cursor:pointer; font-size:20px;" title="View Contacts Map">🗺️</button>
    <button onclick="rebootESP()" style="width:45px; height:45px; padding:0; margin:0 15px 0 0; background:#8b0000; color:white; cursor:pointer; font-size:20px;" title="Reboot Board">🔄</button>
    
    <h2 style="margin:0; font-size:1.3em;">ESP32 Field Logger</h2>
  </div>

  <div class="clock-panel">
    <h3 style="margin-top:0;">Station UTC Clock</h3>
    <form action="/settime" method="POST" style="margin-bottom:0;">
      <input type="date" name="sysdate" id="sysdateInput" class="half-width" required>
      <input type="time" name="systime" id="systimeInput" class="half-width" style="float:right;" step="1" required>
      <button type="submit" class="btn-blue" style="margin-bottom:0; padding:10px;">Manual Clock Sync</button>
    </form>
  </div>
  
  <form action="/log" method="POST">
    <div style="display:flex; justify-content:space-between;">
      <div style="width:48%;">
        <label>Callsign:</label>
        <input type="text" name="call" autocomplete="off" required>
      </div>
      <div style="width:48%;">
        <label>My Grid (Transmitter):</label>
        <input type="text" name="mygrid" id="myGridInput" placeholder="e.g. IN73" pattern="[A-Za-z]{2}[0-9]{2}([A-Za-z]{2})?">
      </div>
    </div>
    
    <div style="display:flex; justify-content:space-between;">
      <div style="width:48%;">
        <label>Band (e.g., 20m):</label>
        <input type="text" name="band" id="bandInput" value="20m" required>
      </div>
      <div style="width:48%;">
        <label>Mode (e.g., SSB):</label>
        <input type="text" name="mode" value="SSB" required>
      </div>
    </div>

    <div style="display:flex; justify-content:space-between;">
      <div style="width:48%;">
        <label>Frequency (MHz):</label>
        <input type="text" inputmode="decimal" name="freq" id="freqInput" placeholder="14.300" required>
      </div>
      <div style="width:48%;">
        <label>Contact Grid Locator:</label>
        <input type="text" name="grid" placeholder="e.g. IN73" pattern="[A-Za-z]{2}[0-9]{2}([A-Za-z]{2})?">
      </div>
    </div>
    
    <div style="display:flex; justify-content:space-between;">
      <div style="width:48%;">
        <label>QSO Date:</label>
        <input type="date" name="qsodate" id="qsoDateInput" required>
      </div>
      <div style="width:48%;">
        <label>QSO Time (UTC):</label>
        <input type="time" name="qsotime" id="qsoTimeInput" required>
      </div>
    </div>
    
    <button type="submit" class="btn-blue" style="margin-top:10px;">Save Contact</button>
  </form>
  
  <hr style="border-color: #333; margin: 20px 0;">
  
  <button onclick="window.location.href='/download'" class="btn-green">Download ADIF Log</button>
  <button type="button" onclick="syncWavelog()" class="btn-orange" style="margin-top: 5px;">Sync Offline QSOs to Wavelog</button>
  <button onclick="clearLog()" class="btn-red" style="margin-top: 5px;">Erase Log Memory</button>

  <!-- Map Container -->
  <div id="map-container">
    <div id="map"></div>
    <div class="map-warning">* Requires Internet connection on this device to load base map graphics.</div>
  </div>

  <div class="footer">
    ESP32 FIELD LOGGER &copy; 2026 by EA1FWG is licensed under CC BY-NC-ND 4.0<br>
    <span class="footer-link">visit www.fwcubelabs.radiogalena.es</span>
  </div>

  <!-- ADD WIFI NETWORK MODAL -->
  <div id="wifiModal" class="modal-overlay">
    <div class="modal-content">
      <h3 style="margin-top:0;">Add WiFi Network</h3>
      <p style="font-size:12px; color:#ccc;">Select a scanned network or type a hidden one manually.</p>
      <form action="/addwifi" method="POST">
        <label>Network Name (SSID):</label>
        <!-- Mobile-friendly dropdown + text input combo -->
        <select id="ssidSelect" style="margin-bottom: 6px;" onchange="document.getElementById('ssidInput').value = this.value;">
          <option value="">Scanning networks...</option>
        </select>
        <input type="text" name="ssid" id="ssidInput" placeholder="Or type SSID manually" required autocomplete="off" style="margin-top: 0;">
        
        <label>Password:</label>
        <input type="password" name="password" required>
        
        <button type="submit" class="btn-blue">Save Network & Reboot</button>
        <button type="button" class="btn-dark" onclick="closeWifiModal()">Cancel</button>
      </form>
    </div>
  </div>

  <!-- MANAGE SAVED WIFI MODAL -->
  <div id="manageWifiModal" class="modal-overlay">
    <div class="modal-content">
      <h3 style="margin-top:0;">Manage Saved WiFis</h3>
      <p style="font-size:12px; color:#ccc;">Select a network to remove it from memory.</p>
      
      <div id="savedWifiList" style="margin-bottom: 15px; max-height: 150px; overflow-y: auto; background: #2c2c2c; padding: 10px; border-radius: 5px;">
        <!-- This list gets populated on the fly by JS -->
      </div>

      <button type="button" class="btn-dark" onclick="closeManageWifiModal()">Cancel</button>
    </div>
  </div>

  <script>
    let espTime = new Date();
    let qsoTimeLocked = false;
    let clockInputsLocked = false; 
    let mapa = null;

    // --- WiFi Scanning & Modal Logic ---
    function openWifiModal() {
      document.getElementById('wifiModal').style.display = 'flex';
      const input = document.getElementById('ssidInput');
      const select = document.getElementById('ssidSelect');
      
      input.value = '';
      select.innerHTML = '<option value="">Scanning networks... please wait</option>';
      
      fetch('/scanwifi')
        .then(response => response.json())
        .then(data => {
          select.innerHTML = '<option value="">-- Select a scanned network --</option>';
          const uniqueSSIDs = [...new Set(data.map(item => item.ssid))];
          uniqueSSIDs.forEach(ssid => {
            if(ssid.trim() !== "") {
              let opt = document.createElement('option');
              opt.value = ssid;
              opt.textContent = ssid;
              select.appendChild(opt);
            }
          });
        })
        .catch(err => {
          select.innerHTML = '<option value="">Error scanning. Type manually below.</option>';
        });
    }

    function closeWifiModal() {
      document.getElementById('wifiModal').style.display = 'none';
    }

    // --- WiFi Management Logic ---
    function openManageWifiModal() {
      document.getElementById('manageWifiModal').style.display = 'flex';
      const listDiv = document.getElementById('savedWifiList');
      listDiv.innerHTML = '<p style="color:#aaa; font-size:13px;">Loading saved networks...</p>';
      
      fetch('/getsavedwifi')
        .then(response => response.json())
        .then(data => {
          if (data.length === 0) {
            listDiv.innerHTML = '<p style="color:#aaa; font-size:13px;">No custom networks saved yet.</p>';
            return;
          }
          listDiv.innerHTML = '';
          data.forEach(ssid => {
            const item = document.createElement('div');
            item.style.display = 'flex';
            item.style.justifyContent = 'space-between';
            item.style.alignItems = 'center';
            item.style.marginBottom = '8px';
            item.style.borderBottom = '1px solid #444';
            item.style.paddingBottom = '5px';
            
            const name = document.createElement('span');
            name.textContent = ssid;
            name.style.overflow = 'hidden';
            name.style.textOverflow = 'ellipsis';
            name.style.whiteSpace = 'nowrap';
            name.style.maxWidth = '250px';
            
            const delBtn = document.createElement('button');
            delBtn.textContent = '❌';
            delBtn.style.width = '40px';
            delBtn.style.padding = '8px';
            delBtn.style.margin = '0';
            delBtn.style.backgroundColor = '#f44336';
            delBtn.style.border = 'none';
            delBtn.style.borderRadius = '4px';
            delBtn.style.cursor = 'pointer';
            delBtn.onclick = () => deleteSpecificWifi(ssid);
            
            item.appendChild(name);
            item.appendChild(delBtn);
            listDiv.appendChild(item);
          });
        })
        .catch(err => {
          listDiv.innerHTML = '<p style="color:#f44336; font-size:13px;">Error loading networks.</p>';
        });
    }

    function closeManageWifiModal() {
      document.getElementById('manageWifiModal').style.display = 'none';
    }

    function deleteSpecificWifi(ssid) {
      if(confirm("Are you sure you want to delete network: " + ssid + "?\n\nThe board will reboot automatically to apply changes.")) {
        fetch('/deletewifi', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: 'ssid=' + encodeURIComponent(ssid)
        }).then(() => {
          alert("Network deleted! Rebooting board...");
        }).catch(() => {
          alert("Rebooting board...");
        });
      }
    }

    // --- Reboot Board ---
    function rebootESP() {
      if(confirm("Are you sure you want to reboot the ESP32? \n\nUseful to force a new WiFi scan if you just enabled your mobile hotspot.")) {
        fetch('/reboot').then(() => {
          alert("Rebooting board... The page will become unresponsive for a few seconds. If you are in Offline mode (POTA_LOGGER), you might need to reconnect to the WiFi manually.");
        }).catch(() => {
          alert("Rebooting board...");
        });
      }
    }

    // --- Manual Wavelog Sync ---
    function syncWavelog() {
      alert("Sincronizando contactos offline pendientes con Wavelog... \nPor favor, espera el mensaje de confirmación.");
      fetch('/sync')
        .then(response => response.text())
        .then(msg => alert(msg))
        .catch(err => alert("Error de red al intentar sincronizar. Verifica tu conexión WiFi."));
    }

    // --- Keep Grid and Freq values across reloads ---
    const myGridField = document.getElementById('myGridInput');
    if(localStorage.getItem('myGridPersisted')) {
        myGridField.value = localStorage.getItem('myGridPersisted');
    }
    myGridField.addEventListener('input', function(e) {
        localStorage.setItem('myGridPersisted', e.target.value);
    });

    const freqField = document.getElementById('freqInput');
    if(localStorage.getItem('freqPersisted')) {
        freqField.value = localStorage.getItem('freqPersisted');
        setTimeout(() => freqField.dispatchEvent(new Event('input')), 100);
    }
    freqField.addEventListener('input', function(e) {
        localStorage.setItem('freqPersisted', e.target.value);
    });

    // --- Auto-Band Selection (IARU Region 1) ---
    document.getElementById('freqInput').addEventListener('input', function(e) {
        let f = parseFloat(e.target.value.replace(',', '.')); 
        if (isNaN(f)) return;
        
        let b = "";
        if (f >= 1.8 && f <= 2.0) b = "160m";
        else if (f >= 3.5 && f <= 3.8) b = "80m";
        else if (f >= 5.35 && f <= 5.36) b = "60m";
        else if (f >= 7.0 && f <= 7.2) b = "40m";
        else if (f >= 10.1 && f <= 10.15) b = "30m";
        else if (f >= 14.0 && f <= 14.35) b = "20m";
        else if (f >= 18.068 && f <= 18.168) b = "17m";
        else if (f >= 21.0 && f <= 21.45) b = "15m";
        else if (f >= 24.89 && f <= 24.99) b = "12m";
        else if (f >= 28.0 && f <= 29.7) b = "10m";
        else if (f >= 50.0 && f <= 52.0) b = "6m";
        else if (f >= 70.0 && f <= 70.5) b = "4m";
        else if (f >= 144.0 && f <= 146.0) b = "2m";
        else if (f >= 430.0 && f <= 440.0) b = "70cm";
        else if (f >= 1240.0 && f <= 1300.0) b = "23cm";
        
        if (b !== "") {
            document.getElementById('bandInput').value = b;
        }
    });

    // --- Clock Sync Magic ---
    fetch('/gettime')
      .then(response => response.json())
      .then(data => {
        let year = parseInt(data.date.split('-')[0]);
        
        if (year < 2024) {
            espTime = new Date(); 
            let formData = new URLSearchParams();
            formData.append("sysdate", espTime.toISOString().split('T')[0]);
            formData.append("systime", espTime.toISOString().split('T')[1].substring(0,8));
            fetch('/settime', { method: 'POST', body: formData });
        } else {
            espTime = new Date(data.date + 'T' + data.time + 'Z');
        }
            
        setInterval(() => {
            espTime.setSeconds(espTime.getSeconds() + 1);
            
            let y = espTime.getUTCFullYear();
            let mo = String(espTime.getUTCMonth() + 1).padStart(2, '0');
            let d = String(espTime.getUTCDate()).padStart(2, '0');
            let h = String(espTime.getUTCHours()).padStart(2, '0');
            let m = String(espTime.getUTCMinutes()).padStart(2, '0');
            let s = String(espTime.getUTCSeconds()).padStart(2, '0');
            
            let dateStr = `${y}-${mo}-${d}`;
            let timeStrWithSec = `${h}:${m}:${s}`;
            let timeStrNoSec = `${h}:${m}`; 
            
            if (!clockInputsLocked) {
                document.getElementById('sysdateInput').value = dateStr;
                document.getElementById('systimeInput').value = timeStrWithSec;
            }
            
            if (!qsoTimeLocked) {
                document.getElementById('qsoDateInput').value = dateStr;
                document.getElementById('qsoTimeInput').value = timeStrNoSec;
            }
        }, 1000);
      });

    document.getElementById('sysdateInput').addEventListener('focus', () => clockInputsLocked = true);
    document.getElementById('systimeInput').addEventListener('focus', () => clockInputsLocked = true);
    document.getElementById('sysdateInput').addEventListener('blur', () => clockInputsLocked = false);
    document.getElementById('systimeInput').addEventListener('blur', () => clockInputsLocked = false);

    document.getElementById('qsoDateInput').addEventListener('input', () => qsoTimeLocked = true);
    document.getElementById('qsoTimeInput').addEventListener('input', () => qsoTimeLocked = true);

    function clearLog() {
      if (confirm("WAIT! Have you downloaded your logs to your PC yet? This will permanently erase all contacts.")) {
        fetch('/clear').then(response => {
          alert("Log memory has been completely cleared!");
          if(mapa) { toggleMap(); toggleMap(); } 
        });
      }
    }

    // --- Map & Coordinates Logic ---
    function maidenheadToLatLon(grid) {
      grid = grid.toUpperCase();
      if (grid.length < 4) return null;
      let lon = (grid.charCodeAt(0) - 65) * 20 - 180 + (grid.charCodeAt(2) - 48) * 2;
      let lat = (grid.charCodeAt(1) - 65) * 10 - 90 + (grid.charCodeAt(3) - 48) * 1;
      
      if (grid.length >= 6) {
          lon += (grid.charCodeAt(4) - 65) * (2/24) + (1/24);
          lat += (grid.charCodeAt(5) - 65) * (1/24) + (0.5/24);
      } else {
          lon += 1; // Just default to the center of the grid
          lat += 0.5;
      }
      return [lat, lon];
    }

    function toggleMap() {
      const container = document.getElementById('map-container');
      
      if (container.style.display === 'block') {
        container.style.display = 'none';
        return;
      }

      if (typeof L === 'undefined') {
          container.style.display = 'block';
          container.innerHTML = '<p style="text-align:center;">Downloading map engine...</p>';

          let css = document.createElement('link');
          css.rel = 'stylesheet';
          css.href = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';
          document.head.appendChild(css);

          let js = document.createElement('script');
          js.src = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';
          js.onload = () => { 
              container.innerHTML = '<div id="map"></div><div class="map-warning">* Requires Internet connection on this device to load base map graphics.</div>';
              initMap(); 
          };
          js.onerror = () => { 
              container.style.display = 'none';
              alert("ERROR: Could not load the map. Verify you have an active Internet connection on your device."); 
          };
          document.head.appendChild(js);
      } else {
          initMap();
      }
    }

    function initMap() {
      const container = document.getElementById('map-container');
      container.style.display = 'block';
      
      if (!mapa) {
        mapa = L.map('map').setView([40.0, -4.0], 4); 
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            maxZoom: 18,
            attribution: '© OpenStreetMap'
        }).addTo(mapa);
      } else {
        mapa.eachLayer((layer) => {
          if(layer instanceof L.Marker || layer instanceof L.CircleMarker) { mapa.removeLayer(layer); }
        });
      }

      setTimeout(() => mapa.invalidateSize(), 200);

      fetch('/download')
        .then(response => response.text())
        .then(adifText => {
            const qsos = adifText.split('<EOR>');
            let marcadores = 0;
            let txGrids = new Set();
            
            qsos.forEach(qso => {
                if(qso.trim() === "") return;
                
                let callMatch = qso.match(/<CALL:\d+>([^<\s]+)/i);
                let call = callMatch ? callMatch[1] : "Unknown";
                
                let gridMatch = qso.match(/<GRIDSQUARE:\d+>([^<\s]+)/i);
                if(gridMatch) {
                    let grid = gridMatch[1];
                    let coords = maidenheadToLatLon(grid);
                    
                    if(coords) {
                        let popupHtml = `<b>${call}</b><br>Locator: ${grid}<br>Lat: ${coords[0].toFixed(4)}<br>Lon: ${coords[1].toFixed(4)}`;
                        L.marker(coords)
                            .bindPopup(popupHtml)
                            .bindTooltip(`QSO with ${call}`)
                            .addTo(mapa);
                        marcadores++;
                    }
                }

                let myGridMatch = qso.match(/<MY_GRIDSQUARE:\d+>([^<\s]+)/i);
                if(myGridMatch) {
                    txGrids.add(myGridMatch[1].toUpperCase());
                }
            });

            txGrids.forEach(txGrid => {
                let coords = maidenheadToLatLon(txGrid);
                if (coords) {
                    L.circleMarker(coords, {
                        color: '#ff0000',
                        fillColor: '#f03',
                        fillOpacity: 0.8,
                        radius: 8,
                        weight: 2
                    }).bindPopup(`<b>📍 My Station (TX)</b><br>Locator: ${txGrid}`)
                      .bindTooltip("Where I operated from")
                      .addTo(mapa);
                }
            });
            
            if (marcadores === 0 && txGrids.size === 0) {
                alert("No contacts with a valid 'Grid Locator' or 'My Grid' found to display.");
            }
        })
        .catch(err => console.error("Error loading map:", err));
    }
  </script>
</body>
</html>
)rawliteral";

// --- Helper Functions ---
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Error mounting LittleFS");
    return;
  }
  if (!LittleFS.exists(logFile)) {
    File file = LittleFS.open(logFile, FILE_WRITE);
    file.print("<EOH>\n");
    file.close();
  }
}

// Let's load up any custom WiFi networks the user saved to flash memory
void loadSavedWiFi() {
  if (LittleFS.exists(wifiFile)) {
    File file = LittleFS.open(wifiFile, "r");
    Serial.println("Loading saved custom WiFi networks...");
    while (file.available()) {
      String ssid = file.readStringUntil('\n');
      String pass = file.readStringUntil('\n');
      ssid.trim();
      pass.trim();
      if (ssid.length() > 0) {
        wifiMulti.addAP(ssid.c_str(), pass.c_str());
        Serial.println(" - Network added: " + ssid);
      }
    }
    file.close();
  }
}

String cleanString(String input, char charToRemove) {
  String output = "";
  for (int i = 0; i < input.length(); i++) {
    if (input[i] != charToRemove) output += input[i];
  }
  return output;
}


// --- Wavelog API Integration ---
// Returns 'true' if the upload went smoothly, 'false' if it choked or there's no internet
bool sendQsoToWavelog(String adifString) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // This is a lifesaver: bypasses strict SSL certificate checks so we don't need to manage root certs
    HTTPClient https;
    https.setTimeout(5000); // 5-second timeout so a bad connection doesn't freeze the whole board

    Serial.println("Sending QSO to Wavelog...");

    if (https.begin(client, wavelog_api_url)) {
      https.addHeader("Content-Type", "application/json");

      // Trim that pesky trailing newline (\n) from the ADIF string so it doesn't break our JSON payload
      adifString.trim();

      // Building the JSON payload by hand, exactly how Wavelog expects it
      String jsonPayload = "{\"key\":\"" + String(wavelog_api_key) + "\",";
      jsonPayload += "\"station_profile_id\":\"" + String(station_profile_id) + "\",";
      jsonPayload += "\"type\":\"adif\",";
      jsonPayload += "\"string\":\"" + adifString + "\"}";

      // Fire away!
      int httpResponseCode = https.POST(jsonPayload);
      https.end();

      if (httpResponseCode >= 200 && httpResponseCode < 300) {
        Serial.printf("Wavelog API Response: Success (Code %d)\n", httpResponseCode);
        return true;
      } else {
        Serial.printf("Error sending to Wavelog: HTTP Code %d\n", httpResponseCode);
      }
    } else {
      Serial.println("Couldn't connect to the Wavelog server.");
    }
  } else {
    Serial.println("No WiFi right now. Don't worry, the QSO is safely saved locally.");
  }
  return false;
}

// --- Offline Queue Syncing ---
String syncPendingQSOs() {
  if (isOfflineMode || WiFi.status() != WL_CONNECTED) {
    return "Error: No internet connection. Hook up to a WiFi first.";
  }
  
  if (!LittleFS.exists(queueFile)) {
    return "All caught up! No pending QSOs to sync.";
  }
  
  File queue = LittleFS.open(queueFile, "r");
  if (!queue || queue.size() == 0) {
    return "The sync queue is completely empty.";
  }

  Serial.println("Syncing pending offline QSOs with Wavelog...");
  String tempQueue = "";
  int successCount = 0;
  int failCount = 0;

  // Go through the temporary queue, line by line (QSO by QSO)
  while (queue.available()) {
    String qso = queue.readStringUntil('\n'); 
    qso.trim(); // Clean up any stray whitespaces
    
    if (qso.length() > 10) { // Just a quick sanity check to ensure it's a valid ADIF line
      qso += "\n"; // Put the newline back so the ADIF format stays correct
      
      if (sendQsoToWavelog(qso)) {
        successCount++;
        Serial.println("Pending QSO successfully uploaded.");
        delay(100); // Give the API a tiny breather so we don't get rate-limited
      } else {
        failCount++;
        tempQueue += qso; // If it failed, keep it in the queue text so we can try again later
      }
    }
  }
  queue.close();

  // Overwrite the queue file (or just delete it if we successfully uploaded everything)
  if (tempQueue.length() == 0) {
    LittleFS.remove(queueFile);
  } else {
    File out = LittleFS.open(queueFile, FILE_WRITE);
    if(out){
      out.print(tempQueue);
      out.close();
    }
  }
  
  String resultado = "Sync complete.\nUploaded: " + String(successCount) + "\nFailed: " + String(failCount);
  Serial.println(resultado);
  return resultado;
}


// --- Web Server Routing ---
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSync() {
  String result = syncPendingQSOs();
  server.send(200, "text/plain", result);
}

void handleGetTime() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  char dateStr[16];
  char timeStr[16]; 
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo); 
  
  String json = "{\"date\":\"" + String(dateStr) + "\",\"time\":\"" + String(timeStr) + "\"}";
  server.send(200, "application/json", json);
}

void handleSetTime() {
  if (server.hasArg("sysdate") && server.hasArg("systime")) {
    String dateStr = server.arg("sysdate");
    String timeStr = server.arg("systime");
    
    int yr, mo, dy, hr, mn, sec = 0;
    sscanf(dateStr.c_str(), "%d-%d-%d", &yr, &mo, &dy);
    sscanf(timeStr.c_str(), "%d:%d:%d", &hr, &mn, &sec);
    
    struct tm t = {0};
    t.tm_year = yr - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = dy;
    t.tm_hour = hr;
    t.tm_min = mn;
    t.tm_sec = sec;
    
    time_t timeSinceEpoch = mktime(&t);
    struct timeval now = { .tv_sec = timeSinceEpoch };
    settimeofday(&now, NULL);
  }
  server.sendHeader("Location", "/");
  server.send(303); 
}

void handleAddWiFi() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String pass = server.arg("password");
    ssid.trim(); pass.trim();

    if (ssid.length() > 0) {
      File f = LittleFS.open(wifiFile, FILE_APPEND);
      if (f) {
        f.println(ssid);
        f.println(pass);
        f.close();
        wifiMulti.addAP(ssid.c_str(), pass.c_str());
        Serial.println("New WiFi network saved: " + ssid);
      }
    }
  }
  
  server.sendHeader("Location", "/");
  server.send(303);
  delay(1000);
  ESP.restart();
}

void handleScanWiFi() {
  int n = WiFi.scanNetworks(false, true);
  String json = "[";
  if (n > 0) {
    bool first = true;
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      ssid.replace("\"", "\\\""); 
      if (ssid.length() > 0) {
        if (!first) json += ",";
        json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        first = false;
      }
    }
  }
  json += "]";
  server.send(200, "application/json", json);
  WiFi.scanDelete(); 
}

void handleGetSavedWiFi() {
  String json = "[";
  if (LittleFS.exists(wifiFile)) {
    File file = LittleFS.open(wifiFile, "r");
    bool first = true;
    while (file.available()) {
      String ssid = file.readStringUntil('\n');
      String pass = file.readStringUntil('\n'); 
      ssid.trim(); pass.trim();
      if (ssid.length() > 0) {
        if (!first) json += ",";
        ssid.replace("\"", "\\\""); 
        json += "\"" + ssid + "\"";
        first = false;
      }
    }
    file.close();
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleDeleteWiFi() {
  if (server.hasArg("ssid")) {
    String targetSSID = server.arg("ssid");
    if (LittleFS.exists(wifiFile)) {
      File file = LittleFS.open(wifiFile, "r");
      File tempFile = LittleFS.open("/wifi_tmp.txt", FILE_WRITE);
      bool modified = false;
      
      while (file.available()) {
        String ssid = file.readStringUntil('\n');
        String pass = file.readStringUntil('\n');
        ssid.trim(); pass.trim();
        
        if (ssid.length() > 0) {
          if (ssid == targetSSID) {
            modified = true; 
            Serial.println("Deleting network: " + ssid);
          } else {
            tempFile.println(ssid);
            tempFile.println(pass);
          }
        }
      }
      
      file.close();
      tempFile.close();
      
      if (modified) {
        LittleFS.remove(wifiFile);
        LittleFS.rename("/wifi_tmp.txt", wifiFile);
      } else {
        LittleFS.remove("/wifi_tmp.txt");
      }
    }
  }
  
  server.send(200, "text/plain", "Network deleted. Rebooting...");
  delay(1000);
  ESP.restart(); 
}

// --- Core Logging & Saving Logic ---
void handleLog() {
  if (server.hasArg("call") && server.hasArg("band") && server.hasArg("mode") && server.hasArg("freq")) {
    String call = server.arg("call"); call.trim(); call.toUpperCase();
    String mygrid = server.arg("mygrid"); mygrid.trim(); mygrid.toUpperCase();
    
    String band = server.arg("band"); band.trim(); band.toUpperCase();
    String mode = server.arg("mode"); mode.trim(); mode.toUpperCase();
    
    String freq = server.arg("freq"); 
    freq.trim(); 
    freq.replace(',', '.'); 
    
    String grid = server.arg("grid"); grid.trim(); grid.toUpperCase();
    
    String qsoDate = cleanString(server.arg("qsodate"), '-'); qsoDate.trim();
    String qsoTime = cleanString(server.arg("qsotime"), ':'); qsoTime.trim();

    String adifEntry = "<CALL:" + String(call.length()) + ">" + call + " ";
    
    if (mygrid.length() > 0) {
      adifEntry += "<MY_GRIDSQUARE:" + String(mygrid.length()) + ">" + mygrid + " ";
    }
    
    adifEntry += "<BAND:" + String(band.length()) + ">" + band + " ";
    adifEntry += "<MODE:" + String(mode.length()) + ">" + mode + " ";
    adifEntry += "<FREQ:" + String(freq.length()) + ">" + freq + " ";
    
    if (grid.length() > 0) {
      adifEntry += "<GRIDSQUARE:" + String(grid.length()) + ">" + grid + " ";
    }
    
    adifEntry += "<QSO_DATE:" + String(qsoDate.length()) + ">" + qsoDate + " ";
    adifEntry += "<TIME_ON:" + String(qsoTime.length()) + ">" + qsoTime + " ";
    adifEntry += "<EOR>\n";

    // 1. Safety first: always write the QSO to our local log file on the flash drive
    File file = LittleFS.open(logFile, FILE_APPEND);
    if (file) {
      file.print(adifEntry);
      file.close();

      // 2. Are we online? Let's try pushing it straight to Wavelog
      bool synced = false;
      if (!isOfflineMode) {
        synced = sendQsoToWavelog(adifEntry);
      }

      // 3. If that failed (offline or server error), stash it in the offline queue to sync later
      if (!synced) {
        File queue = LittleFS.open(queueFile, FILE_APPEND);
        if (queue) {
          queue.print(adifEntry);
          queue.close();
          Serial.println("QSO tucked away in the offline queue for later sync.");
        }
      }

      // 4. Blink the LED to give some visual feedback
      digitalWrite(LED_AZUL, LOW); delay(50);
      digitalWrite(LED_AZUL, HIGH); delay(100);
      digitalWrite(LED_AZUL, LOW); delay(100);
      digitalWrite(LED_AZUL, HIGH); delay(100);
      
      if (!isOfflineMode) {
        digitalWrite(LED_AZUL, HIGH); 
      } else {
        digitalWrite(LED_AZUL, LOW);  
      }
    }
    server.sendHeader("Location", "/");
    server.send(303); 
  } else {
    server.send(400, "text/plain", "Missing form data");
  }
}

void handleDownload() {
  File file = LittleFS.open(logFile, "r");
  if (!file) {
    server.send(404, "text/plain", "Log file not found.");
    return;
  }
  
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Disposition", "attachment; filename=\"FWCUBE_pota_log.adi\"");
  server.sendHeader("Connection", "close");
  
  server.streamFile(file, "application/octet-stream");
  file.close();
}

void handleClear() {
  LittleFS.remove(logFile);
  LittleFS.remove(queueFile); // Wipe out the pending queue too while we're at it
  File file = LittleFS.open(logFile, FILE_WRITE);
  file.print("<EOH>\n");
  file.close();
  server.send(200, "text/plain", "Cleared");
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(500); 
  ESP.restart(); 
}

// --- Main Setup & Loop ---
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_AZUL, OUTPUT);
  digitalWrite(LED_AZUL, LOW);

  Serial.println("\n===========================================================");
  Serial.println("  ESP32 FIELD LOGGER (c) 2026 by EA1FWG is licensed under  ");
  Serial.println("                      CC BY-NC-ND 4.0                      ");
  Serial.println("            visit www.fwcubelabs.radiogalena.es            ");
  Serial.println("===========================================================\n");

  initLittleFS();

  Serial.println("Initializing WiFi Scanner...");
  WiFi.mode(WIFI_STA);

  wifiMulti.addAP("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD"); // Change this to your home or phone's hotspot WiFi details!
  
  loadSavedWiFi();

  Serial.println("Scanning and connecting to strongest known network...");

  int attempts = 0;
  while (wifiMulti.run() != WL_CONNECTED && attempts < 30) {
    digitalWrite(LED_AZUL, HIGH);
    delay(250);
    digitalWrite(LED_AZUL, LOW);
    delay(250);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // ONLINE MODE
    isOfflineMode = false;
    digitalWrite(LED_AZUL, HIGH); 

    Serial.print("\nConnected to: ");
    Serial.println(WiFi.SSID()); 
    
    if (!MDNS.begin(mdns_name)) {
      Serial.println("Error setting up MDNS responder!");
    } else {
      Serial.println("mDNS responder started.");
      Serial.print("Access Logger at: http://");
      Serial.print(mdns_name);
      Serial.println(".local");
    }
    
    Serial.println("Syncing time via NTP...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    
    int retry = 0;
    while (time(nullptr) < 1000000000 && retry < 15) {
      delay(500);
      Serial.print(".");
      retry++;
    }
    if (time(nullptr) > 1000000000) {
      Serial.println("\nTime synced successfully!");
    } else {
      Serial.println("\nNTP timeout. Will rely on browser fallback.");
    }
    
    // We got an internet connection! Let's take a moment to upload any offline QSOs we missed.
    syncPendingQSOs();

  } else {
    // OFFLINE MODE (Access Point)
    isOfflineMode = true;
    digitalWrite(LED_AZUL, LOW); 

    Serial.println("\nNo known networks found. Starting Offline Access Point...");
    
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(); 
    delay(100);
    
    WiFi.softAP(ap_ssid);
    
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    if (!MDNS.begin(mdns_name)) {
      Serial.println("Error setting up MDNS responder!");
    } else {
      Serial.println("mDNS responder started.");
    }
    
    Serial.print("Connect to WiFi network 'POTA_LOGGER', then go to: http://");
    Serial.print(mdns_name);
    Serial.println(".local");
    Serial.print("(Or fall back to http://");
    Serial.print(WiFi.softAPIP());
    Serial.println(")");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/generate_204", HTTP_GET, handleRoot);
  server.on("/log", HTTP_POST, handleLog);
  server.on("/sync", HTTP_GET, handleSync); // Trigger a manual sync from the web UI
  server.on("/gettime", HTTP_GET, handleGetTime);
  server.on("/settime", HTTP_POST, handleSetTime);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/clear", HTTP_GET, handleClear);
  server.on("/addwifi", HTTP_POST, handleAddWiFi); 
  server.on("/scanwifi", HTTP_GET, handleScanWiFi); 
  server.on("/getsavedwifi", HTTP_GET, handleGetSavedWiFi); 
  server.on("/deletewifi", HTTP_POST, handleDeleteWiFi); 
  server.on("/reboot", HTTP_GET, handleReboot);

  server.onNotFound(handleRoot);

  // Let's spin up the web server
  server.begin();
  Serial.println("Web Server Started.");
}

void loop() {
  if (isOfflineMode) {
    dnsServer.processNextRequest(); 
  }
  server.handleClient();
}