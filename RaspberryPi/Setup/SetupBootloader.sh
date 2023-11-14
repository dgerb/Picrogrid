#!/bin/bash

# In all these examples, replace "linuxgpio" with one of the following depending on Reset jumper position:
# linuxgpio26, linuxgpio19, linuxgpio13, linuxgpio6, linuxgpio5, linuxgpio22

echo
echo For which board would you like to flash the bootloader? 
read -p 'Enter GPIO number: ' GPIONUM

# Set Atmega328P Fuses
# sometimes you'll need to set the fuses twice; need to experiment with this

sudo avrdude -c linuxgpio$GPIONUM -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sudo avrdude -c linuxgpio$GPIONUM -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m

# Flash the default bootloader Optiboot
# found on https://github.com/Optiboot/optiboot, optiboot/optiboot/bootloaders/optiboot/optiboot_atmega328.hex
# note that avrdude cannot process “~”, need absolute directory, e.g. /home/pi/...

sudo avrdude -c linuxgpio$GPIONUM -p atmega328p -v -U flash:w:/home/$USER/Picrogrid/RaspberryPi/Setup/optiboot_atmega328.hex:i

