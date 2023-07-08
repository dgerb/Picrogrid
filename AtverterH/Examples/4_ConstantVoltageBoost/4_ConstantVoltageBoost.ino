/*
  4_ConstantVoltageBoost

  A constant voltage boost converter operating with current-mode control.
  Input: terminal 1, 5-24V
  Output: terminal 2, 24V constant voltage

  modified 30 June 2023
  by Daniel Gerber
*/

#include <AtverterH.h>

AtverterH atverterH;

long slowInterruptCounter = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  atverterH.setupPinMode(); // set pins to input or output
  atverterH.initializeSensors(); // set filtered sensor values to initial reading
  atverterH.setCurrentShutdown(6000); // set gate shutdown at 6A peak current 
  atverterH.setThermalShutdown(60); // set gate shutdown at 60°C temperature
  atverterH.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms
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
  // does nothing in this example, but needed for buck or boost mode

  slowInterruptCounter++; // in this example, do some special stuff every 1 second (1000ms)
  if (slowInterruptCounter > 1000) {
    slowInterruptCounter = 0;
    atverterH.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverterH.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverterH.checkThermalShutdown(); // checks average temperature and shut down gates if necessary
    
    // current-mode control loop
    int dutyCycle = atverterH.getDutyCycle();
    dutyCycle = dutyCycle + 5;
    if (dutyCycle > 60)
      dutyCycle = 40;
    atverterH.setDutyCycle(dutyCycle);
  }
}
