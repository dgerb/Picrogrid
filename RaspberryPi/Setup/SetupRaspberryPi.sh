#!/bin/bash

# Prior to running this script, you'll want to do the following:
# cd ~
# sudo apt update
# sudo apt upgrade -y
# sudo apt install -y git
# git clone https://github.com/dgerb/Picrogrid.git

# To run this script, use the following:
# ~/Picrogrid/RaspberryPi/Setup/SetupRaspberryPi.sh

# Prepare the Pi for Picrogrid
# sudo timedatectl set-timezone America/Los_Angeles
sudo apt install -y python3-pip

# Install raspi-gpio, needed to set reset pins to input after flashing via avrdude
sudo apt install -y raspi-gpio

# Enable SPI, I2C
sudo raspi-config nonint do_spi 0 # command to enable SPI; for flashing the Atverter
sudo raspi-config nonint do_i2c 0 # command to enable I2C; for I2C communication to Atverter
# reduce I2C speed to 50kHz (from default 100kHz) to improve signal integrity and prevent hanging
sudo sed -i '/^dtparam=i2c_arm=on/ s/$/,i2c_arm_baudrate=50000/' /boot/firmware/config.txt

# Install AVRDude
sudo apt install -y avrdude=7.1+dfsg-3

# Midpoint Test

echo 
echo 
echo ---------------------------------------------------------------------------------
echo 
echo At this point, you can optionally test if a board is connected.
echo "To do so, use this command and replace \$GPIO depending on Reset jumper position:"
echo sudo avrdude -c linuxspi -p atmega328p -v -P /dev/spidev0.0:/dev/gpiochip0:\$GPIO
echo "    (ex. sudo avrdude -c linuxspi -p atmega328p -v -P /dev/spidev0.0:/dev/gpiochip0:24 )"

echo
echo Would you like to test a board connectivity? 
read -p 'Enter GPIO number: ' GPIO
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v

echo
echo
echo Now rebooting to ensure I2C and SPI are activated.
sudo reboot







# OLD Info for using linuxgpio

# sudo cp ~/Picrogrid/RaspberryPi/Setup/avrdude.conf /usr/local/etc/avrdude.conf

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

# sudo avrdude -c linuxgpio$GPIONUM -p atmega328p -v

