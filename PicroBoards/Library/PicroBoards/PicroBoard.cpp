/*
  PicroBoard.cpp - Base class for all Picrogrid boards. Includes communication functions.
  Created by Daniel Gerber, 7/17/22
  Released into the public domain.
*/

// Open Arduino IDE, select “Sketch” > “Include Library” > “Add .ZIP Library…”, and browse to find your PicroBoards.zip

#include "PicroBoard.h"

PicroBoard::PicroBoard() {
}

// adds a serial command callback to the array
void PicroBoard::addCommandCallback(CommandCallback callback) {
  if (_commandCallbacksEnd < COMMANDCALLBACKSMAXLENGTH) {
    _commandCallbacks[_commandCallbacksEnd] = callback;
    _commandCallbacksEnd++;
  }
}

// parses the given rxBuffer and calls the appropriate valueFunction cooresponding to the command
void PicroBoard::parseRXLine(char* buffer, int receiveProtocol) {
  char* command = strtok(buffer, ":");
  char* value = strtok(NULL, "\n");
  interpretRXCommand(command, value, receiveProtocol);
}

// processes RX command, override it with something useful to the particular board
void PicroBoard::interpretRXCommand(char* command, char* value, int receiveProtocol) {
  // program should never get to here
}

void PicroBoard::respondToMaster(int receiveProtocol) { // responds to the raspberry Pi
  switch (receiveProtocol) {
    case UART_INDEX:
      Serial.println(_txBuffer[UART_INDEX]);
      _txBuffer[UART_INDEX][0] = '\0';
      break;
    case I2C_INDEX:
      // Serial.println(_txBuffer[I2C_INDEX]); // for debugging only (adds a bad delay otherwise)
      // do nothing, wait for next I2C request from master
      break;
    default:
      // do nothing
      break;
  }
}

// get a pointer to the indexed stored transmit buffer
char * PicroBoard::getTXBuffer(int commIndex) {
  return _txBuffer[commIndex];
}

// start serial communications
void PicroBoard::startUART(long baud) {
  Serial.begin(baud);
}

// start serial communications
void PicroBoard::startUART() {
  startUART(38400);
}

// read in from the UART buffer and store it, returns true if \n detected
void PicroBoard::readUART() {
  while (Serial.available())
  {
    char c = Serial.read();
    // _rxBufferUART[_rxCntUART++] = c;
    _rxBufferUART[_rxCntUART] = c;
    _rxCntUART++;
    //
    if ((c == '\n') || (_rxCntUART == COMMBUFFERSIZE-1))
    {
        _rxBufferUART[_rxCntUART] = '\0';
        _rxCntUART = 0;
        parseRXLineUART();
    }
  }
}

// get the stored UART buffer 
char * PicroBoard::getRXBufferUART() {
  return _rxBufferUART;
}

// parses the current UART rxBuffer
void PicroBoard::parseRXLineUART() {
  parseRXLine(_rxBufferUART, UART_INDEX);
}

// start serial communications
void PicroBoard::startI2C(int address, ReceiveEventI2C receiveCallback, RequestEventI2C requestCallback) {

// void PicroBoard::startI2C(int address, void (*callback)(int)) {
  Wire.begin(address);
  Wire.onReceive(receiveCallback);
  Wire.onRequest(requestCallback);
}

// function to handle when an I2C message comes in
void PicroBoard::receiveEventI2C(int howMany) {
  // Serial.println("received");
  for (int i = 0; i < howMany; i++) {
    _rxBufferI2C[i] = Wire.read();
    _rxBufferI2C[i + 1] = '\0'; //add null after ea. char
  }
  //RPi first byte is cmd byte so shift everything to the left 1 pos so temp contains our string
  for (int i = 0; i < howMany; ++i)
    _rxBufferI2C[i] = _rxBufferI2C[i + 1];
  parseRXLineI2C();
}

void PicroBoard::requestEventI2C() {
  // Serial.println("requested");
  Wire.write(_txBuffer[I2C_INDEX]);
  _txBuffer[I2C_INDEX][0] = '\0';
}

char * PicroBoard::getRXBufferI2C() { // get the stored I2C buffer
  return _rxBufferI2C;
}

// parses the current I2C rxBuffer
void PicroBoard::parseRXLineI2C() {
  parseRXLine(_rxBufferI2C, I2C_INDEX);
}


