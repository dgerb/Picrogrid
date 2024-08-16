/*
  MicroPanelH.h - Library for MicroPanelH pin and function mapping
  Created by Daniel Gerber, 8/14/24
  Released into the public domain.
*/

// Open Arduino IDE, select “Sketch” > “Include Library” > “Add .ZIP Library…”, and browse to find your .zip archive.

#ifndef MicroPanelH_h
#define MicroPanelH_h

#include "PicroBoard.h"

// In Arduino IDE, go to Sketch -> Include Library -> Manage Libraries
#include <TimerOne.h> // In Library Manager, search for "TimerOne"
#include <avdweb_AnalogReadFast.h> // In Library Manager, search for "AnalogReadFast"

// pins for turning on the LEDs
const int LED2_PIN = 2; // PD2
const int LED1_PIN = 4; // PD4

// pins for channel signals
const int CH1_PIN = 9; // PB1
const int CH2_PIN = 8; // PB0
const int CH3_PIN = 3; // PD3
const int CH4_PIN = 7; // PD7

// pins for current and voltage sensing
const int VBUS_PIN = A3; // PC3, Bus voltage
const int I1_PIN = A2; // PC2, Terminal 1 current sensor output
const int I2_PIN = A1; // PC1, Terminal 2 current sensor output
const int I3_PIN = A0; // PC0, Terminal 3 current sensor output
const int I4_PIN = A7; // ADC7, Terminal 4 current sensor output

// sensor index enumerator for convenience
enum SensorIndex 
{   VBUS_INDEX = 0,
    I1_INDEX, 
    I2_INDEX,
    I3_INDEX,
    I4_INDEX,
    NUM_SENSORS
};

// moving average sample window length for sensors. Best to use powers of 2 so as to optimize division
//  use this before #include to override in the .ino file: #define XXX YY
//  e.g. to set current averaging window to size 32: #define SENSOR_I_WINDOW_MAX 32
#ifndef SENSOR_V_WINDOW_MAX
#define SENSOR_V_WINDOW_MAX 4
#endif
#ifndef SENSOR_I_WINDOW_MAX
#define SENSOR_I_WINDOW_MAX 16
#endif
constexpr int AVERAGE_WINDOW_MAX[NUM_SENSORS] = {
  SENSOR_V_WINDOW_MAX,
  SENSOR_I_WINDOW_MAX,
  SENSOR_I_WINDOW_MAX,
  SENSOR_I_WINDOW_MAX,
  SENSOR_I_WINDOW_MAX};

class MicroPanelH : public PicroBoard
{
  public:
    MicroPanelH(); // constructor
  // Atmega initialization
    void initialize(); // default initialization routine
    void setupPinMode(); // sets appropriate pinMode() for each const pin
    void initializeSensors(); // initialize sensor average array to sensor read
    void initializeSensors(int avgWindowLength); // initialize sensor average array to sensor read
    void initializeInterruptTimer(long periodus, // starts periodic control timer
      void (*interruptFunction)(void)); // inputs: period (ms), controller function reference
  // channel state set and get
    void setCh1(int state); // sets the state of Channel 1
    void setCh2(int state); // sets the state of Channel 2
    void setCh3(int state); // sets the state of Channel 3
    void setCh4(int state); // sets the state of Channel 4
    void setChannel(int chPin, int state, int holdProtectMicroseconds); // set channel and hold a bit
    void setDefaultInrushOverride(int holdProtectMicroseconds); // set default switch hold delay
    int getCh1(); // gets the state of Channel 1
    int getCh2(); // gets the state of Channel 2
    int getCh3(); // gets the state of Channel 3
    int getCh4(); // gets the state of Channel 4
  // raw sensor values
    void updateVCC(); // updates stored VCC value based on an average
    void updateVISensors(); // updates voltage and current sensor averages
    int readVCC(); // returns the sampled VCC voltage in milliVolts
    int getRawVBus(); // gets bus voltage ADC value (0 to 1023)
    int getRawI1(); // gets Terminal 1 current ADC value (0 to 1023)
    int getRawI2(); // gets Terminal 2 current ADC value (0 to 1023)
    int getRawI3(); // gets Terminal 3 current ADC value (0 to 1023)
    int getRawI4(); // gets Terminal 4 current ADC value (0 to 1023)
  // fully-formatted sensors
    int getVCC(); // returns the averaged VCC value
    unsigned int getVBus(); // returns the averaged VBus mV value
    int getI1(); // returns the averaged I1 mA value
    int getI2(); // returns the averaged I2 mA value
    int getI3(); // returns the averaged I3 mA value
    int getI4(); // returns the averaged I4 mA value
  // diagnostics
    void setLED(int led, int state); // sets an LED to HIGH or LOW
    void setLED1(int state); // sets LED1 (yellow) to HIGH or LOW
    void setLED2(int state); // sets LED2 (green) to HIGH or LOW
  // current limiting and safety
    void shutdownChannels(); // immediately triggers all channels to shutdown
    // void setBusVoltageShutdown(int voltage);
    // void checkBusVoltageShutdown();
    void setCurrentLimit1(int current); // sets the terminal 1 current shutoff limit in mA, max 7500 mA
    void setCurrentLimit2(int current); // sets the terminal 2 current shutoff limit in mA, max 7500 mA
    void setCurrentLimit3(int current); // sets the terminal 3 current shutoff limit in mA, max 7500 mA
    void setCurrentLimit4(int current); // sets the terminal 4 current shutoff limit in mA, max 7500 mA
    void checkCurrentShutdown(); // checks if last sensed current is greater than current limit
  // conversion utility functions
    unsigned int raw2mV(int raw); // converts ADC reading to mV voltage scaled by resistor divider
    int raw2mVADC(int raw); // converts ADC reading to mV voltage at ADC
    int raw2mA(int raw); // converts raw ADC current sense output to mA
    int mV2raw(unsigned int mV); // converts a mV value to raw 10-bit form
    int mA2raw(int mA); // converts a mA value to raw 10-bit form
  // communications
    void interpretRXCommand(char* command, char* value, int receiveProtocol) override; // process RX command
  private:
    // sensors and averaging
    int _sensorAverages[NUM_SENSORS]; // array raw sensor moving averages
    long _sensorAccumulators[NUM_SENSORS];
    int _sensorIterators[NUM_SENSORS];
    int _sensorPastVBus[AVERAGE_WINDOW_MAX[VBUS_INDEX]]; // array raw sensor moving averages
    int _sensorPastI1[AVERAGE_WINDOW_MAX[I1_INDEX]]; // array raw sensor moving averages
    int _sensorPastI2[AVERAGE_WINDOW_MAX[I2_INDEX]]; // array raw sensor moving averages
    int _sensorPastI3[AVERAGE_WINDOW_MAX[I3_INDEX]]; // array raw sensor moving averages
    int _sensorPastI4[AVERAGE_WINDOW_MAX[I4_INDEX]]; // array raw sensor moving averages
    int _vcc; // stored value of vcc measured at start up and/or periodically
    int _currentLimitAmplitudeRaw1 = 444; // the upper raw (0 to 1023) current limit before gate shutoff
    int _currentLimitAmplitudeRaw2 = 444; // the upper raw (0 to 1023) current limit before gate shutoff
    int _currentLimitAmplitudeRaw3 = 444; // the upper raw (0 to 1023) current limit before gate shutoff
    int _currentLimitAmplitudeRaw4 = 444; // the upper raw (0 to 1023) current limit before gate shutoff
    int _holdProtectMicros = 20; // default hold delay intended to override inrush current on switch
    // functions
    void updateSensorRaw(int index, int sample); // updates the raw averaged sensor value
};

#endif
