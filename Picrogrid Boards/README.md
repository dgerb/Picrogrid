# AtverterH

This folder contains Arduino/AVR C++ code that can be run on ATmega-based boards in the Picrogrid ecosystem, including:
- Atverter Hobbyist Board (AtverterH) - a 48V 5A four-switch DC-DC converter with an ATmega-based digital controller
- MicroDDC - Coming soon

## Loading the Libraries

In order to run the Examples, the Library must first be installed. Installation instructions are inside the Library folder.

## Flashing the Picrogrid boards

There are three ways to load your C++ code onto the Picrogrid board:
(1) USB/TTL cable
(2) AVRISP programmer
(3) Raspberry Pi SPI pins

## Flashing the Picrogrid board with the USB/TTL cable

You can use an FTDI USB/TTL cable to flash the Picrogrid board similar to how you would upload code to an Arduino Uno. This method is recommended when you are already using the Arduino serial console for diagnostics. This method requires that the Arduino bootloader is already installed on the ATmega. For blank ATmegas (i.e. the first flash of a new Picrogrid Board), refer to one of the other two methods to flash the bootloader. This method has been tested with a Sparkfun 5V FTDI cable: https://www.sparkfun.com/products/9718

First, plug in the FTDI USB/TTL cable, noting that the black wire corresponds to the GND marking on the board.

In the one of the examples subdirectories, open an example program (such as Blink) in the Arduino IDE. Once open, select the Tools tab, and ensure the following settings:

Board: "Arduino Uno" (found under Arduino AVR Boards)

Port: "/dev/cu.usbserial-AB0JTGZW" (this is what shows up on my mac, will be different for others)

Programmer: "AVRISP mkII"

You should be able to compile and upload the example. If you selected Blink, you should see flashing lights after a few seconds.

## Flashing the Picrogrid board with the AVRISP programmer

You can use an AVRISP programmer to flash the execution code or a boot loader. This method is recommended when you need to flash a new board for the first time, but do not wish to use the board with a Raspberry Pi. The method has been tested with the HiLetgo USBTinyISP programmer: 
https://www.amazon.com/HiLetgo-USBTiny-USBtinyISP-Programmer-Bootloader/dp/B01CZVZ1XM/ref=sr_1_3?hvadid=616931726575&hvdev=c&hvlocphy=9032082&hvnetw=g&hvqmt=e&hvrand=16692277773952607837&hvtargid=kwd-6892195973&hydadcr=24657_13611735&keywords=avr+isp+programmer&qid=1692658294&sr=8-3

First, plug in the AVRISP programmer, noting that the extrusion on the side of the 6-pin connector should point away from the edge of the board.

In the one of the examples subdirectories, open an example program (such as Blink) in the Arduino IDE. Here, you can do one of two things: (a) burn the boot loader and upload code via a FTDI USB/TTL cable, or (b) upload code via the programmer, knowing that this does not upload a boot loader.

(a) To burn the boot loader, select: Tools > Burn Bootloader

(b) To upload code via the programmer, select: Sketch > Upload Using Programmer

## Flashing the Picrogrid board with the Raspberry Pi

The final method of flashing the ATmega is through the SPI pins on a Raspberry Pi. This method is the most complicated, and is only recommended if the board will connect to a Raspberry Pi and may require firmware code updates after installation.

Open the Arduino IDE with the code and settings of "Board: Arduino Uno". Click the ÒVerifyÓ button. Once successfully verified/compiled, go to "Sketch > Export Compiled Binary". This will export the compiled code as two hex files in the project directory. One of these files includes a boot loader, which you will probably want. You need the boot loader to be able to flash the Atverter with the USB/TTL cable.

Open a terminal and copy the binary file to the Pi. Here is an example SCP terminal command from my mac:

scp Blink.ino.with_bootloader.standard.hex pi@raspberrypi.local:Desktop

SSH into the Pi. The terminal command on my mac is (for Windows, use PuTTY):
ssh pi@raspberrypi.local

Once connected to the Pi, use AVRDude to flash the Atmega over SPI. If you haven't done so already, you will need to set up the Pi as an AVR programmer, following the instructions in the "Picrogrid/Raspberry Pi" subdirectory. Also confirm that the board's Reset Jumper corresponds with the listed reset pin in the /usr/local/etc/avrdude.conf file for the selected programmer (typically linuxgpio).

In this example, the avrdude command is:

sudo avrdude -c linuxgpio -p atmega328p -v -U flash:w:Desktop/Blink.ino.with_bootloader.standard.hex:i

If there are problems, you can check connectivity with the Atmega using this command:

sudo avrdude -c linuxgpio -p atmega328p -v


