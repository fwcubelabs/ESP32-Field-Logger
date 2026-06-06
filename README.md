# 📡 ESP32 Field Logger - FWCube Labs

An ultra-lightweight, standalone field logger based on the ESP32, specifically designed for ham radio operators doing portable activations (POTA, SOTA, etc.). 

This firmware lets you log your contacts in a 100% offline environment using your smartphone. The magic happens when it detects an internet connection: it automatically syncs your entire queue of pending QSOs with your main logbook via its REST API. 

**Built for Wavelog:** This project is designed to integrate seamlessly with the fantastic [Wavelog](https://github.com/wavelog/wavelog) open-source logging software.

---

## ✨ Main Features

* **Two Operating Modes:** Offline (Captive Portal) or Online (Real-time).
* **Smart Auto-Sync:** QSOs made offline are queued and synced automatically when back online.
* **Non-Volatile Memory (LittleFS):** Contacts are saved in native ADIF format.
* **Built-in Map:** Visualize contacts via Leaflet map.
* **Mobile-First UI:** Fast, dark-mode web interface.

---

## ⚙️ 1. Pre-Flight Checklist

Before flashing, customize the firmware with your station details in the `.ino` file:

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

---

🔑 2. Setting Up the Wavelog API
Log into your Wavelog account.

Go to Admin -> API.

Click Create API Key (ensure Read & Write permissions).

Copy the key into the code.

⚠️ CRITICAL REQUIREMENT:
The "My Grid (Transmitter)" field in the ESP32 UI must exactly match the Grid Locator configured in your Wavelog Station Profile. If they don't match, the API will reject the QSO!

---

**💻 3. The Web Interface & Functionality**
Navigate to http://logger.local (or the IP shown in your Serial Monitor).

🛠️ Toolbar
⚙️ Add WiFi: Scan and save new networks.

🗑️ Manage Saved WiFis: Delete saved network profiles.

🗺️ View Map: Interactive map of your contacts.

🔄 Reboot: Soft-reset the board.

---

**📡 Data Management**
Sync Offline QSOs: Sync pending QSOs when back online.

Download ADIF: Export your log file.

Erase Memory: Wipes the log. Always download your ADIF file first!


---

License and Credits ESP32 FIELD LOGGER © 2026 by EA1FWG (FWCube Labs)

Licensed under CC BY-NC-ND 4.0

🌐 Visit us at: www.fwcubelabs.radiogalena.es

A special thanks to the Wavelog project for providing such a robust, open-source logging platform.
