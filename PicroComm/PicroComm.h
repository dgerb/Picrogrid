/*
  PicroComm.h - Library for communications between Pi and Picrogrid boards
  Created by Daniel Gerber, 7/17/22
  Released into the public domain.
*/

// Open Arduino IDE, select “Sketch” > “Include Library” > “Add .ZIP Library…”, and browse to find your .zip archive.
// https://docs.arduino.cc/learn/contributions/arduino-creating-library-guide

#ifndef PicroComm_h
#define PicroComm_h

#include "Arduino.h"

// In Arduino IDE, go to Sketch -> Include Library -> Manage Libraries



class PicroComm
{
  public:
    PicroComm(); // constructor
    // Communications initialization
    void PicroComm::startUART(int baud); // start serial communications
    void PicroComm::startUART(); // start serial communications
    // UART
    bool PicroComm::readUART(); // read in from the UART buffer and store it, returns true if \n detected
    char * PicroComm::getUARTBuffer(); // get the stored UART buffer
  private:
    char _rxBuffer [32];
    int _rxCnt = 0;
};

#endif
