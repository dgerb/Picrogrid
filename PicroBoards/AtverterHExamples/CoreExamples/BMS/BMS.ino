/*
  BMS

  WARNING: Never use unprotected batteries... ever. You will burn down your home.

  A step-down battery management system that can charge or discharge in grid forming or following modes. Includes a
  coulumb-counting algorithm for state of charge estimation.
  DC Bus: Terminal 1: 24V nominal (22-28V allowed), or 48V nominal (44-56V allowed)
  Battery: Terminal 2: 12V (10-14V), 24V (20-28V), 36V (30-42V)
  
  Test Set-up (grid-following):
  DC Bus: Voltage supply attached to terminal 1, set to CV at 24V with 3A current limit
  DC Bus: Electronic load attached to terminal 1, is set to CR mode between 15 and 100 ohms
  Battery: Attached to terminal 2 through an Ammeter

  This battery management system uses a custom algorithm that depends on battery voltage in its decision making. While
  state-of-charge based algorithms are more accurate and exhibit higher performance, this algorithm seems robust and 
  does not rely on properly resetting the SOC coulomb counter. There are four control limits that are used
  in the feedback control algorithm:
  (a) Maximum allowable battery voltage (vBatMax)
  (b) Minimum allowable battery voltage (vBatMin)
  (c) Charging current limit (iBatChgLim)
  (d) Discharging current limit (iBatDisLim)

  Grid Following Modes (Charge and Discharge):
  During grid following operation, we charge the battery via constant current control at the reference charging
  current limit. As the battery voltage approaches the max limit, the charging current will decrease until eventually,
  the controller eventually switches to constant voltage mode. It is known that the battery internal resistance causes
  the measured battery voltage to be higher while charging, depending on the current. To compensate for this phenomenom,
  we reduce the reference charging current (iBatChgRef) linearly such that:
  - At vBat = 0.75*vBatMax, iBatChgRef = iBatChgLim
  - At vBat = vBatMax, iBatChgRef = 0
  Such linear interpolation and control is done similarly for battery discharging and the minimum battery current.

  Grid Forming Modes:
  Grid forming mode also uses the four control limits and interpolation to ensure the battery is not abused. When the
  battery voltage and current are within the limits, the controller will instead regulate the terminal 1 bus voltage
  as best as it can. With proper droop resistance (not included in this example), one could conceivably attach
  multiple grid forming BMS Atverters to co-regulate the same DC bus. One challenge of grid forming is that on a cold
  start, the bus voltage is near zero, while the battery voltage is fixed at a higher value. Thus we must operate the
  Atverter in boost mode instead of buck so as to jump start the bus voltage before converting to buck mode. I honestly
  have no idea what's the proper way to do this, but I was able to hard code a cold-start routine that seems to work.

  Created 10/31/23 by Daniel Gerber
*/

#include <AtverterH.h>
AtverterH atverter;

enum BMSModes
{   FOLLOWCHARGE = 0, // charge the battery in grid following mode
    FOLLOWDISCHARGE = 1, // discharge the battery in grid following mode
    FORM = 2, // have the battery grid from, i.e. attempt to regulate a DC bus
    HOLD = 3, // have the battery hold, i.e. will not charge or discharge
    FORMCOLDSTART = 4, // if the bus voltage is less than the battery voltage, must do a cold start for grid forming
    NUM_BMSMODES
};

// specify the absolute max battery values, from battery datasheet
const unsigned int VBATMAX = 14000; // max battery voltage in mV
const unsigned int VBATMIN = 10000; // min battery voltage in mV
const int IBATCHGMAX = 3000; // max battery charging current in mA
const int IBATDISMAX = 5000; // max battery discharging current in mA
const unsigned int VBUSMAX = 28000; // max bus voltage in FORM mode
const unsigned int VBUSMIN = 18000; // min bus voltage in FOLLOWCHARGE mode

// default starting values for several BMS variables
int bmsMode = FOLLOWCHARGE;
const unsigned int VBUSDEFAULT = 24000; // default nominal bus voltage in mV, must be greater than battery voltage here
const int ICHGDEFAULT = 1000; // default charging current
const int IDISDEFAULT = 1500; // default discharging current
int outputMode = CC2; // CV1, CC1, CV2 or CC2 mode for book keeping

// BMS global variables
unsigned int vBusRef = 0; // reference terminal 1 voltage setpoint (raw 0-1023); nominal bus voltage
unsigned int vBusMax = 0; // max bus voltage in FORM mode
unsigned int vBusMin = 0; // min bus voltage in FOLLOWCHARGE mode
unsigned int vBatMax = 0; // max battery voltage
unsigned int vBatMin = 0; // min battery voltage
unsigned int vBat25 = 0; // 25% battery voltage
unsigned int vBat75 = 0; // 75% battery voltage
int iBatChgLim = 0; // peak charging current limit
int iBatDisLim = 0; // peak discharge current limit
int iBatChgRef = 0; // sliding charging current limit
int iBatDisRef = 0; // sliding discharge current limit

// BMS algorithm interpolation slopes
// reference charge current = 0 + (vBatMax - vBat)*chgInterpSlope
//   = 0 + (iBatChgLim - 0) * ((vBatMax - vBat)/(vBatMax - vBat75));
// reference discharge current = 0 + (vBat - vBatMin)*disInterpSlope
//   = 0 + (iBatDisLim - 0) * ((vBat - vBatMin)/(vBat25 - vBatMin))
int chgInterpSlope = 0; // interpolation slope for charging
int disInterpSlope = 0; // interpolation slope for discharging
int setIRefsCounter = 0;

// global variables for coulomb counting (relevant for SOC calculations)
long coulombCounter = 0; // coulomb counter (mA-sec)
long ccntAccumulator = 0; // sub-second column counter accumulator for averaging (raw 0-1023)

// discrete compensator coefficients for classical feedback in FORM mode, regulating the bus with CV1
int compNum [] = {8, 0};
int compDen [] = {8, -8};

long slowInterruptCounter = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  atverter.initialize();
  atverter.setComp(compNum, compDen, sizeof(compNum)/sizeof(compNum[0]), sizeof(compDen)/sizeof(compDen[0]));

  // set up UART and I2C command support
  atverter.addCommandCallback(&interpretRXCommand);
  atverter.startUART();
  ReceiveEventI2C receiveEvent = [] (int howMany) {atverter.receiveEventI2C(howMany);};
  RequestEventI2C requestEvent = [] () {atverter.requestEventI2C();};
  atverter.startI2C(2, receiveEvent, requestEvent); // first argument is the slave device address (max 127)

  // initialize voltage and current limits to default values above
  vBusRef = atverter.mV2raw(VBUSDEFAULT);
  vBusMax = atverter.mV2raw(VBUSMAX);
  vBusMin = atverter.mV2raw(VBUSMIN);

  // set battery raw voltage thresholds (raw 0-1023)
  int vBatDiff = VBATMAX-VBATMIN;
  vBatMin = atverter.mV2raw(VBATMIN);
  vBatMax = atverter.mV2raw(VBATMAX);
  vBat25 = atverter.mV2raw(VBATMIN + vBatDiff/4);
  vBat75 = atverter.mV2raw(VBATMAX - vBatDiff/4);

  // set raw peak current limits (-512 to 512) to default values
  iBatChgLim = atverter.mA2raw(ICHGDEFAULT);
  iBatDisLim = atverter.mA2raw(IDISDEFAULT);
  chgInterpSlope = iBatChgLim/(vBatMax - vBat75);
  disInterpSlope = iBatDisLim/(vBat25 - vBatMin);

  // set initial sliding reference currents based on peak current limits and battery thresholds
  setIRefs(atverter.getRawV2());

  // hold side 2 for buck mode with port 1 input and port 2 output
  atverter.applyHoldHigh2();

  // once all is said and done, start the PWM. Grid-forming cold start has a hard coded start-up procedure
  if (bmsMode != FORMCOLDSTART)
    atverter.startPWM();
  
  // finally let the periodic control algorithm begin
  atverter.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms
}

void loop() { } // we don't use loop() because it does not loop at a fixed period

// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  // periodic update functions for normal operation
  atverter.readUART(); // if using UART, check every cycle if there are new characters in the UART buffer
  atverter.updateVISensors(); // read voltage and current sensors and update moving average
  atverter.checkCurrentShutdown(); // checks average current and shut down gates if necessary
  atverter.checkBootstrapRefresh(); // refresh bootstrap capacitors on a timer

  // get raw voltage and current readings at each port, with attention to sign conventions for current
  int vBat = atverter.getRawV2(); // battery port voltage
  int iBat = -1*(atverter.getRawI2()); // current into battery (positive)
  int vBus = atverter.getRawV1(); // bus port voltage
  int iBus = -1*(atverter.getRawI1()); // current into bus (positive)

  int error;
  bool isCharging; // state variable for FORM mode to designate if the battery is currently charging

  // update the sub-second raw coulomb counter accumulator
  ccntAccumulator += iBat;

  // update sliding reference currents based on the averaged battery voltage measured this cycle
  if (setIRefsCounter > 100) { // slide the reference currents slowly to avoid inrush and instabilities
    setIRefs(vBat);
    setIRefsCounter = 0;
  } else
    setIRefsCounter++;

  // BMS mode finite state machine, which can switch BMS Modes or Output Modes and designates the appropriate error

  // BMS Mode: FOLLOWCHARGE: charge the battery in grid following mode
  if (bmsMode == FOLLOWCHARGE) { // charge the battery in grid following mode
    // Output Mode: FSM state change
    if (outputMode == CC2 && vBat > vBatMax) { // switch to CV if battery voltage exceeds max battery voltage
      outputMode = CV2;
      atverter.resetComp(); // reset compensator past inputs and outputs switching between CV and CC
    } else if (outputMode == CV2 && iBat > iBatChgRef) { // switch back to CC if charge current exceeds reference limit
      outputMode = CC2;
      atverter.resetComp();
    }
    // Output Mode: determine error based on output mode state
    if (vBus < vBusMin)  { // if bus voltage falls below min value, we're charging too much; rapid curtail current
      iBatChgRef = iBatChgRef - 1;
      if (iBatChgRef < 0)
        iBatChgRef = 0;
      error = - iBat;
      atverter.triggerGradDescStep();
    } else if (outputMode == CC2) { // constant current operation
      error = iBatChgRef - iBat; // error is difference between reference charge current limit and battery current
    } else { // constant voltage operation
      error = vBatMax - vBat; // error is difference between max battery voltage and measured battery voltage
    }
  }
  // BMS Mode: FOLLOWDISCHARGE: discharge the battery in grid following mode
  else if (bmsMode == FOLLOWDISCHARGE) {
    // TODO: if (vBus > vBusMax), either decrease iBatDisRef, or run in CV1 mode at vBusMax, or revert to HOLD mode
    // Output Mode: FSM state change
    if (outputMode == CC2 && vBat < vBatMin) { // switch to CV if battery voltage goes below min battery voltage
      outputMode = CV2;
      atverter.resetComp();
    } else if (outputMode == CV2 && iBat < -iBatDisRef) { // switch back to CC if discharge current exceeds reference limit
      outputMode = CC2;
      atverter.resetComp();
    }
    // Output Mode: determine error based on output mode state
    if (outputMode == CC2) {
      error = (-iBatDisRef - iBat); // error is difference between reference discharge current limit and battery current
        // e.g.: -iBatDisRef=-1, iBat=-0.6, error=-0.4, duty down, vBus up or vBat down, => |iBat| up
    } else {
      error = vBatMin - vBat; // error is difference between min battery voltage and measured battery voltage
    }
  }
  // BMS Mode: FORM: have the battery grid from, i.e. attempt to regulate a DC bus
  else if (bmsMode == FORM) {
    if (vBus > vBusMax) { // first, make sure you don't exceed the maximum allowable DC bus voltage
      bmsMode = HOLD; // if so, emergency revert to HOLD mode
    }
    // Output Mode: FSM state change
    if ((outputMode == CV1 || outputMode == CC2) && vBat > vBatMax) {
      // is charging and hits battery max voltage limit
      outputMode = CV2;
      isCharging = true;
      atverter.resetComp();
    } else if ((outputMode == CV1 || outputMode == CV2) && iBat > iBatChgRef) {
      // is charging and hits sliding battery charge current limit
      outputMode = CC2;
      isCharging = true;
      atverter.resetComp();
    } else if ((outputMode == CV1 || outputMode == CC2) && vBat < vBatMin) {
      // is discharging and hits battery min voltage limit
      outputMode = CV2;
      isCharging = false;
      atverter.resetComp();
    } else if ((outputMode == CV1 || outputMode == CV2) && iBat < -iBatDisRef) {
      // is discharging and hits sliding battery discharge current limit
      outputMode = CC2;
      isCharging = false;
      atverter.resetComp();
    } else { // normal FORM operation, no apparent limits were hit
      if ((iBat > -iBatDisRef/2 && iBat < iBatChgRef/2) || (vBat > vBat25 && vBat < vBat75))
        // only switch back CV1 bus regulation mode once battery voltage and current are well outside their respective limits
        outputMode = CV1;
    }
    // Output Mode: determine error based on output mode state and whether the battery is charging or discharging
    if (outputMode == CC2) {
      if (isCharging) {
        error = iBatChgRef - iBat;
      } else {
        error = (-iBatDisRef - iBat);
      }
    } else if (outputMode == CV2) {
      if (isCharging) {
        error = vBatMax - vBat;
      } else {
        error = vBatMin - vBat;
      }
    } else { // CV1: error regulates the bus voltage
      error = -(vBusRef - atverter.getVDroopRaw(iBus) - vBus); // negative sign required when terminal 1 is being controlled
    }
  }
  // BMS Mode: FORMCOLDSTART: if bus voltage < battery voltage, must do a hard-coded cold start before grid forming
  else if (bmsMode == FORMCOLDSTART) {
    if (vBus > vBat) { // if ever the bus voltage exceeds battery voltage, we no longer need to cold start
      atverter.applyHoldHigh2(); // make sure we're in buck (step down) mode
      bmsMode = FORM;
      return;
    }
    slowInterruptCounter++;
    if (slowInterruptCounter == 5000) { // wait for 5 seconds for bus voltages to settle, e.g. after a restart
      atverter.applyHoldHigh1(); // set to boost mode, step up from terminal 1 (bus) to terminal 2 (battery)
      atverter.setDutyCycle(90); // set duty cycle for a large step-up ratio, assuming bus voltage is near zero
      atverter.startPWM();
    } else if (slowInterruptCounter == 5200) { // every 200ms, reduce duty cycle, which increases bus voltage
      atverter.setDutyCycle(70);
    } else if (slowInterruptCounter == 5400) {
      atverter.setDutyCycle(50);
    } else if (slowInterruptCounter == 5600) {
      atverter.setDutyCycle(30);
    } else if (slowInterruptCounter == 5800) {
      atverter.setDutyCycle(10);
    } else if (slowInterruptCounter == 6000) {
      atverter.applyHoldHigh2(); // once bus voltage nearly equals battery voltage, switch to buck (step down) mode
      atverter.setDutyCycle(90); // set duty cycle for a small step-down ratio
    } else if (slowInterruptCounter > 6500) { // after 500ms, switch to grid-forming mode
      bmsMode = FORM;
    }
    return; // we skip everything else in the control loop since FORMCOLDSTART is explicitly hard coded
  }
  // BMS Mode: HOLD: have the battery hold; will not charge or discharge
  else {
    // in HOLD mode, you may instead want to shutdown the gates, which could save power and prevent leakage
    outputMode = CC2;
    error = 0 - iBat;
    if (vBus < vBat && !atverter.isGateShutdown())
      // shutdown gates if the bus goes below battery level (which would otherwise drain the battery)
      atverter.shutdownGates();
  }

  // update array of past compensator inputs
  atverter.updateCompPast(error);

  // We can use classical feedback for rapid bus regulation in FORM CV1 mode
  // Note: the compensation.py calculator only works for CR or CC loads, not for CV like batteries
  // TODO: update compensation.py for batteries, then we can use classical feedback in grid-following modes
  bool isClassicalFB = false;
  if (bmsMode == FORM && outputMode == CV1) { // only use for bus regulation in grid forming
    // 0.5A-5A output: classical feedback voltage mode discrete compensation
    // 0A-0.5A output: slow gradient descent mode
    isClassicalFB = atverter.getRawI1() < -51 || atverter.getRawI1() > + 51;
  }

  if(isClassicalFB) { // classical feedback voltage mode discrete compensation
    // calculate the compensator output based on past values and the numerator and demoninator
    int duty = (atverter.calculateCompOut()*100)/1024;
    atverter.setDutyCycle(duty);
  } else { // slow gradient descent mode, avoids light-load instability
    atverter.gradDescStep(error); // steps duty cycle up or down depending on the sign of the error
  }

  // slow interrupt processes
  slowInterruptCounter++;
  if (slowInterruptCounter > 1000) { // timer set such that each count is 1 ms
    slowInterruptCounter = 0;
    atverter.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverter.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverter.checkThermalShutdown(); // checks average temperature and shut down gates if necessary
    
    // average the sub-second raw coulomb count accumulator, and convert it to a mA-hour value to accumulate
    coulombCounter += atverter.raw2mA(ccntAccumulator/1000);
    ccntAccumulator = 0;

    Serial.print("BMo:");
    Serial.print(bmsMode);
    Serial.print(", OMo:");
    Serial.print(outputMode);
    Serial.print(", dut:");
    Serial.print(atverter.getDutyCycle());
    Serial.print(", vbus:");
    Serial.print(vBus);
    Serial.print(",");
    Serial.print(vBusRef);
    Serial.print(",");
    Serial.print(vBusMax);
    Serial.print(", vdrp:");
    Serial.print(atverter.getVDroopRaw(iBus));
    Serial.print(", vbat:");
    Serial.print(vBatMin);
    Serial.print(",");
    Serial.print(vBat25);
    Serial.print(",");
    Serial.print(atverter.getRawV2());
    Serial.print(",");
    Serial.print(vBat75);
    Serial.print(",");
    Serial.print(vBatMax);
    Serial.print(", BatV:");
    Serial.print(atverter.getV2());
    Serial.print(", cRef:");
    Serial.print(iBatChgRef);
    Serial.print(", dRef:");
    Serial.print(iBatDisRef);
    Serial.print(", iBat:");
    Serial.print(iBat);
    Serial.print(", BatI:");
    Serial.print(atverter.getI2());
    Serial.print(", err:");
    Serial.print(error);
    Serial.print(", ccnt:");
    Serial.print(coulombCounter);
    Serial.print(", igsd:");
    Serial.println(atverter.isGateShutdown());
  }
}

// sets reference current limits (iBatXRef) based on absolute max currents (iBatXLim) and battery voltage (vBatX)
void setIRefs(int vBat) {
  int tempChgRef;
  int tempDisRef;
  // determine what the reference current limit should be (tempXRef) based on battery voltage
  if (vBat < vBatMin) {
    tempChgRef = iBatChgLim;
    tempDisRef = 0;
  } else if (vBat < vBat25) {
    tempChgRef = iBatChgLim;
    tempDisRef = (vBat - vBatMin)*disInterpSlope;
  } else if (vBat < vBat75) {
    tempChgRef = iBatChgLim;
    tempDisRef = iBatDisLim;
  } else if (vBat < vBatMax) {
    tempChgRef = (vBatMax - vBat)*chgInterpSlope;
    tempDisRef = iBatDisLim;
  } else {
    tempChgRef = 0;
    tempDisRef = iBatDisLim;
  }
  // slowly change the actual reference current limit (iBatXRef) in direction of tempXRef
  if (tempChgRef > iBatChgRef) {
    iBatChgRef = iBatChgRef + 1;
  } else {
    iBatChgRef = iBatChgRef - 1;
  }
  if (tempDisRef > iBatDisRef) {
    iBatDisRef = iBatDisRef + 1;
  } else {
    iBatDisRef = iBatDisRef - 1;
  }
}

// serial command interpretation function
void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RFN") == 0) {
    readFileName(value, receiveProtocol);
  } else if (strcmp(command, "WMODE") == 0) {
    writeMODE(value, receiveProtocol);
  } else if (strcmp(command, "RMODE") == 0) {
    readMODE(value, receiveProtocol);
  } else if (strcmp(command, "WICHG") == 0) {
    writeICHG(value, receiveProtocol);
  } else if (strcmp(command, "RICHG") == 0) {
    readICHG(value, receiveProtocol);
  } else if (strcmp(command, "WIDIS") == 0) {
    writeIDIS(value, receiveProtocol);
  } else if (strcmp(command, "RIDIS") == 0) {
    readIDIS(value, receiveProtocol);
  } else if (strcmp(command, "WVBUS") == 0) {
    writeVBUS(value, receiveProtocol);
  } else if (strcmp(command, "RVBUS") == 0) {
    readVBUS(value, receiveProtocol);
  } else if (strcmp(command, "RCCNT") == 0) {
    readCCNT(value, receiveProtocol);
  } else if (strcmp(command, "WRCNT") == 0) {
    resetRCNT(value, receiveProtocol);
  }
}

// outputs the file name to serial
void readFileName(const char* valueStr, int receiveProtocol) {
  sprintf(atverter.getTXBuffer(receiveProtocol), "WFN:%s", "BMS.ino");
  atverter.respondToMaster(receiveProtocol);
}

// sets the BMS operation mode from a serial command string: FCHG, FDIS, FORM, HOLD, FCS
void writeMODE(const char* valueStr, int receiveProtocol) {
  if (strcmp(valueStr, "FCHG") == 0) {
    bmsMode = FOLLOWCHARGE;
    outputMode = CC2;
    iBatChgRef = 0;
  } else if (strcmp(valueStr, "FDIS") == 0) {
    bmsMode = FOLLOWDISCHARGE;
    outputMode = CC2;
    iBatDisRef = 0;
  } else if (strcmp(valueStr, "FORM") == 0) {
    bmsMode = FORM;
    outputMode = CV1;
  } else if (strcmp(valueStr, "FCS") == 0) {
    bmsMode = FORMCOLDSTART;
    outputMode = CV1;
  } else { // HOLD
    bmsMode = HOLD;
    outputMode = CC2;
  }
  sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:=%d", bmsMode);
  atverter.respondToMaster(receiveProtocol);
}

// gets the BMS operation mode and outputs to serial
void readMODE(const char* valueStr, int receiveProtocol) {
  if (bmsMode == FOLLOWCHARGE)
    sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:FCHG");
  else if (bmsMode == FOLLOWDISCHARGE)
    sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:FDIS");
  else if (bmsMode == FORM)
    sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:FORM");
  else if (bmsMode == FORMCOLDSTART)
    sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:FCS");
  else
    sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:HOLD");
  // atverter.resetComp();
  atverter.respondToMaster(receiveProtocol);
}

// sets the battery charging current limit (mA) from a serial command
void writeICHG(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atoi(valueStr);
  if (temp > IBATCHGMAX)
    temp = IBATCHGMAX;
  iBatChgLim = atverter.mA2raw(temp);
  chgInterpSlope = iBatChgLim/(vBatMax - vBat75);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WICHG:=%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// gets the battery charging current limit (mA) and outputs to serial
void readICHG(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atverter.raw2mA(iBatChgLim);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WICHG:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// sets the battery discharging current limit (mA) from a serial command
void writeIDIS(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atoi(valueStr);
  if (temp > IBATDISMAX)
    temp = IBATDISMAX;
  iBatDisLim = atverter.mA2raw(temp);
  disInterpSlope = iBatDisLim/(vBat25 - vBatMin);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WIDIS:=%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// gets the battery discharging current limit (mA) and outputs to serial
void readIDIS(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atverter.mA2raw(iBatDisLim);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WIDIS:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// sets the grid-forming bus voltage limit (mV) from a serial command
void writeVBUS(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atoi(valueStr);
  if (temp > VBUSMAX-1000)
    temp = VBUSMAX-1000;
  vBusRef = atverter.mV2raw(temp);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WVBUS:=%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// gets the grid-forming bus voltage limit (mV) and outputs to serial
void readVBUS(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atverter.raw2mV(vBusRef);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WVBUS:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// gets the integrated battery energy (mA-hour) since last coulomb counter reset and outputs to serial
void readCCNT(const char* valueStr, int receiveProtocol) {
  long temp = coulombCounter/3600; // convert to mA-hour
  sprintf(atverter.getTXBuffer(receiveProtocol), "WCCNT:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// resets the coulomb counter
void resetRCNT(const char* valueStr, int receiveProtocol) {
  coulombCounter = 0;
  ccntAccumulator = 0;
  sprintf(atverter.getTXBuffer(receiveProtocol), "WRCNT:=%d", coulombCounter);
  atverter.respondToMaster(receiveProtocol);
}
