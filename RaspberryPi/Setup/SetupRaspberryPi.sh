#!/bin/bash

# Prior to running this script, you'll want to do the following:
# cd ~
# sudo apt update
# sudo apt upgrade -y
# sudo apt install -y git
# git clone https://github.com/dgerb/Picrogrid.git

# To run this script, use the following:
# cd ~/Picrogrid/RaspberryPi/Setup/
# ./SetupRaspberryPi.sh

# Prepare the Pi for Picrogrid
# sudo timedatectl set-timezone America/Los_Angeles
sudo apt install -y python3-pip

# Enable SPI, I2C
sudo raspi-config nonint do_spi 0 # command to enable SPI; for flashing the Atverter
sudo raspi-config nonint do_i2c 0 # command to enable I2C; for I2C communication to Atverter

# Install AVRDude
sudo apt-get install bison flex -y
wget http://download.savannah.gnu.org/releases/avrdude/avrdude-6.2.tar.gz
tar xfv avrdude-6.2.tar.gz
cd avrdude-6.2/
./configure --enable-linuxgpio
make
sudo make install
cd ..
rm -fr avrdude-6.2 avrdude-6.2.tar.gz

# Configure the SPI and Reset pins

# Here we copy in a pre-configured version of avrdude.conf
# The other method is to sudo nano /usr/local/etc/avrdude.conf
# and look for "linuxgpio", then uncomment and enter pin numbers,
# where reset pin depends on the board's reset jumper:

#programmer
#  id    = "linuxgpio";
#  desc  = "Use the Linux sysfs interface to bitbang GPIO lines";
#  type  = "linuxgpio";
#  reset = ?;
#  sck   = 11;
#  mosi  = 10;
#  miso  = 9;
#;

sudo cp ~/Picrogrid/RaspberryPi/Setup/avrdude.conf /usr/local/etc/avrdude.conf

# Midpoint Test

echo 
echo 
echo ---------------------------------------------------------------------------------
echo 
echo At this point, you can optionally test if a board is connected.
echo To do so, copy one of the following commands, depending on Reset jumper position:
echo sudo avrdude -c linuxgpio24 -p atmega328p -v
echo sudo avrdude -c linuxgpio25 -p atmega328p -v
echo sudo avrdude -c linuxgpio12 -p atmega328p -v
echo sudo avrdude -c linuxgpio16 -p atmega328p -v
echo sudo avrdude -c linuxgpio20 -p atmega328p -v
echo sudo avrdude -c linuxgpio21 -p atmega328p -v

echo
echo Would you like to test a board connectivity? 
read -p 'Enter GPIO number: ' GPIONUM
sudo avrdude -c linuxgpio$GPIONUM -p atmega328p -v

# If you get the error:
# Can't export GPIO XX, already exported/busy?: Device or resource busy avrdude done. Thank you.
# Then use the following commands to try again:
    # echo $GPIONUM > /sys/class/gpio/unexport
    # echo 11 > /sys/class/gpio/unexport
    # echo 10 > /sys/class/gpio/unexport
    # echo 9 > /sys/class/gpio/unexport
    # sudo avrdude -c linuxgpio$GPIONUM -p atmega328p -v



