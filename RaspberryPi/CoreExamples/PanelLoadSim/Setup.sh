#!/bin/bash

# Physical setup:
# 1. Get a MicroPanelH boards. Connect loads to the outputs and a battery to the input
# 2. Place one Reset jumper on the MicroPanel at Reset select 24
# 3. Stack the MicroPanel on a Raspberry Pi using a 40-pin header extension (e.g. PRT-16764) and M2.5 18mm Standoffs
# 4. Run the ~/Picrogrid/RaspberryPi/Setup/SetupRaspberryPi.sh script if necessary
# 5. Run this script on the Raspberry Pi

# Enable I2C
sudo raspi-config nonint do_i2c 0

# Set the Atmega328P Fuses
# sometimes you'll need to set the fuses twice; need to experiment with this
sudo avrdude -c linuxgpio24 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxgpio24 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2

# Flash the MicroPanel
# note that avrdude cannot process “~”, need absolute directory, e.g. /home/pi/...

# MicroPanel: jumper 24
sudo avrdude -c linuxgpio24 -p atmega328p -v -E noreset -U flash:w:/home/$USER/Picrogrid/PicroBoards/MicroPanelHExamples/CoreExamples/BatteryPanel/build/arduino.avr.uno/BatteryPanel.ino.with_bootloader.hex:i
raspi-gpio set $GPIO ip
sleep 2
