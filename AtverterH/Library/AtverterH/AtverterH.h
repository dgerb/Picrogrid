/*
  AtverterH.h - Library for AtverterH pin and function mapping
  Created by Daniel Gerber, 9/19/22
  Released into the public domain.
*/

// Open Arduino IDE, select “Sketch” > “Include Library” > “Add .ZIP Library…”, and browse to find your .zip archive.
// https://docs.arduino.cc/learn/contributions/arduino-creating-library-guide

#ifndef AtverterH_h
#define AtverterH_h

#include "Arduino.h"

// In Arduino IDE, go to Sketch -> Include Library -> Manage Libraries
#include <FastPwmPin.h> // Add zip library from: https://github.com/maxint-rd/FastPwmPin
#include <TimerOne.h> // In Library Manager, search for "TimerOne"

// pins for turning on the LEDs
const int LEDG_PIN = 2; // PD2
const int LEDY_PIN = 19; // PD4

// primary gate signal pin, with FastPWM duty cycle
const int PWM_PIN = 3; // PD3
// From FastPWM Readme:
//  Pins 10 and 9: 16-bit Timer 1, pin 9 only supports toggle mode (50% PWM)
//  Pins 3 and 11: 8-bit Timer 2, pin 11 only supports toggle mode (50% PWM)

// alternate gate signal pin, usually used for buck or boost modes
const int ALT_PIN = 8; // PB0

// multiplexer control pins for Terminal 1 and 2. LOW: PWM_PIN, HIGH: ALT_PIN
const int VCTRL1_PIN = 9; // PB1
const int VCTRL2_PIN = 7; // PD7

// pin to reset the protection circuit, unlatching gate shutdown, enabling the gate driver
const int PRORESET_PIN = 5; // PD5
// gate shutdown diagnostic pin. If HIGH, gate shutdown is latched
const int GATESD_PIN = 6; // PD6

// pins for voltage, current, and temperature sensing
const int V1_PIN = A3; // PC3, Terminal 1 voltage
const int V2_PIN = A7; // ADC7, Terminal 2 voltage
const int I1_PIN = A2; // PC2, Terminal 1 current sensor output
const int I2_PIN = A6; // ADC6, Terminal 2 current sensor output
const int THERM1_PIN = A1; // PC1
const int THERM2_PIN = A0; // PC0

// moving average window length for voltages and currents
const int AVGWIN = 10;

class AtverterH
{
  public:
    AtverterH(); // constructor
  // Atmega initialization
    void setupPinMode(); // sets appropriate pinMode() for each const pin
    void initializeInterruptTimer(int periodms, // starts periodic control timer
      void (*interruptFunction)(void)); // inputs: period (ms), controller function reference
    void enableGateDrivers(); // resets protection latch, enabling the gate drivers
 // duty cycle
    void setDutyCycle(int dutyCycle); // sets duty cycle (0 to 100)
    void setDutyCycleFloat(float dutyCycleFloat); // sets duty cycle (0.0 to 1.0)
    int getDutyCycle(); // gets the current duty cycle (0 to 100)
    float getDutyCycleFloat(); // gets the current duty cycle (0.0 to 1.0)
  // voltages and current sensing
    int getRawV1(); // gets Terminal 1 voltage (0 to 1023)
    int getV1(); // gets Terminal 1 voltage (mV) measured at ADC
    unsigned int getActualV1(); // gets Terminal 1 voltage (mV) at terminal
    int getRawI1(); // gets Terminal 1 current (0 to 1023)
    int getI1(); // gets Terminal 1 current (mA) (-5000 to 5000)
    int getRawV2(); // gets Terminal 2 voltage (0 to 1023)
    int getV2(); // gets Terminal 2 voltage (mV) measured at ADC
    unsigned int getActualV2(); // gets Terminal 2 voltage (mV) at terminal
    int getRawI2(); // gets Terminal 2 current (0 to 1023)
    int getI2(); // gets Terminal 2 current (mA) (-5000 to 5000)
  // diagnostics
    void setLED(int led, int state); // sets an LED to HIGH or LOW
    void setLEDY(int state); // sets yellow LED to HIGH or LOW
    void setLEDG(int state); // sets green LED to HIGH or LOW
    bool isGateShutdown(); // returns true if the gate shutdown signal is currently latched
  // legacy functions
    void startPWM(); // replaced by enableGateDrivers()
    void initializePWMTimer(); // not needed with FastPWM library
  private:
    int _dutyCycle = 50; // the most recently set duty cycle (0 to 100)
    int raw2mV(int raw); // converts raw ADC voltage to mV
    unsigned int raw2mVActual(int raw); // also scales raw2mV by resistor divider
    int raw2mA(int raw); // converts raw ADC current sense output to mA
    long bootstrapCounter; // counter to refresh the gate driver bootstrap caps
};


#endif
