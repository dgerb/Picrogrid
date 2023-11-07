/*
  PicroBoard.h - Base class for all Picrogrid boards. Includes communication functions.
  Created by Daniel Gerber, 7/17/22
  Released into the public domain.
*/

// Open Arduino IDE, select “Sketch” > “Include Library” > “Add .ZIP Library…”, and browse to find your PicroBoards.zip

#ifndef PicroBoard_h
#define PicroBoard_h

#include "Arduino.h"
#include <Wire.h>

// In Arduino IDE, go to Sketch -> Include Library -> Add .ZIP Library

// Function template for interpreting command strings: (char* command, char* value, int receiveProtocol)
typedef void (*CommandCallback)(const char*, const char*, int);

// I2C function templates that work with Wire.h library
typedef void (*ReceiveEventI2C)(int);
typedef void (*RequestEventI2C)();

// communications protocol enumerator
enum CommunicationsProtocol
{   UART_INDEX = 0,
    I2C_INDEX,
    NUM_COMM_MODULES
};

const int COMMBUFFERSIZE = 16; // length of all character buffers (both receive and transmit)
const int COMMANDCALLBACKSMAXLENGTH = 10; // max length of command callback array

class PicroBoard
{
  public:
    PicroBoard(); // constructor
    // Communication functions for commands parsing and interpretation
    void addCommandCallback(CommandCallback callback); // adds a serial command callback to the array
    void parseRXLine(char* buffer, int receiveProtocol); // parses given rxBuffer and calls appropriate valueFunction
    virtual void interpretRXCommand(char* command, char* value, int receiveProtocol); // processes RX command, override it
    void respondToMaster(int receiveProtocol); // responds to the raspberry Pi
    char * getTXBuffer(int commIndex); // get a pointer to the indexed stored transmit buffer
    // Communication functions for UART
    void startUART(long baud); // start serial communications
    void startUART(); // start serial communications
    void readUART(); // read in from the UART buffer and store it, and parse it if \n detected
    char * getRXBufferUART(); // get a pointer to the stored UART receive buffer
    void parseRXLineUART(); // parses the current UART rxBuffer
    // Communication functions for I2C
    void startI2C(int address, ReceiveEventI2C receiveCallback, RequestEventI2C requestCallback); // start I2C
    void receiveEventI2C(int howMany); // function to handle when an I2C transmit message comes in
    void requestEventI2C(); // function to handle when an I2C request message comes in
    char * getRXBufferI2C(); // get a pointer to the stored I2C receive buffer
    void parseRXLineI2C(); // parses the current I2C rxBuffer
  protected:
    CommandCallback _commandCallbacks[COMMANDCALLBACKSMAXLENGTH]; // callback listeners to call at end of interpretRXCommand
    int _commandCallbacksEnd = 0; // moving end index of _commandCallbacks
    char _rxBufferUART [COMMBUFFERSIZE]; // receive holding buffer for UART packets
    int _rxCntUART = 0; // end index of _rxBufferUART
    char _rxBufferI2C [COMMBUFFERSIZE]; // receive holding buffer for I2C packets
    int _rxCntI2C = 0; // end index of _rxBufferUART
    char _txBuffer [NUM_COMM_MODULES][COMMBUFFERSIZE]; // transmit holding buffer prior to transmission  
};

#endif
