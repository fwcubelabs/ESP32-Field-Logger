# 📡 ESP32 Field Logger - FWCube Labs

An ultra-lightweight, standalone field logger based on the ESP32, specifically designed for ham radio operators doing portable activations (POTA, SOTA, etc.). 

This firmware lets you log your contacts in a 100% offline environment using your smartphone. The magic happens when it detects an internet connection: it automatically syncs your entire queue of pending QSOs with your main logbook via its REST API. 

**Built for Wavelog:** This project is designed to integrate seamlessly with the fantastic [Wavelog](https://github.com/wavelog/wavelog) open-source logging software. Zero lost contacts, zero double data entry!

---

## ✨ Main Features

* **Two Operating Modes:**
  * **Offline:** It creates its own WiFi network (Captive Portal) that you can connect to with your phone out in the field.
  * **Online:** It connects to your home WiFi or your phone's mobile hotspot to work in real-time.
* **Smart Auto-Sync:** QSOs made offline are safely stored in a queue. As soon as you get network coverage, the ESP32 automatically shoots them over to your Wavelog.
* **Non-Volatile Memory (LittleFS):** Your contacts are always safe in the board's flash memory in native ADIF format.
* **Built-in Map:** Visualize your contacts' grid squares on an interactive map.
* **Mobile-First UI:** A fast, dark-mode web interface optimized for any smartphone.

---

## ⚙️ 1. Pre-Flight Checklist (Configure Before Flashing)

Before flashing the code to your ESP32, you must customize the firmware with your personal station details. Open the `.ino` file and look for these sections at the very top:

### Wavelog API Setup
```cpp
// --- Wavelog API Setup ---
const char* wavelog_api_url = "https://YOUR_DOMAIN/index.php/api/qso"; 
const char* wavelog_api_key = "YOUR_API_KEY_HERE";                     
const char* station_profile_id = "1";
Default WiFi Setup
C++
// --- WiFi Setup ---
wifiMulti.addAP("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD"); 
🔑 2. Setting Up the Wavelog API
Log into your Wavelog account.

Go to Admin -> API.

Click Create API Key (ensure Read & Write permissions).

Copy the key into the code.

⚠️ CRITICAL REQUIREMENT: > The "My Grid (Transmitter)" field in the ESP32 UI must exactly match the Grid Locator configured in your Wavelog Station Profile. If they don't match, the API will reject the QSO!

💻 3. The Web Interface & Functionality
Navigate to http://logger.local (or the IP shown in your Serial Monitor).

🛠️ Toolbar
⚙️ Add WiFi: Scan and save new networks.

🗑️ Manage Saved WiFis: Delete saved network profiles.

🗺️ View Map: Interactive map of your contacts (requires internet).

🔄 Reboot: Soft-reset the board.

📡 Data Management
Sync Offline QSOs: If you operated offline, the ESP32 stashed your contacts. Connect to the internet and click this button to inject all pending QSOs into Wavelog.

Download ADIF: Export your log file manually.

Erase Memory: Wipes the log. Always download your ADIF file before doing this!

License and Credits ESP32 FIELD LOGGER © 2026 by EA1FWG (FWCube Labs)

Licensed under CC BY-NC-ND 4.0

🌐 Visit us at: www.fwcubelabs.radiogalena.es

A special thanks to the Wavelog project for providing such a robust, open-source logging platform.
