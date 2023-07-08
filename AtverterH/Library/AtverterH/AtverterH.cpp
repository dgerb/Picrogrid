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
  pinMode(THERM1_PIN, INPUT);
  pinMode(THERM2_PIN, INPUT);
}

// initializes the periodic control timer
// inputs: control period in microseconds, controller interrupt function reference
// example usage: atverterH.initializeInterruptTimer(1, &controlUpdate);
void AtverterH::initializeInterruptTimer(int periodus, void (*interruptFunction)(void)) {
  Timer1.initialize(periodus); // arg: period in microseconds
  Timer1.attachInterrupt(interruptFunction); // arg: interrupt function to call
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

// sets the duty cycle, integer argument (0-100)
void AtverterH::setDutyCycle(int dutyCycle) {
  _dutyCycle = dutyCycle;
  // FastPwmPin::enablePwmPin(pin number, frequency, duty cycle 0-100);
  FastPwmPin::enablePwmPin(PWM_PIN, 100000L, dutyCycle);
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

// converts a raw 10-bit analog reading (0-1023) to a mV reading (0-5000)
// TODO: assumes VCC = 5V; if not the case, will have to readVCC
int AtverterH::raw2mV(int raw) {
  long rawL = (long)raw;
  return rawL*5000/1024; // analogRead/1024*VCC
}

// converts a raw 10-bit analog reading (0-1023) to the actual mV (0-65000)
// TODO: assumes VCC = 5V; if not the case, will have to readVCC
unsigned int AtverterH::raw2mVActual(int raw) {
  long rawL = (long)raw;
  return (unsigned int)(rawL*8125/128); // analogRead/1024*VCC * (120k+10k)/10k
}

// converts a raw 10-bit analog reading (0-1023) to a mA reading (-5000 to 5000)
// conversion based on current sensor 400mV/1000mA, 0A=2.5V
// TODO: assumes VCC = 5V; if not the case, will have to readVCC
int AtverterH::raw2mA(int raw) {
  long rawL = (long)raw;
  return (rawL-512)*3125/256; // (analogRead/1024*VCC - 2500)*1000/400
}

// get the raw Terminal 1 voltage (0-1023)
int AtverterH::getRawV1() {
  return analogRead(V1_PIN);
}

// get the Terminal 1 voltage (mV) (0-5000)
int AtverterH::getV1() {
  return raw2mV(getRawV1());
}

// get the Terminal 1 voltage (mV) (0-65000)
unsigned int AtverterH::getActualV1() {
  return raw2mVActual(getRawV1());
}

// get the raw Terminal 1 current (0-1023)
int AtverterH::getRawI1() {
  return analogRead(I1_PIN);
}

// get the Terminal 1 current (mA) (-5000 to 5000)
int AtverterH::getI1() {
  return raw2mA(getRawI1());
}

// get the raw Terminal 2 voltage (0-1023)
int AtverterH::getRawV2() {
  return analogRead(V2_PIN);
}

// get the Terminal 2 voltage (mV) (0-5000)
int AtverterH::getV2() {
  return raw2mV(getRawV2());
}

// get the Terminal 2 voltage (mV) (0-65000)
unsigned int AtverterH::getActualV2() {
  return raw2mVActual(getRawV2());
}

// get the raw Terminal 2 current (0-1023)
int AtverterH::getRawI2() {
  return analogRead(I2_PIN);
}

// get the Terminal 2 current (mA) (-5000 to 5000)
int AtverterH::getI2() {
  return raw2mA(getRawI2());
}

/*
// returns the VCC voltage in milliVolts
long AtverterH::readVCC() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
     ADMUX = _BV(MUX5) | _BV(MUX0) ;
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
 
  delayMicroseconds(100); // Wait for Vref to settle (originally 2 ms delay)
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
*/



