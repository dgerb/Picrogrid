/*
  AtverterH.cpp - Library for AtverterH pin and function mapping
  Created by Daniel Gerber, 9/19/22
  Released into the public domain.
*/

// https://docs.arduino.cc/learn/contributions/arduino-creating-library-guide

#include "AtverterH.h"

AtverterH::AtverterH() {
}

// sets up the pin mode for all AtverterH pins
void AtverterH::setupPinMode() {
  pinMode(LEDG_PIN, OUTPUT);
  pinMode(LEDY_PIN, OUTPUT);
  pinMode(PWM_PIN, OUTPUT);
  pinMode(ALT_PIN, OUTPUT);
  pinMode(VCTRL1_PIN, OUTPUT);
  pinMode(VCTRL2_PIN, OUTPUT);
  pinMode(PRORESET_PIN, OUTPUT);
  pinMode(GATESD_PIN, INPUT);
  pinMode(V1_PIN, INPUT);
  pinMode(I1_PIN, INPUT);
  pinMode(V2_PIN, INPUT);
  pinMode(I2_PIN, INPUT);
  pinMode(T1_PIN, INPUT);
  pinMode(T2_PIN, INPUT);
}

// initialize sensor average array to sensor read
void AtverterH::initializeSensors(int avgWindowLength) {
  _averageWindow = avgWindowLength;
  for (int n = 0; n < _averageWindow; n++) {
    updateVISensors();
    updateTSensors();
  }
}

// initialize sensor average array to sensor read
void AtverterH::initializeSensors() {
  initializeSensors(4); // default average window length 4
}

// initializes the periodic control timer
// inputs: control period in microseconds, controller interrupt function reference
// example usage: atverterH.initializeInterruptTimer(1, &controlUpdate);
void AtverterH::initializeInterruptTimer(long periodus, void (*interruptFunction)(void)) {
  Timer1.initialize(periodus); // arg: period in microseconds
  Timer1.attachInterrupt(interruptFunction); // arg: interrupt function to call
  _bootstrapCounterMax = 10000/periodus; // refresh bootstrap capacitors every 10ms
  refreshBootstrap();
}

// resets protection latch, enabling the gate drivers
void AtverterH::enableGateDrivers() {
  digitalWrite(PRORESET_PIN, HIGH);
  delayMicroseconds(500);
  digitalWrite(PRORESET_PIN, LOW);
}

// legacy; replaced by enableGateDrivers()
void AtverterH::startPWM() {
  enableGateDrivers();
}

// legacy; not needed with FastPWM library
void AtverterH::initializePWMTimer() {
}

// Duty Cycle --------------------------------------------------------------

// sets the duty cycle, integer argument (0-100)
void AtverterH::setDutyCycle(int dutyCycle) {
  _dutyCycle = dutyCycle;
  _dutyCycle = constrain(_dutyCycle, 1, 99);
  // FastPwmPin::enablePwmPin(pin number, frequency, duty cycle 0-100);
  FastPwmPin::enablePwmPin(PWM_PIN, 100000L, _dutyCycle);
}

// sets the duty cycle, float argument (0.0-1.0)
void AtverterH::setDutyCycleFloat(float dutyCycleFloat) {
  setDutyCycle((int)(dutyCycleFloat*100));
}

// get the duty cycle as a int percentage (0-100)
int AtverterH::getDutyCycle() {
  return _dutyCycle;
}

// get the duty cycle as a float (0.0-1.0)
float AtverterH::getDutyCycleFloat() {
  return ((float)getDutyCycle()/100.0);
}

// Alternate Drive Signal --------------------------------------------------

// check bootstrap counter to see if need to refresh caps
void AtverterH::checkBootstrapRefresh() {
  _bootstrapCounter--;
  if (_bootstrapCounter < 0) {
    refreshBootstrap();
  }
}

// refresh the bootstrap capacitors and reset bootstrap counter
void AtverterH::refreshBootstrap() {
  _bootstrapCounter = _bootstrapCounterMax;
  digitalWrite(ALT_PIN, LOW);
  digitalWrite(ALT_PIN, HIGH);
}

// set gate driver 1 to use an always-high alternate signal
void AtverterH::applyHoldHigh1() {
  digitalWrite(VCTRL1_PIN, HIGH);
  digitalWrite(ALT_PIN, HIGH);
  refreshBootstrap();
}

// set gate driver 2 to use an always-high alternate signal
void AtverterH::applyHoldHigh2() {
  digitalWrite(VCTRL2_PIN, HIGH);
  digitalWrite(ALT_PIN, HIGH);
  refreshBootstrap();
}

// sets both gate drivers to use the primary pwm signal
void AtverterH::removeHold() {
  digitalWrite(VCTRL1_PIN, LOW);
  digitalWrite(VCTRL2_PIN, LOW);
  digitalWrite(ALT_PIN, LOW);
}

// Sensor Average Updating -------------------------------------------------

// updates stored VCC value based on an average
void AtverterH::updateVCC() {
  long accumulator = 0;
  int avgLength = 4; // should be less than 10 to avoid overflow
  for (int n = 0; n < avgLength; n++)
    accumulator = accumulator + readVCC();
  _vcc = accumulator/avgLength;
}

void AtverterH::updateTSensors() {
  updateSensorRaw(T1_INDEX, analogReadFast(T1_PIN));
  updateSensorRaw(T2_INDEX, analogReadFast(T2_PIN));
}

void AtverterH::updateVISensors() {
  // analog read measured at 116 microseconds, updateSensorRaw adds negligable time
  // total updateVISensors time is measured at 456 microseconds
  updateSensorRaw(V1_INDEX, analogReadFast(V1_PIN));
  updateSensorRaw(V2_INDEX, analogReadFast(V2_PIN));
  updateSensorRaw(I1_INDEX, analogReadFast(I1_PIN));
  updateSensorRaw(I2_INDEX, analogReadFast(I2_PIN));
}

void AtverterH::updateSensorRaw(int index, int sample) {
  unsigned int accumulator = sample;
  for (int n = 1; n < _averageWindow; n++) {
    _sensorPast[n][index] = _sensorPast[n-1][index];
    accumulator = accumulator + _sensorPast[n][index];
  }
  _sensorPast[0][index] = sample;
  _sensorAverages[index] = (int)(accumulator/_averageWindow);
}

// Raw Sensor Accessor Functions --------------------------------------

// terminal 1 voltage (0 to 1023)
int AtverterH::getRawV1() {
  return _sensorAverages[V1_INDEX];
}

// terminal 2 voltage (0 to 1023)
int AtverterH::getRawV2() {
  return _sensorAverages[V2_INDEX];
}

// terminal 1 current (0 to 1023)
int AtverterH::getRawI1() {
  return _sensorAverages[I1_INDEX];
}

// terminal 2 current (0 to 1023)
int AtverterH::getRawI2() {
  return _sensorAverages[I2_INDEX];
}

// thermistor 1 (0 to 1023)
int AtverterH::getRawT1() {
  return _sensorAverages[T1_INDEX];
}

// thermistor 2 (0 to 1023)
int AtverterH::getRawT2() {
  return _sensorAverages[T2_INDEX];
}

// Fully-Formatted Sensor Accessor Functions --------------------------------------

// returns the averaged VCC value
int AtverterH::getVCC() {
  return _vcc;
}

// returns the averaged V1 value in mV
unsigned int AtverterH::getV1() {
  return raw2mV(getRawV1());
}

// returns the averaged V2 value in mV
unsigned int AtverterH::getV2() {
  return raw2mV(getRawV2());
}

// returns the averaged I1 value in mA
int AtverterH::getI1() {
  return raw2mA(getRawI1());
}

// returns the averaged I2 value in mA
int AtverterH::getI2() {
  return raw2mA(getRawI2());
}

// returns the averaged T1 value in °C
int AtverterH::getT1() {
  return raw2degC(getRawT1());
}

// returns the averaged T2 value in °C
int AtverterH::getT2() {
  return raw2degC(getRawT2());
}

// Conversion Utility Functions ---------------------------------------

// converts a raw 10-bit analog reading (0-1023) to the actual mV (0-65000)
unsigned int AtverterH::raw2mV(int raw) {
  long numerator = (long)raw*getVCC()*13;
  return (unsigned int)(numerator/1024); // analogRead*VCC/1024 * (120k+10k)/10k
}

// converts a raw 10-bit analog reading (0-1023) to ADC mV reading (0-5000)
int AtverterH::raw2mVADC(int raw) {
  long rawL = (long)raw;
  return (int)(rawL*getVCC()/1024); // analogRead*VCC/1024
}

// converts a raw 10-bit analog reading (0-1023) to a mA reading (-5000 to 5000)
//  for MT9221CT-06BR5 current sensor with VCC=5V, sensitivity is 333mV/A, 0A at 2.5V
//  with variable VCC, sensitivity (mV/mA) is VCC/5000*333/1000, 0A at VCC/2
int AtverterH::raw2mA(int raw) {
  long rawL = (long)raw;
  // (analogRead-512) * VCC/1024 * 1/sensitivity = (analogRead-512) * VCC/1024 * 1000/333
  return (rawL-512)*getVCC()*3/1024;
  // return (rawL-512)*1875/128;
}

// converts a raw 10-bit analog reading (0-1023) to a °C reading (0-100)
int AtverterH::raw2degC(int raw) {
  int x0 = 0;
  int x1 = 0;
  int y0 = 0;
  int y1 = 0;
  for (int n = 1; n < sizeof(TTABLE)/sizeof(TTABLE[0]); n++) {
    if (raw < TTABLE[n][0]) {
      x0 = TTABLE[n-1][0];
      x1 = TTABLE[n][0];
      y0 = TTABLE[n-1][1];
      y1 = TTABLE[n][1];
      break;
    }
  }
  return y0 + (raw - x0)*(y1 - y0)/(x1 - x0);
}

// converts a mV value (0-65000) to raw 10-bit form (0-1023)
int AtverterH::mV2raw(unsigned int mV) { // mV * 10k/(10k+120k) * 1024/VCC
  long temp = (long)mV * 79 / getVCC();
  return (int)(temp);
}

// converts a mA value (-5000 to 5000) to raw 10-bit form (0-1023)
int AtverterH::mA2raw(int mA) {
  // mA * sensitivity * 1024/VCC + 512 = mA * 333/1000 * 1024/VCC + 512
  long temp = (long)mA*341/getVCC() + 512;
  return (int)temp;
}

// Sensor Private Utility Functions ---------------------------------------

// returns the official VCC voltage in milliVolts
int AtverterH::readVCC() {
  // reads 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
     ADMUX = _BV(MUX5) | _BV(MUX0) ;
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif
 
  delayMicroseconds(2000); // Wait for Vref to settle (originally 2 ms delay)
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return (int)result; // Vcc in millivolts
}

// Diagnostics -------------------------------------------------------------

// sets one of the LEDs to a given state (HIGH or LOW)
void AtverterH::setLED(int led, int state) {
  digitalWrite(led, state);
}

// sets yellow LED to HIGH or LOW
void AtverterH::setLEDY(int state) {
  setLED(LEDY_PIN, state);
}

// sets green LED to HIGH or LOW
void AtverterH::setLEDG(int state) {
  setLED(LEDG_PIN, state);
}

// Safety -----------------------------------------------------------------

// immediately triggers the gate shutdown
void AtverterH::shutdownGates() {
  pinMode(GATESD_PIN, OUTPUT);
  digitalWrite(GATESD_PIN, LOW);
  delayMicroseconds(10000);
  // digitalWrite(GATESD_PIN, HIGH);
  pinMode(GATESD_PIN, INPUT);
}

// returns true if the gate shutdown signal is currently latched
bool AtverterH::isGateShutdown() {
  return analogRead(GATESD_PIN);
}

// sets the upper current shutoff limit in mA
void AtverterH::setCurrentShutdown(int current_mA) {
  long currentL = (long)current_mA;
  _currentLimitAmplitudeRaw = currentL*128/1875;
}

// checks if last sensed current is greater than current limit
void AtverterH::checkCurrentShutdown() {
  // this function takes negligable microseconds unless actually shutting down
  if (_sensorAverages[I1_INDEX] > 512 + _currentLimitAmplitudeRaw
    || _sensorAverages[I1_INDEX] < 512 - _currentLimitAmplitudeRaw
    || _sensorAverages[I2_INDEX] > 512 + _currentLimitAmplitudeRaw
    || _sensorAverages[I2_INDEX] < 512 - _currentLimitAmplitudeRaw)
    shutdownGates();
}

// sets the upper temperature shutoff in °C
void AtverterH::setThermalShutdown(int temperature_C) {
  _thermalLimitC = temperature_C;
}

// checks if last sensed current is greater than thermal limit
void AtverterH::checkThermalShutdown() {
  if (getT1() > _thermalLimitC || getT2() > _thermalLimitC)
    shutdownGates();
}








