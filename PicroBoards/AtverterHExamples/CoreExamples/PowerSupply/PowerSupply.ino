
/*
  PowerSupply

  A power supply with constant voltage and constant current outputs when voltage and current limited, respectively.
  Input: Terminal 1, 8-48V (compensator tuned for 20V input)
  Output: Terminal 2, adjustable 8-48V (15V default)
  
  Test Set-up:
  Input: Voltage supply attached to terminal 1, set to CV at 8-20V with 3A current limit
  Output: Electronic load attached to terminal 2, is set to CR mode between 8 and 15 ohms

  This example builds on the ConstantVoltageBuck example and adds several features. First, it allows the user to
  select a DC-DC conversion mode, between buck, boost, and buck-boost operation. This feature is set by the line
  const int DCDCMODE.

  The power supply allows an optional current limit, which functions as it would in a standard power supply. If the
  output current exceeds the current limit, the supply will droop/collapse the output voltage until the current limit
  is met. This function can also be used to configure the power supply in constant current mode; set the voltage limit
  to 60000 mV, and control the output current exclusively. In this way, the Atverter can be used as an LED driver.

  The power supply also demonstrates the use of droop control to allow for current sharing between identical supplies
  in parallel. This method droops the reference voltage proportionally to the output current. If one supply outputs
  too much current, its output voltage will droop, allowing the other supply to catch up. Droop control leverages
  an adjustable droop resistance, the proportionality factor between voltage droop and output current.

  The power supply has current limit functionality, which uses a secondary droop resistance term to droop the 
  reference voltage when the current limit has been hit. This effectively functions to collapse the voltage output
  until the output current is lower than the current limit. The current limiter uses a gradient-descent based algorithm
  to increase the resistance, however this can sometimes introduce oscillations in a barely-stable system. In addition,
  collapsing the supply lower than the input voltage in boost mode is not yet supported.

  Finally, this example also adds support for adjusting the voltage limit (WVLIM, RVLIM), current limit (WILIM, RILIM),
  and the droop resistance (WDRP, RDRP) via serial commands of the form <REG:VALUE>.

  TODO: undervoltage lockout

  Created 8/29/23 by Daniel Gerber
*/

#include <AtverterH.h>
AtverterH atverter;

const int DCDCMODE = BUCK; // BUCK, BOOST, BUCKBOOST
const int VLIMDEFAULT = 20000; // default voltage limit setting in mV
const int ILIMDEFAULT = 2500; // default current limit setting in mA

// discrete compensator coefficients
// you may want to customize this for your specific input/output voltage/current operating points

// uncomment for BUCKBOOST:
// int compNum [] = {2, 0};
// int compDen [] = {8, -8};

// // uncomment for BUCK or BOOST:
int compNum [] = {2, 0};
int compDen [] = {8, -8};

int vLim = 0; // reference output voltage setpoint (raw 0-1023)
int iLim = 1024; // current limit (raw 0-1023)
int outputMode = CV2; // constant voltage (CV2) or constant current (CC2) mode finite state machine (on port 2)

long slowInterruptCounter = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  atverter.setupPinMode(); // set pins to input or output
  atverter.initializeSensors(); // set filtered sensor values to initial reading
  atverter.setCurrentShutdown(6000); // set gate shutdown at 6A peak current 
  atverter.setThermalShutdown(60); // set gate shutdown at 60°C temperature

  // set up UART command support
  atverter.addCommandCallback(&interpretRXCommand);
  atverter.startUART();
  ReceiveEventI2C receiveEvent = [] (int howMany) {atverter.receiveEventI2C(howMany);};
  RequestEventI2C requestEvent = [] () {atverter.requestEventI2C();};
  atverter.startI2C(3, receiveEvent, requestEvent); // first argument is the slave device address (max 127)

  // set discrete compensator coefficients for use in classical feedback compensation
  atverter.setComp(compNum, compDen, sizeof(compNum)/sizeof(compNum[0]), sizeof(compDen)/sizeof(compDen[0]));

  // initialize voltage and current limits to default values above
  vLim = atverter.mV2raw(VLIMDEFAULT); // based on VCC; make sure Atverter is powered from side 1 input when this line runs
  iLim = atverter.mA2raw(ILIMDEFAULT);
  // apply holds on either side of the Atverter based on DC-DC operation mode
  switch(DCDCMODE) {
    case BUCK:
      atverter.applyHoldHigh2(); // hold side 2 high for a buck converter with side 1 input
      break;
    case BOOST:
      atverter.applyHoldHigh1(); // hold side 1 high for a boost converter with side 1 input
      break;
    case BUCKBOOST:
      atverter.removeHold(); // removes holds; both sides will be switching
      break;
  }

  atverter.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms
  atverter.startPWM(); // once all is said and done, start the PWM
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
  int iOut = -1*(atverter.getRawI2());
    // multiply by -1, since positive current is technically defined as current into the terminal
    // but here, current out of the terminal is easier to work with and understand

  // check conditions to switch between constant voltage and constant current states
  if (outputMode == CV2 && iOut > iLim) { // switch to constant current if output current exceeds current limit
    outputMode = CC2;
    atverter.resetComp(); // reset compensator past inputs and outputs since not relevant to CC mode
  } else if (outputMode == CC2 && vOut > vLim) { // switch to constant voltage if output voltage exceeds voltage limit
    outputMode = CV2;
    atverter.resetComp(); // reset compensator past inputs and outputs since not relevant to CV mode
  }

  int error;
  if (outputMode == CC2) { // constant current operation
    error = iLim - iOut; // error is difference between current limit and output current
  } else { // constant voltage operation
    // if using droop control, we droop the reference voltage by a term proportional to the output current.
    // error = (reference voltage - droop voltage) - output voltage
    error = (vLim - atverter.getVDroopRaw(iOut)) - vOut;
  }

  // update array of past compensator inputs
  atverter.updateCompPast(error); // argument is the compensator input right now

  // 0.5A-5A output: classical feedback voltage mode discrete compensation
  // 0A-0.5A output: slow gradient descent mode
  bool isClassicalFB = atverter.getRawI2() < -51 || atverter.getRawI2() > 51;
  // bool isClassicalFB = atverter.getRawI2() < -51 || atverter.getRawI2() > 51;

  if(isClassicalFB) { // classical feedback voltage mode discrete compensation
    // calculate the compensator output based on past values and the numerator and demoninator
    // duty cycle (0-100) = compensator output * 100% / 2^10
    int duty = (atverter.calculateCompOut()*100)/1024;
    atverter.setDutyCycle(duty);
  } else { // slow gradient descent mode, avoids light-load instability
    atverter.gradDescStep(error); // steps duty cycle up or down depending on the sign of the error
  }

  slowInterruptCounter++;
  if (slowInterruptCounter > 3000) { // timer set such that each count is 1 ms
    slowInterruptCounter = 0;
    atverter.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverter.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverter.checkThermalShutdown(); // checks average temperature and shut down gates if necessary
  }
}

// serial command interpretation function
void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RFN") == 0) {
    readFileName(value, receiveProtocol);
  } else if (strcmp(command, "WVLIM") == 0) {
    writeVLIM(value, receiveProtocol);
  } else if (strcmp(command, "RVLIM") == 0) {
    readVLIM(value, receiveProtocol);
  } else if (strcmp(command, "WILIM") == 0) {
    writeILIM(value, receiveProtocol);
  } else if (strcmp(command, "RILIM") == 0) {
    readILIM(value, receiveProtocol);
  }
}

// outputs the file name to serial
void readFileName(const char* valueStr, int receiveProtocol) {
  sprintf(atverter.getTXBuffer(receiveProtocol), "WFN:%s", "PowerSupply.ino");
  atverter.respondToMaster(receiveProtocol);
}

// sets the reference output voltage (mV) set point from a serial command
void writeVLIM(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atoi(valueStr);
  vLim = atverter.mV2raw(temp);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WVLIM:=%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// gets the reference output voltage (mV) set point and outputs to serial
void readVLIM(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atverter.raw2mV(vLim);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WVLIM:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// sets the current limit (mA) from a serial command
void writeILIM(const char* valueStr, int receiveProtocol) {
  int temp = atoi(valueStr);
  iLim = atverter.mA2raw(temp); // convert the mA value to a raw 0-1023 form
  sprintf(atverter.getTXBuffer(receiveProtocol), "WILIM:=%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// gets the current limit (mA) and outputs to serial
void readILIM(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atverter.raw2mA(iLim);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WILIM:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}
