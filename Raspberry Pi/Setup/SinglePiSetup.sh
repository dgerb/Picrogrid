#!/bin/bash

# To keep local repository up to date:
    # git reset --hard
    # git pull

# basic provision
# if you haven't already, update/upgrade apt:
    # sudo apt update
    # sudo apt upgrade

sudo apt install -y python3-pip
sudo timedatectl set-timezone America/Los_Angeles

# Enable SPI
sudo raspi-config nonint do_spi 0 # command to enable SPI

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
#  reset = 4;
#  sck   = 11;
#  mosi  = 10;
#  miso  = 9;
#;

sudo cp "~/Picrogrid/Raspberry Pi/Setup/avrdude.conf" /usr/local/etc/avrdude.conf

# Midpoint Test

sudo avrdude -c linuxgpio -p atmega328p -v

