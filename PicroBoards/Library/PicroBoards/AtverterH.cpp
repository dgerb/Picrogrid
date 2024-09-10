/*
  AtverterH.cpp - Library for AtverterH pin and function mapping
  Created by Daniel Gerber, 9/19/22
  Released into the public domain.
*/

#include "AtverterH.h"

AtverterH::AtverterH() {
}

// default initialization routine
void AtverterH::initialize() {
  setupPinMode();
  shutdownGates();
  initializeSensors();
  setCurrentShutdown1(6500); // set default current shutdown above 5A plus ripple
  setCurrentShutdown2(6500); // set default current shutdown above 5A plus ripple
  setThermalShutdown(80); // set thermal shutdown decently high
}

// sets up the pin mode for all AtverterH pins
void AtverterH::setupPinMode() {
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
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

// // initialize sensor average array to sensor read
void AtverterH::initializeSensors() {
  updateVCC();
  int max = 0;
  for (int n = 0; n < NUM_SENSORS; n++)
    if (AVERAGE_WINDOW_MAX[n] > max)
      max = AVERAGE_WINDOW_MAX[n];
  for (int n = 0; n < max; n++) {
    updateVISensors();
    updateTSensors();
  }
}

// initializes the periodic control timer
// inputs: control period in microseconds, controller interrupt function reference
// example usage: atverterH.initializeInterruptTimer(1, &controlUpdate);
void AtverterH::initializeInterruptTimer(long periodus, void (*interruptFunction)(void)) {
  Timer1.initialize(periodus); // arg: period in microseconds
  Timer1.attachInterrupt(interruptFunction); // arg: interrupt function to call
  // _bootstrapCounterMax = 10000/periodus; // refresh bootstrap capacitors every 10ms
  _bootstrapCounterMax = 1000/periodus; // refresh bootstrap capacitors every 10ms
  if (_bootstrapCounterMax < 1)
    _bootstrapCounterMax = 1;
  refreshBootstrap();
}

// resets protection latch, enabling the gate drivers
void AtverterH::enableGateDrivers(int holdProtectMicroseconds) {
  digitalWrite(PRORESET_PIN, HIGH);
  delayMicroseconds(holdProtectMicroseconds);
  digitalWrite(PRORESET_PIN, LOW);
  _shutdownCode = 0; // reset shutdown code (i.e. set to "hardware" shutdown)
}

// resets protection latch, enabling the gate drivers
void AtverterH::enableGateDrivers() {
  enableGateDrivers(3000);
}


// sets initial duty cycle and enables gate drivers
// we recommend you use this in the setup function; ensures you set duty properly before enabling
void AtverterH::startPWM(int initialDuty) {
  setDutyCycle(initialDuty);
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
// note that the duty cycle is referenced to side 1; side 2 duty = 100 - getDutyCycle()
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
  if (_bootstrapCounter <= 0) {
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
  digitalWrite(VCTRL2_PIN, LOW);
  digitalWrite(ALT_PIN, HIGH);
  refreshBootstrap();
}

// set gate driver 2 to use an always-high alternate signal
void AtverterH::applyHoldHigh2() {
  digitalWrite(VCTRL2_PIN, HIGH);
  digitalWrite(VCTRL1_PIN, LOW);
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
  if (_vcc < 4950) { // readVCC() might measure ~4500 mV if connected via USB
    _vcc = 5000; // to avoid incorrect USB VCC, set to approximate supply output voltage 
  }
}

void AtverterH::updateTSensors() {
  updateSensorRaw(T1_INDEX, analogReadFast(T1_PIN));
  updateSensorRaw(T2_INDEX, analogReadFast(T2_PIN));
}

void AtverterH::updateVISensors() {
  // analogRead() measured at 116 microseconds, updateSensorRaw adds negligable time
  // total updateVISensors time is measured at 456 microseconds
  updateSensorRaw(V1_INDEX, analogReadFast(V1_PIN));
  updateSensorRaw(V2_INDEX, analogReadFast(V2_PIN));
  updateSensorRaw(I1_INDEX, analogReadFast(I1_PIN) - 512);
  updateSensorRaw(I2_INDEX, analogReadFast(I2_PIN) - 512);
}

void AtverterH::updateSensorRaw(int index, int sample) {
  // find correct sensorPast array
  int * sensorPast;
  switch (index) {
    case V1_INDEX:
      sensorPast = _sensorPastV1;
      break;
    case V2_INDEX:
      sensorPast = _sensorPastV2;
      break;
    case I1_INDEX:
      sensorPast = _sensorPastI1;
      break;
    case I2_INDEX:
      sensorPast = _sensorPastI2;
      break;
    case T1_INDEX:
      sensorPast = _sensorPastT1;
      break;
    case T2_INDEX:
      sensorPast = _sensorPastT2;
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

// converts a raw 10-bit analog reading centered on zero (-512 to 512) to a mA reading (-5000 to 5000)
//  for MT9221CT-06BR5 current sensor with VCC=5V, sensitivity is 333mV/A, 0A at 2.5V
//  with variable VCC, sensitivity (mV/mA) is VCC/5000*333/1000, 0A at VCC/2
int AtverterH::raw2mA(int raw) {
  long rawL = (long)raw;
  // (analogRead-512) * VCC/1024 * 1/sensitivity = (analogRead-512) * VCC/1024 * 1000/333
  return rawL*getVCC()*3/1024;
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

// converts a mA value (-5000 to 5000) to raw 10-bit form centered around 0 (-512 to 512)
int AtverterH::mA2raw(int mA) {
  // mA * sensitivity * 1024/VCC = mA * 333/1000 * 1024/VCC
  long temp = (long)mA*341/getVCC();
  return (int)temp;
}

// Droop Resistance Conversions -------------------------------------------

// sets the stored droop resistance
void AtverterH::setRDroop(int mOhm) {
  // ohmraw = mV/mA * A mA/rawI / (B mV/rawV) = A/B * ohm = A/(1000*B) mohm
  //  = (mohm / 1000) * (atverter.mA2raw(1000)) / (atverter.mV2raw(1000))
  //  = (mohm / 1000) * (vcc/341) / (vcc/79)
  //  = mohm / 1000 * 79 / 341
  _rDroop = (long)RDROOPFACTOR*mOhm/4316; // here we multiply by 64 so as to avoid using floating point math
}

// gets the stored droop resistance, reported as a mOhm value
int AtverterH::getRDroop() {
  return _rDroop*4316/RDROOPFACTOR;
}

// gets the stored droop resisance in raw form
unsigned int AtverterH::getRDroopRaw() {
  return _rDroop;
}

// get the droop voltage as (droop resistance)*(output current)
int AtverterH::getVDroopRaw(int iOut) {
  return iOut*_rDroop/RDROOPFACTOR;
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
void AtverterH::setLED1(int state) {
  setLED(LED1_PIN, state);
}

// sets green LED to HIGH or LOW
void AtverterH::setLED2(int state) {
  setLED(LED2_PIN, state);
}

// Safety -----------------------------------------------------------------

// immediately triggers the gate shutdown
void AtverterH::shutdownGates() {
  shutdownGates(SOFTWAREUNLABELED);
}

// immediately triggers the gate shutdown
void AtverterH::shutdownGates(int shutdownCode) {
  _shutdownCode = shutdownCode;
  pinMode(GATESD_PIN, OUTPUT);
  digitalWrite(GATESD_PIN, LOW);
  delayMicroseconds(10000);
  // digitalWrite(GATESD_PIN, HIGH);
  pinMode(GATESD_PIN, INPUT);
}

// returns true if the gate shutdown signal is currently latched
bool AtverterH::isGateShutdown() {
  return !digitalRead(GATESD_PIN);
}

// returns the appropriate shutdown code, or -1 if gates not shutdown
int AtverterH::getShutdownCode() {
  if (!isGateShutdown())
    return -1;
  else
    return _shutdownCode;
}

// sets the terminal 1 current shutoff limit in mA, max 7500 mA
// setting the limit greater than 7500 mA will cause the current shutoff never to be triggered
void AtverterH::setCurrentShutdown1(int current_mA) {
  long currentL = (long)current_mA;
  _currentLimitAmplitudeRaw1 = currentL*128/1875;
}

// sets the terminal 2 current shutoff limit in mA, max 7000 mA
// setting the limit greater than 7500 mA will cause the current shutoff never to be triggered
void AtverterH::setCurrentShutdown2(int current_mA) {
  long currentL = (long)current_mA;
  _currentLimitAmplitudeRaw2 = currentL*128/1875;
}

// checks if last sensed current is greater than current limit
void AtverterH::checkCurrentShutdown() {
  // this function takes negligable microseconds unless actually shutting down
  if (_sensorAverages[I1_INDEX] > _currentLimitAmplitudeRaw1
    || _sensorAverages[I1_INDEX] < -_currentLimitAmplitudeRaw1
    || _sensorAverages[I2_INDEX] > _currentLimitAmplitudeRaw2
    || _sensorAverages[I2_INDEX] < -_currentLimitAmplitudeRaw2)
    shutdownGates(OVERCURRENT);
}

// sets the upper temperature shutoff in °C
void AtverterH::setThermalShutdown(int temperature_C) {
  _thermalLimitC = temperature_C;
}

// checks if last sensed current is greater than thermal limit
void AtverterH::checkThermalShutdown() {
  if (getT1() > _thermalLimitC || getT2() > _thermalLimitC)
    shutdownGates(OVERTEMPERATURE);
}

// Compensation for Classical Feedback ---------------------------------------

// set discrete compensator coefficients
// must specify size of numerator and denominator due to how array pointers are passed to functions
void AtverterH::setComp(int num[], int den[], int numSize, int denSize) {
  _compNum = num;
  _compDen = den;
  _compNumSize = numSize;
  _compDenSize = denSize;
}

// update past compensator inputs and outputs
// must do this even if using gradient descent for smooth transition to classical feedback
void AtverterH::updateCompPast(int inputNow) {
// update array of past compensator inputs
  for (int n = _compNumSize - 1; n > 0; n--) {
    _compIn[n] = _compIn[n-1];
  }
  _compIn[0] = inputNow; // set the current compensator input
  // update array of past compensator outputs
  for (int n = _compDenSize - 1; n > 0; n--) {
    _compOut[n] = _compOut[n-1];
  }
}

// returns compensator output for classical feedback discrete compensation
// usage: compNum = {A, B, C}, compDen = {D, E, F}, x = compIn, y = compOut
//   D*y[n] + E*y[n-1] + F*y[n-2] = A*x[n] + B*x[n-1] + C*x[n-2]
//   y[n] = (A*x[n] + B*x[n-1] + C*x[n-2] - E*y[n-1] - F*y[n-2])/D
long AtverterH::calculateCompOut() {
  // accumulate compensator weighted input terms via addition
  long compAcc = 0;
  for (int n = 0; n < _compNumSize; n++) {
    compAcc = compAcc + _compIn[n]*_compNum[n];
  }
  // accumulate compensator weighted past output terms via subtraction
  for (int n = 1; n < _compDenSize; n++) {
    compAcc = compAcc - _compOut[n]*_compDen[n];
  }
  compAcc = compAcc/_compDen[0]; // divide by the first denominator coefficient
  _compOut[0] = compAcc; // the compensator output, y[n]
  return compAcc;
}

// resets the compensator past values when switching between CV and CC
void AtverterH::resetComp() {
  // reset array of past compensator inputs
  for (int n = 0; n < _compNumSize; n++) {
    _compIn[n] = 0;
  }
  // reset array of past compensator outputs
  for (int n = 0; n < _compDenSize; n++) {
    _compOut[n] = getDutyCycle()*10; // backward convert dutyRaw = duty*1024/100 = duty*10
  }
}

// Gradient Descent ----------------------------------------------------------

// set the gradient descent counter overflow to control step speed
void AtverterH::setGradDescCountMax(int settlingCount, int averagingCount) {
  _gradDescSettleMax = settlingCount;
  _gradDescAverageMax = averagingCount;
  _gradDescCount = 0;
}

// set gradient descent to step next call to gradDescStep(), using the given error as only error
void AtverterH::triggerGradDescStep() {
  _gradDescCount = _gradDescSettleMax + _gradDescAverageMax;
  _gradDescErrorAcc = 0;
}

// steps duty cycle based on the sign of the error
void AtverterH::gradDescStep(int error) {
  _gradDescCount++; // use a counter to control the speed of gradient descent
  if (_gradDescCount < _gradDescSettleMax)
    return;
  _gradDescErrorAcc = _gradDescErrorAcc + error;
  if (_gradDescCount > _gradDescSettleMax + _gradDescAverageMax) {
    // store the duty cycle value to compensator output array in case we switch to classical feedback
    long duty = getDutyCycle();
    _compOut[0] = (int)(duty*1024/100);
    // reset counter, calculate and process average error
    _gradDescCount = 0;
    int avgError = _gradDescErrorAcc/_gradDescAverageMax;
    _gradDescErrorAcc = 0;
    if (avgError > 0) { // ascend or descend by 1% duty cycle depending on the sign of the error
      setDutyCycle((int)(duty + 1));
    } else if (error < 0) {
      setDutyCycle((int)(duty - 1));
    }
  }
}

// Communications ------------------------------------------------------------

// process the parsed RX command, overrides base class virtual function
// Atverter readable registers: RV1, RV2, RI1, RI2, RT1, RT2, RVCC, RDUT
// Atverter writable registers: WISD, WTSD
void AtverterH::interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RV1") == 0) { // read voltage at terminal 1
    sprintf(getTXBuffer(receiveProtocol), "WV1:%u", getV1());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RV2") == 0) { // read voltage at terminal 2
    sprintf(getTXBuffer(receiveProtocol), "WV2:%u", getV2());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RI1") == 0) { // read current at terminal 1
    sprintf(getTXBuffer(receiveProtocol), "WI1:%d", getI1());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RI2") == 0) { // read current at terminal 2
    sprintf(getTXBuffer(receiveProtocol), "WI2:%d", getI2());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RT1") == 0) { // read FET temperature of side 1
    sprintf(getTXBuffer(receiveProtocol), "WT1:%d", getT2());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RT2") == 0) { // read FET temperature of side 1
    sprintf(getTXBuffer(receiveProtocol), "WT2:%d", getT2());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RVCC") == 0) { // read the ~5V VCC bus voltage
    sprintf(getTXBuffer(receiveProtocol), "WVCC:%d", getVCC());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RDUT") == 0) { // read the duty cycle
    sprintf(getTXBuffer(receiveProtocol), "WDUT:%d", getDutyCycle());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RDRP") == 0) { // read the stored droop resistance
    sprintf(getTXBuffer(receiveProtocol), "WDRP:%d", getRDroop());
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WIS1") == 0) { // write the terminal 1 current shutdown limit (mA)
    int temp = atoi(value);
    setCurrentShutdown1(temp);
    sprintf(getTXBuffer(receiveProtocol), "WIS1:=%d", temp);
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WIS2") == 0) { // write the terminal 2 current shutdown limit (mA)
    int temp = atoi(value);
    setCurrentShutdown2(temp);
    sprintf(getTXBuffer(receiveProtocol), "WIS2:=%d", temp);
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WTSD") == 0) { // write the thermal shutdown limit (°C)
    int temp = atoi(value);
    setThermalShutdown(temp);
    sprintf(getTXBuffer(receiveProtocol), "WTSD:=%d", temp);
    respondToMaster(receiveProtocol);
  } else if (strcmp(command, "WDRP") == 0) { // set the stored droop resistance
    int temp = atoi(value);
    setRDroop(temp);
    sprintf(getTXBuffer(receiveProtocol), "WDRP:=%d", temp);
    respondToMaster(receiveProtocol);
  } else { // send command data to the callback listener functions, registered from primary .ino file
    for (int n = 0; n < _commandCallbacksEnd; n++) {
      _commandCallbacks[n](command, value, receiveProtocol);
    }
  }
}

