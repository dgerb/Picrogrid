/*
  MultiModeSupply

  A wide-input realtime multi-mode power supply with constant voltage output.
  Input: Terminal 1, 8-48V (compensator tuned for 20V input)
  Output: Terminal 2, adjustable 8-48V (15V default)
  
  Test Set-up:
  Input: Voltage supply attached to terminal 1, set to CV at 8-20V with 3A current limit
  Output: Electronic load attached to terminal 2, is set to CC mode between 0-2A

  This example builds on the PowerSupply example to add realtime multi-mode support. The converter operates in boost,
  buck-boost, or buck mode when the output voltage is below, in proximity, or above the input voltage, respectively.
  The reason for doing this is that buck and boost modes are nearly twice as efficient as buck-boost mode. We also
  constrain the duty cycle somewhat to assist start-up in boost and buck-boost modes and prevent input supply collapse
  under output load.

  Note that realtime multi-mode operation poses a very difficult controls problem, as detailed in:
  https://www.ti.com/lit/an/slyt765/slyt765.pdf
  This code can certainly use improvement; it is a work in progress. I would also like to try to extend realtime
  multi-mode operation to the constant-current examples, but that is much more difficult. Here we have fairly fixed values
  of VIN and VREF by which to compare the mode transition thresholds. With constant current, you must use vOut instead of VREF,
  which can cause all sorts of crazy instabilities since vOut is already being regulated by a control loop. As such, all the
  other examples only use fixed (compile-time) multi-mode operation.

  Created 8/28/23 by Daniel Gerber
*/

#include <AtverterH.h>

enum DCDCMode // multimode support is done through a finite state machine
{   BUCK = 0,
    BOOST, 
    BUCKBOOST,
    NUM_DCDCMODES
};
int dcdcMode = BOOST;

AtverterH atverter;
long slowInterruptCounter = 0;

int compIn [] = {0, 0, 0}; // must be equal or longer than compNum
int compOut [] = {0, 0, 0}; // must be equal or longer than compDen

// discrete compensator coefficients
// these values seem to work well enough for buck, boost, and buck-boost
// but you may want to have different compensators for each mode, depending on operating voltages
// (in particular, buck-boost mode has a pretty terrible response with these coefficients)
int compNum [] = {8, 0};
int compDen [] = {8, -8};

int gradDescCount = 0;

const int INSIZE = sizeof(compIn) / sizeof(compIn[0]);
const int OUTSIZE = sizeof(compOut) / sizeof(compOut[0]);
const int NUMSIZE = sizeof(compNum) / sizeof(compNum[0]);
const int DENSIZE = sizeof(compDen) / sizeof(compDen[0]);

int ILIM = 1024;
int iLimR = 0;

int VREF = 0; // reference output voltage setpoint
unsigned int RDroop32 = 0; // 32 times the raw droop resistance, used this way to avoid a division
// vin/vref thresholds for transitioning between DCDC Modes, multiplied by 10
// {buck-boost to boost, boost to buck-boost, buck to buck-boost, buck-boost to buck}
// thresholds are designed to give some hysteresis between transitions
int threshold10 [] = {8, 9, 11, 12};

// the setup function runs once when you press reset or power the board
void setup() {
  atverter.setupPinMode(); // set pins to input or output
  atverter.initializeSensors(); // set filtered sensor values to initial reading
  atverter.setCurrentShutdown(6000); // set gate shutdown at 6A peak current 
  atverter.setThermalShutdown(60); // set gate shutdown at 60°C temperature
  atverter.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms

  // set up UART command support
  atverter.addCommandCallback(&interpretRXCommand);
  atverter.startUART();

  // better to set VREF before starting the PWM
  setVREF(15000); // based on VCC; make sure Atverter is powered from side 1 input when this line runs
  atverter.startPWM();
}

void loop() { } // we don't use loop() because it does not loop at a fixed period

// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  atverter.readUART(); // if using UART, check every cycle if there are new characters in the UART buffer
  atverter.updateVISensors(); // read voltage and current sensors and update moving average
  atverter.checkCurrentShutdown(); // checks average current and shut down gates if necessary
  atverter.checkBootstrapRefresh(); // refresh bootstrap capacitors on a timer

  int vOut = atverter.getRawV2();
  int iOut = -1*(atverter.getRawI2() - 512);

  // determine the current-limit resistance based on the output current
  if (iOut > ILIM) // if greater than current limit rapidly increase limit resistance
    iLimR = iLimR + 50;
  else if (iOut > ILIM*15/16) // if near current limit, slowly increase limit resistance
    iLimR = iLimR + 1;
  else // slowly decrease current limit resistance until steady state reached
    iLimR = constrain(iLimR - 1, 0, iLimR);

  // the drooped reference voltage is calculated based on the nominal reference voltage, droop resistance,
  //  and the secondary current limiter droop resistance
  int droopVREF = VREF - iOut*RDroop32/32 + iOut*iLimR;

  // error = drooped reference - output voltage
  int error = droopVREF - vOut;

  // update array of past compensator inputs
  for (int n = INSIZE - 1; n > 0; n--) {
    compIn[n] = compIn[n-1];
  }
  compIn[0] = error;
  // update array of past compensator outputs
  for (int n = OUTSIZE - 1; n > 0; n--) {
    compOut[n] = compOut[n-1];
  }

  // 0.5A-5A output: classical feedback voltage mode discrete compensation
  // 0A-0.5A output: slow gradient descent mode
  bool isClassicalFB = atverter.getRawI2() < 512 - 51 || atverter.getRawI2() > 512 + 51;

  if(isClassicalFB) { // classical feedback voltage mode discrete compensation
    // accumulate compensator weighted input terms
    long compAcc = 0;
    for (int n = 0; n < NUMSIZE; n++) {
      compAcc = compAcc + compIn[n]*compNum[n];
    }
    // accumulate compensator weighted past output terms
    for (int n = 1; n < DENSIZE; n++) {
      compAcc = compAcc - compOut[n]*compDen[n];
    }
    compAcc = compAcc/compDen[0];
    compOut[0] = compAcc;
    // duty cycle (0-100) = compensator output * 100% / 2^10
    int duty = (compAcc*100)/1024;
    duty = constrainDuty(duty);
    atverter.setDutyCycle(duty);
  } else { // slow gradient descent mode, avoids light-load instability
    gradDescCount++; // use a counter to control the rate of gradient descent
    if (gradDescCount > 4) {
      long duty = atverter.getDutyCycle();
      duty = constrainDuty(duty);
      compOut[0] = duty*1024/100;
      if (error > 0) { // ascend or descend by 1% duty cycle depending on error
        atverter.setDutyCycle(duty + 1);
      } else {
        atverter.setDutyCycle(duty - 1);
      }
      gradDescCount = 0;
    }
  }

  // dc-dc mode finite state machine (switch between buck, boost, and buck-boost)
  int vIn = atverter.getRawV1();
  int vIn10 = 10*vIn; // multiply by 10 before comparing so as to avoid floating point calcs
  switch(dcdcMode) {
    case BOOST:
      if (vIn10 > threshold10[1]*VREF)
        changeDCDCMode(BUCKBOOST, vIn, VREF);
      break;
    case BUCKBOOST:
      if (vIn10 > threshold10[3]*VREF)
        changeDCDCMode(BUCK, vIn, VREF);
      else if (vIn10 < threshold10[0]*VREF)
        changeDCDCMode(BOOST, vIn, VREF);
      break;
    case BUCK:
      if (vIn10 < threshold10[2]*VREF)
        changeDCDCMode(BUCKBOOST, vIn, VREF);
      break;
  }

  slowInterruptCounter++;
  if (slowInterruptCounter > 3000) { // timer set such that each count is 1 ms
    slowInterruptCounter = 0;
    atverter.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverter.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverter.checkThermalShutdown(); // checks average temperature and shut down gates if necessary
  }
}

// switches the FSM state, changes converter mode,
// updates the duty cycle to an appropriate estimate, and resets compensator past values
void changeDCDCMode(int mode, int vin, int vOut) {
  int duty;
  switch(mode) { // duty cycle on page 3 of: https://www.ti.com/lit/an/slyt765/slyt765.pdf
    case BUCK:
      dcdcMode = BUCK;
      atverter.applyHoldHigh2(); // hold side 2 high for a buck converter with side 1 input
      duty = 100*(long)vOut/vin;
      atverter.setDutyCycle(duty);
      break;
    case BOOST:
      dcdcMode = BOOST;
      atverter.applyHoldHigh1(); // hold side 1 high for a boost converter with side 1 input
      duty = 100*(long)(vOut - vin)/vOut;
      atverter.setDutyCycle(duty);
      break;
    case BUCKBOOST:
      dcdcMode = BUCKBOOST;
      atverter.removeHold();
      duty = 100*(long)vOut/(vin + vOut);
      atverter.setDutyCycle(duty);
      break;
  }
  // reset array of past compensator inputs
  for (int n = 0; n < INSIZE; n++) {
    compIn[n] = 0;
  }
  // reset array of past compensator outputs
  for (int n = 0; n < OUTSIZE; n++) {
    compOut[n] = duty*10; // duty*1024/100
  }
}

// sets the reference output voltage and updates to the appropriate DCDC converter mode
void setVREF(unsigned int vRefMv) {
  VREF = atverter.mV2raw(vRefMv);
  int vIn = atverter.getRawV1();
  int vIn10 = 10*vIn;
  if (vIn10 < threshold10[1]*VREF)
    changeDCDCMode(BOOST, vIn, VREF);
  else if (vIn10 < threshold10[2]*VREF)
    changeDCDCMode(BUCKBOOST, vIn, VREF);
  else
    changeDCDCMode(BUCK, vIn, VREF);
}

// constrains the duty cycle to reasonable values, which can sometimes help prevent the input
// supply from collapsing when it starts up 
int constrainDuty(int duty) {
  switch (dcdcMode) {
    case BOOST:
      duty = constrain(duty, 1, 90);
      break;
    case BUCKBOOST:
      duty = constrain(duty, 10, 90);
      break;
    case BUCK:
      duty = constrain(duty, 10, 99);
      break;
  }
  return duty;
}

// serial command interpretation function
void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "WVREF") == 0) {
    writeVREF(value, receiveProtocol);
  } else if (strcmp(command, "RVREF") == 0) {
    readVREF(value, receiveProtocol);
  } else if (strcmp(command, "WRDRP") == 0) {
    writeDroopRes(value, receiveProtocol);
  } else if (strcmp(command, "RRDRP") == 0) {
    readDroopRes(value, receiveProtocol);
  } else if (strcmp(command, "WILIM") == 0) {
    writeILIM(value, receiveProtocol);
  } else if (strcmp(command, "RILIM") == 0) {
    readILIM(value, receiveProtocol);
  }
}

// sets the reference output voltage (mV) set point from a serial command
void writeVREF(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atoi(valueStr);
  setVREF(temp);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WVREF:=%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// gets the reference output voltage (mV) set point and outputs to serial
void readVREF(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atverter.raw2mV(VREF);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WVREF:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// sets the droop resistance (milli-ohms) from a serial command
// minimum is 135 milli-ohms
void writeDroopRes(const char* valueStr, int receiveProtocol) {
  int temp = atoi(valueStr);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WRDRP:=%d", temp);
  // ohmraw = mV/mA * A mA/rawI / (B mV/rawV) = A/B * ohm = A/(1000*B) mohm
  //  = (mohm / 1000) * (atverter.mA2raw(1000) - 512) / (atverter.mV2raw(1000))
  //  = (mohm / 1000) * (vcc/341) / (vcc/79)
  //  = mohm / 1000 * 79 / 341
  temp = 32*temp/4316;
  RDroop32 = temp;
  atverter.respondToMaster(receiveProtocol);
}

// gets the droop resistance (milli-ohms) and outputs to serial
void readDroopRes(const char* valueStr, int receiveProtocol) {
  long temp = RDroop32*4316/32;
  sprintf(atverter.getTXBuffer(receiveProtocol), "WRDRP:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// sets the current limit (mA) from a serial command
void writeILIM(const char* valueStr, int receiveProtocol) {
  int temp = atoi(valueStr);
  ILIM = atverter.mA2rawSigned(temp);
}

// gets the current limit (mA) and outputs to serial
void readILIM(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atverter.rawSigned2mA(ILIM);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WILIM:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}
