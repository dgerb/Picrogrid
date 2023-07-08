# AtverterE

This folder contains Arduino/AVR C++ code that can be run on the Atverter.

## Loading the Libraries

You will have to install several libraries on your Arduino IDE. You only have to do this section once. Once installed, the IDE will automatically link them any of your projects that include the libraries.

The main library is AtverterE, found in the Library subdirectory. It consists of a header (.h) and C++ (.cpp) file. This library is an API with the Atverter's basic functions, including initialization, duty cycle, voltage and current sensing, messaging, and diagnostics.

Create a zip file (must be zip) that contains Atverter.h and Atverter.cpp.

In the Arduino IDE, go to Sketch > Include Library > Add .ZIP Library...

Select the zip file you just created. The library files be copied to the Arduino/libraries folder (on mac, Arduino is located in Documents).

AtverterE uses several other libraries: TimerOne, MsTimer2

These libraries may need to be added to the Arduino library folder. They are standard libraries and can be found in Sketch > Include Library > Manage Libraries...

To add each library, search for the library name. Once found, select the latest version and click Install. The libraries will each be installed in the Arduino/libraries folder.

## Flashing the Atverter with the USB/TTL cable

In the Examples subdirectory, open an example program such as Blink. Once open, select the Tools tab, and ensure the following settings:

Board: "Arduino Uno" (found under Arduino AVR Boards)

Port: "/dev/cu.usbserial-AB0JTGZW" (this is what shows up on my mac, will be different for others)

Programmer: "AVRISP mkII"

You should be able to compile and upload the example. If you selected Blink, you should see flashing lights after a few seconds.

## Flashing the Atverter with the Raspberry Pi

Open the Arduino IDE with the code and all the correct settings. Click the ÒVerifyÓ button. Once successfully verified/compiled, go to Sketch > Export compiled Binary. This will create export the compiled code as two hex files in the project directory. One of these files includes a boot loader, which you will probably want. You need the boot loader to be able to flash the Atverter with the USB/TTL cable.

Open a terminal and copy the binary file to the Pi. Here is an example SCP terminal command from my mac:

scp PicrogridEducational/Examples/Blink/Blink.ino.with_bootloader.standard.hex pi@raspberrypi.local:Desktop

SSH into the Pi. The terminal command on my mac is:
ssh pi@raspberrypi.local

Once in the Pi, use AVRDude to flash the Atmega over SPI. If you haven't done so already, you will need to set up the Pi as an AVR programmer, following the instructions in the Setup subdirectory. For this example:

sudo avrdude -c linuxgpio -p atmega328p -v -U flash:w:Desktop/Blink.ino.with_bootloader.standard.hex:i

If there are problems, you can check connectivity with the Atmega using this command:

sudo avrdude -c linuxgpio -p atmega328p -v


