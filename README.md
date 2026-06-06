# 📡 ESP32 Field Logger - FWCube Labs

An ultra-lightweight, standalone field logger based on the ESP32, specifically designed for ham radio operators doing portable activations (POTA, SOTA, etc.). 

This firmware lets you log your contacts in a 100% offline environment using your smartphone. The magic happens when it detects an internet connection: it automatically syncs your entire queue of pending QSOs with your main logbook (Wavelog / Cloudlog) via its REST API. Zero lost contacts, zero double data entry!

## ✨ Main Features

* **Two Operating Modes:**
  * **Offline:** It creates its own WiFi network (Captive Portal) that you can connect to with your phone out in the field.
  * **Online:** It connects to your home WiFi or your phone's mobile hotspot to work in real-time.
* **Smart Auto-Sync:** QSOs made offline are safely stored in a queue. As soon as you get network coverage, the ESP32 automatically shoots them over to your Wavelog.
* **Non-Volatile Memory (LittleFS):** Your contacts are always safe in the board's flash memory in native ADIF format, ready to be downloaded whenever you want.
* **Built-in Map:** Visualize your contacts' grid squares on an interactive Leaflet map (requires internet connection).
* **Dynamic WiFi Management:** Scan and add new WiFi networks straight from the web UI without needing to re-flash the board.
* **Mobile-First UI:** A fast, dark-mode web interface optimized to be easily used from any smartphone screen.

## 🚀 Quick Setup

Before compiling and flashing the code to your ESP32, make sure to edit the following lines at the top of the `.ino` file with your personal data:

```cpp
// --- Wavelog API Setup ---
const char* wavelog_api_url = "https://YOUR_DOMAIN/index.php/api/qso"; 
const char* wavelog_api_key = "YOUR_API_KEY_HERE";                     
const char* station_profile_id = "1";                                

// --- WiFi Setup ---
wifiMulti.addAP("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");
