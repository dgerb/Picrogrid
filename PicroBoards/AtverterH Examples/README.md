# AtverterH Examples

## Basic Examples

The basic examples are meant to teach users how to use the Atverter and its API. In each example, the operating test conditions are described in the .ino file header comment.
- 1. Blink - Sets an interrupt timer and blinks LEDs. Recommended means of verifying the board is being programmed.
- 2. Open Loop Buck-Boost - Operates the Atverter in buck-boost mode and varies the duty cycle from 40\% to 60\%. Also reports sensor values via UART (use a FTDI USB/TTL cable).
- 3. Serial - Sets up UART and I2C command register interface and creates several custom commands. Works with the Arduino Serial Console (UART), Pi_UART_Shell.py, and Pi_I2C_Shell.py under Raspberry Pi / Basic Examples.
- 4. Constant Voltage Buck - Operates the Atverter as a buck converter in constant voltage mode. Uses both classical feedback compensation and a gradient descent algorithm. The example's folder also contains a python compensation design tool.

## Core Examples

The core examples illustrate practical operation of the Atverter in several scenarios. Each example is configured with serial operation, and demonstrates operation in buck, boost, and buck-boost modes.
- 1. LED Driver - Operates the Atverter in constant-current mode for an LED output with default 1A set point.

COMING Soon:
- 2. Constant Voltage Bus - Operates the Atverter in constant-voltage mode and maintains a 24V bus output.
- 3. Battery Management System - Operates the Atverter as a bi-directional converter interface with a 24V battery. Includes a state of charge estimation algorithm.
- 4. Solar Power Optimizer - Operates the Atverter with a maximum power-point tracking algorithm, allowing it to draw power from a 24V solar panel.