/*
  ConstantVoltageBuck

  A constant voltage buck converter.
  Input: Terminal 1, 13-48V (compensator tuned for 20V input)
  Output: Terminal 2, 8V or 12V constant voltage (compensator tuned for 1A output)
  
  Test Set-up:
  Input: Voltage supply attached to terminal 1, set to CV at 20V with 3A current limit
  Output: Electronic load attached to terminal 2, is set to CC mode between 0A to 3A

  For a discrete-time compensator design reference, see compensation.py

  The feedback control loop uses a classical feedback voltage-mode discrete-time compensator when the
  output current is between 0.5A to 5A, and a gradient-descent algorithm for when the output current
  is 0A to 0.5A. The reason is that classical voltage-mode feedback loops go unstable at low current since 
  the converter's high Q factor causes multiple zero crossings.

  Created 7/31/23 by Daniel Gerber
*/

#include <AtverterH.h>

AtverterH atverterH;

long slowInterruptCounter = 0;
bool stepUp = false;

int compIn [] = {0, 0, 0}; // must be equal or longer than compNum
int compOut [] = {0, 0, 0}; // must be equal or longer than compDen

// discrete compensator coefficients
// usage: compNum = {A, B, C}, compDen = {D, E, F}, x = compIn, y = compOut
//   D*y[n] + E*y[n-1] + F*y[n-2] = A*x[n] + B*x[n-1] + C*x[n-2]
//   y[n] = (A*x[n] + B*x[n-1] + C*x[n-2] - E*y[n-1] - F*y[n-2])/D

// // Uncomment for phase margin: 81°
int compNum [] = {8, 0};
int compDen [] = {8, -8};

// // Uncomment for phase margin: 119°
// int compNum [] = {12, -10, 0};
// int compDen [] = {8, -12, 4};

// // Uncomment for phase margin: 27°
// int compNum [] = {4, 0, 0};
// int compDen [] = {8, -14, 6};

int gradDescCount = 0;

const int INSIZE = sizeof(compIn) / sizeof(compIn[0]);
const int OUTSIZE = sizeof(compOut) / sizeof(compOut[0]);
const int NUMSIZE = sizeof(compNum) / sizeof(compNum[0]);
const int DENSIZE = sizeof(compDen) / sizeof(compDen[0]);

int VREF = atverterH.mV2raw(8000);

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

  // measure Vout and calculate error

  // raw 10-bit output voltage = actual voltage * 10k/(10k+120k) * 2^10 / 5V
  int vOut = atverterH.getRawV2();
  // error = reference - output voltage
  int vErr = VREF - vOut;

  // update past compensator inputs and outputs
  // must do this even if using gradient descent for smooth transition to classical feedback

  // update array of past compensator inputs
  for (int n = INSIZE - 1; n > 0; n--) {
    compIn[n] = compIn[n-1];
  }
  compIn[0] = vErr;
  // update array of past compensator outputs
  for (int n = OUTSIZE - 1; n > 0; n--) {
    compOut[n] = compOut[n-1];
  }

  // 0.5A-5A output: classical feedback voltage mode discrete compensation
  // 0A-0.5A output: slow gradient descent mode
  bool isClassicalFB = atverterH.getRawI2() < 512 - 51 || atverterH.getRawI2() > 512 + 51;

  if(isClassicalFB) { // classical feedback voltage mode discrete compensation
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
  } else { // slow gradient descent mode, avoids light-load instability
    gradDescCount++; // use a counter to control the rate of gradient descent
    if (gradDescCount > 4) {
      long duty = atverterH.getDutyCycle();
      compOut[0] = duty*1024/100;
      if (vErr > 0) { // ascend or descend by 1% duty cycle depending on error
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
    Serial.print("VREF: ");
    Serial.println(VREF);
    Serial.print("VOut: ");
    Serial.println(vOut);
    Serial.print("VErr: ");
    Serial.println(vErr);
    Serial.print("Duty: ");
    Serial.println(atverterH.getDutyCycle());

    if (stepUp) {
      VREF = atverterH.mV2raw(12000); // step output up to approximately 12V
    } else {
      VREF = atverterH.mV2raw(8000); // step output down approximately 8V
    }
    stepUp = !stepUp;

  }
}
