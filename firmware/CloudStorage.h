#ifndef __CLOUD_STORAGE_H__
#define __CLOUD_STORAGE_H__

/*
 * CloudStorage.h - Retrieves DTC configuration and logs timestamped temperature data/collector
 *                  state to a Firebase database.
 */

#include <FirebaseArduino.h>

class CloudStorage {
  private:
    // We store as much of the configuration as possible in the cloud so that we can change
    // these parameters without reflashing the device.  The values below are overwritten by
    // the value stored the specified path in the Firebase database (if any).

    const char* const _config_ref                   = "config";

    // The fixed resistance in ohms of the resistor in the voltage divider.
    // (See R1 in 'docs/schematic.png'.)
    const char* const _series_resistor_ref          = "seriesResistor";
    float   _series_resistor                        = 8170;

    // The measured resistance of the thermistor (in ohms) at a known temperature.
    const char* const _resistance_at_0_ref          = "resistanceAt0";
    float   _resistance_at_0                        = 9555.55;
    
    // The known temperature of the thermistor at which '_resistance_at_0' was measured.
    const char* const _temperature_at_0_ref         = "temperatureAt0";
    float   _temperature_at_0                       = 25;

    // The calculated B-coefficient of the thermistor in the B parameter equation.
    // (See https://en.wikipedia.org/wiki/Thermistor#B_or_.CE.B2_parameter_equation)
    const char* const _b_coefficient_ref            = "bCoefficient";
    float   _b_coefficient                          = 3380;

    // The frequency at which we make a decision about engaging/disengaging the solar
    // collector (and at which we log temperature data to the Firebase database).
    const char* const _polling_milliseconds_ref     = "pollingMilliseconds";
    int     _polling_milliseconds                   = 5 * 1000;

    // The maximum number of temperature sample points we store in the Firebase database.
    const char* const _max_entries_ref              = "maxEntries";
    int     _max_entries                            = 0;

    // The NTP server used to synchronize the 'Time' library.
    const char* const _ntp_server_ref               = "ntpServer";
    String  _ntp_server                             = "pool.ntp.org";

    // The GMT offset.  Only used when printing diagnostics to the serial monitor.
    // All data logged to the Firebase uses UTC timestamps.
    const char* const _gmt_offset_ref               = "gmtOffset";
    int     _gmt_offset                             = 0;

    // The minimum absolute temperature required to engage the solar collector.  (Used
    // to prevent engaging the collector during near freezing conditions.)
    const char* const _min_t_on_ref               = "minTOn";
    float   _min_t_on                             = 10;

    // The minimum temperature differential between the solar collector and room, pool, etc.
    // required to engage the solar collector.
    // 
    // (Set this sufficiently higher than '_delta_t_off' such that the collector doesn't
    // immediately disengage when we begin circulating air, water, etc. through it.)
    const char* const _delta_t_on_ref               = "deltaTOn";
    float   _delta_t_on                             = 10;

    // The minimal temperature differential between the solar collector and room, pool, etc.
    // required to keep the collector engaged.  (See notes on '_delta_t_on' declaration.)
    const char* const _delta_t_off_ref             = "deltaTOff";
    float   _delta_t_off                            = 1;

    // The number of temperature sample points taken and averaged between each iteration
    // of the polling loop.  (Used to smooth transient noise.)
    const char* const _oversample_ref               = "oversample";
    int     _oversample                             = 16;

    // Path to here datapoints are logged in the Firebase database.
    const String _log_ref                           = String("log");

    // The current log entry (wraps at '_max_entries'.)
    uint32_t _current_entry                         = 0;

  public:
    // Public read-only accessors for exposed fields.  (See comments on field declarations above.)
    int getPollingMilliseconds() const { return _polling_milliseconds; }
    double getSeriesResistor() const { return _series_resistor; }
    double getResistanceAt0() const { return _resistance_at_0; }
    double getTemperatureAt0() const { return _temperature_at_0; }
    double getBCoefficient() const { return _b_coefficient; }
    double getMinTOn() const { return _min_t_on; }
    double getDeltaTOn() const { return _delta_t_on; }
    double getDeltaTOff() const { return _delta_t_off; }
    double getOversample() const { return _oversample; }
    const char* const getNtpServer() const { return _ntp_server.c_str(); }
    int8_t getGmtOffset() const {
      assert(-11 <= _gmt_offset && _gmt_offset <= 13);
      
      return static_cast<int8_t>(_gmt_offset);
    }
    
  private:
    // Convenience wrapper around 'Firebase.failed()' that additionally prints '[FAILED]'
    // plus the error message when 'Firebase.failed()' returns true.
    //
    // Prints nothing when 'Firebase.failed()' returns false, as the caller typically
    // prints the value on success:
    //
    //     Serial.print("Get 'foo/bar': ");
    //     int value = Firebase.getInt(...);
    //     if (!failed()) {                         // Implicitly print failure message if unsuccessful.
    //       Serial.println(value);                 // ...otherwise print the value.
    //     }
    //
    bool failed() {
      // If the last operation was successful, early exit.
      if (!Firebase.failed()) {
        return false;
      }

      // Otherwise print the failure message.
      Serial.println("[FAILED]");
      Serial.print("    (Firebase Error: '"); Serial.print(Firebase.error()); Serial.println("')");
      return true;
    }
    
    // Template used by 'Firebase_maybeUpdate*()' (below) to update the given 'value' with the
    // value stored at 'path', if we're able to successfully retrieve it.  Otherwise, return
    // false and leave 'value' unmodified.
    //
    // (This was used during development to fallback on built-in default values before the Firebase
    // database was populated.)
    template <typename T> bool maybeUpdate(T (*getFn)(FirebaseObject& obj, const String& path), FirebaseObject& obj, const char* const path, T& value) {
      Serial.print("  Accessing '"); Serial.print(path); Serial.print("': ");
      T maybeNewValue = getFn(obj, path);
      if (failed()) {
        return false;
      }

      value = maybeNewValue;
      Serial.println(value);
      return true;
    };

    // Thunks w/known addresses so we can create function pointers for use with 'maybeUpdate<T>()'.
    static int Firebase_getInt(FirebaseObject& obj, const String& path) { return obj.getInt(path); }
    static float Firebase_getFloat(FirebaseObject& obj, const String& path) { return obj.getFloat(path); }
    static String Firebase_getString(FirebaseObject& obj, const String& path) { return obj.getString(path); }

    // Updates 'value' with the Firebase value at 'path', if any.  Otherwise leaves
    // 'value' unmodified and returns false.
    bool maybeUpdateInt(FirebaseObject& obj, const char* const path, int& value) {
      return maybeUpdate<int>(Firebase_getInt, obj, path, value);
    }
    
    // Updates 'value' with the Firebase value at 'path', if any.  Otherwise leaves
    // 'value' unmodified and returns false.
    bool maybeUpdateFloat(FirebaseObject& obj, const char* const path, float& value) {
      return maybeUpdate<float>(Firebase_getFloat, obj, path, value);
    }

    // Updates 'value' with the Firebase value at 'path', if any.  Otherwise leaves
    // 'value' unmodified and returns false.
    bool maybeUpdateString(FirebaseObject& obj, const char* const path, String& value) {
      return maybeUpdate<String>(Firebase_getString, obj, path, value);
    }

  public:
    // Update cached configuration with values from Firebase.  Returns false if 'config'
    // was inaccessible, or any of the expected properties were missing so that caller
    // may optionally retry.
    bool update(Device& device) {
      Serial.print("Updating config from Firebase: ");

      // Blink the LED rapidly to indicate network activity.
      device.blinkLed(25);

      // Load the config as a single FirebaseObject.
      FirebaseObject configObj = Firebase.get(_config_ref);

      // If loading the config failed, return 'false'.  The caller may optionally call 'update()'
      // again to retry.
      if (failed()) {
        return false;
      }

      // Pretty print the loaded FirebaseObject.
      configObj.getJsonVariant().printTo(Serial); Serial.println();

      // Extract the individual values from the FirebaseObject.  Any missing values will cause 'update()'
      // to return false, but preserve the defaults hardcoded above.  (Useful for bootstrapping/testing.)
      //
      // (See comments on variable declarations above for a description of each value.)
      bool success = true;
      success &= maybeUpdateFloat(configObj, _series_resistor_ref, _series_resistor);
      success &= maybeUpdateFloat(configObj, _temperature_at_0_ref, _temperature_at_0);
      success &= maybeUpdateFloat(configObj, _resistance_at_0_ref, _resistance_at_0);
      success &= maybeUpdateFloat(configObj, _b_coefficient_ref, _b_coefficient);
      success &= maybeUpdateInt(configObj,_polling_milliseconds_ref, _polling_milliseconds);
      success &= maybeUpdateInt(configObj, _max_entries_ref, _max_entries);
      success &= maybeUpdateString(configObj, _ntp_server_ref, _ntp_server);
      success &= maybeUpdateInt(configObj, _gmt_offset_ref, _gmt_offset);
      success &= maybeUpdateFloat(configObj, _delta_t_on_ref, _delta_t_on);
      success &= maybeUpdateFloat(configObj, _delta_t_off_ref, _delta_t_off);
      success &= maybeUpdateFloat(configObj, _min_t_on_ref, _min_t_on);
      success &= maybeUpdateInt(configObj, _oversample_ref, _oversample);
      
      // Stop blinking the LED.
      device.setLed(true);

      return success;
    }

    // Initializes connection to Firebase database.
    bool init(const String& firebase_host, const String& firebase_auth) {
      Serial.print("Conecting to Firebase '"); Serial.print(firebase_host); Serial.print("': ");

      Firebase.begin(firebase_host, firebase_auth);

      // Note: In v0.1.0 of the firebase-arduino library, 'Firebase.begin()' appears to succeed
      //       even when the Firebase is inaccessible, etc.  (i.e., the below always prints '[OK]').
      if (!failed()) {
        Serial.println("[OK]");
      }
    }

    // Log the given sample to the next available slot in Firebase.
    void log(Device& device, time_t timestamp, double adc0, double adc1, bool active) {
      // Build a JsonObject containing all the sample information.
      DynamicJsonBuffer _json_buffer;
      JsonObject& root = _json_buffer.createObject();
      root["time"] = timestamp;
      root["0"] = adc0;
      root["1"] = adc1;
      root["active"] = active;

      // Calculate the Firebase ref to the next log entry to write.
      String slotRef = _log_ref + "/" + _current_entry;

      Serial.print("  Logging '"); Serial.print(slotRef); Serial.print("': ");

      // Make three attempts to write the sample to log, and then give up.
      for (int i = 0; i < 3; i++) {
        // Rapidly blink the LED to indicate that network activity is in progress.
        device.blinkLed(19);
        
        Firebase.set(slotRef, root);

        // Stop blinking the LED.
        device.setLed(true);

        if (!failed()) {
          // If we successfully logged the value, pretty print it and advance _current_entry
          // to the next slot.  (Note that the log wraps at '_max_entries'.)
          root.printTo(Serial); Serial.println();
          _current_entry = (_current_entry + 1) % _max_entries;

          break;  // Terminate the loop on success.
        }

        // Otherwise, a short delay before retrying a failed attempt.
        Serial.print("  ... ");
        delay(100);        
      }
    }
};

#endif // __CLOUD_STORAGE_H__
