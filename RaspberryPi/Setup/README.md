# Setup Instructions

These instructions will let you set up a Raspberry Pi to flash Atmega328P microcontroller on a PicroBoard. It allows you to flash the bootloader, enabling the PicroBoard to be programmable via the USB TTL FTDI 5V cable from the Arduino environment. It also sets up the Pi to program HEX code to the PicroBoard directly via the SPI pins.

## Option 1: Flash Pi with Raspberry Pi Imager

Download the Raspberry Pi Imager:
https://www.raspberrypi.com/software/

Insert your micro SD card. In the imager, select the appropriate type of Raspberry Pi, the recommended Raspbian OS, and the SD card's drive image.

You will want to edit customization settings. Enter the desired hostname, username, and password. Typical defaults are raspberrypi, pi, raspberry. Enter the WiFi network name and password. Select the box for Show Password. On the next tab, select to enable SSH.

Flash the SD card and put it into the Raspberry Pi. The Pi should boot up and connect to your WiFi network with SSH enabled. 

## Option 2: Flash Pi with BerryLan

BerryLan lets you connect your Pi for headless startup using a phone app, similar to how you would connect any smart device to your WiFi network. The main advantage is that BerryLan makes it easy to reconnect the Pi if the WiFi network's name or password changes.

Download BerryLan enabled Raspbian image from URL (find the "Raspbian image" link):
https://github.com/nymea/berrylan

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

You are now done! You can now go into the example folders and run the setup scripts for that example.

Optional: If you are not running example code, and you want to program the PicroBoard with the USB TTL FTDI cable, you will need to flash the bootloader to the Picroboard. To do this:

~/Picrogrid/RaspberryPi/Setup/SetupBootloader.sh

In general, this Pi is now configured initialize future PicroBoards with HEX code.

