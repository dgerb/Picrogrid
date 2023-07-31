/*
  3_ConstantVoltageBuck

  A constant voltage buck converter.
  Input: terminal 1, ~20V
  Output: terminal 2, 10V constant voltage, ~1A

  created 7/31/23 by Daniel Gerber
*/

#include <AtverterH.h>

AtverterH atverterH;

long slowInterruptCounter = 0;
bool stepUp = false;

int x [] = {0, 0, 0}; // must be equal or longer than compNum
int y [] = {0, 0, 0}; // must be equal or longer than compDen

// // Phase margin: 81°
int compNum [] = {8, 0};
int compDen [] = {8, -8};
// // Phase margin: 119°
// int compNum [] = {12, -10, 0};
// int compDen [] = {8, -12, 4};
// // Phase margin: 27°
// int compNum [] = {4, 0, 0};
// int compDen [] = {8, -14, 6};

const int YSIZE = sizeof(y) / sizeof(y[0]);
const int XSIZE = sizeof(x) / sizeof(x[0]);
const int NUMSIZE = sizeof(compNum) / sizeof(compNum[0]);
const int DENSIZE = sizeof(compDen) / sizeof(compDen[0]);

int VREF = int(8.0*(10.0/(10.0+120.0))*(1024.0/5.0));

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

  // voltage control loop and compensation

  // raw 10-bit output voltage = actual voltage * 10k/(10k+120k) * 2^10 / 5V
  int vOut = atverterH.getRawV2();
  // error = reference - output voltage
  int vErr = VREF - vOut;
  // update array of past compensator inputs (x)
  for (int n = XSIZE - 1; n > 0; n--) {
    x[n] = x[n-1];
  }
  x[0] = vErr;
  // update array of past compensator outputs (y)
  for (int n = YSIZE - 1; n > 0; n--) {
    y[n] = y[n-1];
  }
  // accumulate compensator weighted input terms
  long compOut = 0;
  for (int n = 0; n < NUMSIZE; n++) {
    compOut = compOut + x[n]*compNum[n];
  }
  // accumulate compensator weighted past output terms
  for (int n = 1; n < DENSIZE; n++) {
    compOut = compOut - y[n]*compDen[n];
  }
  compOut = compOut/compDen[0];
  y[0] = compOut;
  // duty cycle (0-100) = compensator output * 100% / 2^10
  int duty = (compOut*100)/1024;
  atverterH.setDutyCycle(duty);

  // in this example, step from 8V to 12V or vice versa every 3 seconds
  slowInterruptCounter++;
  if (slowInterruptCounter > 3000) { // timer set such that each count is 1 ms
    slowInterruptCounter = 0;
    atverterH.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverterH.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverterH.checkThermalShutdown(); // checks average temperature and shut down gates if necessary

    Serial.println("");
    Serial.print("VREF: ");
    Serial.println(VREF);
    Serial.print("VOut: ");
    Serial.println(vOut);
    Serial.print("VErr: ");
    Serial.println(vErr);
    Serial.print("Duty: ");
    Serial.println(duty);

    if (stepUp) {
      VREF = 190; // step output up to approximately 12V
    } else {
      VREF = 126; // step output down approximately 8V
    }
    stepUp = !stepUp;

  }
}
