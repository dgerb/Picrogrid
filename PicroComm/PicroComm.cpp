/*
  PicroComm.h - Library for communications between Pi and Picrogrid boards
  Created by Daniel Gerber, 7/17/22
  Released into the public domain.
*/

// https://docs.arduino.cc/learn/contributions/arduino-creating-library-guide

#include "PicroComm.h"

PicroComm::PicroComm() {
}

// start serial communications
void PicroComm::startUART(int baud) {
  Serial.begin(baud);
}

// start serial communications
void PicroComm::startUART() {
  startUART(9600);
}

// read in from the UART buffer and store it, returns true if \n detected
bool PicroComm::readUART() {
  while (Serial.available())
  {
    char c = Serial.read();
    _rxBuffer[_rxCnt++] = c;
    if ((c == '\n') || (_rxCnt == sizeof(rxBuffer)-1))
    {
        _rxBuffer[_rxCnt] = '\0';
        _rxCnt = 0;
        return true;
    }
  }
  return false;
}

// get the stored UART buffer 
char * PicroComm::getUARTBuffer() {
  return _rxBuffer;
}
// typical usage on Picrogrid boards:
// buff = getUARTBuffer();
// char * command = strtok(buff, ","); // first arg is command: 'R' or 'W'
// char * key = strtok(NULL, ","); // second arg is key or register
// char * value = strtok(NULL, "\n");  // third arg is value
// if((command[0]=='R') && (strcmp(key, "V1") == 0)) {
//   sprintf(temp, "W,%s,%d", key, v1); // ex. "W,V1,5000"
//   Serial.println(temp); }






