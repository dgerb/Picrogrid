/*
  Port Toggle

  Toggles the channels on and off. Measures channel state, bus voltage, and channel current,
  and outputs to serial console

  modified 15 August 2024
  by Daniel Gerber
*/

#include <MicroPanelH.h>

MicroPanelH micropanel;

// test variables for this example
int ledState = HIGH;
long slowInterruptCounter = 0;
int channelState = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  micropanel.setupPinMode(); // set pins to input or output
  micropanel.initializeSensors(); // set filtered sensor values to initial reading
  micropanel.setCurrentLimit1(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit2(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit3(6000); // set gate shutdown at 6A peak current 
  micropanel.setCurrentLimit4(6000); // set gate shutdown at 6A peak current 

  // initialize interrupt timer for periodic calls to control update funciton
  micropanel.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms (= 1000 microseconds)

  Serial.begin(38400); // in this example, send messages to computer via basic UART serial
}

void loop() { } // we don't use loop() because it does not loop at a fixed period

// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  micropanel.updateVISensors(); // read voltage and current sensors and update moving average
  micropanel.checkCurrentShutdown(); // checks average current and shut down gates if necessary

  slowInterruptCounter++; // in this example, do some special stuff every 1 second (1000ms)
  if (slowInterruptCounter > 1000) {
    slowInterruptCounter = 0;
    micropanel.updateVCC(); // read on-board VCC voltage, update stored average (shouldn't change)
    // prints channel state (as binary), VCC, VBus, I1, I2, I3, I4 to the serial console of attached computer
    Serial.print("State: ");
    Serial.print(micropanel.getCh1());
    Serial.print(micropanel.getCh2());
    Serial.print(micropanel.getCh3());
    Serial.print(micropanel.getCh4());
    Serial.print(", VCC = ");
    Serial.print(micropanel.getVCC());
    Serial.print("mV, VBus = ");
    Serial.print(micropanel.getVBus());
    Serial.print("mV, I1 = ");  
    Serial.print(micropanel.getI1());
    Serial.print("mA, I2 = ");  
    Serial.print(micropanel.getI2());
    Serial.print("mA, I3 = ");  
    Serial.print(micropanel.getI3());
    Serial.print("mA, I4 = ");  
    Serial.print(micropanel.getI4());
    Serial.println("mA");
    // update channel state
    channelState++;
    if (channelState > 15)
      channelState = 0;
    micropanel.setCh1(bitRead(channelState, 0));
    micropanel.setCh2(bitRead(channelState, 1));
    micropanel.setCh3(bitRead(channelState, 2));
    micropanel.setCh4(bitRead(channelState, 3));
    // in this example, set lights to alternate blink every 1 second
    micropanel.setLED2(ledState);
    micropanel.setLED1(!ledState);
    ledState = !ledState;
  }
}
