/*
  PiSupplyH.cpp - Library for PiSupplyH pin and function mapping
  Created by Daniel Gerber, 9/29/25
  Released into the public domain.
*/

#include "PiSupplyH.h"

PiSupplyH::PiSupplyH() {
}

// default initialization routine
void PiSupplyH::initialize() {
  setupPinMode();
  shutdownOutputChannels();
  initializeSensors();
}

// sets up the pin mode for all pins
void PiSupplyH::setupPinMode() {
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(CHPI_PIN, OUTPUT);
  pinMode(CH5V_PIN, OUTPUT);
  pinMode(CHGPIO_PIN, OUTPUT);
  pinMode(CH12V_PIN, OUTPUT);
  pinMode(V48_PIN, INPUT);
  pinMode(V12_PIN, INPUT);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A6, INPUT);
  pinMode(A7, INPUT);
  pinMode(D3, INPUT);
  pinMode(D9, INPUT);
}

// initialize sensor average array to sensor read
void PiSupplyH::initializeSensors() {
  updateVCC();
  int max = 0;
  for (int n = 0; n < NUM_SENSORS; n++)
    if (AVERAGE_WINDOW_MAX[n] > max)
      max = AVERAGE_WINDOW_MAX[n];
  for (int n = 0; n < max; n++) {
    updateSensors();
  }
}

// initializes the periodic control timer
// inputs: control period in microseconds, controller interrupt function reference
// example usage: PiSupplyH.initializeInterruptTimer(1, &controlUpdate);
void PiSupplyH::initializeInterruptTimer(long periodus, void (*interruptFunction)(void)) {
  Timer1.initialize(periodus); // arg: period in microseconds
  Timer1.attachInterrupt(interruptFunction); // arg: interrupt function to call
}

// Channel State Set and Get -----------------------------------------------

// sets the state of the Pi power channel (5V) to on (HIGH) or off (LOW)
void PiSupplyH::setChPi(int state) {
  digitalWrite(CHPI_PIN, !state);
}

// sets the state of 5V output power channel to on (HIGH) or off (LOW)
void PiSupplyH::setCh5V(int state) {
  digitalWrite(CH5V_PIN, !state);
}

// sets the state of GPIO power channel (5V) to on (HIGH) or off (LOW)
void PiSupplyH::setChGPIO(int state) {
  digitalWrite(CHGPIO_PIN, !state);
}

// sets the state of 12V output power channel to on (HIGH) or off (LOW)
void PiSupplyH::setCh12V(int state) {
  digitalWrite(CH12V_PIN, state);
}

int PiSupplyH::getChPi() {
  return !digitalRead(CHPI_PIN);
}

int PiSupplyH::getCh5V() {
  return !digitalRead(CH5V_PIN);
}

int PiSupplyH::getChGPIO() {
  return !digitalRead(CHGPIO_PIN);
}

int PiSupplyH::getCh12V() {
  return digitalRead(CH12V_PIN);
}

// Sensor Average Updating -------------------------------------------------

// updates stored VCC value based on an average
void PiSupplyH::updateVCC() {
  long accumulator = 0;
  int avgLength = 4; // should be less than 10 to avoid overflow
  for (int n = 0; n < avgLength; n++)
    accumulator = accumulator + readVCC();
  _vcc = accumulator/avgLength;
  // if (_vcc < 4950) { // readVCC() might measure ~4500 mV if connected via USB
  //   _vcc = 5000; // to avoid incorrect USB VCC, set to approximate supply output voltage 
  // }
}

// updates voltage sensor averages
void PiSupplyH::updateVSensors() {
  // analogRead() measured at 116 microseconds, updateSensorRaw adds negligable time
  updateSensorRaw(V48_INDEX, analogReadFast(V48_PIN));
  updateSensorRaw(V12_INDEX, analogReadFast(V12_PIN));
}

// updates a single analog GPIO sensor average (0, 1, 6, or 7)
void PiSupplyH::updateASensor(int sensor) {
  // analogRead() measured at 116 microseconds, updateSensorRaw adds negligable time
  switch (sensor) {
    case 0:
      updateSensorRaw(A0_INDEX, analogReadFast(A0_PIN));
      break;
    case 1:
      updateSensorRaw(A1_INDEX, analogReadFast(A1_PIN));
      break;
    case 6:
      updateSensorRaw(A6_INDEX, analogReadFast(A6_PIN));
      break;
    case 7:
      updateSensorRaw(A7_INDEX, analogReadFast(A7_PIN));
      break;
  }
}

// updates all analog GPIO sensor averages
void PiSupplyH::updateASensors() {
  updateASensor(0);
  updateASensor(1);
  updateASensor(6);
  updateASensor(7);
}

void PiSupplyH::updateSensors() {
  updateVSensors();
  updateASensors();
}

void PiSupplyH::updateSensorRaw(int index, int sample) {
  // find correct sensorPast array
  int * sensorPast;
  switch (index) {
    case V48_INDEX:
      sensorPast = _sensorPastV48;
      break;
    case V12_INDEX:
      sensorPast = _sensorPastV12;
      break;
    case A0_INDEX:
      sensorPast = _sensorPastA0;
      break;
    case A1_INDEX:
      sensorPast = _sensorPastA1;
      break;
    case A6_INDEX:
      sensorPast = _sensorPastA6;
      break;
    case A7_INDEX:
      sensorPast = _sensorPastA7;
      break;
    default:
      return;
  }
  // subtract oldest value from accumulator, set new value in array, add new value to accumulator
  _sensorAccumulators[index] -= sensorPast[_sensorIterators[index]];
  sensorPast[_sensorIterators[index]] = sample;
  _sensorAccumulators[index] += sensorPast[_sensorIterators[index]];
  // update sensor average
  _sensorAverages[index] = (int)(_sensorAccumulators[index]/AVERAGE_WINDOW_MAX[index]);
  // increment iterator (or return it to zero)
  _sensorIterators[index] ++;
  if (_sensorIterators[index] >= AVERAGE_WINDOW_MAX[index])
    _sensorIterators[index] = 0;
}

// Raw Sensor Accessor Functions --------------------------------------

// 48V input bus voltage (0 to 1023)
int PiSupplyH::getRawV48() {
  return _sensorAverages[V48_INDEX];
}

// 12V bus voltage (0 to 1023)
int PiSupplyH::getRawV12() {
  return _sensorAverages[V12_INDEX];
}

// Analog GPIO pin voltage (0 to 1023)
int PiSupplyH::getRawAnalog(int analogInd) {
  switch(analogInd) {
    case 0:
      return _sensorAverages[A0_INDEX];
      break;
    case 1:
      return _sensorAverages[A1_INDEX];
      break;
    case 6:
      return _sensorAverages[A6_INDEX];
      break;
    case 7:
      return _sensorAverages[A7_INDEX];
      break;
  }
}

// Fully-Formatted Sensor Accessor Functions --------------------------------------

// returns the averaged VCC value
int PiSupplyH::getVCC() {
  return _vcc;
}

// returns the averaged 48V input bus voltage value in mV
unsigned int PiSupplyH::getV48() {
  return raw2mV(getRawV48());
}

// returns the averaged 12V bus voltage value in mA
int PiSupplyH::getV12() {
  return raw2mV(getRawV12());
}

// returns the averaged 0-5000mV value of a analog GPIO pin (0, 1, 6, 7)
int PiSupplyH::getAnalog(int analogInd) {
  return (long)getRawAnalog(analogInd)*getVCC()/1024;
}

// Conversion Utility Functions ---------------------------------------

// converts a raw 10-bit analog reading (0-1023) to the actual mV (0-65000)
unsigned int PiSupplyH::raw2mV(int raw) {
  long numerator = (long)raw*getVCC()*13;
  return (unsigned int)(numerator/1024); // analogRead*VCC/1024 * (120k+10k)/10k
}

// converts a raw 10-bit analog reading (0-1023) to ADC mV reading (0-5000)
int PiSupplyH::raw2mVADC(int raw) {
  long rawL = (long)raw;
  return (int)(rawL*getVCC()/1024); // analogRead*VCC/1024
}

// converts a raw 10-bit analog reading centered on zero (-512 to 512) to a mA reading (-5000 to 5000)
//  for MT9221CT-06BR5 current sensor with VCC=5V, sensitivity is 333mV/A, 0A at 2.5V
//  with variable VCC, sensitivity (mV/mA) is VCC/5000*333/1000, 0A at VCC/2
int PiSupplyH::raw2mA(int raw) {
  long rawL = (long)raw;
  // (analogRead-512) * VCC/1024 * 1/sensitivity = (analogRead-512) * VCC/1024 * 1000/333
  return rawL*getVCC()*3/1024;
}

// converts a mV value (0-65000) to raw 10-bit form (0-1023)
int PiSupplyH::mV2raw(unsigned int mV) { // mV * 10k/(10k+120k) * 1024/VCC
  long temp = (long)mV * 79 / getVCC();
  return (int)(temp);
}

// converts a mA value (-5000 to 5000) to raw 10-bit form centered around 0 (-512 to 512)
int PiSupplyH::mA2raw(int mA) {
  // mA * sensitivity * 1024/VCC = mA * 333/1000 * 1024/VCC
  long temp = (long)mA*341/getVCC();
  return (int)temp;
}

// Sensor Private Utility Functions ---------------------------------------

// returns the official VCC voltage in milliVolts
int PiSupplyH::readVCC() {
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
 
  result = 1126400L / result; // https://github.com/openenergymonitor/EmonLib/blob/master/EmonLib.h
  // result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return (int)result; // Vcc in millivolts
}

// Diagnostics -------------------------------------------------------------

// sets one of the LEDs to a given state (HIGH or LOW)
void PiSupplyH::setLED(int led, int state) {
  digitalWrite(led, state);
}

// sets yellow LED to HIGH or LOW
void PiSupplyH::setLED1(int state) {
  setLED(LED1_PIN, state);
}

// sets green LED to HIGH or LOW
void PiSupplyH::setLED2(int state) {
  setLED(LED2_PIN, state);
}

// Safety -----------------------------------------------------------------

// immediately triggers the all channels except the Pi to shut down
void PiSupplyH::shutdownOutputChannels() {
  setCh5V(LOW);
  setChGPIO(LOW);
  setCh12V(LOW);
}

// immediately triggers all channels to shut down
void PiSupplyH::shutdownAllChannels() {
  setChPi(LOW);
  setCh5V(LOW);
  setChGPIO(LOW);
  setCh12V(LOW);
}

// Communications ------------------------------------------------------------

// process the parsed RX command, overrides base class virtual function
// MicroPanel readable registers: RVB, RI1, RI2, RI3, RI4, RIT, RVCC, RCH1, RCH2, RCH3, RCH4
// MicroPanel writable registers: WCH1, WCH2, WCH3, WCH4, WIL1, WIL2, WIL3, WIL4
void PiSupplyH::interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RV48") == 0) { // read 48V input bus voltage
    sprintf(getTXBuffer(receiveProtocol), "WV48:%u", getV48());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RV12") == 0) { // read 12V bus voltage
    sprintf(getTXBuffer(receiveProtocol), "WV12:%d", getV12());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RVCC") == 0) { // read the ~5V VCC bus voltage
    sprintf(getTXBuffer(receiveProtocol), "WVCC:%d", getVCC());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RCPI") == 0) { // read state of the Pi power channel
    sprintf(getTXBuffer(receiveProtocol), "WCPI:%d", getChPi());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RC5V") == 0) { // read state of the 5V output power channel
    sprintf(getTXBuffer(receiveProtocol), "WC5V:%d", getCh5V());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RCGP") == 0) { // read state of the GPIO power channel
    sprintf(getTXBuffer(receiveProtocol), "WCGP:%d", getChGPIO());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RC12V") == 0) { // read state of the 12V output power channel
    sprintf(getTXBuffer(receiveProtocol), "WC12V:%d", getCh12V());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WCPI") == 0) { // write the desired Pi power channel state
    int temp = atoi(value);
    setChPi(temp);
    sprintf(getTXBuffer(receiveProtocol), "WCPI:=%d", getChPi());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WC5V") == 0) { // write the desired 5V output power channel state
    int temp = atoi(value);
    setCh5V(temp);
    sprintf(getTXBuffer(receiveProtocol), "WC5V:=%d", getCh5V());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WCGP") == 0) { // write the desired GPIO power channel state
    int temp = atoi(value);
    setChGPIO(temp);
    sprintf(getTXBuffer(receiveProtocol), "WCGP:=%d", getChGPIO());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WC12V") == 0) { // write the desired 12V output power channel state
    int temp = atoi(value);
    setCh12V(temp);
    sprintf(getTXBuffer(receiveProtocol), "WC12V:=%d", getCh12V());
    respondToMaster(receiveProtocol);
  } else { // send command data to the callback listener functions, registered from primary .ino file
    for (int n = 0; n < _commandCallbacksEnd; n++) {
      _commandCallbacks[n](command, value, receiveProtocol);
    }
  }
}

