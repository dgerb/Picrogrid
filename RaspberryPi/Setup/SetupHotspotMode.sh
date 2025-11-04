#!/bin/bash

# Only run this script if you want to configure the Pi as a hotspot, using its WiFi chip to broadcast a network devices can connect to

# Copy in PicrogridHotspot NetworkManager profile file, with pre-configured SSID, password, WiFi mode, interface, autoconnect IP address
cp ~/Picrogrid/RaspberryPi/Setup/PicrogridHotspot.nmconnection /etc/NetworkManager/system-connections/PicrogridHotspot.nmconnection

# Must deactivate the current wlan0 network in order to start up the new one
sudo nmcli connection down preconfigured

# Activate the new hotspot network to wlan0
sudo nmcli con up PicrogridHotspot ifname wlan0