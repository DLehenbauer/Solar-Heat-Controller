#ifndef __NTPTIME_H__
#define __NTPTIME_H__

/*
 * NTPTime.h - Synchronize Arduino 'Time' library with an NTP server.
 * 
 * Uses the NTP functionality from the Esp8266 SDK.
 */

#include <Time.h>
#include <TimeLib.h>

extern "C" {
  #include <sntp.h>
}

class NTPTime {
  private:
    // Used by 'timeAndDate()' (below) to prepends a leading zero if 'digits' is < 10.
    String asTwoDigits(int digits){
      return digits < 10
        ? String("0") + digits
        : String(digits);
    }

    // Pretty-print the current time/date in the device's local timezone for logging to
    // the serial monitor.
    String timeAndDate(){
      String result = "";
      
      result += hour();
      result += asTwoDigits(minute());
      result += asTwoDigits(second());
      result += " ";
      result += month();
      result += "/";
      result += day();
      result += "/";
      result += year();
      
      return result;
    }
    
  public:
    void init(const char* const ntpServer, int8_t gmtOffset) {
      // Set the NTP server.
      Serial.print("Syncronizing clock with NTP server '"); Serial.print(ntpServer); Serial.println("': ");
      sntp_setservername(0, const_cast<char*>(ntpServer));

      // Set the timezone of the local device to use when printing to the log with 'timeAndDate()'.
      bool success = sntp_set_timezone(gmtOffset);
      assert(success);

      // The 'Time' library periodically invokes the below lambda to synchronize it's clock.
      setSyncProvider([](){
        // Request a new timestamp from the NTP server.
        uint32_t timestamp = sntp_get_current_timestamp();

        // The returned 'timestamp' may be zero if we have not yet recieved any responses from
        // the NTP server.
        if (timestamp > 0) {
          Serial.print("  Clock synchronized to: "); Serial.print(sntp_get_real_time(timestamp));
        }

        // Return 'timestamp' to the 'Time' library as the new current time.
        return static_cast<time_t>(timestamp);
      });

      // Initialize 'sntp' and beginning polling at a frequency of 1 second for our initial
      // timestamp.
      sntp_init();
      setSyncInterval(1);

      // Block until we've recieved our first response from the NTP server.
      do {
        delay(100);
      } while((timeStatus() == timeNotSet));

      // Once we have the set the time/date, relax the sync interval to once every 30 minutes.
      setSyncInterval(30 * 60);
    }
};

#endif __NTPTIME_H__
