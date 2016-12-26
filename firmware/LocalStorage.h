#ifndef __LOCAL_STORAGE_H__
#define __LOCAL_STORAGE_H__

/*
 * LocalStorage.h - Stores WiFi and Firebase configuration on the Esp8266's onboard SD/Flash.
 * 
 * Configuration is loaded from SPIFFS during init() from the file '/config.txt'.  '/config.txt'
 * is a binary file containing 4 null terimnated strings.
 * 
 * Note: You can reset previously saved configuration by pressing the RESET button during
 *       boot while the built-in LED is rapidly flashing (i.e., press RESET, wait for rapid
 *       flashing, press RESET again.)  This functionality is implemented in 'init()'.
 */

#include <assert.h>
#include "FS.h"
#include "Device.h"

class LocalStorage {
  private:
    bool _isConfigLoaded = false;   // True if '/config.txt' was successfully loaded during 'init()'.
  
    String _wifi_ssid = "";         // During 'init()', these fields are populated from the contents
    String _wifi_password = "";     // of '/config.txt', if it exists.
    String _firebase_host = "";
    String _firebase_auth = "";

    const char* const _config_file_name = "/config.txt";
    const char* const _reset_sentinel_file_name = "/reset-config.txt";
    const char* const _for_write = "w";
    const char* const _for_read = "r";

    // Wraps 'SPIFFS.open()' with some helpful logging.
    File openFile(const char* const fileName, const char* const mode) {
      // 'mode' must either be '_for_write' or '_for_read'.
      assert(mode == _for_write || mode == _for_read);

      // 'SPIFFS.open()' automatically creates, opens, or replaces as appropriate.
      // Differentiate these cases in the log.
      Serial.print(
        mode == _for_write
          ? SPIFFS.exists(fileName)
            ? "Replacing '"
            : "Creating '"
          : "Opening '");
      
      Serial.print(_config_file_name); Serial.print("' for '"); Serial.print(mode); Serial.print("': ");

      // Open the file and log success/failure.
      File file = SPIFFS.open(fileName, mode);
      if (!file) {
        Serial.println("[FAILED]");
        return file;
      }

      Serial.println("[OK]");

      // Note: 'file' will be NULL if 'SPIFFS.open()' failed.
      return file;
    }

    // Opens '/config.txt'.
    File openConfigFile(const char* const mode) {
      return openFile(_config_file_name, mode);
    }

    // Loads the next null-terminated string from the given 'file'.  The 'name'
    // parameter is only used for identifying which string we're reading in the log.
    String loadString(File file, const char* const name) {
      Serial.print("  "); Serial.print(name); Serial.print(": ");
      String value = file.readStringUntil('\0');
      Serial.print("'"); Serial.print(value); Serial.println("'");
      return value;
    }

    // Saves the null-terminated string to the given 'file'.  The 'name' parameter
    // is only used for identifying which string we're writing in the log.
    void saveString(File file, const char* const name, const char* const value) {
      Serial.print("  "); Serial.print(name); Serial.print(": '"); Serial.print(value); Serial.println("'");
      file.print(value);
      file.print('\0');   // The above 'file.print(value)' does not include the null-terminator.
    }

    // Initializes this class's member variables with the values saved in '/config.txt'.
    bool loadConfig() {
      Serial.println("Loading local configuration: "); Serial.print("  ");
      File configFile = openConfigFile("r");
      if (!configFile) {
        Serial.println("(Local configuration has been cleared.)");
        return false;
      }
      
      _wifi_ssid = loadString(configFile, "WiFi SSID    ");
      _wifi_password = loadString(configFile, "WiFi Password");
      _firebase_host = loadString(configFile, "Firebase Host");
      _firebase_auth = loadString(configFile, "Firebase Auth");
      configFile.close();
    }
  
  public:
    // Called by 'Network::init()' when we have updated configuration to save from the
    // captive portal.
    void saveConfig(
      const char* const wifiSsid,
      const char* const wifiPassword,
      const char* const firebaseHost,
      const char* const firebaseAuth
    ) {
      Serial.println("Saving local configuration: "); Serial.print("  ");
      File configFile = openConfigFile("w");
      if (!configFile) {
        Serial.println("Saving local configuration: [FAILED]");
        return;
      }

      saveString(configFile, "WiFi SSID    ", wifiSsid);
      saveString(configFile, "WiFi Password", wifiPassword);
      saveString(configFile, "Firebase Host", firebaseHost);
      saveString(configFile, "Firebase Auth", firebaseAuth);
      configFile.close();

      // Remove the sentinel file that indicates that local configuration should be/has been
      // cleared (if it exists.)
      SPIFFS.remove(_reset_sentinel_file_name);
    }

    void init(Device& device) {
      Serial.print("Mounting SPIFFS file system (be patient if formatting a new device): ");
      if (!SPIFFS.begin()) {
        Serial.println("[FAILED]");
      }

      Serial.println("[OK]");

      // If the sentinel file exists, the user has requested that we delete our saved configuration.
      if (SPIFFS.exists(_reset_sentinel_file_name)) {
        // Remove '/config.txt', if it exists.
        SPIFFS.remove(_config_file_name);
        Serial.println();
        Serial.println("*** Note: Local configuration has been cleared.");
      } else {
        // Create a sentinel file that we use to detect if the device resets in the
        // next three seconds.
        File sentinelFile = openFile(_reset_sentinel_file_name, "w");

        // Prompt the user to press RESET now if they want to clear the local configuration,
        // both via a serial terminal (if connected) and by blinking the LED rapidly.
        Serial.println();
        Serial.print("*** Press [Reset] now to clear local configuration: ");
        device.blinkLed(/* rateInMilliseconds = */ 100);

        // Give the user a window of three seconds to respond.
        delay(3000);

        // User does not want to clear local config.  Remove the sentinel file.
        SPIFFS.remove(_reset_sentinel_file_name);

        // Notify the user that the window for clearing local config has expired.
        device.setLed(true);
        Serial.println("[Timeout]");
      }

      Serial.println();

      // Load the local config from '/config.txt', if it exists.
      _isConfigLoaded = loadConfig();
    }

    bool isConfigLoaded() const                 { return _isConfigLoaded; }
    const char* const getWifiSSID() const       { return _wifi_ssid.c_str(); }
    const char* const getWifiPassword() const   { return _wifi_password.c_str(); }
    const char* const getFirebaseHost() const   { return _firebase_host.c_str(); }
    const char* const getFirebaseAuth() const   { return _firebase_auth.c_str(); }
};

#endif // __LOCAL_STORAGE_H__
