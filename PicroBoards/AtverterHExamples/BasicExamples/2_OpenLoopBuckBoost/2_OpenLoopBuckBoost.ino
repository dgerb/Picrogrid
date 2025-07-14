
/*
  OpenLoopBuckBoost

  An open loop test of the Atverter in buck/boost mode. Sweeps duty cycle from 40%-60%.
  Monitors the voltage (V1, V2) and current (I1, I2) at each terminal, the temperature (T1, T2)
  of each side's H-bridge, and the internal VCC bus voltage. Reports all values on the Arduino
  serial monitor at 9600 baud rate.

  created 6/30/23 by Daniel Gerber
*/

#include <AtverterH.h>

AtverterH atverter;

// test variables for this example
int ledState = HIGH;
long slowInterruptCounter = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  atverter.setupPinMode(); // set pins to input or output
  atverter.initializeSensors(); // set filtered sensor values to initial reading
  atverter.setCurrentShutdown1(6000); // set gate shutdown at 6A peak current 
  atverter.setCurrentShutdown2(6000); // set gate shutdown at 6A peak current 
  atverter.setThermalShutdown(80); // set gate shutdown at 60°C temperature

  // atverter.applyHoldHigh1(); // uncomment to hold side 1 high for a boost converter with side 1 input
  // atverter.applyHoldHigh2(); // uncomment to hold side 2 high for a buck converter with side 1 input

  atverter.startPWM(50); // start PWM with 50% duty cycle (V1 = V2)
  // usually you want to start the PWM before the interrupt timer is intialized
  atverter.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms (= 1000 microseconds)

  Serial.begin(38400); // in this example, send messages to computer via basic UART serial
}

// during loop(), analog read the voltage and current sensors and update their moving averages
// for reference, loop() usually takes 112-148 microseconds
void loop() {
  atverter.updateVISensors(); // read voltage and current sensors and update moving average
}
// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  atverter.checkCurrentShutdown(); // checks average current and shut down gates if necessary
  atverter.checkThermalShutdown(); // checks switch temperature and shut down gates if necessary
  atverter.checkBootstrapRefresh(); // refresh bootstrap capacitors on a timer
  // does nothing in this example, but needed for buck or boost mode

  slowInterruptCounter++; // in this example, do some special stuff every 1 second (1000ms)
  if (slowInterruptCounter > 1000) {
    slowInterruptCounter = 0;
    atverter.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    atverter.updateTSensors(); // occasionally read thermistors and update temperature moving average
    atverter.checkThermalShutdown(); // checks average temperature and shut down gates if necessary
    // prints duty cycle, VCC, V1, V2, I1, I2, T1, T2 to the serial console of attached computer
    Serial.print("PWM Duty = ");
    Serial.print(atverter.getDutyCycle());
    Serial.print("%, VCC = ");
    Serial.print(atverter.getVCC());
    Serial.print("mV, V1 = ");
    Serial.print(atverter.getV1());
    Serial.print("mV, V2 = ");  
    Serial.print(atverter.getV2());
    Serial.print("mV, I1 = ");  
    Serial.print(atverter.getI1());
    Serial.print("mA, I2 = ");  
    Serial.print(atverter.getI2());
    Serial.print("mA, T1 = ");
    Serial.print(atverter.getT1());
    Serial.print("°C, T2 = ");
    Serial.print(atverter.getT2());
    Serial.println("°C");
    // in this example, sweep duty cycle between 40% and 60% in 5% increments every 1 second
    int dutyCycle = atverter.getDutyCycle();
    dutyCycle = dutyCycle + 2;
    if (dutyCycle > 54)
      dutyCycle = 46;
    atverter.setDutyCycle(dutyCycle);
    // in this example, set lights to alternate blink every 1 second
    atverter.setLED2(ledState);
    atverter.setLED1(!ledState);
    ledState = !ledState;
  }
}

