/*
  Solar Converter

  A step-up solar power converter (a.k.a. power optimizer) for connecting solar panels to a DC microgrid bus. Can operate
  in grid-following (does MPPT) or grid-forming (regulates the DC bus).
  Solar Panel: Terminal 1: 8-24V open-circuit voltage (24V DC bus), or 8-48V OCV (48V DC bus)
  DC Bus: Terminal 2: 24V nominal (22-26V allowed), or 48V nominal (44-56V allowed)
  
  Test Set-up (for grid-following):
  Solar Panel: Attached to terminal 2 through an Ammeter
  DC Bus: Voltage supply attached to terminal 2, set to CV at 24V with 3A current limit
  DC Bus: Electronic load attached to terminal 2, is set to CR mode between 15 and 100 ohms

  Grid Following Mode (MPPT):
  During grid following operation, the solar controller uses a gradient-based perturb and observe method to find the
  maximum power point in realtime for any level of irradiation or shading. Users can curtail the solar power by
  specifying a maximum power limit. The controller will also curtail the power if the bus voltage exceeds a specified
  maximum. This mode requires something else such as a battery or power supply to form and maintain the DC bus.

  Grid Forming Mode:
  The solar controller can also grid form, where it attempts to maintain the DC bus at a specified voltage. It will
  automatically adjust duty cycle to accomodate changes in load on the DC bus. If the DC bus is overloaded, it may
  cause the solar panel voltage to collapse. If the solar panel voltage falls below a specified limit, the controller
  will disconnect the panel for several seconds.

  Primary control strategy:
  Upon start-up, the controller assumes there are no other power sources to maintain the DC bus. Thus it will enter
  grid-forming mode and maintain the bus at VBUSDEFAULT (20V). If another power source such as a battery begins
  to grid form, it will likely pull the DC bus up to the nominal 24V. When the bus voltage crosses VBUSSWITCH (22V),
  the solar controller will assume the battery is grid-forming, and will switch to grid-following MPPT mode. If the
  battery stops grid-forming, the bus voltage may fall, but if it crosses VBUSSWITCH, the solar converter will go
  back into grid-forming mode.

  Created 12/23/23 by Daniel Gerber
*/

#include <AtverterH.h>
AtverterH atverter;

enum SolarModes
{   FOLLOW = 0, // perform MPPT grid following mode, with the option to curtail power
    FORM = 1, // grid-form with the solar converter to maintain a DC bus on terminal 2
    DISCONNECT = 2, // have the battery hold, i.e. will not charge or discharge
    NUM_SOLARMODES
};

// specify the absolute max values, from solar panel datasheet
const unsigned int VSOLARMIN = 5000; // minimum solar voltage before converter disconnects
const unsigned int PLIMDEFAULT = 20000; // default power limit in mW (max 65535)

// default starting values for several variables
const unsigned int VBUSDEFAULT = 20000; // default bus voltage (mV) in FORM, must be greater than solar panel voltage here
const unsigned int VBUSSWITCH = 22000; // min bus voltage (mV) in FOLLOW mode, max bus voltage in FORM mode
const unsigned int VBUSMAX = 26000; // max bus voltage (mV) in FOLLOW mode
const unsigned int RDROOP = 200; // default droop resistance (mohms) in FORM mode

int solarMode = FORM; // solar controller operating mode: FOLLOW, FORM, DISCONNECT
int outputMode = CV1; // CV1 (FOLLOW), CV2 or CC2 (FORM) for book keeping

// solar set point raw global variables
long pLim = 0; // limit of power from solar panel in FOLLOW mode raw
unsigned int vBusMax = 0; // max bus voltage in FOLLOW mode
unsigned int vBusSwitch = 0; // max bus voltage in FORM mode before automatically switching to FOLLOW mode
unsigned int vBusLim = 0; // reference terminal 1 voltage setpoint (raw 0-1023); nominal bus voltage

// FOLLOW variables
const int PAVGLENGTH = 128; // averaging length for power measurements
int avgCounter = 0; // counts up to PAVGLENGTH, and then computes power average
long powerAcc = 0; // accumulator for measured power values before they are averaged
long powerVal [] = {0, 0, 0}; // several past power averages for comparison
long pRef = 0; // reference power setting (always less than pLim) for controls and error
int stepDirection = 1; // 1 or -1 for direction of stepping voltage
bool skip = false; // used to skip power calculation every other cycle to allow proper settling
bool vBusInRange = false; // latches to true once the bus voltage is in the appropriate FOLLOW range

// FORM and DISCONNECT variables
int vSolarMin = 0; // in FORM mode, if solar panel voltage drops below vSolarMin, then disconnect
unsigned int disconnectTimer = 0; // remain disconnected for this many milliseconds
unsigned int switchTimer = 0; // count how long bus voltage must be above vBusSwitch until switch to FOLLOW

// discrete compensator coefficients for classical feedback
int compNum [] = {8, 0};
int compDen [] = {8, -8};

long slowInterruptCounter = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  atverter.initialize();
  atverter.setComp(compNum, compDen, sizeof(compNum)/sizeof(compNum[0]), sizeof(compDen)/sizeof(compDen[0]));
  atverter.setRDroop(RDROOP);

  // set up UART and I2C command support
  atverter.addCommandCallback(&interpretRXCommand);
  atverter.startUART();
  ReceiveEventI2C receiveEvent = [] (int howMany) {atverter.receiveEventI2C(howMany);};
  RequestEventI2C requestEvent = [] () {atverter.requestEventI2C();};
  atverter.startI2C(1, receiveEvent, requestEvent); // first argument is the slave device address (max 127)

  // initialize raw (0-1023) voltage limits to default mV values above
  vBusLim = atverter.mV2raw(VBUSDEFAULT);
  vBusMax = atverter.mV2raw(VBUSMAX);
  vBusSwitch = atverter.mV2raw(VBUSSWITCH);
  vSolarMin = atverter.mV2raw(VSOLARMIN);

  // set default power limit in FOLLOW mode
  // power raw is equal to mV raw * mA raw. We can do that by substituting PLIMDEFAULT as a mV value
  pLim = (long) atverter.mV2raw(PLIMDEFAULT) * atverter.mA2raw(1000);
  pRef = pLim;

  // hold side 1 for boost mode with port 1 input and port 2 output
  atverter.applyHoldHigh1();

  // once all is said and done, start the PWM. Duty cycle is set so as to prevent inrush
  if (solarMode == FORM)
    connectForm();
  else if (solarMode == FOLLOW)
    connectFollow();
  
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
  int vSolar = atverter.getRawV1(); // solar port voltage
  int iSolar = atverter.getRawI1(); // current out of solar panel (positive)
  int vBus = atverter.getRawV2(); // bus port voltage
  int iBus = -1*(atverter.getRawI2()); // current into bus (positive)
  long pSolar = (long) vSolar * (long) iSolar; // power (raw) out of solar panel (positive)

  int error = 0; // error variable for feedback or gradient descent

  // Solar Mode finite state machine

  // Solar Mode: FOLLOW: maximum power-point tracking mode with curtailing
  if (solarMode == FOLLOW) {
    outputMode = CV1; // just for book keeping
    if (vBus > vBusMax) { // hold or curtail if bus voltage exceeds maximum threshold
      if (pRef > pLim - 100) // if pRef is close to pLim, quickly curtail reference power
        pRef = pRef * 3 / 4;
      else // if pRef is much lower than pLim slowly curtail reference power
        pRef = pRef - 10;
      if (pRef < 10) // make sure pRef doesn't go negative
        pRef = 10;
      error = 1; // increase duty cycle so as to decrease solar voltage
      atverter.triggerGradDescStep(); // make sure to immediately curtail
    } else if (vBus < vBusSwitch && vBusInRange) { // switch to FORM mode if bus voltage falls below minimum threshold
      solarMode = FORM;
      vBusInRange = false;
      return;
    } else if (iSolar < 0) { // make sure no current going into solar panel, if so perform corrective action
      error = 1;
    } else { // MPPT algorithm (gradient descent)
      vBusInRange = true; // latch vBusInRange to indicate bus voltage has reached appropriate range
      if (avgCounter >= PAVGLENGTH) {
        // first latch in the average power and reset the averaging counter
        powerVal[0] = powerAcc/PAVGLENGTH;
        powerAcc = 0;
        avgCounter = 0;
        // check average power and step duty cycle appropriately (do it every other averaging cycle)
        if (!skip) { // doing every other cycle allows power measurement to settle, improving accurately
          // determine the step direction based on previous power and reference power
          if (powerVal[0] < pRef) { // we are below the reference power: try to increase power
            if (powerVal[0] < powerVal[2]) { // measured power is less than previous measured power
              stepDirection = -1*stepDirection; // reverse step direction
            } // and if measured power > previous measured power, continue stepping in same direction
          } else { // (powerVal[0] > pRef) we are above the reference power: decrease the power
            if (powerVal[0] > powerVal[2]) { // measured power is greater than previous measured power
              stepDirection = -1*stepDirection; // reverse step direction
            } // and if measured power > previous measured power, continue stepping in same direction
          }
          // set error
          if (atverter.getDutyCycle() >= 99)
            error = -1; // bounce back duty cycle to avoid getting stuck at high end
          else if (atverter.getDutyCycle() <= 1)
            error = 1; // bounce back duty cycle to avoid getting stuck at low end
          else
            error = stepDirection; // gradient descent algorithm below only cares about the sign of the error
          atverter.triggerGradDescStep(); // immediately step duty cycle
        }
        // update previous power values
        powerVal[2] = powerVal[1];
        powerVal[1] = powerVal[0];
        skip = !skip;
      } else { // averaging is not yet complete, update counter and accumulator
        avgCounter++;
        powerAcc += pSolar;
      }
      // slowly bring reference power back toward power limit if it had previously been curtailed
      if (pRef < pLim - 10 && vBus < vBusMax)
        pRef = pRef + 10;
      if (pRef > pLim)
        pRef = pLim;
    }
  }
  // Solar Mode: FORM: adjust duty cycle so as to maintain bus voltage
  else if (solarMode == FORM) {
    outputMode = CV2; // just for bookkeeping
    // check if the bus and solar voltages are in acceptable range for FORM mode
    if (vBus > vBusSwitch) { // check if bus voltage goes above the "switch" threshold
      switchTimer ++;
      if (switchTimer > 5000) // has vBus been held high for 5 seconds
        solarMode = FOLLOW; // if so, switch to FOLLOW mode
    } else {
      switchTimer = 0; // if ever vBus falls below vBusSwitch, reset switch timer
    }
    if (vSolar < vSolarMin && vBus < 3*vBusLim/4) { // disconnect if voltage below min needed to power Atmega
      disconnect(5000);
    } else if (iSolar < 0) { // make sure no current going into solar panel, if so perform corrective action
      error = 0 - iSolar;
      atverter.triggerGradDescStep();
    } else if (vBus > vBusMax) { // make sure vBus < vBusMax, if so perform corrective action
      error = vBus - vBusMax;
      atverter.triggerGradDescStep();
    } else {
      // if bus voltage is low (e.g. startup), then use a counter to slowly change duty cycle to avoid inrush
      if (vBus < 3*vBusLim/4 && avgCounter < 32)
        avgCounter++;
      else { // bus voltage is in appropriate range and all systems go
        error = vBusLim - atverter.getVDroopRaw(iBus) - vBus; // error based on standard constant voltage control
        avgCounter = 0;
      }
    }
  }
  // Solar Mode: DISCONNECT
  else { // in DISCONNECT mode, wait until disconnect timer is done, then revert to FORM mode
    if (disconnectTimer <= 0) {
      connectForm();
    } else if (disconnectTimer < 60000) { // arbitrary max of 1 minutes
      disconnectTimer --;
    }
  }

  // update array of past compensator inputs
  atverter.updateCompPast(error);

  // We can use classical feedback for rapid bus regulation in FORM CV2 mode
  bool isClassicalFB = false;
  if (solarMode == FORM && outputMode == CV2) { // only use for bus regulation in grid forming
    // 0.5A-5A output: classical feedback voltage mode discrete compensation
    // 0A-0.5A output: slow gradient descent mode
    isClassicalFB = (iSolar > 0) && (iBus > 51);
  }

  if(isClassicalFB) { // classical feedback voltage mode discrete compensation
    // calculate the compensator output based on past values and the numerator and demoninator
    int duty = (atverter.calculateCompOut()*100)/1024;
    atverter.setDutyCycle(duty);
  } else { // slow gradient descent mode
    atverter.gradDescStep(error); // steps duty cycle up or down depending on the sign of the error
  }

  // slow interrupt processes
  slowInterruptCounter++;
  if (slowInterruptCounter > 1000) { // if each count is 1 ms, triggers every 1 second
    slowInterruptCounter = 0;
    atverter.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverter.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverter.checkThermalShutdown(); // checks average temperature and shut down gates if necessary

  // long pSolar1 = 1024*1024; // power (raw) out of solar panel (positive)
  // long pSolar2 = (long) vSolar * (long) iSolar; // power (raw) out of solar panel (positive)
  // long pSolar3 = atverter.raw2mV(vSolar); // power (raw) out of solar panel (positive)
  // long pSolar4 = atverter.raw2mA(iSolar); // power (raw) out of solar panel (positive)
  // long pSolar5 = (long) atverter.raw2mV(vSolar) * atverter.raw2mA(iSolar) / 1000;
  // long pSolar6 = (long) atverter.mV2raw(-1*pSolar5)*atverter.mA2raw(1000);
  // // long pSolar6 = atverter.raw2mV(pSolar2/atverter.mA2raw(1000));

    Serial.print("SMo:");
    Serial.print(solarMode);
    Serial.print(", dut:");
    Serial.print(atverter.getDutyCycle());
    Serial.print(", vbus:");
    Serial.print(vBus);
    Serial.print(", ");
    Serial.print(vBusLim);
    Serial.print(", vsol:");
    Serial.print(vSolar);
    Serial.print(", ");
    Serial.print(vSolarMin);
    Serial.print(", isol:");
    Serial.print(iSolar);    
    Serial.print(", psol:");
    Serial.print(pSolar);
    Serial.print(", ");
    Serial.print(powerVal[1]);
    Serial.print(", ");
    Serial.print(powerVal[2]);
    Serial.print(", pRef:");
    Serial.print(pRef);
    Serial.print(", ");
    Serial.print(pLim);
    Serial.print(", diff:");
    Serial.print(stepDirection);
    Serial.print(", ");
    Serial.println(powerVal[1] - powerVal[2]);
  }
}

void disconnect(int dcTimer) {
  disconnectTimer = dcTimer; // equivalent to 10 seconds assuming 1ms updates
  solarMode = DISCONNECT;
  atverter.shutdownGates(4);
}

void connectForm() {
  atverter.setDutyCycle(1);
  if(atverter.isGateShutdown())
    atverter.enableGateDrivers();
  solarMode = FORM;
}

void connectFollow() {
  atverter.setDutyCycle((long)100*VSOLARMIN/VBUSMAX);
  if(atverter.isGateShutdown())
    atverter.enableGateDrivers();
  solarMode = FOLLOW;
}

// serial command interpretation function
void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RFN") == 0) {
    readFileName(value, receiveProtocol);
  } else if (strcmp(command, "WMODE") == 0) {
    writeMODE(value, receiveProtocol);
  } else if (strcmp(command, "RMODE") == 0) {
    readMODE(value, receiveProtocol);
  } else if (strcmp(command, "WPLIM") == 0) {
    writePLIM(value, receiveProtocol);
  } else if (strcmp(command, "RPLIM") == 0) {
    readPLIM(value, receiveProtocol);
  }
}

// outputs the file name to serial
void readFileName(const char* valueStr, int receiveProtocol) {
  sprintf(atverter.getTXBuffer(receiveProtocol), "WFN:%s", "SolarConverter.ino");
  atverter.respondToMaster(receiveProtocol);
}

// sets the Solar operation mode from a serial command string: FOL, FORM, DISC
void writeMODE(const char* valueStr, int receiveProtocol) {
  if(atverter.isGateShutdown())
    atverter.enableGateDrivers();
  if (strcmp(valueStr, "FOL") == 0) {
    connectFollow();
  } else if (strcmp(valueStr, "FORM") == 0) {
    connectForm();
  } else if (strcmp(valueStr, "DISC") == 0) {
    disconnect(65000); // anything above 60000 disables the disconnect timer
  }
  sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:=%d", solarMode);
  atverter.respondToMaster(receiveProtocol);
}

// gets the Solar operation mode and outputs to serial
void readMODE(const char* valueStr, int receiveProtocol) {
  if (solarMode == FOLLOW)
    sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:FOL");
  else if (solarMode == FORM)
    sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:FORM");
  else
    sprintf(atverter.getTXBuffer(receiveProtocol), "WMODE:DISC");
  // atverter.resetComp();
  atverter.respondToMaster(receiveProtocol);
}

// sets the MPPT power limit (mW) from a serial command
void writePLIM(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atoi(valueStr);
  pLim = (long) atverter.mV2raw(temp) * atverter.mA2raw(1000);
  if (pRef > pLim)
    pRef = pLim;
  sprintf(atverter.getTXBuffer(receiveProtocol), "WPLIM:=%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

// gets the MPPT power limit (mW) and outputs to serial
void readPLIM(const char* valueStr, int receiveProtocol) {
  unsigned int temp = atverter.raw2mV(pLim / atverter.mA2raw(1000));
  sprintf(atverter.getTXBuffer(receiveProtocol), "WPLIM:%d", temp);
  atverter.respondToMaster(receiveProtocol);
}

