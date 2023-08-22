# Setup Instructions

These instructions will allow you to set up an Atverter with a blank Atmega328P to be programmable via the USB TTL FTDI 5V cable from the Arduino environment. You will need a Raspberry Pi to flash the bootloader via SPI.

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

The default password is "raspberry".

## Use these commands to download the code base:

cd ~
  
sudo apt update

sudo apt upgrade -y
  
sudo apt install -y git
  
git clone https://github.com/dgerb/Picrogrid.git

## Run setup script

Run the single pi basic setup script with the command:
  
~/Picrogrid/Setup/SinglePiSetup.sh

To flash the bootloader, use the command:
  
~/Picrogrid/Setup/AtverterSetup.sh

You can now program the Atverter with the USB TTL FTDI cable. This Pi can be used initialize future Atverters. To do so, you just need to run the AtverterSetup script.

