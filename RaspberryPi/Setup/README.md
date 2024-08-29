# Setup Instructions

These instructions will let you set up a Raspberry Pi to flash Atmega328P microcontroller on a PicroBoard. It allows you to flash the bootloader, enabling the PicroBoard to be programmable via the USB TTL FTDI 5V cable from the Arduino environment. It also sets up the Pi to program HEX code to the PicroBoard directly via the SPI pins.

## Flash Pi with BerryLan

Download BerryLan enabled Raspbian image from URL:
https://berrylan.org/

Flash BerryLan image to micro SD card using Raspberry Pi Imager:
https://www.raspberrypi.com/software/

Under Operating System, choose the Use Custom option and select the downloaded BerryLan image.

Put micro SD card in Pi and power it up.

## Connect Pi to WiFi network

Get the BerryLan app for your phone.

Follow the directions on the app to connect the Pi to WiFi.

Record the IP Address the app displays when done.

## SSH into your Pi

Using the IP Address you recorded, SSH into your Pi using the terminal command:

ssh pi@[IP Address]

The default password is "raspberry". Note that sometimes the BerryLan distribution uses "nymea" as the user, hostname, and password (accessible via: nymea@[IP Address]).

## Use these commands to download the code base:

cd ~
  
sudo apt update

sudo apt upgrade -y
  
sudo apt install -y git
  
git clone https://github.com/dgerb/Picrogrid.git

## Run setup script

Run the single pi basic setup script with the command:
  
~/Picrogrid/RaspberryPi/Setup/SetupRaspberryPi.sh

To flash the bootloader, use the command:
  
~/Picrogrid/RaspberryPi/Setup/SetupBootloader.sh

You can now program the PicroBoard with the USB TTL FTDI cable. This Pi can also be used initialize future PicroBoards with HEX code.

