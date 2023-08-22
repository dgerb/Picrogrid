# Picrogrid Board Libraries

This folder contains Arduino/AVR C++ code that can be run on the ATmega328p microcontroller of all boards in the Picrogrid ecosystem. The classes include:
- PicrogridBoard.h - base class for all boards in the Picrogrid ecosystem. Contains functions that simplify communication between the board and a Raspberry Pi, including UART and I2C protocols.
- AtverterH.h - class that manages an Atverter Hobbyist board. Contains functions to initialize the timers and pins,  configure the converter mode, read sensors (V1, V2, I1, I2, T1, T2, VCC), and set vital parameters (duty cycle, current and thermal shutoff limits, and diagnostic LEDs).
- FUTURE WORK: MicroDDC.h

## Loading the Libraries

To load the Picrogrid Board libraries into the Arduino IDE:
1. Go to Sketch > Include Library > Add .ZIP Library...
2. Select PicrogridBoards.zip

The Picrogrid Board libraries require several more third-party libraries to function.

To load standard Arduino libraries into the Arduino IDE:
1. Go to Sketch > Include Library > Manage Libraries...
2. Search for the proper library name
3. Once found, select the latest version and click Install. 

## PicrogridBoard.h

Requires the following:
Arduino.h - Already loaded into Arduino IDE
Wire.h - Sketch > Include Library > Manage Libraries...

## AtverterH.h

Requires the following:
PicrogridBoard.h - Loaded into Arduino IDE above
TimerOne.h - Sketch > Include Library > Manage Libraries...
avdweb_AnalogReadFast.h - Sketch > Include Library > Manage Libraries...
FastPwmPin.h - Sketch > Include Library > Add .ZIP Library... (https://github.com/maxint-rd/FastPwmPin)






