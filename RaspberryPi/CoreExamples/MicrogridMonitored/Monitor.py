#!/usr/bin/env python

# to start a session through ssh and log off:
# sudo apt install tmux
# tmux
# python3 printVICSV.py printVI.csv &
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

fileName = 'output.csv'
solarAddress = 0x01
bmsAddress = 0x02
supplyAddress = 0x03

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

print("Remember to make sure I2C is enabled in raspi-config.")
bus = smbus2.SMBus(1)
sleep(2) # Give the I2C device time to settle

header = "Year,Month,Day,Hour,Minute,Second," + \
        "SolarV1,SolarI1,SolarV2,SolarI2," + \
        "BMSV1,BMSI1,BMSV2,BMSI2," + \
        "SupplyV1,SupplyI1,SupplyV2,SupplyI2"
print(header)
with open(fileName, 'w') as the_file:
    the_file.write(header + '\n')
# addresses = [solarAddress, bmsAddress, supplyAddress]
addresses = [supplyAddress]
commands = [["RV1:\n", "RI1:\n", "RV2:\n", "RI2:\n"], \
            ["RV1:\n", "RI1:\n", "RV2:\n", "RI2:\n"], \
            ["RV1:\n", "RI1:\n", "RV2:\n", "RI2:\n"] \
            ]

while True:
    now = datetime.now() # current date and time
    line = ''
    line = now.strftime("%Y,%m,%d,%H,%M,%S")
    for n in range(len(addresses)):
        address = addresses[n]
        commandSet = commands[n]
        for command in commandSet:
            byteValue = StringToBytes(command) # I2C requires bytes
            bus.write_i2c_block_data(address, 0x00, byteValue) # Send the byte packet to the slave.
            data = bus.read_i2c_block_data(address, 0x00, 32) # Send a register byte to slave with request flag.
            outString = ""
            for n in data:
                if n==255: # apparently this is the python char conversion result of '\0' in C++
                    break
                outString = outString + chr(n) # assemble the byte result as a string to print
            line = line + ',' + parseValueStr(outString)
            sleep(0.2)
    print(line)
    with open(fileName, 'a') as the_file:
        the_file.write(line + '\n')
    sleep(2.6)

    