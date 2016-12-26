#ifndef __NETWORK_H__
#define __NETWORK_H__

/*
 * Network.h - Configures/connects ESP8266 to WiFi and Firebase realtime database.
 * 
 * Connects the Esp8266 using the WiFi SSID/Password stored in LocalSettings.  If the
 * Wifi/Firebase config is missing from 'LocalSettings' (or WiFi does not connect in a
 * reasonable time), starts a captive portal with SSID 'Solar-XXXXXX' to enable
 * configuration over WiFi.
 * 
 * The captive portal is used to configure both WiFi and Firebase, since the device needs
 * both to connect to the cloud and retrieve its remaining configuration.
 * 
 * Note: You can force the captive portal to reconfigure by pressing the RESET button
 *       during boot to delete the locally stored settings.  (See note in LocalStorage.h.)
 */

#include <assert.h>
#include <WiFiManager.h>
#include <user_interface.h>
#include "LocalStorage.h"
#include "CloudStorage.h"

// Used within init() to detect if 'WiFiManager::setSaveConfigCallback()' lambda was invoked.
static bool _shouldSave;

class Network {
  public:
    void init(Device& device, LocalStorage& localStorage) {
      // Blink the built-in LED at a medium pace to indicate that a connection is in progress.
      device.blinkLed(/* rateInMilliseconds = */ 500);
      
      WiFiManager wifiManager;

      // WiFiManager uses the 'setSaveConfigCallback' to indicate that the configuration has
      // changed.  Note when this occurs by setting '_shouldSave'.
      _shouldSave = false;
      wifiManager.setSaveConfigCallback([](){ _shouldSave = true; });

      // Add custom parameters to the WiFiManager for configuring the Firebase host and secret.
      char firebase_host_buffer[128] = "";
      WiFiManagerParameter firebase_host_param("firebase_host", "Firebase Host", firebase_host_buffer, sizeof(firebase_host_buffer));
      wifiManager.addParameter(&firebase_host_param);
      
      char firebase_auth_buffer[128] = "";
      WiFiManagerParameter firebase_auth_param("firebase_auth", "Firebase Secret", firebase_auth_buffer, sizeof(firebase_auth_buffer));
      wifiManager.addParameter(&firebase_auth_param);

      // Construct a stable SSID for the captive portal using the Esp8266's unique chip ID.
      String configPortalSSID = "Solar-";
      configPortalSSID.concat(String(system_get_chip_id(), HEX));
      
      // If we have saved setting in 'localStorage' attempt to connect using them.
      if (localStorage.isConfigLoaded()) {
        // Note: We always store/retrieve SSID/Password from 'localSettings', even though
        //       the WiFiManager will use the last successful values stored in EEPROM.  We
        //       do this because 'localSettings' survives firmware updates while the EEPROM
        //       does not.
        const char* const wifiSsid = localStorage.getWifiSSID();
        const char* const wifiPassword = localStorage.getWifiPassword();
        
        Serial.println("Starting WiFi: ");
        Serial.print("  SSID:     '"); Serial.print(wifiSsid); Serial.println("'");
        Serial.print("  Password: '"); Serial.print(wifiPassword); Serial.println("'");
  
        // We call 'WiFi.begin()' ourselves instead of letting 'WiFiManager::autoConnect()'
        // do it so we can specify the SSID/Password.
        WiFi.begin(wifiSsid, wifiPassword);

        // Have 'WiFiManager' wait for a successful connection.  If the connection fails,
        // 'WiFiManager' will automatically start the captive portal using the SSID specified
        // below.
        wifiManager.autoConnect(configPortalSSID.c_str());
      } else {
        // There were no settings saved in local storage, go directly to the captive portal.
        Serial.println("Starting configuration portal:");
        wifiManager.startConfigPortal(configPortalSSID.c_str());
      }

      // If 'WiFiManager::setSaveConfigCallback()' invoked our callback, save the new configuration.
      if (_shouldSave) {
        Serial.println("Retrieving configuration and saving.");

        // Retrieve the WiFi SSID/Password using the Expressif SDK.
        station_config conf;
        wifi_station_get_config(&conf);

        // Retrieve the Firebase host/secret from our custom WiFiManager parameters.
        strcpy(firebase_host_buffer, firebase_host_param.getValue());
        strcpy(firebase_auth_buffer, firebase_auth_param.getValue());

        // Save the new config to 'LocalStorage'.
        localStorage.saveConfig(
          reinterpret_cast<const char*>(conf.ssid),
          reinterpret_cast<const char*>(conf.password),
          firebase_host_buffer,
          firebase_auth_buffer);

        _shouldSave = false;
      }

      // Paranoid busy loop to await the WiFi connection.  
      Serial.println(); Serial.println(); Serial.print("Connecting to WiFi: ");
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
      }
      Serial.println(WiFi.localIP());

      // Stop blinking the built-in LED.
      device.setLed(true);
    }
};

#endif // __NETWORK_H__
