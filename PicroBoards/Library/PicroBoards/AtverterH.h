/*
  AtverterH.h - Library for AtverterH pin and function mapping
  Created by Daniel Gerber, 9/19/22
  Released into the public domain.
*/

// Open Arduino IDE, select “Sketch” > “Include Library” > “Add .ZIP Library…”, and browse to find your .zip archive.

#ifndef AtverterH_h
#define AtverterH_h

// #include "Arduino.h"
#include "PicroBoard.h"

// In Arduino IDE, go to Sketch -> Include Library -> Manage Libraries
#include <FastPwmPin.h> // Add zip library from: https://github.com/maxint-rd/FastPwmPin
#include <TimerOne.h> // In Library Manager, search for "TimerOne"
#include <avdweb_AnalogReadFast.h> // In Library Manager, search for "AnalogReadFast"

// pins for turning on the LEDs
const int LEDG_PIN = 2; // PD2
const int LEDY_PIN = 4; // PD4

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
const int T1_PIN = A1; // PC1, Thermistor resistor divider
const int T2_PIN = A0; // PC0, Thermistor resistor divider

// sensor index enumerator for convenience
enum SensorIndex 
{   V1_INDEX = 0,
    V2_INDEX, 
    I1_INDEX,
    I2_INDEX,
    T1_INDEX,
    T2_INDEX,
    NUM_SENSORS
};

// DC-DC operation modes enumerator for convenience
enum DCDCModes
{   BUCK = 0,
    BOOST, 
    BUCKBOOST,
    NUM_DCDCMODES
};

// output modes enumerator for convenience
enum OuputModes
{   CV = 0,
    CC,
    NUM_OUTPUTMODES
};

// moving average sample window length for sensors
// best to use lengths of 1, 2, 4, or 8 to avoid overflow and optimize division
const int AVERAGE_WINDOW_MAX = 8;

// NCP15WF104F03RC thermistor look up table (ADC value, temperature °C)
const int TTABLE[14][2] = {
  139, 10,
  211, 20,
  301, 30,
  404, 40,
  510, 50,
  612, 60,
  658, 65,
  701, 70,
  740, 75,
  776, 80,
  807, 85,
  835, 90,
  859, 95,
  880, 100
};

class AtverterH : public PicroBoard
{
  public:
    AtverterH(); // constructor
  // Atmega initialization
    void setupPinMode(); // sets appropriate pinMode() for each const pin
    void initializeSensors(); // initialize sensor average array to sensor read
    void initializeSensors(int avgWindowLength); // initialize sensor average array to sensor read
    void initializeInterruptTimer(long periodus, // starts periodic control timer
      void (*interruptFunction)(void)); // inputs: period (ms), controller function reference
    void enableGateDrivers(); // resets protection latch, enabling the gate drivers
  // duty cycle
    void setDutyCycle(int dutyCycle); // sets duty cycle (0 to 100)
    void setDutyCycleFloat(float dutyCycleFloat); // sets duty cycle (0.0 to 1.0)
    int getDutyCycle(); // gets the current duty cycle (0 to 100)
    float getDutyCycleFloat(); // gets the current duty cycle (0.0 to 1.0)
  // alternate drive signal
    void checkBootstrapRefresh(); // check bootstrap counter to see if need to refresh caps
    void refreshBootstrap(); // refresh the bootstrap capacitors and reset bootstrap counter
    void applyHoldHigh1(); // set gate driver 1 to use an always-high alternate signal
    void applyHoldHigh2(); // set gate driver 2 to use an always-high alternate signal
    void removeHold(); // sets both gate drivers to use the primary pwm signal
  // raw sensor values
    void updateVCC(); // updates stored VCC value based on an average
    void updateVISensors(); // updates voltage and current sensor averages
    void updateTSensors(); // updates thermistor sensor averages
    int readVCC(); // returns the sampled VCC voltage in milliVolts
    int getRawV1(); // gets Terminal 1 voltage ADC value (0 to 1023)
    int getRawV2(); // gets Terminal 2 voltage ADC value (0 to 1023)
    int getRawI1(); // gets Terminal 1 current ADC value (0 to 1023)
    int getRawI2(); // gets Terminal 2 current ADC value (0 to 1023)
    int getRawT1(); // gets Thermistor 1 ADC value (0 to 1023)
    int getRawT2(); // gets Thermistor 1 ADC value (0 to 1023)
  // fully-formatted sensors
    int getVCC(); // returns the averaged VCC value
    unsigned int getV1(); // returns the averaged V1 mV value
    unsigned int getV2(); // returns the averaged V2 mV value
    int getI1(); // returns the averaged I1 mA value
    int getI2(); // returns the averaged I2 mA value
    int getT1(); // returns the averaged Thermistor 1 value (°C)
    int getT2(); // returns the averaged Thermistor 2 value (°C)
  // diagnostics
    void setLED(int led, int state); // sets an LED to HIGH or LOW
    void setLEDY(int state); // sets yellow LED to HIGH or LOW
    void setLEDG(int state); // sets green LED to HIGH or LOW
  // safety
    void shutdownGates(); // immediately triggers the gate shutdown
    bool isGateShutdown(); // returns true if the gate shutdown signal is currently latched
    void setCurrentShutdown(int current); // sets the upper current shutoff limit in mA
    void checkCurrentShutdown(); // checks if last sensed current is greater than current limit
    void setThermalShutdown(int temperature); // sets the upper temperature shutoff in °C
    void checkThermalShutdown(); // checks if last sensed current is greater than thermal limit
  // conversion utility functions
    unsigned int raw2mV(int raw); // converts ADC reading to mV voltage scaled by resistor divider
    int raw2mVADC(int raw); // converts ADC reading to mV voltage at ADC
    int raw2mA(int raw); // converts raw ADC current sense output to mA
    int rawSigned2mA(int raw); // converts raw ADC current sense output to mA
    int raw2degC(int raw); // converts raw ADC current sense output to °C
    int mV2raw(unsigned int mV); // converts a mV value to raw 10-bit form
    int mA2raw(int mA); // converts a mA value to raw 10-bit form
    int mA2rawSigned(int mA); // converts a mA value to raw 10-bit form centered around 0
  // communications
    void interpretRXCommand(char* command, char* value, int receiveProtocol) override; // process RX command
  // legacy functions
    void startPWM(); // replaced by enableGateDrivers()
    void initializePWMTimer(); // not needed with FastPWM library
  private:
    int _dutyCycle = 50; // the most recently set duty cycle (0 to 100)
    long _bootstrapCounter; // counter to refresh the gate driver bootstrap caps
    long _bootstrapCounterMax; // reset value for bootstrap counter
    int _averageWindow = 4; // average window length (< AVERAGE_WINDOW_MAX)
    int _sensorAverages[NUM_SENSORS]; // array raw sensor moving averages
    int _sensorPast[AVERAGE_WINDOW_MAX][NUM_SENSORS]; // array raw sensor moving averages
    int _vcc; // stored value of vcc measured at start up
    int _currentLimitAmplitudeRaw = 375; // the upper raw (0 to 1023) current limit before gate shutoff
    int _thermalLimitC = 170; // the upper °C thermal limit before gate shutoff
    void updateSensorRaw(int index, int sample); // updates the raw averaged sensor value
};

#endif
