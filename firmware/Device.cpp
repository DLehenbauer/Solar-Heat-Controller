#include <arduino.h>
#include <assert.h>
#include "Device.h"

/*
 * Device.cpp - (See description in Device.h)
 */

// Explicit conversion from bool -> HIGH/LOW.
/* static */ uint32_t Device::boolToDigital(bool state) {
  return state ? HIGH : LOW;
}

// Explicit from HIGH/LOW -> bool.
/* static */ bool Device::digitalToBool(uint32_t state) {
  assert(state == HIGH || state == LOW);
  return state != LOW;
}

// Negates the current value of the give 'pin'.
/* static */ bool Device::negateDigital(uint32_t pin) {
  bool newState = !digitalToBool(digitalRead(pin));
  digitalWrite(pin, boolToDigital(newState));
  return newState;
}

// Selects the mux input specified by 'channel'.  This mux output connects to the
// A0 analog pin of the ESP8266.
void Device::selectAdc(int channel) const {
  assert(0 <= channel && channel <= 1);

  digitalToBool(digitalRead(0));

  bool s0_active = (channel & 0x01) != 0;
  digitalWrite(_thermistor_mux_s0_pin, boolToDigital(s0_active));

  // 74HC4051 rise/fall rate max 139ns/V @ 4.5v Vcc
  delay(1);
}

// Sets the state of the relay (true -> closed/energized, false -> open/default).
void Device::setRelay(bool closed) const {
  // We negate 'closed' below because the design uses a low-side relay driver that engages
  digitalWrite(_relay_pin, boolToDigital(closed));
}

// Gets the current state of the relay (true -> closed/energized, false -> open/default).
bool Device::getRelay() const {
  // We negate 'closed' below because the design uses a low-side relay driver that engages
  return digitalToBool(digitalRead(_relay_pin));
}

// Sets the state of the built-in blue LED on the ESP8266.
void Device::setLed(bool on) {
  // Setting the LED state implicitly halts any previous calls to blinkLed().
  _led_ticker.detach();
  
  digitalWrite(_blue_led_pin, boolToDigital(!on));
}

// Toggles the current state of the built-in blue LED on the ESP8266.
/* static */ bool Device::toggleLed() {
  return negateDigital(_blue_led_pin);
}

// Blinks the built-in blue LED on the ESP8266 at the specified rate.  (Call 'setLed()' to
// stop blinking.)
void Device::blinkLed(uint32_t rateInMilliseconds) {
  _led_ticker.attach_ms(rateInMilliseconds, [](){ Device::toggleLed(); });
}

// Samples the current value of the mux input specified by 'channel'.
int Device::readAdc(int channel) const {
  selectAdc(channel);
  return analogRead(_thermistor_adc_pin);
}

// Sets the device to its inital state (relay open, LED on, MUX channel 0).
void Device::init() {
  pinMode(_relay_pin, OUTPUT);
  setRelay(false);

  pinMode(_blue_led_pin, OUTPUT);
  setLed(true);

  pinMode(_thermistor_mux_s0_pin, OUTPUT);
  selectAdc(0);
}
