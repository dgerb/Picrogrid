#!/bin/bash

# Set Atmega328P Fuses
# sometimes you'll need to set the fuses twice; need to experiment with this

sudo avrdude -c linuxgpio -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sudo avrdude -c linuxgpio -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m

# Flash the default bootloader Optiboot
# found on https://github.com/Optiboot/optiboot, optiboot/optiboot/bootloaders/optiboot/optiboot_atmega328.hex
# note that avrdude cannot process “~”, need absolute directory, e.g. /home/pi/...

sudo avrdude -c linuxgpio -p atmega328p -v -U flash:w:/home/pi/PicrogridEducational/Setup/optiboot_atmega328.hex:i
