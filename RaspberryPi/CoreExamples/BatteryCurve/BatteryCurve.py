#!/usr/bin/env python3

# Created: 11/5/2025, Daniel Gerber
# This script can be used to characterize a battery's SOC vs voltage curve. It discharges the battery
#    for part of a minute, open circuits for the rest of the minute, and records average battery voltages
# Attach battery to 20A port of Micropanel. Make sure battery has an internal BMS
# Attach a DC electronic load to Ch 1. The DC load should not shut off if its voltage drops to zero
# Outputs a file with minute averages of:
#    Amp-seconds discharged over this minute interval
#    Watt-seconds discharged over this minute interval
#    Average battery voltage measured over the steady-state phase of the minute
#    Average battery voltage measured over the discharge phase of the minute

# first enable I2C: sudo raspi-config
# also install smbus2: pip3 install smbus2

# optionally install i2c-tools: sudo apt install i2c-tools
# check I2C connections with: sudo i2cdetect -y 1
# note that the Atverters won't show up, but this can test the health of the I2C bus

# to start a session through ssh and log off:
# sudo apt install tmux
# tmux
# python3 ~/Picrogrid/RaspberryPi/CoreExamples/PanelLoadSim/PanelLoadSim.py &
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
SDIS = 50 # seconds (per minute) during which we discharge the battery
UVCutoff = 15000 # exit if battery voltage < this mV level (BMS under voltage protection has open-circuited)

def StringToBytes(val):
    retVal = []
    for c in val:
        retVal.append(ord(c))
    return retVal

def parseValueStr(message):
    splitArr = message.split(':')
    if len(splitArr) < 2 or splitArr[0] == "Error":
        return ["Error", False]
    else:
        valueStr = splitArr[1]
        if valueStr[0] == '=':
            return ["Error", False]
        valueStr = valueStr.replace('\n','')
        valueStr = valueStr.replace('\r','')
        return [valueStr, True]

def tryValueInt(valueStr):
    try:
        valueInt = int(valueStr)
        return [valueInt, True]
    except:
        return [-1, False]

def sendI2CCommand(address, command):
    try:
        byteValue = StringToBytes(command) # I2C requires bytes
        bus.write_i2c_block_data(address, 0x00, byteValue) # Send the byte packet to the slave.
        sleep(0.1)
        data = bus.read_i2c_block_data(address, 0x00, 32) # Send a register byte to slave with request flag.
        sleep(0.1)
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

# set up monitoring loop and initialize loop variables
header = "Hour," + "Minute," + "Discharged (A*s)," + "Discharged (W*s)," + "VBus @SS," + "VBus @Dis"
with open(fileName, 'w') as the_file:
    the_file.write(header + '\n')
lastSecond = -1 # most recent recorded seconds value
dataIndex = 0 # 0 = not discharging, 1 = discharging
accumulator = [0, 0] # accumulator for vbus values
quantity = [0, 0] # quantity of data points accumulated
failureCounter = 0 # how many times the I2C bus failed
dischargeIAcc = 0 # A*s accumulator
dischargeWAcc = 0 # W*s accumulator
vbus = 0 # bus voltage reading
i1 = 0 # channel 1 current reading

# main monitoring loop
while True:
    while datetime.now().second == lastSecond:
        continue
    now = datetime.now()
    lastSecond = now.second
    if lastSecond < SDIS:
        [outString, success] = sendI2CCommand(panelAddress, "WCH1:1\n")
        if not success:
            failureCounter = failureCounter + 1
        dataIndex = 1
    elif lastSecond >= SDIS:
        [outString, success] = sendI2CCommand(panelAddress, "WCH1:0\n")
        if not success:
            failureCounter = failureCounter + 1
        dataIndex = 0

    # read the bus voltage and store that data into the proper accumulator
    [outString, success] = sendI2CCommand(panelAddress, "RVB:\n")
    [valueStr, successVal] = parseValueStr(outString)
    [valueInt, successInt] = tryValueInt(valueStr)
    if success and successVal and successInt: # I2C successful response, response has parsable value, value is an int
        vbus = valueInt
        if vbus < UVCutoff: # exit if battery internal BMS cuts off and bus voltage droops
            sys.exit()
        accumulator[dataIndex] = accumulator[dataIndex] + vbus # add bus voltage reading to the proper accumulator
        quantity[dataIndex] = quantity[dataIndex] + 1 # increment quantity of values in proper accumulator
    else:
        failureCounter = failureCounter + 1 # increment failure counter if anything went wrong in reading value

    # read the current in channel 1
    [outString, success] = sendI2CCommand(panelAddress, "RI1:\n")
    [valueStr, successVal] = parseValueStr(outString)
    [valueInt, successInt] = tryValueInt(valueStr)
    if success and successVal and successInt:
        i1 = valueInt
    else:
        failureCounter = failureCounter + 1

    # print a diagnostic statement every second to make sure things are going smoothly
    print(str(lastSecond) + ", " + str(dataIndex) + ", " + str(vbus)) + ", " + str(i1)

    # calculate and accumulate discharge A*s and W*s
    dischargeIAcc = dischargeIAcc + i1/1000*1
    dischargeWAcc = dischargeWAcc + i1/1000*vbus/1000*1

    # if it's been a minute, log the data and reset Vbus accumulators
    if lastSecond >= 59:
        dataIndex = 1
        line = str(now.hour) + "," + str(now.minute) + "," + str(int(dischargeIAcc)) + "," + str(int(dischargeWAcc)) \
             + "," + str(int(accumulator[0]/(quantity[0]+.001))) + "," + str(int(accumulator[1]/(quantity[1]+.001)))
        print(header)
        print(line)
        with open(fileName, 'a') as the_file:
            the_file.write(line + '\n')
        accumulator = [0, 0]
        quantity = [0, 0]

    # if too many i2c communications exceptions, i2c bus is likely stuck and must be reset
    if failureCounter > 10:
        print("resetting PicroBoards in order to clear the I2C bus...\n")
        pin = 24
        GPIO.setup(pin, GPIO.OUT)
        GPIO.output(pin, False)
        GPIO.output(pin, True)
        GPIO.setup(pin, GPIO.IN)
        line = "error"
        print(line)
        with open(fileName, 'a') as the_file:
            the_file.write(line + '\n')
        failureCounter = 0



