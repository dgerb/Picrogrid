/*
  Blink

  Blinks the LEDs in an alternating fashion. This is the most basic test of the 
  microcontroller's functionality and method of uploading a program.

  modified 30 June 2023
  by Daniel Gerber
*/

#include <AtverterH.h>

AtverterH atverterH;

// test variables for this example
int ledState = HIGH;
long interruptCounter = 0;

// the setup function runs once when you press reset or power the board
void setup() {
  atverterH.setupPinMode(); // set pins to input or output
  atverterH.initializeInterruptTimer(1000, &controlUpdate); // control update every 1ms
}

void loop() { } // we don't use loop() because it does not loop at a fixed period

// main controller update function, which runs on every timer interrupt
void controlUpdate(void)
{
  interruptCounter++; // in this example, do some special stuff every 1 second (1000ms)
  if (interruptCounter > 1000) {
    interruptCounter = 0;
    // int his example, set lights to alternate blink every 1 second
    atverterH.setLEDG(ledState);
    atverterH.setLEDY(!ledState);
    ledState = !ledState;
  }
}
