/*
 * firmware.ino - Contains the 'setup()' and 'loop()' entry points for ESP8266 firmware.
 * 
 * See project description at https://github.com/DLehenbauer/differential-temperature-controller.
 */

#include <Time.h>
#include "Device.h"
#include "LocalStorage.h"
#include "Network.h"
#include "CloudStorage.h"
#include "Thermistor.h"
#include "NTPTime.h"

Device _device;           // I/O driver for the hardware device (set relay state, set LED state, etc.)
CloudStorage _cloud;      // Load/store data in the Firebase realtime database.
Thermistor _thermistor;   // For converting ADC values to temperatures.

void setup() {
  // Use same baudrate as the ESP8266 bootloader, so that boot messages are readable.
  Serial.begin(74880);

  // Initialize hardware with initial settings (LED on, Relay open, etc.)
  Serial.println();
  Serial.println("Begin: Setup()");
  _device.init();

  // Load saved Wifi SSID/Password and Firebase auth/host info from built-in flash.
  Serial.println();
  LocalStorage localStorage;
  localStorage.init(_device);

  // Connect to WiFi.  If saved Wifi settings are missing or invalid, creates a
  // captive portal that the end user can use to configure the device.
  Serial.println();
  Network network;
  network.init(_device, localStorage);

  // Connect to Firebase.
  Serial.println();
  _cloud.init(localStorage.getFirebaseHost(), localStorage.getFirebaseAuth());
  
  // Poll until we've been able to update our cloud-stored config for Firebase.
  while (!_cloud.update(_device)) {
    delay(300);
  }

  // Begin synchronizing the 'Time' library with the NTP server.
  Serial.println();
  NTPTime ntp;
  ntp.init(_cloud.getNtpServer(), _cloud.getGmtOffset());

  // Configure the thermistor class with Steinhart–Hart equation parameters from
  // our config stored in Firebase.
  Serial.println();
  _thermistor.init(
    _cloud.getSeriesResistor(),
    _cloud.getResistanceAt0(),
    _cloud.getTemperatureAt0(),
    _cloud.getBCoefficient());

  Serial.println("End: Setup()");
}

// Returns true if the collector should be engaged.  't0' is the temperature of the
// pool.  't1' is the temperature of the collector.
bool getShouldEngageCollector(double t0, double t1) {
  // If either the pool or the collector are below our minimum temperature, do
  // not engage the collector.
  double minT = _cloud.getMinTOn();
  if (t0 < minT || t1 < minT) {
    Serial.print("Temperature below minimum safe operating temperature "); + Serial.print(minT); Serial.println(" celsius.");
    return false;
  }

  // If the delta between the pool and collector is large, engage the collector.
  // If the delta is small or negative, ensure the collector is not engaged.
  double delta = t1 - t0;
  double deltaTOn = _cloud.getDeltaTOn();
  double deltaTOff = _cloud.getDeltaTOff();
  
  if (delta > deltaTOn) {
    Serial.print("Delta "); Serial.print(delta); Serial.print(" > "); Serial.print(deltaTOn); Serial.println(": Collector active.");
    return true;
  } else if (delta < deltaTOff) {
    Serial.print("Delta "); Serial.print(delta); Serial.print(" < "); Serial.print(deltaTOff); Serial.println(": Collector inactive.");
    return false;
  }
}

void loop() {
  int oversample = _cloud.getOversample();                      // # of samples to take for each loop.
  int duration = _cloud.getPollingMilliseconds() / oversample;  // Duration between sample points.

  // Takes evenly spaced samples through the 'getPollingMilliseconds()' period.
  double adc[2] = {};
  for (int i = 0; i < oversample; i++) {
    delay(duration);    
    for (int channel = 0; channel < 2; channel++) {
      uint32_t sample = _device.readAdc(channel);
      Serial.print("adc"); Serial.print(channel); Serial.print(": "); Serial.println(sample);
      adc[channel] += sample;
    }
  }

  // Record timestamp and convert ADC averages to temperature readings.
  time_t timestamp = now();
  ThermistorReading t0 = _thermistor.toReading(adc[0] / oversample);
  ThermistorReading t1 = _thermistor.toReading(adc[1] / oversample);

  Serial.print("adc0: "); t0.print();
  Serial.print("adc1: "); t1.print();

  // Given the temperature data, engage/disengage the collector as appropriate.
  _device.setRelay(getShouldEngageCollector(t0._celsius, t1._celsius));

  // Log the temperature data for this period, and the state of the solar collector.
  _cloud.log(_device, timestamp, t0._adc, t1._adc, _device.getRelay());
  Serial.println();
}
