#!/bin/bash

# Physical setup:
# 1. Get a MicroPanelH boards. Connect loads to the outputs and a battery to the input
# 2. Place one Reset jumper on the MicroPanel at Reset select 24
# 3. Stack the MicroPanel on a Raspberry Pi using a 40-pin header extension (e.g. PRT-16764) and M2.5 18mm Standoffs
# 4. Run the ~/Picrogrid/RaspberryPi/Setup/SetupRaspberryPi.sh script if necessary
# 5. Run this script on the Raspberry Pi

# Enable I2C
sudo raspi-config nonint do_i2c 0

# Set the Atmega328P Fuses
# sometimes you'll need to set the fuses twice; need to experiment with this

GPIO=24
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
GPIO=25
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
sleep 2

# Flash the Atmega328Ps
# note that avrdude cannot process “~”, need absolute directory, e.g. /home/pi/...
GPIO=24
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -E noreset -U flash:w:/home/$USER/Picrogrid/PicroBoards/MicroPanelHExamples/CoreExamples/BatteryPanel/build/arduino.avr.uno/BatteryPanel.ino.with_bootloader.hex:i
raspi-gpio set $GPIO pu
sleep 2
GPIO=25
sudo avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0:/dev/gpiochip0:$GPIO -v -E noreset -U flash:w:/home/$USER/Picrogrid/PicroBoards/PiSupplyHExamples/CTMeasure/build/arduino.avr.uno/CTMeasure.ino.with_bootloader.hex:i
raspi-gpio set $GPIO pu
sleep 2

# # OLD: Set the Atmega328P Fuses
# # sometimes you'll need to set the fuses twice; need to experiment with this
# sudo avrdude -c linuxgpio24 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
# sleep 2
# sudo avrdude -c linuxgpio24 -p atmega328p -v -U lfuse:w:0xFF:m -U hfuse:w:0xDE:m -U efuse:w:0xFD:m
# sleep 2
# # Flash the MicroPanel
# # note that avrdude cannot process “~”, need absolute directory, e.g. /home/pi/...
# # MicroPanel: jumper 24
# sudo avrdude -c linuxgpio24 -p atmega328p -v -U flash:w:/home/$USER/Picrogrid/PicroBoards/MicroPanelHExamples/CoreExamples/BatteryPanel/build/arduino.avr.uno/BatteryPanel.ino.with_bootloader.hex:i
# sleep 2

# Set up MariaDB database on Pi
# Tested with mariadb-server version: 1:10.11.3-1+rpi1
cd ~
export DIR="./Picrogrid/RaspberryPi/CoreExamples/SmartPanelDashboard"
export DB_USER="panelpi"
export DB_PW="panelpipw"
export DB_NAME="paneldb"
# Install and initialize MariaDB (a type of MySQL database that works great on a Pi)
# last tested with version 1:10.11.3-1+rpi1
sudo apt install mariadb-server -y
sudo systemctl start mariadb
sudo systemctl enable mariadb
pip3 install mysql-connector-python --break-system-packages
pip3 install smbus2 --break-system-packages
# Create a new database with the proper settings
echo "setting time zone"
sudo mysql -u root -p -e "SET GLOBAL time_zone = '+00:00';"
echo "cleaning existing database $DB_NAME"
sudo mysql -u root -p -e "DROP DATABASE IF EXISTS $DB_NAME;"
echo "creating new database $DB_NAME"
sudo mysql -u root -p -e "CREATE DATABASE $DB_NAME;"
echo "configuring $DB_NAME from setup file"
sudo mysql -u root -p $DB_NAME < $DIR/SetupFiles/DBSetup.sql

# Install Grafana on Pi
# Tested with grafana version 12.2.1
export GRAFANA_ADMIN_PW="panelpipw"
export GRAFANA_ADMIN_TOKEN=$(echo -n 'admin:'$GRAFANA_ADMIN_PW | base64)
# Add the APT key used to authenticate packages and add the Grafana APT repository
sudo mkdir -p /etc/apt/keyrings/
wget -q -O - https://apt.grafana.com/gpg.key | gpg --dearmor | sudo tee /etc/apt/keyrings/grafana.gpg > /dev/null
echo "deb [signed-by=/etc/apt/keyrings/grafana.gpg] https://apt.grafana.com stable main" | sudo tee /etc/apt/sources.list.d/grafana.list
# Install Grafana and ButtonPanel plugin
sudo apt-get update
sudo apt-get install -y grafana=12.2.1
sudo apt install -y jq=1.6-2.1+deb12u1
sudo grafana-cli plugins install speakyourcode-button-panel
# Set Grafana such that other local machines can access the server hosted on port 3000
sudo sed -i 's/http_addr =.*/http_addr = 0.0.0.0/' /etc/grafana/grafana.ini
# Enable and start the Grafana server
sudo /bin/systemctl enable grafana-server
sudo /bin/systemctl start grafana-server
# Set time zone to raspberry pi local time zone
sudo sed -i 's/^;*default_timezone.*/default_timezone = local/' /etc/grafana/grafana.ini && sudo systemctl restart grafana-server
# Reset admin password
#   https://community.grafana.com/t/admin-password-reset/19455/22
sleep 10 # must wait a bit after start
sudo grafana-cli --homepath "/usr/share/grafana" admin reset-admin-password $GRAFANA_ADMIN_PW
sudo /bin/systemctl restart grafana-server

# Add the MariaDB database as a data source for Grafana
sleep 10 # must wait a bit after restart before can send http requests
curl -X POST \
  http://localhost:3000/api/datasources \
  -H "Authorization: Basic "$GRAFANA_ADMIN_TOKEN \
  -H "Content-Type: application/json" \
  -d '{
        "name": "MariaDB",
        "type": "mysql",
        "access": "proxy",
        "url": "localhost:3306",
        "database": "paneldb",
        "user": "panelpi",
        "password": "panelpipw",
        "isDefault": true,
        "jsonData": {
          "tlsSkipVerify": false
        },
        "secureJsonData": {
            "password": "panelpipw"
        },
        "uid": "fe7briho1xcsgd"
      }'
      # OLD: "tlsSkipVerify": true

# Import dashboard info
#   After exporting a dashboard, must format the .json file slightly to import from REST API
#   {“dashboard”:{
#       Here is where you copy-paste in the exported json structure
#   },“folderId”: 0,“overwrite”: true}
#   Also set the first "id" field to Null or it complains dashboard not found
#   https://community.grafana.com/t/import-dashboard-from-file-via-api/22266/3
sleep 3 # must wait a bit after between curl requests
curl -X POST \
  -H "Content-Type: application/json" \
  -H "Authorization: Basic "$GRAFANA_ADMIN_TOKEN \
  -d @$DIR/SetupFiles/PanelDashboard.json \
  http://localhost:3000/api/dashboards/db

# Make it so that anonymous views can view the dashboard
sleep 3 # must wait a bit after between curl requests
sudo bash -c '
sed -i "/^\[auth.anonymous\]/,/^\[/{ 
    s/^[;#]*\s*enabled\s*=.*/enabled = true/;
    s/^[;#]*\s*org_name\s*=.*/org_name = Main Org./;
    s/^[;#]*\s*org_role\s*=.*/org_role = Viewer/;
}" /etc/grafana/grafana.ini
'
sudo systemctl restart grafana-server

# To force new user into kiosk mode, open this URL:
# https://<grafana-host>/d/<dashboard_uid>?kiosk
# For example:
# http://nanogridpi.local:3000/d/display?kiosk

