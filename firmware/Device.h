#ifndef __DEVICE_H__
#define __DEVICE_H__

/*
 * Device.h - I/O driver for the DTC hardware.
 * 
 * The schematic for the DTC hardware can be found at:
 * https://raw.githubusercontent.com/DLehenbauer/differential-temperature-controller/master/docs/schematic.png
 */

#include <Ticker.h>

class Device {
  private:
    // LOW turns on the blue LED built into the ESP8266.
    static const uint32_t _blue_led_pin = 2;             // D4
    
    // Controls which thermistor is connected to the ADC.  This GPIO pin is connected to the
    // S0 pin of the 74HC4051 mux.
    static const uint32_t _thermistor_mux_s0_pin = 0;    // D3

    // Analog pin used to sample the current value of the thermistor.  Analog pin 0 is the
    // 'A0' pin of the Esp8266.  It's a coincidence it has the same ordinal index as the
    // digital 'D3' pin that controls the mux (above).
    static const uint32_t _thermistor_adc_pin = 0;       // A0
    
    // LOW closes the SPDT relay.  This GPIO pin is connected to the base of the NPN transistor
    // in the low-side relay driver.
    static const uint32_t _relay_pin = 4;                // D2

  public:
    void setRelay(bool closed) const;
    bool getRelay() const;
    void setLed(bool on);
    void blinkLed(uint32_t rateInMilliseconds);
    int readAdc(int channel) const;
    void init();

  private:
    static uint32_t boolToDigital(bool state);
    static bool digitalToBool(uint32_t state);
    static bool negateDigital(uint32_t pin);
    static bool toggleLed();
    void selectAdc(int channel) const;

    Ticker _led_ticker;
};

#endif // __DEVICE_H__
