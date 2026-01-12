#!/bin/bash

# Physical setup:
# 1. Get four AtverterH boards:
#    - The Solar Atverter connects on terminal 1 to DC bus and terminal 2 to a solar panel
#    - The Battery Atverter connects on terminal 1 to DC bus and terminal 2 to a battery
#    - The LED Driver Atverter connects on terminal 1 to the DC bus and terminal 2 to an LED string
#    - The Fan Atverter connects on terminal 1 to the DC bus and terminal 2 to a DC fan
# 2. Place one Reset jumper on each Atverter at the following locations:
#    - Solar Atverter: 26
#    - Battery Atverter: 19
#    - LED Driver Atverter: 13
#    - Fan Atverter: 6
# 3. Stack the Atverters on a Raspberry Pi using a 40-pin header extension (e.g. PRT-16764) and M2.5 18mm Standoffs
# 4. Run the ~/Picrogrid/RaspberryPi/Setup/SetupRaspberryPi.sh script if necessary
# 5. Run this script on the Raspberry Pi

# Enable I2C
sudo raspi-config nonint do_i2c 0

# Set the Atmega328P Fuses
# sometimes you'll need to set the fuses twice; need to experiment with this
sudo avrdude -c linuxgpio26 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxgpio26 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxgpio19 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxgpio19 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxgpio13 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxgpio13 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2

# Flash the Atverters
# note that avrdude cannot process “~”, need absolute directory, e.g. /home/pi/...

# SolarConverter: jumper 26
sudo avrdude -c linuxgpio26 -p atmega328p -v -E noreset -U flash:w:/home/$USER/Picrogrid/PicroBoards/AtverterHExamples/CoreExamples/SolarConverter/build/arduino.avr.uno/SolarConverter.ino.with_bootloader.hex:i
# pinctrl -e set $GPIO pu
sleep 2
# BatteryConverter: jumper 19
sudo avrdude -c linuxgpio19 -p atmega328p -v -E noreset -U flash:w:/home/$USER/Picrogrid/PicroBoards/AtverterHExamples/CoreExamples/BatteryConverter/build/arduino.avr.uno/BatteryConverter.ino.with_bootloader.hex:i
# pinctrl -e set $GPIO pu
sleep 2
# PowerSupply: jumper 13
sudo avrdude -c linuxgpio13 -p atmega328p -v -E noreset -U flash:w:/home/$USER/Picrogrid/PicroBoards/AtverterHExamples/CoreExamples/PowerSupply/build/arduino.avr.uno/PowerSupply.ino.with_bootloader.hex:i
# pinctrl -e set $GPIO pu
sleep 2

