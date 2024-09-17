#!/usr/bin/env python

# first enable I2C: sudo raspi-config
# also install smbus2: pip3 install smbus2

# optionally install i2c-tools: sudo apt install i2c-tools
# check I2C connections with: sudo i2cdetect -y 1
# note that the Atverters won't show up, but this can test the health of the I2C bus

# to start a session through ssh and log off:
# sudo apt install tmux
# tmux
# python3 ~/Picrogrid/RaspberryPi/CoreExamples/PanelMonitored/PanelMonitor.py &
# <ctrl+b and $ (to name session)>
# <ctrl+b and d (to exit tmux)>

# to get back into your session:
# tmux ls
# tmux attach-session -t <session-name>
# ps
# kill -9 <python3 process name>
# <ctrl+b and d (to exit tmux)>
# tmux kill-session -t <session-name>

from time import sleep
import sys
from datetime import datetime
import csv
import smbus2
import os
import RPi.GPIO as GPIO

fileName = 'output.csv'
panelAddress = 0x08

def StringToBytes(val):
    retVal = []
    for c in val:
        retVal.append(ord(c))
    return retVal

def parseValueStr(message):
    splitArr = message.split(':')
    if len(splitArr) < 2 or splitArr[0] == "Error":
        return "Error"
    else:
        valueStr = splitArr[1]
        valueStr = valueStr.replace('\n','')
        valueStr = valueStr.replace('\r','')
        return valueStr

def sendI2CCommand(address, command):
    try:
        byteValue = StringToBytes(command) # I2C requires bytes
        bus.write_i2c_block_data(address, 0x00, byteValue) # Send the byte packet to the slave.
        sleep(0.2)
        data = bus.read_i2c_block_data(address, 0x00, 32) # Send a register byte to slave with request flag.
        outString = ""
        for n in data:
            if n==255: # apparently this is the python char conversion result of '\0' in C++
                break
            outString = outString + chr(n) # assemble the byte result as a string to print
        return [outString, True]
    except:
        outString = '???'
        return [outString, False]

print("Remember to make sure I2C is enabled in raspi-config.")
GPIO.setmode(GPIO.BCM)
bus = smbus2.SMBus(1)
sleep(2) # Give the I2C device time to settle

# turn on all channels first
for channel in range(1, 5):
    success = False
    command = "WCH"+str(channel)+":1\n"
    while not success:
        [outString, success] = sendI2CCommand(panelAddress, command)
        if (outString[0:4] != command[0:4]):
            print("Failed to set channel. outString: " + outString)
            success = False
        sleep(0.2)

# set up monitoring loop
header = "Year,Month,Day,Hour,Minute,Second," + "VBus,I1,I2,I3,I4,CH1,CH2,CH3,CH4"
print(header)
with open(fileName, 'w') as the_file:
    the_file.write(header + '\n')
addresses = [panelAddress]
commands = [["RVB:\n", "RI1:\n", "RI2:\n", "RI3:\n", "RI4:\n", "RCH1:\n", "RCH2:\n", "RCH3:\n", "RCH4:\n"]]

# main monitoring loop
while True:
    failureCounter = 0
    now = datetime.now() # current date and time
    line = ''
    line = now.strftime("%Y,%m,%d,%H,%M,%S")
    for n in range(len(addresses)):
        address = addresses[n]
        commandSet = commands[n]
        for command in commandSet:
            [outString, success] = sendI2CCommand(address, command)
            if success:
                line = line + ',' + parseValueStr(outString)
            else:
                line = line + ',' + '???'
                failureCounter = failureCounter + 1
            sleep(0.2)
    print(line)
    with open(fileName, 'a') as the_file:
        the_file.write(line + '\n')
    if failureCounter > 10: # if too many i2c communications exceptions, i2c bus is likely stuck and must be reset
        print("resetting Atverters in order to clear the I2C bus...\n")
        pin = 24
        GPIO.setup(pin, GPIO.OUT)
        GPIO.output(pin, False)
        GPIO.output(pin, True)
        GPIO.setup(pin, GPIO.IN)
    sleep(0.2)
