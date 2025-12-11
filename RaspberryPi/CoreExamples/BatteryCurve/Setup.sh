#!/bin/bash

# Physical setup:
# 1. Get one MicroPanelH board:
#    - Connect ports 1-4 to a DC electronic load set to 10A
# 2. Place one Reset jumper on the MicroPanelH at location 24
# 3. Stack the MicroPanelH on a Raspberry Pi using a 40-pin header extension (e.g. PRT-16764) and M2.5 18mm Standoffs
# 4. Run the ~/Picrogrid/RaspberryPi/Setup/SetupRaspberryPi.sh script if necessary
# 5. Run this script on the Raspberry Pi

# Enable I2C
sudo raspi-config nonint do_i2c 0

# Set the Atmega328P Fuses
# sometimes you'll need to set the fuses twice; need to experiment with this

GPIO=24
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2

# Flash the Atmega328Ps
# note that avrdude cannot process “~”, need absolute directory, e.g. /home/pi/...
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -E noreset -U flash:w:/home/$USER/Picrogrid/PicroBoards/MicroPanelHExamples/BasicExamples/3_SmartPanel/build/arduino.avr.uno/3_SmartPanel.ino.with_bootloader.hex:i
raspi-gpio set $GPIO pu
sleep 2


# OLD:
# sudo avrdude -c linuxgpio24 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
# sleep 2
# sudo avrdude -c linuxgpio24 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
# sleep 2
# sudo avrdude -c linuxgpio24 -p atmega328p -v -U flash:w:/home/$USER/Picrogrid/PicroBoards/MicroPanelHExamples/BasicExamples/3_SmartPanel/build/arduino.avr.uno/3_SmartPanel.ino.with_bootloader.hex:i
# sleep 2

