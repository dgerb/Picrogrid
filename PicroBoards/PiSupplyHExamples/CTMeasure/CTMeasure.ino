/*
  CT Measure

  Uses the PiSupply analog GPIO pins to measure current from a hall current transducer
  and package the results for a Pi to download.

  For DC current transducers, we use HSTS016L/A and a QNHCK2-16, both rated at 20A/2.5V+/-2V

  CT1 Setup:
  Power (+) connects to A1 or A0 VCC
  Power (-) connects to A1 or A0 GND
  Output connects to A1 PINS
  Ref connects to A0 PINS

  CT7 Setup:
  Power (+) connects to A7 or A6 VCC
  Power (-) connects to A7 or A6 GND
  Output connects to A7 PINS
  Ref connects to A6 PINS

  created 2 October 2025
  by Daniel Gerber
*/

#include "PiSupplyH.h"
PiSupplyH pisupply;

const int CTMV2MA = 10; // multiply to convert the CT mV output to the equivlant mA value: 20000mA / 2000mV

// the setup function runs once when you press reset or power the board
void setup() {
  pisupply.initialize();
  pisupply.setChGPIO(HIGH); // turn on power to GPIO pins, which will power the CTs
    // use setCh5V(), setCh12V(), setChPi() similarly as desired for your application
  pisupply.setCh5V(HIGH);

  // set up UART and I2C command support
  // add the command interpretation function below as a listener for serial commands
  pisupply.addCommandCallback(&interpretRXCommand);
  // initialize UART communication at 9600 bits per second
  pisupply.startUART(); // can optionally specify baud rate with an argument
  // initialize I2C communciation and register receive and request callback functions
  // requires using lambda functions; it's clunky but mandatory since Wire.h doesn't allow member callbacks
  // just copy the following code for any I2C projects
  ReceiveEventI2C receiveEvent = [] (int howMany) {pisupply.receiveEventI2C(howMany);};
  RequestEventI2C requestEvent = [] () {pisupply.requestEventI2C();};
  pisupply.startI2C(5, receiveEvent, requestEvent); // first argument is the slave device address (max 127)

  pisupply.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms
}

void loop() {
  pisupply.updateSensors();
}

// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  pisupply.readUART(); // if using UART, check every cycle if there are new characters in the UART buffer
}

void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RVA1") == 0) { // read voltage on Analog pin 1
    sprintf(pisupply.getTXBuffer(receiveProtocol), "WVA1:%u", pisupply.getAnalog(1));
    pisupply.respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RVA0") == 0) { // read voltage on Analog pin 0
    sprintf(pisupply.getTXBuffer(receiveProtocol), "WVA0:%u", pisupply.getAnalog(0));
    pisupply.respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RVA7") == 0) { // read voltage on Analog pin 7
    sprintf(pisupply.getTXBuffer(receiveProtocol), "WVA7:%u", pisupply.getAnalog(7));
    pisupply.respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RVA6") == 0) { // read voltage on Analog pin 6
    sprintf(pisupply.getTXBuffer(receiveProtocol), "WVA6:%u", pisupply.getAnalog(6));
    pisupply.respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RIA1") == 0) { // read CT current calculated from Analog pin 1
    Serial.println(pisupply.getAnalog(1));
    Serial.println(pisupply.getAnalog(0));
    Serial.println(pisupply.getAnalog(1) - pisupply.getAnalog(0));
    Serial.println(CTMV2MA*(pisupply.getAnalog(1) - pisupply.getAnalog(0)));
    sprintf(pisupply.getTXBuffer(receiveProtocol), "WIA1:%i", convertCT(pisupply.getAnalog(1), pisupply.getAnalog(0)));
    pisupply.respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RIA7") == 0) { // read CT current calculated from Analog pin 7
    sprintf(pisupply.getTXBuffer(receiveProtocol), "WIA7:%i", convertCT(pisupply.getAnalog(7), pisupply.getAnalog(6)));
    pisupply.respondToMaster(receiveProtocol);
  } else if (strcmp(command, "RFN") == 0) {
    readFileName(value, receiveProtocol);
  }
}

// converts analog pin voltage to current through current transducer.
int convertCT(int ctoutvolts, int ctrefvolts) {
  return CTMV2MA*(ctoutvolts - ctrefvolts);
}

// outputs the file name to serial
void readFileName(const char* valueStr, int receiveProtocol) {
  sprintf(pisupply.getTXBuffer(receiveProtocol), "WFN:%s", "CTMeasure.ino");
  pisupply.respondToMaster(receiveProtocol);
}





// "RV48" // read 48V input bus voltage
// "RV12" // read 12V bus voltage
// "RVCC" // read the ~5V VCC bus voltage
// "RCPI" // read state of the Pi power channel
// "RC5V" // read state of the 5V output power channel
// "RCGP" // read state of the GPIO power channel
// "RC12V" // read state of the 12V output power channel
// "WCPI" // write the desired Pi power channel state
// "WC5V" // write the desired 5V output power channel state
// "WCGP" // write the desired GPIO power channel state
// "WC12V" // write the desired 12V output power channel state


