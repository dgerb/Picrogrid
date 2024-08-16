/*
  Smart Panel

  Allows serial control over channel states from a Pi or serial console, with the ability to read
  bus voltage, channel current, and channel state

  modified 15 August 2024
  by Daniel Gerber
*/

#include <MicroPanelH.h>

MicroPanelH micropanel;
long slowInterruptCounter = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  micropanel.setupPinMode(); // set pins to input or output
  micropanel.initializeSensors(); // set filtered sensor values to initial reading
  micropanel.setCurrentLimit1(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit2(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit3(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit4(6000); // set gate shutdown at 6A peak current 

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

  slowInterruptCounter++; // in this example, do some special stuff every 1 second (1000ms)
  if (slowInterruptCounter > 1000) {
    slowInterruptCounter = 0;
    micropanel.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    // prints channel state (as binary), VCC, VBus, I1, I2, I3, I4 to the serial console of attached computer
  //   Serial.print("State: ");
  //   Serial.print(micropanel.getCh1());
  //   Serial.print(micropanel.getCh2());
  //   Serial.print(micropanel.getCh3());
  //   Serial.print(micropanel.getCh4());
  //   Serial.print(", VCC = ");
  //   Serial.print(micropanel.getVCC());
  //   Serial.print("mV, VBus = ");
  //   Serial.print(micropanel.getVBus());
  //   Serial.print("mV, I1 = ");  
  //   Serial.print(micropanel.getI1());
  //   Serial.print("mA, I2 = ");  
  //   Serial.print(micropanel.getI2());
  //   Serial.print("mA, I3 = ");  
  //   Serial.print(micropanel.getI3());
  //   Serial.print("mA, I4 = ");  
  //   Serial.print(micropanel.getI4());
  //   Serial.println("mA");
  }
}

void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RFN") == 0) {
    readFileName(value, receiveProtocol);
  }
}

// outputs the file name to serial
void readFileName(const char* valueStr, int receiveProtocol) {
  sprintf(micropanel.getTXBuffer(receiveProtocol), "WFN:%s", "4_SmartPanel.ino");
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

