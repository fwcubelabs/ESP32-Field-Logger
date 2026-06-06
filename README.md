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
* **Non-Volatile Memory (LittleFS):** Your contacts are always safe in the board's flash memory in native ADIF format, ready to be downloaded whenever you want.
* **Built-in Map:** Visualize your contacts' grid squares on an interactive Leaflet map (requires internet connection).
* **Dynamic WiFi Management:** Scan and add new WiFi networks straight from the web UI without needing to re-flash the board.
* **Mobile-First UI:** A fast, dark-mode web interface optimized to be easily used from any smartphone screen.

---

## ⚙️ 1. Pre-Flight Checklist (Configure Before Flashing)

Before you hit the "Upload" button in your Arduino IDE or PlatformIO, you need to customize the firmware with your personal station details and home WiFi. 

Open the `.ino` file and look for these sections at the very top:

### Wavelog API Setup
You need to point the ESP32 to your specific logbook installation. 

```cpp
const char* wavelog_api_url = "https://YOUR_DOMAIN/index.php/api/qso"; 
const char* wavelog_api_key = "YOUR_API_KEY_HERE";                     
const char* station_profile_id = "1";                                
wavelog_api_url: The endpoint for your Wavelog installation. Usually, you just need to replace YOUR_DOMAIN with your actual website (e.g., https://myradio.com/index.php/api/qso).

wavelog_api_key: Your unique API key (see Section 2 below on how to get this).

station_profile_id: The ID of your station location in Wavelog. If you only have one station set up, it's almost always 1.

Default WiFi Setup
Set up your default network (like your home WiFi or your phone's mobile hotspot). This is the network the ESP32 will look for first when it boots up.

C++
wifiMulti.addAP("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD"); 
🔑 2. Setting Up the Wavelog API
For the ESP32 to push contacts to your logbook, Wavelog needs to give it permission. Here is how to set it up:

Log into your Wavelog account.

Click on the Admin gear icon (or your username) in the top right corner.

Select API.

Click on Create API Key.

Give it a recognizable description (e.g., "ESP32 Field Logger").

Crucial step: Ensure the key has Read & Write permissions.

Copy the generated string of characters and paste it into the wavelog_api_key variable in your code.

⚠️ CRITICAL REQUIREMENT FOR THE API TO WORK:
Wavelog is very strict about station locations. When you log a contact using the ESP32 web interface, the "My Grid (Transmitter)" field must exactly match the Grid Locator configured in the Station Profile on your Wavelog account. If the grid locator you input on the ESP32 doesn't match the one Wavelog expects for that station_profile_id, the API will reject the QSO and it will fail to sync!

#### 💻 3. The Web Interface & Functionality
Once the board is flashed and running, connect to its WiFi (if offline) or your home network, and navigate to http://logger.local (or the IP address shown in the Serial Monitor).

##### 🛠️ The Top Toolbar
⚙️ Add WiFi Network: Opens a modal that scans for available WiFi networks. Enter the password, hit save, and the board will store it and reboot.

🗑️ Manage Saved WiFis: Shows a list of all custom networks you've added.

🗺️ View Contacts Map: If the board has an active internet connection, clicking this pulls up an interactive map showing your transmission location and the grid squares of the stations you've contacted!

🔄 Reboot Board: A handy soft-reset button.

###### 📝 Logging a Contact
Smart Persistence: The My Grid and Frequency fields remember what you typed even if you refresh the page.

Auto-Band (IARU Region 1): Just type the frequency in MHz (e.g., 14.250). The logger will automatically detect the frequency and fill in the "Band" field (e.g., 20m) for you!

#######📡 Data Management & Syncing
Download ADIF Log: Downloads a standard .adi file.

Sync Offline QSOs to Wavelog: If you operated offline, the ESP32 stashed all your contacts in a secret queue. Once you connect the board to the internet, click this button to inject all the missing QSOs automatically.

Erase Log Memory: Wipes the local flash memory clean. Always download your ADIF file before doing this!

License and Credits

ESP32 FIELD LOGGER © 2026 by EA1FWG (FWCube Labs)

Licensed under CC BY-NC-ND 4.0

####### 🌐 Visit us at: www.fwcubelabs.radiogalena.es

A special thanks to the Wavelog project for providing such a robust, open-source logging platform for the amateur radio community.
