#!/bin/bash

# Only works for Raspbian Bookworm, does not work in Trixie as of testing in 11/4/2025 due to a NetworkManager bug

# Run this script if you want to configure the Pi as a hotspot, using its WiFi chip to broadcast a network devices can connect to
# To run: ~/Picrogrid/RaspberryPi/Setup/SetupHotspotMode.sh

# Copy in PicrogridHotspot NetworkManager profile file, with pre-configured SSID, password, WiFi mode, interface, autoconnect IP address
sudo cp ~/Picrogrid/RaspberryPi/Setup/PicrogridHotspot.nmconnection /etc/NetworkManager/system-connections/PicrogridHotspot.nmconnection

# Change ownership to root
sudo chmod 600 /etc/NetworkManager/system-connections/PicrogridHotspot.nmconnection
sudo chown root:root /etc/NetworkManager/system-connections/PicrogridHotspot.nmconnection

# Inform NetworkManager to reload its connection profiles so it recognizes the newly added file
sudo nmcli connection reload

echo "Here's the new list of NetworkManager connections:"
echo " "

# Show connections as a diagnostic
sudo nmcli connection show

echo " "
echo "Now deactivating the current WiFi connection in order to activate hotspot mode."
echo "   If you get disconnected from SSH, it is because the WiFi got deactivated."
echo "   To exit shell, press: return, tilde (.), period (.)"
echo "Wait for a minute, and then look for the new hotspot WiFi network."
echo "Default hotspot network settings:"
echo "   SSID: picrogrid"
echo "   Password: picrogrid"
echo "   Pi IP address: 192.168.25.1"
echo "   New SSH command: ssh pi@192.168.25.1"

# Must deactivate the current wlan0 network in order to start up the new one
sudo nmcli connection down preconfigured

# Activate the new hotspot network to wlan0
sudo nmcli con up PicrogridHotspot ifname wlan0

# Reboot to restart NetworkManager properly
sudo reboot

# Restore original preconfigured WiFi network with
    # sudo nmcli con up preconfigured ifname wlan0