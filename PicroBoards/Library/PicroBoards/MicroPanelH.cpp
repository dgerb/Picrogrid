/*
  MicroPanelH.cpp - Library for MicroPanelH pin and function mapping
  Created by Daniel Gerber, 8/14/24
  Released into the public domain.
*/

#include "MicroPanelH.h"

MicroPanelH::MicroPanelH() {
}

// default initialization routine
void MicroPanelH::initialize() {
  setupPinMode();
  shutdownChannels();
  initializeSensors();
  setCurrentLimit1(6500); // set default current shutdown above 5A
  setCurrentLimit2(6500); // set default current shutdown above 5A
  setCurrentLimit3(6500); // set default current shutdown above 5A
  setCurrentLimit4(6500); // set default current shutdown above 5A
  setCurrentLimitTotal(22000); // set total default current shutdown above 20A
}

// sets up the pin mode for all MicroPanelH pins
void MicroPanelH::setupPinMode() {
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(CH1_PIN, INPUT);
  pinMode(CH2_PIN, INPUT);
  pinMode(CH3_PIN, INPUT);
  pinMode(CH4_PIN, INPUT);
  pinMode(VBUS_PIN, INPUT);
  pinMode(I1_PIN, INPUT);
  pinMode(I2_PIN, INPUT);
  pinMode(I3_PIN, INPUT);
  pinMode(I4_PIN, INPUT);
}

// // initialize sensor average array to sensor read
void MicroPanelH::initializeSensors() {
  updateVCC();
  int max = 0;
  for (int n = 0; n < NUM_SENSORS; n++)
    if (AVERAGE_WINDOW_MAX[n] > max)
      max = AVERAGE_WINDOW_MAX[n];
  for (int n = 0; n < max; n++) {
    updateVISensors();
  }
}

// initializes the periodic control timer
// inputs: control period in microseconds, controller interrupt function reference
// example usage: MicroPanelH.initializeInterruptTimer(1, &controlUpdate);
void MicroPanelH::initializeInterruptTimer(long periodus, void (*interruptFunction)(void)) {
  Timer1.initialize(periodus); // arg: period in microseconds
  Timer1.attachInterrupt(interruptFunction); // arg: interrupt function to call
}

// Channel State Set and Get -----------------------------------------------

// set Channel 1 to a state HIGH or LOW
void MicroPanelH::setCh1(int state) {
  setChannel(CH1_PIN, state, _hardwareShutoffEnabled[0], _holdProtectMicros[0]);
}

// set Channel 2 to a state HIGH or LOW
void MicroPanelH::setCh2(int state) {
  setChannel(CH2_PIN, state, _hardwareShutoffEnabled[1], _holdProtectMicros[1]);
}

// set Channel 3 to a state HIGH or LOW
void MicroPanelH::setCh3(int state) {
  setChannel(CH3_PIN, state, _hardwareShutoffEnabled[2], _holdProtectMicros[2]);
}

// set Channel 4 to a state HIGH or LOW
void MicroPanelH::setCh4(int state) {
  setChannel(CH4_PIN, state, _hardwareShutoffEnabled[3], _holdProtectMicros[3]);
}

// generic set channel function
// chPin - pin number cooresponding to the channel gate (e.g. CH1_PIN)
// state - desired channel state, HIGH or LOW
// hardwareShutoffEnabled - when setting the channel, should we keep the hardware shutoff enabled?
// holdProtectMicroseconds - microsecond duration during which we disable hardware shutoff before re-enabling
void MicroPanelH::setChannel(int chPin, int state, bool hardwareShutoffEnabled, int holdProtectMicroseconds) {
  pinMode(chPin, OUTPUT);
  digitalWrite(chPin, state);
  if(hardwareShutoffEnabled) {
    delayMicroseconds(holdProtectMicroseconds);
    pinMode(chPin, INPUT);
  }
}

void MicroPanelH::setDefaultInrushOverride(int holdProtectMicroseconds) {
  setDefaultInrushOverride(1, holdProtectMicroseconds);
  setDefaultInrushOverride(2, holdProtectMicroseconds);
  setDefaultInrushOverride(3, holdProtectMicroseconds);
  setDefaultInrushOverride(4, holdProtectMicroseconds);
}

void MicroPanelH::setDefaultInrushOverride(int channel1234, int holdProtectMicroseconds) {
  if(channel1234 < 1 || channel1234 > 4)
    return;
  _holdProtectMicros[channel1234-1] = holdProtectMicroseconds;
}

// Expert Only: disable channel hardware shutoff
void MicroPanelH::disableHardwareShutoff(int channel1234) {
  switch(channel1234) {
  case 1:
    _hardwareShutoffEnabled[0] = false;
    pinMode(CH1_PIN, OUTPUT);
    break;
  case 2:
    _hardwareShutoffEnabled[1] = false;
    pinMode(CH2_PIN, OUTPUT);
    break;
  case 3:
    _hardwareShutoffEnabled[2] = false;
    pinMode(CH3_PIN, OUTPUT);
    break;
  case 4:
    _hardwareShutoffEnabled[3] = false;
    pinMode(CH4_PIN, OUTPUT);
    break;
  }
}

// Expert Only: enable channel hardware shutoff
void MicroPanelH::enableHardwareShutoff(int channel1234) {
  switch(channel1234) {
  case 1:
    _hardwareShutoffEnabled[0] = true;
    pinMode(CH1_PIN, INPUT);
    break;
  case 2:
    _hardwareShutoffEnabled[1] = true;
    pinMode(CH2_PIN, INPUT);
    break;
  case 3:
    _hardwareShutoffEnabled[2] = true;
    pinMode(CH3_PIN, INPUT);
    break;
  case 4:
    _hardwareShutoffEnabled[3] = true;
    pinMode(CH4_PIN, INPUT);
    break;
  }
}

int MicroPanelH::getCh1() {
  return digitalRead(CH1_PIN);
}

int MicroPanelH::getCh2() {
  return digitalRead(CH2_PIN);
}

int MicroPanelH::getCh3() {
  return digitalRead(CH3_PIN);
}

int MicroPanelH::getCh4() {
  return digitalRead(CH4_PIN);
}

// Sensor Average Updating -------------------------------------------------

// updates stored VCC value based on an average
void MicroPanelH::updateVCC() {
  long accumulator = 0;
  int avgLength = 4; // should be less than 10 to avoid overflow
  for (int n = 0; n < avgLength; n++)
    accumulator = accumulator + readVCC();
  _vcc = accumulator/avgLength;
  if (_vcc < 4950) { // readVCC() might measure ~4500 mV if connected via USB
    _vcc = 5000; // to avoid incorrect USB VCC, set to approximate supply output voltage 
  }
}

void MicroPanelH::updateVISensors() {
  // analogRead() measured at 116 microseconds, updateSensorRaw adds negligable time
  // total updateVISensors time is measured at 456 microseconds
  updateSensorRaw(VBUS_INDEX, analogReadFast(VBUS_PIN));
  updateSensorRaw(I1_INDEX, analogReadFast(I1_PIN) - 512);
  updateSensorRaw(I2_INDEX, analogReadFast(I2_PIN) - 512);
  updateSensorRaw(I3_INDEX, analogReadFast(I3_PIN) - 512);
  updateSensorRaw(I4_INDEX, analogReadFast(I4_PIN) - 512);
}

void MicroPanelH::updateSensorRaw(int index, int sample) {
  // find correct sensorPast array
  int * sensorPast;
  switch (index) {
    case VBUS_INDEX:
      sensorPast = _sensorPastVBus;
      break;
    case I1_INDEX:
      sensorPast = _sensorPastI1;
      break;
    case I2_INDEX:
      sensorPast = _sensorPastI2;
      break;
    case I3_INDEX:
      sensorPast = _sensorPastI3;
      break;
    case I4_INDEX:
      sensorPast = _sensorPastI4;
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

// bus voltage (0 to 1023)
int MicroPanelH::getRawVBus() {
  return _sensorAverages[VBUS_INDEX];
}

// terminal 1 current (0 to 1023)
int MicroPanelH::getRawI1() {
  return _sensorAverages[I1_INDEX];
}

// terminal 2 current (0 to 1023)
int MicroPanelH::getRawI2() {
  return _sensorAverages[I2_INDEX];
}

// terminal 3 current (0 to 1023)
int MicroPanelH::getRawI3() {
  return _sensorAverages[I3_INDEX];
}

// terminal 4 current (0 to 1023)
int MicroPanelH::getRawI4() {
  return _sensorAverages[I4_INDEX];
}

// total current
int MicroPanelH::getRawITotal() {
  return getRawI1() + getRawI2() + getRawI3() + getRawI4();
}

// Fully-Formatted Sensor Accessor Functions --------------------------------------

// returns the averaged VCC value
int MicroPanelH::getVCC() {
  return _vcc;
}

// returns the averaged VBus value in mV
unsigned int MicroPanelH::getVBus() {
  return raw2mV(getRawVBus());
}

// returns the averaged I1 value in mA
int MicroPanelH::getI1() {
  return raw2mA(getRawI1());
}

// returns the averaged I2 value in mA
int MicroPanelH::getI2() {
  return raw2mA(getRawI2());
}

// returns the averaged I3 value in mA
int MicroPanelH::getI3() {
  return raw2mA(getRawI3());
}

// returns the averaged I4 value in mA
int MicroPanelH::getI4() {
  return raw2mA(getRawI4());
}

// returns the averaged total current value in mA
int MicroPanelH::getITotal() {
  return getI1() + getI2() + getI3() + getI4();
}

// Conversion Utility Functions ---------------------------------------

// converts a raw 10-bit analog reading (0-1023) to the actual mV (0-65000)
unsigned int MicroPanelH::raw2mV(int raw) {
  long numerator = (long)raw*getVCC()*13;
  return (unsigned int)(numerator/1024); // analogRead*VCC/1024 * (120k+10k)/10k
}

// converts a raw 10-bit analog reading (0-1023) to ADC mV reading (0-5000)
int MicroPanelH::raw2mVADC(int raw) {
  long rawL = (long)raw;
  return (int)(rawL*getVCC()/1024); // analogRead*VCC/1024
}

// converts a raw 10-bit analog reading centered on zero (-512 to 512) to a mA reading (-5000 to 5000)
//  for MT9221CT-06BR5 current sensor with VCC=5V, sensitivity is 333mV/A, 0A at 2.5V
//  with variable VCC, sensitivity (mV/mA) is VCC/5000*333/1000, 0A at VCC/2
int MicroPanelH::raw2mA(int raw) {
  long rawL = (long)raw;
  // (analogRead-512) * VCC/1024 * 1/sensitivity = (analogRead-512) * VCC/1024 * 1000/333
  return rawL*getVCC()*3/1024;
}

// converts a mV value (0-65000) to raw 10-bit form (0-1023)
int MicroPanelH::mV2raw(unsigned int mV) { // mV * 10k/(10k+120k) * 1024/VCC
  long temp = (long)mV * 79 / getVCC();
  return (int)(temp);
}

// converts a mA value (-5000 to 5000) to raw 10-bit form centered around 0 (-512 to 512)
int MicroPanelH::mA2raw(int mA) {
  // mA * sensitivity * 1024/VCC = mA * 333/1000 * 1024/VCC
  long temp = (long)mA*341/getVCC();
  return (int)temp;
}

// Droop Resistance Conversions -------------------------------------------

// sets the stored droop resistance
void MicroPanelH::setRDroop(int mOhm) {
  // ohmraw = mV/mA * A mA/rawI / (B mV/rawV) = A/B * ohm = A/(1000*B) mohm
  //  = (mohm / 1000) * (micropanel.mA2raw(1000)) / (micropanel.mV2raw(1000))
  //  = (mohm / 1000) * (vcc/341) / (vcc/79)
  //  = mohm / 1000 * 79 / 341
  _rDroop = (long)RDROOPFACTOR*mOhm/4316; // here we multiply by 64 so as to avoid using floating point math
}

// gets the stored droop resistance, reported as a mOhm value
int MicroPanelH::getRDroop() {
  return (_rDroop*4316)/RDROOPFACTOR;
}

// gets the stored droop resisance in raw form
unsigned int MicroPanelH::getRDroopRaw() {
  return _rDroop;
}

// get the droop voltage as (droop resistance)*(output current)
int MicroPanelH::getVDroopRaw(int iOut) {
  return iOut*_rDroop/RDROOPFACTOR;
}

// Sensor Private Utility Functions ---------------------------------------

// returns the official VCC voltage in milliVolts
int MicroPanelH::readVCC() {
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
void MicroPanelH::setLED(int led, int state) {
  digitalWrite(led, state);
}

// sets yellow LED to HIGH or LOW
void MicroPanelH::setLED1(int state) {
  setLED(LED1_PIN, state);
}

// sets green LED to HIGH or LOW
void MicroPanelH::setLED2(int state) {
  setLED(LED2_PIN, state);
}

// Safety -----------------------------------------------------------------

// immediately triggers the all channels to shut down
void MicroPanelH::shutdownChannels() {
  pinMode(CH1_PIN, OUTPUT);
  pinMode(CH2_PIN, OUTPUT);
  pinMode(CH3_PIN, OUTPUT);
  pinMode(CH4_PIN, OUTPUT);
  digitalWrite(CH1_PIN, LOW);
  digitalWrite(CH2_PIN, LOW);
  digitalWrite(CH3_PIN, LOW);
  digitalWrite(CH4_PIN, LOW);
  delayMicroseconds(10000);
  pinMode(CH1_PIN, INPUT);
  pinMode(CH2_PIN, INPUT);
  pinMode(CH3_PIN, INPUT);
  pinMode(CH4_PIN, INPUT);
}

// checks if one or more channels are active
bool MicroPanelH::isSomeChannelsActive() {
  return getCh1() || getCh2() || getCh3() || getCh4();
}

// sets the terminal 1 current shutoff limit in mA, max 7500 mA
// setting the limit greater than 7500 mA will cause the current shutoff never to be triggered
void MicroPanelH::setCurrentLimit1(int current_mA) {
  long currentL = (long)current_mA;
  _currentLimitAmplitudeRaw1 = currentL*128/1875;
}

// sets the terminal 2 current shutoff limit in mA, max 7000 mA
// setting the limit greater than 7500 mA will cause the current shutoff never to be triggered
void MicroPanelH::setCurrentLimit2(int current_mA) {
  long currentL = (long)current_mA;
  _currentLimitAmplitudeRaw2 = currentL*128/1875;
}

// sets the terminal 3 current shutoff limit in mA, max 7500 mA
// setting the limit greater than 7500 mA will cause the current shutoff never to be triggered
void MicroPanelH::setCurrentLimit3(int current_mA) {
  long currentL = (long)current_mA;
  _currentLimitAmplitudeRaw3 = currentL*128/1875;
}

// sets the terminal 4 current shutoff limit in mA, max 7000 mA
// setting the limit greater than 7500 mA will cause the current shutoff never to be triggered
void MicroPanelH::setCurrentLimit4(int current_mA) {
  long currentL = (long)current_mA;
  _currentLimitAmplitudeRaw4 = currentL*128/1875;
}

// sets the total terminal current shutoff limit in mA
void MicroPanelH::setCurrentLimitTotal(int current_mA) {
  long currentL = (long)current_mA;
  _currentLimitAmplitudeRawTotal = currentL*128/1875;
}

// checks if last sensed current is greater than current limit
void MicroPanelH::checkCurrentShutdown() {
  // this function takes negligable microseconds unless actually shutting down
  if (_sensorAverages[I1_INDEX] > _currentLimitAmplitudeRaw1)
    setCh1(LOW);
  if (_sensorAverages[I2_INDEX] > _currentLimitAmplitudeRaw2)
    setCh2(LOW);
  if (_sensorAverages[I3_INDEX] > _currentLimitAmplitudeRaw3)
    setCh3(LOW);
  if (_sensorAverages[I4_INDEX] > _currentLimitAmplitudeRaw4)
    setCh4(LOW);
  if (_sensorAverages[I1_INDEX] + _sensorAverages[I2_INDEX] + 
    _sensorAverages[I3_INDEX] + _sensorAverages[I4_INDEX] > _currentLimitAmplitudeRawTotal) {
    setCh1(LOW);
    setCh2(LOW);
    setCh3(LOW);
    setCh4(LOW);
  }
}
// void MicroPanelH::checkCurrentShutdown() {
//   // this function takes negligable microseconds unless actually shutting down
//   if (_sensorAverages[I1_INDEX] > _currentLimitAmplitudeRaw1
//     || _sensorAverages[I1_INDEX] < -_currentLimitAmplitudeRaw1)
//     setCh1(LOW);
//   if (_sensorAverages[I2_INDEX] > _currentLimitAmplitudeRaw2
//     || _sensorAverages[I2_INDEX] < -_currentLimitAmplitudeRaw2)
//     setCh2(LOW);
//   if (_sensorAverages[I3_INDEX] > _currentLimitAmplitudeRaw3
//     || _sensorAverages[I3_INDEX] < -_currentLimitAmplitudeRaw3)
//     setCh3(LOW);
//   if (_sensorAverages[I4_INDEX] > _currentLimitAmplitudeRaw4
//     || _sensorAverages[I4_INDEX] < -_currentLimitAmplitudeRaw4)
//     setCh4(LOW);
//   if (_sensorAverages[I1_INDEX] + _sensorAverages[I2_INDEX] + 
//     _sensorAverages[I3_INDEX] + _sensorAverages[I4_INDEX] > _currentLimitAmplitudeRawTotal
//     || _sensorAverages[I1_INDEX] + _sensorAverages[I2_INDEX] + 
//     _sensorAverages[I3_INDEX] + _sensorAverages[I4_INDEX] < -_currentLimitAmplitudeRawTotal) {
//     setCh1(LOW);
//     setCh2(LOW);
//     setCh3(LOW);
//     setCh4(LOW);
//   }
// }

// Communications ------------------------------------------------------------

// process the parsed RX command, overrides base class virtual function
// MicroPanel readable registers: RVB, RI1, RI2, RI3, RI4, RIT, RVCC, RCH1, RCH2, RCH3, RCH4
// MicroPanel writable registers: WCH1, WCH2, WCH3, WCH4, WIL1, WIL2, WIL3, WIL4
void MicroPanelH::interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RVB") == 0) { // read bus voltage
    sprintf(getTXBuffer(receiveProtocol), "WVB:%u", getVBus());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RI1") == 0) { // read current at terminal 1
    sprintf(getTXBuffer(receiveProtocol), "WI1:%d", getI1());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RI2") == 0) { // read current at terminal 2
    sprintf(getTXBuffer(receiveProtocol), "WI2:%d", getI2());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RI3") == 0) { // read current at terminal 3
    sprintf(getTXBuffer(receiveProtocol), "WI3:%d", getI3());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RI4") == 0) { // read current at terminal 4
    sprintf(getTXBuffer(receiveProtocol), "WI4:%d", getI4());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RIT") == 0) { // read total current
    sprintf(getTXBuffer(receiveProtocol), "WIT:%d", getITotal());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RVCC") == 0) { // read the ~5V VCC bus voltage
    sprintf(getTXBuffer(receiveProtocol), "WVCC:%d", getVCC());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RCH1") == 0) { // read state of terminal 1
    sprintf(getTXBuffer(receiveProtocol), "WCH1:%d", getCh1());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RCH2") == 0) { // read state of terminal 2
    sprintf(getTXBuffer(receiveProtocol), "WCH2:%d", getCh2());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RCH3") == 0) { // read state of terminal 3
    sprintf(getTXBuffer(receiveProtocol), "WCH3:%d", getCh3());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RCH4") == 0) { // read state of terminal 4
    sprintf(getTXBuffer(receiveProtocol), "WCH4:%d", getCh4());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WCH1") == 0) { // write the desired terminal 1 state
    int temp = atoi(value);
    setCh1(temp);
    sprintf(getTXBuffer(receiveProtocol), "WCH1:=%d", getCh1());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WCH2") == 0) { // write the desired terminal 2 state
    int temp = atoi(value);
    setCh2(temp);
    sprintf(getTXBuffer(receiveProtocol), "WCH2:=%d", getCh2());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WCH3") == 0) { // write the desired terminal 3 state
    int temp = atoi(value);
    setCh3(temp);
    sprintf(getTXBuffer(receiveProtocol), "WCH3:=%d", getCh3());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WCH4") == 0) { // write the desired terminal 4 state
    int temp = atoi(value);
    setCh4(temp);
    sprintf(getTXBuffer(receiveProtocol), "WCH4:=%d", getCh4());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WIL1") == 0) { // write the terminal 1 current shutdown limit (mA)
    int temp = atoi(value);
    setCurrentLimit1(temp);
    sprintf(getTXBuffer(receiveProtocol), "WIL1:=%d", temp);
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WIL2") == 0) { // write the terminal 2 current shutdown limit (mA)
    int temp = atoi(value);
    setCurrentLimit2(temp);
    sprintf(getTXBuffer(receiveProtocol), "WIL2:=%d", temp);
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WIL3") == 0) { // write the terminal 3 current shutdown limit (mA)
    int temp = atoi(value);
    setCurrentLimit3(temp);
    sprintf(getTXBuffer(receiveProtocol), "WIL3:=%d", temp);
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WIL4") == 0) { // write the terminal 4 current shutdown limit (mA)
    int temp = atoi(value);
    setCurrentLimit4(temp);
    sprintf(getTXBuffer(receiveProtocol), "WIL4:=%d", temp);
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WILT") == 0) { // write the total terminal current shutdown limit (mA)
    int temp = atoi(value);
    setCurrentLimitTotal(temp);
    sprintf(getTXBuffer(receiveProtocol), "WILT:=%d", temp);
    respondToMaster(receiveProtocol);
  } else { // send command data to the callback listener functions, registered from primary .ino file
    for (int n = 0; n < _commandCallbacksEnd; n++) {
      _commandCallbacks[n](command, value, receiveProtocol);
    }
  }
}

