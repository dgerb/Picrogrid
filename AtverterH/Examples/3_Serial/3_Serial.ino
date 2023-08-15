/*
  SerialTest

  A test program that shows how to set up the Atverter's serial communication, including
  UART and I2C. Standard commands include reading sensed or stored values such as terminal voltages,
  currents and FET temperatures, in addition to writing to and setting various software-defined limits.
  This program also shows how to create your own command registers by registering a command interpretation
  function as a callback listener.

  created 8/14/23 by Daniel Gerber
*/

#include <AtverterH.h>

AtverterH atverter;

long slowInterruptCounter = 0;
char helloMessage[16] = "Hello World!";

void setup() {
  atverter.initializeSensors();
  // add the command interpretation function below as a listener for serial commands
  atverter.addCommandCallback(&interpretRXCommand);
  // initialize UART communication at 9600 bits per second
  atverter.startUART(); // can optionally specify baud rate with an argument
  // initialize I2C communciation and register receive and request callback functions
  // requires using lambda functions; it's clunky but mandatory since Wire.h doesn't allow member callbacks
  // just copy the following code for any I2C projects
  ReceiveEventI2C receiveEvent = [] (int howMany) {atverter.receiveEventI2C(howMany);};
  RequestEventI2C requestEvent = [] () {atverter.requestEventI2C();};
  atverter.startI2C(8, receiveEvent, requestEvent); // first argument is the slave device address (max 127)
}

void loop() {
  atverter.readUART(); // if using UART, check every cycle if there are new characters in the UART buffer

  // everything below is just for standard sensor updating
  atverter.updateVISensors(); // read voltage and current sensors and update moving average
  slowInterruptCounter++; // in this example, do some special stuff every 1 second (1000ms)
  if (slowInterruptCounter > 5000) {
    slowInterruptCounter = 0;
    atverter.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverter.updateTSensors(); // occasionally read thermistors and update temperature moving average
  }
}

// command interpretation function specific to this program
// compares the incoming command string with preset register names and calls the appropriate functions
void interpretRXCommand(char* command, char* value, int receiveProtocol) {
  if (strcmp(command, "RFN") == 0) {
    readFileName(value, receiveProtocol);
  } else if (strcmp(command, "RAUT") == 0) {
    readAuthor(value, receiveProtocol);
  } else if (strcmp(command, "RHM") == 0) {
    readHelloMessage(value, receiveProtocol);
  } else if (strcmp(command, "WHM") == 0) {
    writeHelloMessage(value, receiveProtocol);
  }
}

// sample function that responds to RFN
void readFileName(const char* valueStr, int receiveProtocol) {
  sprintf(atverter.getTXBuffer(receiveProtocol), "WFN:%d%s", 3, "_Serial.ino");
  atverter.respondToMaster(receiveProtocol);
}

// sample function that responds to RAUT
void readAuthor(const char* valueStr, int receiveProtocol) {
  sprintf(atverter.getTXBuffer(receiveProtocol), "WAUT:%s", "Daniel Gerber");
  atverter.respondToMaster(receiveProtocol);
}

// sample function that responds to RHM, reports a char array stored in this program
void readHelloMessage(const char* valueStr, int receiveProtocol) {
  sprintf(atverter.getTXBuffer(receiveProtocol), "WHM:%s", helloMessage);
  atverter.respondToMaster(receiveProtocol);
}

// sample function that responds to WHM, writes to a char array stored in this program
void writeHelloMessage(const char* valueStr, int receiveProtocol) {
  strcpy(helloMessage, valueStr);
  sprintf(atverter.getTXBuffer(receiveProtocol), "WHM:=%s", helloMessage);
  atverter.respondToMaster(receiveProtocol);
}
