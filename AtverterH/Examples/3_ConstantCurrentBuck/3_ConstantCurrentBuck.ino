/*
  4_ConstantVoltageBoost

  A constant current buck converter.
  Input: terminal 1, 12-48V
  Output: terminal 2, 1A constant current, 0-12V

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

  // current-mode control loop
  int dutyCycle = atverterH.getDutyCycle();
  dutyCycle = dutyCycle + 5;
  if (dutyCycle > 60)
    dutyCycle = 40;
  atverterH.setDutyCycle(dutyCycle);

  // in this example, do some special stuff every 1 second (1000ms)
  slowInterruptCounter++;
  if (slowInterruptCounter > 1000) {
    slowInterruptCounter = 0;
    atverterH.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverterH.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverterH.checkThermalShutdown(); // checks average temperature and shut down gates if necessary
    // prints duty cycle, VCC, V1, V2, I1, I2, T1, T2 to the serial console of attached computer
    Serial.print("PWM Duty = ");
    Serial.print(atverterH.getDutyCycle());
    Serial.print("/100, VCC = ");
    Serial.print(atverterH.getVCC());
    Serial.print("mV, V1 = ");
    Serial.print(atverterH.getV1());
    Serial.print("mV, V2 = ");  
    Serial.print(atverterH.getV2());
    Serial.print("mV, I1 = ");  
    Serial.print(atverterH.getI1());
    Serial.print("mA, I2 = ");  
    Serial.print(atverterH.getI2());
    Serial.print("mA, T1 = ");
    Serial.print(atverterH.getT1());
    Serial.print("°C, T2 = ");
    Serial.print(atverterH.getT2());
    Serial.println("°C");
  }
}
