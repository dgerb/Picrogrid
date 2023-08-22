# PicroBoard Libraries

PicroBoards are Picrogrid's ATmega328p based circuit boards, all of which are compatible with the Raspberry Pi and Arduino platforms. This folder contains Arduino/AVR C++ code that can be run on the ATmega328p microcontroller. The classes include:
- PicroBoard - base class for all boards in the Picrogrid ecosystem. Contains functions that simplify communication between the board and a Raspberry Pi, including UART and I2C protocols.
- AtverterH - class that manages an Atverter Hobbyist board. Contains functions to initialize the timers and pins,  configure the converter mode, read sensors (V1, V2, I1, I2, T1, T2, VCC), and set vital parameters (duty cycle, current and thermal shutoff limits, and diagnostic LEDs).
- MicroDDC - (future work)

## Loading the Libraries

To load the PicroBoard libraries into the Arduino IDE:
- 1. Go to Sketch > Include Library > Add .ZIP Library...
- 2. Select PicroBoards.zip

The PicroBoard libraries require several more third-party libraries to function. To load standard Arduino libraries into the Arduino IDE:

- 1. Go to Sketch > Include Library > Manage Libraries...
- 2. Search for the proper library name
- 3. Once found, select the latest version and click Install. 

## PicroBoard.h

Requires the following:
- Arduino.h - Already loaded into Arduino IDE
- Wire.h - Sketch > Include Library > Manage Libraries...

## AtverterH.h

Requires the following:
- PicroBoard.h - Loaded into Arduino IDE above
- TimerOne.h - Sketch > Include Library > Manage Libraries...
- avdweb_AnalogReadFast.h - Sketch > Include Library > Manage Libraries...
- FastPwmPin.h - Sketch > Include Library > Add .ZIP Library... (https://github.com/maxint-rd/FastPwmPin)






