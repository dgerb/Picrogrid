/*
  4_ConstantCurrentBuck

  A constant current buck converter.
  Input: Terminal 1, 20V
  Output: Terminal 2, 0.5A to 2A constant current
  
  Test Setup:
  Input: Voltage supply attached to terminal 1, set to CV at 20V with 3A current limit
  Output: Electronic load attached to terminal 2, is set to CR mode at 5 ohms

  For a discrete-time compensator design reference, see compensation.py

  For constant current control, a classical feedback voltage-mode discrete-time compensator or a
  gradient-descent algorithm are both viable, since loop gain scales with output current.

  created 8/6/23 by Daniel Gerber
*/

#include <AtverterH.h>

AtverterH atverterH;

// set the feedback algorithm. Either is viable over the full current range
//  isClassicalFB = true: uses discrete compensation and a classical feedback loop
//  isClassicalFB = false: uses a slow gradient descent feedback algorithm
bool isClassicalFB = true;

long slowInterruptCounter = 0;
bool stepUp = false;

int compIn [] = {0, 0, 0}; // must be equal or longer than compNum
int compOut [] = {0, 0, 0}; // must be equal or longer than compDen

// discrete compensator coefficients
// usage: compNum = {A, B, C}, compDen = {D, E, F}, x = compIn, y = compOut
//   D*y[n] + E*y[n-1] + F*y[n-2] = A*x[n] + B*x[n-1] + C*x[n-2]
//   y[n] = (A*x[n] + B*x[n-1] + C*x[n-2] - E*y[n-1] - F*y[n-2])/D
int compNum [] = {8, 0};
int compDen [] = {8, -8};

int gradDescCount = 0;

const int INSIZE = sizeof(compIn) / sizeof(compIn[0]);
const int OUTSIZE = sizeof(compOut) / sizeof(compOut[0]);
const int NUMSIZE = sizeof(compNum) / sizeof(compNum[0]);
const int DENSIZE = sizeof(compDen) / sizeof(compDen[0]);

// Current flowing out of the terminal is measured as being negative
int IREF = atverterH.mA2raw(-2000);

// the setup function runs once when you press reset or power the board
void setup() {
  atverterH.setupPinMode(); // set pins to input or output
  atverterH.initializeSensors(); // set filtered sensor values to initial reading
  atverterH.setCurrentShutdown(6000); // set gate shutdown at 6A peak current 
  atverterH.setThermalShutdown(60); // set gate shutdown at 60°C temperature
  atverterH.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms
  atverterH.applyHoldHigh2(); // hold side 2 high for a buck converter with side 1 input
  atverterH.setDutyCycle(50);
  atverterH.startPWM();

  Serial.begin(9600); // in this example, send messages to computer via basic UART serial
}

void loop() { } // we don't use loop() because it does not loop at a fixed period

// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  atverterH.updateVISensors(); // read voltage and current sensors and update moving average
  atverterH.checkCurrentShutdown(); // checks average current and shut down gates if necessary
  atverterH.checkBootstrapRefresh(); // refresh bootstrap capacitors on a timer

  // measure Iout and calculate error

  // raw 10-bit output current = actual current * 0.333 * 2^10 / 5V - 512
  int iOut = atverterH.getRawI2();
  // error = -(reference - output current)
  // note the negative sign. measured current flowing out is denoted as negative, but this causes the 
  //  error to be negatively coorelated with the duty cycle, thus we swap the sign for error calculations.
  int iErr = -(IREF - iOut);

  if(isClassicalFB) { // classical feedback current control with discrete compensation
    // update array of past compensator inputs
    for (int n = INSIZE - 1; n > 0; n--) {
      compIn[n] = compIn[n-1];
    }
    compIn[0] = iErr;
    // update array of past compensator outputs
    for (int n = OUTSIZE - 1; n > 0; n--) {
      compOut[n] = compOut[n-1];
    }
    // accumulate compensator weighted input terms
    long compAcc = 0;
    for (int n = 0; n < NUMSIZE; n++) {
      compAcc = compAcc + compIn[n]*compNum[n];
    }
    // accumulate compensator weighted past output terms
    for (int n = 1; n < DENSIZE; n++) {
      compAcc = compAcc - compOut[n]*compDen[n];
    }
    compAcc = compAcc/compDen[0];
    compOut[0] = compAcc;
    // duty cycle (0-100) = compensator output * 100% / 2^10
    int duty = (compAcc*100)/1024;
    atverterH.setDutyCycle(duty);
  } else { // slow gradient descent mode
    gradDescCount++; // use a counter to control the rate of gradient descent
    if (gradDescCount > 4) {
      long duty = atverterH.getDutyCycle();
      compOut[0] = duty*1024/100;
      if (iErr > 0) { // ascend or descend by 1% duty cycle depending on error
        atverterH.setDutyCycle(duty + 1);
      } else {
        atverterH.setDutyCycle(duty - 1);
      }
      gradDescCount = 0;
    }
  }

  // in this example, report loop values and step from 8V to 12V or vice versa every 3 seconds
  slowInterruptCounter++;
  if (slowInterruptCounter > 3000) { // timer set such that each count is 1 ms
    slowInterruptCounter = 0;
    atverterH.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverterH.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverterH.checkThermalShutdown(); // checks average temperature and shut down gates if necessary

    Serial.println("");
    if (isClassicalFB) {
      Serial.println("Classical FB");
    } else {
      Serial.println("Gradient Descent");
    }
    Serial.print("IREF: ");
    Serial.println(IREF);
    Serial.print("IOut: ");
    Serial.println(iOut);
    Serial.print("IErr: ");
    Serial.println(iErr);
    Serial.print("Duty: ");
    Serial.println(atverterH.getDutyCycle());

    if (stepUp) {
      IREF = atverterH.mA2raw(-2000); // step output up to approximately 2A
    } else {
      IREF = atverterH.mA2raw(-500); // step output down approximately 0.5A
    }
    stepUp = !stepUp;
  }
}
