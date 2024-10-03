/*
  Battery Panel

  WARNING: Never use unprotected batteries... ever. You will burn down your home.

  Enhances the Smart Panel example with battery discharge control. Assumes a nanogrid with battery is connected to
  the high power port and loads connected to low power ports. Ensures battery does not get over discharged with a
  low-voltage cutoff.

  created 20 August 2024
  by Daniel Gerber
*/

#include <MicroPanelH.h>

MicroPanelH micropanel;
long slowInterruptCounter = 0;

// specify the follwoing absolute max battery values from battery datasheet
const unsigned int VBATMIN = 11500*4; // min battery voltage in mV when drawing 0A
const unsigned int VBATMINABS = 11000*4; // absolute min battery voltage in mV regardless of current
const int IBATDISMAX = 15000; // max battery discharging current in mA
const unsigned int RINTERNAL = 100*4; // estimated internal resistance (mohms)
const unsigned int VACTIVATE = VBATMIN + 2000; // if undervoltage cutoff, needs this voltage to reactivate

// Battery Converter global variables
unsigned int vBatMin = 0; // min battery voltage at 0A
unsigned int vBatMinAbs = 0; // absolute min battery voltage any current
unsigned int vActivate = 0; // if undervoltage cutoff, needs this voltage to reactivate

// global variables for coulomb counting (relevant for SOC calculations)
long coulombCounter = 0; // coulomb counter (mA-sec)
long ccntAccumulator = 0; // sub-second column counter accumulator for averaging (raw 0-1023)

// the setup function runs once when you press reset or power the board
void setup() {
  micropanel.setupPinMode(); // set pins to input or output
  micropanel.initializeSensors(); // set filtered sensor values to initial reading
  micropanel.setCurrentLimit1(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit2(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit3(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit4(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimitTotal(IBATDISMAX); // set gate shutdown at max battery current 

  // set up UART and I2C command support
  // add the command interpretation function below as a listener for serial commands
  micropanel.addCommandCallback(&interpretRXCommand);
  // initialize UART communication at 9600 bits per second
  micropanel.startUART(); // can optionally specify baud rate with an argument
  // initialize I2C communciation and register receive and request callback functions
  // requires using lambda functions; it's clunky but mandatory since Wire.h doesn't allow member callbacks
  // just copy the following code for any I2C projects
  ReceiveEventI2C receiveEvent = [] (int howMany) {micropanel.receiveEventI2C(howMany);};
  RequestEventI2C requestEvent = [] () {micropanel.requestEventI2C();};
  micropanel.startI2C(8, receiveEvent, requestEvent); // first argument is the slave device address (max 127)

  // set battery raw parameters (raw 0-1023)
  vBatMin = micropanel.mV2raw(VBATMIN);
  vBatMinAbs = micropanel.mV2raw(VBATMINABS);
  vActivate = micropanel.mV2raw(VACTIVATE);
  micropanel.setRDroop(RINTERNAL);

  // initialize inrush override for channels
  micropanel.setDefaultInrushOverride(200); // hold default channel protection for 200us to ride through inrush current
  micropanel.setDefaultInrushOverride(2, 1000); // channel 2 (fans) needs 1000us inrush ride through
  micropanel.setDefaultInrushOverride(4, 1000); // channel 4 (refrigerator) needs 1000us inrush ride through

  // initialize interrupt timer for periodic calls to control update funciton
  micropanel.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms (= 1000 microseconds)
}

void loop() { } // we don't use loop() because it does not loop at a fixed period

// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  micropanel.readUART(); // if using UART, check every cycle if there are new characters in the UART buffer
  micropanel.updateVISensors(); // read voltage and current sensors and update moving average
  micropanel.checkCurrentShutdown(); // checks average current and shut down gates if necessary

  int vBat = micropanel.getRawVBus(); // battery port voltage, aka. bus voltage
  int iBat = micropanel.getRawITotal(); // current out of battery (positive)

  if (vBat < vBatMinAbs || vBat < vBatMin - micropanel.getVDroopRaw(iBat)) { // battery voltage goes below min
    if (micropanel.isSomeChannelsActive())
      micropanel.shutdownChannels();
  }

  slowInterruptCounter++; // in this example, do some special stuff every 1 second (1000ms)
  if (slowInterruptCounter > 1000) {
    slowInterruptCounter = 0;
    micropanel.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)

    // prints channel state (as binary), VCC, VBus, I1, I2, I3, I4 to the serial console of attached computer
    // Serial.print("State: ");
    // Serial.print(micropanel.getCh1());
    // Serial.print(micropanel.getCh2());
    // Serial.print(micropanel.getCh3());
    // Serial.print(micropanel.getCh4());
    // Serial.print(", VCC=");
    // Serial.print(micropanel.getVCC());
    // Serial.print("mV, VBus=");
    // Serial.print(micropanel.getVBus());
    // Serial.print("mV, I1=");  
    // Serial.print(micropanel.getI1());
    // Serial.print("mA, I2=");  
    // Serial.print(micropanel.getI2());
    // Serial.print("mA, I3=");  
    // Serial.print(micropanel.getI3());
    // Serial.print("mA, I4=");  
    // Serial.print(micropanel.getI4());
    // Serial.print("mA, ITot=");
    // Serial.print(micropanel.getITotal());
    // Serial.print("mA, vbat:");
    // Serial.print(vBat);
    // Serial.print(", vdrp:");
    // Serial.println(micropanel.getVDroopRaw(iBat));
  }
}

void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RFN") == 0) {
    readFileName(value, receiveProtocol);
  } else {
    // Write Channel Protected: checks if battery voltage is above activate threshold before enabling channel
    int temp = atoi(value);
    int vBat = micropanel.getRawVBus(); // battery port voltage, aka. bus voltage
    if (strcmp(command, "WCP1") == 0) { // write the desired terminal 1 state
      if (temp == 1 && micropanel.getCh1() == 0 && vBat < vActivate)
        temp = 0;
      micropanel.setCh1(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCP1:=%d", temp);
    } else if (strcmp(command, "WCP2") == 0) { // write the desired terminal 2 state
      if (temp == 1 && micropanel.getCh2() == 0 && vBat < vActivate)
        temp = 0;
      micropanel.setCh2(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCP2:=%d", temp);
    } else if (strcmp(command, "WCP3") == 0) { // write the desired terminal 3 state
      if (temp == 1 && micropanel.getCh3() == 0 && vBat < vActivate)
        temp = 0;
      micropanel.setCh3(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCP3:=%d", temp);
    } else if (strcmp(command, "WCP4") == 0) { // write the desired terminal 4 state
      if (temp == 1 && micropanel.getCh4() == 0 && vBat < vActivate)
        temp = 0;
      micropanel.setCh4(temp);
      sprintf(micropanel.getTXBuffer(receiveProtocol), "WCP4:=%d", temp);
    }
    micropanel.respondToMaster(receiveProtocol);
  }
}

// outputs the file name to serial
void readFileName(const char* valueStr, int receiveProtocol) {
  sprintf(micropanel.getTXBuffer(receiveProtocol), "WFN:%s", "BatteryPanel.ino");
  micropanel.respondToMaster(receiveProtocol);
}

  // --- Default Register Guide from Library ---
  // "RVB": read bus voltage
  // "RI1": read current at terminal 1
  // "RI2": read current at terminal 2
  // "RI3": read current at terminal 3
  // "RI4": read current at terminal 4
  // "RVCC": read the ~5V VCC bus voltage
  // "RCH1": read current at terminal 1
  // "RCH2": read current at terminal 2
  // "RCH3": read current at terminal 3
  // "RCH4": read current at terminal 4
  // "WCH1": write the desired terminal 1 state
  // "WCH2": write the desired terminal 2 state
  // "WCH3": write the desired terminal 3 state
  // "WCH4": write the desired terminal 4 state
  // "WIL1": write the terminal 1 current shutdown limit (mA)
  // "WIL2": write the terminal 2 current shutdown limit (mA)
  // "WIL3": write the terminal 3 current shutdown limit (mA)
  // "WIL4": write the terminal 4 current shutdown limit (mA)

