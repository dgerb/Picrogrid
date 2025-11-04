#!/bin/bash

# Only run this script if you want to configure the Pi as a hotspot, using its WiFi chip to broadcast a network devices can connect to
# To run: ~/Picrogrid/RaspberryPi/Setup/SetupHotspotMode.sh

# Copy in PicrogridHotspot NetworkManager profile file, with pre-configured SSID, password, WiFi mode, interface, autoconnect IP address
sudo cp ~/Picrogrid/RaspberryPi/Setup/PicrogridHotspot.nmconnection /etc/NetworkManager/system-connections/PicrogridHotspot.nmconnection

# Change ownership to root
sudo chmod 600 /etc/NetworkManager/system-connections/PicrogridHotspot.nmconnection
sudo chown root:root /etc/NetworkManager/system-connections/PicrogridHotspot.nmconnection

# Inform NetworkManager to reload its connection profiles so it recognizes the newly added file
sudo nmcli connection reload

# Show connections as a diagnostic
sudo nmcli connection show

# Must deactivate the current wlan0 network in order to start up the new one
sudo nmcli connection down preconfigured

# Activate the new hotspot network to wlan0
sudo nmcli con up PicrogridHotspot ifname wlan0

# Reboot to restart NetworkManager properly
sudo reboot

# Restore original preconfigured WiFi network with
    # sudo nmcli con up preconfigured ifname wlan0