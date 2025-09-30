/*
  PiSupplyH.h - Library for PiSupplyH pin and function mapping
  Created by Daniel Gerber, 9/29/25
  Released into the public domain.
*/

// Open Arduino IDE, select “Sketch” > “Include Library” > “Add .ZIP Library…”, and browse to find your .zip archive.

#ifndef PiSupplyH_h
#define PiSupplyH_h

#include "PicroBoard.h"

// In Arduino IDE, go to Sketch -> Include Library -> Manage Libraries
#include <TimerOne.h> // In Library Manager, search for "TimerOne"
#include <avdweb_AnalogReadFast.h> // In Library Manager, search for "AnalogReadFast"

// pins for turning on the LEDs
const int LED2_PIN = 2; // PD2
const int LED1_PIN = 4; // PD4

// pins for channel signals
const int CHPI_PIN = 5; // PD5
const int CH5V_PIN = 6; // PD6
const int CH12V_PIN = 7; // PD7

// pins for voltage sensing
const int V48_PIN = A2; // PC2, High voltage (~48V) input bus voltage
const int V12_PIN = A3; // PC3, 12V bus voltage

// GPIO pins nicknames: A0, A1, A6, A7, D3, D8, D9
const int A0_PIN = A0;
const int A1_PIN = A1;
const int A6_PIN = A6;
const int A7_PIN = A7;
const int D3 = 3; // PD3
const int D3_PIN = 3; // PD3
const int D9 = 9; // PB1
const int D9_PIN = 9; // PB1
const int D8 = 8; // PB0
const int D8_PIN = 8; // PB0

// sensor index enumerator for convenience
enum SensorIndex 
{   V48_INDEX = 0,
    V12_INDEX,
    A0_INDEX,
    A1_INDEX,
    A6_INDEX,
    A7_INDEX,
    NUM_SENSORS
};

// moving average sample window length for sensors. Best to use powers of 2 so as to optimize division
//  use this before #include to override in the .ino file: #define XXX YY
//  e.g. to set current averaging window to size 32: #define SENSOR_I_WINDOW_MAX 32
#ifndef SENSOR_V_WINDOW_MAX
#define SENSOR_V_WINDOW_MAX 32
#endif
#ifndef SENSOR_A_WINDOW_MAX
#define SENSOR_A_WINDOW_MAX 32
#endif
constexpr int AVERAGE_WINDOW_MAX[NUM_SENSORS] = {
  SENSOR_V_WINDOW_MAX,
  SENSOR_V_WINDOW_MAX,
  SENSOR_A_WINDOW_MAX,
  SENSOR_A_WINDOW_MAX,
  SENSOR_A_WINDOW_MAX,
  SENSOR_A_WINDOW_MAX
};

class PiSupplyH : public PicroBoard
{
  public:
    PiSupplyH(); // constructor
  // Atmega initialization
    void initialize(); // default initialization routine
    void setupPinMode(); // sets appropriate pinMode() for each const pin
    void initializeSensors(); // initialize sensor average array to sensor read
    void initializeSensors(int avgWindowLength); // initialize sensor average array to sensor read
    void initializeInterruptTimer(long periodus, // starts periodic control timer
      void (*interruptFunction)(void)); // inputs: period (ms), controller function reference
  // channel state set and get
    void setChPi(int state); // sets the state of the Pi power channel (5V) to on (HIGH) or off (LOW)
    void setCh5V(int state); // sets the state of 5V output power channel to on (HIGH) or off (LOW)
    void setCh12V(int state); // sets the state of 12V output power channel to on (HIGH) or off (LOW)
    void setChannel(int chPin, int state); // set channel
    void shutdownChannels(); // turns off all channels immediately
    int getChPi(); // gets the state of Pi power channel (5V)
    int getCh5V(); // gets the state of 5V output power channel
    int getCh12V(); // gets the state of 12V output power channel
  // raw sensor values
    void updateVCC(); // updates stored VCC value based on an average
    void updateVSensors(); // updates voltage sensor averages
    void updateASensor(int sensor); // updates a single analog GPIO sensor average (0, 1, 6, or 7)
    void updateASensors(); // updates all analog GPIO sensor averages
    void updateSensors(); // updates all sensor averages
    int readVCC(); // returns the sampled VCC voltage in milliVolts
    int getRawV48(); // gets 48V input bus voltage ADC value (0 to 1023)
    int getRawV12(); // gets 12V bus voltage value (0 to 1023)
  // fully-formatted sensors
    int getVCC(); // returns the averaged VCC value
    unsigned int getV48(); // returns the averaged 48V input bus voltage mV value
    int getV12(); // returns the averaged 12V input bus voltage mV value
  // diagnostics
    void setLED(int led, int state); // sets an LED to HIGH or LOW
    void setLED1(int state); // sets LED1 (yellow) to HIGH or LOW
    void setLED2(int state); // sets LED2 (green) to HIGH or LOW
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
    int _sensorPastV48[AVERAGE_WINDOW_MAX[V48_INDEX]]; // array raw sensor moving averages
    int _sensorPastV12[AVERAGE_WINDOW_MAX[V12_INDEX]]; // array raw sensor moving averages
    int _sensorPastA0[AVERAGE_WINDOW_MAX[A0_INDEX]]; // array raw sensor moving averages
    int _sensorPastA1[AVERAGE_WINDOW_MAX[A1_INDEX]]; // array raw sensor moving averages
    int _sensorPastA6[AVERAGE_WINDOW_MAX[A6_INDEX]]; // array raw sensor moving averages
    int _sensorPastA7[AVERAGE_WINDOW_MAX[A7_INDEX]]; // array raw sensor moving averages
    int _vcc; // stored value of vcc measured at start up and/or periodically
    // functions
    void updateSensorRaw(int index, int sample); // updates the raw averaged sensor value
};

#endif
